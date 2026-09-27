#!/usr/bin/env python3
"""Print a release's changelog as Markdown: the subject lines of the commits since the previous
release tag (v*), oldest first.

Only main's own commits count (first parent, no merges), so Valve's commits that arrive with an
upstream merge stay out. The first release starts at the SDK commit this repo forked from. The
version bump commits ("Bump the version to ...") are left out.

Usage:
  py tools/changelog.py [TAG]        default: the newest v* tag on HEAD, else everything since it
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# The Valve SDK commit the mod forked from (upstream "Fixes", 2025-05-17).
FORK_BASE = "b8cfb12"

SKIP = re.compile(r"^Bump the version to ")


def git(*args, check=True):
    result = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)
    if check and result.returncode:
        sys.exit(result.stderr.strip())
    return result.stdout.strip() if result.returncode == 0 else None


def previous_tag(rev):
    """The newest v* tag before rev, or None."""
    return git("describe", "--tags", "--abbrev=0", "--match", "v*", f"{rev}^", check=False)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("tag", nargs="?", help="the release tag (default: the v* tag on HEAD, else HEAD)")
    args = ap.parse_args()

    rev = args.tag or git("describe", "--tags", "--exact-match", "--match", "v*", "HEAD", check=False) or "HEAD"
    base = previous_tag(rev) or FORK_BASE

    subjects = git("log", "--first-parent", "--no-merges", "--reverse", "--format=%s", f"{base}..{rev}")
    lines = [s for s in subjects.splitlines() if s and not SKIP.match(s)]

    date = git("log", "-1", "--format=%cs", rev)
    title = rev if rev != "HEAD" else "Unreleased"
    since = f"since {base}" if base != FORK_BASE else "since the fork from Source SDK 2013"
    print(f"## {title} ({date})\n")
    print(f"{len(lines)} changes {since}.\n")
    for line in lines:
        print(f"- {line}")


if __name__ == "__main__":
    main()
