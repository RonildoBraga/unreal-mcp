<div align="center">

# Model Context Protocol for Unreal Engine
<span style="color: #555555">unreal-mcp — extended fork</span>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)
[![Status](https://img.shields.io/badge/Status-Experimental-red)](https://github.com/RonildoBraga/unreal-mcp)

</div>

> **Fork notice.** This is a fork of [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp) maintained by [RonildoBraga](https://github.com/RonildoBraga) for the Lauder project. It adds:
>
> - **UE 5.7 compatibility patches** (`ANY_PACKAGE` removal in UE 5.5+, `BufferSize` name collision with `TCHAR_TO_UTF8` internals — see `CHANGELOG.md`)
> - **Phase 5.3 UMG endpoint extensions** (~435 lines) for HUD-style widget work
> - **Planned 50+ additional tools** across asset management, materials, Niagara, level management, performance profiling — see project roadmap
>
> Original upstream remains MIT-licensed; this fork preserves the license and attribution. For the unmodified original, go to [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp).

This project enables AI assistant clients like Cursor, Windsurf and Claude Desktop to control Unreal Engine through natural language using the Model Context Protocol (MCP).

## ⚠️ Experimental Status

This project is currently in an **EXPERIMENTAL** state. The API, functionality, and implementation details are subject to significant changes. While we encourage testing and feedback, please be aware that:

- Breaking changes may occur without notice
- Features may be incomplete or unstable
- Documentation may be outdated or missing
- Production use is not recommended at this time

## 🌟 Overview

The Unreal MCP integration provides comprehensive tools for controlling Unreal Engine through natural language:

| Category | Capabilities |
|----------|-------------|
| **Actor Management** | • Create and delete actors (cubes, spheres, lights, cameras, etc.)<br>• Set actor transforms (position, rotation, scale)<br>• Query actor properties and find actors by name<br>• List all actors in the current level |
| **Blueprint Development** | • Create new Blueprint classes with custom components<br>• Add and configure components (mesh, camera, light, etc.)<br>• Set component properties and physics settings<br>• Compile Blueprints and spawn Blueprint actors<br>• Create input mappings for player controls |
| **Blueprint Node Graph** | • Add event nodes (BeginPlay, Tick, etc.)<br>• Create function call nodes and connect them<br>• Add variables with custom types and default values<br>• Create component and self references<br>• Find and manage nodes in the graph |
| **Editor Control** | • Focus viewport on specific actors or locations<br>• Control viewport camera orientation and distance |

All these capabilities are accessible through natural language commands via AI assistants, making it easy to automate and control Unreal Engine workflows.

## 🧩 Components

### Sample Project (MCPGameProject) `MCPGameProject`
- Based off the Blank Project, but with the UnrealMCP plugin added.

### Plugin (UnrealMCP) `MCPGameProject/Plugins/UnrealMCP`
- Native TCP server for MCP communication
- Integrates with Unreal Editor subsystems
- Implements actor manipulation tools
- Handles command execution and response handling

### Python MCP Server `Python/unreal_mcp_server.py`
- Implemented in `unreal_mcp_server.py`
- Manages TCP socket connections to the C++ plugin (port 55557)
- Handles command serialization and response parsing
- Provides error handling and connection management
- Loads and registers tool modules from the `tools` directory
- Uses the FastMCP library to implement the Model Context Protocol

## 📂 Directory Structure

- **`plugin/`** — ★ THE UE PLUGIN. Drops into any UE 5.7+ project's `Plugins/`.
  - `UnrealMCP.uplugin` — plugin descriptor
  - `Source/UnrealMCP/` — C++ source (handlers in `Private/Commands/`)
- **`server/`** — ★ THE PYTHON MCP SERVER. Launched by your MCP client.
  - `unreal_mcp_server.py` — FastMCP server entry point
  - `tools/` — one Python module per tool category (asset, editor, level, blueprint, …)
- **`sample/`** — minimal UE 5.7 project for plugin dev/test (see `docs/installing.md`)
- **`docs/`** — architecture, installation, per-tool reference
- **`tests/`** — integration tests (populated in Sprint 2+)
- **`examples/`** — example MCP client config + sample workflows
- **`scripts/`** — developer convenience (junction setup)

Full layout in `CONTRIBUTING.md`. Architecture explained in `docs/architecture.md`.

## 🚀 Quick Start Guide

### Prerequisites
- Unreal Engine 5.7+
- Python 3.12+
- MCP Client (e.g., Claude Desktop, Claude Code, Cursor, Windsurf)

### Installing into your existing project

1. **Drop the plugin into your project's `Plugins/`** — copy the `plugin/`
   directory of this repo (or extract a release zip) so you end up with:
   ```
   YourProject/Plugins/UnrealMCP/
   ├── UnrealMCP.uplugin
   └── Source/UnrealMCP/
   ```
2. **Set up the Python server.** Clone or download the `server/` directory
   to a known location, install its deps:
   ```
   cd server
   uv venv
   uv pip install -e .
   ```
3. **Register the server with your MCP client.** See
   `examples/mcp-client-config.json` for the exact JSON. Place it at your
   client's MCP-config location (table further down).
4. **Open your UE project.** The plugin's TCP server starts automatically
   on port 55557 once the editor is loaded. Restart your MCP client to pick
   up the new tools.

Full walk-through (including the contributor dev-junction setup) in
`docs/installing.md`.

### Hacking on the plugin itself

If you're modifying the plugin source, work in this repo:
```
git clone https://github.com/RonildoBraga/unreal-mcp.git
cd unreal-mcp
.\scripts\setup-dev-junction.ps1     # creates sample/Plugins/UnrealMCP junction
```
Then open `sample/UnrealMCPSample.uproject` in UE 5.7. Edits to `plugin/Source/`
are immediately visible to the sample's build. See `docs/installing.md` for
why this works.

### Python Server Setup

See `server/README.md` for venv setup and run instructions.

### Configuring your MCP Client

Use the following JSON for your mcp configuration based on your MCP client.

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "<path/to/the/folder/PYTHON>",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

A full example is at `examples/mcp-client-config.json`.

### MCP Configuration Locations

Depending on which MCP client you're using, the configuration file location will differ:

| MCP Client | Configuration File Location | Notes |
|------------|------------------------------|-------|
| Claude Desktop | `~/.config/claude-desktop/mcp.json` | On Windows: `%USERPROFILE%\.config\claude-desktop\mcp.json` |
| Cursor | `.cursor/mcp.json` | Located in your project root directory |
| Windsurf | `~/.config/windsurf/mcp.json` | On Windows: `%USERPROFILE%\.config\windsurf\mcp.json` |

Each client uses the same JSON format as shown in the example above. 
Simply place the configuration in the appropriate location for your MCP client.


## License
MIT

## Questions

For questions, you can reach me on X/Twitter: [@chongdashu](https://www.x.com/chongdashu)