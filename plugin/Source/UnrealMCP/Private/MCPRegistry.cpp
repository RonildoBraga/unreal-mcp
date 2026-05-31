#include "MCPRegistry.h"
#include "MCPResponse.h"

FMCPRegistry& FMCPRegistry::Get()
{
	// Local-static singleton — well-defined init order, lazy.
	static FMCPRegistry Instance;
	return Instance;
}

void FMCPRegistry::Register(const FString& CommandName, FHandler Handler)
{
	if (Handlers.Contains(CommandName))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[UnrealMCP] Command '%s' re-registered — overwriting prior handler"),
		       *CommandName);
	}
	Handlers.Add(CommandName, MoveTemp(Handler));
}

TSharedPtr<FJsonObject> FMCPRegistry::Dispatch(const FString& CommandName,
                                               const TSharedPtr<FJsonObject>& Params) const
{
	const FHandler* Handler = Handlers.Find(CommandName);
	if (!Handler)
	{
		return FMCPResponse::Error(
			FString::Printf(TEXT("Unknown command: %s"), *CommandName));
	}
	return (*Handler)(Params);
}

bool FMCPRegistry::Has(const FString& CommandName) const
{
	return Handlers.Contains(CommandName);
}

TArray<FString> FMCPRegistry::GetAllCommandNames() const
{
	TArray<FString> Names;
	Handlers.GetKeys(Names);
	Names.Sort();
	return Names;
}

int32 FMCPRegistry::Num() const
{
	return Handlers.Num();
}
