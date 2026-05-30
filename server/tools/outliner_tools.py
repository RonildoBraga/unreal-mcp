"""
Outliner Tools for Unreal MCP — Sprint 2 v0.7.0.

Actor folder organization in the World Outliner panel.

UE's Outliner groups actors by virtual folder paths — folders are FName
labels on each AActor, not assets. The set of folders shown is the union
of every actor's folder label plus any pending empty folders registered
via FActorFolders.

Tool surface (4 tools):

    get_outliner_folders      list every folder in the current world
    move_actor_to_folder       set an actor's folder label
    create_outliner_folder     register a pending (potentially empty) folder
    get_actors_in_folder       list actors at a given folder path

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPOutlinerCommands.cpp`.
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


def register_outliner_tools(mcp: FastMCP):
    """Register Outliner tools with the MCP server."""

    @mcp.tool()
    def get_outliner_folders(ctx: Context) -> Dict[str, Any]:
        """List every folder path in the current world's Outliner.

        Includes folders that contain actors AND pending empty folders
        registered via FActorFolders (so newly-created-but-empty folders
        appear before any actors are moved in).

        Returns:
            {"folders": ["Lighting", "Lighting/Candles", ...], "count": N}

        Folder paths use forward slashes as hierarchy separators. An actor
        in `Lighting/Candles` appears nested under `Lighting` in the
        Outliner panel.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return _unwrap(unreal.send_command("get_outliner_folders", {}))
        except Exception as e:
            logger.error(f"get_outliner_folders error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def move_actor_to_folder(
        ctx: Context,
        actor_name: str,
        folder_path: str,
    ) -> Dict[str, Any]:
        """Set an actor's Outliner folder label.

        Args:
            actor_name:  Actor's display label (the name shown in the Outliner,
                         not the internal UObject name). Case-insensitive match.
            folder_path: Slash-separated folder path (e.g. "Lighting/Candles").
                         Pass empty string to move actor to the Outliner root.
                         Folder is auto-created if it doesn't exist.

        Returns:
            {"actor_name": "...", "folder_path": "...", "success": bool}
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return _unwrap(unreal.send_command("move_actor_to_folder", {
                "actor_name": actor_name,
                "folder_path": folder_path,
            }))
        except Exception as e:
            logger.error(f"move_actor_to_folder error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def create_outliner_folder(ctx: Context, folder_path: str) -> Dict[str, Any]:
        """Register a pending (potentially empty) folder in the Outliner.

        UE folders are virtual — they exist as labels on actors. To create
        a folder *before* moving actors into it (useful for set-up
        organization), register it via FActorFolders. The folder appears
        immediately in the Outliner; if you don't add actors before reloading
        the level, the folder may not persist.

        Args:
            folder_path: Slash-separated path (e.g. "Migrated/GoddessTemple").

        Returns:
            {"folder_path": "...", "success": bool, "note"?: "..."}
            note appears on success=False with a hint.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return _unwrap(unreal.send_command("create_outliner_folder", {"folder_path": folder_path}))
        except Exception as e:
            logger.error(f"create_outliner_folder error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def get_actors_in_folder(ctx: Context, folder_path: str) -> Dict[str, Any]:
        """List actors at a specific Outliner folder path.

        Args:
            folder_path: Slash-separated path. Empty string targets actors at
                         Outliner root (no folder).

        Returns:
            {
              "folder_path": "...",
              "actors": [
                {"name": "<display label>", "class_name": "...", "internal_name": "..."},
                ...
              ],
              "count": N
            }

        Only actors with *exactly* this folder path are returned — children
        of subfolders are not included. To get a folder's full subtree, call
        get_outliner_folders first and iterate.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return _unwrap(unreal.send_command("get_actors_in_folder", {"folder_path": folder_path}))
        except Exception as e:
            logger.error(f"get_actors_in_folder error: {e}")
            return {"error": str(e)}
