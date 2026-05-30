#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Asset Management MCP commands.
 *
 * All tools in this category operate on the Asset Registry — they don't
 * modify the loaded world. Cheap, fast, safe to call before any level
 * is loaded.
 *
 * Tools:
 *   list_assets              — enumerate by path + recursion + class filter
 *   get_asset_info           — full metadata + dependencies + referencers
 *   find_assets_by_class     — find every asset of a given UClass
 *   get_asset_dependencies   — packages this asset depends on
 *   get_asset_references     — packages that reference this asset
 *   move_asset               — rename to a new /Game/ path
 *   delete_asset             — delete an asset (creates a redirector)
 *   rename_asset             — change asset's leaf name in place
 *   duplicate_asset          — copy to a new path
 *
 * Sprint 2:
 *   migrate_assets           — copy assets + deps to another project's Content dir
 *   import_asset             — import an .fbx / .png / .wav / etc. file into the project
 */
class UNREALMCP_API FUnrealMCPAssetCommands
{
public:
    FUnrealMCPAssetCommands();

    /** Top-level entry point used by UUnrealMCPBridge. */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Asset registry queries
    TSharedPtr<FJsonObject> HandleListAssets(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindAssetsByClass(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAssetDependencies(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAssetReferences(const TSharedPtr<FJsonObject>& Params);

    // Asset mutations
    TSharedPtr<FJsonObject> HandleMoveAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRenameAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params);

    // Sprint 2 — cross-project migration + asset import
    TSharedPtr<FJsonObject> HandleMigrateAssets(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleImportAsset(const TSharedPtr<FJsonObject>& Params);
};
