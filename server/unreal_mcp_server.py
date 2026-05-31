"""
Unreal Engine MCP Server

A simple MCP server for interacting with Unreal Engine.
"""

import logging
import socket
import sys
import json
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional
from mcp.server.fastmcp import FastMCP

# Configure logging with more detailed format
logging.basicConfig(
    level=logging.DEBUG,  # Change to DEBUG level for more details
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler('unreal_mcp.log'),
        # logging.StreamHandler(sys.stdout) # Remove this handler to unexpected non-whitespace characters in JSON
    ]
)
logger = logging.getLogger("UnrealMCP")

# Configuration
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557

class UnrealConnection:
    """Connection to an Unreal Engine instance."""
    
    def __init__(self):
        """Initialize the connection."""
        self.socket = None
        self.connected = False
    
    def connect(self) -> bool:
        """Connect to the Unreal Engine instance."""
        try:
            # Close any existing socket
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            
            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(5)  # 5 second timeout
            
            # Set socket options for better stability
            self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            
            # Set larger buffer sizes
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
            
            self.socket.connect((UNREAL_HOST, UNREAL_PORT))
            self.connected = True
            logger.info("Connected to Unreal Engine")
            return True
            
        except Exception as e:
            logger.error(f"Failed to connect to Unreal: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """Disconnect from the Unreal Engine instance."""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.socket = None
        self.connected = False

    def receive_full_response(self, sock, buffer_size=4096) -> bytes:
        """Receive a complete response from Unreal, handling chunked data."""
        chunks = []
        sock.settimeout(5)  # 5 second timeout
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise Exception("Connection closed before receiving data")
                    break
                chunks.append(chunk)
                
                # Process the data received so far
                data = b''.join(chunks)
                decoded_data = data.decode('utf-8')
                
                # Try to parse as JSON to check if complete
                try:
                    json.loads(decoded_data)
                    logger.info(f"Received complete response ({len(data)} bytes)")
                    return data
                except json.JSONDecodeError:
                    # Not complete JSON yet, continue reading
                    logger.debug(f"Received partial response, waiting for more data...")
                    continue
                except Exception as e:
                    logger.warning(f"Error processing response chunk: {str(e)}")
                    continue
        except socket.timeout:
            logger.warning("Socket timeout during receive")
            if chunks:
                # If we have some data already, try to use it
                data = b''.join(chunks)
                try:
                    json.loads(data.decode('utf-8'))
                    logger.info(f"Using partial response after timeout ({len(data)} bytes)")
                    return data
                except:
                    pass
            raise Exception("Timeout receiving Unreal response")
        except Exception as e:
            logger.error(f"Error during receive: {str(e)}")
            raise
    
    def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """Send a command to Unreal Engine and get the response."""
        # Always reconnect for each command, since Unreal closes the connection after each command
        # This is different from Unity which keeps connections alive
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
        
        if not self.connect():
            logger.error("Failed to connect to Unreal Engine for command")
            return None
        
        try:
            # Match Unity's command format exactly
            command_obj = {
                "type": command,  # Use "type" instead of "command"
                "params": params or {}  # Use Unity's params or {} pattern
            }
            
            # Send without newline, exactly like Unity
            command_json = json.dumps(command_obj)
            logger.info(f"Sending command: {command_json}")
            self.socket.sendall(command_json.encode('utf-8'))
            
            # Read response using improved handler
            response_data = self.receive_full_response(self.socket)
            response = json.loads(response_data.decode('utf-8'))
            
            # Log complete response for debugging
            logger.info(f"Complete response from Unreal: {response}")
            
            # Check for both error formats: {"status": "error", ...} and {"success": false, ...}
            if response.get("status") == "error":
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (status=error): {error_message}")
                # We want to preserve the original error structure but ensure error is accessible
                if "error" not in response:
                    response["error"] = error_message
            elif response.get("success") is False:
                # This format uses {"success": false, "error": "message"} or {"success": false, "message": "message"}
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (success=false): {error_message}")
                # Convert to the standard format expected by higher layers
                response = {
                    "status": "error",
                    "error": error_message
                }
            
            # Always close the connection after command is complete
            # since Unreal will close it on its side anyway
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
            
            return response
            
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            # Always reset connection state on any error
            self.connected = False
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            return {
                "status": "error",
                "error": str(e)
            }

# Global connection state
_unreal_connection: UnrealConnection = None

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Get the connection to Unreal Engine."""
    global _unreal_connection
    try:
        if _unreal_connection is None:
            _unreal_connection = UnrealConnection()
            if not _unreal_connection.connect():
                logger.warning("Could not connect to Unreal Engine")
                _unreal_connection = None
        else:
            # Verify connection is still valid with a ping-like test
            try:
                # Simple test by sending an empty buffer to check if socket is still connected
                _unreal_connection.socket.sendall(b'\x00')
                logger.debug("Connection verified with ping test")
            except Exception as e:
                logger.warning(f"Existing connection failed: {e}")
                _unreal_connection.disconnect()
                _unreal_connection = None
                # Try to reconnect
                _unreal_connection = UnrealConnection()
                if not _unreal_connection.connect():
                    logger.warning("Could not reconnect to Unreal Engine")
                    _unreal_connection = None
                else:
                    logger.info("Successfully reconnected to Unreal Engine")
        
        return _unreal_connection
    except Exception as e:
        logger.error(f"Error getting Unreal connection: {e}")
        return None

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    """Handle server startup and shutdown."""
    global _unreal_connection
    logger.info("UnrealMCP server starting up")
    try:
        _unreal_connection = get_unreal_connection()
        if _unreal_connection:
            logger.info("Connected to Unreal Engine on startup")
        else:
            logger.warning("Could not connect to Unreal Engine on startup")
    except Exception as e:
        logger.error(f"Error connecting to Unreal Engine on startup: {e}")
        _unreal_connection = None
    
    try:
        yield {}
    finally:
        if _unreal_connection:
            _unreal_connection.disconnect()
            _unreal_connection = None
        logger.info("Unreal MCP server shut down")

# Initialize server
mcp = FastMCP(
    "UnrealMCP",
    description="Unreal Engine integration via Model Context Protocol",
    lifespan=server_lifespan
)

# Import and register tools
from tools.editor_tools import register_editor_tools
from tools.blueprint_tools import register_blueprint_tools
from tools.node_tools import register_blueprint_node_tools
from tools.project_tools import register_project_tools
from tools.umg_tools import register_umg_tools
from tools.asset_tools import register_asset_tools
from tools.level_tools import register_level_tools
from tools.material_tools import register_material_tools
from tools.outliner_tools import register_outliner_tools

# Register tools
register_editor_tools(mcp)
register_blueprint_tools(mcp)
register_blueprint_node_tools(mcp)
register_project_tools(mcp)
register_umg_tools(mcp)
register_asset_tools(mcp)
register_level_tools(mcp)
register_material_tools(mcp)
register_outliner_tools(mcp)

@mcp.prompt()
def info():
    """High-level overview of the UnrealMCP server.

    The authoritative tool catalog is what you discover via the MCP
    `tools/list` request — every `@mcp.tool()` function's docstring is
    surfaced there with full signature + return-shape documentation.
    Keeping a parallel prose summary in this prompt got out of date almost
    immediately as tools were added, so we deliberately don't try.
    """
    return """
# Unreal MCP — overview

Categories of tools available (use `tools/list` for the full catalog):

- **Editor** (actors, viewport camera + mode, screenshots, console, CVars,
  output log tail, async compile status)
- **Asset** (registry queries, mutations, cross-project migration with
  `migrate_assets` + `finalize_migration`, generic `import_asset`)
- **Material** (instance creation, parameter tuning, parent + uses)
- **Outliner** (folder organization)
- **Level** (load, save, save-all-dirty)
- **Blueprint** (class creation, components, compile)
- **Blueprint nodes** (event/function nodes, pins, connections, variables)
- **UMG widget** (widget BPs, text/button, bindings, viewport)
- **Project** (input action mappings)

## Power tools worth knowing

- `set_actor_property(name, "Path.To.Property", value)` — walks dotted
  paths through component, struct, AND array-index hops (so
  `Settings.AutoExposureBias` on PostProcessVolume works, as does
  `StaticMeshComponent.OverrideMaterials.0`). Accepts JSON-typed values
  for vectors, rotators, Vector4, colors, and `/Game/`-prefixed asset
  path strings for object references. Broadcasts PostEditChangeProperty
  after each write so the renderer reflects the change.
- `get_actor_property(name, "Path.To.Property")` — read counterpart.
  Returns numeric/string/struct/object-path values matching the leaf type.
- `spawn_actor(name, type, ...)` — generic UClass lookup. Accepts any
  AActor subclass: short names like `"SkyAtmosphere"` resolve via
  `/Script/Engine.<name>`; full paths also work.
- `spawn_static_mesh_actor` — spawn + mesh assignment + optional Outliner
  folder placement in one call.
- `set_static_mesh_material(name, material_path, slot)` — ergonomic
  fix for "Megascans migration lost the parent material, swap slot 0".
- `take_screenshot` — returns the PNG inline; forces a fresh viewport
  redraw so you actually see your most recent changes.
- `read_output_log` — tail the editor log when a tool's behavior is
  surprising.
- `get_async_compile_status` — poll before invoking heavy batches.

## PIE control (v0.7.11) — autonomous walkability oracle

- `start_pie` / `stop_pie` / `is_pie_active` — programmatic Play-In-Editor.
- `pie_get_player` — reads pawn 0's `{location, rotation, velocity,
  movement_mode, is_falling, is_movement_in_progress}`. The
  walkability oracle: `is_falling=true` at a spot means no floor there.
- `pie_set_player(location?, rotation?)` — teleport for spot-testing.
- `pie_apply_movement(direction, duration, scale)` — fire-and-forget
  "hold W for N seconds" equivalent. Sleep `duration` client-side
  then `pie_get_player` to read the result.
- `pie_screenshot(filename)` — in-game viewport, no editor gizmos.

Pattern: `start_pie → pie_set_player(test_spot) → sleep → pie_get_player
→ pie_apply_movement → sleep → pie_get_player → pie_screenshot → stop_pie`.

## Best practices

- **Reuse actor names**: `spawn_actor` and `spawn_static_mesh_actor`
  reject duplicate names. Pick a stable scheme.
- **Save explicitly**: changes don't persist to the .umap file until you
  call `save_current_level` (or `save_all_dirty`).
- **Validate via the catalog**: if a tool returns "Unknown command",
  the C++ DLL is older than the Python wrapper. Reopen the editor on
  the rebuilt DLL.
- **Path style**: object paths use the `/Game/Foo/Bar.Bar` form, package
  paths use `/Game/Foo/Bar` (no suffix). Most tools accept either and
  normalize internally.
"""

# Run the server
if __name__ == "__main__":
    logger.info("Starting MCP server with stdio transport")
    mcp.run(transport='stdio') 