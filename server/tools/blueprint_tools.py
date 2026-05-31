"""Blueprint Tools for Unreal MCP.

Create + modify Blueprint class assets, components, properties.

Tool surface (7 tools — set_pawn_properties is intentionally NOT registered;
set_component_property covers it generically):

    create_blueprint                new BP class derived from a parent
    add_component_to_blueprint      add a USceneComponent + transform
    set_static_mesh_properties      set the mesh on a StaticMeshComponent in a BP
    set_component_property          set a property on a BP component
    set_physics_properties          enable physics / set mass+damping
    compile_blueprint               recompile the BP after edits
    set_blueprint_property          set a property on the BP's CDO

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintCommands.cpp`.
"""

import logging
from typing import Any, Dict, List

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool

logger = logging.getLogger("UnrealMCP")


def register_blueprint_tools(mcp: FastMCP):
    """Register Blueprint tools with the MCP server."""

    @unreal_tool(mcp)
    def create_blueprint(
        ctx: Context,
        name: str,
        parent_class: str,
    ) -> Dict[str, Any]:
        """Create a new Blueprint class.

        Args:
            name:         Asset name for the new Blueprint.
            parent_class: Parent class (e.g. "Actor", "Pawn", "Character",
                          or any full class path).

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def add_component_to_blueprint(
        ctx: Context,
        blueprint_name: str,
        component_type: str,
        component_name: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0],
        scale: List[float] = [1.0, 1.0, 1.0],
        component_properties: Dict[str, Any] = {},
    ) -> Dict[str, Any]:
        """Add a component to a Blueprint.

        Args:
            blueprint_name:       Name of the target Blueprint.
            component_type:       Component class name without the U prefix
                                  (e.g. "StaticMeshComponent", "PointLightComponent").
            component_name:       Name for the new component instance.
            location:             [X, Y, Z] coordinates for the component's
                                  relative position.
            rotation:             [Pitch, Yaw, Roll] values for the component's
                                  relative rotation.
            scale:                [X, Y, Z] values for the component's scale.
            component_properties: Optional additional properties to set on
                                  the component at creation.

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def set_static_mesh_properties(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        static_mesh: str = "/Engine/BasicShapes/Cube.Cube",
    ) -> Dict[str, Any]:
        """Set static mesh properties on a StaticMeshComponent inside a Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint.
            component_name: Name of the StaticMeshComponent.
            static_mesh:    Path to the static mesh asset
                            (default: "/Engine/BasicShapes/Cube.Cube").

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def set_component_property(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """Set a property on a component in a Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint.
            component_name: Name of the component.
            property_name:  Name of the property.
            property_value: Value to assign (any JSON-compatible type).

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def set_physics_properties(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        simulate_physics: bool = True,
        gravity_enabled: bool = True,
        mass: float = 1.0,
        linear_damping: float = 0.01,
        angular_damping: float = 0.0,
    ) -> Dict[str, Any]:
        """Set physics properties on a component.

        Args:
            blueprint_name:   Name of the target Blueprint.
            component_name:   Name of the component.
            simulate_physics: Whether the component should simulate physics.
            gravity_enabled:  Whether the component should be affected by gravity.
            mass:             Mass in kilograms.
            linear_damping:   Linear damping coefficient.
            angular_damping:  Angular damping coefficient.

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def compile_blueprint(
        ctx: Context,
        blueprint_name: str,
    ) -> Dict[str, Any]:
        """Compile a Blueprint after edits.

        Returns:
            {"success": bool, ...}
        """

    @unreal_tool(mcp)
    def set_blueprint_property(
        ctx: Context,
        blueprint_name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """Set a property on a Blueprint class default object.

        Args:
            blueprint_name: Name of the target Blueprint.
            property_name:  Name of the property to set.
            property_value: Value to set the property to.

        Returns:
            {"success": bool, ...}
        """

    logger.info("Blueprint tools registered successfully")
