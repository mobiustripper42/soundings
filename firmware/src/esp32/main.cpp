#include <Arduino.h>
#include <esp_sleep.h>
#include "runcycle.h"
#include "tank_preset.h"
#include "sensor_registry.h"
#include "distance_sampler.h"
#include "nodemanifest.h"
#include "sx1262_radio.h"
#include "iclock.h"
#include "ibattery.h"
#include "isleeper.h"
#include "idistance.h"
#include "ota_client.h"

// The tank node. Phase 3.9b turns this from the Phase 1.1 skeleton into the real cycle:
// wake → sample → assemble → transmit → listen → sleep.
//
// ⚠ Two of the four seams are still stubs, and they are stubbed HONESTLY rather than
// faked plausibly:
//   - IDistance   — the A02YYUW driver exists (a02yyuw.{h,cpp}) but its ESP32 binding and
//                   its rail are issue #71. Until then the channel reports a fault, which
//                   is a true statement about a node with no sensor fitted.
//   - IBattery    — issue #49 (battery ADC, gated by ADC_Ctrl).
// A fault bit is the correct output for a declared sensor that did not answer (DEC-002),
// so the packets this sends are honest, not fabricated. Nothing downstream has to know
// the difference between "not fitted yet" and "broken", because operationally there
// isn't one.

using namespace soundings;

namespace {

struct ArduinoClock : IClock {
    uint32_t millis() const override { return ::millis(); }
};

// Deep sleep, and it does not return: the ESP32 restarts from setup() on wake, which is
// why RunCycle::runOnce() is the whole program (runcycle.h).
struct DeepSleeper : ISleeper {
    void sleepFor(uint32_t ms) override {
        esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
        esp_deep_sleep_start();
        // Not reached.
    }
};

// Entropy for the wake jitter. esp_random() is hardware-backed and available before WiFi
// is up; the jitter only has to decorrelate nodes that booted together, not be secure.
struct HwRandom : IRandom {
    uint32_t next() override { return esp_random(); }
};

// RTC memory survives deep sleep but not a power cycle — which is the right lifetime for
// a sequence number. A node that loses power starts again at 0, and a gap or a restart in
// `seq` is exactly the signal the gateway should see when that happens (issue #30).
RTC_DATA_ATTR uint16_t g_seq = 0;
struct RtcSeqStore : ISeqStore {
    uint16_t load() const override      { return g_seq; }
    void     store(uint16_t v) override { g_seq = v; }
};

// Placeholder seams, pending their own tasks. Deliberately reporting failure rather than
// a plausible number — see the warning above.
struct UnfittedDistance : IDistance {
    Reading read() override { return Reading{0, false}; }
};
struct UnfittedBattery : IBattery {
    Reading read() override { return Reading{0, false}; }
};

ArduinoClock     g_clock;
DeepSleeper      g_sleeper;
HwRandom         g_rng;
RtcSeqStore      g_seqStore;
UnfittedDistance g_distance;
UnfittedBattery  g_battery;
Sx1262Radio      g_radio(kTxPowerDbm);
DistanceSampler  g_distanceSampler(g_distance);

constexpr uint8_t  kNodeId     = 7;
// ⚠ BUMP THIS FOR EVERY IMAGE YOU PUBLISH. The daemon compares it against the manifest's
// version to decide whether a node is stale, so two different builds sharing a value are
// indistinguishable and the node will believe it is already current (issue #79).
constexpr uint16_t kFwVersion  = 0x0106;

// The OTA client — the real IDownlinkHandler (issue #79). Declared after kFwVersion
// because it needs it: bit 0 means "you are not running what I have", and "what I am
// running" is this constant.
OtaClient g_ota(kFwVersion);

#ifdef SOUNDINGS_BENCH
// Bench readout, chained IN FRONT of the OTA client rather than replacing it. It exists
// because the node is the only thing that can answer "did the reply land while the window
// was still open" (DEC-010 flagged that direction as never measured). Swapping it in
// place of g_ota — which is what the first version of this did — would mean the bench
// build never exercises the code the field build runs, which is the opposite of what a
// bench is for.
struct PrintingDownlinkHandler : IDownlinkHandler {
    void onDownlink(const Downlink& d) override {
        Serial.printf("DOWNLINK heard at %lu ms: node=%u flags=0x%04X\n",
                      (unsigned long)::millis(), (unsigned)d.node_id, (unsigned)d.flags);
        g_ota.onDownlink(d);
    }
};
PrintingDownlinkHandler g_downlinkHandler;
#endif

} // namespace

void setup() {
    Serial.begin(115200);

    // ⚠ If the radio does not come up we still run the cycle and still sleep. The
    // transmit fails, no window is held, and the node tries again in fifteen minutes —
    // which is recoverable. Halting here would be a node that never wakes again, and
    // getting it back means a trip up the tank with a cable.
#ifdef SOUNDINGS_BENCH
    // Bench-only, and it earns its place: a board that boots and then goes quiet is
    // indistinguishable from one that crashed, and on a node whose whole program ends in
    // deep sleep there is no second chance to ask. These three lines are the difference
    // between "it hangs somewhere" and a pin number.
    delay(200);                       // let the USB bridge settle before the first line
    Serial.println("\nsoundings node: setup");
#endif

    const bool radioUp = g_radio.begin();
#ifdef SOUNDINGS_BENCH
    Serial.printf("radio.begin -> %s (status %d)\n", radioUp ? "up" : "DOWN",
                  (int)g_radio.lastStatus());
#else
    (void)radioUp;
#endif

    const NodeManifest manifest = tankPreset(kNodeId);

    // The registry is what this BINARY can do; the manifest is what this NODE has
    // (DEC-002). Identity is data — the same image runs on a bed node with a different
    // manifest and binds a different subset.
    const SamplerEntry registry[] = {
        { kSensorTypeDistance, &g_distanceSampler },
    };

    SensorSlot slots[kMaxChannels];
    size_t     slotCount = 0;
    // A bad bind is not fatal. bindManifest resolves an unknown declaration to a
    // MissingSampler that rides as declared-and-faulted, so the worst case is a packet
    // full of fault bits — which is a true report, and reaches the gateway, where it can
    // be seen. Halting instead would send nothing and look identical to a flat battery.
    bindManifest(manifest, registry, sizeof(registry) / sizeof(registry[0]),
                 slots, kMaxChannels, slotCount);

    RunCycleConfig cfg = configFromManifest(manifest, kFwVersion);
#ifdef SOUNDINGS_BENCH
    // A 15-minute cadence makes a bench sitting unwatchable — one packet, then the board
    // is gone for a quarter of an hour and you cannot tell "sleeping correctly" from
    // "crashed" without waiting it out. 20 s makes the cycle observable; the jitter stays
    // proportional so the clamp and the window are still exercised, not bypassed.
    cfg.intervalMs = 20000;
    cfg.jitterMs   = 2000;
    // Overridable at the bench without a source edit, so the round-trip measurement is a
    // sweep rather than a recompile per data point. SOUNDINGS_RX_WINDOW_MS comes from the
    // build flags; absent, the node keeps runcycle.h's default.
#ifdef SOUNDINGS_RX_WINDOW_MS
    cfg.rxWindowMs = SOUNDINGS_RX_WINDOW_MS;
#endif
    Serial.printf("rx window: %lu ms\n", (unsigned long)cfg.rxWindowMs);
#endif

    RunCycle cycle(cfg,
                   slots, slotCount,
                   g_battery, g_radio, g_clock, g_sleeper, g_rng, g_seqStore,
#ifdef SOUNDINGS_BENCH
                   &g_downlinkHandler   // prints, then delegates to g_ota
#else
                   &g_ota
#endif
                   );

    // ⚠ TERMINAL. runOnce() ends in deep sleep, which resets the MCU, so this call does
    // not return on hardware. Nothing may be added below it — it would be dead code
    // on-target and green in host tests, which is the worst combination.
#ifdef SOUNDINGS_BENCH
    Serial.printf("cycle: sleeping %lu ms\n", (unsigned long)cycle.nextSleepMs());
#endif
    cycle.runOnce();
}

void loop() {}   // never reached; setup() ends in deep sleep
