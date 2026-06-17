"""IPacketSource — the gateway's radio seam.

A packet source yields raw packet frames exactly as they'd arrive off the LoRa
radio. The gateway loops over a source, decodes each frame, and never knows or
cares where the bytes came from. In simulation the source is fed by the synthetic
fleet emitter; at the bench (Phase 5) a real SX1262 driver implements the same
interface — swapping one for the other is the whole point of the software-first
build (CLAUDE.md "adapters everywhere").
"""
from __future__ import annotations

from abc import ABC, abstractmethod
from collections.abc import Iterable, Iterator


class IPacketSource(ABC):
    """Yields raw packet frames (bytes). Iteration ends when the source is done
    (sim run complete); a real radio source would block/yield indefinitely."""

    @abstractmethod
    def __iter__(self) -> Iterator[bytes]: ...


class FakePacketSource(IPacketSource):
    """A source backed by a fixed iterable of pre-built frames — for tests and
    replay of captured packets. The fleet emitter (emitter.py) is the live sim
    source; this one is for deterministic unit tests."""

    def __init__(self, packets: Iterable[bytes]):
        self._packets = list(packets)

    def __iter__(self) -> Iterator[bytes]:
        return iter(self._packets)
