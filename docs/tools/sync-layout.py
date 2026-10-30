"""同步 docs/*.html 的頁首導覽與頁尾。

各頁以 <!-- layout:header --> … <!-- /layout:header --> 與
<!-- layout:footer --> … <!-- /layout:footer --> 標記可替換區塊；
修改導覽或頁尾時只改這裡，再從專案根目錄執行：

    python docs/tools/sync-layout.py
"""
from pathlib import Path
import re

DOCS = Path(__file__).resolve().parent.parent
REPO = "https://github.com/ZHANG-XICHANG/stm32f103-flight-controller"
GITHUB_ICON = (
    '<svg viewBox="0 0 16 16" width="16" height="16" aria-hidden="true"><path fill="currentColor" d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"/></svg>'
)

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
        + "\n  </ul></nav>\n"
        f'  <a class="gh-link" href="{REPO}" target="_blank" rel="noopener" title="GitHub 原始碼">{GITHUB_ICON}<span>GitHub 原始碼</span></a>\n'
        "</div></header>"
    )


FOOTER = f"""<footer class="site-footer"><div class="inner">
  <div class="legend-tags">標記說明：
    <span class="tag measured">實測</span>實驗資料或實拍
    <span class="tag derived">推算</span>由程式碼或資料計算
    <span class="tag sim">模擬</span>假設模型的示意結果
    <span class="tag code">程式碼</span>原始碼設定值
    <span class="tag missing">待補</span>尚無資料
  </div>
  <div>技術內容依韌體 commit <code>2040300</code>；<span class="mono">⌗</span> 標記可連到 GitHub 對應行號。原始碼：<a href="{REPO}" target="_blank" rel="noopener">ZHANG-XICHANG/stm32f103-flight-controller</a></div>
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
