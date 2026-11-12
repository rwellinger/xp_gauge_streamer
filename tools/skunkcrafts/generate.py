#!/usr/bin/env python3
"""
SkunkCrafts Updater control-file generator for xp_gauge_streamer.

Walks a staged plugin tree and emits the control files the updater compares
against:

    skunkcrafts_updater_whitelist.txt   <relative_path>|<crc32 unsigned decimal>
    skunkcrafts_updater_sizeslist.txt   <relative_path>|<size in bytes>
    skunkcrafts_updater_oncelist.txt    empty -- see below
    skunkcrafts_updater.cfg             rendered from the .cfg.template (version filled in)

The oncelist stays empty: the user's settings.cfg lives under
<X-Plane>/Output/xp_gauge_streamer/, outside the plugin tree, so nothing inside
it is user-owned. config/settings.cfg in the tree is only the shipped default
and may be overwritten. Anything not in the whitelist is left untouched.

Usage:
    python3 tools/skunkcrafts/generate.py --tree dist/xp_gauge_streamer --version 1.2.0

Run it against the release tree about to be published; the release workflow
then pushes that tree to the `release` branch the cfg `module` URL points at.
"""
import argparse
import fnmatch
import os
import zlib
from pathlib import Path

# Paths (relative to the plugin root) the updater must not manage.
IGNORE_GLOBS = [
    ".DS_Store",
    "**/.DS_Store",
    "skunkcrafts_updater_*.txt",
    "skunkcrafts_updater.cfg",
]


def crc32(path: Path) -> int:
    checksum = 0
    with path.open("rb") as file:
        while chunk := file.read(65536):
            checksum = zlib.crc32(chunk, checksum)
    return checksum & 0xFFFFFFFF


def is_ignored(relative_path: str) -> bool:
    return any(fnmatch.fnmatch(relative_path, pattern) for pattern in IGNORE_GLOBS)


def tracked_files(tree: Path) -> list[Path]:
    files = []
    for directory, _subdirectories, names in os.walk(tree):
        for name in names:
            path = Path(directory) / name
            if not is_ignored(path.relative_to(tree).as_posix()):
                files.append(path)
    return sorted(files, key=lambda path: path.relative_to(tree).as_posix())


def write_lines(path: Path, lines: list[str]) -> None:
    path.write_text("".join(f"{line}\n" for line in lines))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tree", required=True, help="staged plugin root directory")
    parser.add_argument("--version", required=True, help="version string for the cfg")
    args = parser.parse_args()

    tree = Path(args.tree).resolve()
    if not tree.is_dir():
        raise SystemExit(f"not a directory: {tree}")

    template = (Path(__file__).parent / "skunkcrafts_updater.cfg.template").read_text()
    files = tracked_files(tree)

    def relative(path: Path) -> str:
        return path.relative_to(tree).as_posix()

    write_lines(tree / "skunkcrafts_updater_whitelist.txt",
                [f"{relative(path)}|{crc32(path)}" for path in files])
    write_lines(tree / "skunkcrafts_updater_sizeslist.txt",
                [f"{relative(path)}|{path.stat().st_size}" for path in files])
    write_lines(tree / "skunkcrafts_updater_oncelist.txt", [])
    (tree / "skunkcrafts_updater.cfg").write_text(template.replace("@VERSION@", args.version))

    print(f"tracked {len(files)} files; wrote control files into {tree}")


if __name__ == "__main__":
    main()
