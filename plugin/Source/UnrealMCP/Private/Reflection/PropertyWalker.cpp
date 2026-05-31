#include "Reflection/PropertyWalker.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "UObject/UnrealType.h"
#include "UObject/Field.h"
#include "UObject/EnumProperty.h"

// ─── Private helpers ──────────────────────────────────────────────────────
//
// Lifted from v0.7.5 / v0.7.10 work in Commands/UnrealMCPCommonUtils.cpp.
// File-scope static so they don't pollute any header — only Set/GetValue
// reach them.

namespace
{
	/**
	 * Pure address-based value writer. No UObject dependency in any branch,
	 * so struct-owned leaves work the same as UObject-owned ones.
	 */
	bool SetValueAtAddress(FProperty* Property, void* PropertyAddr,
	                       const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
	{
		if (!Property || !PropertyAddr)
		{
			OutErrorMessage = TEXT("Null property or container in SetValueAtAddress");
			return false;
		}
		const FString PropertyName = Property->GetName();

		if (Property->IsA<FBoolProperty>())
		{
			static_cast<FBoolProperty*>(Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
			return true;
		}
		if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
		{
			IntProperty->SetPropertyValue(PropertyAddr, static_cast<int32>(Value->AsNumber()));
			return true;
		}
		if (Property->IsA<FFloatProperty>())
		{
			static_cast<FFloatProperty*>(Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
			return true;
		}
		if (Property->IsA<FDoubleProperty>())
		{
			static_cast<FDoubleProperty*>(Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
			return true;
		}
		if (Property->IsA<FStrProperty>())
		{
			static_cast<FStrProperty*>(Property)->SetPropertyValue(PropertyAddr, Value->AsString());
			return true;
		}
		if (Property->IsA<FNameProperty>())
		{
			static_cast<FNameProperty*>(Property)->SetPropertyValue(PropertyAddr, FName(*Value->AsString()));
			return true;
		}

		// Struct properties — vectors, colors, rotators.
		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			UScriptStruct* StructType = StructProp->Struct;
			if (!StructType)
			{
				OutErrorMessage = FString::Printf(TEXT("Struct property '%s' has no script struct"), *PropertyName);
				return false;
			}

			auto ParseVec4 = [&](double& X, double& Y, double& Z, double& W) -> bool
			{
				X = Y = Z = 0.0; W = 1.0;
				if (Value->Type == EJson::Array)
				{
					const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();
					if (Arr.Num() >= 1) X = Arr[0]->AsNumber();
					if (Arr.Num() >= 2) Y = Arr[1]->AsNumber();
					if (Arr.Num() >= 3) Z = Arr[2]->AsNumber();
					if (Arr.Num() >= 4) W = Arr[3]->AsNumber();
					return true;
				}
				if (Value->Type == EJson::Object)
				{
					const TSharedPtr<FJsonObject>& Obj = Value->AsObject();
					if (!Obj.IsValid()) return false;
					auto Take = [&Obj](const TCHAR* A, const TCHAR* B, const TCHAR* C, double& Out) {
						double V = 0;
						if (Obj->TryGetNumberField(A, V) || (B && Obj->TryGetNumberField(B, V)) || (C && Obj->TryGetNumberField(C, V)))
							Out = V;
					};
					Take(TEXT("x"), TEXT("X"), TEXT("r"), X);
					Take(TEXT("y"), TEXT("Y"), TEXT("g"), Y);
					Take(TEXT("z"), TEXT("Z"), TEXT("b"), Z);
					Take(TEXT("w"), TEXT("W"), TEXT("a"), W);
					double V = 0;
					if (Obj->TryGetNumberField(TEXT("Pitch"), V) || Obj->TryGetNumberField(TEXT("pitch"), V)) X = V;
					if (Obj->TryGetNumberField(TEXT("Yaw"),   V) || Obj->TryGetNumberField(TEXT("yaw"),   V)) Y = V;
					if (Obj->TryGetNumberField(TEXT("Roll"),  V) || Obj->TryGetNumberField(TEXT("roll"),  V)) Z = V;
					return true;
				}
				return false;
			};

			if (StructType == TBaseStructure<FVector>::Get())
			{
				double X, Y, Z, W; if (!ParseVec4(X, Y, Z, W)) { OutErrorMessage = TEXT("Vector expects [x,y,z]"); return false; }
				*static_cast<FVector*>(PropertyAddr) = FVector(X, Y, Z);
				return true;
			}
			if (StructType == TBaseStructure<FRotator>::Get())
			{
				double P, Y, R, W; if (!ParseVec4(P, Y, R, W)) { OutErrorMessage = TEXT("Rotator expects [pitch,yaw,roll]"); return false; }
				*static_cast<FRotator*>(PropertyAddr) = FRotator(P, Y, R);
				return true;
			}
			if (StructType == TBaseStructure<FVector4>::Get())
			{
				double X, Y, Z, W; if (!ParseVec4(X, Y, Z, W)) { OutErrorMessage = TEXT("Vector4 expects [x,y,z,w]"); return false; }
				*static_cast<FVector4*>(PropertyAddr) = FVector4(X, Y, Z, W);
				return true;
			}
			if (StructType == TBaseStructure<FLinearColor>::Get())
			{
				double R, G, B, A; if (!ParseVec4(R, G, B, A)) { OutErrorMessage = TEXT("LinearColor expects [r,g,b,a] 0..1"); return false; }
				*static_cast<FLinearColor*>(PropertyAddr) = FLinearColor(R, G, B, A);
				return true;
			}
			if (StructType == TBaseStructure<FColor>::Get())
			{
				double R, G, B, A; if (!ParseVec4(R, G, B, A)) { OutErrorMessage = TEXT("Color expects [r,g,b,a] 0..255"); return false; }
				*static_cast<FColor*>(PropertyAddr) = FColor(
					FMath::Clamp<int32>((int32)R, 0, 255),
					FMath::Clamp<int32>((int32)G, 0, 255),
					FMath::Clamp<int32>((int32)B, 0, 255),
					FMath::Clamp<int32>((int32)A, 0, 255));
				return true;
			}

			OutErrorMessage = FString::Printf(TEXT("Unsupported struct '%s' (handles: Vector, Rotator, Vector4, LinearColor, Color)"),
			                                  *StructType->GetName());
			return false;
		}

		// Asset / UObject reference — load by path string.
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
		{
			if (Value->Type != EJson::String)
			{
				OutErrorMessage = FString::Printf(TEXT("Object property '%s' expects a /Game/-prefixed string"), *PropertyName);
				return false;
			}
			const FString AssetPath = Value->AsString();
			if (AssetPath.IsEmpty())
			{
				ObjProp->SetObjectPropertyValue(PropertyAddr, nullptr);
				return true;
			}
			UObject* Loaded = LoadObject<UObject>(nullptr, *AssetPath);
			if (!Loaded)
			{
				OutErrorMessage = FString::Printf(TEXT("Could not load asset: %s"), *AssetPath);
				return false;
			}
			if (ObjProp->PropertyClass && !Loaded->IsA(ObjProp->PropertyClass))
			{
				OutErrorMessage = FString::Printf(TEXT("Asset is %s, slot expects %s"),
				                                  *Loaded->GetClass()->GetName(), *ObjProp->PropertyClass->GetName());
				return false;
			}
			ObjProp->SetObjectPropertyValue(PropertyAddr, Loaded);
			return true;
		}

		// Byte property — regular bytes + TEnumAsByte.
		if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			if (Value->Type == EJson::Number)
			{
				ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(Value->AsNumber()));
				return true;
			}
			if (UEnum* EnumDef = ByteProp->GetIntPropertyEnum())
			{
				FString S = Value->AsString();
				if (S.Contains(TEXT("::"))) S.Split(TEXT("::"), nullptr, &S);
				int64 EnumValue = EnumDef->GetValueByNameString(S);
				if (EnumValue != INDEX_NONE)
				{
					ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
					return true;
				}
				OutErrorMessage = FString::Printf(TEXT("Enum value '%s' not found"), *S);
				return false;
			}
		}
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			UEnum* EnumDef = EnumProp->GetEnum();
			FNumericProperty* Numeric = EnumProp->GetUnderlyingProperty();
			if (EnumDef && Numeric)
			{
				int64 Out = 0;
				if (Value->Type == EJson::Number) Out = static_cast<int64>(Value->AsNumber());
				else
				{
					FString S = Value->AsString();
					if (S.Contains(TEXT("::"))) S.Split(TEXT("::"), nullptr, &S);
					Out = EnumDef->GetValueByNameString(S);
					if (Out == INDEX_NONE) { OutErrorMessage = FString::Printf(TEXT("Enum value '%s' not found"), *S); return false; }
				}
				Numeric->SetIntPropertyValue(PropertyAddr, Out);
				return true;
			}
		}

		OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"),
		                                  *Property->GetClass()->GetName(), *PropertyName);
		return false;
	}


	/**
	 * Read counterpart. Returns a JSON value matching the FProperty type,
	 * or nullptr + OutError on unsupported types.
	 */
	TSharedPtr<FJsonValue> GetValueAtAddress(FProperty* Property, const void* PropertyAddr,
	                                          FString& OutErrorMessage)
	{
		if (!Property || !PropertyAddr)
		{
			OutErrorMessage = TEXT("Null property or container in GetValueAtAddress");
			return nullptr;
		}
		const FString PropertyName = Property->GetName();

		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(PropertyAddr));
		}
		if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
		{
			return MakeShared<FJsonValueNumber>(IntProp->GetPropertyValue(PropertyAddr));
		}
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
		{
			return MakeShared<FJsonValueNumber>(FloatProp->GetPropertyValue(PropertyAddr));
		}
		if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
		{
			return MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue(PropertyAddr));
		}
		if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
		{
			return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(PropertyAddr));
		}
		if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
		{
			return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(PropertyAddr).ToString());
		}
		if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			const uint8 V = ByteProp->GetPropertyValue(PropertyAddr);
			if (UEnum* E = ByteProp->GetIntPropertyEnum())
			{
				return MakeShared<FJsonValueString>(E->GetNameStringByValue(V));
			}
			return MakeShared<FJsonValueNumber>(V);
		}
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			FNumericProperty* Numeric = EnumProp->GetUnderlyingProperty();
			UEnum* E = EnumProp->GetEnum();
			if (Numeric && E)
			{
				const int64 V = Numeric->GetSignedIntPropertyValue(PropertyAddr);
				return MakeShared<FJsonValueString>(E->GetNameStringByValue(V));
			}
		}
		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			UScriptStruct* StructType = StructProp->Struct;
			auto Pack3 = [](double X, double Y, double Z) {
				TArray<TSharedPtr<FJsonValue>> Arr;
				Arr.Add(MakeShared<FJsonValueNumber>(X));
				Arr.Add(MakeShared<FJsonValueNumber>(Y));
				Arr.Add(MakeShared<FJsonValueNumber>(Z));
				return MakeShared<FJsonValueArray>(Arr);
			};
			auto Pack4 = [](double X, double Y, double Z, double W) {
				TArray<TSharedPtr<FJsonValue>> Arr;
				Arr.Add(MakeShared<FJsonValueNumber>(X));
				Arr.Add(MakeShared<FJsonValueNumber>(Y));
				Arr.Add(MakeShared<FJsonValueNumber>(Z));
				Arr.Add(MakeShared<FJsonValueNumber>(W));
				return MakeShared<FJsonValueArray>(Arr);
			};

			if (StructType == TBaseStructure<FVector>::Get())
			{
				const FVector& V = *static_cast<const FVector*>(PropertyAddr);
				return Pack3(V.X, V.Y, V.Z);
			}
			if (StructType == TBaseStructure<FRotator>::Get())
			{
				const FRotator& R = *static_cast<const FRotator*>(PropertyAddr);
				return Pack3(R.Pitch, R.Yaw, R.Roll);
			}
			if (StructType == TBaseStructure<FVector4>::Get())
			{
				const FVector4& V = *static_cast<const FVector4*>(PropertyAddr);
				return Pack4(V.X, V.Y, V.Z, V.W);
			}
			if (StructType == TBaseStructure<FLinearColor>::Get())
			{
				const FLinearColor& C = *static_cast<const FLinearColor*>(PropertyAddr);
				return Pack4(C.R, C.G, C.B, C.A);
			}
			if (StructType == TBaseStructure<FColor>::Get())
			{
				const FColor& C = *static_cast<const FColor*>(PropertyAddr);
				return Pack4(C.R, C.G, C.B, C.A);
			}

			OutErrorMessage = FString::Printf(TEXT("Unsupported struct '%s' for read"),
			                                  *StructType->GetName());
			return nullptr;
		}
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
		{
			UObject* Obj = ObjProp->GetObjectPropertyValue(PropertyAddr);
			return MakeShared<FJsonValueString>(Obj ? Obj->GetPathName() : FString());
		}
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProp, PropertyAddr);
			TSharedPtr<FJsonObject> ArrObj = MakeShared<FJsonObject>();
			ArrObj->SetStringField(TEXT("kind"), TEXT("Array"));
			ArrObj->SetNumberField(TEXT("length"), Helper.Num());
			ArrObj->SetStringField(TEXT("inner"), ArrayProp->Inner->GetClass()->GetName());
			return MakeShared<FJsonValueObject>(ArrObj);
		}

		OutErrorMessage = FString::Printf(TEXT("Unsupported property type for read: %s on %s"),
		                                  *Property->GetClass()->GetName(), *PropertyName);
		return nullptr;
	}
}

// ─── Public API ───────────────────────────────────────────────────────────

bool FPropertyWalker::Walk(UObject* Root, const FString& Path,
                            FTarget& OutTarget, FString& OutError)
{
	if (!Root)
	{
		OutError = TEXT("Null root passed to FPropertyWalker::Walk");
		return false;
	}

	TArray<FString> Segments;
	Path.ParseIntoArray(Segments, TEXT("."));
	if (Segments.Num() == 0)
	{
		OutError = TEXT("Empty property path");
		return false;
	}

	// Initial state: walking from a UObject root.
	void*    ContainerAddr = Root;
	UStruct* ContainerType = Root->GetClass();
	UObject* OwningObj     = Root;

	// Walk every segment except the last.
	for (int32 i = 0; i < Segments.Num() - 1; i++)
	{
		const FString& Segment = Segments[i];

		FProperty* Prop = ContainerType->FindPropertyByName(*Segment);

		// Fallback for actor root: try GetComponents() by name. Runtime-added
		// components may not have UPROPERTY backing on the actor itself.
		if (!Prop && ContainerAddr == OwningObj)
		{
			AActor* AsActor = Cast<AActor>(OwningObj);
			if (AsActor)
			{
				UActorComponent* Found = nullptr;
				for (UActorComponent* Comp : AsActor->GetComponents())
				{
					if (Comp && Comp->GetName().Equals(Segment, ESearchCase::IgnoreCase))
					{
						Found = Comp;
						break;
					}
				}
				if (Found)
				{
					ContainerAddr = Found;
					ContainerType = Found->GetClass();
					OwningObj     = Found;
					continue;
				}
			}
		}

		if (!Prop)
		{
			OutError = FString::Printf(
				TEXT("Property or component '%s' not found on %s"),
				*Segment, *ContainerType->GetName());
			return false;
		}

		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
		{
			UObject* Next = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(ContainerAddr));
			if (!Next)
			{
				OutError = FString::Printf(
					TEXT("Path segment '%s' resolves to a null UObject reference"), *Segment);
				return false;
			}
			ContainerAddr = Next;
			ContainerType = Next->GetClass();
			OwningObj     = Next;
			continue;
		}

		if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
		{
			ContainerAddr = StructProp->ContainerPtrToValuePtr<void>(ContainerAddr);
			ContainerType = StructProp->Struct;
			continue;
		}

		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
		{
			if (i + 1 >= Segments.Num())
			{
				OutError = FString::Printf(
					TEXT("Array property '%s' is missing an index segment"), *Segment);
				return false;
			}
			const FString& IdxSeg = Segments[i + 1];
			if (!IdxSeg.IsNumeric())
			{
				OutError = FString::Printf(
					TEXT("Array property '%s' expects a numeric index next, got '%s'"),
					*Segment, *IdxSeg);
				return false;
			}
			const int32 Idx = FCString::Atoi(*IdxSeg);
			FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ContainerAddr));
			if (Idx < 0 || Idx >= Helper.Num())
			{
				OutError = FString::Printf(
					TEXT("Array '%s' index %d out of bounds (size %d)"),
					*Segment, Idx, Helper.Num());
				return false;
			}
			void* ElementAddr = Helper.GetRawPtr(Idx);
			FProperty* Inner = ArrayProp->Inner;

			const bool bIndexIsFinalHop = (i + 1 == Segments.Num() - 1);
			if (bIndexIsFinalHop)
			{
				OutTarget.LeafPropertyOverride = Inner;
				OutTarget.LeafAddressOverride  = ElementAddr;
				OutTarget.OwningObject         = OwningObj;
				OutTarget.LeafPropertyName     = IdxSeg;
				OutTarget.OuterPropertyName    = Segments[0];
				OutTarget.ContainerAddress     = ContainerAddr;
				OutTarget.ContainerType        = ContainerType;
				return true;
			}

			if (FObjectProperty* InnerObj = CastField<FObjectProperty>(Inner))
			{
				UObject* Next = InnerObj->GetObjectPropertyValue(ElementAddr);
				if (!Next)
				{
					OutError = FString::Printf(
						TEXT("Array element '%s[%d]' is a null UObject reference"), *Segment, Idx);
					return false;
				}
				ContainerAddr = Next;
				ContainerType = Next->GetClass();
				OwningObj     = Next;
			}
			else if (FStructProperty* InnerStruct = CastField<FStructProperty>(Inner))
			{
				ContainerAddr = ElementAddr;
				ContainerType = InnerStruct->Struct;
			}
			else
			{
				OutError = FString::Printf(
					TEXT("Array element '%s[%d]' is a %s — can't descend further (need Object or Struct inner)"),
					*Segment, Idx, *Inner->GetClass()->GetName());
				return false;
			}

			i++;  // consume the index segment we just used
			continue;
		}

		OutError = FString::Printf(
			TEXT("Path segment '%s' is a %s — not traversable (need FObjectProperty, FStructProperty, or FArrayProperty)"),
			*Segment, *Prop->GetClass()->GetName());
		return false;
	}

	OutTarget.ContainerAddress  = ContainerAddr;
	OutTarget.ContainerType     = ContainerType;
	OutTarget.OwningObject      = OwningObj;
	OutTarget.LeafPropertyName  = Segments.Last();
	OutTarget.OuterPropertyName = Segments[0];
	return true;
}


bool FPropertyWalker::SetValue(const FTarget& Target,
                                const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
	// Array-index leaves bypass the named lookup entirely.
	FProperty* LeafProp = Target.LeafPropertyOverride;
	void*      LeafAddr = Target.LeafAddressOverride;

	if (!LeafProp)
	{
		if (!Target.ContainerAddress || !Target.ContainerType)
		{
			OutError = TEXT("Invalid target — null container");
			return false;
		}

		LeafProp = Target.ContainerType->FindPropertyByName(*Target.LeafPropertyName);
		if (!LeafProp)
		{
			OutError = FString::Printf(
				TEXT("Leaf property '%s' not found on %s"),
				*Target.LeafPropertyName, *Target.ContainerType->GetName());
			return false;
		}

		LeafAddr = LeafProp->ContainerPtrToValuePtr<void>(Target.ContainerAddress);
	}
	if (!SetValueAtAddress(LeafProp, LeafAddr, Value, OutError))
	{
		return false;
	}

	// Broadcast PostEditChangeProperty so the renderer/editor refreshes.
	// UActorComponent subclasses use this to call MarkRenderStateDirty
	// (lights, fog, primitive components), which is what makes
	// set_actor_property visibly affect the scene.
	if (Target.OwningObject)
	{
		FPropertyChangedEvent ChangeEvent(LeafProp, EPropertyChangeType::ValueSet);
		Target.OwningObject->PostEditChangeProperty(ChangeEvent);
	}
	return true;
}


TSharedPtr<FJsonValue> FPropertyWalker::GetValue(const FTarget& Target, FString& OutError)
{
	FProperty* LeafProp = Target.LeafPropertyOverride;
	const void* LeafAddr = Target.LeafAddressOverride;

	if (!LeafProp)
	{
		if (!Target.ContainerAddress || !Target.ContainerType)
		{
			OutError = TEXT("Invalid target — null container");
			return nullptr;
		}
		LeafProp = Target.ContainerType->FindPropertyByName(*Target.LeafPropertyName);
		if (!LeafProp)
		{
			OutError = FString::Printf(
				TEXT("Leaf property '%s' not found on %s"),
				*Target.LeafPropertyName, *Target.ContainerType->GetName());
			return nullptr;
		}
		LeafAddr = LeafProp->ContainerPtrToValuePtr<void>(Target.ContainerAddress);
	}

	return GetValueAtAddress(LeafProp, LeafAddr, OutError);
}
