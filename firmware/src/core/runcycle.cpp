#include "runcycle.h"
#include "elapsed.h"

namespace soundings {

size_t RunCycle::assemble(uint8_t* buf, size_t cap) {
    Packet p;
    p.node_id    = cfg_.node_id;
    p.fw_version = cfg_.fw_version;
    p.seq        = seq_.load();

    // A failed battery read does not suppress the packet — the sensor data is the point.
    // 0 goes on the wire rather than a remembered or invented value: this node did not
    // measure a voltage this cycle, and 0 mV is not a level a live pack can report.
    IBattery::Reading b = battery_.read();
    p.battery_mv = b.ok ? b.millivolts : 0;

    for (size_t i = 0; i < slotCount_; ++i) {
        const SensorSlot& slot = slots_[i];
        if (!slot.sampler) continue;
        ISampler::Sample s = slot.sampler->sample();
        // Declared either way (DEC-002). A channel dropped on failure would be
        // indistinguishable downstream from a sensor that was never fitted, which is the
        // exact ambiguity the fault mask exists to remove.
        p.setChannel(slot.channelBit, s.ok ? s.raw : 0);
        if (!s.ok) p.setFault(slot.channelBit);
    }

    return serialize(p, buf, cap);
}

bool RunCycle::transmit(const uint8_t* buf, size_t len) {
    // Bounded by BOTH a retry count and a wall-clock window. The count is the ordinary
    // limit; the window is the one that matters when the count is misconfigured, because
    // DEC-006's finding is that the battery killer is a node stuck awake — not one that
    // sleeps badly. A lost packet is always cheaper than a flat pack.
    Elapsed window(clock_);
    window.arm(cfg_.txWindowMs);

    uint32_t attempts = 0;
    for (;;) {
        IRadio::TxResult r = radio_.send(buf, len);
        ++attempts;
        if (r == IRadio::TxResult::Ok)     return true;
        // Failed is not retryable: the radio is broken, not busy, and hammering it spends
        // the power budget for nothing. Busy is the retryable one — that is why TxResult
        // keeps them apart instead of returning a bool.
        if (r == IRadio::TxResult::Failed) return false;
        if (attempts > cfg_.txRetries)     return false;
        if (window.expired())              return false;
    }
}

uint32_t RunCycle::nextSleepMs() {
    if (cfg_.jitterMs == 0) return cfg_.intervalMs;

    // Clamped, because `intervalMs - jitterMs` is unsigned: a config with jitter larger
    // than the interval underflows to ~49 days, and on a node whose sleep is TERMINAL
    // that is not a long nap — it is a brick until someone walks out and pulls the cell.
    // Nothing in RunCycleConfig can enforce the relationship between two independent
    // fields, and 3.5 will be filling both from a manifest, so the guard lives at the one
    // point that consumes them.
    //
    // Clamp rather than assert on purpose: a node that wakes too often is wrong and
    // recoverable, a node that aborts on boot is a reflash in a tunnel.
    const uint32_t jitter = (cfg_.jitterMs > cfg_.intervalMs) ? cfg_.intervalMs
                                                              : cfg_.jitterMs;
    // The modulus is what bounds the window, not the raw entropy — a generator returning
    // 0xFFFFFFFF must still land inside ±jitter, or one node sleeps for an hour.
    const uint32_t span   = jitter * 2u + 1u;             // inclusive of both extremes
    const uint32_t offset = rng_.next() % span;           // [0, 2*jitter]
    return cfg_.intervalMs - jitter + offset;
}

bool RunCycle::shouldListenThisWake() const {
    if (cfg_.rxWindowMs == 0) return false;      // listening disabled outright
    if (cfg_.rxEveryNWakes <= 1) return true;    // 0 and 1 both mean "every wake"

    // The sequence number IS the wake counter — it advances once per cycle and lives in
    // RTC memory, which survives deep sleep but not a power cycle. Reusing it rather than
    // adding a second counter is deliberate: two RTC counters that must stay in step are
    // two things to get wrong, and this one already exists for issue #30's gap detection.
    // Reading it for a second purpose does not disturb that.
    //
    // A power cycle resets seq to 0, and 0 % N == 0, so a node someone has just walked out
    // and repowered listens on its very next wake. That is the behaviour you want from the
    // one action a person takes when a node is misbehaving.
    return (seq_.load() % cfg_.rxEveryNWakes) == 0;
}

void RunCycle::listen() {
    downlinkValid_ = false;
    if (!shouldListenThisWake()) return;   // do not touch the radio

    uint8_t buf[kDownlinkLen];
    const size_t n = radio_.receive(buf, sizeof(buf), cfg_.rxWindowMs);
    // n == 0 is silence, which is what almost every window looks like. Not a fault, not
    // a retry, and nothing downstream is told about it (contracts/downlink-v1.md).
    if (n == 0) return;

    // Every rejection inside decodeDownlink — wrong length, wrong protocol, bad CRC, or
    // addressed to another node — lands here identically: downlinkValid_ stays false and
    // the cycle carries on. A downlink we could not read is indistinguishable from one
    // that was never sent, and the quiet case is already the normal one.
    downlinkValid_ = decodeDownlink(buf, n, cfg_.node_id, downlink_);
}

void RunCycle::runOnce() {
    uint8_t buf[kMaxPacketLen];
    const size_t n = assemble(buf, sizeof(buf));

    if (n > 0) {
        // The window is held only after a transmit the radio ACCEPTED. Listening after a
        // failed send is airtime spent waiting for a reply to something nobody heard.
        if (transmit(buf, n)) {
            listen();
            // ⚠ HERE, and not after the sleep, because there is no after — the sleep
            // below resets the MCU on hardware. This is the "OTA path inside the cycle
            // itself" this file has pointed at since 3.9b.
            if (downlinkValid_ && downlinkHandler_ != nullptr) {
                downlinkHandler_->onDownlink(downlink_);
            }
        }
        // Advanced whether or not the transmit succeeded. A dropped packet consuming its
        // sequence number is what makes the loss VISIBLE downstream — reusing it would
        // close the gap and hide exactly what issue #30 is meant to detect.
        seq_.store((uint16_t)(seq_.load() + 1));
    }
    // n == 0 means serialize() refused the packet — an unsized channel bit, i.e. a bad
    // registry constant, not bad input. Nothing goes out and the sequence does not
    // advance, because no packet was ever spent.

    // ⚠ TERMINAL ON HARDWARE. Deep sleep resets the MCU (isleeper.h), so this is the last
    // statement of the program in the field. NOTHING may be added below this line — it
    // would run in host tests, where FakeSleeper returns, and never on a node.
    sleeper_.sleepFor(nextSleepMs());
}

} // namespace soundings
