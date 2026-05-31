#include "UnrealMCPBridge.h"

#include "MCPServerRunnable.h"
#include "MCPRegistry.h"
#include "MCPResponse.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"

// Command handler classes — kept until Day 2d migrates handlers into the
// new Assets/, World/, Editor/, Project/ folder layout.
#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPAssetCommands.h"
#include "Commands/UnrealMCPLevelCommands.h"
#include "Commands/UnrealMCPMaterialCommands.h"
#include "Commands/UnrealMCPOutlinerCommands.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

// ─── Construction ─────────────────────────────────────────────────────────

UUnrealMCPBridge::UUnrealMCPBridge()
{
	EditorCommands       = MakeShared<FUnrealMCPEditorCommands>();
	BlueprintCommands    = MakeShared<FUnrealMCPBlueprintCommands>();
	BlueprintNodeCommands = MakeShared<FUnrealMCPBlueprintNodeCommands>();
	ProjectCommands      = MakeShared<FUnrealMCPProjectCommands>();
	UMGCommands          = MakeShared<FUnrealMCPUMGCommands>();
	AssetCommands        = MakeShared<FUnrealMCPAssetCommands>();
	LevelCommands        = MakeShared<FUnrealMCPLevelCommands>();
	MaterialCommands     = MakeShared<FUnrealMCPMaterialCommands>();
	OutlinerCommands     = MakeShared<FUnrealMCPOutlinerCommands>();
}

UUnrealMCPBridge::~UUnrealMCPBridge()
{
	EditorCommands.Reset();
	BlueprintCommands.Reset();
	BlueprintNodeCommands.Reset();
	ProjectCommands.Reset();
	UMGCommands.Reset();
	AssetCommands.Reset();
	LevelCommands.Reset();
	MaterialCommands.Reset();
	OutlinerCommands.Reset();
}

// ─── Subsystem lifecycle ──────────────────────────────────────────────────

namespace
{
	/**
	 * Register a batch of command names that all dispatch through the same
	 * stateless command-class instance. Each handler lambda captures a
	 * weak-style copy of the shared pointer and the command name, and
	 * forwards to the old per-class HandleCommand dispatch.
	 *
	 * Day 2d will inline each HandleXxx into its own free function under
	 * the new folder layout and replace this loop with direct
	 * REGISTER_MCP_COMMAND calls at file scope.
	 */
	template<typename TCommandClass>
	void RegisterBatch(const TSharedPtr<TCommandClass>& Owner,
	                   std::initializer_list<const TCHAR*> Names)
	{
		FMCPRegistry& R = FMCPRegistry::Get();
		for (const TCHAR* RawName : Names)
		{
			const FString CommandName(RawName);
			TSharedPtr<TCommandClass> Captured = Owner;
			R.Register(CommandName, [Captured, CommandName](const TSharedPtr<FJsonObject>& Params)
			{
				return Captured->HandleCommand(CommandName, Params);
			});
		}
	}
}

void UUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Initializing"));

	bIsRunning = false;
	ListenerSocket = nullptr;
	ConnectionSocket = nullptr;
	ServerThread = nullptr;
	Port = MCP_SERVER_PORT;
	FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

	// ─── Command registration (Day 2b) ───────────────────────────────
	//
	// All 78 existing commands re-routed through FMCPRegistry. No behavioral
	// change vs the prior if/else chain — same handlers, same response
	// shapes. Day 2c will flip the wire format; Day 2d will migrate the
	// handlers themselves into the new folder layout.

	FMCPRegistry& Registry = FMCPRegistry::Get();

	// Bridge-level virtual command.
	Registry.Register(TEXT("ping"), [](const TSharedPtr<FJsonObject>& /*Params*/)
	{
		TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("message"), TEXT("pong"));
		return Out;
	});

	// Editor — actors, viewport, screenshots, console, PIE, selection.
	RegisterBatch(EditorCommands, {
		TEXT("get_actors_in_level"),
		TEXT("find_actors_by_name"),
		TEXT("spawn_actor"),
		TEXT("create_actor"),
		TEXT("spawn_static_mesh_actor"),
		TEXT("set_static_mesh_actor_mesh"),
		TEXT("set_static_mesh_material"),
		TEXT("delete_actor"),
		TEXT("set_actor_transform"),
		TEXT("get_actor_properties"),
		TEXT("get_actor_property"),
		TEXT("set_actor_property"),
		TEXT("spawn_blueprint_actor"),
		TEXT("focus_viewport"),
		TEXT("take_screenshot"),
		TEXT("get_viewport_camera"),
		TEXT("set_viewport_camera"),
		TEXT("execute_console_command"),
		TEXT("set_cvar"),
		TEXT("get_cvar"),
		TEXT("get_viewport_mode"),
		TEXT("set_viewport_mode"),
		TEXT("read_output_log"),
		TEXT("get_async_compile_status"),
		TEXT("start_pie"),
		TEXT("stop_pie"),
		TEXT("is_pie_active"),
		TEXT("pie_get_player"),
		TEXT("pie_set_player"),
		TEXT("pie_apply_movement"),
		TEXT("pie_screenshot"),
		TEXT("get_selected_actors"),
	});

	RegisterBatch(BlueprintCommands, {
		TEXT("create_blueprint"),
		TEXT("add_component_to_blueprint"),
		TEXT("set_component_property"),
		TEXT("set_physics_properties"),
		TEXT("compile_blueprint"),
		TEXT("set_blueprint_property"),
		TEXT("set_static_mesh_properties"),
		TEXT("set_pawn_properties"),
	});

	RegisterBatch(BlueprintNodeCommands, {
		TEXT("connect_blueprint_nodes"),
		TEXT("add_blueprint_get_self_component_reference"),
		TEXT("add_blueprint_self_reference"),
		TEXT("find_blueprint_nodes"),
		TEXT("add_blueprint_event_node"),
		TEXT("add_blueprint_input_action_node"),
		TEXT("add_blueprint_function_node"),
		TEXT("add_blueprint_variable"),
	});

	RegisterBatch(ProjectCommands, {
		TEXT("create_input_mapping"),
	});

	RegisterBatch(AssetCommands, {
		TEXT("list_assets"),
		TEXT("get_asset_info"),
		TEXT("find_assets_by_class"),
		TEXT("get_asset_dependencies"),
		TEXT("get_asset_references"),
		TEXT("move_asset"),
		TEXT("delete_asset"),
		TEXT("rename_asset"),
		TEXT("duplicate_asset"),
		TEXT("migrate_assets"),
		TEXT("import_asset"),
		TEXT("finalize_migration"),
	});

	RegisterBatch(LevelCommands, {
		TEXT("get_current_level"),
		TEXT("open_level"),
		TEXT("save_current_level"),
		TEXT("save_all_dirty"),
	});

	RegisterBatch(MaterialCommands, {
		TEXT("get_material_parameters"),
		TEXT("set_material_instance_param"),
		TEXT("create_material_instance"),
		TEXT("get_material_uses"),
		TEXT("list_material_instances_of_parent"),
	});

	RegisterBatch(OutlinerCommands, {
		TEXT("get_outliner_folders"),
		TEXT("move_actor_to_folder"),
		TEXT("create_outliner_folder"),
		TEXT("get_actors_in_folder"),
	});

	RegisterBatch(UMGCommands, {
		TEXT("create_umg_widget_blueprint"),
		TEXT("add_text_block_to_widget"),
		TEXT("add_button_to_widget"),
		TEXT("bind_widget_event"),
		TEXT("set_text_block_binding"),
		TEXT("add_widget_to_viewport"),
		// v2 widget tree commands
		TEXT("add_widget_to_tree"),
		TEXT("set_widget_text"),
		TEXT("set_progress_bar_percent"),
		TEXT("set_progress_bar_fill_color"),
		TEXT("set_horizontal_box_slot_fill"),
		TEXT("set_canvas_slot_anchor"),
		TEXT("delete_widget_from_tree"),
		TEXT("compile_widget_blueprint"),
	});

	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: %d commands registered"), Registry.Num());

	StartServer();
}

void UUnrealMCPBridge::Deinitialize()
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Shutting down"));
	StopServer();
}

// ─── Server lifecycle (unchanged) ─────────────────────────────────────────

void UUnrealMCPBridge::StartServer()
{
	if (bIsRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealMCPBridge: Server is already running"));
		return;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to get socket subsystem"));
		return;
	}

	TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
	if (!NewListenerSocket.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create listener socket"));
		return;
	}

	NewListenerSocket->SetReuseAddr(true);
	NewListenerSocket->SetNonBlocking(true);

	FIPv4Endpoint Endpoint(ServerAddress, Port);
	if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to bind listener socket to %s:%d"),
		       *ServerAddress.ToString(), Port);
		return;
	}

	if (!NewListenerSocket->Listen(5))
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to start listening"));
		return;
	}

	ListenerSocket = NewListenerSocket;
	bIsRunning = true;
	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server started on %s:%d"),
	       *ServerAddress.ToString(), Port);

	ServerThread = FRunnableThread::Create(
		new FMCPServerRunnable(this, ListenerSocket),
		TEXT("UnrealMCPServerThread"),
		0, TPri_Normal
	);

	if (!ServerThread)
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create server thread"));
		StopServer();
		return;
	}
}

void UUnrealMCPBridge::StopServer()
{
	if (!bIsRunning) return;

	bIsRunning = false;

	if (ServerThread)
	{
		ServerThread->Kill(true);
		delete ServerThread;
		ServerThread = nullptr;
	}

	if (ConnectionSocket.IsValid())
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
		ConnectionSocket.Reset();
	}

	if (ListenerSocket.IsValid())
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
		ListenerSocket.Reset();
	}

	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server stopped"));
}

// ─── Command execution ────────────────────────────────────────────────────
//
// Day 2b — thin dispatch through FMCPRegistry. The handler decides the inner
// response shape; the bridge wraps it in the legacy {status, result|error}
// envelope. Day 2c flips that envelope to strict {success, error?} and
// updates the Python wrappers in the same commit.

FString UUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Executing command: %s"), *CommandType);

	TPromise<FString> Promise;
	TFuture<FString> Future = Promise.GetFuture();

	AsyncTask(ENamedThreads::GameThread, [CommandType, Params, Promise = MoveTemp(Promise)]() mutable
	{
		TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();

		try
		{
			TSharedPtr<FJsonObject> ResultJson = FMCPRegistry::Get().Dispatch(CommandType, Params);

			// Inspect the handler's response. Handlers either return
			// {success:true|false, error?, ...} (the new shape, used by
			// FMCPResponse) or a bare object that's implicitly success.
			bool bSuccess = true;
			FString ErrorMessage;

			if (ResultJson.IsValid() && ResultJson->HasField(TEXT("success")))
			{
				bSuccess = ResultJson->GetBoolField(TEXT("success"));
				if (!bSuccess && ResultJson->HasField(TEXT("error")))
				{
					ErrorMessage = ResultJson->GetStringField(TEXT("error"));
				}
			}

			if (bSuccess)
			{
				ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
				ResponseJson->SetObjectField(TEXT("result"), ResultJson);
			}
			else
			{
				ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
				ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
			}
		}
		catch (const std::exception& e)
		{
			ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
			ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
		}

		FString ResultString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
		FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
		Promise.SetValue(ResultString);
	});

	return Future.Get();
}
