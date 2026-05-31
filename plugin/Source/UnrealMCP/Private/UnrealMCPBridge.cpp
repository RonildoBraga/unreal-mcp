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

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

// ─── Construction ─────────────────────────────────────────────────────────
//
// Day 2c-ii-a — the bridge no longer owns any command-class instances.
// Registrations live in MCPRegistrations.cpp's file-scope FAutoRegistrar,
// which runs at DLL load (initial + Live Coding patch). Command-class
// instances are lazy-singletons inside that file.
//
// The bridge subsystem owns ONLY the server lifecycle: sockets, listener
// thread, command dispatch.

UUnrealMCPBridge::UUnrealMCPBridge() = default;
UUnrealMCPBridge::~UUnrealMCPBridge() = default;

// ─── Subsystem lifecycle ──────────────────────────────────────────────────

void UUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Initializing"));

	bIsRunning = false;
	ListenerSocket = nullptr;
	ConnectionSocket = nullptr;
	ServerThread = nullptr;
	Port = MCP_SERVER_PORT;
	FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

	// Commands self-register at DLL load via MCPRegistrations.cpp. The
	// number we report here is informational — confirms the file-scope
	// FAutoRegistrar already ran by the time the subsystem started.
	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: %d commands available"),
	       FMCPRegistry::Get().Num());

	StartServer();
}

void UUnrealMCPBridge::Deinitialize()
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Shutting down"));
	StopServer();
}

// ─── Server lifecycle ─────────────────────────────────────────────────────

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
// Day 2c-i — strict {success, error?, ...payload} on the wire (per
// architecture-v0.8-plan §3.7 and §8 question 1). The legacy
// {status, result|error} envelope is gone; handler's inner shape IS the
// wire shape, with two small normalizations:
//
//   1. If the handler omitted `success`, treat as implicit success.
//   2. If the handler used the legacy CreateSuccessResponse pattern
//      (which builds `{success: true, data: {...}}`), flatten the
//      `data` fields into the top level so the wire shape stays
//      consistent regardless of which helper the handler used.
//
// This lets all 294 existing CreateSuccessResponse / CreateErrorResponse
// call sites keep their bodies unchanged; Day 2c-ii migrates the Python
// wrappers; Day 2c-iii drops `data`-flattening once handlers have stopped
// using the legacy nested form.

FString UUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Executing command: %s"), *CommandType);

	TPromise<FString> Promise;
	TFuture<FString> Future = Promise.GetFuture();

	AsyncTask(ENamedThreads::GameThread, [CommandType, Params, Promise = MoveTemp(Promise)]() mutable
	{
		TSharedPtr<FJsonObject> ResponseJson;

		try
		{
			ResponseJson = FMCPRegistry::Get().Dispatch(CommandType, Params);
			if (!ResponseJson.IsValid())
			{
				ResponseJson = FMCPResponse::Error(TEXT("Empty response from handler"));
			}

			// Normalize 1: ensure `success` field exists.
			if (!ResponseJson->HasField(TEXT("success")))
			{
				ResponseJson->SetBoolField(TEXT("success"), true);
			}

			// Normalize 2: flatten legacy `{success: true, data: {...}}`
			// shape so payload keys live at the top level.
			const TSharedPtr<FJsonObject>* DataPtr = nullptr;
			if (ResponseJson->GetBoolField(TEXT("success"))
			    && ResponseJson->TryGetObjectField(TEXT("data"), DataPtr)
			    && DataPtr && DataPtr->IsValid())
			{
				for (const auto& Pair : (*DataPtr)->Values)
				{
					if (Pair.Key != TEXT("success"))
					{
						ResponseJson->SetField(Pair.Key, Pair.Value);
					}
				}
				ResponseJson->RemoveField(TEXT("data"));
			}
		}
		catch (const std::exception& e)
		{
			ResponseJson = FMCPResponse::Error(UTF8_TO_TCHAR(e.what()));
		}

		FString ResultString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
		FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
		Promise.SetValue(ResultString);
	});

	return Future.Get();
}
