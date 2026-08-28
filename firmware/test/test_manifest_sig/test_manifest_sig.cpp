#include <unity.h>
#include <ArduinoJson.h>
#include <fstream>
#include <sstream>
#include <string>
#include "fw_manifest.h"
#include "ed25519.h"

// Signed-manifest tests (manifest v2), graded against the SHARED golden vectors
// (contracts/vectors/manifest-sig-v1.json) — the same fixtures the Python daemon
// and publish tool are checked against. Neither side is graded against the other's
// idea of correct, which is the arrangement packet-v1 already uses.
//
// The vector that matters most is `signature_over_another_manifest`: a genuine
// signature by the real key, over different content. An implementation that
// verifies a signature without binding it to THIS manifest accepts it, and every
// other test here still passes. That is the shape a test suite can miss.

using namespace soundings;

// Path is relative to the CWD `pio test` runs from (the firmware/ project dir).
#ifndef SIG_VECTORS_JSON
#define SIG_VECTORS_JSON "../contracts/vectors/manifest-sig-v1.json"
#endif

static JsonDocument g_doc;
static uint8_t      g_pubkey[32];

void setUp() {}
void tearDown() {}

static size_t fromHex(const std::string& s, uint8_t* out, size_t cap) {
    size_t n = s.size() / 2;
    TEST_ASSERT_TRUE(n <= cap);
    for (size_t i = 0; i < n; ++i)
        out[i] = (uint8_t)std::stoi(s.substr(i * 2, 2), nullptr, 16);
    return n;
}

static void loadVectors() {
    std::ifstream f(SIG_VECTORS_JSON);
    TEST_ASSERT_TRUE_MESSAGE(f.good(),
        "cannot open " SIG_VECTORS_JSON " — run `pio test` from firmware/");
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string json = ss.str();
    DeserializationError e = deserializeJson(g_doc, json);
    TEST_ASSERT_TRUE_MESSAGE(!e, "golden-vector JSON failed to parse");
    const std::string pk = g_doc["test_key"]["public_key_hex"].as<const char*>();
    TEST_ASSERT_EQUAL_size_t(32, fromHex(pk, g_pubkey, sizeof(g_pubkey)));
}

static JsonObject vectorNamed(const char* name) {
    for (JsonObject v : g_doc["vectors"].as<JsonArray>())
        if (std::string(v["name"].as<const char*>()) == name) return v;
    TEST_FAIL_MESSAGE("no such vector in the golden file");
    return JsonObject();
}

// --- Every vector, both outcomes ------------------------------------------

// Walks the whole golden file rather than naming vectors one at a time: a vector
// added to the JSON is a vector this test runs, with no C++ edit to forget.
static void test_all_golden_vectors(void) {
    for (JsonObject v : g_doc["vectors"].as<JsonArray>()) {
        const std::string name = v["name"].as<const char*>();
        const std::string text = v["manifest"].as<const char*>();
        const bool wantParse   = v["expect_parse"].as<bool>();
        const bool wantVerify  = v["expect_verify"].as<bool>();

        FwManifest m;
        const bool parsed = parseManifest(text.c_str(), text.size(), m);
        TEST_ASSERT_EQUAL_MESSAGE(wantParse, parsed, name.c_str());
        if (!parsed) continue;

        const bool verified = verifyManifest(m, g_pubkey);
        TEST_ASSERT_EQUAL_MESSAGE(wantVerify, verified, name.c_str());
    }
}

// --- The canonical message ------------------------------------------------

// The signed bytes are reconstructed from parsed values, not copied from the
// file. Two manifests whose LAYOUT differs but whose VALUES agree must produce
// byte-identical messages — otherwise three parsers could disagree about what
// was signed while all believing they agreed.
static void test_canonical_message_ignores_layout(void) {
    const std::string a = vectorNamed("valid")["manifest"].as<const char*>();
    const std::string b =
        vectorNamed("layout_varies_signature_holds")["manifest"].as<const char*>();

    FwManifest ma, mb;
    TEST_ASSERT_TRUE(parseManifest(a.c_str(), a.size(), ma));
    TEST_ASSERT_TRUE(parseManifest(b.c_str(), b.size(), mb));

    char bufA[kMaxSignedMessageBytes];
    char bufB[kMaxSignedMessageBytes];
    const size_t na = canonicalMessage(ma, bufA, sizeof(bufA));
    const size_t nb = canonicalMessage(mb, bufB, sizeof(bufB));

    TEST_ASSERT_TRUE(na > 0);
    TEST_ASSERT_EQUAL_size_t(na, nb);
    TEST_ASSERT_EQUAL_MEMORY(bufA, bufB, na);
}

// The exact bytes, spelled out. This is the one assertion that pins the format
// rather than merely checking two implementations agree with each other.
static void test_canonical_message_exact_bytes(void) {
    const std::string t = vectorNamed("valid")["manifest"].as<const char*>();
    FwManifest m;
    TEST_ASSERT_TRUE(parseManifest(t.c_str(), t.size(), m));

    const char* expected =
        "version: 261\n"
        "size: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n";

    char buf[kMaxSignedMessageBytes];
    const size_t n = canonicalMessage(m, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(strlen(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, buf, n);
}

// A buffer too small must fail rather than truncate. A truncated message is a
// message that hashes to something, and something is what an attacker wants.
static void test_canonical_message_refuses_short_buffer(void) {
    const std::string t = vectorNamed("valid")["manifest"].as<const char*>();
    FwManifest m;
    TEST_ASSERT_TRUE(parseManifest(t.c_str(), t.size(), m));

    char small[16];
    TEST_ASSERT_EQUAL_size_t(0, canonicalMessage(m, small, sizeof(small)));
}

// --- The primitive itself -------------------------------------------------

// Graded directly, not only through the manifest layer: if the curve arithmetic
// is wrong, this says so in one line instead of as eleven confusing failures.
static void test_ed25519_verifies_the_golden_signature(void) {
    const std::string t = vectorNamed("valid")["manifest"].as<const char*>();
    FwManifest m;
    TEST_ASSERT_TRUE(parseManifest(t.c_str(), t.size(), m));

    char msg[kMaxSignedMessageBytes];
    const size_t n = canonicalMessage(m, msg, sizeof(msg));
    TEST_ASSERT_TRUE(verifyEd25519((const uint8_t*)msg, n, m.sig, g_pubkey));
}

// Every single-bit flip of the signature must fail. Cheap, and it catches an
// implementation that returns true on some structural property of the input
// rather than on the actual curve equation.
static void test_ed25519_rejects_every_flipped_signature_byte(void) {
    const std::string t = vectorNamed("valid")["manifest"].as<const char*>();
    FwManifest m;
    TEST_ASSERT_TRUE(parseManifest(t.c_str(), t.size(), m));

    char msg[kMaxSignedMessageBytes];
    const size_t n = canonicalMessage(m, msg, sizeof(msg));

    for (size_t i = 0; i < 64; ++i) {
        uint8_t bad[64];
        memcpy(bad, m.sig, 64);
        bad[i] ^= 0x01;
        TEST_ASSERT_FALSE_MESSAGE(
            verifyEd25519((const uint8_t*)msg, n, bad, g_pubkey),
            "a one-bit change to the signature was accepted");
    }
}

// A message longer than the bound must be refused, not silently truncated into
// the fixed buffer the verifier hashes from.
static void test_ed25519_refuses_oversized_message(void) {
    uint8_t big[kMaxSignedMessageBytes + 1] = {};
    uint8_t sig[64] = {};
    TEST_ASSERT_FALSE(verifyEd25519(big, sizeof(big), sig, g_pubkey));
}

int main(int, char**) {
    UNITY_BEGIN();
    loadVectors();
    RUN_TEST(test_all_golden_vectors);
    RUN_TEST(test_canonical_message_ignores_layout);
    RUN_TEST(test_canonical_message_exact_bytes);
    RUN_TEST(test_canonical_message_refuses_short_buffer);
    RUN_TEST(test_ed25519_verifies_the_golden_signature);
    RUN_TEST(test_ed25519_rejects_every_flipped_signature_byte);
    RUN_TEST(test_ed25519_refuses_oversized_message);
    return UNITY_END();
}
