#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * v0.8.0 — strict response shape.
 *
 *   Success: { "success": true, ...payload merged at top level }
 *   Error:   { "success": false, "error": "<human-readable message>" }
 *
 * No `status`/`result`/`message` synonyms. No nested `data` envelope. Every
 * handler returns one of these three shapes; the Python @mcp_tool decorator
 * unwraps a single uniform contract.
 *
 * Header-only — all methods are small JSON-builder helpers. No .cpp.
 */
class FMCPResponse
{
public:
	/** `{ "success": true }`. */
	static TSharedPtr<FJsonObject> Success()
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("success"), true);
		return Out;
	}

	/**
	 * `{ "success": true, ...Payload }` — payload fields merged at the top
	 * level. Caller owns Payload; we copy values, not the ref.
	 *
	 * Payload may be null (equivalent to Success() with no extra fields).
	 * A `"success"` key in Payload is overwritten (true wins).
	 */
	static TSharedPtr<FJsonObject> Success(const TSharedPtr<FJsonObject>& Payload)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		if (Payload.IsValid())
		{
			for (const auto& Pair : Payload->Values)
			{
				Out->SetField(Pair.Key, Pair.Value);
			}
		}
		Out->SetBoolField(TEXT("success"), true);
		return Out;
	}

	/** `{ "success": false, "error": "<Message>" }`. */
	static TSharedPtr<FJsonObject> Error(const FString& Message)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("success"), false);
		Out->SetStringField(TEXT("error"), Message);
		return Out;
	}
};
