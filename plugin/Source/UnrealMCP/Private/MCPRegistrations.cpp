// v0.8.0 Day 2c-ii-a — file-scope command registrations.
//
// Replaces the per-subsystem registration block that used to live in
// UUnrealMCPBridge::Initialize(). Why move? Because Initialize() runs
// exactly once at editor startup — but Unreal's Live Coding patches the
// plugin DLL in place, and the patch reload blanks function-local statics
// (including FMCPRegistry's singleton storage). Without re-running
// registrations after a patch, every MCP call returns "Unknown command"
// until the editor is fully restarted. Discovered during Day 2c-i+ — see
// task #61.
//
// Fix: registrations live in a file-scope static FAutoRegistrar whose
// constructor runs at DLL load — initial load AND every Live Coding patch
// reload. The Registry repopulates automatically.
//
// Each handler class is accessed through a function-local-static
// singleton (Singleton<T>()), which lazy-inits the first time a command
// of that category fires. Stateless command classes — no problem with
// re-entrancy across reloads.
//
// Day 2d will inline the HandleXxx methods into free functions per
// command, scattered into Assets/, World/, Editor/, Project/ folders,
// at which point this central registrar file goes away and each handler
// file uses REGISTER_MCP_COMMAND directly. For now: one central place,
// one auto-registration, every command available immediately on load.

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "MCPRegistry.h"
#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPAssetCommands.h"

#include <initializer_list>

namespace
{
	/** Lazy-initialized singleton for each stateless command class. */
	template<typename T> T& Singleton()
	{
		static T Instance;
		return Instance;
	}

	/**
	 * Register a single command name; the handler lambda forwards to the
	 * owning command class's existing HandleCommand dispatch.
	 *
	 * The command name string is captured by value into the lambda so the
	 * Registry's TMap key + the lambda's argument stay in sync after the
	 * temporary FString from TEXT() expires.
	 */
	template<typename T>
	void RegOne(const TCHAR* Name)
	{
		const FString CommandName(Name);
		FMCPRegistry::Get().Register(CommandName, [CommandName](const TSharedPtr<FJsonObject>& Params)
		{
			return Singleton<T>().HandleCommand(CommandName, Params);
		});
	}

	template<typename T>
	void RegBatch(std::initializer_list<const TCHAR*> Names)
	{
		for (const TCHAR* N : Names)
		{
			RegOne<T>(N);
		}
	}

	/**
	 * The auto-registrar. File-scope static — its constructor runs once
	 * per DLL load. Initial editor open: registers everything before the
	 * subsystem even starts. Live Coding patch: re-runs after the patch
	 * DLL is mapped in, repopulating the Registry that the new
	 * FMCPRegistry::Get() singleton points at.
	 */
	struct FAutoRegistrar
	{
		FAutoRegistrar()
		{
			// Bridge-level virtual command.
			FMCPRegistry::Get().Register(TEXT("ping"),
				[](const TSharedPtr<FJsonObject>& /*Params*/)
				{
					TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
					Out->SetStringField(TEXT("message"), TEXT("pong"));
					return Out;
				});

			// ─── Editor: actors, viewport, screenshots, console, PIE, selection ────
			RegBatch<FUnrealMCPEditorCommands>({
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
				TEXT("set_selected_actors"),
				TEXT("clear_selection"),
				TEXT("focus_selected_actors"),
				TEXT("find_actors"),
				TEXT("spawn_actor_batch"),
				TEXT("delete_actor_batch"),
				TEXT("get_object_property"),
				TEXT("set_object_property"),
				TEXT("frame_actor"),
				TEXT("set_show_flag"),
				TEXT("wait_for_async_compile"),
				TEXT("dismiss_modal_dialog"),
				TEXT("get_actor_transform"),
				TEXT("recompile_live"),
			});

			RegBatch<FUnrealMCPBlueprintCommands>({
				TEXT("create_blueprint"),
				TEXT("add_component_to_blueprint"),
				TEXT("set_component_property"),
				TEXT("set_physics_properties"),
				TEXT("compile_blueprint"),
				TEXT("set_blueprint_property"),
				TEXT("set_static_mesh_properties"),
				TEXT("set_pawn_properties"),
			});

			RegBatch<FUnrealMCPBlueprintNodeCommands>({
				TEXT("connect_blueprint_nodes"),
				TEXT("add_blueprint_get_self_component_reference"),
				TEXT("add_blueprint_self_reference"),
				TEXT("find_blueprint_nodes"),
				TEXT("add_blueprint_event_node"),
				TEXT("add_blueprint_input_action_node"),
				TEXT("add_blueprint_function_node"),
				TEXT("add_blueprint_variable"),
			});

			// project.* — create_input_mapping, get_ini, set_ini, execute_python
			// now self-register via REGISTER_MCP_COMMAND at handler definition
			// sites in Commands/UnrealMCPProjectCommands.cpp (v0.8.x §6.2
			// completion). FUnrealMCPProjectCommands class deleted.

			RegBatch<FUnrealMCPAssetCommands>({
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
				TEXT("focus_in_browser"),
				TEXT("navigate_to_folder"),
				TEXT("open_in_editor"),
				TEXT("static_mesh_get_info"),
			});

			// level.* now self-registers in Commands/UnrealMCPLevelCommands.cpp
			// (v0.8.x §6.2 completion). FUnrealMCPLevelCommands class deleted.

			// material.* self-registers in Commands/UnrealMCPMaterialCommands.cpp.

			// outliner.* self-registers in Commands/UnrealMCPOutlinerCommands.cpp.

			RegBatch<FUnrealMCPUMGCommands>({
				TEXT("create_umg_widget_blueprint"),
				TEXT("add_text_block_to_widget"),
				TEXT("add_button_to_widget"),
				TEXT("bind_widget_event"),
				TEXT("set_text_block_binding"),
				TEXT("add_widget_to_viewport"),
				TEXT("add_widget_to_tree"),
				TEXT("set_widget_text"),
				TEXT("set_progress_bar_percent"),
				TEXT("set_progress_bar_fill_color"),
				TEXT("set_horizontal_box_slot_fill"),
				TEXT("set_canvas_slot_anchor"),
				TEXT("delete_widget_from_tree"),
				TEXT("compile_widget_blueprint"),
			});

			UE_LOG(LogTemp, Display,
				TEXT("[UnrealMCP] %d commands auto-registered at DLL load (Day 2c-ii-a)"),
				FMCPRegistry::Get().Num());
		}
	};

	static FAutoRegistrar GAutoRegistrar;
}
