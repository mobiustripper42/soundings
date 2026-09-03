# Future Ideas — parking lot

*Ideas worth keeping but not worth building yet. Nothing curates this file — tidy
it by hand or let it get untidy. Parking something here is not a rejection — it's
a decision to not decide now (DEC-001).*

Nothing in this file is committed scope. Check `docs/SPEC.md` § Not V1 and the
current `docs/PROJECT_PLAN.md` phase before promoting anything out of it.

---

## Index — by rough priority

| # | Idea | Origin | Cost to un-park |
|---|------|--------|-----------------|
| FI-1 | Tank **water** temperature sensor | `CHAT_HANDOFF.md` HW-10(b), 2026-08-08 | **Not low** — the sensor is cheap, the mid-depth placement is the cost. Parked on physical difficulty, not schedule. |
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

**Why it's parked.** Owner's call, 2026-08-08: **it is physically difficult to
place, and it increases complexity — the value it might provide doesn't cover
that.** Getting a probe to a stable mid-depth position means a suspension method
that won't drift or foul, and it puts a submerged sensor in irrigation water that
someone has to service.

Chat costed this at "+$5 for the second sensor," which is the sensor and nothing
else — not the stainless-sheathed probe, the suspension, the wet penetration, or
the service story.

Secondary: `SPEC.md` §3 scopes the tank node to level only, and adding a submerged
probe to the build meant to *de-risk* first contact with hardware works against
the point of the sequencing (DEC-005).

**Un-park when.** Note that the primary objection is **physical, not schedule** —
it does not decay as the project matures, so "once the tank node is trusted" is
*not* the trigger. Un-park when either:

- someone works out a mounting method that is genuinely simple (a probe fixed to
  an existing internal structure at a known depth, say), **or**
- the tank is already open for another reason and the marginal cost of adding it
  collapses.

A freeze event severe enough to damage the pump or Dosatron would also force a
re-look, but on the alerting case's own merits rather than because this got easier.

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
