#!/usr/bin/env python3
"""Fail-closed check for sensitive files and credentials in the Git index.

This deliberately has no third-party dependencies so it works on macOS and
Linux before the deployment environment has been built.  It checks the staged
index, not the working tree: a file that is ignored locally is still rejected
if somebody force-adds it.
"""

from __future__ import print_function

import re
import subprocess
import sys


SENSITIVE_DIRS = {
    "accounts",
    "muls",
    "save",
    "scripts",
}

SENSITIVE_NAMES = {
    ".env",
    ".netrc",
    "sphere.ini",
    "sphereacct.scp",
    "sphereaccu.scp",
    "spherechars.scp",
    "spheredata.scp",
    "spherestatics.scp",
    "sphereworld.scp",
}

SENSITIVE_SUFFIXES = (
    ".7z",
    ".bak",
    ".core",
    ".dmp",
    ".key",
    ".log",
    ".mul",
    ".pem",
    ".pfx",
    ".p12",
    ".scp",
    ".tar",
    ".tgz",
    ".zip",
)

SECRET_PATTERNS = (
    ("GitLab access token", re.compile(r"\bglpat-[A-Za-z0-9_-]{20,}\b")),
    ("GitHub access token", re.compile(r"\b(?:gh[pousr]|github_pat)_[A-Za-z0-9_\-]{20,}\b")),
    ("AWS access key", re.compile(r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b")),
    ("OpenAI-style API key", re.compile(r"\bsk-(?:proj-)?[A-Za-z0-9_-]{20,}\b")),
    ("private key", re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----")),
    (
        "credential assignment",
        re.compile(
            r"(?i)\b(?:password|passwd|secret|token|api[_-]?key|access[_-]?key)"
            r"\b\s*[:=]\s*(?:\"[^\"\r\n]{12,}\"|'[^'\r\n]{12,}'|[A-Za-z0-9_+/=-]{16,})"
        ),
    ),
)


def git(root, *args):
    return subprocess.check_output(
        ["git"] + list(args), cwd=root, stderr=subprocess.STDOUT
    )


def staged_paths(root, all_files=False):
    if all_files:
        raw = git(root, "ls-files", "-z")
    else:
        raw = git(root, "diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR")
    return [item.decode("utf-8", "surrogateescape") for item in raw.split(b"\0") if item]


def staged_blob(root, path):
    raw = git(root, "ls-files", "-s", "--", path)
    for entry in raw.splitlines():
        metadata, separator, indexed_path = entry.partition(b"\t")
        if not separator or indexed_path.decode("utf-8", "surrogateescape") != path:
            continue
        fields = metadata.split()
        if len(fields) != 3 or fields[2] != b"0":
            continue
        if fields[0] == b"160000":
            return b""  # submodule gitlink; its own hook scans its tree
        object_id = fields[1].decode("ascii")
        if git(root, "cat-file", "-t", object_id).strip() != b"blob":
            return b""  # submodule or another non-file entry
        return git(root, "cat-file", "blob", object_id)
    return b""


def path_reason(path):
    lowered = path.lower()
    parts = lowered.split("/")
    name = parts[-1]
    if any(part in SENSITIVE_DIRS or part.endswith("-scripts") for part in parts):
        return "private runtime/script directory"
    if name in SENSITIVE_NAMES:
        return "private runtime/config file"
    if name.endswith(SENSITIVE_SUFFIXES):
        return "runtime data, archive, log, key, or script file"
    if name.startswith(("core.", "vgcore.")) or re.match(r"^sphereb.*\.scp$", name):
        return "runtime dump or world backup"
    return None


def scan_content(path, blob):
    # The scanner itself contains detection expressions; do not report its
    # examples as a credential when the guard is being changed.
    if path.endswith("tools/check_secrets.py"):
        return []
    text = blob.decode("utf-8", "replace")
    findings = []
    for line_number, line in enumerate(text.splitlines(), 1):
        for label, pattern in SECRET_PATTERNS:
            match = pattern.search(line)
            if match:
                redacted = line[: match.start()] + "***" + line[match.end() :]
                findings.append((line_number, label, redacted[:240]))
    return findings


def main():
    all_files = "--all" in sys.argv[1:]
    try:
        root = git(".", "rev-parse", "--show-toplevel").decode().strip()
        paths = staged_paths(root, all_files=all_files)
    except (OSError, subprocess.CalledProcessError) as exc:
        print("ERROR: cannot inspect the Git index (fail-closed): %s" % exc, file=sys.stderr)
        return 1

    violations = []
    for path in paths:
        reason = path_reason(path)
        if reason:
            violations.append((path, reason, None))
            continue
        try:
            blob = staged_blob(root, path)
        except subprocess.CalledProcessError as exc:
            print("ERROR: cannot read staged file %s (fail-closed): %s" % (path, exc), file=sys.stderr)
            return 1
        for line_number, label, excerpt in scan_content(path, blob):
            violations.append((path, label, (line_number, excerpt)))

    if violations:
        print("ERROR: commit blocked by the public/private secret boundary.", file=sys.stderr)
        print("Remove the sensitive file/content from the index and retry.", file=sys.stderr)
        for path, reason, detail in violations:
            if detail is None:
                print("  - %s: %s" % (path, reason), file=sys.stderr)
            else:
                line_number, excerpt = detail
                print("  - %s:%d: %s (%s)" % (path, line_number, excerpt, reason), file=sys.stderr)
        return 1

    print("secret check: staged paths and content are clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
