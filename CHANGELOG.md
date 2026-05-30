# Changelog

All notable changes to this fork of `chongdashu/unreal-mcp` are tracked here.

The format is loosely based on [Keep a Changelog](https://keepachangelog.com/), and the project follows informal semantic versioning until it stabilizes out of experimental status.

## [Unreleased]

Planned (see Lauder project roadmap, MCP sprints):

- **Asset management category (~9 tools)** — `list_assets`, `get_asset_info`, `find_assets_by_class`, `get_asset_dependencies`, `get_asset_references`, `move_asset`, `delete_asset`, `rename_asset`, `duplicate_asset`
- **Editor state extensions (~5 tools)** — wire `take_screenshot` to Python side (already in C++), `get_viewport_camera`, `set_viewport_camera`, `execute_console_command`, `set_cvar`
- **Level management category (~9 tools)** — `get_current_level`, `open_level`, `save_level`, `save_all_dirty`, `create_level`, `list_levels_in_project`, `check_map_errors`, `build_lighting`, `add_streaming_sublevel`
- **Asset import / migrate (~5 tools)** — `import_fbx`, `import_texture`, `import_audio`, `migrate_assets_from_project`, `cook_for_migration`
- **Materials category (~5 tools)** — `get_material_parameters`, `set_material_instance_param`, `create_material_instance`, `get_material_uses`, `list_material_instances_of_parent`
- **Niagara category (~5 tools)** — `spawn_niagara_actor`, `set_niagara_user_param`, `get_niagara_systems_in_level`, `activate_niagara`, `deactivate_niagara`
- **Performance profiling (~5 tools)** — `get_frame_stats`, `get_gpu_stats`, `start_stat_capture`, `stop_stat_capture`, `dump_memory_usage`
- **Outliner / organization (~4 tools)** — `get_outliner_folders`, `move_actor_to_folder`, `create_outliner_folder`, `get_actors_in_folder`
- **Blueprint introspection extensions (~4 tools)** — `get_blueprint_graph_json`, `get_blueprint_variables`, `get_blueprint_functions`, `find_blueprint_compile_errors`

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
