// v0.8.x Day 2d-equivalent — project-wide MCP command handlers, lifted out of
// the v0.7-era FUnrealMCPProjectCommands class. Each command is a free
// function in the anonymous namespace + a file-scope REGISTER_MCP_COMMAND
// call at the bottom. No central dispatcher, no wrapping class. The
// auto-registration runs via the FAutoRegistrar pattern (anonymous-namespace
// struct whose constructor inserts into FMCPRegistry::Get() at DLL load).
//
// Tools:
//   create_input_mapping     legacy DefaultInput.ini action/axis mapping
//   get_ini / set_ini        DefaultEngine.ini etc. via GConfig
//   execute_python           v0.8.1 escape hatch via IPythonScriptPlugin

#include "Commands/UnrealMCPCommonUtils.h"
#include "MCPRegistry.h"

#include "GameFramework/InputSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
// v0.8.1 — execute_python escape hatch.
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"

namespace
{

// ─── Helpers ────────────────────────────────────────────────────────────────

// Convert a bare INI leaf name ("DefaultEngine.ini") into a full path
// under the project's Config/ directory. Absolute paths pass through.
FString ResolveIniPath(const FString& InFile)
{
    if (InFile.IsEmpty()) return FString();
    if (FPaths::IsRelative(InFile))
    {
        return FPaths::Combine(FPaths::ProjectConfigDir(), InFile);
    }
    return InFile;
}


// ─── create_input_mapping ───────────────────────────────────────────────────

TSharedPtr<FJsonObject> HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    FString Key;
    if (!Params->TryGetStringField(TEXT("key"), Key))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
    }

    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    FInputActionKeyMapping ActionMapping;
    ActionMapping.ActionName = FName(*ActionName);
    ActionMapping.Key = FKey(*Key);
    if (Params->HasField(TEXT("shift"))) { ActionMapping.bShift = Params->GetBoolField(TEXT("shift")); }
    if (Params->HasField(TEXT("ctrl")))  { ActionMapping.bCtrl  = Params->GetBoolField(TEXT("ctrl"));  }
    if (Params->HasField(TEXT("alt")))   { ActionMapping.bAlt   = Params->GetBoolField(TEXT("alt"));   }
    if (Params->HasField(TEXT("cmd")))   { ActionMapping.bCmd   = Params->GetBoolField(TEXT("cmd"));   }

    InputSettings->AddActionMapping(ActionMapping);
    InputSettings->SaveConfig();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("action_name"), ActionName);
    Result->SetStringField(TEXT("key"), Key);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}


// ─── get_ini / set_ini ──────────────────────────────────────────────────────
//
// Reads + writes project INI files (DefaultEngine.ini, DefaultGame.ini,
// DefaultInput.ini, etc.) via GConfig. Going through GConfig (not raw file
// read/write) means the change is live in the current editor AND persisted
// to disk on Flush — UE's config cache stays consistent.

TSharedPtr<FJsonObject> HandleGetIni(const TSharedPtr<FJsonObject>& Params)
{
    FString File, Section, Key;
    if (!Params->TryGetStringField(TEXT("file"), File) || File.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'file' parameter (e.g. 'DefaultEngine.ini')"));
    }
    if (!Params->TryGetStringField(TEXT("section"), Section) || Section.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'section' parameter (e.g. '/Script/Engine.Engine')"));
    }
    Params->TryGetStringField(TEXT("key"), Key);  // optional — empty key = dump section

    const FString IniPath = ResolveIniPath(File);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("file"), File);
    Result->SetStringField(TEXT("section"), Section);

    if (Key.IsEmpty())
    {
        // Section dump — return all key/value pairs.
        TArray<FString> Lines;
        GConfig->GetSection(*Section, Lines, IniPath);

        TSharedPtr<FJsonObject> Pairs = MakeShared<FJsonObject>();
        for (const FString& Line : Lines)
        {
            int32 Eq = INDEX_NONE;
            if (Line.FindChar(TEXT('='), Eq))
            {
                Pairs->SetStringField(Line.Left(Eq), Line.Mid(Eq + 1));
            }
        }
        Result->SetObjectField(TEXT("pairs"), Pairs);
        Result->SetNumberField(TEXT("pair_count"), Pairs->Values.Num());
    }
    else
    {
        FString Value;
        const bool bHit = GConfig->GetString(*Section, *Key, Value, IniPath);
        Result->SetStringField(TEXT("key"), Key);
        Result->SetStringField(TEXT("value"), bHit ? Value : FString());
        Result->SetBoolField(TEXT("present"), bHit);
    }

    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> HandleSetIni(const TSharedPtr<FJsonObject>& Params)
{
    FString File, Section, Key, Value;
    if (!Params->TryGetStringField(TEXT("file"), File) || File.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'file' parameter (e.g. 'DefaultEngine.ini')"));
    }
    if (!Params->TryGetStringField(TEXT("section"), Section) || Section.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'section' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("key"), Key) || Key.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("value"), Value))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter (string)"));
    }

    const FString IniPath = ResolveIniPath(File);

    FString PriorValue;
    const bool bHadValue = GConfig->GetString(*Section, *Key, PriorValue, IniPath);

    GConfig->SetString(*Section, *Key, *Value, IniPath);
    GConfig->Flush(false, IniPath);  // false = preserve other entries

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("file"), File);
    Result->SetStringField(TEXT("section"), Section);
    Result->SetStringField(TEXT("key"), Key);
    Result->SetStringField(TEXT("value"), Value);
    Result->SetStringField(TEXT("prior_value"), PriorValue);
    Result->SetBoolField(TEXT("created"), !bHadValue);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}


// ─── execute_python (v0.8.1 escape hatch, unsafe-by-default) ────────────────
//
// Closes the one capability gap relative to kvick-games/UnrealMCP. They
// expose arbitrary Python via execute_python; we deliberately don't have a
// typed wrapper for every possible UE Python API call. This tool gives the
// agent that fallback path -- gated behind an explicit unsafe=true flag.
//
// Wire response shape is {success, error?, stdout, stderr} -- intentionally
// NOT the strict {success, error?, ...payload} contract the typed 100+ use.
// This is the documented escape-hatch shape.

TSharedPtr<FJsonObject> HandleExecutePython(const TSharedPtr<FJsonObject>& Params)
{
    FString Code;
    if (!Params->TryGetStringField(TEXT("code"), Code) || Code.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'code' parameter (non-empty Python source)"));
    }

    bool bUnsafe = false;
    Params->TryGetBoolField(TEXT("unsafe"), bUnsafe);
    if (!bUnsafe)
    {
        TSharedPtr<FJsonObject> Refused = MakeShared<FJsonObject>();
        Refused->SetBoolField(TEXT("success"), false);
        Refused->SetStringField(TEXT("error"),
            TEXT("execute_python requires unsafe=true to run. This tool is an arbitrary-Python escape hatch -- pass unsafe=true to confirm you accept that there's no schema, type checking, or safety net on the code body."));
        Refused->SetStringField(TEXT("stdout"), FString());
        Refused->SetStringField(TEXT("stderr"), FString());
        return Refused;
    }

    IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
    if (!Python || !Python->IsPythonAvailable() || !Python->IsPythonInitialized())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("PythonScriptPlugin is not available or not initialized. Enable 'Python Editor Script Plugin' in Edit -> Plugins and restart the editor."));
    }

    FPythonCommandEx Command;
    Command.Command = Code;
    // ExecuteFile = multi-line "treat input as script body" (despite the
    // name). ExecuteStatement only accepts a single statement.
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
    Command.FileExecutionScope = EPythonFileExecutionScope::Private;

    const bool bOk = Python->ExecPythonCommandEx(Command);

    TArray<FString> StdoutLines;
    TArray<FString> StderrLines;
    for (const FPythonLogOutputEntry& Entry : Command.LogOutput)
    {
        if (Entry.Type == EPythonLogOutputType::Info)
        {
            StdoutLines.Add(Entry.Output);
        }
        else
        {
            StderrLines.Add(Entry.Output);
        }
    }

    FString Stdout = FString::Join(StdoutLines, TEXT(""));
    FString Stderr = FString::Join(StderrLines, TEXT(""));
    if (!bOk && !Command.CommandResult.IsEmpty())
    {
        Stderr = Command.CommandResult + TEXT("\n") + Stderr;
    }

    constexpr int32 MaxCaptureBytes = 32 * 1024;
    auto Truncate = [](FString& S) {
        if (S.Len() > MaxCaptureBytes)
        {
            S = S.Left(MaxCaptureBytes) + TEXT("\n... [truncated]");
        }
    };
    Truncate(Stdout);
    Truncate(Stderr);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bOk);
    Result->SetStringField(TEXT("stdout"), Stdout);
    Result->SetStringField(TEXT("stderr"), Stderr);
    if (!bOk)
    {
        Result->SetStringField(TEXT("error"),
            TEXT("Python execution failed -- see stderr for the traceback."));
    }
    return Result;
}

}  // anonymous namespace


// ─── Self-registration at definition site ──────────────────────────────────
//
// Each command name registers a handler pointer with FMCPRegistry at DLL
// load. Macro expands to a file-scope FAutoRegistrar struct whose
// constructor inserts into the singleton -- runs on initial editor open
// AND on every Live Coding patch reload (the file-scope static survives
// the patch). Same pattern as MCPRegistrations.cpp's central RegBatch
// used to use, now distributed to each handler's definition file.

REGISTER_MCP_COMMAND("create_input_mapping", &HandleCreateInputMapping);
REGISTER_MCP_COMMAND("get_ini",              &HandleGetIni);
REGISTER_MCP_COMMAND("set_ini",              &HandleSetIni);
REGISTER_MCP_COMMAND("execute_python",       &HandleExecutePython);
