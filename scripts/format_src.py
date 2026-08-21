#!/usr/bin/env python3
"""
format_src.py - Format the EZ C++ source tree with clang-format.

Uses the project's .clang-format (at the repo root) so every file gets the
same style, applied consistently instead of by hand. Safe by default: it
only REPORTS what would change until you pass --write.

Usage:
    python3 scripts/format_src.py                 # check mode (default):
                                                    # lists files that are not
                                                    # correctly formatted,
                                                    # changes nothing, exits 1
                                                    # if any need reformatting
    python3 scripts/format_src.py --diff           # check mode + print a
                                                    # unified diff per file
    python3 scripts/format_src.py --write          # actually reformat files
                                                    # in place
    python3 scripts/format_src.py --write path/to/file.cpp path/to/dir
                                                    # limit to specific
                                                    # files/directories

Requires clang-format on PATH (tested with clang-format 18; any clang-format
>= 11 that understands this repo's .clang-format options should work).
"""

import argparse
import concurrent.futures
import difflib
import shutil
import subprocess
import sys
from pathlib import Path

SOURCE_EXTENSIONS = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".hxx"}

# Directories under the repo root that should never be touched, even if
# they happen to contain files with a matching extension (build output,
# vendored/third-party code, etc.). Extend this if the repo grows one.
EXCLUDED_DIR_NAMES = {"build", ".git", "dlls", "lib"}


def safe_rel(path: Path, repo_root: Path) -> Path:
    """path.relative_to(repo_root), falling back to the absolute path for
    anything outside repo_root (e.g. --paths pointed at an external directory)."""
    try:
        return path.relative_to(repo_root)
    except ValueError:
        return path


def find_repo_root(start: Path) -> Path:
    """Walk up from `start` looking for the .clang-format that anchors the repo."""
    for candidate in (start, *start.parents):
        if (candidate / ".clang-format").is_file():
            return candidate
    raise SystemExit(
        "Could not find a .clang-format file above {}. "
        "Run this from inside the EZ-language checkout, or pass explicit "
        "paths.".format(start)
    )


def discover_files(paths: list[Path], repo_root: Path) -> list[Path]:
    """Expand the given paths (files or directories) into a sorted, deduped
    list of source files, skipping anything under an excluded directory."""
    files: set[Path] = set()

    def is_excluded(p: Path) -> bool:
        # A path outside repo_root (e.g. --paths given an external directory
        # for testing) has no meaningful relation to EXCLUDED_DIR_NAMES;
        # just check its own components rather than failing on relative_to().
        try:
            parts = p.relative_to(repo_root).parts
        except ValueError:
            parts = p.parts
        return any(part in EXCLUDED_DIR_NAMES for part in parts)

    for p in paths:
        p = p.resolve()
        if p.is_file():
            if p.suffix in SOURCE_EXTENSIONS and not is_excluded(p):
                files.add(p)
            elif p.suffix not in SOURCE_EXTENSIONS:
                print(f"warning: skipping {p} (not a recognized C/C++ extension)", file=sys.stderr)
        elif p.is_dir():
            for ext in SOURCE_EXTENSIONS:
                for f in p.rglob(f"*{ext}"):
                    if not is_excluded(f):
                        files.add(f)
        else:
            print(f"warning: path does not exist, skipping: {p}", file=sys.stderr)

    return sorted(files)


def formatted_text(clang_format: str, path: Path, repo_root: Path) -> str:
    """Run clang-format on `path` and return the formatted text, without
    touching the file on disk. -style=file walks up from the file's own
    directory for .clang-format, which is what we want."""
    result = subprocess.run(
        [clang_format, f"-style=file:{repo_root / '.clang-format'}", "--", str(path)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"clang-format failed on {path} (exit {result.returncode}):\n{result.stderr.strip()}"
        )
    return result.stdout


def process_file(clang_format: str, path: Path, repo_root: Path, write: bool, show_diff: bool):
    """Returns (path, changed: bool, diff_text: str | None, error: str | None)."""
    try:
        original = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as e:
        return path, False, None, f"not valid UTF-8, skipped ({e})"

    try:
        new_text = formatted_text(clang_format, path, repo_root)
    except RuntimeError as e:
        return path, False, None, str(e)

    changed = new_text != original
    diff_text = None
    if changed and show_diff:
        rel = safe_rel(path, repo_root)
        diff_text = "".join(
            difflib.unified_diff(
                original.splitlines(keepends=True),
                new_text.splitlines(keepends=True),
                fromfile=f"a/{rel}",
                tofile=f"b/{rel}",
            )
        )

    if changed and write:
        path.write_text(new_text, encoding="utf-8")

    return path, changed, diff_text, None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "paths",
        nargs="*",
        default=["src"],
        help="Files or directories to format (default: src/)",
    )
    parser.add_argument("--write", action="store_true", help="Reformat files in place (default: check only)")
    parser.add_argument("--diff", action="store_true", help="Print a unified diff for each file that would change")
    parser.add_argument("--jobs", type=int, default=None, help="Parallel workers (default: CPU count)")
    parser.add_argument("--clang-format", default=None, help="Path to the clang-format binary (default: search PATH)")
    args = parser.parse_args()

    clang_format = args.clang_format or shutil.which("clang-format")
    if not clang_format:
        print("error: clang-format not found on PATH. Install it (e.g. `apt install clang-format` "
              "or `brew install clang-format`) and re-run.", file=sys.stderr)
        return 2

    script_dir = Path(__file__).resolve().parent
    repo_root = find_repo_root(script_dir)

    input_paths = [Path(p) if Path(p).is_absolute() else repo_root / p for p in args.paths]
    files = discover_files(input_paths, repo_root)
    if not files:
        print("No source files found to check.", file=sys.stderr)
        return 0

    mode = "Formatting" if args.write else "Checking"
    print(f"{mode} {len(files)} file(s) under {repo_root} with {Path(clang_format).name}...")

    changed_files: list[Path] = []
    errors: list[tuple[Path, str]] = []

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {
            pool.submit(process_file, clang_format, f, repo_root, args.write, args.diff): f
            for f in files
        }
        for future in concurrent.futures.as_completed(futures):
            path, changed, diff_text, error = future.result()
            rel = safe_rel(path, repo_root)
            if error:
                errors.append((path, error))
                continue
            if changed:
                changed_files.append(path)
                verb = "reformatted" if args.write else "would reformat"
                print(f"  {verb}: {rel}")
                if diff_text:
                    print(diff_text)

    print()
    if errors:
        print(f"{len(errors)} file(s) had errors:", file=sys.stderr)
        for path, error in errors:
            print(f"  {safe_rel(path, repo_root)}: {error}", file=sys.stderr)

    if args.write:
        print(f"Done. Reformatted {len(changed_files)}/{len(files)} file(s).")
        return 1 if errors else 0
    else:
        if changed_files:
            print(f"{len(changed_files)}/{len(files)} file(s) are not formatted. "
                  f"Re-run with --write to fix them (add --diff first to preview).")
        else:
            print(f"All {len(files)} file(s) are already formatted.")
        # Non-zero on pending changes, like `black --check` / `gofmt -l`.
        return 1 if (changed_files or errors) else 0


if __name__ == "__main__":
    sys.exit(main())
