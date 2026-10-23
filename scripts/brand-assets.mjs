import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const out = join(root, 'docs', 'assets', 'brand');
mkdirSync(join(out, 'ru'), { recursive: true });
mkdirSync(join(out, 'de'), { recursive: true });
mkdirSync(join(out, 'icons'), { recursive: true });

// The app's own tokens (src/ui/theme.h), so the docs look like the window they describe.
const T = {
  base: '#000000', card: '#111113', text: '#f2f3f5', text2: '#b5bac1', muted: '#949ba4', faint: '#80838c',
  accent: '#5865f2', accentText: '#8b95f7', success: '#23a55a', divider: '#1e1e20',
  orange: '#ff8a3d', green: '#3ddc97',
};
const FONT = `'Segoe UI Variable Text','Segoe UI',system-ui,-apple-system,'Helvetica Neue',Arial,sans-serif`;
const DISPLAY = `'Segoe UI Variable Display','Segoe UI',system-ui,-apple-system,'Helvetica Neue',Arial,sans-serif`;
const MONO = `'Cascadia Mono',Consolas,'SFMono-Regular',Menlo,monospace`;
const f = (n) => +n.toFixed(2);
const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;');

// Rough Segoe UI advance widths; good enough to place text after text without measuring.
const charWidth = (c) =>
  /[mwMWжшщыюМШЩЖЮ]/.test(c) ? 0.8 : /[A-ZА-ЯЁÄÖÜ]/.test(c) ? 0.62 : /[0-9]/.test(c) ? 0.53 : /[ijltfrI.,:;'!|]/.test(c) ? 0.3
    : /s/.test(c) ? 0.27 : /[a-zа-яёäöüß]/.test(c) ? 0.53 : 0.56;
const textWidth = (s, size, weight = 400) =>
  [...s].reduce((a, c) => a + charWidth(c), 0) * size * (weight >= 600 ? 1.05 : 1);

function text(x, y, s, { size = 13, weight = 400, fill = T.text, anchor, font = FONT, ls } = {}) {
  return `<text x="${f(x)}" y="${f(y)}"${anchor ? ` text-anchor="${anchor}"` : ''} font-family="${font}" font-size="${size}"${weight !== 400 ? ` font-weight="${weight}"` : ''}${ls ? ` letter-spacing="${ls}"` : ''} fill="${fill}">${esc(s)}</text>`;
}

// Squircle of the app icon, kept so the docs mark and the taskbar icon share a silhouette.
const SQUIRCLE = (() => {
  const pts = [];
  for (let i = 0; i < 120; i++) {
    const t = (2 * Math.PI * i) / 120, c = Math.cos(t), s = Math.sin(t);
    pts.push(`${f(50 + 47 * Math.sign(c) * Math.abs(c) ** 0.4)} ${f(50 + 47 * Math.sign(s) * Math.abs(s) ** 0.4)}`);
  }
  return `M${pts.join('L')}Z`;
})();

// Flat mark: an "N" drawn as one stroke between two endpoints, this computer and the server.
// Same geometry as the app icon, without the glow, so it holds up from 16 px to the hero.
function logo(x, y, size) {
  const s = size / 100;
  const sw = size < 28 ? 9.5 : 8;
  return `<g transform="translate(${f(x)} ${f(y)}) scale(${f(s)})">
  <path d="${SQUIRCLE}" fill="#18181b" stroke="#fff" stroke-opacity=".14" stroke-width="${f(1 / s)}"/>
  <path d="M31 72.3V27.7L68.9 72.3V27.7" fill="none" stroke="${T.text}" stroke-width="${sw}" stroke-linecap="round" stroke-linejoin="round"/>
  <circle cx="31" cy="72.3" r="10" fill="#18181b"/><circle cx="31" cy="72.3" r="6.6" fill="${T.accent}"/>
  <circle cx="68.9" cy="27.7" r="10" fill="#18181b"/><circle cx="68.9" cy="27.7" r="6.6" fill="${T.accent}"/>
</g>`;
}

const svg = (w, h, body, title) =>
  `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}" ${title ? `role="img" aria-label="${title}">
<title>${title}</title>` : 'aria-hidden="true">'}
${body}
</svg>
`;

// Docs surfaces: one flat card colour and one hairline, readable on GitHub's light and dark themes.
const card = (w, h, r = 12) => `<rect width="${w}" height="${h}" rx="${r}" fill="${T.card}"/>
${r ? `<rect x=".5" y=".5" width="${w - 1}" height="${h - 1}" rx="${r - 0.5}" fill="none" stroke="#fff" stroke-opacity=".08"/>` : ''}`;

// 24 px glyphs from the app's line icon set, stroked at the size they are drawn.
const glyph = (x, y, size, color, d, sw = 1.6) =>
  `<g transform="translate(${f(x)} ${f(y)}) scale(${f(size / 24)})" fill="none" stroke="${color}" stroke-width="${f(sw * 24 / size)}" stroke-linecap="round" stroke-linejoin="round">${d}</g>`;
const G = {
  computer: '<rect x="3.5" y="4.5" width="17" height="11.5" rx="2"/><path d="M9 20h6M12 16v4"/>',
  server: '<rect x="4" y="4.5" width="16" height="6.5" rx="2"/><rect x="4" y="13" width="16" height="6.5" rx="2"/>',
  back: '<path d="M14.5 6l-6 6 6 6"/>', fwd: '<path d="M9.5 6l6 6-6 6"/>', up: '<path d="M12 19V5M6.5 10.5L12 5l5.5 5.5"/>',
  chevron: '<path d="M10 7.5l4.5 4.5-4.5 4.5"/>', refresh: '<path d="M18.5 12a6.5 6.5 0 1 1-1.9-4.6M18.5 5v4h-4"/>',
  search: '<circle cx="10.5" cy="10.5" r="6"/><path d="M15 15l4.5 4.5"/>', plus: '<path d="M12 5v14M5 12h14"/>',
  close: '<path d="M7 7l10 10M17 7L7 17"/>', min: '<path d="M6 12h12"/>', max: '<rect x="6.5" y="6.5" width="11" height="11" rx="1"/>',
  upload: '<path d="M12 15V4.5M7.5 9L12 4.5 16.5 9M5 15v3.5h14V15"/>', download: '<path d="M12 4.5V15M7.5 10.5L12 15l4.5-4.5M5 15v3.5h14V15"/>',
  newFolder: '<path d="M3.5 7.5q0-2 2-2h3.7l2 2h7.3q2 0 2 2v7q0 2-2 2h-13q-2 0-2-2zM12 10.5v5M9.5 13h5"/>',
  trash: '<path d="M5 7h14M9.5 7V5h5v2M7 7l.8 12h8.4L17 7"/>', transfers: '<path d="M8 19V5M5 8l3-3 3 3M16 5v14M13 16l3 3 3-3"/>',
};
const folderIcon = (x, y) => `<path d="M${f(x)} ${f(y + 2)}q0-2 2-2h3.6l1.8 1.8h6.6q2 0 2 2v7.2q0 2-2 2h-12q-2 0-2-2z" fill="${T.accent}" fill-opacity=".9"/><path d="M${f(x)} ${f(y + 5.5)}h16" stroke="#000" stroke-opacity=".25"/>`;
const fileIcon = (x, y) => `<path d="M${f(x + 8)} ${f(y)}h-5.5q-1.5 0-1.5 1.5v11q0 1.5 1.5 1.5h9q1.5 0 1.5-1.5v-8z" fill="none" stroke="${T.faint}" stroke-width="1.2" stroke-linejoin="round"/>`;

// The session window, drawn from the real layout at 1:1: title bar, connection strip,
// two file panes and the transfer dock. Rows and numbers come from the demo profile.
function windowMock(L, wx, wy) {
  const ww = 656, wh = 512;
  const X = (x) => wx + x, Y = (y) => wy + y;
  const line = (y, x1 = 0, x2 = ww) => `<rect x="${X(x1)}" y="${Y(y)}" width="${x2 - x1}" height="1" fill="${T.divider}"/>`;

  const tabs = (() => {
    let x = 40;
    return [['web-prod', T.orange, true], ['Storage', T.green, false]].map(([name, label, active]) => {
      const w = Math.round(32 + textWidth(name, 12.5, active ? 600 : 400) + (active ? 30 : 14));
      const s = `${active ? `<rect x="${X(x)}" y="${Y(6)}" width="${w}" height="28" rx="6" fill="#fff" fill-opacity=".08"/>` : ''}
  <rect x="${X(x + 10)}" y="${Y(14)}" width="2" height="12" rx="1" fill="${label}"/>
  <circle cx="${X(x + 20)}" cy="${Y(20)}" r="3" fill="${T.success}"/>
  ${text(X(x + 29), Y(24.5), name, { size: 12.5, weight: active ? 600 : 400, fill: active ? T.text : T.text2 })}
  ${active ? glyph(X(x + w - 22), Y(14), 12, T.muted, G.close, 1.3) : ''}`;
      x += w + 4;
      return s;
    }).join('\n  ') + `\n  ${glyph(X(x + 6), Y(13), 14, T.muted, G.plus, 1.4)}`;
  })();

  const searchX = ww - 332;
  const titleBar = `${logo(X(12), Y(10), 20)}
  ${tabs}
  <rect x="${X(searchX) + 0.5}" y="${Y(7) + 0.5}" width="199" height="25" rx="6" fill="none" stroke="#fff" stroke-opacity=".12"/>
  ${text(X(searchX + 11), Y(24), L.search, { size: 12, fill: T.muted })}
  <rect x="${X(searchX + 154)}" y="${Y(12)}" width="38" height="16" rx="4" fill="#fff" fill-opacity=".09"/>
  ${text(X(searchX + 173), Y(23.8), 'Ctrl K', { size: 10, weight: 600, fill: T.text2, anchor: 'middle' })}
  ${glyph(X(ww - 106), Y(14), 12, T.text2, G.min, 1.2)}${glyph(X(ww - 66), Y(14), 12, T.text2, G.max, 1.2)}${glyph(X(ww - 26), Y(14), 12, T.text2, G.close, 1.2)}`;

  const addrX = 32 + textWidth('web-prod', 15, 600) + 10;
  const stateX = addrX + textWidth(`deploy@web-01.example.com:22 · SFTP`, 13) + 12;
  const strip = `<circle cx="${X(19)}" cy="${Y(62)}" r="4" fill="${T.success}"/>
  ${text(X(32), Y(67), 'web-prod', { size: 15, weight: 600 })}
  ${text(X(addrX), Y(66.5), 'deploy@web-01.example.com:22 · SFTP', { size: 13, fill: T.text2 })}
  ${text(X(stateX), Y(66), L.connected, { size: 12, fill: T.muted })}
  ${[G.upload, G.newFolder, G.download, G.trash].map((d, i) => glyph(X(ww - 112 + i * 26), Y(54), 16, T.text2, d, 1.4)).join('')}`;

  const pane = (px, pw, { label, icon, crumbs, rows, footer, focused }) => {
    const sizeR = pw - 112, modX = pw - 100;
    const body = rows.map(([kind, name, size, mod, sel], i) => {
      const y = 180 + i * 30;
      return `${sel ? `<rect x="${X(px + 6)}" y="${Y(y + 1)}" width="${pw - 12}" height="28" rx="6" fill="${T.accent}" fill-opacity=".2"/>` : ''}
  ${kind === 'd' ? folderIcon(X(px + 16), Y(y + 8)) : fileIcon(X(px + 17), Y(y + 7.5))}
  ${text(X(px + 42), Y(y + 19.5), name, { size: 13, fill: T.text })}
  ${size ? text(X(px + sizeR), Y(y + 19.5), L.kb(size), { size: 11.5, fill: T.muted, anchor: 'end' }) : ''}
  ${text(X(px + modX), Y(y + 19.5), mod, { size: 11.5, fill: T.muted })}`;
    }).join('\n  ');
    let cx = px + 92;
    const trail = crumbs.map((c, i) => {
      const last = i === crumbs.length - 1;
      const s = `${text(X(cx), Y(141), c, { size: 13, fill: last ? T.text : T.text2 })}${last ? '' : glyph(X(cx + textWidth(c, 13) + 3), Y(130), 12, T.faint, G.chevron, 1.3)}`;
      cx += textWidth(c, 13) + 20;
      return s;
    }).join('');
    return `${focused ? `<rect x="${X(px + 8)}" y="${Y(85)}" width="${pw - 16}" height="2" rx="1" fill="${T.accent}"/>` : ''}
  ${glyph(X(px + 16), Y(95), 14, T.text2, icon, 1.5)}
  ${text(X(px + 37), Y(106.5), label, { size: 11, weight: 600, fill: focused ? T.text : T.text2, ls: '.6' })}
  <rect x="${X(px + pw - 108) + 0.5}" y="${Y(91) + 0.5}" width="95" height="23" rx="6" fill="none" stroke="#fff" stroke-opacity=".2"/>
  ${glyph(X(px + pw - 100), Y(97), 12, T.faint, G.search, 1.3)}
  ${text(X(px + pw - 82), Y(107), L.filter, { size: 11.5, fill: T.faint })}
  ${glyph(X(px + 12), Y(130), 12, T.muted, G.back, 1.4)}${glyph(X(px + 32), Y(130), 12, T.muted, G.fwd, 1.4)}${glyph(X(px + 52), Y(130), 12, T.muted, G.up, 1.4)}
  ${glyph(X(px + 72), Y(129), 14, T.muted, icon, 1.4)}
  ${trail}
  ${glyph(X(px + pw - 26), Y(130), 12, T.muted, G.refresh, 1.4)}
  ${text(X(px + 16), Y(170), L.cols[0], { size: 10.5, weight: 600, fill: T.muted, ls: '.5' })}
  ${text(X(px + sizeR), Y(170), L.cols[1], { size: 10.5, weight: 600, fill: T.muted, ls: '.5', anchor: 'end' })}
  ${text(X(px + modX), Y(170), L.cols[2], { size: 10.5, weight: 600, fill: T.muted, ls: '.5' })}
  ${body}
  ${text(X(px + 16), Y(407), footer, { size: 11, fill: T.faint })}`;
  };

  const d = L.dates;
  const left = pane(0, 328, {
    label: L.local, icon: G.computer, crumbs: ['D:', 'Projects', 'website'], focused: true, footer: L.footerLocal,
    rows: [['d', 'app', '', d[0]], ['d', 'public', '', d[1], 1], ['d', 'resources', '', d[2]],
      ['f', 'composer.json', '2.4', d[3]], ['f', 'deploy.sh', '3.1', d[4]], ['f', 'package.json', '1.8', d[5]], ['f', 'vite.config.js', '1.1', d[6]]],
  });
  const right = pane(329, 327, {
    label: L.server, icon: G.server, crumbs: ['var', 'www', 'shop'], footer: L.footerServer,
    rows: [['d', 'app', '', d[1]], ['d', 'bootstrap', '', d[7]], ['d', 'config', '', d[8]], ['d', 'public', '', d[1]],
      ['d', 'resources', '', d[2]], ['f', 'composer.json', '2.4', d[3]], ['f', 'deploy.sh', '3.1', d[4]]],
  });

  const progress = 0.34, bx = 372, bw = 232;
  const chipW = textWidth('web-prod', 11, 600) + 14, nameW = textWidth('public', 13.5, 600);
  const dock = `<rect x="${X(16)}" y="${Y(429)}" width="28" height="28" rx="6" fill="${T.accent}" fill-opacity=".16"/>
  ${glyph(X(22), Y(435), 16, T.accentText, G.upload, 1.5)}
  ${text(X(56), Y(440), 'public', { size: 13.5, weight: 600 })}
  <rect x="${f(X(64 + nameW)) + 0.5}" y="${Y(427) + 0.5}" width="${f(chipW)}" height="17" rx="4" fill="none" stroke="#fff" stroke-opacity=".16"/>
  ${text(X(64 + nameW + chipW / 2), Y(439.5), 'web-prod', { size: 11, weight: 600, fill: T.text2, anchor: 'middle' })}
  ${text(X(56), Y(458), '/var/www/shop/public', { size: 11.5, fill: T.muted })}
  <rect x="${X(bx)}" y="${Y(432)}" width="${bw}" height="4" rx="2" fill="#fff" fill-opacity=".1"/>
  <rect x="${X(bx)}" y="${Y(432)}" width="${f(bw * progress)}" height="4" rx="2" fill="${T.accent}"/>
  ${text(X(bx), Y(455), L.done, { size: 11.5, fill: T.muted })}
  ${text(X(bx + bw), Y(455), L.rate, { size: 11.5, weight: 600, anchor: 'end' })}
  ${glyph(X(ww - 34), Y(437), 12, T.muted, G.close, 1.3)}
  <rect x="${X(0)}" y="${Y(470)}" width="${ww}" height="1" fill="${T.divider}"/>
  <rect x="${X(0)}" y="${Y(470)}" width="${f(ww * progress)}" height="2" fill="${T.accent}"/>
  ${glyph(X(15), Y(484), 15, T.accentText, G.transfers, 1.5)}
  ${text(X(38), Y(495.5), L.transfers, { size: 13, weight: 600 })}
  ${text(X(50 + textWidth(L.transfers, 13, 600)), Y(495.5), L.active, { size: 12, fill: T.muted })}
  ${text(X(ww - 16), Y(495.5), L.rule, { size: 12, weight: 600, anchor: 'end' })}
  ${text(X(ww - 22 - textWidth(L.rule, 12, 600)), Y(495.5), L.ifExists, { size: 12, fill: T.muted, anchor: 'end' })}`;

  return `<rect x="${wx}" y="${wy}" width="${ww}" height="${wh}" rx="8" fill="${T.base}"/>
${line(40)}
${line(84)}
<rect x="${X(328)}" y="${Y(85)}" width="1" height="331" fill="${T.divider}"/>
${line(416)}
${titleBar}
${strip}
${left}
${right}
${dock}
<rect x="${wx + 0.5}" y="${wy + 0.5}" width="${ww - 1}" height="${wh - 1}" rx="7.5" fill="none" stroke="#fff" stroke-opacity=".14"/>`;
}

// README banner and social preview share one 2:1 layout: the claim on the left, the window on the right,
// drawn 10% over 1:1 so its 13 px rows still read when GitHub scales the banner to the README column.
function banner(L, { radius = 12 } = {}) {
  const W = 1280, H = 640;
  const lead = L.lead.map((t, i) => `<tspan x="64" y="${334 + i * 38}">${esc(t)}</tspan>`).join('');
  const rest = L.rest.map((t, i) => `<tspan x="64" y="${418 + i * 30}">${esc(t)}</tspan>`).join('');
  return svg(W, H, `${card(W, H, radius)}
${logo(64, 200, 60)}
${text(144, 250, 'Nexa', { size: 60, weight: 700, ls: '-1.2', font: DISPLAY })}
<text font-family="${DISPLAY}" font-size="27" font-weight="500" fill="${T.text}">${lead}</text>
<text font-family="${FONT}" font-size="20" fill="${T.muted}">${rest}</text>
<g transform="translate(518 38) scale(1.1)">
${windowMock(L, 0, 0)}
</g>`, L.title);
}

function stats(L) {
  const W = 1280, H = 168, cw = W / 4;
  const cells = L.cards.map(([value, unit, label, sub], i) => {
    const x = i * cw + 32;
    const vw = textWidth(value, 46, 700);
    return `${i ? `<rect x="${i * cw}" y="24" width="1" height="${H - 48}" fill="#fff" fill-opacity=".08"/>` : ''}
${text(x, 80, value, { size: 46, weight: 700, ls: '-.8', font: DISPLAY })}${unit ? text(x + vw + 6, 80, unit, { size: 22, weight: 600, fill: T.accentText }) : ''}
${text(x, 114, label, { size: 17, weight: 600 })}
${text(x, 139, sub, { size: 14, fill: T.muted })}`;
  }).join('\n');
  return svg(W, H, `${card(W, H)}\n${cells}`, L.statsTitle);
}

function header() {
  const W = 1280, H = 96;
  let x = W - 32;
  const protocols = ['FTPS', 'FTP', 'SFTP'].map((p, i) => {
    const w = textWidth(p, 15, 500);
    x -= w;
    const s = `${text(x, 54, p, { size: 15, weight: 500, fill: T.text2, font: MONO })}${i ? `<rect x="${f(x + w + 13)}" y="38" width="1" height="20" fill="#fff" fill-opacity=".14"/>` : ''}`;
    x -= 27;
    return s;
  }).join('');
  return svg(W, H, `${card(W, H)}
${logo(28, 24, 48)}
${text(92, 59, 'Nexa', { size: 32, weight: 700, ls: '-.5', font: DISPLAY })}
${protocols}`, 'Nexa');
}

// A hairline with the 2 px accent bar the app draws over the focused pane.
function divider() {
  const W = 1280, H = 24;
  return svg(W, H, `<rect x="0" y="11.5" width="${W}" height="1" fill="#8b949e" fill-opacity=".3"/>
<rect x="${W / 2 - 24}" y="11" width="48" height="2" rx="1" fill="${T.accent}"/>`, '');
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
// Heading icons sit on the same dark card as the header, so they read the same in both GitHub themes.
function iconTile(name) {
  const S = 32;
  return svg(S, S, `<rect x=".5" y=".5" width="31" height="31" rx="8" fill="${T.card}" stroke="#fff" stroke-opacity=".1"/>
<g transform="translate(7 7) scale(.75)" fill="none" stroke="${T.accentText}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">${ICONS[name]}</g>`, '');
}

const EN = {
  title: 'Nexa — a fast SFTP, FTP and FTPS client for Windows',
  lead: ['A fast SFTP, FTP and FTPS', 'client for Windows.'],
  rest: ['One tab per server.', 'Transfers that never drop.'],
  local: 'THIS COMPUTER', server: 'SERVER', cols: ['NAME', 'SIZE', 'MODIFIED'],
  search: 'Search and commands', filter: 'Filter', connected: 'Connected · ping 24 ms',
  dates: ['today 00:11', 'today 00:09', 'Oct 22, 21:14', 'Oct 22, 00:02', 'today 00:42', 'Oct 22, 22:11', 'Oct 21, 23:15', 'Oct 21, 00:45', 'Oct 22, 00:43'],
  kb: (v) => `${v} KB`,
  footerLocal: 'Folders: 3 · Files: 4 · 8.4 KB', footerServer: 'Folders: 5 · Files: 2 · 5.5 KB',
  done: '412 MB / 1.2 GB · 1186/3240', rate: '38 MB/s · 22 s',
  transfers: 'Transfers', active: 'active: 1 · 38 MB/s · 22 s left', ifExists: 'If the file exists:', rule: 'Resume',
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
  lead: ['Быстрый SFTP-, FTP- и FTPS-', 'клиент для Windows.'],
  rest: ['Вкладка на каждый сервер.', 'Передачи без обрывов.'],
  local: 'ЭТОТ КОМПЬЮТЕР', server: 'СЕРВЕР', cols: ['ИМЯ', 'РАЗМЕР', 'ИЗМЕНЁН'],
  search: 'Поиск и команды', filter: 'Фильтр', connected: 'Подключено · ping 24 мс',
  dates: ['сегодня 00:11', 'сегодня 00:09', '22 окт. 21:14', '22 окт. 00:02', 'сегодня 00:42', '22 окт. 22:11', '21 окт. 23:15', '21 окт. 00:45', '22 окт. 00:43'],
  kb: (v) => `${v.replace('.', ',')} КБ`,
  footerLocal: 'Папок: 3 · Файлов: 4 · 8,4 КБ', footerServer: 'Папок: 5 · Файлов: 2 · 5,5 КБ',
  done: '412 МБ / 1,2 ГБ · 1186/3240', rate: '38 МБ/с · 22 с',
  transfers: 'Передачи', active: 'активно: 1 · 38 МБ/с · осталось 22 с', ifExists: 'Если файл есть:', rule: 'Докачать',
  statsTitle: 'Nexa в цифрах',
  cards: [
    ['4,3', 'с', '3000 мелких файлов', 'scp — 673 с при 50 мс'],
    ['64', '', 'файла «в полёте»', 'на одно соединение'],
    ['1–16', '', 'параллельных соединений', 'на сервер'],
    ['0,44', 'с', 'до первого списка папки', 'после входа при 200 мс'],
  ],
};
const DE = {
  title: 'Nexa — ein schneller SFTP-, FTP- und FTPS-Client für Windows',
  lead: ['Schneller SFTP-, FTP- und', 'FTPS-Client für Windows.'],
  rest: ['Ein Tab pro Server.', 'Übertragungen ohne Abbrüche.'],
  local: 'DIESER PC', server: 'SERVER', cols: ['NAME', 'GRÖSSE', 'GEÄNDERT'],
  search: 'Suche und Befehle', filter: 'Filter', connected: 'Verbunden · Ping 24 ms',
  dates: ['heute 00:11', 'heute 00:09', '22.10. 21:14', '22.10. 00:02', 'heute 00:42', '22.10. 22:11', '21.10. 23:15', '21.10. 00:45', '22.10. 00:43'],
  kb: (v) => `${v.replace('.', ',')} KB`,
  footerLocal: 'Ordner: 3 · Dateien: 4 · 8,4 KB', footerServer: 'Ordner: 5 · Dateien: 2 · 5,5 KB',
  done: '412 MB / 1,2 GB · 1186/3240', rate: '38 MB/s · 22 s',
  transfers: 'Übertragungen', active: 'aktiv: 1 · 38 MB/s · noch 22 s', ifExists: 'Wenn die Datei existiert:', rule: 'Fortsetzen',
  statsTitle: 'Nexa in Zahlen',
  cards: [
    ['4,3', 's', '3.000 kleine Dateien', 'scp braucht 673 s bei 50 ms'],
    ['64', '', 'Dateien gleichzeitig', 'pro Verbindung'],
    ['1–16', '', 'parallele Verbindungen', 'pro Server'],
    ['0,44', 's', 'bis zur ersten Liste', 'nach der Anmeldung bei 200 ms'],
  ],
};

writeFileSync(join(out, 'logo.svg'), svg(100, 100, logo(0, 0, 100), 'Nexa'));
writeFileSync(join(out, 'hero.svg'), banner(EN));
writeFileSync(join(out, 'ru', 'hero.svg'), banner(RU));
writeFileSync(join(out, 'de', 'hero.svg'), banner(DE));
writeFileSync(join(out, 'social.svg'), banner(EN, { radius: 0 }));
writeFileSync(join(out, 'stats.svg'), stats(EN));
writeFileSync(join(out, 'ru', 'stats.svg'), stats(RU));
writeFileSync(join(out, 'de', 'stats.svg'), stats(DE));
writeFileSync(join(out, 'header.svg'), header());
writeFileSync(join(out, 'divider.svg'), divider());
for (const name of Object.keys(ICONS)) writeFileSync(join(out, 'icons', `${name}.svg`), iconTile(name));
