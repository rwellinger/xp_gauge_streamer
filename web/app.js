'use strict';

// The bezel is drawn here rather than captured: X-Plane only hands out the
// GNS screen, never its frame. Labels follow the real unit, the grouping
// follows what a finger can hit — button names are the sim's command suffixes.
const BEZEL_GROUPS = [
    {
        label: 'Range',
        keys: [['RNG −', 'zoom_out'], ['RNG +', 'zoom_in'], ['D→', 'direct'], ['MENU', 'menu']],
    },
    {
        label: 'Entry',
        keys: [['CLR', 'clr'], ['ENT', 'ent'], ['CRSR', 'cursor'], ['MSG', 'msg']],
    },
    {
        label: 'Pages',
        keys: [['CHAP ◀', 'chapter_dn'], ['CHAP ▶', 'chapter_up'], ['PAGE ◀', 'page_dn'], ['PAGE ▶', 'page_up']],
    },
    {
        label: 'Knob',
        keys: [['OUTER −', 'coarse_down'], ['OUTER +', 'coarse_up'], ['INNER −', 'fine_down'], ['INNER +', 'fine_up']],
    },
    {
        label: 'Nav',
        keys: [['FPL', 'fpl'], ['PROC', 'proc'], ['VNAV', 'vnav'], ['OBS', 'obs'], ['CDI', 'cdi']],
    },
    {
        label: 'Radio',
        keys: [['COM ⇄', 'com_ff'], ['NAV ⇄', 'nav_ff'], ['NAV/COM', 'nav_com_tog']],
    },
];

const FIRST_RETRY_MS = 500;
const LONGEST_RETRY_MS = 5000;

const elements = {
    devices: document.getElementById('devices'),
    status: document.getElementById('status'),
    stream: document.getElementById('stream'),
    bezel: document.getElementById('bezel'),
};

let socket = null;
let selected = null;
let retryDelay = FIRST_RETRY_MS;

// ── Status ───────────────────────────────────────────────────────────────────

// A dead frontend that looks alive is worse in flight than a visible error.
function setStatus(state, text) {
    elements.status.dataset.state = state;
    elements.status.textContent = text;
}

// ── Sending ──────────────────────────────────────────────────────────────────

function press(button) {
    if (!selected || !socket || socket.readyState !== WebSocket.OPEN) {
        return;
    }
    socket.send(JSON.stringify({ device: selected, button }));
}

function buildBezel() {
    elements.bezel.replaceChildren(...BEZEL_GROUPS.map(group => {
        const section = document.createElement('div');
        section.className = 'group';

        const label = document.createElement('span');
        label.className = 'group-label';
        label.textContent = group.label;
        section.append(label);

        for (const [caption, button] of group.keys) {
            const key = document.createElement('button');
            key.textContent = caption;
            key.type = 'button';

            // On pointerdown, not click: a press should register the moment the
            // finger lands, without waiting out a possible double tap.
            key.addEventListener('pointerdown', event => {
                event.preventDefault();
                key.dataset.pressed = 'true';
                press(button);
            });
            const release = () => delete key.dataset.pressed;
            key.addEventListener('pointerup', release);
            key.addEventListener('pointercancel', release);
            key.addEventListener('pointerleave', release);

            // detail === 0 means the click came from the keyboard, where no
            // pointerdown fired.
            key.addEventListener('click', event => {
                if (event.detail === 0) {
                    press(button);
                }
            });

            section.append(key);
        }

        return section;
    }));
}

// ── Devices ──────────────────────────────────────────────────────────────────

function selectDevice(slug) {
    selected = slug;
    location.hash = slug;
    // The timestamp forces a fresh request even when the same unit is selected
    // again — after a reconnect the old stream is a dead socket, not an image.
    elements.stream.src = `/stream/${slug}?t=${Date.now()}`;

    for (const button of elements.devices.children) {
        button.setAttribute('aria-current', String(button.value === slug));
    }
}

function showDevices(devices) {
    elements.devices.replaceChildren(...devices.map(device => {
        const button = document.createElement('button');
        button.type = 'button';
        button.textContent = device.name;
        button.value = device.slug;
        button.addEventListener('click', () => selectDevice(device.slug));
        return button;
    }));

    // The hash keeps the choice across reloads and makes a single unit
    // linkable — a tablet can bookmark just its own screen.
    const wanted = location.hash.slice(1);
    const known = devices.some(device => device.slug === wanted);
    selectDevice(known ? wanted : devices[0].slug);
}

// Asked again on every reconnect: after an X-Plane restart the aircraft, and
// with it the set of units, may be a different one.
function refreshDevices() {
    fetch('/devices')
        .then(response => response.json())
        .then(({ devices }) => {
            const streaming = devices.filter(device => device.streaming);
            if (streaming.length === 0) {
                elements.devices.replaceChildren();
                elements.stream.removeAttribute('src');
                setStatus('dead', 'No GNS unit in this aircraft');
                return;
            }

            showDevices(streaming);
            setStatus('live', 'Connected');
        })
        .catch(() => setStatus('dead', 'Plugin not reachable'));
}

// ── Connection ───────────────────────────────────────────────────────────────

function connect() {
    setStatus('connecting', 'Connecting…');
    socket = new WebSocket(`ws://${location.host}/control`);

    socket.addEventListener('open', () => {
        retryDelay = FIRST_RETRY_MS;
        refreshDevices();
    });

    // Covers an X-Plane restart as well as a WLAN dropout: back off a little
    // further each time, but keep trying so the page recovers on its own.
    socket.addEventListener('close', () => {
        socket = null;
        setStatus('dead', 'Reconnecting…');
        setTimeout(connect, retryDelay);
        retryDelay = Math.min(retryDelay * 2, LONGEST_RETRY_MS);
    });

    socket.addEventListener('error', () => socket && socket.close());
}

buildBezel();
connect();
