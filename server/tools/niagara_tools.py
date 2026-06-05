"""Niagara VFX Tools for Unreal MCP.

Let the LLM own particle VFX (fire, sparks, smoke, dust, magic) end-to-end —
spawn a system, discover its tunable knobs, tune them, and preview the result
in a screenshot — with the editor reserved only for authoring/exposing a base
system's parameters.

Tool surface (4 tools):

    spawn_niagara             place a Niagara System as a persistent actor
    list_niagara_user_params  enumerate a system's tunable User Parameters
                              (C++-only — NiagaraSystem exposes no param list
                              to Python; this is the discovery keystone)
    set_niagara_param         override one User Parameter by name
    seek_niagara              advance + RENDER the sim at a chosen time so an
                              editor screenshot shows it (a plain Activate does
                              not render in a non-realtime viewport)

Typical loop:
    1. list_niagara_user_params(system) -> learn the knob names/types
    2. spawn_niagara(system, location) -> place it
    3. set_niagara_param(actor, name, type, value) -> tune (color/rate/size...)
    4. seek_niagara(actor, age=2.0) -> preview, then take_screenshot
    5. repeat 3-4; before saving the level call seek_niagara(actor, live=True)
       so the system ticks normally in-game.

Built-in template systems available with no import include:
    /Niagara/DefaultAssets/Templates/Systems/{FountainLightweight,
    SimpleExplosion, RadialBurst, DirectionalBurst, MinimalLightweight}

Wire format: each tool sends `{type: "<command_name>", params: {...}}` to the
C++ plugin. C++ side: Private/Commands/UnrealMCPNiagaraCommands.cpp.
"""

import logging
from typing import Any, Dict, Optional

from mcp.server.fastmcp import Context, FastMCP

from _registry import unreal_tool

logger = logging.getLogger("UnrealMCP")


def register_niagara_tools(mcp: FastMCP):
    """Register Niagara VFX tools with the MCP server."""

    @unreal_tool(mcp)
    def list_niagara_user_params(ctx: Context, system_path: str) -> Dict[str, Any]:
        """Enumerate a Niagara System's exposed User Parameters — the knobs the
        LLM can tune via set_niagara_param.

        This is C++-only on purpose: UE 5.7's Python bindings expose no way to
        list a system's parameters, so blindly guessing names with
        set_niagara_param silently no-ops. Call this FIRST to learn the real
        names and types.

        Args:
            system_path: /Game/- or /Niagara/-prefixed system path. Object-path
                         form is also accepted.

        Returns:
            {
              "success": true,
              "system": "FountainLightweight",
              "user_parameters": [{"name": "Color", "type": "LinearColor"}, ...],
              "count": N
            }
            count == 0 means the system exposes nothing tunable — pick a system
            that does, or author/expose params for it in the editor.
        """

    @unreal_tool(mcp)
    def spawn_niagara(
        ctx: Context,
        system_path: str,
        location: Optional[Dict[str, float]] = None,
        rotation: Optional[Dict[str, float]] = None,
        label: str = "",
    ) -> Dict[str, Any]:
        """Spawn a Niagara System as a persistent, savable NiagaraActor.

        Args:
            system_path: /Game/- or /Niagara/-prefixed NiagaraSystem path.
            location:    world spawn {x, y, z}. Default origin.
            rotation:    world {pitch, yaw, roll} or {x, y, z}. Default none.
            label:       outliner label for the actor (recommended so seek/
                         set_niagara_param can address it). Default auto.

        The component is activated on spawn and left in normal (live) tick mode.
        Use seek_niagara to preview it in a still screenshot.

        Returns:
            {"success": true, "name": "<label>", "internal_name": "...",
             "system": "..."}
        """

    @unreal_tool(mcp)
    def set_niagara_param(
        ctx: Context,
        actor: str,
        name: str,
        type: str,
        value: Any,
    ) -> Dict[str, Any]:
        """Override one User Parameter on a spawned Niagara actor.

        The param `name` must match an actual exposed User Parameter — get the
        valid names from list_niagara_user_params first. Setting an unknown name
        silently does nothing (Niagara behavior).

        Args:
            actor: outliner label or internal name of the Niagara actor.
            name:  User Parameter name (e.g. "Color", "SpawnRate").
            type:  one of float | int | bool | vec2 | vec3 | position | color.
            value: the value, shaped for the type:
                     float/int -> number; bool -> true/false;
                     vec2 -> [x, y]; vec3/position -> [x, y, z];
                     color -> [r, g, b, a]  (a optional, defaults 1).

        Returns: {"success": true, "actor": "...", "name": "...", "type": "..."}
        """

    @unreal_tool(mcp)
    def seek_niagara(
        ctx: Context,
        actor: str,
        age: float = 1.0,
        live: bool = False,
    ) -> Dict[str, Any]:
        """Advance a Niagara sim to a chosen time AND render it, so an editor
        screenshot shows the effect.

        Why this exists: in a non-realtime editor viewport a freshly-spawned
        Niagara system does not render in a screenshot (the sim isn't ticking).
        This forces SetCanRenderWhileSeeking + DesiredAge mode + seek, which
        renders the system at `age` seconds. After tuning, call once with
        live=True to restore normal gameplay ticking before saving the level.

        Args:
            actor: outliner label or internal name of the Niagara actor.
            age:   seconds to advance to for the preview (default 1.0). Use a
                   value past the particles' spawn delay so they're visible.
            live:  True -> restore normal (TickDeltaTime) gameplay sim and
                   re-activate; ignores `age`. Do this before saving the level.

        Returns: {"success": true, "actor": "...", "live": bool}
        """

    @unreal_tool(mcp)
    def set_niagara_module_default(
        ctx: Context,
        system_path: str,
        input: str,
        value: Any,
        module: str = "",
        emitter: str = "",
    ) -> Dict[str, Any]:
        """Set a module input's BAKED default ON THE ASSET, then recompile — no
        User Parameter required.

        This is the "edit the recipe" counterpart to set_niagara_param's "season
        one plate". set_niagara_param overrides a value on a live, already-spawned
        actor and only works for inputs exposed as User Parameters. This instead
        edits the NiagaraSystem asset itself (e.g. shrink "Uniform Sprite Size
        Max" in "Initialize Particle"), so EVERY future spawn uses the new value —
        and it works on ANY module input, exposed or not. Use it to author the
        fixed look of an effect; use set_niagara_param/User Parameters only for
        knobs you want to vary at runtime.

        Drives the same editor view-model the Niagara UI sits on, headless.

        Args:
            system_path: /Game/- or /Niagara/- system path.
            input:       module input name, e.g. "Uniform Sprite Size Max"
                         (case-insensitive, must match exactly).
            value:       a number, or array of numbers for vector/color types
                         (float-backed numeric types only: float/vec2/vec3/color).
            module:      optional case-insensitive substring filter on the
                         owning module's INTERNAL script name — no spaces, e.g.
                         "InitializeParticle", "GravityForce", "AddVelocity" —
                         NOT the display title "Initialize Particle". Use it to
                         disambiguate when an input name appears in >1 module.
            emitter:     optional substring filter on the emitter name.

        Self-correcting: if `input` matches nothing (or is ambiguous), the error
        response includes "available_inputs" — the full
        [{emitter, module, input, type}] list — so you can read the real module
        names + inputs and retry. The asset is left dirty (not auto-saved);
        persist with a save tool afterward.

        Note: building the headless view-model can exceed the MCP socket timeout
        on a COLD compile cache (the edit still completes server-side; the JSON
        response — success or the available_inputs dump — lands in the editor
        log). A warm cache returns normally.

        Returns: {"success": true, "system": "...", "module": "...",
                  "input": "...", "type": "...", "floats_written": N,
                  "dirty": true}
        """

    @unreal_tool(mcp)
    def add_niagara_module(
        ctx: Context,
        system_path: str,
        module: str,
        stage: str = "ParticleUpdate",
        emitter: str = "",
        before: str = "",
    ) -> Dict[str, Any]:
        """Add a whole MODULE to an emitter's stack, then recompile — the
        complement to set_niagara_module_default (which only edits an existing
        module's input). Use it to give an effect a capability it lacks, e.g.
        add a "Curl Noise Force" to Particle Update so particles drift/zig-zag
        on turbulent air currents.

        Args:
            system_path: /Game/- or /Niagara/- system path.
            module:      the module script to add — either a "/Niagara/..."
                         asset path (most reliable) or a bare name matched
                         against module scripts valid for `stage` (e.g.
                         "CurlNoiseForce"). The Curl Noise Force module is
                         "/Niagara/Modules/Update/Forces/CurlNoiseForce".
            stage:       which script stage to add into — ParticleSpawn,
                         ParticleUpdate (default), EmitterSpawn, EmitterUpdate,
                         SystemSpawn, SystemUpdate.
            emitter:     optional substring filter on the emitter name
                         (default: first emitter).
            before:      optional internal module name to insert BEFORE (e.g.
                         "SolveForcesAndVelocity"). ORDER MATTERS for forces — a
                         force added AFTER SolveForcesAndVelocity won't affect
                         that frame's motion. Default: append to the end.

        Then tune the new module's inputs with set_niagara_module_default (e.g.
        CurlNoiseForce's "Noise Strength" / "Noise Frequency"). Same cold-cache
        socket-timeout caveat as set_niagara_module_default (completes
        server-side; response in the editor log).

        Returns: {"success": true, "system": "...", "emitter": "...",
                  "module": "...", "added_node": "...", "stage": "...",
                  "index": N, "dirty": true}
        """
