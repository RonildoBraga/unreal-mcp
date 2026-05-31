#pragma once

#include "CoreMinimal.h"

/**
 * v0.8.0 — generic UObject resolution from a target string.
 *
 * The leverage move from the §5 plan: lift `get/set_actor_property` to a
 * generalized `get/set_object_property` that works on ANY UObject. The only
 * thing that changes per call is how we get from the target string to the
 * UObject — that's this class.
 *
 * Supported target syntax (first match wins):
 *
 *   1. /Game/-prefixed path                — loaded as an asset
 *      "/Game/M_Stone/M_Stone.M_Stone"     → UMaterial*
 *      "/Game/BP_Door.BP_Door"             → UBlueprint*
 *      "/Game/BP_Door.BP_Door_C"           → UClass* (BP-generated class)
 *
 *   2. /Script/-prefixed path              — engine type or CDO
 *      "/Script/Engine.StaticMeshActor"    → UClass*
 *
 *   3. Actor display label or internal name in the current editor world
 *      "Altar_Lantern"                     → AActor*  (by GetActorLabel)
 *      "Altar_Lantern_2"                   → AActor*  (by GetName fallback)
 *
 * Component lookup ("Actor.Component") is NOT this class's job — that's
 * handled inside PropertyWalker via the `.`-segmented path. ObjectLookup
 * returns the root UObject; PropertyWalker descends from there.
 */
class UNREALMCP_API FObjectLookup
{
public:
	/**
	 * Resolve Target to a UObject*. Returns nullptr + fills OutError on
	 * failure. Failure modes:
	 *   - empty target
	 *   - /Game/... path doesn't resolve to a loaded asset
	 *   - no editor world available (e.g. cooked-only runs)
	 *   - actor name not found in the editor world
	 */
	static UObject* Resolve(const FString& Target, FString& OutError);
};
