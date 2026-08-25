#include "ota_client.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include <string.h>

namespace soundings {
namespace {

// mbedtls ships with the ESP-IDF, so SHA-256 costs no new dependency and no flash beyond
// what is already linked.
class Sha256 {
public:
    Sha256()  { mbedtls_sha256_init(&ctx_); mbedtls_sha256_starts(&ctx_, 0); }
    ~Sha256() { mbedtls_sha256_free(&ctx_); }
    void update(const uint8_t* p, size_t n) { mbedtls_sha256_update(&ctx_, p, n); }
    void finish(uint8_t out[32])            { mbedtls_sha256_finish(&ctx_, out); }
private:
    mbedtls_sha256_context ctx_;
};

bool configured() {
    // An empty ssid or host means node_secret.ini was absent at build time. That compiles
    // clean by design, so this is the check that turns "not configured" into "do nothing"
    // rather than into a nonsense URL fetched from a green build.
    return strlen(SOUNDINGS_WIFI_SSID) > 0 && strlen(SOUNDINGS_OTA_HOST) > 0;
}

String baseUrl() {
    // ⚠ path must begin and end with '/'. An empty or unslashed path yields
    // http://host:8080manifest.txt, which is a broken URL from a build that succeeded.
    return String("http://") + SOUNDINGS_OTA_HOST + ":" + SOUNDINGS_OTA_PORT + SOUNDINGS_OTA_PATH;
}

bool pathLooksSane() {
    const char* p = SOUNDINGS_OTA_PATH;
    const size_t n = strlen(p);
    return n >= 1 && p[0] == '/' && p[n - 1] == '/';
}

} // namespace

bool OtaClient::joinWifi() {
    // Radio on. Everywhere else in this firmware's life it is off (SPEC.md §4).
    WiFi.persistent(false);      // do not write credentials to NVS on every join
    WiFi.mode(WIFI_STA);
    WiFi.begin(SOUNDINGS_WIFI_SSID, SOUNDINGS_WIFI_PASS);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > kWifiJoinTimeoutMs) {
            Serial.printf("ota: wifi join timed out after %lu ms\n",
                          (unsigned long)(millis() - start));
            return false;
        }
        delay(50);   // the one place a delay is right: nothing else runs during a join
    }
    Serial.printf("ota: wifi up in %lu ms, ip %s, rssi %d dBm\n",
                  (unsigned long)(millis() - start),
                  WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
    return true;
}

void OtaClient::dropWifi() {
    // Unconditional, on every path out. A node that sleeps with its radio associated is
    // spending milliamps against a budget measured in microamps (DEC-006).
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
}

bool OtaClient::fetchManifest(FwManifest& out) {
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    const String url = baseUrl() + "manifest.txt";
    if (!http.begin(url)) return false;

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("ota: manifest GET %s -> %d\n", url.c_str(), code);
        http.end();
        return false;
    }

    // Bounded before reading. The manifest comes from a remote party and the node has
    // 320 KB of RAM; kMaxManifestBytes is the contract's cap.
    const int len = http.getSize();
    if (len <= 0 || (size_t)len > kMaxManifestBytes) {
        Serial.printf("ota: manifest length %d out of range\n", len);
        http.end();
        return false;
    }

    char buf[kMaxManifestBytes + 1] = {};
    const int got = http.getStream().readBytes(buf, len);
    http.end();
    if (got != len) return false;

    // The parser is in src/core and host-tested — see fw_manifest.h for why.
    return parseManifest(buf, (size_t)got, out);
}

bool OtaClient::fetchAndFlash(const FwManifest& m) {
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);
    const String url = baseUrl() + m.file;
    if (!http.begin(url)) return false;

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("ota: image GET %s -> %d\n", url.c_str(), code);
        http.end();
        return false;
    }
    // The server must agree with the manifest about length before a byte is written to
    // the OTA slot. Disagreement here means the two were published out of step.
    if ((size_t)http.getSize() != m.size) {
        Serial.printf("ota: image is %d bytes, manifest says %u\n",
                      http.getSize(), (unsigned)m.size);
        http.end();
        return false;
    }

    if (!Update.begin(m.size)) {
        Serial.printf("ota: Update.begin(%u) failed: %s\n",
                      (unsigned)m.size, Update.errorString());
        http.end();
        return false;
    }

    // Streamed, hashing as it goes. The image is far larger than RAM, so there is no
    // version of this that buffers first and checks afterwards.
    Sha256 sha;
    uint8_t  chunk[1024];
    size_t   written = 0;
    const uint32_t start = millis();
    WiFiClient* stream = http.getStreamPtr();

    while (written < m.size) {
        if (millis() - start > kDownloadTimeoutMs) {
            Serial.println("ota: download timed out");
            Update.abort();
            http.end();
            return false;
        }
        const size_t avail = stream->available();
        if (avail == 0) { delay(1); continue; }

        const size_t want = (avail > sizeof(chunk)) ? sizeof(chunk) : avail;
        const int n = stream->readBytes(chunk, want);
        if (n <= 0) continue;

        sha.update(chunk, (size_t)n);
        if (Update.write(chunk, (size_t)n) != (size_t)n) {
            Serial.printf("ota: Update.write failed: %s\n", Update.errorString());
            Update.abort();
            http.end();
            return false;
        }
        written += (size_t)n;
    }
    http.end();

    // ⚠ VERIFY BEFORE END, NOT AFTER. Update.end() is what makes the new image the boot
    // target; checking the hash afterwards would mean committing to an image we had not
    // finished checking. This is the node's own verification of what arrived OVER THE
    // AIR — the daemon separately verified the file on disk, which is a different failure.
    uint8_t got[32];
    sha.finish(got);
    if (memcmp(got, m.sha256, 32) != 0) {
        Serial.println("ota: SHA-256 MISMATCH — image discarded, not flashed");
        Update.abort();
        return false;
    }

    if (!Update.end(true)) {
        Serial.printf("ota: Update.end failed: %s\n", Update.errorString());
        return false;
    }

    Serial.printf("ota: flashed v%u (%u bytes in %lu ms) — rebooting\n",
                  (unsigned)m.version, (unsigned)m.size,
                  (unsigned long)(millis() - start));
    Serial.flush();
    dropWifi();
    ESP.restart();      // does not return
    return true;
}

void OtaClient::onDownlink(const Downlink& d) {
    if ((d.flags & 0x0001) == 0) return;    // nothing waiting; the ordinary case

    if (!configured()) {
        Serial.println("ota: flagged, but this build has no credentials — ignoring");
        return;
    }
    if (!pathLooksSane()) {
        // Caught here rather than producing http://host:8080manifest.txt and a 404 that
        // looks like a server problem.
        Serial.printf("ota: SOUNDINGS_OTA_PATH '%s' must begin and end with '/'\n",
                      SOUNDINGS_OTA_PATH);
        return;
    }

    Serial.println("ota: update flagged; bringing WiFi up");
    if (!joinWifi()) { dropWifi(); return; }

    FwManifest m;
    if (!fetchManifest(m)) { dropWifi(); return; }

    // Inequality, not ordering — republishing an older manifest is the only downgrade
    // path there is, because there is no rollback (operator call, 2026-08-23).
    if (m.version == running_) {
        // The gateway thought we were stale and the manifest disagrees. Harmless, and
        // worth a line: it means the manifest moved between the flag and the fetch.
        Serial.printf("ota: manifest says v%u and we are already running it\n",
                      (unsigned)m.version);
        dropWifi();
        return;
    }

    // On success this does not return; it reboots into the new image.
    if (!fetchAndFlash(m)) {
        Serial.println("ota: update failed — sleeping; the flag will still be set next wake");
    }
    dropWifi();
}

} // namespace soundings
