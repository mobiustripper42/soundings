#!/usr/bin/env node
// One-time migration: a monolithic `docs/DECISIONS.md` → one file per decision under
// `docs/decisions/`, plus the `_preamble.md` and `_config.json` the generator needs
// (DEC-S036).
//
// Run once per project, then `npm run gen:decisions` to write the index back.
//
// WHAT IT DOES NOT DO, deliberately: it does not invent `amends:` edges. The relation
// between two decisions is a judgment — "superseded" and "amended in this one leg" look
// identical in prose, and an audit of the originating project found that every single
// decision recorded as fully superseded still had a live leg. Guessing here would write a
// wrong fact into a generated banner, which is worse than the strike-through prose it
// replaced, because a generated line reads as checked. So supersession prose is left
// exactly where it is, and every line that smells like an edge is REPORTED for a human to
// declare in frontmatter afterwards.
//
// Same for topics: every decision lands in the config's first topic and the report says
// so. A wrong topic is visible in the index the moment you read it; a wrong amendment edge
// is not.

import { readFileSync, writeFileSync, mkdirSync, existsSync, readdirSync } from 'node:fs'

const SRC = process.argv[2] ?? 'docs/DECISIONS.md'
const DIR = process.argv[3] ?? 'docs/decisions'

/** `## DEC-001: Title`, `## DEC-001 — Title`, `## DEC-MSG-1 - Title`, `## DEC-TBD: …`. Both
 *  separators are in use across the projects this runs on; neither is worth normalizing
 *  first when the parser can just accept both. */
const HEADING = /^## (DEC-(?:\d+|[A-Z]+-?\d+|TBD))\s*[:—–-]\s*(.+?)\s*$/

/**
 * Prose that probably encodes an amendment edge. Reported, never acted on.
 *
 * Two conditions, both required: a word that names a relation, AND a citation of some other
 * decision. A relation word alone is far too loose — the decision template's own
 * "**Revisit if:**" field contains `revis`, so a bare word-match flagged every decision in the
 * first repo this ran on, and a report that flags everything is a report nobody reads.
 */
const RELATION_WORD = /~~|\bsupersed|\bamend(s|ed|ment)|\breplaced by\b|\breplaces\b|\bretired by\b|\bretires\b|\breverses?\b|\brevised by\b|\brefines\b|\bcorrects\b|\bextends\b/i
const CITES_DECISION = /\bDEC-(?:\d+|[A-Z]+-?\d+|TBD)\b/
const hasEdgeHint = (s) => RELATION_WORD.test(s) && CITES_DECISION.test(s)

export function slug(title, limit = 50) {
  const s = title
    .toLowerCase()
    .replace(/[`*_"'’]/g, '')
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-+|-+$/g, '')
  if (s.length <= limit) return s
  const cut = s.slice(0, limit)
  const at = cut.lastIndexOf('-')
  return (at > 0 ? cut.slice(0, at) : cut).replace(/-+$/, '')
}

const quote = (s) => `"${String(s).replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`

/**
 * @param {string} text the monolithic DECISIONS.md
 * @returns {{preamble: string, decisions: {id: string, title: string, body: string, line: number}[]}}
 */
export function parse(text) {
  const lines = text.split('\n')
  const decisions = []
  let preambleEnd = lines.length
  let current = null

  lines.forEach((line, i) => {
    const m = line.match(HEADING)
    if (!m) {
      if (current) current.body.push(line)
      return
    }
    if (!decisions.length) preambleEnd = i
    current = { id: m[1], title: m[2], body: [], line: i + 1 }
    decisions.push(current)
  })

  return {
    preamble: lines.slice(0, preambleEnd).join('\n').replace(/\s+$/, ''),
    decisions: decisions.map((d) => ({ ...d, body: d.body.join('\n').replace(/^\n+/, '').replace(/\s+$/, '') })),
  }
}

/** Families present in the parsed ids, mapped to a rank position the caller must sanity-check.
 *  A family is placed just before the lowest numeric id, which is right when the family
 *  predates the main line and wrong when it doesn't — hence the report line. */
export function families(ids) {
  const found = new Set()
  for (const id of ids) {
    const m = id.match(/^DEC-([A-Z]+)-?\d+$/)
    if (m) found.add(m[1])
  }
  const lowest = Math.min(...ids.map((id) => Number(id.match(/^DEC-(\d+)$/)?.[1] ?? Infinity)))
  const at = Number.isFinite(lowest) ? lowest - 0.5 : 0
  return Object.fromEntries([...found].sort().map((f) => [f, at]))
}

export function render(d, topic) {
  const fm = ['---', `id: ${d.id}`, `title: ${quote(d.title)}`, `topic: ${quote(topic)}`, '---']
  return `${fm.join('\n')}\n\n## ${d.id}: ${d.title}\n\n${d.body}\n`
}

if (process.argv[1]?.endsWith('split-decisions.mjs')) {
  if (!existsSync(SRC)) {
    console.error(`✗ ${SRC} does not exist`)
    process.exit(1)
  }
  if (existsSync(DIR) && readdirSync(DIR).some((f) => f.startsWith('DEC-'))) {
    console.error(`✗ ${DIR} already holds decision files — this migration runs once`)
    process.exit(1)
  }

  const { preamble, decisions } = parse(readFileSync(SRC, 'utf8'))
  if (!decisions.length) {
    console.error(`✗ ${SRC} — no \`## DEC-…\` headings found; nothing to split`)
    process.exit(1)
  }

  const seen = new Map()
  const dupes = []
  for (const d of decisions) {
    if (seen.has(d.id)) dupes.push(`${d.id} appears at line ${seen.get(d.id)} and ${d.line}`)
    else seen.set(d.id, d.line)
  }
  if (dupes.length) {
    console.error(`✗ ${SRC} — duplicate ids, resolve them before splitting:\n`)
    for (const d of dupes) console.error(`  ${d}`)
    process.exit(1)
  }

  mkdirSync(DIR, { recursive: true })

  const cfgPath = `${DIR}/_config.json`
  const cfg = existsSync(cfgPath)
    ? JSON.parse(readFileSync(cfgPath, 'utf8'))
    : { spec: 'docs/SPEC.md', families: families(decisions.map((d) => d.id)), topics: ['Uncategorized'] }
  if (!existsSync(cfgPath)) writeFileSync(cfgPath, `${JSON.stringify(cfg, null, 2)}\n`)

  const topic = cfg.topics[0]
  for (const d of decisions) writeFileSync(`${DIR}/${d.id}-${slug(d.title)}.md`, render(d, topic))
  writeFileSync(`${DIR}/_preamble.md`, `${preamble}\n`)

  const edges = decisions.filter((d) => hasEdgeHint(d.body) || hasEdgeHint(d.title))

  console.log(`✓ split ${SRC} → ${decisions.length} files in ${DIR}/`)
  console.log(`\n  Every decision landed in topic ${JSON.stringify(topic)}. Next, by hand:`)
  console.log(`    1. Write the real topic list into ${cfgPath} and re-topic each file.`)
  if (Object.keys(cfg.families).length) {
    console.log(`    2. Check the id-family positions in ${cfgPath}: ${JSON.stringify(cfg.families)}`)
    console.log(`       — each number is where that family sits among the numeric ids, and it is a guess.`)
  }
  console.log(`    3. Declare amendment edges in frontmatter (\`amends:\`), then \`npm run gen:decisions\`.`)
  if (edges.length) {
    console.log(`\n  ${edges.length} decision${edges.length === 1 ? '' : 's'} mention an amendment in prose —`)
    console.log('  these are the candidates for an `amends:` declaration (not auto-converted, on purpose):')
    for (const d of edges) console.log(`    ${d.id} — ${d.title}`)
  }
}
