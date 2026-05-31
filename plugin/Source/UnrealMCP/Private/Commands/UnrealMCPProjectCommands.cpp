#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/InputSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

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