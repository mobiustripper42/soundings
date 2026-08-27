#!/usr/bin/env node
// Validates the decision record (DEC-S036, from muster DEC-141). Text-only, no deps —
// runs first in `npm run verify` so it fails in milliseconds rather than behind
// typecheck/test/build. CI needs no workflow step of its own; the existing verify job
// already runs `verify`.
//
// The check that matters most is FRESHNESS: the generator is a manual step, and this is
// what makes forgetting it a red build instead of an invisible defect. That — not
// discipline — is the actual fix for a hand-maintained index's decay.
//
// It does not stop a DEC-number collision happening; two branches can still both pick 142.
// It stops one being silent. The second to merge goes red, where the originating project's
// collision sat unnoticed across two branches until an audit swept all 134 decisions.
//
// Like its generator, this file is byte-identical across projects — every project-specific
// knob is in `docs/decisions/_config.json`.

import { readFileSync, readdirSync, existsSync, statSync } from 'node:fs'
// Named import, not default: js-yaml is CommonJS, and `import yaml from 'js-yaml'` resolves
// under vitest's transform but throws under plain node — which is how this script actually
// runs. The test suite passing is not evidence the gate runs.
import { load as parseYaml } from 'js-yaml'
import {
  DIR,
  OUT,
  SPEC,
  TOPICS,
  generate,
  load,
  referencePattern,
  sectionNumber,
  specSections,
} from './gen-decisions-index.mjs'

/** Exported because `check-docs.mjs` applies the same rule to the rest of the doc set.
 *  This script's scan stops at `docs/decisions/` + the index, which was never a statement
 *  that references elsewhere are safe — only that this script's subject is the record. */
export const REFERENCE = referencePattern()

// ── Schema v1 ────────────────────────────────────────────────────────────────
//
// Two parsers, on purpose. Legacy records keep going through `parseFrontmatter`, which is a
// hand-rolled reader for one flat block. Records carrying `schema: 1` go through js-yaml,
// because their frontmatter has nested lists of objects and a hand-rolled reader is how the
// original record ended up with `dumb:` and `boat:` as keys — a colon in prose parsing as a
// mapping and nobody noticing.
//
// The gate is the `schema: 1` key itself. A record without it is grandfathered and skipped.
// In jig that path should stay dead: the corpus started empty on 2026-08-27 and every record
// here is v1, so a record without the key is a mistake rather than history.
//
// THE COMMENT THAT USED TO BE HERE IS THE REASON THIS ONE IS. It declared a one-line blocker
// — `LISTS` knowing only `amends_spec`, so a `claims:` list threw and broke every downstream
// check — and closed with "convert nothing until the generator's parser is widened." The
// generator was widened in the very next commit; `LISTS` has held `claims` and `supersedes`
// since. The instruction outlived its condition by weeks and was still being obeyed when jig
// read it, because nothing retires a comment. This is the corpus-rot argument arriving one
// level down, and the reason `revisit_if` is required in jig's schema and optional in
// muster's: prose about the state of the code cannot be trusted to notice it changed.
//
// Claim TARGETS are deliberately not resolved. `kind` is checked against the enum and the
// shape is checked; whether `src/foo.ts` exists is a resolver's job, and resolvers get built
// once the rewritten records show which kinds actually appear.

const SCHEMA_PATH = `${DIR}/decision-record.schema.json`

/** A `**Bold:**` / `**Bold.**` lead-in — the drift that put `Decision:`, `Tradeoff:` and
 *  `Status:` in prose. All structure lives in frontmatter now. Anchored to the line start so
 *  ordinary mid-sentence bold is untouched. */
const BOLD_LEAD_IN = /^\*\*([A-Z][^*\n]{0,40})[:.]\*\*/m

const MAX_BYTES = 2000

/**
 * The two frontmatter predicates the sweep runs on, exported so tests assert the real thing.
 *
 * Three tests used to define their own copy of each and assert against the copy, so changing the
 * implementation left all three green — a test that cannot fail is worse than no test, because it
 * reads as coverage. `hasSchemaKey` in particular guards the opt-in gate, and that gate was
 * already broken when the review found it.
 *
 * `hasSchemaKey` matches the KEY, not a formatting of its value: `/^schema: *1 *$/m` is stricter
 * than YAML, so `schema: 1  # v1 draft` and `schema: "1"` both read as "never opted in" and the
 * record got zero enforcement with nothing saying so.
 */
export const hasSchemaKey = (block) => /^schema:/m.test(block)
export const idOf = (block) => block.match(/^id: *(\S+)/m)?.[1]

/**
 * Bytes of frontmatter + decision, excluding any `## Amendment` sections.
 *
 * Measured as "the whole file minus the amendments" rather than by re-serializing, so the number
 * a reader is told still corresponds to something they can see on disk. `statSync` is not used
 * because it counts the amendments the cap deliberately ignores.
 */
export const sizeOf = (text) => Buffer.byteLength(beforeAmendments(text), 'utf8')

/**
 * Everything up to the first real `## Amendment` heading — one that is not inside a fenced block.
 *
 * THE FENCE CHECK IS THE WHOLE POINT. A bare `text.search(/^## Amendment\b/m)` truncates at the
 * first such line anywhere, including inside ```` ```md ```` — so a record that QUOTES the
 * amendment convention to explain it measures 21 bytes and passes a 2,000-byte cap, and
 * `BOLD_LEAD_IN` is defeated the same way. A decision about how to write amendments is exactly
 * the kind this repo writes, and it would have disabled both rules on itself.
 *
 * Fences are counted rather than matched in pairs: an unclosed fence leaves the rest of the file
 * "inside" one, which fails toward measuring MORE of the file, not less.
 */
export function beforeAmendments(text) {
  const lines = text.split('\n')
  let fenced = false
  for (let i = 0; i < lines.length; i++) {
    if (/^\s*(?:```|~~~)/.test(lines[i])) { fenced = !fenced; continue }
    if (!fenced && /^## Amendment\b/.test(lines[i])) return lines.slice(0, i).join('\n')
  }
  return text
}

/** The subset of draft 2020-12 the schema actually uses. Written out rather than pulled in
 *  because it is ~70 lines against a dependency, and because every message here has to name
 *  the key and the limit — a validator that says "does not match schema" is a validator
 *  nobody can act on. */
function validateAgainstSchema(schema, value, path, root, errs) {
  if (schema.$ref) {
    const target = schema.$ref.replace(/^#\//, '').split('/')
    return validateAgainstSchema(target.reduce((o, k) => o[k], root), value, path, root, errs)
  }
  const at = path || 'frontmatter'

  if (schema.const !== undefined && value !== schema.const) {
    errs.push(`${at} must be ${JSON.stringify(schema.const)}, not ${JSON.stringify(value)}`)
    return errs
  }
  if (schema.enum && !schema.enum.includes(value)) {
    errs.push(`${at} is ${JSON.stringify(value)} — allowed: ${schema.enum.join(', ')}`)
    return errs
  }
  if (schema.type === 'array') {
    if (!Array.isArray(value)) {
      errs.push(`${at} must be a list`)
      return errs
    }
    if (schema.minItems !== undefined && value.length < schema.minItems) {
      errs.push(`${at} needs at least ${schema.minItems} entry — it has ${value.length}`)
    }
    if (schema.items) value.forEach((v, i) => validateAgainstSchema(schema.items, v, `${at}[${i}]`, root, errs))
    return errs
  }
  if (schema.type === 'string') {
    if (typeof value !== 'string') {
      errs.push(`${at} must be text, not ${value === null ? 'empty' : typeof value}`)
      return errs
    }
    if (schema.maxLength !== undefined && value.length > schema.maxLength) {
      errs.push(`${at} is ${value.length} characters — the limit is ${schema.maxLength}`)
    }
    if (schema.minLength !== undefined && value.length < schema.minLength) {
      errs.push(`${at} is ${value.length} characters — the minimum is ${schema.minLength}`)
    }
    if (schema.pattern && !new RegExp(schema.pattern).test(value)) {
      errs.push(`${at} is ${JSON.stringify(value)}, which does not match ${schema.pattern}`)
    }
    if (schema.format === 'date' && !/^\d{4}-\d{2}-\d{2}$/.test(value)) {
      errs.push(`${at} is ${JSON.stringify(value)} — a date must be YYYY-MM-DD`)
    }
    return errs
  }
  if (schema.type === 'object' || schema.properties) {
    if (value === null || typeof value !== 'object' || Array.isArray(value)) {
      errs.push(`${at} must be a block of keys`)
      return errs
    }
    for (const key of schema.required ?? []) {
      if (!(key in value)) errs.push(`${at === 'frontmatter' ? '' : `${at}: `}missing required key \`${key}\``)
    }
    if (schema.additionalProperties === false) {
      for (const key of Object.keys(value)) {
        if (!(key in (schema.properties ?? {}))) {
          errs.push(`${at === 'frontmatter' ? '' : `${at}: `}unknown key \`${key}\``)
        }
      }
    }
    for (const [key, sub] of Object.entries(schema.properties ?? {})) {
      if (key in value) validateAgainstSchema(sub, value[key], at === 'frontmatter' ? key : `${at}.${key}`, root, errs)
    }
  }
  return errs
}

/**
 * Validate one `schema: 1` record. Returns a list of readable problems, empty when clean.
 *
 * @param {object} meta   parsed frontmatter
 * @param {string} body   everything after the frontmatter block
 * @param {number} bytes  size of the whole file on disk
 */
export function validateSchemaRecord(meta, body, bytes, schema = JSON.parse(readFileSync(SCHEMA_PATH, 'utf8'))) {
  // The topic enum is injected here rather than baked into the schema: the list is a
  // project fact and the schema file is not project-specific.
  const withTopics = {
    ...schema,
    properties: { ...schema.properties, topic: { ...schema.properties.topic, enum: TOPICS } },
  }
  const errs = validateAgainstSchema(withTopics, meta, '', withTopics, [])

  if (bytes > MAX_BYTES) {
    errs.push(`is ${bytes} bytes — the cap is ${MAX_BYTES}. A decision that will not fit is more than one decision`)
  }
  /**
   * Scoped to the decision for the same reason the byte cap is, and it collided the same way.
   *
   * This rule exists because `**Decision:**`, `**Tradeoff:**` and `**Status:**` used to be prose
   * lead-ins standing in for structure that now lives in frontmatter. An amendment has no
   * frontmatter and never will — it is dated prose appended under a heading — and the preamble
   * prescribes the very phrasing this caught: *"appended as a dated `## Amendment` section saying
   * what still stands."* Seeds wrote that as `**What this changes, and what still stands.**` in
   * every one of its amendments.
   *
   * So the first amendment written under this schema failed the gate for following the
   * instruction the gate's own repo gives. Both halves were right; the scope was wrong.
   */
  const lead = beforeAmendments(body).match(BOLD_LEAD_IN)
  if (lead) {
    errs.push(`body opens a paragraph with \`**${lead[1]}:**\` — structure lives in frontmatter now, not in bold prose`)
  }
  return errs
}

export function check() {
  const failures = []
  const fail = (where, msg) => failures.push(`${where} — ${msg}`)

  // ── Schema v1, and the id sweep ───────────────────────────────────────────
  //
  // Both run BEFORE `load()`, deliberately. `load()`'s reader has no shape for schema v1's
  // nested claim list, so the first rewritten record throws there. Running first means the
  // schema errors still print rather than being swallowed by that throw — but it does NOT
  // rescue the checks below `load()`, which stop for the whole corpus. See the blocker note
  // at the top of this section.
  const schemaFile = existsSync(SCHEMA_PATH) ? JSON.parse(readFileSync(SCHEMA_PATH, 'utf8')) : null
  const seenIds = new Map()
  const rewritten = new Map()
  const sweep = (dir, label) => {
    if (!existsSync(dir)) return
    for (const f of readdirSync(dir).filter((f) => f.endsWith('.md') && f !== '_preamble.md')) {
      const path = `${dir}/${f}`
      const text = readFileSync(path, 'utf8')
      const block = text.startsWith('---\n') ? text.slice(4, text.indexOf('\n---\n', 3)) : ''

      // One id may exist in exactly one file. `load()` catches a duplicate within its own
      // directory and stops there; an archived copy alongside the live record is the case it
      // cannot see, and it is the one that makes a citation ambiguous.
      const id = idOf(block)
      if (id) {
        const prior = seenIds.get(id)
        if (prior) fail(path, `id ${id} is already used by ${prior} — one id, one file`)
        else seenIds.set(id, `${label}${f}`)
      }

      // The opt-in gate reads the PARSED value, not the raw line. A regex here is stricter
      // than YAML: `schema: 1  # v1` and `schema: "1"` are both valid and both fail
      // `/^schema: *1 *$/m`, so the record would be silently treated as legacy and get zero
      // enforcement — a rule that looks applied and isn't, which is the exact class this
      // whole gate exists to close. The cheap `^schema:` pre-filter keeps js-yaml off the
      // ~158 legacy blocks that have no such key.
      /**
       * NO GRANDFATHERING IN JIG, and this line is where that stops being a comment.
       *
       * Muster skips a record with no `schema:` key, because its 158 legacy blocks convert one at
       * a time. jig started empty on 2026-08-27, so a record without the key is not history — it
       * is a record that was written after the rule existed and dodges every rule at once. A
       * 4,188-byte file with a `**Decision:**` lead-in and no schema key passed clean while the
       * comment forty lines above declared that path dead.
       *
       * A skip is the wrong shape for this repo regardless of the answer: `continue` is silent,
       * and every defect this rebuild found was silent.
       */
      if (!hasSchemaKey(block)) {
        fail(path, 'has no `schema:` key — every record here is v1, and there is nothing to grandfather')
        continue
      }
      let meta
      try {
        meta = parseYaml(block)
      } catch (e) {
        fail(path, `frontmatter is not valid YAML — ${e.message.split('\n')[0]}`)
        continue
      }
      if (meta?.schema !== 1) {
        fail(path, `\`schema:\` is ${JSON.stringify(meta?.schema)} — the only version is 1`)
        continue
      }
      if (!schemaFile) {
        fail(path, `declares \`schema: 1\` but ${SCHEMA_PATH} does not exist`)
        continue
      }
      const body = text.slice(text.indexOf('\n---\n', 3) + 5)
      /**
       * THE CAP MEASURES THE DECISION, NOT THE FILE. Two of this record's own rules collided the
       * first time anything tried to amend one:
       *
       *   "a decision that will not fit is more than one decision"  — the 2000-byte cap
       *   "a change to what a decision decided goes IN that file"   — the amendment rule
       *
       * Together they mean a record can be amended roughly never. DEC-J001 sat at 1,986 bytes of
       * 2,000, so its first amendment would have failed the gate, and the only ways out are the
       * two this record explicitly forbids: open a second decision on the same subject, or delete
       * argued reasoning to make room for later reasoning.
       *
       * The cap's own sentence says what it is for, and it is scope — one decision, one subject.
       * That is a claim about the decision, not about how much history has accumulated under it.
       * Amendments are the history, they are dated and append-only by design, and a cap on them
       * would be a cap on recording what changed.
       */
      for (const problem of validateSchemaRecord(meta, body, sizeOf(text), schemaFile)) fail(path, problem)
      rewritten.set(meta.id, { ...meta, path })
    }
  }
  sweep(DIR, '')
  sweep(`${DIR}/archive`, 'archive/')

  // `superseded_by` must land on a record that exists and is still the live one. Pointing at
  // a record that is itself superseded is a chain a reader has to walk, and pointing at
  // nothing is the dangling citation the whole gate exists to stop.
  for (const [id, d] of rewritten) {
    if (!d.superseded_by) continue
    const target = rewritten.get(d.superseded_by)
    if (!target) {
      if (!seenIds.has(d.superseded_by)) {
        fail(d.path, `superseded_by ${d.superseded_by}, which has no decision file`)
      } else {
        fail(d.path, `superseded_by ${d.superseded_by}, which has not been rewritten and so carries no status`)
      }
    } else if (target.status !== 'active') {
      fail(d.path, `superseded_by ${d.superseded_by}, whose status is ${target.status} — point at the live record`)
    }
    if (d.superseded_by === id) fail(d.path, `superseded_by itself`)
  }

  let decisions
  try {
    decisions = load()
  } catch (e) {
    return [...failures, `${DIR} — ${e.message}`]
  }

  // `load()` only looks at files shaped `DEC-*.md`. Anything else in the directory —
  // a lowercase `dec-014-...`, a missing hyphen, a stray draft carrying a duplicate id —
  // would be invisible to both the generator and every check below, which is the silent
  // rot this whole record was split to eliminate. A shape it does not look at is worse
  // than a shape it cannot parse.
  const hasSpec = existsSync(SPEC)
  const sections = hasSpec ? specSections(readFileSync(SPEC, 'utf8')) : new Map()
  const loaded = new Set([...decisions.values()].map((d) => d.file))
  for (const f of readdirSync(DIR).filter((f) => f.endsWith('.md') && f !== '_preamble.md')) {
    if (!loaded.has(f)) {
      fail(`${DIR}/${f}`, 'is not a recognized decision file — the name must be `DEC-<id>-<slug>.md`')
    }
  }

  for (const [id, d] of decisions) {
    const at = `${DIR}/${d.file}`

    if (!d.file.startsWith(`${id}-`) && d.file !== `${id}.md`) {
      fail(at, `filename does not start with its id (${id})`)
    }
    if (!d.title) fail(at, 'no title')
    if (!TOPICS.includes(d.topic)) {
      fail(at, `unknown topic ${JSON.stringify(d.topic)} — add it to topics in ${DIR}/_config.json if it is real`)
    }

    // A declared spec amendment must land on a section that exists. The originating
    // audit's largest finding class was the opposite direction — a decision claiming to
    // change SPEC and the change never landing — and an anchor nobody validates is how a
    // claim goes quiet: the section gets renumbered, the pointer stops resolving, and
    // prose says nothing.
    for (const a of d.amends_spec ?? []) {
      const sec = sectionNumber(a.section)
      if (!hasSpec) {
        fail(at, `amends_spec §${sec}, but ${SPEC} does not exist`)
        continue
      }
      if (!sections.has(sec)) {
        fail(at, `amends_spec §${sec}, which is not a numbered section of ${SPEC}`)
      }
      if (!a.scope?.trim()) {
        fail(at, `amends_spec §${sec} with no scope — the scope is what tells a reader of that section what changed`)
      }
    }

    // Every scope is rendered inside a bold span, so `**` in the text nests and the
    // banner renders as garbage — silently, since nothing about the build notices how
    // markdown looks. Caught the first time anyone wrote one.
    for (const a of d.amends_spec ?? []) {
      if (a.scope?.includes('**')) {
        fail(
          at,
          `scope for §${sectionNumber(a.section)} contains \`**\` — it renders inside a bold span, so the emphasis nests and breaks`,
        )
      }
    }
  }

  // Every decision id mentioned in a decision file or the index resolves to a real decision.
  const sources = [
    ...readdirSync(DIR)
      .filter((f) => f.endsWith('.md'))
      .map((f) => [`${DIR}/${f}`, readFileSync(`${DIR}/${f}`, 'utf8')]),
    [OUT, readFileSync(OUT, 'utf8')],
  ]
  for (const [path, text] of sources) {
    text.split('\n').forEach((line, i) => {
      for (const ref of line.matchAll(REFERENCE)) {
        if (!decisions.has(ref[0])) fail(`${path}:${i + 1}`, `reference to ${ref[0]}, which has no decision file`)
      }
    })
  }

  if (failures.length) return failures

  // Freshness. Everything above can pass on a record whose index was never regenerated,
  // which is the exact defect this replaces.
  const { index, files, spec } = generate()
  if (readFileSync(OUT, 'utf8') !== index) {
    fail(OUT, 'index is stale — run `npm run gen:decisions`')
  }
  for (const [file, text] of files) {
    if (readFileSync(`${DIR}/${file}`, 'utf8') !== text) {
      fail(`${DIR}/${file}`, 'frontmatter is stale — run `npm run gen:decisions`')
    }
  }
  // This is what makes a declared amendment LAND: the pointer in the amended section is
  // regenerated from the declaration, so a claim that never reached the spec is a red
  // build rather than a line of prose nobody cross-read.
  if (spec !== null && readFileSync(SPEC, 'utf8') !== spec) {
    fail(SPEC, 'declared spec amendments have not landed in the section — run `npm run gen:decisions`')
  }

  return failures
}

if (process.argv[1]?.endsWith('check-decisions.mjs')) {
  const failures = check()
  if (failures.length) {
    console.error(`✗ decision record — ${failures.length} problem${failures.length === 1 ? '' : 's'}:\n`)
    for (const f of failures) console.error(`  ${f}`)
    console.error('')
    process.exit(1)
  }
  const decisions = load()

  const specEdges = [...decisions.values()].reduce((n, d) => n + (d.amends_spec?.length ?? 0), 0)
  console.log(
    `✓ decision record — ${decisions.size} decisions in ${DIR}/, ` +
      `${specEdges} spec amendments landed, index fresh, all references resolve`,
  )
}
