"""Project Tools for Unreal MCP — input mappings.

Project-wide settings & configuration. v0.8.0 surface is intentionally
minimal — INI editing, plugins, build, source control all slot into
post-v0.8.0 project.* modules (see architecture-v0.8-plan.md §4.1).

Tool surface (1 tool):

    create_input_mapping    add a legacy input action or axis mapping

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp`.
"""

import logging
from typing import Any, Dict

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool

logger = logging.getLogger("UnrealMCP")


def register_project_tools(mcp: FastMCP):
    """Register Project tools with the MCP server."""

    @unreal_tool(mcp)
    def create_input_mapping(
        ctx: Context,
        action_name: str,
        key: str,
        input_type: str = "Action",
    ) -> Dict[str, Any]:
        """Create a legacy input mapping (action or axis) in DefaultInput.ini.

        For Enhanced Input (the modern path used by ALauderCharacter), edit
        the IMC asset directly — this tool only writes legacy mappings.

        Args:
            action_name: Name of the input action (e.g. "Jump", "Fire").
            key:         Key to bind ("SpaceBar", "LeftMouseButton", ...).
            input_type:  "Action" or "Axis". Default "Action".

        Returns:
            {"success": bool, "action_name": ..., "key": ..., "input_type": ...}
        """

    logger.info("Project tools registered successfully")
