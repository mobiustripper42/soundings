"""Gateway decode loop — raw frames in, decoded readings out to the broker.

Pulls frames from an IPacketSource, decodes each via the shared parser, stamps a
receipt time (field nodes have no RTC — the gateway owns the timestamp), and hands
the JSON-friendly reading to a publisher. The publisher is injected (MQTT in
production, a list in tests) so this loop has no broker dependency and stays unit-
testable. Malformed frames are already logged + dropped by decode(); we just count.
"""
from __future__ import annotations

import logging
import time
from collections.abc import Callable

from .packet import decode
from .source import IPacketSource

log = logging.getLogger(__name__)

# Takes a JSON-friendly reading dict (Reading.to_dict() + received_at).
Publisher = Callable[[dict], None]

# Called with the same reading, while the node's receive window is still open. What
# it decides to say (if anything) is its own business; see contracts/downlink-v1.md.
Responder = Callable[[dict], None]


class Gateway:
    def __init__(self, source: IPacketSource, publish: Publisher, *,
                 clock: Callable[[], float] = time.time,
                 respond: Responder | None = None):
        self.source = source
        self.publish = publish
        self.clock = clock
        # Optional, and the default of None is the whole sim path: nothing that
        # replays synthetic packets has a node listening for an answer.
        self.respond = respond
        self.decoded = 0
        self.dropped = 0

    def run(self) -> int:
        """Drain the source. Returns the count of successfully decoded readings."""
        for raw in self.source:
            reading = decode(raw)
            if reading is None:
                self.dropped += 1
                continue
            msg = reading.to_dict()
            msg["received_at"] = self.clock()
            self.publish(msg)
            self.decoded += 1
            # After publishing, never instead of it: a reading that triggers a reply
            # is still a reading. And only for frames that DECODED — a corrupt frame's
            # node_id byte just failed its own checksum, so it is not an address.
            if self.respond is not None:
                self.respond(msg)
        log.info("gateway done: %d decoded, %d dropped", self.decoded, self.dropped)
        return self.decoded
