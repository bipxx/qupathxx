"""Snapshot official format indexes and extract their complete factual census.

Only two explicitly selected HTML indexes are fetched. No sample, image, reader
implementation, rating or license classification is downloaded or inferred.
Reparse retained snapshots offline by omitting --fetch.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timezone
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import sys
from urllib.parse import urljoin, urlsplit
from urllib.request import Request, urlopen


SOURCES = (
    ("bio-formats", "https://bio-formats.readthedocs.io/en/stable/supported-formats.html", "bio-formats-supported-formats.html"),
    ("openslide", "https://openslide.org/formats/", "openslide-formats.html"),
)
MAX_HTML_BYTES = 8 * 1024 * 1024
VOID = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr"}
BLOCK = {"p", "div", "br", "dt", "dd", "li", "h1", "h2", "h3", "td", "th"}


def normalized(value: str) -> str:
    return " ".join(value.split())


@dataclass
class Node:
    tag: str
    attributes: dict[str, str | None] = field(default_factory=dict)
    children: list[Node | str] = field(default_factory=list)

    def walk(self, tag: str | None = None):
        if tag is None or self.tag == tag:
            yield self
        for child in self.children:
            if isinstance(child, Node):
                yield from child.walk(tag)

    def leaves(self):
        if self.tag in BLOCK:
            yield " "
        for child in self.children:
            if isinstance(child, Node):
                yield from child.leaves()
            else:
                yield child
        if self.tag in BLOCK:
            yield " "

    def text(self) -> str:
        return normalized("".join(self.leaves()))


class Document(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.root = Node("document")
        self.stack = [self.root]

    def handle_starttag(self, tag, attrs):
        # HTML permits omitted closing tags in these lists/tables. HTMLParser
        # tokenizes them without browser-style repair, so close only the
        # specific optional-end-tag elements used by the official indexes.
        optional = {"dt": ({"dt", "dd"}, "dl"), "dd": ({"dt", "dd"}, "dl"),
                    "tr": ({"tr"}, "table"), "td": ({"td", "th"}, "tr"),
                    "th": ({"td", "th"}, "tr")}
        if tag in optional:
            close, boundary = optional[tag]
            for index in range(len(self.stack) - 1, 0, -1):
                if self.stack[index].tag == boundary:
                    break
                if self.stack[index].tag in close:
                    del self.stack[index:]
                    break
        node = Node(tag, dict(attrs))
        self.stack[-1].children.append(node)
        if tag not in VOID:
            self.stack.append(node)

    def handle_startendtag(self, tag, attrs):
        self.stack[-1].children.append(Node(tag, dict(attrs)))

    def handle_endtag(self, tag):
        for index in range(len(self.stack) - 1, 0, -1):
            if self.stack[index].tag == tag:
                del self.stack[index:]
                return

    def handle_data(self, data):
        self.stack[-1].children.append(data)


def parse_html(raw: bytes) -> Node:
    document = Document()
    document.feed(raw.decode("utf-8"))
    document.close()
    return document.root


def extension_aliases(value: str) -> list[str]:
    text = normalized(value)
    if text.casefold() in {"", "none", "(none)", "n/a", "no extension", "no extensions"}:
        return []
    aliases = [normalized(part) for part in text.split(",")]
    # The UltraVIEW row lists numbered suffixes followed by literal "etc.".
    # Retain that open-ended source notation separately, never invent aliases.
    if aliases and aliases[-1].casefold() == "etc.":
        aliases.pop()
    if any(not re.fullmatch(r"\.[^\s,]+", alias) for alias in aliases):
        raise ValueError(f"Unrecognized extension-alias cell (retained source needs review): {text!r}")
    return aliases


def official_link(base: str, link: str, family: str) -> str:
    result = urljoin(base, link)
    parts = urlsplit(result)
    required_host = "bio-formats.readthedocs.io" if family == "bio-formats" else "openslide.org"
    if parts.scheme != "https" or parts.netloc != required_host or "/formats/" not in parts.path:
        raise ValueError(f"Unexpected per-format documentation URL: {result}")
    return result


def title(root: Node) -> str:
    return next((node.text() for node in root.walk("title")), "")


def bio_formats(root: Node, base: str):
    matching = []
    for table in root.walk("table"):
        rows = list(table.walk("tr"))
        if not rows:
            continue
        header = [node.text().casefold() for node in rows[0].children if isinstance(node, Node) and node.tag in {"th", "td"}]
        if header[:2] == ["format", "extensions"]:
            matching.append((table, rows))
    if len(matching) != 1:
        raise ValueError(f"Expected one Bio-Formats Format/Extensions table, found {len(matching)}")
    _, table_rows = matching[0]
    result = []
    data_rows = []
    for row in table_rows:
        cells = [node for node in row.children if isinstance(node, Node) and node.tag == "td"]
        if not cells:
            continue
        if len(cells) < 2:
            raise ValueError("Bio-Formats data row has fewer than two cells")
        data_rows.append(cells)
    for position, cells in enumerate(data_rows, 1):
        links = [node for node in cells[0].walk("a") if node.attributes.get("href") and not node.attributes["href"].startswith("#")]
        if len(links) != 1:
            raise ValueError(f"Bio-Formats format row {position} does not have exactly one documentation link")
        name = links[0].text()
        extensions_text = cells[1].text()
        if not name:
            raise ValueError(f"Bio-Formats format row {position} has no name")
        record = {
            "family": "bio-formats", "name": name,
            "extensions": extension_aliases(extensions_text),
            "extensions_text": extensions_text,
            "url": official_link(base, links[0].attributes["href"], "bio-formats"),
            "source_row": position,
        }
        if extensions_text.split(",")[-1].strip().casefold() == "etc.":
            record["extension_notation"] = "etc."
        result.append(record)
    if not result:
        raise ValueError("Bio-Formats index yielded no format rows")
    return result, {"source_data_rows": len(data_rows), "emitted_rows": len(result), "format_tables": 1}


def openslide(root: Node, base: str):
    nodes = list(root.walk())
    headings = [index for index, node in enumerate(nodes) if node.tag == "h2"]
    result = []
    for position, start in enumerate(headings, 1):
        stop = headings[position] if position < len(headings) else len(nodes)
        name = nodes[start].text()
        fields = {}
        key = None
        for node in nodes[start + 1:stop]:
            if node.tag == "dt":
                key = node.text().casefold()
            elif node.tag == "dd" and key:
                if key in fields:
                    raise ValueError(f"OpenSlide section {name!r} repeats field {key!r}")
                fields[key] = node
                key = None
        if set(fields) != {"file extensions", "openslide vendor backend", "more info"}:
            raise ValueError(f"OpenSlide section {name!r} has unexpected/missing fields: {sorted(fields)}")
        extension_text = fields["file extensions"].text()
        aliases = extension_aliases(extension_text)
        code_aliases = [node.text() for node in fields["file extensions"].walk("code")]
        if aliases != code_aliases:
            raise ValueError(f"OpenSlide extension text/code coverage differs in {name!r}")
        links = [node for node in fields["more info"].walk("a") if node.attributes.get("href")]
        backend = fields["openslide vendor backend"].text()
        if not name or not backend or len(links) != 1:
            raise ValueError(f"OpenSlide section {name!r} lacks a name/backend/unique documentation link")
        result.append({
            "family": "openslide", "name": name, "extensions": aliases,
            "extensions_text": extension_text,
            "url": official_link(base, links[0].attributes["href"], "openslide"),
            "source_row": position, "upstream_backend": backend,
        })
    if not result:
        raise ValueError("OpenSlide index yielded no format sections")
    return result, {"source_format_sections": len(headings), "emitted_rows": len(result)}


def write_json(path: Path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True) + "\n", encoding="utf-8")


def snapshot_sources(output: Path, fetch: bool):
    manifest_path = output / "source-manifest.json"
    existing = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {"schema": "qpx.upstream-format-sources/1", "sources": []}
    if existing.get("schema") != "qpx.upstream-format-sources/1":
        raise ValueError("Unsupported upstream source-manifest schema")
    sources = {record["family"]: record for record in existing["sources"]}
    fetched = []
    expected_families = {family for family, _, _ in SOURCES}
    if not set(sources).issubset(expected_families) or len(sources) != len(existing["sources"]):
        raise ValueError("Unexpected or duplicate source family in retained manifest")
    for family, url, filename in SOURCES:
        path = output / filename
        if family in sources:
            record = sources[family]
            if record["url"] != url or record["html_path"] != filename:
                raise ValueError(f"Retained source URL/path changed for {family}")
            raw = path.read_bytes()
            if len(raw) != record["bytes"] or hashlib.sha256(raw).hexdigest() != record["sha256"]:
                raise ValueError(f"Retained HTML changed for {family}")
            continue
        if not fetch:
            raise ValueError(f"Missing retained snapshot for {family}; use --fetch in an empty output directory")
        if path.exists():
            raise ValueError(f"Refusing to overwrite HTML without its provenance: {path}")
        request = Request(url, headers={"User-Agent": "qupathxx-format-inventory/1 (metadata-only)", "Accept": "text/html"})
        with urlopen(request, timeout=60) as response:
            raw = response.read(MAX_HTML_BYTES + 1)
            final_url = response.geturl()
            content_type = response.headers.get("Content-Type", "")
            if len(raw) > MAX_HTML_BYTES or "text/html" not in content_type.casefold():
                raise ValueError(f"Unexpected HTML size/content type for {family}")
            if urlsplit(final_url).scheme != "https" or urlsplit(final_url).netloc != urlsplit(url).netloc:
                raise ValueError(f"Unexpected upstream redirect for {family}: {final_url}")
            record = {
                "family": family, "url": url, "final_url": final_url,
                "fetched_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
                "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw),
                "html_path": filename, "content_type": content_type,
                "etag": response.headers.get("ETag"),
                "last_modified": response.headers.get("Last-Modified"),
            }
        path.write_bytes(raw)
        sources[family] = record
        fetched.append(family)
        write_json(manifest_path, {"schema": "qpx.upstream-format-sources/1", "sources": [sources[key] for key in sorted(sources)]})
    return [sources[family] for family, _, _ in SOURCES], fetched


def census(output: Path, sources):
    formats = []
    coverage = {}
    source_records = []
    for record in sources:
        root = parse_html((output / record["html_path"]).read_bytes())
        rows, check = bio_formats(root, record["final_url"]) if record["family"] == "bio-formats" else openslide(root, record["final_url"])
        formats.extend(rows)
        coverage[record["family"]] = check
        details = dict(record, title=title(root))
        if record["family"] == "bio-formats":
            version = re.search(r"Bio-Formats\s+([0-9]+(?:\.[0-9]+)+)", details["title"])
            if version:
                details["upstream_version"] = version.group(1)
        source_records.append(details)
    identities = Counter((row["family"], row["name"], row["url"]) for row in formats)
    duplicates = [{"family": key[0], "name": key[1], "url": key[2], "count": count}
                  for key, count in sorted(identities.items()) if count > 1]
    aliases = defaultdict(list)
    duplicate_aliases = []
    for row in formats:
        local = Counter(value.casefold() for value in row["extensions"])
        for alias, count in sorted(local.items()):
            if count > 1:
                duplicate_aliases.append({"family": row["family"], "name": row["name"], "alias": alias, "count": count})
        for alias in sorted(local):
            aliases[alias].append({"family": row["family"], "name": row["name"], "source_row": row["source_row"]})
    checks = {
        "coverage": coverage,
        "format_count": len(formats),
        "counts_by_family": dict(sorted(Counter(row["family"] for row in formats).items())),
        "extension_alias_count": sum(len(row["extensions"]) for row in formats),
        "unique_casefolded_extensions": len(aliases),
        "duplicate_format_rows": duplicates,
        "duplicate_aliases_within_row": duplicate_aliases,
        "unexpanded_extension_notations": [{"family": row["family"], "name": row["name"],
            "source_row": row["source_row"], "notation": row["extension_notation"]}
            for row in formats if "extension_notation" in row],
        "shared_extension_aliases": {key: value for key, value in sorted(aliases.items()) if len(value) > 1},
        "all_source_rows_retained": all(row["emitted_rows"] == row.get("source_data_rows", row.get("source_format_sections")) for row in coverage.values()),
        "ratings_or_descriptive_prose_extracted": False,
        "licensing_or_implementation_support_inferred": False,
    }
    if not checks["all_source_rows_retained"] or duplicates:
        raise ValueError("Census has missing or duplicate format rows; inspect retained source")
    value = {"schema": "qpx.upstream-format-census/1", "sources": source_records,
             "formats": formats, "checks": checks}
    write_json(output / "formats.json", value)
    write_json(output / "checks.json", checks)
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fetch", action="store_true", help="Fetch missing official index snapshots; never overwrite retained ones")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    try:
        sources, fetched = snapshot_sources(output, args.fetch)
        checks = census(output, sources)
        command = {"command": [sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]],
                   "cwd": str(Path.cwd()), "script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                   "network_requested": args.fetch, "network_fetches_performed": fetched,
                   "counts_by_family": checks["counts_by_family"],
                   "all_source_rows_retained": checks["all_source_rows_retained"]}
        write_json(output / ("fetch-command.json" if args.fetch else "offline-command.json"), command)
        print(json.dumps({"output": str(output), "counts": checks["counts_by_family"],
                          "extensions": checks["extension_alias_count"], "complete": checks["all_source_rows_retained"]}, sort_keys=True))
    except (OSError, ValueError, KeyError, UnicodeError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
