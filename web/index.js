'use strict';

// The selection page. It shows what the aircraft has and links to the device
// pages — it opens no stream, so nothing here costs the sim capture time.

import { loadBezel, renderThumbnail } from '/bezel.js';

// An aircraft change swaps the whole panel, and this page may sit open on the
// tablet across one. Asking every few seconds costs a small JSON reply.
const REFRESH_INTERVAL_MS = 5000;

const elements = {
    devices: document.getElementById('devices'),
    status: document.getElementById('status'),
};

// A dead frontend that looks alive is worse in flight than a visible error.
function setStatus(state, text) {
    elements.status.dataset.state = state;
    elements.status.textContent = text;
}

function renderTile(device, definition) {
    const tile = document.createElement(device.present ? 'a' : 'div');
    tile.className = 'tile';

    if (device.present) {
        tile.href = `/device/${device.slug}`;
    } else {
        tile.dataset.absent = 'true';
    }

    const name = document.createElement('span');
    name.className = 'tile-name';
    name.textContent = device.name;

    const state = document.createElement('span');
    state.className = 'tile-state';
    state.textContent = device.present ? 'In this aircraft' : 'Not in this aircraft';

    tile.append(renderThumbnail(definition), name, state);
    return tile;
}

// A device whose bezel definition is missing is a broken install, not an empty
// aircraft — say so instead of hiding the unit.
function tileFor(device) {
    return loadBezel(device.type)
        .then(definition => renderTile(device, definition))
        .catch(() => {
            const broken = document.createElement('div');
            broken.className = 'tile';
            broken.dataset.absent = 'true';
            broken.textContent = `${device.name}: no bezel for type ${device.type}`;
            return broken;
        });
}

function showDevices(devices) {
    Promise.all(devices.map(tileFor)).then(tiles => elements.devices.replaceChildren(...tiles));

    const present = devices.filter(device => device.present).length;
    setStatus(present > 0 ? 'live' : 'dead',
              present > 0 ? `${present} of ${devices.length} in this aircraft` : 'No unit in this aircraft');
}

// Rebuilding the tiles on every poll would flicker for nothing — the answer
// only changes when the aircraft does.
let lastAnswer = '';

function refreshDevices() {
    fetch('/devices')
        .then(response => response.text())
        .then(answer => {
            if (answer === lastAnswer) {
                return;
            }
            lastAnswer = answer;
            showDevices(JSON.parse(answer).devices);
        })
        .catch(() => {
            lastAnswer = '';
            setStatus('dead', 'Plugin not reachable');
        });
}

refreshDevices();
setInterval(refreshDevices, REFRESH_INTERVAL_MS);
