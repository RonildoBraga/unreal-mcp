"""@unreal_tool decorator — absorbs per-wrapper boilerplate.

v0.8.0 Stage 2c-ii-b. Replaces the ~10 LOC of repeated
connection / send / error-envelope code in every per-category tool
file (asset_tools.py, editor_tools.py, etc.) with a single decorator
that introspects the function signature.

## Pattern

Before (v0.7.x — repeated 78 times):

    @mcp.tool()
    def get_current_level(ctx: Context) -> Dict[str, Any]:
        '''Get the currently-loaded editor level.'''
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}
            return _unwrap(unreal.send_command("get_current_level", {}))
        except Exception as e:
            logger.error(f"get_current_level error: {e}")
            return {"error": str(e)}

After (v0.8.0):

    @unreal_tool(mcp)
    def get_current_level(ctx: Context) -> Dict[str, Any]:
        '''Get the currently-loaded editor level.'''

The decorator:

1. Default command name = function name (`get_current_level`).
   Override with `@unreal_tool(mcp, command="...")` if needed.
2. Parameters (other than `ctx`) become wire payload keys, byname.
3. Args whose runtime value is the `OMIT` sentinel are dropped from
   the payload (use for tools with optional override args — e.g.
   `set_actor_transform(name, location=OMIT, rotation=OMIT, scale=OMIT)`).
4. Connection failure / no response / exception → strict-shape error
   dict `{"success": False, "error": "..."}`.
5. Successful response — returned verbatim (the v0.8.0 wire format
   is already `{success, error?, ...payload}` so no unwrapping is
   needed).

## Post-processing

A few tools need to transform the response after dispatch (e.g.
`take_screenshot` loads the PNG bytes off disk and returns an
`Image` content object). Those tools DON'T use this decorator —
they call `dispatch_unreal_command()` explicitly inside a raw
`@mcp.tool()` function.
"""

import functools
import inspect
import logging
from typing import Any, Callable, Dict, Optional

from mcp.server.fastmcp import FastMCP

logger = logging.getLogger("UnrealMCP")


class _Sentinel:
    """Sentinel object — distinct from None, identity-comparable."""

    def __repr__(self) -> str:  # pragma: no cover
        return "OMIT"


OMIT = _Sentinel()
"""Default value for optional wrapper args — drops the arg from the wire payload.

The decorator also treats Python ``None`` as a drop signal, so wrappers
can use the more conventional ``Optional[str] = None`` pattern for
optional-string fields (FastMCP / pydantic generate clean JSON schemas
from those, but reject the custom ``OMIT`` sentinel as a default for
typed parameters). Use ``OMIT`` only when the type itself is non-Optional
(e.g. ``location: List[float] = OMIT`` for a vector param where ``None``
isn't a valid runtime value).

Example using both:

    @unreal_tool(mcp)
    def set_actor_transform(
        ctx: Context,
        name: str,
        location: List[float] = OMIT,        # vector — type-honest
        rotation: List[float] = OMIT,
        scale:    List[float] = OMIT,
    ) -> Dict[str, Any]:
        '''Set one or more transform components on an actor.'''

    @unreal_tool(mcp)
    def list_assets(
        ctx: Context,
        path: str,
        class_filter: Optional[str] = None,  # str|None — pydantic-friendly
    ) -> Dict[str, Any]:
        '''List assets — class_filter omitted when None.'''

Callers pass only what they want updated; ``OMIT``-defaulted and
``None``-valued args don't reach the C++ handler at all, so the
handler's "preserve existing on missing field" branch is honored.
"""


def dispatch_unreal_command(
    command: str,
    params: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """Send a command to Unreal — strict-shape response, no exceptions thrown.

    Returns ``{"success": False, "error": "..."}`` on any failure
    (connection refused, no response, dispatch exception). On success
    returns whatever the C++ handler produced, which in v0.8.0 is
    always ``{"success": True, ...payload}``.

    Use this directly when a wrapper needs to pre-process params,
    post-process the response, or chain multiple commands. The
    ``@unreal_tool`` decorator wraps this for the common case.
    """
    # Imported lazily to break the unreal_mcp_server ↔ tools circular import.
    from unreal_mcp_server import get_unreal_connection

    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        response = unreal.send_command(command, params or {})
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        return response

    except Exception as e:
        logger.exception(f"{command} dispatch error")
        return {"success": False, "error": str(e)}


def unreal_tool(
    mcp: FastMCP,
    *,
    command: Optional[str] = None,
    transform_params: Optional[Callable[[Dict[str, Any]], Dict[str, Any]]] = None,
) -> Callable[[Callable[..., Any]], Callable[..., Any]]:
    """Register an Unreal MCP tool — auto-dispatch from function signature.

    Args:
        mcp:              The FastMCP server instance to register on.
        command:          Wire-level command name. Defaults to ``func.__name__``.
        transform_params: Optional function that mutates the params dict
                          before send. Use for cases like uppercasing
                          a type field or ensuring vector arg shape.
                          Receives the post-OMIT-filter dict, returns
                          the dict to actually send.

    The decorated function's body is IGNORED — only its signature and
    docstring matter. Keep the body as just a docstring (Python
    requires *some* statement; an empty docstring counts).

    For tools that need to post-process the response or run additional
    logic, don't use this decorator — write a plain ``@mcp.tool()`` that
    calls ``dispatch_unreal_command()`` explicitly.
    """

    def decorator(func: Callable[..., Any]) -> Callable[..., Any]:
        cmd = command or func.__name__
        sig = inspect.signature(func)

        # Parameters that map onto the wire payload — every signature
        # parameter except `ctx` (FastMCP's Context injection).
        param_names = [
            name
            for name, p in sig.parameters.items()
            if name != "ctx"
            and p.kind
            in (
                inspect.Parameter.POSITIONAL_OR_KEYWORD,
                inspect.Parameter.KEYWORD_ONLY,
            )
        ]

        @functools.wraps(func)
        def wrapper(*args, **kwargs) -> Dict[str, Any]:
            try:
                bound = sig.bind(*args, **kwargs)
                bound.apply_defaults()
            except TypeError as e:
                return {"success": False, "error": f"Invalid arguments: {e}"}

            params: Dict[str, Any] = {}
            for name in param_names:
                value = bound.arguments.get(name, OMIT)
                # Drop both OMIT sentinels and None values — both mean
                # "user did not specify, let C++ default apply".
                if value is OMIT or value is None:
                    continue
                params[name] = value

            if transform_params is not None:
                try:
                    params = transform_params(params)
                except Exception as e:
                    return {"success": False, "error": f"Param transform failed: {e}"}

            return dispatch_unreal_command(cmd, params)

        # Register with FastMCP. functools.wraps preserves __wrapped__
        # so inspect.signature(wrapper) sees the original signature —
        # FastMCP's tools/list surfaces the docstring + types correctly.
        return mcp.tool()(wrapper)

    return decorator
