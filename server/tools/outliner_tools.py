"""Outliner Tools for Unreal MCP.

Actor folder organization in the World Outliner panel.

UE's Outliner groups actors by virtual folder paths — folders are FName
labels on each AActor, not assets. The set of folders shown is the union
of every actor's folder label plus any pending empty folders registered
via FActorFolders.

Tool surface (4 tools):

    get_outliner_folders      list every folder in the current world
    move_actor_to_folder      set an actor's folder label
    create_outliner_folder    register a pending (potentially empty) folder
    get_actors_in_folder      list actors at a given folder path

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPOutlinerCommands.cpp`.
"""

import logging
from typing import Any, Dict

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool

logger = logging.getLogger("UnrealMCP")


def register_outliner_tools(mcp: FastMCP):
    """Register Outliner tools with the MCP server."""

    @unreal_tool(mcp)
    def get_outliner_folders(ctx: Context) -> Dict[str, Any]:
        """List every folder path in the current world's Outliner.

        Includes folders that contain actors AND pending empty folders
        registered via FActorFolders (so newly-created-but-empty folders
        appear before any actors are moved in).

        Returns:
            {"success": true, "folders": ["Lighting", "Lighting/Candles", ...], "count": N}

        Folder paths use forward slashes as hierarchy separators. An actor
        in `Lighting/Candles` appears nested under `Lighting` in the
        Outliner panel.
        """

    @unreal_tool(mcp)
    def move_actor_to_folder(
        ctx: Context,
        name: str,
        folder_path: str,
    ) -> Dict[str, Any]:
        """Set an actor's Outliner folder label.

        Args:
            name:        Actor's display label (the name shown in the Outliner,
                         not the internal UObject name). Case-insensitive match.
                         Matches the `name` convention used by spawn_actor /
                         get_actor_properties / set_actor_property.
            folder_path: Slash-separated folder path (e.g. "Lighting/Candles").
                         Pass empty string to move actor to the Outliner root.
                         Folder is auto-created if it doesn't exist.

        Returns:
            {"success": bool, "name": "...", "folder_path": "..."}
        """

    @unreal_tool(mcp)
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
            {"success": bool, "folder_path": "...", "note"?: "..."}
        """

    @unreal_tool(mcp)
    def get_actors_in_folder(ctx: Context, folder_path: str) -> Dict[str, Any]:
        """List actors at a specific Outliner folder path.

        Args:
            folder_path: Slash-separated path. Empty string targets actors at
                         Outliner root (no folder).

        Returns:
            {
              "success": true,
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

    logger.info("Outliner tools registered successfully")
