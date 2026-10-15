// Nexo website — web emulator front-end.
// Loads the WebAssembly build of the real firmware UI (website/assets/emu/)
// and drives it with on-screen keys + the physical keyboard.
// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
(function () {
  'use strict';

  const canvas = document.getElementById('emu-canvas');
  const stage = document.getElementById('emu');
  const statusEl = document.getElementById('emu-status');
  const input = document.getElementById('emu-input');

  function fail(msg) {
    if (stage) stage.classList.add('emu-failed');
    statusEl.textContent = msg;
  }

  if (typeof createNexoEmu !== 'function') {
    fail('Emulator build not found. Run the "emulator" GitHub Actions workflow.');
    return;
  }

  let ctx = canvas.getContext('2d');
  let cw = 240, ch = 135;

  createNexoEmu().then(function (Mod) {
    const press = Mod.cwrap('nexoemu_press', 'void', ['string']);
    const typeChar = Mod.cwrap('nexoemu_char', 'void', ['string']);
    const tick = Mod.cwrap('nexoemu_tick', 'void', []);
    const frame = Mod.cwrap('nexoemu_frame', 'number', []);

    cw = Mod.cwrap('nexoemu_width', 'number', [])();
    ch = Mod.cwrap('nexoemu_height', 'number', [])();
    canvas.width = cw;
    canvas.height = ch;

    let img = ctx.createImageData(cw, ch);

    function paint() {
      const ptr = frame();
      const src = Mod.HEAPU8.subarray(ptr, ptr + cw * ch * 3);
      const data = img.data;
      for (let i = 0, j = 0; i < cw * ch; i++) {
        data[j++] = src[i * 3];
        data[j++] = src[i * 3 + 1];
        data[j++] = src[i * 3 + 2];
        data[j++] = 255;
      }
      ctx.putImageData(img, 0, 0);
    }

    window.nexoBeep = function (freq, ms) {
      try {
        const AC = window.AudioContext || window.webkitAudioContext;
        if (!AC) return;
        const ac = (window.nexoBeep._ac = window.nexoBeep._ac || new AC());
        const osc = ac.createOscillator();
        const gain = ac.createGain();
        osc.frequency.value = freq;
        osc.type = 'square';
        gain.gain.value = 0.05;
        osc.connect(gain).connect(ac.destination);
        osc.start();
        osc.stop(ac.currentTime + Math.max(0.02, ms / 1000));
      } catch (_) {}
    };

    Mod._nexoemu_init();
    paint();
    statusEl.textContent = 'Interactive — use the keys below or your keyboard.';

    // Keep timers/toasts alive with a lightweight tick.
    setInterval(function () {
      tick();
      paint();
    }, 500);

    function doKey(name) {
      press(name);
      paint();
    }
    function doChar(c) {
      typeChar(c);
      paint();
    }

    document.querySelectorAll('[data-key]').forEach(function (el) {
      el.addEventListener('click', function () {
        doKey(el.getAttribute('data-key'));
      });
    });

    const MAP = {
      ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right',
      Enter: 'enter', Escape: 'esc', Backspace: 'backspace', Delete: 'del',
      Tab: 'tab', ' ': 'space'
    };
    window.addEventListener('keydown', function (e) {
      if (document.activeElement === input) return;
      if (MAP[e.key]) {
        e.preventDefault();
        doKey(MAP[e.key]);
      } else if (e.key.length === 1) {
        e.preventDefault();
        doChar(e.key);
      }
    });

    input.addEventListener('keydown', function (e) {
      if (e.key === 'Enter') {
        e.preventDefault();
        doKey('enter');
        input.value = '';
        return;
      }
      if (e.key === 'Backspace') {
        e.preventDefault();
        if (input.value) input.value = input.value.slice(0, -1);
        doKey('backspace');
        return;
      }
      if (e.key === 'Escape') {
        input.value = '';
        doKey('esc');
        return;
      }
      if (e.key.length === 1) {
        e.preventDefault();
        input.value += e.key;
        doChar(e.key);
      }
    });
  }).catch(function (err) {
    fail('Failed to load emulator: ' + err);
  });
})();
