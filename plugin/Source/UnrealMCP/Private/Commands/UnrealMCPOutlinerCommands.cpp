// v0.8.x §6.2 completion -- outliner command handlers, lifted out of the
// v0.7-era FUnrealMCPOutlinerCommands class.

#include "Commands/UnrealMCPCommonUtils.h"
#include "MCPRegistry.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "EditorActorFolders.h"

namespace
{

UWorld* EditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}


TSharedPtr<FJsonObject> HandleGetOutlinerFolders(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = EditorWorld();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    TSet<FString> UniqueFolders;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        const FName FolderPath = It->GetFolderPath();
        if (!FolderPath.IsNone())
        {
            UniqueFolders.Add(FolderPath.ToString());
        }
    }

    FActorFolders::Get().ForEachFolder(*World, [&UniqueFolders](const FFolder& Folder)
    {
        if (!Folder.IsNone())
        {
            UniqueFolders.Add(Folder.GetPath().ToString());
        }
        return true;
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


TSharedPtr<FJsonObject> HandleMoveActorToFolder(const TSharedPtr<FJsonObject>& Params)
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

    Target->SetFolderPath(FolderPath.IsEmpty() ? NAME_None : FName(*FolderPath));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), ActorName);
    Result->SetStringField(TEXT("folder_path"), FolderPath);
    Result->SetBoolField(TEXT("success"), true);
    return Result;
}


TSharedPtr<FJsonObject> HandleCreateOutlinerFolder(const TSharedPtr<FJsonObject>& Params)
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

    // Two-step construction avoids the "most vexing parse" — writing
    // `const FFolder Folder(FFolder::FRootObject(World), FName(*FolderPath));`
    // would be parsed as a function declaration.
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


TSharedPtr<FJsonObject> HandleGetActorsInFolder(const TSharedPtr<FJsonObject>& Params)
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


TSharedPtr<FJsonObject> HandleMoveActorToFolderBatch(const TSharedPtr<FJsonObject>& Params)
{
    const TArray<TSharedPtr<FJsonValue>>* MovesJson = nullptr;
    if (!Params->TryGetArrayField(TEXT("moves"), MovesJson) || MovesJson == nullptr)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'moves' parameter (array of {name, folder_path} objects)"));
    }

    UWorld* World = EditorWorld();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No editor world loaded"));
    }

    TMap<FString, AActor*> ByLabel;
    TMap<FString, AActor*> ByInternal;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* A = *It;
        if (!A) continue;
        ByLabel.Add(A->GetActorLabel(), A);
        ByInternal.Add(A->GetName(), A);
    }

    int32 MovedCount = 0;
    TArray<FString> Missing;

    for (const TSharedPtr<FJsonValue>& V : *MovesJson)
    {
        if (!V.IsValid() || V->Type != EJson::Object) continue;
        const TSharedPtr<FJsonObject>& M = V->AsObject();

        FString Name, FolderPath;
        if (!M->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
        {
            continue;
        }
        M->TryGetStringField(TEXT("folder_path"), FolderPath);

        AActor** Found = ByLabel.Find(Name);
        if (!Found) { Found = ByInternal.Find(Name); }
        if (!Found || !*Found)
        {
            Missing.Add(Name);
            continue;
        }

        (*Found)->SetFolderPath(FolderPath.IsEmpty() ? NAME_None : FName(*FolderPath));
        MovedCount++;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("requested_count"), MovesJson->Num());
    Result->SetNumberField(TEXT("moved_count"), MovedCount);

    TArray<TSharedPtr<FJsonValue>> MissingJson;
    MissingJson.Reserve(Missing.Num());
    for (const FString& Mname : Missing) { MissingJson.Add(MakeShared<FJsonValueString>(Mname)); }
    Result->SetArrayField(TEXT("missing"), MissingJson);

    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

}  // anonymous namespace


REGISTER_MCP_COMMAND("get_outliner_folders",       &HandleGetOutlinerFolders);
REGISTER_MCP_COMMAND("move_actor_to_folder",       &HandleMoveActorToFolder);
REGISTER_MCP_COMMAND("create_outliner_folder",     &HandleCreateOutlinerFolder);
REGISTER_MCP_COMMAND("get_actors_in_folder",       &HandleGetActorsInFolder);
REGISTER_MCP_COMMAND("move_actor_to_folder_batch", &HandleMoveActorToFolderBatch);
