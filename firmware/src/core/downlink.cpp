#include "downlink.h"
#include "packet.h"

namespace soundings {
namespace {

// Offsets, named rather than spelled inline so the encoder and decoder cannot drift
// apart by one. contracts/downlink-v1.md is the source.
constexpr size_t kOffProto = 0;
constexpr size_t kOffNode  = 1;
constexpr size_t kOffFlags = 2;
constexpr size_t kOffCrc   = 4;

} // namespace

bool decodeDownlink(const uint8_t* buf, size_t len, uint8_t myNodeId, Downlink& out) {
    // Exactly six. Not "at least six": a longer buffer is not a downlink with something
    // appended, it is a buffer whose provenance we do not understand, and guessing that
    // the first six bytes are the interesting ones is how a parser starts inventing.
    if (len != kDownlinkLen) return false;

    if (buf[kOffProto] != kDownlinkProtoV1) return false;

    const uint16_t claimed = (uint16_t)(buf[kOffCrc] | ((uint16_t)buf[kOffCrc + 1] << 8));
    if (crc16_ccitt_false(buf, kOffCrc) != claimed) return false;

    // Addressing is checked LAST, after the message is known to be well-formed. The
    // other order would let a corrupt frame's node_id byte decide whether we bother
    // checking the CRC — and a byte that failed its checksum is not an address.
    if (buf[kOffNode] != myNodeId) return false;

    out.node_id = buf[kOffNode];
    out.flags   = (uint16_t)(buf[kOffFlags] | ((uint16_t)buf[kOffFlags + 1] << 8));
    return true;
}

size_t encodeDownlink(const Downlink& d, uint8_t* out, size_t cap) {
    if (cap < kDownlinkLen) return 0;

    out[kOffProto]     = kDownlinkProtoV1;
    out[kOffNode]      = d.node_id;
    out[kOffFlags]     = (uint8_t)(d.flags & 0xFF);
    out[kOffFlags + 1] = (uint8_t)(d.flags >> 8);

    const uint16_t crc = crc16_ccitt_false(out, kOffCrc);
    out[kOffCrc]     = (uint8_t)(crc & 0xFF);
    out[kOffCrc + 1] = (uint8_t)(crc >> 8);
    return kDownlinkLen;
}

} // namespace soundings
