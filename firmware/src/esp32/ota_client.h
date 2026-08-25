#pragma once
#include "idownlinkhandler.h"
#include "fw_manifest.h"

namespace soundings {

// The OTA client — what the node DOES about a downlink with bit 0 set.
//
// Binds to IDownlinkHandler, so it runs INSIDE the wake, before deep sleep. On hardware
// there is no "after runOnce()" (isleeper.h), which is why the seam exists at all.
//
// ⚠ THIS FILE IS COMPILED BY NO TEST ENV. Everything decidable without a radio lives in
// src/core/fw_manifest.{h,cpp} and is host-tested there; what remains here is WiFi, HTTP,
// mbedtls and Update — four libraries that only exist on-target. Keep it that way: logic
// that migrates into this file becomes logic that is first exercised at a bench, which is
// how 3.9b shipped a broken frame reader and how the microseconds-for-milliseconds bug
// survived every test in the repo.
//
// WiFi is off at every other moment of the node's life (SPEC.md §4/§8). It comes up here
// and only here, and it comes down before this function returns.

// Credentials and server address arrive as build flags interpolated from
// firmware/node_secret.ini (see docs/DEV_REFERENCE.md). An ABSENT secret file compiles to
// empty strings rather than failing the build — which means "not configured", and the
// client refuses to do anything. That is the case to get right: an empty host builds
// clean and would otherwise produce a nonsense URL from a green build.
#ifndef SOUNDINGS_WIFI_SSID
#define SOUNDINGS_WIFI_SSID ""
#endif
#ifndef SOUNDINGS_WIFI_PASS
#define SOUNDINGS_WIFI_PASS ""
#endif
#ifndef SOUNDINGS_OTA_HOST
#define SOUNDINGS_OTA_HOST ""
#endif
#ifndef SOUNDINGS_OTA_PORT
#define SOUNDINGS_OTA_PORT ""
#endif
#ifndef SOUNDINGS_OTA_PATH
#define SOUNDINGS_OTA_PATH ""
#endif

// ⚠ EVERY ONE OF THESE IS A BOUND ON TIME SPENT AWAKE, NOT A NICETY. DEC-006's finding is
// that the battery killer is a node stuck awake, and this is the only code path in the
// project that talks to a network. Each step gets its own deadline and every failure ends
// the same way: WiFi off, sleep, try again in fifteen minutes with the flag still set.
constexpr uint32_t kWifiJoinTimeoutMs   = 15000;   // association + DHCP, cold, from a cold radio
constexpr uint32_t kHttpTimeoutMs       = 10000;   // per request
constexpr uint32_t kDownloadTimeoutMs   = 120000;  // whole image; ~320 KB on a slow link

class OtaClient : public IDownlinkHandler {
public:
    explicit OtaClient(uint16_t runningVersion) : running_(runningVersion) {}

    // IDownlinkHandler. Does nothing unless bit 0 is set, and nothing if this build has no
    // credentials. Never throws, never blocks past its deadlines, and always leaves WiFi
    // off. On success it does not return at all: Update ends in a reboot.
    void onDownlink(const Downlink& d) override;

private:
    bool joinWifi();
    void dropWifi();
    // Fetch <base>manifest.txt into `out`. False on any HTTP or parse failure.
    bool fetchManifest(FwManifest& out);
    // Stream the image into the OTA slot, verifying SHA-256 as it goes. Does not return
    // on success — it reboots.
    bool fetchAndFlash(const FwManifest& m);

    uint16_t running_;
};

} // namespace soundings
