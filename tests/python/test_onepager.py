"""Unit tests for F3 — the QSE one-pager PDF generator.

Done-when: the PDF exists in docs/ and renders correctly. Here we assert the
builder emits a valid, single-page PDF (and degrades gracefully when the embedded
figure is missing); the committed artifact is docs/QSE_one_pager.pdf.
"""

import re
import sys
from pathlib import Path

import pytest

pytest.importorskip("matplotlib")  # notebook/plot toolchain, not a base dep

_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_ROOT / "scripts" / "analysis"))

import onepager  # noqa: E402

FLAGSHIP = _ROOT / "docs" / "research" / "microstructure" / "slippage_audit.png"


def _pages(pdf_bytes: bytes) -> int:
    return len(re.findall(rb"/Type\s*/Page[^s]", pdf_bytes))


def test_builds_single_page_pdf(tmp_path):
    out = onepager.build(tmp_path / "one.pdf", FLAGSHIP)
    assert out.exists()
    data = out.read_bytes()
    assert data[:5] == b"%PDF-"  # a real PDF
    assert _pages(data) == 1  # a one-pager, literally
    assert len(data) > 10_000  # non-trivial (figure embedded)


def test_survives_missing_figure(tmp_path):
    # a fresh clone might not have the PNG yet — the builder must still produce a
    # valid PDF with a placeholder rather than raising
    out = onepager.build(tmp_path / "nofig.pdf", tmp_path / "does_not_exist.png")
    assert out.read_bytes()[:5] == b"%PDF-"
    assert _pages(out.read_bytes()) == 1


def test_committed_onepager_is_present_and_valid():
    committed = _ROOT / "docs" / "QSE_one_pager.pdf"
    assert committed.exists(), "run scripts/analysis/onepager.py to build docs/QSE_one_pager.pdf"
    assert committed.read_bytes()[:5] == b"%PDF-"
