#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Selection.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// JSON Utilities
TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateErrorResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);
    ResponseObject->SetStringField(TEXT("error"), Message);
    return ResponseObject;
}

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), true);
    
    if (Data.IsValid())
    {
        ResponseObject->SetObjectField(TEXT("data"), Data);
    }
    
    return ResponseObject;
}

void FUnrealMCPCommonUtils::GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((int32)Value->AsNumber());
        }
    }
}

void FUnrealMCPCommonUtils::GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((float)Value->AsNumber());
        }
    }
}

FVector2D FUnrealMCPCommonUtils::GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector2D Result(0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 2)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
    }
    
    return Result;
}

FVector FUnrealMCPCommonUtils::GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
        Result.Z = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

FRotator FUnrealMCPCommonUtils::GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FRotator Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.Pitch = (float)(*JsonArray)[0]->AsNumber();
        Result.Yaw = (float)(*JsonArray)[1]->AsNumber();
        Result.Roll = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

// Blueprint Utilities
UBlueprint* FUnrealMCPCommonUtils::FindBlueprint(const FString& BlueprintName)
{
    return FindBlueprintByName(BlueprintName);
}

UBlueprint* FUnrealMCPCommonUtils::FindBlueprintByName(const FString& BlueprintName)
{
    FString AssetPath = TEXT("/Game/Blueprints/") + BlueprintName;
    return LoadObject<UBlueprint>(nullptr, *AssetPath);
}

UEdGraph* FUnrealMCPCommonUtils::FindOrCreateEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Try to find the event graph
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName().Contains(TEXT("EventGraph")))
        {
            return Graph;
        }
    }
    
    // Create a new event graph if none exists
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(TEXT("EventGraph")), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
    return NewGraph;
}

// Blueprint node utilities
UK2Node_Event* FUnrealMCPCommonUtils::CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Check for existing event node with this exact name
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Using existing event node with name %s (ID: %s)"), 
                *EventName, *EventNode->NodeGuid.ToString());
            return EventNode;
        }
    }

    // No existing node found, create a new one
    UK2Node_Event* EventNode = nullptr;
    
    // Find the function to create the event
    UClass* BlueprintClass = Blueprint->GeneratedClass;
    UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));
    
    if (EventFunction)
    {
        EventNode = NewObject<UK2Node_Event>(Graph);
        EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
        EventNode->NodePosX = Position.X;
        EventNode->NodePosY = Position.Y;
        Graph->AddNode(EventNode, true);
        EventNode->PostPlacedNewNode();
        EventNode->AllocateDefaultPins();
        UE_LOG(LogTemp, Display, TEXT("Created new event node with name %s (ID: %s)"), 
            *EventName, *EventNode->NodeGuid.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find function for event name: %s"), *EventName);
    }
    
    return EventNode;
}

UK2Node_CallFunction* FUnrealMCPCommonUtils::CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position)
{
    if (!Graph || !Function)
    {
        return nullptr;
    }
    
    UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
    FunctionNode->SetFromFunction(Function);
    FunctionNode->NodePosX = Position.X;
    FunctionNode->NodePosY = Position.Y;
    Graph->AddNode(FunctionNode, true);
    FunctionNode->CreateNewGuid();
    FunctionNode->PostPlacedNewNode();
    FunctionNode->AllocateDefaultPins();
    
    return FunctionNode;
}

UK2Node_VariableGet* FUnrealMCPCommonUtils::CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableGet* VariableGetNode = NewObject<UK2Node_VariableGet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableGetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableGetNode->NodePosX = Position.X;
        VariableGetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableGetNode, true);
        VariableGetNode->PostPlacedNewNode();
        VariableGetNode->AllocateDefaultPins();
        
        return VariableGetNode;
    }
    
    return nullptr;
}

UK2Node_VariableSet* FUnrealMCPCommonUtils::CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableSet* VariableSetNode = NewObject<UK2Node_VariableSet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableSetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableSetNode->NodePosX = Position.X;
        VariableSetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableSetNode, true);
        VariableSetNode->PostPlacedNewNode();
        VariableSetNode->AllocateDefaultPins();
        
        return VariableSetNode;
    }
    
    return nullptr;
}

UK2Node_InputAction* FUnrealMCPCommonUtils::CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_InputAction* InputActionNode = NewObject<UK2Node_InputAction>(Graph);
    InputActionNode->InputActionName = FName(*ActionName);
    InputActionNode->NodePosX = Position.X;
    InputActionNode->NodePosY = Position.Y;
    Graph->AddNode(InputActionNode, true);
    InputActionNode->CreateNewGuid();
    InputActionNode->PostPlacedNewNode();
    InputActionNode->AllocateDefaultPins();
    
    return InputActionNode;
}

UK2Node_Self* FUnrealMCPCommonUtils::CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
    SelfNode->NodePosX = Position.X;
    SelfNode->NodePosY = Position.Y;
    Graph->AddNode(SelfNode, true);
    SelfNode->CreateNewGuid();
    SelfNode->PostPlacedNewNode();
    SelfNode->AllocateDefaultPins();
    
    return SelfNode;
}

bool FUnrealMCPCommonUtils::ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                           UEdGraphNode* TargetNode, const FString& TargetPinName)
{
    if (!Graph || !SourceNode || !TargetNode)
    {
        return false;
    }
    
    UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);
    
    if (SourcePin && TargetPin)
    {
        SourcePin->MakeLinkTo(TargetPin);
        return true;
    }
    
    return false;
}

UEdGraphPin* FUnrealMCPCommonUtils::FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }
    
    // Log all pins for debugging
    UE_LOG(LogTemp, Display, TEXT("FindPin: Looking for pin '%s' (Direction: %d) in node '%s'"), 
           *PinName, (int32)Direction, *Node->GetName());
    
    for (UEdGraphPin* Pin : Node->Pins)
    {
        UE_LOG(LogTemp, Display, TEXT("  - Available pin: '%s', Direction: %d, Category: %s"), 
               *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
    }
    
    // First try exact match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found exact matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If no exact match and we're looking for a component reference, try case-insensitive match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) && 
            (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If we're looking for a component output and didn't find it by name, try to find the first data output pin
    if (Direction == EGPD_Output && Cast<UK2Node_VariableGet>(Node) != nullptr)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                UE_LOG(LogTemp, Display, TEXT("  - Found fallback data output pin: '%s'"), *Pin->PinName.ToString());
                return Pin;
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("  - No matching pin found for '%s'"), *PinName);
    return nullptr;
}

// Actor utilities
TSharedPtr<FJsonValue> FUnrealMCPCommonUtils::ActorToJson(AActor* Actor)
{
    if (!Actor)
    {
        return MakeShared<FJsonValueNull>();
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return MakeShared<FJsonValueObject>(ActorObject);
}

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::ActorToJsonObject(AActor* Actor, bool bDetailed)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return ActorObject;
}

UK2Node_Event* FUnrealMCPCommonUtils::FindExistingEventNode(UEdGraph* Graph, const FString& EventName)
{
    if (!Graph)
    {
        return nullptr;
    }

    // Look for existing event nodes
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Found existing event node with name: %s"), *EventName);
            return EventNode;
        }
    }

    return nullptr;
}

bool FUnrealMCPCommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName,
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
    
    // Handle different property types
    if (Property->IsA<FBoolProperty>())
    {
        ((FBoolProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }
    else if (Property->IsA<FIntProperty>())
    {
        int32 IntValue = static_cast<int32>(Value->AsNumber());
        FIntProperty* IntProperty = CastField<FIntProperty>(Property);
        if (IntProperty)
        {
            IntProperty->SetPropertyValue_InContainer(Object, IntValue);
            return true;
        }
    }
    else if (Property->IsA<FFloatProperty>())
    {
        ((FFloatProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (Property->IsA<FStrProperty>())
    {
        ((FStrProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp ? ByteProp->GetIntPropertyEnum() : nullptr;
        
        // If this is a TEnumAsByte property (has associated enum)
        if (EnumDef)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
                ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %d"), 
                      *PropertyName, ByteValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    uint8 ByteValue = FCString::Atoi(*EnumValueName);
                    ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %d"), 
                          *PropertyName, *EnumValueName, ByteValue);
                    return true;
                }
                
                // Handle qualified enum names (e.g., "Player0" or "EAutoReceiveInput::Player0")
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
        else
        {
            // Regular byte property
            uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
            ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
            return true;
        }
    }
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
        FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;
        
        if (EnumDef && UnderlyingNumericProp)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                int64 EnumValue = static_cast<int64>(Value->AsNumber());
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %lld"), 
                      *PropertyName, EnumValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    int64 EnumValue = FCString::Atoi64(*EnumValueName);
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                
                // Handle qualified enum names
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
    }
    
    // v0.7.4: FStructProperty handling — vectors, colors, rotators
    if (Property->IsA<FStructProperty>())
    {
        FStructProperty* StructProp = CastField<FStructProperty>(Property);
        UScriptStruct* StructType = StructProp ? StructProp->Struct : nullptr;

        if (!StructType)
        {
            OutErrorMessage = FString::Printf(TEXT("Struct property '%s' has no script struct"), *PropertyName);
            return false;
        }

        const FName StructName = StructType->GetFName();

        // Helper: parse a 3- or 4-vector from either a JSON array or object form.
        // Returns true on success and fills X/Y/Z/W (W defaults to 1 for objects
        // / arrays that don't supply it). The caller decides which channels to use.
        auto ParseVec4 = [&](double& X, double& Y, double& Z, double& W) -> bool
        {
            X = Y = Z = 0.0;
            W = 1.0;
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
                // Accept x/y/z/w and X/Y/Z/W and r/g/b/a + Pitch/Yaw/Roll
                auto Take = [&Obj](const TCHAR* A, const TCHAR* B, const TCHAR* C, double& Out)
                {
                    double V = 0;
                    if (Obj->TryGetNumberField(A, V) || (B && Obj->TryGetNumberField(B, V)) || (C && Obj->TryGetNumberField(C, V)))
                    {
                        Out = V;
                    }
                };
                Take(TEXT("x"), TEXT("X"), TEXT("r"), X);
                Take(TEXT("y"), TEXT("Y"), TEXT("g"), Y);
                Take(TEXT("z"), TEXT("Z"), TEXT("b"), Z);
                Take(TEXT("w"), TEXT("W"), TEXT("a"), W);
                // Rotator-specific aliases
                double V = 0;
                if (Obj->TryGetNumberField(TEXT("Pitch"), V) || Obj->TryGetNumberField(TEXT("pitch"), V)) X = V;
                if (Obj->TryGetNumberField(TEXT("Yaw"),   V) || Obj->TryGetNumberField(TEXT("yaw"),   V)) Y = V;
                if (Obj->TryGetNumberField(TEXT("Roll"),  V) || Obj->TryGetNumberField(TEXT("roll"),  V)) Z = V;
                return true;
            }
            return false;
        };

        if (StructName == NAME_Vector || StructType == TBaseStructure<FVector>::Get())
        {
            double X, Y, Z, W;
            if (!ParseVec4(X, Y, Z, W))
            {
                OutErrorMessage = TEXT("Vector property expects [x,y,z] array or {x,y,z} object");
                return false;
            }
            FVector* Dest = static_cast<FVector*>(PropertyAddr);
            *Dest = FVector(X, Y, Z);
            return true;
        }
        if (StructName == NAME_Rotator || StructType == TBaseStructure<FRotator>::Get())
        {
            double Pitch, Yaw, Roll, W;
            if (!ParseVec4(Pitch, Yaw, Roll, W))
            {
                OutErrorMessage = TEXT("Rotator property expects [pitch,yaw,roll] array or {pitch,yaw,roll} object");
                return false;
            }
            FRotator* Dest = static_cast<FRotator*>(PropertyAddr);
            *Dest = FRotator(Pitch, Yaw, Roll);
            return true;
        }
        if (StructType == TBaseStructure<FLinearColor>::Get())
        {
            double R, G, B, A;
            if (!ParseVec4(R, G, B, A))
            {
                OutErrorMessage = TEXT("LinearColor expects [r,g,b,a] array or {r,g,b,a} object (0..1 floats)");
                return false;
            }
            FLinearColor* Dest = static_cast<FLinearColor*>(PropertyAddr);
            *Dest = FLinearColor(R, G, B, A);
            return true;
        }
        if (StructType == TBaseStructure<FColor>::Get())
        {
            double R, G, B, A;
            if (!ParseVec4(R, G, B, A))
            {
                OutErrorMessage = TEXT("Color expects [r,g,b,a] array or {r,g,b,a} object (0..255 ints)");
                return false;
            }
            FColor* Dest = static_cast<FColor*>(PropertyAddr);
            *Dest = FColor(
                FMath::Clamp<int32>(static_cast<int32>(R), 0, 255),
                FMath::Clamp<int32>(static_cast<int32>(G), 0, 255),
                FMath::Clamp<int32>(static_cast<int32>(B), 0, 255),
                FMath::Clamp<int32>(static_cast<int32>(A), 0, 255));
            return true;
        }

        OutErrorMessage = FString::Printf(
            TEXT("Unsupported struct type '%s' for property '%s' (handles: Vector, Rotator, LinearColor, Color)"),
            *StructName.ToString(), *PropertyName);
        return false;
    }

    // v0.7.4: FObjectProperty handling — set asset/UObject references by path
    // Useful for setting things like SkeletalMeshComponent.SkeletalMesh,
    // PrimitiveComponent.Material via path strings rather than UObject pointers.
    if (Property->IsA<FObjectProperty>())
    {
        FObjectProperty* ObjProp = CastField<FObjectProperty>(Property);
        if (Value->Type != EJson::String)
        {
            OutErrorMessage = FString::Printf(
                TEXT("Object property '%s' expects a /Game/-prefixed string path"), *PropertyName);
            return false;
        }
        const FString AssetPath = Value->AsString();
        if (AssetPath.IsEmpty())
        {
            // Explicit null assignment is legitimate (clearing a slot).
            ObjProp->SetObjectPropertyValue_InContainer(Object, nullptr);
            return true;
        }
        UObject* Loaded = LoadObject<UObject>(nullptr, *AssetPath);
        if (!Loaded)
        {
            OutErrorMessage = FString::Printf(TEXT("Could not load asset at path: %s"), *AssetPath);
            return false;
        }
        // Type-check: target slot must accept this UClass
        UClass* ExpectedClass = ObjProp->PropertyClass;
        if (ExpectedClass && !Loaded->IsA(ExpectedClass))
        {
            OutErrorMessage = FString::Printf(
                TEXT("Loaded asset is a %s, but property '%s' expects %s"),
                *Loaded->GetClass()->GetName(), *PropertyName, *ExpectedClass->GetName());
            return false;
        }
        ObjProp->SetObjectPropertyValue_InContainer(Object, Loaded);
        return true;
    }

    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"),
                                    *Property->GetClass()->GetName(), *PropertyName);
    return false;
}


// v0.7.5 — internal helper. Pure address-based value setter (no UObject
// dependency in any branch). Used by SetPropertyAtTarget so that struct-owned
// leaves work the same way as UObject-owned ones.
static bool SetValueAtAddress(FProperty* Property, void* PropertyAddr,
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

    // Struct properties — vectors, colors, rotators
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

        OutErrorMessage = FString::Printf(TEXT("Unsupported struct '%s' (handles: Vector, Rotator, LinearColor, Color)"),
                                          *StructType->GetName());
        return false;
    }

    // Asset / UObject reference — load by path string
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

    // Byte property — handles regular bytes + TEnumAsByte
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


bool FUnrealMCPCommonUtils::WalkPropertyPath(UObject* Root, const FString& Path,
                                             FPropertyTarget& OutTarget, FString& OutErrorMessage)
{
    if (!Root)
    {
        OutErrorMessage = TEXT("Null root passed to WalkPropertyPath");
        return false;
    }

    TArray<FString> Segments;
    Path.ParseIntoArray(Segments, TEXT("."));
    if (Segments.Num() == 0)
    {
        OutErrorMessage = TEXT("Empty property path");
        return false;
    }

    // Initial state: walking from a UObject root.
    void*    ContainerAddr = Root;
    UStruct* ContainerType = Root->GetClass();
    UObject* OwningObj     = Root;

    // Walk every segment except the last. Each non-leaf hop must be either
    // FObjectProperty (traverse to inner UObject) or FStructProperty (compute
    // inner struct address and update container type).
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
            OutErrorMessage = FString::Printf(
                TEXT("Property or component '%s' not found on %s"),
                *Segment, *ContainerType->GetName());
            return false;
        }

        if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
        {
            UObject* Next = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(ContainerAddr));
            if (!Next)
            {
                OutErrorMessage = FString::Printf(
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
            // Step into the struct: container address moves to the struct's
            // memory location, container type becomes the struct's UScriptStruct.
            // OwningObj stays at the last UObject so PostEditChangeProperty
            // can still notify the editor that the actor's data changed.
            ContainerAddr = StructProp->ContainerPtrToValuePtr<void>(ContainerAddr);
            ContainerType = StructProp->Struct;
            continue;
        }

        OutErrorMessage = FString::Printf(
            TEXT("Path segment '%s' is a %s — not traversable (need FObjectProperty or FStructProperty)"),
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


bool FUnrealMCPCommonUtils::SetPropertyAtTarget(const FPropertyTarget& Target,
                                                const TSharedPtr<FJsonValue>& Value,
                                                FString& OutErrorMessage)
{
    if (!Target.ContainerAddress || !Target.ContainerType)
    {
        OutErrorMessage = TEXT("Invalid target — null container");
        return false;
    }

    FProperty* LeafProp = Target.ContainerType->FindPropertyByName(*Target.LeafPropertyName);
    if (!LeafProp)
    {
        OutErrorMessage = FString::Printf(
            TEXT("Leaf property '%s' not found on %s"),
            *Target.LeafPropertyName, *Target.ContainerType->GetName());
        return false;
    }

    void* LeafAddr = LeafProp->ContainerPtrToValuePtr<void>(Target.ContainerAddress);
    if (!SetValueAtAddress(LeafProp, LeafAddr, Value, OutErrorMessage))
    {
        return false;
    }

    // v0.7.9 — broadcast PostEditChangeProperty so the renderer/editor
    // refreshes. UActorComponent subclasses use this to call
    // MarkRenderStateDirty (lights, fog, primitive components), which is
    // what makes set_actor_property visibly affect the scene. Without it,
    // the FProperty value is written but the cached scene proxy keeps
    // the old value until the editor is restarted.
    if (Target.OwningObject)
    {
        FPropertyChangedEvent ChangeEvent(LeafProp, EPropertyChangeType::ValueSet);
        Target.OwningObject->PostEditChangeProperty(ChangeEvent);
    }
    return true;
}
