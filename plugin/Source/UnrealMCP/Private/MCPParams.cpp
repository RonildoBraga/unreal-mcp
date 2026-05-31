#include "MCPParams.h"

// ─── Helpers ──────────────────────────────────────────────────────────────

namespace
{
	FString MissingError(const FString& Name, const TCHAR* Expected)
	{
		return FString::Printf(TEXT("Missing required parameter '%s' (expected %s)"),
		                       *Name, Expected);
	}

	FString WrongTypeError(const FString& Name, const TCHAR* Expected)
	{
		return FString::Printf(TEXT("Parameter '%s' has wrong type (expected %s)"),
		                       *Name, Expected);
	}

	/**
	 * Read x/y/z (case-insensitive) from a JSON object into Out. Returns false
	 * if any component is missing or non-numeric.
	 */
	bool ReadXYZObject(const TSharedPtr<FJsonObject>& Obj, double& X, double& Y, double& Z)
	{
		if (!Obj.IsValid()) return false;
		// JSON field lookup is case-sensitive in UE; try lowercase first.
		const auto Pick = [&](const FString& Lower, const FString& Upper, double& Out) -> bool
		{
			if (Obj->HasTypedField<EJson::Number>(Lower)) { Out = Obj->GetNumberField(Lower); return true; }
			if (Obj->HasTypedField<EJson::Number>(Upper)) { Out = Obj->GetNumberField(Upper); return true; }
			return false;
		};
		return Pick(TEXT("x"), TEXT("X"), X)
		    && Pick(TEXT("y"), TEXT("Y"), Y)
		    && Pick(TEXT("z"), TEXT("Z"), Z);
	}
}

// ─── Required ─────────────────────────────────────────────────────────────

bool FMCPParams::GetString(const FString& Name, FString& OutValue, FString& OutError) const
{
	if (!Json.IsValid() || !Json->HasField(Name))
	{
		OutError = MissingError(Name, TEXT("string")); return false;
	}
	if (!Json->TryGetStringField(Name, OutValue))
	{
		OutError = WrongTypeError(Name, TEXT("string")); return false;
	}
	return true;
}

bool FMCPParams::GetInt(const FString& Name, int32& OutValue, FString& OutError) const
{
	if (!Json.IsValid() || !Json->HasField(Name))
	{
		OutError = MissingError(Name, TEXT("integer")); return false;
	}
	if (!Json->TryGetNumberField(Name, OutValue))
	{
		OutError = WrongTypeError(Name, TEXT("integer")); return false;
	}
	return true;
}

bool FMCPParams::GetFloat(const FString& Name, float& OutValue, FString& OutError) const
{
	double D;
	if (!GetDouble(Name, D, OutError)) return false;
	OutValue = static_cast<float>(D);
	return true;
}

bool FMCPParams::GetDouble(const FString& Name, double& OutValue, FString& OutError) const
{
	if (!Json.IsValid() || !Json->HasField(Name))
	{
		OutError = MissingError(Name, TEXT("number")); return false;
	}
	if (!Json->TryGetNumberField(Name, OutValue))
	{
		OutError = WrongTypeError(Name, TEXT("number")); return false;
	}
	return true;
}

bool FMCPParams::GetBool(const FString& Name, bool& OutValue, FString& OutError) const
{
	if (!Json.IsValid() || !Json->HasField(Name))
	{
		OutError = MissingError(Name, TEXT("bool")); return false;
	}
	if (!Json->TryGetBoolField(Name, OutValue))
	{
		OutError = WrongTypeError(Name, TEXT("bool")); return false;
	}
	return true;
}

bool FMCPParams::GetVector(const FString& Name, FVector& OutValue, FString& OutError) const
{
	if (!Json.IsValid() || !Json->HasField(Name))
	{
		OutError = MissingError(Name, TEXT("vector [X,Y,Z] or {x,y,z}")); return false;
	}

	// Array form: [X, Y, Z]
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Json->TryGetArrayField(Name, Arr) && Arr && Arr->Num() >= 3)
	{
		OutValue.X = (*Arr)[0]->AsNumber();
		OutValue.Y = (*Arr)[1]->AsNumber();
		OutValue.Z = (*Arr)[2]->AsNumber();
		return true;
	}

	// Object form: {x,y,z}
	const TSharedPtr<FJsonObject>* Obj = nullptr;
	if (Json->TryGetObjectField(Name, Obj) && Obj)
	{
		double X = 0, Y = 0, Z = 0;
		if (ReadXYZObject(*Obj, X, Y, Z))
		{
			OutValue = FVector(X, Y, Z);
			return true;
		}
	}

	OutError = WrongTypeError(Name, TEXT("vector [X,Y,Z] or {x,y,z}"));
	return false;
}

bool FMCPParams::GetRotator(const FString& Name, FRotator& OutValue, FString& OutError) const
{
	if (!Json.IsValid() || !Json->HasField(Name))
	{
		OutError = MissingError(Name, TEXT("rotator [Pitch,Yaw,Roll]")); return false;
	}

	// Array form: [Pitch, Yaw, Roll]
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Json->TryGetArrayField(Name, Arr) && Arr && Arr->Num() >= 3)
	{
		OutValue.Pitch = (*Arr)[0]->AsNumber();
		OutValue.Yaw   = (*Arr)[1]->AsNumber();
		OutValue.Roll  = (*Arr)[2]->AsNumber();
		return true;
	}

	// Object form: {pitch, yaw, roll}
	const TSharedPtr<FJsonObject>* Obj = nullptr;
	if (Json->TryGetObjectField(Name, Obj) && Obj)
	{
		const auto Pick = [&](const FString& Lower, const FString& Upper, double& Out) -> bool
		{
			if ((*Obj)->HasTypedField<EJson::Number>(Lower)) { Out = (*Obj)->GetNumberField(Lower); return true; }
			if ((*Obj)->HasTypedField<EJson::Number>(Upper)) { Out = (*Obj)->GetNumberField(Upper); return true; }
			return false;
		};
		double P = 0, Y = 0, R = 0;
		if (Pick(TEXT("pitch"), TEXT("Pitch"), P)
		 && Pick(TEXT("yaw"),   TEXT("Yaw"),   Y)
		 && Pick(TEXT("roll"),  TEXT("Roll"),  R))
		{
			OutValue = FRotator(P, Y, R);
			return true;
		}
	}

	OutError = WrongTypeError(Name, TEXT("rotator [Pitch,Yaw,Roll]"));
	return false;
}

bool FMCPParams::GetObject(const FString& Name, TSharedPtr<FJsonObject>& OutValue, FString& OutError) const
{
	if (!Json.IsValid() || !Json->HasField(Name))
	{
		OutError = MissingError(Name, TEXT("object")); return false;
	}
	const TSharedPtr<FJsonObject>* Found = nullptr;
	if (!Json->TryGetObjectField(Name, Found) || !Found)
	{
		OutError = WrongTypeError(Name, TEXT("object")); return false;
	}
	OutValue = *Found;
	return true;
}

bool FMCPParams::GetArray(const FString& Name, TArray<TSharedPtr<FJsonValue>>& OutValue, FString& OutError) const
{
	if (!Json.IsValid() || !Json->HasField(Name))
	{
		OutError = MissingError(Name, TEXT("array")); return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Found = nullptr;
	if (!Json->TryGetArrayField(Name, Found) || !Found)
	{
		OutError = WrongTypeError(Name, TEXT("array")); return false;
	}
	OutValue = *Found;
	return true;
}

bool FMCPParams::GetStringArray(const FString& Name, TArray<FString>& OutValue, FString& OutError) const
{
	TArray<TSharedPtr<FJsonValue>> Raw;
	if (!GetArray(Name, Raw, OutError)) return false;
	OutValue.Reset(Raw.Num());
	for (int32 i = 0; i < Raw.Num(); ++i)
	{
		FString S;
		if (!Raw[i].IsValid() || !Raw[i]->TryGetString(S))
		{
			OutError = FString::Printf(TEXT("Parameter '%s' element %d is not a string"), *Name, i);
			return false;
		}
		OutValue.Add(MoveTemp(S));
	}
	return true;
}

bool FMCPParams::GetIntArray(const FString& Name, TArray<int32>& OutValue, FString& OutError) const
{
	TArray<TSharedPtr<FJsonValue>> Raw;
	if (!GetArray(Name, Raw, OutError)) return false;
	OutValue.Reset(Raw.Num());
	for (int32 i = 0; i < Raw.Num(); ++i)
	{
		double N = 0;
		if (!Raw[i].IsValid() || !Raw[i]->TryGetNumber(N))
		{
			OutError = FString::Printf(TEXT("Parameter '%s' element %d is not numeric"), *Name, i);
			return false;
		}
		OutValue.Add(static_cast<int32>(N));
	}
	return true;
}

bool FMCPParams::GetFloatArray(const FString& Name, TArray<float>& OutValue, FString& OutError) const
{
	TArray<TSharedPtr<FJsonValue>> Raw;
	if (!GetArray(Name, Raw, OutError)) return false;
	OutValue.Reset(Raw.Num());
	for (int32 i = 0; i < Raw.Num(); ++i)
	{
		double N = 0;
		if (!Raw[i].IsValid() || !Raw[i]->TryGetNumber(N))
		{
			OutError = FString::Printf(TEXT("Parameter '%s' element %d is not numeric"), *Name, i);
			return false;
		}
		OutValue.Add(static_cast<float>(N));
	}
	return true;
}

// ─── Optional ─────────────────────────────────────────────────────────────

FString FMCPParams::GetStringOr(const FString& Name, const FString& Default) const
{
	FString V; FString E;
	return GetString(Name, V, E) ? V : Default;
}

int32 FMCPParams::GetIntOr(const FString& Name, int32 Default) const
{
	int32 V; FString E;
	return GetInt(Name, V, E) ? V : Default;
}

float FMCPParams::GetFloatOr(const FString& Name, float Default) const
{
	float V; FString E;
	return GetFloat(Name, V, E) ? V : Default;
}

double FMCPParams::GetDoubleOr(const FString& Name, double Default) const
{
	double V; FString E;
	return GetDouble(Name, V, E) ? V : Default;
}

bool FMCPParams::GetBoolOr(const FString& Name, bool Default) const
{
	bool V; FString E;
	return GetBool(Name, V, E) ? V : Default;
}
