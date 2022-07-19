// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "../common/constants";
import { interpolate } from "../common/util";
import { SelectionBroker } from "../selection/selection-broker";
import { TextView } from "./text-view";
import { SelectionMap } from "../selection/selection-map";
import { InstructionSelectionHandler } from "../selection/selection-handler";
import { TurbolizerInstructionStartInfo } from "../phases/instructions-phase";
import { SelectionStorage } from "../selection/selection-storage";

const toolboxHTML =
  `<div id="disassembly-toolbox">
    <form>
      <label><input id="show-instruction-address" type="checkbox" name="instruction-address">Show addresses</label>
      <label><input id="show-instruction-binary" type="checkbox" name="instruction-binary">Show binary literal</label>
      <label><input id="highlight-gap-instructions" type="checkbox" name="instruction-binary">Highlight gap instructions</label>
    </form>
  </div>`;

export class DisassemblyView extends TextView {
  SOURCE_POSITION_HEADER_REGEX: any;
  addrEventCounts: any;
  totalEventCounts: any;
  maxEventCounts: any;
  posLines: Array<any>;
  instructionSelectionHandler: InstructionSelectionHandler;
  offsetSelection: SelectionMap;
  showInstructionAddressHandler: () => void;
  showInstructionBinaryHandler: () => void;
  highlightGapInstructionsHandler: () => void;

  constructor(parentId, broker: SelectionBroker) {
    super(parentId, broker);
    const view = this;
    const ADDRESS_STYLE = {
      associateData: (text, fragment: HTMLElement) => {
        const matches = text.match(/(?<address>0?x?[0-9a-fA-F]{8,16})(?<addressSpace>\s+)(?<offset>[0-9a-f]+)(?<offsetSpace>\s*)/);
        const offset = Number.parseInt(matches.groups["offset"], 16);
        const instructionKind = view.sourceResolver.instructionsPhase
          .getInstructionKindForPCOffset(offset);
        fragment.dataset.instructionKind = instructionKind;
        fragment.title = view.sourceResolver.instructionsPhase
          .instructionKindToReadableName(instructionKind);
        const blockIds = view.sourceResolver.disassemblyPhase.getBlockIdsForOffset(offset);
        const blockIdElement = document.createElement("SPAN");
        blockIdElement.className = "block-id com linkable-text";
        blockIdElement.innerText = "";
        if (blockIds && blockIds.length > 0) {
          blockIds.forEach(blockId => view.addHtmlElementForBlockId(blockId, fragment));
          blockIdElement.innerText = `B${blockIds.join(",")}:`;
          blockIdElement.dataset.blockId = `${blockIds.join(",")}`;
        }
        fragment.appendChild(blockIdElement);
        const addressElement = document.createElement("SPAN");
        addressElement.className = "instruction-address";
        addressElement.innerText = matches.groups["address"];
        const offsetElement = document.createElement("SPAN");
        offsetElement.innerText = matches.groups["offset"];
        fragment.appendChild(addressElement);
        fragment.appendChild(document.createTextNode(matches.groups["addressSpace"]));
        fragment.appendChild(offsetElement);
        fragment.appendChild(document.createTextNode(matches.groups["offsetSpace"]));
        fragment.classList.add('tag');

        if (!Number.isNaN(offset)) {
          let pcOffset = view.sourceResolver.instructionsPhase.getKeyPcOffset(offset);
          if (pcOffset == -1) pcOffset = Number(offset);
          fragment.dataset.pcOffset = `${pcOffset}`;
          addressElement.classList.add('linkable-text');
          offsetElement.classList.add('linkable-text');
        }
        return true;
      }
    };
    const UNCLASSIFIED_STYLE = {
      css: 'com'
    };
    const NUMBER_STYLE = {
      css: ['instruction-binary', 'lit']
    };
    const COMMENT_STYLE = {
      css: 'com'
    };
    const OPCODE_ARGS = {
      associateData: function (text, fragment) {
        fragment.innerHTML = text;
        const replacer = (match, hexOffset) => {
          const offset = Number.parseInt(hexOffset, 16);
          let keyOffset = view.sourceResolver.instructionsPhase.getKeyPcOffset(offset);
          if (keyOffset == -1) keyOffset = Number(offset);
          const blockIds = view.sourceResolver.disassemblyPhase.getBlockIdsForOffset(offset);
          let block = "";
          let blockIdData = "";
          if (blockIds && blockIds.length > 0) {
            block = `B${blockIds.join(",")} `;
            blockIdData = `data-block-id="${blockIds.join(",")}"`;
          }
          return `<span class="tag linkable-text" data-pc-offset="${keyOffset}" ${blockIdData}>${block}${match}</span>`;
        };
        const html = text.replace(/<.0?x?([0-9a-fA-F]+)>/g, replacer);
        fragment.innerHTML = html;
        return true;
      }
    };
    const OPCODE_STYLE = {
      css: 'kwd'
    };
    const BLOCK_HEADER_STYLE = {
      associateData: function (text, fragment) {
        if (view.sourceResolver.disassemblyPhase.hasBlockStartInfo()) return false;
        const matches = /\d+/.exec(text);
        if (!matches) return true;
        const blockId = matches[0];
        fragment.dataset.blockId = blockId;
        fragment.innerHTML = text;
        fragment.className = "com block";
        return true;
      }
    };
    const SOURCE_POSITION_HEADER_STYLE = {
      css: 'com'
    };
    view.SOURCE_POSITION_HEADER_REGEX = /^\s*--[^<]*<.*(not inlined|inlined\((\d+)\)):(\d+)>\s*--/;
    const patterns = [
      [
        [/^0?x?[0-9a-fA-F]{8,16}\s+[0-9a-f]+\s+/, ADDRESS_STYLE, 1],
        [view.SOURCE_POSITION_HEADER_REGEX, SOURCE_POSITION_HEADER_STYLE, -1],
        [/^\s+-- B\d+ start.*/, BLOCK_HEADER_STYLE, -1],
        [/^.*/, UNCLASSIFIED_STYLE, -1]
      ],
      [
        [/^\s*[0-9a-f]+\s+/, NUMBER_STYLE, 2],
        [/^\s*[0-9a-f]+\s+[0-9a-f]+\s+/, NUMBER_STYLE, 2],
        [/^.*/, null, -1]
      ],
      [
        [/^REX.W \S+\s+/, OPCODE_STYLE, 3],
        [/^\S+\s+/, OPCODE_STYLE, 3],
        [/^\S+$/, OPCODE_STYLE, -1],
        [/^.*/, null, -1]
      ],
      [
        [/^\s+/, null],
        [/^[^;]+$/, OPCODE_ARGS, -1],
        [/^[^;]+/, OPCODE_ARGS, 4],
        [/^;/, COMMENT_STYLE, 5]
      ],
      [
        [/^.+$/, COMMENT_STYLE, -1]
      ]
    ];
    view.setPatterns(patterns);

    const linkHandler = (e: MouseEvent) => {
      if (!(e.target instanceof HTMLElement)) return;
      const offsetAsString = typeof e.target.dataset.pcOffset != "undefined" ? e.target.dataset.pcOffset : e.target.parentElement.dataset.pcOffset;
      const offset = Number.parseInt(offsetAsString, 10);
      if ((typeof offsetAsString) != "undefined" && !Number.isNaN(offset)) {
        view.offsetSelection.select([offset], true);
        const nodes = view.sourceResolver.instructionsPhase.nodesForPCOffset(offset);
        if (nodes.length > 0) {
          e.stopPropagation();
          if (!e.shiftKey) {
            view.selectionHandler.clear();
          }
          view.selectionHandler.select(nodes, true);
        } else {
          view.updateSelection();
        }
      }
      return undefined;
    };
    view.divNode.addEventListener('click', linkHandler);

    const linkHandlerBlock = e => {
      const blockId = e.target.dataset.blockId;
      if (typeof blockId != "undefined") {
        const blockIds = blockId.split(",");
        if (!e.shiftKey) {
          view.selectionHandler.clear();
        }
        view.blockSelectionHandler.select(blockIds, true);
      }
    };
    view.divNode.addEventListener('click', linkHandlerBlock);

    this.offsetSelection = new SelectionMap(offset => String(offset));
    const instructionSelectionHandler = {
      clear: function () {
        view.offsetSelection.clear();
        view.updateSelection();
        broker.broadcastClear(instructionSelectionHandler);
      },
      select: function (instructionIds, selected) {
        view.offsetSelection.select(instructionIds, selected);
        view.updateSelection();
        broker.broadcastBlockSelect(instructionSelectionHandler, instructionIds, selected);
      },
      brokeredInstructionSelect: function (instructionIds, selected) {
        const firstSelect = view.offsetSelection.isEmpty();
        const keyPcOffsets = view.sourceResolver.instructionsPhase
          .instructionsToKeyPcOffsets(instructionIds);
        view.offsetSelection.select(keyPcOffsets, selected);
        view.updateSelection(firstSelect);
      },
      brokeredClear: function () {
        view.offsetSelection.clear();
        view.updateSelection();
      }
    };
    this.instructionSelectionHandler = instructionSelectionHandler;
    broker.addInstructionHandler(instructionSelectionHandler);

    const toolbox = document.createElement("div");
    toolbox.id = "toolbox-anchor";
    toolbox.innerHTML = toolboxHTML;
    view.divNode.insertBefore(toolbox, view.divNode.firstChild);
    const instructionAddressInput: HTMLInputElement = view.divNode.querySelector("#show-instruction-address");
    const lastShowInstructionAddress = window.sessionStorage.getItem("show-instruction-address");
    instructionAddressInput.checked = lastShowInstructionAddress == 'true';
    const showInstructionAddressHandler = () => {
      window.sessionStorage.setItem("show-instruction-address", `${instructionAddressInput.checked}`);
      for (const el of view.divNode.querySelectorAll(".instruction-address")) {
        el.classList.toggle("invisible", !instructionAddressInput.checked);
      }
    };
    instructionAddressInput.addEventListener("change", showInstructionAddressHandler);
    this.showInstructionAddressHandler = showInstructionAddressHandler;

    const instructionBinaryInput: HTMLInputElement = view.divNode.querySelector("#show-instruction-binary");
    const lastShowInstructionBinary = window.sessionStorage.getItem("show-instruction-binary");
    instructionBinaryInput.checked = lastShowInstructionBinary == 'true';
    const showInstructionBinaryHandler = () => {
      window.sessionStorage.setItem("show-instruction-binary", `${instructionBinaryInput.checked}`);
      for (const el of view.divNode.querySelectorAll(".instruction-binary")) {
        el.classList.toggle("invisible", !instructionBinaryInput.checked);
      }
    };
    instructionBinaryInput.addEventListener("change", showInstructionBinaryHandler);
    this.showInstructionBinaryHandler = showInstructionBinaryHandler;

    const highlightGapInstructionsInput: HTMLInputElement = view.divNode.querySelector("#highlight-gap-instructions");
    const lastHighlightGapInstructions = window.sessionStorage.getItem("highlight-gap-instructions");
    highlightGapInstructionsInput.checked = lastHighlightGapInstructions == 'true';
    const highlightGapInstructionsHandler = () => {
      window.sessionStorage.setItem("highlight-gap-instructions", `${highlightGapInstructionsInput.checked}`);
      view.divNode.classList.toggle("highlight-gap-instructions", highlightGapInstructionsInput.checked);
    };

    highlightGapInstructionsInput.addEventListener("change", highlightGapInstructionsHandler);
    this.highlightGapInstructionsHandler = highlightGapInstructionsHandler;
  }

  public createViewElement(): HTMLDivElement {
    const pane = document.createElement("div");
    pane.setAttribute("id", C.DISASSEMBLY_PANE_ID);
    pane.innerHTML =
      `<pre id="disassembly-text-pre" class="prettyprint prettyprinted">
         <ul class="disassembly-list nolinenums noindent"></ul>
       </pre>`;
    return pane;
  }

  public updateSelection(scrollIntoView: boolean = false): void {
    super.updateSelection(scrollIntoView);
    const selectedKeys = this.selection.selectedKeys();
    const keyPcOffsets: Array<TurbolizerInstructionStartInfo | string> = [
      ...this.sourceResolver.instructionsPhase.nodesToKeyPcOffsets(selectedKeys)
    ];
    if (this.offsetSelection) {
      for (const key of this.offsetSelection.selectedKeys()) {
        keyPcOffsets.push(key);
      }
    }
    for (const keyPcOffset of keyPcOffsets) {
      const elementsToSelect = this.divNode.querySelectorAll(`[data-pc-offset='${keyPcOffset}']`);
      for (const el of elementsToSelect) {
        el.classList.toggle("selected", true);
      }
    }
  }

  public searchInputAction(searchInput: HTMLInputElement, e: Event, onlyVisible: boolean): void {
    throw new Error("Method not implemented.");
  }

  public detachSelection(): SelectionStorage {
    return null;
  }

  public adaptSelection(selection: SelectionStorage): SelectionStorage {
    return selection;
  }

  initializeCode(sourceText, sourcePosition: number = 0) {
    const view = this;
    view.addrEventCounts = null;
    view.totalEventCounts = null;
    view.maxEventCounts = null;
    view.posLines = new Array();
    // Comment lines for line 0 include sourcePosition already, only need to
    // add sourcePosition for lines > 0.
    view.posLines[0] = sourcePosition;
    if (sourceText && sourceText != "") {
      const base = sourcePosition;
      let current = 0;
      const sourceLines = sourceText.split("\n");
      for (let i = 1; i < sourceLines.length; i++) {
        // Add 1 for newline character that is split off.
        current += sourceLines[i - 1].length + 1;
        view.posLines[i] = base + current;
      }
    }
  }

  initializePerfProfile(eventCounts) {
    const view = this;
    if (eventCounts !== undefined) {
      view.addrEventCounts = eventCounts;

      view.totalEventCounts = {};
      view.maxEventCounts = {};
      for (const evName in view.addrEventCounts) {
        if (view.addrEventCounts.hasOwnProperty(evName)) {
          const keys = Object.keys(view.addrEventCounts[evName]);
          const values = keys.map(key => view.addrEventCounts[evName][key]);
          view.totalEventCounts[evName] = values.reduce((a, b) => a + b);
          view.maxEventCounts[evName] = values.reduce((a, b) => Math.max(a, b));
        }
      }
    } else {
      view.addrEventCounts = null;
      view.totalEventCounts = null;
      view.maxEventCounts = null;
    }
  }

  public showContent(data): void {
    console.time("disassembly-view");
    super.initializeContent(data, null);
    this.showInstructionAddressHandler();
    this.showInstructionBinaryHandler();
    this.highlightGapInstructionsHandler();
    console.timeEnd("disassembly-view");
  }

  // Shorten decimals and remove trailing zeroes for readability.
  humanize(num) {
    return num.toFixed(3).replace(/\.?0+$/, "") + "%";
  }

  processLine(line) {
    const view = this;
    let fragments = super.processLine(line);

    // Add profiling data per instruction if available.
    if (view.totalEventCounts) {
      const matches = /^(0x[0-9a-fA-F]+)\s+\d+\s+[0-9a-fA-F]+/.exec(line);
      if (matches) {
        const newFragments = [];
        for (const event in view.addrEventCounts) {
          if (!view.addrEventCounts.hasOwnProperty(event)) continue;
          const count = view.addrEventCounts[event][matches[1]];
          let str = " ";
          const cssCls = "prof";
          if (count !== undefined) {
            const perc = count / view.totalEventCounts[event] * 100;

            let col = { r: 255, g: 255, b: 255 };
            for (let i = 0; i < C.PROF_COLS.length; i++) {
              if (perc === C.PROF_COLS[i].perc) {
                col = C.PROF_COLS[i].col;
                break;
              } else if (perc > C.PROF_COLS[i].perc && perc < C.PROF_COLS[i + 1].perc) {
                const col1 = C.PROF_COLS[i].col;
                const col2 = C.PROF_COLS[i + 1].col;

                const val = perc - C.PROF_COLS[i].perc;
                const max = C.PROF_COLS[i + 1].perc - C.PROF_COLS[i].perc;

                col.r = Math.round(interpolate(val, max, col1.r, col2.r));
                col.g = Math.round(interpolate(val, max, col1.g, col2.g));
                col.b = Math.round(interpolate(val, max, col1.b, col2.b));
                break;
              }
            }

            str = C.UNICODE_BLOCK;

            const fragment = view.createFragment(str, cssCls);
            fragment.title = event + ": " + view.humanize(perc) + " (" + count + ")";
            fragment.style.color = "rgb(" + col.r + ", " + col.g + ", " + col.b + ")";

            newFragments.push(fragment);
          } else {
            newFragments.push(view.createFragment(str, cssCls));
          }
        }
        fragments = newFragments.concat(fragments);
      }
    }
    return fragments;
  }
}
