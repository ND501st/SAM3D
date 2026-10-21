"""Keep Claude Code connected to the Unreal editor's MCP server while the editor is closed or restarting.

Claude Code connects to the servers in .mcp.json once, when a session starts, and does not retry. The editor's MCP
server (http://127.0.0.1:8000/mcp, unreal/README.md) exists only while the editor is open, so a session started
before the editor never got `unreal-mcp`, and the desktop app has no /mcp to reconnect it. This proxy is what
.mcp.json starts instead (stdio): it answers at once, finds the editor when it comes up, and tells Claude Code to
fetch the tools again (notifications/tools/list_changed). The editor closing or restarting is handled the same way,
and the editor's own list_changed notifications (sent on its event stream as toolsets register) are passed on.
Standard library only.

    python tools/unreal_mcp_proxy.py [--url http://127.0.0.1:8000/mcp] [--poll 5]

Besides the editor's tools it always offers one of its own, `proxy_status`: whether the editor is reachable, checked
when called. Logs go to stderr, which Claude Code keeps in its MCP logs. UNREAL_MCP_URL overrides the default URL.
"""
import argparse
import itertools
import json
import os
import sys
import threading
import urllib.error
import urllib.request

DEFAULT_URL = os.environ.get("UNREAL_MCP_URL", "http://127.0.0.1:8000/mcp")
PROTOCOL_VERSION = "2025-06-18"
CHECK_TIMEOUT_S = 3.0
LIST_TIMEOUT_S = 30.0
STATUS_TOOL = {
    "name": "proxy_status",
    "description": "Whether the Unreal editor's MCP server is reachable now (checked when called). The editor's own "
                   "tools appear in this server's tool list only while the editor is open with its MCP server on.",
    "inputSchema": {"type": "object", "properties": {}},
}
INSTRUCTIONS = ("Proxy to the Unreal editor's MCP server. The editor's tools are listed while the editor is open "
                "and vanish when it closes; call proxy_status to check the connection.")
PASSED_ON = ("notifications/tools/list_changed", "notifications/resources/list_changed")


def log(text):
    print(f"unreal_mcp_proxy: {text}", file=sys.stderr, flush=True)


class Offline(Exception):
    """The editor's MCP server can't be reached: the editor is closed or its server is off."""


class Busy(Exception):
    """The editor's server is there but didn't answer in time (its game thread is busy)."""


class ServerError(Exception):
    """The editor's server is there but answered with an HTTP error."""


class Upstream:
    """One MCP session with the editor's server, over Streamable HTTP."""

    def __init__(self, url):
        self.url = url
        self.session_id = None
        self.protocol_version = None
        self.ids = itertools.count(1)
        self.lock = threading.Lock()
        self.opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))  # never via an HTTP proxy

    def reset(self):
        self.session_id = None
        self.protocol_version = None

    def request(self, method, params=None, request_id=None, timeout=None):
        """Send one request and return its JSON-RPC response (a dict with "result" or "error")."""
        message = {"jsonrpc": "2.0", "id": request_id if request_id is not None else f"proxy-{next(self.ids)}",
                   "method": method}
        if params is not None:
            message["params"] = params
        with self.lock:
            if self.session_id is None:
                self._initialize()
        try:
            return self._post(message, timeout)
        except urllib.error.HTTPError as e:
            if e.code not in (400, 404):
                raise ServerError(f"HTTP {e.code} for {method}")
        # The editor restarted and forgot our session: start a new one and try once more.
        with self.lock:
            self.reset()
            self._initialize()
        try:
            return self._post(message, timeout)
        except urllib.error.HTTPError as e:
            raise ServerError(f"HTTP {e.code} for {method} in a new session")

    def notify(self, method, params):
        """Pass a notification on, if there is a session to pass it to; failures don't matter."""
        if self.session_id is None:
            return
        try:
            self._post({"jsonrpc": "2.0", "method": method, "params": params}, CHECK_TIMEOUT_S)
        except (urllib.error.HTTPError, Offline, Busy):
            pass

    def events(self):
        """The messages the editor's server sends on its event stream (GET), until the stream ends."""
        headers = self._headers()
        headers["Accept"] = "text/event-stream"
        try:
            with self.opener.open(urllib.request.Request(self.url, headers=headers, method="GET")) as resp:
                yield from sse_messages(resp)
        except urllib.error.HTTPError as e:
            if e.code == 405:  # this server offers no event stream
                raise ServerError(f"HTTP {e.code} for the event stream")
            raise Offline(f"HTTP {e.code} for the event stream")
        except (urllib.error.URLError, OSError, ValueError) as e:
            raise Offline(str(e))

    def _initialize(self):
        params = {"protocolVersion": PROTOCOL_VERSION, "capabilities": {},
                  "clientInfo": {"name": "sam3d-unreal-mcp-proxy", "version": "1"}}
        try:
            answer = self._post({"jsonrpc": "2.0", "id": f"proxy-{next(self.ids)}", "method": "initialize",
                                 "params": params}, CHECK_TIMEOUT_S)
            if "result" not in answer:
                raise Offline(f"initialize failed: {answer.get('error')}")
            self.protocol_version = answer["result"].get("protocolVersion")
            self._post({"jsonrpc": "2.0", "method": "notifications/initialized"}, CHECK_TIMEOUT_S)
        except urllib.error.HTTPError as e:
            self.reset()
            raise Offline(f"HTTP {e.code} for initialize")
        except (Offline, Busy):
            self.reset()
            raise

    def _headers(self):
        headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        if self.protocol_version:
            headers["MCP-Protocol-Version"] = self.protocol_version
        return headers

    def _post(self, message, timeout):
        req = urllib.request.Request(self.url, data=json.dumps(message).encode("utf-8"), headers=self._headers(),
                                     method="POST")
        try:
            with self.opener.open(req, timeout=timeout) as resp:
                session_id = resp.headers.get("Mcp-Session-Id")
                if session_id:
                    self.session_id = session_id
                if "id" not in message:
                    return None
                return read_answer(resp, message["id"])
        except urllib.error.HTTPError:
            raise
        except urllib.error.URLError as e:
            if isinstance(e.reason, TimeoutError):
                raise Busy(str(e.reason))
            raise Offline(str(e.reason))
        except TimeoutError as e:
            raise Busy(str(e))
        except (OSError, ValueError) as e:
            raise Offline(str(e))


def sse_messages(resp):
    """The JSON messages in an event stream (text/event-stream), one per event."""
    data = []
    for raw in resp:
        line = raw.decode("utf-8").rstrip("\r\n")
        if line.startswith("data:"):
            data.append(line[6:] if line.startswith("data: ") else line[5:])
        elif line == "" and data:
            yield json.loads("\n".join(data))
            data = []
    if data:
        yield json.loads("\n".join(data))


def read_answer(resp, request_id):
    """The response to request_id from a plain JSON body or from an event stream."""
    if resp.headers.get("Content-Type", "").startswith("text/event-stream"):
        for message in sse_messages(resp):
            answer = matching(message, request_id)
            if answer:
                return answer
        raise Offline("the event stream ended without an answer")
    answer = matching(json.loads(resp.read() or b"null"), request_id)
    if not answer:
        raise Offline("no answer in the response")
    return answer


def matching(message, request_id):
    for m in message if isinstance(message, list) else [message]:
        if isinstance(m, dict) and m.get("id") == request_id and ("result" in m or "error" in m):
            return m
    return None


class Proxy:
    """The stdio MCP server Claude Code talks to."""

    def __init__(self, upstream, out, poll_s):
        self.upstream = upstream
        self.out = out
        self.poll_s = poll_s
        self.online = None
        self.ready = threading.Event()
        self.stopped = threading.Event()
        self.write_lock = threading.Lock()
        self.state_lock = threading.Lock()
        self.calls_lock = threading.Lock()
        self.calls_in_flight = 0

    def send(self, message):
        with self.write_lock:
            self.out.write((json.dumps(message) + "\n").encode("utf-8"))
            self.out.flush()

    def tell(self, method):
        """Send the client a notification, once it has finished initializing."""
        if self.ready.is_set():
            self.send({"jsonrpc": "2.0", "method": method})

    def check(self):
        """Ask the editor whether it is there; on a change, tell the client to list the tools again."""
        reason = None
        try:
            self.upstream.request("ping", timeout=CHECK_TIMEOUT_S)
            online = True
        except Busy:
            return bool(self.online)
        except ServerError:
            online = True
        except Offline as e:
            self.upstream.reset()
            online, reason = False, e
        with self.state_lock:
            was, self.online = self.online, online
        if online != was:
            log(f"editor MCP server at {self.upstream.url} is {'up' if online else f'down ({reason})'}")
            if was is not None:  # the first look changes nothing the client has fetched
                self.tell("notifications/tools/list_changed")
                self.tell("notifications/resources/list_changed")
        return online

    def poll(self):
        while not self.stopped.wait(self.poll_s):
            with self.calls_lock:
                busy = self.calls_in_flight > 0
            if not busy:
                self.check()

    def listen(self):
        """Pass on the editor's own list_changed notifications, from its event stream."""
        while not self.stopped.wait(self.poll_s):
            if not self.online or self.upstream.session_id is None:
                continue
            try:
                for message in self.upstream.events():
                    if isinstance(message, dict) and message.get("method") in PASSED_ON:
                        self.tell(message["method"])
            except ServerError as e:
                log(f"no event stream from the editor ({e}); its own list changes won't be passed on")
                return
            except (Offline, Busy):
                pass

    def handle(self, message):
        method, request_id = message.get("method"), message.get("id")
        if method is None:
            return
        if request_id is None:
            if method == "notifications/initialized":
                self.ready.set()
            elif method == "notifications/cancelled" and self.online:
                self.upstream.notify(method, message.get("params") or {})
            return
        try:
            result = self.dispatch(method, message.get("params") or {}, request_id)
        except Exception as e:  # a bug here must not kill the connection
            log(f"{method} failed: {e!r}")
            result = {"error": {"code": -32603, "message": f"proxy error: {e}"}}
        self.send({"jsonrpc": "2.0", "id": request_id, **result})

    def dispatch(self, method, params, request_id):
        if method == "initialize":
            return {"result": {"protocolVersion": params.get("protocolVersion", PROTOCOL_VERSION),
                               "capabilities": {"tools": {"listChanged": True}, "resources": {"listChanged": True}},
                               "serverInfo": {"name": "unreal-mcp-proxy", "version": "1.0"},
                               "instructions": INSTRUCTIONS}}
        if method == "ping":
            return {"result": {}}
        if method == "tools/list":
            return self.list_tools(params, request_id)
        if method == "tools/call":
            return self.call_tool(params, request_id)
        if method == "resources/list":
            return self.forward(method, params, request_id, {"resources": []})
        if method == "resources/templates/list":
            return self.forward(method, params, request_id, {"resourceTemplates": []})
        if method == "resources/read":
            return self.forward(method, params, request_id, None)
        return {"error": {"code": -32601, "message": f"Method not found: {method}"}}

    def forward(self, method, params, request_id, offline_result, timeout=LIST_TIMEOUT_S):
        """Pass a request to the editor; offline_result (or an error, if None) when it isn't there."""
        offline = ({"result": offline_result} if offline_result is not None
                   else {"error": {"code": -32603, "message": self.offline_text()}})
        if not self.online and not self.check():
            return offline
        try:
            answer = self.upstream.request(method, params or None, request_id=request_id, timeout=timeout)
        except (Offline, Busy):
            self.check()
            return offline
        except ServerError as e:
            return {"error": {"code": -32603, "message": str(e)}}
        return {key: answer[key] for key in ("result", "error") if key in answer}

    def list_tools(self, params, request_id):
        own = [] if params.get("cursor") else [STATUS_TOOL]
        answer = self.forward("tools/list", params, request_id, {"tools": []})
        if "result" in answer:
            answer["result"] = dict(answer["result"], tools=list(answer["result"].get("tools", [])) + own)
        return answer

    def call_tool(self, params, request_id):
        if params.get("name") == STATUS_TOOL["name"]:
            online = self.check()
            text = (f"The Unreal editor's MCP server at {self.upstream.url} is reachable."
                    if online else self.offline_text())
            return {"result": {"content": [{"type": "text", "text": text}], "isError": not online}}
        if not self.online and not self.check():
            return {"result": {"content": [{"type": "text", "text": self.offline_text()}], "isError": True}}
        with self.calls_lock:
            self.calls_in_flight += 1
        lost = None
        try:
            answer = self.upstream.request("tools/call", params, request_id=request_id)
        except (Offline, Busy, ServerError) as e:
            lost = e
        finally:
            with self.calls_lock:
                self.calls_in_flight -= 1
        if lost:
            self.check()
            text = f"Lost the Unreal editor during the call ({lost})."
            return {"result": {"content": [{"type": "text", "text": text}], "isError": True}}
        return {key: answer[key] for key in ("result", "error") if key in answer}

    def offline_text(self):
        return (f"The Unreal editor's MCP server at {self.upstream.url} is not reachable: open "
                "unreal/SAM3D/SAM3D.uproject (the server starts with the editor), or run "
                "ModelContextProtocol.StartServer in the editor console. Its tools appear here by themselves once it "
                "is up.")

    def serve(self, stream):
        threading.Thread(target=self.poll, daemon=True).start()
        threading.Thread(target=self.listen, daemon=True).start()
        for raw in stream:
            line = raw.decode("utf-8").strip()
            if not line:
                continue
            try:
                message = json.loads(line)
            except ValueError:
                self.send({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "Parse error"}})
                continue
            for m in message if isinstance(message, list) else [message]:
                if isinstance(m, dict):
                    threading.Thread(target=self.handle, args=(m,), daemon=True).start()
        self.stopped.set()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--url", default=DEFAULT_URL, help="the editor's MCP endpoint")
    parser.add_argument("--poll", type=float, default=5.0, help="seconds between checks for the editor")
    args = parser.parse_args(argv)
    proxy = Proxy(Upstream(args.url), sys.stdout.buffer, args.poll)
    log(f"started for {args.url}")
    proxy.serve(sys.stdin.buffer)
    return 0


if __name__ == "__main__":
    sys.exit(main())
