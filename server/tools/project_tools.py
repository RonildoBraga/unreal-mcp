"""Project Tools for Unreal MCP — input mappings + INI editing.

Project-wide settings & configuration.

Tool surface (3 tools):

    create_input_mapping    add a legacy input action or axis mapping
    get_ini                 read a key (or dump a section) from a project INI (v0.8.0)
    set_ini                 write a key into a project INI + persist to disk (v0.8.0)

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp`.
"""

import logging
from typing import Any, Dict, Optional

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

    @unreal_tool(mcp)
    def get_ini(
        ctx: Context,
        file: str,
        section: str,
        key: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Read a key or section from a project INI file (v0.8.0).

        Goes through GConfig (not raw file read) so the result reflects the
        live merged config — Engine + Project. Without `key`, dumps every
        key/value pair in the section.

        Args:
            file:    INI leaf name (e.g. "DefaultEngine.ini",
                     "DefaultGame.ini", "DefaultInput.ini"). Resolved under
                     <Project>/Config/.
            section: Section header without brackets (e.g.
                     "/Script/Engine.Engine" or "/Script/EngineSettings.GameMapsSettings").
            key:     Optional key. Omit to dump the whole section.

        Returns:
            With key:    {"success": true, "file": "...", "section": "...",
                          "key": "...", "value": "...", "present": bool}
            Without key: {"success": true, "file": "...", "section": "...",
                          "pairs": {key: value, ...}, "pair_count": N}
        """

    @unreal_tool(mcp)
    def set_ini(
        ctx: Context,
        file: str,
        section: str,
        key: str,
        value: str,
    ) -> Dict[str, Any]:
        """Write a key into a project INI file + persist to disk (v0.8.0).

        Goes through GConfig.SetString + Flush — the change is live in the
        current editor AND persisted to disk. The most common use case for
        Lauder: clearing a per-level GameMode override or pinning a default
        framework class without opening Project Settings UI.

        Args:
            file:    INI leaf name (e.g. "DefaultEngine.ini").
            section: Section header (e.g. "/Script/Engine.Engine").
            key:     Key name.
            value:   String value to write. Numbers + class paths all go in
                     as strings — UE parses the INI side.

        Returns:
            {"success": true, "file": "...", "section": "...", "key": "...",
             "value": "...", "prior_value": "...", "created": bool}
        """

    logger.info("Project tools registered successfully")
