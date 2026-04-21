/**
 * tree-view.js — Visual tree renderer for MoveTree.
 *
 * Renders the move tree as an interactive horizontal flow:
 *   - Main line flows left → right
 *   - Variations branch downward
 *   - Current node is highlighted
 *   - Click any node to jump to that position
 *
 * Uses pure DOM (no canvas) for accessibility and easy styling.
 */

'use strict';

class TreeView {
  /**
   * @param {HTMLElement} container — DOM element to render into
   * @param {object} opts
   * @param {function(MoveNode): void} opts.onNodeClick — callback when node clicked
   */
  constructor(container, opts = {}) {
    this.container = container;
    this.onNodeClick = opts.onNodeClick || null;
    this.tree = null;
  }

  /**
   * Set the MoveTree and render.
   * @param {MoveTree} tree
   */
  setTree(tree) {
    this.tree = tree;
    this.render();
  }

  /**
   * Re-render the tree (call after navigation/edits).
   */
  render() {
    if (!this.tree || !this.container) return;

    this.container.innerHTML = '';
    const wrapper = document.createElement('div');
    wrapper.className = 'tv-wrapper';

    // Render from root
    this._renderLine(this.tree.root, wrapper, true);

    this.container.appendChild(wrapper);

    // Scroll current node into view
    this._scrollToCurrent();
  }

  /**
   * Render a line of moves (main line from `startNode`).
   * When a node has variations, render them as indented sub-lines below.
   *
   * @param {MoveNode} startNode
   * @param {HTMLElement} parent
   * @param {boolean} isMainLine
   */
  _renderLine(startNode, parent, isMainLine) {
    const lineEl = document.createElement('div');
    lineEl.className = 'tv-line' + (isMainLine ? ' tv-line--main' : ' tv-line--var');

    const nodesRow = document.createElement('div');
    nodesRow.className = 'tv-nodes-row';

    let node = startNode;

    // For root, render a small root indicator then proceed to children
    if (node.isRoot) {
      // Don't render root as a visible node, just proceed
      if (node.children.length === 0) {
        const emptyEl = document.createElement('span');
        emptyEl.className = 'tv-empty';
        emptyEl.textContent = 'Bàn trống';
        nodesRow.appendChild(emptyEl);
        lineEl.appendChild(nodesRow);
        parent.appendChild(lineEl);
        return;
      }
      // Render main line starting from first child
      node = node.children[0];
      // But first render node pill for the first child
    }

    // Walk the main line from this node
    while (node) {
      // Render this node as a pill
      const pill = this._createNodePill(node);
      nodesRow.appendChild(pill);

      // If this node has variations (more than 1 child), render them
      if (node.children.length > 1) {
        // Close current nodes row, attach it
        lineEl.appendChild(nodesRow);

        // Render variation lines
        const varsContainer = document.createElement('div');
        varsContainer.className = 'tv-variations';

        for (let i = 1; i < node.children.length; i++) {
          this._renderLine(node.children[i], varsContainer, false);
        }

        lineEl.appendChild(varsContainer);

        // Continue main line in a new row
        const contRow = document.createElement('div');
        contRow.className = 'tv-nodes-row tv-nodes-row--cont';

        node = node.children[0]; // Follow main line
        // Continue loop with contRow as current row container
        // But we need to add subsequent nodes to contRow
        while (node) {
          const p = this._createNodePill(node);
          contRow.appendChild(p);

          if (node.children.length > 1) {
            lineEl.appendChild(contRow);
            const vc = document.createElement('div');
            vc.className = 'tv-variations';
            for (let j = 1; j < node.children.length; j++) {
              this._renderLine(node.children[j], vc, false);
            }
            lineEl.appendChild(vc);

            const nextRow = document.createElement('div');
            nextRow.className = 'tv-nodes-row tv-nodes-row--cont';
            node = node.children[0];
            // Continue in nextRow
            while (node) {
              nextRow.appendChild(this._createNodePill(node));
              if (node.children.length > 1) break;
              node = node.children.length > 0 ? node.children[0] : null;
            }
            if (nextRow.childNodes.length > 0) lineEl.appendChild(nextRow);
            if (node && node.children.length > 1) continue; // re-enter outer handling
            break;
          }

          node = node.children.length > 0 ? node.children[0] : null;
        }

        if (contRow.childNodes.length > 0 && !contRow.parentElement) {
          lineEl.appendChild(contRow);
        }
        parent.appendChild(lineEl);
        return;
      }

      // No variations — follow main line
      node = node.children.length > 0 ? node.children[0] : null;
    }

    lineEl.appendChild(nodesRow);
    parent.appendChild(lineEl);
  }

  /**
   * Create a clickable pill element for a node.
   * @param {MoveNode} node
   * @returns {HTMLElement}
   */
  _createNodePill(node) {
    const pill = document.createElement('button');
    pill.className = 'tv-node';
    pill.type = 'button';
    pill.dataset.nodeId = node.id;

    // Color indicator
    if (node.move) {
      pill.classList.add(node.move.color === 'BLACK' ? 'tv-node--black' : 'tv-node--white');
    }

    // Current node highlight
    if (this.tree && node === this.tree.currentNode) {
      pill.classList.add('tv-node--current');
    }

    // Variation indicator
    if (node.isVariation) {
      pill.classList.add('tv-node--var');
    }

    // Has children indicator (branching)
    if (node.children.length > 1) {
      pill.classList.add('tv-node--branch');
    }

    // Label
    const numSpan = document.createElement('span');
    numSpan.className = 'tv-node__num';
    numSpan.textContent = node.depth;

    const coordSpan = document.createElement('span');
    coordSpan.className = 'tv-node__coord';
    coordSpan.textContent = node.shortLabel;

    pill.appendChild(numSpan);
    pill.appendChild(coordSpan);

    // Click handler
    pill.addEventListener('click', (e) => {
      e.stopPropagation();
      if (this.onNodeClick) this.onNodeClick(node);
    });

    return pill;
  }

  /**
   * Scroll the tree container so the current node is visible.
   */
  _scrollToCurrent() {
    if (!this.tree) return;
    const currentEl = this.container.querySelector('.tv-node--current');
    if (currentEl) {
      currentEl.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'nearest' });
    }
  }
}
