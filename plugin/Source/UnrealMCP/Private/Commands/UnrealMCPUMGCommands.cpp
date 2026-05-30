#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"
// We'll create widgets using regular Factory classes
#include "Factories/Factory.h"
// Remove problematic includes that don't exist in UE 5.5
// #include "UMGEditorSubsystem.h"
// #include "WidgetBlueprintFactory.h"
#include "WidgetBlueprintEditor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/Button.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_Event.h"

FUnrealMCPUMGCommands::FUnrealMCPUMGCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandName == TEXT("create_umg_widget_blueprint"))
	{
		return HandleCreateUMGWidgetBlueprint(Params);
	}
	else if (CommandName == TEXT("add_text_block_to_widget"))
	{
		return HandleAddTextBlockToWidget(Params);
	}
	else if (CommandName == TEXT("add_widget_to_viewport"))
	{
		return HandleAddWidgetToViewport(Params);
	}
	else if (CommandName == TEXT("add_button_to_widget"))
	{
		return HandleAddButtonToWidget(Params);
	}
	else if (CommandName == TEXT("bind_widget_event"))
	{
		return HandleBindWidgetEvent(Params);
	}
	else if (CommandName == TEXT("set_text_block_binding"))
	{
		return HandleSetTextBlockBinding(Params);
	}
	// === v2 commands (path-flexible, parent-aware, with property setters) ===
	else if (CommandName == TEXT("add_widget_to_tree"))
	{
		return HandleAddWidgetToTree(Params);
	}
	else if (CommandName == TEXT("set_widget_text"))
	{
		return HandleSetWidgetText(Params);
	}
	else if (CommandName == TEXT("set_progress_bar_percent"))
	{
		return HandleSetProgressBarPercent(Params);
	}
	else if (CommandName == TEXT("set_progress_bar_fill_color"))
	{
		return HandleSetProgressBarFillColor(Params);
	}
	else if (CommandName == TEXT("set_horizontal_box_slot_fill"))
	{
		return HandleSetHorizontalBoxSlotFill(Params);
	}
	else if (CommandName == TEXT("set_canvas_slot_anchor"))
	{
		return HandleSetCanvasSlotAnchor(Params);
	}
	else if (CommandName == TEXT("delete_widget_from_tree"))
	{
		return HandleDeleteWidgetFromTree(Params);
	}
	else if (CommandName == TEXT("compile_widget_blueprint"))
	{
		return HandleCompileWidgetBlueprint(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown UMG command: %s"), *CommandName));
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	// Create the full asset path
	FString PackagePath = TEXT("/Game/Widgets/");
	FString AssetName = BlueprintName;
	FString FullPath = PackagePath + AssetName;

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' already exists"), *BlueprintName));
	}

	// Create package
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
	}

	// Create Widget Blueprint using KismetEditorUtilities
	UBlueprint* NewBlueprint = FKismetEditorUtilities::CreateBlueprint(
		UUserWidget::StaticClass(),  // Parent class
		Package,                     // Outer package
		FName(*AssetName),           // Blueprint name
		BPTYPE_Normal,               // Blueprint type
		UBlueprint::StaticClass(),   // Blueprint class
		UBlueprintGeneratedClass::StaticClass(), // Generated class
		FName("CreateUMGWidget")     // Creation method name
	);

	// Make sure the Blueprint was created successfully
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(NewBlueprint);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Widget Blueprint"));
	}

	// Add a default Canvas Panel if one doesn't exist
	if (!WidgetBlueprint->WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetBlueprint->WidgetTree->RootWidget = RootCanvas;
	}

	// Mark the package dirty and notify asset registry
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(WidgetBlueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), FullPath);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	// Find the Widget Blueprint
	FString FullPath = TEXT("/Game/Widgets/") + BlueprintName;
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(FullPath));
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *BlueprintName));
	}

	// Get optional parameters
	FString InitialText = TEXT("New Text Block");
	Params->TryGetStringField(TEXT("text"), InitialText);

	FVector2D Position(0.0f, 0.0f);
	if (Params->HasField(TEXT("position")))
	{
		const TArray<TSharedPtr<FJsonValue>>* PosArray;
		if (Params->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
		{
			Position.X = (*PosArray)[0]->AsNumber();
			Position.Y = (*PosArray)[1]->AsNumber();
		}
	}

	// Create Text Block widget
	UTextBlock* TextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *WidgetName);
	if (!TextBlock)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Text Block widget"));
	}

	// Set initial text
	TextBlock->SetText(FText::FromString(InitialText));

	// Add to canvas panel
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root Canvas Panel not found"));
	}

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(TextBlock);
	PanelSlot->SetPosition(Position);

	// Mark the package dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
	ResultObj->SetStringField(TEXT("text"), InitialText);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	// Find the Widget Blueprint
	FString FullPath = TEXT("/Game/Widgets/") + BlueprintName;
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(FullPath));
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *BlueprintName));
	}

	// Get optional Z-order parameter
	int32 ZOrder = 0;
	Params->TryGetNumberField(TEXT("z_order"), ZOrder);

	// Create widget instance
	UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
	if (!WidgetClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get widget class"));
	}

	// Note: This creates the widget but doesn't add it to viewport
	// The actual addition to viewport should be done through Blueprint nodes
	// as it requires a game context

	// Create success response with instructions
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("class_path"), WidgetClass->GetPathName());
	ResultObj->SetNumberField(TEXT("z_order"), ZOrder);
	ResultObj->SetStringField(TEXT("note"), TEXT("Widget class ready. Use CreateWidget and AddToViewport nodes in Blueprint to display in game."));
	return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString ButtonText;
	if (!Params->TryGetStringField(TEXT("text"), ButtonText))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing text parameter"));
		return Response;
	}

	// Load the Widget Blueprint
	const FString BlueprintPath = FString::Printf(TEXT("/Game/Widgets/%s.%s"), *BlueprintName, *BlueprintName);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintPath));
		return Response;
	}

	// Create Button widget
	UButton* Button = NewObject<UButton>(WidgetBlueprint->GeneratedClass->GetDefaultObject(), UButton::StaticClass(), *WidgetName);
	if (!Button)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to create Button widget"));
		return Response;
	}

	// Set button text
	UTextBlock* ButtonTextBlock = NewObject<UTextBlock>(Button, UTextBlock::StaticClass(), *(WidgetName + TEXT("_Text")));
	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(FText::FromString(ButtonText));
		Button->AddChild(ButtonTextBlock);
	}

	// Get canvas panel and add button
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		Response->SetStringField(TEXT("error"), TEXT("Root widget is not a Canvas Panel"));
		return Response;
	}

	// Add to canvas and set position
	UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(Button);
	if (ButtonSlot)
	{
		const TArray<TSharedPtr<FJsonValue>>* Position;
		if (Params->TryGetArrayField(TEXT("position"), Position) && Position->Num() >= 2)
		{
			FVector2D Pos(
				(*Position)[0]->AsNumber(),
				(*Position)[1]->AsNumber()
			);
			ButtonSlot->SetPosition(Pos);
		}
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString EventName;
	if (!Params->TryGetStringField(TEXT("event_name"), EventName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing event_name parameter"));
		return Response;
	}

	// Load the Widget Blueprint
	const FString BlueprintPath = FString::Printf(TEXT("/Game/Widgets/%s.%s"), *BlueprintName, *BlueprintName);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintPath));
		return Response;
	}

	// Create the event graph if it doesn't exist
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBlueprint);
	if (!EventGraph)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to find or create event graph"));
		return Response;
	}

	// Find the widget in the blueprint
	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(*WidgetName);
	if (!Widget)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find widget: %s"), *WidgetName));
		return Response;
	}

	// Create the event node (e.g., OnClicked for buttons)
	UK2Node_Event* EventNode = nullptr;
	
	// Find existing nodes first
	TArray<UK2Node_Event*> AllEventNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(WidgetBlueprint, AllEventNodes);
	
	for (UK2Node_Event* Node : AllEventNodes)
	{
		if (Node->CustomFunctionName == FName(*EventName) && Node->EventReference.GetMemberParentClass() == Widget->GetClass())
		{
			EventNode = Node;
			break;
		}
	}

	// If no existing node, create a new one
	if (!EventNode)
	{
		// Calculate position - place it below existing nodes
		float MaxHeight = 0.0f;
		for (UEdGraphNode* Node : EventGraph->Nodes)
		{
			MaxHeight = FMath::Max(MaxHeight, Node->NodePosY);
		}
		
		const FVector2D NodePos(200, MaxHeight + 200);

		// Call CreateNewBoundEventForClass, which returns void, so we can't capture the return value directly
		// We'll need to find the node after creating it
		FKismetEditorUtilities::CreateNewBoundEventForClass(
			Widget->GetClass(),
			FName(*EventName),
			WidgetBlueprint,
			nullptr  // We don't need a specific property binding
		);

		// Now find the newly created node
		TArray<UK2Node_Event*> UpdatedEventNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(WidgetBlueprint, UpdatedEventNodes);
		
		for (UK2Node_Event* Node : UpdatedEventNodes)
		{
			if (Node->CustomFunctionName == FName(*EventName) && Node->EventReference.GetMemberParentClass() == Widget->GetClass())
			{
				EventNode = Node;
				
				// Set position of the node
				EventNode->NodePosX = NodePos.X;
				EventNode->NodePosY = NodePos.Y;
				
				break;
			}
		}
	}

	if (!EventNode)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to create event node"));
		return Response;
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("event_name"), EventName);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetTextBlockBinding(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing widget_name parameter"));
		return Response;
	}

	FString BindingName;
	if (!Params->TryGetStringField(TEXT("binding_name"), BindingName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing binding_name parameter"));
		return Response;
	}

	// Load the Widget Blueprint
	const FString BlueprintPath = FString::Printf(TEXT("/Game/Widgets/%s.%s"), *BlueprintName, *BlueprintName);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintPath));
		return Response;
	}

	// Create a variable for binding if it doesn't exist
	FBlueprintEditorUtils::AddMemberVariable(
		WidgetBlueprint,
		FName(*BindingName),
		FEdGraphPinType(UEdGraphSchema_K2::PC_Text, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType())
	);

	// Find the TextBlock widget
	UTextBlock* TextBlock = Cast<UTextBlock>(WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)));
	if (!TextBlock)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find TextBlock widget: %s"), *WidgetName));
		return Response;
	}

	// Create binding function
	const FString FunctionName = FString::Printf(TEXT("Get%s"), *BindingName);
	UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
		WidgetBlueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (FuncGraph)
	{
		// Add the function to the blueprint with proper template parameter
		// Template requires null for last parameter when not using a signature-source
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(WidgetBlueprint, FuncGraph, false, nullptr);

		// Create entry node
		UK2Node_FunctionEntry* EntryNode = nullptr;
		
		// Create entry node - use the API that exists in UE 5.5
		EntryNode = NewObject<UK2Node_FunctionEntry>(FuncGraph);
		FuncGraph->AddNode(EntryNode, false, false);
		EntryNode->NodePosX = 0;
		EntryNode->NodePosY = 0;
		EntryNode->FunctionReference.SetExternalMember(FName(*FunctionName), WidgetBlueprint->GeneratedClass);
		EntryNode->AllocateDefaultPins();

		// Create get variable node
		UK2Node_VariableGet* GetVarNode = NewObject<UK2Node_VariableGet>(FuncGraph);
		GetVarNode->VariableReference.SetSelfMember(FName(*BindingName));
		FuncGraph->AddNode(GetVarNode, false, false);
		GetVarNode->NodePosX = 200;
		GetVarNode->NodePosY = 0;
		GetVarNode->AllocateDefaultPins();

		// Connect nodes
		UEdGraphPin* EntryThenPin = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* GetVarOutPin = GetVarNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
		if (EntryThenPin && GetVarOutPin)
		{
			EntryThenPin->MakeLinkTo(GetVarOutPin);
		}
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(BlueprintPath, false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("binding_name"), BindingName);
	return Response;
}


// =============================================================================
// === v2 helpers and command implementations                                ===
// =============================================================================

namespace
{
	UWidgetBlueprint* LoadWBPFromPath(const FString& Path)
	{
		UObject* Loaded = UEditorAssetLibrary::LoadAsset(Path);
		return Cast<UWidgetBlueprint>(Loaded);
	}

	UWidget* FindWidgetByName(UWidgetBlueprint* WBP, const FString& Name)
	{
		if (!WBP || !WBP->WidgetTree)
		{
			return nullptr;
		}
		return WBP->WidgetTree->FindWidget(FName(*Name));
	}

	bool ReadVector2DField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, FVector2D& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Obj->TryGetArrayField(Field, Arr) || !Arr || Arr->Num() < 2)
		{
			return false;
		}
		Out.X = (*Arr)[0]->AsNumber();
		Out.Y = (*Arr)[1]->AsNumber();
		return true;
	}

	bool ReadLinearColorField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, FLinearColor& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Obj->TryGetArrayField(Field, Arr) || !Arr || Arr->Num() < 3)
		{
			return false;
		}
		Out.R = (*Arr)[0]->AsNumber();
		Out.G = (*Arr)[1]->AsNumber();
		Out.B = (*Arr)[2]->AsNumber();
		Out.A = Arr->Num() >= 4 ? (*Arr)[3]->AsNumber() : 1.0f;
		return true;
	}

	UClass* ResolveWidgetClass(const FString& ClassName)
	{
		if (ClassName == TEXT("ProgressBar"))    return UProgressBar::StaticClass();
		if (ClassName == TEXT("TextBlock"))      return UTextBlock::StaticClass();
		if (ClassName == TEXT("HorizontalBox"))  return UHorizontalBox::StaticClass();
		if (ClassName == TEXT("VerticalBox"))    return UVerticalBox::StaticClass();
		if (ClassName == TEXT("CanvasPanel"))    return UCanvasPanel::StaticClass();
		if (ClassName == TEXT("Button"))         return UButton::StaticClass();
		return nullptr;
	}

	void FinalizeWBP(UWidgetBlueprint* WBP)
	{
		WBP->MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(WBP);
		UEditorAssetLibrary::SaveAsset(WBP->GetPathName(), false);
	}
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddWidgetToTree(const TSharedPtr<FJsonObject>& Params)
{
	FString Path, WidgetName, WidgetClassName;
	if (!Params->TryGetStringField(TEXT("path"), Path))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path'"));
	}
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name'"));
	}
	if (!Params->TryGetStringField(TEXT("widget_class"), WidgetClassName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_class'"));
	}

	UWidgetBlueprint* WBP = LoadWBPFromPath(Path);
	if (!WBP)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not load WBP at '%s'"), *Path));
	}

	UClass* WClass = ResolveWidgetClass(WidgetClassName);
	if (!WClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Unknown widget_class '%s'"), *WidgetClassName));
	}

	UPanelWidget* Parent = nullptr;
	FString ParentName;
	if (Params->TryGetStringField(TEXT("parent_name"), ParentName) && !ParentName.IsEmpty())
	{
		UWidget* Found = FindWidgetByName(WBP, ParentName);
		Parent = Cast<UPanelWidget>(Found);
		if (!Parent)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				FString::Printf(TEXT("Parent '%s' not found or not a PanelWidget"), *ParentName));
		}
	}
	else
	{
		Parent = Cast<UPanelWidget>(WBP->WidgetTree->RootWidget);
		if (!Parent)
		{
			if (WClass == UCanvasPanel::StaticClass())
			{
				UCanvasPanel* NewRoot = WBP->WidgetTree->ConstructWidget<UCanvasPanel>(
					UCanvasPanel::StaticClass(), FName(*WidgetName));
				WBP->WidgetTree->RootWidget = NewRoot;
				// Same GUID-map registration as the non-root branch below.
				WBP->WidgetVariableNameToGuidMap.Add(NewRoot->GetFName(), FGuid::NewGuid());
				FinalizeWBP(WBP);
				TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
				Ok->SetBoolField(TEXT("success"), true);
				Ok->SetStringField(TEXT("widget_name"), WidgetName);
				return Ok;
			}
			return FUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Root widget missing; create a CanvasPanel first"));
		}
	}

	UWidget* NewWidget = WBP->WidgetTree->ConstructWidget<UWidget>(WClass, FName(*WidgetName));
	if (!NewWidget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("ConstructWidget returned null"));
	}

	// Register the widget in the BP's GUID map BEFORE compile. The WBP compiler
	// (FWidgetBlueprintCompilerContext::ValidateAndFixUpVariableGuids) ensures
	// every widget in the tree has a corresponding GUID; widgets added through
	// the Designer path get one auto-assigned, but ConstructWidget alone does
	// not. Without this, every add_widget_to_tree call would log a "Widget X
	// was added but did not get a GUID" ensure failure.
	WBP->WidgetVariableNameToGuidMap.Add(NewWidget->GetFName(), FGuid::NewGuid());

	UPanelSlot* AddedSlot = Parent->AddChild(NewWidget);
	if (!AddedSlot)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Could not add to parent '%s'"), *ParentName));
	}

	FinalizeWBP(WBP);

	TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
	Ok->SetBoolField(TEXT("success"), true);
	Ok->SetStringField(TEXT("widget_name"), WidgetName);
	Ok->SetStringField(TEXT("widget_class"), WidgetClassName);
	Ok->SetStringField(TEXT("parent_name"), ParentName);
	return Ok;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetWidgetText(const TSharedPtr<FJsonObject>& Params)
{
	FString Path, WidgetName, Text;
	if (!Params->TryGetStringField(TEXT("path"), Path) ||
	    !Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
	    !Params->TryGetStringField(TEXT("text"), Text))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Need path, widget_name, text"));
	}
	UWidgetBlueprint* WBP = LoadWBPFromPath(Path);
	if (!WBP)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WBP not found"));
	}
	UTextBlock* TB = Cast<UTextBlock>(FindWidgetByName(WBP, WidgetName));
	if (!TB)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found or not a TextBlock"), *WidgetName));
	}
	TB->SetText(FText::FromString(Text));
	FinalizeWBP(WBP);
	TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
	Ok->SetBoolField(TEXT("success"), true);
	return Ok;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetProgressBarPercent(const TSharedPtr<FJsonObject>& Params)
{
	FString Path, WidgetName;
	double Percent = 1.0;
	if (!Params->TryGetStringField(TEXT("path"), Path) ||
	    !Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
	    !Params->TryGetNumberField(TEXT("percent"), Percent))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Need path, widget_name, percent"));
	}
	UWidgetBlueprint* WBP = LoadWBPFromPath(Path);
	if (!WBP)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WBP not found"));
	}
	UProgressBar* PB = Cast<UProgressBar>(FindWidgetByName(WBP, WidgetName));
	if (!PB)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found or not a ProgressBar"), *WidgetName));
	}
	PB->SetPercent(static_cast<float>(Percent));
	FinalizeWBP(WBP);
	TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
	Ok->SetBoolField(TEXT("success"), true);
	return Ok;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetProgressBarFillColor(const TSharedPtr<FJsonObject>& Params)
{
	FString Path, WidgetName;
	if (!Params->TryGetStringField(TEXT("path"), Path) ||
	    !Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Need path, widget_name"));
	}
	FLinearColor Color;
	if (!ReadLinearColorField(Params, TEXT("color"), Color))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Need 'color' as [r,g,b,a]"));
	}
	UWidgetBlueprint* WBP = LoadWBPFromPath(Path);
	if (!WBP)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WBP not found"));
	}
	UProgressBar* PB = Cast<UProgressBar>(FindWidgetByName(WBP, WidgetName));
	if (!PB)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found or not a ProgressBar"), *WidgetName));
	}
	PB->SetFillColorAndOpacity(Color);
	FinalizeWBP(WBP);
	TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
	Ok->SetBoolField(TEXT("success"), true);
	return Ok;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetHorizontalBoxSlotFill(const TSharedPtr<FJsonObject>& Params)
{
	FString Path, WidgetName;
	if (!Params->TryGetStringField(TEXT("path"), Path) ||
	    !Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Need path, widget_name"));
	}
	UWidgetBlueprint* WBP = LoadWBPFromPath(Path);
	if (!WBP)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WBP not found"));
	}
	UWidget* W = FindWidgetByName(WBP, WidgetName);
	if (!W)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));
	}
	UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(W->Slot);
	if (!HSlot)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' is not in a HorizontalBox"), *WidgetName));
	}
	FSlateChildSize FillSize;
	FillSize.Value = 1.0f;
	FillSize.SizeRule = ESlateSizeRule::Fill;
	HSlot->SetSize(FillSize);
	FinalizeWBP(WBP);
	TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
	Ok->SetBoolField(TEXT("success"), true);
	return Ok;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetCanvasSlotAnchor(const TSharedPtr<FJsonObject>& Params)
{
	FString Path, WidgetName;
	if (!Params->TryGetStringField(TEXT("path"), Path) ||
	    !Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Need path, widget_name"));
	}
	UWidgetBlueprint* WBP = LoadWBPFromPath(Path);
	if (!WBP)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WBP not found"));
	}
	UWidget* W = FindWidgetByName(WBP, WidgetName);
	if (!W)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' not found"), *WidgetName));
	}
	UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(W->Slot);
	if (!CSlot)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget '%s' is not in a CanvasPanel"), *WidgetName));
	}

	FVector2D AnchorMin, AnchorMax;
	const bool bHasMin = ReadVector2DField(Params, TEXT("anchor_min"), AnchorMin);
	const bool bHasMax = ReadVector2DField(Params, TEXT("anchor_max"), AnchorMax);
	if (bHasMin || bHasMax)
	{
		FAnchors A;
		A.Minimum = bHasMin ? AnchorMin : CSlot->GetAnchors().Minimum;
		A.Maximum = bHasMax ? AnchorMax : CSlot->GetAnchors().Maximum;
		CSlot->SetAnchors(A);
	}

	FVector2D Position;
	if (ReadVector2DField(Params, TEXT("position"), Position))
	{
		CSlot->SetPosition(Position);
	}
	FVector2D Size;
	if (ReadVector2DField(Params, TEXT("size"), Size))
	{
		CSlot->SetSize(Size);
		CSlot->SetAutoSize(false);
	}
	FVector2D Alignment;
	if (ReadVector2DField(Params, TEXT("alignment"), Alignment))
	{
		CSlot->SetAlignment(Alignment);
	}

	FinalizeWBP(WBP);
	TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
	Ok->SetBoolField(TEXT("success"), true);
	return Ok;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleDeleteWidgetFromTree(const TSharedPtr<FJsonObject>& Params)
{
	FString Path, WidgetName;
	if (!Params->TryGetStringField(TEXT("path"), Path) ||
	    !Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Need path, widget_name"));
	}
	UWidgetBlueprint* WBP = LoadWBPFromPath(Path);
	if (!WBP)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WBP not found"));
	}
	UWidget* W = FindWidgetByName(WBP, WidgetName);
	if (!W)
	{
		// Soft success — re-running stays idempotent.
		TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
		Ok->SetBoolField(TEXT("success"), true);
		Ok->SetStringField(TEXT("note"), TEXT("widget not present; no-op"));
		return Ok;
	}
	const bool bRemoved = WBP->WidgetTree->RemoveWidget(W);
	if (!bRemoved)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("RemoveWidget failed for '%s'"), *WidgetName));
	}
	FinalizeWBP(WBP);
	TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
	Ok->SetBoolField(TEXT("success"), true);
	return Ok;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCompileWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	FString Path;
	if (!Params->TryGetStringField(TEXT("path"), Path))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Need 'path'"));
	}
	UWidgetBlueprint* WBP = LoadWBPFromPath(Path);
	if (!WBP)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("WBP not found"));
	}
	FinalizeWBP(WBP);
	TSharedPtr<FJsonObject> Ok = MakeShared<FJsonObject>();
	Ok->SetBoolField(TEXT("success"), true);
	return Ok;
} 