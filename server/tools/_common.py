"""Shared helpers for the per-category MCP tool wrappers.

Previously this module's `_unwrap` was duplicated identically across four
tool files (asset_tools, level_tools, material_tools, outliner_tools).
Centralizing it here keeps the response-envelope contract in one place.
"""
from typing import Any, Dict, Optional


def _unwrap(response: Optional[Dict[str, Any]]) -> Dict[str, Any]:
    """Pull the result payload out of an Unreal MCP response envelope.

    The C++ side wraps successful results as `{result: {...}}` and errors as
    `{error: "..."}`. Both shapes have been seen historically so we accept
    either, and fall back to returning the whole response on shape mismatch.
    """
    if not response:
        return {"error": "no response from Unreal"}
    if "error" in response:
        return {"error": response["error"]}
    if "result" in response:
        return response["result"]
    return response
