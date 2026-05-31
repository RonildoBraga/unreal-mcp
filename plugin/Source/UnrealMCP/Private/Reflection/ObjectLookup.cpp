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
	// 4. /Game/.../Foo.Default or /Script/Module.Type.Default → class default object.
	//    Promised in v0.8.0 architecture plan §5; wired here in v0.8.1 after
	//    the test_resolve_target_engine_class_path integration test surfaced
	//    the gap (the prior version returned the UClass directly, which the
	//    PropertyWalker then rejected because UClass has no UPROPERTY of the
	//    name the caller actually wanted -- e.g. KillZ on AWorldSettings).
	if (Target.StartsWith(TEXT("/")))
	{
		FString PathToLoad = Target;
		bool bWantCDO = false;
		if (Target.EndsWith(TEXT(".Default")))
		{
			PathToLoad = Target.LeftChop(8);  // strip ".Default" (8 chars)
			bWantCDO = true;
		}

		UObject* Loaded = StaticLoadObject(UObject::StaticClass(), nullptr, *PathToLoad);
		if (!Loaded)
		{
			OutError = FString::Printf(TEXT("Object not found at path: %s"), *PathToLoad);
			return nullptr;
		}

		if (bWantCDO)
		{
			UClass* AsClass = Cast<UClass>(Loaded);
			if (!AsClass)
			{
				OutError = FString::Printf(
					TEXT(".Default suffix used but '%s' did not resolve to a UClass (got %s)"),
					*PathToLoad, *Loaded->GetClass()->GetName());
				return nullptr;
			}
			return AsClass->GetDefaultObject();
		}

		return Loaded;
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
