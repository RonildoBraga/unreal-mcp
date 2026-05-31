// v0.8.x §6.2 completion -- level command handlers, lifted out of the
// v0.7-era FUnrealMCPLevelCommands class. Free functions in anonymous
// namespace + REGISTER_MCP_COMMAND self-registration at definition site.

#include "Commands/UnrealMCPCommonUtils.h"
#include "MCPRegistry.h"

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


TSharedPtr<FJsonObject> HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params)
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
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}


TSharedPtr<FJsonObject> HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
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


TSharedPtr<FJsonObject> HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params)
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

}  // anonymous namespace


REGISTER_MCP_COMMAND("get_current_level",   &HandleGetCurrentLevel);
REGISTER_MCP_COMMAND("open_level",          &HandleOpenLevel);
REGISTER_MCP_COMMAND("save_current_level",  &HandleSaveCurrentLevel);
