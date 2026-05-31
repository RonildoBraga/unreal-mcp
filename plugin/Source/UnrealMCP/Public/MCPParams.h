#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * v0.8.0 — typed access to MCP request parameters.
 *
 * Wraps a TSharedPtr<FJsonObject> from the bridge and gives handlers one-line
 * access to typed values with consistent error messages. Replaces ~10 LOC of
 * `if (!Params->TryGet...) return Error(...)` per handler.
 *
 * Two flavors per type:
 *   - GetX(name, OutValue, OutError) — required, returns bool, fills OutError
 *     when missing/wrong type. Caller pattern is
 *         FString N; FString E;
 *         if (!P.GetString("name", N, E)) return FMCPResponse::Error(E);
 *   - GetXOr(name, default) — optional, returns the default when missing.
 *
 * Vector / Rotator accept either a 3-element JSON array `[X, Y, Z]` or an
 * object with `x`/`y`/`z` (case-insensitive) fields.
 */
class UNREALMCP_API FMCPParams
{
public:
	explicit FMCPParams(const TSharedPtr<FJsonObject>& InJson)
		: Json(InJson)
	{
	}

	// ─── Required (bool return + OutError) ────────────────────────────────

	bool GetString(const FString& Name, FString& OutValue, FString& OutError) const;
	bool GetInt   (const FString& Name, int32&   OutValue, FString& OutError) const;
	bool GetFloat (const FString& Name, float&   OutValue, FString& OutError) const;
	bool GetDouble(const FString& Name, double&  OutValue, FString& OutError) const;
	bool GetBool  (const FString& Name, bool&    OutValue, FString& OutError) const;

	bool GetVector (const FString& Name, FVector&  OutValue, FString& OutError) const;
	bool GetRotator(const FString& Name, FRotator& OutValue, FString& OutError) const;

	bool GetObject(const FString& Name, TSharedPtr<FJsonObject>& OutValue, FString& OutError) const;
	bool GetArray (const FString& Name, TArray<TSharedPtr<FJsonValue>>& OutValue, FString& OutError) const;

	bool GetStringArray(const FString& Name, TArray<FString>& OutValue, FString& OutError) const;
	bool GetIntArray   (const FString& Name, TArray<int32>&   OutValue, FString& OutError) const;
	bool GetFloatArray (const FString& Name, TArray<float>&   OutValue, FString& OutError) const;

	// ─── Optional (returns default if missing) ────────────────────────────

	FString GetStringOr(const FString& Name, const FString& Default = TEXT("")) const;
	int32   GetIntOr   (const FString& Name, int32   Default = 0) const;
	float   GetFloatOr (const FString& Name, float   Default = 0.0f) const;
	double  GetDoubleOr(const FString& Name, double  Default = 0.0)  const;
	bool    GetBoolOr  (const FString& Name, bool    Default = false) const;

	// ─── Introspection ────────────────────────────────────────────────────

	bool Has(const FString& Name) const
	{
		return Json.IsValid() && Json->HasField(Name);
	}

	/** Underlying JSON object — for handlers that need to forward params. */
	const TSharedPtr<FJsonObject>& Raw() const { return Json; }

private:
	TSharedPtr<FJsonObject> Json;
};
