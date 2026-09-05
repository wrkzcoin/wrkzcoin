"""Emit AI-readable copies of the docs alongside the rendered site.

Three artefacts, written into the site directory at build time:

  llms.txt        an index in the llmstxt.org format - one line per page,
                  grouped by the nav section it sits in
  llms-full.txt   every page concatenated, for pasting or fetching whole
  <page>.md       the markdown source mirrored next to each rendered page,
                  so appending ".md" to any docs URL returns the source

Nothing here affects the HTML build. MkDocs loads this file through the
`hooks:` key in mkdocs.yml; it needs no plugin and no extra dependency.
"""

import os
import re

# A page's own H1 and any leading front matter are dropped from the summary
# line; what is wanted is the first sentence of real prose.
_HEADING = re.compile(r"^#{1,6}\s")
_ADMONITION = re.compile(r"^(!!!|\?\?\?|===)")
_LINK = re.compile(r"\[([^\]]+)\]\([^)]+\)")
_CODEFENCE = re.compile(r"^(```|~~~)")


def _paragraphs(markdown):
    """Yield prose paragraphs, skipping code, headings, lists and tables.

    Blocks are only considered whole: taking the first line that merely looks
    like prose picks up list continuations, which read as sentence fragments.
    """
    in_fence = False
    block = []

    for raw in markdown.splitlines() + [""]:
        line = raw.rstrip()

        if _CODEFENCE.match(line.strip()):
            in_fence = not in_fence
            block = []
            continue

        if in_fence:
            continue

        if line.strip():
            block.append(line)
            continue

        if block:
            first = block[0].strip()
            skip = (
                _HEADING.match(first)
                or _ADMONITION.match(first)
                or first.startswith(("|", ">", "-", "*", "+", "1.", "---", "==="))
                or first.startswith("    ")
            )
            if not skip:
                yield " ".join(l.strip() for l in block)
            block = []


def _first_sentence(markdown):
    """First sentence of the first real prose paragraph, links flattened."""
    for paragraph in _paragraphs(markdown):
        text = _LINK.sub(r"\1", paragraph)
        text = text.replace("**", "").replace("`", "")
        text = " ".join(text.split())

        # A lead-in ("Primary knobs:") introduces a list rather than saying
        # anything; keep looking.
        if text.endswith(":"):
            continue

        # Stop at the first sentence end that is not a decimal or a version.
        match = re.search(r"(?<![0-9])\.(?:\s|$)", text)
        if match:
            text = text[: match.start() + 1]

        # A "sentence" full of figures can still run on. Index lines are for
        # picking a page, not for reading it.
        if len(text) > 220:
            text = text[:220].rsplit(" ", 1)[0].rstrip(",;:") + " …"

        if len(text) > 20:
            return text

    return ""


def _title(markdown, fallback):
    for line in markdown.splitlines():
        if line.startswith("# "):
            return line[2:].strip()
    return fallback


def _walk_nav(items, section=None, out=None):
    """Flatten the resolved nav into (section, title, page) triples."""
    if out is None:
        out = []

    for item in items:
        if item.is_section:
            _walk_nav(item.children or [], item.title, out)
        elif item.is_page:
            out.append((section, item.title, item))

    return out


_RESOLVED_NAV = []


def on_nav(nav, config, files, **kwargs):
    """`config["nav"]` is still raw dicts at post_build; keep the real one."""
    global _RESOLVED_NAV
    _RESOLVED_NAV = _walk_nav(list(nav))
    return nav


def on_post_build(config, **kwargs):
    site_dir = config["site_dir"]
    docs_dir = config["docs_dir"]
    site_url = (config.get("site_url") or "").rstrip("/")

    nav_entries = _RESOLVED_NAV

    # Fall back to every file on disk when the nav could not be resolved, so a
    # page is never silently left out of the AI copies.
    if not nav_entries:
        return

    index_lines = [
        "# %s" % config["site_name"],
        "",
        "> %s" % (config.get("site_description") or ""),
        "",
        "Documentation for the WrkzCoin daemon, wallets and RPC surfaces.",
        "Generated from the docs sources; the code in the repository is always",
        "the source of truth. Every page below is also available as raw",
        "markdown by appending `.md` to its URL.",
        "",
    ]

    full_lines = [
        "# %s - full text" % config["site_name"],
        "",
        "Every documentation page concatenated. Source: %s"
        % (config.get("repo_url") or ""),
        "",
    ]

    # Grouped rather than streamed: a nav with two top-level pages would
    # otherwise emit the same fallback section heading twice.
    grouped = []
    seen = {}
    written = 0

    for section, title, page in nav_entries:
        # src_path uses the host separator; URLs and the mirror layout must not.
        src_path = page.file.src_path.replace(os.sep, "/")
        abs_src = os.path.join(docs_dir, src_path.replace("/", os.sep))

        try:
            with open(abs_src, encoding="utf-8") as handle:
                markdown = handle.read()
        except OSError:
            continue

        # Mirror the source next to the rendered page: docs/guides/x.md is
        # served at /guides/x/ and the source lands at /guides/x.md.
        dest = os.path.join(site_dir, src_path.replace("/", os.sep))
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(markdown)
        written += 1

        url = "%s/%s" % (site_url, src_path) if site_url else src_path
        summary = _first_sentence(markdown)
        heading = title or _title(markdown, src_path)
        name = section or "Overview"

        if name not in seen:
            seen[name] = []
            grouped.append(name)

        seen[name].append(
            "- [%s](%s)%s" % (heading, url, (": %s" % summary) if summary else "")
        )

        full_lines.append("")
        full_lines.append("<!-- source: %s -->" % src_path)
        full_lines.append("")
        full_lines.append(markdown.rstrip())
        full_lines.append("")
        full_lines.append("---")

    for name in grouped:
        index_lines.append("## %s" % name)
        index_lines.append("")
        index_lines.extend(seen[name])
        index_lines.append("")

    with open(os.path.join(site_dir, "llms.txt"), "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(index_lines))

    with open(
        os.path.join(site_dir, "llms-full.txt"), "w", encoding="utf-8", newline="\n"
    ) as handle:
        handle.write("\n".join(full_lines))

    print("llms_txt: wrote llms.txt, llms-full.txt and %d raw markdown pages" % written)
