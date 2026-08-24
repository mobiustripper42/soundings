#pragma once
#include <stdint.h>
#include <stddef.h>
#include "packet.h"

namespace soundings {

// Serial framing v1 — the encode half of contracts/serial-framing-v1.md.
//
// A serial port delivers an undelimited byte stream, and packet-v1 is binary and
// variable-length, so the gateway board wraps each received packet in a 3-byte envelope
// before writing it to USB. The Python daemon unwraps it (gateway/soundings_gateway/
// framing.py). That is the entire job.
//
// This lives in src/core, not in the gateway sketch, for one practical reason: src/core
// is what `pio test -e native` compiles (platformio.ini), so the encoder is host-tested.
// A framer sitting in src/esp32 would be exercised for the first time on a bench, against
// a decoder it had never met.
//
// It is NOT behind an adapter seam, and should not be. A seam earns its keep where a real
// implementation and a fake both exist; this is a pure function over bytes with no
// hardware behind it.

constexpr uint8_t kSerialSync0 = 0xA5;
constexpr uint8_t kSerialSync1 = 0x5A;

// Sync (2) + length (1). No trailing checksum: the payload carries CRC-16/CCITT-FALSE
// over its own bytes already, and a second checksum over the same bytes is a second
// thing to get wrong (contracts/serial-framing-v1.md).
constexpr size_t kSerialFrameOverhead = 3;

// The smallest payload this envelope carries, across every type it carries.
//
// Was kHeaderLen + kCrcLen (14) — the floor of a packet-v1 frame — when the link ran one
// way and carried one thing. 3.9b added the reverse leg: a 6-byte downlink-v1 message
// travels daemon -> board over the same cable, and the smaller type sets the floor.
//
// This weakens the range check, and that is accepted rather than overlooked: the check
// was never what establishes validity. It avoids committing to an absurd length, and the
// payload's own CRC-16/CCITT-FALSE is what decides whether a candidate is real. Both
// carried types have one.
constexpr size_t kMinFramedPayload = 6;

// Wrap one packet for the wire. Returns bytes written to `out`, or 0 if it refused.
//
// Refuses — rather than truncating or writing a partial frame — when the payload length
// is outside [kMinFramedPayload, kMaxPacketLen] or `out` is too small. A partial frame is
// worse than no frame: it desynchronises the reader for one frame instead of simply not
// existing, and the caller (which has a real packet in hand) would have no way to tell.
size_t frameForSerial(const uint8_t* payload, size_t len, uint8_t* out, size_t cap);

// The DECODE half, and it lives here for the same reason the encoder does: src/core is
// what `pio test -e native` compiles.
//
// It was hand-rolled into the gateway sketch first, on the grounds that it was only
// fifteen lines — which is exactly the reasoning rejected for the encoder one task
// earlier, and it went wrong immediately. The sketch version discarded all three header
// bytes on an invalid length instead of exactly one, so a stream like `A5 A5 5A 06 ...`
// lost the real frame behind the false sync. That is the specific case
// contracts/serial-framing-v1.md step 3 exists to name, the Python reader gets it right,
// and nothing could have caught the C++ version because nothing could run it.
//
// Byte at a time, matching A02yyuwFrameParser — the caller has a byte and no obligation
// to buffer on its behalf.
class SerialFrameReader {
public:
    // Feed one byte. Returns the payload length when a frame completed (payload copied to
    // `out`), otherwise 0. A completed frame larger than `cap` is consumed and dropped —
    // reporting 0 rather than a truncated payload, which would invite parsing.
    size_t feed(uint8_t b, uint8_t* out, size_t cap);

    // Bytes discarded hunting for a frame. A resync counter, useful at a bench for
    // telling "the cable is noisy" from "nothing is transmitting".
    uint32_t discarded() const { return discarded_; }

    void reset() { held_ = 0; }

private:
    // Sync + length + the largest payload, plus the byte being fed. The algorithm never
    // retains more than one frame's worth, so this cannot be outgrown.
    uint8_t  buf_[kMaxPacketLen + kSerialFrameOverhead + 1] = {};
    size_t   held_ = 0;
    uint32_t discarded_ = 0;
};

} // namespace soundings
