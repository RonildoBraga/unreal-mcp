// v0.8.x §6.2 completion -- material command handlers, lifted out of the
// v0.7-era FUnrealMCPMaterialCommands class.

#include "Commands/UnrealMCPCommonUtils.h"
#include "MCPRegistry.h"

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

/** Load a material or material instance from a /Game/ object path. */
UMaterialInterface* LoadMaterialInterface(const FString& AssetPath)
{
    return Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(AssetPath));
}

/** Load specifically a material instance (rejects base materials). */
UMaterialInstanceConstant* LoadMaterialInstanceConstant(const FString& AssetPath)
{
    return Cast<UMaterialInstanceConstant>(UEditorAssetLibrary::LoadAsset(AssetPath));
}

// Material-prefixed name to avoid an ODR collision in non-unity builds
// (AssetCommands.cpp has its own GetRegistry()).
IAssetRegistry* GetAssetRegistryForMaterials()
{
    FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    return &Module.Get();
}


TSharedPtr<FJsonObject> HandleGetMaterialParameters(const TSharedPtr<FJsonObject>& Params)
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

    TArray<TSharedPtr<FJsonValue>> Scalars;
    TArray<TSharedPtr<FJsonValue>> Vectors;
    TArray<TSharedPtr<FJsonValue>> Textures;

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


TSharedPtr<FJsonObject> HandleSetMaterialInstanceParam(const TSharedPtr<FJsonObject>& Params)
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


TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
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

    const FString TargetPackagePath = FPackageName::GetLongPackagePath(TargetPath);
    const FString TargetName = FPackageName::GetShortName(TargetPath);

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


TSharedPtr<FJsonObject> HandleGetMaterialUses(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath) || MaterialPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }

    IAssetRegistry* Registry = GetAssetRegistryForMaterials();
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


TSharedPtr<FJsonObject> HandleListMaterialInstancesOfParent(const TSharedPtr<FJsonObject>& Params)
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

    IAssetRegistry* Registry = GetAssetRegistryForMaterials();
    if (!Registry)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetRegistry module unavailable"));
    }

    FARFilter Filter;
    Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.PackagePaths.Add(FName(*SearchPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetData;
    Registry->GetAssets(Filter, AssetData);

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

}  // anonymous namespace


REGISTER_MCP_COMMAND("get_material_parameters",          &HandleGetMaterialParameters);
REGISTER_MCP_COMMAND("set_material_instance_param",      &HandleSetMaterialInstanceParam);
REGISTER_MCP_COMMAND("create_material_instance",         &HandleCreateMaterialInstance);
REGISTER_MCP_COMMAND("get_material_uses",                &HandleGetMaterialUses);
REGISTER_MCP_COMMAND("list_material_instances_of_parent", &HandleListMaterialInstancesOfParent);
