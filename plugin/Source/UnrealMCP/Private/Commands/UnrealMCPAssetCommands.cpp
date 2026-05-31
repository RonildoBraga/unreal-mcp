#include "Commands/UnrealMCPAssetCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "EditorAssetLibrary.h"
#include "UObject/Class.h"
#include "UObject/TopLevelAssetPath.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "AssetToolsModule.h"
#include "AssetImportTask.h"
#include "IAssetTools.h"
// v0.8.0 Day 3-4 — Content Browser sync + asset editor + static mesh inspection.
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
    /** Resolve the global asset registry. Returns nullptr if module not loaded. */
    IAssetRegistry* GetRegistry()
    {
        FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        return &Module.Get();
    }

    /** Convert a single FAssetData into a JSON object. Kept consistent across all
     *  asset tools so Python-side code doesn't have to special-case shapes. */
    TSharedPtr<FJsonObject> AssetDataToJson(const FAssetData& AssetData)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        Obj->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
        Obj->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
        Obj->SetStringField(TEXT("object_path"), AssetData.GetObjectPathString());
        // AssetClassPath: UE 5.1+ canonical form; ClassName retained for human-friendly
        Obj->SetStringField(TEXT("class_path"), AssetData.AssetClassPath.ToString());
        Obj->SetStringField(TEXT("class_name"), AssetData.AssetClassPath.GetAssetName().ToString());
        return Obj;
    }

    /** Best-effort class resolution: accepts either bare class name ("StaticMesh"),
     *  full path ("/Script/Engine.StaticMesh"), or class-default-object path. */
    FTopLevelAssetPath ResolveClassPath(const FString& ClassName)
    {
        // Already a full path?
        if (ClassName.Contains(TEXT(".")) && ClassName.StartsWith(TEXT("/Script/")))
        {
            return FTopLevelAssetPath(ClassName);
        }
        // Try common Engine module first — covers StaticMesh, SkeletalMesh, Material, etc.
        FTopLevelAssetPath EnginePath(TEXT("/Script/Engine"), *ClassName);
        if (FindObject<UClass>(EnginePath))
        {
            return EnginePath;
        }
        // Fallback: try the loaded UClass directly
        if (UClass* Found = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous))
        {
            return Found->GetClassPathName();
        }
        return FTopLevelAssetPath();
    }
}


FUnrealMCPAssetCommands::FUnrealMCPAssetCommands()
{
}


TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("list_assets"))            return HandleListAssets(Params);
    if (CommandType == TEXT("get_asset_info"))         return HandleGetAssetInfo(Params);
    if (CommandType == TEXT("find_assets_by_class"))   return HandleFindAssetsByClass(Params);
    if (CommandType == TEXT("get_asset_dependencies")) return HandleGetAssetDependencies(Params);
    if (CommandType == TEXT("get_asset_references"))   return HandleGetAssetReferences(Params);
    if (CommandType == TEXT("move_asset"))             return HandleMoveAsset(Params);
    if (CommandType == TEXT("delete_asset"))           return HandleDeleteAsset(Params);
    if (CommandType == TEXT("rename_asset"))           return HandleRenameAsset(Params);
    if (CommandType == TEXT("duplicate_asset"))        return HandleDuplicateAsset(Params);
    if (CommandType == TEXT("migrate_assets"))         return HandleMigrateAssets(Params);
    if (CommandType == TEXT("import_asset"))           return HandleImportAsset(Params);
    if (CommandType == TEXT("finalize_migration"))     return HandleFinalizeMigration(Params);
    if (CommandType == TEXT("focus_in_browser"))       return HandleFocusInBrowser(Params);
    if (CommandType == TEXT("navigate_to_folder"))     return HandleNavigateToFolder(Params);
    if (CommandType == TEXT("open_in_editor"))         return HandleOpenInEditor(Params);
    if (CommandType == TEXT("static_mesh_get_info"))   return HandleStaticMeshGetInfo(Params);

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}


TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleListAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString Path;
    if (!Params->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'path' parameter"));
    }

    bool bRecursive = true;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    FString ClassFilter;
    Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

    IAssetRegistry* Registry = GetRegistry();
    if (!Registry)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetRegistry module unavailable"));
    }

    TArray<FAssetData> AssetData;
    Registry->GetAssetsByPath(FName(*Path), AssetData, bRecursive, /*bIncludeOnlyOnDiskAssets*/ false);

    // Optional client-side class filter — easier than the FARFilter setup for this case
    FTopLevelAssetPath FilterClass;
    const bool bHasFilter = !ClassFilter.IsEmpty();
    if (bHasFilter)
    {
        FilterClass = ResolveClassPath(ClassFilter);
    }

    TArray<TSharedPtr<FJsonValue>> Assets;
    for (const FAssetData& AD : AssetData)
    {
        if (bHasFilter && AD.AssetClassPath != FilterClass)
        {
            continue;
        }
        Assets.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AD)));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), Assets);
    Result->SetNumberField(TEXT("count"), Assets.Num());
    Result->SetStringField(TEXT("path"), Path);
    Result->SetBoolField(TEXT("recursive"), bRecursive);
    if (bHasFilter)
    {
        Result->SetStringField(TEXT("class_filter"), ClassFilter);
    }
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'asset_path' parameter"));
    }

    IAssetRegistry* Registry = GetRegistry();
    if (!Registry)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetRegistry module unavailable"));
    }

    FAssetData AD = Registry->GetAssetByObjectPath(FSoftObjectPath(AssetPath));
    if (!AD.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = AssetDataToJson(AD);

    // Dependencies (what THIS asset needs)
    TArray<FName> Dependencies;
    Registry->GetDependencies(AD.PackageName, Dependencies);
    TArray<TSharedPtr<FJsonValue>> DepValues;
    for (const FName& Dep : Dependencies)
    {
        DepValues.Add(MakeShared<FJsonValueString>(Dep.ToString()));
    }
    Result->SetArrayField(TEXT("dependencies"), DepValues);
    Result->SetNumberField(TEXT("dependency_count"), DepValues.Num());

    // Referencers (what NEEDS this asset)
    TArray<FName> Referencers;
    Registry->GetReferencers(AD.PackageName, Referencers);
    TArray<TSharedPtr<FJsonValue>> RefValues;
    for (const FName& Ref : Referencers)
    {
        RefValues.Add(MakeShared<FJsonValueString>(Ref.ToString()));
    }
    Result->SetArrayField(TEXT("referencers"), RefValues);
    Result->SetNumberField(TEXT("referencer_count"), RefValues.Num());

    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleFindAssetsByClass(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName;
    if (!Params->TryGetStringField(TEXT("class_name"), ClassName) || ClassName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'class_name' parameter"));
    }

    FString SearchPath = TEXT("/Game");
    Params->TryGetStringField(TEXT("search_path"), SearchPath);

    bool bRecursive = true;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    FTopLevelAssetPath ClassPath = ResolveClassPath(ClassName);
    if (!ClassPath.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not resolve class: %s (try full path like /Script/Engine.StaticMesh)"), *ClassName));
    }

    IAssetRegistry* Registry = GetRegistry();
    if (!Registry)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetRegistry module unavailable"));
    }

    // Use FARFilter for combined path+class filtering — single AR query, fast.
    FARFilter Filter;
    Filter.ClassPaths.Add(ClassPath);
    Filter.bRecursiveClasses = true;            // also include subclasses (e.g. Material → MaterialInstance)
    Filter.PackagePaths.Add(FName(*SearchPath));
    Filter.bRecursivePaths = bRecursive;

    TArray<FAssetData> AssetData;
    Registry->GetAssets(Filter, AssetData);

    TArray<TSharedPtr<FJsonValue>> Assets;
    for (const FAssetData& AD : AssetData)
    {
        Assets.Add(MakeShared<FJsonValueObject>(AssetDataToJson(AD)));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("assets"), Assets);
    Result->SetNumberField(TEXT("count"), Assets.Num());
    Result->SetStringField(TEXT("class_name"), ClassName);
    Result->SetStringField(TEXT("class_path"), ClassPath.ToString());
    Result->SetStringField(TEXT("search_path"), SearchPath);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetDependencies(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'asset_path' parameter"));
    }

    bool bRecursive = false;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    IAssetRegistry* Registry = GetRegistry();
    if (!Registry)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetRegistry module unavailable"));
    }

    // Convert object-path → package-name for the dependency query
    FName PackageName(*FPackageName::ObjectPathToPackageName(AssetPath));

    TSet<FName> Visited;
    TArray<FName> ToProcess;
    ToProcess.Add(PackageName);

    while (ToProcess.Num() > 0)
    {
        FName Current = ToProcess.Pop(EAllowShrinking::No);
        if (Visited.Contains(Current))
        {
            continue;
        }
        Visited.Add(Current);

        TArray<FName> Deps;
        Registry->GetDependencies(Current, Deps);
        for (const FName& Dep : Deps)
        {
            if (!Visited.Contains(Dep))
            {
                if (bRecursive)
                {
                    ToProcess.Add(Dep);
                }
                else if (Current == PackageName)
                {
                    // Non-recursive: only record direct deps from the original
                    Visited.Add(Dep);
                }
            }
        }
    }

    // Don't include the original package itself in the dependency list
    Visited.Remove(PackageName);

    TArray<TSharedPtr<FJsonValue>> DepValues;
    for (const FName& Dep : Visited)
    {
        DepValues.Add(MakeShared<FJsonValueString>(Dep.ToString()));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetBoolField(TEXT("recursive"), bRecursive);
    Result->SetArrayField(TEXT("dependencies"), DepValues);
    Result->SetNumberField(TEXT("count"), DepValues.Num());
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetReferences(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'asset_path' parameter"));
    }

    IAssetRegistry* Registry = GetRegistry();
    if (!Registry)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetRegistry module unavailable"));
    }

    FName PackageName(*FPackageName::ObjectPathToPackageName(AssetPath));

    TArray<FName> Referencers;
    Registry->GetReferencers(PackageName, Referencers);

    TArray<TSharedPtr<FJsonValue>> RefValues;
    for (const FName& Ref : Referencers)
    {
        RefValues.Add(MakeShared<FJsonValueString>(Ref.ToString()));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetArrayField(TEXT("referencers"), RefValues);
    Result->SetNumberField(TEXT("count"), RefValues.Num());
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleMoveAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString FromPath, ToPath;
    if (!Params->TryGetStringField(TEXT("from_path"), FromPath) || FromPath.IsEmpty() ||
        !Params->TryGetStringField(TEXT("to_path"),   ToPath)   || ToPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'from_path' or 'to_path'"));
    }

    // EditorAssetLibrary::RenameAsset == move (creates a redirector at FromPath).
    const bool bSuccess = UEditorAssetLibrary::RenameAsset(FromPath, ToPath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("from_path"), FromPath);
    Result->SetStringField(TEXT("to_path"),   ToPath);
    Result->SetBoolField(TEXT("success"), bSuccess);
    if (!bSuccess)
    {
        Result->SetStringField(TEXT("note"),
            TEXT("RenameAsset returned False — typical causes: target path exists, "
                 "source is CDO-pinned by C++ ConstructorHelpers, asset is open in an "
                 "editor, or asset is checked out by source control."));
    }
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'asset_path' parameter"));
    }

    const bool bSuccess = UEditorAssetLibrary::DeleteAsset(AssetPath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetBoolField(TEXT("success"), bSuccess);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath, NewName;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty() ||
        !Params->TryGetStringField(TEXT("new_name"),   NewName)   || NewName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' or 'new_name'"));
    }

    // Compute the new full path: replace the leaf name in AssetPath
    // e.g. "/Game/Foo/Bar.Bar" + "Baz" → "/Game/Foo/Baz"
    FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
    FString NewPath = FString::Printf(TEXT("%s/%s"), *PackagePath, *NewName);

    const bool bSuccess = UEditorAssetLibrary::RenameAsset(AssetPath, NewPath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("new_name"),   NewName);
    Result->SetStringField(TEXT("new_path"),   NewPath);
    Result->SetBoolField(TEXT("success"), bSuccess);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath, TargetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"),  AssetPath)  || AssetPath.IsEmpty() ||
        !Params->TryGetStringField(TEXT("target_path"), TargetPath) || TargetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' or 'target_path'"));
    }

    UObject* DuplicatedObject = UEditorAssetLibrary::DuplicateAsset(AssetPath, TargetPath);
    const bool bSuccess = (DuplicatedObject != nullptr);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"),  AssetPath);
    Result->SetStringField(TEXT("target_path"), TargetPath);
    Result->SetBoolField(TEXT("success"), bSuccess);
    if (bSuccess)
    {
        Result->SetStringField(TEXT("new_object_path"), DuplicatedObject->GetPathName());
    }
    return Result;
}


// ─── Sprint 2 — cross-project migration ───────────────────────────────────────
//
// Implementation strategy: do the work ourselves rather than calling into UE's
// IAssetTools::MigratePackages. The built-in MigratePackages drives a modal
// dialog ("Select destination project") that we can't satisfy from a headless
// MCP call, and its non-UI overloads vary across UE 5.x point releases.
//
// What we do instead: compute the dependency closure via the AssetRegistry,
// resolve each /Game/-prefixed package name to its on-disk .uasset/.umap path
// in the source project, and copy each file to the destination Content/ tree
// preserving the /Game/-relative directory layout. This is exactly what
// UE Migrate does under the hood for the actual file-copy step.

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleMigrateAssets(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Parse params
    const TArray<TSharedPtr<FJsonValue>>* AssetPathsJson = nullptr;
    if (!Params->TryGetArrayField(TEXT("asset_paths"), AssetPathsJson) || AssetPathsJson->Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing or empty 'asset_paths' parameter (expected array of /Game/-prefixed object paths)"));
    }

    FString DestContentRoot;
    if (!Params->TryGetStringField(TEXT("destination_content_path"), DestContentRoot) || DestContentRoot.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'destination_content_path' parameter (absolute filesystem path to target project's Content/ folder)"));
    }
    DestContentRoot = DestContentRoot.TrimChar(TEXT('/')).TrimChar(TEXT('\\'));

    bool bIncludeDeps = true;
    Params->TryGetBoolField(TEXT("include_dependencies"), bIncludeDeps);

    bool bForceOverwrite = false;
    Params->TryGetBoolField(TEXT("force_overwrite"), bForceOverwrite);

    // 2. Resolve initial /Game/... object paths → package names
    TArray<FName> InitialPackages;
    for (const auto& Val : *AssetPathsJson)
    {
        FString ObjectPath = Val->AsString();
        if (ObjectPath.IsEmpty()) continue;
        FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
        if (PackageName.StartsWith(TEXT("/Game/")))
        {
            InitialPackages.AddUnique(FName(*PackageName));
        }
    }
    if (InitialPackages.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("No valid /Game/-prefixed paths in 'asset_paths' (all entries must resolve to a /Game/ package)"));
    }

    // 3. Compute dependency closure if requested
    TSet<FName> AllPackages;
    for (const FName& Pkg : InitialPackages)
    {
        AllPackages.Add(Pkg);
    }

    if (bIncludeDeps)
    {
        IAssetRegistry* Registry = GetRegistry();
        if (!Registry)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetRegistry module unavailable"));
        }

        TArray<FName> ToProcess = InitialPackages;
        while (ToProcess.Num() > 0)
        {
            FName Current = ToProcess.Pop(EAllowShrinking::No);
            TArray<FName> Deps;
            Registry->GetDependencies(Current, Deps);
            for (const FName& Dep : Deps)
            {
                const FString DepStr = Dep.ToString();
                // Only include /Game/ deps; skip engine + plugin packages
                if (DepStr.StartsWith(TEXT("/Game/")) && !AllPackages.Contains(Dep))
                {
                    AllPackages.Add(Dep);
                    ToProcess.Add(Dep);
                }
            }
        }
    }

    // 4. Copy each package's file to the destination, preserving /Game-relative layout
    int32 CopiedCount = 0;
    int32 SkippedCount = 0;
    TArray<TSharedPtr<FJsonValue>> ErrorMessages;

    IFileManager& FileManager = IFileManager::Get();

    for (const FName& Pkg : AllPackages)
    {
        const FString PackageStr = Pkg.ToString();

        // Convert /Game/Foo/Bar package name to an absolute filesystem path (no extension)
        FString SourceBase;
        if (!FPackageName::TryConvertLongPackageNameToFilename(PackageStr, SourceBase))
        {
            ErrorMessages.Add(MakeShared<FJsonValueString>(
                FString::Printf(TEXT("Could not resolve filesystem path for: %s"), *PackageStr)));
            continue;
        }

        // Figure out which extension this asset uses on disk (.uasset for normal assets, .umap for levels)
        FString SourceFile;
        if (FPaths::FileExists(SourceBase + TEXT(".uasset")))
        {
            SourceFile = SourceBase + TEXT(".uasset");
        }
        else if (FPaths::FileExists(SourceBase + TEXT(".umap")))
        {
            SourceFile = SourceBase + TEXT(".umap");
        }
        else
        {
            ErrorMessages.Add(MakeShared<FJsonValueString>(
                FString::Printf(TEXT("Source file not found on disk: %s.uasset|.umap"), *SourceBase)));
            continue;
        }

        // Compute destination path: strip "/Game/" prefix, prepend DestContentRoot, append extension
        FString RelativeUnderGame = PackageStr;
        RelativeUnderGame.RemoveFromStart(TEXT("/Game/"));
        const FString DestExt = FPaths::GetExtension(SourceFile, /*bIncludeDot*/ true);
        const FString DestFile = FPaths::Combine(DestContentRoot, RelativeUnderGame) + DestExt;

        // Honor force_overwrite
        if (FPaths::FileExists(DestFile) && !bForceOverwrite)
        {
            SkippedCount++;
            continue;
        }

        // Ensure destination directory exists
        const FString DestDir = FPaths::GetPath(DestFile);
        if (!FileManager.MakeDirectory(*DestDir, /*Tree*/ true))
        {
            ErrorMessages.Add(MakeShared<FJsonValueString>(
                FString::Printf(TEXT("Could not create destination directory: %s"), *DestDir)));
            continue;
        }

        // Copy the file
        if (FileManager.Copy(*DestFile, *SourceFile) == COPY_OK)
        {
            CopiedCount++;
        }
        else
        {
            ErrorMessages.Add(MakeShared<FJsonValueString>(
                FString::Printf(TEXT("Copy failed: %s -> %s"), *SourceFile, *DestFile)));
        }
    }

    // 5. Build response
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), ErrorMessages.Num() == 0 && CopiedCount > 0);
    Result->SetNumberField(TEXT("initial_count"), InitialPackages.Num());
    Result->SetNumberField(TEXT("total_with_dependencies"), AllPackages.Num());
    Result->SetNumberField(TEXT("copied_count"), CopiedCount);
    Result->SetNumberField(TEXT("skipped_count"), SkippedCount);
    Result->SetStringField(TEXT("destination_root"), DestContentRoot);
    Result->SetBoolField(TEXT("include_dependencies"), bIncludeDeps);
    Result->SetArrayField(TEXT("errors"), ErrorMessages);
    return Result;
}


// ─── Sprint 2 — asset import ──────────────────────────────────────────────────
//
// Single generic import tool. UE's UAssetImportTask + IAssetTools::ImportAssetTasks
// auto-detects the file type from extension and selects the appropriate factory
// (FBX → mesh/skeletal/anim, .png/.tga/.psd → texture, .wav/.mp3 → sound, etc.).
// Specialized variants per-type would be duplication; if FBX-specific options
// like LOD / material import behavior become needed, add an optional
// `import_options` JSON struct rather than splitting the tool.

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("file_path"), FilePath) || FilePath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'file_path' parameter (absolute filesystem path to the source file)"));
    }

    FString DestinationPath;
    if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath) || DestinationPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'destination_path' parameter (/Game/-prefixed package path)"));
    }

    bool bReplaceExisting = true;
    Params->TryGetBoolField(TEXT("replace_existing"), bReplaceExisting);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    if (!FPaths::FileExists(FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Source file does not exist: %s"), *FilePath));
    }

    // Build the import task. UE will choose the factory based on extension.
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    Task->Filename = FilePath;
    Task->DestinationPath = DestinationPath;
    Task->bAutomated = true;             // no modal UI prompts
    Task->bSave = bSave;
    Task->bReplaceExisting = bReplaceExisting;
    Task->bReplaceExistingSettings = bReplaceExisting;

    // Pin the task in the GC root so it survives until ImportAssetTasks completes
    Task->AddToRoot();

    TArray<UAssetImportTask*> Tasks;
    Tasks.Add(Task);

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    AssetToolsModule.Get().ImportAssetTasks(Tasks);

    // Collect the imported object paths
    TArray<TSharedPtr<FJsonValue>> ImportedObjects;
    for (const FString& Path : Task->ImportedObjectPaths)
    {
        ImportedObjects.Add(MakeShared<FJsonValueString>(Path));
    }
    const int32 ImportedCount = ImportedObjects.Num();

    Task->RemoveFromRoot();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("file_path"), FilePath);
    Result->SetStringField(TEXT("destination_path"), DestinationPath);
    Result->SetArrayField(TEXT("imported_object_paths"), ImportedObjects);
    Result->SetNumberField(TEXT("imported_count"), ImportedCount);
    Result->SetBoolField(TEXT("success"), ImportedCount > 0);
    if (ImportedCount == 0)
    {
        Result->SetStringField(TEXT("note"),
            TEXT("ImportAssetTasks returned 0 imported objects. Typical causes: "
                 "unsupported file extension (no factory registered), source file "
                 "corrupted, destination_path already has an asset and bReplaceExisting=false, "
                 "or the import factory raised an error (check editor log)."));
    }
    return Result;
}


// ─── v0.7.3 — finalize_migration ─────────────────────────────────────────────
//
// Companion to migrate_assets. After cross-project copy, the migrated .uasset
// files contain serialized hard refs to their original /Game/-relative paths
// (e.g. SM_ChapelStructure references /Game/Masters/01_Masters/M_StandardMaster).
// If migrate_assets put them under a subfolder like /Game/Migrated/, those
// refs don't resolve in the destination project — materials show as
// checkerboard defaults.
//
// This tool fixes the existing broken state by batch-renaming every asset
// under `migrated_root` (e.g. "/Game/Migrated") to `target_root` (default
// "/Game"), stripping the offending subfolder. UE's FAssetRenameManager
// handles the heavy lifting: file moves on disk, ref updates across the
// entire content tree (mesh → MI → master chains), level-actor ref updates,
// and redirector creation at the old paths for any external references.
//
// One call, all paths fixed. Idempotent (re-running on already-renamed
// content is a no-op).

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleFinalizeMigration(const TSharedPtr<FJsonObject>& Params)
{
    FString MigratedRoot;
    if (!Params->TryGetStringField(TEXT("migrated_root"), MigratedRoot) || MigratedRoot.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'migrated_root' parameter (e.g. '/Game/Migrated')"));
    }
    FString TargetRoot = TEXT("/Game");
    Params->TryGetStringField(TEXT("target_root"), TargetRoot);
    if (TargetRoot.IsEmpty())
    {
        TargetRoot = TEXT("/Game");
    }

    // Normalize: drop trailing slashes so we can do clean string-replace operations
    while (MigratedRoot.EndsWith(TEXT("/")))
    {
        MigratedRoot.LeftChopInline(1, EAllowShrinking::No);
    }
    while (TargetRoot.EndsWith(TEXT("/")))
    {
        TargetRoot.LeftChopInline(1, EAllowShrinking::No);
    }

    // Safety: both must start with /Game/ to avoid going outside content tree
    if (!MigratedRoot.StartsWith(TEXT("/Game")) || !TargetRoot.StartsWith(TEXT("/Game")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Both 'migrated_root' and 'target_root' must be /Game/-rooted paths"));
    }
    if (MigratedRoot.Equals(TargetRoot))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("'migrated_root' equals 'target_root' — nothing to do"));
    }

    IAssetRegistry* Registry = GetRegistry();
    if (!Registry)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetRegistry module unavailable"));
    }

    // Enumerate every asset that currently sits under migrated_root.
    // bSearchSubClasses=true is the default; we explicitly want recursive=true
    // so nested folders (Masters/01_Masters/BaseTextures/...) are included.
    TArray<FAssetData> Assets;
    Registry->GetAssetsByPath(FName(*MigratedRoot), Assets, /*bRecursive=*/ true);

    if (Assets.Num() == 0)
    {
        TSharedPtr<FJsonObject> Empty = MakeShared<FJsonObject>();
        Empty->SetBoolField(TEXT("success"), true);
        Empty->SetNumberField(TEXT("renamed_count"), 0);
        Empty->SetStringField(TEXT("note"),
            FString::Printf(TEXT("No assets found under %s — nothing to finalize"), *MigratedRoot));
        return Empty;
    }

    // Build the rename batch. Each entry maps the asset from its current
    // package path under migrated_root to the equivalent path under target_root.
    TArray<FAssetRenameData> RenameList;
    RenameList.Reserve(Assets.Num());

    for (const FAssetData& AssetData : Assets)
    {
        const FString OldPackageName = AssetData.PackageName.ToString();      // e.g. /Game/Migrated/Masters/01_Masters/M_StandardMaster
        const FString OldPackagePath = AssetData.PackagePath.ToString();      // e.g. /Game/Migrated/Masters/01_Masters

        // Map old → new by string substitution at the front of the path.
        FString NewPackagePath = OldPackagePath;
        if (!NewPackagePath.StartsWith(MigratedRoot))
        {
            // Asset registry returned something outside our root — shouldn't
            // happen given GetAssetsByPath semantics, but skip defensively.
            continue;
        }
        NewPackagePath.RemoveFromStart(MigratedRoot);
        NewPackagePath = TargetRoot + NewPackagePath;

        // GetAsset() forces a load; required because FAssetRenameManager
        // operates on already-loaded UObjects (it rewrites refs on UObject
        // graphs, not raw .uasset bytes).
        UObject* Asset = AssetData.GetAsset();
        if (!Asset)
        {
            continue;
        }

        FAssetRenameData Data;
        Data.Asset = Asset;
        Data.OldObjectPath = AssetData.GetSoftObjectPath().ToString();
        Data.NewPackagePath = NewPackagePath;
        Data.NewName = AssetData.AssetName.ToString();
        RenameList.Add(MoveTemp(Data));
    }

    if (RenameList.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("No assets could be loaded for rename. Possible causes: "
                 "asset registry stale (try AssetRegistry::ScanFilesSynchronous), "
                 "or all assets failed to load."));
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    IAssetTools& AssetTools = AssetToolsModule.Get();

    // RenameAssets does the whole job: file move, ref updates across the
    // content tree (including level actor refs), redirector creation at the
    // old paths. UE 5.7 returns bool (true=success, false=failure); older
    // versions returned an EAssetRenameResult enum.
    const bool bSuccess = AssetTools.RenameAssets(RenameList);

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), bSuccess);
    Out->SetNumberField(TEXT("renamed_count"), RenameList.Num());
    Out->SetNumberField(TEXT("scanned_count"), Assets.Num());
    Out->SetStringField(TEXT("migrated_root"), MigratedRoot);
    Out->SetStringField(TEXT("target_root"), TargetRoot);
    Out->SetStringField(TEXT("result"), bSuccess ? TEXT("Success") : TEXT("Failure"));
    if (!bSuccess)
    {
        Out->SetStringField(TEXT("note"),
            TEXT("RenameAssets returned false. Common causes: "
                 "(a) one or more assets are referenced by a C++ ConstructorHelpers path "
                 "(CDO-pinned — see feedback_unreal_rename_asset_cdo_pin.md); "
                 "(b) destination path conflict (an asset already exists at the new path); "
                 "(c) checkout failed for source-controlled content. "
                 "Check the editor log for per-asset error details. Partial success "
                 "is possible — some assets may have moved successfully even when the "
                 "overall call returned false."));
    }
    return Out;
}


// ─── v0.8.0 Day 3-4 — cooperative workflows: point user at our work ──────────
//
// When the agent does something in the Content Browser, the human user often
// wants to see what happened (verify before, after, or as it goes). These
// three tools surface our work — no state change, just a viewport sync.
//
//   focus_in_browser(asset_path)     — scrolls to + highlights the asset
//   navigate_to_folder(folder_path)  — pivots the Content Browser to a folder
//   open_in_editor(asset_path)       — opens the asset's dedicated editor
//                                       (material editor, BP editor, etc.)
//
// Path conventions match list_assets: "/Game/Lauder/Materials/M_Stone" (no
// .Asset suffix needed — we tolerate both with and without).

namespace
{
    // Strip a trailing ".Name" object suffix and re-resolve. UE returns full
    // ObjectPath strings ("/Game/Foo.Foo") from list_assets but folder operations
    // want just the package path ("/Game/Foo"). LoadObject handles both, but
    // this helper makes intent explicit for callers that pass either.
    UObject* LoadAssetByPath(const FString& AssetPath)
    {
        if (AssetPath.IsEmpty()) return nullptr;
        UObject* Loaded = LoadObject<UObject>(nullptr, *AssetPath);
        if (Loaded) return Loaded;

        // Fallback: caller passed package path without object name.
        int32 LastSlash = INDEX_NONE;
        AssetPath.FindLastChar(TEXT('/'), LastSlash);
        if (LastSlash == INDEX_NONE) return nullptr;
        const FString LeafName = AssetPath.Mid(LastSlash + 1);
        const FString FullPath = FString::Printf(TEXT("%s.%s"), *AssetPath, *LeafName);
        return LoadObject<UObject>(nullptr, *FullPath);
    }
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleFocusInBrowser(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UObject* Asset = LoadAssetByPath(AssetPath);
    if (!Asset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    FContentBrowserModule& CBM = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    TArray<UObject*> Assets = { Asset };
    CBM.Get().SyncBrowserToAssets(Assets);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), Asset->GetPathName());
    Result->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleNavigateToFolder(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("folder_path"), FolderPath) || FolderPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'folder_path' parameter"));
    }

    // Normalize: caller may pass "/Game/Lauder" or just "/Lauder" — the
    // Content Browser wants a virtual-tree path starting with /Game/ or
    // similar mount root. Bare names get a /Game/ prefix.
    if (!FolderPath.StartsWith(TEXT("/")))
    {
        FolderPath = FString::Printf(TEXT("/Game/%s"), *FolderPath);
    }

    FContentBrowserModule& CBM = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    TArray<FString> Folders = { FolderPath };
    CBM.Get().SyncBrowserToFolders(Folders);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("folder_path"), FolderPath);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleOpenInEditor(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UObject* Asset = LoadAssetByPath(AssetPath);
    if (!Asset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
    }

    UAssetEditorSubsystem* Subsys = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (!Subsys)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("UAssetEditorSubsystem unavailable"));
    }
    const bool bOpened = Subsys->OpenEditorForAsset(Asset);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), Asset->GetPathName());
    Result->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
    Result->SetBoolField(TEXT("opened"), bOpened);
    Result->SetBoolField(TEXT("success"), bOpened);
    if (!bOpened)
    {
        Result->SetStringField(TEXT("error"),
            FString::Printf(TEXT("No registered editor for class %s"), *Asset->GetClass()->GetName()));
    }
    return Result;
}


// ─── v0.8.0 Day 3-4 — static-mesh bounds + slot inspection ──────────────────
//
// Before placing a Megascans asset at scale we need to know how big it is
// and what material slots it exposes. Eyeballing from screenshots is wrong
// for sub-meter and over-100m assets. This returns the local-space bounding
// box (extent + center), the world-space scaled version derivable from a
// caller-supplied scale, and the material slot list with current default
// assignments.

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleStaticMeshGetInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UObject* Asset = LoadAssetByPath(AssetPath);
    UStaticMesh* Mesh = Cast<UStaticMesh>(Asset);
    if (!Mesh)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset is not a UStaticMesh: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_path"), Mesh->GetPathName());

    // Bounds — local-space, before any actor scale.
    const FBox Box = Mesh->GetBoundingBox();
    auto MakeVecArray = [](const FVector& V) {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(V.X));
        Arr.Add(MakeShared<FJsonValueNumber>(V.Y));
        Arr.Add(MakeShared<FJsonValueNumber>(V.Z));
        return Arr;
    };
    const FVector Center = Box.GetCenter();
    const FVector Extent = Box.GetExtent();  // half-size

    TSharedPtr<FJsonObject> BoundsObj = MakeShared<FJsonObject>();
    BoundsObj->SetArrayField(TEXT("center"), MakeVecArray(Center));
    BoundsObj->SetArrayField(TEXT("extent"), MakeVecArray(Extent));         // half-size
    BoundsObj->SetArrayField(TEXT("size"), MakeVecArray(Extent * 2.0f));    // full size
    BoundsObj->SetArrayField(TEXT("min"), MakeVecArray(Box.Min));
    BoundsObj->SetArrayField(TEXT("max"), MakeVecArray(Box.Max));
    Result->SetObjectField(TEXT("bounds"), BoundsObj);

    // Material slots — slot name + current default material asset path (if any).
    TArray<TSharedPtr<FJsonValue>> SlotsJson;
    const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
    SlotsJson.Reserve(Materials.Num());
    for (int32 i = 0; i < Materials.Num(); ++i)
    {
        const FStaticMaterial& Slot = Materials[i];
        TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
        SlotObj->SetNumberField(TEXT("index"), i);
        SlotObj->SetStringField(TEXT("slot_name"), Slot.MaterialSlotName.ToString());
        SlotObj->SetStringField(TEXT("material"),
            Slot.MaterialInterface ? Slot.MaterialInterface->GetPathName() : FString());
        SlotsJson.Add(MakeShared<FJsonValueObject>(SlotObj));
    }
    Result->SetArrayField(TEXT("material_slots"), SlotsJson);
    Result->SetNumberField(TEXT("slot_count"), SlotsJson.Num());

    // LOD count — cheap, sometimes needed for "is this a high-poly hero mesh?"
    Result->SetNumberField(TEXT("lod_count"), Mesh->GetNumLODs());

    Result->SetBoolField(TEXT("success"), true);
    return Result;
}
