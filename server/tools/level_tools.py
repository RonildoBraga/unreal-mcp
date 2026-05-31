"""Level Management Tools for Unreal MCP.

Basic level lifecycle: know what's loaded, switch levels, save changes.
Level creation, World Partition queries, and streaming-sublevel management
come post-v0.8.0.

Tool surface (4 tools):

    get_current_level     name + path of the loaded editor world
    open_level            load a level by /Game/ path
    save_current_level    save the currently loaded level
    save_all_dirty        batch-save every dirty level + content package
                          (slated for deletion in v0.8.0 Day 5 cleanup —
                          never called by any current workflow)

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin and returns the response. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPLevelCommands.cpp`.
"""

import logging
from typing import Any, Dict

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
    def save_all_dirty(ctx: Context) -> Dict[str, Any]:
        """Save every dirty level and content package in the project.

        Equivalent to UE's "Save All" button. UE handles save conflicts in
        its own UI (e.g. if source control is in the way); this tool only
        confirms the call was dispatched, not the outcome of every save.

        Returns: {"success": bool, "note": "..."}
        """

    logger.info("Level tools registered successfully")
