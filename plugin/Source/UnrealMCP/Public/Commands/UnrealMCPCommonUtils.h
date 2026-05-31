#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations
class AActor;
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node_Event;
class UK2Node_CallFunction;
class UK2Node_VariableGet;
class UK2Node_VariableSet;
class UK2Node_InputAction;
class UK2Node_Self;
class UFunction;

/**
 * Common utilities for UnrealMCP commands
 */
class UNREALMCP_API FUnrealMCPCommonUtils
{
public:
    // JSON utilities
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Message);
    static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);
    static void GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray);
    static void GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray);
    static FVector2D GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    static FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    static FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    
    // Actor utilities
    static TSharedPtr<FJsonValue> ActorToJson(AActor* Actor);
    static TSharedPtr<FJsonObject> ActorToJsonObject(AActor* Actor, bool bDetailed = false);
    
    // Blueprint utilities
    static UBlueprint* FindBlueprint(const FString& BlueprintName);
    static UBlueprint* FindBlueprintByName(const FString& BlueprintName);
    static UEdGraph* FindOrCreateEventGraph(UBlueprint* Blueprint);
    
    // Blueprint node utilities
    static UK2Node_Event* CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position);
    static UK2Node_CallFunction* CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position);
    static UK2Node_VariableGet* CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position);
    static UK2Node_VariableSet* CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position);
    static UK2Node_InputAction* CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position);
    static UK2Node_Self* CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position);
    static bool ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                UEdGraphNode* TargetNode, const FString& TargetPinName);
    static UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction = EGPD_MAX);
    static UK2Node_Event* FindExistingEventNode(UEdGraph* Graph, const FString& EventName);

    // Property utilities
    static bool SetObjectProperty(UObject* Object, const FString& PropertyName,
                                 const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage);

    /**
     * v0.7.4 / v0.7.5 — describes where a dotted-path resolution ends up.
     *
     * `ContainerAddress` + `ContainerType` define what UE reflection needs to
     * look up the leaf property and set its value: a raw pointer to a UObject
     * or a struct, plus the UClass / UScriptStruct that describes the layout.
     * `OwningObject` is the nearest enclosing UObject (used for
     * `PostEditChangeProperty` so the editor refreshes), and `OuterPropertyName`
     * is the first path segment — handed to `PostEditChangeProperty` so the
     * editor knows which top-level actor UPROPERTY conceptually changed.
     */
    struct FPropertyTarget
    {
        void*    ContainerAddress  = nullptr;
        UStruct* ContainerType     = nullptr;
        UObject* OwningObject      = nullptr;
        FString  LeafPropertyName;
        FString  OuterPropertyName;

        // v0.7.10 — when the path ends in an array index (e.g. "OverrideMaterials.0"),
        // the leaf isn't a named UPROPERTY on a container; it's an element address
        // inside a TArray. The walker fills these and SetPropertyAtTarget honors
        // them in preference to the name-based lookup.
        FProperty* LeafPropertyOverride = nullptr;
        void*      LeafAddressOverride  = nullptr;
    };

    /**
     * Walk a dotted property path through FObjectProperty and FStructProperty
     * hops. Examples that all work:
     *   - "Intensity"                                      (plain — passes through)
     *   - "PointLightComponent.Intensity"                  (object hop → leaf)
     *   - "Settings.AutoExposureBias"                      (struct hop → leaf, v0.7.5)
     *   - "Settings.ColorGrading.Highlights.Saturation"    (nested struct chain, v0.7.5)
     *   - "PointLightComponent.LightFunctionMaterial"      (object hop → object leaf)
     *
     * On any failure (missing property, non-traversable hop, null ref) returns
     * false with OutErrorMessage describing where it broke.
     */
    static bool WalkPropertyPath(UObject* Root, const FString& Path,
                                 FPropertyTarget& OutTarget, FString& OutErrorMessage);

    /**
     * Set a JSON value at the leaf of a resolved FPropertyTarget. Handles every
     * type SetObjectProperty handles — they share an internal dispatch helper.
     */
    static bool SetPropertyAtTarget(const FPropertyTarget& Target,
                                    const TSharedPtr<FJsonValue>& Value,
                                    FString& OutErrorMessage);

    /**
     * v0.7.10 — read counterpart to SetPropertyAtTarget. Resolves the leaf
     * via the same FPropertyTarget shape (honoring array-index leaf overrides),
     * serializes the FProperty value to a JSON node matching the type:
     *   - Bool / Int / Float / Double / Byte → number
     *   - Str / Name / Enum                  → string
     *   - Struct (Vector, Rotator, LinearColor, Color, Vector4) → [..] array
     *   - Object reference                   → object path string ("" if null)
     *   - Array                              → {kind: "Array", length: N, inner: "..."}
     * Returns nullptr + error message on unsupported / unreadable types.
     */
    static TSharedPtr<FJsonValue> GetPropertyAtTarget(const FPropertyTarget& Target,
                                                      FString& OutErrorMessage);
};