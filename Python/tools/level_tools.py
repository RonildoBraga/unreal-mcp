"""
Level Management Tools for Unreal MCP — Sprint 1 partial.

Basic level lifecycle: know what's loaded, switch levels, save changes.
Save-with-built-data, level creation, World Partition queries, and
streaming-sublevel management come in Sprint 3.

Tool surface (4 tools):

    get_current_level     name + path of the loaded editor world
    open_level            load a level by /Game/ path
    save_current_level    save the currently loaded level
    save_all_dirty        batch-save every dirty level + content package

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin and returns the response. C++ side in
`Plugin/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPLevelCommands.cpp`.
"""

import logging
from typing import Any, Dict

from mcp.server.fastmcp import Context, FastMCP

logger = logging.getLogger("UnrealMCP")


def _unwrap(response):
    if not response:
        return {"error": "no response from Unreal"}
    if "error" in response:
        return {"error": response["error"]}
    if "result" in response:
        return response["result"]
    return response


def register_level_tools(mcp: FastMCP):
    """Register Level Management tools with the MCP server."""

    @mcp.tool()
    def get_current_level(ctx: Context) -> Dict[str, Any]:
        """Get the currently-loaded editor level.

        Returns:
            {
              "name": "L_Base",
              "package_name": "/Game/Lauder/Levels/L_Base",
              "object_path": "/Game/Lauder/Levels/L_Base.L_Base",
              "map_name": "UEDPIE_0_L_Base"   (or non-PIE variant)
            }
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return _unwrap(unreal.send_command("get_current_level", {}))
        except Exception as e:
            logger.error(f"get_current_level error: {e}")
            return {"error": str(e)}

    @mcp.tool()
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
            {"level_path": "...", "success": bool, "note"?: "..."}
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return _unwrap(unreal.send_command("open_level", {"level_path": level_path}))
        except Exception as e:
            logger.error(f"open_level error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def save_current_level(ctx: Context) -> Dict[str, Any]:
        """Save the currently-loaded level.

        Returns: {"success": bool}
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return _unwrap(unreal.send_command("save_current_level", {}))
        except Exception as e:
            logger.error(f"save_current_level error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def save_all_dirty(ctx: Context) -> Dict[str, Any]:
        """Save every dirty level and content package in the project.

        Equivalent to UE's "Save All" button. UE handles save conflicts in
        its own UI (e.g. if source control is in the way); this tool only
        confirms the call was dispatched, not the outcome of every save.

        Returns: {"success": True, "note": "..."}
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return _unwrap(unreal.send_command("save_all_dirty", {}))
        except Exception as e:
            logger.error(f"save_all_dirty error: {e}")
            return {"error": str(e)}
