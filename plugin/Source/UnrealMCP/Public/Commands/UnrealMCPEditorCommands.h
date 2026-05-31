#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Editor-related MCP commands
 * Handles viewport control, actor manipulation, and level management
 */
class UNREALMCP_API FUnrealMCPEditorCommands
{
public:
    FUnrealMCPEditorCommands();

    // Handle editor commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Actor manipulation commands
    TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSpawnStaticMeshActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetStaticMeshActorMesh(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetStaticMeshMaterial(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params);

    // Blueprint actor spawning
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);

    // Editor viewport commands
    TSharedPtr<FJsonObject> HandleFocusViewport(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params);

    // Editor state extensions (Sprint 1)
    TSharedPtr<FJsonObject> HandleGetViewportCamera(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetCVar(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetCVar(const TSharedPtr<FJsonObject>& Params);

    // v0.7.6 — viewport mode + introspection
    TSharedPtr<FJsonObject> HandleGetViewportMode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetViewportMode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReadOutputLog(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAsyncCompileStatus(const TSharedPtr<FJsonObject>& Params);

    // v0.7.11 — PIE (Play-In-Editor) control for self-verification
    TSharedPtr<FJsonObject> HandleStartPIE(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleStopPIE(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleIsPIEActive(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePIEGetPlayer(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePIESetPlayer(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePIEApplyMovement(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandlePIEScreenshot(const TSharedPtr<FJsonObject>& Params);

    // v0.7.12 — read the editor's current actor selection
    TSharedPtr<FJsonObject> HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params);

    // v0.8.0 Day 2c-i+ — trigger Live Coding compile (no editor restart needed)
    TSharedPtr<FJsonObject> HandleRecompileLive(const TSharedPtr<FJsonObject>& Params);
};