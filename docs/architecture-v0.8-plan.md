# v0.8.0 — MCP as programmatic editor (architecture plan)

**Status:** Draft for review. No code lands until §8 approved.
**Revision date:** 2026-05-31

---

## 0. Guiding principle (set by user)

> *The Unreal Editor is a UI that lets the user click, navigate, inspect, and modify the project. The MCP should give Claude programmatic equivalents for the same operations — inspect, navigate, query, create, modify, manage — without requiring UI interaction.*

This is the load-bearing constraint. Everything below is derived from it.

Three corollaries:

1. **The module structure should mirror the editor's capability surface**, not Claude's anticipated needs. A capability has a home regardless of whether we've built it yet.
2. **Files exist only when they have working code.** Comprehensive design, lean implementation. No empty placeholders.
3. **For every editor verb, the MCP exposes the equivalent programmatic verb.** The verbs are: *navigate, inspect, query, create, modify, delete, organize, run, observe*. If the editor lets you do it, MCP should let Claude do it.

Single user, no compat burden, no deprecation policy — dead code is deleted on sight.

---

## 1. The editor surface (what the user can do in the Unreal Editor)

This is the complete enumeration. Asterisks mark what current MCP covers.

### 1.1 Content Browser
- Navigate folder hierarchy
- View assets (thumbnails, list)
- Filter by class / tag / path
- Search by name / metadata *
- Right-click → rename / duplicate / delete / move *
- Right-click → migrate to other project *
- Right-click → find references *
- Right-click → reference viewer / size map
- Drag-drop into viewport (placement)
- Import new assets (drag-drop from OS) *
- Open an asset in its editor (double-click)
- Focus on an asset (show in browser)
- Asset class viewer

### 1.2 World Outliner
- List actors in level *
- Folder hierarchy *
- Group by class / type
- Select actors *
- Right-click → focus / delete / move folder *
- Drag-reparent

### 1.3 Details panel (for any selected UObject)
- View every UPROPERTY
- Edit every editable UPROPERTY *
- Reset to default
- Copy / paste values
- Browse object references
- Function picker (for UFunction-typed properties)

### 1.4 Viewport (Perspective / Orthographic)
- Move camera (WASD, RMB drag) *
- View modes (Lit, Unlit, Wireframe, etc.) *
- Toggle individual show flags (Sprites, Grid, BSP, etc.)
- Screenshot *
- Transformation gizmos (translate, rotate, scale)
- Snap-to-grid / snap-to-surface
- Set viewport bookmarks (Ctrl+1..0)
- Focus selected (F key)
- Frame actor (auto-fit bounds)
- Game View toggle (G key)
- Stat overlay (`stat fps`, etc.) *

### 1.5 Modes (Place / Select / Modeling / Landscape / Foliage)
- Switch active mode
- Place mode: drag assets from browser
- Select mode: marquee, click selection *
- Modeling mode: extrude, bevel, boolean
- Landscape mode: sculpt, paint
- Foliage mode: paint instance density

### 1.6 Play / Simulate
- Start PIE *
- Stop PIE *
- Eject (possess player vs editor camera)
- Pause / step
- Standalone game launch
- Get/set player state *
- Send player input *
- Take in-game screenshot *

### 1.7 Output Log
- Read messages *
- Filter by category / verbosity
- Search
- Clear
- Save to file

### 1.8 World Settings (per-level)
- GameMode override
- Default game type
- Kill Z
- Gravity
- Time settings
- Lightmass

### 1.9 Project Settings (project-wide)
- Engine settings (rendering, physics, audio)
- Game settings (default maps, game modes)
- Editor settings
- Plugins page (enable/disable)
- Input mappings (Enhanced + legacy) *
- Packaging / cooking

### 1.10 Editor Preferences (per-user, separate from project)
- Theme
- Hotkeys
- Loading & saving behavior
- Source control

### 1.11 Window menu (panels and layout)
- Show/hide individual panels
- Save / load layout

### 1.12 Build menu
- Build lighting
- Build geometry
- Build navigation
- Build HLODs
- Cook content
- Package project

### 1.13 Tools menu
- Asset audit
- Statistics
- Reference viewer
- Size map
- Find in Blueprints
- Cook content

### 1.14 Source Control panel (when configured)
- Check out / check in
- Status, history
- Branch / merge (Perforce only)

### 1.15 Asset editors (per-type)
- **Static Mesh editor** — LODs, collision, materials, lightmap UVs, build settings
- **Skeletal Mesh editor** — skeleton, sockets, anim retargeting
- **Material editor** — node graph for masters, parameter tuning for instances *
- **Blueprint editor** — class settings, components, variables, functions, graphs, nodes, compile *
- **UMG Widget designer** — visual layout, hierarchy, bindings *
- **Animation Blueprint editor** — state machines, transition rules, blend spaces
- **Animation Sequence editor** — keyframes, retargeting, notify tracks
- **Niagara editor** — emitter modules, system parameters
- **Sequencer** — tracks, keyframes, cameras
- **Texture editor** — settings (sRGB, compression, mip-gen)
- **Data Table editor** — row CRUD against a struct
- **Curve / Curve Atlas editor**
- **Sound Cue editor**
- **Behavior Tree editor**
- **Environment Query editor**

### 1.16 Console / debug
- Console commands *
- CVar set/get *
- Stat commands *
- Show flags via console
- Bug-it / dump cameras

### 1.17 Bookmarks, Layers, Bookmarks (organization)
- Bookmarks: viewport positions
- Layers: visibility groups (separate from Outliner folders)

That's the comprehensive editor surface. The MCP should have a home for each.

---

## 2. Current MCP coverage gap analysis

| Editor surface | Coverage | Notes |
|----------------|---------:|-------|
| Content Browser navigation | 0% | No focus, no folder navigation, no double-click-to-open |
| Content Browser asset ops (rename/move/delete/duplicate/migrate/import) | ~95% | Solid via `assets/` tools |
| Content Browser reference graph (find refs, deps) | 60% | API present, no "view" tools |
| Outliner — actor list + folders | ~80% | Need paged `find_actors`; folder ops solid |
| Outliner — selection | 0% | v0.7.12 was in flight; lands here |
| Details panel — read | ~50% | `get_actor_property` covers actors only |
| Details panel — write | ~70% | `set_actor_property` works on actors via dotted paths; broken for non-actor UObjects |
| Viewport — camera + mode + screenshot | ~95% | Solid |
| Viewport — show flags | 0% | Console-only workaround |
| Viewport — bookmarks | 0% | |
| Viewport — frame actor / focus selected | 0% | |
| Modes (Place/Select/Modeling/Landscape/Foliage) | 0% | None |
| PIE | ~80% | Movement is `AddMovementInput` only; no Enhanced Input dispatch |
| Output Log | 50% | Tail works; no filter/search |
| World Settings | 0% | None (despite this being CRITICAL — Lauder lost half a day to GameMode override) |
| Project Settings (INI) | 0% | None |
| Editor Preferences | 0% | None |
| Window/Layout | 0% | None (probably out of scope forever) |
| Build menu | 0% | None |
| Tools menu | 0% | None |
| Source Control | 0% | None |
| Static Mesh editor | 0% | No metadata, no settings |
| Skeletal Mesh editor | 0% | None |
| Material editor | ~70% | Instance tuning solid; master material editing absent |
| Blueprint editor | ~70% | Class + node CRUD solid; some node types missing |
| UMG Designer | ~85% | Solid for current widget needs |
| Anim BP editor | 0% | None |
| Anim Sequence editor | 0% | None |
| Niagara | 0% | None |
| Sequencer | 0% | None |
| Texture editor | 0% | None |
| Data Table editor | 0% | None |
| Console / debug | ~90% | Solid |
| Bookmarks | 0% | |
| Layers | 0% | |

**Coverage today: ~35% of the editor surface.** Plenty of high-value gaps.

---

## 3. Design principles

Locked. Will not be revisited mid-implementation.

1. **One responsibility per file.** A file is named for one editor surface and only handles operations in that surface.
2. **Self-registering commands.** Adding a tool touches one file. Bridge dispatch is reflection-driven (a map), not a hand-maintained allowlist.
3. **Typed parameter access.** One line per parameter, consistent errors.
4. **Reflection over allowlists.** Use `FindObject<UClass>`, `FProperty` iteration, `UFunction` reflection where available. Hardcoded type switches age badly.
5. **Symmetric read/write.** Every `set_X` ships with `get_X` in the same change.
6. **Verb consistency across surfaces.** Same verbs for the same kinds of operations: `list`, `get`, `find`, `set`, `create`, `delete`, `move`, `rename`, `duplicate`. Avoid synonyms (no `query_*` for `find_*`).
7. **Generalize the UObject surface.** `get/set_actor_property` becomes a special case of `get/set_object_property` that works on any UObject path (asset, component, actor). Solves the Details-panel parity problem for free.
8. **Lean files, comprehensive structure.** The folder tree covers the full editor surface so future additions have homes. Empty folders / placeholder files are forbidden.

---

## 4. Target architecture

### 4.1 C++ layout — full editor surface mapped

Folder tree is comprehensive (covers everything in §1). Files exist only where there is working code.

```
plugin/Source/UnrealMCP/
├── UnrealMCP.Build.cs
├── UnrealMCP.uplugin
├── Public/
│   ├── MCP.h
│   ├── MCPBridge.h
│   ├── MCPParams.h               — typed param access
│   ├── MCPResponse.h             — strict {success, error?} builder
│   ├── MCPRegistry.h             — REGISTER_MCP_COMMAND
│   └── Reflection/
│       ├── PropertyWalker.h
│       ├── ClassLookup.h
│       ├── FunctionInvoker.h     — call UFunctions
│       └── ObjectLookup.h        — resolve actor name OR /Game/ path → UObject*
└── Private/
    ├── MCP.cpp
    ├── MCPBridge.cpp             — thin dispatch (~25 LOC)
    ├── MCPServerRunnable.cpp
    ├── MCPParams.cpp
    ├── MCPRegistry.cpp
    ├── Reflection/
    │   ├── PropertyWalker.cpp    — lifted from v0.7.10 CommonUtils
    │   ├── ClassLookup.cpp
    │   ├── FunctionInvoker.cpp   [v0.8.0 if scope permits — otherwise post-v0.8.0]
    │   └── ObjectLookup.cpp
    │
    │   ─── Content Browser (assets/) ─────────────────
    ├── Assets/
    │   ├── Registry.cpp          [v0.8.0] list, find, info, deps, refs
    │   ├── Mutations.cpp         [v0.8.0] move, delete, rename, duplicate
    │   ├── Migration.cpp         [v0.8.0] migrate, finalize_migration + modal handling
    │   ├── Import.cpp            [v0.8.0] generic import
    │   ├── Browser.cpp           [v0.8.0] focus_in_browser, navigate_to_folder, open_in_editor
    │   ├── Materials.cpp         [v0.8.0] MI tuning + parent + uses
    │   ├── StaticMesh.cpp        [v0.8.0] get_info (bounds, slots, LODs)
    │   ├── Blueprint.cpp         [v0.8.0] class CRUD + compile (lifted from blueprint_tools)
    │   ├── BlueprintNodes.cpp    [v0.8.0] graph node CRUD (lifted from node_tools)
    │   ├── Widget.cpp            [v0.8.0] UMG widget BPs + bindings (lifted from umg_tools)
    │   ├── SkeletalMesh.cpp      [post-v0.8.0]
    │   ├── Texture.cpp           [post-v0.8.0]
    │   ├── DataTable.cpp         [post-v0.8.0]
    │   ├── AnimBlueprint.cpp     [post-v0.8.0]
    │   ├── AnimSequence.cpp      [post-v0.8.0]
    │   ├── Niagara.cpp           [post-v0.8.0]
    │   ├── Sequencer.cpp         [post-v0.8.0]
    │   └── SoundCue.cpp          [post-v0.8.0]
    │
    │   ─── World (loaded level) ──────────────────────
    ├── World/
    │   ├── Actors.cpp            [v0.8.0] spawn, delete, transform, find, list, batches
    │   ├── Properties.cpp        [v0.8.0] get/set on any UObject (generalized)
    │   ├── Materials.cpp         [v0.8.0] override on actors (was editor.set_static_mesh_material)
    │   ├── Outliner.cpp          [v0.8.0] folder mgmt + batches
    │   ├── Selection.cpp         [v0.8.0] get/set/clear/focus
    │   ├── PIE.cpp               [v0.8.0] lifted from v0.7.11
    │   ├── WorldSettings.cpp     [v0.8.0] GameMode override etc. — CRITICAL gap
    │   ├── Layers.cpp            [post-v0.8.0]
    │   └── Bookmarks.cpp         [post-v0.8.0] viewport bookmarks
    │
    │   ─── Editor IDE state ──────────────────────────
    ├── Editor/
    │   ├── Viewport.cpp          [v0.8.0] camera, mode, screenshot, show flags, frame_actor
    │   ├── Console.cpp           [v0.8.0] commands, cvars, log tail, async compile
    │   ├── Levels.cpp            [v0.8.0] open, save, current
    │   ├── Modes.cpp             [post-v0.8.0] Place/Select/Modeling/Landscape/Foliage
    │   └── Preferences.cpp       [post-v0.8.0]
    │
    │   ─── Project-wide ──────────────────────────────
    └── Project/
        ├── Settings.cpp          [v0.8.0] INI editing (DefaultEngine, etc.)
        ├── Input.cpp             [v0.8.0] mappings (lifted from project_tools)
        ├── Plugins.cpp           [post-v0.8.0] enable/disable
        ├── Build.cpp             [post-v0.8.0] cook/package
        └── SourceControl.cpp     [post-v0.8.0]
```

### 4.2 Python layout — mirror

```
server/
├── pyproject.toml
├── unreal_mcp_server.py          — FastMCP server + lifespan
├── _client.py                    — UnrealConnection
├── _registry.py                  — @mcp_tool decorator + auto-dispatch
├── _params.py                    — argument validation
└── tools/
    ├── _common.py                — _unwrap + send_command
    ├── assets/
    │   ├── registry.py
    │   ├── mutations.py
    │   ├── migration.py
    │   ├── import_.py
    │   ├── browser.py
    │   ├── materials.py
    │   ├── static_mesh.py
    │   ├── blueprint.py
    │   ├── blueprint_nodes.py
    │   └── widget.py
    ├── world/
    │   ├── actors.py
    │   ├── properties.py
    │   ├── materials.py
    │   ├── outliner.py
    │   ├── selection.py
    │   ├── pie.py
    │   └── world_settings.py
    ├── editor/
    │   ├── viewport.py
    │   ├── console.py
    │   └── levels.py
    └── project/
        ├── settings.py
        └── input.py
```

**v0.8.0 lands:** 4 categories × 23 modules in C++, 4 × 21 modules in Python. Everything in §1 has a documented C++ folder home for post-v0.8.0 expansion.

### 4.3 Dispatch refactor in numbers

| Metric | v0.7.12 | v0.8.0 |
|--------|--------:|------:|
| Lines in `UnrealMCPBridge.cpp` dispatch | ~150 | ~25 |
| Files touched per new tool | 3-4 | 1 |
| Param validation LOC per tool | ~10 | <3 |
| Dead C++ deleted | 0 | ~480 |
| Python wrapper boilerplate per tool | ~10 | 0 (decorator) |

### 4.4 Naming conventions (locked)

- **Commands:** `snake_case`, flat names on the wire. Module location is documentation. Verbs from the principle 6 list.
- **Parameters:** `name` (actor display label), `internal_name` (`UObject::GetName`), `path` (`/Game/`-prefixed), `location`/`rotation`/`scale` (float arrays), `folder_path` (Outliner).
- **Response:** success → `{"success": true, ...}`. Error → `{"success": false, "error": "<msg>"}`. Strict. Old `{"status": "error", "error": "..."}` shape is rewritten by the bridge; no shim.
- **C++ handlers:** `HandleXxx` where `Xxx` is the command name in PascalCase.
- **Property paths:** Same dotted syntax as v0.7.10 (`Component.Property.Sub`, `Array.0`). Applies uniformly to actors, components, assets.

---

## 5. Generalized UObject access (the §3.7 principle)

The biggest leverage in this redesign: lift `get/set_actor_property` to `get/set_object_property` that works on **any UObject**, addressed by either:

- Actor display name (existing semantics — `set_object_property(target="Altar_Lantern", path="StaticMeshComponent.Intensity")`)
- `/Game/`-prefixed asset path (`set_object_property(target="/Game/Megascans/Foo.Foo", path="BuildScale3D")`)
- Component within actor (`set_object_property(target="Altar_Lantern", path="LightComponent.Intensity")` — already works)
- Class default object (`set_object_property(target="/Script/Engine.ExponentialHeightFogComponent.Default", path="FogDensity")` — rare but legitimate)

`ObjectLookup::Resolve(target)` returns a `UObject*` from any of these. Then `WalkPropertyPath` walks the rest.

This delivers the Details-panel programmatic equivalent in one tool. Most of §1.3 is then a thin wrapper.

---

## 6. v0.8.0 scope — what we build now

Pick what's needed for current Phase 7 work + the cleanups identified in §2. Everything else is structural-only.

### 6.1 Foundations (Day 1-2)

- `MCPRegistry` + `REGISTER_MCP_COMMAND` macro
- `MCPParams` typed param access
- `MCPResponse` strict `{success, error?}` builder
- `Reflection/PropertyWalker` (lifted from v0.7.10 CommonUtils)
- `Reflection/ClassLookup` (lifted from v0.7.10 spawn_actor)
- `Reflection/ObjectLookup` (NEW — generalizes actor lookup)
- `MCPBridge` thin dispatch
- All existing 78 tools rewired through registry, command names unchanged, response shape rewritten

### 6.2 File-move-only (Day 2)

Migrate existing handlers into the new folder layout. No semantic changes.

### 6.3 New capability (Day 3-4)

Priority order based on what blocks current work:

| # | Tool | Why now |
|---|------|---------|
| 1 | `world.selection.get/set/clear/focus` (4 tools) | Already needed for RomanCave subset capture |
| 2 | `world.actors.find` (paged) | `get_actors_in_level` returns 744KB on RomanCave |
| 3 | `world.actors.{spawn,delete,move_to_folder}_batch` | Dense scene placement (RomanCave-style) |
| 4 | `world.world_settings.get/set` | Critical — Lauder lost half a day to GameMode override; project memory feedback_unreal_level_gamemode_override_hides_default.md |
| 5 | `world.properties.get/set_object_property` (generalized) | Details-panel parity for non-actor UObjects |
| 6 | `assets.browser.{focus_in_browser, navigate_to_folder, open_in_editor}` | Cooperative workflows — point user at what we're doing |
| 7 | `assets.static_mesh.get_info` | Bounds + slot count before scaling |
| 8 | `editor.viewport.{frame_actor, set_show_flag}` | Auto-fit + Game View / Sprite toggles |
| 9 | `editor.console.{wait_for_async_compile, dismiss_modal_dialog}` | Unblocks `finalize_migration` |
| 10 | `world.pie.screenshot` Python unwrap fix | One-line bug |
| 11 | `project.settings.{get_ini, set_ini}` | INI editing — DefaultEngine etc. |
| 12 | Read counterparts: `get_actor_transform`, `get_component_property`, `get_static_mesh_material` | Symmetry rule |

Total new tools in v0.8.0: **~22**. Combined with the dispatch refactor and the deletions in §7, the result is **~95 well-organized tools** in 4 clean categories.

### 6.4 Cleanup (Day 5)

- Drop `save_all_dirty`, `project_tools::create_input_mapping` (lifted into project.input), README "Fork notice" bullet enumeration
- Trim `info()` prompt to a 10-line pointer to `tools/list`
- Sweep stale comments
- v0.8.0 CHANGELOG entry

---

## 7. Hard deletions (no compat shim)

| Item | Where | LOC |
|------|-------|----:|
| `FUnrealMCPCommonUtils::SetObjectProperty` (v0.7.4 dead path) | CommonUtils.cpp | ~330 |
| Duplicate FProperty type switch in v0.7.4 path | CommonUtils.cpp | ~150 |
| Bridge if/else dispatch chain | UnrealMCPBridge.cpp | ~130 |
| `level_tools::save_all_dirty` (never used) | level_tools.py | ~25 |
| `project_tools.py` whole file (1 tool, used twice ever) | project_tools.py | ~50 |
| `take_screenshot::show_ui` parameter (advisory only) | editor + cpp | ~8 |
| Python wrapper connection/error boilerplate × 78 wrappers | tools/*.py | ~390 |
| `info()` prose prompt | unreal_mcp_server.py | ~40 |
| README "Fork notice" multi-bullet enumeration | README.md | ~12 |
| Stale pre-v0.4.0 path comments | various | scan |

**Total: ~1,100 LOC deleted.** Net for v0.8.0 (delete + add new + decorator) = **−250 LOC** while adding 22 new tools + comprehensive structural coverage.

---

## 8. Open questions for approval

Answer these and Day 1 begins.

1. **Response shape change.** Adopt strict `{"success": bool, "error"?: "..."}` everywhere, no shim. Confirm: **yes/no**.

2. **`info()` prompt.** Trim to a 10-line pointer to `tools/list`, or drop entirely? Recommendation: **keep 10-line pointer**. Confirm.

3. **Python registration via `@mcp_tool` decorator.** Slightly magic but absorbs all boilerplate. Confirm: **decorator**.

4. **Replace `get_actors_in_level` with paged `find_actors`?** Class filter, name pattern, folder filter, `limit`/`offset`. Confirm: **yes**.

5. **Smoke test.** Single integration test that pings every registered command with empty params and asserts dispatch reaches a handler (not the unknown-command fallback). Catches "did I forget to wire X". Confirm: **yes**.

6. **`take_screenshot::show_ui` drop.** Confirm: **drop**.

7. **Generalized `get/set_object_property`.** Worth the abstraction now (any UObject by name/path), or keep actor-only `get/set_actor_property` and ship object-property in a future patch? Recommendation: **do it in v0.8.0** — it's where most of §1.3 Details-panel parity comes from for ~zero extra cost.

8. **`world.world_settings`.** Highest-value missing capability not yet on the v0.8.0 list — Lauder explicitly lost dev time to a GameMode override bug. Worth landing in v0.8.0? Recommendation: **yes** — it's a thin wrapper over `AWorldSettings` and `get/set_object_property`.

9. **Bookmarks + Layers.** Both standalone files in `World/`, neither used by current work. Recommendation: **defer post-v0.8.0** — structural slot reserved, no file created.

10. **`UFunction` invocation.** Calling any BP function on any actor by name. Recommendation: **defer post-v0.8.0** — useful but not blocking.

---

## 9. What this delivers

After v0.8.0 ships:

- **MCP coverage of the editor surface jumps from ~35% to ~60%.** New surfaces (Niagara, Sequencer, etc.) slot into pre-designed homes.
- **Adding a new tool touches one file.** Bridge stays at ~25 LOC.
- **Errors are consistent.** Response shapes are stable.
- **The codebase is ~250 LOC smaller** and 23 modules cleanly organized vs 9 sprawling categories.
- **Documentation derives from code.** `tools/list` is canonical, `info()` is a 10-line pointer.
- **Future capabilities land mechanically.** When Niagara is needed, the file gets created, registers commands, and the bridge sees it without edits.

---

## 10. Risks

1. **Day 1 refactor breaks a tool I didn't realize was used.** Mitigated by the smoke test in question 5.
2. **Generalized `get/set_object_property` has edge cases with CDOs or transient UObjects.** Mitigated by keeping `get/set_actor_property` as an alias that pre-resolves the target.
3. **The dev junction breaks.** Mitigated by verifying on Day 1.
4. **Day 3-4 scope creep.** Mitigated by the §6.3 list being concrete; anything outside it is post-v0.8.0.

---

## 11. Recommendation

Read §1 (editor surface) and §2 (coverage gaps) — that's the load-bearing decision: are we building an MCP that mirrors the editor (yes), or one that mirrors anticipated Claude needs (no). Then §6.3 (new capability priorities) and §8 (open questions).

If §6.3 priorities are right and §8 questions get answered, Day 1 begins.
