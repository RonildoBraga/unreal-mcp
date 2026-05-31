#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/InputSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
// v0.8.1 — execute_python escape hatch.
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"

FUnrealMCPProjectCommands::FUnrealMCPProjectCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_input_mapping"))
    {
        return HandleCreateInputMapping(Params);
    }
    if (CommandType == TEXT("get_ini"))
    {
        return HandleGetIni(Params);
    }
    if (CommandType == TEXT("set_ini"))
    {
        return HandleSetIni(Params);
    }
    if (CommandType == TEXT("execute_python"))
    {
        return HandleExecutePython(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown project command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
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

    // Get the input settings
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    // Create the input action mapping
    FInputActionKeyMapping ActionMapping;
    ActionMapping.ActionName = FName(*ActionName);
    ActionMapping.Key = FKey(*Key);

    // Add modifiers if provided
    if (Params->HasField(TEXT("shift")))
    {
        ActionMapping.bShift = Params->GetBoolField(TEXT("shift"));
    }
    if (Params->HasField(TEXT("ctrl")))
    {
        ActionMapping.bCtrl = Params->GetBoolField(TEXT("ctrl"));
    }
    if (Params->HasField(TEXT("alt")))
    {
        ActionMapping.bAlt = Params->GetBoolField(TEXT("alt"));
    }
    if (Params->HasField(TEXT("cmd")))
    {
        ActionMapping.bCmd = Params->GetBoolField(TEXT("cmd"));
    }

    // Add the mapping
    InputSettings->AddActionMapping(ActionMapping);
    InputSettings->SaveConfig();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("action_name"), ActionName);
    ResultObj->SetStringField(TEXT("key"), Key);
    return ResultObj;
}


// ─── v0.8.0 Day 3-4 — INI file get/set ──────────────────────────────────────
//
// Reads + writes project INI files (DefaultEngine.ini, DefaultGame.ini,
// DefaultInput.ini, etc.) via GConfig. Section + key + value are
// canonical INI fields. The file argument accepts the bare DefaultXxx.ini
// leaf name — we resolve it under Project/Config/.
//
// Why not just edit the file via Bash? Because UE's GConfig caches the
// merged result of Engine + Project INIs. Editing the file on disk doesn't
// update the cache until a restart. Going through GConfig means the change
// is live AND persisted to disk on Flush.

namespace
{
    // Convert a bare INI leaf name ("DefaultEngine.ini") into a full path
    // under the project's Config/ directory. Absolute paths pass through.
    // Returns empty FString on invalid input.
    FString ResolveIniPath(const FString& InFile)
    {
        if (InFile.IsEmpty()) return FString();
        if (FPaths::IsRelative(InFile))
        {
            return FPaths::Combine(FPaths::ProjectConfigDir(), InFile);
        }
        return InFile;
    }
}

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleGetIni(const TSharedPtr<FJsonObject>& Params)
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
            // Each line is "Key=Value" — split on the first '='.
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

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleSetIni(const TSharedPtr<FJsonObject>& Params)
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

    // Read prior value so the caller can see what changed.
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


// ─── v0.8.1 — execute_python escape hatch (unsafe-by-default) ───────────────
//
// Closes the one capability gap relative to kvick-games/UnrealMCP. They expose
// arbitrary Python via execute_python; we deliberately don't have a typed
// wrapper for every possible UE Python API call (there are tens of thousands).
// This tool gives the agent that fallback path -- gated behind an explicit
// unsafe=true flag so the LLM has to opt in per call.
//
// Routes to IPythonScriptPlugin::ExecPythonCommandEx, which captures stdout /
// stderr / Python tracebacks back into FPythonCommandEx.CommandResult and
// .LogOutput. We unpack those into a {stdout, stderr} pair and surface
// success based on the plugin's return value (false on Python exception).
//
// Safety model -- documented in the Python wrapper docstring + at every error
// edge here:
//   - Without unsafe=true: refuse with a clean error, NO execution
//   - PythonScriptPlugin not loaded: clean error, NO execution
//   - Code execution failures: success=false, traceback in stderr
//   - Wire response shape is {success, error?, stdout, stderr} -- NOT the
//     strict {success, error?, ...payload} contract the typed 101 use. This
//     is the explicit escape hatch shape.
//
// Threading note: runs on the game thread (same as every other MCP handler
// here). Long-running Python code will freeze the editor -- a real-world
// limitation worth surfacing in the docstring.

TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleExecutePython(const TSharedPtr<FJsonObject>& Params)
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
    // ExecuteFile despite the name: this is the multi-line "treat the input as
    // a script body" mode. ExecuteStatement only accepts a single statement
    // and fails on multi-line input with "multiple statements found while
    // compiling a single statement" -- not useful for any real workflow.
    // EvaluateStatement is for single-expression-returns-value usage; that's
    // a future v0.8.2 nuance if we want repl-style return values.
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
    Command.FileExecutionScope = EPythonFileExecutionScope::Private;

    const bool bOk = Python->ExecPythonCommandEx(Command);

    // FPythonCommandEx packs Info-level log lines AND uncaught traceback into
    // LogOutput. CommandResult is empty for ExecuteStatement on success; on
    // failure it holds the formatted Python exception.
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
            // Warning + Error both into stderr -- the LLM doesn't care to
            // distinguish, and Python warnings frequently indicate something
            // worth surfacing.
            StderrLines.Add(Entry.Output);
        }
    }

    FString Stdout = FString::Join(StdoutLines, TEXT(""));
    FString Stderr = FString::Join(StderrLines, TEXT(""));
    if (!bOk && !Command.CommandResult.IsEmpty())
    {
        // CommandResult on failure is the Python traceback -- prepend it so
        // the LLM sees the immediate exception above the warning chatter.
        Stderr = Command.CommandResult + TEXT("\n") + Stderr;
    }

    // Truncate at 32 KB each so a runaway print loop doesn't blow the MCP
    // response budget. 32 KB is plenty for any reasonable diagnostic.
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