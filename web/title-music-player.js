/* Bounded finite-source player for the generated menu score. */
(function(root) {
  'use strict';

  function createTitleMusicPlayer(options) {
    const score = options.score, maxEvents = options.maxEvents || 32, maxSources = options.maxSources || 64;
    const sources = new Set();
    let running = false, index = 0, loopStart = 0, lastScheduled = 0;
    let totalScheduled = 0, totalSources = 0, peakSources = 0, faults = 0, invalidStops = 0;
    let completedLoops = 0, furthestIndex = 0, discontinuityResets = 0;
    const context = () => options.context();
    const musicGain = () => options.musicGain();
    const noiseBuffer = () => options.noiseBuffer();
    const frequency = note => 440 * Math.pow(2, (note - 69) / 12);

    function remember(source, stopAt) {
      if (!Number.isFinite(stopAt)) { invalidStops++; throw new Error('non-finite title source stop'); }
      const record = { source, stopAt };
      sources.add(record); totalSources++;
      source.addEventListener('ended', () => sources.delete(record), { once: true });
      source.stop(stopAt);
      if (sources.size > peakSources) peakSources = sources.size;
    }
    function output(pan, cutoff) {
      const ac = context(), gain = ac.createGain(), filter = ac.createBiquadFilter();
      filter.type = 'lowpass'; filter.frequency.value = cutoff; gain.connect(filter);
      if (ac.createStereoPanner) {
        const panner = ac.createStereoPanner(); panner.pan.value = pan;
        filter.connect(panner); panner.connect(musicGain());
      } else filter.connect(musicGain());
      return gain;
    }
    function tone(channel, note, velocity, when, duration) {
      const ac = context(), profiles = [
        { type: 'sawtooth', volume: .085, pan: -.12, cutoff: 3400, attack: .012 },
        { type: 'triangle', volume: .075, pan: .22, cutoff: 1800, attack: .045 },
        { type: 'square', volume: .095, pan: -.28, cutoff: 920, attack: .008 }
      ];
      const profile = profiles[channel], end = when + Math.max(.04, duration), oscillators = [];
      const gain = output(profile.pan, profile.cutoff), volume = profile.volume * velocity / 100;
      gain.gain.setValueAtTime(.0001, when);
      gain.gain.exponentialRampToValueAtTime(volume, when + Math.min(profile.attack, duration * .3));
      gain.gain.setValueAtTime(volume, Math.max(when + .01, end - .045));
      gain.gain.exponentialRampToValueAtTime(.0001, end);
      const oscillator = ac.createOscillator(); oscillator.type = profile.type;
      oscillator.frequency.setValueAtTime(frequency(note), when);
      oscillator.connect(gain); oscillator.start(when); oscillators.push(oscillator);
      if (channel === 0) {
        const chorus = ac.createOscillator(), chorusGain = ac.createGain();
        chorus.type = 'square'; chorus.frequency.setValueAtTime(frequency(note) * 1.003, when);
        chorusGain.gain.value = .18; chorus.connect(chorusGain); chorusGain.connect(gain);
        chorus.start(when); oscillators.push(chorus);
      }
      for (const source of oscillators) remember(source, end + .035);
    }
    function drum(note, velocity, when, duration) {
      const ac = context(), life = Math.min(duration, note === 42 ? .055 : .16);
      const end = when + Math.max(.03, life);
      if (note === 36) {
        const oscillator = ac.createOscillator(), gain = output(0, 1300);
        oscillator.type = 'sine'; oscillator.frequency.setValueAtTime(150, when);
        oscillator.frequency.exponentialRampToValueAtTime(48, end);
        gain.gain.setValueAtTime(.17 * velocity / 100, when);
        gain.gain.exponentialRampToValueAtTime(.0001, end);
        oscillator.connect(gain); oscillator.start(when); remember(oscillator, end + .025); return;
      }
      const source = ac.createBufferSource(), filter = ac.createBiquadFilter(), gain = ac.createGain();
      source.buffer = noiseBuffer(); source.loop = true; filter.type = note === 42 ? 'highpass' : 'bandpass';
      filter.frequency.value = note === 42 ? 6200 : 1900;
      gain.gain.setValueAtTime((note === 42 ? .045 : .09) * velocity / 100, when);
      gain.gain.exponentialRampToValueAtTime(.0001, end);
      source.connect(filter); filter.connect(gain); gain.connect(musicGain());
      source.start(when); remember(source, end + .025);
    }
    function stop() {
      running = false; index = 0; loopStart = 0; lastScheduled = 0;
      const ac = context(), stopAt = ac ? ac.currentTime + .01 : 0;
      for (const record of sources) { try { record.source.stop(stopAt); } catch (_) {} }
      sources.clear();
    }
    function reset(delay) {
      stop();
      const ac = context();
      if (!ac || ac.state !== 'running') return;
      running = true; loopStart = ac.currentTime + (delay === undefined ? .06 : delay);
    }
    function start() { if (!running) reset(); }
    function normalizeCursor() {
      if (index < score.notes.length) return;
      index = 0;
      loopStart += score.loop / score.hz;
      completedLoops++;
    }
    function schedule() {
      const ac = context();
      if (!ac || ac.state !== 'running') return;
      if (!running) start();
      if (!running) return;
      normalizeCursor();
      const pendingAt = loopStart + score.notes[index][0] / score.hz;
      if (pendingAt < ac.currentTime - .1) {
        discontinuityResets++;
        reset(.05);
      }
      const horizon = ac.currentTime + .30;
      let dispatched = 0;
      while (dispatched < maxEvents) {
        normalizeCursor();
        const span = score.notes[index], when = loopStart + span[0] / score.hz;
        if (when >= horizon) break;
        if (sources.size >= maxSources - 4) { faults++; reset(.08); return; }
        const duration = span[1] / score.hz;
        if (span[2] === 3) drum(span[3], span[5], when, duration);
        else tone(span[2], span[3], span[5], when, duration);
        index++; dispatched++; totalScheduled++;
        if (index > furthestIndex) furthestIndex = index;
      }
      lastScheduled = dispatched;
      if (dispatched === maxEvents) { faults++; reset(.08); }
    }
    function debug() {
      const ac = context();
      return { completedLoops, context: ac ? ac.state : 'none', discontinuityResets,
        faults, furthestIndex, index, invalidStops,
        liveSources: sources.size, peakSources, running, scheduledThisFrame: lastScheduled,
        totalScheduled, totalSources };
    }
    return { debug, reset, schedule, start, stop };
  }

  root.createTitleMusicPlayer = createTitleMusicPlayer;
  if (typeof module !== 'undefined' && module.exports) module.exports = { createTitleMusicPlayer };
})(typeof globalThis !== 'undefined' ? globalThis : this);
