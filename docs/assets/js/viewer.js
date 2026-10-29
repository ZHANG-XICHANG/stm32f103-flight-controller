/* 實測遙測資料檢視器：讀取 data/*.csv（原始記錄的副本），去除重複序號後繪圖並計算統計。 */
(function () {
  'use strict';

  const RUNS = [
    { id: 'run-p-2.3', file: 'data/run-p-2.3.csv', orig: 'telemetry_p_2.3.csv', when: '2026-09-22 23:39', label: 'p_2.3：擾動後衰減振盪',
      note: '檔名標示 P 2.3。11.8 s 的擾動事件與 PID/未整理/2026-09-22 23 39 18.png 截圖（seq 9200）吻合。', events: [[10.5, 14.5, '11.8 s 擾動事件']] },
    { id: 'run-new', file: 'data/run-new.csv', orig: 'telemetry_new.csv', when: '2026-09-22 23:23', label: 'new：外環 Kp≈3、長時間記錄',
      note: '此檔沒有 P 項欄位。24.3 s 的擾動事件與 PID/D低通濾波/外環3_0_0.01.png 截圖吻合。', events: [[23, 27.5, '24.3 s 擾動事件'], [6.5, 9.5, '7.7 s 反向擾動']] },
    { id: 'run-d-0.13', file: 'data/run-d-0.13.csv', orig: 'telemetry_D_0.13.csv', when: '2026-09-22 23:36', label: 'D_0.13：穩態 + 單次擾動',
      note: '檔名標示 D 0.13（D 項增益無法由資料反推驗證）。', events: [[15.5, 19, '16.6 s 擾動事件']] },
    { id: 'run-p-2.33', file: 'data/run-p-2.33.csv', orig: 'telemetry_p_2.33.csv', when: '2026-09-22 23:42', label: 'p_2.33：大角度擺動',
      note: '檔名標示 P 2.33。外環斜率擬合偏離 3，代表期間有搖桿輸入或大角度擺動。', events: [[46.5, 51, '48.3 s 大擾動（roll −35°）'], [6.5, 10, '7.9 s 擾動']] },
    { id: 'run-d-0.12', file: 'data/run-d-0.12.csv', orig: 'telemetry_D_0.12.csv', when: '2026-09-22 23:31', label: 'D_0.12：大傾角（約 −70°）',
      note: '檔名標示 D 0.12。記錄開始時 roll 約 −69°，PID 輸出長時間很大；記錄條件未留存。', events: [[0, 15, '前 15 s 大傾角']] },
    { id: 'run-legacy-x', file: 'data/run-legacy-x.csv', orig: 'telemetry.csv', when: '2026-09-21 18:16', label: '舊格式：只有角速度 / 誤差 / 輸出', legacy: true,
      note: '舊版 serial_scope 格式，沒有飛控時間戳，時間軸為電腦接收時間。輸出 / 誤差的中位數比值為 3.0。', events: [] },
  ];

  const $ = (id) => document.getElementById(id);
  const cache = new Map();

  function parseCsv(text) {
    const lines = text.trim().split(/\r?\n/);
    const head = lines[0].split(',');
    const cols = Object.fromEntries(head.map((h) => [h, []]));
    const iseq = head.indexOf('sequence');
    let last = null, rows = 0, dup = 0;
    const seen = new Set();
    for (let i = 1; i < lines.length; i++) {
      const c = lines[i].split(',');
      if (c.length !== head.length) continue;
      rows++;
      const s = +c[iseq];
      if (seen.has(s)) { dup++; continue; } // 重複序號＝同一筆快照被送了兩次
      seen.add(s); last = s;
      head.forEach((h, k) => cols[h].push(c[k] === '' ? NaN : +c[k]));
    }
    const out = {};
    for (const h of head) out[h] = Float64Array.from(cols[h]);
    return { head, cols: out, rows, dup, n: out[head[0]].length };
  }

  async function load(run) {
    if (cache.has(run.id)) return cache.get(run.id);
    const res = await fetch(run.file);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const d = parseCsv(await res.text());
    cache.set(run.id, d);
    return d;
  }

  // ---------- 數值工具 ----------
  const rms = (a) => Math.sqrt(a.reduce((s, v) => s + v * v, 0) / a.length);
  function lsSlope(x, y) { // y ≈ k x + c
    let sx = 0, sy = 0, sxx = 0, sxy = 0; const n = x.length;
    for (let i = 0; i < n; i++) { sx += x[i]; sy += y[i]; sxx += x[i] * x[i]; sxy += x[i] * y[i]; }
    const k = (n * sxy - sx * sy) / (n * sxx - sx * sx);
    return { k, c: (sy - k * sx) / n };
  }
  function median(a) { const s = Array.from(a).sort((p, q) => p - q); return s.length ? s[s.length >> 1] : NaN; }
  function fft(re, im) {
    const n = re.length;
    for (let i = 1, j = 0; i < n; i++) { let bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; } }
    for (let len = 2; len <= n; len <<= 1) {
      const ang = (-2 * Math.PI) / len, wr = Math.cos(ang), wi = Math.sin(ang);
      for (let i = 0; i < n; i += len) {
        let cr = 1, ci = 0;
        for (let k = 0; k < len / 2; k++) {
          const ur = re[i + k], ui = im[i + k];
          const vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci, vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
          re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
          const t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
        }
      }
    }
  }
  // 依序號重建 6 ms 等間隔訊號（缺口線性內插），回傳振幅頻譜
  function spectrum(seq, sig, dt) {
    const s0 = seq[0], N0 = seq[seq.length - 1] - s0 + 1;
    const u = new Float64Array(N0);
    let j = 0;
    for (let i = 0; i < N0; i++) {
      const s = s0 + i;
      while (j < seq.length - 2 && seq[j + 1] <= s) j++;
      const a = seq[j], b = seq[j + 1] ?? a;
      u[i] = b === a ? sig[j] : sig[j] + ((sig[j + 1] - sig[j]) * (s - a)) / (b - a);
    }
    let N = 1; while (N < N0) N <<= 1;
    const re = new Float64Array(N), im = new Float64Array(N);
    const mean = u.reduce((p, v) => p + v, 0) / N0;
    for (let i = 0; i < N0; i++) re[i] = (u[i] - mean) * (0.5 - 0.5 * Math.cos((2 * Math.PI * i) / (N0 - 1)));
    fft(re, im);
    const half = N / 2, f = new Float64Array(half), m = new Float64Array(half);
    for (let i = 0; i < half; i++) { f[i] = i / (N * dt); m[i] = (2 * Math.hypot(re[i], im[i])) / (N0 * 0.5); }
    return { f, m };
  }
  function peakIn(sp, lo, hi) { let bi = -1; for (let i = 0; i < sp.f.length; i++) if (sp.f[i] >= lo && sp.f[i] <= hi && (bi < 0 || sp.m[i] > sp.m[bi])) bi = i; return bi < 0 ? null : { f: sp.f[bi], m: sp.m[bi] }; }

  function stats(run, d) {
    const c = d.cols, out = [];
    const seq = c.sequence;
    const span = seq[d.n - 1] - seq[0] + 1;
    out.push(['原始列數 / 去重後', `${d.rows.toLocaleString()} / ${d.n.toLocaleString()}（重複 ${d.dup}）`, 'measured']);
    out.push(['序號覆蓋率', `${((d.n / span) * 100).toFixed(1)}%（${d.n} / ${span}）`, 'measured']);
    if (c.time_ms) {
      const dts = []; for (let i = 1; i < d.n; i++) if (seq[i] === seq[i - 1] + 1) dts.push(c.time_ms[i] - c.time_ms[i - 1]);
      const six = dts.filter((v) => v === 6).length;
      out.push(['記錄長度（飛控時間）', `${((c.time_ms[d.n - 1] - c.time_ms[0]) / 1000).toFixed(2)} s`, 'measured']);
      out.push(['相鄰樣本間隔 = 6 ms', `${six} / ${dts.length}（${((six / dts.length) * 100).toFixed(1)}%）`, 'measured']);
      const a = c.roll_angle_deg;
      out.push(['Roll 角範圍 / RMS', `${Math.min(...a).toFixed(1)} ~ ${Math.max(...a).toFixed(1)}° / ${rms(a).toFixed(2)}°`, 'measured']);
      out.push(['角速度 RMS / 誤差 RMS', `${rms(c.gyro_x_deg_s).toFixed(1)} / ${rms(c.gyro_x_error_deg_s).toFixed(1)} °/s`, 'measured']);
      const fit = lsSlope(a, c.gyro_x_target_deg_s);
      out.push(['外環 Kp 推算（目標 ≈ −k·角度）', `${(-fit.k).toFixed(2)}（截距 ${fit.c.toFixed(2)}）`, 'derived']);
      if (c.gyro_x_p_term) {
        const r = []; for (let i = 0; i < d.n; i++) if (Math.abs(c.gyro_x_error_deg_s[i]) > 5) r.push(c.gyro_x_p_term[i] / c.gyro_x_error_deg_s[i]);
        out.push(['內環 Kp 推算（P 項 / 誤差 中位數）', median(r).toFixed(2), 'derived']);
      }
      const sp = spectrum(seq, c.gyro_x_deg_s, 0.006);
      const pk = peakIn(sp, 2, 10), pkAll = peakIn(sp, 0.3, 30);
      out.push(['角速度頻譜峰值（2–10 Hz）', pk ? `${pk.f.toFixed(2)} Hz` : '—', 'derived']);
      out.push(['角速度頻譜峰值（全頻段）', pkAll ? `${pkAll.f.toFixed(2)} Hz` : '—', 'derived']);
      return { rows: out, sp };
    }
    const r = []; for (let i = 0; i < d.n; i++) if (Math.abs(c.pid_error_x_deg_s[i]) > 5) r.push(c.pid_output_x[i] / c.pid_error_x_deg_s[i]);
    out.push(['記錄長度（電腦時間）', `${(c.host_elapsed_s[d.n - 1] - c.host_elapsed_s[0]).toFixed(2)} s`, 'measured']);
    out.push(['角速度 RMS / 誤差 RMS', `${rms(c.gyro_x_deg_s).toFixed(1)} / ${rms(c.pid_error_x_deg_s).toFixed(1)} °/s`, 'measured']);
    out.push(['輸出 / 誤差 中位數', median(r).toFixed(2), 'derived']);
    return { rows: out, sp: null };
  }

  // ---------- 介面 ----------
  function init() {
    const host = $('viewer-plots');
    if (!host) return;
    const P = window.Plot;
    const sel = $('run-select');
    RUNS.forEach((r) => { const o = document.createElement('option'); o.value = r.id; o.textContent = r.label; sel.appendChild(o); });
    const cs = getComputedStyle(document.documentElement);
    const C = (i) => cs.getPropertyValue(`--series-${i}`).trim();
    let group = null, plots = [], current = null, spPlot = null, spGroup = null;

    function buildPlots(run) {
      host.innerHTML = '';
      const xopt = { xName: 't', xLabel: run.legacy ? 's（電腦）' : 's（飛控）', xFormat: (v) => v.toFixed(3) + ' s' };
      if (run.legacy) {
        plots = [
          new P.LinePlot(host, Object.assign({ title: '角速度', yLabel: '°/s', series: [{ key: 'gyro_x_deg_s', label: '量測角速度', color: C(1) }] }, xopt)),
          new P.LinePlot(host, Object.assign({ title: '角速度誤差', yLabel: '°/s', series: [{ key: 'pid_error_x_deg_s', label: '誤差', color: C(2) }] }, xopt)),
          new P.LinePlot(host, Object.assign({ title: 'PID 輸出', yLabel: 'µs', series: [{ key: 'pid_output_x', label: '輸出', color: C(3) }] }, xopt)),
        ];
      } else {
        const terms = [{ key: 'gyro_x_pid_output', label: '總輸出', color: C(3) }, { key: 'gyro_x_d_term', label: 'D 項', color: C(2) }];
        if (current.d.cols.gyro_x_p_term) terms.unshift({ key: 'gyro_x_p_term', label: 'P 項', color: C(1) });
        plots = [
          new P.LinePlot(host, Object.assign({ title: 'Roll 角', yLabel: '°', height: 150, series: [{ key: 'roll_angle_deg', label: 'roll', color: C(1) }] }, xopt)),
          new P.LinePlot(host, Object.assign({ title: 'X 軸角速度', yLabel: '°/s', series: [{ key: 'gyro_x_target_deg_s', label: '目標（外環輸出）', color: '#8d9bab', dash: true }, { key: 'gyro_x_deg_s', label: '量測', color: C(1) }] }, xopt)),
          new P.LinePlot(host, Object.assign({ title: '角速度誤差', yLabel: '°/s', height: 140, series: [{ key: 'gyro_x_error_deg_s', label: '誤差', color: C(2) }] }, xopt)),
          new P.LinePlot(host, Object.assign({ title: '內環 PID 各項', yLabel: 'µs', series: terms }, xopt)),
        ];
      }
      group = new P.PlotGroup(plots, { onView: syncWindowUi });
    }

    function syncWindowUi(v) {
      if (!current) return;
      const [a, b] = v, [f0, f1] = group.full;
      $('win-info').textContent = `顯示 ${a.toFixed(2)} – ${b.toFixed(2)} s（共 ${(f1 - f0).toFixed(1)} s）`;
      const pos = $('win-pos');
      pos.max = Math.max(0, f1 - f0 - (b - a)).toFixed(2);
      pos.value = (a - f0).toFixed(2);
    }

    let showToken = 0;
    async function show(id) {
      const run = RUNS.find((r) => r.id === id);
      const token = ++showToken;
      $('viewer-status').textContent = `讀取 ${run.file}…`;
      let d;
      try { d = await load(run); if (token !== showToken) return; } // 使用者已切換到別筆記錄
      catch (err) {
        $('viewer-status').innerHTML = `<span style="color:#e0a43a">無法讀取 CSV（${err.message}）。若直接以 file:// 開啟，瀏覽器會阻擋讀檔，請改用本機 HTTP 伺服器（見實驗頁底部指令）。</span>`;
        return;
      }
      current = { run, d };
      buildPlots(run);
      const x = run.legacy ? d.cols.host_elapsed_s : d.cols.flight_elapsed_s;
      group.setData(x, d.cols);
      syncWindowUi(group.full);
      $('viewer-status').innerHTML = `<span class="tag measured">實測</span> 原始檔 <code>${run.orig}</code>（檔案時間 ${run.when}）· <a href="${run.file}" download>下載 CSV</a>`;
      $('run-note').textContent = run.note;
      const ev = $('event-buttons'); ev.innerHTML = '';
      run.events.forEach(([a, b, label]) => { const btn = document.createElement('button'); btn.textContent = '跳到：' + label; btn.addEventListener('click', () => group.setView([a, b])); ev.appendChild(btn); });
      const st = stats(run, d);
      $('run-stats').innerHTML = st.rows.map(([k, v, tag]) => `<tr><td>${k}</td><td class="num">${v}</td><td><span class="tag ${tag}">${tag === 'measured' ? '實測' : '推算'}</span></td></tr>`).join('');
      drawSpectrum(st.sp);
    }

    function drawSpectrum(sp) {
      const box = $('spectrum');
      if (!box) return;
      box.innerHTML = '';
      if (!sp) { box.innerHTML = '<p class="small muted">舊格式資料沒有飛控時間戳，不計算頻譜。</p>'; return; }
      const keep = []; for (let i = 0; i < sp.f.length && sp.f[i] <= 20; i++) keep.push(i);
      const f = Float64Array.from(keep.map((i) => sp.f[i])), m = Float64Array.from(keep.map((i) => sp.m[i]));
      // 陀螺一階低通（α = 0.15、dt = 6 ms）的增益，乘上最大值方便對照形狀
      const g = Float64Array.from(f, (fr) => { const w = 2 * Math.PI * fr * 0.006; const re = 1 - 0.85 * Math.cos(w), im = 0.85 * Math.sin(w); return 0.15 / Math.hypot(re, im); });
      const mx = Math.max(...m);
      const gs = Float64Array.from(g, (v) => v * mx);
      spPlot = new P.LinePlot(box, { title: '量測角速度振幅頻譜', yLabel: '°/s', height: 180, xName: 'f', xLabel: 'Hz', xFormat: (v) => v.toFixed(2) + ' Hz', digits: 3,
        series: [{ key: 'm', label: '頻譜（實測資料計算）', color: C(1) }, { key: 'g', label: '陀螺低通增益形狀（×峰值，推算）', color: '#8d9bab', dash: true }] });
      spGroup = new P.PlotGroup([spPlot]);
      spGroup.setData(f, { m, g: gs });
    }

    sel.addEventListener('change', () => show(sel.value));
    $('win-all').addEventListener('click', () => group && group.reset());
    $('win-len').addEventListener('change', () => {
      if (!group) return;
      const L = +$('win-len').value; const [a] = plots[0].view;
      group.setView(L ? [a, a + L] : group.full);
    });
    $('win-pos').addEventListener('input', () => {
      if (!group) return;
      const [a, b] = plots[0].view; const L = b - a; const s = group.full[0] + +$('win-pos').value;
      group.setView([s, s + L]);
    });
    const params = new URLSearchParams(location.search);
    const initial = RUNS.find((r) => r.id === params.get('run')) ? params.get('run') : RUNS[0].id;
    sel.value = initial;
    show(initial).then(() => { if (initial === 'run-p-2.3' && !params.get('run')) group.setView([10.5, 14.5]); });
  }
  init();
})();
