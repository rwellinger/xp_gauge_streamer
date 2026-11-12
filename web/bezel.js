'use strict';

// Turns a bezel definition into a page. The definition owns the layout — where
// a key sits, what it is called, which command suffix it sends — so a new
// device type is a new JSON file, never a change in here.
//
// Control kinds:
//   button  one key, one command
//   rocker  two keys sharing one body, e.g. RNG − / RNG +
//   knob    concentric rings, each with an increase and a decrease half, and an
//           optional press in the centre
//   grid    a block of equally sized keys in `columns` columns — a CDU's line
//           selects, function keys and letter pad, which would otherwise be
//           dozens of hand-placed boxes

const definitions = new Map();

export function loadBezel(type) {
    if (!definitions.has(type)) {
        definitions.set(type, fetch(`/bezels/${type}.json`).then(response => {
            if (!response.ok) {
                throw new Error(`no bezel definition for ${type}`);
            }
            return response.json();
        }));
    }
    return definitions.get(type);
}

// ── Geometry ─────────────────────────────────────────────────────────────────

// Boxes are given in the definition's own units; everything is placed relative
// to the bezel, so the whole panel scales with the viewport.
function place(node, box, size) {
    node.style.left = `${(box.x / size.width) * 100}%`;
    node.style.top = `${(box.y / size.height) * 100}%`;
    node.style.width = `${(box.width / size.width) * 100}%`;
    node.style.height = `${(box.height / size.height) * 100}%`;
}

// The bezel keeps its aspect ratio and takes whichever dimension runs out
// first — landscape on a tablet then needs no scrolling.
export function fitBezel(bezel, size, container) {
    const scale = Math.min(container.clientWidth / size.width, container.clientHeight / size.height);

    bezel.style.width = `${size.width * scale}px`;
    bezel.style.height = `${size.height * scale}px`;
    // Lets labels grow with the bezel instead of staying at a fixed pixel size.
    bezel.style.setProperty('--unit', `${scale}px`);
}

// ── Keys ─────────────────────────────────────────────────────────────────────

function attachPress(key, command, press) {
    // On pointerdown, not click: a press should register the moment the finger
    // lands, without waiting out a possible double tap.
    key.addEventListener('pointerdown', event => {
        event.preventDefault();
        key.dataset.pressed = 'true';
        press(command);
    });

    const release = () => delete key.dataset.pressed;
    key.addEventListener('pointerup', release);
    key.addEventListener('pointercancel', release);
    key.addEventListener('pointerleave', release);

    // detail === 0 means the click came from the keyboard, where no pointerdown
    // fired.
    key.addEventListener('click', event => {
        if (event.detail === 0) {
            press(command);
        }
    });
}

// Which physical key reaches this one. A caption of a single character stands
// for itself, so a whole letter pad needs no annotation; anything else says so
// with `shortcut`, using the name the browser reports (" ", "Backspace", …).
// A definition that wants no keyboard at all simply names none.
function shortcutFor(entry) {
    if (entry.shortcut) {
        return entry.shortcut.toLowerCase();
    }
    return entry.label && [...entry.label].length === 1 ? entry.label.toLowerCase() : null;
}

function key(className, label, command, press, shortcut) {
    const node = document.createElement('button');
    node.type = 'button';
    node.className = className;
    node.textContent = label;

    if (shortcut) {
        node.dataset.shortcut = shortcut;
    }

    attachPress(node, command, press);
    return node;
}

// ── Control kinds ────────────────────────────────────────────────────────────

function renderButton(control, press) {
    return key('key', control.label, control.command, press, shortcutFor(control));
}

function renderRocker(control, press) {
    const node = document.createElement('div');
    node.className = `rocker ${control.orientation === 'vertical' ? 'vertical' : 'horizontal'}`;
    node.append(...control.segments.map(segment =>
        key('key', segment.label, segment.command, press, shortcutFor(segment))));
    return node;
}

// An entry without a command leaves its cell empty, so a block can skip a
// position without splitting into two controls.
function renderGrid(control, press) {
    const node = document.createElement('div');
    node.className = 'grid';
    node.style.gridTemplateColumns = `repeat(${control.columns}, 1fr)`;
    node.append(...control.keys.map(entry => entry.command
        ? key('key', entry.label, entry.command, press, shortcutFor(entry))
        : document.createElement('span')));
    return node;
}

// Angles run counter-clockwise from "east", the way a unit circle does; the
// screen's y axis points the other way, hence the minus on every sine.
const CENTRE_INSET = 30;
const GLYPH_RADIUS = 34;
const SECTOR_GAP_DEGREES = 1.5;

// Reaches past the circle into the square's corners: a press that lands just
// outside the drawn rim still belongs to the sector under the finger.
const SECTOR_RADIUS = 75;

function sectorClipPath(centreAngle, halfWidth) {
    const points = ['50% 50%'];

    for (let step = 0; step <= 8; step += 1) {
        const radians = ((centreAngle - halfWidth + (2 * halfWidth * step) / 8) * Math.PI) / 180;
        points.push(`${50 + SECTOR_RADIUS * Math.cos(radians)}% ${50 - SECTOR_RADIUS * Math.sin(radians)}%`);
    }

    return `polygon(${points.join(', ')})`;
}

function sector(command, glyph, centreAngle, halfWidth, depth, press) {
    const node = key('half', '', command, press);
    node.dataset.depth = String(depth);
    node.style.clipPath = sectorClipPath(centreAngle, halfWidth - SECTOR_GAP_DEGREES);

    const radians = (centreAngle * Math.PI) / 180;
    const label = document.createElement('span');
    label.className = 'glyph';
    label.textContent = glyph;
    label.style.left = `${50 + GLYPH_RADIUS * Math.cos(radians)}%`;
    label.style.top = `${50 - GLYPH_RADIUS * Math.sin(radians)}%`;

    node.append(label);
    return node;
}

// Rings split the face by angle, not by radius: every ring keeps the full
// radius, so a sector stays wide enough for a finger however many rings a
// device stacks. Two rings give the familiar cross — outer left/right, inner
// up/down — around the press in the centre.
function renderKnob(control, press) {
    const node = document.createElement('div');
    node.className = 'knob';

    const face = document.createElement('div');
    face.className = 'face';

    const sectorHalfWidth = 90 / control.rings.length;

    control.rings.forEach((ring, depth) => {
        // Every ring inwards turns the pair a sector further round, so the
        // second one ends up increasing upwards and decreasing downwards.
        const axis = depth * 2 * sectorHalfWidth;
        face.append(sector(ring.increase, '+', axis, sectorHalfWidth, depth, press),
                    sector(ring.decrease, '−', axis + 180, sectorHalfWidth, depth, press));
    });

    if (control.press) {
        const centre = key('press', control.press.label, control.press.command, press);
        centre.style.inset = `${CENTRE_INSET}%`;
        face.append(centre);
    }

    node.append(face);

    if (control.caption) {
        const caption = document.createElement('span');
        caption.className = 'caption';
        caption.textContent = control.caption;
        node.append(caption);
    }

    return node;
}

const CONTROL_KINDS = {
    button: renderButton,
    rocker: renderRocker,
    knob: renderKnob,
    grid: renderGrid,
};

// ── Keyboard ─────────────────────────────────────────────────────────────────

// Long enough to see which key answered, short enough not to lag fast typing.
const KEY_FLASH_MS = 120;

// A CDU is a thing you type on, so where a keyboard exists it should drive the
// keys. Which physical key belongs to which one is the definition's business —
// every key carries its own, and this only looks for a match. A device that
// names none simply has no keyboard.
export function attachKeyboard(bezel) {
    window.addEventListener('keydown', event => {
        // Leave the browser's own shortcuts alone.
        if (event.metaKey || event.ctrlKey || event.altKey) {
            return;
        }

        const target = bezel.querySelector(`[data-shortcut="${CSS.escape(event.key.toLowerCase())}"]`);
        if (!target) {
            return;
        }

        event.preventDefault();
        // The same path a click takes, so a typed press is nothing special.
        target.click();

        target.dataset.pressed = 'true';
        setTimeout(() => delete target.dataset.pressed, KEY_FLASH_MS);
    });
}

// ── Bezel ────────────────────────────────────────────────────────────────────

// The caller hands in the screen node — a streamed picture or a text grid —
// so the renderer stays free of streaming concerns and only places it.
export function renderBezel(definition, press, screen) {
    const bezel = document.createElement('div');
    bezel.className = 'bezel notranslate';
    // Belt and braces with the page's notranslate meta: a key says DES, not
    // "of the", whatever the browser thinks the language is.
    bezel.translate = false;

    place(screen, definition.screen, definition.size);

    // A device may hand out more black margin than it uses. `crop` gives how
    // much of the capture to drop, in its own pixels; the screen box must then
    // carry the ratio of what is left, and the offset decides how the surplus
    // is split between top and bottom.
    const crop = definition.screen.crop;
    if (crop) {
        screen.style.objectFit = 'cover';
        screen.style.objectPosition = `center ${(crop.top / (crop.top + crop.bottom)) * 100}%`;
    }

    bezel.append(screen);

    for (const control of definition.controls) {
        const render = CONTROL_KINDS[control.kind];
        if (!render) {
            console.warn(`unknown control kind: ${control.kind}`);
            continue;
        }

        const node = render(control, press);
        node.classList.add('control');

        // In bezel units like every other measure, so a label keeps its size
        // relative to its key however large the panel is drawn.
        if (control.labelSize) {
            node.style.setProperty('--label-size', control.labelSize);
        }

        place(node, control.box, definition.size);
        bezel.append(node);
    }

    return bezel;
}

// ── Thumbnail ────────────────────────────────────────────────────────────────

const SVG_NAMESPACE = 'http://www.w3.org/2000/svg';

function shape(name, attributes) {
    const node = document.createElementNS(SVG_NAMESPACE, name);
    for (const [attribute, value] of Object.entries(attributes)) {
        node.setAttribute(attribute, value);
    }
    return node;
}

// Returns the shapes for one control: a grid draws a cell per key, so the CDU
// reads as a keypad rather than as one empty slab.
function thumbnailShapes(control) {
    const box = control.box;

    if (control.kind === 'knob') {
        const radius = box.width / 2;
        return [shape('circle', { cx: box.x + radius, cy: box.y + radius, r: radius, class: 'sketch-key' })];
    }

    if (control.kind !== 'grid') {
        return [shape('rect', { x: box.x, y: box.y, width: box.width, height: box.height, rx: 6, class: 'sketch-key' })];
    }

    const columns = control.columns;
    const rows = Math.ceil(control.keys.length / columns);
    const cellWidth = box.width / columns;
    const cellHeight = box.height / rows;
    const inset = Math.min(cellWidth, cellHeight) * 0.12;

    return control.keys.map((entry, index) => entry.command ? shape('rect', {
        x: box.x + (index % columns) * cellWidth + inset,
        y: box.y + Math.floor(index / columns) * cellHeight + inset,
        width: cellWidth - 2 * inset,
        height: cellHeight - 2 * inset,
        rx: 3,
        class: 'sketch-key',
    }) : null).filter(Boolean);
}

// The tile's picture of the device is drawn from the same definition as the
// page itself — a new device type brings its own likeness along.
export function renderThumbnail(definition) {
    const { size, screen, controls } = definition;

    const sketch = shape('svg', {
        viewBox: `0 0 ${size.width} ${size.height}`,
        class: 'sketch',
        role: 'img',
        'aria-label': `${definition.name} layout`,
    });

    sketch.append(shape('rect', { x: 0, y: 0, width: size.width, height: size.height, rx: 12, class: 'sketch-body' }),
                  shape('rect', {
                      x: screen.x, y: screen.y, width: screen.width, height: screen.height, rx: 4,
                      class: 'sketch-screen',
                  }),
                  ...controls.flatMap(thumbnailShapes));

    return sketch;
}
