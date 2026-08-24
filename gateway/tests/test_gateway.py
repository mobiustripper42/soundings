"""The gateway decode loop's responder hook — the daemon's half of the receive window.

`contracts/downlink-v1.md`: the node holds a short window immediately after each
transmit, and that window is the only moment it can be told anything. The daemon is
the only thing that knows whether there is anything to say, so the decision lives
here and the gateway board just relays (DEC-010).

The hook is a plain callable rather than a policy object because everything
interesting about *what* to send is a pure function of the reading plus a manifest,
and pure functions are what this project prefers to test.
"""
from __future__ import annotations

import json
import pathlib

from soundings_gateway.gateway import Gateway
from soundings_gateway.source import FakePacketSource

VECTORS = json.loads(
    (pathlib.Path(__file__).resolve().parents[2] / "contracts/vectors/packet-v1.json").read_text()
)
GOOD = bytes.fromhex(VECTORS["vectors"][0]["expected"]["hex"])

# A frame that decodes to nothing: right length, wrong CRC. The last two bytes are
# the CRC, so flipping one is the cheapest way to make a real packet undecodable.
CORRUPT = GOOD[:-1] + bytes([GOOD[-1] ^ 0xFF])


def test_responder_is_called_once_per_decoded_reading():
    seen: list[dict] = []
    gw = Gateway(FakePacketSource([GOOD, GOOD]), lambda _m: None, respond=seen.append)
    assert gw.run() == 2
    assert len(seen) == 2


def test_the_responder_receives_the_decoded_reading_not_the_raw_frame():
    """It needs node_id and fw_version to decide anything, so it gets the decoded
    dict — the same one the publisher sees."""
    seen: list[dict] = []
    Gateway(FakePacketSource([GOOD]), lambda _m: None, respond=seen.append).run()
    assert seen[0]["node_id"] == VECTORS["vectors"][0]["fields"]["node_id"]
    assert seen[0]["fw_version"] == VECTORS["vectors"][0]["fields"]["fw_version"]


def test_a_frame_that_does_not_decode_gets_no_reply():
    """Silence is a legal answer and a corrupt frame is not a node asking a
    question — replying would mean addressing a downlink from a node_id byte that
    just failed its own checksum."""
    seen: list[dict] = []
    gw = Gateway(FakePacketSource([CORRUPT]), lambda _m: None, respond=seen.append)
    assert gw.run() == 0
    assert gw.dropped == 1
    assert seen == []
    # Positive control, same test: the identical setup with a GOOD frame does reply.
    # Without it this passes against a Gateway that never calls respond at all.
    ok: list[dict] = []
    assert Gateway(FakePacketSource([GOOD]), lambda _m: None, respond=ok.append).run() == 1
    assert len(ok) == 1


def test_a_gateway_with_no_responder_still_decodes_and_publishes():
    """The hook is optional. The sim spine and every existing caller pass no
    responder, and must be untouched by its existence."""
    published: list[dict] = []
    assert Gateway(FakePacketSource([GOOD, GOOD]), published.append).run() == 2
    assert len(published) == 2


def test_publishing_happens_even_when_there_is_a_responder():
    """The reply is additional to the record, never instead of it. A reading that
    triggered an update flag still has to reach the broker."""
    published: list[dict] = []
    replied: list[dict] = []
    Gateway(FakePacketSource([GOOD]), published.append, respond=replied.append).run()
    assert len(published) == 1 and len(replied) == 1
