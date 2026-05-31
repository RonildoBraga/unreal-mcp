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

    // v0.8.0 Day 3-4 — selection mutation + framing (round-trip with get_selected_actors).
    TSharedPtr<FJsonObject> HandleSetSelectedActors(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleClearSelection(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFocusSelectedActors(const TSharedPtr<FJsonObject>& Params);

    // v0.8.0 Day 3-4 — paged actor enumeration (replaces get_actors_in_level
    // for any caller that doesn't want the full 744 KB dump). Class filter,
    // name pattern, folder prefix, limit/offset.
    TSharedPtr<FJsonObject> HandleFindActors(const TSharedPtr<FJsonObject>& Params);

    // v0.8.0 Day 3-4 — batch actor mutations. One MCP round-trip per scene
    // construction step instead of N (RomanCave-style dense placement).
    TSharedPtr<FJsonObject> HandleSpawnActorBatch(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteActorBatch(const TSharedPtr<FJsonObject>& Params);

    // v0.8.0 Day 3-4 — generalized Details-panel parity (§5).
    // Same property-path traversal as get/set_actor_property, but the target
    // can be ANY UObject — actor display label/internal name, /Game/ asset
    // path, /Script/ engine class, or class default object.
    TSharedPtr<FJsonObject> HandleGetObjectProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetObjectProperty(const TSharedPtr<FJsonObject>& Params);

    // v0.8.0 Day 3-4 — viewport: frame a single actor, toggle show flags.
    TSharedPtr<FJsonObject> HandleFrameActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetShowFlag(const TSharedPtr<FJsonObject>& Params);

    // v0.8.0 Day 3-4 — console-bridge unblockers for migration flow.
    TSharedPtr<FJsonObject> HandleWaitForAsyncCompile(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDismissModalDialog(const TSharedPtr<FJsonObject>& Params);

    // v0.8.0 Day 3-4 — read counterpart for actor transform (symmetry with set).
    TSharedPtr<FJsonObject> HandleGetActorTransform(const TSharedPtr<FJsonObject>& Params);
};