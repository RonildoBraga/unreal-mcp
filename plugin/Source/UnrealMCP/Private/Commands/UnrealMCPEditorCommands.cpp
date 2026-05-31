#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Containers/Ticker.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Misc/App.h"

FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor") || CommandType == TEXT("create_actor"))
    {
        if (CommandType == TEXT("create_actor"))
        {
            UE_LOG(LogTemp, Warning, TEXT("'create_actor' command is deprecated and will be removed in a future version. Please use 'spawn_actor' instead."));
        }
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("spawn_static_mesh_actor"))
    {
        return HandleSpawnStaticMeshActor(Params);
    }
    else if (CommandType == TEXT("set_static_mesh_actor_mesh"))
    {
        return HandleSetStaticMeshActorMesh(Params);
    }
    else if (CommandType == TEXT("set_static_mesh_material"))
    {
        return HandleSetStaticMeshMaterial(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        return HandleGetActorProperties(Params);
    }
    else if (CommandType == TEXT("get_actor_property"))
    {
        return HandleGetActorProperty(Params);
    }
    else if (CommandType == TEXT("set_actor_property"))
    {
        return HandleSetActorProperty(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Editor viewport commands
    else if (CommandType == TEXT("focus_viewport"))
    {
        return HandleFocusViewport(Params);
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        return HandleTakeScreenshot(Params);
    }
    // Editor state extensions (Sprint 1)
    else if (CommandType == TEXT("get_viewport_camera"))
    {
        return HandleGetViewportCamera(Params);
    }
    else if (CommandType == TEXT("set_viewport_camera"))
    {
        return HandleSetViewportCamera(Params);
    }
    else if (CommandType == TEXT("execute_console_command"))
    {
        return HandleExecuteConsoleCommand(Params);
    }
    else if (CommandType == TEXT("set_cvar"))
    {
        return HandleSetCVar(Params);
    }
    else if (CommandType == TEXT("get_cvar"))
    {
        return HandleGetCVar(Params);
    }
    // v0.7.6 — viewport mode + introspection
    else if (CommandType == TEXT("get_viewport_mode"))
    {
        return HandleGetViewportMode(Params);
    }
    else if (CommandType == TEXT("set_viewport_mode"))
    {
        return HandleSetViewportMode(Params);
    }
    else if (CommandType == TEXT("read_output_log"))
    {
        return HandleReadOutputLog(Params);
    }
    else if (CommandType == TEXT("get_async_compile_status"))
    {
        return HandleGetAsyncCompileStatus(Params);
    }
    // v0.7.11 — PIE control
    else if (CommandType == TEXT("start_pie"))
    {
        return HandleStartPIE(Params);
    }
    else if (CommandType == TEXT("stop_pie"))
    {
        return HandleStopPIE(Params);
    }
    else if (CommandType == TEXT("is_pie_active"))
    {
        return HandleIsPIEActive(Params);
    }
    else if (CommandType == TEXT("pie_get_player"))
    {
        return HandlePIEGetPlayer(Params);
    }
    else if (CommandType == TEXT("pie_set_player"))
    {
        return HandlePIESetPlayer(Params);
    }
    else if (CommandType == TEXT("pie_apply_movement"))
    {
        return HandlePIEApplyMovement(Params);
    }
    else if (CommandType == TEXT("pie_screenshot"))
    {
        return HandlePIEScreenshot(Params);
    }
    // v0.7.12 — selection introspection
    else if (CommandType == TEXT("get_selected_actors"))
    {
        return HandleGetSelectedActors(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    // v0.7.10 — generic UClass lookup so spawn_actor accepts ANY AActor subclass.
    // Accepts three input shapes:
    //   (a) full path:  "/Script/Engine.SkyAtmosphere"        — used as-is
    //   (b) bare name:  "SkyAtmosphere"                       — tries "/Script/Engine.<name>",
    //                                                          then "/Script/UnrealEd.<name>"
    //   (c) bare name not in Engine/UnrealEd                  — wildcard package search via
    //                                                          UClass::TryFindTypeSlow (rare)
    // Replaces the v0.7.0 hardcoded if/else over 5 known types — Phase 7.2 needed
    // SkyAtmosphere and ExponentialHeightFog, neither of which was in the list.
    UClass* ActorClass = nullptr;
    if (ActorType.Contains(TEXT(".")))
    {
        ActorClass = LoadObject<UClass>(nullptr, *ActorType);
    }
    else
    {
        static const TCHAR* CandidateModules[] = { TEXT("Engine"), TEXT("UnrealEd") };
        for (const TCHAR* Module : CandidateModules)
        {
            const FString FullPath = FString::Printf(TEXT("/Script/%s.%s"), Module, *ActorType);
            ActorClass = LoadObject<UClass>(nullptr, *FullPath);
            if (ActorClass)
            {
                break;
            }
        }
        if (!ActorClass)
        {
            ActorClass = UClass::TryFindTypeSlow<UClass>(ActorType);
        }
    }

    if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown actor type: %s (not a resolvable AActor subclass)"), *ActorType));
    }

    NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnStaticMeshActor(const TSharedPtr<FJsonObject>& Params)
{
    // spawn_static_mesh_actor combines spawn_actor + mesh assignment into one call.
    // This is the ergonomic path for placing Megascans / Quixel meshes from the
    // /Game/Migrated/ tree without round-tripping through set_actor_property
    // (which only sees actor-level UPROPERTYs and can't reach the inner
    // UStaticMeshComponent::StaticMesh).
    FString ActorName, MeshPath;
    if (!Params->TryGetStringField(TEXT("name"), ActorName) || ActorName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath) || MeshPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
    }

    FVector  Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector  Scale(1.0f, 1.0f, 1.0f);
    if (Params->HasField(TEXT("location"))) Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    if (Params->HasField(TEXT("rotation"))) Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    if (Params->HasField(TEXT("scale")))    Scale    = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    // Reject duplicate names up-front (matches HandleSpawnActor behavior).
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    // Resolve the mesh. LoadObject<UStaticMesh> handles both
    // "/Game/.../SM_X.SM_X" and "/Game/.../SM_X" forms.
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!Mesh)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("StaticMesh not found at path: %s"), *MeshPath));
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!NewActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn StaticMeshActor"));
    }

    // Apply scale (SpawnActor doesn't take a scale parameter)
    FTransform Transform = NewActor->GetTransform();
    Transform.SetScale3D(Scale);
    NewActor->SetActorTransform(Transform);

    // Assign mesh. Default mobility to Movable so the LLM/user can nudge the
    // actor afterward via set_actor_transform; Static is the right baking
    // choice but it locks out further programmatic edits.
    if (UStaticMeshComponent* MeshComp = NewActor->GetStaticMeshComponent())
    {
        MeshComp->SetMobility(EComponentMobility::Movable);
        MeshComp->SetStaticMesh(Mesh);
    }

    // Optional Outliner folder placement, since the common case here is
    // "spawn 50 architecture pieces under Sanctuary/Columns/ for organization".
    FString FolderPath;
    if (Params->TryGetStringField(TEXT("folder_path"), FolderPath) && !FolderPath.IsEmpty())
    {
        NewActor->SetFolderPath(FName(*FolderPath));
    }

    return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetStaticMeshActorMesh(const TSharedPtr<FJsonObject>& Params)
{
    // Retroactive mesh swap on an already-spawned StaticMeshActor. Useful for
    // (a) replacing a placeholder mesh with the real Megascans one once it's
    // migrated, or (b) cycling through mesh variants without respawning.
    FString ActorName, MeshPath;
    if (!Params->TryGetStringField(TEXT("name"), ActorName) || ActorName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath) || MeshPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    AStaticMeshActor* SMA = Cast<AStaticMeshActor>(TargetActor);
    if (!SMA)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor '%s' is not a StaticMeshActor"), *ActorName));
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!Mesh)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("StaticMesh not found at path: %s"), *MeshPath));
    }

    UStaticMeshComponent* MeshComp = SMA->GetStaticMeshComponent();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("StaticMeshActor has no StaticMeshComponent"));
    }
    MeshComp->SetStaticMesh(Mesh);

    return FUnrealMCPCommonUtils::ActorToJsonObject(SMA, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetStaticMeshMaterial(const TSharedPtr<FJsonObject>& Params)
{
    // v0.7.10 — material-slot write convenience. Equivalent to dotted-path
    // "StaticMeshComponent.OverrideMaterials.<slot>" with v0.7.10's FArrayProperty
    // walker, but this tool is the ergonomic path when all you want is "fix the
    // broken-parent magenta on slot 0". Calls UStaticMeshComponent::SetMaterial
    // which handles MarkRenderStateDirty internally.
    FString ActorName, MaterialPath;
    int32   Slot = 0;
    if (!Params->TryGetStringField(TEXT("name"), ActorName) || ActorName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath) || MaterialPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path' parameter"));
    }
    Params->TryGetNumberField(TEXT("slot"), Slot);  // optional, defaults 0

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    AStaticMeshActor* SMA = Cast<AStaticMeshActor>(TargetActor);
    if (!SMA)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor '%s' is not a StaticMeshActor"), *ActorName));
    }

    UStaticMeshComponent* MeshComp = SMA->GetStaticMeshComponent();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("StaticMeshActor has no StaticMeshComponent"));
    }

    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Material not found at path: %s"), *MaterialPath));
    }

    const int32 NumSlots = MeshComp->GetNumMaterials();
    if (Slot < 0 || Slot >= NumSlots)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Slot %d out of range (mesh has %d material slot(s))"),
                Slot, NumSlots));
    }

    MeshComp->SetMaterial(Slot, Material);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actor"), ActorName);
    Result->SetNumberField(TEXT("slot"), Slot);
    Result->SetStringField(TEXT("material_path"), MaterialPath);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Always return detailed properties for this command
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // v0.7.10 — read counterpart to set_actor_property. Same dotted-path
    // resolution (FObjectProperty hops, FStructProperty hops, FArrayProperty
    // hops with numeric index), but returns the leaf value instead of writing.
    // Useful for inspecting mesh refs, material slot assignments, light
    // intensities, etc. without restarting the editor.
    FString ActorName, PropertyPath;
    if (!Params->TryGetStringField(TEXT("name"), ActorName) || ActorName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyPath) || PropertyPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    FUnrealMCPCommonUtils::FPropertyTarget Target;
    FString ErrorMessage;
    if (!FUnrealMCPCommonUtils::WalkPropertyPath(TargetActor, PropertyPath, Target, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonValue> Value = FUnrealMCPCommonUtils::GetPropertyAtTarget(Target, ErrorMessage);
    if (!Value.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actor"), ActorName);
    Result->SetStringField(TEXT("property"), PropertyPath);
    Result->SetField(TEXT("value"), Value);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Get property value
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }
    
    TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));

    // v0.7.4 + v0.7.5: PropertyName may be a dotted path like:
    //   - "PointLightComponent.Intensity"               (object hop → leaf)
    //   - "Settings.AutoExposureBias"                   (struct hop → leaf, v0.7.5)
    //   - "Settings.ColorGrading.Saturation"            (nested struct, v0.7.5)
    // WalkPropertyPath resolves the chain to an FPropertyTarget — a
    // (container addr, container type, owning UObject) triple that's enough
    // to look up + set the leaf regardless of whether it lives on a UObject
    // or a struct.
    FUnrealMCPCommonUtils::FPropertyTarget Target;
    FString WalkError;
    if (!FUnrealMCPCommonUtils::WalkPropertyPath(TargetActor, PropertyName, Target, WalkError))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(WalkError);
    }

    FString SetError;
    if (!FUnrealMCPCommonUtils::SetPropertyAtTarget(Target, PropertyValue, SetError))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(SetError);
    }

    // PostEditChangeProperty on the owning UObject with the outermost property
    // so the editor knows the actor's data changed and refreshes Details panel
    // + viewport. Picking the outer (first segment) is "good enough" — the
    // editor doesn't need the exact struct chain to refresh the actor.
    if (Target.OwningObject)
    {
        if (FProperty* OuterProp = Target.OwningObject->GetClass()->FindPropertyByName(*Target.OuterPropertyName))
        {
            FPropertyChangedEvent ChangeEvent(OuterProp);
            Target.OwningObject->PostEditChangeProperty(ChangeEvent);
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("property"), PropertyName);
    ResultObj->SetBoolField(TEXT("success"), true);
    if (Target.OwningObject != TargetActor || PropertyName.Contains(TEXT(".")))
    {
        ResultObj->SetStringField(TEXT("leaf_property"), Target.LeafPropertyName);
        ResultObj->SetStringField(TEXT("leaf_container"), Target.ContainerType ? Target.ContainerType->GetName() : FString());
        ResultObj->SetStringField(TEXT("owning_object_class"), Target.OwningObject ? Target.OwningObject->GetClass()->GetName() : FString());
    }
    ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the blueprint
    if (BlueprintName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint name is empty"));
    }

    FString Root      = TEXT("/Game/Blueprints/");
    FString AssetPath = Root + BlueprintName;

    if (!FPackageName::DoesPackageExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found – it must reside under /Game/Blueprints"), *BlueprintName));
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get target actor name if provided
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName);

    // Get location if provided
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // Get distance
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // Get orientation if provided
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    // Get the active viewport
    FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    // If we have a target actor, focus on it
    if (HasTargetActor)
    {
        // Find the actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }

        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }

        // Focus on the actor
        ViewportClient->SetViewLocation(TargetActor->GetActorLocation() - FVector(Distance, 0.0f, 0.0f));
    }
    // Otherwise use the provided location
    else if (HasLocation)
    {
        ViewportClient->SetViewLocation(Location - FVector(Distance, 0.0f, 0.0f));
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // Set orientation if provided
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }

    // Force viewport to redraw
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Accept "filepath" (the established name) or "filename" (a common alias
    // used by callers more familiar with file-system terms). One must be present.
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath) &&
        !Params->TryGetStringField(TEXT("filename"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'filepath' (or 'filename') parameter"));
    }
    if (FilePath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Empty filepath"));
    }

    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    // Anchor bare filenames under <Project>/Saved/Screenshots/ so we (a) match
    // the UE convention for editor screenshots and (b) hand back a predictable
    // absolute path that the Python wrapper can read for inline-image return.
    // Anything already absolute is left alone.
    if (FPaths::IsRelative(FilePath))
    {
        FilePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), FilePath);
    }
    FilePath = FPaths::ConvertRelativePathToFull(FilePath);

    // Ensure the destination directory exists (FFileHelper::SaveArrayToFile
    // can fail silently if the parent dir isn't there).
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), /*Tree=*/true);

    if (!GEditor || !GEditor->GetActiveViewport())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor viewport"));
    }

    FViewport* Viewport = GEditor->GetActiveViewport();

    // v0.7.7 — force a fresh redraw before grabbing pixels.
    // UE's editor viewport is event-driven: ReadPixels grabs whatever is in
    // the backbuffer, which is the last-drawn frame. Without explicit
    // invalidation, MCP-driven state changes (lights, transforms, properties)
    // don't show up in the screenshot until the user moves their mouse over
    // the viewport. We invalidate the client + the viewport, ask for a draw,
    // then flush rendering commands so the GPU has finished before we read.
    if (FViewportClient* Client = Viewport->GetClient())
    {
        if (FEditorViewportClient* EditorClient = static_cast<FEditorViewportClient*>(Client))
        {
            EditorClient->Invalidate();
        }
    }
    Viewport->Invalidate();
    Viewport->Draw();
    FlushRenderingCommands();

    TArray<FColor> Bitmap;
    const FIntRect ViewportRect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);

    if (!Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Viewport::ReadPixels failed"));
    }

    TArray<uint8> CompressedBitmap;
    // UE 5.7 deprecates CompressImageArray in favor of PNGCompressImageArray, but the
    // new API requires TArrayView64<const FColor> + TArray64<uint8> — switching the
    // surrounding ReadPixels path to 64-bit arrays is non-trivial and gets clean
    // treatment in a future sprint when we rewrite via FImageView/FImageBuilder.
    // For now: eat the deprecation warning, keep the working code.
    FImageUtils::CompressImageArray(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, Bitmap, CompressedBitmap);

    if (!FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to write screenshot to %s"), *FilePath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("filepath"), FilePath);
    ResultObj->SetNumberField(TEXT("width"),  Viewport->GetSizeXY().X);
    ResultObj->SetNumberField(TEXT("height"), Viewport->GetSizeXY().Y);
    ResultObj->SetNumberField(TEXT("size_bytes"), CompressedBitmap.Num());
    return ResultObj;
}


// ─── Editor state extensions (Sprint 1) ──────────────────────────────────────

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
    UUnrealEditorSubsystem* EditorSub = GEditor ? GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>() : nullptr;
    if (!EditorSub)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("UnrealEditorSubsystem unavailable"));
    }

    FVector Location;
    FRotator Rotation;
    EditorSub->GetLevelViewportCameraInfo(Location, Rotation);

    TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
    LocObj->SetNumberField(TEXT("x"), Location.X);
    LocObj->SetNumberField(TEXT("y"), Location.Y);
    LocObj->SetNumberField(TEXT("z"), Location.Z);

    TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
    RotObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
    RotObj->SetNumberField(TEXT("yaw"),   Rotation.Yaw);
    RotObj->SetNumberField(TEXT("roll"),  Rotation.Roll);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetObjectField(TEXT("location"), LocObj);
    Result->SetObjectField(TEXT("rotation"), RotObj);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
    UUnrealEditorSubsystem* EditorSub = GEditor ? GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>() : nullptr;
    if (!EditorSub)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("UnrealEditorSubsystem unavailable"));
    }

    // Accepts either a {location:{x,y,z}, rotation:{pitch,yaw,roll}} object form
    // or flat arrays as a convenience: location: [x,y,z], rotation: [p,y,r].
    auto ReadVector = [](const TSharedPtr<FJsonObject>& Parent, const FString& Field, FVector& Out) -> bool
    {
        const TSharedPtr<FJsonObject>* AsObj = nullptr;
        if (Parent->TryGetObjectField(Field, AsObj))
        {
            Out.X = (*AsObj)->GetNumberField(TEXT("x"));
            Out.Y = (*AsObj)->GetNumberField(TEXT("y"));
            Out.Z = (*AsObj)->GetNumberField(TEXT("z"));
            return true;
        }
        const TArray<TSharedPtr<FJsonValue>>* AsArr = nullptr;
        if (Parent->TryGetArrayField(Field, AsArr) && AsArr->Num() == 3)
        {
            Out.X = (*AsArr)[0]->AsNumber();
            Out.Y = (*AsArr)[1]->AsNumber();
            Out.Z = (*AsArr)[2]->AsNumber();
            return true;
        }
        return false;
    };

    auto ReadRotator = [](const TSharedPtr<FJsonObject>& Parent, const FString& Field, FRotator& Out) -> bool
    {
        const TSharedPtr<FJsonObject>* AsObj = nullptr;
        if (Parent->TryGetObjectField(Field, AsObj))
        {
            Out.Pitch = (*AsObj)->GetNumberField(TEXT("pitch"));
            Out.Yaw   = (*AsObj)->GetNumberField(TEXT("yaw"));
            Out.Roll  = (*AsObj)->GetNumberField(TEXT("roll"));
            return true;
        }
        const TArray<TSharedPtr<FJsonValue>>* AsArr = nullptr;
        if (Parent->TryGetArrayField(Field, AsArr) && AsArr->Num() == 3)
        {
            Out.Pitch = (*AsArr)[0]->AsNumber();
            Out.Yaw   = (*AsArr)[1]->AsNumber();
            Out.Roll  = (*AsArr)[2]->AsNumber();
            return true;
        }
        return false;
    };

    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    if (!ReadVector(Params, TEXT("location"), Location) || !ReadRotator(Params, TEXT("rotation"), Rotation))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Need 'location' (Vector) and 'rotation' (Rotator). "
                 "Accept either {x,y,z}/{pitch,yaw,roll} objects or 3-element arrays."));
    }

    EditorSub->SetLevelViewportCameraInfo(Location, Rotation);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
    FString Command;
    if (!Params->TryGetStringField(TEXT("command"), Command) || Command.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'command' parameter"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || !GEngine)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world / engine to execute against"));
    }

    const bool bSuccess = GEngine->Exec(World, *Command);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("command"), Command);
    Result->SetBoolField(TEXT("success"), bSuccess);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetCVar(const TSharedPtr<FJsonObject>& Params)
{
    FString Name, Value;
    if (!Params->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("value"), Value))
    {
        // Allow numeric values to come through as numbers
        double NumValue = 0.0;
        if (Params->TryGetNumberField(TEXT("value"), NumValue))
        {
            Value = FString::SanitizeFloat(NumValue);
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'value' parameter"));
        }
    }

    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
    if (!CVar)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("CVar not found: %s"), *Name));
    }

    CVar->Set(*Value, ECVF_SetByConsole);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Name);
    Result->SetStringField(TEXT("value"), Value);
    Result->SetStringField(TEXT("current_value"), CVar->GetString());
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetCVar(const TSharedPtr<FJsonObject>& Params)
{
    FString Name;
    if (!Params->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'name' parameter"));
    }

    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
    if (!CVar)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("CVar not found: %s"), *Name));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Name);
    Result->SetStringField(TEXT("value"), CVar->GetString());
    // Expose the typed variants so callers can use them without parsing
    Result->SetNumberField(TEXT("float_value"), CVar->GetFloat());
    Result->SetNumberField(TEXT("int_value"), CVar->GetInt());
    Result->SetBoolField(TEXT("bool_value"), CVar->GetBool());
    return Result;
}


// ─── v0.7.6 — viewport mode + introspection ──────────────────────────────────

namespace
{
    /** Resolve the active editor viewport client for view-mode operations. */
    FEditorViewportClient* GetActiveEditorViewportClient()
    {
        if (!GEditor || !GEditor->GetActiveViewport()) return nullptr;
        FViewportClient* Client = GEditor->GetActiveViewport()->GetClient();
        return static_cast<FEditorViewportClient*>(Client);
    }

    /** Map a view-mode string to UE's EViewModeIndex (case-insensitive). */
    bool StringToViewModeIndex(const FString& Name, EViewModeIndex& Out)
    {
        const FString N = Name.ToLower();
        if (N == TEXT("lit"))                { Out = VMI_Lit;                return true; }
        if (N == TEXT("unlit"))              { Out = VMI_Unlit;              return true; }
        if (N == TEXT("wireframe"))          { Out = VMI_Wireframe;          return true; }
        if (N == TEXT("brushwireframe"))     { Out = VMI_BrushWireframe;     return true; }
        if (N == TEXT("detaillighting"))     { Out = VMI_Lit_DetailLighting; return true; }
        if (N == TEXT("lightingonly"))       { Out = VMI_LightingOnly;       return true; }
        if (N == TEXT("lightcomplexity"))    { Out = VMI_LightComplexity;    return true; }
        if (N == TEXT("shadercomplexity"))   { Out = VMI_ShaderComplexity;   return true; }
        if (N == TEXT("lightmapdensity"))    { Out = VMI_LightmapDensity;    return true; }
        if (N == TEXT("reflectionoverride")) { Out = VMI_ReflectionOverride; return true; }
        if (N == TEXT("visualizebuffer"))    { Out = VMI_VisualizeBuffer;    return true; }
        if (N == TEXT("pathtracing"))        { Out = VMI_PathTracing;        return true; }
        return false;
    }

    FString ViewModeIndexToString(EViewModeIndex Mode)
    {
        switch (Mode)
        {
            case VMI_Lit:                return TEXT("Lit");
            case VMI_Unlit:              return TEXT("Unlit");
            case VMI_Wireframe:          return TEXT("Wireframe");
            case VMI_BrushWireframe:     return TEXT("BrushWireframe");
            case VMI_Lit_DetailLighting: return TEXT("DetailLighting");
            case VMI_LightingOnly:       return TEXT("LightingOnly");
            case VMI_LightComplexity:    return TEXT("LightComplexity");
            case VMI_ShaderComplexity:   return TEXT("ShaderComplexity");
            case VMI_LightmapDensity:    return TEXT("LightmapDensity");
            case VMI_ReflectionOverride: return TEXT("ReflectionOverride");
            case VMI_VisualizeBuffer:    return TEXT("VisualizeBuffer");
            case VMI_PathTracing:        return TEXT("PathTracing");
            default:                     return FString::Printf(TEXT("Unknown(%d)"), static_cast<int32>(Mode));
        }
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetViewportMode(const TSharedPtr<FJsonObject>& Params)
{
    FEditorViewportClient* Client = GetActiveEditorViewportClient();
    if (!Client)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor viewport client"));
    }
    const EViewModeIndex Mode = Client->GetViewMode();
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("mode"), ViewModeIndexToString(Mode));
    Result->SetNumberField(TEXT("mode_index"), static_cast<int32>(Mode));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetViewportMode(const TSharedPtr<FJsonObject>& Params)
{
    FString Mode;
    if (!Params->TryGetStringField(TEXT("mode"), Mode) || Mode.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'mode' parameter (Lit, Unlit, Wireframe, DetailLighting, LightingOnly, ReflectionOverride, etc.)"));
    }

    EViewModeIndex Resolved;
    if (!StringToViewModeIndex(Mode, Resolved))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Unknown view mode: %s"), *Mode));
    }

    FEditorViewportClient* Client = GetActiveEditorViewportClient();
    if (!Client)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor viewport client"));
    }

    Client->SetViewMode(Resolved);
    Client->Invalidate();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("mode"), ViewModeIndexToString(Resolved));
    Result->SetNumberField(TEXT("mode_index"), static_cast<int32>(Resolved));
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleReadOutputLog(const TSharedPtr<FJsonObject>& Params)
{
    // The editor's full log lives at <Project>/Saved/Logs/<ProjectName>.log. We
    // tail-read the last N lines via FFileHelper. Cheap and reliable.
    int32 NumLines = 50;
    if (Params->HasField(TEXT("lines")))
    {
        NumLines = FMath::Clamp(static_cast<int32>(Params->GetNumberField(TEXT("lines"))), 1, 5000);
    }

    const FString LogDir = FPaths::ProjectLogDir();
    const FString ProjectName = FApp::GetProjectName();
    const FString LogFilePath = FPaths::Combine(LogDir, ProjectName + TEXT(".log"));

    FString Contents;
    if (!FFileHelper::LoadFileToString(Contents, *LogFilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not read log file: %s"), *LogFilePath));
    }

    TArray<FString> Lines;
    Contents.ParseIntoArrayLines(Lines);
    const int32 Start = FMath::Max(0, Lines.Num() - NumLines);

    TArray<TSharedPtr<FJsonValue>> Tail;
    Tail.Reserve(Lines.Num() - Start);
    for (int32 i = Start; i < Lines.Num(); i++)
    {
        Tail.Add(MakeShared<FJsonValueString>(Lines[i]));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("log_path"), LogFilePath);
    Result->SetNumberField(TEXT("total_lines"), Lines.Num());
    Result->SetNumberField(TEXT("returned_lines"), Tail.Num());
    Result->SetArrayField(TEXT("lines"), Tail);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetAsyncCompileStatus(const TSharedPtr<FJsonObject>& Params)
{
    // Surfaces the editor's async compile queue counts so the caller (Claude /
    // any MCP client) can detect stalls like the v0.7.3 finalize_migration hang
    // BEFORE invoking a heavy batch operation. Reports per-manager counts +
    // the aggregate FAssetCompilingManager total.
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    int32 ShaderJobs = 0;
    if (GShaderCompilingManager)
    {
        ShaderJobs = GShaderCompilingManager->GetNumRemainingJobs();
    }
    Result->SetNumberField(TEXT("shader_jobs"), ShaderJobs);

    // Asset compiling manager aggregates StaticMesh, Texture, etc. compile queues.
    int32 AssetCompiles = static_cast<int32>(FAssetCompilingManager::Get().GetNumRemainingAssets());
    Result->SetNumberField(TEXT("asset_compiles"), AssetCompiles);

    Result->SetBoolField(TEXT("is_idle"), ShaderJobs == 0 && AssetCompiles == 0);
    return Result;
}


// ─── v0.7.11 — PIE control + player state for self-verification ───────────────

namespace
{
    /** Resolve the active PIE world if a session is in progress, else nullptr. */
    UWorld* GetPlayWorld()
    {
        if (!GEditor) return nullptr;
        return GEditor->PlayWorld;
    }

    /** Resolve the player pawn at index 0 from the active PIE world, else nullptr. */
    APawn* GetPIEPawn()
    {
        UWorld* PlayWorld = GetPlayWorld();
        if (!PlayWorld) return nullptr;
        return UGameplayStatics::GetPlayerPawn(PlayWorld, 0);
    }

    /** Pack (X, Y, Z) into a JSON [..] array. */
    TSharedPtr<FJsonValue> PackVec3(const FVector& V)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(V.X));
        Arr.Add(MakeShared<FJsonValueNumber>(V.Y));
        Arr.Add(MakeShared<FJsonValueNumber>(V.Z));
        return MakeShared<FJsonValueArray>(Arr);
    }

    /** Pack rotator (P, Y, R) into a JSON [..] array. */
    TSharedPtr<FJsonValue> PackRot(const FRotator& R)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Add(MakeShared<FJsonValueNumber>(R.Pitch));
        Arr.Add(MakeShared<FJsonValueNumber>(R.Yaw));
        Arr.Add(MakeShared<FJsonValueNumber>(R.Roll));
        return MakeShared<FJsonValueArray>(Arr);
    }

    /** Human-readable label for EMovementMode — useful for "is the character on the floor?" */
    FString MovementModeToString(EMovementMode Mode)
    {
        switch (Mode)
        {
            case MOVE_None:       return TEXT("None");
            case MOVE_Walking:    return TEXT("Walking");
            case MOVE_NavWalking: return TEXT("NavWalking");
            case MOVE_Falling:    return TEXT("Falling");
            case MOVE_Swimming:   return TEXT("Swimming");
            case MOVE_Flying:     return TEXT("Flying");
            case MOVE_Custom:     return TEXT("Custom");
            default:              return FString::Printf(TEXT("Unknown(%d)"), static_cast<int32>(Mode));
        }
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleStartPIE(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No GEditor — editor not running"));
    }
    if (GEditor->PlayWorld)
    {
        // Already in PIE — treat as success but flag it.
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetBoolField(TEXT("already_active"), true);
        return R;
    }

    // Queue a play-session request using the project's last-used PIE settings.
    // No FRequestPlaySessionParams customization — sticks with the user's
    // configured play mode, viewport, and settings from Editor Preferences.
    FRequestPlaySessionParams PlayParams;
    GEditor->RequestPlaySession(PlayParams);
    GEditor->StartQueuedPlaySessionRequest();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("already_active"), false);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleStopPIE(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No GEditor — editor not running"));
    }
    const bool bWasActive = GEditor->PlayWorld != nullptr;
    GEditor->RequestEndPlayMap();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("was_active"), bWasActive);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleIsPIEActive(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("active"), GEditor && GEditor->PlayWorld != nullptr);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePIEGetPlayer(const TSharedPtr<FJsonObject>& Params)
{
    APawn* Pawn = GetPIEPawn();
    if (!Pawn)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No PIE pawn — is PIE active?"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetField(TEXT("location"), PackVec3(Pawn->GetActorLocation()));
    Result->SetField(TEXT("rotation"), PackRot(Pawn->GetActorRotation()));
    Result->SetField(TEXT("velocity"), PackVec3(Pawn->GetVelocity()));
    Result->SetStringField(TEXT("pawn_class"), Pawn->GetClass()->GetName());

    // Useful walkability signals — is the character on the ground?
    if (UCharacterMovementComponent* Move = Pawn->FindComponentByClass<UCharacterMovementComponent>())
    {
        Result->SetStringField(TEXT("movement_mode"), MovementModeToString(Move->MovementMode));
        Result->SetBoolField(TEXT("is_falling"), Move->IsFalling());
        Result->SetBoolField(TEXT("is_movement_in_progress"), Move->Velocity.SizeSquared() > 0.01f);
    }

    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePIESetPlayer(const TSharedPtr<FJsonObject>& Params)
{
    APawn* Pawn = GetPIEPawn();
    if (!Pawn)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No PIE pawn — is PIE active?"));
    }

    FVector  Location = Pawn->GetActorLocation();
    FRotator Rotation = Pawn->GetActorRotation();
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }

    // SweepTest = false means we teleport regardless of collision; the
    // intent here is "put the player at exactly this location for testing",
    // not "simulate physically arriving here".
    Pawn->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetField(TEXT("location"), PackVec3(Pawn->GetActorLocation()));
    Result->SetField(TEXT("rotation"), PackRot(Pawn->GetActorRotation()));
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePIEApplyMovement(const TSharedPtr<FJsonObject>& Params)
{
    APawn* Pawn = GetPIEPawn();
    if (!Pawn)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No PIE pawn — is PIE active?"));
    }

    FVector Direction(1.0f, 0.0f, 0.0f);
    if (Params->HasField(TEXT("direction")))
    {
        Direction = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("direction"));
    }
    Direction.Normalize();

    double Duration = 1.0;
    Params->TryGetNumberField(TEXT("duration"), Duration);
    Duration = FMath::Clamp(Duration, 0.05, 30.0);

    double Scale = 1.0;
    Params->TryGetNumberField(TEXT("scale"), Scale);

    // Fire-and-forget movement — register a ticker that calls
    // AddMovementInput each frame for the requested duration, then
    // unregisters itself. Caller waits client-side, then queries
    // pie_get_player to read the result.
    TWeakObjectPtr<APawn> WeakPawn(Pawn);
    const double EndTime = FPlatformTime::Seconds() + Duration;
    const FVector Dir = Direction;
    const float ScaleVal = static_cast<float>(Scale);

    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([WeakPawn, Dir, ScaleVal, EndTime](float /*DeltaSeconds*/) -> bool
        {
            APawn* P = WeakPawn.Get();
            if (!P) return false;                          // pawn died → stop
            if (FPlatformTime::Seconds() >= EndTime) return false; // time's up → stop
            P->AddMovementInput(Dir, ScaleVal);
            return true;                                   // keep ticking
        }),
        0.0f);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetField(TEXT("direction"), PackVec3(Direction));
    Result->SetNumberField(TEXT("duration"), Duration);
    Result->SetNumberField(TEXT("scale"), Scale);
    Result->SetStringField(TEXT("note"),
        TEXT("Movement queued asynchronously. Sleep at least 'duration' seconds before reading pie_get_player to see the final state."));
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandlePIEScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* PlayWorld = GetPlayWorld();
    if (!PlayWorld)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PIE not active"));
    }
    UGameViewportClient* GameViewport = PlayWorld->GetGameViewport();
    if (!GameViewport || !GameViewport->Viewport)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No PIE game viewport"));
    }

    // Resolve output path (same convention as HandleTakeScreenshot).
    FString Filename = TEXT("pie_screenshot.png");
    Params->TryGetStringField(TEXT("filename"), Filename);
    if (!Filename.EndsWith(TEXT(".png")))
    {
        Filename += TEXT(".png");
    }
    FString OutputPath;
    if (FPaths::IsRelative(Filename))
    {
        OutputPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), Filename);
    }
    else
    {
        OutputPath = Filename;
    }

    // Force a fresh redraw before capture (v0.7.7-style stale-buffer guard).
    FViewport* Viewport = GameViewport->Viewport;
    Viewport->Invalidate();
    Viewport->Draw();
    FlushRenderingCommands();

    // Capture pixel array → PNG (FImageUtils legacy API; deprecated warning
    // tracked separately).
    TArray<FColor> Bitmap;
    if (!Viewport->ReadPixels(Bitmap))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("ReadPixels failed on PIE viewport"));
    }
    for (FColor& Px : Bitmap) { Px.A = 255; }

    TArray64<uint8> CompressedBitmap;
    FImageUtils::PNGCompressImageArray(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, Bitmap, CompressedBitmap);

    if (!FFileHelper::SaveArrayToFile(CompressedBitmap, *OutputPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to write screenshot: %s"), *OutputPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"), OutputPath);
    Result->SetNumberField(TEXT("width"), Viewport->GetSizeXY().X);
    Result->SetNumberField(TEXT("height"), Viewport->GetSizeXY().Y);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}


// ─── v0.7.12 — read the editor's current actor selection ─────────────────────

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
    // Returns the current viewport / Outliner actor selection. Used to capture
    // a hand-curated subset of a scene (e.g. "the candle clusters + lanterns +
    // floor tiles in the foreground" of a large showcase scene) without making
    // the client guess from screenshots.
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No GEditor — editor not running"));
    }

    USelection* Selection = GEditor->GetSelectedActors();
    if (!Selection)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Selection subsystem unavailable"));
    }

    TArray<TSharedPtr<FJsonValue>> ActorsJson;
    const int32 Num = Selection->Num();
    ActorsJson.Reserve(Num);

    for (int32 i = 0; i < Num; i++)
    {
        AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
        if (!Actor) continue;

        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), Actor->GetActorLabel());
        Obj->SetStringField(TEXT("internal_name"), Actor->GetName());
        Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

        const FName FolderPath = Actor->GetFolderPath();
        Obj->SetStringField(TEXT("folder_path"), FolderPath.IsNone() ? FString() : FolderPath.ToString());

        // Lightweight transform so the caller can decide what to do without
        // re-querying per-actor. Heavier per-property reads still go through
        // get_actor_property.
        const FVector L = Actor->GetActorLocation();
        TArray<TSharedPtr<FJsonValue>> Loc;
        Loc.Add(MakeShared<FJsonValueNumber>(L.X));
        Loc.Add(MakeShared<FJsonValueNumber>(L.Y));
        Loc.Add(MakeShared<FJsonValueNumber>(L.Z));
        Obj->SetArrayField(TEXT("location"), Loc);

        ActorsJson.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("actors"), ActorsJson);
    Result->SetNumberField(TEXT("count"), ActorsJson.Num());
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}