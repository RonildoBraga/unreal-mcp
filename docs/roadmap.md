# Roadmap

A single living document for "what's next." Replaces the multiple
architecture-vXxx-plan.md docs that were starting to encode commitments we
don't actually need to make.

This is a **solo-developer side project that serves Lauder.** Not a
product. Not a competitor to anyone. Decisions in this file should be
honest about that.

---

## Guiding principles

1. **Lauder drives the tool surface.** We add MCP capabilities when Lauder
   needs them — not when a roadmap says we should. A 200-tool MCP that
   doesn't ship a Lauder feature is worth less than a 100-tool MCP that does.

2. **Solo dev means no multi-agent coordination overhead.** One head, one
   build cycle, one source of truth. Don't split work across agents/tools
   unless the split saves more time than the coordination costs.

3. **Defer breadth until the use case is concrete.** Adding asset-editor
   tools "because we might need them" creates maintenance burden across
   UE version bumps. Adding them when Phase 7.x surfaces a real gap is
   correct.

4. **The CHANGELOG is the canonical record.** Plans are scaffolding; once
   work ships, the CHANGELOG entry is what survives. Don't keep multiple
   forward-looking docs in lockstep.

5. **Honest reporting beats optimistic reporting.** If a feature didn't
   land as planned, say so. If a roadmap item turned out to be
   unnecessary, kill it.

---

## Current state (truth, 2026-05-31)

**v0.8.x architecture migration (#64 / #66) — 7 of 9 categories done.**

```
Lifted to self-registration (REGISTER_MCP_COMMAND inline):
  ✅ project    (4 handlers)
  ✅ level      (3 handlers)
  ✅ material   (5 handlers)
  ✅ outliner   (5 handlers)
  ✅ umg        (14 handlers)
  ✅ editor     (33 handlers)
  ✅ asset      (16 handlers)
                 ────────────
                 80 handlers self-registering

Still in MCPRegistrations.cpp RegBatch (last 2 categories):
  ⏳ blueprint  (7 handlers)
  ⏳ node       (8 handlers)
                 ────────────
                 15 handlers
```

**Tool surface: 102 commands.** Smoke (`smoke_dispatch.py`) shows
95/95 dispatched, 0 unknown. Pytest (`tests/test_object_property.py`)
shows 21/21 pass.

**v0.9 status: undecided.** Codex's tool-parity plan was deleted along with
this rewrite. Whether there's a v0.9 at all, and what's in it, is an open
question that this roadmap answers honestly: we don't know yet, and that's
fine.

---

## Open work (concrete, this side)

### Finish architecture migration (#64 / #66 closeout)

The 15 remaining handlers (Blueprint + Node) follow the same mechanical
pattern as the previous 80. One commit, one close-build-reopen cycle.
Reference implementations: Phase 1 (commit `fda5576`), Phase 2a (`e2d7a16`),
Phase 2b-claude (`ca3d98a`).

**Status: queued for the next session unless picked up earlier.**

After this lands:
- `MCPRegistrations.cpp` reduces to ~30 LOC (just `ping` + the
  FAutoRegistrar shell).
- All 9 categories use the same self-registration shape.
- `#64` and `#66` close.

---

## Lauder Phase 7 (the actual game work)

These are the next priorities for this entire project — not "v0.9":

- **Phase 7.3:** L_Zone01 v2 — Dark Ruins migrated, hostile expedition zone
- **Phase 7.4:** Portal / ritual transition between sanctuary and expedition
- **Phase 7.5:** Visual quality gate — GO/NO-GO on Lauder's visual ceiling

Each of these will tell us, by running into them, what's actually missing
in the MCP toolkit. The list below ("On hold") is our hypothesis about
what's missing; Phase 7 work either confirms or kills each item.

---

## On hold — revisit when Phase 7 surfaces the need

These are real ideas, not bad ideas. They go here because the trigger for
"build it now" should be observing a concrete gap in Lauder work, not
abstract reasoning that they'd be useful.

### Transactions (`FScopedTransaction`)

Wrap mutating actor commands so the editor's Undo can roll them back.

**Trigger:** the first time autonomous MCP work corrupts a level and we'd
have given anything for Ctrl+Z. Until then, we have version control.

### MCP resources

Expose `unreal://level/current`, `unreal://selection/actors`,
`unreal://viewport/state`, etc. as MCP `resources` (not tools). LLMs pull
context cheaply without burning tool calls.

**Trigger:** observing the LLM make >5 tool calls per response just to
read state. Currently the read tools we have (`get_current_level`,
`get_selected_actors`, `find_actors`, etc.) seem to be enough; the
saturation point isn't here yet.

### Tool manifest

YAML/declarative metadata per tool, drives generated docs + validation +
preset membership.

**Trigger:** the per-tool docstrings start drifting from reality, or the
tool count exceeds ~150 and we can't keep the catalog in our head. At
102 tools, manageable.

### Structured errors

`{code, message, details, suggestions, retryable}` instead of the current
plain `{error: "..."}` strings.

**Trigger:** an autonomous-loop workflow keeps tripping on the same
unstructured error and needs better classification to retry intelligently.

### Tool presets / scope gates

`minimal`, `scene`, `assets`, `runtime`, `full` — present a smaller surface
to the LLM by default to reduce noise + decision fatigue.

**Trigger:** the LLM consistently picks the wrong tool from the 102-tool
list, OR we want to gate destructive tools behind explicit opt-in.

---

## Deferred indefinitely — until Lauder needs it specifically

These are all real Unreal subsystems that COULD have tool surfaces. We
won't build them until Lauder requires them, which it likely never will
for the v0 scope.

| Subsystem | Status |
|---|---|
| Sequencer / cinematics | Deferred. Lauder v0 has no cutscene scope. |
| Niagara / VFX | Deferred. Lauder uses simple particle effects from BP. |
| MetaSound | Deferred. Lauder uses raw `.wav` + standard SFX wiring. |
| Landscape / foliage / PCG | Deferred. Lauder uses placed Megascans, no terrain authoring. |
| GAS / behavior trees / EQS / StateTree | Deferred. Lauder uses bespoke C++ for AI. |
| Native HTTP MCP | Deferred. TCP+JSON works fine for our scale. |

If we ever ship Lauder beyond v0, half of these become real. For now they
exist on the "no" list to keep us focused.

---

## Anti-goals

Explicit "we will NOT do this" so we don't drift into doing it:

- Tool-count parity with closed-source competitors. Their goals aren't
  ours. We have 102 tools that work; that's enough until it isn't.
- Speculative breadth ("adding asset-editor tools because we might need
  them"). Maintenance burden scales linearly with surface area; ship
  value, not surface.
- Multi-version roadmap locking us into commitments before Phase 7 reveals
  what's actually needed. The "v0.9 → v0.10 → v0.11" framing was useful
  for v0.8.0 because we had a concrete redesign in mind; we don't right now.
- Multi-agent coordination overhead. One agent at a time. The Codex split
  in Phase 2b cost more in plan-doc authoring + protocol agreement than
  it saved in execution.
- Architecture re-design without a concrete failure mode driving it. The
  current architecture (FMCPRegistry + @unreal_tool decorator + strict
  wire format + per-file self-registration) works; don't touch it unless
  Phase 7 work breaks against it.

---

## Decision log

When we kill, defer, or commit to something, capture WHY here. So
future-us doesn't spend hours rediscovering the reasoning.

### 2026-05-31: Codex coordination removed

**Decision:** Drop the multi-agent coordination model. All MCP development
proceeds single-agent.

**Why:** Two attempts to use Codex (v0.9 plan, Phase 2b split) showed
coordination overhead (writing plan docs, agreeing on protocols,
sequencing commits) consistently exceeded the execution savings. For a
solo-dev project the bottleneck is decisions-per-hour, not
keystrokes-per-hour; adding another agent doesn't increase
decisions-per-hour, it just adds a translation layer.

**Consequence:** Phase 2b-codex (Blueprint + Node migration) is now Claude
work, not Codex work. Three plan docs deleted.

### 2026-05-31: v0.9 scope undecided, explicitly

**Decision:** Don't pick a v0.9 scope yet. Finish v0.8.x architecture work.
Pause MCP. Do Lauder Phase 7. Decide v0.9 (or whether there even is one)
based on what Phase 7 reveals.

**Why:** Codex's tool-parity plan was StraySpark-shaped: optimize for
breadth and feature checkboxes. That's a SaaS product's framing, not a
solo game project's. We don't know yet what Lauder Phase 7 needs from the
toolkit; pretending we do is fiction.

**Consequence:** No v0.9 milestone in this roadmap. The "On hold" section
captures the hypotheses about what might be needed, with explicit
triggers. If none of those triggers fire during Phase 7, the right answer
might be "v0.8.x is the long-term stable version."

---

## How to use this doc

- **Reading it for the first time?** Start at "Current state" and
  "Open work."
- **Considering adding a new MCP capability?** Check "On hold" first —
  if it's there, the trigger hasn't fired. If it's in "Deferred
  indefinitely," explain why Lauder now needs it.
- **About to write another `architecture-vXxx-plan.md`?** Don't. Add a
  section here, or extend the relevant CHANGELOG entry.
- **Killed a feature?** Add it to "Decision log" with the WHY.
