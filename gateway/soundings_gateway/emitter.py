"""Synthetic field-node fleet — the sim's stand-in for real LoRa nodes.

Models a slow soil drydown (Watermark resistance climbing as the soil dries),
encodes it into real packet v1 frames via the shared serializer, and emits them
on a jittered transmit schedule. This is net-new sim harness: it lets the whole
pipeline run — and the dashboard visibly move — with zero hardware.

The drydown is the visible curve the spine exists to show. Other channels (soil
temp, air T/RH, battery) ride along for realism. All values cross the wire raw
(D1); deriving kPa/VPD is downstream and out of scope here.
"""
from __future__ import annotations

import math
import random
from collections.abc import Iterator
from dataclasses import dataclass, field

from . import packet
from .source import IPacketSource

# Channel bit lookup by name, from the shared registry — no magic numbers.
_BIT = {name: bit for bit, (name, _t) in packet.CHANNEL_REGISTRY.items()}


def _clamp_u16(v: float) -> int:
    return max(0, min(0xFFFF, int(round(v))))


def soil_tension_raw(wet_raw: int, dry_raw: int, tau_min: float, sim_min: float) -> int:
    """Drydown curve: resistance rises from wet_raw toward dry_raw with time
    constant tau_min, plus a small diurnal wiggle so the trace looks alive.
    Units are the contract's 0.1 kΩ/LSB (SOIL_TENSION_*)."""
    approach = 1.0 - math.exp(-sim_min / tau_min)
    diurnal = 0.03 * (dry_raw - wet_raw) * math.sin(2 * math.pi * sim_min / 1440.0)
    return _clamp_u16(wet_raw + (dry_raw - wet_raw) * approach + diurnal)


def _soil_temp_raw(temp_c: float) -> int:
    """DS18B20 raw: 1/16 °C/LSB, signed (stored as two's-complement u16)."""
    return int(round(temp_c * 16)) & 0xFFFF


def _sht_temp_ticks(temp_c: float) -> int:
    """SHT45 raw ticks: T = -45 + 175·ticks/65535."""
    return _clamp_u16((temp_c + 45.0) / 175.0 * 65535.0)


def _sht_rh_ticks(rh_pct: float) -> int:
    """SHT45 raw ticks: RH = -6 + 125·ticks/65535."""
    return _clamp_u16((rh_pct + 6.0) / 125.0 * 65535.0)


@dataclass
class NodeSpec:
    """A simulated bed node: a Watermark (drydown), a soil-temp probe, and a
    canopy SHT45. Defaults are plausible tunnel values, not calibrated."""

    node_id: int
    wet_raw: int = 120          # ~12 kΩ, freshly watered
    dry_raw: int = 2000         # ~200 kΩ, dry — well under the 0xFFFE ceiling
    tau_min: float = 2880.0     # ~2-day drydown time constant
    soil_temp_c: float = 18.5
    air_temp_c: float = 24.0
    air_rh_pct: float = 62.0
    battery_mv_full: int = 4050
    battery_drain_mv_per_day: float = 4.0

    def packet_at(self, sim_min: float, seq: int, fw_version: int = 100) -> bytes:
        """Build one packet v1 frame for this node at sim time `sim_min`."""
        battery = _clamp_u16(
            self.battery_mv_full - self.battery_drain_mv_per_day * (sim_min / 1440.0)
        )
        # Gentle diurnal swing on air temp/RH so VPD-relevant channels move too.
        phase = 2 * math.pi * sim_min / 1440.0
        air_t = self.air_temp_c + 4.0 * math.sin(phase)
        air_rh = self.air_rh_pct - 12.0 * math.sin(phase)
        channels = {
            _BIT["SOIL_TENSION_0"]: soil_tension_raw(
                self.wet_raw, self.dry_raw, self.tau_min, sim_min
            ),
            _BIT["SOIL_TEMP_0"]: _soil_temp_raw(self.soil_temp_c),
            _BIT["AIR_TEMP"]: _sht_temp_ticks(air_t),
            _BIT["AIR_RH"]: _sht_rh_ticks(air_rh),
        }
        return packet.encode(
            node_id=self.node_id,
            fw_version=fw_version,
            seq=seq & 0xFFFF,
            battery_mv=battery,
            channels=channels,
        )


@dataclass
class EmitEvent:
    sim_min: float      # the actual (jittered) transmit time, in sim minutes
    node_id: int
    packet: bytes


@dataclass
class FleetEmitter(IPacketSource):
    """A fleet of NodeSpecs transmitting on a shared cadence with independent
    ±jitter_s wake jitter (SPEC §10 — prevents radio collisions). Iterating
    yields encoded frames in transmit-time order; in realtime mode it sleeps the
    (compressed) gap between transmits so the dashboard moves as you watch.
    """

    specs: list[NodeSpec]
    cadence_min: float = 12.0       # nominal transmit interval (real nodes: 10–15 min)
    max_minutes: float = 1440.0     # how much sim time to run
    jitter_s: float = 30.0          # ±wake jitter
    seed: int = 1
    realtime: bool = False
    time_scale: float = 720.0       # sim-seconds per real-second in realtime mode
    fw_version: int = 100
    _rng: random.Random = field(init=False, repr=False)

    def __post_init__(self) -> None:
        self._rng = random.Random(self.seed)

    def schedule(self) -> list[tuple[float, int, float]]:
        """(nominal_min, node_id, jitter_s) for every transmit, transmit-time
        ordered. Exposed so tests and a fleet eyeball can confirm the jitter
        window keeps nodes off each other's slots."""
        out: list[tuple[float, int, float]] = []
        ticks = int(self.max_minutes // self.cadence_min)
        for k in range(1, ticks + 1):
            nominal = k * self.cadence_min
            for spec in self.specs:
                jitter = self._rng.uniform(-self.jitter_s, self.jitter_s)
                out.append((nominal, spec.node_id, jitter))
        out.sort(key=lambda e: e[0] + e[2] / 60.0)
        return out

    def events(self) -> list[EmitEvent]:
        by_id = {s.node_id: s for s in self.specs}
        seq: dict[int, int] = {s.node_id: 0 for s in self.specs}
        evs: list[EmitEvent] = []
        for nominal, node_id, jitter in self.schedule():
            actual = nominal + jitter / 60.0
            seq[node_id] += 1
            evs.append(
                EmitEvent(
                    sim_min=actual,
                    node_id=node_id,
                    packet=by_id[node_id].packet_at(actual, seq[node_id], self.fw_version),
                )
            )
        return evs

    def __iter__(self) -> Iterator[bytes]:
        import time

        prev: float | None = None
        for ev in self.events():
            if self.realtime and prev is not None:
                gap_real_s = (ev.sim_min - prev) * 60.0 / self.time_scale
                if gap_real_s > 0:
                    time.sleep(gap_real_s)
            prev = ev.sim_min
            yield ev.packet
