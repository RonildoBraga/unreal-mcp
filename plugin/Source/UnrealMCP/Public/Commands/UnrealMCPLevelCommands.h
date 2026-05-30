#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Level Management MCP commands.
 *
 * Sprint 1 partial — 4 tools covering the minimum loop: know what level
 * you're in, switch to a different one, save the current state, batch-save
 * everything dirty. Save-with-built-data + create_level + streaming-sublevel
 * support come in Sprint 3.
 *
 * Tools:
 *   get_current_level   — name + path of the loaded editor world
 *   open_level          — load a level by /Game/ path
 *   save_current_level  — save the currently loaded level
 *   save_all_dirty      — batch-save every dirty level + content package
 */
class UNREALMCP_API FUnrealMCPLevelCommands
{
public:
    FUnrealMCPLevelCommands();

    /** Top-level entry point used by UUnrealMCPBridge. */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleOpenLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveAllDirty(const TSharedPtr<FJsonObject>& Params);
};
