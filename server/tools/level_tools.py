"""Level Management Tools for Unreal MCP.

Basic level lifecycle: know what's loaded, switch levels, save changes.
World Partition queries and streaming-sublevel management come later.

Tool surface (7 tools):

    get_current_level         name + path of the loaded editor world
    open_level                load a level by /Game/ path
    save_current_level        save the currently loaded level
    create_landscape          spawn a Landscape Actor (C++ — Python can't)
    sculpt_landscape_noise    additively layer fractal Perlin noise
    sculpt_landscape_hill     gaussian peak/depression at a world XY
    flatten_landscape         set every vertex to a uniform height

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin and returns the response. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPLevelCommands.cpp`.
"""

import logging
from typing import Any, Dict, Optional

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool

logger = logging.getLogger("UnrealMCP")


def register_level_tools(mcp: FastMCP):
    """Register Level Management tools with the MCP server."""

    @unreal_tool(mcp)
    def get_current_level(ctx: Context) -> Dict[str, Any]:
        """Get the currently-loaded editor level.

        Returns:
            {
              "success": true,
              "name": "L_Base",
              "package_name": "/Game/Lauder/Levels/L_Base",
              "object_path": "/Game/Lauder/Levels/L_Base.L_Base",
              "map_name": "UEDPIE_0_L_Base"   (or non-PIE variant)
            }
        """

    @unreal_tool(mcp)
    def open_level(ctx: Context, level_path: str) -> Dict[str, Any]:
        """Load a level into the editor.

        Args:
            level_path: /Game/-prefixed package path, e.g. "/Game/Lauder/Levels/L_Base".
                        Object-path form ("/Game/.../L_Base.L_Base") is also accepted
                        — the trailing object suffix is stripped automatically.

        If the current level has unsaved changes, UE may show a save-prompt
        dialog before loading the new level. The tool returns success=False
        with a note if loading fails.

        Returns:
            {"success": bool, "level_path": "...", "note"?: "..."}
        """

    @unreal_tool(mcp)
    def save_current_level(ctx: Context) -> Dict[str, Any]:
        """Save the currently-loaded level.

        Returns: {"success": bool}
        """

    @unreal_tool(mcp)
    def create_landscape(
        ctx: Context,
        label: str = "Landscape",
        location: Optional[Dict[str, float]] = None,
        scale: Optional[Dict[str, float]] = None,
        component_count_x: int = 4,
        component_count_y: int = 4,
        sections_per_component: int = 2,
        quads_per_section: int = 63,
    ) -> Dict[str, Any]:
        """Spawn an ALandscape actor with proper components and a flat heightmap.

        Why this exists: UE 5.7's Python bindings explicitly walls off Landscape
        construction — spawn_actor_from_class(unreal.Landscape) returns a
        LandscapePlaceholder stub. ALandscape::Import is C++ only. This tool
        wraps that C++ API.

        Args:
            label: Outliner label for the spawned actor (default "Landscape").
            location: World-space spawn location {x, y, z}. Default (0, 0, 0).
            scale:    World-space scale {x, y, z}. Default (100, 100, 100) —
                      i.e. 1 quad = 1 m in world units. Z-scale is the height
                      multiplier on the uint16 heightmap.
            component_count_x: number of components along X (default 4).
            component_count_y: number of components along Y (default 4).
            sections_per_component: 1 or 2 (UE constraint). Default 2.
            quads_per_section: one of 7, 15, 31, 63, 127, 255 (UE constraint).
                               Default 63 — Epic's recommended sweet spot.

        Total quads per side = component_count * sections_per_component * quads_per_section.
        With all defaults: 4 * 2 * 63 = 504 quads per side → 505 × 505 verts.
        At scale 100, that's ~504 m square — a single-zone sanctuary footprint.

        The landscape is created flat at z = 0 (uint16 midpoint 32768) and with
        no material layers; wire a material with set_landscape_material once
        the actor exists.

        Returns:
            {
              "success": true,
              "name": "...", "label": "...", "location": {...},
              "component_count_x": 4, "component_count_y": 4,
              "sections_per_component": 2, "quads_per_section": 63,
              "total_verts_x": 505, "total_verts_y": 505,
              "total_components": 16
            }
        """

    @unreal_tool(mcp)
    def sculpt_landscape_noise(
        ctx: Context,
        label: str,
        scale_meters: float = 80.0,
        height_meters: float = 8.0,
        octaves: int = 4,
        seed: int = 42,
    ) -> Dict[str, Any]:
        """Additively layer fractal Perlin noise onto an existing Landscape.

        Compose with sculpt_landscape_hill: noise first for rolling terrain,
        then hill calls for specific landmarks.

        Args:
            label:        Outliner label of the Landscape actor.
            scale_meters: wavelength of the base octave in world meters. Bigger
                          = wider, gentler features. 80 m gives valley/ridge
                          patterns you can walk through; 20 m gives bumpy
                          ground; 200 m gives subtle continental shape.
            height_meters: max ± height variation. 8 m is a noticeable but
                          not dramatic roll. Iceland-rugged tends 10-25 m.
            octaves:      1-6. Each octave halves amplitude, doubles freq.
                          4 octaves gives a good rugged-but-not-noisy look.
            seed:         offsets the noise position; change for variation
                          without retuning scale/height.

        Returns:
            {"success": true, "label": "...", "verts_modified": 255025,
             "scale_meters": 80, "height_meters": 8, "octaves": 4, "seed": 42}
        """

    @unreal_tool(mcp)
    def sculpt_landscape_hill(
        ctx: Context,
        label: str,
        center: Optional[Dict[str, float]] = None,
        radius_meters: float = 30.0,
        peak_meters: float = 10.0,
    ) -> Dict[str, Any]:
        """Add a gaussian peak (positive) or depression (negative) on a Landscape.

        Smooth falloff: at distance == radius, contribution is ~37% of peak;
        at 2× radius, ~1.8%. Influence capped at 3× radius (negligible beyond).

        Args:
            label:         Outliner label of the Landscape actor.
            center:        {x, y, z?} in world cm. Z is ignored — we project
                           onto the landscape's XY plane. Default (0, 0, 0).
            radius_meters: gaussian half-width in meters (default 30).
            peak_meters:   height at center, in meters. Positive = hill,
                           negative = pit/crater (default 10).

        Returns:
            {"success": true, "label": "...", "verts_modified": N,
             "center": {"x": ..., "y": ...}, "radius_meters": 30,
             "peak_meters": 10}
        """

    @unreal_tool(mcp)
    def flatten_landscape(
        ctx: Context,
        label: str,
        z_meters: float = 0.0,
    ) -> Dict[str, Any]:
        """Reset every vertex of a Landscape to a uniform world Z.

        Useful as an "undo" before re-running a procedural pass.

        Args:
            label:    Outliner label of the Landscape actor.
            z_meters: target world Z (default 0, the create_landscape baseline).

        Returns:
            {"success": true, "label": "...", "verts_modified": N, "z_meters": 0}
        """

    logger.info("Level tools registered successfully")
