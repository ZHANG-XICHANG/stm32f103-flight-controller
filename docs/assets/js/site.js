/* 共用：程式碼依據連結、章節目錄高亮、架構圖延遲載入。 */
(function () {
  'use strict';

  // 所有程式碼引用固定在繪製本網站時的韌體版本，行號才不會隨後續修改而失準。
  const REPO = 'https://github.com/ZHANG-XICHANG/stm32f103-flight-controller';
  const REV = '20403009182e01366f5a8977ab0dfac5825229a3';
  window.SITE_REPO = { url: REPO, rev: REV };

  // <span class="src" data-src="Core/Src/flight.c:270-301"> → GitHub 行號連結
  function linkSources(root) {
    root.querySelectorAll('[data-src]').forEach((el) => {
      if (el.dataset.linked) return;
      const spec = el.dataset.src;
      const m = spec.match(/^([^:]+)(?::(\d+)(?:-(\d+))?)?$/);
      if (!m) return;
      const [, path, start, end] = m;
      const a = document.createElement('a');
      a.className = el.className || 'src';
      a.href = `${REPO}/blob/${REV}/${path}` + (start ? `#L${start}` + (end ? `-L${end}` : '') : '');
      a.target = '_blank';
      a.rel = 'noopener';
      a.title = `${path}（commit ${REV.slice(0, 7)}）`;
      a.textContent = el.textContent.trim() || path.split('/').pop() + (start ? `:${start}` + (end ? `-${end}` : '') : '');
      a.dataset.linked = '1';
      el.replaceWith(a);
    });
  }
  window.linkSources = linkSources;
  linkSources(document);

  // 章節目錄：捲動時標示目前章節
  const tocLinks = [...document.querySelectorAll('.toc a[href^="#"]')];
  if (tocLinks.length && 'IntersectionObserver' in window) {
    const map = new Map(tocLinks.map((a) => [a.getAttribute('href').slice(1), a]));
    const visible = new Set();
    const io = new IntersectionObserver((entries) => {
      entries.forEach((e) => (e.isIntersecting ? visible.add(e.target.id) : visible.delete(e.target.id)));
      const first = [...map.keys()].find((id) => visible.has(id));
      if (first) tocLinks.forEach((a) => a.classList.toggle('active', a === map.get(first)));
    }, { rootMargin: '-70px 0px -55% 0px' });
    map.forEach((_, id) => { const s = document.getElementById(id); if (s) io.observe(s); });
  }
  // 手機版：點目錄後收合
  document.querySelectorAll('.toc details a').forEach((a) =>
    a.addEventListener('click', () => { const d = a.closest('details'); if (d && innerWidth < 1000) d.open = false; }));
  const tocDetails = document.querySelector('.toc details');
  if (tocDetails && innerWidth >= 1000) tocDetails.open = true;

  // 架構圖：點擊後才載入 iframe（避免一次載入數 MB 的互動圖）
  document.querySelectorAll('[data-embed]').forEach((box) => {
    const btn = box.querySelector('button[data-load]');
    if (!btn) return;
    btn.addEventListener('click', () => {
      const frame = document.createElement('iframe');
      frame.src = box.dataset.embed;
      frame.title = box.dataset.title || '互動架構圖';
      frame.loading = 'lazy';
      box.querySelector('.embed-placeholder').replaceWith(frame);
    });
  });
})();
