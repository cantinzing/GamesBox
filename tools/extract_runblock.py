"""Extract a workflow step's run block into a .ps1 file, verbatim.

Used to dry-run CI PowerShell locally against a synthetic directory tree
before pushing, so that a logic bug cannot hide behind a "green" CI run.

Usage:
    python extract_runblock.py <workflow.yml> <step name> <out.ps1>
"""

import sys
import yaml

def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    wf, step_name, out = sys.argv[1], sys.argv[2], sys.argv[3]
    doc = yaml.safe_load(open(wf, encoding="utf-8"))
    steps = doc["jobs"]["build"]["steps"]
    hit = None
    for s in steps:
        if s.get("name") == step_name:
            hit = s
            break
    if hit is None:
        names = [s.get("name") for s in steps]
        raise SystemExit(f"step not found: {step_name!r}\nknown: {names}")
    body = hit["run"]
    # The runner writes run blocks without a BOM; mirror that exactly.
    with open(out, "wb") as fh:
        fh.write(body.encode("ascii") if body.isascii() else body.encode("utf-8"))
    print(f"wrote {out}: {len(body.splitlines())} lines, ascii={body.isascii()}")
    shells = [s.get("shell") for s in steps if s.get("name") == step_name]
    print(f"shell: {shells[0]}")

if __name__ == "__main__":
    main()
