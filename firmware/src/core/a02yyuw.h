#pragma once
#include <stdint.h>
#include "idistance.h"
#include "ibytesource.h"
#include "ipowerrail.h"
#include "iclock.h"

namespace soundings {

// The A02YYUW's wire protocol, and the driver that turns it into one IDistance reading.
//
// Protocol, from DFRobot's own page for SEN0311
// (https://wiki.dfrobot.com/sen0311/docs/21651 — the URL already cited in
// docs/CHAT_HANDOFF.md HW-08), NOT inferred from the part's behaviour:
//
//   frame     : 0xFF, DATA_H, DATA_L, SUM        (4 bytes, 9600 8N1, free-running)
//   checksum  : SUM = (Header + Data_H + Data_L) & 0x00FF
//   distance  : Data_H * 256 + Data_L, in millimetres
//   worked ex.: FF 07 A1 A7  ->  0x07A1 = 1953 mm
//
// The sensor emits these continuously whenever it has power — there is no trigger and no
// request. Everything here is therefore about finding frames in a stream that started
// mid-flight, and about not trusting the ones that arrive damaged.

// A frame parser that resynchronises byte by byte.
//
// It has to: the rail comes up, the sensor starts talking, and the first byte the driver
// sees is whatever the sensor happened to be in the middle of. Nothing announces frame
// boundaries except the 0xFF header, which is also a legal SUM value.
class A02yyuwFrameParser {
public:
    // Feed one byte. Returns true exactly when a checksum-VALID frame has just completed,
    // writing its distance to mm. A malformed frame returns false and is dropped here —
    // it never reaches the driver, let alone the packet (issue 47: "malformed frames
    // dropped not propagated").
    bool feed(uint8_t b, uint16_t& mm);

    // Frames that completed with a wrong SUM. Not an error path the driver acts on — it
    // just keeps reading — but a diagnostic worth having at the bench, where "the sensor
    // is silent" and "the sensor is shouting garbage" want different cables checked.
    uint16_t badChecksums() const { return badChecksums_; }

    // Drop any partial frame and hunt for a header again. Called when the byte stream is
    // known to have been interrupted; not needed between frames.
    void reset() { held_ = 0; }

private:
    uint8_t  buf_[3]        = {};   // header, DATA_H, DATA_L — SUM is checked, never stored
    uint8_t  held_          = 0;    // bytes of the current frame in hand (0..3)
    uint16_t badChecksums_  = 0;
};

// Read strategy, from docs/tank-level-sensor.md "Read strategy (HW-08)". The physical
// numbers are seeds to bench-confirm, not gospel (CLAUDE.md conventions), so they are
// named here rather than buried in the driver.
struct A02yyuwConfig {
    // ~100 ms after power-up before frames are trusted. DFRobot publishes no power-on
    // settling spec at all; 100 ms is the RX-low response time, used as the best
    // available proxy. Bytes arriving inside this window are dropped wholesale.
    uint32_t settleMs      = 100;

    // Discard the first frames outright, then take the median of the next few. Failure
    // modes are outliers in both directions — a dropped echo reads long, multipath and
    // sidewall returns read short — so a median ignores them where a mean chases them.
    uint8_t  discardFrames = 3;
    uint8_t  sampleFrames  = 5;    // clamped to [1, kMaxSampleFrames]

    // The whole read, bounded. 3 + 5 frames at the sensor's ~100 ms cadence is ~800 ms of
    // rail-on (HARDWARE_BUILD_PLAN.md §6), so this is that plus room for the frames the
    // parser throws away. A node stuck awake is the battery killer DEC-006 found; a
    // failed reading is a fault mask bit and costs nothing.
    uint32_t deadlineMs    = 1500;

    // The SENSOR's own range: 3–450 cm per DFRobot's spec table
    // (docs/tank-level-sensor.md "Dead zone"). Anything outside it is not a distance,
    // and 0x0000 — the value a sensor with nothing to say emits — falls below minMm.
    //
    // ⚠ This is NOT a tank-geometry check. Whether 1200 mm is a plausible level for THIS
    // tank depends on the tank, and that derivation lives gateway-side in Python
    // (DEC-004) precisely so it stays re-revisable against stored raw data without
    // reflashing a node. The node only knows what its sensor can physically report.
    uint16_t minMm         = 30;
    uint16_t maxMm         = 4500;
};

class A02yyuwDistance : public IDistance {
public:
    // Ceiling on sampleFrames: the read strategy says 5–7, and a fixed array keeps the
    // driver allocation-free on a node that must never fragment a heap it cannot inspect.
    static const uint8_t kMaxSampleFrames = 7;

    A02yyuwDistance(IByteSource& bytes, IPowerRail& rail, IClock& clock,
                    const A02yyuwConfig& cfg = A02yyuwConfig())
        : bytes_(bytes), rail_(rail), clock_(clock), cfg_(cfg) {}

    // One call is one complete sample cycle: rail on, settle, discard, collect, median,
    // rail off. Deliberately whole rather than a start/poll/finish trio — the run cycle
    // walks a set of sensors it knows nothing about through ISampler::sample(), and a
    // driver that needed its own multi-call protocol would have to leak back through that
    // seam into every node type.
    Reading read() override;

    uint16_t badChecksums() const { return parser_.badChecksums(); }

private:
    IByteSource&        bytes_;
    IPowerRail&         rail_;
    IClock&             clock_;
    A02yyuwConfig       cfg_;
    A02yyuwFrameParser  parser_;
};

} // namespace soundings
