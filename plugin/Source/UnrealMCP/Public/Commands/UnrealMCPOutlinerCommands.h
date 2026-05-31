#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Outliner / actor-organization MCP commands (Sprint 2).
 *
 * UE's "World Outliner" panel groups actors by virtual folder paths. Folders
 * are not assets — they're labels stored as FName on each AActor. The set of
 * folders shown in the Outliner is the union of all actors' folder labels,
 * plus any "pending" folders registered via FActorFolders that have no actors
 * in them yet.
 *
 * Tools:
 *   get_outliner_folders     — list every folder path in the current world
 *   move_actor_to_folder      — set an actor's folder label
 *   create_outliner_folder    — register a pending folder so it appears empty
 *   get_actors_in_folder      — list actors whose folder path matches
 *
 * Use case for Lauder Phase 7.2+: as we migrate Goddess Temple assets and
 * place them in L_Base v2, the Outliner gets crowded. Folder organization
 * keeps actors discoverable.
 */
class UNREALMCP_API FUnrealMCPOutlinerCommands
{
public:
    FUnrealMCPOutlinerCommands();

    /** Top-level entry point used by UUnrealMCPBridge. */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleGetOutlinerFolders(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleMoveActorToFolder(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateOutlinerFolder(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorsInFolder(const TSharedPtr<FJsonObject>& Params);
    // v0.8.0 Day 3-4 — batch organize after a dense scene placement.
    TSharedPtr<FJsonObject> HandleMoveActorToFolderBatch(const TSharedPtr<FJsonObject>& Params);
};
