#pragma once
#include <stdint.h>
#include <stddef.h>
#include "packet.h"
#include "iclock.h"
#include "iradio.h"
#include "ibattery.h"
#include "isleeper.h"
#include "isampler.h"
#include "irandom.h"
#include "iseqstore.h"

namespace soundings {

// RunCycle — wake → sample → assemble → transmit → sleep, once.
//
// ⚠ THERE IS NO LOOP. On hardware, sleepFor() resets the MCU (isleeper.h), so runOnce()
// is the whole program: it is entered from setup() and it does not return in the field.
// Anything written after the sleepFor() call is dead code on-target and green in host
// tests, because FakeSleeper returns. Treat the sleep as the end of the function and
// review it that way — the compiler will not help.
//
// Node-type-agnostic: it walks an array of SensorSlot and knows nothing about what any of
// them measure. Phase 3.5 builds that array from a declared manifest; here it is passed in.

// Cadence: SPEC §"Power budget" settles on 15 min (a 10-min interval cuts the margin too
// far), with ±30 s jitter so nodes that boot together do not collide on every wake.
constexpr uint32_t kDefaultIntervalMs = 15u * 60u * 1000u;   // 900000
constexpr uint32_t kDefaultJitterMs   = 30u * 1000u;         // ±30 s
constexpr uint8_t  kDefaultTxRetries  = 3;
// Hard ceiling on the Busy-retry spin. DEC-006's finding is that the battery killer is a
// node stuck AWAKE, not one that sleeps badly — so a wedged radio must never be able to
// hold the cycle open. Retries are bounded by count AND by this window, whichever trips
// first, and a lost packet is always cheaper than a flat pack.
constexpr uint32_t kDefaultTxWindowMs = 1000;

struct SensorSlot {
    uint8_t   channelBit;   // index into the packet.h channel registry
    ISampler* sampler;
};

struct RunCycleConfig {
    uint8_t  node_id      = 0;
    uint16_t fw_version   = 0;
    uint32_t intervalMs   = kDefaultIntervalMs;
    uint32_t jitterMs     = kDefaultJitterMs;
    uint8_t  txRetries    = kDefaultTxRetries;
    uint32_t txWindowMs   = kDefaultTxWindowMs;
};

class RunCycle {
public:
    RunCycle(const RunCycleConfig& cfg,
             const SensorSlot* slots, size_t slotCount,
             IBattery& battery, IRadio& radio, IClock& clock,
             ISleeper& sleeper, IRandom& rng, ISeqStore& seq)
        : cfg_(cfg), slots_(slots), slotCount_(slotCount),
          battery_(battery), radio_(radio), clock_(clock),
          sleeper_(sleeper), rng_(rng), seq_(seq) {}

    // One cycle. Terminal on hardware — see the warning above.
    void runOnce();

    // Exposed for tests and for a caller that wants the schedule without the sleep.
    // Returns intervalMs offset by a jittered amount in [-jitterMs, +jitterMs].
    uint32_t nextSleepMs();

private:
    // Returns bytes written into buf, or 0 if the packet could not be serialized.
    size_t assemble(uint8_t* buf, size_t cap);
    // Returns true if the radio accepted the frame.
    bool transmit(const uint8_t* buf, size_t len);

    const RunCycleConfig cfg_;
    const SensorSlot*    slots_;
    size_t               slotCount_;
    IBattery&            battery_;
    IRadio&              radio_;
    IClock&              clock_;
    ISleeper&            sleeper_;
    IRandom&             rng_;
    ISeqStore&           seq_;
};

} // namespace soundings
