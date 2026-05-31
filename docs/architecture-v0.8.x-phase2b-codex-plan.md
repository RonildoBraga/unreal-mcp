# Phase 2b — Blueprint + Node migration (Codex's half)

**Status:** Plan ready for Codex execution. Claude is handling the Editor +
Asset half of Phase 2b in parallel/sequence.
**Revision:** 2026-05-31

---

## 0. Context

Phase 1 of the architecture migration shipped 4 categories
(`project`, `level`, `material`, `outliner`). Phase 2a shipped UMG. The
remaining 4 categories are split for Phase 2b:

- **Claude:** Editor (33 handlers, ~2700 LOC) + Asset (16 handlers, ~1100 LOC)
- **Codex:** Blueprint (7 handlers, ~1000 LOC) + Node (8 handlers, ~600 LOC)

Total Codex scope: **15 handlers across ~1600 LOC**.

This document captures the pattern, the files to touch, the edge cases, and
the validation contract.

---

## 1. The pattern (codified from 5 reference migrations)

Each category's `.cpp` becomes the canonical structure shown below. The
wrapping class disappears, the `HandleCommand` if/else dispatcher
disappears, and each command's handler self-registers at the bottom of the
same file via `REGISTER_MCP_COMMAND`.

```cpp
// v0.8.x §6.2 completion -- <category> command handlers, lifted out of the
// v0.7-era FUnrealMCP<X>Commands class.

#include "Commands/UnrealMCPCommonUtils.h"
#include "MCPRegistry.h"

// ... category-specific includes (UE module headers) ...

namespace
{

// ─── Private helpers ───────────────────────────────────────────────────────
// (Move any helper functions that the old class had as static utilities,
// or that lived in an inner anonymous namespace, into this single outer
// anonymous namespace.)

UEdGraph* SomeGraphHelper(/* ... */) { /* ... */ }


// ─── Handlers ──────────────────────────────────────────────────────────────
// Strip the `FUnrealMCP<X>Commands::` qualifier from every former method
// definition. Bodies stay identical otherwise.

TSharedPtr<FJsonObject> HandleSomeCommand(const TSharedPtr<FJsonObject>& Params)
{
    // ... body verbatim from FUnrealMCP<X>Commands::HandleSomeCommand ...
}

// ... more handlers, one per former private method ...

}  // anonymous namespace


// ─── Self-registration at definition site ──────────────────────────────────

REGISTER_MCP_COMMAND("some_command", &HandleSomeCommand);
REGISTER_MCP_COMMAND("another_command", &HandleAnotherCommand);
// ... one REGISTER_MCP_COMMAND line per command name ...
```

### What goes away

| Before | After |
|---|---|
| Header `UnrealMCP<X>Commands.h` declaring the class | **Deleted** |
| `FUnrealMCP<X>Commands::FUnrealMCP<X>Commands() {}` ctor | **Deleted** |
| `FUnrealMCP<X>Commands::HandleCommand(...)` if/else dispatcher | **Deleted** |
| `RegBatch<FUnrealMCP<X>Commands>({...})` in `MCPRegistrations.cpp` | **Deleted**, replaced by a one-line pointer comment |

### What stays

- Every handler function body — VERBATIM. Don't rewrite, don't refactor,
  don't "improve". The whole point is zero behavior change.
- Private helper functions from inside the class (move them into the
  anonymous namespace).
- Per-handler `#include`s — keep them all; some are subtle (e.g. `K2Node_*`
  headers).

---

## 2. Files Codex touches

### Codex creates / overwrites

```text
plugin/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintCommands.cpp
plugin/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintNodeCommands.cpp
```

### Codex deletes

```text
plugin/Source/UnrealMCP/Public/Commands/UnrealMCPBlueprintCommands.h
plugin/Source/UnrealMCP/Public/Commands/UnrealMCPBlueprintNodeCommands.h
```

### Codex edits (light, surgical — DO NOT touch Editor or Asset blocks)

```text
plugin/Source/UnrealMCP/Private/MCPRegistrations.cpp
```

In that file, remove ONLY these specific things:

1. The include lines
   ```cpp
   #include "Commands/UnrealMCPBlueprintCommands.h"
   #include "Commands/UnrealMCPBlueprintNodeCommands.h"
   ```
2. The two `RegBatch` calls
   ```cpp
   RegBatch<FUnrealMCPBlueprintCommands>({...});
   RegBatch<FUnrealMCPBlueprintNodeCommands>({...});
   ```
   Replace each with a one-line pointer comment, matching the Phase 1 style:
   ```cpp
   // blueprint.* self-registers in Commands/UnrealMCPBlueprintCommands.cpp.
   // blueprint_node.* self-registers in Commands/UnrealMCPBlueprintNodeCommands.cpp.
   ```

**DO NOT touch the Editor or Asset blocks in `MCPRegistrations.cpp`.** Claude
will handle those.

### Mirror to lauder3

Every change made in `plugin/` must be mirrored to the equivalent path under

```text
lauder3/Lauder/Plugins/UnrealMCP/Source/UnrealMCP/
```

The lauder3 project pins the plugin source as a tracked snapshot. Without
the mirror the editor builds against stale C++.

---

## 3. Blueprint specifics

### Commands to migrate (7)

```text
create_blueprint
add_component_to_blueprint
set_component_property
set_physics_properties
set_static_mesh_properties
set_pawn_properties
set_blueprint_property
compile_blueprint
```

Wait — there are actually 8 if-branches in the existing
`FUnrealMCPBlueprintCommands::HandleCommand`. The bridge dispatcher does
NOT route `spawn_blueprint_actor` here (it goes to Editor instead). Confirm
by inspecting the existing `HandleCommand` and only register the 7 that the
dispatcher actually handles + any extra commands in the class.

### Edge cases

1. **`FUnrealMCPCommonUtils::SetObjectProperty` callers.** Five of the
   Blueprint handlers — `HandleSetBlueprintProperty`,
   `HandleSetPawnProperties`, `HandleSetStaticMeshProperties`,
   `HandleSetPhysicsProperties`, `HandleAddComponentToBlueprint` — call
   `FUnrealMCPCommonUtils::SetObjectProperty(...)`. That function still
   exists (Day 5 reduced it from 352 lines to a 12-line thin wrapper over
   `WalkPropertyPath` + `SetPropertyAtTarget`, but the external contract is
   identical). **No changes at those call sites.**

2. **Component template + CDO writes** in these handlers are intentionally
   transactionless. The v0.9 plan will add `FScopedTransaction` wrapping
   later; do NOT add it here. Behavior-preserving migration only.

3. **The `set_pawn_properties` handler in particular** has a couple of
   array-iteration patterns that need to stay intact. Trust the existing
   code; copy it verbatim.

### Registration order

When you write the `REGISTER_MCP_COMMAND` block at the bottom, preserve the
exact order from the existing `RegBatch` call in `MCPRegistrations.cpp` (or
the `HandleCommand` if/else, since they should match). The order doesn't
matter functionally but it makes the diff easier to review.

---

## 4. Blueprint Node specifics

### Commands to migrate (8)

```text
connect_blueprint_nodes
add_blueprint_get_self_component_reference
add_blueprint_self_reference
find_blueprint_nodes
add_blueprint_event_node
add_blueprint_input_action_node
add_blueprint_function_node
add_blueprint_variable
```

### Edge cases

1. **`UK2Node_*` types** — these are in `BlueprintGraph` module (already in
   `Build.cs`'s `PrivateDependencyModuleNames`). No build glue needed.

2. **`UEdGraph` pin connections** via `FBlueprintEditorUtils` and
   `MakeLinkTo`. These are stable across UE 5.7. No surprises expected.

3. **`add_blueprint_variable`** has a switch on the `variable_type` string
   ("Float", "Boolean", "Integer", "Vector", "String", "Object", etc.) that
   maps to `FEdGraphPinType`. Keep the switch verbatim.

4. **`find_blueprint_nodes`** uses `FBlueprintEditorUtils::GetAllNodesOfClass`
   with templated `UK2Node_*` classes. The template instantiations stay as
   in the existing code.

---

## 5. Validation contract

After Codex's commit + Claude's commit + a merge of both branches into
`main` + a full UBT rebuild:

```bash
# From the unreal-mcp repo root:
cd server

# 1. Dispatch smoke -- proves every wire command name registered
./.venv/Scripts/python smoke_dispatch.py
# Expect: "=== 95/95 dispatched, 3 TCP-race confirmed, 0 unconfirmed, 0 unknown ==="

# 2. Targeted integration tests
./.venv/Scripts/python -m pytest tests/
# Expect: "21 passed in <Ns>"
```

The "3 TCP-race confirmed" is the pre-existing Windows TCP RST-after-FIN
race on `focus_selected_actors`, `pie_get_player`, `pie_set_player` — not
introduced by this migration.

Cold-start log line check:

```bash
grep "commands auto-registered" lauder3/Lauder/Saved/Logs/Lauder.log | tail -1
# Expect: "[UnrealMCP] 9X commands auto-registered at DLL load"
# (Exact count varies with static-init order; ≥ 90 is fine. The smoke
# verifies the FULL count is correct.)
```

If any of the above fails — `Unknown command:` in smoke, a pytest fail, or
the UBT build erroring out — **do not push.** Open a discussion with
Claude about the failure pattern; we've debugged similar issues before
(see the GUID-map fix in UMG `add_widget_to_tree`).

---

## 6. Coordination with Claude

Sequential workflow agreed (Claude does Editor + Asset first):

```text
Time 0   Claude commits Phase 2b-claude (Editor + Asset) to main, pushes.
Time 1   Codex pulls main, starts work on Blueprint + Node.
Time 2   Codex commits Phase 2b-codex (Blueprint + Node) to main, pushes.
Time 3   User closes editor.
Time 4   User runs UBT (Claude triggers it via background pipeline).
Time 5   Build succeeds, user reopens editor.
Time 6   smoke_dispatch.py + pytest run, both green.
Time 7   #66 marked completed. Architecture migration done.
```

If Codex starts before Claude finishes, the `MCPRegistrations.cpp` hunks
will conflict — wait for Claude to push first.

---

## 7. Commit message template

```text
refactor: v0.8.x §6.2 Phase 2b-codex — architecture migration (Blueprint, Node, 15 handlers)

Continues Codex follow-up #64 / Task #66 — lifts the Blueprint and
Blueprint-node command handlers out of FUnrealMCPBlueprintCommands and
FUnrealMCPBlueprintNodeCommands into anonymous-namespace free
functions with REGISTER_MCP_COMMAND self-registration at definition
site. Same pattern as Phase 1 (project, level, material, outliner)
and Phase 2a (UMG).

Handlers migrated:
  - Blueprint (7): create_blueprint, add_component_to_blueprint,
    set_component_property, set_physics_properties,
    set_static_mesh_properties, set_pawn_properties,
    set_blueprint_property, compile_blueprint
  - Node (8): connect_blueprint_nodes,
    add_blueprint_get_self_component_reference,
    add_blueprint_self_reference, find_blueprint_nodes,
    add_blueprint_event_node, add_blueprint_input_action_node,
    add_blueprint_function_node, add_blueprint_variable

Validation:
  - UBT clean rebuild.
  - 15/15 migrated wire command names verified in linked DLL.
  - smoke_dispatch.py: 95/95 dispatched, 0 unknown.
  - pytest tests/: 21/21 pass.

Wrapping classes + headers deleted; corresponding RegBatch entries
removed from MCPRegistrations.cpp; lauder3 plugin snapshot mirrored.

This commit + Claude's Phase 2b-claude commit complete the §6.2
architecture migration. All 9 categories now use the self-registration
pattern; MCPRegistrations.cpp is reduced to its minimum surface (just
the `ping` virtual command + the FAutoRegistrar shell).

Co-Authored-By: Codex <noreply@openai.com>
```

Adjust as needed.

---

## 8. Open questions for Codex

If anything in this doc is unclear, or you spot an edge case I missed, raise
it before starting the migration. The biggest risks are:

1. **A handler that references `this->`** in a way I didn't catch. If you
   see any `this->`-qualified access inside a `FUnrealMCPBlueprint*::
   Handle*` body, flag it — it shouldn't be there in our current code, but
   it would block the lift to a free function.
2. **A private helper method on the wrapping class** (e.g. some
   `FUnrealMCPBlueprintCommands::FindNodeByTitle(...)` that's NOT a
   handler). If you find one, lift it into the anonymous namespace alongside
   the handlers — same pattern.
3. **A static class member.** Don't think there are any, but worth checking
   while you're in the file.

Otherwise: trust the pattern. Five categories landed clean with it. The
review burden is on the diff being non-behavior-changing, which the smoke
+ pytest validate end-to-end.

Good luck.
