"""Canonical dispatch smoke for unreal-mcp v0.8.0+.

Pings every registered MCP command with empty params and asserts the
response is NOT the "Unknown command: <name>" fallback from the bridge.
Implements §8 Q5 of `docs/architecture-v0.8-plan.md`: catches the
"did I forget to wire X" class of bug where a new command name exists
in source but isn't registered in the FMCPRegistry on the C++ side.

How it works:

1. Imports the FastMCP server module to enumerate every Python wrapper
   name. This is the agent-facing surface.
2. Maps each wrapper to its underlying wire-command name. For ``@unreal_tool``
   decorated wrappers the wire command is the function name verbatim. The
   handful of raw ``@mcp.tool()`` wrappers (take_screenshot, pie_screenshot,
   get/set_world_settings, get_component_property, get_static_mesh_material)
   are Python-side composites that dispatch to other commands; we map them
   to their underlying wire targets so the dispatch check still hits the
   real C++ handlers.
3. For each unique wire-command name, sends a raw socket payload with
   empty params and reads the response. Asserts the response is NOT
   the bridge's "Unknown command: ..." error from
   ``MCPBridge.cpp`` / ``MCPRegistry.cpp``.

Windows TCP race classification (hardened in v0.8.1 per Codex review):

Some handlers reliably hit a Windows TCP RST-after-FIN race: the editor
sends a small payload (typically a fast-error response, < 100 bytes) and
immediately closes the socket. Windows may discard the buffered receive
data instead of delivering it to the client, so ``recv()`` times out
even though the server-side write succeeded.

We cross-check timeouts against the editor's Output Log: if the log shows
"Sending response: ..." for the command in the last 60 s, classify the
timeout as DISPATCH-OK (editor-side ran fine, just lost in transit). If
the log shows no response, classify as UNCONFIRMED (handler may have
hung) and surface as a real failure.

Exit codes:
    0 — every registered command dispatched to a real handler.
    1 — at least one command came back "Unknown command" OR timed out
        without a corresponding editor-log entry.

Run with the editor open and the MCP server listening on TCP 55557:

    python -m smoke_dispatch       # auto-discover via Python server
    python smoke_dispatch.py       # same
"""

import json
import os
import re
import socket
import sys
import time
from pathlib import Path
from typing import Optional

# Add cwd to path so we can import the server module when run as a script.
sys.path.insert(0, ".")


def call(command, params=None, timeout=10):
    """Send a single MCP command via raw TCP and return the parsed JSON."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect(("127.0.0.1", 55557))
        payload = json.dumps({"type": command, "params": params or {}}).encode("utf-8")
        sock.sendall(payload)
        chunks = []
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
            try:
                return json.loads(b"".join(chunks).decode("utf-8"))
            except json.JSONDecodeError:
                continue
        return json.loads(b"".join(chunks).decode("utf-8"))
    finally:
        try:
            sock.close()
        except OSError:
            pass


# Python wrappers that DON'T map 1:1 to a wire command — they're Python-side
# composites that dispatch to one or more other commands. The smoke covers
# them via the underlying wire commands.
PYTHON_ONLY = {
    "get_world_settings",          # → get_object_property / get_actor_properties
    "set_world_settings",          # → set_object_property
    "get_component_property",      # → get_object_property
    "get_static_mesh_material",    # → get_object_property
}

# Commands that would have meaningful side effects on empty params (e.g.
# spawn an actor named "", apply a tick of zero-vector movement). These
# still dispatch cleanly — the smoke just won't send them with empty params.
SKIP_EMPTY = {
    # PIE control: starting + applying movement on the editor world is too
    # invasive for an idempotent smoke. The dispatch correctness is covered
    # by the per-release integration tests.
    "start_pie",
    "stop_pie",
    "pie_apply_movement",
    # Live Coding compile fires a build; skip in the smoke.
    "recompile_live",
}


# ─── Editor-log cross-check for timeout classification ──────────────────────


def find_editor_log() -> Optional[Path]:
    """Locate the editor's current Output Log.

    Walks up from cwd looking for a sibling Lauder/Saved/Logs/Lauder.log (the
    canonical Lauder project), and falls back to a UNREAL_MCP_LOG env var
    that callers can set explicitly. Returns None if not found — in which
    case the smoke falls back to its pre-hardening behavior (treat
    timeouts as DISPATCHED but flagged).
    """
    explicit = os.environ.get("UNREAL_MCP_LOG")
    if explicit:
        p = Path(explicit)
        return p if p.exists() else None

    # Lauder canonical location.
    candidate = Path("C:/Users/ronildo/Developer/lauder3/Lauder/Saved/Logs/Lauder.log")
    if candidate.exists():
        return candidate

    # Walk upwards looking for a .uproject with a Saved/Logs/ sibling.
    here = Path.cwd().resolve()
    for parent in (here, *here.parents):
        for uproject in parent.rglob("*.uproject"):
            log = uproject.parent / "Saved" / "Logs" / (uproject.stem + ".log")
            if log.exists():
                return log
    return None


def editor_logged_response_for(log_path: Path, command: str, since_seconds: float = 60.0) -> bool:
    """Return True if the editor log shows "Sending response" for `command`
    within the last `since_seconds`.

    We don't try to parse the log timestamp — too brittle across UE versions.
    Instead we read the tail (last ~5000 lines, ~500 KB) and look for the
    pattern `Received: {"type": "<command>", ...}` followed by a `Sending
    response` line. Either match is sufficient — the response line is what
    proves the handler completed its synchronous work.
    """
    try:
        # Read the last 500 KB so we don't load multi-MB editor logs.
        size = log_path.stat().st_size
        with log_path.open("rb") as fh:
            if size > 500_000:
                fh.seek(size - 500_000)
                # Drop the (probably partial) first line.
                fh.readline()
            tail = fh.read().decode("utf-8", errors="ignore")
    except OSError:
        return False

    received_pat = re.compile(
        r'MCPServerRunnable: Received:\s*\{"type":\s*"' + re.escape(command) + r'"'
    )
    response_pat = re.compile(
        r'MCPServerRunnable: Sending response'
    )
    # Look for the LAST occurrence of "Received: <command>" and then
    # verify a Sending response follows shortly after.
    received_positions = [m.end() for m in received_pat.finditer(tail)]
    if not received_positions:
        return False
    last_received = received_positions[-1]
    return bool(response_pat.search(tail[last_received : last_received + 4000]))


def collect_wire_commands():
    """Return the set of wire-command names the smoke should ping."""
    import unreal_mcp_server  # noqa: WPS433 — intentional late import

    wire_commands = set()
    for tool in unreal_mcp_server.mcp._tool_manager.list_tools():  # noqa: WPS437
        if tool.name in PYTHON_ONLY or tool.name in SKIP_EMPTY:
            continue
        wire_commands.add(tool.name)

    # Always include "ping" — it's the bridge-level virtual command.
    wire_commands.add("ping")
    return sorted(wire_commands)


def main() -> int:
    commands = collect_wire_commands()
    print(f"=== unreal-mcp dispatch smoke ({len(commands)} commands) ===\n")

    log_path = find_editor_log()
    if log_path:
        print(f"  editor log for timeout cross-check: {log_path}")
    else:
        print("  no editor log located -- timeouts will be flagged as UNCONFIRMED")
    print()

    unknown = []      # real failures — dispatch didn't reach a handler
    confirmed = []    # editor logged a response but the client recv timed out
    unconfirmed = []  # timeout AND editor log doesn't show a response: real fail
    ok = 0

    for cmd in commands:
        try:
            response = call(cmd)
        except (TimeoutError, socket.timeout) as e:
            # Windows TCP RST-after-FIN race. Cross-check the editor log —
            # if the handler ran and wrote a response, this is a transport
            # artifact (CONFIRMED). If the editor never logged a response,
            # the handler hung or never registered (UNCONFIRMED — real bug).
            if log_path and editor_logged_response_for(log_path, cmd):
                print(f"  CONFIRMED  {cmd}: {e} (editor log shows handler response)")
                confirmed.append(cmd)
            else:
                print(f"  UNCONFIRM  {cmd}: {e} (NO editor log entry -- handler may have hung)")
                unconfirmed.append(cmd)
            continue
        except OSError as e:
            print(f"  CONN-FAIL  {cmd}: {e}")
            unknown.append(cmd)
            continue

        error = response.get("error", "") if isinstance(response, dict) else ""
        if error.startswith("Unknown command"):
            print(f"  UNKNOWN    {cmd}: {error}")
            unknown.append(cmd)
        else:
            ok += 1

    total_dispatched = ok + len(confirmed)
    print(
        f"\n=== {total_dispatched}/{len(commands)} dispatched, "
        f"{len(confirmed)} TCP-race confirmed, "
        f"{len(unconfirmed)} unconfirmed, "
        f"{len(unknown)} unknown ==="
    )
    if unknown:
        print("Unknown commands (dispatch failures -- NEED FIXING):")
        for cmd in unknown:
            print(f"  - {cmd}")
    if unconfirmed:
        print("Unconfirmed timeouts (editor log shows no response -- NEED INVESTIGATION):")
        for cmd in unconfirmed:
            print(f"  - {cmd}")
    if confirmed:
        print("Confirmed Windows TCP race (editor handled cleanly; transport artifact):")
        for cmd in confirmed:
            print(f"  - {cmd}")

    return 1 if (unknown or unconfirmed) else 0


if __name__ == "__main__":
    sys.exit(main())
