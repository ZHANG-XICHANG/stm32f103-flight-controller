/* 控制頁：互動 PID 方塊圖、混控計算器、串級 PID 示意模擬。 */
(function () {
  'use strict';

  // ---------------- 1. 方塊圖說明 ----------------
  const BLOCKS = {
    stick: { t: '遙控搖桿（remoteData）', b: '<p>通訊任務解碼 17 B 控制封包，Roll / Pitch / Yaw / 油門各為 0–1000，置中 500。</p>', s: ['Core/Src/receive.c:129-134'] },
    sp: { t: '設定值換算', b: '<p><code>desire = (x − 500) × 0.02</code>，搖桿全行程對應 <b>±10°</b>。直接當作角度外環的目標，沒有斜率限制或濾波。</p>', s: ['Core/Src/flight.c:273-285'] },
    sum1: { t: '角度誤差', b: '<p><code>error = desire − measure</code>，measure 為 Fusion 輸出的 roll / pitch 角。</p>', s: ['Core/Src/com_pid.c:9'] },
    outer: { t: '角度外環 PID（roll_pid / pitch_pid）', b: '<p>Kp 7、Ki 0、Kd 0.05；D 項為<b>誤差</b>微分且不濾波。輸出單位為 °/s，直接成為內環 desire。</p><p>原始碼預設值與 README 記載的實飛參數一致。</p>', s: ['Core/Src/flight.c:34-56', 'Core/Src/com_pid.c:55-67'] },
    sum2: { t: '角速度誤差', b: '<p><code>error = 外環輸出 − body_rate</code>（經低通與零偏修正的陀螺值）。</p>', s: ['Core/Src/flight.c:278', 'Core/Src/flight.c:288'] },
    inner: { t: '角速度內環 PID（gyro_x_pid / gyro_y_pid）', b: '<p>Kp 0.5、Ki 0、Kd 0.01；<code>d_on_measurement = 1</code>：D 取 <code>−d(measure)/dt</code>，再經 10 Hz 一階低通（α ≈ 0.726）。</p><p>輸出單位為 PWM 修正量（µs），沒有輸出限幅與積分抗飽和。</p>', s: ['Core/Src/flight.c:40-65', 'Core/Src/com_pid.c:6-52'] },
    mixer: { t: 'X 型混控 + 限幅', b: '<p>四顆馬達 = 基礎脈寬 ± pitch ± roll ± yaw，各自限制在 1100–1940 µs。公式見下方計算器。</p>', s: ['Core/Src/flight.c:383-429'] },
    gate: { t: '油門與飛行狀態閘控', b: '<p>只有 <b>NORMAL</b> 且油門 ≥ 10 才混控；IDLE、FAIL、FIX_HEIGHT 或低油門一律輸出 1100 µs。基礎脈寬 = 1100 + thr × 840 / 1000。</p>', s: ['Core/Src/flight.c:117-130', 'Core/Src/flight.c:310-332'] },
    esc: { t: 'TIM4 → ESC', b: '<p>CCR1–4 = 脈寬 µs；PWM 400 Hz（2.5 ms），1 µs 解析度。</p>', s: ['Core/Src/ESC.c:61-71', 'Core/Src/tim.c:45-47'] },
    plant: { t: '機體動態（物理）', b: '<p>推力差產生力矩 → 角加速度 → 角速度 → 角度。<span class="tag missing">待補</span> 轉動慣量、馬達時間常數等參數尚未量測或識別。</p>', s: [] },
    mpu: { t: 'MPU6050', b: '<p>±2000 °/s、±2 g；每 6 ms 由飛控任務以 I2C 讀取一次並扣除開機零偏。</p>', s: ['Core/Src/MPU6050.c:131-175'] },
    pre: { t: '陀螺低通 + FusionBias', b: '<p>一階低通 α = 0.15（<span class="tag derived">推算</span> 截止約 4.3 Hz、4 Hz 時相位落後約 39°），再由 FusionBias 在靜止時追蹤零偏，得到 <code>body_rate</code>。</p>', s: ['Core/Src/flight.c:204-247', 'Core/Src/com_filter.c:13-15'] },
    ahrs: { t: 'Fusion AHRS', b: '<p>四元數積分 + 加速度傾角修正（gain 0.5），轉成歐拉角後回授給外環。</p>', s: ['Core/Src/flight.c:249-260'] },
    yawsp: { t: 'Yaw 設定值', b: '<p><code>(x − 500) / 500 × 90</code>：搖桿對應 <b>±90 °/s</b> 角速度目標（不是角度）。</p>', s: ['Core/Src/flight.c:292-297'] },
    sum3: { t: 'Yaw 角速度誤差', b: '<p><code>error = 目標 − body_rate.z</code>。</p>', s: ['Core/Src/flight.c:296'] },
    gyroz: { t: 'Yaw 角速度 PI（gyro_z_pid）', b: '<p>Kp 2、Ki 0.1、Kd 0。單環控制，沒有航向角外環；<code>yaw_pid</code> 雖宣告且可線上調參，但未進入控制路徑。</p><p>積分項沒有限幅，長時間偏差會持續累積。</p>', s: ['Core/Src/flight.c:67-81', 'Core/Src/flight.c:297'] },
  };

  function initBlocks() {
    const svg = document.getElementById('pid-diagram');
    const panel = document.getElementById('block-detail');
    if (!svg || !panel) return;
    const show = (id) => {
      const d = BLOCKS[id]; if (!d) return;
      svg.querySelectorAll('.blk').forEach((g) => g.classList.toggle('selected', g.dataset.block === id));
      panel.innerHTML = `<h4>${d.t}</h4>${d.b}` + (d.s.length ? `<div class="sources"><span>依據</span>${d.s.map((s) => `<span class="src" data-src="${s}"></span>`).join('')}</div>` : '');
      if (window.linkSources) window.linkSources(panel);
    };
    svg.querySelectorAll('.blk').forEach((g) => {
      g.setAttribute('tabindex', '0');
      g.setAttribute('role', 'button');
      g.addEventListener('click', () => show(g.dataset.block));
      g.addEventListener('keydown', (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); show(g.dataset.block); } });
    });
    show('inner');
  }

  // ---------------- 2. 混控計算器（重現 flight.c 的整數運算） ----------------
  function initMixer() {
    const root = document.getElementById('mixer');
    if (!root) return;
    const $ = (id) => document.getElementById(id);
    const inputs = ['mx-thr', 'mx-roll', 'mx-pitch', 'mx-yaw'].map($);
    const state = $('mx-state');
    const MIN = 1100, MAX = 1940;
    const trunc = (v) => (v < 0 ? Math.ceil(v) : Math.floor(v));
    function calc() {
      const thr = +inputs[0].value, r = +inputs[1].value, p = +inputs[2].value, y = +inputs[3].value;
      inputs.forEach((el) => { $(el.id + '-out').value = el.value; });
      let raw, out, note = '';
      if (state.value !== 'NORMAL') {
        raw = [MIN, MIN, MIN, MIN]; out = raw; note = `${state.value} 狀態：Flight_StopAllMotors()，四顆皆輸出 1100 µs。`;
      } else if (thr < 10) {
        raw = [MIN, MIN, MIN, MIN]; out = raw; note = '油門 < 10：直接停機（1100 µs）。';
      } else {
        const base = MIN + trunc((Math.min(thr, 1000) * (MAX - MIN)) / 1000);
        raw = [base - p - r + y, base - p + r - y, base + p + r + y, base + p - r - y];
        out = raw.map((v) => Math.max(MIN, Math.min(MAX, v)));
        note = `基礎脈寬 = 1100 + ${thr} × 840 / 1000 = <b>${base} µs</b>`;
        const clipped = raw.filter((v, i) => v !== out[i]).length;
        if (clipped) note += `；<span style="color:#e0a43a">${clipped} 顆被限幅，實際力矩比 PID 要求的小（無飽和回饋給 PID）。</span>`;
      }
      $('mx-note').innerHTML = note;
      out.forEach((v, i) => {
        const bar = $('mx-bar-' + i), lab = $('mx-val-' + i);
        bar.style.width = (((v - 1000) / 1000) * 100).toFixed(1) + '%';
        bar.style.background = raw[i] !== v ? '#e0a43a' : 'var(--series-' + (i + 1) + ')';
        lab.textContent = v + ' µs' + (raw[i] !== v ? `（原 ${raw[i]}）` : '');
      });
    }
    inputs.concat([state]).forEach((el) => el.addEventListener('input', calc));
    document.querySelectorAll('[data-mx]').forEach((b) => b.addEventListener('click', () => {
      const [t, r, p, y] = b.dataset.mx.split(',');
      inputs[0].value = t; inputs[1].value = r; inputs[2].value = p; inputs[3].value = y; state.value = 'NORMAL'; calc();
    }));
    calc();
  }

  // ---------------- 3. 串級 PID 示意模擬 ----------------
  // 控制器部分逐行對照 com_pid.c；機體與致動器為「假設」的二階 + 一階模型。
  function makePid(Kp, Ki, Kd, dOnMeas, cutoff, dt) {
    return { Kp, Ki, Kd, dOnMeas, cutoff, dt, integral: 0, derivative: 0, prevE: 0, prevM: 0, init: false, desire: 0, measure: 0, output: 0 };
  }
  function pidCalc(p) {
    const e = p.desire - p.measure;
    p.integral += e * p.dt;
    if (!p.init) { p.derivative = 0; p.init = true; }
    else {
      const raw = p.dOnMeas ? -(p.measure - p.prevM) / p.dt : (e - p.prevE) / p.dt;
      if (p.cutoff > 0) { const a = 1 / (1 + 2 * Math.PI * p.cutoff * p.dt); p.derivative = a * p.derivative + (1 - a) * raw; }
      else p.derivative = raw;
    }
    p.output = p.Kp * e + p.Ki * p.integral + p.Kd * p.derivative;
    p.prevE = e; p.prevM = p.measure;
    return p;
  }
  function simulate(cfg) {
    const dt = 0.006, sub = 12, h = dt / sub, T = cfg.T;
    const outer = makePid(cfg.kpo, 0, cfg.kdo, false, 0, dt);
    const inner = makePid(cfg.kpi, cfg.kii, cfg.kdi, true, 10, dt);
    let th = 0, w = 0, uf = 0, wf = 0, thHat = 0;
    const n = Math.floor(T / dt);
    const out = { t: new Float64Array(n), sp: new Float64Array(n), th: new Float64Array(n), thHat: new Float64Array(n), rt: new Float64Array(n), w: new Float64Array(n), wf: new Float64Array(n), u: new Float64Array(n) };
    let diverged = false;
    for (let k = 0; k < n; k++) {
      const t = k * dt;
      const sp = cfg.scenario === 'step' && t >= 0.5 ? 5 : 0;
      outer.desire = sp; outer.measure = thHat; pidCalc(outer);
      inner.desire = outer.output; inner.measure = wf; pidCalc(inner);
      const u = Math.max(-cfg.umax, Math.min(cfg.umax, inner.output)); // 混控限幅的近似
      for (let s = 0; s < sub; s++) {
        uf += ((u - uf) * h) / cfg.tau;
        const ts = t + s * h;
        const dist = cfg.scenario === 'dist' && ts >= 0.5 && ts < 0.6 ? 600 : 0;
        w += (cfg.b * uf + dist) * h;
        th += w * h;
      }
      wf = cfg.lpf ? 0.15 * w + 0.85 * wf : w;
      thHat += (wf + 0.5 * (th - thHat)) * dt; // AHRS 以一階互補濾波近似
      if (!isFinite(th) || Math.abs(th) > 180) diverged = true;
      const clip = (v, m) => Math.max(-m, Math.min(m, v));
      out.t[k] = t; out.sp[k] = sp; out.th[k] = clip(th, 90); out.thHat[k] = clip(thHat, 90);
      out.rt[k] = clip(outer.output, 600); out.w[k] = clip(w, 600); out.wf[k] = clip(wf, 600); out.u[k] = u;
    }
    return { out, diverged };
  }
  function dominantHz(sig, dt, from) {
    const x = Array.from(sig.slice(Math.floor(from / dt)));
    const N = x.length; if (N < 64) return null;
    const mean = x.reduce((a, b) => a + b, 0) / N;
    let best = 0, bestF = null;
    for (let f = 0.5; f <= 20; f += 0.05) {
      let re = 0, im = 0;
      for (let i = 0; i < N; i++) { const v = (x[i] - mean) * (0.5 - 0.5 * Math.cos((2 * Math.PI * i) / (N - 1))); re += v * Math.cos(2 * Math.PI * f * i * dt); im -= v * Math.sin(2 * Math.PI * f * i * dt); }
      const m = re * re + im * im; if (m > best) { best = m; bestF = f; }
    }
    return bestF;
  }

  function initSim() {
    const host = document.getElementById('sim-plots');
    if (!host) return;
    const $ = (id) => document.getElementById(id);
    const P = window.Plot;
    const plots = [
      new P.LinePlot(host, { title: '角度', yLabel: '°', height: 170, xName: 't', xLabel: 's', series: [
        { key: 'sp', label: '設定值', color: '#8d9bab', dash: true },
        { key: 'th', label: '真實角度（模型）', color: 'var(--series-1)' },
        { key: 'thHat', label: 'AHRS 估計（近似）', color: '#d95926' }] }),
      new P.LinePlot(host, { title: '角速度', yLabel: '°/s', height: 170, xName: 't', xLabel: 's', series: [
        { key: 'rt', label: '外環輸出＝內環目標', color: '#8d9bab', dash: true },
        { key: 'w', label: '真實角速度', color: '#3987e5' },
        { key: 'wf', label: '低通後（回授值）', color: '#199e70' }] }),
      new P.LinePlot(host, { title: '內環輸出（修正量）', yLabel: 'µs', height: 140, xName: 't', xLabel: 's', series: [
        { key: 'u', label: '修正量', color: '#c98500' }] }),
    ];
    // CSS 變數不能直接用在 canvas，先解析
    const cs = getComputedStyle(document.documentElement);
    plots.forEach((p) => p.series.forEach((s) => { if (s.color.startsWith('var(')) s.color = cs.getPropertyValue(s.color.slice(4, -1)).trim(); }));
    const group = new P.PlotGroup(plots);

    const ids = ['sim-kpo', 'sim-kdo', 'sim-kpi', 'sim-kii', 'sim-kdi', 'sim-b', 'sim-tau'];
    const DEFAULTS = { 'sim-kpo': 7, 'sim-kdo': 0.05, 'sim-kpi': 0.5, 'sim-kii': 0, 'sim-kdi': 0.01, 'sim-b': 40, 'sim-tau': 0.04 };
    function run() {
      ids.forEach((id) => { $(id + '-out').value = (+$(id).value).toString(); });
      const cfg = {
        kpo: +$('sim-kpo').value, kdo: +$('sim-kdo').value, kpi: +$('sim-kpi').value, kii: +$('sim-kii').value, kdi: +$('sim-kdi').value,
        b: +$('sim-b').value, tau: +$('sim-tau').value, lpf: $('sim-lpf').checked, scenario: $('sim-scenario').value, umax: 400, T: 4,
      };
      const { out, diverged } = simulate(cfg);
      group.setData(out.t, out);
      const hz = dominantHz(out.w, 0.006, 1.2);
      const tail = Array.from(out.w.slice(-150));
      const amp = Math.max(...tail) - Math.min(...tail);
      let verdict;
      if (diverged) verdict = '<b style="color:#e66767">發散</b>（角度超出 ±180°，圖中已截斷）';
      else if (amp > 5) verdict = `<b style="color:#e0a43a">持續振盪</b>，主頻約 ${hz ? hz.toFixed(1) : '—'} Hz（末 0.9 s 角速度峰對峰 ${amp.toFixed(1)} °/s）`;
      else verdict = `<b style="color:#3fbf8a">收斂</b>${hz && amp > 0.2 ? `，殘餘振盪約 ${hz.toFixed(1)} Hz` : ''}`;
      $('sim-verdict').innerHTML = '模擬結果：' + verdict;
    }
    ids.concat(['sim-lpf', 'sim-scenario']).forEach((id) => $(id).addEventListener('input', run));
    $('sim-reset').addEventListener('click', () => { ids.forEach((id) => ($(id).value = DEFAULTS[id])); $('sim-lpf').checked = true; run(); });
    document.querySelectorAll('[data-sim]').forEach((b) => b.addEventListener('click', () => {
      const o = JSON.parse(b.dataset.sim);
      ids.forEach((id) => ($(id).value = DEFAULTS[id]));
      Object.entries(o).forEach(([k, v]) => { if (k === 'lpf') $('sim-lpf').checked = v; else $(k).value = v; });
      run();
    }));
    run();
  }

  initBlocks();
  initMixer();
  initSim();
})();
