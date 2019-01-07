// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { GraphView } from "../src/graph-view"
import { ScheduleView } from "../src/schedule-view"
import { SequenceView } from "../src/sequence-view"
import { SourceResolver } from "../src/source-resolver"
import { SelectionBroker } from "../src/selection-broker"
import { View, PhaseView } from "../src/view"

const multiviewID = "multiview";

const toolboxHTML = `
<div class="graph-toolbox">
  <input id="layout" type="image" title="layout graph" src="layout-icon.png" alt="layout graph" class="button-input">
  <input id="show-all" type="image" title="show all nodes" src="expand-all.jpg" alt="show all nodes" class="button-input">
  <input id="toggle-hide-dead" type="image" title="show only live nodes" src="live.png" alt="only live nodes"
    class="button-input">
  <input id="hide-unselected" type="image" title="hide unselected nodes" src="hide-unselected.png" alt="hide unselected nodes"
    class="button-input">
  <input id="hide-selected" type="image" title="hide selected nodes" src="hide-selected.png" alt="hide selected nodes"
    class="button-input">
  <input id="zoom-selection" type="image" title="zoom to selection" src="search.png" alt="zoom to selection"
    class="button-input">
  <input id="toggle-types" type="image" title="show/hide types" src="types.png" alt="show/hide types" class="button-input">
  <input id="search-input" type="text" title="search nodes for regex" alt="search node for regex" class="search-input"
    placeholder="find with regexp&hellip;">
  <select id="display-selector">
    <option disabled selected>(please open a file)</option>
  </select>
</div>`

export class GraphMultiView extends View {
  sourceResolver: SourceResolver;
  selectionBroker: SelectionBroker;
  graph: GraphView;
  schedule: ScheduleView;
  sequence: SequenceView;
  selectMenu: HTMLSelectElement;
  currentPhaseView: View & PhaseView;

  createViewElement() {
    const pane = document.createElement('div');
    pane.setAttribute('id', multiviewID);
    pane.className = "viewpane";
    return pane;
  }

  constructor(id, selectionBroker, sourceResolver) {
    super(id);
    const view = this;
    view.sourceResolver = sourceResolver;
    view.selectionBroker = selectionBroker;
    const toolbox = document.createElement("div")
    toolbox.className = "toolbox-anchor";
    toolbox.innerHTML = toolboxHTML
    view.divNode.appendChild(toolbox);
    const searchInput = toolbox.querySelector("#search-input") as HTMLInputElement;
    searchInput.addEventListener("keyup", e => {
      if (!view.currentPhaseView) return;
      view.currentPhaseView.searchInputAction(searchInput, e)
    });
    view.divNode.addEventListener("keyup", (e: KeyboardEvent) => {
      if (e.keyCode == 191) { // keyCode == '/'
        searchInput.focus();
      }
    });
    searchInput.setAttribute("value", window.sessionStorage.getItem("lastSearch") || "");
    this.graph = new GraphView(this.divNode, selectionBroker,
      (phaseName) => view.displayPhaseByName(phaseName));
    this.schedule = new ScheduleView(this.divNode, selectionBroker);
    this.sequence = new SequenceView(this.divNode, selectionBroker);
    this.selectMenu = (<HTMLSelectElement>toolbox.querySelector('#display-selector'));
  }

  initializeSelect() {
    const view = this;
    view.selectMenu.innerHTML = '';
    view.sourceResolver.forEachPhase((phase) => {
      const optionElement = document.createElement("option");
      let maxNodeId = "";
      if (phase.type == "graph" && phase.highestNodeId != 0) {
        maxNodeId = ` ${phase.highestNodeId}`;
      }
      optionElement.text = `${phase.name}${maxNodeId}`;
      view.selectMenu.add(optionElement);
    });
    this.selectMenu.onchange = function (this: HTMLSelectElement) {
      window.sessionStorage.setItem("lastSelectedPhase", this.selectedIndex.toString());
      view.displayPhase(view.sourceResolver.getPhase(this.selectedIndex));
    }
  }

  show(data, rememberedSelection) {
    super.show(data, rememberedSelection);
    this.initializeSelect();
    const lastPhaseIndex = +window.sessionStorage.getItem("lastSelectedPhase");
    const initialPhaseIndex = this.sourceResolver.repairPhaseId(lastPhaseIndex);
    this.selectMenu.selectedIndex = initialPhaseIndex;
    this.displayPhase(this.sourceResolver.getPhase(initialPhaseIndex));
  }

  initializeContent() { }

  displayPhase(phase) {
    if (phase.type == 'graph') {
      this.displayPhaseView(this.graph, phase.data);
    } else if (phase.type == 'schedule') {
      this.displayPhaseView(this.schedule, phase);
    } else if (phase.type == 'sequence') {
      this.displayPhaseView(this.sequence, phase);
    }
  }

  displayPhaseView(view, data) {
    const rememberedSelection = this.hideCurrentPhase();
    view.show(data, rememberedSelection);
    this.divNode.classList.toggle("scrollable", view.isScrollable());
    this.currentPhaseView = view;
  }

  displayPhaseByName(phaseName) {
    const phaseId = this.sourceResolver.getPhaseIdByName(phaseName);
    this.selectMenu.selectedIndex = phaseId - 1;
    this.displayPhase(this.sourceResolver.getPhase(phaseId));
  }

  hideCurrentPhase() {
    let rememberedSelection = null;
    if (this.currentPhaseView != null) {
      rememberedSelection = this.currentPhaseView.detachSelection();
      this.currentPhaseView.hide();
      this.currentPhaseView = null;
    }
    return rememberedSelection;
  }

  onresize() {
    if (this.currentPhaseView) this.currentPhaseView.onresize();
  }

  deleteContent() {
    this.hideCurrentPhase();
  }

  detachSelection() {
    return null;
  }
}
