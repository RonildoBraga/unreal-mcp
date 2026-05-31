#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Project-wide MCP commands
 */
class UNREALMCP_API FUnrealMCPProjectCommands
{
public:
    FUnrealMCPProjectCommands();

    // Handle project commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Specific project command handlers
    TSharedPtr<FJsonObject> HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params);

    // v0.8.0 Day 3-4 — INI file editing (DefaultEngine, DefaultGame, etc.)
    TSharedPtr<FJsonObject> HandleGetIni(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetIni(const TSharedPtr<FJsonObject>& Params);

    // v0.8.1 — arbitrary Python execution escape hatch (unsafe-by-default).
    // Gated on `unsafe=true`. Routes to IPythonScriptPlugin::ExecPythonCommandEx.
    TSharedPtr<FJsonObject> HandleExecutePython(const TSharedPtr<FJsonObject>& Params);
};