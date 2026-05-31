<div align="center">

# unreal-mcp

**Model Context Protocol server for Unreal Engine 5.7**
101 editor tools, driven from any MCP client via natural language

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)
[![Tools](https://img.shields.io/badge/tools-101-green)](server/smoke_dispatch.py)

</div>

Drive Unreal Engine 5.7 from any Model Context Protocol (MCP) client — Claude Desktop, Claude Code, Cursor, Codex CLI, Windsurf, and others — via natural language. The system bridges your MCP client to a C++ plugin running inside the UE editor via a small Python server. Everything you can do in the editor UI — spawning actors, tweaking properties, navigating the Content Browser, taking screenshots, running PIE — is now a tool the LLM can call.

## What it can do

101 tools across 9 modules. Each Python wrapper docstring is the source of truth (your MCP client sees them via `tools/list`); the breakdown below is the architectural map.

| Module | Count | What's in here |
|---|---|---|
| **Editor** (`editor_tools.py`) | 48 | Actors, selection, viewport, screenshots, console, PIE, generalized object reflection |
| **Assets** (`asset_tools.py`) | 16 | Registry queries, mutations, import, cross-project migration, Content Browser navigation, static mesh inspection |
| **Blueprint nodes** (`node_tools.py`) | 8 | Event nodes, function calls, pin connections, variable get/set, input action nodes |
| **Blueprints** (`blueprint_tools.py`) | 7 | Class creation, components, property writes, compile |
| **UMG widgets** (`umg_tools.py`) | 6 | Widget BPs, text + buttons, event binding, viewport mount |
| **Materials** (`material_tools.py`) | 5 | Create instances, tune parameters, query parent + uses |
| **Outliner** (`outliner_tools.py`) | 5 | Folders + actor placement + batch organize |
| **Levels** (`level_tools.py`) | 3 | Get / open / save |
| **Project** (`project_tools.py`) | 3 | Legacy input mappings + DefaultEngine.ini get/set |

### Actors + selection + reflection (the big surface — 28 tools)

Spawn any `AActor` subclass by name (`StaticMeshActor`, `PointLight`, `ExponentialHeightFog`, `SkyAtmosphere`, or any custom Blueprint class — the lookup handles the `A` prefix and engine module paths). Single-shot or batched. Paged enumeration with class + name + folder filters. Selection state read + write, framing in the viewport.

- **Spawn / delete:** `spawn_actor`, `spawn_actor_batch`, `spawn_static_mesh_actor`, `spawn_blueprint_actor`, `delete_actor`, `delete_actor_batch`
- **Transform:** `set_actor_transform` (selective L/R/S), `get_actor_transform`
- **Enumeration:** `find_actors` (paged, class + pattern + folder filters), `find_actors_by_name`, `get_actors_in_level` (deprecated)
- **Selection:** `get_selected_actors`, `set_selected_actors`, `clear_selection`, `focus_selected_actors`
- **Mesh + materials:** `set_static_mesh_actor_mesh`, `set_static_mesh_material`, `get_static_mesh_material`
- **Per-actor properties:** `get_actor_properties` (dump), `get_actor_property` / `set_actor_property` (dotted path: `PointLightComponent.Intensity`, `Settings.AutoExposureBias`, `StaticMeshComponent.OverrideMaterials.0`), `get_component_property`
- **Generalized reflection — the Details-panel surface for any UObject:**
  - `get_object_property(target, path)` and `set_object_property(target, path, value)` — `target` resolves to an actor display label / internal name, a `/Game/...` asset path, a `/Script/...` engine class path, or a class default object. Same dotted-path syntax as the actor variants.
  - `get_world_settings(path?)` / `set_world_settings(path, value)` — convenience pinning target to the level's `AWorldSettings`.

### Viewport + screenshots (10 tools)

Camera, render mode, show-flag toggles, framing, and inline-image screenshot capture (PNG bytes returned to the LLM, not a path).

- `get_viewport_camera`, `set_viewport_camera`, `frame_actor`
- `get_viewport_mode` / `set_viewport_mode` (`Lit` / `Unlit` / `Wireframe` / `DetailLighting` / `LightComplexity` / `ShaderComplexity` / `PathTracing` / etc.)
- `set_show_flag` (Lighting / BillboardSprites / Bounds / Grid / PostProcessing / Game / Atmosphere / Fog / Particles / …)
- `take_screenshot` — editor viewport, returned as an MCP `Image` content block

### PIE control — the autonomous walkability oracle (7 tools)

Programmatic Play-in-Editor: start / stop, query the player pawn, teleport, drive WASD-equivalent input for N seconds, capture the in-game viewport. With these the agent can verify scene changes end-to-end (e.g. spawn a mesh, drop the player on it, drive forward 2 s, check `is_falling`) without you pressing Play.

- `start_pie`, `stop_pie`, `is_pie_active`
- `pie_get_player` — pawn location / rotation / velocity / `movement_mode` / `is_falling`
- `pie_set_player` — teleport
- `pie_apply_movement(direction, seconds)` — fire-and-forget input
- `pie_screenshot` — in-game viewport capture (no editor gizmos)

### Editor introspection + console (5 tools)

- `execute_console_command`, `get_cvar`, `set_cvar`
- `read_output_log` — tail the editor's log
- `get_async_compile_status`, `wait_for_async_compile` — block until shader + asset compile queues drain (essential before `finalize_migration`)
- `dismiss_modal_dialog` — close transient editor popups (best-effort: menus + modal windows)
- `recompile_live` — trigger Live Coding rebuild of the plugin DLL

### Assets (16 tools)

- **Registry:** `list_assets`, `get_asset_info`, `find_assets_by_class`, `get_asset_dependencies`, `get_asset_references`
- **Mutations:** `move_asset`, `delete_asset`, `rename_asset`, `duplicate_asset`
- **Import + migration:** `import_asset` (generic FBX / PNG / WAV / etc.), `migrate_assets` (cross-project), `finalize_migration` (post-migrate reference fix-up)
- **Content Browser navigation:** `focus_in_browser`, `navigate_to_folder`, `open_in_editor` (opens Material Editor / BP Editor / etc. for the asset's class)
- **Inspection:** `static_mesh_get_info` — bounds (center / extent / size / min / max), material slot list with current default assignments, LOD count

### Outliner (5 tools)

`get_outliner_folders`, `move_actor_to_folder`, `move_actor_to_folder_batch`, `create_outliner_folder`, `get_actors_in_folder`.

### Materials (5 tools)

`create_material_instance`, `set_material_instance_param`, `get_material_parameters`, `get_material_uses`, `list_material_instances_of_parent`.

### Levels (3 tools)

`get_current_level`, `open_level`, `save_current_level`.

### Blueprints + nodes (15 tools combined)

Build a class, add components + variables, wire event nodes + function calls + variable get/set, compile, spawn instances. Useful for end-to-end scaffolding of new gameplay classes from a prompt.

- **Classes + components:** `create_blueprint`, `add_component_to_blueprint`, `set_component_property`, `set_blueprint_property`, `set_pawn_properties`, `set_physics_properties`, `set_static_mesh_properties`, `add_blueprint_variable`, `compile_blueprint`
- **Graph nodes:** `add_blueprint_event_node`, `add_blueprint_function_node`, `add_blueprint_input_action_node`, `add_blueprint_get_self_component_reference`, `add_blueprint_self_reference`, `connect_blueprint_nodes`, `find_blueprint_nodes`
- **Spawning BP instances:** `spawn_blueprint_actor`

### UMG widgets (6 tools)

`create_umg_widget_blueprint`, `add_text_block_to_widget`, `add_button_to_widget`, `bind_widget_event`, `set_text_block_binding`, `add_widget_to_viewport`.

### Project settings (3 tools)

`create_input_mapping` (legacy DefaultInput.ini action / axis), `get_ini` (read a DefaultEngine.ini / DefaultGame.ini key or full section), `set_ini` (write + live + persist via GConfig).

### Coverage canary

`server/smoke_dispatch.py` pings every wire command with empty params and asserts no response comes back `"Unknown command: …"`. Run it from `server/` after the editor is up:

```bash
./.venv/Scripts/python smoke_dispatch.py   # 94/94 dispatched, 0 unknown
```

The smoke runs through the raw socket (port 55557), not through the MCP transport, so it isolates "did I forget to wire X" from MCP-client-side issues. It's the canonical regression test going forward.

## Layout

```
unreal-mcp/
├── plugin/           ★ THE UE PLUGIN — drop into any UE 5.7+ project's Plugins/
├── server/           ★ THE PYTHON MCP SERVER — launched by your MCP client
├── sample/           minimal UE 5.7 dev/test project (plugin junctioned in)
├── docs/             architecture, install, per-tool reference
├── tests/            integration tests
├── examples/         MCP client configs + sample workflows
└── scripts/          setup-dev-junction.ps1
```

Full layout in `CONTRIBUTING.md`. Architecture diagram in `docs/architecture.md`.

## Quick start

### Prerequisites

- Unreal Engine 5.7+
- Python 3.12+
- [`uv`](https://docs.astral.sh/uv/) (`pip install uv` or via your package manager)
- An MCP client (see configuration section below)

### 1. Install the plugin into your UE project

Copy the `plugin/` directory of this repo (or extract a release zip) into your project's `Plugins/` folder so you end up with:

```
YourProject/Plugins/UnrealMCP/
├── UnrealMCP.uplugin
└── Source/UnrealMCP/
```

On next editor open, the plugin's TCP server starts automatically on port 55557.

### 2. Install the Python server

```bash
cd server
uv venv
uv pip install -e .
```

### 3. Configure your MCP client

Pick your client below and add the entry to its config file. The schema is the same across clients — only the file location and format differ.

#### Claude Desktop

Config file: `%APPDATA%\Claude\claude_desktop_config.json` (Windows) or `~/Library/Application Support/Claude/claude_desktop_config.json` (macOS).

```json
{
  "mcpServers": {
    "unreal-mcp": {
      "command": "uv",
      "args": [
        "--directory", "/absolute/path/to/unreal-mcp/server",
        "run", "unreal_mcp_server.py"
      ]
    }
  }
}
```

#### Claude Code (CLI)

Config file: `~/.claude.json` (top-level, contains an `mcpServers` object).

```json
{
  "mcpServers": {
    "unreal-mcp": {
      "type": "stdio",
      "command": "uv",
      "args": [
        "--directory", "/absolute/path/to/unreal-mcp/server",
        "run", "unreal_mcp_server.py"
      ]
    }
  }
}
```

#### Codex CLI (OpenAI)

Config file: `~/.codex/config.toml`.

```toml
[mcp_servers.unreal-mcp]
command = "uv"
args = ["--directory", "/absolute/path/to/unreal-mcp/server", "run", "unreal_mcp_server.py"]
```

#### Cursor

Config file: `.cursor/mcp.json` (project root) or `~/.cursor/mcp.json` (user-level).

Same JSON shape as Claude Desktop.

#### Windsurf

Config file: `~/.codeium/windsurf/mcp_config.json`.

Same JSON shape as Claude Desktop.

### 4. Open your UE project

The plugin's TCP server starts when the editor loads. **Restart your MCP client** so it picks up the newly-configured server and discovers the available tools.

A working example config is at `examples/mcp-client-config.json`.

## Working on the plugin source (contributors)

If you're modifying the plugin itself:

```bash
git clone https://github.com/RonildoBraga/unreal-mcp.git
cd unreal-mcp
.\scripts\setup-dev-junction.ps1     # creates sample/Plugins/UnrealMCP junction
```

Open `sample/UnrealMCPSample.uproject` in UE 5.7. Edits to `plugin/Source/` are immediately visible to the sample's build via the Windows junction.

For style conventions when adding new tools, see `CONTRIBUTING.md`. Architecture explained in `docs/architecture.md`.

## License

MIT. See `LICENSE`.
