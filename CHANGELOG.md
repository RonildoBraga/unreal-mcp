# Changelog

All notable changes to this fork of `chongdashu/unreal-mcp` are tracked here.

The format is loosely based on [Keep a Changelog](https://keepachangelog.com/), and the project follows informal semantic versioning until it stabilizes out of experimental status.

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
