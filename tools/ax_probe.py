#!/usr/bin/env python3
"""Drive Calculator.app via REA observe-native-ui scenarios and print display state per step."""
import json, subprocess, sys

REA = ["npx", "-y", "rea-agents@6.4.0", "capture-native-ui-scenario",
       "/System/Applications/Calculator.app", "--pid", "12659",
       "--window-id", "2945", "--no-screenshot", "--format", "json"]

EXPR = [0, 0, 1, 0, 0, 0]
RESULT = [0, 0, 1, 0, 1, 0]

def displays(nodes):
    texts = [(n["path"], n["value"]) for n in nodes
             if n["role"] == "AXStaticText" and n.get("value")]
    return texts or "∅"

def run(steps):
    out = subprocess.run(REA + ["--steps", json.dumps(steps)],
                         capture_output=True, text=True)
    try:
        data = json.loads(out.stdout)
    except json.JSONDecodeError:
        print("STDOUT:", out.stdout[:2000]); print("STDERR:", out.stderr[:2000]); sys.exit(1)
    nr = data.get("normalized_result", {})
    if not nr:
        print("ERROR:", json.dumps(data, indent=1)[:2000]); sys.exit(1)
    print(f"  initial: {displays(nr['initial']['nodes'])}")
    for i, s in enumerate(nr.get("steps", [])):
        info = {k: s.get(k) for k in ("kind", "error")}
        st = displays((s.get("after") or {}).get("nodes") or [])
        print(f"  step{i} {info}: {st}")

if __name__ == "__main__":
    run(json.load(open(sys.argv[1])) if len(sys.argv) > 1 else json.load(sys.stdin))
