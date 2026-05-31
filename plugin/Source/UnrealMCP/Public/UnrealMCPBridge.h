#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "UnrealMCPBridge.generated.h"

class FMCPServerRunnable;

/**
 * Editor subsystem for the MCP bridge.
 *
 * Owns the TCP listener on port 55557, the server thread, and the
 * dispatch entry point. Commands are JSON-decoded by the runnable,
 * passed to ExecuteCommand, dispatched through FMCPRegistry, and
 * normalized into the strict {success, error?, ...payload} wire shape.
 *
 * v0.8.0 Day 2c-ii-a: registrations are done by MCPRegistrations.cpp's
 * file-scope FAutoRegistrar (runs on DLL load — initial AND Live Coding
 * patch reloads). The bridge no longer owns any command-class instances.
 */
UCLASS()
class UNREALMCP_API UUnrealMCPBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UUnrealMCPBridge();
	virtual ~UUnrealMCPBridge();

	// UEditorSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Server
	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

	// Command execution — JSON in, serialized JSON string out.
	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Server state
	bool bIsRunning;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ConnectionSocket;
	FRunnableThread* ServerThread;

	// Server configuration
	FIPv4Address ServerAddress;
	uint16 Port;
};
