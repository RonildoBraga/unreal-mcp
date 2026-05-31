#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Level Management MCP commands.
 *
 * Minimum loop: know what level you're in, switch to a different one,
 * save the current state. Save-with-built-data + create_level +
 * streaming-sublevel support come post-v0.8.0.
 *
 * Tools:
 *   get_current_level   -- name + path of the loaded editor world
 *   open_level          -- load a level by /Game/ path
 *   save_current_level  -- save the currently loaded level
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
};
