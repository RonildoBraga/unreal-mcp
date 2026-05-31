<div align="center">

# Model Context Protocol for Unreal Engine
<span style="color: #555555">unreal-mcp — extended fork</span>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)

</div>

> **Fork notice.** This is a fork of [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp) maintained by [RonildoBraga](https://github.com/RonildoBraga) for the Lauder project. Adds:
>
> - **UE 5.7 compatibility patches** (`ANY_PACKAGE` removal in UE 5.5+, `BufferSize` name collision with `TCHAR_TO_UTF8` internals — see `CHANGELOG.md`)
> - **UMG endpoint extensions** (Phase 5.3) for HUD-style widget work
> - **Sprint 1 + Sprint 2** (v0.2.0 – v0.7.0): asset management, editor state, level management, asset migration, materials, outliner — ~32 new tools
> - **v0.7.4 – v0.7.7:** dotted-path property traversal (component + struct hops + Vector4), viewport mode control, screenshot redraw fix, async-compile introspection
> - **v0.7.9 – v0.7.11:** `PostEditChangeProperty` broadcast (writes actually affect the renderer), generic `spawn_actor` UClass lookup, `FArrayProperty` walker, `set_static_mesh_material`, `get_actor_property` read counterpart, **PIE control** (`start_pie` / `stop_pie` / `pie_get_player` / `pie_set_player` / `pie_apply_movement` / `pie_screenshot`) for autonomous walkability verification
>
> Original upstream remains MIT-licensed; this fork preserves the license and attribution.

Drive Unreal Engine 5.7 from any Model Context Protocol (MCP) client — Claude Desktop, Claude Code, Cursor, Codex CLI, Windsurf, and others — via natural language. The system bridges your MCP client to a C++ plugin running inside the UE editor via a small Python server.

## What it can do

| Category | Capabilities |
|----------|-------------|
| **Actors** | Spawn (any `AActor` subclass by name — `SkyAtmosphere`, `ExponentialHeightFog`, custom Blueprint actors), `spawn_static_mesh_actor` with mesh assignment + Outliner folder, delete, transform, get/set arbitrary properties via dotted paths (`PointLightComponent.Intensity`, `Settings.AutoExposureBias`, `StaticMeshComponent.OverrideMaterials.0`), `set_static_mesh_material` convenience |
| **Blueprints** | Create classes with custom components; add nodes; compile; spawn instances |
| **UMG widgets** | Create widget BPs; add text/buttons; bind events; viewport |
| **Editor state** | Viewport camera + mode (Lit/Unlit/Wireframe/…); inline-image screenshots; console commands; CVars; output log tail; async compile-queue status |
| **PIE control** | `start_pie` / `stop_pie` / `is_pie_active`; read player pawn location + rotation + velocity + `movement_mode` + `is_falling`; teleport for spot-testing; fire-and-forget movement input for N seconds; in-game viewport screenshot — the autonomous walkability oracle |
| **Levels** | Get/open/save current level; save all dirty |
| **Assets** | List, search by class, dependency + referencer graph; move, delete, rename, duplicate; **migrate cross-project + finalize_migration ref-fixup, generic import** |
| **Materials** | Create material instances; tune parameters; query parent + uses; list instances of a parent |
| **Outliner** | Get/create folders; move actors to folders; list actors by folder |
| **Input** | Input action mapping creation |

See `docs/tools/README.md` for the full v0.7.11 tool catalog (78 tools).

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

MIT. See `LICENSE`. Attribution to upstream `chongdashu/unreal-mcp` preserved.
