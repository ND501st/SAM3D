"""tools/unreal_mcp_proxy.py keeps `unreal-mcp` connected while the editor comes and goes (unreal/README.md).

A stub stands in for the editor's MCP server (Streamable HTTP, sessions, answers as JSON or as an event stream); the
proxy runs as Claude Code runs it, over stdio.
"""
import json
import queue
import socket
import subprocess
import sys
import threading
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

PROXY = Path(__file__).resolve().parent.parent / "unreal_mcp_proxy.py"
EDITOR_TOOL = {"name": "list_toolsets", "description": "stub", "inputSchema": {"type": "object", "properties": {}}}


class StubEditor:
    """The editor's MCP server, as far as the proxy can tell."""

    def __init__(self, port):
        self.session = None
        self.events = queue.Queue()
        self.stream_open = threading.Event()
        self.closed = threading.Event()
        stub = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                if self.headers.get("Mcp-Session-Id") != stub.session:
                    self.send_response(404)
                    self.end_headers()
                    return
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.end_headers()
                stub.stream_open.set()
                while not stub.closed.is_set():
                    try:
                        note = stub.events.get(timeout=0.1)
                    except queue.Empty:
                        continue
                    self.wfile.write(f"data: {json.dumps(note)}\n\n".encode("utf-8"))
                    self.wfile.flush()

            def do_POST(self):
                message = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                method = message.get("method")
                if method == "initialize":
                    stub.session = uuid.uuid4().hex
                    self.reply({"protocolVersion": "2025-06-18", "capabilities": {"tools": {}},
                                "serverInfo": {"name": "stub-editor", "version": "0"}}, message, stub.session)
                    return
                if self.headers.get("Mcp-Session-Id") != stub.session:
                    self.send_response(404)
                    self.end_headers()
                    return
                if "id" not in message:
                    self.send_response(202)
                    self.end_headers()
                elif method == "tools/list":
                    self.reply({"tools": [EDITOR_TOOL]}, message, stream=True)
                elif method == "tools/call":
                    self.reply({"content": [{"type": "text", "text": f"called {message['params']['name']}"}]},
                               message)
                elif method == "resources/list":
                    self.reply({"resources": [{"uri": "unreal://level", "name": "level"}]}, message)
                else:
                    self.reply({}, message)

            def reply(self, result, message, session=None, stream=False):
                body = json.dumps({"jsonrpc": "2.0", "id": message["id"], "result": result})
                if stream:
                    note = json.dumps({"jsonrpc": "2.0", "method": "notifications/message", "params": {}})
                    data = f"event: message\ndata: {note}\n\nevent: message\ndata: {body}\n\n".encode("utf-8")
                else:
                    data = body.encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream" if stream else "application/json")
                self.send_header("Content-Length", str(len(data)))
                if session:
                    self.send_header("Mcp-Session-Id", session)
                self.end_headers()
                self.wfile.write(data)

        self.server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def forget_sessions(self):
        self.session = "restarted"

    def announce(self, method):
        self.events.put({"jsonrpc": "2.0", "method": method})

    def close(self):
        self.closed.set()
        self.server.shutdown()
        self.server.server_close()


class Client:
    """Claude Code's side of the stdio connection."""

    def __init__(self, url):
        self.proc = subprocess.Popen([sys.executable, str(PROXY), "--url", url, "--poll", "0.2"],
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        self.answers, self.notes, self.ids = {}, queue.Queue(), iter(range(1, 1000))
        self.arrived = threading.Condition()
        threading.Thread(target=self.read, daemon=True).start()

    def read(self):
        for line in self.proc.stdout:
            message = json.loads(line)
            if "id" in message:
                with self.arrived:
                    self.answers[message["id"]] = message
                    self.arrived.notify_all()
            else:
                self.notes.put(message)

    def write(self, message):
        self.proc.stdin.write((json.dumps(message) + "\n").encode("utf-8"))
        self.proc.stdin.flush()

    def rpc(self, method, params=None):
        request_id = next(self.ids)
        self.write({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params or {}})
        with self.arrived:
            assert self.arrived.wait_for(lambda: request_id in self.answers, timeout=10), method
            return self.answers.pop(request_id)

    def tool_names(self):
        return [t["name"] for t in self.rpc("tools/list")["result"]["tools"]]

    def wait_for_list_changed(self):
        deadline = time.time() + 10
        while time.time() < deadline:
            note = self.notes.get(timeout=deadline - time.time())
            if note["method"] == "notifications/tools/list_changed":
                return
        raise AssertionError("no tools/list_changed")

    def drain_notes(self):
        while not self.notes.empty():
            self.notes.get()

    def close(self):
        self.proc.stdin.close()
        self.proc.wait(timeout=10)


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def test_the_editor_coming_restarting_and_going():
    port = free_port()
    client = Client(f"http://127.0.0.1:{port}/mcp")
    editor = None
    try:
        hello = client.rpc("initialize", {"protocolVersion": "2025-11-25", "capabilities": {},
                                          "clientInfo": {"name": "test", "version": "0"}})["result"]
        assert hello["capabilities"]["tools"]["listChanged"] is True
        assert hello["protocolVersion"] == "2025-11-25"
        client.write({"jsonrpc": "2.0", "method": "notifications/initialized"})

        # Editor closed: the session still connects, with only the proxy's own tool.
        assert client.tool_names() == ["proxy_status"]
        call = client.rpc("tools/call", {"name": "list_toolsets", "arguments": {}})["result"]
        assert call["isError"] and "not reachable" in call["content"][0]["text"]
        assert client.rpc("resources/list")["result"] == {"resources": []}

        # The editor opens: the client is told, and the editor's tools are listed and work.
        editor = StubEditor(port)
        client.wait_for_list_changed()
        assert client.tool_names() == ["list_toolsets", "proxy_status"]
        call = client.rpc("tools/call", {"name": "list_toolsets", "arguments": {}})["result"]
        assert call["content"][0]["text"] == "called list_toolsets"
        status = client.rpc("tools/call", {"name": "proxy_status", "arguments": {}})["result"]
        assert not status["isError"]
        assert client.rpc("resources/list")["result"]["resources"][0]["uri"] == "unreal://level"

        # The editor's own list_changed (toolsets registering after start-up) is passed on.
        assert editor.stream_open.wait(10)
        client.drain_notes()
        editor.announce("notifications/tools/list_changed")
        client.wait_for_list_changed()

        # The editor restarts and forgets the session: the proxy starts a new one by itself.
        editor.forget_sessions()
        call = client.rpc("tools/call", {"name": "list_toolsets", "arguments": {}})["result"]
        assert call["content"][0]["text"] == "called list_toolsets"

        # The editor closes: the client is told, and only the proxy's tool is left.
        editor.close()
        editor = None
        client.wait_for_list_changed()
        assert client.tool_names() == ["proxy_status"]
    finally:
        if editor:
            editor.close()
        client.close()


def test_unknown_methods_are_not_found():
    client = Client(f"http://127.0.0.1:{free_port()}/mcp")
    try:
        assert client.rpc("server/discover")["error"]["code"] == -32601
        assert client.rpc("ping")["result"] == {}
    finally:
        client.close()
