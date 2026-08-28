#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "fw_manifest.h"

// Phase 3.9e — the firmware manifest parser (contracts/firmware-manifest-v2.md).
//
// The node fetches this over HTTP before it fetches an image, so every byte here came
// from the network. It is the first thing in this project that parses something a
// remote party chose the length of, which is why the bounds are tests rather than
// comments.
//
// Rejection is silent and TOTAL: a manifest that fails any rule updates nothing and the
// node sleeps. There is no partial application and no error path, matching downlink-v1 —
// a node that cannot read a manifest is indistinguishable, from the field, from a node
// with no update waiting.
//
// This file covers PARSING only, including the shape of `sig` but never its validity.
// Whether a signature actually verifies is test_manifest_sig, graded against the shared
// golden vectors. The split is deliberate: a parser that accepts 128 lowercase hex chars
// and a verifier that checks a curve equation fail for different reasons and should say
// so separately.

using namespace soundings;

void setUp() {}
void tearDown() {}

// The real signature over the shared literal, by the published TEST key
// (contracts/vectors/manifest-sig-v1.json). Real rather than 128 arbitrary hex chars so
// that kGood is a manifest which both parses AND verifies — a fixture that parsed but
// could never verify would quietly stop being a positive control the day someone added
// verification to this file.
#define SIG_LINE \
    "sig: 6039b503a526fc8ee5a574da60a0e09f3cae0ab8625376564540af9d9882785e" \
    "4dccdd86bcef4c029d475b5022ef9680baa06909554da402e0738fab5d55310b\n"

// The shared literal from the contract. SHA-256 of the four ASCII bytes "test", which is
// reproducible with `printf 'test' | sha256sum` rather than trusted — the first draft of
// that document carried an invented hash that looked exactly as plausible.
static const char* kGood =
    "version: 261\n"
    "size: 4\n"
    "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
    "file: t.bin\n"
    SIG_LINE;

static const uint8_t kGoodSha[32] = {
    0x9f,0x86,0xd0,0x81,0x88,0x4c,0x7d,0x65,0x9a,0x2f,0xea,0xa0,0xc5,0x5a,0xd0,0x15,
    0xa3,0xbf,0x4f,0x1b,0x2b,0x0b,0x82,0x2c,0xd1,0x5d,0x6c,0x15,0xb0,0xf0,0x0a,0x08,
};

static bool parseStr(const char* s, FwManifest& out) {
    return parseManifest(s, strlen(s), out);
}

// Every rejection test calls this afterwards, in the same test. A parser that rejects
// EVERYTHING satisfies every refusal assertion below — this is the positive control that
// makes them mean something, and it is the defect this repo has shipped in five
// consecutive tasks.
static void assertGoodStillParses() {
    FwManifest m;
    TEST_ASSERT_TRUE(parseStr(kGood, m));
    TEST_ASSERT_EQUAL_UINT16(261, m.version);
}

// ---- The happy path --------------------------------------------------------

void test_parses_the_shared_literal() {
    FwManifest m;
    TEST_ASSERT_TRUE(parseStr(kGood, m));
    TEST_ASSERT_EQUAL_UINT16(261, m.version);
    TEST_ASSERT_EQUAL_UINT32(4, m.size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(kGoodSha, m.sha256, 32);
    TEST_ASSERT_EQUAL_STRING("t.bin", m.file);
    TEST_ASSERT_EQUAL_HEX8(0x60, m.sig[0]);
    TEST_ASSERT_EQUAL_HEX8(0x0b, m.sig[63]);
}

void test_key_order_does_not_matter() {
    FwManifest m;
    TEST_ASSERT_TRUE(parseStr(
        SIG_LINE
        "file: t.bin\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "size: 4\n"
        "version: 261\n", m));
    TEST_ASSERT_EQUAL_UINT16(261, m.version);
    TEST_ASSERT_EQUAL_STRING("t.bin", m.file);
}

void test_unknown_keys_are_ignored() {
    // The only forward compatibility this format has, and it is one-way: a v3 key
    // carrying a REQUIREMENT would be silently skipped, so anything mandatory needs a
    // version bump. `sig` is exactly that case, which is why it arrived as v2 rather than
    // as a new key on v1 (DEC-013). Note `signature:` below is NOT `sig:` — a near-miss
    // key is ignored like any other, and does not satisfy the requirement.
    FwManifest m;
    TEST_ASSERT_TRUE(parseStr(
        "version: 261\n"
        "released_by: eric\n"
        "size: 4\n"
        "signature: not-the-key-you-are-looking-for\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n"
        SIG_LINE, m));
    TEST_ASSERT_EQUAL_UINT16(261, m.version);
}

void test_blank_lines_and_comments_are_ignored() {
    FwManifest m;
    TEST_ASSERT_TRUE(parseStr(
        "# built by publish_firmware.py\n"
        "\n"
        "version: 261\n"
        "\n"
        "size: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n"
        SIG_LINE
        "\n", m));
    TEST_ASSERT_EQUAL_UINT16(261, m.version);
}

void test_trailing_newline_is_optional() {
    FwManifest m;
    TEST_ASSERT_TRUE(parseStr(
        "version: 261\n"
        "size: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        SIG_LINE
        "file: t.bin", m));
    TEST_ASSERT_EQUAL_STRING("t.bin", m.file);
}

void test_crlf_line_endings_parse() {
    // The manifest is written on a Linux box today, but it arrives over HTTP and nothing
    // stops a future tool or an editor from using CRLF. A stray \r would otherwise land
    // inside the filename and turn every fetch into a 404 — silently, since a failed
    // fetch just sleeps. Since 3.9e it would also land inside the signature, where the
    // symptom is a refusal that looks like a forged manifest.
    FwManifest m;
    TEST_ASSERT_TRUE(parseStr(
        "version: 261\r\n"
        "size: 4\r\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\r\n"
        "file: t.bin\r\n"
        "sig: 6039b503a526fc8ee5a574da60a0e09f3cae0ab8625376564540af9d9882785e"
        "4dccdd86bcef4c029d475b5022ef9680baa06909554da402e0738fab5d55310b\r\n", m));
    TEST_ASSERT_EQUAL_STRING("t.bin", m.file);
    TEST_ASSERT_EQUAL_HEX8(0x0b, m.sig[63]);
}

// ---- Missing required keys -------------------------------------------------

void test_a_missing_version_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "size: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n"
        SIG_LINE, m));
    assertGoodStillParses();
}

void test_a_missing_size_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n"
        SIG_LINE, m));
    assertGoodStillParses();
}

void test_a_missing_sha_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr("version: 261\nsize: 4\nfile: t.bin\n" SIG_LINE, m));
    assertGoodStillParses();
}

void test_a_missing_file_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\n"
        "size: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        SIG_LINE, m));
    assertGoodStillParses();
}

void test_a_missing_sig_is_rejected() {
    // The v1 manifest, unchanged and unsigned. It was valid before 3.9e and must not be
    // now — this is the assertion that makes signing mandatory rather than advisory.
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\n"
        "size: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n", m));
    assertGoodStillParses();
}

void test_an_empty_manifest_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr("", m));
    assertGoodStillParses();
}

// ---- The hash --------------------------------------------------------------

void test_a_short_hash_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 4\nsha256: 9f86d081\nfile: t.bin\n" SIG_LINE, m));
    assertGoodStillParses();
}

void test_an_uppercase_hash_is_rejected() {
    // Lowercase only, per the contract. Accepting both would be two implementations
    // agreeing to differ, and the hash is the one field where "close enough" means
    // flashing an image nobody checked.
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 4\n"
        "sha256: 9F86D081884C7D659A2FEAA0C55AD015A3BF4F1B2B0B822CD15D6C15B0F00A08\n"
        "file: t.bin\n" SIG_LINE, m));
    assertGoodStillParses();
}

void test_a_non_hex_hash_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 4\n"
        "sha256: zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz\n"
        "file: t.bin\n" SIG_LINE, m));
    assertGoodStillParses();
}

// ---- The signature, as a FIELD ---------------------------------------------
// Shape only. Whether it verifies is test_manifest_sig's job.

void test_a_short_sig_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n"
        "sig: 6039b503\n", m));
    assertGoodStillParses();
}

void test_an_uppercase_sig_is_rejected() {
    // Same rule as sha256, and for the same reason: one canonical spelling, so two
    // implementations cannot agree to differ about a field that gates flashing.
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n"
        "sig: 6039B503A526FC8EE5A574DA60A0E09F3CAE0AB8625376564540AF9D9882785E"
        "4DCCDD86BCEF4C029D475B5022EF9680BAA06909554DA402E0738FAB5D55310B\n", m));
    assertGoodStillParses();
}

void test_a_non_hex_sig_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n"
        "sig: zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
        "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz\n", m));
    assertGoodStillParses();
}

// ---- The filename ----------------------------------------------------------

void test_a_filename_containing_a_slash_is_rejected() {
    // `file` resolves against a fixed base URL. A manifest that could name a path is a
    // manifest that can point a field node at an arbitrary address, and the node has no
    // rollback.
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: sub/t.bin\n" SIG_LINE, m));
    assertGoodStillParses();
}

void test_a_filename_containing_dotdot_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: ..bin\n" SIG_LINE, m));
    assertGoodStillParses();
}

void test_an_absolute_url_as_filename_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: http://elsewhere/evil.bin\n" SIG_LINE, m));
    assertGoodStillParses();
}

void test_an_empty_filename_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: \n" SIG_LINE, m));
    assertGoodStillParses();
}

// ---- Numbers ---------------------------------------------------------------

void test_a_version_beyond_u16_is_rejected() {
    // fw_version is u16 on the wire (contracts/packet-v1.md). A manifest naming 65536
    // would wrap to 0 and the node would compare against a version no image reports.
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 65536\nsize: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n" SIG_LINE, m));
    assertGoodStillParses();

    // ...and the largest legal value IS accepted, so this pins the boundary rather than
    // a general dislike of large numbers.
    FwManifest ok;
    TEST_ASSERT_TRUE(parseStr(
        "version: 65535\nsize: 4\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n" SIG_LINE, ok));
    TEST_ASSERT_EQUAL_UINT16(65535, ok.version);
}

void test_a_zero_size_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: 0\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n" SIG_LINE, m));
    assertGoodStillParses();
}

void test_a_non_numeric_size_is_rejected() {
    FwManifest m;
    TEST_ASSERT_FALSE(parseStr(
        "version: 261\nsize: lots\n"
        "sha256: 9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08\n"
        "file: t.bin\n" SIG_LINE, m));
    assertGoodStillParses();
}

// ---- Bounds ----------------------------------------------------------------

void test_the_signed_manifest_fits_the_byte_cap_with_room() {
    // The cap did not move when `sig` arrived, so this is the test that says the new
    // field actually fits rather than that someone widened the bound to make it fit.
    // Every field at its maximum width, which is the worst case a real publish can make.
    char worst[kMaxManifestBytes + 128] = {};
    size_t at = 0;
    at += (size_t)snprintf(worst + at, sizeof(worst) - at,
                           "version: 65535\nsize: 4294967295\nsha256: ");
    for (int i = 0; i < 64; ++i) worst[at++] = 'a';
    at += (size_t)snprintf(worst + at, sizeof(worst) - at, "\nfile: ");
    for (size_t i = 0; i < kMaxManifestFileLen; ++i) worst[at++] = 'x';
    at += (size_t)snprintf(worst + at, sizeof(worst) - at, "\nsig: ");
    for (int i = 0; i < 128; ++i) worst[at++] = 'b';
    worst[at++] = '\n';

    TEST_ASSERT_TRUE_MESSAGE(at <= kMaxManifestBytes,
        "a maximum-width signed manifest no longer fits the 512-byte cap");

    FwManifest m;
    TEST_ASSERT_TRUE(parseManifest(worst, at, m));
    TEST_ASSERT_EQUAL_UINT16(65535, m.version);
}

void test_a_manifest_over_the_byte_cap_is_rejected() {
    // The node reads this over HTTP before it can trust anything about it. An unbounded
    // read is a remote party choosing how much of a battery device's RAM to consume.
    char big[kMaxManifestBytes + 64];
    memset(big, '#', sizeof(big));       // all comment lines' worth of filler
    big[sizeof(big) - 1] = '\0';
    FwManifest m;
    TEST_ASSERT_FALSE(parseManifest(big, sizeof(big) - 1, m));
    assertGoodStillParses();
}

void test_a_manifest_over_the_line_cap_is_rejected() {
    char many[kMaxManifestBytes] = {};
    size_t at = 0;
    for (int i = 0; i < (int)kMaxManifestLines + 5 && at + 3 < sizeof(many); ++i) {
        many[at++] = '#';
        many[at++] = '\n';
    }
    many[at] = '\0';
    FwManifest m;
    TEST_ASSERT_FALSE(parseManifest(many, at, m));
    assertGoodStillParses();
}

void test_a_manifest_at_exactly_the_byte_cap_is_accepted() {
    // The boundary from the legal side. Without this, the two bounds tests above pass
    // against a parser that rejects everything longer than the shared literal.
    char buf[kMaxManifestBytes + 1] = {};
    const size_t bodyLen = strlen(kGood);
    memcpy(buf, kGood, bodyLen);
    // Pad with spaces: they form one trailing line with no ": " separator, which is
    // skipped, so this exercises the BYTE cap without also pushing the line count up.
    memset(buf + bodyLen, ' ', kMaxManifestBytes - bodyLen);

    FwManifest m;
    TEST_ASSERT_TRUE(parseManifest(buf, kMaxManifestBytes, m));
    TEST_ASSERT_EQUAL_UINT16(261, m.version);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_the_shared_literal);
    RUN_TEST(test_key_order_does_not_matter);
    RUN_TEST(test_unknown_keys_are_ignored);
    RUN_TEST(test_blank_lines_and_comments_are_ignored);
    RUN_TEST(test_trailing_newline_is_optional);
    RUN_TEST(test_crlf_line_endings_parse);
    RUN_TEST(test_a_missing_version_is_rejected);
    RUN_TEST(test_a_missing_size_is_rejected);
    RUN_TEST(test_a_missing_sha_is_rejected);
    RUN_TEST(test_a_missing_file_is_rejected);
    RUN_TEST(test_a_missing_sig_is_rejected);
    RUN_TEST(test_an_empty_manifest_is_rejected);
    RUN_TEST(test_a_short_hash_is_rejected);
    RUN_TEST(test_an_uppercase_hash_is_rejected);
    RUN_TEST(test_a_non_hex_hash_is_rejected);
    RUN_TEST(test_a_short_sig_is_rejected);
    RUN_TEST(test_an_uppercase_sig_is_rejected);
    RUN_TEST(test_a_non_hex_sig_is_rejected);
    RUN_TEST(test_a_filename_containing_a_slash_is_rejected);
    RUN_TEST(test_a_filename_containing_dotdot_is_rejected);
    RUN_TEST(test_an_absolute_url_as_filename_is_rejected);
    RUN_TEST(test_an_empty_filename_is_rejected);
    RUN_TEST(test_a_version_beyond_u16_is_rejected);
    RUN_TEST(test_a_zero_size_is_rejected);
    RUN_TEST(test_a_non_numeric_size_is_rejected);
    RUN_TEST(test_the_signed_manifest_fits_the_byte_cap_with_room);
    RUN_TEST(test_a_manifest_over_the_byte_cap_is_rejected);
    RUN_TEST(test_a_manifest_over_the_line_cap_is_rejected);
    RUN_TEST(test_a_manifest_at_exactly_the_byte_cap_is_accepted);
    return UNITY_END();
}
