"""Pure-Python tests for smoke_dispatch command classification.

These tests intentionally do not use the `call` fixture from conftest.py, so
they never connect to a live Unreal editor.
"""

from smoke_dispatch import (
    NEVER_EMPTY_SMOKE,
    PYTHON_ONLY,
    SAFE_DEFAULT_WIRE_COMMANDS,
    classify_wire_commands,
)


def test_default_selection_excludes_project_mutations():
    selection = classify_wire_commands(
        [
            "get_current_level",
            "find_actors",
            "create_landscape",
            "save_current_level",
            "delete_actor",
            "set_object_property",
            "take_screenshot",
        ]
    )

    assert selection.commands == ["find_actors", "get_current_level", "ping"]
    assert "create_landscape" in selection.skipped_unsafe
    assert "save_current_level" in selection.skipped_unsafe
    assert "delete_actor" in selection.skipped_unsafe
    assert "set_object_property" in selection.skipped_unsafe
    assert "take_screenshot" in selection.skipped_unsafe


def test_mutating_opt_in_includes_project_mutations_but_not_lifecycle_commands():
    selection = classify_wire_commands(
        [
            "get_current_level",
            "create_landscape",
            "save_current_level",
            "delete_actor",
            "start_pie",
            "recompile_live",
        ],
        allow_mutating=True,
    )

    assert "create_landscape" in selection.commands
    assert "save_current_level" in selection.commands
    assert "delete_actor" in selection.commands
    assert "start_pie" not in selection.commands
    assert "recompile_live" not in selection.commands
    assert "start_pie" in selection.skipped_never_empty
    assert "recompile_live" in selection.skipped_never_empty
    assert not selection.skipped_unsafe


def test_python_only_composites_are_never_sent_as_wire_commands():
    selection = classify_wire_commands(
        [
            "get_component_property",
            "get_static_mesh_material",
            "get_world_settings",
            "set_world_settings",
        ],
        allow_mutating=True,
    )

    assert selection.commands == ["ping"]
    assert selection.skipped_python_only == [
        "get_component_property",
        "get_static_mesh_material",
        "get_world_settings",
        "set_world_settings",
    ]


def test_new_unclassified_tool_is_skipped_by_safe_default():
    selection = classify_wire_commands(["totally_new_command"])

    assert selection.commands == ["ping"]
    assert selection.skipped_unsafe == ["totally_new_command"]


def test_default_allowlist_does_not_overlap_never_empty_smoke():
    assert SAFE_DEFAULT_WIRE_COMMANDS.isdisjoint(NEVER_EMPTY_SMOKE)


def test_default_allowlist_does_not_overlap_python_only_composites():
    assert SAFE_DEFAULT_WIRE_COMMANDS.isdisjoint(PYTHON_ONLY)


def test_default_allowlist_entries_are_known_wire_commands():
    import unreal_mcp_server

    tool_names = {
        tool.name
        for tool in unreal_mcp_server.mcp._tool_manager.list_tools()  # noqa: WPS437
    }
    known_wire_commands = (tool_names - PYTHON_ONLY) | {"ping"}

    assert SAFE_DEFAULT_WIRE_COMMANDS <= known_wire_commands
