/* 輕量折線圖：多面板共用 X 軸、十字準線與提示框、拖曳框選放大、雙擊重設。
 * 不依賴任何外部套件；以 canvas 繪製並依像素欄做 min/max 抽樣，萬筆資料仍流暢。 */
(function () {
  'use strict';

  const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  const GRID = '#1f2a37';
  const AXIS = '#3a4a5e';
  const ZERO = '#4a5b70';

  function niceStep(span, target) {
    const raw = span / Math.max(1, target);
    const mag = Math.pow(10, Math.floor(Math.log10(raw)));
    const n = raw / mag;
    return (n < 1.5 ? 1 : n < 3 ? 2 : n < 7 ? 5 : 10) * mag;
  }
  function fmt(v, step) {
    const d = step >= 1 ? 0 : Math.min(4, Math.ceil(-Math.log10(step)));
    return v.toFixed(d);
  }
  function lowerBound(arr, v) {
    let lo = 0, hi = arr.length;
    while (lo < hi) { const mid = (lo + hi) >> 1; if (arr[mid] < v) lo = mid + 1; else hi = mid; }
    return lo;
  }

  class LinePlot {
    constructor(container, opts) {
      this.opts = Object.assign({ height: 170, yRange: null, xLabel: '', yLabel: '', minSpan: 0, digits: 2 }, opts);
      this.series = this.opts.series.map((s, i) => Object.assign({ color: css(`--series-${i + 1}`) || '#888', visible: true }, s));
      this.box = document.createElement('div');
      this.box.className = 'chart';
      if (this.opts.title) {
        const t = document.createElement('div');
        t.className = 'chart-title';
        t.innerHTML = `<b>${this.opts.title}</b>` + (this.opts.yLabel ? `<span class="muted">${this.opts.yLabel}</span>` : '');
        const lg = document.createElement('span');
        lg.className = 'legend';
        if (this.series.length > 1) {
          lg.innerHTML = this.series.map((s) =>
            `<span><span class="sw${s.dash ? ' dash' : ''}" style="background:${s.color};border-color:${s.color}"></span>${s.label}</span>`).join('');
        }
        t.appendChild(lg);
        this.box.appendChild(t);
      }
      this.canvas = document.createElement('canvas');
      this.canvas.setAttribute('role', 'img');
      this.canvas.setAttribute('aria-label', (this.opts.title || '折線圖') + '：' + this.series.map((s) => s.label).join('、'));
      this.box.appendChild(this.canvas);
      this.tip = document.createElement('div');
      this.tip.className = 'tooltip';
      this.box.appendChild(this.tip);
      container.appendChild(this.box);
      this.ctx = this.canvas.getContext('2d');
      this.x = null; this.ys = {}; this.view = null; this.hoverX = null; this.sel = null;
      this.pad = { l: 54, r: 12, t: 8, b: 24 };
      this.resize();
    }

    resize() {
      const w = this.box.clientWidth || 600;
      const h = innerWidth < 600 ? Math.round(this.opts.height * 0.8) : this.opts.height;
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      this.w = w; this.h = h;
      this.canvas.width = Math.round(w * dpr); this.canvas.height = Math.round(h * dpr);
      this.canvas.style.height = h + 'px';
      this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      this.pad.l = w < 420 ? 44 : 54;
      this.pad.b = this.opts.xLabel ? 34 : 24;
      this.draw();
    }

    setData(x, ys) { this.x = x; this.ys = ys; if (!this.view && x && x.length) this.view = [x[0], x[x.length - 1]]; }
    setView(v) { this.view = v; this.draw(); }
    setVisible(key, on) { const s = this.series.find((q) => q.key === key); if (s) { s.visible = on; this.draw(); } }

    yExtent(i0, i1) {
      if (this.opts.yRange) return this.opts.yRange;
      let lo = Infinity, hi = -Infinity;
      for (const s of this.series) {
        if (!s.visible) continue;
        const y = this.ys[s.key]; if (!y) continue;
        for (let i = i0; i < i1; i++) { const v = y[i]; if (v < lo) lo = v; if (v > hi) hi = v; }
      }
      if (!isFinite(lo)) { lo = -1; hi = 1; }
      if (hi - lo < this.opts.minSpan) { const c = (hi + lo) / 2; lo = c - this.opts.minSpan / 2; hi = c + this.opts.minSpan / 2; }
      const p = (hi - lo) * 0.08 || 1;
      return [lo - p, hi + p];
    }

    draw() {
      const { ctx, w, h, pad } = this;
      ctx.clearRect(0, 0, w, h);
      if (!this.x || !this.x.length || !this.view) return;
      const [x0, x1] = this.view;
      const i0 = Math.max(0, lowerBound(this.x, x0) - 1);
      const i1 = Math.min(this.x.length, lowerBound(this.x, x1) + 2);
      const [y0, y1] = this.yExtent(i0, i1);
      this.yr = [y0, y1];
      const pw = w - pad.l - pad.r, ph = h - pad.t - pad.b;
      const sx = (v) => pad.l + ((v - x0) / (x1 - x0)) * pw;
      const sy = (v) => pad.t + (1 - (v - y0) / (y1 - y0)) * ph;
      this.sx = sx; this.sy = sy;

      ctx.font = '11px ' + (css('--mono') || 'monospace');
      ctx.textBaseline = 'middle';
      // Y 格線
      const ys = niceStep(y1 - y0, ph < 120 ? 3 : 4);
      ctx.strokeStyle = GRID; ctx.lineWidth = 1; ctx.fillStyle = '#8d9bab'; ctx.textAlign = 'right';
      for (let v = Math.ceil(y0 / ys) * ys; v <= y1; v += ys) {
        const y = Math.round(sy(v)) + 0.5;
        ctx.strokeStyle = Math.abs(v) < ys / 1e6 ? ZERO : GRID;
        ctx.beginPath(); ctx.moveTo(pad.l, y); ctx.lineTo(w - pad.r, y); ctx.stroke();
        ctx.fillText(fmt(v, ys), pad.l - 6, y);
      }
      // X 格線
      const xs = niceStep(x1 - x0, Math.max(2, Math.floor(pw / 90)));
      ctx.textAlign = 'center'; ctx.textBaseline = 'top';
      for (let v = Math.ceil(x0 / xs) * xs; v <= x1; v += xs) {
        const x = Math.round(sx(v)) + 0.5;
        ctx.strokeStyle = GRID; ctx.beginPath(); ctx.moveTo(x, pad.t); ctx.lineTo(x, pad.t + ph); ctx.stroke();
        ctx.fillText(fmt(v, xs), x, pad.t + ph + 5);
      }
      ctx.strokeStyle = AXIS; ctx.strokeRect(pad.l + 0.5, pad.t + 0.5, pw, ph);
      if (this.opts.xLabel) { ctx.textAlign = 'right'; ctx.fillStyle = '#6f7d8d'; ctx.fillText(this.opts.xLabel, w - pad.r, pad.t + ph + 19); }

      // 資料線（裁切在繪圖區內）
      ctx.save();
      ctx.beginPath(); ctx.rect(pad.l, pad.t, pw, ph); ctx.clip();
      ctx.lineJoin = 'round'; ctx.lineCap = 'round';
      for (const s of this.series) {
        if (!s.visible) continue;
        const y = this.ys[s.key]; if (!y) continue;
        ctx.strokeStyle = s.color; ctx.lineWidth = s.width || 1.6;
        ctx.setLineDash(s.dash ? [6, 4] : []);
        ctx.beginPath();
        const n = i1 - i0;
        if (n > pw * 2) {
          // 依像素欄取 min/max，保留尖峰
          let col = -1, mn = 0, mx = 0, first = true;
          const flush = () => { if (col < 0) return; const X = pad.l + col; if (first) { ctx.moveTo(X, sy(mn)); first = false; } else ctx.lineTo(X, sy(mn)); ctx.lineTo(X, sy(mx)); };
          for (let i = i0; i < i1; i++) {
            const c = Math.floor(sx(this.x[i]) - pad.l);
            const v = y[i];
            if (c !== col) { flush(); col = c; mn = mx = v; } else { if (v < mn) mn = v; if (v > mx) mx = v; }
          }
          flush();
        } else {
          for (let i = i0; i < i1; i++) { const X = sx(this.x[i]), Y = sy(y[i]); i === i0 ? ctx.moveTo(X, Y) : ctx.lineTo(X, Y); }
        }
        ctx.stroke();
      }
      ctx.setLineDash([]);
      ctx.restore();

      // 框選範圍
      if (this.sel) {
        const a = Math.max(pad.l, Math.min(this.sel[0], this.sel[1])), b = Math.min(w - pad.r, Math.max(this.sel[0], this.sel[1]));
        ctx.fillStyle = 'rgba(76,181,255,0.14)'; ctx.fillRect(a, pad.t, b - a, ph);
      }
      // 十字準線
      if (this.hoverX != null && this.hoverX >= x0 && this.hoverX <= x1) {
        const idx = this.nearest(this.hoverX);
        const X = Math.round(sx(this.x[idx])) + 0.5;
        ctx.strokeStyle = '#7f90a4'; ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo(X, pad.t); ctx.lineTo(X, pad.t + ph); ctx.stroke();
        for (const s of this.series) {
          if (!s.visible || !this.ys[s.key]) continue;
          const Y = sy(this.ys[s.key][idx]);
          if (Y < pad.t || Y > pad.t + ph) continue;
          ctx.fillStyle = s.color; ctx.strokeStyle = '#121922'; ctx.lineWidth = 2;
          ctx.beginPath(); ctx.arc(X, Y, 4, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
        }
      }
    }

    nearest(xv) {
      const i = lowerBound(this.x, xv);
      if (i <= 0) return 0;
      if (i >= this.x.length) return this.x.length - 1;
      return (xv - this.x[i - 1] < this.x[i] - xv) ? i - 1 : i;
    }

    showTip(px, py) {
      if (this.hoverX == null || !this.x) { this.tip.style.display = 'none'; return; }
      const idx = this.nearest(this.hoverX);
      const xf = this.opts.xFormat || ((v) => v.toFixed(3));
      let html = `<div style="color:#8d9bab">${this.opts.xName || 't'} = ${xf(this.x[idx])}</div>`;
      for (const s of this.series) {
        if (!s.visible || !this.ys[s.key]) continue;
        const v = this.ys[s.key][idx];
        html += `<div><span class="sw" style="background:${s.color}"></span>${s.label}：${Number.isFinite(v) ? v.toFixed(this.opts.digits) : '—'}</div>`;
      }
      this.tip.innerHTML = html;
      this.tip.style.display = 'block';
      const tw = this.tip.offsetWidth, th = this.tip.offsetHeight;
      const top = (this.opts.title ? 22 : 0);
      let left = px + 14; if (left + tw > this.w) left = px - tw - 14; if (left < 0) left = 2;
      let tp = top + Math.max(0, Math.min(py - th / 2, this.h - th));
      this.tip.style.left = left + 'px'; this.tip.style.top = tp + 'px';
    }
    hideTip() { this.tip.style.display = 'none'; }
    pxToX(px) { const [x0, x1] = this.view; return x0 + ((px - this.pad.l) / (this.w - this.pad.l - this.pad.r)) * (x1 - x0); }
  }

  /* 多面板群組：共用視窗範圍、同步十字準線、滑鼠拖曳框選放大。 */
  class PlotGroup {
    constructor(plots, opts = {}) {
      this.plots = plots; this.full = null; this.onView = opts.onView || null;
      plots.forEach((p) => this.bind(p));
      let raf = 0;
      const ro = new ResizeObserver(() => { cancelAnimationFrame(raf); raf = requestAnimationFrame(() => plots.forEach((p) => p.resize())); });
      plots.forEach((p) => ro.observe(p.box));
    }
    setData(x, ys) {
      this.full = x.length ? [x[0], x[x.length - 1]] : null;
      this.plots.forEach((p) => { p.setData(x, ys); p.view = this.full; });
      this.redraw();
    }
    setView(v) {
      if (!this.full) return;
      let [a, b] = v;
      a = Math.max(this.full[0], a); b = Math.min(this.full[1], b);
      if (b - a <= 0) return;
      this.plots.forEach((p) => (p.view = [a, b]));
      this.redraw();
      if (this.onView) this.onView([a, b]);
    }
    reset() { if (this.full) this.setView(this.full); }
    redraw() { this.plots.forEach((p) => p.draw()); }
    bind(p) {
      const cv = p.canvas;
      const local = (e) => { const r = cv.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; };
      let dragStart = null;
      const move = (e) => {
        if (!p.x) return;
        const [px, py] = local(e);
        const xv = p.pxToX(px);
        this.plots.forEach((q) => { q.hoverX = xv; if (q !== p) { q.hideTip(); } });
        if (dragStart != null) { p.sel = [dragStart, px]; }
        this.redraw();
        p.showTip(px, py);
      };
      cv.addEventListener('pointermove', move);
      cv.addEventListener('pointerleave', () => { this.plots.forEach((q) => { q.hoverX = null; q.hideTip(); }); if (dragStart == null) this.redraw(); });
      cv.addEventListener('pointerdown', (e) => {
        if (e.pointerType !== 'mouse') { move(e); return; } // 觸控：點一下即顯示該點數值
        if (e.button !== 0) return;
        dragStart = local(e)[0]; p.sel = null; cv.setPointerCapture(e.pointerId);
      });
      cv.addEventListener('pointerup', (e) => {
        if (dragStart == null) return;
        const end = local(e)[0];
        const a = Math.min(dragStart, end), b = Math.max(dragStart, end);
        dragStart = null; p.sel = null;
        if (b - a > 6) this.setView([p.pxToX(a), p.pxToX(b)]); else this.redraw();
      });
      cv.addEventListener('dblclick', () => this.reset());
    }
  }

  window.Plot = { LinePlot, PlotGroup, niceStep };
})();
