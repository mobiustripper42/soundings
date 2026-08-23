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

// The smallest legal packet-v1 payload: a header and a CRC with no channels declared.
// Named here because the framer range-checks the length field against it, and a reader
// that accepts a shorter one would hand the parser something that cannot be a packet.
constexpr size_t kMinPacketLen = kHeaderLen + kCrcLen;   // 14

// Wrap one packet for the wire. Returns bytes written to `out`, or 0 if it refused.
//
// Refuses — rather than truncating or writing a partial frame — when the payload length
// is outside [kMinPacketLen, kMaxPacketLen] or `out` is too small. A partial frame is
// worse than no frame: it desynchronises the reader for one frame instead of simply not
// existing, and the caller (which has a real packet in hand) would have no way to tell.
size_t frameForSerial(const uint8_t* payload, size_t len, uint8_t* out, size_t cap);

} // namespace soundings
