#!/usr/bin/env python3
"""Fail-closed check for sensitive files and credentials in the Git index.

This deliberately has no third-party dependencies so it works on macOS and
Linux before the deployment environment has been built.  It checks the staged
index, not the working tree: a file that is ignored locally is still rejected
if somebody force-adds it.
"""

from __future__ import print_function

import re
import os
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


def _is_within(path, parent):
    try:
        return os.path.commonpath([path, parent]) == parent
    except ValueError:
        return False


def external_denylist_path(root):
    """Return the configured machine-local denylist, if one exists."""
    configured = os.environ.get("SPHERE_PRIVATE_DENYLIST")
    if configured:
        path = os.path.realpath(os.path.abspath(os.path.expanduser(configured)))
        if not os.path.isfile(path):
            raise ValueError("configured external denylist does not exist")
    else:
        info_dir = git(root, "rev-parse", "--git-path", "info").decode().strip()
        if not os.path.isabs(info_dir):
            info_dir = os.path.join(root, info_dir)
        path = os.path.realpath(os.path.join(info_dir, "private-denylist"))
        if not os.path.isfile(path):
            return None

    root_real = os.path.realpath(root)
    git_dir = git(root, "rev-parse", "--git-dir").decode().strip()
    if not os.path.isabs(git_dir):
        git_dir = os.path.join(root, git_dir)
    git_dir = os.path.realpath(git_dir)
    if _is_within(path, root_real) and not _is_within(path, git_dir):
        raise ValueError("external denylist must be outside the worktree")
    return path


def compile_denylist(lines):
    """Compile non-empty denylist lines as case-insensitive patterns.

    Bare lines are regular expressions.  Use ``literal:...`` for an exact
    literal and ``regex:...`` when the intent should be explicit.
    """
    patterns = []
    for raw_line in lines:
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        mode, separator, value = line.partition(":")
        if separator and mode.lower() in ("literal", "regex"):
            line = value.strip()
            if mode.lower() == "literal":
                line = re.escape(line)
        if not line:
            raise ValueError("external denylist contains an empty pattern")
        try:
            patterns.append(re.compile(line, re.IGNORECASE))
        except re.error:
            raise ValueError("external denylist contains an invalid regex")
    return patterns


def load_external_denylist(root):
    path = external_denylist_path(root)
    if path is None:
        return []
    try:
        with open(path, "r", encoding="utf-8") as denylist:
            return compile_denylist(denylist)
    except (OSError, UnicodeError):
        raise ValueError("cannot read external denylist")


def staged_added_lines(root):
    """Yield (path, line number, line) for added text lines."""
    raw = git(
        root,
        "diff",
        "--cached",
        "--unified=0",
        "--no-color",
        "--no-ext-diff",
        "--diff-filter=ACMR",
        "--",
    )
    current_path = None
    new_line_number = None
    for raw_line in raw.decode("utf-8", "replace").splitlines():
        if raw_line.startswith("+++ b/"):
            current_path = raw_line[6:]
            continue
        if raw_line.startswith("+++ /dev/null"):
            current_path = None
            continue
        if raw_line.startswith("@@ "):
            match = re.search(r" \+(\d+)(?:,\d+)? ", raw_line)
            new_line_number = int(match.group(1)) if match else None
            continue
        if current_path is None or new_line_number is None:
            continue
        if raw_line.startswith("+"):
            yield current_path, new_line_number, raw_line[1:]
            new_line_number += 1
        elif not raw_line.startswith("\\"):
            new_line_number += 1


def scan_external_content(path, text, patterns):
    findings = []
    for line_number, line in enumerate(text.splitlines(), 1):
        if any(pattern.search(line) for pattern in patterns):
            # Never echo a private identifier or the rest of its line.
            findings.append(
                (line_number, "external denylist", "[redacted denylist match]")
            )
    return findings


def scan_external_staged(root, patterns, all_files):
    violations = []
    if not patterns:
        return violations

    if all_files:
        paths = staged_paths(root, all_files=True)
        for path in paths:
            blob = staged_blob(root, path)
            for line_number, label, excerpt in scan_external_content(
                path, blob.decode("utf-8", "replace"), patterns
            ):
                violations.append((path, label, (line_number, excerpt)))
        return violations

    for path, line_number, line in staged_added_lines(root):
        if any(pattern.search(line) for pattern in patterns):
            violations.append(
                (path, "external denylist", (line_number, "[redacted denylist match]"))
            )
    return violations


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
    args = sys.argv[1:]
    all_files = "--all" in args
    commit_msg_path = None
    if "--commit-msg" in args:
        index = args.index("--commit-msg")
        if index + 1 >= len(args):
            print("ERROR: --commit-msg requires a message-file path", file=sys.stderr)
            return 1
        commit_msg_path = args[index + 1]
    try:
        root = git(".", "rev-parse", "--show-toplevel").decode().strip()
        paths = staged_paths(root, all_files=all_files)
        denylist_patterns = load_external_denylist(root)
    except (OSError, subprocess.CalledProcessError) as exc:
        print("ERROR: cannot inspect the Git index (fail-closed): %s" % exc, file=sys.stderr)
        return 1
    except ValueError as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
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

    try:
        violations.extend(scan_external_staged(root, denylist_patterns, all_files))
    except (OSError, subprocess.CalledProcessError) as exc:
        print("ERROR: cannot inspect staged additions (fail-closed): %s" % exc, file=sys.stderr)
        return 1

    if commit_msg_path:
        try:
            with open(commit_msg_path, "rb") as message_file:
                message = message_file.read()
        except OSError as exc:
            print("ERROR: cannot read commit message (fail-closed): %s" % exc, file=sys.stderr)
            return 1
        for line_number, label, excerpt in scan_content("COMMIT_MESSAGE", message):
            violations.append(("COMMIT_MESSAGE", label, (line_number, excerpt)))
        for line_number, label, excerpt in scan_external_content(
            "COMMIT_MESSAGE", message.decode("utf-8", "replace"), denylist_patterns
        ):
            violations.append(("COMMIT_MESSAGE", label, (line_number, excerpt)))

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
