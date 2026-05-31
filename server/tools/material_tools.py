"""Material Tools for Unreal MCP.

Inspect, create, tune material instances; trace usage relationships.

Tool surface (5 tools):

    get_material_parameters             scalar/vector/texture params + values
    set_material_instance_param         set a param on a material instance
    create_material_instance            create new MI from a parent material
    get_material_uses                   assets referencing this material
    list_material_instances_of_parent   every MI derived from a given parent

Use case driving the work: Lauder Phase 7.2 — once Goddess Temple master
materials (M_BlendMaster, M_SSSMaster, M_StandardMaster, etc.) are
migrated into Lauder3, we'll create instances of them, tune
scalar/vector/texture params to suit the cozy temple alcove mood, and
inspect what assets use which.

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPMaterialCommands.cpp`.
"""

import logging
from typing import Any, Dict, Union

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool

logger = logging.getLogger("UnrealMCP")


def register_material_tools(mcp: FastMCP):
    """Register Material tools with the MCP server."""

    @unreal_tool(mcp)
    def get_material_parameters(ctx: Context, material_path: str) -> Dict[str, Any]:
        """Read the parameters of a Material or MaterialInstance.

        Works on both base materials (returns default values) and material
        instances (returns current values, which may override the base).

        Args:
            material_path: Object path like "/Game/Fab/VFX/.../M_Foo.M_Foo".

        Returns:
            {
              "success": true,
              "material_path": "...",
              "class_name": "Material" | "MaterialInstanceConstant" | ...,
              "scalar_parameters": [{name, value}, ...],
              "vector_parameters": [{name, r, g, b, a}, ...],
              "texture_parameters": [{name, texture_path}, ...],
              "scalar_count": N, "vector_count": M, "texture_count": K
            }
        """

    @unreal_tool(mcp)
    def set_material_instance_param(
        ctx: Context,
        material_instance_path: str,
        param_name: str,
        param_type: str,
        value: Union[float, Dict[str, float], str],
    ) -> Dict[str, Any]:
        """Override a parameter on a UMaterialInstanceConstant.

        Args:
            material_instance_path: Object path to a UMaterialInstanceConstant
                                    (not a base UMaterial — those have no
                                    overridable parameters per se).
            param_name:             Name of the parameter to set.
            param_type:             One of "scalar" | "vector" | "texture".
            value:                  - For "scalar": a number (e.g. 1.5).
                                    - For "vector": a {r, g, b, a} dict
                                      (e.g. {"r": 1.0, "g": 0.5, "b": 0.0, "a": 1.0}).
                                    - For "texture": a /Game/-prefixed object
                                      path to a Texture asset.

        The change is saved to the material instance on success. To revert,
        call again with the original value, or recreate the instance.

        Returns:
            {"success": bool, "material_instance_path": ..., "param_name": ..., "param_type": ...}
        """

    @unreal_tool(mcp)
    def create_material_instance(
        ctx: Context,
        parent_material_path: str,
        target_path: str,
    ) -> Dict[str, Any]:
        """Create a new UMaterialInstanceConstant derived from a parent material.

        Args:
            parent_material_path: Object path to the parent (either a base
                                  UMaterial or another UMaterialInstance —
                                  parameter inheritance chain follows).
            target_path:          /Game/-prefixed package path for the new
                                  instance, e.g. "/Game/Fab/.../MI_MyVariant".
                                  Subfolders created as needed; existing assets
                                  at the path cause failure (the create returns
                                  null).

        Returns:
            {"success": bool, "parent_material_path": ..., "target_path": ..., "created_object_path": ...}
        """

    @unreal_tool(mcp)
    def get_material_uses(ctx: Context, material_path: str) -> Dict[str, Any]:
        """List assets that reference this material — meshes, blueprints, etc.

        Useful before modifying or deleting a material to know what would
        be affected. Equivalent to UE's "Reference Viewer" / "Show Asset
        References" Content Browser action.

        Args:
            material_path: Object path to the material or material instance.

        Returns:
            {"success": true, "material_path": ..., "referencers": ["...", ...], "count": N}
        """

    @unreal_tool(mcp)
    def list_material_instances_of_parent(
        ctx: Context,
        parent_material_path: str,
        search_path: str = "/Game",
    ) -> Dict[str, Any]:
        """List every UMaterialInstanceConstant whose parent is the given material.

        Args:
            parent_material_path: Object path to a parent material.
            search_path:          /Game/ prefix to scope the search.
                                  Default "/Game" (whole project).

        Returns:
            {
              "success": true,
              "parent_material_path": "...",
              "search_path": "/Game/...",
              "material_instances": [{name, object_path, package_path}, ...],
              "count": N
            }

        Notes:
        - Searches only UMaterialInstanceConstant (the editor-time MI type
          you usually create). Runtime-only dynamic instances are not
          surfaced here — they don't live as assets.
        - Each candidate MI is loaded to read its parent, which is reliable
          but not free. For very large projects, narrow search_path.
        """

    logger.info("Material tools registered successfully")
