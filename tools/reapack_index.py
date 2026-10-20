#!/usr/bin/env python3
"""Add or update a release <version> in the ReaPack index (index.xml).

If the version is already in the index, only its changelog is replaced.

Example:
  python tools/reapack_index.py --version 0.1.0 --author me --changelog-file notes.md \
      --source win64 reaper_drop_to_sampler-x64.dll https://.../reaper_drop_to_sampler-x64.dll
"""
import argparse
import datetime
import re
import sys
from xml.sax.saxutils import escape, quoteattr


def cdata(text):
    return "<![CDATA[" + text.replace("]]>", "]]]]><![CDATA[>") + "]]>"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--index", default="index.xml")
    parser.add_argument("--version", required=True, help="e.g. 0.1.0")
    parser.add_argument("--author", required=True)
    parser.add_argument("--changelog-file", required=True)
    parser.add_argument("--source", nargs=3, action="append", required=True,
                        metavar=("PLATFORM", "FILE", "URL"), help="repeat per platform")
    args = parser.parse_args()

    with open(args.changelog_file, encoding="utf-8") as f:
        changelog = f.read().replace("\r\n", "\n").strip()
    if not changelog:
        sys.exit("Changelog is empty: add a description to the release")

    with open(args.index, encoding="utf-8", newline="") as f:
        xml = f.read()
    nl = "\r\n" if "\r\n" in xml else "\n"
    changelog_xml = f"<changelog>{cdata(changelog.replace(chr(10), nl))}</changelog>"

    existing = re.search(r'<version\s+name="%s".*?</version>' % re.escape(args.version), xml, re.S)
    if existing:
        updated = re.sub(r"<changelog>.*?</changelog>", lambda _: changelog_xml, existing.group(0), count=1, flags=re.S)
        if updated == existing.group(0):
            print(f"{args.index}: version {args.version} is up to date")
            return 0
        xml = xml[:existing.start()] + updated + xml[existing.end():]
        print(f"{args.index}: updated changelog of version {args.version}")
    else:
        packages = list(re.finditer(r"<reapack\b[^>]*>", xml))
        if len(packages) != 1:
            sys.exit(f"{args.index}: expected exactly one <reapack> package, found {len(packages)}")
        start = packages[0].end()
        end = xml.index("</reapack>", start)
        meta = xml.find("</metadata>", start, end)
        pos = meta + len("</metadata>") if meta != -1 else start

        time = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        lines = [
            f"      <version name={quoteattr(args.version)} author={quoteattr(args.author)} time={quoteattr(time)}>",
            f"        {changelog_xml}",
        ]
        for platform, file, url in args.source:
            lines.append(f"        <source platform={quoteattr(platform)} file={quoteattr(file)}>{escape(url)}</source>")
        lines.append("      </version>")
        xml = xml[:pos] + nl + nl.join(lines) + xml[pos:]
        print(f"{args.index}: added version {args.version}")

    with open(args.index, "w", encoding="utf-8", newline="") as f:
        f.write(xml)
    return 0


if __name__ == "__main__":
    sys.exit(main())
