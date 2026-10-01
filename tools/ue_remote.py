"""Run Python inside a running Unreal editor (Python remote execution) and print what it prints.

Needs the editor open with remote execution on (Config/DefaultEngine.ini,
[/Script/PythonScriptPlugin.PythonScriptPluginSettings] bRemoteExecution=True). Uses the client that ships with
the engine, so nothing to install.

    python tools/ue_remote.py "print(unreal.SystemLibrary.get_engine_version())"
    python tools/ue_remote.py -f tools/ue/pie_start.py

The code runs as a script in the editor's Python (the `unreal` module is imported for you). Exit codes: 0 ok,
1 the code raised, 2 no editor found. Set UE_ROOT if the engine isn't at D:\\UE_5.7.
"""
import argparse
import os
import sys
import time

UE_ROOT = os.environ.get("UE_ROOT", r"D:\UE_5.7")
sys.path.insert(0, os.path.join(UE_ROOT, "Engine", "Plugins", "Experimental", "PythonScriptPlugin", "Content", "Python"))
import remote_execution  # noqa: E402  (ships with the engine)

PROJECT = "SAM3D"


def find_node(rex, timeout_s):
    """The SAM3D editor if several are running, otherwise the first one found."""
    deadline = time.time() + timeout_s
    nodes = []
    while time.time() < deadline:
        nodes = rex.remote_nodes
        if nodes:
            time.sleep(0.3)  # let every running editor answer
            nodes = rex.remote_nodes
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
    code = "import unreal\n" + code

    rex = remote_execution.RemoteExecution(remote_execution.RemoteExecutionConfig())
    rex.start()
    try:
        node = find_node(rex, args.timeout)
        if node is None:
            print("No Unreal editor answered. Is it open, with remote execution on?", file=sys.stderr)
            return 2
        rex.open_command_connection(node["node_id"])
        result = rex.run_command(code, unattended=True, exec_mode=remote_execution.MODE_EXEC_FILE)
    finally:
        rex.stop()
    for line in result.get("output", []):
        text = line.get("output", "")
        print(text, end="" if text.endswith("\n") else "\n")
    if not result.get("success"):
        print(result.get("result", "failed"), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
