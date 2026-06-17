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


class Gateway:
    def __init__(self, source: IPacketSource, publish: Publisher, *, clock: Callable[[], float] = time.time):
        self.source = source
        self.publish = publish
        self.clock = clock
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
        log.info("gateway done: %d decoded, %d dropped", self.decoded, self.dropped)
        return self.decoded
