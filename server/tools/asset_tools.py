"""Asset Management Tools for Unreal MCP.

Query and mutate the Unreal Asset Registry. These are intentionally cheap
and read-mostly so they can be called before any level is loaded (e.g. to
enumerate a freshly-imported sample project's kit).

Tool surface (11 tools):

    list_assets               enumerate assets by path
    get_asset_info            full metadata + deps + referencers for one asset
    find_assets_by_class      every asset of a UClass (e.g. all StaticMesh)
    get_asset_dependencies    packages this asset needs
    get_asset_references      packages that reference this asset
    move_asset                rename to a new /Game/ path (creates redirector)
    delete_asset              delete an asset
    rename_asset              change asset's leaf name in place
    duplicate_asset           copy to a new path
    migrate_assets            cross-project Content-folder file copy
    finalize_migration        fix up internal refs after subfolder migrate
    import_asset              generic source-file → UAsset import

Wire format: each tool sends `{type: "<command_name>", params: {...}}` over
TCP to the C++ plugin. C++ side in
`plugin/Source/UnrealMCP/Private/Commands/UnrealMCPAssetCommands.cpp`.
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool

logger = logging.getLogger("UnrealMCP")


def register_asset_tools(mcp: FastMCP):
    """Register Asset Management tools with the MCP server."""

    @unreal_tool(mcp)
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
              "success": true,
              "assets": [{name, package_path, package_name, object_path, class_path, class_name}, ...],
              "count": N,
              "path": "/Game/...",
              "recursive": bool,
              "class_filter": "..." (only if filter was applied)
            }
        """

    @unreal_tool(mcp)
    def get_asset_info(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Full metadata for one asset, including dependency + referencer graph.

        Args:
            asset_path: Object path like "/Game/Foo/Bar.Bar" (UE convention is the
                        leaf name appears twice — once as package, once as object).

        Returns:
            {
              "success": true,
              ...AssetData fields...,
              "dependencies": [package_name, ...],
              "dependency_count": N,
              "referencers": [package_name, ...],
              "referencer_count": N
            }
        """

    @unreal_tool(mcp)
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
              "success": true,
              "assets": [...],
              "count": N,
              "class_name": "...",
              "class_path": "/Script/Engine.StaticMesh",
              "search_path": "/Game/..."
            }
        """

    @unreal_tool(mcp)
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
            {"success": true, "asset_path": ..., "recursive": bool, "dependencies": [...], "count": N}
        """

    @unreal_tool(mcp)
    def get_asset_references(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Packages that reference this asset (who depends on it).

        Useful before delete/move/rename to know what would break. Pairs with
        UE's "Reference Viewer" panel functionality.

        Args:
            asset_path: Object path like "/Game/Foo/Bar.Bar".

        Returns:
            {"success": true, "asset_path": ..., "referencers": [...], "count": N}
        """

    @unreal_tool(mcp)
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
            {"success": bool, "from_path": ..., "to_path": ..., "note"?: "..."}
        """

    @unreal_tool(mcp)
    def delete_asset(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Delete an asset. May fail if other assets reference it.

        Args:
            asset_path: Object path of the asset to delete.

        Returns:
            {"success": bool, "asset_path": ...}
        """

    @unreal_tool(mcp)
    def rename_asset(ctx: Context, asset_path: str, new_name: str) -> Dict[str, Any]:
        """Rename an asset in place (same /Game/ folder, new leaf name).

        Functionally a move within the same package path. Same caveats as
        move_asset re: redirectors and CDO pins.

        Args:
            asset_path: Current object path like "/Game/Foo/Bar.Bar".
            new_name:   New leaf name (without /Game/ prefix or .extension).

        Returns:
            {"success": bool, "asset_path": ..., "new_name": ..., "new_path": ...}
        """

    @unreal_tool(mcp)
    def duplicate_asset(ctx: Context, asset_path: str, target_path: str) -> Dict[str, Any]:
        """Duplicate an asset to a new path.

        Args:
            asset_path:  Source object path, e.g. "/Game/Foo/Bar.Bar".
            target_path: Target object path for the duplicate, e.g. "/Game/Baz/Bar_v2.Bar_v2".

        Returns:
            {"success": bool, "asset_path": ..., "target_path": ..., "new_object_path"?: "..."}
        """

    # ─── Cross-project migration ─────────────────────────────────────────

    @unreal_tool(mcp)
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
              "initial_count": N,
              "total_with_dependencies": M,
              "copied_count": K,
              "skipped_count": S,
              "destination_root": "...",
              "include_dependencies": bool,
              "errors": ["...", ...]
            }
        """

    @unreal_tool(mcp)
    def finalize_migration(
        ctx: Context,
        migrated_root: str,
        target_root: str = "/Game",
    ) -> Dict[str, Any]:
        """Fix up serialized references after a subfolder-style migrate_assets.

        Run this in the DESTINATION editor when migrated assets landed under
        a /Game/<subfolder>/ path (e.g. /Game/Migrated/Megascans/...) and
        their internal hard refs point at original /Game/Megascans/... paths
        that don't exist. Symptom: meshes render with checkerboard / default
        materials despite the materials being present at the migrated paths.

        Implementation: batch-renames every asset under `migrated_root` to
        the equivalent path under `target_root` using UE's IAssetTools::RenameAssets.
        That single call handles file moves, internal ref rewrites
        (mesh→MI→master chains), level-actor ref updates, and redirector
        creation at the old paths — all atomic, all UE-canonical.

        Args:
            migrated_root: /Game/-rooted path containing the wrongly-placed
                           assets (e.g. "/Game/Migrated"). Required.
            target_root:   Where to move them. Default "/Game" — strips the
                           offending subfolder so original /Game/Foo/Bar refs
                           resolve again.

        Returns:
            {
              "success": bool,
              "renamed_count": N,
              "scanned_count": M,
              "result": "Success"|"Failure"|"Pending",
              "migrated_root": "...",
              "target_root": "...",
              "note": "..."
            }
        """

    @unreal_tool(mcp)
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

        Args:
            file_path:        Absolute filesystem path to the source file.
            destination_path: /Game/-prefixed package path where the imported
                              asset should land (e.g. "/Game/Imported/MyMesh").
                              The folder is created if it doesn't exist.
            replace_existing: Whether to overwrite an asset already at the
                              destination path. Default True.
            save:             Save the new asset to disk immediately after
                              import. Default True.

        Returns:
            {
              "success": bool,
              "file_path": "...",
              "destination_path": "/Game/...",
              "imported_object_paths": ["/Game/Imported/MyMesh.MyMesh", ...],
              "imported_count": N,
              "note"?: "..."
            }
        """

    logger.info("Asset tools registered successfully")
