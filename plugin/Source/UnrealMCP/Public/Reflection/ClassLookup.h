#pragma once

#include "CoreMinimal.h"

/**
 * v0.8.0 — name → UClass* lookup.
 *
 * Lifted from v0.7.10 spawn_actor's inline path resolution and generalized.
 * Used by actor spawning, asset class filters, CDO access — anywhere the
 * caller has a free-form class string and needs the UClass.
 *
 * Resolution order (first hit wins):
 *   1. Full /Script/Module.Type path                — exact load
 *   2. /Script/Engine.<Name>                        — common engine types
 *   3. /Script/UnrealEd.<Name>                      — editor-only types
 *   4. TryFindTypeSlow<UClass>(Name)                — anywhere in loaded modules
 *
 * The "A" prefix on actor classes (AStaticMeshActor) is handled by the
 * caller — pass the engine-internal name (StaticMeshActor) or the full path.
 */
class UNREALMCP_API FClassLookup
{
public:
	/**
	 * Resolve TypeName to a UClass*.
	 *
	 * Returns nullptr + fills OutError on failure.
	 *
	 * Examples that work:
	 *   - "StaticMeshActor"                          → AStaticMeshActor
	 *   - "/Script/Engine.StaticMeshActor"           → AStaticMeshActor
	 *   - "ExponentialHeightFog"                     → AExponentialHeightFog
	 *   - "SkyAtmosphere"                            → ASkyAtmosphere
	 *   - "PointLightComponent"                      → UPointLightComponent
	 */
	static UClass* Resolve(const FString& TypeName, FString& OutError);
};
