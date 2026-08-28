"""The publish envelope — what soundings hands to Poop Deck.

Soundings has two contracts and they are deliberately unalike. `contracts/packet-v1.md`
is what crosses the radio: 46 bytes, a 16-channel ceiling, channel names that are warts.
`contracts/publish-v1.md` — this one — is what crosses the broker, where none of that
scarcity applies, so it carries names, a location, derived values, and a timestamp the
node cannot produce because field nodes have no RTC.

This module builds that document. It does not send it; the transport belongs to the
caller, so the shape stays testable without a broker.
"""
from __future__ import annotations

import logging
import time
from datetime import datetime, timezone

from . import derive

log = logging.getLogger(__name__)

__all__ = ["SCHEMA_V", "iso_utc", "envelope"]

# Bumped when a field changes meaning, or a REQUIRED field is added or removed. Adding an
# optional field does not bump it — a consumer that ignores unknown keys keeps working,
# which is why `v` is a gate on the consumer rather than a negotiation between us.
SCHEMA_V = 1


def iso_utc(epoch_s: float) -> str:
    """Epoch seconds → `2026-08-13T04:12:00Z`.

    `…Z`, not `+00:00`. Both are valid ISO-8601, but Poop Deck's only implemented
    producer emits the Z form and its ingest hands the string straight to Postgres for a
    TIMESTAMPTZ column. Matching the existing producer costs nothing and removes a class
    of parser surprise that would only show up in their logs, not ours.
    """
    return datetime.fromtimestamp(epoch_s, tz=timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def envelope(msg: dict, cfg, derived: dict | None = None) -> dict | None:
    """Build the publishable document for one decoded reading.

    Returns None if the reading has no node id — there is nothing to key on and no topic
    to publish to. Everything else is survivable and still gets published: a node absent
    from the map, no channels, a failed battery read. This runs in a long-lived daemon,
    so it logs and degrades rather than raising.

    `derived` may be passed pre-computed; otherwise it is computed here from the same
    derivation the live-scalar branch uses, so the stored document and the dashboard can
    never disagree about a number.
    """
    try:
        node_id = msg.get("node_id")
        if node_id is None:
            log.warning("reading with no node_id, not publishable")
            return None

        # seq is the other half of the natural key (node_id, seq). Without it the document
        # cannot be stored idempotently — and if Poop Deck's column is NOT NULL, as the
        # natural-key index on their irrigation table suggests it will be, it cannot be
        # stored at all. Publishing it anyway would look like success on this side and be
        # a dropped row on theirs, which is the worst available outcome. Note `is None`
        # and not a truthiness test: seq 0 is the first packet after a power cycle, and a
        # falsy check would discard exactly the reading that marks a node restart.
        if msg.get("seq") is None:
            log.warning("node %s: reading with no seq, cannot be keyed — not published", node_id)
            return None

        received_at = msg.get("received_at")
        if received_at is None:
            # The gateway owns the timestamp (field nodes have no RTC). A document with
            # no time is not storable, so stamping now beats publishing without one.
            received_at = time.time()

        env: dict = {
            "v": SCHEMA_V,
            "node_id": node_id,
            "seq": msg.get("seq"),
            "ts": iso_utc(received_at),
            "fw_version": msg.get("fw_version"),
            # Always present, even when every channel faulted: a node whose sensors have
            # all failed is still worth recording as alive. 0 means the read itself failed.
            "battery_mv": msg.get("battery_mv", 0),
            "channels": list(msg.get("channels", [])),
            "derived": {},
        }

        node = cfg.nodes.get(node_id)
        if node is not None:
            # Gateway-side enrichment (D7 / DEC-004). Omitted rather than guessed for an
            # unmapped node — but the reading still goes out, because a provisioning gap
            # is not a reason to throw a measurement away.
            env["location"] = node.location

        env["derived"] = derived if derived is not None else _derived_map(msg, cfg)
        return env
    except Exception:  # noqa: BLE001 — the daemon outlives one bad reading
        log.exception("failed to build publish envelope (node %s)", msg.get("node_id"))
        return None


def _derived_map(msg: dict, cfg) -> dict:
    """The derived values as a flat map, keyed by metric name.

    Reuses derive_reading so there is exactly one derivation in the codebase: the scalars
    a dashboard subscribes to and the numbers Poop Deck stores come from the same call,
    and cannot drift into disagreeing about the same tank. The role dispatch lives in
    derive.py, so a bed node lands here without this function knowing bed nodes exist.

    Keys are OMITTED when not derivable — never nulled, never zeroed. A null reads as "we
    measured nothing"; a zero reads as a real measurement of zero. Absence is the only
    encoding that says "could not be computed this cycle".
    """
    out: dict = {}
    for topic, payload in derive.derive_reading(msg, cfg):
        # derive.metric_name, not a split here: that module owns the topic shape, so the
        # parse belongs next to the construction rather than guessing at it from outside.
        out[derive.metric_name(topic)] = float(payload)
    return out
