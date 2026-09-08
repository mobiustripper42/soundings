#include <Arduino.h>
#include <esp_sleep.h>
#include "runcycle.h"
#include "tank_preset.h"
#include "sensor_registry.h"
#include "distance_sampler.h"
#include "temp_sampler.h"
#include "nodemanifest.h"
#include "sx1262_radio.h"
#include "iclock.h"
#include "ibattery.h"
#include "isleeper.h"
#include "idistance.h"
#include "itemp.h"
#include "a02yyuw.h"
#include "ds18b20.h"
#include "uart_bytesource.h"
#include "onewire_bus.h"
#include "vext_rail.h"
#include "ota_client.h"

// The tank node. Phase 3.9b turns this from the Phase 1.1 skeleton into the real cycle:
// wake → sample → assemble → transmit → listen → sleep.
//
// ⚠ ONE of the four seams is still a stub, and it is stubbed HONESTLY rather than faked
// plausibly:
//   - IBattery    — issue #49 (battery ADC, gated by ADC_Ctrl).
// A fault bit is the correct output for a declared sensor that did not answer (DEC-002),
// so the packets this sends are honest, not fabricated. Nothing downstream has to know
// the difference between "not fitted yet" and "broken", because operationally there
// isn't one.
//
// IDistance became real in 3.8b (issue #71): a live A02YYUW on GPIO6, gated by the Vext
// rail on GPIO36. ITemp became real in 3.8c (issue #94): a DS18B20 on GPIO7 behind a
// bit-banged 1-Wire bus, on the same rail. The node now ships two channels — distance on
// bit 8 and headspace temperature on bit 4 — which is what DEC-007 requires for the
// gateway's speed-of-sound correction to have an input.

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

// Placeholder seam, pending its own task. Deliberately reporting failure rather than a
// plausible number — see the warning above.
struct UnfittedBattery : IBattery {
    Reading read() override { return Reading{0, false}; }
};

ArduinoClock     g_clock;
DeepSleeper      g_sleeper;
HwRandom         g_rng;
RtcSeqStore      g_seqStore;
UnfittedBattery  g_battery;
Sx1262Radio      g_radio(kTxPowerDbm);

// The real tank sensor. Declared after g_clock because it holds a reference to it, and a
// global initialised before the thing it refers to is a bug that compiles.
UartByteSource   g_sensorBytes;
VextRail         g_sensorRail;
A02yyuwDistance  g_distance(g_sensorBytes, g_sensorRail, g_clock);
DistanceSampler  g_distanceSampler(g_distance);

// The headspace probe. It shares g_sensorRail with the distance sensor deliberately: each
// driver cycles the rail itself, so the free-running A02YYUW is unpowered while the DS18B20
// converts. That is two rail cycles per wake instead of one, and it is simpler than
// ref-counting a rail two owners would have to agree about.
RTC_DATA_ATTR bool g_healAttempted = false;
struct RtcHealAttemptFlag : IHealAttemptFlag {
    bool attempted() const override { return g_healAttempted; }
    void markAttempted() override   { g_healAttempted = true; }
};
RtcHealAttemptFlag g_healFlag;
OneWireBus       g_oneWire;
Ds18b20Temp      g_temp(g_oneWire, g_sensorRail, g_clock, g_healFlag);
TempSampler      g_tempSampler(g_temp);

constexpr uint8_t  kNodeId     = 7;
// ⚠ BUMP THIS FOR EVERY IMAGE YOU PUBLISH. The daemon compares it against the manifest's
// version to decide whether a node is stale, so two different builds sharing a value are
// indistinguishable and the node will believe it is already current (issue #79).
constexpr uint16_t kFwVersion  = 0x0109;   // 265 — adds the live DS18B20 headspace channel

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

    // Serial1 on GPIO6, for the sensor. Not in a constructor: a global built before
    // Arduino's init() would be configuring a peripheral that is not up yet.
    g_sensorBytes.begin();

    // Idle the 1-Wire line released before the rail ever comes up. A pad left as an output
    // from a previous boot would hold DQ low through the DS18B20's power-on and the part
    // would never answer a presence pulse.
    g_oneWire.begin();

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
    // Reported at the START of a wake, not the end, because runOnce() is terminal — there is
    // no "after" on this program. The flag lives in RTC memory, so what this prints is the
    // verdict of a PREVIOUS wake: set means the probe's EEPROM refused a 9-bit write and the
    // node has stopped retrying until reboot (DEC-015). Clear is the ordinary state and says
    // nothing about whether a heal ever happened.
    Serial.printf("ds18b20 heal flag: %s\n",
                  g_healAttempted ? "SET — a previous wake could not write the probe's EEPROM"
                                  : "clear");
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
        { kSensorTypeDsTemp,   &g_tempSampler     },
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
