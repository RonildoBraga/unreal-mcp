<div align="center">

# unreal-mcp

**Drive Unreal Engine through an LLM, not a UI.**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)
[![Tools](https://img.shields.io/badge/tools-101-green)](docs/tools/)

</div>

## The intention

Unreal Engine is the most powerful real-time renderer in the industry. It is also, by tradition, controlled through a dense graphical editor full of nested panels, hidden checkboxes, and decades of UI convention that take months to internalize. New users spend their first months learning *where things are* before they can think clearly about *what should be where*.

`unreal-mcp` exists to flip that ratio.

Over time, you should not need to open editor menus for most scene-construction tasks. You should be able to say:

> *"Add a directional light angled low like the setting sun, tint it warm, intensity 5."*

…and have it done. Unreal stays the renderer, stays the source of truth — but the interface between you and Unreal becomes an LLM, not menus and gizmos.

The split:

- **You** focus on **intent**: what the scene should feel like, what the player should experience, why each piece exists.
- **The LLM** focuses on **execution**: which actor to spawn, which property to set, where to put it, how to verify it landed.
- **Unreal** focuses on what it has always done: rendering, simulation, gameplay.

## What it does

The system bridges your MCP client (Claude Desktop, Claude Code, Cursor, Codex CLI, Windsurf, …) to a C++ plugin running inside the Unreal Editor via a Python server on `localhost:55557`. Once configured, the LLM can spawn actors, set properties, run Play-in-Editor, take screenshots, walk the asset registry, drive UMG widgets, import meshes — 101 tools spanning the entire editor surface — all from natural language.

Full tool catalog: [docs/tools/](docs/tools/) (or run `server/smoke_dispatch.py --list-only` for the live smoke selection).
Architecture: [docs/architecture.md](docs/architecture.md).

## Quick start

**Prerequisites:** Unreal Engine 5.7+, Python 3.12+, [`uv`](https://docs.astral.sh/uv/), an MCP client.

### 1. Install the plugin into your UE project

Copy `plugin/` into your project's `Plugins/` folder:

```
YourProject/Plugins/UnrealMCP/
├── UnrealMCP.uplugin
└── Source/UnrealMCP/
```

On next editor open, the plugin's TCP server starts on port 55557.

### 2. Install the Python server

```bash
cd server
uv venv
uv pip install -e .
```

### 3. Configure your MCP client

The JSON shape is the same across clients — only the config file location differs.

Example (Claude Desktop, `%APPDATA%\Claude\claude_desktop_config.json`):

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

Other clients (Claude Code, Cursor, Codex CLI, Windsurf): see [examples/mcp-client-config.json](examples/mcp-client-config.json).

### 4. Open your UE project and restart your MCP client

The plugin starts its TCP server when the editor loads. Restart the MCP client so it discovers the tools.

## Contributors

```bash
git clone https://github.com/RonildoBraga/unreal-mcp.git
cd unreal-mcp
./scripts/setup-dev-junction.ps1     # creates sample/Plugins/UnrealMCP junction
```

Open `sample/UnrealMCPSample.uproject` in UE 5.7. Edits to `plugin/Source/` are immediately picked up.

Style + architecture: [CONTRIBUTING.md](CONTRIBUTING.md), [docs/architecture.md](docs/architecture.md).

## License

MIT. See [LICENSE](LICENSE).
