#include "Commands/UnrealMCPLevelCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "LevelEditorSubsystem.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

namespace
{
    /** Resolve the level editor subsystem. Returns nullptr if not available. */
    ULevelEditorSubsystem* LevelSub()
    {
        return GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
    }
}


FUnrealMCPLevelCommands::FUnrealMCPLevelCommands()
{
}


TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_current_level"))   return HandleGetCurrentLevel(Params);
    if (CommandType == TEXT("open_level"))          return HandleOpenLevel(Params);
    if (CommandType == TEXT("save_current_level"))  return HandleSaveCurrentLevel(Params);
    if (CommandType == TEXT("save_all_dirty"))      return HandleSaveAllDirty(Params);

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown level command: %s"), *CommandType));
}


TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    // World's package name is the /Game/-prefixed path. Object path is package + leaf.
    const FString PackageName = World->GetPackage() ? World->GetPackage()->GetName() : FString();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), World->GetName());
    Result->SetStringField(TEXT("package_name"), PackageName);
    if (!PackageName.IsEmpty())
    {
        Result->SetStringField(TEXT("object_path"), FString::Printf(TEXT("%s.%s"), *PackageName, *World->GetName()));
    }
    Result->SetStringField(TEXT("map_name"), World->GetMapName());
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params->TryGetStringField(TEXT("level_path"), LevelPath) || LevelPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'level_path' parameter"));
    }

    ULevelEditorSubsystem* Sub = LevelSub();
    if (!Sub)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelEditorSubsystem unavailable"));
    }

    // LoadLevel accepts /Game/-prefixed package path. Object-path form ("...L_Base.L_Base")
    // also works — strip the trailing object suffix if present so callers can use either form.
    if (LevelPath.Contains(TEXT(".")))
    {
        LevelPath = FPackageName::ObjectPathToPackageName(LevelPath);
    }

    const bool bSuccess = Sub->LoadLevel(LevelPath);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("level_path"), LevelPath);
    Result->SetBoolField(TEXT("success"), bSuccess);
    if (!bSuccess)
    {
        Result->SetStringField(TEXT("note"),
            TEXT("LoadLevel returned False — typical causes: path doesn't exist, "
                 "level has unsaved changes the user must address first, or the level "
                 "depends on a sublevel that failed to load."));
    }
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    ULevelEditorSubsystem* Sub = LevelSub();
    if (!Sub)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelEditorSubsystem unavailable"));
    }

    const bool bSuccess = Sub->SaveCurrentLevel();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleSaveAllDirty(const TSharedPtr<FJsonObject>& Params)
{
    ULevelEditorSubsystem* Sub = LevelSub();
    if (!Sub)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("LevelEditorSubsystem unavailable"));
    }

    // SaveAllDirtyLevels returns void; success determined by whether anything failed
    // during the save process. UE displays its own UI for save conflicts, so this is
    // mostly fire-and-forget for the MCP layer.
    Sub->SaveAllDirtyLevels();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("note"),
        TEXT("SaveAllDirtyLevels invoked. UE handles save conflicts in its own UI; "
             "this response indicates only that the call was dispatched, not the "
             "outcome of every individual save."));
    return Result;
}
