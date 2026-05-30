"""
Asset Management Tools for Unreal MCP — Sprint 1.

Tools in this module query and mutate the Unreal Asset Registry. They are
intentionally cheap and read-mostly so they can be called before any level
is loaded (e.g. to enumerate a freshly-imported sample project's kit).

Tool surface (9 tools):

    list_assets               — enumerate assets by path
    get_asset_info            — full metadata + deps + referencers for one asset
    find_assets_by_class      — every asset of a UClass (e.g. all StaticMesh)
    get_asset_dependencies    — packages this asset needs
    get_asset_references      — packages that reference this asset
    move_asset                — rename to a new /Game/ path (creates redirector)
    delete_asset              — delete an asset
    rename_asset              — change asset's leaf name in place
    duplicate_asset           — copy to a new path

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin and returns the response's `result` field. The C++ side
is implemented in `Plugin/UnrealMCP/Source/UnrealMCP/Private/Commands/
UnrealMCPAssetCommands.cpp` — see that file for the underlying UE 5.7 APIs.
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

logger = logging.getLogger("UnrealMCP")


def _unwrap(response: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    """Pull the result payload out of an Unreal MCP response envelope.

    The C++ side wraps successful results as `{result: {...}}` and errors as
    `{error: "..."}`. Both shapes have been seen historically so we accept
    either, and fall back to returning the whole response on shape mismatch.
    """
    if not response:
        return {"error": "no response from Unreal"}
    if "error" in response:
        return {"error": response["error"]}
    if "result" in response:
        return response["result"]
    return response


def register_asset_tools(mcp: FastMCP):
    """Register Asset Management tools with the MCP server."""

    @mcp.tool()
    def list_assets(
        ctx: Context,
        path: str,
        recursive: bool = True,
        class_filter: Optional[str] = None,
    ) -> Dict[str, Any]:
        """List assets under a /Game/ path.

        Args:
            path:         /Game/-prefixed package path (e.g. "/Game/Megascans/3D_Assets").
            recursive:    descend into subfolders. Default True.
            class_filter: optional class name (e.g. "StaticMesh") to filter results.
                          Accepts bare names ("StaticMesh"), full paths
                          ("/Script/Engine.StaticMesh"), or anything FindObject<UClass>
                          can resolve.

        Returns:
            {
              "assets": [{name, package_path, package_name, object_path, class_path, class_name}, ...],
              "count": N,
              "path": "/Game/...",
              "recursive": bool,
              "class_filter": "..." (only if filter was applied)
            }
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {"path": path, "recursive": recursive}
            if class_filter:
                params["class_filter"] = class_filter

            return _unwrap(unreal.send_command("list_assets", params))

        except Exception as e:
            logger.error(f"list_assets error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def get_asset_info(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Full metadata for one asset, including dependency + referencer graph.

        Args:
            asset_path: Object path like "/Game/Foo/Bar.Bar" (UE convention is the
                        leaf name appears twice — once as package, once as object).

        Returns:
            {
              ...AssetData fields...,
              "dependencies": [package_name, ...],
              "dependency_count": N,
              "referencers": [package_name, ...],
              "referencer_count": N
            }
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            return _unwrap(unreal.send_command("get_asset_info", {"asset_path": asset_path}))

        except Exception as e:
            logger.error(f"get_asset_info error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def find_assets_by_class(
        ctx: Context,
        class_name: str,
        search_path: str = "/Game",
        recursive: bool = True,
    ) -> Dict[str, Any]:
        """Find every asset of a given UClass under a search path.

        Includes subclasses by default (e.g. searching for Material also returns
        MaterialInstance, MaterialInstanceConstant, etc.). Use this for kit
        inventory — "all StaticMesh under /Game/Megascans" type questions.

        Args:
            class_name:  Class name. Accepts bare ("StaticMesh"), full path
                         ("/Script/Engine.StaticMesh"), or anything UE can resolve.
            search_path: /Game/ prefix to scope the search. Default "/Game".
            recursive:   descend into subfolders. Default True.

        Returns:
            {
              "assets": [...],
              "count": N,
              "class_name": "...",
              "class_path": "/Script/Engine.StaticMesh",
              "search_path": "/Game/..."
            }
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            return _unwrap(unreal.send_command("find_assets_by_class", {
                "class_name": class_name,
                "search_path": search_path,
                "recursive": recursive,
            }))

        except Exception as e:
            logger.error(f"find_assets_by_class error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def get_asset_dependencies(
        ctx: Context,
        asset_path: str,
        recursive: bool = False,
    ) -> Dict[str, Any]:
        """Packages this asset depends on (what it loads to function).

        Args:
            asset_path: Object path like "/Game/Foo/Bar.Bar".
            recursive:  if True, walks the full transitive dependency graph.
                        Default False (one hop only). Recursive can return
                        large sets for complex assets (Niagara emitters,
                        materials with many functions, etc.) — start non-recursive.

        Returns:
            {"asset_path": ..., "recursive": bool, "dependencies": [...], "count": N}
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            return _unwrap(unreal.send_command("get_asset_dependencies", {
                "asset_path": asset_path,
                "recursive": recursive,
            }))

        except Exception as e:
            logger.error(f"get_asset_dependencies error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def get_asset_references(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Packages that reference this asset (who depends on it).

        Useful before delete/move/rename to know what would break. Pairs with
        UE's "Reference Viewer" panel functionality.

        Args:
            asset_path: Object path like "/Game/Foo/Bar.Bar".

        Returns:
            {"asset_path": ..., "referencers": [...], "count": N}
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            return _unwrap(unreal.send_command("get_asset_references", {"asset_path": asset_path}))

        except Exception as e:
            logger.error(f"get_asset_references error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def move_asset(ctx: Context, from_path: str, to_path: str) -> Dict[str, Any]:
        """Move/rename an asset to a different /Game/ path.

        Creates a redirector at the old path so existing references continue
        to resolve. Run "Fix Up Redirectors" later to collapse the chain.

        Common failure modes (returns success=False with a note):
          - Target path already exists
          - Source is CDO-pinned via C++ ConstructorHelpers (Lauder hit this
            in Phase 6.5 — see feedback_unreal_rename_asset_cdo_pin in memory)
          - Asset is open in a sub-editor (e.g. material editor)
          - Asset is checked out by source control

        Args:
            from_path: Object path of the current asset, e.g. "/Game/Foo/Bar.Bar".
            to_path:   Object path of the new location, e.g. "/Game/Baz/Bar.Bar".

        Returns:
            {"from_path": ..., "to_path": ..., "success": bool, "note"?: "..."}
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            return _unwrap(unreal.send_command("move_asset", {
                "from_path": from_path,
                "to_path": to_path,
            }))

        except Exception as e:
            logger.error(f"move_asset error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def delete_asset(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Delete an asset. May fail if other assets reference it.

        Args:
            asset_path: Object path of the asset to delete.

        Returns:
            {"asset_path": ..., "success": bool}
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            return _unwrap(unreal.send_command("delete_asset", {"asset_path": asset_path}))

        except Exception as e:
            logger.error(f"delete_asset error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def rename_asset(ctx: Context, asset_path: str, new_name: str) -> Dict[str, Any]:
        """Rename an asset in place (same /Game/ folder, new leaf name).

        Functionally a move within the same package path. Same caveats as
        move_asset re: redirectors and CDO pins.

        Args:
            asset_path: Current object path like "/Game/Foo/Bar.Bar".
            new_name:   New leaf name (without /Game/ prefix or .extension).

        Returns:
            {"asset_path": ..., "new_name": ..., "new_path": ..., "success": bool}
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            return _unwrap(unreal.send_command("rename_asset", {
                "asset_path": asset_path,
                "new_name": new_name,
            }))

        except Exception as e:
            logger.error(f"rename_asset error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def duplicate_asset(ctx: Context, asset_path: str, target_path: str) -> Dict[str, Any]:
        """Duplicate an asset to a new path.

        Args:
            asset_path:  Source object path, e.g. "/Game/Foo/Bar.Bar".
            target_path: Target object path for the duplicate, e.g. "/Game/Baz/Bar_v2.Bar_v2".

        Returns:
            {"asset_path": ..., "target_path": ..., "success": bool, "new_object_path"?: "..."}
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            return _unwrap(unreal.send_command("duplicate_asset", {
                "asset_path": asset_path,
                "target_path": target_path,
            }))

        except Exception as e:
            logger.error(f"duplicate_asset error: {e}")
            return {"error": str(e)}

    # ─── Sprint 2 — cross-project migration ──────────────────────────────────

    @mcp.tool()
    def migrate_assets(
        ctx: Context,
        asset_paths: List[str],
        destination_content_path: str,
        include_dependencies: bool = True,
        force_overwrite: bool = False,
    ) -> Dict[str, Any]:
        """Copy assets from the current editor's project to another project's Content dir.

        Designed for the Lauder Phase 7.2 use case: editor open on the source
        sample project (Goddess Temple, Dark Ruins, etc.), MCP call drives
        copy of selected assets + their dependency closure into the target
        project's Content folder, preserving /Game/-relative directory layout.

        Implementation: walks the AssetRegistry for dependencies, then copies
        the underlying .uasset / .umap files via IFileManager. Equivalent to
        UE's "Migrate" workflow's file-copy step but headless (no modal
        dialog) and idempotent (skips existing files unless force_overwrite).

        Args:
            asset_paths:               List of /Game/-prefixed object paths to migrate.
                                       Must be assets visible in the currently-loaded
                                       editor's asset registry.
            destination_content_path:  Absolute filesystem path to the target project's
                                       Content/ folder. Subfolders created automatically.
            include_dependencies:      Whether to also migrate every /Game/ asset that
                                       the named assets depend on. Default True.
                                       Engine + plugin packages always skipped.
            force_overwrite:           Whether to overwrite existing files at destination.
                                       Default False (existing files counted in
                                       skipped_count instead).

        Returns:
            {
              "success": bool,
              "initial_count": N,                    # how many /Game/ paths you asked for
              "total_with_dependencies": M,           # incl. transitive /Game/ deps
              "copied_count": K,                     # files actually copied
              "skipped_count": S,                    # existed at destination + no overwrite
              "destination_root": "...",
              "include_dependencies": bool,
              "errors": ["...", ...]                 # per-file error strings, empty on full success
            }

        After migration: open the destination project in UE and let it auto-discover
        the new assets, or refresh the Content Browser. References from migrated
        assets to /Game/ assets you didn't include will break (show as missing).
        Engine + plugin references survive (they resolve in any project).
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            return _unwrap(unreal.send_command("migrate_assets", {
                "asset_paths": asset_paths,
                "destination_content_path": destination_content_path,
                "include_dependencies": include_dependencies,
                "force_overwrite": force_overwrite,
            }))

        except Exception as e:
            logger.error(f"migrate_assets error: {e}")
            return {"error": str(e)}

    @mcp.tool()
    def import_asset(
        ctx: Context,
        file_path: str,
        destination_path: str,
        replace_existing: bool = True,
        save: bool = True,
    ) -> Dict[str, Any]:
        """Import a source file (.fbx, .png, .wav, etc.) into the project as a UAsset.

        Single generic import tool — UE's `UAssetImportTask` auto-detects the
        file type from extension and selects the appropriate factory:

            FBX / OBJ         → StaticMesh, SkeletalMesh, AnimSequence
            PNG / TGA / PSD
            EXR / HDR / JPG   → Texture2D
            WAV / MP3 / OGG   → SoundWave
            FBX (skeleton)    → Skeleton / PhysicsAsset

        Use cases for Lauder:
        - Importing Megascans surface PBR sets that didn't come over with
          migrate_assets (e.g., variants downloaded later from Quixel Bridge).
        - Importing custom audio cues for the workbench / extraction beacon.
        - Importing Blender-exported FBX meshes for custom props.

        Args:
            file_path:        Absolute filesystem path to the source file.
            destination_path: /Game/-prefixed package path where the imported
                              asset should land (e.g. "/Game/Imported/MyMesh").
                              The folder is created if it doesn't exist.
            replace_existing: Whether to overwrite an asset already at the
                              destination path. Default True.
            save:             Save the new asset to disk immediately after
                              import. Default True (False keeps it in-memory
                              only — useful for batch operations followed by
                              a single save_all_dirty call).

        Returns:
            {
              "file_path": "...",
              "destination_path": "/Game/...",
              "imported_object_paths": ["/Game/Imported/MyMesh.MyMesh", ...],
              "imported_count": N,
              "success": bool,
              "note"?: "..."  (only on imported_count==0)
            }

        Note: a single source file can produce multiple imported objects.
        FBX with skeletal animation, for example, produces SkeletalMesh +
        Skeleton + AnimSequence + PhysicsAsset. All paths returned in
        imported_object_paths.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            return _unwrap(unreal.send_command("import_asset", {
                "file_path": file_path,
                "destination_path": destination_path,
                "replace_existing": replace_existing,
                "save": save,
            }))

        except Exception as e:
            logger.error(f"import_asset error: {e}")
            return {"error": str(e)}
