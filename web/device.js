'use strict';

// One device, one bezel. Which bezel is decided by the device's type, and the
// layout comes from that type's definition — this file knows about no device
// in particular.

import { attachKeyboard, fitBezel, loadBezel, renderBezel } from '/bezel.js';
import { createTextScreen } from '/text_screen.js';

const FIRST_RETRY_MS = 500;
const LONGEST_RETRY_MS = 5000;

const elements = {
    panel: document.getElementById('panel'),
    name: document.getElementById('device-name'),
    status: document.getElementById('status'),
};

const slug = decodeURIComponent(location.pathname.split('/').pop());

let socket = null;
let retryDelay = FIRST_RETRY_MS;

// A dead frontend that looks alive is worse in flight than a visible error.
function setStatus(state, text) {
    elements.status.dataset.state = state;
    elements.status.textContent = text;
}

function press(command) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
        return;
    }
    socket.send(JSON.stringify({ device: slug, button: command }));
}

// ── Page ─────────────────────────────────────────────────────────────────────

// The timestamp forces a fresh request even for the same unit — after a
// reconnect the old stream is a dead socket, not an image.
function createPictureScreen(name) {
    const node = document.createElement('img');
    node.className = 'screen';
    node.alt = `${name} screen`;

    return { node, start: () => { node.src = `/stream/${slug}?t=${Date.now()}`; } };
}

// The plugin says how a unit's screen travels: as a picture of the framebuffer
// or, for a display X-Plane does not render itself, as text.
function createScreen(device, definition) {
    return device.screen === 'text' ? createTextScreen(slug) : createPictureScreen(definition.name);
}

function showBezel(device, definition) {
    const screen = createScreen(device, definition);
    const bezel = renderBezel(definition, press, screen.node);

    elements.name.textContent = device.name;
    elements.panel.replaceChildren(bezel);

    const fit = () => fitBezel(bezel, definition.size, elements.panel);
    fit();
    new ResizeObserver(fit).observe(elements.panel);
    attachKeyboard(bezel);

    return screen;
}

function reportProblem(text) {
    elements.name.textContent = slug;
    elements.panel.replaceChildren();
    setStatus('dead', text);
}

// ── Connection ───────────────────────────────────────────────────────────────

function connect(screen) {
    setStatus('connecting', 'Connecting…');
    socket = new WebSocket(`ws://${location.host}/control`);

    socket.addEventListener('open', () => {
        retryDelay = FIRST_RETRY_MS;
        screen.start();
        setStatus('live', 'Connected');
    });

    // Covers an X-Plane restart as well as a WLAN dropout: back off a little
    // further each time, but keep trying so the page recovers on its own.
    socket.addEventListener('close', () => {
        socket = null;
        setStatus('dead', 'Reconnecting…');
        setTimeout(() => connect(screen), retryDelay);
        retryDelay = Math.min(retryDelay * 2, LONGEST_RETRY_MS);
    });

    socket.addEventListener('error', () => socket && socket.close());
}

// ── Start ────────────────────────────────────────────────────────────────────

const PRESENCE_RETRY_MS = 5000;

function describeDevice() {
    return fetch('/devices')
        .then(response => response.json())
        .then(({ devices }) => devices.find(entry => entry.slug === slug));
}

// Streaming a unit the aircraft does not have would register a viewer and start
// capture for a black picture. The bezel is drawn all the same, so the page
// stays recognisable, and the check repeats — the next aircraft may have it.
function waitForAircraft(screen) {
    describeDevice()
        .then(device => {
            if (device && device.present) {
                connect(screen);
                return;
            }

            setStatus('dead', 'Not in this aircraft');
            setTimeout(() => waitForAircraft(screen), PRESENCE_RETRY_MS);
        })
        .catch(() => {
            setStatus('dead', 'Plugin not reachable');
            setTimeout(() => waitForAircraft(screen), PRESENCE_RETRY_MS);
        });
}

describeDevice()
    .then(device => {
        if (!device) {
            throw new Error(`unknown device ${slug}`);
        }

        return loadBezel(device.type).then(definition => waitForAircraft(showBezel(device, definition)));
    })
    .catch(() => reportProblem(`Cannot open ${slug}`));
