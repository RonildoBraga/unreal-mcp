#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Material-related MCP commands (Sprint 2).
 *
 * Five tools covering the common workflows: inspect parameters on a base
 * material or instance, create new material instances, tune their parameter
 * overrides, and find usage relationships.
 *
 * Tools:
 *   get_material_parameters           — scalar/vector/texture param names + values
 *   set_material_instance_param        — override a param on a material instance
 *   create_material_instance           — create a new MI from a parent material
 *   get_material_uses                  — assets that reference this material
 *   list_material_instances_of_parent  — every MI derived from a given parent
 *
 * Use case driving the work: Lauder Phase 7.2 — once Goddess Temple master
 * materials (M_BlendMaster, M_SSSMaster, M_StandardMaster, etc.) are migrated
 * into Lauder3, we'll create instances of them, tune scalar/vector/texture
 * params to suit the cozy temple alcove mood, and inspect what assets use
 * which.
 */
class UNREALMCP_API FUnrealMCPMaterialCommands
{
public:
    FUnrealMCPMaterialCommands();

    /** Top-level entry point used by UUnrealMCPBridge. */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleGetMaterialParameters(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceParam(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetMaterialUses(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListMaterialInstancesOfParent(const TSharedPtr<FJsonObject>& Params);
};
