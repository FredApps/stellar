'use strict';
const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const path = require('path');
const { createTitleMusicPlayer } = require('../../web/title-music-player.js');

const dataPath = path.join(__dirname, '../../web/title-music-data.js');
const score = vm.runInNewContext(fs.readFileSync(dataPath, 'utf8') + ';TITLE_MUSIC_SCORE');

class MockParam {
  constructor(record) { this.value = 0; this.record = record; }
  cancelScheduledValues() {}
  exponentialRampToValueAtTime() {}
  linearRampToValueAtTime() {}
  setTargetAtTime() {}
  setValueAtTime(value) { if (this.record) this.record(value); }
}
class MockNode {
  constructor(context, source) {
    this.context = context; this.source = source;
    this.frequency = new MockParam(source ? value => context.frequencies.push(value) : null);
    this.gain = new MockParam(); this.pan = new MockParam(); this.listeners = [];
    this.stopAt = Infinity; this.stopCalls = 0; this.ended = false;
    if (source) context.sources.push(this);
  }
  addEventListener(type, callback) { if (type === 'ended') this.listeners.push(callback); }
  connect() { return this; }
  start() {}
  stop(when) {
    assert(Number.isFinite(when), 'every source must have a finite stop time');
    this.stopAt = Math.min(this.stopAt, when); this.stopCalls++;
  }
  finish() {
    if (this.ended) return; this.ended = true;
    for (const callback of this.listeners) callback();
  }
}
class MockContext {
  constructor() { this.currentTime = 0; this.state = 'running'; this.sources = []; this.frequencies = []; this.destination = new MockNode(this, false); }
  createBiquadFilter() { return new MockNode(this, false); }
  createBufferSource() { return new MockNode(this, true); }
  createGain() { return new MockNode(this, false); }
  createOscillator() { return new MockNode(this, true); }
  createStereoPanner() { return new MockNode(this, false); }
  advance(seconds) {
    this.currentTime += seconds;
    for (const source of this.sources) if (!source.ended && source.stopAt <= this.currentTime) source.finish();
  }
}

const context = new MockContext(), output = new MockNode(context, false);
const player = createTitleMusicPlayer({
  context: () => context, musicGain: () => output, noiseBuffer: () => ({}),
  score, maxEvents: 32, maxSources: 64
});

player.reset(.01);
for (let step = 0; step < 12000; step++) {
  context.advance(.05); player.schedule();
  assert(player.debug().liveSources < 64, 'live source limit exceeded');
}
let result = player.debug();
assert.strictEqual(result.faults, 0, 'ten-minute soak hit a scheduler fault');
assert.strictEqual(result.invalidStops, 0, 'invalid source stop detected');
assert(result.totalScheduled > 6000, 'title loop did not continue through soak');
assert(result.completedLoops >= 50, 'title cursor did not complete full loops');
assert.strictEqual(result.furthestIndex, score.notes.length, 'title cursor did not traverse the full score');
assert.strictEqual(result.discontinuityResets, 0, 'continuous playback incorrectly reset the cursor');
const scoreFrequencies = new Set(context.frequencies.filter(value => value > 200).map(value => Math.round(value)));
assert(scoreFrequencies.size >= 10, 'title playback did not produce sufficient pitch variety');
assert(scoreFrequencies.has(Math.round(440 * Math.pow(2, (74 - 69) / 12))), 'late title melody pitch was never played');
assert(result.peakSources < 32, 'unexpected title polyphony growth');
assert(context.sources.every(source => source.stopCalls > 0), 'a source was created without stop()');

for (let i = 0; i < 100; i++) {
  player.reset(.01); player.schedule(); player.stop(); context.advance(.02);
  assert.strictEqual(player.debug().liveSources, 0, 'reset leaked a source');
}
context.state = 'suspended'; player.start(); player.schedule();
assert.strictEqual(player.debug().running, false, 'suspended context started title music');
context.state = 'running'; player.reset(); player.schedule(); player.stop();
assert.strictEqual(player.debug().liveSources, 0, 'track transition leaked a source');

result = player.debug();
console.log(JSON.stringify({ ok: true, peakSources: result.peakSources,
  simulatedSeconds: 600, totalSources: result.totalSources }));
