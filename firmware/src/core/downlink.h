#pragma once
#include <stdint.h>
#include <stddef.h>

namespace soundings {

// Downlink v1 — the decode half of contracts/downlink-v1.md.
//
// Six bytes the gateway may send while a node holds its receive window (DEC-010):
//
//   proto_ver:u8 | node_id:u8 | flags:u16 | crc16:u16       little-endian
//
// The node is asleep ~99% of the time, so this is the only moment it can be told
// anything, and it pays for the window on every wake whether or not anything is said.
// Hence: tiny, addressed, and silence is a legal answer.
//
// Encoding lives here too, and is not dead weight — the tests need it, and it is the
// only executable statement of what a valid downlink looks like on this side of the
// link. The Python daemon has its own independent encoder, which is the arrangement
// packet-v1 already uses (DEC-003).

constexpr size_t  kDownlinkLen     = 6;

// Bit 0 — firmware update waiting (issue #79, contracts/downlink-v1.md). Means "you are
// not running what I have", never "go do an update": the daemon derives it from a
// fw_version mismatch, so a lost downlink costs fifteen minutes and no correctness
// (DEC-011). The daemon's equivalent is FLAG_UPDATE_WAITING in downlink.py.
//
// 15 bits remain. Spend them only on states the node reports back — an action bit would
// need exactly-once delivery, which this link deliberately does not provide.
constexpr uint16_t kFlagUpdateWaiting = 0x0001;
constexpr uint8_t kDownlinkProtoV1 = 0x01;

struct Downlink {
    uint8_t  node_id = 0;
    uint16_t flags   = 0;   // 0 means "heard, nothing for you" — still the usual answer
};

// Decode a downlink addressed to `myNodeId`. Returns false — silently, and this is the
// ordinary case rather than an error — for anything that is not exactly six bytes, not
// proto v1, fails its CRC, or is addressed to a different node. Two nodes in earshot
// both hear both replies, so "not for me" is the common rejection, not the rare one.
//
// There is no error enum on purpose. Every rejection has the same consequence: the
// window closes and the node sleeps. A downlink that failed to parse is
// indistinguishable from one that was never sent, and a quiet window is already normal
// — making a bad one louder than an absent one would invent a failure mode out of the
// sensor's resting state.
bool decodeDownlink(const uint8_t* buf, size_t len, uint8_t myNodeId, Downlink& out);

// Build one. Returns bytes written, or 0 if `cap` is too small — refusing rather than
// writing a partial frame, same reasoning as serial_framing.h.
size_t encodeDownlink(const Downlink& d, uint8_t* out, size_t cap);

} // namespace soundings
