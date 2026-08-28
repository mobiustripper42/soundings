#!/usr/bin/env python3
"""Golden-vector generator for the signed firmware manifest (manifest v2).

FIXTURE AUTHORING ONLY. Like gen_vectors.py, this is deliberately not one of the
three implementations it grades: the node's C++ verifier, the daemon's Python
verifier, and the publish tool are each checked against the JSON this emits, so
none of them is checked against another's idea of what is correct.

Run from the repo root:  python3 contracts/tools/gen_sig_vectors.py
Rewrites contracts/vectors/manifest-sig-v1.json in place.

Requires `cryptography` (the same dependency the gateway takes for signing).

⚠ THE PRIVATE KEY IN THE EMITTED FILE IS PUBLISHED ON PURPOSE. It is a test key
and it is derived from a fixed ASCII phrase below, so anyone can regenerate the
whole file and check it rather than trust it. It must never sign a real release;
the production key is offline and matches `*ota-signing-key*` in .gitignore.
"""
import hashlib
import json
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import (
    Encoding, NoEncryption, PrivateFormat, PublicFormat,
)

# Deterministic so re-running does not churn the file, and reproducible in one
# command so the seed is checkable rather than magic:
#     printf 'soundings manifest-sig-v1 TEST KEY -- not for production' | sha256sum
SEED_PHRASE = b"soundings manifest-sig-v1 TEST KEY -- not for production"
SEED = hashlib.sha256(SEED_PHRASE).digest()

# SHA-256 of the four ASCII bytes `test`, the same reproducible value manifest v1
# already uses as its shared literal:  printf 'test' | sha256sum
SHA_OF_TEST = "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"

REQUIRED_ORDER = ("version", "size", "sha256", "file")


def signed_message(version: int, size: int, sha256: str, file: str) -> bytes:
    """The exact bytes that get signed — a CANONICAL RECONSTRUCTION, not the file.

    Built from the four parsed values in fixed order, so line order, comments,
    blank lines, trailing spaces and CRLF cannot change it. That is the point:
    three independent parsers cannot disagree about which bytes were signed if
    none of them ever looks at the bytes as they arrived.
    """
    return (
        f"version: {version}\n"
        f"size: {size}\n"
        f"sha256: {sha256}\n"
        f"file: {file}\n"
    ).encode()


def render(fields: dict, *, sig: str | None, order=REQUIRED_ORDER,
           extra_lines: tuple = (), line_end: str = "\n") -> str:
    """Render manifest text. Deliberately flexible so a vector can vary the
    layout — order, comments, CRLF — while the signed message stays identical."""
    lines = [f"{k}: {fields[k]}" for k in order]
    if sig is not None:
        lines.append(f"sig: {sig}")
    lines.extend(extra_lines)
    return "".join(line + line_end for line in lines)


def main() -> int:
    key = Ed25519PrivateKey.from_private_bytes(SEED)
    pub = key.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw)
    priv = key.private_bytes(Encoding.Raw, PrivateFormat.Raw, NoEncryption())

    good = {"version": 261, "size": 4, "sha256": SHA_OF_TEST, "file": "t.bin"}
    msg = signed_message(**good)
    sig = key.sign(msg).hex()

    # A signature that is valid Ed25519 over a DIFFERENT manifest. This is the
    # case a naive implementation passes: the signature verifies against its own
    # message, so anything that forgets to bind it to THIS manifest accepts it.
    other = dict(good, version=262, file="t2.bin")
    other_sig = key.sign(signed_message(**other)).hex()

    vectors = [
        {
            "name": "valid",
            "description": "The shared literal, signed. Parses and verifies.",
            "manifest": render(good, sig=sig),
            "expect_parse": True,
            "expect_verify": True,
        },
        {
            "name": "layout_varies_signature_holds",
            "description": (
                "Same four values, reordered, with a comment, a blank line, "
                "trailing spaces and CRLF endings. The signature is unchanged "
                "and still verifies — this is what canonical reconstruction buys."
            ),
            "manifest": render(
                good, sig=sig,
                order=("file", "sha256", "size", "version"),
                extra_lines=("# published by hand", "", "size: 4   "),
                line_end="\r\n",
            ),
            "expect_parse": True,
            "expect_verify": True,
        },
        {
            "name": "tampered_sha256",
            "description": "sha256 edited after signing. The attack the whole field exists to stop.",
            "manifest": render(dict(good, sha256="0" * 64), sig=sig),
            "expect_parse": True,
            "expect_verify": False,
        },
        {
            "name": "tampered_file",
            "description": "file edited after signing — points the node at another image.",
            "manifest": render(dict(good, file="evil.bin"), sig=sig),
            "expect_parse": True,
            "expect_verify": False,
        },
        {
            "name": "tampered_version",
            "description": "version edited after signing.",
            "manifest": render(dict(good, version=262), sig=sig),
            "expect_parse": True,
            "expect_verify": False,
        },
        {
            "name": "tampered_size",
            "description": "size edited after signing.",
            "manifest": render(dict(good, size=5), sig=sig),
            "expect_parse": True,
            "expect_verify": False,
        },
        {
            "name": "signature_over_another_manifest",
            "description": (
                "A genuine signature by the real key, over a DIFFERENT manifest. "
                "Verifies against its own message and must not verify against this one."
            ),
            "manifest": render(good, sig=other_sig),
            "expect_parse": True,
            "expect_verify": False,
        },
        {
            "name": "missing_sig",
            "description": "No sig line. Required in v2, so the parse fails outright.",
            "manifest": render(good, sig=None),
            "expect_parse": False,
            "expect_verify": False,
        },
        {
            "name": "sig_wrong_length",
            "description": "127 hex chars. Exactly 128 or nothing.",
            "manifest": render(good, sig="a" * 127),
            "expect_parse": False,
            "expect_verify": False,
        },
        {
            "name": "sig_not_hex",
            "description": "Right length, non-hex characters.",
            "manifest": render(good, sig="z" * 128),
            "expect_parse": False,
            "expect_verify": False,
        },
        {
            "name": "sig_uppercase",
            "description": (
                "The real signature uppercased. Lowercase only, matching sha256 — "
                "a parser that accepts both is two implementations agreeing to differ."
            ),
            "manifest": render(good, sig=sig.upper()),
            "expect_parse": False,
            "expect_verify": False,
        },
        {
            "name": "oversized",
            "description": (
                "Padded past the 512-byte cap with comment lines. Bounds are checked "
                "before anything is interpreted, so this dies before the signature."
            ),
            "manifest": render(good, sig=sig) + "# pad\n" * 60,
            "expect_parse": False,
            "expect_verify": False,
        },
    ]

    out = {
        "format": "soundings-manifest-sig",
        "manifest_ver": 2,
        "algorithm": {
            "name": "Ed25519",
            "signed_message": (
                "A canonical reconstruction from the four parsed values, in the "
                "fixed order version, size, sha256, file — each rendered as "
                "'<key>: <value>' and terminated by a single \\n. NOT the bytes "
                "of the file as received."
            ),
            "sig_encoding": "128 lowercase hex chars (the 64-byte detached signature)",
        },
        "test_key": {
            "warning": (
                "TEST KEY, PUBLISHED DELIBERATELY. Never signs a real release. "
                "Regenerate: printf '%s' | sha256sum" % SEED_PHRASE.decode()
            ),
            "seed_phrase": SEED_PHRASE.decode(),
            "private_key_hex": priv.hex(),
            "public_key_hex": pub.hex(),
        },
        "vectors": vectors,
    }

    dest = Path(__file__).resolve().parent.parent / "vectors" / "manifest-sig-v1.json"
    dest.write_text(json.dumps(out, indent=2) + "\n")
    print(f"wrote {len(vectors)} vectors -> {dest}")
    print(f"  public key {pub.hex()}")
    for v in vectors:
        print(f"  {v['name']:34s} parse={v['expect_parse']!s:5s} verify={v['expect_verify']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
