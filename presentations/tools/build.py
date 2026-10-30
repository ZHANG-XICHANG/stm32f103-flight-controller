"""以 Marp CLI 匯出簡報：HTML → docs/slides/index.html，PDF / PPTX → presentations/。

    python presentations/tools/build.py

HTML 版引用 assets/ 的相對路徑，因此一併把圖片複製到 docs/slides/assets/，
讓 GitHub Pages 的 /slides/ 子路徑可以直接顯示。
"""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PRES = ROOT / "presentations"
SRC = PRES / "flight-controller.md"
SLIDES = ROOT / "docs" / "slides"
MARP = ["marp", "--theme-set", str(PRES / "assets" / "theme.css"), "--html", "--allow-local-files"]


def run(*args):
    cmd = MARP + [str(SRC), *args]
    print(">", " ".join(cmd[1:]))
    subprocess.run(cmd, check=True, shell=(shutil.which("marp") or "").lower().endswith((".cmd", ".bat")))


SLIDES.mkdir(parents=True, exist_ok=True)
run("-o", str(SLIDES / "index.html"))
run("--pdf", "-o", str(PRES / "flight-controller.pdf"))
run("--pptx", "-o", str(PRES / "flight-controller.pptx"))

dst = SLIDES / "assets"
if dst.exists():
    shutil.rmtree(dst)
for sub in ("img", "diagrams", "figures"):
    shutil.copytree(PRES / "assets" / sub, dst / sub)
print("copied assets ->", dst)
