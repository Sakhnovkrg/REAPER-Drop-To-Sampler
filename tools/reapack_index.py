#!/usr/bin/env python3
"""Add or update a release <version> in the ReaPack index (index.xml).

If the version is already in the index, its changelog and sources are replaced
(author and time are kept), so editing a release also picks up added or removed binaries.

Example:
  python tools/reapack_index.py --version 0.1.0 --author me --changelog-file notes.md \
      --source win64 reaper_drop_to_sampler-x64.dll https://.../reaper_drop_to_sampler-x64.dll
"""
import argparse
import datetime
import re
import sys
from xml.sax.saxutils import escape, quoteattr, unescape

# Platform names ReaPack understands (reapack/source.cpp)
PLATFORMS = {
    "all", "windows", "win32", "win64", "windows-arm64ec",
    "darwin", "darwin32", "darwin64", "darwin-arm64",
    "linux", "linux32", "linux64", "linux-armv7l", "linux-aarch64",
}


def cdata(text):
    return "<![CDATA[" + text.replace("]]>", "]]]]><![CDATA[>") + "]]>"


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--index", default="index.xml")
    parser.add_argument("--version", required=True, help="e.g. 0.1.0")
    parser.add_argument("--author", required=True)
    parser.add_argument("--changelog-file", required=True)
    parser.add_argument("--source", nargs=3, action="append", required=True,
                        metavar=("PLATFORM", "FILE", "URL"), help="repeat per platform")
    args = parser.parse_args()

    for platform, _, _ in args.source:
        if platform not in PLATFORMS:
            sys.exit(f"Unknown ReaPack platform '{platform}', expected one of: {', '.join(sorted(PLATFORMS))}")
    platforms = [platform for platform, _, _ in args.source]
    if len(set(platforms)) != len(platforms):
        sys.exit("Each platform may be given only once")

    with open(args.changelog_file, encoding="utf-8") as f:
        changelog = f.read().replace("\r\n", "\n").strip()
    if not changelog:
        sys.exit("Changelog is empty: add a description to the release")

    with open(args.index, encoding="utf-8", newline="") as f:
        xml = f.read()
    nl = "\r\n" if "\r\n" in xml else "\n"
    changelog_xml = f"<changelog>{cdata(changelog.replace(chr(10), nl))}</changelog>"

    def version_block(author, time):
        lines = [
            f"      <version name={quoteattr(args.version)} author={quoteattr(author)} time={quoteattr(time)}>",
            f"        {changelog_xml}",
        ]
        for platform, file, url in args.source:
            lines.append(f"        <source platform={quoteattr(platform)} file={quoteattr(file)}>{escape(url)}</source>")
        lines.append("      </version>")
        return nl.join(lines)

    existing = re.search(r'[ \t]*<version\s+name="%s".*?</version>' % re.escape(args.version), xml, re.S)
    if existing:
        old = existing.group(0)
        author = re.search(r'\bauthor="([^"]*)"', old)
        time = re.search(r'\btime="([^"]*)"', old)
        # Back to raw text: version_block() escapes again
        attr = lambda m: unescape(m.group(1), {"&quot;": '"'})
        updated = version_block(attr(author) if author else args.author,
                                attr(time) if time else utc_now())
        if updated == old:
            print(f"{args.index}: version {args.version} is up to date")
            return 0
        xml = xml[:existing.start()] + updated + xml[existing.end():]
        print(f"{args.index}: updated version {args.version}")
    else:
        packages = list(re.finditer(r"<reapack\b[^>]*>", xml))
        if len(packages) != 1:
            sys.exit(f"{args.index}: expected exactly one <reapack> package, found {len(packages)}")
        start = packages[0].end()
        end = xml.index("</reapack>", start)
        meta = xml.find("</metadata>", start, end)
        pos = meta + len("</metadata>") if meta != -1 else start

        xml = xml[:pos] + nl + version_block(args.author, utc_now()) + xml[pos:]
        print(f"{args.index}: added version {args.version}")

    with open(args.index, "w", encoding="utf-8", newline="") as f:
        f.write(xml)
    return 0


if __name__ == "__main__":
    sys.exit(main())
