#include "Commands/UnrealMCPMaterialCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditingLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "UObject/Class.h"
#include "UObject/TopLevelAssetPath.h"
#include "Misc/PackageName.h"

namespace
{
    /** Load a material or material instance from a /Game/ object path. Returns nullptr on failure. */
    UMaterialInterface* LoadMaterialInterface(const FString& AssetPath)
    {
        return Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(AssetPath));
    }

    /** Load specifically a material instance (rejects base materials). */
    UMaterialInstanceConstant* LoadMaterialInstanceConstant(const FString& AssetPath)
    {
        return Cast<UMaterialInstanceConstant>(UEditorAssetLibrary::LoadAsset(AssetPath));
    }

    IAssetRegistry* GetRegistry()
    {
        FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        return &Module.Get();
    }
}


FUnrealMCPMaterialCommands::FUnrealMCPMaterialCommands()
{
}


TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_material_parameters"))           return HandleGetMaterialParameters(Params);
    if (CommandType == TEXT("set_material_instance_param"))        return HandleSetMaterialInstanceParam(Params);
    if (CommandType == TEXT("create_material_instance"))           return HandleCreateMaterialInstance(Params);
    if (CommandType == TEXT("get_material_uses"))                  return HandleGetMaterialUses(Params);
    if (CommandType == TEXT("list_material_instances_of_parent"))  return HandleListMaterialInstancesOfParent(Params);

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}


TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetMaterialParameters(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath) || MaterialPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    UMaterialInterface* Material = LoadMaterialInterface(MaterialPath);
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load material at: %s"), *MaterialPath));
    }

    // Build arrays per parameter type. Same shape regardless of whether the
    // source is a base UMaterial (returns default values) or a UMaterialInstance
    // (returns current values, which may override the base).
    TArray<TSharedPtr<FJsonValue>> Scalars;
    TArray<TSharedPtr<FJsonValue>> Vectors;
    TArray<TSharedPtr<FJsonValue>> Textures;

    // Scalar parameters
    TArray<FMaterialParameterInfo> ScalarInfo;
    TArray<FGuid> ScalarGuids;
    Material->GetAllScalarParameterInfo(ScalarInfo, ScalarGuids);
    for (const FMaterialParameterInfo& Info : ScalarInfo)
    {
        float Value = 0.0f;
        Material->GetScalarParameterValue(Info, Value);

        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Info.Name.ToString());
        Entry->SetNumberField(TEXT("value"), Value);
        Scalars.Add(MakeShared<FJsonValueObject>(Entry));
    }

    // Vector parameters
    TArray<FMaterialParameterInfo> VectorInfo;
    TArray<FGuid> VectorGuids;
    Material->GetAllVectorParameterInfo(VectorInfo, VectorGuids);
    for (const FMaterialParameterInfo& Info : VectorInfo)
    {
        FLinearColor Value = FLinearColor::Black;
        Material->GetVectorParameterValue(Info, Value);

        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Info.Name.ToString());
        Entry->SetNumberField(TEXT("r"), Value.R);
        Entry->SetNumberField(TEXT("g"), Value.G);
        Entry->SetNumberField(TEXT("b"), Value.B);
        Entry->SetNumberField(TEXT("a"), Value.A);
        Vectors.Add(MakeShared<FJsonValueObject>(Entry));
    }

    // Texture parameters
    TArray<FMaterialParameterInfo> TextureInfo;
    TArray<FGuid> TextureGuids;
    Material->GetAllTextureParameterInfo(TextureInfo, TextureGuids);
    for (const FMaterialParameterInfo& Info : TextureInfo)
    {
        UTexture* Value = nullptr;
        Material->GetTextureParameterValue(Info, Value);

        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Info.Name.ToString());
        Entry->SetStringField(TEXT("texture_path"), Value ? Value->GetPathName() : FString());
        Textures.Add(MakeShared<FJsonValueObject>(Entry));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("material_path"), MaterialPath);
    Result->SetStringField(TEXT("class_name"), Material->GetClass()->GetName());
    Result->SetArrayField(TEXT("scalar_parameters"), Scalars);
    Result->SetArrayField(TEXT("vector_parameters"), Vectors);
    Result->SetArrayField(TEXT("texture_parameters"), Textures);
    Result->SetNumberField(TEXT("scalar_count"), Scalars.Num());
    Result->SetNumberField(TEXT("vector_count"), Vectors.Num());
    Result->SetNumberField(TEXT("texture_count"), Textures.Num());
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialInstanceParam(const TSharedPtr<FJsonObject>& Params)
{
    FString MIPath, ParamName, ParamType;
    if (!Params->TryGetStringField(TEXT("material_instance_path"), MIPath) || MIPath.IsEmpty() ||
        !Params->TryGetStringField(TEXT("param_name"), ParamName) || ParamName.IsEmpty() ||
        !Params->TryGetStringField(TEXT("param_type"), ParamType) || ParamType.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Need 'material_instance_path', 'param_name', and 'param_type' ('scalar' | 'vector' | 'texture')"));
    }

    UMaterialInstanceConstant* MI = LoadMaterialInstanceConstant(MIPath);
    if (!MI)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load material instance at: %s (must be UMaterialInstanceConstant)"), *MIPath));
    }

    bool bSuccess = false;
    FString TypeLower = ParamType.ToLower();

    if (TypeLower == TEXT("scalar"))
    {
        double NumVal = 0.0;
        if (!Params->TryGetNumberField(TEXT("value"), NumVal))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing numeric 'value' for scalar param"));
        }
        UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MI, FName(*ParamName), static_cast<float>(NumVal));
        bSuccess = true;
    }
    else if (TypeLower == TEXT("vector"))
    {
        const TSharedPtr<FJsonObject>* VecObj = nullptr;
        if (!Params->TryGetObjectField(TEXT("value"), VecObj))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("Missing object 'value' for vector param (need {r, g, b, a})"));
        }
        FLinearColor Color(
            (*VecObj)->GetNumberField(TEXT("r")),
            (*VecObj)->GetNumberField(TEXT("g")),
            (*VecObj)->GetNumberField(TEXT("b")),
            (*VecObj)->GetNumberField(TEXT("a")));
        UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MI, FName(*ParamName), Color);
        bSuccess = true;
    }
    else if (TypeLower == TEXT("texture"))
    {
        FString TexturePath;
        if (!Params->TryGetStringField(TEXT("value"), TexturePath) || TexturePath.IsEmpty())
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("Missing 'value' string (texture asset path) for texture param"));
        }
        UTexture* Tex = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexturePath));
        if (!Tex)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Could not load texture at: %s"), *TexturePath));
        }
        UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MI, FName(*ParamName), Tex);
        bSuccess = true;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown param_type '%s' — expected 'scalar', 'vector', or 'texture'"), *ParamType));
    }

    // Save changes
    if (bSuccess)
    {
        UMaterialEditingLibrary::UpdateMaterialInstance(MI);
        UEditorAssetLibrary::SaveAsset(MIPath, /*bOnlyIfDirty*/ true);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("material_instance_path"), MIPath);
    Result->SetStringField(TEXT("param_name"), ParamName);
    Result->SetStringField(TEXT("param_type"), ParamType);
    Result->SetBoolField(TEXT("success"), bSuccess);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString ParentPath, TargetPath;
    if (!Params->TryGetStringField(TEXT("parent_material_path"), ParentPath) || ParentPath.IsEmpty() ||
        !Params->TryGetStringField(TEXT("target_path"), TargetPath) || TargetPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Need 'parent_material_path' and 'target_path' (both /Game/-prefixed)"));
    }

    UMaterialInterface* Parent = LoadMaterialInterface(ParentPath);
    if (!Parent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load parent material at: %s"), *ParentPath));
    }

    // Split target into package path + name. CreateAsset takes them separately.
    const FString TargetPackagePath = FPackageName::GetLongPackagePath(TargetPath);
    const FString TargetName = FPackageName::GetShortName(TargetPath);

    // Build the factory with the desired parent. Note: UE 5.7's
    // UMaterialEditingLibrary has no CreateMaterialInstanceAsset helper, so we
    // go through the factory + IAssetTools path. The factory holds InitialParent
    // which the resulting MI inherits at construction.
    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Parent;

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    UObject* NewAsset = AssetToolsModule.Get().CreateAsset(
        TargetName,
        TargetPackagePath,
        UMaterialInstanceConstant::StaticClass(),
        Factory);

    UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(NewAsset);
    if (!MI)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("CreateAsset returned null. Target '%s' may already exist or parent is invalid."), *TargetPath));
    }

    // Belt-and-suspenders: in case the factory didn't pick up InitialParent for
    // some reason, set it explicitly. Also handles the case where Parent is
    // itself a UMaterialInstance (chained instances).
    if (Parent != MI->Parent)
    {
        UMaterialEditingLibrary::SetMaterialInstanceParent(MI, Parent);
        UMaterialEditingLibrary::UpdateMaterialInstance(MI);
    }

    UEditorAssetLibrary::SaveAsset(MI->GetPathName(), /*bOnlyIfDirty*/ true);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("parent_material_path"), ParentPath);
    Result->SetStringField(TEXT("target_path"), TargetPath);
    Result->SetStringField(TEXT("created_object_path"), MI->GetPathName());
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetMaterialUses(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath) || MaterialPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    IAssetRegistry* Registry = GetRegistry();
    if (!Registry)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetRegistry module unavailable"));
    }

    FName PackageName(*FPackageName::ObjectPathToPackageName(MaterialPath));

    TArray<FName> Referencers;
    Registry->GetReferencers(PackageName, Referencers);

    TArray<TSharedPtr<FJsonValue>> RefValues;
    for (const FName& Ref : Referencers)
    {
        RefValues.Add(MakeShared<FJsonValueString>(Ref.ToString()));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("material_path"), MaterialPath);
    Result->SetArrayField(TEXT("referencers"), RefValues);
    Result->SetNumberField(TEXT("count"), RefValues.Num());
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleListMaterialInstancesOfParent(const TSharedPtr<FJsonObject>& Params)
{
    FString ParentPath;
    if (!Params->TryGetStringField(TEXT("parent_material_path"), ParentPath) || ParentPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_material_path' parameter"));
    }

    FString SearchPath = TEXT("/Game");
    Params->TryGetStringField(TEXT("search_path"), SearchPath);

    UMaterialInterface* Parent = LoadMaterialInterface(ParentPath);
    if (!Parent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load parent material at: %s"), *ParentPath));
    }

    IAssetRegistry* Registry = GetRegistry();
    if (!Registry)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetRegistry module unavailable"));
    }

    // Find all MaterialInstanceConstant assets under SearchPath
    FARFilter Filter;
    Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.PackagePaths.Add(FName(*SearchPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetData;
    Registry->GetAssets(Filter, AssetData);

    // For each, load it and check parent. We have to actually load to read the
    // parent reference reliably across UE versions.
    TArray<TSharedPtr<FJsonValue>> Matches;
    for (const FAssetData& AD : AssetData)
    {
        UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(AD.GetAsset());
        if (!MI || !MI->Parent) continue;

        if (MI->Parent == Parent)
        {
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), AD.AssetName.ToString());
            Entry->SetStringField(TEXT("object_path"), AD.GetObjectPathString());
            Entry->SetStringField(TEXT("package_path"), AD.PackagePath.ToString());
            Matches.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("parent_material_path"), ParentPath);
    Result->SetStringField(TEXT("search_path"), SearchPath);
    Result->SetArrayField(TEXT("material_instances"), Matches);
    Result->SetNumberField(TEXT("count"), Matches.Num());
    return Result;
}
