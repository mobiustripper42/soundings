# contracts/

Soundings has **two** contracts, and they meet different neighbours.

- **`packet-v1.md`** crosses the *radio*, between this project's own two halves — the C++
  serializer and the Python parser. Byte-exact, 46 bytes at most, pinned by shared golden
  vectors so the two sides can never drift.
- **`publish-v1.md`** crosses the *broker*, to Poop Deck — a different repo that cannot
  import ours and will be written against this document rather than against our code.
  JSON, no byte pressure, and it carries what the packet has no room for: names, a
  location, derived values, and a timestamp the node cannot produce.

A change to the first is a change to a wire format two implementations here must agree on.
A change to the second is a change someone else has to be told about.

| Path | What |
|------|------|
| [`publish-v1.md`](publish-v1.md) | The **broker** contract: the JSON document published per node wake, its `v` gate, the `(node_id, seq)` natural key, and what Poop Deck still needs on its side. |
| [`packet-v1-explained.md`](packet-v1-explained.md) | **Start here** — plain-English "packets for dummies" walkthrough, no jargon. |
| [`packet-v1.md`](packet-v1.md) | The wire contract: layout, channel registry, CRC, fault/manifest semantics, versioning (resolves §12 D2). |
| [`vectors/packet-v1.json`](vectors/packet-v1.json) | Language-neutral golden vectors (decoded fields ↔ exact hex). Both implementations round-trip every case. |
| [`tools/gen_vectors.py`](tools/gen_vectors.py) | Fixture authoring tool — **not** the parser. Regenerates the vectors; re-run when the channel registry grows. |

Adding a sensor: assign the next free channel bit + a registry row in
`gen_vectors.py`, run `python3 contracts/tools/gen_vectors.py`, commit the
regenerated JSON. See `packet-v1.md` § Versioning.
