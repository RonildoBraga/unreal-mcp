"""Project Tools for Unreal MCP — input mappings, INI editing, Python escape hatch.

Project-wide settings & configuration + the unsafe-by-default Python execution
escape hatch (v0.8.1).

Tool surface (4 tools):

    create_input_mapping    add a legacy input action or axis mapping
    get_ini                 read a key (or dump a section) from a project INI (v0.8.0)
    set_ini                 write a key into a project INI + persist to disk (v0.8.0)
    execute_python          arbitrary Python source via IPythonScriptPlugin (v0.8.1,
                            unsafe-by-default — pass unsafe=True to actually run)

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp`.
"""

import logging
from typing import Any, Dict, Optional

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool, dispatch_unreal_command

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

    @mcp.tool()
    def execute_python(ctx: Context, code: str, unsafe: bool = False) -> Dict[str, Any]:
        """ESCAPE HATCH — execute arbitrary Python inside UE (v0.8.1).

        Runs the given source through `IPythonScriptPlugin::ExecPythonCommandEx`
        in the editor's embedded Python interpreter. Use ONLY when no typed
        wrapper exists for what you need — the 100+ typed tools (find_actors,
        set_object_property, the batches, etc.) are preferred for autonomous
        workflows. Those have schemas, validation, and predictable response
        shapes. This one doesn't.

        Safety model:

          - Defaults to refusing the call. Pass `unsafe=True` explicitly per
            call to actually execute. One opt-in per call (no global flag).
          - No schema or type checking on the body. Errors come back as Python
            tracebacks in `stderr`.
          - Runs on the UE editor's game thread. Long-running code freezes
            the editor. Don't `while True:` in here.
          - Wire response shape is `{success, error?, stdout, stderr}` — does
            NOT match the strict `{success, error?, ...payload}` contract of
            the typed tools. This is the documented escape-hatch shape.

        Args:
            code:   Python source. Multi-line OK. `print()` output is captured
                    into `stdout`; warnings + tracebacks into `stderr`.
            unsafe: Required True to actually run. Default False refuses with
                    `{"success": false, "error": "execute_python requires
                    unsafe=true ..."}`.

        Returns:
            On `unsafe=False`:
                {"success": false, "error": "execute_python requires unsafe=true ...",
                 "stdout": "", "stderr": ""}
            On `unsafe=True` success:
                {"success": true, "stdout": "<captured stdout>", "stderr": "<warnings>"}
            On `unsafe=True` failure (Python exception, plugin not loaded, ...):
                {"success": false, "error": "Python execution failed ...",
                 "stdout": "...", "stderr": "<traceback>"}

        stdout + stderr are each truncated at 32 KB.

        Example:
            execute_python(
                code="import unreal\\n"
                     "print(unreal.SystemLibrary.get_engine_version())",
                unsafe=True,
            )
        """
        return dispatch_unreal_command(
            "execute_python",
            {"code": code, "unsafe": unsafe},
        )

    logger.info("Project tools registered successfully")
