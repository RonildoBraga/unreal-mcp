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
