"""同步 docs/*.html 的頁首導覽與頁尾。

各頁以 <!-- layout:header --> … <!-- /layout:header --> 與
<!-- layout:footer --> … <!-- /layout:footer --> 標記可替換區塊；
修改導覽或頁尾時只改這裡，再從專案根目錄執行：

    python docs/tools/sync-layout.py
"""
from pathlib import Path
import re

DOCS = Path(__file__).resolve().parent.parent

NAV = [
    ("index.html", "", "首頁"),
    ("hardware.html", "01", "硬體"),
    ("software.html", "02", "軟體"),
    ("ahrs.html", "03", "AHRS"),
    ("control.html", "04", "控制"),
    ("comm.html", "05", "通訊"),
    ("experiments.html", "06", "實驗"),
    ("research.html", "07", "研究延伸"),
    ("architecture/", "", "架構圖"),
]


def header(current: str) -> str:
    items = []
    for href, num, label in NAV:
        cur = ' aria-current="page"' if href == current else ""
        n = f'<span class="num">{num}</span>' if num else ""
        items.append(f'    <li><a href="{href}"{cur}>{n}{label}</a></li>')
    return (
        '<header class="site-header"><div class="inner">\n'
        '  <a class="brand" href="index.html">STM32F103 四旋翼飛控</a>\n'
        '  <nav class="primary-nav" aria-label="章節"><ul>\n'
        + "\n".join(items)
        + "\n  </ul></nav>\n</div></header>"
    )


FOOTER = """<footer class="site-footer"><div class="inner">
  <div class="legend-tags">標記說明：
    <span class="tag measured">實測</span>實驗資料或實拍
    <span class="tag derived">推算</span>由程式碼或資料計算
    <span class="tag sim">模擬</span>假設模型的示意結果
    <span class="tag code">程式碼</span>原始碼設定值
    <span class="tag missing">待補</span>尚無資料
  </div>
  <div>技術內容依韌體 commit <code>2040300</code>；<span class="mono">⌗</span> 標記可連到 GitHub 對應行號。原始碼：<a href="https://github.com/ZHANG-XICHANG/stm32f103-flight-controller" target="_blank" rel="noopener">ZHANG-XICHANG/stm32f103-flight-controller</a></div>
</div></footer>"""


def replace_block(text: str, name: str, body: str) -> str:
    pattern = re.compile(rf"<!-- layout:{name} -->.*?<!-- /layout:{name} -->", re.S)
    if not pattern.search(text):
        raise SystemExit(f"missing layout:{name} marker")
    return pattern.sub(lambda _: f"<!-- layout:{name} -->\n{body}\n<!-- /layout:{name} -->", text)


for page in sorted(DOCS.glob("*.html")):
    text = page.read_text(encoding="utf-8")
    if "<!-- layout:header -->" not in text:
        continue
    text = replace_block(text, "header", header(page.name))
    text = replace_block(text, "footer", FOOTER)
    page.write_text(text, encoding="utf-8", newline="\n")
    print("synced", page.name)
