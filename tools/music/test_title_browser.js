'use strict';

const { spawn } = require('child_process');
const { mkdtempSync } = require('fs');
const { tmpdir } = require('os');
const { join } = require('path');

const edge = process.env.EDGE_PATH || 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const url = process.argv[2] || 'http://127.0.0.1:8138/';
const mobileWindowed = process.argv.includes('--mobile-windowed');
const port = 9337;
const profile = mkdtempSync(join(tmpdir(), 'ayrien-edge-'));
const child = spawn(edge, [
  '--headless=new', `--remote-debugging-port=${port}`, `--user-data-dir=${profile}`,
  '--autoplay-policy=no-user-gesture-required', '--disable-background-timer-throttling',
  '--disable-renderer-backgrounding', url
], { stdio: 'ignore' });

const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

async function pageTarget() {
  for (let attempt = 0; attempt < 40; attempt++) {
    try {
      const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
      const page = targets.find(target => target.type === 'page' && target.url.startsWith(url));
      if (page) return page;
    } catch (_) {}
    await sleep(250);
  }
  throw new Error('headless browser did not expose the Ayrien Assault page');
}

async function smoke() {
  const target = await pageTarget();
  const socket = new WebSocket(target.webSocketDebuggerUrl);
  const pending = new Map(), errors = [];
  let serial = 0;
  await new Promise((resolve, reject) => { socket.onopen = resolve; socket.onerror = reject; });
  socket.onmessage = event => {
    const message = JSON.parse(event.data);
    if (message.id) {
      const waiter = pending.get(message.id);
      if (!waiter) return;
      pending.delete(message.id);
      if (message.error) waiter.reject(new Error(JSON.stringify(message.error)));
      else waiter.resolve(message.result);
    } else if (message.method === 'Runtime.exceptionThrown') {
      const detail = message.params.exceptionDetails;
      const description = detail.exception && detail.exception.description;
      errors.push(description || `${detail.text} at ${detail.url || ''}:${detail.lineNumber || 0}`);
    } else if (message.method === 'Log.entryAdded' && message.params.entry.level === 'error') {
      const entry = message.params.entry;
      if (!entry.url || !entry.url.endsWith('/favicon.ico')) errors.push(`${entry.text} ${entry.url || ''}`.trim());
    }
  };
  const call = (method, params = {}) => new Promise((resolve, reject) => {
    const id = ++serial;
    pending.set(id, { resolve, reject });
    socket.send(JSON.stringify({ id, method, params }));
  });
  const evaluateValue = async expression => {
    const answer = await call('Runtime.evaluate', { expression, returnByValue: true, userGesture: true });
    if (answer.exceptionDetails) {
      const detail = answer.exceptionDetails;
      throw new Error((detail.exception && detail.exception.description) || detail.text);
    }
    return answer.result.value;
  };
  const evaluate = async expression => JSON.parse(await evaluateValue(expression));
  const key = async (name, code, virtualKey) => {
    const fields = { key: name, code, windowsVirtualKeyCode: virtualKey, nativeVirtualKeyCode: virtualKey };
    await call('Input.dispatchKeyEvent', { type: 'keyDown', ...fields });
    await sleep(80);
    await call('Input.dispatchKeyEvent', { type: 'keyUp', ...fields });
  };

  await call('Runtime.enable');
  await call('Log.enable');
  await call('Page.enable');
  if (mobileWindowed) {
    await call('Emulation.setDeviceMetricsOverride', {
      width: 844, height: 390, deviceScaleFactor: 1, mobile: true,
      screenOrientation: { type: 'landscapePrimary', angle: 90 }
    });
    await call('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });
    await call('Page.reload', { ignoreCache: true });
  }
  for (let attempt = 0; attempt < 60; attempt++) {
    if (await evaluateValue(`typeof AYRIEN_TITLE_MUSIC_DEBUG === 'function'`)) break;
    await sleep(100);
  }
  if (mobileWindowed) {
    const point = await evaluateValue(`(() => { const b=document.querySelector('#continueWindowed'),r=b.getBoundingClientRect(); return {x:r.left+r.width/2,y:r.top+r.height/2}; })()`);
    const touch = [{ x: point.x, y: point.y, radiusX: 4, radiusY: 4, force: 1, id: 1 }];
    await call('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: touch });
    await sleep(100);
    await call('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    await sleep(2200);
    const state = await evaluate(`JSON.stringify({music:AYRIEN_TITLE_MUSIC_DEBUG(),fullscreen:!!(document.fullscreenElement||document.webkitFullscreenElement),mobile:document.body.classList.contains('mobile-ui'),launch:document.body.classList.contains('mobile-launch')})`);
    socket.close();
    if (errors.length) throw new Error(`browser console errors: ${errors.join('; ')}`);
    if (!state.mobile || state.launch || state.fullscreen || state.music.context !== 'running' ||
        state.music.cue !== 'TITLE' || state.music.liveSources < 1 || state.music.faults !== 0) {
      throw new Error(`windowed mobile gesture did not unlock title music: ${JSON.stringify(state)}`);
    }
    console.log(JSON.stringify({ ok: true, url, mobileWindowed: state }));
    return;
  }
  await key('h', 'KeyH', 72);
  await sleep(12000);
  const menu = await evaluate('JSON.stringify(AYRIEN_TITLE_MUSIC_DEBUG())');

  await key('Escape', 'Escape', 27);
  await sleep(300);
  await key(' ', 'Space', 32);
  await sleep(900);
  const game = await evaluate('JSON.stringify(AYRIEN_TITLE_MUSIC_DEBUG())');
  const switched = await evaluate('snd_music_game(1); JSON.stringify(AYRIEN_TITLE_MUSIC_DEBUG())');
  await sleep(500);
  const nextChapter = await evaluate('JSON.stringify(AYRIEN_TITLE_MUSIC_DEBUG())');
  await key('p', 'KeyP', 80);
  await sleep(300);
  const paused = await evaluate('JSON.stringify(AYRIEN_TITLE_MUSIC_DEBUG())');
  socket.close();

  if (errors.length) throw new Error(`browser console errors: ${errors.join('; ')}`);
  if (menu.context !== 'running' || menu.cue !== 'TITLE' || menu.liveSources < 1 ||
      menu.liveSources >= 64 || menu.faults !== 0 || menu.invalidStops !== 0 ||
      menu.discontinuityResets !== 0 || menu.completedLoops < 1 || menu.furthestIndex < 113) {
    throw new Error(`invalid title runtime state: ${JSON.stringify(menu)}`);
  }
  if (game.cue !== 'GAME' || game.liveSources !== 0 || game.running || game.legacyLiveSources < 1) {
    throw new Error(`invalid gameplay music state: ${JSON.stringify(game)}`);
  }
  if (switched.chapter !== 1 || switched.legacyLiveSources !== 0 ||
      nextChapter.chapter !== 1 || nextChapter.legacyLiveSources < 1) {
    throw new Error(`chapter transition overlapped music: ${JSON.stringify({ switched, nextChapter })}`);
  }
  if (!paused.paused || paused.liveSources !== 0 || paused.legacyLiveSources !== 0) {
    throw new Error(`music survived pause: ${JSON.stringify(paused)}`);
  }
  console.log(JSON.stringify({ ok: true, url, menu, game, switched, nextChapter, paused }));
}

smoke().catch(error => { console.error(error.stack || error); process.exitCode = 1; })
  .finally(() => child.kill());
