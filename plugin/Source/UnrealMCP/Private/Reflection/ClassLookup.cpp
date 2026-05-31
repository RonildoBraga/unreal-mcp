#include "Reflection/ClassLookup.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

UClass* FClassLookup::Resolve(const FString& TypeName, FString& OutError)
{
	if (TypeName.IsEmpty())
	{
		OutError = TEXT("Empty type name");
		return nullptr;
	}

	UClass* Found = nullptr;

	// 1. Full /Script/Module.Type path.
	if (TypeName.Contains(TEXT(".")))
	{
		Found = LoadObject<UClass>(nullptr, *TypeName);
		if (Found) return Found;
	}

	// 2-3. Try common modules with the bare name.
	static const TCHAR* CandidateModules[] = {
		TEXT("Engine"),
		TEXT("UnrealEd"),
	};
	for (const TCHAR* Module : CandidateModules)
	{
		const FString FullPath = FString::Printf(TEXT("/Script/%s.%s"), Module, *TypeName);
		Found = LoadObject<UClass>(nullptr, *FullPath);
		if (Found) return Found;
	}

	// 4. Slow fallback — search every loaded module.
	Found = UClass::TryFindTypeSlow<UClass>(TypeName);
	if (Found) return Found;

	OutError = FString::Printf(TEXT("Class not found: %s"), *TypeName);
	return nullptr;
}
