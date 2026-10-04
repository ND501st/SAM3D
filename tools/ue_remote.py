"""Run Python inside a running Unreal editor (Python remote execution) and print what it prints.

Needs the editor open with remote execution on (Config/DefaultEngine.ini,
[/Script/PythonScriptPlugin.PythonScriptPluginSettings] bRemoteExecution=True). Uses the client that ships with
the engine, so nothing to install.

    python tools/ue_remote.py "print(unreal.SystemLibrary.get_engine_version())"
    python tools/ue_remote.py -f tools/ue/pie_start.py

The code runs as a script in the editor's Python (the `unreal` module is imported for you). Exit codes: 0 ok,
1 the code raised, 2 no editor found. Set UE_ROOT if the engine isn't at D:\\UE_5.8.

From other scripts:

    with Editor() as ue:
        ok, output = ue.run("print(1 + 1)")

Each call runs on the editor's game thread and blocks it, so keep calls short and wait between them out here.
"""
import argparse
import os
import sys
import time

UE_ROOT = os.environ.get("UE_ROOT", r"D:\UE_5.8")
sys.path.insert(0, os.path.join(UE_ROOT, "Engine", "Plugins", "Experimental", "PythonScriptPlugin", "Content", "Python"))
import remote_execution  # noqa: E402  (ships with the engine)

PROJECT = "SAM3D"


class EditorNotFound(RuntimeError):
    pass


class Editor:
    """A command connection to the open SAM3D editor (the first editor found if none is SAM3D)."""

    def __init__(self, timeout_s=5.0):
        self._timeout_s = timeout_s
        self._rex = None

    def __enter__(self):
        self._rex = remote_execution.RemoteExecution(remote_execution.RemoteExecutionConfig())
        self._rex.start()
        node = self._find_node()
        if node is None:
            self._rex.stop()
            raise EditorNotFound("No Unreal editor answered. Is it open, with remote execution on?")
        self._rex.open_command_connection(node["node_id"])
        return self

    def __exit__(self, *exc):
        self._rex.stop()
        return False

    def run(self, code):
        """Run code in the editor. Returns (succeeded, printed output; the error text on failure)."""
        result = self._rex.run_command("import unreal\n" + code, unattended=True,
                                       exec_mode=remote_execution.MODE_EXEC_FILE)
        out = "".join(line.get("output", "") if line.get("output", "").endswith("\n") else line.get("output", "") + "\n"
                      for line in result.get("output", []))
        if not result.get("success"):
            out += str(result.get("result", "failed")) + "\n"
        return bool(result.get("success")), out

    def _find_node(self):
        deadline = time.time() + self._timeout_s
        nodes = []
        while time.time() < deadline:
            nodes = self._rex.remote_nodes
            if nodes:
                time.sleep(0.3)  # let every running editor answer
                nodes = self._rex.remote_nodes
                break
            time.sleep(0.1)
        for node in nodes:
            if node.get("project_name") == PROJECT:
                return node
        return nodes[0] if nodes else None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("code", nargs="?", help="Python code to run in the editor")
    parser.add_argument("-f", "--file", help="run this Python file in the editor instead")
    parser.add_argument("--timeout", type=float, default=5.0, help="seconds to wait for an editor to answer")
    args = parser.parse_args()
    if bool(args.code) == bool(args.file):
        parser.error("give either code or --file")
    code = args.code
    if args.file:
        with open(args.file, encoding="utf-8") as f:
            code = f.read()
    try:
        with Editor(args.timeout) as ue:
            ok, output = ue.run(code)
    except EditorNotFound as e:
        print(e, file=sys.stderr)
        return 2
    print(output, end="", file=sys.stdout if ok else sys.stderr)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
