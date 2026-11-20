#!/usr/bin/env python3
"""Summarise gcovr JSON coverage for the response-matching code, as Markdown.

Usage: coverage_summary.py coverage.json [output.md]

Prints line and branch coverage for each file under src/ that the matching code lives in, and
per function for the CM_Common functions that frame, match and time out responses, with the
line numbers not yet covered.
"""
import json
import sys

FILES = ["src/CM_Common.cpp", "src/Message.cpp", "src/ReportManipulation.cpp", "src/DeviceReport.cpp"]

# CM_Common functions that take part in matching responses to requests
FUNCTIONS = [
    "SendMsg",
    "AwaitCompletionIfSerialised",
    "ExtractFrame",
    "ResponseMatches",
    "OutstandingMessages",
    "MarkResponseLost",
    "RouteFrame",
    "ProcessRxStream",
    "HandleMessage",
    "HandleResponseDone",
    "HandleUnexpectedChar",
    "HandleTimeoutsAndCleanup",
    "MessageListManager",
]


def pct(hit, total):
    return f"{100.0 * hit / total:.1f}%" if total else "n/a"


def line_ranges(lines):
    """Compress [1,2,3,7,8] to '1-3, 7-8'."""
    out, start, prev = [], None, None
    for n in sorted(lines):
        if start is None:
            start = prev = n
        elif n == prev + 1:
            prev = n
        else:
            out.append(f"{start}-{prev}" if prev != start else f"{start}")
            start = prev = n
    if start is not None:
        out.append(f"{start}-{prev}" if prev != start else f"{start}")
    return ", ".join(out)


def counts(lines):
    lh = sum(1 for l in lines if l.get("count", 0) > 0)
    branches = [b for l in lines for b in l.get("branches", [])]
    bh = sum(1 for b in branches if b.get("count", 0) > 0)
    return lh, len(lines), bh, len(branches)


def main():
    data = json.load(open(sys.argv[1]))
    files = {f["file"].replace("\\", "/"): f for f in data["files"]}
    out = []

    out.append("## Coverage of the response-matching code\n")
    out.append("| File | Lines | Branches |")
    out.append("|---|---|---|")
    for name in FILES:
        f = files.get(name)
        if not f:
            out.append(f"| `{name}` | not measured | |")
            continue
        lh, lt, bh, bt = counts(f["lines"])
        out.append(f"| `{name}` | {lh}/{lt} ({pct(lh, lt)}) | {bh}/{bt} ({pct(bh, bt)}) |")

    cm = files.get("src/CM_Common.cpp")
    if cm:
        # Assign each line to the function that starts most recently before it
        funcs = sorted(cm.get("functions", []), key=lambda fn: fn["lineno"])
        starts = []
        for fn in funcs:
            name = fn.get("demangled_name") or fn.get("name", "")
            starts.append((fn["lineno"], name))

        def owner(line_no):
            current = None
            for start, name in starts:
                if start <= line_no:
                    current = name
                else:
                    break
            return current or ""

        by_func = {}
        for l in cm["lines"]:
            by_func.setdefault(owner(l["line_number"]), []).append(l)

        out.append("\n### CM_Common matching functions\n")
        out.append("| Function | Lines | Branches | Lines not covered |")
        out.append("|---|---|---|---|")
        for short in FUNCTIONS:
            key = f"CM_Common::{short}("
            matched = [name for name in by_func if key in name]
            if not matched:
                out.append(f"| `{short}` | not found | | |")
                continue
            lines = [l for name in matched for l in by_func[name]]
            lh, lt, bh, bt = counts(lines)
            missed = line_ranges([l["line_number"] for l in lines if l.get("count", 0) == 0])
            out.append(f"| `{short}` | {lh}/{lt} ({pct(lh, lt)}) | {bh}/{bt} ({pct(bh, bt)}) | {missed or '-'} |")

    text = "\n".join(out) + "\n"
    if len(sys.argv) > 2:
        open(sys.argv[2], "w").write(text)
    print(text)


if __name__ == "__main__":
    main()
