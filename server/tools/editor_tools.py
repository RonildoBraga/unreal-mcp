"""
Editor Tools for Unreal MCP.

This module provides tools for controlling the Unreal Editor viewport and other editor functionality.
"""

import logging
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from mcp.server.fastmcp import Context, FastMCP

try:
    # FastMCP's Image content type — lets tools return image bytes inline
    # so the LLM sees the screenshot rather than just a file path.
    from mcp.server.fastmcp.utilities.types import Image
except ImportError:  # older FastMCP fallback
    Image = None  # type: ignore[assignment, misc]

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_editor_tools(mcp: FastMCP):
    """Register editor tools with the MCP server."""
    
    @mcp.tool()
    def get_actors_in_level(ctx: Context) -> List[Dict[str, Any]]:
        """Get a list of all actors in the current level."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning("Failed to connect to Unreal Engine")
                return []
                
            response = unreal.send_command("get_actors_in_level", {})
            
            if not response:
                logger.warning("No response from Unreal Engine")
                return []
                
            # Log the complete response for debugging
            logger.info(f"Complete response from Unreal: {response}")
            
            # Check response format
            if "result" in response and "actors" in response["result"]:
                actors = response["result"]["actors"]
                logger.info(f"Found {len(actors)} actors in level")
                return actors
            elif "actors" in response:
                actors = response["actors"]
                logger.info(f"Found {len(actors)} actors in level")
                return actors
                
            logger.warning(f"Unexpected response format: {response}")
            return []
            
        except Exception as e:
            logger.error(f"Error getting actors: {e}")
            return []

    @mcp.tool()
    def find_actors_by_name(ctx: Context, pattern: str) -> List[str]:
        """Find actors by name pattern."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning("Failed to connect to Unreal Engine")
                return []
                
            response = unreal.send_command("find_actors_by_name", {
                "pattern": pattern
            })
            
            if not response:
                return []
                
            return response.get("actors", [])
            
        except Exception as e:
            logger.error(f"Error finding actors: {e}")
            return []
    
    @mcp.tool()
    def spawn_actor(
        ctx: Context,
        name: str,
        type: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """Create a new actor in the current level.
        
        Args:
            ctx: The MCP context
            name: The name to give the new actor (must be unique)
            type: The type of actor to create (e.g. StaticMeshActor, PointLight)
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            
        Returns:
            Dict containing the created actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Ensure all parameters are properly formatted
            params = {
                "name": name,
                "type": type.upper(),  # Make sure type is uppercase
                "location": location,
                "rotation": rotation
            }
            
            # Validate location and rotation formats
            for param_name in ["location", "rotation"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"success": False, "message": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            logger.info(f"Creating actor '{name}' of type '{type}' with params: {params}")
            response = unreal.send_command("spawn_actor", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            # Log the complete response for debugging
            logger.info(f"Actor creation response: {response}")
            
            # Handle error responses correctly
            if response.get("status") == "error":
                error_message = response.get("error", "Unknown error")
                logger.error(f"Error creating actor: {error_message}")
                return {"success": False, "message": error_message}
            
            return response
            
        except Exception as e:
            error_msg = f"Error creating actor: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def spawn_static_mesh_actor(
        ctx: Context,
        name: str,
        mesh_path: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0],
        scale: List[float] = [1.0, 1.0, 1.0],
        folder_path: str = "",
    ) -> Dict[str, Any]:
        """Spawn a StaticMeshActor and assign its mesh in one call.

        This is the ergonomic path for placing Megascans / Quixel assets from
        the /Game/Migrated/ tree. Internally combines spawn_actor (StaticMeshActor)
        with a mesh-load + SetStaticMesh on the component, sidestepping the fact
        that set_actor_property cannot reach the inner UStaticMeshComponent::StaticMesh.

        Args:
            name:        Unique actor name in the level.
            mesh_path:   /Game/-prefixed object path of the StaticMesh
                         (e.g. "/Game/Migrated/.../SM_RomanColumn_01.SM_RomanColumn_01").
                         Bare package path without the trailing object suffix
                         also works.
            location:    [x, y, z] world cm.
            rotation:    [pitch, yaw, roll] degrees.
            scale:       [x, y, z] uniform-or-non-uniform scale. Default [1,1,1].
            folder_path: Optional Outliner folder for organization
                         (e.g. "Sanctuary/Columns"). Empty = root.

        Returns:
            The created actor's details (same shape as spawn_actor).
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            for param_name, vec in (("location", location), ("rotation", rotation), ("scale", scale)):
                if not isinstance(vec, list) or len(vec) != 3:
                    return {"success": False, "message": f"Invalid {param_name} format. Must be a list of 3 float values."}

            params = {
                "name": name,
                "mesh_path": mesh_path,
                "location": [float(v) for v in location],
                "rotation": [float(v) for v in rotation],
                "scale":    [float(v) for v in scale],
            }
            if folder_path:
                params["folder_path"] = folder_path

            response = unreal.send_command("spawn_static_mesh_actor", params)
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            if response.get("status") == "error":
                return {"success": False, "message": response.get("error", "Unknown error")}
            return response
        except Exception as e:
            logger.error(f"Error spawning static mesh actor: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def set_static_mesh_actor_mesh(ctx: Context, name: str, mesh_path: str) -> Dict[str, Any]:
        """Swap the mesh on an existing StaticMeshActor (in-place).

        Useful for replacing placeholders, cycling through Megascans variants,
        or fixing up legacy actors after asset migration.

        Args:
            name:      Actor name in the current level.
            mesh_path: /Game/-prefixed object path of the new StaticMesh.

        Returns:
            Updated actor details on success, error dict otherwise.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("set_static_mesh_actor_mesh", {
                "name": name,
                "mesh_path": mesh_path,
            })
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            if response.get("status") == "error":
                return {"success": False, "message": response.get("error", "Unknown error")}
            return response
        except Exception as e:
            logger.error(f"Error setting static mesh: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def delete_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Delete an actor by name."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("delete_actor", {
                "name": name
            })
            return response or {}
            
        except Exception as e:
            logger.error(f"Error deleting actor: {e}")
            return {}
    
    @mcp.tool()
    def set_actor_transform(
        ctx: Context,
        name: str,
        location: List[float]  = None,
        rotation: List[float]  = None,
        scale: List[float] = None
    ) -> Dict[str, Any]:
        """Set the transform of an actor."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            params = {"name": name}
            if location is not None:
                params["location"] = location
            if rotation is not None:
                params["rotation"] = rotation
            if scale is not None:
                params["scale"] = scale
                
            response = unreal.send_command("set_actor_transform", params)
            return response or {}
            
        except Exception as e:
            logger.error(f"Error setting transform: {e}")
            return {}
    
    @mcp.tool()
    def get_actor_properties(ctx: Context, name: str) -> Dict[str, Any]:
        """Get all properties of an actor."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("get_actor_properties", {
                "name": name
            })
            return response or {}
            
        except Exception as e:
            logger.error(f"Error getting properties: {e}")
            return {}

    @mcp.tool()
    def set_actor_property(
        ctx: Context,
        name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """
        Set a property on an actor.
        
        Args:
            name: Name of the actor
            property_name: Name of the property to set
            property_value: Value to set the property to
            
        Returns:
            Dict containing response from Unreal with operation status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("set_actor_property", {
                "name": name,
                "property_name": property_name,
                "property_value": property_value
            })
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set actor property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting actor property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    # @mcp.tool() commented out because it's buggy
    def focus_viewport(
        ctx: Context,
        target: str = None,
        location: List[float] = None,
        distance: float = 1000.0,
        orientation: List[float] = None
    ) -> Dict[str, Any]:
        """
        Focus the viewport on a specific actor or location.
        
        Args:
            target: Name of the actor to focus on (if provided, location is ignored)
            location: [X, Y, Z] coordinates to focus on (used if target is None)
            distance: Distance from the target/location
            orientation: Optional [Pitch, Yaw, Roll] for the viewport camera
            
        Returns:
            Response from Unreal Engine
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            params = {}
            if target:
                params["target"] = target
            elif location:
                params["location"] = location
            
            if distance:
                params["distance"] = distance
                
            if orientation:
                params["orientation"] = orientation
                
            response = unreal.send_command("focus_viewport", params)
            return response or {}
            
        except Exception as e:
            logger.error(f"Error focusing viewport: {e}")
            return {"status": "error", "message": str(e)}

    @mcp.tool()
    def spawn_blueprint_actor(
        ctx: Context,
        blueprint_name: str,
        actor_name: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """Spawn an actor from a Blueprint.
        
        Args:
            ctx: The MCP context
            blueprint_name: Name of the Blueprint to spawn from
            actor_name: Name to give the spawned actor
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            
        Returns:
            Dict containing the spawned actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Ensure all parameters are properly formatted
            params = {
                "blueprint_name": blueprint_name,
                "actor_name": actor_name,
                "location": location or [0.0, 0.0, 0.0],
                "rotation": rotation or [0.0, 0.0, 0.0]
            }
            
            # Validate location and rotation formats
            for param_name in ["location", "rotation"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"success": False, "message": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            logger.info(f"Spawning blueprint actor with params: {params}")
            response = unreal.send_command("spawn_blueprint_actor", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Spawn blueprint actor response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error spawning blueprint actor: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    # ─── Sprint 1 — editor state extensions ─────────────────────────────────

    @mcp.tool()
    def take_screenshot(
        ctx: Context,
        filename: str = "screenshot.png",
        show_ui: bool = False,
    ) -> Union["Image", Dict[str, Any]]:  # type: ignore[name-defined]
        """Capture a screenshot of the active editor viewport.

        Returns the PNG inline as an MCP Image (so the calling LLM can see it
        directly), with the absolute filepath also recorded on disk for
        long-term reference.

        Args:
            filename: Output filename. Bare names ("screenshot.png") land under
                      <Project>/Saved/Screenshots/. Absolute paths are honored
                      verbatim. ".png" is appended if missing. Default
                      "screenshot.png".
            show_ui:  Include editor UI gizmos (selection outlines, grids).
                      Default False — cleaner captures for visual evaluation.
                      (Currently advisory only; UE's editor viewport screenshot
                      path captures whatever the active viewport renders.)

        Returns:
            On success: a FastMCP Image content block containing the PNG.
            On failure: an error dict — most commonly because the editor has
            no active viewport, or the destination path can't be written.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            # C++ side accepts both "filepath" and "filename" since v0.7.2; we
            # send "filepath" — it's the original wire-format name and matches
            # what the handler primarily reads.
            response = unreal.send_command(
                "take_screenshot",
                {"filepath": filename, "show_ui": show_ui},
            )
            if not response:
                return {"error": "No response from Unreal Engine"}
            if response.get("status") == "error":
                return {"error": response.get("error", "Unknown error")}

            # Plugin returns the absolute path of the saved PNG. Read it back
            # and hand the bytes to the LLM as inline image content.
            payload = response.get("result", response)
            abs_path = payload.get("filepath") if isinstance(payload, dict) else None
            if not abs_path:
                return {"error": "Response missing filepath", "raw": response}

            png_path = Path(abs_path)
            if not png_path.exists():
                return {"error": f"Screenshot saved but file not found at {abs_path}"}

            if Image is None:
                # FastMCP too old to support Image type — fall back to path.
                return {"filepath": str(png_path), "size_bytes": png_path.stat().st_size}

            return Image(path=str(png_path))
        except Exception as e:
            logger.error(f"take_screenshot error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def get_viewport_camera(ctx: Context) -> Dict[str, Any]:
        """Read the current editor viewport camera location and rotation.

        Returns:
            {
              "location": {"x": float, "y": float, "z": float},   # cm, UE world space
              "rotation": {"pitch": float, "yaw": float, "roll": float}  # degrees
            }
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            response = unreal.send_command("get_viewport_camera", {})
            if response and "result" in response:
                return response["result"]
            return response or {"error": "no response"}
        except Exception as e:
            logger.error(f"get_viewport_camera error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def set_viewport_camera(ctx: Context, location: List[float], rotation: List[float]) -> Dict[str, Any]:
        """Move the editor viewport camera to a specific pose.

        Args:
            location: [x, y, z] in UE world units (cm). UE is Z-up, left-handed.
            rotation: [pitch, yaw, roll] in degrees. Pitch negative looks down.

        Useful for setting up consistent screenshot vantages — e.g. matching
        Lauder's gameplay camera angle (~55° pitch down) when evaluating
        scene mockups in the editor.

        Returns: {"success": True} on success.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            response = unreal.send_command("set_viewport_camera", {
                "location": location,
                "rotation": rotation,
            })
            if response and "result" in response:
                return response["result"]
            return response or {"error": "no response"}
        except Exception as e:
            logger.error(f"set_viewport_camera error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def execute_console_command(ctx: Context, command: str) -> Dict[str, Any]:
        """Execute an arbitrary UE console command against the editor world.

        Common uses:
            "stat fps"                  show FPS overlay
            "HighResShot 1920x1080"     take a high-res screenshot
            "showflag.Sprites 0"        hide editor gizmos
            "r.Lumen.Reflections 0"     toggle a CVar via console syntax

        For CVar set/get specifically, prefer the typed `set_cvar` / `get_cvar`
        tools — they parse the value as the right type.

        Args:
            command: Full console command string.

        Returns: {"command": "...", "success": bool}.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            response = unreal.send_command("execute_console_command", {"command": command})
            if response and "result" in response:
                return response["result"]
            return response or {"error": "no response"}
        except Exception as e:
            logger.error(f"execute_console_command error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def set_cvar(ctx: Context, name: str, value: str) -> Dict[str, Any]:
        """Set a UE console variable (CVar) to a specific value.

        CVars control engine behavior at runtime. Examples relevant to
        Lauder Phase 7 evaluation:
            r.RayTracing.SkyLight.MaxRayDistance   100000
            r.Lumen.Reflections                    1
            r.SSGI.Quality                         1

        Args:
            name:  CVar name (e.g. "r.Lumen.Reflections").
            value: New value as a string. Numeric values accepted as strings.

        Returns:
            {"name": "...", "value": "...", "current_value": "...", "success": bool}
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            response = unreal.send_command("set_cvar", {"name": name, "value": value})
            if response and "result" in response:
                return response["result"]
            return response or {"error": "no response"}
        except Exception as e:
            logger.error(f"set_cvar error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def get_cvar(ctx: Context, name: str) -> Dict[str, Any]:
        """Read the current value of a UE console variable.

        Args:
            name: CVar name.

        Returns:
            {
              "name": "...", "value": "<string>",
              "float_value": float, "int_value": int, "bool_value": bool
            }
            The typed variants are convenience accessors so callers don't
            have to parse the string.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            response = unreal.send_command("get_cvar", {"name": name})
            if response and "result" in response:
                return response["result"]
            return response or {"error": "no response"}
        except Exception as e:
            logger.error(f"get_cvar error: {e}")
            return {"error": str(e)}

    # ─── v0.7.6 — viewport mode + editor introspection ─────────────────────

    @mcp.tool()
    def get_viewport_mode(ctx: Context) -> Dict[str, Any]:
        """Get the current editor viewport render mode.

        Returns:
            {"mode": "Lit"|"Unlit"|"Wireframe"|..., "mode_index": int}
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return unreal.send_command("get_viewport_mode", {}) or {}
        except Exception as e:
            logger.error(f"get_viewport_mode error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def set_viewport_mode(ctx: Context, mode: str) -> Dict[str, Any]:
        """Set the active editor viewport render mode.

        Args:
            mode: One of "Lit", "Unlit", "Wireframe", "BrushWireframe",
                  "DetailLighting", "LightingOnly", "LightComplexity",
                  "ShaderComplexity", "LightmapDensity", "ReflectionOverride",
                  "VisualizeBuffer", "PathTracing". Case-insensitive.

        Returns:
            {"mode": "...", "mode_index": int, "success": True}
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return unreal.send_command("set_viewport_mode", {"mode": mode}) or {}
        except Exception as e:
            logger.error(f"set_viewport_mode error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def read_output_log(ctx: Context, lines: int = 50) -> Dict[str, Any]:
        """Read the last N lines of the editor's Output Log.

        Useful for diagnosing what UE logged during a recent operation —
        especially after migrate_assets, finalize_migration, or any tool
        that returned a non-obvious error.

        Args:
            lines: Number of trailing lines to return. Clamped to [1, 5000].
                   Default 50.

        Returns:
            {
              "log_path":      "<absolute path to .log>",
              "total_lines":   N,
              "returned_lines": K,
              "lines": ["...", "...", ...]
            }
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return unreal.send_command("read_output_log", {"lines": lines}) or {}
        except Exception as e:
            logger.error(f"read_output_log error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def get_async_compile_status(ctx: Context) -> Dict[str, Any]:
        """Snapshot the editor's async compile queues.

        Reports remaining shader-compile jobs + total asset-compile jobs
        (StaticMesh, Texture, etc.). Used to detect stalls before invoking
        heavy operations like finalize_migration. is_idle=True means there
        are no pending compiles right now.

        Returns:
            {"shader_jobs": N, "asset_compiles": M, "is_idle": bool}
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return unreal.send_command("get_async_compile_status", {}) or {}
        except Exception as e:
            logger.error(f"get_async_compile_status error: {e}")
            return {"error": str(e)}

    logger.info("Editor tools registered successfully")
