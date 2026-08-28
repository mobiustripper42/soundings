#include "fw_manifest.h"
#include <string.h>

namespace soundings {
namespace {

// Written out rather than using strtoul/isxdigit: strtoul's overflow behaviour is locale-
// and errno-dependent, and this parser's whole job is to be boring about input it did not
// choose. Explicit is cheaper to verify than correct-if-you-know-the-standard.

bool hexNibble(char c, uint8_t& out) {
    // Lowercase ONLY. Accepting uppercase too would be two implementations agreeing to
    // differ on the one field where "close enough" means flashing an unchecked image.
    if (c >= '0' && c <= '9') { out = (uint8_t)(c - '0');      return true; }
    if (c >= 'a' && c <= 'f') { out = (uint8_t)(c - 'a' + 10); return true; }
    return false;
}

// Decode exactly `bytes` bytes from exactly 2*`bytes` hex chars. Any other length, or any
// non-hex char, fails. Shared by `sha256` (32 bytes) and `sig` (64) so the two fields
// cannot drift into disagreeing about case or padding.
bool parseHex(const char* v, size_t len, uint8_t* out, size_t bytes) {
    if (len != bytes * 2) return false;
    for (size_t i = 0; i < bytes; ++i) {
        uint8_t hi = 0, lo = 0;
        if (!hexNibble(v[i * 2], hi) || !hexNibble(v[i * 2 + 1], lo)) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// Append a decimal integer. Returns false if it would not fit — the caller turns that
// into a total failure rather than a short message.
bool appendUint(uint32_t v, char* buf, size_t cap, size_t& n) {
    char tmp[10];
    size_t d = 0;
    do { tmp[d++] = (char)('0' + (v % 10)); v /= 10; } while (v > 0);
    if (n + d > cap) return false;
    while (d > 0) buf[n++] = tmp[--d];
    return true;
}

bool appendLit(const char* s, char* buf, size_t cap, size_t& n) {
    const size_t len = strlen(s);
    if (n + len > cap) return false;
    memcpy(buf + n, s, len);
    n += len;
    return true;
}

bool appendHex(const uint8_t* b, size_t bytes, char* buf, size_t cap, size_t& n) {
    static const char* kHex = "0123456789abcdef";
    if (n + bytes * 2 > cap) return false;
    for (size_t i = 0; i < bytes; ++i) {
        buf[n++] = kHex[b[i] >> 4];
        buf[n++] = kHex[b[i] & 0x0F];
    }
    return true;
}

// Decimal, bounded by `limit`. Rejects empty, non-digits, and anything that would exceed
// the field — checked BEFORE the multiply, so it never overflows to find out.
bool parseUint(const char* v, size_t len, uint32_t limit, uint32_t& out) {
    if (len == 0) return false;
    uint32_t n = 0;
    for (size_t i = 0; i < len; ++i) {
        if (v[i] < '0' || v[i] > '9') return false;
        const uint32_t d = (uint32_t)(v[i] - '0');
        if (n > (limit - d) / 10) return false;
        n = n * 10 + d;
    }
    out = n;
    return true;
}

// A bare filename: no path separators, no traversal, no scheme. `file` resolves against a
// fixed base URL, so a value that could express a path is a value that can point a field
// node at an arbitrary address — and the node has no rollback.
bool validFilename(const char* v, size_t len) {
    if (len == 0 || len > kMaxManifestFileLen) return false;
    for (size_t i = 0; i < len; ++i) {
        const char c = v[i];
        if (c == '/' || c == '\\' || c == ':') return false;
        if (c < 0x21 || c > 0x7E) return false;   // printable, no spaces
    }
    // Rejects "..", "..bin", and any leading-dot traversal attempt. A legitimate image
    // name never starts with a dot.
    if (v[0] == '.') return false;
    return true;
}

bool keyIs(const char* k, size_t klen, const char* lit) {
    return klen == strlen(lit) && memcmp(k, lit, klen) == 0;
}

} // namespace

bool parseManifest(const char* text, size_t len, FwManifest& out) {
    // Bounds first, before a single byte is interpreted. Checked against what the caller
    // says it HAS rather than against a NUL, because the bytes came off a socket.
    if (text == nullptr || len == 0 || len > kMaxManifestBytes) return false;

    FwManifest m;
    bool haveVersion = false, haveSize = false, haveSha = false, haveFile = false;
    bool haveSig = false;
    size_t lines = 0;
    size_t i = 0;

    while (i < len) {
        if (++lines > kMaxManifestLines) return false;

        // Find the end of this line.
        size_t e = i;
        while (e < len && text[e] != '\n') ++e;
        size_t lineEnd = e;
        if (lineEnd > i && text[lineEnd - 1] == '\r') --lineEnd;   // tolerate CRLF

        const char*  line    = text + i;
        const size_t lineLen = lineEnd - i;
        i = e + 1;

        if (lineLen == 0) continue;          // blank
        if (line[0] == '#') continue;        // comment

        // Separator is ": " — a colon AND a space. Not split-on-first-colon: a future
        // value could legitimately contain a colon, and a parser that guessed would
        // mangle it silently.
        size_t sep = 0;
        bool found = false;
        for (size_t j = 0; j + 1 < lineLen; ++j) {
            if (line[j] == ':' && line[j + 1] == ' ') { sep = j; found = true; break; }
        }
        // A malformed line is skipped rather than fatal. It cannot hide a problem: any
        // required key it was trying to be is now missing, and that IS fatal below.
        if (!found) continue;

        const char*  key  = line;
        const size_t klen = sep;
        const char*  val  = line + sep + 2;
        size_t       vlen = lineLen - sep - 2;

        // Trailing spaces are tolerated; a value is never space-terminated in this format.
        while (vlen > 0 && val[vlen - 1] == ' ') --vlen;

        if (keyIs(key, klen, "version")) {
            uint32_t v = 0;
            if (!parseUint(val, vlen, 0xFFFF, v)) return false;
            m.version = (uint16_t)v;
            haveVersion = true;
        } else if (keyIs(key, klen, "size")) {
            uint32_t v = 0;
            if (!parseUint(val, vlen, 0xFFFFFFFFu, v)) return false;
            if (v == 0) return false;      // a zero-length image is not an image
            m.size = v;
            haveSize = true;
        } else if (keyIs(key, klen, "sha256")) {
            if (!parseHex(val, vlen, m.sha256, 32)) return false;
            haveSha = true;
        } else if (keyIs(key, klen, "file")) {
            if (!validFilename(val, vlen)) return false;
            memcpy(m.file, val, vlen);
            m.file[vlen] = '\0';
            haveFile = true;
        } else if (keyIs(key, klen, "sig")) {
            if (!parseHex(val, vlen, m.sig, 64)) return false;
            haveSig = true;
        }
        // Unknown key: ignored. See the header for why this is one-way.
    }

    if (!haveVersion || !haveSize || !haveSha || !haveFile || !haveSig) return false;

    out = m;
    return true;
}

size_t canonicalMessage(const FwManifest& m, char* buf, size_t cap) {
    if (buf == nullptr) return 0;
    size_t n = 0;

    // Fixed order, regardless of how the fields appeared in the file. Every step is
    // capacity-checked and any failure aborts the whole thing: a short buffer must
    // produce NO message rather than a shorter one.
    if (!appendLit("version: ", buf, cap, n))            return 0;
    if (!appendUint(m.version, buf, cap, n))             return 0;
    if (!appendLit("\nsize: ", buf, cap, n))             return 0;
    if (!appendUint(m.size, buf, cap, n))                return 0;
    if (!appendLit("\nsha256: ", buf, cap, n))           return 0;
    if (!appendHex(m.sha256, 32, buf, cap, n))           return 0;
    if (!appendLit("\nfile: ", buf, cap, n))             return 0;
    if (!appendLit(m.file, buf, cap, n))                 return 0;
    if (!appendLit("\n", buf, cap, n))                   return 0;

    return n;
}

bool verifyManifest(const FwManifest& m, const uint8_t pubkey[32]) {
    char msg[kMaxSignedMessageBytes];
    const size_t n = canonicalMessage(m, msg, sizeof(msg));
    if (n == 0) return false;
    return verifyEd25519((const uint8_t*)msg, n, m.sig, pubkey);
}

} // namespace soundings
