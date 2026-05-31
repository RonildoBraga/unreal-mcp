#include "Reflection/ObjectLookup.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

UObject* FObjectLookup::Resolve(const FString& Target, FString& OutError)
{
	if (Target.IsEmpty())
	{
		OutError = TEXT("Empty target");
		return nullptr;
	}

	// 1-2. /Game/... or /Script/... → asset/class load.
	if (Target.StartsWith(TEXT("/")))
	{
		if (UObject* Loaded = StaticLoadObject(UObject::StaticClass(), nullptr, *Target))
		{
			return Loaded;
		}
		OutError = FString::Printf(TEXT("Object not found at path: %s"), *Target);
		return nullptr;
	}

	// 3. Actor lookup in the editor world.
	if (!GEditor)
	{
		OutError = TEXT("GEditor unavailable — cannot resolve actor names");
		return nullptr;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		OutError = TEXT("No editor world available — cannot resolve actor names");
		return nullptr;
	}

	// Two passes — display label first (what the user sees in the Outliner),
	// then internal name as fallback.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->GetActorLabel() == Target)
		{
			return A;
		}
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->GetName() == Target)
		{
			return A;
		}
	}

	OutError = FString::Printf(TEXT("Actor not found: %s"), *Target);
	return nullptr;
}
