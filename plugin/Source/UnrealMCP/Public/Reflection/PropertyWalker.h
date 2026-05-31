#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * v0.8.0 — dotted property-path traversal.
 *
 * Lifted from v0.7.5 / v0.7.9 / v0.7.10 work in Commands/UnrealMCPCommonUtils.
 * Same code, dedicated home. The struct is renamed to FTarget here for
 * locality; FUnrealMCPCommonUtils::FPropertyTarget remains as a typedef so
 * existing call sites compile through Day 2.
 *
 * Path syntax:
 *   - "Intensity"                                      plain leaf
 *   - "PointLightComponent.Intensity"                  object hop → leaf
 *   - "Settings.AutoExposureBias"                      struct hop → leaf
 *   - "Settings.ColorGrading.Highlights.Saturation"    nested struct chain
 *   - "OverrideMaterials.0"                            array index leaf
 *   - "Settings.ColorSaturation.0"                     vec4 component leaf
 *
 * What walks:
 *   FObjectProperty   — follows pointer, container becomes the inner UObject
 *   FStructProperty   — descends into struct memory, container becomes the
 *                       UScriptStruct (OwningObject stays at last UObject so
 *                       PostEditChangeProperty still notifies the editor)
 *   FArrayProperty    — next segment must be numeric index. Element address
 *                       becomes container (descend) or the leaf override
 *                       (terminal index).
 *
 * Component fallback: when the container is an actor root and a segment
 * doesn't match a UPROPERTY, the walker scans GetComponents() for a
 * component whose name matches.
 */
class UNREALMCP_API FPropertyWalker
{
public:
	/**
	 * Where a dotted-path resolution lands. After a successful Walk the
	 * leaf is either named (FindPropertyByName(LeafPropertyName) on
	 * ContainerType, applied at ContainerAddress) or addressed directly
	 * (LeafPropertyOverride + LeafAddressOverride — the array-index case).
	 *
	 * OwningObject is the nearest enclosing UObject — held so SetValue can
	 * broadcast PostEditChangeProperty and trigger MarkRenderStateDirty on
	 * components like lights and fog.
	 *
	 * OuterPropertyName is the first path segment — handed to
	 * PostEditChangeProperty so the editor knows which top-level UPROPERTY
	 * conceptually changed.
	 */
	struct FTarget
	{
		void*    ContainerAddress  = nullptr;
		UStruct* ContainerType     = nullptr;
		UObject* OwningObject      = nullptr;
		FString  LeafPropertyName;
		FString  OuterPropertyName;

		FProperty* LeafPropertyOverride = nullptr;
		void*      LeafAddressOverride  = nullptr;
	};

	/**
	 * Walk Path from Root, filling OutTarget. Returns false + OutError on
	 * any failure (missing property, non-traversable hop, null reference,
	 * out-of-bounds array index, etc.).
	 */
	static bool Walk(UObject* Root, const FString& Path,
	                 FTarget& OutTarget, FString& OutError);

	/**
	 * Write Value at OutTarget's leaf. Handles bool/int/float/double/str/
	 * name/byte/enum/object/struct (Vector, Rotator, Vector4, LinearColor,
	 * Color). Broadcasts PostEditChangeProperty on OwningObject so the
	 * editor refreshes (this is what makes light/fog edits visible without
	 * an editor restart).
	 */
	static bool SetValue(const FTarget& Target,
	                     const TSharedPtr<FJsonValue>& Value, FString& OutError);

	/**
	 * Read the leaf as a JSON node:
	 *   Bool/Int/Float/Double/Byte → number
	 *   Str/Name/Enum              → string
	 *   Struct                     → [..] array (Vector/Rotator) or [r,g,b,a]
	 *   Object                     → asset path string ("" if null)
	 *   Array                      → {kind:"Array", length:N, inner:"..."}
	 * Returns nullptr + OutError on unsupported types.
	 */
	static TSharedPtr<FJsonValue> GetValue(const FTarget& Target, FString& OutError);
};
