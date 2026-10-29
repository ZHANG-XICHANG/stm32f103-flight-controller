// Export Archify HTML diagrams to 16:9 slide SVG + high-res PNG via Chrome CDP.
// usage: node export-slides.mjs <jobs.json>
// jobs.json: { outDir, theme, slides: [{ html, name, title, subtitle, footer }] }
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const CHROME = 'C:/Program Files/Google/Chrome/Application/chrome.exe';
const PORT = 9347;
const W = 1920, H = 1080, SCALE = 2;

const jobs = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'cdp-'));
const chrome = spawn(CHROME, [
  '--headless=new', `--remote-debugging-port=${PORT}`, `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--hide-scrollbars', 'about:blank',
], { stdio: 'ignore' });

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function wsUrl() {
  for (let i = 0; i < 100; i++) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
      const page = list.find((t) => t.type === 'page');
      if (page) return page.webSocketDebuggerUrl;
    } catch {}
    await sleep(200);
  }
  throw new Error('chrome did not start');
}

const ws = new WebSocket(await wsUrl());
await new Promise((r) => ws.addEventListener('open', r, { once: true }));
let nextId = 1;
const pending = new Map();
const events = [];
ws.addEventListener('message', (m) => {
  const msg = JSON.parse(m.data);
  if (msg.id && pending.has(msg.id)) {
    const { resolve, reject } = pending.get(msg.id);
    pending.delete(msg.id);
    msg.error ? reject(new Error(JSON.stringify(msg.error))) : resolve(msg.result);
  } else if (msg.method) events.push(msg);
});
const send = (method, params = {}) => new Promise((resolve, reject) => {
  const id = nextId++;
  pending.set(id, { resolve, reject });
  ws.send(JSON.stringify({ id, method, params }));
});
const evaluate = async (expression) => {
  const r = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error(JSON.stringify(r.exceptionDetails).slice(0, 800));
  return r.result.value;
};
async function load(url) {
  events.length = 0;
  await send('Page.navigate', { url });
  for (let i = 0; i < 150 && !events.some((e) => e.method === 'Page.loadEventFired'); i++) await sleep(100);
  await sleep(800);
}

await send('Page.enable');
await send('Runtime.enable');
await send('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });

// Runs in the page: freeze theme, inline computed paint so the SVG is self-contained.
const extractSvg = (theme) => `(async () => {
  document.documentElement.setAttribute('data-theme', ${JSON.stringify(theme)});
  try { localStorage.setItem('archify-theme', ${JSON.stringify(theme)}); } catch {}
  document.querySelectorAll('*').forEach((el) => { el.getAnimations?.().forEach((a) => a.finish?.()); });
  await new Promise((r) => setTimeout(r, 400));
  const src = document.querySelector('svg[data-reader-fit], .diagram-container svg');
  const props = ['fill','fill-opacity','stroke','stroke-width','stroke-opacity','stroke-dasharray',
    'stroke-linecap','stroke-linejoin','opacity','font-family','font-size','font-weight','font-style',
    'text-anchor','dominant-baseline','letter-spacing','paint-order','marker-start','marker-end','marker-mid',
    'visibility','rx','ry'];
  const clone = src.cloneNode(true);
  const a = [src, ...src.querySelectorAll('*')];
  const b = [clone, ...clone.querySelectorAll('*')];
  const drop = [];
  a.forEach((el, i) => {
    const cs = getComputedStyle(el);
    const c = b[i];
    if (cs.display === 'none' || cs.visibility === 'hidden' || el.closest('.no-print') || el.matches('[data-ui], foreignObject')) { drop.push(c); return; }
    const style = props.map((p) => {
      let v = cs.getPropertyValue(p);
      if (!v) return '';
      if (p === 'font-family') v = v + ", 'Microsoft JhengHei', 'Noto Sans TC', sans-serif";
      return p + ':' + v;
    }).filter(Boolean).join(';');
    c.setAttribute('style', style);
    c.removeAttribute('class');
  });
  drop.forEach((n) => n.remove());
  const bg = getComputedStyle(document.body).backgroundColor;
  const ink = getComputedStyle(document.body).color;
  return { svg: new XMLSerializer().serializeToString(clone), viewBox: src.getAttribute('viewBox'), bg, ink };
})()`;

const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
fs.mkdirSync(jobs.outDir, { recursive: true });

for (const slide of jobs.slides) {
  await send('Emulation.setDeviceMetricsOverride', { width: 1600, height: 1000, deviceScaleFactor: 1, mobile: false });
  await load(pathToFileURL(path.resolve(slide.html)).href);
  const { svg, viewBox, bg, ink } = await evaluate(extractSvg(jobs.theme));
  const [, , vw, vh] = viewBox.split(/\s+/).map(Number);

  // 16:9 frame: title band, diagram area, footer with code evidence.
  const top = 130, bottom = 70, side = 60;
  const areaW = W - 2 * side, areaH = H - top - bottom;
  const s = Math.min(areaW / vw, areaH / vh);
  const dw = vw * s, dh = vh * s;
  const dx = (W - dw) / 2, dy = top + (areaH - dh) / 2;
  const inner = svg.replace(/^<svg\b[^>]*>/, '').replace(/<\/svg>\s*$/, '');
  const font = "'Microsoft JhengHei','Noto Sans TC','PingFang TC',sans-serif";
  const accent = '#2563eb';
  const out = `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">
<rect width="${W}" height="${H}" fill="${bg}"/>
<rect x="${side}" y="44" width="8" height="52" rx="2" fill="${accent}"/>
<text x="${side + 24}" y="82" font-family="${font}" font-size="40" font-weight="700" fill="${ink}">${esc(slide.title)}</text>
<text x="${W - side}" y="82" font-family="${font}" font-size="20" text-anchor="end" fill="${ink}" fill-opacity="0.62">${esc(slide.subtitle)}</text>
<line x1="${side}" y1="114" x2="${W - side}" y2="114" stroke="${ink}" stroke-opacity="0.18" stroke-width="1.5"/>
<svg x="${dx.toFixed(1)}" y="${dy.toFixed(1)}" width="${dw.toFixed(1)}" height="${dh.toFixed(1)}" viewBox="${viewBox}" preserveAspectRatio="xMidYMid meet">${inner}</svg>
<line x1="${side}" y1="${H - 56}" x2="${W - side}" y2="${H - 56}" stroke="${ink}" stroke-opacity="0.18" stroke-width="1"/>
<text x="${side}" y="${H - 24}" font-family="${font}" font-size="17" fill="${ink}" fill-opacity="0.62">${esc(slide.footer)}</text>
</svg>`;
  const svgPath = path.join(jobs.outDir, `${slide.name}.svg`);
  fs.writeFileSync(svgPath, out);

  await send('Emulation.setDeviceMetricsOverride', { width: W, height: H, deviceScaleFactor: SCALE, mobile: false });
  await load(pathToFileURL(path.resolve(svgPath)).href);
  const shot = await send('Page.captureScreenshot', { format: 'png', clip: { x: 0, y: 0, width: W, height: H, scale: 1 } });
  fs.writeFileSync(path.join(jobs.outDir, `${slide.name}.png`), Buffer.from(shot.data, 'base64'));
  console.log('exported', slide.name, `${vw}x${vh} -> scale ${s.toFixed(2)}`);
}

ws.close();
chrome.kill();
