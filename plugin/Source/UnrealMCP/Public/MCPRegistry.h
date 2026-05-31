#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * v0.8.0 — self-registering command map.
 *
 * Adding a new MCP command takes exactly one line in the .cpp that defines
 * its handler:
 *
 *     REGISTER_MCP_COMMAND("spawn_actor", &HandleSpawnActor);
 *
 * The macro emits an anonymous file-scope struct whose constructor runs at
 * static-init time and inserts the handler into the singleton map.
 *
 * The bridge then dispatches with a single call:
 *
 *     return FMCPRegistry::Get().Dispatch(CommandName, Params);
 *
 * No more if/else chain. No more bridge edits when adding commands.
 *
 * Handler signature: takes the raw params JSON, returns a response JSON
 * (always built via FMCPResponse::Success/Error so the shape is uniform).
 * Wrap with FMCPParams inside the handler for typed access.
 */
class UNREALMCP_API FMCPRegistry
{
public:
	using FHandler = TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>& /*Params*/)>;

	/** Singleton accessor — local-static so init order is well-defined. */
	static FMCPRegistry& Get();

	/**
	 * Register a command handler. Called by REGISTER_MCP_COMMAND at static
	 * init. Re-registering the same name overwrites (and logs a warning).
	 */
	void Register(const FString& CommandName, FHandler Handler);

	/**
	 * Look up and invoke a command. Returns an error response if the command
	 * is unknown. Any exception inside the handler will propagate — the bridge
	 * is responsible for serializing into wire format.
	 */
	TSharedPtr<FJsonObject> Dispatch(const FString& CommandName,
	                                 const TSharedPtr<FJsonObject>& Params) const;

	/** True if a handler is registered for CommandName. */
	bool Has(const FString& CommandName) const;

	/** All registered command names, sorted lexicographically. Used by the smoke test. */
	TArray<FString> GetAllCommandNames() const;

	/** Number of registered commands. */
	int32 Num() const;

private:
	TMap<FString, FHandler> Handlers;
};

// ─── Registration macro ───────────────────────────────────────────────────
//
// Standard C++ token-pasting trick: __LINE__ doesn't expand inside ## until
// you bounce it through one extra macro level.
//
// Usage:
//   REGISTER_MCP_COMMAND("spawn_actor", &HandleSpawnActor);
//   REGISTER_MCP_COMMAND("delete_actor", [](const TSharedPtr<FJsonObject>& P) {
//       ...
//   });

#define MCP_CONCAT_INNER(A, B) A##B
#define MCP_CONCAT(A, B) MCP_CONCAT_INNER(A, B)

#define REGISTER_MCP_COMMAND(CommandName, HandlerExpr)                         \
	namespace {                                                                \
	struct MCP_CONCAT(FMCPAutoReg_, __LINE__)                                  \
	{                                                                          \
		MCP_CONCAT(FMCPAutoReg_, __LINE__)()                                   \
		{                                                                      \
			FMCPRegistry::Get().Register(TEXT(CommandName), HandlerExpr);      \
		}                                                                      \
	};                                                                         \
	static MCP_CONCAT(FMCPAutoReg_, __LINE__) MCP_CONCAT(GMCPAutoReg_, __LINE__); \
	}
