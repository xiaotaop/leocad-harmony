#!/usr/bin/env python3
"""Fetch the official LDraw parts library into a local cache.

The first release ships a curated subset of the official library. The full
`complete.zip` (~140 MB) is large and must never enter the repository, so it is
cached outside the working tree and only the generated subset is committed.

Usage:
    scripts/library/fetch_ldraw.py                # download or reuse cache
    scripts/library/fetch_ldraw.py --offline      # fail if cache is missing
    scripts/library/fetch_ldraw.py --cache DIR    # override cache location

The script prints a JSON document describing the cache so that
build_library.py and the manifest generator can record provenance.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

SOURCE_URL = "https://library.ldraw.org/library/updates/complete.zip"
DEFAULT_CACHE = Path.home() / ".cache" / "leocad-harmony" / "ldraw"

# library.ldraw.org returns 403 for the default urllib User-Agent, so the
# project identifies itself explicitly on every request.
USER_AGENT = ("leocad-harmony-library-fetch/1.0 "
              "(+https://github.com/xiaotaop/leocad-harmony)")

# The archive is only re-downloaded when the remote ETag/Last-Modified changes.
META_NAME = "cache-meta.json"
ARCHIVE_NAME = "complete.zip"


def default_cache_dir() -> Path:
    env = os.environ.get("LEOCAD_LDRAW_CACHE")
    return Path(env) if env else DEFAULT_CACHE


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def remote_headers(url: str) -> dict:
    """Read validator headers without downloading the whole archive.

    library.ldraw.org answers HEAD with 403, so a one-byte ranged GET is used
    instead. The body is never read; only the headers are inspected.
    """
    request = urllib.request.Request(
        url, headers={"Range": "bytes=0-0", "User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            content_range = response.headers.get("Content-Range", "")
            # "bytes 0-0/146853687" -> full size is the part after '/'.
            size = (content_range.rsplit("/", 1)[-1]
                    if "/" in content_range else "")
            return {
                "etag": response.headers.get("ETag", ""),
                "last_modified": response.headers.get("Last-Modified", ""),
                "content_length": size or response.headers.get("Content-Length", ""),
            }
    except (urllib.error.HTTPError, urllib.error.URLError) as error:
        raise SystemExit(f"cannot reach {url}: {error}") from error


def download(url: str, destination: Path, content_length: str) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_suffix(".part")
    expected = int(content_length) if content_length.isdigit() else 0
    print(f"downloading {url}", file=sys.stderr)
    print(f"  -> {destination} ({expected // (1024 * 1024)} MB)", file=sys.stderr)

    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=120) as response, \
            partial.open("wb") as handle:
        done = 0
        while True:
            chunk = response.read(1024 * 256)
            if not chunk:
                break
            handle.write(chunk)
            done += len(chunk)
            if expected and done % (1024 * 1024 * 16) < len(chunk):
                percent = done * 100 // expected
                print(f"\r  {percent:3d}%  {done // (1024 * 1024)} MB",
                      end="", file=sys.stderr, flush=True)
    print("", file=sys.stderr)

    if expected and partial.stat().st_size != expected:
        partial.unlink(missing_ok=True)
        raise SystemExit("download truncated: size mismatch")

    partial.replace(destination)


def read_library_release(archive: Path) -> str:
    """Return the 'UPDATE <date>' token from LDConfig.ldr inside the archive.

    This is the only reliable version marker the parts library carries; the
    per-file `!LDRAW_ORG` headers carry per-part update revisions instead.
    """
    import zipfile

    with zipfile.ZipFile(archive) as bundle:
        for name in bundle.namelist():
            if name.lower().endswith("ldconfig.ldr"):
                header = bundle.read(name)[:4096].decode("utf-8", "replace")
                match = re.search(r"!LDRAW_ORG\s+Configuration\s+UPDATE\s+(\S+)",
                                  header)
                if match:
                    return f"UPDATE {match.group(1)}"
                return "unknown"
    return "missing"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=default_cache_dir())
    parser.add_argument("--offline", action="store_true",
                        help="never touch the network; require an existing cache")
    parser.add_argument("--force", action="store_true",
                        help="re-download even when the cache looks current")
    args = parser.parse_args()

    cache_dir = args.cache.expanduser().resolve()
    cache_dir.mkdir(parents=True, exist_ok=True)
    archive = cache_dir / ARCHIVE_NAME
    meta_path = cache_dir / META_NAME

    meta: dict = {}
    if meta_path.exists():
        try:
            meta = json.loads(meta_path.read_text("utf-8"))
        except (OSError, json.JSONDecodeError):
            meta = {}

    stale = args.force or not archive.exists()
    headers: dict = {}
    if not args.offline:
        headers = remote_headers(SOURCE_URL)
        if archive.exists() and not args.force:
            # Reuse the cache unless the upstream archive changed.
            if (headers.get("etag") and meta.get("etag")
                    and headers["etag"] != meta["etag"]):
                stale = True
            elif (headers.get("last_modified")
                  and meta.get("last_modified")
                  and headers["last_modified"] != meta["last_modified"]):
                stale = True
    elif stale:
        raise SystemExit(
            f"--offline requested but no cached archive at {archive}")

    if stale:
        if not headers:
            raise SystemExit("cannot download while --offline is set")
        download(SOURCE_URL, archive, headers.get("content_length", ""))
        meta = {
            "url": SOURCE_URL,
            "etag": headers.get("etag", ""),
            "last_modified": headers.get("last_modified", ""),
            "content_length": headers.get("content_length", ""),
        }

    digest = sha256_of(archive)
    # A cached archive whose hash does not match the record is replaced.
    if meta.get("sha256") and meta["sha256"] != digest and not args.offline:
        print("cache hash drift: re-downloading", file=sys.stderr)
        headers = headers or remote_headers(SOURCE_URL)
        download(SOURCE_URL, archive, headers.get("content_length", ""))
        digest = sha256_of(archive)
        meta.update({
            "url": SOURCE_URL,
            "etag": headers.get("etag", ""),
            "last_modified": headers.get("last_modified", ""),
            "content_length": headers.get("content_length", ""),
        })

    meta["sha256"] = digest
    meta["bytes"] = archive.stat().st_size
    meta["library_release"] = read_library_release(archive)
    meta["archive"] = str(archive)
    meta_path.write_text(json.dumps(meta, indent=2) + "\n", "utf-8")

    json.dump(meta, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
