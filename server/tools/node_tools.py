"""Blueprint Node Tools for Unreal MCP.

Create + connect + query event-graph nodes inside a Blueprint.

Tool surface (8 tools):

    add_blueprint_event_node                event entry node (BeginPlay, Tick, …)
    add_blueprint_input_action_node         legacy InputAction event node
    add_blueprint_function_node             function call node
    connect_blueprint_nodes                 wire two nodes' pins together
    add_blueprint_variable                  add a typed variable to the BP
    add_blueprint_get_self_component_reference  drag-from-Components-panel node
    add_blueprint_self_reference            Get Self node
    find_blueprint_nodes                    query nodes by type / event

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintNodeCommands.cpp`.
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool

logger = logging.getLogger("UnrealMCP")


def register_blueprint_node_tools(mcp: FastMCP):
    """Register Blueprint Node tools with the MCP server."""

    @unreal_tool(mcp)
    def add_blueprint_event_node(
        ctx: Context,
        blueprint_name: str,
        event_name: str,
        node_position: List[float] = [0.0, 0.0],
    ) -> Dict[str, Any]:
        """Add an event node to a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint.
            event_name:     Event name. Use the 'Receive' prefix for standard
                            events ('ReceiveBeginPlay', 'ReceiveTick', etc.).
            node_position:  [X, Y] position in the graph.

        Returns:
            {"success": bool, "node_id": "...", ...}
        """

    @unreal_tool(mcp)
    def add_blueprint_input_action_node(
        ctx: Context,
        blueprint_name: str,
        action_name: str,
        node_position: List[float] = [0.0, 0.0],
    ) -> Dict[str, Any]:
        """Add a legacy InputAction event node to a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint.
            action_name:    Input action to respond to.
            node_position:  [X, Y] position in the graph.

        Returns:
            {"success": bool, "node_id": "...", ...}
        """

    @unreal_tool(mcp)
    def add_blueprint_function_node(
        ctx: Context,
        blueprint_name: str,
        target: str,
        function_name: str,
        params: Dict[str, Any] = {},
        node_position: List[float] = [0.0, 0.0],
    ) -> Dict[str, Any]:
        """Add a function call node to a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint.
            target:         Target object for the function (component name
                            or "self").
            function_name:  Name of the function to call.
            params:         Optional parameters to set on the function node.
            node_position:  [X, Y] position in the graph.

        Returns:
            {"success": bool, "node_id": "...", ...}
        """

    @unreal_tool(mcp)
    def connect_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        source_node_id: str,
        source_pin: str,
        target_node_id: str,
        target_pin: str,
    ) -> Dict[str, Any]:
        """Connect two nodes in a Blueprint's event graph.

        Args:
            blueprint_name:  Name of the target Blueprint.
            source_node_id:  ID of the source node.
            source_pin:      Name of the output pin on the source node.
            target_node_id:  ID of the target node.
            target_pin:      Name of the input pin on the target node.

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def add_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        variable_type: str,
        is_exposed: bool = False,
    ) -> Dict[str, Any]:
        """Add a variable to a Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint.
            variable_name:  Name of the variable.
            variable_type:  Type ("Boolean", "Integer", "Float", "Vector", etc.).
            is_exposed:     Whether to expose the variable to the editor.

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def add_blueprint_get_self_component_reference(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        node_position: List[float] = [0.0, 0.0],
    ) -> Dict[str, Any]:
        """Add a node that gets a reference to a component owned by this Blueprint.

        Equivalent to dragging a component from the Components panel into
        the event graph.

        Args:
            blueprint_name: Name of the target Blueprint.
            component_name: Name of the component to reference.
            node_position:  [X, Y] position in the graph.

        Returns:
            {"success": bool, "node_id": "...", ...}
        """

    @unreal_tool(mcp)
    def add_blueprint_self_reference(
        ctx: Context,
        blueprint_name: str,
        node_position: List[float] = [0.0, 0.0],
    ) -> Dict[str, Any]:
        """Add a 'Get Self' node to a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint.
            node_position:  [X, Y] position in the graph.

        Returns:
            {"success": bool, "node_id": "...", ...}
        """

    @unreal_tool(mcp)
    def find_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        node_type: Optional[str] = None,
        event_type: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Find nodes in a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint.
            node_type:      Optional node type filter ("Event", "Function",
                            "Variable", ...).
            event_type:     Optional specific event filter ("BeginPlay",
                            "Tick", ...).

        Returns:
            {"success": bool, "node_ids": [...], ...}
        """

    logger.info("Blueprint node tools registered successfully")
