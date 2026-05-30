# Installing

Two install paths depending on whether you're a **user** of the plugin or a
**contributor** to it.

## For users — drop-in zip install

The plugin is meant to drop into any existing UE 5.7+ project's `Plugins/`
folder, no compile step required if you grab the release zip (prebuilt
binaries included).

1. Download the latest release zip from
   [github.com/RonildoBraga/unreal-mcp/releases](https://github.com/RonildoBraga/unreal-mcp/releases)
   (or grab the `plugin/` directory from `main` if you're tracking head).
2. Extract into your project's `Plugins/` folder so that:

   ```
   YourProject/
   └── Plugins/
       └── UnrealMCP/
           ├── UnrealMCP.uplugin
           ├── Binaries/         (prebuilt for UE 5.7 + Win64)
           └── Source/
   ```

3. **Set up the Python MCP server side.** Clone or download the `server/`
   directory from this repo to a known location, then register it with your
   MCP client. Example for Claude clients:

   ```json
   {
     "mcpServers": {
       "unrealMCP": {
         "command": "uv",
         "args": [
           "--directory", "/absolute/path/to/unreal-mcp/server",
           "run", "unreal_mcp_server.py"
         ]
       }
     }
   }
   ```

   See `examples/mcp-client-config.json` for the full example.

4. **Open your UE project.** On first load you may see a "BP-only project
   loading C++ plugin" prompt — accept it. The plugin's TCP server starts
   automatically and listens on port 55557.

5. **Restart your MCP client** so it spawns the new Python server and
   discovers the tools.

If you're on UE 5.7 + Win64 the prebuilt binaries Just Work. For other
platforms (UE 5.7 + Mac/Linux) you need a project with C++ scaffolding so
UBT can compile the plugin on first load.

## For contributors — dev junction setup

If you're hacking on the plugin itself, work in this repo with a single
source of truth on disk:

1. Clone the repo:
   ```
   git clone https://github.com/RonildoBraga/unreal-mcp.git
   cd unreal-mcp
   ```

2. Set up the dev junction so the sample project sees the plugin source:
   ```powershell
   .\scripts\setup-dev-junction.ps1
   ```
   This creates a Windows junction at `sample/Plugins/UnrealMCP` pointing at
   the repo's `plugin/` directory. UE sees the plugin via the junction; edits
   to `plugin/` are immediately visible to the sample project's build.

3. Set up the Python server's venv:
   ```
   cd server
   uv venv
   uv pip install -e .
   ```

4. Open `sample/UnrealMCPSample.uproject` in UE 5.7. UBT compiles the plugin
   on first open; subsequent builds are incremental.

5. Iterate. Edit C++ in `plugin/Source/`; edit Python in `server/tools/`.
   Rebuild via UBT or use Live Coding (`Ctrl+Alt+F11` in the editor).

The dev junction means you don't have to copy plugin files between repos as
you iterate — one edit, one rebuild, immediately reflected in the sample
project. If you also have a "real" UE project that uses this plugin (like
Lauder), keep its `Plugins/UnrealMCP/` as a separate drop-in snapshot and
copy `plugin/` into it when you want to ship a snapshot to that project.
