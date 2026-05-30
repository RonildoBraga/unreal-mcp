#include "Commands/UnrealMCPAssetCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "EditorAssetLibrary.h"
#include "UObject/Class.h"
#include "UObject/TopLevelAssetPath.h"
#include "Misc/PackageName.h"

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
