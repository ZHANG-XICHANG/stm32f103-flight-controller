/* 3D 姿態模型：NWU 座標（X 前、Y 左、Z 上），ZYX（Yaw→Pitch→Roll）旋轉順序，
 * 四元數轉歐拉角使用與 Fusion FusionQuaternionToEuler() 相同的公式。純 canvas 繪製。 */
(function () {
  'use strict';
  const D2R = Math.PI / 180, R2D = 180 / Math.PI;

  // ---------- 四元數工具 ----------
  function quatFromEuler(rollDeg, pitchDeg, yawDeg) {
    const cr = Math.cos(rollDeg * D2R / 2), sr = Math.sin(rollDeg * D2R / 2);
    const cp = Math.cos(pitchDeg * D2R / 2), sp = Math.sin(pitchDeg * D2R / 2);
    const cy = Math.cos(yawDeg * D2R / 2), sy = Math.sin(yawDeg * D2R / 2);
    // q = qz(yaw) ⊗ qy(pitch) ⊗ qx(roll)
    return {
      w: cr * cp * cy + sr * sp * sy,
      x: sr * cp * cy - cr * sp * sy,
      y: cr * sp * cy + sr * cp * sy,
      z: cr * cp * sy - sr * sp * cy,
    };
  }
  // 與 Middlewares/Third_Party/Fusion/FusionMath.h:548-559 相同
  function fusionQuaternionToEuler(q) {
    const asinSafe = (v) => (v <= -1 ? -Math.PI / 2 : v >= 1 ? Math.PI / 2 : Math.asin(v));
    return {
      roll: R2D * Math.atan2(q.y * q.z + q.w * q.x, q.w * q.w + q.z * q.z - 0.5),
      pitch: R2D * asinSafe(2 * (q.w * q.y - q.x * q.z)),
      yaw: R2D * Math.atan2(q.x * q.y + q.w * q.z, q.w * q.w + q.x * q.x - 0.5),
    };
  }
  function quatToMatrix(q) {
    const { w, x, y, z } = q;
    return [
      [1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
      [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
      [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)],
    ];
  }
  const mul = (R, p) => [R[0][0] * p[0] + R[0][1] * p[1] + R[0][2] * p[2], R[1][0] * p[0] + R[1][1] * p[1] + R[1][2] * p[2], R[2][0] * p[0] + R[2][1] * p[1] + R[2][2] * p[2]];
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const norm = (a) => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };

  // ---------- 機體幾何（機體座標） ----------
  const A = 0.72; // 臂長在 X、Y 的分量
  const MOTORS = [
    { name: 'M1', p: [A, A, 0], ccw: true },
    { name: 'M2', p: [A, -A, 0], ccw: false },
    { name: 'M3', p: [-A, -A, 0], ccw: true },
    { name: 'M4', p: [-A, A, 0], ccw: false },
  ];
  const COLORS = { x: '#d95926', y: '#199e70', z: '#3987e5', frame: '#5d6f86', front: '#4cb5ff', ccw: '#3987e5', cw: '#d95926' };

  class Attitude3D {
    constructor(canvas) {
      this.cv = canvas; this.ctx = canvas.getContext('2d');
      this.az = Math.atan2(-2.2, -3.4); this.el = 0.5; this.dist = 4.6;
      this.q = quatFromEuler(0, 0, 0);
      this.bindOrbit();
      new ResizeObserver(() => this.resize()).observe(canvas);
      this.resize();
    }
    resize() {
      const dpr = Math.min(devicePixelRatio || 1, 2);
      const w = this.cv.clientWidth, h = this.cv.clientHeight;
      this.w = w; this.h = h;
      this.cv.width = Math.round(w * dpr); this.cv.height = Math.round(h * dpr);
      this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      this.draw();
    }
    bindOrbit() {
      let last = null;
      this.cv.addEventListener('pointerdown', (e) => { last = [e.clientX, e.clientY]; this.cv.setPointerCapture(e.pointerId); this.cv.style.cursor = 'grabbing'; });
      this.cv.addEventListener('pointermove', (e) => {
        if (!last) return;
        this.az -= (e.clientX - last[0]) * 0.008;
        this.el = Math.max(-0.2, Math.min(1.45, this.el + (e.clientY - last[1]) * 0.006));
        last = [e.clientX, e.clientY]; this.draw();
      });
      const end = () => { last = null; this.cv.style.cursor = 'grab'; };
      this.cv.addEventListener('pointerup', end); this.cv.addEventListener('pointercancel', end);
    }
    setQuat(q) { this.q = q; this.draw(); }

    camera() {
      const c = [this.dist * Math.cos(this.el) * Math.cos(this.az), this.dist * Math.cos(this.el) * Math.sin(this.az), this.dist * Math.sin(this.el)];
      const f = norm(sub([0, 0, 0], c));
      const r = norm(cross(f, [0, 0, 1]));
      const u = cross(r, f);
      return { c, f, r, u };
    }
    project(p, cam) {
      const v = sub(p, cam.c);
      const z = dot(v, cam.f);
      const F = Math.min(this.w, this.h) * 1.25;
      return { x: this.w / 2 + (F * dot(v, cam.r)) / z, y: this.h / 2 - (F * dot(v, cam.u)) / z + this.h * 0.04, z };
    }

    draw() {
      const { ctx, w, h } = this;
      if (!w) return;
      ctx.clearRect(0, 0, w, h);
      const cam = this.camera();
      const R = quatToMatrix(this.q);
      const P = (p) => this.project(p, cam);
      const B = (p) => P(mul(R, p));

      // 地面格線（世界座標，z = -0.75）
      ctx.lineWidth = 1;
      for (let i = -2; i <= 2; i += 0.5) {
        ctx.strokeStyle = i === 0 ? '#2b3a4d' : '#1b2633';
        let a = P([i, -2, -0.75]), b = P([i, 2, -0.75]);
        ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke();
        a = P([-2, i, -0.75]); b = P([2, i, -0.75]);
        ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke();
      }
      // 世界座標方位標示
      ctx.font = '12px ' + getComputedStyle(document.body).fontFamily;
      ctx.fillStyle = '#566579'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      [['N (+X)', [2.25, 0, -0.75]], ['W (+Y)', [0, 2.25, -0.75]]].forEach(([t, p]) => { const s = P(p); ctx.fillText(t, s.x, s.y); });

      // 收集繪製元素，依深度由遠到近畫
      const items = [];
      // 機臂
      MOTORS.forEach((m, i) => {
        const a = B([0, 0, 0]), b = B(m.p);
        items.push({ z: (a.z + b.z) / 2, draw: () => {
          ctx.strokeStyle = i < 2 ? COLORS.front : COLORS.frame; ctx.lineWidth = 7; ctx.lineCap = 'round';
          ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke();
        } });
      });
      // 中心機體（上板）
      const plate = [[0.24, 0.16, 0.04], [0.24, -0.16, 0.04], [-0.24, -0.16, 0.04], [-0.24, 0.16, 0.04]].map(B);
      items.push({ z: plate.reduce((s, p) => s + p.z, 0) / 4 - 0.01, draw: () => {
        ctx.fillStyle = '#1d2a3a'; ctx.strokeStyle = '#4cb5ff'; ctx.lineWidth = 1.5;
        ctx.beginPath(); plate.forEach((p, k) => (k ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y))); ctx.closePath(); ctx.fill(); ctx.stroke();
      } });
      // 槳盤（圓以多邊形近似）與馬達標籤
      MOTORS.forEach((m) => {
        const pts = [];
        for (let k = 0; k < 28; k++) { const t = (k / 28) * Math.PI * 2; pts.push(B([m.p[0] + 0.36 * Math.cos(t), m.p[1] + 0.36 * Math.sin(t), 0.1])); }
        const c = B([m.p[0], m.p[1], 0.1]);
        const col = m.ccw ? COLORS.ccw : COLORS.cw;
        items.push({ z: c.z, draw: () => {
          ctx.fillStyle = col + '2e'; ctx.strokeStyle = col; ctx.lineWidth = 1.6;
          ctx.beginPath(); pts.forEach((p, k) => (k ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y))); ctx.closePath(); ctx.fill(); ctx.stroke();
          ctx.fillStyle = '#e6edf3'; ctx.font = 'bold 13px sans-serif'; ctx.fillText(m.name, c.x, c.y - 1);
          ctx.fillStyle = col; ctx.font = '11px sans-serif'; ctx.fillText(m.ccw ? '↺' : '↻', c.x, c.y + 13);
        } });
      });
      // 機體座標軸
      [['x', [1.25, 0, 0], 'X 前'], ['y', [0, 1.25, 0], 'Y 左'], ['z', [0, 0, 1.0], 'Z 上']].forEach(([k, v, label]) => {
        const a = B([0, 0, 0.05]), b = B(v);
        items.push({ z: Math.min(a.z, b.z) - 0.2, draw: () => {
          ctx.strokeStyle = COLORS[k]; ctx.fillStyle = COLORS[k]; ctx.lineWidth = 2.2;
          ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke();
          ctx.beginPath(); ctx.arc(b.x, b.y, 3.5, 0, Math.PI * 2); ctx.fill();
          ctx.font = 'bold 12px sans-serif'; ctx.fillText(label, b.x + (b.x > a.x ? 16 : -16), b.y - 8);
        } });
      });
      items.sort((a, b) => b.z - a.z).forEach((it) => it.draw());

      ctx.fillStyle = '#566579'; ctx.font = '11px sans-serif'; ctx.textAlign = 'left';
      ctx.fillText('拖曳可旋轉視角', 10, h - 12);
    }
  }

  // ---------- 頁面控制 ----------
  function init() {
    const canvas = document.getElementById('att-canvas');
    if (!canvas) return;
    const view = new Attitude3D(canvas);
    const ids = ['roll', 'pitch', 'yaw'];
    const sliders = Object.fromEntries(ids.map((k) => [k, document.getElementById('att-' + k)]));
    const outs = Object.fromEntries(ids.map((k) => [k, document.getElementById('att-' + k + '-out')]));
    const readout = document.getElementById('att-readout');
    const status = document.getElementById('att-status');
    const f = (v, d = 4) => (v >= 0 ? ' ' : '') + v.toFixed(d);

    function update() {
      const e = Object.fromEntries(ids.map((k) => [k, Number(sliders[k].value)]));
      ids.forEach((k) => (outs[k].value = e[k].toFixed(1) + '°'));
      const q = quatFromEuler(e.roll, e.pitch, e.yaw);
      view.setQuat(q);
      const back = fusionQuaternionToEuler(q);
      const R = quatToMatrix(q);
      const gimbal = Math.abs(e.pitch) > 85;
      readout.innerHTML =
        `<span class="k">四元數 q</span>  w=${f(q.w)}  x=${f(q.x)}  y=${f(q.y)}  z=${f(q.z)}\n`.replace(/\n/g, '<br>') +
        `<span class="k">旋轉矩陣 R（機體→世界）</span><br>` +
        R.map((r) => '[' + r.map((v) => f(v, 3)).join(' ') + ' ]').join('<br>') + '<br>' +
        `<span class="k">Fusion 公式反算</span>  roll=${back.roll.toFixed(2)}°  pitch=${back.pitch.toFixed(2)}°  yaw=${back.yaw.toFixed(2)}°` +
        (gimbal ? '<br><span style="color:#e0a43a">|pitch| 接近 90°：萬向鎖，roll 與 yaw 無法唯一分離</span>' : '');
    }
    ids.forEach((k) => sliders[k].addEventListener('input', () => { stopPlay(); update(); }));
    document.getElementById('att-reset').addEventListener('click', () => { stopPlay(); ids.forEach((k) => (sliders[k].value = 0)); update(); });
    document.querySelectorAll('[data-att-preset]').forEach((b) => b.addEventListener('click', () => {
      stopPlay();
      const [r, p, y] = b.dataset.attPreset.split(',').map(Number);
      sliders.roll.value = r; sliders.pitch.value = p; sliders.yaw.value = y; update();
    }));

    // 播放實測 Roll（CSV 只記錄所選軸，這份資料是 X 軸 → roll）
    const playBtn = document.getElementById('att-play');
    let timer = 0, data = null;
    function stopPlay() { if (timer) { cancelAnimationFrame(timer); timer = 0; playBtn.setAttribute('aria-pressed', 'false'); playBtn.textContent = '▶ 播放實測 Roll'; } }
    async function loadRun() {
      if (data) return data;
      status.textContent = '讀取 CSV…';
      const res = await fetch(playBtn.dataset.csv);
      if (!res.ok) throw new Error('HTTP ' + res.status);
      const lines = (await res.text()).trim().split(/\r?\n/);
      const head = lines[0].split(',');
      const it = head.indexOf('flight_elapsed_s'), ia = head.indexOf('roll_angle_deg'), is = head.indexOf('sequence');
      const t = [], a = []; let lastSeq = -1;
      for (let i = 1; i < lines.length; i++) {
        const c = lines[i].split(',');
        const s = +c[is]; if (s === lastSeq) continue; lastSeq = s;
        t.push(+c[it]); a.push(+c[ia]);
      }
      data = { t, a };
      return data;
    }
    let loading = false;
    playBtn.addEventListener('click', async () => {
      if (timer) { stopPlay(); return; }
      if (loading) return;
      loading = true;
      try { await loadRun(); } catch (err) { status.textContent = '無法讀取 CSV（請用 HTTP 伺服器開啟本站）：' + err.message; return; } finally { loading = false; }
      status.textContent = '';
      const start0 = Number(playBtn.dataset.start || 0), end0 = Number(playBtn.dataset.end || data.t[data.t.length - 1]);
      sliders.pitch.value = 0; sliders.yaw.value = 0;
      playBtn.setAttribute('aria-pressed', 'true'); playBtn.textContent = '■ 停止';
      const t0 = performance.now();
      let i = 0;
      const step = (now) => {
        const tt = start0 + (now - t0) / 1000;
        if (tt > end0) { stopPlay(); status.textContent = '播放結束。'; return; }
        while (i < data.t.length - 1 && data.t[i + 1] <= tt) i++;
        sliders.roll.value = Math.max(-90, Math.min(90, data.a[i]));
        update();
        status.innerHTML = `<span class="tag measured">實測</span> run-p-2.3.csv　t = ${data.t[i].toFixed(3)} s　roll = ${data.a[i].toFixed(2)}°（pitch、yaw 未記錄，固定為 0）`;
        timer = requestAnimationFrame(step);
      };
      timer = requestAnimationFrame(step);
    });
    update();
  }
  window.AttitudeMath = { quatFromEuler, fusionQuaternionToEuler, quatToMatrix };
  init();
})();
