"""IPacketSource — the gateway's radio seam.

A packet source yields raw packet frames exactly as they'd arrive off the LoRa
radio. The gateway loops over a source, decodes each frame, and never knows or
cares where the bytes came from. In simulation the source is fed by the synthetic
fleet emitter; at the bench (Phase 5) a real SX1262 driver implements the same
interface — swapping one for the other is the whole point of the software-first
build (CLAUDE.md "adapters everywhere").
"""
from __future__ import annotations

import logging
from abc import ABC, abstractmethod
from collections.abc import Iterable, Iterator
from typing import Protocol

from .framing import SerialFramer

log = logging.getLogger(__name__)


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


class ByteStream(Protocol):
    """Anything that hands over bytes when asked. `serial.Serial` satisfies it.

    Declared structurally, and deliberately: it keeps pyserial out of this module
    entirely, so the framing and the drop-don't-raise behaviour are testable with a
    scripted stream and the gateway's dependency list is unchanged. The real port
    gets constructed at the edge, in whatever wires the daemon up.
    """

    def read(self, size: int) -> bytes: ...


class SerialPacketSource(IPacketSource):
    """Packet frames off a serial port (contracts/serial-framing-v1.md).

    The gateway radio board is a dumb bridge: it receives a LoRa packet, wraps it in
    the serial envelope, and writes it to USB. This unwraps it. Everything about
    *which* bytes are real is left to the packet CRC downstream.

    Two conventions the stream has to meet, because they are the only way an
    iterator over a live port can terminate at all:

    - **`b""` means "nothing right now"**, and iteration continues. That is what a
      real port returns on a read timeout, and treating it as end-of-stream would
      stop the daemon the first quiet minute. It follows that the stream must
      BLOCK — a non-blocking port returning `b""` immediately would spin this loop
      hot, and the pacing has to come from the port's own timeout.
    - **`EOFError` means the source is finished**, and iteration stops. A real
      serial port never raises it on a timeout, so nothing in production trips this;
      it exists so a test can script a finite stream without a test-only flag on the
      production class.
    """

    def __init__(self, stream: ByteStream, *, chunk_size: int = 64):
        self.stream = stream
        self.chunk_size = chunk_size
        self.framer = SerialFramer()
        self.frames = 0

    def __iter__(self) -> Iterator[bytes]:
        while True:
            try:
                chunk = self.stream.read(self.chunk_size)
            except EOFError:
                log.info("serial source finished: %d frames, %d bytes discarded",
                         self.frames, self.framer.discarded)
                return
            if not chunk:
                continue   # read timeout — the port is quiet, not closed
            for payload in self.framer.feed(chunk):
                self.frames += 1
                yield payload
