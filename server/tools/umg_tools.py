"""UMG Tools for Unreal MCP — Widget Blueprint creation + composition.

Tool surface (6 Python-exposed tools):

    create_umg_widget_blueprint    new UserWidget-derived asset
    add_text_block_to_widget       UTextBlock + canvas slot
    add_button_to_widget           UButton with child UTextBlock
    bind_widget_event              event → function binding (auto-creates fn)
    add_widget_to_viewport         spawn instance to player viewport
    set_text_block_binding         property binding on a UTextBlock

The C++ side also registers 8 additional commands (add_widget_to_tree,
set_widget_text, set_progress_bar_percent, set_progress_bar_fill_color,
set_horizontal_box_slot_fill, set_canvas_slot_anchor, delete_widget_from_tree,
compile_widget_blueprint) that came from Phase 5.2 HUD work. They're
reachable via direct send_command but lack Python wrappers — slated to be
filled in alongside the v0.8.0 file-layout move (Day 2d).

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPUMGCommands.cpp`.
"""

import logging
from typing import Any, Dict, List

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool

logger = logging.getLogger("UnrealMCP")


def register_umg_tools(mcp: FastMCP):
    """Register UMG tools with the MCP server."""

    @unreal_tool(mcp)
    def create_umg_widget_blueprint(
        ctx: Context,
        widget_name: str,
        parent_class: str = "UserWidget",
        path: str = "/Game/UI",
    ) -> Dict[str, Any]:
        """Create a new UMG Widget Blueprint.

        Args:
            widget_name:  Name for the new widget blueprint.
            parent_class: Parent class (default "UserWidget").
            path:         Content browser path (default "/Game/UI").

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def add_text_block_to_widget(
        ctx: Context,
        widget_name: str,
        text_block_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 50.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0],
    ) -> Dict[str, Any]:
        """Add a UTextBlock widget to a UMG Widget Blueprint.

        Args:
            widget_name:     Name of the target Widget Blueprint.
            text_block_name: Name for the new text block.
            text:            Initial text.
            position:        [X, Y] position on the canvas.
            size:            [Width, Height].
            font_size:       Font size in points.
            color:           [R, G, B, A] in 0–1 range.

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def add_button_to_widget(
        ctx: Context,
        widget_name: str,
        button_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 50.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0],
        background_color: List[float] = [0.1, 0.1, 0.1, 1.0],
    ) -> Dict[str, Any]:
        """Add a UButton widget (with a child UTextBlock) to a UMG Widget Blueprint.

        Args:
            widget_name:      Name of the target Widget Blueprint.
            button_name:      Name for the new button.
            text:             Text displayed on the button.
            position:         [X, Y] position on the canvas.
            size:             [Width, Height].
            font_size:        Font size for the button's text.
            color:            [R, G, B, A] text color, 0–1 range.
            background_color: [R, G, B, A] button background color, 0–1 range.

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def bind_widget_event(
        ctx: Context,
        widget_name: str,
        widget_component_name: str,
        event_name: str,
        function_name: str = "",
    ) -> Dict[str, Any]:
        """Bind an event on a widget component to a function.

        Args:
            widget_name:           Target Widget Blueprint.
            widget_component_name: Component name (button, etc.).
            event_name:            Event name ("OnClicked", ...).
            function_name:         Function to create/bind. Empty string ->
                                   ``f"{widget_component_name}_{event_name}"``
                                   (the C++ handler applies this fallback).

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def add_widget_to_viewport(
        ctx: Context,
        widget_name: str,
        z_order: int = 0,
    ) -> Dict[str, Any]:
        """Add a Widget Blueprint instance to the player viewport.

        Args:
            widget_name: Widget Blueprint to instantiate.
            z_order:     Layer ordering (higher = on top).

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def set_text_block_binding(
        ctx: Context,
        widget_name: str,
        text_block_name: str,
        binding_property: str,
        binding_type: str = "Text",
    ) -> Dict[str, Any]:
        """Set up a property binding for a Text Block widget.

        Args:
            widget_name:      Target Widget Blueprint.
            text_block_name:  Name of the Text Block.
            binding_property: Property to bind to.
            binding_type:     Type of binding ("Text", "Visibility", ...).

        Returns:
            {"success": bool, ...}
        """

    logger.info("UMG tools registered successfully")
