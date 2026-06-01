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
// v0.9.x — sculpt_landscape_* support. FLandscapeEditDataInterface is the
// C++ surface UE's brushes use for heightmap writes; LANDSCAPE_INV_ZSCALE is
// the uint16→local-cm factor (128).
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEditLayer.h"     // ULandscapeEditLayerBase::GetGuid for edit-layer writes
#include "LandscapeComponent.h"     // ELandscapeLayerUpdateMode
#include "EngineUtils.h"  // TActorIterator for label lookup

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

// =========================================================================
// Sculpting tools — modify heightmap of an existing landscape.
// All three commands share a small helper layer:
//   - LookupLandscapeByLabel: find the ALandscape with the given outliner label
//   - ApplyHeightTransform:   read the whole heightmap, run a per-vertex
//                             function, write it back through SetHeightData
//   - MetersToUint16Delta:    convert a world-space height delta (meters)
//                             into the uint16 delta the landscape stores.
// =========================================================================

ALandscape* LookupLandscapeByLabel(const FString& Label)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) return nullptr;
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        if (It->GetActorLabel() == Label) return *It;
    }
    return nullptr;
}

/**
 * Read the whole landscape heightmap, run TransformFn(X, Y, CurrentUint16) per
 * vertex, write it back. Returns the number of verts touched, or -1 on error
 * (with reason populated in OutError).
 */
int32 ApplyHeightTransform(ALandscape* Landscape,
                           TFunctionRef<uint16(int32 X, int32 Y, uint16 Current)> TransformFn,
                           FString& OutError)
{
    ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
    if (!Info)
    {
        OutError = TEXT("Landscape has no LandscapeInfo (level may not be registered)");
        return -1;
    }

    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    if (!Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
    {
        OutError = TEXT("Failed to query landscape extent");
        return -1;
    }

    const int32 SizeX = MaxX - MinX + 1;
    const int32 SizeY = MaxY - MinY + 1;

    // The actual read/transform/write, factored out so we can run it inside or
    // outside an edit-layer scope without duplicating the body.
    auto ReadTransformWrite = [&]()
    {
        TArray<uint16> Heights;
        Heights.SetNumUninitialized(SizeX * SizeY);

        // GetHeightData takes X/Y by ref — they get clamped to actual extent. Pass copies.
        FLandscapeEditDataInterface EditIface(Info);
        int32 ReadMinX = MinX, ReadMinY = MinY, ReadMaxX = MaxX, ReadMaxY = MaxY;
        EditIface.GetHeightData(ReadMinX, ReadMinY, ReadMaxX, ReadMaxY, Heights.GetData(), 0);

        for (int32 Y = MinY; Y <= MaxY; ++Y)
        {
            for (int32 X = MinX; X <= MaxX; ++X)
            {
                const int32 Idx = (Y - MinY) * SizeX + (X - MinX);
                Heights[Idx] = TransformFn(X, Y, Heights[Idx]);
            }
        }

        // Write back. bCalcNormals=true so lighting + collision pick up the change.
        EditIface.SetHeightData(MinX, MinY, MaxX, MaxY, Heights.GetData(), 0, /*bCalcNormals*/ true);
    };

    // CRITICAL — edit-layer landscapes recompose their final heightmap from edit
    // layers. A bare FLandscapeEditDataInterface::SetHeightData writes the composed
    // result, which the layer system immediately stomps on the next update — so the
    // sculpt silently vanishes (verts_modified reports success, but the terrain stays
    // flat). When the landscape has layers content, the write MUST target a specific
    // edit layer inside an FScopedSetLandscapeEditingLayer scope. The completion
    // callback forces the heightmap recompose so the change renders. Mirrors the
    // engine's own pattern in LandscapeEdMode.cpp.
    FGuid EditLayerGuid;
    if (Landscape->HasLayersContent())
    {
        const ULandscapeEditLayerBase* EditLayer = Landscape->GetEditLayerConst(0);
        if (!EditLayer)
        {
            // Layers content enabled but no edit layer yet — create the default one
            // (create_landscape's low-level Import doesn't auto-populate it).
            Landscape->CreateDefaultLayer();
            EditLayer = Landscape->GetEditLayerConst(0);
        }
        if (!EditLayer)
        {
            OutError = TEXT("Landscape has edit-layer content but no edit layer could be created");
            return -1;
        }
        EditLayerGuid = EditLayer->GetGuid();
    }

    if (EditLayerGuid.IsValid())
    {
        FScopedSetLandscapeEditingLayer Scope(Landscape, EditLayerGuid,
            [Landscape] { Landscape->RequestLayersContentUpdate(ELandscapeLayerUpdateMode::Update_Heightmap_All); });
        ReadTransformWrite();
    }
    else
    {
        // Non-layered (legacy) landscape — direct write sticks.
        ReadTransformWrite();
    }
    return SizeX * SizeY;
}

/**
 * Convert a world-space height (meters) into the signed uint16 delta that
 * landscape data uses. Landscape heights are uint16 with 32768 = z=0; each
 * step represents LANDSCAPE_ZSCALE units of LOCAL height. Local→world Z is
 * multiplied by the actor's Z scale.
 *
 *     world_z_cm  = (uint16 - 32768) * LANDSCAPE_ZSCALE * actor.ScaleZ
 *     => uint16   = 32768 + world_z_cm * LANDSCAPE_INV_ZSCALE / actor.ScaleZ
 *
 * Returns the delta to add to current uint16. ActorScaleZ is the Z scale of
 * the landscape actor (typically 100 from create_landscape).
 */
float MetersToUint16Delta(float Meters, float ActorScaleZ)
{
    if (ActorScaleZ <= 0.0f) return 0.0f;
    return (Meters * 100.0f * LANDSCAPE_INV_ZSCALE) / ActorScaleZ;
}


/**
 * sculpt_landscape_noise — additively layer fractal Perlin noise onto the
 * existing heightmap. Use this first to get rolling terrain, then call
 * sculpt_landscape_hill to place specific landmarks on top.
 *
 * Params:
 *   label         : string  (required)
 *   scale_meters  : float   (default 80)  wavelength in world units
 *   height_meters : float   (default 8)   max ± height variation
 *   octaves       : int     (default 4)   1-6 (each octave halves amplitude, doubles freq)
 *   seed          : int     (default 42)  noise position offset
 */
TSharedPtr<FJsonObject> HandleSculptLandscapeNoise(const TSharedPtr<FJsonObject>& Params)
{
    FString Label;
    if (!Params->TryGetStringField(TEXT("label"), Label))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'label' parameter"));
    }
    ALandscape* Landscape = LookupLandscapeByLabel(Label);
    if (!Landscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("No Landscape with label '%s'"), *Label));
    }

    double ScaleMeters = 80.0;
    double HeightMeters = 8.0;
    int32 Octaves = 4;
    int32 Seed = 42;
    Params->TryGetNumberField(TEXT("scale_meters"), ScaleMeters);
    Params->TryGetNumberField(TEXT("height_meters"), HeightMeters);
    Params->TryGetNumberField(TEXT("octaves"), Octaves);
    Params->TryGetNumberField(TEXT("seed"), Seed);
    Octaves = FMath::Clamp(Octaves, 1, 6);

    // Quad size in world cm = actor.ScaleX (default 100 → 1 quad = 1 m).
    const FVector ActorScale = Landscape->GetActorScale3D();
    const float QuadSizeCm = FMath::Max(ActorScale.X, 1.0f);
    const float WavelengthQuads = FMath::Max((float)(ScaleMeters * 100.0 / QuadSizeCm), 1.0f);
    const float BaseFreq = 1.0f / WavelengthQuads;

    auto NoiseFn = [&](int32 X, int32 Y, uint16 Current) -> uint16
    {
        // Fractal noise: sum octaves, normalize.
        float Sum = 0.0f;
        float Amplitude = 1.0f;
        float Frequency = BaseFreq;
        float MaxAmp = 0.0f;
        for (int32 i = 0; i < Octaves; ++i)
        {
            const float Px = (X + (float)Seed * 13.7f) * Frequency;
            const float Py = (Y + (float)Seed * 7.3f) * Frequency;
            Sum += FMath::PerlinNoise2D(FVector2D(Px, Py)) * Amplitude;
            MaxAmp += Amplitude;
            Amplitude *= 0.5f;
            Frequency *= 2.0f;
        }
        const float Normalized = (MaxAmp > 0.0f) ? (Sum / MaxAmp) : 0.0f;  // ~[-1, 1]

        const float DeltaUint16 = Normalized * (float)HeightMeters
            * LANDSCAPE_INV_ZSCALE * 100.0f / FMath::Max(ActorScale.Z, 0.001f);
        const int32 NewVal = FMath::Clamp(FMath::RoundToInt((float)Current + DeltaUint16), 0, 65535);
        return (uint16)NewVal;
    };

    FString Err;
    const int32 Touched = ApplyHeightTransform(Landscape, NoiseFn, Err);
    if (Touched < 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Err);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("label"), Label);
    Data->SetNumberField(TEXT("verts_modified"), Touched);
    Data->SetNumberField(TEXT("scale_meters"), ScaleMeters);
    Data->SetNumberField(TEXT("height_meters"), HeightMeters);
    Data->SetNumberField(TEXT("octaves"), Octaves);
    Data->SetNumberField(TEXT("seed"), Seed);
    return FUnrealMCPCommonUtils::CreateSuccessResponse(Data);
}


/**
 * sculpt_landscape_hill — add a gaussian peak (positive peak_meters) or
 * depression (negative) centered at a world XY coord. Falloff is a smooth
 * gaussian — at distance == radius, contribution is ~37% of peak; at 2×radius,
 * ~1.8%. Cap the influence at 3×radius (where contribution is negligible).
 *
 * Params:
 *   label         : string  (required)
 *   center        : Vector  {x, y, z?} — world cm; z is ignored
 *   radius_meters : float   (default 30) gaussian half-width
 *   peak_meters   : float   (default 10) height at center (negative = pit)
 */
TSharedPtr<FJsonObject> HandleSculptLandscapeHill(const TSharedPtr<FJsonObject>& Params)
{
    FString Label;
    if (!Params->TryGetStringField(TEXT("label"), Label))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'label' parameter"));
    }
    ALandscape* Landscape = LookupLandscapeByLabel(Label);
    if (!Landscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("No Landscape with label '%s'"), *Label));
    }

    FVector Center = FVector::ZeroVector;
    if (Params->HasField(TEXT("center")))
    {
        Center = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("center"));
    }

    double RadiusMeters = 30.0;
    double PeakMeters = 10.0;
    Params->TryGetNumberField(TEXT("radius_meters"), RadiusMeters);
    Params->TryGetNumberField(TEXT("peak_meters"), PeakMeters);

    const FVector ActorScale = Landscape->GetActorScale3D();
    const FVector ActorLoc = Landscape->GetActorLocation();
    const float QuadSizeCmX = FMath::Max(ActorScale.X, 1.0f);
    const float QuadSizeCmY = FMath::Max(ActorScale.Y, 1.0f);

    // Center in landscape-local quad space.
    const float CenterQuadX = (Center.X - ActorLoc.X) / QuadSizeCmX;
    const float CenterQuadY = (Center.Y - ActorLoc.Y) / QuadSizeCmY;
    const float RadiusQuads = (float)(RadiusMeters * 100.0 / QuadSizeCmX);
    const float SigmaSq = RadiusQuads * RadiusQuads;
    const float MaxInfluenceSq = (3.0f * RadiusQuads) * (3.0f * RadiusQuads);

    auto HillFn = [&](int32 X, int32 Y, uint16 Current) -> uint16
    {
        const float DX = (float)X - CenterQuadX;
        const float DY = (float)Y - CenterQuadY;
        const float DistSq = DX * DX + DY * DY;
        if (DistSq > MaxInfluenceSq) return Current;

        // gaussian: exp(-d² / (2*sigma²))
        const float Influence = FMath::Exp(-DistSq / (2.0f * SigmaSq));
        const float DeltaUint16 = Influence * (float)PeakMeters
            * LANDSCAPE_INV_ZSCALE * 100.0f / FMath::Max(ActorScale.Z, 0.001f);
        const int32 NewVal = FMath::Clamp(FMath::RoundToInt((float)Current + DeltaUint16), 0, 65535);
        return (uint16)NewVal;
    };

    FString Err;
    const int32 Touched = ApplyHeightTransform(Landscape, HillFn, Err);
    if (Touched < 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Err);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("label"), Label);
    Data->SetNumberField(TEXT("verts_modified"), Touched);
    TSharedPtr<FJsonObject> CenterObj = MakeShared<FJsonObject>();
    CenterObj->SetNumberField(TEXT("x"), Center.X);
    CenterObj->SetNumberField(TEXT("y"), Center.Y);
    Data->SetObjectField(TEXT("center"), CenterObj);
    Data->SetNumberField(TEXT("radius_meters"), RadiusMeters);
    Data->SetNumberField(TEXT("peak_meters"), PeakMeters);
    return FUnrealMCPCommonUtils::CreateSuccessResponse(Data);
}


/**
 * flatten_landscape — set every vertex to a uniform height (default z=0).
 * Useful for "undo" before redoing a procedural pass, or to reset a region.
 *
 * Params:
 *   label    : string  (required)
 *   z_meters : float   (default 0)  target world Z in meters
 */
TSharedPtr<FJsonObject> HandleFlattenLandscape(const TSharedPtr<FJsonObject>& Params)
{
    FString Label;
    if (!Params->TryGetStringField(TEXT("label"), Label))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'label' parameter"));
    }
    ALandscape* Landscape = LookupLandscapeByLabel(Label);
    if (!Landscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("No Landscape with label '%s'"), *Label));
    }

    double ZMeters = 0.0;
    Params->TryGetNumberField(TEXT("z_meters"), ZMeters);

    const FVector ActorScale = Landscape->GetActorScale3D();
    const float TargetUint16Float = 32768.0f
        + (float)ZMeters * 100.0f * LANDSCAPE_INV_ZSCALE / FMath::Max(ActorScale.Z, 0.001f);
    const uint16 TargetUint16 = (uint16)FMath::Clamp(FMath::RoundToInt(TargetUint16Float), 0, 65535);

    auto FlattenFn = [TargetUint16](int32 X, int32 Y, uint16 Current) -> uint16
    {
        return TargetUint16;
    };

    FString Err;
    const int32 Touched = ApplyHeightTransform(Landscape, FlattenFn, Err);
    if (Touched < 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Err);
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("label"), Label);
    Data->SetNumberField(TEXT("verts_modified"), Touched);
    Data->SetNumberField(TEXT("z_meters"), ZMeters);
    return FUnrealMCPCommonUtils::CreateSuccessResponse(Data);
}

}  // anonymous namespace


REGISTER_MCP_COMMAND("get_current_level",        &HandleGetCurrentLevel);
REGISTER_MCP_COMMAND("open_level",               &HandleOpenLevel);
REGISTER_MCP_COMMAND("save_current_level",       &HandleSaveCurrentLevel);
REGISTER_MCP_COMMAND("create_landscape",         &HandleCreateLandscape);
REGISTER_MCP_COMMAND("sculpt_landscape_noise",   &HandleSculptLandscapeNoise);
REGISTER_MCP_COMMAND("sculpt_landscape_hill",    &HandleSculptLandscapeHill);
REGISTER_MCP_COMMAND("flatten_landscape",        &HandleFlattenLandscape);
