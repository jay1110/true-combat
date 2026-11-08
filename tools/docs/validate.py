"""Validate public documentation metadata and all local links in built HTML."""
import argparse
from html.parser import HTMLParser
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]


class Page(HTMLParser):
    def __init__(self, text):
        super().__init__(convert_charrefs=True)
        self.ids = set()
        self.links = []
        self.language = None
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "html": self.language = attrs.get("lang")
        for key in ("id", "name"):
            if attrs.get(key): self.ids.add(attrs[key])
        if tag == "a" and attrs.get("href"): self.links.append(attrs["href"])


def validate(site):
    site = site.resolve()
    errors = []
    pages = {p.resolve(): Page(p.read_text(encoding="utf8")) for p in site.rglob("*.html")}
    if not pages: return ["No built HTML pages; build Sphinx first."], 0, 0
    checked = 0
    for path, page in pages.items():
        if page.language != "en": errors.append(f"Non-English HTML language: {path.name}")
        for href in page.links:
            url = urlsplit(href)
            if url.scheme or url.netloc: continue
            # These fragments are browser-side tab switches, not target IDs.
            if href == "#": continue
            target = (path.parent / unquote(url.path)).resolve() if url.path else path
            if target.is_dir(): target /= "index.html"
            checked += 1
            if not target.is_file():
                errors.append(f"{path.relative_to(site)}: missing local target {href}")
            elif url.fragment and target in pages and unquote(url.fragment) not in pages[target].ids:
                errors.append(f"{path.relative_to(site)}: missing anchor {href}")
    release = re.search(r'^#define TCE_VERSION "([^"]+)"',
                        (ROOT/"src/game/tce_version.h").read_text(), re.M).group(1)
    for name in ("README.md", "CHANGELOG.md"):
        if release not in (ROOT/name).read_text(encoding="utf8"):
            errors.append(f"{name}: missing current version {release}")
    config = (ROOT/".readthedocs.yaml").read_text(encoding="utf8")
    for value in ("version: 2", "Docs/conf.py", "Docs/requirements.txt", "fail_on_warning: true"):
        if value not in config: errors.append("Incomplete Read the Docs configuration: "+value)
    for path in [ROOT/"README.md", ROOT/"BUILDING.md", *ROOT.glob("Docs/*.md")]:
        text = path.read_text(encoding="utf8")
        if re.search(r"(?:[A-Za-z]:[/\\]Users[/\\]|/(?:home|Users)/)", text):
            errors.append(f"Personal filesystem path in {path.relative_to(ROOT)}")
    return errors, len(pages), checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--html", type=Path, default=ROOT/"build/docs/html")
    args = parser.parse_args()
    errors, pages, links = validate(args.html)
    for error in errors: print(error)
    print(f"Documentation: {pages} English HTML pages, {links} local links, {len(errors)} errors")
    return bool(errors)


if __name__ == "__main__":
    raise SystemExit(main())
