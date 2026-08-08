# Future Ideas — parking lot

*Ideas worth keeping but not worth building yet. Curated by `@ideas`. Parking
something here is not a rejection — it's a decision to not decide now (DEC-001).*

Nothing in this file is committed scope. Check `docs/SPEC.md` §3 "Not V1" and the
current `docs/PROJECT_PLAN.md` phase before promoting anything out of it.

---

## Index — by rough priority

| # | Idea | Origin | Cost to un-park |
|---|------|--------|-----------------|
| FI-1 | Tank **water** temperature sensor | `CHAT_HANDOFF.md` HW-10(b), 2026-08-08 | Low — one more DS18B20 on an existing 1-Wire bus, but a wet penetration |
| FI-2 | Second headspace temperature sensor (stratification) | DEC-007 residual, 2026-08-08 | Low — only if deployed data shows the residual |
| FI-3 | Channel registry rename `SOIL_TEMP_n` → `DS_TEMP_n` | DEC-007, 2026-08-08 | Low, but touches vectors + both parsers |

---

## FI-1 — Tank water temperature sensor

**Idea.** A second DS18B20, submerged at **mid-depth** in one of the catchment
tanks, riding the same 1-Wire bus as the required headspace sensor (DEC-007).
Mid-depth specifically: tanks stratify, so a surface probe reads air and a bottom
probe reads the coldest slug.

**Why it's interesting.** Three uses, none of which the tank-level measurement
needs, all of which are real:

- **Early-season irrigation timing.** Shift April/May runs to the afternoon rather
  than drenching PEX-warmed soil with 45 °F water.
- **Summer biofilm and clog-risk scheduling** for header and tape flushes.
- **Freeze alerting** for the pump, Dosatron, and header — probably the strongest
  of the three, since it protects equipment rather than informing a preference.

**Why it's parked.** Chat costed this at "+$5 for the second sensor," which is the
sensor and nothing else. The real cost is a stainless-sheathed probe, a suspension
method that holds mid-depth without drifting or fouling, a wet penetration (or a
shared one) on the **first hardware the farm has ever deployed**, and a
service story for a sensor hanging in irrigation water.

`SPEC.md` §3 scopes the tank node to level only. The headspace sensor is not new
scope — it's an accuracy fix to a measurement already committed to — but this one
is. Adding a submerged probe to the build that is meant to *de-risk* first contact
with hardware inverts the point of the sequencing (DEC-005).

**Un-park when.** The dry tank node is deployed and trusted, or a freeze event
makes the alerting case urgent enough to pull forward on its own merits.

---

## FI-2 — Second headspace temperature sensor

**Idea.** A second DS18B20 higher or lower in the tank headspace, interpolating
the air column instead of sampling one point of it.

**Why it's interesting.** DEC-007 accepts a known residual: a single sensor
measures one point in an air column that stratifies, and the error is worst when
the tank is near-empty and the headspace is tall — which is exactly the low-level
regime the whole sensor exists to protect.

**Why it's parked.** The single-sensor correction already converts a ~14 cm error
into low-single-digit centimetres. Chasing the residual before seeing it in real
data is gold-plating (DEC-001).

**Un-park when.** Deployed data shows level readings that track headspace
temperature after correction — i.e. the residual is visible, not theoretical.

---

## FI-3 — Rename `SOIL_TEMP_n` → `DS_TEMP_n` in the channel registry

**Idea.** A documentation-and-naming change to the packet contract: the registry
names channels 4/5 `SOIL_TEMP_0/1`, but what they actually encode is "DS18B20 raw,
i16, 1/16 °C" — which is why DEC-007 puts the tank's *headspace air* sensor on
bit 4 without spending a reserved bit.

**Why it's interesting.** The name is now actively misleading for at least one
deployed node type, and anyone reading `contracts/packet-v1.md` cold will assume
channel 4 means soil.

**Why it's parked.** It is not free: the bit, width, and encoding stay identical
and no `proto_ver` bump is needed, but the field names appear in
`contracts/vectors/packet-v1.json` and in both the C++ and Python parsers. That's
a real (small) change to the one artifact in the repo whose entire value is not
drifting. Not worth doing on its own.

**Un-park when.** Something else is already touching the registry — the next
channel addition is the natural moment to fold it in.
