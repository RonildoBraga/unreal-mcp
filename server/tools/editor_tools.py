"""Editor Tools for Unreal MCP — actors, viewport, screenshots, console, PIE.

Tool surface (33 tools):

  Actors (12)
    get_actors_in_level             enumerate every actor in the loaded world
    find_actors_by_name             pattern-match against actor display names
    spawn_actor                     generic UClass spawn (any AActor subclass)
    spawn_static_mesh_actor         spawn StaticMeshActor + assign mesh in one call
    set_static_mesh_actor_mesh      swap a StaticMeshActor's mesh in place
    set_static_mesh_material        swap a slot's material on a SMC
    delete_actor                    remove an actor from the level
    set_actor_transform             write location/rotation/scale selectively
    get_actor_properties            dump every UPROPERTY on an actor
    get_actor_property              read a single dotted-path property
    set_actor_property              write a single dotted-path property
    spawn_blueprint_actor           spawn an actor from a BP class

  Viewport + state (8)
    take_screenshot                 PNG of editor viewport (inline Image bytes)
    get_viewport_camera             camera location + rotation
    set_viewport_camera             move camera to a pose
    get_viewport_mode               Lit / Unlit / Wireframe / etc.
    set_viewport_mode               switch render mode
    execute_console_command         arbitrary `stat fps`-style command
    set_cvar                        write a UE CVar
    get_cvar                        read a UE CVar

  Editor introspection (3)
    read_output_log                 tail the editor's Output Log
    get_async_compile_status        shader + asset compile queues
    recompile_live                  trigger Live Coding rebuild of the plugin DLL

  PIE control (8)
    start_pie                       enter Play-In-Editor
    stop_pie                        exit PIE
    is_pie_active                   PIE running?
    pie_get_player                  pawn 0 state (location, velocity, fall flag)
    pie_set_player                  teleport pawn 0
    pie_apply_movement              hold-W-for-N-seconds equivalent
    pie_screenshot                  in-game viewport capture (inline Image bytes)
    get_selected_actors             current editor selection

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp`.
"""

import logging
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool, OMIT, dispatch_unreal_command

try:
    # FastMCP's Image content type — lets tools return image bytes inline
    # so the LLM sees the screenshot rather than just a file path.
    from mcp.server.fastmcp.utilities.types import Image
except ImportError:  # pragma: no cover — older FastMCP fallback
    Image = None  # type: ignore[assignment, misc]

logger = logging.getLogger("UnrealMCP")


def register_editor_tools(mcp: FastMCP):
    """Register editor tools with the MCP server."""

    # ─── Actors ──────────────────────────────────────────────────────────

    @unreal_tool(mcp)
    def get_actors_in_level(ctx: Context) -> Dict[str, Any]:
        """List every actor in the currently-loaded editor level.

        Returns:
            {"success": true, "actors": [{name, class_name, ...}, ...], "count": N}
        """

    @unreal_tool(mcp)
    def find_actors_by_name(ctx: Context, pattern: str) -> Dict[str, Any]:
        """Find actors by name pattern (case-insensitive substring match).

        Args:
            pattern: Substring to search for in actor display names.

        Returns:
            {"success": true, "actors": [...], "count": N}
        """

    @unreal_tool(mcp)
    def spawn_actor(
        ctx: Context,
        name: str,
        type: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0],
    ) -> Dict[str, Any]:
        """Create a new actor in the current level.

        Args:
            name:     Unique actor display name.
            type:     Actor class. Accepts (a) full path "/Script/Engine.SkyAtmosphere",
                      (b) bare name "SkyAtmosphere" (tried under /Script/Engine and
                      /Script/UnrealEd), (c) any UClass name resolvable via
                      TryFindTypeSlow. The v0.7.10 generic lookup handles any
                      AActor subclass — no hardcoded list.
            location: [X, Y, Z] world cm.
            rotation: [Pitch, Yaw, Roll] degrees.

        Returns:
            {"success": bool, ...actor data}
        """

    @unreal_tool(mcp)
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

        Ergonomic path for placing Megascans / Quixel assets — combines
        spawn_actor (StaticMeshActor) with a mesh-load + SetStaticMesh on
        the component, sidestepping the fact that set_actor_property cannot
        reach the inner UStaticMeshComponent::StaticMesh.

        Args:
            name:        Unique actor name in the level.
            mesh_path:   /Game/-prefixed object path of the StaticMesh.
                         Bare package path without trailing object suffix also OK.
            location:    [X, Y, Z] world cm.
            rotation:    [Pitch, Yaw, Roll] degrees.
            scale:       [X, Y, Z] scale. Default [1, 1, 1].
            folder_path: Optional Outliner folder. Empty = root.

        Returns:
            {"success": bool, ...actor data}
        """

    @unreal_tool(mcp)
    def set_static_mesh_actor_mesh(
        ctx: Context, name: str, mesh_path: str
    ) -> Dict[str, Any]:
        """Swap the mesh on an existing StaticMeshActor in place.

        Args:
            name:      Actor name in the current level.
            mesh_path: /Game/-prefixed object path of the new StaticMesh.

        Returns:
            {"success": bool, ...updated actor data}
        """

    @unreal_tool(mcp)
    def set_static_mesh_material(
        ctx: Context,
        name: str,
        material_path: str,
        slot: int = 0,
    ) -> Dict[str, Any]:
        """Swap one material slot on a StaticMeshActor (v0.7.10).

        Equivalent to dotted-path "StaticMeshComponent.OverrideMaterials.<slot>"
        via set_actor_property, but ergonomic for the common "Megascans
        migration lost the parent material, swap slot 0" case. Calls
        UStaticMeshComponent::SetMaterial under the hood (handles
        MarkRenderStateDirty internally).

        Args:
            name:          Actor name in the current level.
            material_path: /Game/-prefixed object path of the material or MI.
            slot:          Material slot index. Default 0.

        Returns:
            {"success": bool, "actor": ..., "slot": int, "material_path": ...}
        """

    @unreal_tool(mcp)
    def delete_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Delete an actor by name.

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def set_actor_transform(
        ctx: Context,
        name: str,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """Set one or more transform components on an actor.

        Args:
            name:     Actor name in the current level.
            location: [X, Y, Z] world cm. Omit to keep current.
            rotation: [Pitch, Yaw, Roll] degrees. Omit to keep current.
            scale:    [X, Y, Z] scale. Omit to keep current.

        Returns:
            {"success": bool, ...updated transform}
        """

    @unreal_tool(mcp)
    def get_actor_properties(ctx: Context, name: str) -> Dict[str, Any]:
        """Get all UPROPERTY values on an actor.

        Returns:
            {"success": true, "properties": {...}, ...}
        """

    @unreal_tool(mcp)
    def get_actor_property(
        ctx: Context, name: str, property_name: str
    ) -> Dict[str, Any]:
        """Read a single property at a dotted path (v0.7.10 read counterpart).

        Walks the same dotted paths set_actor_property walks — FObjectProperty
        hops, FStructProperty hops, FArrayProperty index hops — and returns
        the leaf value serialized to JSON:

          - Bool / Int / Float / Double / Byte  → number
          - Str / Name / Enum                   → string
          - Vector / Rotator / LinearColor / Color / Vector4 → list of components
          - UObject* (StaticMesh, Material, ...) → object path string ("" if null)
          - TArray                              → {kind: "Array", length, inner}

        Args:
            name:          Actor's display name in the current level.
            property_name: Dotted path, same syntax as set_actor_property.

        Returns:
            {"success": bool, "actor": ..., "property": ..., "value": <json>}
        """

    @unreal_tool(mcp)
    def set_actor_property(
        ctx: Context,
        name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """Set a property on an actor at a dotted path.

        Args:
            name:           Actor name in the current level.
            property_name:  Dotted path (Component.Struct.Field, Array.0, etc.).
            property_value: JSON-typed value (number, string, [x,y,z], etc.).

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def spawn_blueprint_actor(
        ctx: Context,
        blueprint_name: str,
        actor_name: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0],
    ) -> Dict[str, Any]:
        """Spawn an actor from a Blueprint class.

        Args:
            blueprint_name: Name of the Blueprint to spawn from.
            actor_name:     Display name for the spawned actor.
            location:       [X, Y, Z] world cm.
            rotation:       [Pitch, Yaw, Roll] degrees.

        Returns:
            {"success": bool, ...actor data}
        """

    # ─── Viewport + screenshots ──────────────────────────────────────────

    @mcp.tool()
    def take_screenshot(
        ctx: Context,
        filename: str = "screenshot.png",
    ) -> Union["Image", Dict[str, Any]]:  # type: ignore[name-defined]
        """Capture a screenshot of the active editor viewport.

        Returns the PNG inline as an MCP Image (so the calling LLM can see
        it directly), with the absolute filepath also recorded on disk for
        long-term reference.

        Args:
            filename: Output filename. Bare names ("screenshot.png") land
                      under <Project>/Saved/Screenshots/. Absolute paths
                      are honored verbatim. ".png" is appended if missing.

        Returns:
            On success: a FastMCP Image content block containing the PNG.
            On failure: {"success": False, "error": "..."}.
        """
        # NB: not a `@unreal_tool` — needs to read the PNG off disk and
        # wrap into the FastMCP Image content type before returning.
        # The C++ side accepts both "filepath" and "filename" since v0.7.2;
        # we send "filepath" — wire-format name predates the rename.
        response = dispatch_unreal_command(
            "take_screenshot", {"filepath": filename}
        )
        if not response.get("success", False):
            return response

        abs_path = response.get("filepath")
        if not abs_path:
            return {
                "success": False,
                "error": "Response missing filepath",
                "raw": response,
            }

        png_path = Path(abs_path)
        if not png_path.exists():
            return {
                "success": False,
                "error": f"Screenshot saved but file not found at {abs_path}",
            }

        if Image is None:  # pragma: no cover
            return {
                "success": True,
                "filepath": str(png_path),
                "size_bytes": png_path.stat().st_size,
            }

        return Image(path=str(png_path))

    @unreal_tool(mcp)
    def get_viewport_camera(ctx: Context) -> Dict[str, Any]:
        """Read the current editor viewport camera location and rotation.

        Returns:
            {
              "success": true,
              "location": {"x": float, "y": float, "z": float},
              "rotation": {"pitch": float, "yaw": float, "roll": float}
            }
        """

    @unreal_tool(mcp)
    def set_viewport_camera(
        ctx: Context, location: List[float], rotation: List[float]
    ) -> Dict[str, Any]:
        """Move the editor viewport camera to a specific pose.

        Args:
            location: [X, Y, Z] world cm.
            rotation: [Pitch, Yaw, Roll] degrees. Pitch-negative looks down.

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def get_viewport_mode(ctx: Context) -> Dict[str, Any]:
        """Get the current editor viewport render mode.

        Returns:
            {"success": true, "mode": "Lit"|"Unlit"|"Wireframe"|..., "mode_index": int}
        """

    @unreal_tool(mcp)
    def set_viewport_mode(ctx: Context, mode: str) -> Dict[str, Any]:
        """Set the active editor viewport render mode.

        Args:
            mode: One of "Lit", "Unlit", "Wireframe", "BrushWireframe",
                  "DetailLighting", "LightingOnly", "LightComplexity",
                  "ShaderComplexity", "LightmapDensity", "ReflectionOverride",
                  "VisualizeBuffer", "PathTracing". Case-insensitive.

        Returns:
            {"success": bool, "mode": "...", "mode_index": int}
        """

    @unreal_tool(mcp)
    def execute_console_command(ctx: Context, command: str) -> Dict[str, Any]:
        """Execute an arbitrary UE console command against the editor world.

        Examples:
            "stat fps"               show FPS overlay
            "HighResShot 1920x1080"  high-res screenshot
            "showflag.Sprites 0"     hide editor gizmos

        For CVar set/get specifically, prefer the typed `set_cvar` / `get_cvar`
        tools.

        Args:
            command: Full console command string.

        Returns:
            {"success": bool, "command": "..."}
        """

    @unreal_tool(mcp)
    def set_cvar(ctx: Context, name: str, value: str) -> Dict[str, Any]:
        """Set a UE console variable (CVar) to a specific value.

        Args:
            name:  CVar name (e.g. "r.Lumen.Reflections").
            value: New value as string. Numeric values accepted as strings.

        Returns:
            {"success": bool, "name": "...", "value": "...", "current_value": "..."}
        """

    @unreal_tool(mcp)
    def get_cvar(ctx: Context, name: str) -> Dict[str, Any]:
        """Read the current value of a UE console variable.

        Args:
            name: CVar name.

        Returns:
            {
              "success": true,
              "name": "...", "value": "<string>",
              "float_value": float, "int_value": int, "bool_value": bool
            }
        """

    # ─── Editor introspection ────────────────────────────────────────────

    @unreal_tool(mcp)
    def read_output_log(ctx: Context, lines: int = 50) -> Dict[str, Any]:
        """Read the last N lines of the editor's Output Log.

        Args:
            lines: Number of trailing lines. Clamped to [1, 5000]. Default 50.

        Returns:
            {
              "success": true,
              "log_path": "<absolute path>",
              "total_lines": N,
              "returned_lines": K,
              "lines": ["...", ...]
            }
        """

    @unreal_tool(mcp)
    def get_async_compile_status(ctx: Context) -> Dict[str, Any]:
        """Snapshot the editor's async compile queues.

        Reports remaining shader-compile jobs + total asset-compile jobs
        (StaticMesh, Texture, etc.). Use to detect stalls before heavy
        operations like finalize_migration. ``is_idle=True`` means the
        engine has no pending compiles.

        Returns:
            {"success": true, "shader_jobs": N, "asset_compiles": M, "is_idle": bool}
        """

    @unreal_tool(mcp)
    def recompile_live(ctx: Context) -> Dict[str, Any]:
        """Trigger an Unreal Live Coding rebuild of the plugin's C++ module.

        Same effect as pressing Ctrl+Alt+F11 inside the editor: UE detects
        modified source files and patches the running DLLs in place — no
        editor restart required.

        Autonomous build loop:
            1. Edit / Write the .cpp / .h files on disk
            2. (Mirror them into the lauder3 plugin snapshot if needed)
            3. Call `recompile_live()`
            4. Wait ~30 seconds (Live Coding is async)
            5. Tools added or changed in the new source are now callable

        UE logs `LogLiveCoding: Live coding succeeded` (or `failed`) when
        the patch completes. Use `read_output_log(lines=N)` to verify.

        Returns:
            On success: {"success": true, "started": true, "note": "..."}
            If Live Coding is disabled or no editor world is available:
            {"success": false, "error": "..."}
        """

    # ─── PIE (Play-In-Editor) control ────────────────────────────────────

    @unreal_tool(mcp)
    def start_pie(ctx: Context) -> Dict[str, Any]:
        """Start a Play-In-Editor session (v0.7.11).

        Equivalent to pressing Alt+P. Returns immediately; the actual
        game-thread spin-up is asynchronous. Use the pie_* tools to drive
        and inspect the running session.

        Returns:
            {"success": bool, "already_active": bool}
        """

    @unreal_tool(mcp)
    def stop_pie(ctx: Context) -> Dict[str, Any]:
        """End the active Play-In-Editor session.

        Returns:
            {"success": bool, "was_active": bool}
        """

    @unreal_tool(mcp)
    def is_pie_active(ctx: Context) -> Dict[str, Any]:
        """Check whether a PIE session is currently running.

        Returns:
            {"success": true, "active": bool}
        """

    @unreal_tool(mcp)
    def pie_get_player(ctx: Context) -> Dict[str, Any]:
        """Read the active PIE player pawn's state (v0.7.11).

        Walkability oracle — after pie_apply_movement, check movement_mode +
        is_falling + final location to confirm the character actually
        landed where you expected.

        Returns:
            {
              "success": true,
              "location": [x, y, z],
              "rotation": [pitch, yaw, roll],
              "velocity": [vx, vy, vz],
              "pawn_class": "...",
              "movement_mode": "Walking" | "Falling" | "None" | ...,
              "is_falling": bool,
              "is_movement_in_progress": bool
            }
        """

    @unreal_tool(mcp)
    def pie_set_player(
        ctx: Context,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """Teleport the active PIE player pawn (v0.7.11).

        Bypasses collision (TeleportPhysics). Useful for spot-testing
        whether a specific position is walkable — drop the player there,
        wait a tick, then pie_get_player to see if movement_mode became
        Walking or stayed Falling.

        Args:
            location: [X, Y, Z] world cm. Omit to keep current.
            rotation: [Pitch, Yaw, Roll] degrees. Omit to keep current.

        Returns:
            {"success": bool, "location": [..], "rotation": [..]}
        """

    @unreal_tool(mcp)
    def pie_apply_movement(
        ctx: Context,
        direction: List[float],
        duration: float = 1.0,
        scale: float = 1.0,
    ) -> Dict[str, Any]:
        """Drive the PIE pawn in a direction for N seconds (v0.7.11).

        Calls APawn::AddMovementInput each tick for the requested duration —
        equivalent to holding a movement key. Fire-and-forget: returns
        immediately, but the movement keeps running. Sleep at least
        ``duration`` seconds client-side before reading pie_get_player.

        Args:
            direction: [X, Y, Z] world-space direction. Normalized internally.
            duration:  Seconds. Clamped to [0.05, 30.0]. Default 1.0.
            scale:     Input scale (1.0 = full speed). Default 1.0.

        Returns:
            {"success": bool, "direction": [..], "duration": float, "scale": float, "note": "..."}
        """

    @mcp.tool()
    def pie_screenshot(
        ctx: Context, filename: str = "pie_screenshot.png"
    ) -> Any:
        """Capture a screenshot from the PIE game viewport (v0.7.11).

        Different from take_screenshot — that one captures the editor's
        active viewport which when PIE is running may show editor gizmos.
        This captures the player's actual in-game view, no overlay.

        Args:
            filename: Output filename. Bare names land under
                      <Project>/Saved/Screenshots/. Default "pie_screenshot.png".

        Returns:
            FastMCP Image content (inline PNG bytes) on success,
            else {"success": False, "error": "..."}.
        """
        # NB: not a `@unreal_tool` — wraps the response into an Image content
        # block before returning, same pattern as take_screenshot.
        response = dispatch_unreal_command("pie_screenshot", {"filename": filename})
        if not response.get("success", False):
            return response

        path = response.get("path")
        if Image is not None and path:
            try:
                return Image(path=path)
            except Exception as e:  # pragma: no cover
                logger.warning(f"pie_screenshot Image() wrap failed: {e}")
        return response

    @unreal_tool(mcp)
    def get_selected_actors(ctx: Context) -> Dict[str, Any]:
        """Read the editor's current actor selection (v0.7.12).

        Useful for capturing a hand-curated subset of a large scene without
        describing them from a screenshot.

        Returns:
            {
              "success": true,
              "actors": [
                {
                  "name": "<display label>",
                  "internal_name": "<UObject name>",
                  "class": "...",
                  "folder_path": "Outliner/folder/path",
                  "location": [x, y, z]
                },
                ...
              ],
              "count": N
            }
        """

    logger.info("Editor tools registered successfully")
