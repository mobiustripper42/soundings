#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ibytesource.h"
#include "iclock.h"
#include "serial_framing.h"

namespace soundings {

// The gateway board's reply window — the daemon -> board -> node leg, timed.
//
// This lives in src/core rather than in src/gateway/main.cpp because NO test env
// compiles the sketch (platformio.ini). That is not a theoretical objection: in 3.9b the
// frame reader was hand-rolled in that file on the grounds that it was "only fifteen
// lines", got the invalid-length resync wrong, and nothing in the repo could have caught
// it. It was moved to core at review. This is the same class of logic and starts here.
//
// WHAT PROBLEM THIS SOLVES. The node holds a short receive window immediately after it
// transmits (DEC-010, runcycle.h kDefaultRxWindowMs). The board's loop used to relay a
// received packet up to the daemon and then re-enter a 200 ms BLOCKING LoRa receive —
// so the daemon's reply sat unread in the UART buffer for up to 200 ms of a 250 ms
// budget, and the downlink was transmitted, if at all, with the window nearly shut.
//
// The fix follows the protocol's own shape rather than tuning a constant: the moment a
// packet has gone up to the daemon, the node that sent it is holding its window RIGHT
// NOW, and nothing else will transmit for fifteen minutes. So the board stops listening
// to the radio and polls the serial port until the reply lands or the window closes.
//
// ⚠ This is a BUSY POLL against a deadline, deliberately. IByteSource is non-blocking by
// contract (ibytesource.h) and the gateway board is mains-powered (DEC-009), so there is
// nothing to save by sleeping and everything to lose by adding latency to the one path
// whose whole purpose is to be fast.

// Poll `src` until a complete framed payload arrives or `windowMs` elapses.
//
// Returns the payload length (copied into `out`), or 0 if the window closed first —
// which is the ordinary outcome, because the daemon replies only when it has something
// to say (contracts/downlink-v1.md).
//
// `reader` is passed in rather than owned so its partial state SURVIVES a closed window.
// A real port splits writes wherever it likes; a frame half-delivered when the window
// shuts must be completable by the next one rather than discarded and re-hunted.
//
// windowMs == 0 disables the reply window and reads nothing at all — mirroring
// rxWindowMs == 0 on the node side. It must not drain the port on the way past: bytes
// consumed by a bridge that is not relaying are bytes the next window needed.
size_t awaitFramedPayload(IByteSource& src, SerialFrameReader& reader, IClock& clock,
                          uint32_t windowMs, uint8_t* out, size_t cap);

} // namespace soundings
