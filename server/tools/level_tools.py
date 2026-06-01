"""Level Management Tools for Unreal MCP.

Basic level lifecycle: know what's loaded, switch levels, save changes.
World Partition queries and streaming-sublevel management come later.

Tool surface (4 tools):

    get_current_level     name + path of the loaded editor world
    open_level            load a level by /Game/ path
    save_current_level    save the currently loaded level
    create_landscape      spawn a Landscape Actor (C++ — Python can't)

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

    logger.info("Level tools registered successfully")
