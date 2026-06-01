// v0.8.x §6.2 completion -- level command handlers, lifted out of the
// v0.7-era FUnrealMCPLevelCommands class. Free functions in anonymous
// namespace + REGISTER_MCP_COMMAND self-registration at definition site.

#include "Commands/UnrealMCPCommonUtils.h"
#include "MCPRegistry.h"

#include "Editor.h"
#include "LevelEditorSubsystem.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

// v0.9.x — create_landscape support. ALandscape::Import is on ALandscapeProxy;
// FLandscapeImportLayerInfo + ELandscapeImportAlphamapType are in LandscapeProxy.h too.
#include "Landscape.h"
#include "LandscapeProxy.h"

namespace
{

/** Resolve the level editor subsystem. Returns nullptr if not available. */
ULevelEditorSubsystem* LevelSub()
{
    return GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
}


TSharedPtr<FJsonObject> HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    // World's package name is the /Game/-prefixed path. Object path is package + leaf.
    const FString PackageName = World->GetPackage() ? World->GetPackage()->GetName() : FString();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), World->GetName());
    Result->SetStringField(TEXT("package_name"), PackageName);
    if (!PackageName.IsEmpty())
    {
        Result->SetStringField(TEXT("object_path"), FString::Printf(TEXT("%s.%s"), *PackageName, *World->GetName()));
    }
    Result->SetStringField(TEXT("map_name"), World->GetMapName());
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}


TSharedPtr<FJsonObject> HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params->TryGetStringField(TEXT("level_path"), LevelPath) || LevelPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'level_path' parameter"));
    }

    ULevelEditorSubsystem* Sub = LevelSub();
    if (!Sub)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelEditorSubsystem unavailable"));
    }

    // LoadLevel accepts /Game/-prefixed package path. Object-path form ("...L_Base.L_Base")
    // also works — strip the trailing object suffix if present so callers can use either form.
    if (LevelPath.Contains(TEXT(".")))
    {
        LevelPath = FPackageName::ObjectPathToPackageName(LevelPath);
    }

    const bool bSuccess = Sub->LoadLevel(LevelPath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("level_path"), LevelPath);
    Result->SetBoolField(TEXT("success"), bSuccess);
    if (!bSuccess)
    {
        Result->SetStringField(TEXT("note"),
            TEXT("LoadLevel returned False — typical causes: path doesn't exist, "
                 "level has unsaved changes the user must address first, or the level "
                 "depends on a sublevel that failed to load."));
    }
    return Result;
}


TSharedPtr<FJsonObject> HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    ULevelEditorSubsystem* Sub = LevelSub();
    if (!Sub)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelEditorSubsystem unavailable"));
    }

    const bool bSuccess = Sub->SaveCurrentLevel();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    return Result;
}


/**
 * Create a new ALandscape actor with proper components and initial heightmap.
 *
 * Why this exists: UE's Python bindings don't expose Landscape construction.
 * spawn_actor_from_class(unreal.Landscape) returns a `LandscapePlaceholder`
 * stub — an explicit "not from script" marker. ALandscape::Import is C++ only.
 *
 * Params (all optional with defaults):
 *   location               : Vector  (default 0,0,0)
 *   scale                  : Vector  (default 100,100,100 → 1 quad = 1 m)
 *   label                  : string  (default "Landscape")
 *   component_count_x      : int     (default 4)   — components along X
 *   component_count_y      : int     (default 4)   — components along Y
 *   sections_per_component : int     (default 2)   — 1 or 2 (UE constraint)
 *   quads_per_section      : int     (default 63)  — one of 7/15/31/63/127/255
 *
 * Total quads per side = components * sections_per_component * quads_per_section.
 * With defaults: 4 * 2 * 63 = 504 quads per side → 505 × 505 verts → ~504 m square at scale 100.
 *
 * Initial heightmap is uniform 32768 (the uint16 midpoint = z=0 in landscape space).
 * No material layers are imported — the landscape uses the default material until
 * a separate set_landscape_material call wires one. That keeps this tool focused.
 */
TSharedPtr<FJsonObject> HandleCreateLandscape(const TSharedPtr<FJsonObject>& Params)
{
    // --- parse params ---
    FVector Location = FVector::ZeroVector;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }

    FVector Scale(100.0, 100.0, 100.0);
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    FString Label = TEXT("Landscape");
    Params->TryGetStringField(TEXT("label"), Label);

    int32 ComponentCountX = 4;
    int32 ComponentCountY = 4;
    int32 SectionsPerComponent = 2;
    int32 QuadsPerSection = 63;
    Params->TryGetNumberField(TEXT("component_count_x"), ComponentCountX);
    Params->TryGetNumberField(TEXT("component_count_y"), ComponentCountY);
    Params->TryGetNumberField(TEXT("sections_per_component"), SectionsPerComponent);
    Params->TryGetNumberField(TEXT("quads_per_section"), QuadsPerSection);

    // --- validate ---
    if (ComponentCountX < 1 || ComponentCountY < 1)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("component_count_x and component_count_y must be >= 1"));
    }
    if (SectionsPerComponent != 1 && SectionsPerComponent != 2)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("sections_per_component must be 1 or 2 (UE Landscape constraint)"));
    }
    // UE Landscape allows section sizes that are 2^n - 1 within {7, 15, 31, 63, 127, 255}.
    const TArray<int32> ValidQuadSizes = {7, 15, 31, 63, 127, 255};
    if (!ValidQuadSizes.Contains(QuadsPerSection))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("quads_per_section must be one of 7, 15, 31, 63, 127, 255 "
                 "(UE Landscape constraint — 63 is the recommended default)"));
    }

    // --- compute the vertex grid ---
    const int32 TotalQuadsX = ComponentCountX * SectionsPerComponent * QuadsPerSection;
    const int32 TotalQuadsY = ComponentCountY * SectionsPerComponent * QuadsPerSection;
    const int32 TotalVertsX = TotalQuadsX + 1;
    const int32 TotalVertsY = TotalQuadsY + 1;

    // --- world ---
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    // --- spawn the ALandscape actor (NOT LandscapePlaceholder — we're in C++) ---
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ALandscape* Landscape = World->SpawnActor<ALandscape>(
        ALandscape::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
    if (!Landscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("World->SpawnActor<ALandscape> returned null"));
    }
    Landscape->SetActorScale3D(Scale);
    Landscape->SetActorLabel(Label);

    // --- build a flat heightmap. 32768 = uint16 midpoint = world-Z 0 in landscape space ---
    TArray<uint16> HeightData;
    HeightData.Init(32768, TotalVertsX * TotalVertsY);

    // The map key is an FGuid for the landscape edit layer; default GUID is the base layer.
    TMap<FGuid, TArray<uint16>> HeightDataPerLayer;
    HeightDataPerLayer.Add(FGuid(), MoveTemp(HeightData));

    // --- import: this is the C++-only call that initialises components + heightmap ---
    // ALandscapeProxy::Import asserts that HeightData map size == MaterialLayerInfos map size
    // (one entry per edit layer). Since we added one base-layer entry above, mirror it here
    // with an empty material-layer list (no painted layers yet).
    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerInfosPerLayer;
    MaterialLayerInfosPerLayer.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
    // UE 5.7 added a required 12th arg — InImportLayers (edit-layer descriptors).
    // Empty array = no extra edit layers, just the base.
    TArray<FLandscapeLayer> ImportLayers;
    Landscape->Import(
        FGuid::NewGuid(),
        /*MinX*/ 0, /*MinY*/ 0,
        /*MaxX*/ TotalQuadsX, /*MaxY*/ TotalQuadsY,
        SectionsPerComponent,
        QuadsPerSection,
        HeightDataPerLayer,
        /*HeightmapFile*/ nullptr,
        MaterialLayerInfosPerLayer,
        ELandscapeImportAlphamapType::Additive,
        TArrayView<const FLandscapeLayer>(ImportLayers)
    );

    // --- build response ---
    TSharedPtr<FJsonObject> Data = FUnrealMCPCommonUtils::ActorToJsonObject(Landscape);
    Data->SetNumberField(TEXT("component_count_x"),      ComponentCountX);
    Data->SetNumberField(TEXT("component_count_y"),      ComponentCountY);
    Data->SetNumberField(TEXT("sections_per_component"), SectionsPerComponent);
    Data->SetNumberField(TEXT("quads_per_section"),      QuadsPerSection);
    Data->SetNumberField(TEXT("total_verts_x"),          TotalVertsX);
    Data->SetNumberField(TEXT("total_verts_y"),          TotalVertsY);
    Data->SetNumberField(TEXT("total_components"),       ComponentCountX * ComponentCountY);
    return FUnrealMCPCommonUtils::CreateSuccessResponse(Data);
}

}  // anonymous namespace


REGISTER_MCP_COMMAND("get_current_level",   &HandleGetCurrentLevel);
REGISTER_MCP_COMMAND("open_level",          &HandleOpenLevel);
REGISTER_MCP_COMMAND("save_current_level",  &HandleSaveCurrentLevel);
REGISTER_MCP_COMMAND("create_landscape",    &HandleCreateLandscape);
