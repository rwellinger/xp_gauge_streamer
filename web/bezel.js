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

function key(className, label, command, press) {
    const node = document.createElement('button');
    node.type = 'button';
    node.className = className;
    node.textContent = label;
    attachPress(node, command, press);
    return node;
}

// ── Control kinds ────────────────────────────────────────────────────────────

function renderButton(control, press) {
    return key('key', control.label, control.command, press);
}

function renderRocker(control, press) {
    const node = document.createElement('div');
    node.className = `rocker ${control.orientation === 'vertical' ? 'vertical' : 'horizontal'}`;
    node.append(...control.segments.map(segment => key('key', segment.label, segment.command, press)));
    return node;
}

// Rings are drawn outermost first, each one covering the middle of the one
// before it — that is what makes a dual concentric knob read as one control
// rather than as four keys.
function renderKnob(control, press) {
    const node = document.createElement('div');
    node.className = 'knob';

    const face = document.createElement('div');
    face.className = 'face';

    // Rings share whatever the centre leaves them, so every band stays wide
    // enough for a finger however many rings a device stacks.
    const coreInset = control.press ? 30 : 50;
    const bandWidth = coreInset / control.rings.length;

    control.rings.forEach((ring, depth) => {
        const halves = document.createElement('div');
        halves.className = 'ring';
        halves.dataset.depth = String(depth);
        halves.style.inset = `${depth * bandWidth}%`;
        halves.append(key('half decrease', '−', ring.decrease, press),
                      key('half increase', '+', ring.increase, press));
        face.append(halves);
    });

    if (control.press) {
        const centre = key('press', control.press.label, control.press.command, press);
        centre.style.inset = `${coreInset}%`;
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
};

// ── Bezel ────────────────────────────────────────────────────────────────────

// Returns the bezel and its screen element; the caller decides what the screen
// shows, so the renderer stays free of streaming concerns.
export function renderBezel(definition, press) {
    const bezel = document.createElement('div');
    bezel.className = 'bezel';

    const screen = document.createElement('img');
    screen.className = 'screen';
    screen.alt = `${definition.name} screen`;
    place(screen, definition.screen, definition.size);
    bezel.append(screen);

    for (const control of definition.controls) {
        const render = CONTROL_KINDS[control.kind];
        if (!render) {
            console.warn(`unknown control kind: ${control.kind}`);
            continue;
        }

        const node = render(control, press);
        node.classList.add('control');
        place(node, control.box, definition.size);
        bezel.append(node);
    }

    return { bezel, screen };
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

function thumbnailShape(control) {
    const box = control.box;
    if (control.kind !== 'knob') {
        return shape('rect', { x: box.x, y: box.y, width: box.width, height: box.height, rx: 6, class: 'sketch-key' });
    }

    const radius = box.width / 2;
    return shape('circle', { cx: box.x + radius, cy: box.y + radius, r: radius, class: 'sketch-key' });
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
                  ...controls.map(thumbnailShape));

    return sketch;
}
