"""Build the public manual without importing the game or requiring its assets."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "src/game/tce_version.h").read_text(encoding="utf-8")
release = re.search(r'^#define TCE_VERSION "([^"]+)"', header, re.M).group(1)
version = release
project = "TCE2"
author = "TCE2 contributors"
copyright = "2026, TCE2 contributors and credited component authors"
language = "en"
extensions = ["myst_parser"]
source_suffix = {".rst": "restructuredtext", ".md": "markdown"}
root_doc = "index"
exclude_patterns = ["CHANGELOG.de.txt", "requirements.txt", "README.md"]
nitpicky = True
myst_enable_extensions = ["colon_fence", "deflist"]
myst_heading_anchors = 4
html_theme = "sphinx_rtd_theme"
html_title = f"TCE2 {release} documentation"
html_short_title = "TCE2"
html_static_path = ["_static"]
html_css_files = ["tce2.css"]
html_theme_options = {"navigation_depth": 3, "style_nav_header_background": "#18232e"}
html_context = {
    "display_github": True,
    "github_user": "jay1110",
    "github_repo": "true-combat",
    "github_version": "final",
    "conf_py_path": "/Docs/",
}
html_show_sourcelink = True
epub_title = "TCE2 Manual"
epub_author = author
epub_exclude_files = ["search.html", "genindex.html"]
linkcheck_ignore = [r"https://github\.com/.*/(?:blob|tree)/final/.*"]
linkcheck_timeout = 15
linkcheck_retries = 1
