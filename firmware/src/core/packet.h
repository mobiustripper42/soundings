#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cassert>

// Soundings packet v1 serializer/deserializer — the C++ (node) side of the wire
// contract in contracts/packet-v1.md (DEC-003). Byte-for-byte pinned to the
// shared golden vectors in contracts/vectors/packet-v1.json; the Python gateway
// parser (Phase 1.4) is graded against the same vectors so the two can't drift.
//
// Platform-independent: <stdint.h>/<stddef.h> only, no Arduino, no platform
// leakage — compiles for the native test runner and the ESP32-S3 alike. All
// multi-byte fields are little-endian (written byte-by-byte, so the code is
// endian-agnostic regardless of host).

namespace soundings {

constexpr uint8_t kProtoV1   = 0x01;
constexpr int     kMaxChannels = 16;   // channel_mask / fault_mask are 16-bit
constexpr size_t  kHeaderLen = 12;     // proto,node,fw,seq,batt,chan_mask,fault_mask
constexpr size_t  kCrcLen    = 2;
// Largest possible v1 packet: header + every channel at 2 B + CRC.
constexpr size_t  kMaxPacketLen = kHeaderLen + kMaxChannels * 2 + kCrcLen;  // 46

// Channel registry v1: byte width per channel bit. 0 = reserved/unknown — a bit
// the parser can't size, so a packet declaring it MUST be dropped (see spec
// "Forward-compat assumption"). Mirrors contracts/packet-v1.md § Channel registry.
constexpr uint8_t kChannelWidth[kMaxChannels] = {
    2, 2, 2, 2,   // 0-3  SOIL_TENSION_0..3   (u16)
    2, 2,         // 4-5  SOIL_TEMP_0..1       (i16)
    2, 2,         // 6-7  AIR_TEMP, AIR_RH     (u16)
    2,            // 8    TANK_DISTANCE        (u16)
    2, 2, 2, 2,   // 9-12 SOIL_TEMP_2, LEAF_WETNESS, AIR_TEMP_1, AIR_RH_1
    0, 0, 0,      // 13-15 reserved
};

// A decoded packet. Channel values are stored as raw 16-bit words indexed by
// channel bit; signedness (i16 vs u16) is a downstream interpretation concern
// and does not affect the bytes (two's-complement little-endian is identical).
// A value at index `bit` is meaningful only when (channel_mask & (1<<bit)).
struct Packet {
    uint8_t  proto_ver    = kProtoV1;
    uint8_t  node_id      = 0;
    uint16_t fw_version   = 0;
    uint16_t seq          = 0;
    uint16_t battery_mv   = 0;
    uint16_t channel_mask = 0;
    uint16_t fault_mask   = 0;
    uint16_t channels[kMaxChannels] = {0};

    // Declare a channel and set its raw value (sets the channel_mask bit).
    // `bit` must be a valid channel index — an out-of-range bit is a caller bug
    // (a bad registry constant), not bad input, so it's asserted, not masked.
    void setChannel(int bit, uint16_t raw) {
        assert(bit >= 0 && bit < kMaxChannels);
        channel_mask |= (uint16_t)(1u << bit);
        channels[bit] = raw;
    }
    // Mark an already-declared channel as faulted this cycle (DEC-002).
    void setFault(int bit) {
        assert(bit >= 0 && bit < kMaxChannels);
        fault_mask |= (uint16_t)(1u << bit);
    }

    bool hasChannel(int bit) const {
        assert(bit >= 0 && bit < kMaxChannels);
        return channel_mask & (1u << bit);
    }
    bool isFault(int bit) const {
        assert(bit >= 0 && bit < kMaxChannels);
        return fault_mask & (1u << bit);
    }
};

enum class ParseResult {
    Ok,
    TooShort,       // fewer bytes than header + CRC
    UnknownProto,   // proto_ver != kProtoV1
    UnknownChannel, // channel_mask declares a bit with no registry width — can't size the layout, MUST drop
    BadFaultMask,   // fault_mask is not a subset of channel_mask
    LengthMismatch, // declared channels imply a different total length
    BadCrc,         // CRC check failed
};

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0x0000.
uint16_t crc16_ccitt_false(const uint8_t* data, size_t len);

// Serialize `p` into `buf`. Returns bytes written, or 0 if `cap` is too small or
// the packet declares an unknown (unsized) channel.
size_t serialize(const Packet& p, uint8_t* buf, size_t cap);

// Parse `len` bytes into `out`. Returns ParseResult::Ok only on a fully valid,
// CRC-checked packet; `out` is left untouched on any error.
ParseResult deserialize(const uint8_t* buf, size_t len, Packet& out);

} // namespace soundings
