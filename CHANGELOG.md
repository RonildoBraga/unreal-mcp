# Changelog

All notable changes to this fork of `chongdashu/unreal-mcp` are tracked here.

The format is loosely based on [Keep a Changelog](https://keepachangelog.com/), and the project follows informal semantic versioning until it stabilizes out of experimental status.

## [0.7.9] — 2026-05-31 — set_actor_property actually affects the scene + Vector4 + API consistency

Three fixes shipped together because Phase 7.2 of lauder3 hit all three
in the same tuning session — light/fog/exposure changes that returned
`success` but produced zero visible change in the editor viewport.

### The render-staleness bug — root cause + fix

**v0.7.5's CHANGELOG promised this and the code didn't deliver.** That
release's prose said "PostEditChangeProperty fires on the owning UObject…
so the editor knows the actor's data was modified and refreshes Details
panel / viewport." The actual `SetPropertyAtTarget` skipped the broadcast
and just returned after `SetValueAtAddress`. The renderer kept its cached
scene-proxy values; the FProperty held the new ones; nothing reconciled
until the editor restarted.

Phase 7.2 caught this in the worst-case way: a dozen consecutive writes
(fog density, sky source type, exposure clamps, directional light color
and intensity, post-process volume settings) all returning `success` while
the screenshot stayed pixel-identical. Confirmed via delete-actor sanity
test (delete propagated correctly; only `set_actor_property` was broken).

`SetPropertyAtTarget` now does what v0.7.5 advertised — after
`SetValueAtAddress` succeeds, it constructs a `FPropertyChangedEvent`
with the **leaf** FProperty and `EPropertyChangeType::ValueSet`, then
calls `OwningObject->PostEditChangeProperty(Event)`. For
`UActorComponent` subclasses (which is most of what people set via this
path — lights, fog components, post-process volumes, primitive
components) PostEditChangeProperty internally fires
`MarkRenderStateDirty`, which is the exact hook the rendering thread
listens on. The next viewport draw picks up the new value.

### `FVector4` struct support

The walker handled `FVector`, `FRotator`, `FLinearColor`, `FColor` — but
not `FVector4`, which UE5 uses for some `FPostProcessSettings` channels
(`ColorSaturation`, `ColorContrast`, `ColorGain`, `ColorOffset`, etc. —
the entire ACES color-grading panel). Setting
`Settings.ColorSaturation` as a four-vector previously errored with
"Unsupported struct 'Vector4'"; users had to drill into individual
components (`Settings.ColorSaturation.W`) one float at a time.

Added a single block in `SetValueAtAddress` checking
`TBaseStructure<FVector4>::Get()` and parsing the same `[x,y,z,w]` array
shape (W defaults to 1.0 if omitted). Slot-in alongside the existing
struct cases.

### `move_actor_to_folder` API consistency

The tool took `actor_name` while every other actor-targeting tool
(`spawn_actor`, `delete_actor`, `set_actor_property`,
`get_actor_properties`, `set_actor_transform`) uses `name`. Easy paper
cut — when a workflow chained these tools, the parameter name suddenly
flipped once and produced a confusing Pydantic validation error.

Renamed to `name` in:
- the Python tool signature + docstring + JSON payload key
- the C++ handler's `TryGetStringField` lookup + error message + result
  field

No deprecation alias kept — this fork's API is private and v0.7.x is
explicitly experimental.

### Why this matters

Three real road-blocks for downstream work — chiefly Phase 7.2's
visual-quality gate, where 90% of the tuning surface lives in
`PostProcessSettings.*`, `LightComponent.*`, `FogComponent.*`. v0.7.5
opened the door (dotted-path traversal); v0.7.9 actually walks through
it.

Verified: full UBT rebuild of `LauderEditor` passes after the changes.

## [0.7.8] — 2026-05-31 — Cleanup pass (dead code + dedup + doc sync)

No new functionality. Post-Sprint-2 audit pruned material that had drifted:

### Dead code removed
- `FUnrealMCPBlueprintCommands::HandleSpawnBlueprintActor` and its dispatch
  branch + private declaration. The bridge routes `spawn_blueprint_actor`
  exclusively to `FUnrealMCPEditorCommands`; this category's copy was
  unreachable and at drift risk from the live one. ~60 lines removed.
- Dead `add_blueprint_get_component_node` bridge routing — the command
  name is not handled by any category, so a stray external call would
  produce a confusing nested-error response. Routing line removed.

### Python dedup
- New `server/tools/_common.py` exporting `_unwrap`. The four-way duplicate
  in `asset_tools.py`, `level_tools.py`, `material_tools.py`,
  `outliner_tools.py` consolidated to one import. ~30 lines removed.

### Build.cs slim
- Dropped unused module deps: `HTTP`, `KismetCompiler`, `PropertyEditor`,
  `ToolMenus`, `BlueprintEditorLibrary`. Verified via grep across
  `plugin/Source/UnrealMCP/` — no reference to any API from these modules.
- Dropped the stale `#include "Http.h"` in `UnrealMCPBridge.h`.

### Stale documentation sync
- `README.md`, `docs/tools/README.md`: tool count v0.3.0 / ~54 → v0.7.7 / ~69,
  category table re-aligned with shipped reality (Material + Outliner
  rows added; capability summaries reflect v0.7.x power tools).
- `docs/architecture.md` ASCII diagram: added MaterialCommands +
  OutlinerCommands hops on the C++ side; added `_common` and the two
  new tool modules on the Python side.
- `CONTRIBUTING.md`: paths corrected for the v0.4.0 restructure
  (`Python/tools/...` → `server/tools/...`, `MCPGameProject/Plugins/...`
  → `plugin/...`).
- `plugin/UnrealMCP.uplugin`: VersionName 1.0 → 0.7.7, Version 1 → 8,
  CreatedBy / DocsURL / SupportURL filled in.
- `sample/Config/DefaultEngine.ini`: removed dead `MCPGameProject` actor
  redirect (pre-v0.4.0 leftover; sample module is now `UnrealMCPSample`).
- `unreal_mcp_server.py` `info()` prompt: replaced the stale ~25-tool
  hand-maintained list with a one-screen overview + pointer to
  `tools/list` for the authoritative catalog.

### Why this matters

These items were all from before v0.4.0 (the plugin / server / sample
restructure) or v0.5+ (Material / Outliner additions). They created small
"is this still real?" papercuts for anyone reading the code or docs. Single
cleanup commit clears the deck before Sprint 3 work begins.

Verified: full UBT rebuild of `LauderEditor` passes after the changes.

## [0.7.5–0.7.7] — 2026-05-31 — Struct traversal, viewport mode, screenshot redraw + introspection

Three closely-coupled patches landing together because Phase 7.2 of lauder3
exposed all of them in the same iteration loop. Plus two bonus
introspection tools so future Sprint 3 work has a runway.

### v0.7.5 — `WalkPropertyPath` struct traversal

The v0.7.4 walker only stepped through `FObjectProperty` hops. Real-world
editor paths often go through `FStructProperty` hops too — most importantly
`APostProcessVolume::Settings.*` (the whole visual-look surface),
`USkyAtmosphereComponent` scattering, color grading, vignette, bloom.

**The data model.** `WalkPropertyPath` now returns a `FPropertyTarget`
holding `(container_address, container_type, owning_object, leaf_property_name)`.
The walker maintains a void pointer + UStruct (UClass for UObject containers,
UScriptStruct for struct containers) and steps both kinds of hops:

```cpp
// "Settings.AutoExposureBias" on PostProcessVolume_0
target.ContainerAddress = &volume->Settings   // computed via ContainerPtrToValuePtr
target.ContainerType    = FPostProcessSettings::StaticStruct()
target.OwningObject     = volume              // stays at last UObject seen
target.LeafPropertyName = "AutoExposureBias"
```

**The dispatch.** Introduced internal `SetValueAtAddress(FProperty*, void*, ...)`
pure address-based helper. Existing `SetObjectProperty` and new
`SetPropertyAtTarget` both route through it. Added types: `FDoubleProperty`,
`FNameProperty` (alongside the existing Bool / Int / Float / Str / Byte /
Enum / Struct / Object handlers from v0.7.4).

**PostEditChangeProperty.** Fires on the owning UObject with the OUTERMOST
path segment as the changed property — so the editor knows the actor's
data was modified and refreshes Details panel / viewport, even when the
leaf change was deep inside a struct chain.

### v0.7.6 — viewport mode + editor introspection

Console `viewmode lit` failed silently through MCP because the routing
hops it took weren't viewport-scoped. Added direct API tools:

- **`get_viewport_mode`** → returns current mode (Lit / Unlit / Wireframe /
  DetailLighting / LightingOnly / etc.)
- **`set_viewport_mode(mode)`** → switches via `FEditorViewportClient::SetViewMode`
  with case-insensitive string → `EViewModeIndex` mapping. Includes the
  full menu: Lit, Unlit, Wireframe, BrushWireframe, DetailLighting,
  LightingOnly, LightComplexity, ShaderComplexity, LightmapDensity,
  ReflectionOverride, VisualizeBuffer, PathTracing.

Plus two introspection tools that close major Sprint 3 gaps:

- **`read_output_log(lines=50)`** → tail-reads `<Project>/Saved/Logs/<Project>.log`.
  Surfaces UE log lines the MCP client never sees. Critical for diagnosing
  the recurring "tool returned success but nothing happened" failure mode.
- **`get_async_compile_status`** → snapshot of `FShaderCompilingManager::GetNumRemainingJobs`
  + `FAssetCompilingManager::GetNumRemainingAssets`. Would have caught the
  finalize_migration hang (v0.7.3) in real time — the LLM can now poll
  this before invoking long batches and bail if the queue isn't draining.

### v0.7.7 — `take_screenshot` forces a fresh redraw

Phase 7.2 caught this the hard way: lighting changes applied successfully,
five screenshots in a row came back identical. UE's editor viewport is
event-driven — `ReadPixels` grabs whatever's in the GPU backbuffer, which
is the last-drawn frame. Without explicit invalidation, MCP state changes
don't appear until the user manually nudges the mouse.

The fix in `HandleTakeScreenshot`:

```cpp
EditorClient->Invalidate();           // mark the client dirty
Viewport->Invalidate();
Viewport->Draw();                     // force a frame
FlushRenderingCommands();             // wait for GPU
Viewport->ReadPixels(Bitmap, ...);    // now read a known-fresh frame
```

This single fix is what makes the entire AI-driven visual-iteration loop
actually work. Every other tool in the toolkit assumes screenshots
reflect current state.

## [0.7.4] — 2026-05-31 — set_actor_property dotted-paths + struct + asset values

### The pattern that kept biting us

`set_actor_property` historically only resolved names against the actor's own
UPROPERTY table. Every interesting tunable on every standard UE actor lives
on a *component* (or sub-object) underneath:

| Actor              | What you actually want    | Where it lives                              |
|--------------------|---------------------------|---------------------------------------------|
| `AStaticMeshActor` | `StaticMesh`              | `UStaticMeshComponent::StaticMesh`          |
| `APointLight`      | `Intensity`, `LightColor` | `UPointLightComponent::Intensity` etc.      |
| `APostProcessVolume` | exposure / color grading | `APostProcessVolume::Settings.<sub-struct>` |
| `ASkyAtmosphere`   | scattering distances      | `USkyAtmosphereComponent` properties        |

Every one of these failed with "Property not found" on the actor. We patched
the `StaticMesh` case with a targeted `spawn_static_mesh_actor` (v0.7.1).
The lights case (Phase 7.2 sanctuary tuning) made it clear the targeted
approach doesn't scale — there will be a third, fourth, fifth instance of
this same gap.

### The fix

Two layers, both in `UnrealMCPCommonUtils`:

1. **`WalkPropertyPath(Root, "A.B.C", &leafName, &error)`** — splits the
   dotted path, walks through each non-leaf segment as an `FObjectProperty`
   (or, for actor roots, falls back to `GetComponents()` name lookup so
   runtime-added components are reachable too), and returns the leaf-owning
   UObject + the leaf property name. Plain (un-dotted) names pass through
   unchanged so the call site stays uniform.

2. **`SetObjectProperty` extended** with handlers for the missing types:
   - **FStructProperty** — `FVector`, `FRotator`, `FLinearColor`, `FColor`.
     Accepts JSON arrays (`[r,g,b,a]`) or objects (`{r:1, g:0.5, b:0.2, a:1}`).
     Rotator accepts `{pitch, yaw, roll}` aliases.
   - **FObjectProperty** — accepts a `/Game/`-prefixed asset path string,
     does `LoadObject<UObject>`, type-checks against the slot's declared class,
     sets the reference. Empty string clears the slot. Generalizes the
     hard-coded mesh-load logic from `spawn_static_mesh_actor` so it works
     for any UObject reference (skeletal mesh, material, audio, etc.).

`HandleSetActorProperty` now resolves dotted paths via `WalkPropertyPath`
before dispatching to `SetObjectProperty`, and pushes a `PostEditChangeProperty`
so the editor's Details panel and viewport pick up the change.

### What this unlocks

Programmatic light tuning, post-process volume tuning, sky atmosphere
tuning, sub-object material slot setting, BlendableObject assignment —
basically every property the editor's Details panel exposes that lives
under a component. No more per-type patches; one tool handles it all:

```
set_actor_property("Altar_KeyLight", "PointLightComponent.Intensity", 50000)
set_actor_property("Altar_KeyLight", "PointLightComponent.LightColor", [255, 170, 80, 255])
set_actor_property("Altar_KeyLight", "PointLightComponent.AttenuationRadius", 2500.0)
set_actor_property("PostProcessVolume_0", "Settings.AutoExposureBias", 1.5)  # struct-of-struct works via repeated dotting
set_actor_property("Wall_BackL", "StaticMeshComponent.OverrideMaterials.0", "/Game/.../MI_Wet.MI_Wet")  # FUTURE: array indexing
```

(Array element setting like the last line isn't supported yet — it's a
follow-up for v0.7.5; everything else above works as of this release.)

## [0.7.3] — 2026-05-31 — finalize_migration: fix migrate_assets ref resolution

### Added — `finalize_migration` tool

Companion to `migrate_assets`, addressing a real-world breakage discovered
mid-Phase 7.2 build:

**The bug.** `migrate_assets` copies `.uasset` files byte-for-byte from
source to destination, preserving each asset's `/Game/`-relative path under
the caller-provided `destination_content_path`. But the copied files still
carry their *original* serialized hard references — e.g. an
`SM_ChapelStructure.uasset` migrated from Goddess Temple to `lauder3`
references `/Game/Masters/01_Masters/M_StandardMaster` (the path in GT).

If the caller passed `destination_content_path = ".../Content/Migrated"`
(a subfolder), the migrated material lands at
`/Game/Migrated/Masters/01_Masters/M_StandardMaster` — **not** the path
the serialized ref points at. Every reference breaks, materials default to
null, meshes render checkerboard.

**The fix.** `finalize_migration(migrated_root, target_root="/Game")` uses
`IAssetTools::RenameAssets` to batch-relocate every asset under
`migrated_root` to the equivalent path under `target_root`. UE's
`FAssetRenameManager` handles atomically:

- File moves on disk
- Internal hard + soft reference rewrites across the entire content tree
  (mesh → MI → master, plus everything else)
- Level actor reference updates (so spawned `AStaticMeshActor`s pointing at
  the migrated meshes get auto-fixed — no respawn needed)
- Redirector creation at the old `/Game/Migrated/...` paths for any
  external/lazy references that come along later

**Usage pattern.** The two-phase workflow is now: (1) in source editor,
`migrate_assets` — copies files; (2) in destination editor,
`finalize_migration("/Game/Migrated")` — fixes paths. Idempotent: a second
run after success is a no-op.

**Why not generic reference rewriting in `migrate_assets` itself?** Because
reference rewriting requires the destination editor to be running
(`FAssetRenameManager` operates on loaded `UObject` graphs, not raw
.uasset bytes). The source editor doing the migrate can't fix refs that
only exist after the destination loads the copied files. Two-phase is
honest about that constraint.

**Alternative for new migrations.** If the caller passes
`destination_content_path = "<Project>/Content"` (the Content root, no
subfolder), the migrated paths match the original `/Game/`-relative refs
and no finalize step is needed. `finalize_migration` is for the
already-broken case or when the caller deliberately wants segregation.

## [0.7.2] — 2026-05-31 — Inline-image screenshot return

### Fixed — `take_screenshot` end-to-end usability

Three independent problems made `take_screenshot` unusable from an
AI-driven workflow:

1. **Param name mismatch** — Python wrapper sent `"filename"`, C++ handler
   read `"filepath"`. Every call errored with `Missing 'filepath' parameter`.
2. **Unpredictable output location** — C++ called `SaveArrayToFile` with the
   raw passed-in path, so a bare `"screenshot.png"` landed at the engine's
   working directory (varies by launch context) rather than under the
   project's `Saved/` tree.
3. **No way for the LLM to actually see the result** — the response carried
   only `{"filepath": "..."}`, meaning the user had to manually open the file
   or drag it into chat. Fundamentally broken for an iterative
   build-then-eyeball-then-adjust loop.

### Changes

- **C++ (`HandleTakeScreenshot`)**:
  - Accept both `"filepath"` and `"filename"` (alias) — backward-compat plus
    natural-name convenience.
  - Anchor bare filenames under `FPaths::ProjectSavedDir() / Screenshots/`,
    matching UE's editor-screenshot convention.
  - `FPaths::ConvertRelativePathToFull` to absolutize before writing, so the
    response carries a path the Python side can immediately read.
  - `IFileManager::MakeDirectory(Tree=true)` so first-run subfolders are
    created instead of silently failing.
  - Return `{"filepath", "width", "height", "size_bytes"}` for richer client
    feedback. Improved error messages for the read/write failure modes.
- **Python (`server/tools/editor_tools.py::take_screenshot`)**:
  - Send `"filepath"` over the wire (matches C++ primary name).
  - After success, read the saved PNG via `pathlib.Path` and return
    `mcp.server.fastmcp.utilities.types.Image(path=...)` so the calling LLM
    sees the screenshot inline as image content rather than just a string.
  - Graceful fallback to `{"filepath", "size_bytes"}` dict when FastMCP is
    too old to expose `Image`.

### Why this matters

Visual iteration is the main loop for the lauder3 Phase 7 sandbox work
(cozy temple sanctuary, hostile expedition zone, portal transition). Without
inline screenshots, every layout adjustment burned a user round-trip just
to evaluate what changed. With this patch, Claude can take a screenshot
mid-build, see the result, decide the next nudge, and execute — all in
one tool-call cycle.

## [0.7.1] — 2026-05-31 — Direct mesh-asset placement (2 tools)

### Added — `spawn_static_mesh_actor` + `set_static_mesh_actor_mesh`

Discovered during Phase 7.2 of `lauder3` (cozy temple sanctuary build): a real
gap in the actor-placement workflow. `set_actor_property` resolves names against
the actor's UPROPERTY table only — it cannot reach properties owned by
components (e.g. `UStaticMeshComponent::StaticMesh`). Combined with
`spawn_actor` emitting an empty-mesh `AStaticMeshActor`, this made
single-call Megascans/Quixel placement impossible without dropping to
editor-side Python.

Two new handlers in `FUnrealMCPEditorCommands`:

- **`spawn_static_mesh_actor(name, mesh_path, location, rotation, scale, folder_path?)`** —
  combines `World->SpawnActor<AStaticMeshActor>` + `LoadObject<UStaticMesh>` +
  `UStaticMeshComponent::SetStaticMesh` into one round-trip. Defaults the
  component mobility to `Movable` so the LLM/user can continue to nudge the
  actor via `set_actor_transform`. Optional `folder_path` calls `SetFolderPath`
  inline so a batch of architecture pieces can land already-organized under
  e.g. `Sanctuary/Columns`.
- **`set_static_mesh_actor_mesh(name, mesh_path)`** — retroactive mesh swap on
  an existing `AStaticMeshActor`. Cast-checks the actor class up-front; returns
  a useful error if you point it at a non-StaticMeshActor.

Both handlers use `LoadObject<UStaticMesh>(nullptr, *MeshPath)` for asset
resolution, which accepts either `"/Game/.../SM_X.SM_X"` or `"/Game/.../SM_X"`.

### Why not a generic dotted-path traversal in `set_actor_property`?

That's the cleaner long-term design and lives in Sprint 3 backlog: walk
`Path.ParseIntoArray('.')`, traverse through `FObjectProperty` references, set
the leaf via reflection (handling `UObject*` properties via `LoadObject`).
Doing it right requires careful handling of `FArrayProperty`, `FStructProperty`,
and the asset-reference loading semantics — meaningful scope. The targeted
`spawn_static_mesh_actor` unblocks Phase 7.2 today with ~80 lines C++ and zero
risk to the existing dispatcher.

## [Unreleased] — Sprint 3 planning

Sprint 2 ✅ DONE. **11 tools shipped across 4 categories.** Original plan was
14 tools; we consolidated import_fbx/import_texture/import_audio into one
`import_asset` (UE's `UAssetImportTask` auto-detects type), and deferred
`cook_for_migration` (not needed for Phase 7.2 — `migrate_assets` covers it).

Sprint 3 surface (per project roadmap):

- **Niagara category** (~5 tools): spawn_niagara_actor, set_niagara_user_param,
  get_niagara_systems_in_level, activate_niagara, deactivate_niagara
- **Editor state remainder** (~3 tools): wire `take_screenshot` *return-path* to
  surface file paths properly, get_recent_log_lines, enable_realtime_viewport
- **Level management remainder** (~5 tools): create_level, list_levels_in_project,
  check_map_errors, build_lighting, add_streaming_sublevel

Deferred from Sprint 1 (still pending):

- **Proper screenshot path migration** to `FImageView` / `FImageBuilder`. UE 5.7
  deprecates `FImageUtils::CompressImageArray`; the replacement uses
  `TArrayView64<const FColor>` + `TArray64<uint8>` so we need to rewrite the
  surrounding `ReadPixels` path. Warning today, hard error next UE release.

## [0.7.0] — 2026-05-31 — Sprint 2 final: Outliner category (4 tools)

### Added — outliner organization handlers

New `FUnrealMCPOutlinerCommands` C++ class wired into `UUnrealMCPBridge` dispatch.
Four tools for managing actor folder organization in the World Outliner panel:

- **`get_outliner_folders()`** — every folder path visible in the current world's Outliner. Returns the union of every actor's folder label + any pending empty folders registered via `FActorFolders::ForEachFolder`.
- **`move_actor_to_folder(actor_name, folder_path)`** — set an actor's folder label via `AActor::SetFolderPath`. Empty path moves to root. Folder auto-created if it doesn't exist.
- **`create_outliner_folder(folder_path)`** — pre-create an empty folder via `FActorFolders::CreateFolder`. Useful when setting up organization before placing actors.
- **`get_actors_in_folder(folder_path)`** — list actors at exactly the given folder path. Returns display name, class name, and internal UObject name per actor.

**Use case for Lauder:** as Phase 7.2 migrates Megascans assets into L_Base v2,
the Outliner gets crowded fast. Folder organization (e.g.
`Migrated/Goddess_Temple/Candles`, `Lighting/CandlePoints`) keeps actors
discoverable and the scene maintainable.

### Known fix history (for future API archaeology)

- First include attempt: `#include "ActorFolders.h"` — header doesn't exist
  in UE 5.7. The actual path is `EditorActorFolders.h` (UnrealEd module's
  Public include dir).
- Anonymous-namespace `GetRegistry()` collision: when adaptive non-unity
  build allows MaterialCommands and AssetCommands to land in the same TU,
  both files' `(anonymous)::GetRegistry()` conflict. Renamed to
  `GetAssetRegistryForMaterials()` in MaterialCommands to disambiguate.
- C++ "most vexing parse" on the line
  `const FFolder Folder(FFolder::FRootObject(World), FName(*FolderPath));`
  Compiler reads it as a function declaration with two function-pointer
  parameters. Split into three named locals to force variable-decl parsing.

### Verified

Live Coding patch against UE 5.7 LauderEditor — 8.4s incremental, 4/4 actions
clean, plugin DLL patched in-place.

## [0.6.0] — 2026-05-31 — Materials category (5 tools)

## [0.6.0] — 2026-05-31 — Materials category (5 tools)

### Added — material category command handler

New `FUnrealMCPMaterialCommands` C++ class wired into `UUnrealMCPBridge` dispatch.
Five tools covering the common workflows for inspecting, creating, and tuning
material instances:

- **`get_material_parameters(material_path)`** — read scalar/vector/texture parameter names + values. Works on base `UMaterial` (returns defaults) or `UMaterialInstance` (returns current values which may override base).
- **`set_material_instance_param(material_instance_path, param_name, param_type, value)`** — override a parameter on a `UMaterialInstanceConstant`. `param_type` is `"scalar"` (number), `"vector"` (`{r,g,b,a}` object), or `"texture"` (asset path). Saves the instance on success.
- **`create_material_instance(parent_material_path, target_path)`** — create a new `UMaterialInstanceConstant` derived from a parent. Uses `UMaterialInstanceConstantFactoryNew` + `IAssetTools::CreateAsset` (the public `UMaterialEditingLibrary::CreateMaterialInstanceAsset` helper doesn't exist in UE 5.7's API surface — common gotcha).
- **`get_material_uses(material_path)`** — list assets that reference this material. Equivalent to UE's "Reference Viewer" Content Browser action.
- **`list_material_instances_of_parent(parent_material_path, search_path="/Game")`** — every `UMaterialInstanceConstant` whose parent is the given material. Loads each candidate to read its parent reference (necessary for accuracy across UE versions).

**Use case driving the work:** Lauder Phase 7.2 — once Goddess Temple master
materials (`M_BlendMaster`, `M_SSSMaster`, `M_StandardMaster`,
`M_FoliageCustomWind`) are migrated into Lauder3, we'll create instances of
them, tune scalar/vector/texture params to suit the cozy temple alcove mood,
and inspect which assets use which.

### Changed — Build.cs

Added `"MaterialEditor"` to `PrivateDependencyModuleNames`. Required to link
against `UMaterialEditingLibrary` (which lives in the MaterialEditor module,
not the runtime Engine module). UE 5.7 fact: `MaterialEditor` is editor-only,
which matches our plugin's `Type = "Editor"` declaration in `UnrealMCP.uplugin`.

### Verified

Full UBT rebuild against UE 5.7 LauderEditor — 13.3s, all 6 actions
(3 compile + 2 link + 1 metadata) succeeded, `UnrealEditor-UnrealMCP.dll`
relinked cleanly with the new MaterialEditor dep.

### Known fix history (worth keeping for future API archaeology)

- First attempt called `UMaterialEditingLibrary::CreateMaterialInstanceAsset(parent, name, path)` — function doesn't exist in UE 5.7 (likely never did publicly).
- Fixed by switching to the factory-based path: `UMaterialInstanceConstantFactoryNew` (UnrealEd module) + `IAssetTools::CreateAsset` (AssetTools module). The factory's `InitialParent` UPROPERTY carries the parent through `FactoryCreateNew`.
- First link attempt then failed because the rest of the Material API (`SetMaterialInstanceParent`, `SetMaterialInstance*ParameterValue`, `UpdateMaterialInstance`) lives in the `MaterialEditor` module, not `UnrealEd`. Build.cs change resolved it.

## [0.5.1] — 2026-05-31 — import_asset

### Added — `import_asset` (asset management category)

Generic source-file → UAsset import. UE's `UAssetImportTask` +
`IAssetTools::ImportAssetTasks` auto-detects the file type from extension and
selects the appropriate factory:

| Extension | Imported as |
|---|---|
| `.fbx`, `.obj`, `.gltf` | StaticMesh / SkeletalMesh / AnimSequence |
| `.png`, `.tga`, `.psd`, `.exr`, `.hdr`, `.jpg` | Texture2D |
| `.wav`, `.mp3`, `.ogg`, `.flac` | SoundWave |
| FBX with skeleton | Skeleton + PhysicsAsset + AnimSequence + SkeletalMesh |

This consolidates the originally-planned `import_fbx` / `import_texture` /
`import_audio` tools into one — they'd have been near-duplicate wrappers
since UE picks the factory automatically. If FBX-specific options become
needed later (LOD splitting, material import behavior), they'll be added
as an optional `import_options` JSON struct on this same tool rather than
splitting the API.

**Args:** `file_path`, `destination_path`, `replace_existing=True`, `save=True`.

**Returns:** `imported_object_paths[]`, `imported_count`, `success`,
plus a `note` field explaining typical failure causes when count is 0.

C++ side: `UnrealMCPAssetCommands::HandleImportAsset`. Python side:
`server/tools/asset_tools.py::import_asset`. No `Build.cs` change needed —
`AssetTools` module is pulled in transitively via `UnrealEd`.

### Verified

Live Coding patch 2 against UE 5.7 LauderEditor: 7.3s incremental,
clean link, plugin DLL patched in-place.

## [0.5.0] — 2026-05-31 — Sprint 2 kickoff: cross-project asset migration

### Added — `migrate_assets` (asset management category)

The headlining Sprint 2 tool. Copies a set of `/Game/`-prefixed assets — plus
their dependency closure — from the currently-loaded editor project to
another project's `Content/` directory.

**Implementation approach:** rather than calling `IAssetTools::MigratePackages`
(which drives a modal "select destination project" dialog and has non-UI
overloads that vary across UE 5.x point releases), we compute the dependency
closure via `IAssetRegistry::GetDependencies` and copy the underlying
`.uasset` / `.umap` files via `IFileManager::Copy`, preserving the
`/Game/`-relative directory layout at the destination. This is exactly what
UE's Migrate workflow does internally for the file-copy step — just headless
(no modal dialog), idempotent (skips existing files unless `force_overwrite=True`),
and stable across UE versions.

**Use case driving the work:** Lauder Phase 7.2 — migrating selected
Goddess Temple Megascans Sample assets (the ones the v0.2.0 asset tools
inventoried) from the RomanCave editor session into the Lauder3 project's
`Content/Migrated/` folder programmatically.

**Args:**
- `asset_paths: List[str]` — `/Game/`-prefixed object paths
- `destination_content_path: str` — absolute filesystem path to target project's `Content/`
- `include_dependencies: bool = True` — walk the transitive `/Game/` dependency graph
- `force_overwrite: bool = False` — idempotent by default

**Returns:**
```
{
  "success": bool,
  "initial_count": N,             # how many paths you asked for
  "total_with_dependencies": M,    # incl. transitive /Game/ deps
  "copied_count": K,              # files actually copied
  "skipped_count": S,             # existed at destination + no overwrite
  "destination_root": "...",
  "include_dependencies": bool,
  "errors": [...]
}
```

C++ side: `plugin/Source/UnrealMCP/Private/Commands/UnrealMCPAssetCommands.cpp`
+ header. Python wrapper: `server/tools/asset_tools.py`.

### Verified

Live Coding patch against UE 5.7 LauderEditor: 14s incremental, all 3
affected TUs (Bridge, AssetCommands, Module) compiled clean, patch linked
successfully, plugin DLL updated in-place in the running editor. The
pre-existing `CompressImageArray` deprecation warning (Sprint 1 deferred
item) is unchanged.

## [0.4.0] — 2026-05-30 — Repo restructure for clarity
- **Level management category (~9 tools)** — `get_current_level`, `open_level`, `save_level`, `save_all_dirty`, `create_level`, `list_levels_in_project`, `check_map_errors`, `build_lighting`, `add_streaming_sublevel`
- **Asset import / migrate (~5 tools)** — `import_fbx`, `import_texture`, `import_audio`, `migrate_assets_from_project`, `cook_for_migration`
- **Materials category (~5 tools)** — `get_material_parameters`, `set_material_instance_param`, `create_material_instance`, `get_material_uses`, `list_material_instances_of_parent`
- **Niagara category (~5 tools)** — `spawn_niagara_actor`, `set_niagara_user_param`, `get_niagara_systems_in_level`, `activate_niagara`, `deactivate_niagara`
- **Performance profiling (~5 tools)** — `get_frame_stats`, `get_gpu_stats`, `start_stat_capture`, `stop_stat_capture`, `dump_memory_usage`
- **Outliner / organization (~4 tools)** — `get_outliner_folders`, `move_actor_to_folder`, `create_outliner_folder`, `get_actors_in_folder`
- **Blueprint introspection extensions (~4 tools)** — `get_blueprint_graph_json`, `get_blueprint_variables`, `get_blueprint_functions`, `find_blueprint_compile_errors`

## [0.4.0] — 2026-05-30 — Repo restructure for clarity

Pre-Sprint-2 hygiene pass. Restructured the repo layout from the inherited
upstream "UE project containing the plugin at depth 4" pattern into a clean
three-product layout: `plugin/`, `server/`, `sample/`.

### Restructured — top-level layout

The plugin is now the **first thing a visitor sees**, not buried four levels
deep inside a sample UE project. The Python server got the matching prominent
sibling treatment.

**Before:**
```
MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/...  (the plugin, 4 levels deep)
Python/tools/                                           (the server, generic name)
Docs/Tools/                                             (stale upstream docs)
mcp.json                                                (example config, top-level)
```

**After:**
```
plugin/                  ★ THE UE PLUGIN
server/                  ★ THE PYTHON MCP SERVER
sample/                  ★ minimal dev/test UE project (plugin junctioned in)
docs/                    architecture, install, tool reference
tests/                   integration tests (stub at v0.4)
examples/                example MCP config + workflows (kit_inventory.md)
scripts/                 setup-dev-junction.ps1
README.md, CHANGELOG.md, CONTRIBUTING.md, LICENSE
```

`git mv` preserved history across all file moves — `git log --follow plugin/Source/UnrealMCP/Private/UnrealMCPBridge.cpp` traces all the way back through `MCPGameProject/Plugins/UnrealMCP/...` and into upstream chongdashu commits.

### Renamed

- `MCPGameProject` → `sample` (clearer naming — it's a dev/test bed, not "the project")
- `MCPGameProject.uproject` → `UnrealMCPSample.uproject`
- `Source/MCPGameProject/` module → `Source/UnrealMCPSample/`
- All inner module file/class names: `MCPGameProject` → `UnrealMCPSample`
- Sample `.uproject` bumped: `EngineAssociation: "5.5"` → `"5.7"`, `BuildSettingsVersion.V5` → `V6`, `IncludeOrderVersion.Unreal5_5` → `Latest`
- `Python/` → `server/`
- `Docs/CONTRIBUTING.md` → `CONTRIBUTING.md` (top-level convention)
- `mcp.json` → `examples/mcp-client-config.json`

### Removed (stale upstream artifacts)

- `Docs/Tools/*.md` (5 files) — written against upstream v0.1 surface, predate our v0.2/v0.3 additions. Per-category docs will regenerate as Sprint 2 lands.
- `Docs/README.md` — upstream landing page; top-level README.md covers it.
- `Python/scripts/*.py` (7 files) — upstream integration test stubs, never validated against our state.
- `MCPGameProject.sln` — UE regenerates per-machine; never useful in git.

### Added

- **`LICENSE`** — MIT, with attribution to both upstream chongdashu and this fork. Previously the repo had no `LICENSE` file despite the MIT badge in README.
- **`docs/architecture.md`** — process diagram + protocol explanation.
- **`docs/installing.md`** — separate user-vs-contributor install paths.
- **`docs/tools/README.md`** — explains the v0.3 tool catalog; per-category docs to follow in Sprint 2.
- **`tests/README.md`** — testing strategy + rationale for why there aren't tests yet.
- **`examples/kit_inventory.md`** — the actual Lauder Phase 7.1 workflow as a worked example of the asset tools.
- **`scripts/setup-dev-junction.ps1`** — bootstraps the `sample/Plugins/UnrealMCP` junction for contributors. Idempotent.
- **`sample/Plugins/.gitkeep`** — keeps the directory in git so the junction script has a known place to land.

### Fixed — pre-existing UMG bug

The Phase 5.3 UMG extensions (v0.1.0, 8 new handler methods) were defined in
`UnrealMCPUMGCommands.cpp` but never declared in the header. This snuck past
Lauder3's incremental builds because UE's adaptive non-unity build skipped
recompiling unchanged files. The sample project's clean-from-scratch build
caught it. Added 8 missing declarations to
`plugin/Source/UnrealMCP/Public/Commands/UnrealMCPUMGCommands.h`.

### Verified

- `mklink /J sample\Plugins\UnrealMCP plugin\` creates the junction; UE follows it transparently.
- `Build.bat UnrealMCPSampleEditor Win64 Development -Project="...UnrealMCPSample.uproject"` builds clean from scratch (5.7s incremental after first-time setup, 56s first-time including UHT manifest generation).
- All Lauder3-side incremental builds continue working unchanged (plugin snapshot at `lauder3/Lauder/Plugins/UnrealMCP/` is the consumer side, untouched by this restructure beyond the UMG header sync).

## [0.3.0] — 2026-05-30 — Sprint 1 completion: editor state + level management + cleanup

### Added — Editor State extensions (5 tools)

Extended `FUnrealMCPEditorCommands` with viewport + console-variable handlers:

- **`take_screenshot(filename="screenshot.png", show_ui=False)`** — Python wrapper for the existing C++ `HandleTakeScreenshot`. Previously only callable via raw `execute_command`; now first-class.
- **`get_viewport_camera()`** — returns the editor viewport camera's location (`{x,y,z}`) and rotation (`{pitch,yaw,roll}`). Reads `UUnrealEditorSubsystem::GetLevelViewportCameraInfo`.
- **`set_viewport_camera(location, rotation)`** — moves the viewport camera. Accepts either object form (`{x,y,z}` / `{pitch,yaw,roll}`) or flat array form (`[x,y,z]` / `[p,y,r]`).
- **`execute_console_command(command)`** — runs an arbitrary UE console command (e.g. `"stat fps"`, `"HighResShot 1920x1080"`).
- **`set_cvar(name, value)`** and **`get_cvar(name)`** — typed CVar access via `IConsoleManager`. The getter returns string + float + int + bool variants so callers don't parse.

### Added — Level Management partial (4 tools)

New command handler class `FUnrealMCPLevelCommands` at
`MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPLevelCommands.cpp`
(+ header in `Public/Commands/`). Wired into `UUnrealMCPBridge` dispatch.

- **`get_current_level()`** — name + package_name + object_path + map_name of the loaded editor world.
- **`open_level(level_path)`** — load a level by `/Game/`-prefixed package path. Accepts either package-path or object-path form (auto-strips trailing object suffix).
- **`save_current_level()`** — save the currently-loaded level.
- **`save_all_dirty()`** — batch-save every dirty level + content package.

Uses `ULevelEditorSubsystem` from the `LevelEditor` module.

### Changed — Build system

Added `"LevelEditor"` to `PrivateDependencyModuleNames` in `UnrealMCP.Build.cs` so the
plugin can link against `ULevelEditorSubsystem`. Required by the level management tools.

### Removed — stale upstream cruft (separate commit 1f574d6)

- `.cursor/rules/`, `.windsurfrules`, `.clinerules`, `.github/copilot-instructions.md` — IDE-specific rule files carrying duplicated content for Cursor/Windsurf/Cline/Copilot users. Consolidated into a single `Docs/CONTRIBUTING.md` with our style guide additions.
- `MCPGameProject/Docs/REFACTOR_COMMANDS.md` — historical refactoring plan; the refactor it described has been done since before our fork.
- Net: 411 lines removed, 138 added (CONTRIBUTING.md), -270 net.

### Verified

Builds clean against UE 5.7 in LauderEditor target. One MSVC warning silenced
(false-positive uninit on `FRotator` in `HandleSetViewportCamera`; locals now
explicitly initialized to `ZeroRotator`/`ZeroVector`).

## [0.2.0] — 2026-05-29 — Sprint 1: Asset Management

### Added — Asset Management category (9 tools)

New command handler class `FUnrealMCPAssetCommands` at
`MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPAssetCommands.cpp`
(+ header in `Public/Commands/`). Wired into `UUnrealMCPBridge` dispatch.

Tools:

- **`list_assets(path, recursive=True, class_filter=None)`** — enumerate assets under a `/Game/` path. Optional client-side class filter.
- **`get_asset_info(asset_path)`** — full `FAssetData` metadata plus the asset's dependency and referencer lists. Single-call answer to "what is this and what does it touch?".
- **`find_assets_by_class(class_name, search_path="/Game", recursive=True)`** — every asset of a given UClass under a path. Uses `FARFilter` with `bRecursiveClasses=true` so a query for `Material` also returns `MaterialInstance` etc.
- **`get_asset_dependencies(asset_path, recursive=False)`** — packages this asset depends on. Non-recursive by default (one hop); recursive walks the full transitive graph.
- **`get_asset_references(asset_path)`** — packages that reference this asset. Useful pre-flight for delete/move operations.
- **`move_asset(from_path, to_path)`** — rename to a new path; creates a redirector at the old location.
- **`delete_asset(asset_path)`** — delete via `UEditorAssetLibrary::DeleteAsset`.
- **`rename_asset(asset_path, new_name)`** — change the leaf name in place. Convenience wrapper over `move_asset` that computes the new path automatically.
- **`duplicate_asset(asset_path, target_path)`** — copy to a new path via `UEditorAssetLibrary::DuplicateAsset`.

All tools tolerate the CDO-pin failure mode documented in
`feedback_unreal_rename_asset_cdo_pin` (Lauder Phase 6.5 memory) by returning
`{success: False, note: "..."}` rather than throwing.

### Added — Python wrapper module

`Python/tools/asset_tools.py` with `register_asset_tools(mcp)` registered in
`unreal_mcp_server.py`. Each Python tool documents the response shape, the
underlying C++ APIs it calls, and the common failure modes.

### Verified

Builds clean against UE 5.7 — 9 actions, 38.5s incremental, no warnings or
errors from the new code. Pre-existing `FImageUtils::CompressImageArray`
deprecation warning in upstream `UnrealMCPEditorCommands.cpp:588` flagged for
Sprint 1 editor-state work.

## [0.1.0] — 2026-05-29

Initial fork from `chongdashu/unreal-mcp` @ `4e5f00d` ("Revert to state of commit fa7a84a").

### Added — Phase 5.3 UMG endpoint extensions

`MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPUMGCommands.cpp` (+435 lines):

- Extended widget creation endpoints to support HUD-style placements
- Text block binding to Blueprint variables
- Button event wiring helpers
- Viewport add/remove helpers
- Used in active development for the Lauder project's run-state HUD (HP bar, stamina bar, resource counters, weapon-tier indicator)

### Fixed — UE 5.7 compatibility

`ANY_PACKAGE` was removed in UE 5.5. The upstream uses it in several places, which causes hard compile errors on UE 5.7. Replaced with explicit package targeting using `FindObject<...>(nullptr, *AssetPath)` or similar.

- `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintCommands.cpp` (8 line delta)
- `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintNodeCommands.cpp` (10 line delta)

`BufferSize` is a private name inside UE's `TCHAR_TO_UTF8` macro expansion. Local variables named `BufferSize` in the socket read path collided with the macro and produced obscure errors. Renamed local variables to disambiguate.

- `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/MCPServerRunnable.cpp` (substantial trim — 205 line delta, mostly simplification of the read loop while fixing the name collision)
- `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/UnrealMCPBridge.cpp` (11 line delta — wiring adjustments)

These fixes are necessary to load the plugin in UE 5.7. Tested on Lauder project (UE 5.7 + VS 2022 BuildTools + MSVC 14.44 + Windows 10/11 SDK + .NET Framework 4.8 SDK).
