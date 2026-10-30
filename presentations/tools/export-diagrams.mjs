// 將 docs/architecture/ 的 Archify 互動圖匯出成簡報用 PNG（深色主題、無標題列、簡報底色）。
// usage: node presentations/tools/export-diagrams.mjs
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');
const OUT = path.join(ROOT, 'presentations/assets/diagrams');
const CHROME = 'C:/Program Files/Google/Chrome/Application/chrome.exe';
const PORT = 9348;
const BG = '#0B1220';
const WIDTH = 2400;
const DIAGRAMS = [
  ['01-system-architecture', 'system-architecture'],
  ['02-freertos-dataflow', 'freertos-dataflow'],
  ['03-cascade-pid', 'cascade-pid'],
  ['04a-ack-sequence-control', 'ack-sequence-control'],
  ['04b-ack-sequence-tuning', 'ack-sequence-tuning'],
];

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

// 在頁面內執行：固定深色主題、結束動畫，把計算後的樣式寫回 SVG 使其可獨立顯示。
const extractSvg = `(async () => {
  document.documentElement.setAttribute('data-theme', 'dark');
  try { localStorage.setItem('archify-theme', 'dark'); } catch {}
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
  return { svg: new XMLSerializer().serializeToString(clone), viewBox: src.getAttribute('viewBox') };
})()`;

fs.mkdirSync(OUT, { recursive: true });
for (const [src, name] of DIAGRAMS) {
  await send('Emulation.setDeviceMetricsOverride', { width: 1600, height: 1000, deviceScaleFactor: 1, mobile: false });
  await load(pathToFileURL(path.join(ROOT, 'docs/architecture', `${src}.html`)).href);
  const { svg, viewBox } = await evaluate(extractSvg);
  const [, , vw, vh] = viewBox.split(/\s+/).map(Number);
  const h = Math.round(WIDTH * vh / vw);
  const inner = svg.replace(/^<svg\b[^>]*>/, '').replace(/<\/svg>\s*$/, '');
  const out = `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="${WIDTH}" height="${h}" viewBox="${viewBox}">
<rect x="${viewBox.split(/\s+/)[0]}" y="${viewBox.split(/\s+/)[1]}" width="${vw}" height="${vh}" fill="${BG}"/>${inner}</svg>`;
  const tmp = path.join(profile, `${name}.svg`);
  fs.writeFileSync(tmp, out);
  await send('Emulation.setDeviceMetricsOverride', { width: WIDTH, height: h, deviceScaleFactor: 1, mobile: false });
  await load(pathToFileURL(tmp).href);
  const shot = await send('Page.captureScreenshot', { format: 'png', clip: { x: 0, y: 0, width: WIDTH, height: h, scale: 1 } });
  fs.writeFileSync(path.join(OUT, `${name}.png`), Buffer.from(shot.data, 'base64'));
  console.log('exported', name, `${vw}x${vh}`);
}

ws.close();
chrome.kill();
