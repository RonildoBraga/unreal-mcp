"""Shared fixtures for unreal-mcp integration tests.

The tests in this directory require:
  1. The UE editor running with the UnrealMCP plugin loaded.
  2. The plugin's TCP server listening on 127.0.0.1:55557 (default).
  3. A level loaded — most tests target L_Base, but any level with at least
     one WorldSettings actor will work for the WorldSettings round-trips.

Run from the `server/` directory:

    ./.venv/Scripts/pytest tests/                 # all tests
    ./.venv/Scripts/pytest tests/test_object_property.py -v
    ./.venv/Scripts/pytest tests/ -k "world_settings"

If the editor isn't running, every test errors out at the first `call()` —
this is intentional. We don't auto-skip; integration tests should fail
loudly when their preconditions aren't met.
"""
import json
import socket

import pytest

UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557


def _call(command: str, params: dict | None = None, timeout: float = 10.0) -> dict:
    """Send a single MCP command via raw TCP and return the parsed JSON.

    Used directly by the fixtures below. Tests get a higher-level `call`
    fixture that wraps this with a retry on the Windows TCP RST-after-FIN
    race we see on fast-error responses.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect((UNREAL_HOST, UNREAL_PORT))
        payload = json.dumps({"type": command, "params": params or {}}).encode("utf-8")
        sock.sendall(payload)
        chunks: list[bytes] = []
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


@pytest.fixture(scope="session")
def editor_running() -> None:
    """Verify the editor is up before any tests run.

    Hits the `ping` virtual command. Fails the entire session with a
    helpful message if the socket connect or ping fails.
    """
    try:
        response = _call("ping", timeout=2.0)
    except (TimeoutError, socket.timeout, ConnectionRefusedError, OSError) as e:
        pytest.fail(
            f"Could not reach the UnrealMCP plugin on {UNREAL_HOST}:{UNREAL_PORT} "
            f"({type(e).__name__}: {e}). Start the editor with the UnrealMCP "
            f"plugin loaded, then re-run pytest."
        )
    if not response.get("success"):
        pytest.fail(f"Editor reachable but ping failed: {response}")


@pytest.fixture
def call(editor_running):
    """Test-facing call function with TCP-race retry.

    The Windows TCP RST-after-FIN race on small-payload fast-error responses
    (documented in smoke_dispatch.py) means a single TimeoutError isn't a
    real failure — the editor sent the response, the client just didn't
    read it in time. We retry twice on TimeoutError before propagating.
    """

    def _do_call(command: str, params: dict | None = None, timeout: float = 10.0) -> dict:
        last_err: Exception | None = None
        for _attempt in range(3):
            try:
                return _call(command, params, timeout)
            except (TimeoutError, socket.timeout, ConnectionResetError) as e:
                last_err = e
                continue
        # Genuine transport failure — let the test catch it as itself.
        raise last_err or RuntimeError("call failed without exception")

    return _do_call
