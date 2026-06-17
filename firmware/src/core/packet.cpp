#include "packet.h"

namespace soundings {

namespace {

// Little-endian byte cursors. Writing/reading byte-by-byte keeps the code correct
// on any host endianness (the native test runner and the LE ESP32-S3 both agree).
inline void putU16(uint8_t* buf, size_t& o, uint16_t v) {
    buf[o++] = (uint8_t)(v & 0xFF);
    buf[o++] = (uint8_t)(v >> 8);
}
inline uint16_t getU16(const uint8_t* buf, size_t& o) {
    uint16_t v = (uint16_t)buf[o] | ((uint16_t)buf[o + 1] << 8);
    o += 2;
    return v;
}

// Mask of channel bits this build knows how to size (registry width > 0).
inline uint16_t knownMask() {
    uint16_t m = 0;
    for (int bit = 0; bit < kMaxChannels; ++bit)
        if (kChannelWidth[bit]) m |= (uint16_t)(1u << bit);
    return m;
}

// Sum of declared channels' widths. Caller must have validated channel_mask
// against knownMask() first (an unknown bit has width 0 and would undercount).
inline size_t payloadLen(uint16_t channel_mask) {
    size_t n = 0;
    for (int bit = 0; bit < kMaxChannels; ++bit)
        if (channel_mask & (1u << bit)) n += kChannelWidth[bit];
    return n;
}

} // namespace

uint16_t crc16_ccitt_false(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                                 : (uint16_t)(crc << 1);
    }
    return crc;
}

size_t serialize(const Packet& p, uint8_t* buf, size_t cap) {
    if (p.channel_mask & ~knownMask()) return 0;  // can't size an unknown channel
    const size_t total = kHeaderLen + payloadLen(p.channel_mask) + kCrcLen;
    if (cap < total) return 0;

    size_t o = 0;
    buf[o++] = p.proto_ver;
    buf[o++] = p.node_id;
    putU16(buf, o, p.fw_version);
    putU16(buf, o, p.seq);
    putU16(buf, o, p.battery_mv);
    putU16(buf, o, p.channel_mask);
    putU16(buf, o, p.fault_mask);
    for (int bit = 0; bit < kMaxChannels; ++bit)        // ascending-bit wire order
        if (p.channel_mask & (1u << bit)) putU16(buf, o, p.channels[bit]);
    putU16(buf, o, crc16_ccitt_false(buf, o));
    return o;  // == total
}

ParseResult deserialize(const uint8_t* buf, size_t len, Packet& out) {
    if (len < kHeaderLen + kCrcLen) return ParseResult::TooShort;
    if (buf[0] != kProtoV1)         return ParseResult::UnknownProto;

    Packet p;
    size_t o = 0;
    p.proto_ver    = buf[o++];
    p.node_id      = buf[o++];
    p.fw_version   = getU16(buf, o);
    p.seq          = getU16(buf, o);
    p.battery_mv   = getU16(buf, o);
    p.channel_mask = getU16(buf, o);
    p.fault_mask   = getU16(buf, o);

    // An unrecognized channel bit can't be sized — the rest of the packet is
    // unwalkable, so drop rather than best-effort parse (spec § Versioning).
    if (p.channel_mask & ~knownMask())     return ParseResult::UnknownChannel;
    if (p.fault_mask & ~p.channel_mask)    return ParseResult::BadFaultMask;

    const size_t total = kHeaderLen + payloadLen(p.channel_mask) + kCrcLen;
    if (len != total)                      return ParseResult::LengthMismatch;

    for (int bit = 0; bit < kMaxChannels; ++bit)
        if (p.channel_mask & (1u << bit)) p.channels[bit] = getU16(buf, o);

    const uint16_t calc = crc16_ccitt_false(buf, o);
    size_t co = o;
    if (calc != getU16(buf, co))           return ParseResult::BadCrc;

    out = p;
    return ParseResult::Ok;
}

} // namespace soundings
