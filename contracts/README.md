# contracts/

The binary packet schema — the single source of truth both the node firmware
(C++ serializer) and the gateway (Python parser) build against. Pinned by shared
golden vectors so the two sides can never drift.

| Path | What |
|------|------|
| [`packet-v1-explained.md`](packet-v1-explained.md) | **Start here** — plain-English "packets for dummies" walkthrough, no jargon. |
| [`packet-v1.md`](packet-v1.md) | The wire contract: layout, channel registry, CRC, fault/manifest semantics, versioning (resolves §12 D2). |
| [`vectors/packet-v1.json`](vectors/packet-v1.json) | Language-neutral golden vectors (decoded fields ↔ exact hex). Both implementations round-trip every case. |
| [`tools/gen_vectors.py`](tools/gen_vectors.py) | Fixture authoring tool — **not** the parser. Regenerates the vectors; re-run when the channel registry grows. |

Adding a sensor: assign the next free channel bit + a registry row in
`gen_vectors.py`, run `python3 contracts/tools/gen_vectors.py`, commit the
regenerated JSON. See `packet-v1.md` § Versioning.
