#include "Commands/UnrealMCPOutlinerCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "EditorActorFolders.h"

namespace
{
    /** Get the currently-loaded editor world; null if no world available. */
    UWorld* EditorWorld()
    {
        return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    }
}


FUnrealMCPOutlinerCommands::FUnrealMCPOutlinerCommands()
{
}


TSharedPtr<FJsonObject> FUnrealMCPOutlinerCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_outliner_folders"))    return HandleGetOutlinerFolders(Params);
    if (CommandType == TEXT("move_actor_to_folder"))    return HandleMoveActorToFolder(Params);
    if (CommandType == TEXT("create_outliner_folder"))  return HandleCreateOutlinerFolder(Params);
    if (CommandType == TEXT("get_actors_in_folder"))    return HandleGetActorsInFolder(Params);

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown outliner command: %s"), *CommandType));
}


TSharedPtr<FJsonObject> FUnrealMCPOutlinerCommands::HandleGetOutlinerFolders(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = EditorWorld();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    // Walk every actor and collect its folder label. The Outliner-visible folder
    // set is the union of these plus any pending folders registered via
    // FActorFolders that have no actors yet (which we add below).
    TSet<FString> UniqueFolders;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        const FName FolderPath = It->GetFolderPath();
        if (!FolderPath.IsNone())
        {
            UniqueFolders.Add(FolderPath.ToString());
        }
    }

    // Add any "pending empty" folders FActorFolders has registered for this world.
    FActorFolders::Get().ForEachFolder(*World, [&UniqueFolders](const FFolder& Folder)
    {
        if (!Folder.IsNone())
        {
            UniqueFolders.Add(Folder.GetPath().ToString());
        }
        return true; // continue iteration
    });

    TArray<TSharedPtr<FJsonValue>> Folders;
    Folders.Reserve(UniqueFolders.Num());
    for (const FString& F : UniqueFolders)
    {
        Folders.Add(MakeShared<FJsonValueString>(F));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("folders"), Folders);
    Result->SetNumberField(TEXT("count"), Folders.Num());
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPOutlinerCommands::HandleMoveActorToFolder(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName, FolderPath;
    if (!Params->TryGetStringField(TEXT("name"), ActorName) || ActorName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("folder_path"), FolderPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'folder_path' parameter"));
    }

    UWorld* World = EditorWorld();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    // Find the actor by label (matches Outliner display name, not internal name)
    AActor* Target = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
        {
            Target = *It;
            break;
        }
    }

    if (!Target)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Empty folder path moves to root
    Target->SetFolderPath(FolderPath.IsEmpty() ? NAME_None : FName(*FolderPath));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), ActorName);
    Result->SetStringField(TEXT("folder_path"), FolderPath);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPOutlinerCommands::HandleCreateOutlinerFolder(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("folder_path"), FolderPath) || FolderPath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'folder_path' parameter"));
    }

    UWorld* World = EditorWorld();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    // CreateFolder registers a pending empty folder for the level. Folders
    // generally need at least one actor to persist across reloads, but the
    // Outliner shows the pending entry immediately.
    //
    // Note the explicit two-step construction below: writing
    //   const FFolder Folder(FFolder::FRootObject(World), FName(*FolderPath));
    // hits the "most vexing parse" — the compiler interprets the whole line as
    // a function declaration. Splitting the args out forces variable-decl parsing.
    const FFolder::FRootObject Root(World);
    const FName FolderName(*FolderPath);
    const FFolder Folder(Root, FolderName);
    const bool bCreated = FActorFolders::Get().CreateFolder(*World, Folder);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("folder_path"), FolderPath);
    Result->SetBoolField(TEXT("success"), bCreated);
    if (!bCreated)
    {
        Result->SetStringField(TEXT("note"),
            TEXT("CreateFolder returned False — most likely the folder already exists, "
                 "or the world is in a state that doesn't accept new folders. "
                 "Not necessarily an error."));
    }
    return Result;
}


TSharedPtr<FJsonObject> FUnrealMCPOutlinerCommands::HandleGetActorsInFolder(const TSharedPtr<FJsonObject>& Params)
{
    FString FolderPath;
    if (!Params->TryGetStringField(TEXT("folder_path"), FolderPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'folder_path' parameter"));
    }

    UWorld* World = EditorWorld();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    const FName TargetFolder = FolderPath.IsEmpty() ? NAME_None : FName(*FolderPath);

    TArray<TSharedPtr<FJsonValue>> Actors;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetFolderPath() == TargetFolder)
        {
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), It->GetActorLabel());
            Entry->SetStringField(TEXT("class_name"), It->GetClass()->GetName());
            Entry->SetStringField(TEXT("internal_name"), It->GetName());
            Actors.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("folder_path"), FolderPath);
    Result->SetArrayField(TEXT("actors"), Actors);
    Result->SetNumberField(TEXT("count"), Actors.Num());
    return Result;
}
