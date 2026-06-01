"""Editor Tools for Unreal MCP — actors, viewport, screenshots, console, PIE.

Tool surface (52 tools):

  Actors (18)
    get_actors_in_level             enumerate every actor in the loaded world (deprecated; prefer find_actors)
    find_actors                     paged enumeration with class/name/folder filters (v0.8.0)
    find_actors_by_name             pattern-match against actor display names
    spawn_actor                     generic UClass spawn (any AActor subclass)
    spawn_actor_batch               batched spawn (v0.8.0)
    spawn_static_mesh_actor         spawn StaticMeshActor + assign mesh in one call
    set_static_mesh_actor_mesh      swap a StaticMeshActor's mesh in place
    set_static_mesh_material        swap a slot's material on a SMC
    delete_actor                    remove an actor from the level
    delete_actor_batch              batched delete (v0.8.0)
    set_actor_transform             write location/rotation/scale selectively
    get_actor_transform             read location/rotation/scale (v0.8.0)
    get_actor_properties            dump every UPROPERTY on an actor
    get_actor_property              read a single dotted-path property
    set_actor_property              write a single dotted-path property
    get_component_property          read a property on a named component (v0.8.0 thin shim)
    get_static_mesh_material        read the current material on a SMC slot (v0.8.0 thin shim)
    spawn_blueprint_actor           spawn an actor from a BP class

  Generalized object access (4) (v0.8.0)
    get_object_property             read any UObject's property (actor / asset / CDO / class)
    set_object_property             write any UObject's property
    get_world_settings              convenience: get_object_property(target="WorldSettings")
    set_world_settings              convenience: set_object_property(target="WorldSettings")

  Selection (4)
    get_selected_actors             current editor selection
    set_selected_actors             replace selection by display label / internal name
    clear_selection                 deselect everything
    focus_selected_actors           frame current selection in the viewport

  Viewport + state (10)
    frame_actor                     frame a single actor (no need to select first) (v0.8.0)
    set_show_flag                   toggle Lighting/Sprites/Bounds/etc. show flags (v0.8.0)
    take_screenshot                 PNG of editor viewport (inline Image bytes)
    get_viewport_camera             camera location + rotation
    set_viewport_camera             move camera to a pose
    get_viewport_mode               Lit / Unlit / Wireframe / etc.
    set_viewport_mode               switch render mode
    execute_console_command         arbitrary `stat fps`-style command
    set_cvar                        write a UE CVar
    get_cvar                        read a UE CVar

  Editor introspection (5)
    read_output_log                 tail the editor's Output Log
    get_async_compile_status        shader + asset compile queues
    wait_for_async_compile          block until compile queues empty (v0.8.0)
    dismiss_modal_dialog            close transient editor popups (v0.8.0)
    recompile_live                  trigger Live Coding rebuild of the plugin DLL

  PIE control (7)
    start_pie                       enter Play-In-Editor
    stop_pie                        exit PIE
    is_pie_active                   PIE running?
    pie_get_player                  pawn 0 state (location, velocity, fall flag)
    pie_set_player                  teleport pawn 0
    pie_apply_movement              hold-W-for-N-seconds equivalent
    pie_screenshot                  in-game viewport capture (inline Image bytes)

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
    def find_actors(
        ctx: Context,
        pattern: Optional[str] = None,
        class_filter: Optional[str] = None,
        folder: Optional[str] = None,
        limit: int = 200,
        offset: int = 0,
    ) -> Dict[str, Any]:
        """Paged actor enumeration with optional class / name / folder filters (v0.8.0).

        Replaces `get_actors_in_level` for the common case where you don't
        actually want the full scene dump. On RomanCave (~3000 actors),
        `get_actors_in_level` returns ~744 KB; `find_actors()` with a class
        filter or page limit returns a fraction of that.

        All filters are optional and combine with AND. With no filters and the
        default `limit=200`, returns the first 200 actors in the level and a
        `total` so you know whether to page.

        Filter semantics:
          pattern       Case-insensitive substring; matches against display
                        label OR internal name (an actor matches if either does).
          class_filter  AActor subclass. Bare name ("StaticMeshActor"),
                        /Script/Engine.* path, or anywhere-in-loaded-modules
                        — same lookup rules as `spawn_actor`'s `type`.
          folder        Outliner folder PREFIX match (case-insensitive).
                        Passing "Sanctuary" matches "Sanctuary/Floor" too.
                        Pass empty string to match only root-folder actors.

        Args:
            pattern: Optional name substring.
            class_filter: Optional AActor subclass name or full /Script/ path.
            folder: Optional Outliner folder prefix.
            limit: Page size. Default 200. Pass 0 for "all matches" (be careful).
            offset: Page offset. Default 0.

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
              "count": N,        # in this page (== len(actors))
              "total": K,        # total matching the filter (pre-paging)
              "offset": M,       # echoed
              "limit":  L        # echoed
            }
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
    def spawn_actor_batch(ctx: Context, actors: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Spawn multiple actors in a single MCP round-trip (v0.8.0).

        Each item in `actors` is a spawn descriptor with the same fields as
        `spawn_actor`: required `type` + `name`, optional `location`, `rotation`,
        `scale`. Items that fail (class lookup failed, name collision, etc.)
        are reported per-item in `errors` — the batch as a whole still succeeds.

        Use this for dense scene placement (e.g. RomanCave-style migration)
        where N singular calls would each pay ~50 ms of TCP round-trip overhead.

        Args:
            actors: List of spawn descriptors, e.g.
                [{"type": "StaticMeshActor", "name": "Wall_01",
                  "location": [0,0,0], "rotation": [0,0,0]}, ...]

        Returns:
            {
              "success": true,
              "requested_count": M,
              "spawned_count": N,
              "spawned": [{"name": "..."}, ...],
              "errors":  [{"name": "...", "error": "..."}, ...]
            }
        """

    @unreal_tool(mcp)
    def delete_actor_batch(ctx: Context, names: List[str]) -> Dict[str, Any]:
        """Delete multiple actors in a single MCP round-trip (v0.8.0).

        Each name resolves via the same two-pass display-label / internal-name
        lookup that `set_selected_actors` uses, so callers can pass either form
        (matches what `get_selected_actors` returns).

        Args:
            names: List of actor names (display labels or internal names).

        Returns:
            {
              "success": true,
              "requested_count": M,
              "deleted_count": N,
              "missing": [...]   # names that didn't resolve
            }
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
    def get_actor_transform(ctx: Context, name: str) -> Dict[str, Any]:
        """Read an actor's transform (v0.8.0 — read counterpart of set_actor_transform).

        Symmetry tool — returns location + rotation + scale as one payload
        so tight inspection loops don't pay three get_actor_property calls.
        Two-pass label / internal-name lookup, matches set_selected_actors.

        Args:
            name: Actor display label or internal name.

        Returns:
            {
              "success": true,
              "name": "<display label>",
              "internal_name": "<UObject name>",
              "location": [X, Y, Z],
              "rotation": [Pitch, Yaw, Roll],
              "scale":    [X, Y, Z]
            }
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

    @mcp.tool()
    def get_component_property(
        ctx: Context, actor: str, component: str, path: str
    ) -> Dict[str, Any]:
        """Read a property on a named component of an actor (v0.8.0 thin shim).

        Convenience wrapper over `get_object_property` — composes the dotted
        path `<component>.<path>` and pins the target to the actor name. Use
        when the call site is naturally component-rooted (e.g. inspecting a
        light's Intensity without typing out the StaticMeshComponent prefix).

        Args:
            actor:     Actor display label or internal name.
            component: Component name on the actor (e.g. "PointLightComponent").
            path:      Property path under that component (e.g. "Intensity",
                       "LightColor", "AttenuationRadius").

        Returns:
            Same shape as get_object_property.
        """
        return dispatch_unreal_command(
            "get_object_property",
            {"target": actor, "path": f"{component}.{path}"},
        )

    @mcp.tool()
    def get_static_mesh_material(
        ctx: Context, actor: str, slot_index: int = 0
    ) -> Dict[str, Any]:
        """Read the current material on a static mesh component's slot (v0.8.0 thin shim).

        Convenience wrapper over `get_object_property` — uses the
        OverrideMaterials.{slot} path under the actor's StaticMeshComponent.
        Returns the asset path of the assigned material, or empty when the
        slot inherits the mesh's default.

        Args:
            actor:      Actor display label or internal name.
            slot_index: Material slot index. Default 0.

        Returns:
            Same shape as get_object_property, with `value` as the asset
            path string for the slot.
        """
        return dispatch_unreal_command(
            "get_object_property",
            {
                "target": actor,
                "path": f"StaticMeshComponent.OverrideMaterials.{slot_index}",
            },
        )

    @unreal_tool(mcp)
    def get_object_property(ctx: Context, target: str, path: str) -> Dict[str, Any]:
        """Read a property on ANY UObject by target + dotted path (v0.8.0).

        The target string is resolved by FObjectLookup::Resolve — first match
        wins:

          1. /Game/-prefixed path        →  loaded as an asset
             "/Game/M_Stone.M_Stone"     →  UMaterial
          2. /Script/-prefixed path      →  engine type or CDO
             "/Script/Engine.StaticMeshActor"
          3. Actor display label or internal name in the current editor world
             "Altar_Lantern"

        After resolution, `path` is a dotted property traversal — same syntax
        as `get_actor_property`'s `property_name`:

          "Intensity"                                   plain leaf
          "PointLightComponent.Intensity"               component hop → leaf
          "Settings.AutoExposureBias"                   struct hop → leaf
          "Settings.ColorGrading.Highlights.Saturation" nested struct chain
          "OverrideMaterials.0"                         array index leaf

        Args:
            target: Object lookup string.
            path:   Dotted property path from the resolved root.

        Returns:
            {
              "success": true,
              "target": "...",
              "path": "...",
              "root_class": "<class of resolved root>",
              "value": <typed leaf value>
            }
        """

    @unreal_tool(mcp)
    def set_object_property(ctx: Context, target: str, path: str, value: Any) -> Dict[str, Any]:
        """Write a property on ANY UObject by target + dotted path (v0.8.0).

        Mirror of `get_object_property` for the write side. Broadcasts
        PostEditChangeProperty on the owning UObject after write, so the
        editor refreshes the Details panel + viewport (and renders update for
        components like lights and fog).

        Args:
            target: Object lookup string (see get_object_property for syntax).
            path:   Dotted property path.
            value:  JSON-typed leaf value. Supported leaf kinds:
                      number  → bool/int/float/double/byte/enum
                      string  → str/name/enum/object (asset path)
                      array   → struct (Vector/Rotator/Vector4/LinearColor/Color)

        Returns:
            {
              "success": true,
              "target": "...",
              "path": "...",
              "root_class": "...",
              "leaf_property": "...",
              "leaf_container": "...",
              "owning_object_class": "..."
            }
        """

    @mcp.tool()
    def get_world_settings(ctx: Context, path: Optional[str] = None) -> Dict[str, Any]:
        """Read the current level's WorldSettings actor (v0.8.0).

        Convenience wrapper over `get_object_property` with `target` pinned to
        "WorldSettings" — the AWorldSettings actor in the loaded editor level.

        Pass `path` to read a single property (e.g. `"DefaultGameMode"`).
        Without `path`, falls back to `get_actor_properties` for a full dump.

        Lauder explicitly lost a half-day to a per-level GameMode override
        hiding the project default — see project memory
        `feedback_unreal_level_gamemode_override_hides_default.md`. This is the
        programmatic equivalent of opening Window → World Settings.

        Args:
            path: Optional property path. If omitted, dumps every UPROPERTY.

        Returns:
            With path:    {success, target, path, root_class, value}
            Without path: {success, name, properties: {...}}
        """
        if path:
            return dispatch_unreal_command(
                "get_object_property",
                {"target": "WorldSettings", "path": path},
            )
        return dispatch_unreal_command(
            "get_actor_properties",
            {"name": "WorldSettings"},
        )

    @mcp.tool()
    def set_world_settings(ctx: Context, path: str, value: Any) -> Dict[str, Any]:
        """Write a property on the current level's WorldSettings actor (v0.8.0).

        Convenience wrapper over `set_object_property` with `target` pinned to
        "WorldSettings". Use to fix per-level GameMode overrides, default
        game framework class swaps, kill-Z thresholds, etc., without opening
        Window → World Settings.

        Args:
            path:  Dotted property path on AWorldSettings
                   (e.g. "DefaultGameMode", "KillZ").
            value: JSON-typed leaf value.

        Returns:
            {success, target, path, root_class, leaf_property, ...}
        """
        return dispatch_unreal_command(
            "set_object_property",
            {"target": "WorldSettings", "path": path, "value": value},
        )

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

        NOT supported during Play-In-Editor — the editor viewport client
        is replaced by a game viewport client during PIE, and the editor-
        screenshot path can't capture from it. Use `pie_screenshot` for
        in-game capture while PIE is active. Calling this during PIE
        returns a clear error rather than crashing the editor (v0.8.2).

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
    def frame_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Frame a single actor in the perspective viewports (v0.8.0).

        Equivalent of selecting the actor and pressing F. Faster than the
        two-call (set_selected_actors → focus_selected_actors) form when you
        only need to look at one thing. Two-pass label/internal-name lookup.

        Args:
            name: Actor display label or internal name.

        Returns:
            {"success": true, "framed": "<display label>"}
        """

    @unreal_tool(mcp)
    def set_show_flag(ctx: Context, flag: str, enabled: bool) -> Dict[str, Any]:
        """Toggle a viewport show flag (v0.8.0).

        Show flags control what's drawn in the editor viewport: Lighting,
        BillboardSprites (the placeholder icons for lights/cameras/triggers),
        Bounds, Grid, PostProcessing, etc. Use to clean up screenshots —
        e.g. set_show_flag("BillboardSprites", False) to hide the yellow
        icons before a viewport capture.

        Args:
            flag: Show flag CODE name (not the editor display label).
                  Many flags differ between the two — e.g. the UI shows
                  "Sprites" but the code symbol is "BillboardSprites".
                  Common code names: BillboardSprites, Bounds, Grid,
                  Lighting, PostProcessing, Game, Atmosphere, Fog,
                  AmbientCubemap, AntiAliasing, MotionBlur, DepthOfField,
                  Particles, Wireframe, BSP, StaticMeshes, SkeletalMeshes.
            enabled: Target state.

        Returns:
            {"success": true, "flag": "...", "enabled": bool}
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
    def wait_for_async_compile(ctx: Context, timeout_seconds: float = 60.0) -> Dict[str, Any]:
        """Block until shader + asset compiles finish (v0.8.0).

        Drains FAssetCompilingManager + GShaderCompilingManager from the
        editor side, ticking them in a 100 ms loop until empty or timeout.
        Use BEFORE `finalize_migration` so the post-migrate fixup doesn't
        race the mesh compile (the long-standing issue tracked in project
        memory as task #40).

        Args:
            timeout_seconds: Max wait. Default 60. Pass larger for slow
                first imports of high-poly Megascans assets.

        Returns:
            On success:  {"success": true, "elapsed_seconds": N, "iterations": K}
            On timeout: {"success": false, "timed_out": true,
                         "remaining_jobs": M, "elapsed_seconds": N, "error": "..."}
        """

    @unreal_tool(mcp)
    def dismiss_modal_dialog(ctx: Context) -> Dict[str, Any]:
        """Close transient editor popups + modal windows (v0.8.0).

        Best-effort: calls FSlateApplication::DismissAllMenus + iterates the
        top-level window list and destroys anything `IsModalWindow()`.
        Useful after migrate_assets / finalize_migration when an "Importing..."
        progress window lingers and blocks further input.

        Returns:
            {"success": true, "dismissed_count": N}
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
    ) -> Union["Image", Dict[str, Any]]:  # type: ignore[name-defined]
        """Capture a screenshot from the PIE game viewport (v0.7.11, unwrap-fixed v0.8.0).

        Different from take_screenshot — that one captures the editor's
        active viewport which when PIE is running may show editor gizmos.
        This captures the player's actual in-game view, no overlay.

        Args:
            filename: Output filename. Bare names land under
                      <Project>/Saved/Screenshots/. Default "pie_screenshot.png".

        Returns:
            On success: a FastMCP Image content block containing the PNG.
            On failure: {"success": False, "error": "..."}.
        """
        # NB: not a `@unreal_tool` — needs to read the PNG off disk and wrap
        # into the FastMCP Image content type before returning. Mirrors the
        # take_screenshot wrapper exactly except for the field name the C++
        # side returns: `path` (here) vs. `filepath` (take_screenshot — wire
        # name predates the rename, kept for compat).
        response = dispatch_unreal_command("pie_screenshot", {"filename": filename})
        if not response.get("success", False):
            return response

        abs_path = response.get("path")
        if not abs_path:
            return {
                "success": False,
                "error": "Response missing path",
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
                "path": str(png_path),
                "size_bytes": png_path.stat().st_size,
                "width": response.get("width"),
                "height": response.get("height"),
            }

        return Image(path=str(png_path))

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

    @unreal_tool(mcp)
    def set_selected_actors(ctx: Context, names: List[str]) -> Dict[str, Any]:
        """Replace the editor's actor selection with the given names (v0.8.0).

        Round-trips with `get_selected_actors`: each entry in `names` is matched
        first against display labels (the Outliner-visible name), then against
        internal UObject names. Both shapes returned by `get_selected_actors`
        are accepted, so callers can pass back either field.

        Existing selection is cleared first — pass an empty list to deselect
        everything (equivalent to `clear_selection`).

        `selected_count` is post-state authoritative: UE silently refuses to
        multi-select certain special actors (AWorldSettings being the canonical
        example). Such actors are reported in `rejected` — they were resolved
        by name but did NOT end up in the actual selection. `missing` is the
        separate case of names that didn't resolve to any actor in the level.

        Args:
            names: Actor display labels or internal names to select.

        Returns:
            {
              "success": true,
              "selected_count": N,        # actually in selection after the call
              "requested_count": M,       # length of `names`
              "missing":  ["...", ...],   # names that matched no actor
              "rejected": ["...", ...]    # actors UE refused to select
            }
        """

    @unreal_tool(mcp)
    def clear_selection(ctx: Context) -> Dict[str, Any]:
        """Deselect everything in the editor (v0.8.0).

        Equivalent to `set_selected_actors(names=[])` but more explicit.

        Returns:
            {"success": true}
        """

    @unreal_tool(mcp)
    def focus_selected_actors(ctx: Context) -> Dict[str, Any]:
        """Frame the current editor selection in the viewport (v0.8.0).

        Equivalent of pressing 'F' in the editor — moves perspective viewport
        cameras so the selected actors fill the frame. Composes with
        `set_selected_actors` for "select + frame" in two calls.

        Errors if nothing is selected; call `set_selected_actors` first.

        Returns:
            {"success": true, "framed_count": N}
        """

    logger.info("Editor tools registered successfully")
