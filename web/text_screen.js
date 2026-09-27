'use strict';

// A screen the plugin sends as text rather than as a picture — the ToLiss
// MCDU, which X-Plane never renders into a framebuffer of its own. Every event
// on /screen/<slug> carries the whole display: one entry per row with a
// character, a colour letter and a size letter per column.

const COLUMNS = 24;
const ROWS = 14;

function createCells(node) {
    const cells = [];
    for (let index = 0; index < COLUMNS * ROWS; index += 1) {
        const cell = document.createElement('span');
        cell.className = 'cell';
        cells.push(cell);
    }
    node.append(...cells);
    return cells;
}

// Only cells that changed are touched: typing into the scratchpad changes one
// character, and the page should not repaint the other 335 for it.
function paintRow(cells, rowIndex, row) {
    const glyphs = Array.from(row.text);

    for (let column = 0; column < COLUMNS; column += 1) {
        const cell = cells[rowIndex * COLUMNS + column];
        const glyph = glyphs[column] ?? ' ';
        const color = row.colors[column] ?? 'w';
        const size = row.sizes[column] ?? 'L';

        if (cell.textContent !== glyph) {
            cell.textContent = glyph;
        }
        if (cell.dataset.color !== color) {
            cell.dataset.color = color;
        }
        if (cell.dataset.size !== size) {
            cell.dataset.size = size;
        }
    }
}

// Returns the screen node and `start`, which (re)opens the event stream.
// EventSource reconnects on its own after a dropout; `start` is for the caller
// who knows better, e.g. after the control socket came back.
export function createTextScreen(slug) {
    const node = document.createElement('div');
    node.className = 'screen text-screen';
    const cells = createCells(node);

    let source = null;

    function start() {
        if (source) {
            source.close();
        }

        source = new EventSource(`/screen/${slug}`);
        source.addEventListener('message', event => {
            const { rows } = JSON.parse(event.data);
            rows.slice(0, ROWS).forEach((row, index) => paintRow(cells, index, row));
        });
    }

    return { node, start };
}
