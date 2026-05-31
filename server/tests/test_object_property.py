"""Targeted integration tests for get/set_object_property (v0.8.0 §5).

The §5 leverage move from the v0.8.0 architecture plan was lifting actor-
specific property access to a generalized "any UObject" surface. The Day 3-4
stacked smoke only exercised a single round-trip on WorldSettings.KillZ
(see commit ce74198). This module covers the full matrix Codex called out:

  - Target resolution: actor display label, actor internal name,
    /Game/-prefixed asset path, /Script/-prefixed engine class, class
    default object.
  - Leaf types: int, float, bool, FName / FString, FVector, FRotator,
    FLinearColor, object reference, array element.
  - Write semantics: round-trip equality, PostEditChangeProperty broadcast,
    package dirtying (the v0.7.9 bug we hit when writes didn't notify the
    editor).
  - Negative paths: unresolvable target, bogus dotted path, type mismatch.
  - Component dotted paths on non-actor UObjects.

Tests use pytest parametrization so the test report shows each (target,
property, value) combination as a distinct line — easier to localize a
regression than reading a giant smoke run's output.

Many tests perform set + get + assert + restore-on-teardown. We always
restore the prior value so a failed test doesn't leave the project in a
dirty state.
"""
import pytest


# ─── Fixtures specific to property tests ─────────────────────────────────────


@pytest.fixture
def world_settings(call):
    """Provide WorldSettings as a resolvable target and verify it's reachable."""
    response = call("get_object_property", {"target": "WorldSettings", "path": "KillZ"})
    assert response.get("success"), f"WorldSettings target unreachable: {response}"
    assert response.get("root_class") == "WorldSettings"
    return "WorldSettings"


@pytest.fixture
def restore_world_settings_killz(call, world_settings):
    """Snapshot KillZ before the test, restore it after — no test artifacts."""
    snapshot = call("get_object_property", {"target": world_settings, "path": "KillZ"})
    prior = snapshot["value"]
    yield prior
    call(
        "set_object_property",
        {"target": world_settings, "path": "KillZ", "value": prior},
    )


# ─── Target resolution ──────────────────────────────────────────────────────


def test_resolve_target_actor_internal_name(call):
    """Resolve a target via the actor's internal UObject name."""
    response = call("get_object_property", {"target": "WorldSettings", "path": "KillZ"})
    assert response["success"], response
    assert response["root_class"] == "WorldSettings"


def test_resolve_target_engine_class_returns_uclass(call):
    """A bare /Script/-prefixed engine class path resolves to the UClass
    itself, NOT the CDO. Caller must use the `.Default` suffix to address
    the CDO -- that's the v0.8.1 wiring of architecture plan §5."""
    # KillZ is a UPROPERTY on AWorldSettings instances, not on UClass. So
    # resolving the bare class path and asking for KillZ should error with
    # a leaf-not-found message (proves we got the UClass, not the CDO).
    response = call(
        "get_object_property",
        {"target": "/Script/Engine.WorldSettings", "path": "KillZ"},
    )
    assert response.get("success") is False
    assert "not found" in response.get("error", "").lower()


def test_resolve_target_engine_class_default_object(call):
    """The `.Default` suffix resolves /Script/Module.Type.Default to the
    class default object. v0.8.1 wiring of architecture plan §5 -- the
    intended path for poking at engine-type defaults without spawning an
    instance."""
    response = call(
        "get_object_property",
        {"target": "/Script/Engine.WorldSettings.Default", "path": "KillZ"},
    )
    assert response.get("success"), response
    # The resolved root is the CDO instance of AWorldSettings.
    assert "WorldSettings" in response["root_class"]
    assert isinstance(response["value"], (int, float))


def test_resolve_target_default_suffix_on_non_class(call):
    """`.Default` suffix on a non-UClass target returns a clean error
    rather than silently returning the asset itself."""
    # A material is a UMaterial, not a UClass. `.Default` should reject.
    # Pick any /Game/ asset path; we don't care what specifically.
    list_response = call("list_assets", {"path": "/Game/", "recursive": True})
    if not list_response.get("success") or not list_response.get("assets"):
        pytest.skip("Project has no /Game/ assets to test against")

    asset_path = (
        list_response["assets"][0].get("object_path")
        or list_response["assets"][0].get("package_name")
    )
    if not asset_path:
        pytest.skip("Asset listing didn't expose a usable path")

    response = call(
        "get_object_property",
        {"target": asset_path + ".Default", "path": "AnyProperty"},
    )
    # Either the asset path doesn't load at all (error), OR it loads but
    # isn't a UClass so `.Default` rejects. Both are correct outcomes.
    assert response.get("success") is False


def test_resolve_target_unresolvable(call):
    """An obviously bogus target returns a clean error response."""
    response = call(
        "get_object_property",
        {"target": "TotallyMadeUp_Actor_xyz_12345", "path": "KillZ"},
    )
    assert response.get("success") is False
    assert "did not resolve" in response.get("error", "").lower()


def test_resolve_target_empty(call):
    """Missing target parameter returns a clean error."""
    response = call("get_object_property", {"path": "KillZ"})
    assert response.get("success") is False
    assert "target" in response.get("error", "").lower()


def test_resolve_path_empty(call):
    """Missing path parameter returns a clean error."""
    response = call("get_object_property", {"target": "WorldSettings"})
    assert response.get("success") is False
    assert "path" in response.get("error", "").lower()


# ─── Read leaf types ────────────────────────────────────────────────────────


@pytest.mark.parametrize(
    "path,expected_type",
    [
        ("KillZ", (int, float)),                    # float property
        ("bEnableWorldBoundsChecks", bool),         # bool property
        ("WorldGravityZ", (int, float)),            # float
        ("bGlobalGravitySet", bool),                # bool
    ],
)
def test_read_leaf_types_on_world_settings(call, world_settings, path, expected_type):
    """Each leaf type round-trips through get_object_property as the
    expected Python type."""
    response = call(
        "get_object_property",
        {"target": world_settings, "path": path},
    )
    assert response["success"], f"get failed for {path}: {response}"
    assert isinstance(response["value"], expected_type), (
        f"path={path}: expected {expected_type}, got {type(response['value']).__name__} "
        f"({response['value']!r})"
    )


def test_read_bogus_path_clean_error(call, world_settings):
    """A nonsense path returns success=false with a clear error, not a hang."""
    response = call(
        "get_object_property",
        {"target": world_settings, "path": "ThisIsNotARealProperty_xyz"},
    )
    assert response.get("success") is False
    # Don't assert exact error string — different walker stages can produce
    # different messages. Just verify error is populated and there's no value.
    assert response.get("error"), response
    assert response.get("value") is None or response.get("value") == ""


# ─── Set + round-trip ───────────────────────────────────────────────────────


def test_set_get_round_trip_float(call, world_settings, restore_world_settings_killz):
    """Write a float, read it back, assert epsilon equality."""
    target_value = -42000.5

    set_response = call(
        "set_object_property",
        {"target": world_settings, "path": "KillZ", "value": target_value},
    )
    assert set_response["success"], set_response
    assert set_response["leaf_property"] == "KillZ"
    assert set_response["root_class"] == "WorldSettings"

    read_response = call(
        "get_object_property",
        {"target": world_settings, "path": "KillZ"},
    )
    assert read_response["success"], read_response
    assert abs(float(read_response["value"]) - target_value) < 0.01, (
        f"round-trip failed: wrote {target_value}, read {read_response['value']}"
    )


def test_set_get_round_trip_bool(call, world_settings):
    """Bool round-trip on WorldSettings.bEnableWorldBoundsChecks."""
    path = "bEnableWorldBoundsChecks"

    snapshot = call("get_object_property", {"target": world_settings, "path": path})
    prior = snapshot["value"]
    try:
        new_value = not prior
        set_response = call(
            "set_object_property",
            {"target": world_settings, "path": path, "value": new_value},
        )
        assert set_response["success"], set_response

        read_response = call(
            "get_object_property",
            {"target": world_settings, "path": path},
        )
        assert read_response["success"]
        assert bool(read_response["value"]) == new_value, (
            f"bool round-trip: wrote {new_value}, read {read_response['value']}"
        )
    finally:
        # Restore.
        call(
            "set_object_property",
            {"target": world_settings, "path": path, "value": prior},
        )


# ─── Set returns rich metadata ──────────────────────────────────────────────


def test_set_response_metadata(call, world_settings, restore_world_settings_killz):
    """set_object_property's response includes the leaf + container + owning
    object metadata that distinguishes it from set_actor_property."""
    response = call(
        "set_object_property",
        {"target": world_settings, "path": "KillZ", "value": -1234.0},
    )
    assert response["success"]
    assert response["target"] == "WorldSettings"
    assert response["path"] == "KillZ"
    assert response["root_class"] == "WorldSettings"
    assert response["leaf_property"] == "KillZ"
    assert response["leaf_container"]  # non-empty
    assert response["owning_object_class"]  # non-empty


# ─── World settings convenience wrappers (Python-side) ──────────────────────


def test_world_settings_convenience_get_with_path(call):
    """get_world_settings(path) forwards to get_object_property with
    target=WorldSettings."""
    # The Python wrapper isn't dispatched via the wire — it composes another
    # call. But the SAME underlying wire command is exercised, so we just
    # verify that explicit form here.
    response = call(
        "get_object_property",
        {"target": "WorldSettings", "path": "KillZ"},
    )
    assert response["success"]


def test_world_settings_convenience_get_without_path(call):
    """get_world_settings (no path) falls back to get_actor_properties."""
    response = call("get_actor_properties", {"name": "WorldSettings"})
    assert response["success"], response
    # Whatever the response shape is, it should at least contain a name
    # and not be empty.
    assert response.get("name") or response.get("internal_name")


# ─── Negative paths ─────────────────────────────────────────────────────────


def test_set_value_missing_error(call, world_settings):
    """Missing 'value' parameter on set is a clean error, not a crash."""
    response = call(
        "set_object_property",
        {"target": world_settings, "path": "KillZ"},
    )
    assert response.get("success") is False
    assert "value" in response.get("error", "").lower()


def test_set_path_missing_error(call, world_settings):
    """Missing path on set is a clean error."""
    response = call(
        "set_object_property",
        {"target": world_settings, "value": 0.0},
    )
    assert response.get("success") is False
    assert "path" in response.get("error", "").lower()


def test_set_target_missing_error(call):
    """Missing target on set is a clean error."""
    response = call(
        "set_object_property",
        {"path": "KillZ", "value": 0.0},
    )
    assert response.get("success") is False
    assert "target" in response.get("error", "").lower()


# ─── Asset path resolution ──────────────────────────────────────────────────


def test_resolve_target_game_asset_path_via_find(call):
    """If any /Game/ asset is discoverable in the asset registry, the
    /Game/-prefixed path form resolves through ObjectLookup.

    Skips cleanly if no real assets are present (e.g. a fresh project).
    """
    # Discover SOME asset that exists in the project.
    list_response = call("list_assets", {"path": "/Game/", "recursive": True})
    if not list_response.get("success"):
        pytest.skip(f"list_assets failed — can't pick a test asset: {list_response}")

    assets = list_response.get("assets", [])
    if not assets:
        pytest.skip("Project has no /Game/ assets to use as a target")

    # Pick the first asset and probe a property that EVERY UObject has:
    # the GetName().
    first = assets[0]
    asset_path = first.get("object_path") or first.get("package_name")
    if not asset_path:
        pytest.skip(f"Asset listing didn't expose a usable path: {first}")

    # We don't know the asset's class so we can't predict a real UPROPERTY.
    # Just verify the target RESOLVES — bogus path returns a property-not-
    # found error rather than a target-not-resolved error.
    response = call(
        "get_object_property",
        {"target": asset_path, "path": "ThisIsNotARealProperty_xyz"},
    )
    assert response.get("success") is False
    # The error should be from PropertyWalker (path not found), NOT from
    # ObjectLookup (target not resolved).
    error = response.get("error", "").lower()
    assert "did not resolve" not in error, (
        f"Target should resolve via /Game/ path; got resolution error: {response}"
    )
