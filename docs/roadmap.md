# Roadmap

A single living document for "what's next" in **unreal-mcp** (the plugin
+ server, not any particular game project that consumes it).

This is **a solo-maintainer side project.** Decisions here should be
honest about that: small surface, slow growth, no commitments to feature
parity with any other tool.

---

## Guiding principles

These shape every "add this?" / "kill this?" / "rewrite this?" decision.

1. **Real use drives tool addition.** A capability lands when a concrete
   workflow needs it. Not "this could be useful." Not "for parity with
   X." Not "users might want it." If nobody hits the missing thing while
   doing real editor work, it doesn't ship.

2. **One responsibility per file.** Every command handler is a free
   function in an anonymous namespace, self-registering at its definition
   site via `REGISTER_MCP_COMMAND`. No central dispatchers, no wrapping
   classes. The `architecture.md` doc explains the pattern; new commands
   follow it.

3. **Strict wire format.** Every response is
   `{success: bool, error?: string, ...payload}`. No envelopes. No
   `status: "success"` wrappers. The Python decorator and C++ response
   builder both enforce this; deviations are deliberate (e.g.
   `execute_python` returns `{success, stdout, stderr}` and is documented
   as the escape hatch shape).

4. **Docstrings are the catalog.** Every `@unreal_tool` / `@mcp.tool()`
   function's docstring is what an MCP client sees via `tools/list`.
   That's the source of truth. Markdown reference docs are scaffolding;
   they drift out of date almost immediately. If a doc and a docstring
   disagree, the docstring wins.

5. **The CHANGELOG is the canonical history.** Plans are scaffolding.
   Once work ships, the CHANGELOG entry is what survives. This roadmap
   is forward-looking; the CHANGELOG is the record.

6. **Defer breadth until a concrete trigger fires.** Adding asset-editor
   tools "because we might need them" creates maintenance burden across
   UE version bumps. Adding them when a real workflow surfaces the gap
   is correct. "On hold" + "Deferred" sections below name the triggers.

7. **Honest reporting beats optimistic reporting.** If a feature didn't
   land as planned, say so. If a roadmap item turned out to be
   unnecessary, kill it in the Decision log.

---

## Current state (truth, 2026-06-02)

**Large MCP surface.** Cold-start auto-registration shows commands from the
`FAutoRegistrar` plus file-scope `REGISTER_MCP_COMMAND` declarations in each
handler's `.cpp`. `smoke_dispatch.py` now defaults to an explicit
safe/read-only command allowlist (29 commands in the current tool surface)
and requires `--allow-mutating` for full empty-param dispatch (99 currently
selected, with lifecycle commands still skipped). Pytest
(`tests/test_object_property.py`) shows 21/21 pass; the pure-Python smoke
classifier tests show 7/7 pass.

**Architecture: complete.** All 9 command categories use the
self-registration pattern (see commit `adb1a8b`). `MCPRegistrations.cpp`
is at its minimum surface — just the `ping` virtual command + the
`FAutoRegistrar` shell. Adding a new command takes one line in the
handler's own `.cpp`, no central edits.

**v0.9 status: undecided.** No tool-parity roadmap is committed.
Whether there's a v0.9 at all, and what's in it, depends on what
real-use feedback surfaces.

**Tool surface map** (more in `docs/tools/README.md` + each
`server/tools/*.py` module's header docstring):

| Module | Count | Surface |
|---|---|---|
| Editor | 48 | Actors, selection, viewport, screenshots, console, PIE, reflection |
| Assets | 16 | Registry, mutations, import, migration, browser nav, static-mesh inspection |
| Blueprint nodes | 8 | K2 graph nodes, pin connections |
| Blueprints | 7 | Classes, components, properties, compile |
| UMG | 6 | Widget BPs, text + buttons, event binding |
| Materials | 5 | Instances, parameters, parent + uses |
| Outliner | 5 | Folders + batch organize |
| Levels | 3 | Get / open / save |
| Project | 4 | Input mappings + INI editing + `execute_python` escape hatch |

---

## Open work (concrete)

Nothing currently in progress. Recent architectural commitments
(self-registration migration, smoke + pytest infrastructure, docs
sweep) all closed in late May 2026.

The natural next move is **real-use feedback** — using the existing
102-tool surface for actual editor automation work and watching what's
missing or awkward. Without that signal, anything added here would be
speculation.

---

## On hold — hypothesized features with explicit triggers

These are real ideas, not bad ideas. They go here because the trigger
for "build it now" should be a concrete observation from real-use
workflow, not abstract reasoning that they'd be useful.

### Transactions (`FScopedTransaction`)

Wrap mutating actor commands so the editor's Undo / Ctrl+Z rolls them
back. Standard mutation-response fields would include `undo_label` +
`changed` / `created` / `deleted` lists.

**Trigger:** the first time autonomous MCP work corrupts a level and
recovery via git would have been painful. Until that happens in practice,
version control is the safety net.

### MCP resources

Expose ambient state as MCP `resources` rather than tools:
`unreal://level/current`, `unreal://selection/actors`,
`unreal://viewport/state`, `unreal://pie/state`. LLMs pull context
without burning a tool call per read.

**Trigger:** observed pattern of LLMs making >5 tool calls per response
just to read state. The existing read tools (`get_current_level`,
`get_selected_actors`, `find_actors`, ...) might be enough; resources are
only worth the surface complexity if context-fetching becomes a hot
path.

### Tool manifest

YAML/declarative metadata per tool — drives generated docs, validation,
preset membership, scope gating.

**Trigger:** tool count exceeds ~150 OR the docstrings start drifting
from reality (an empirically-observed pattern, not a theoretical risk).
At 102 tools, manageable from docstrings alone.

### Structured errors

`{code, message, details, suggestions, retryable}` instead of plain
`{error: "..."}` strings.

**Trigger:** an autonomous-loop workflow keeps tripping on the same
unstructured error and needs better classification to retry
intelligently. Currently the plain-string errors are sufficient for
human-supervised use.

### Tool presets and scope gates

`minimal`, `scene`, `assets`, `runtime`, `full` — present a smaller
surface to the LLM by default to reduce noise, and gate destructive
tools behind explicit opt-in.

**Trigger:** observed pattern of the LLM consistently picking the wrong
tool from the 102-tool list (a discoverability problem), OR a specific
request for safer-by-default behavior in autonomous loops.

### Generalized batch ops

Right now `spawn_actor_batch`, `delete_actor_batch`,
`move_actor_to_folder_batch` exist. A generalized `batch(commands=[...])`
that runs an arbitrary list of MCP commands in one round-trip would let
the agent compose multi-command sequences without N socket round-trips.

**Trigger:** observed LLM workflow that benefits from atomic sequences
+ all-or-nothing semantics. Currently each command is one round-trip
and the editor TCP overhead is ~50 ms — annoying but not painful.

---

## Deferred indefinitely

These are real Unreal subsystems that COULD have tool surfaces. They're
not on the active list because adding them speculatively creates
maintenance burden across UE version bumps for capabilities we don't
have a concrete workflow for.

| Subsystem | Note |
|---|---|
| Sequencer / cinematics | No concrete request for editor-driven cinematic authoring. |
| Niagara / VFX | Heavy editor surface; not worth speculative coverage. |
| MetaSound | Same. |
| Landscape / foliage / PCG | World-building authoring — heavy surface, narrow audience. |
| GAS / Behavior Trees / EQS / StateTree | AI/gameplay framework support — each is a substantial sub-project. |
| Native HTTP MCP inside the plugin | TCP + JSON works fine for current scale; HTTP is a research project. |
| Asset-editor parity (static mesh / texture / skeletal mesh property surfaces beyond `static_mesh_get_info`) | Each editor is its own world; speculative coverage hard to maintain. |

If real-use surfaces a gap that any of these would close, that becomes a
trigger and the item migrates to "On hold" or "Open work."

---

## Anti-goals

Explicit "we will NOT do this":

- **Tool-count parity with other MCP servers.** Whatever their counts,
  whatever their goals, those aren't ours. We have a surface that works
  for our use; that's enough until something breaks against it.

- **Speculative breadth.** "Adding asset-editor tools because we might
  need them" — no. Maintenance burden scales linearly with surface area;
  ship value, not surface.

- **Multi-version roadmap locking us into commitments before real-use
  feedback arrives.** The "v0.9 → v0.10 → v0.11" framing was useful for
  v0.8.0 because we had a concrete architectural target; we don't right
  now. When we do, we'll write a focused v0.9 plan with a clear scope.

- **Multi-agent coordination overhead.** Two attempts at splitting work
  with Codex (a v0.9 plan, a Phase 2b migration split) consistently cost
  more in coordination than they saved in execution. Solo-maintainer
  project, solo-agent execution.

- **Architecture redesign without a concrete failure mode driving it.**
  The current architecture (`FMCPRegistry` + `@unreal_tool` decorator +
  strict wire format + per-file self-registration) works. Don't touch it
  unless a real workflow breaks against it.

- **Markdown documentation that duplicates docstring content.** The
  docstrings ARE the docs. Don't write `docs/tools/spawn_actor.md` —
  write a good docstring in `editor_tools.py`.

---

## How to propose a new tool

If you're adding a tool (yourself or as a contributor):

1. **Pick a real workflow it would unlock.** "I'm doing X, and I keep
   hand-writing the same Python in `execute_python` to do Y" is a
   concrete-need signal. "Other tools have this" is not.

2. **Decide if it deserves a typed wrapper or just `execute_python`.**
   If it's a one-off, `execute_python(unsafe=True, ...)` is the escape
   hatch. If you'd use it ≥10 times across different workflows, it's
   worth a typed wrapper.

3. **Pick the home file.** Match the category map above. New categories
   need their own `server/tools/<x>_tools.py` + a `.cpp` file at
   `plugin/Source/UnrealMCP/Private/Commands/`. New categories are rare;
   most additions slot into existing files.

4. **Match the pattern.** `@unreal_tool(mcp)` if the wire response is the
   strict `{success, error?, ...payload}` shape; `@mcp.tool()` + raw
   `dispatch_unreal_command` otherwise (image returns, Python-side
   composites, the `execute_python` escape hatch). On the C++ side: free
   function in an anonymous namespace + `REGISTER_MCP_COMMAND` at the
   bottom of the same `.cpp`.

5. **Write a real docstring.** What it does, args, response shape with
   examples, failure modes. This is what the LLM sees; vague docstrings
   produce confused tool calls.

6. **Smoke test.** Classify the command in `server/smoke_dispatch.py`.
   Read-only commands can join the safe default allowlist; mutating commands
   remain skipped unless `--allow-mutating` is passed. Targeted integration
   test in `server/tests/` if the behavior is non-trivial.

7. **Bump the CHANGELOG.** A line under the next release's section.
   Don't write a separate plan doc.

---

## Decision log

When a feature ships, gets deferred, or gets killed, capture WHY here so
future-us doesn't spend hours rediscovering the reasoning.

### 2026-06-02: Smoke tests are safe by default

**Decision:** `server/smoke_dispatch.py` only dispatches an explicit
safe/read-only allowlist by default. Mutating, saving, viewport/UI,
screenshot, PIE-control, and code-execution tools are skipped unless the
caller passes `--allow-mutating`; lifecycle commands like `start_pie`,
`stop_pie`, `pie_apply_movement`, and `recompile_live` are still skipped
for empty-param smoke even with that opt-in.

**Why:** Empty-param dispatch proved that handlers are registered, but it
also executed tools whose default arguments have real editor effects
(`create_landscape`, followed by `save_current_level`). Validation should
not modify a user's currently open project.

**Consequence:** New tools are not smoke-dispatched by default until they
are deliberately classified. `--list-only` shows selected and skipped
commands without opening a socket, and `tests/test_smoke_dispatch.py`
guards the classification rules.

### 2026-05-31: Single-maintainer execution

**Decision:** Drop multi-agent coordination model. All MCP development
proceeds single-maintainer.

**Why:** Two attempts to coordinate with Codex (a v0.9 tool-parity plan,
a Phase 2b architecture-migration split) consistently showed
coordination overhead (writing plan docs, agreeing on protocols,
sequencing commits) exceeded execution savings. For a solo-maintainer
project the bottleneck is decisions-per-hour, not keystrokes-per-hour;
adding another agent doesn't move the bottleneck, it adds a translation
layer.

**Consequence:** Three plan documents deleted (`architecture-v0.8-plan.md`
served its purpose during the v0.8.0 sprint and is now historical
clutter; `architecture-v0.8.x-phase2b-codex-plan.md` and
`architecture-v0.9-tool-parity-plan.md` were coordination scaffolding
with no constituency). This `roadmap.md` is the single replacement doc.

### 2026-05-31: v0.9 scope explicitly undecided

**Decision:** No v0.9 milestone in the roadmap. Existing features go
into the "On hold" list with explicit triggers; the next major release
gets a focused plan when real-use feedback dictates the scope.

**Why:** The deleted Codex tool-parity plan was StraySpark-shaped:
optimize for breadth and feature checkboxes against a competitor. That's
a SaaS-product framing, not a side-project's. We don't know yet what
real-use feedback will surface as the right v0.9 scope; pretending we do
is fiction. Writing a multi-version roadmap before that signal arrives
locks us into commitments we'll regret.

**Consequence:** No "v0.9", "v0.10", ... section in this doc. Just the
present and the hypotheses. The release-numbering decision waits for the
next concrete capability.

### 2026-05-31: Architecture migration complete

**Decision:** Lift all 96 command handlers from the v0.7-era
`FUnrealMCP<X>Commands` classes into anonymous-namespace free functions
with `REGISTER_MCP_COMMAND` self-registration at the definition site.
The 9 wrapping classes + their headers deleted; `MCPRegistrations.cpp`
reduced from 222 LOC to 118 LOC. Net ~900 LOC of dispatch boilerplate
removed.

**Why:** The v0.8.0 architecture plan §3.7 said "adding a tool touches
one file." Until this migration, that wasn't true — every new command
required edits in `MCPRegistrations.cpp` AND the relevant wrapping class
AND the wrapping class's header. Now it really is one file.

**Consequence:** The "how to add a new tool" recipe (above) became
simpler. The dispatch layer is now stable; future churn happens in
handler files, not in the central registry plumbing.

---

## How to use this doc

- **Reading it for the first time?** Start at "Current state" and
  "How to propose a new tool."
- **Considering adding a new MCP capability?** Check "On hold" — if it's
  there, the trigger hasn't fired yet. If it's in "Deferred
  indefinitely," explain in your PR what changed.
- **About to write another `architecture-vXxx-plan.md`?** Don't. Add a
  section here, or extend the relevant CHANGELOG entry.
- **Killed a feature?** Add it to "Decision log" with the WHY.
