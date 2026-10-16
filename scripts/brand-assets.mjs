import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const out = join(root, 'docs', 'assets', 'brand');
mkdirSync(join(out, 'ru'), { recursive: true });
mkdirSync(join(out, 'icons'), { recursive: true });

const T = {
  base: '#070709', text: '#f4f4f5', text2: '#a1a1aa', muted: '#71717a', faint: '#52525b',
  accent: '#ff4256', success: '#3ddc97', sheet: '#111114',
};
const FONT = `'Segoe UI Variable Display','Segoe UI',Inter,'Helvetica Neue',Arial,sans-serif`;
const MONO = `'Cascadia Mono',Consolas,'SFMono-Regular',Menlo,monospace`;
const f = (n) => +n.toFixed(2);

function logo(id, x, y, size) {
  const s = size / 100;
  const arrow = (ax, top, bottom, up, grad) => {
    const tip = up ? top : bottom, tail = up ? bottom : top, h = 13, d = up ? h : -h;
    return `<path d="M${ax} ${tail}L${ax} ${tip}M${ax - h} ${tip + d}L${ax} ${tip}L${ax + h} ${tip + d}" fill="none" stroke="url(#${id}${grad})" stroke-width="9.5" stroke-linecap="round" stroke-linejoin="round"/>`;
  };
  return `<g transform="translate(${x} ${y}) scale(${f(s)})">
  <defs>
    <linearGradient id="${id}b" x1="0" y1="3" x2="0" y2="97" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#222228"/><stop offset="1" stop-color="#0a0a0c"/></linearGradient>
    <radialGradient id="${id}g" cx="24" cy="92" r="70" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#ff4256" stop-opacity=".47"/><stop offset="1" stop-color="#ff4256" stop-opacity="0"/></radialGradient>
    <linearGradient id="${id}r" x1="0" y1="3" x2="0" y2="97" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#fff" stop-opacity=".27"/><stop offset=".5" stop-color="#fff" stop-opacity=".055"/><stop offset="1" stop-color="#fff" stop-opacity=".1"/></linearGradient>
    <linearGradient id="${id}red" x1="0" y1="24" x2="0" y2="72" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#ff707e"/><stop offset="1" stop-color="#e4223e"/></linearGradient>
    <linearGradient id="${id}silver" x1="0" y1="28" x2="0" y2="76" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#fff"/><stop offset="1" stop-color="#aaaab4"/></linearGradient>
  </defs>
  <rect x="3" y="3" width="94" height="94" rx="26" fill="url(#${id}b)"/>
  <rect x="3" y="3" width="94" height="94" rx="26" fill="url(#${id}g)"/>
  <rect x="3.8" y="3.8" width="92.4" height="92.4" rx="25.2" fill="none" stroke="url(#${id}r)" stroke-width="1.6"/>
  ${arrow(38, 25, 70, true, 'red')}
  ${arrow(62, 30, 75, false, 'silver')}
</g>`;
}

const svg = (w, h, body, title) =>
  `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" ${title ? `role="img" aria-label="${title}">
<title>${title}</title>` : 'aria-hidden="true">'}
${body}
</svg>
`;

function backdrop(id, w, h, r = 28, glow = 1) {
  return `<defs>
  <clipPath id="${id}clip"><rect width="${w}" height="${h}" rx="${r}"/></clipPath>
  <radialGradient id="${id}h1" cx="0" cy="0" r="${Math.max(w, h) * 0.62}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#ff4256" stop-opacity="${f(0.26 * glow)}"/><stop offset=".55" stop-color="#ff4256" stop-opacity="${f(0.06 * glow)}"/><stop offset="1" stop-color="#ff4256" stop-opacity="0"/></radialGradient>
  <radialGradient id="${id}h2" cx="${w}" cy="${h}" r="${Math.max(w, h) * 0.5}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#c81e3a" stop-opacity="${f(0.2 * glow)}"/><stop offset="1" stop-color="#c81e3a" stop-opacity="0"/></radialGradient>
</defs>
<g clip-path="url(#${id}clip)">
  <rect width="${w}" height="${h}" fill="${T.base}"/>
  <rect width="${w}" height="${h}" fill="url(#${id}h1)"/>
  <rect width="${w}" height="${h}" fill="url(#${id}h2)"/>
</g>
${r ? `<rect x=".5" y=".5" width="${w - 1}" height="${h - 1}" rx="${r - 0.5}" fill="none" stroke="#fff" stroke-opacity=".09"/>` : ''}`;
}

const textWidth = (s, size, weight = 400) =>
  [...s].reduce((a, c) => a + (/[A-ZА-ЯЁ]/.test(c) ? 0.66 : /[a-zа-яё]/.test(c) ? 0.53 : /\s/.test(c) ? 0.27 : 0.55), 0) * size * (weight >= 600 ? 1.04 : 1);

function chip(x, y, label, { accent = false, h = 34, size = 15 } = {}) {
  const w = Math.round(textWidth(label, size, 600) + 30);
  const fill = accent ? `fill="#ff4256" fill-opacity=".15" stroke="#ff4256" stroke-opacity=".45"` : `fill="#fff" fill-opacity=".05" stroke="#fff" stroke-opacity=".14"`;
  return {
    w,
    s: `<rect x="${x + 0.5}" y="${y + 0.5}" width="${w - 1}" height="${h - 1}" rx="${(h - 1) / 2}" ${fill}/>
  <text x="${x + w / 2}" y="${y + h / 2 + size * 0.35}" text-anchor="middle" font-family="${FONT}" font-size="${size}" font-weight="600" fill="${accent ? '#ff7b8a' : T.text}">${label}</text>`,
  };
}

function chips(x, y, labels, opts) {
  let cx = x;
  return labels.map((l, i) => {
    const c = chip(cx, y, l, { ...opts, accent: opts?.accentFirst && i === 0 });
    cx += c.w + 10;
    return c.s;
  }).join('\n  ');
}

function mockWindow(L, mx, my) {
  const mw = 516, mh = 308;
  const bar = (x, y, w, o = 0.13) => `<rect x="${x}" y="${y}" width="${w}" height="7" rx="3.5" fill="#fff" fill-opacity="${o}"/>`;
  const folder = (x, y) => `<path d="M${x} ${y + 2.5}q0-2.5 2.5-2.5h3.7l2 2h7.3q2.5 0 2.5 2.5v7q0 2.5-2.5 2.5h-13q-2.5 0-2.5-2.5z" fill="#ff4256" transform="translate(0 0)"/>`;
  const file = (x, y) => `<path d="M${x + 9} ${y}h-6.5q-2.5 0-2.5 2.5v11q0 2.5 2.5 2.5h9q2.5 0 2.5-2.5v-8.5z" fill="none" stroke="#fff" stroke-opacity=".35" stroke-width="1.3"/>`;
  const rows = (x, y, items) => items.map(([kind, w, size], i) => {
    const ry = y + i * 26;
    return `${kind === 'd' ? folder(x, ry) : file(x + 2, ry - 1)}
    ${bar(x + 28, ry + 4, w, kind === 'd' ? 0.2 : 0.13)}${size ? `\n    ${bar(x + 168, ry + 4, size, 0.07)}` : ''}`;
  }).join('\n    ');
  const pane = (x, label, icon, items) => `<rect x="${x + 0.5}" y="${my + 88.5}" width="235" height="168" rx="12" fill="#fff" fill-opacity=".025" stroke="#fff" stroke-opacity=".07"/>
    ${icon(x + 16, my + 104)}
    <text x="${x + 38}" y="${my + 114}" font-family="${FONT}" font-size="10.5" font-weight="700" letter-spacing=".8" fill="${T.muted}">${label}</text>
    ${rows(x + 16, my + 132, items)}`;
  const computer = (x, y) => `<g transform="translate(${x} ${y}) scale(.55)" fill="none" stroke="${T.muted}" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"><rect x="3.5" y="4.5" width="17" height="11.5" rx="2.5"/><path d="M9 20h6M12 16v4"/></g>`;
  const server = (x, y) => `<g transform="translate(${x} ${y}) scale(.55)" fill="none" stroke="${T.muted}" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="4.5" width="16" height="6.5" rx="2.5"/><rect x="4" y="13" width="16" height="6.5" rx="2.5"/></g>`;
  const tab = (x, w, name, color, active) => `<rect x="${x}" y="${my + 9}" width="${w}" height="24" rx="8" fill="#fff" fill-opacity="${active ? '.08' : '0'}"/>
    <rect x="${x + 8}" y="${my + 15}" width="2.5" height="12" rx="1.25" fill="${color}"/>
    <circle cx="${x + 19}" cy="${my + 21}" r="3" fill="${T.success}"/>
    <text x="${x + 28}" y="${my + 25.5}" font-family="${FONT}" font-size="12" font-weight="${active ? 700 : 500}" fill="${active ? T.text : T.text2}">${name}</text>`;
  const progress = 0.62;
  return { defs: `<defs>
  <linearGradient id="hp" x1="0" x2="1"><stop offset="0" stop-color="#ff5468"/><stop offset="1" stop-color="#e52a45"/></linearGradient>
  <radialGradient id="hmg" cx="${mx}" cy="${my}" r="420" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#ff4256" stop-opacity=".16"/><stop offset="1" stop-color="#ff4256" stop-opacity="0"/></radialGradient>
  <filter id="hs" x="-10%" y="-10%" width="120%" height="130%"><feDropShadow dx="0" dy="18" stdDeviation="22" flood-color="#000" flood-opacity=".55"/></filter>
</defs>`, body: `<g filter="url(#hs)">
  <rect x="${mx}" y="${my}" width="${mw}" height="${mh}" rx="18" fill="${T.sheet}"/>
</g>
<rect x="${mx}" y="${my}" width="${mw}" height="${mh}" rx="18" fill="url(#hmg)"/>
<rect x="${mx + 0.5}" y="${my + 0.5}" width="${mw - 1}" height="${mh - 1}" rx="17.5" fill="none" stroke="#fff" stroke-opacity=".12"/>
<g>
  ${logo('hm', mx + 12, my + 10, 22)}
  ${tab(mx + 44, 100, 'web-prod', '#ff8a3d', true)}
  ${tab(mx + 150, 90, 'Storage', '#ffc53d', false)}
  <rect x="${mx + 352.5}" y="${my + 9.5}" width="151" height="23" rx="8" fill="#fff" fill-opacity=".04" stroke="#fff" stroke-opacity=".1"/>
  <g transform="translate(${mx + 360} ${my + 13}) scale(.62)" fill="none" stroke="${T.muted}" stroke-width="2.4" stroke-linecap="round"><circle cx="10.5" cy="10.5" r="6"/><path d="M15 15l4.5 4.5"/></g>
  ${bar(mx + 382, my + 18, 62, 0.1)}
  <rect x="${mx + 458}" y="${my + 14}" width="38" height="14" rx="4" fill="#fff" fill-opacity=".08"/>
  <text x="${mx + 477}" y="${my + 24.5}" text-anchor="middle" font-family="${FONT}" font-size="9" font-weight="700" fill="${T.text2}">Ctrl K</text>
  <rect x="${mx + 12.5}" y="${my + 44.5}" width="${mw - 25}" height="34" rx="11" fill="#fff" fill-opacity=".03" stroke="#fff" stroke-opacity=".07"/>
  <circle cx="${mx + 29}" cy="${my + 61.5}" r="4" fill="${T.success}"/>
  <text x="${mx + 41}" y="${my + 66}" font-family="${FONT}" font-size="13" font-weight="700" fill="${T.text}">web-prod</text>
  <text x="${mx + 108}" y="${my + 66}" font-family="${FONT}" font-size="12" fill="${T.muted}">SFTP · ${L.ping}</text>
  ${pane(mx + 12, L.local, computer, [['d', 70, 0], ['d', 52, 0], ['f', 92, 28], ['f', 64, 22], ['f', 80, 30]])}
  ${pane(mx + 269, L.server, server, [['d', 56, 0], ['d', 38, 0], ['d', 72, 0], ['f', 88, 26], ['f', 60, 34]])}
  <rect x="${mx + 12.5}" y="${my + 266.5}" width="${mw - 25}" height="30" rx="10" fill="#fff" fill-opacity=".03" stroke="#fff" stroke-opacity=".07"/>
  <g transform="translate(${mx + 22} ${my + 273}) scale(.7)" fill="none" stroke="#ff4256" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M8 19V5M5.31 7.69L8 5l2.69 2.69M16 5v14M13.31 16.31L16 19l2.69-2.69"/></g>
  <text x="${mx + 44}" y="${my + 286}" font-family="${FONT}" font-size="12" font-weight="700" fill="${T.text}">${L.transfers}</text>
  <rect x="${mx + 140}" y="${my + 279}" width="260" height="5" rx="2.5" fill="#fff" fill-opacity=".09"/>
  <rect x="${mx + 140}" y="${my + 279}" width="${260 * progress}" height="5" rx="2.5" fill="url(#hp)"/>
  <text x="${mx + mw - 24}" y="${my + 286}" text-anchor="end" font-family="${FONT}" font-size="12" font-weight="700" fill="${T.text}">${L.speed}</text>
</g>` };
}

function hero(L) {
  const W = 1280, H = 420;
  const mock = mockWindow(L, 700, 56);
  const body = `${backdrop('h', W, H)}
${mock.defs}
${logo('hl', 72, 78, 92)}
<text x="186" y="149" font-family="${FONT}" font-size="76" font-weight="700" letter-spacing="-1.5" fill="${T.text}">Nexa</text>
<text font-family="${FONT}" font-size="26" fill="${T.text2}"><tspan x="74" y="226">${L.tag1}</tspan><tspan x="74" y="262">${L.tag2}</tspan></text>
<g>
  ${chips(74, 302, ['SFTP', 'FTP', 'FTPS', L.windows], { accentFirst: false })}
</g>
<text x="76" y="374" font-family="${FONT}" font-size="15" fill="${T.muted}">${L.look}</text>
${mock.body}`;
  return svg(W, H, body, L.title);
}

function social(L) {
  const W = 1280, H = 640;
  const mock = mockWindow(L, 0, 0);
  const facts = L.cards.slice(0, 3).map(([value, unit, label], i) => {
    const x = 92 + i * 196;
    return `<rect x="${x}" y="478" width="22" height="4" rx="2" fill="#ff4256"/>
  <text x="${x - 2}" y="530" font-family="${FONT}" font-size="38" font-weight="700" letter-spacing="-.8" fill="${T.text}">${value}${unit ? `<tspan font-size="21" font-weight="600" fill="#ff4256" dx="5">${unit}</tspan>` : ''}</text>
  <text x="${x}" y="558" font-family="${FONT}" font-size="15" fill="${T.muted}">${label}</text>`;
  }).join('\n  ');
  const body = `${backdrop('o', W, H, 0, 1.15)}
${logo('ol', 88, 92, 104)}
<text x="216" y="176" font-family="${FONT}" font-size="88" font-weight="700" letter-spacing="-2" fill="${T.text}">Nexa</text>
<text font-family="${FONT}" font-size="31" fill="${T.text2}"><tspan x="90" y="272">${L.social1}</tspan><tspan x="90" y="314">${L.social2}</tspan></text>
<g>
  ${chips(90, 356, ['SFTP', 'FTP', 'FTPS', L.windows], { h: 40, size: 17 })}
</g>
<g>
  ${facts}
</g>
<g transform="translate(712 162) scale(1.02)">
${mock.defs}
${mock.body}
</g>`;
  return svg(W, H, body, L.title);
}

function stats(L) {
  const W = 1280, H = 188, gap = 16, cw = (W - gap * 3) / 4;
  const cards = L.cards.map(([value, unit, label, sub], i) => {
    const x = i * (cw + gap);
    const vw = textWidth(value, 50, 700);
    return `<g>
  <defs>
    <radialGradient id="s${i}" cx="${x}" cy="0" r="300" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#ff4256" stop-opacity=".2"/><stop offset="1" stop-color="#ff4256" stop-opacity="0"/></radialGradient>
  </defs>
  <rect x="${x}" y="0" width="${cw}" height="${H}" rx="20" fill="${T.base}"/>
  <rect x="${x}" y="0" width="${cw}" height="${H}" rx="20" fill="url(#s${i})"/>
  <rect x="${x + 0.5}" y=".5" width="${cw - 1}" height="${H - 1}" rx="19.5" fill="none" stroke="#fff" stroke-opacity=".09"/>
  <rect x="${x + 28}" y="28" width="26" height="4" rx="2" fill="#ff4256"/>
  <text x="${x + 26}" y="98" font-family="${FONT}" font-size="50" font-weight="700" letter-spacing="-1" fill="${T.text}">${value}${unit ? `<tspan font-size="26" font-weight="600" fill="#ff4256" dx="6">${unit}</tspan>` : ''}</text>
  <text x="${x + 28}" y="134" font-family="${FONT}" font-size="18" font-weight="600" fill="${T.text}">${label}</text>
  <text x="${x + 28}" y="160" font-family="${FONT}" font-size="15" fill="${T.muted}">${sub}</text>
</g>`;
  }).join('\n');
  return svg(W, H, cards, L.statsTitle);
}

function header() {
  const W = 1280, H = 104;
  let x = W - 32;
  const labels = ['FTPS', 'FTP', 'SFTP'];
  const parts = labels.map((l) => {
    const w = Math.round(textWidth(l, 14, 600) + 28);
    x -= w;
    const c = chip(x, 35, l, { h: 32, size: 14 }).s;
    x -= 10;
    return c;
  }).join('\n  ');
  return svg(W, H, `${backdrop('d', W, H, 22, 0.9)}
${logo('dl', 28, 22, 60)}
<text x="104" y="66" font-family="${FONT}" font-size="34" font-weight="700" letter-spacing="-.5" fill="${T.text}">Nexa</text>
<g>
  ${parts}
</g>`, 'Nexa');
}

function divider() {
  const W = 1280, H = 24;
  return svg(W, H, `<defs><linearGradient id="dv" x1="0" x2="${W}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#ff4256" stop-opacity="0"/><stop offset=".5" stop-color="#ff4256" stop-opacity=".85"/><stop offset="1" stop-color="#ff4256" stop-opacity="0"/></linearGradient>
<radialGradient id="dg" cx="${W / 2}" cy="12" r="60" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#ff4256" stop-opacity=".45"/><stop offset="1" stop-color="#ff4256" stop-opacity="0"/></radialGradient></defs>
<rect x="${W / 2 - 60}" y="0" width="120" height="24" fill="url(#dg)"/>
<rect x="0" y="11.5" width="${W}" height="1" fill="url(#dv)"/>
<rect x="${W / 2 - 4}" y="8" width="8" height="8" rx="2" transform="rotate(45 ${W / 2} 12)" fill="#ff4256"/>`, '');
}

function arrow(from, to, head) {
  const a = Math.atan2(-(from[1] - to[1]), from[0] - to[0]);
  const p = (d) => [to[0] + head * Math.cos(a + d), to[1] - head * Math.sin(a + d)];
  const l = p(Math.PI / 4), r = p(-Math.PI / 4);
  return `M${from[0]} ${from[1]}L${to[0]} ${to[1]}M${f(l[0])} ${f(l[1])}L${to[0]} ${to[1]}L${f(r[0])} ${f(r[1])}`;
}
const pl = (pts) => 'M' + pts.map((p) => p.join(' ')).join('L');
const folderD = 'M3 7.5Q3 5 5.5 5L9.2 5L11.2 7L18.5 7Q21 7 21 9.5L21 16.5Q21 19 18.5 19L5.5 19Q3 19 3 16.5Z';
const ICONS = {
  server: '<rect x="4" y="4.5" width="16" height="6.5" rx="2.5"/><rect x="4" y="13" width="16" height="6.5" rx="2.5"/><circle cx="7.8" cy="7.75" r=".4"/><circle cx="7.8" cy="16.25" r=".4"/>',
  transfers: `<path d="${arrow([8, 19], [8, 5], 3.8)}${arrow([16, 5], [16, 19], 3.8)}"/>`,
  command: `<rect x="3.5" y="5" width="17" height="14" rx="3.5"/><path d="${pl([[7.5, 10], [10, 12.5], [7.5, 15]])}M12 15L16.5 15"/>`,
  plus: '<path d="M12 5L12 19M5 12L19 12"/>',
  sliders: '<path d="M4 7L12 7M17 7L20 7M4 12L6 12M11 12L20 12M4 17L10 17M15 17L20 17"/><circle cx="14.5" cy="7" r="2.3"/><circle cx="8.5" cy="12" r="2.3"/><circle cx="12.5" cy="17" r="2.3"/>',
  bolt: `<path d="${pl([[13, 3.5], [5.5, 13.5], [11.5, 13.5], [10.5, 20.5], [18.5, 10.5], [12.5, 10.5], [13, 3.5]])}"/>`,
  check: `<path d="${pl([[5, 12.5], [10, 17.5], [19, 7]])}"/>`,
  terminal: `<path d="${pl([[5, 7], [10, 12], [5, 17]])}M12 17.5L19 17.5"/>`,
  folder: `<path d="${folderD}"/>`,
  shield: `<path d="M12 3.5L19 6.5L19 11.5C19 16 15.8 19 12 20.5C8.2 19 5 16 5 11.5L5 6.5Z"/><path d="${pl([[9, 12], [11.2, 14.2], [15.2, 10]])}"/>`,
  key: '<circle cx="8" cy="15.5" r="4"/><path d="M10.9 12.6L19 4.5M16 7.5L18.5 10M13.8 9.7L15.8 11.7"/>',
  eye: '<path d="M2.5 12C5 7 8.5 5.5 12 5.5C15.5 5.5 19 7 21.5 12C19 17 15.5 18.5 12 18.5C8.5 18.5 5 17 2.5 12Z"/><circle cx="12" cy="12" r="3"/>',
  info: '<circle cx="12" cy="12" r="8.5"/><path d="M12 11L12 16"/><circle cx="12" cy="8" r=".35"/>',
  refresh: '<path d="M15.75 5.5A7.5 7.5 0 1 0 19.39 13.3"/><path d="M17.2 3.6L16.3 7.9L20.5 8.3"/>',
  forward: `<path d="${pl([[9.5, 6], [15.5, 12], [9.5, 18]])}"/>`,
  alert: `<path d="${pl([[12, 4], [21, 19.5], [3, 19.5], [12, 4]])}M12 10L12 14"/><circle cx="12" cy="16.8" r=".35"/>`,
  link: '<rect x="3" y="9" width="9" height="6" rx="3"/><rect x="12" y="9" width="9" height="6" rx="3"/><path d="M9 12L15 12"/>',
  edit: `<path d="${pl([[15.5, 5], [19, 8.5], [9, 18.5], [4.5, 19.5], [5.5, 15], [15.5, 5]])}M13.5 7L17 10.5"/>`,
  trash: `<path d="M4.5 7L19.5 7"/><path d="${pl([[9.5, 7], [9.5, 4.5], [14.5, 4.5], [14.5, 7]])}"/><path d="${pl([[6.5, 7], [7.3, 18.2], [8.8, 19.5], [15.2, 19.5], [16.7, 18.2], [17.5, 7]])}"/><path d="M10.3 10.5L10.3 16M13.7 10.5L13.7 16"/>`,
  plug: `<path d="M9 3.5L9 7.5M15 3.5L15 7.5"/><path d="${pl([[6, 7.5], [18, 7.5], [18, 11], [12, 17], [6, 11], [6, 7.5]])}M12 17L12 20.5"/>`,
};
function iconTile(name) {
  const S = 32;
  return svg(S, S, `<rect x=".5" y=".5" width="31" height="31" rx="9" fill="#ff4256" fill-opacity=".14" stroke="#ff4256" stroke-opacity=".4"/>
<g transform="translate(6.4 6.4) scale(.8)" fill="none" stroke="#ff4256" stroke-width="2.1" stroke-linecap="round" stroke-linejoin="round">${ICONS[name]}</g>`, '');
}

const EN = {
  title: 'Nexa — a fast SFTP, FTP and FTPS client for Windows',
  tag1: 'A fast SFTP, FTP and FTPS client for Windows.',
  tag2: 'One tab per server. Transfers that never drop.',
  social1: 'A fast SFTP, FTP and FTPS client',
  social2: 'for Windows. One tab per server.',
  windows: 'Windows 10 · 11',
  look: 'Onyx black · frosted glass · red accent · no sidebars',
  local: 'THIS COMPUTER', server: 'SERVER', transfers: 'Transfers', ping: 'ping 24 ms', speed: '38 MB/s',
  statsTitle: 'Nexa in numbers',
  cards: [
    ['4.3', 's', '3,000 small files', 'scp needs 673 s at 50 ms'],
    ['64', '', 'files in flight', 'per connection'],
    ['1–16', '', 'parallel connections', 'per server'],
    ['0.44', 's', 'to the first listing', 'after login at 200 ms'],
  ],
};
const RU = {
  title: 'Nexa — быстрый SFTP-, FTP- и FTPS-клиент для Windows',
  tag1: 'Быстрый SFTP-, FTP- и FTPS-клиент для Windows.',
  tag2: 'Вкладка на каждый сервер. Передачи без обрывов.',
  windows: 'Windows 10 · 11',
  look: 'Чёрный оникс · матовое стекло · красный акцент · без боковых панелей',
  local: 'ЭТОТ КОМПЬЮТЕР', server: 'СЕРВЕР', transfers: 'Передачи', ping: 'пинг 24 мс', speed: '38 МБ/с',
  statsTitle: 'Nexa в цифрах',
  cards: [
    ['4,3', 'с', '3000 мелких файлов', 'scp — 673 с при 50 мс'],
    ['64', '', 'файла «в полёте»', 'на одно соединение'],
    ['1–16', '', 'параллельных соединений', 'на сервер'],
    ['0,44', 'с', 'до первого списка папки', 'после входа при 200 мс'],
  ],
};

writeFileSync(join(out, 'logo.svg'), svg(100, 100, logo('l', 0, 0, 100), 'Nexa'));
writeFileSync(join(out, 'hero.svg'), hero(EN));
writeFileSync(join(out, 'ru', 'hero.svg'), hero(RU));
writeFileSync(join(out, 'stats.svg'), stats(EN));
writeFileSync(join(out, 'social.svg'), social(EN));
writeFileSync(join(out, 'ru', 'stats.svg'), stats(RU));
writeFileSync(join(out, 'header.svg'), header());
writeFileSync(join(out, 'divider.svg'), divider());
for (const name of Object.keys(ICONS)) writeFileSync(join(out, 'icons', `${name}.svg`), iconTile(name));
