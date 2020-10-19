// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { V8CustomElement, defineCustomElement } from "../helper.mjs";
import { FocusEvent } from "../events.mjs";

defineCustomElement(
  "./map-panel/map-transitions",
  (templateText) =>
    class MapTransitions extends V8CustomElement {
      _map;
      _selectedMapLogEntries;
      constructor() {
        super(templateText);
        this.transitionView.addEventListener("mousemove", (e) =>
          this.handleTransitionViewChange(e)
        );
        this.currentNode = this.transitionView;
        this.currentMap = undefined;
      }

      get transitionView() {
        return this.$("#transitionView");
      }

      get tooltip() {
        return this.$("#tooltip");
      }

      get tooltipContents() {
        return this.$("#tooltipContents");
      }

      set map(value) {
        this._map = value;
        this.showMap();
      }

      handleTransitionViewChange(e) {
        this.tooltip.style.left = e.pageX + "px";
        this.tooltip.style.top = e.pageY + "px";
        let map = e.target.map;
        if (map) {
          this.tooltipContents.innerText = map.description;
        }
      }

      selectMap(map) {
        this.currentMap = map;
        this.showMap();
        this.dispatchEvent(new FocusEvent(map));
      }

      dblClickSelectMap(map) {
        this.dispatchEvent(new FocusEvent(map));
      }

      showMap() {
        // Called when a map selected
        if (this.currentMap === this._map) return;
        this.currentMap = this._map;
        this.selectedMapLogEntries = [this._map];
        this.dispatchEvent(new FocusEvent(this._map));
      }

      showMaps() {
        // Timeline dbl click to show map transitions of selected maps
        this.transitionView.style.display = "none";
        this.removeAllChildren(this.transitionView);
        this.selectedMapLogEntries.forEach((map) =>
          this.addMapAndParentTransitions(map));
        this.transitionView.style.display = "";
      }

      set selectedMapLogEntries(list) {
        this._selectedMapLogEntries = list;
        this.showMaps();
      }

      get selectedMapLogEntries() {
        return this._selectedMapLogEntries;
      }

      addMapAndParentTransitions(map) {
        if (map === void 0) return;
        this.currentNode = this.transitionView;
        let parents = map.getParents();
        if (parents.length > 0) {
          this.addTransitionTo(parents.pop());
          parents.reverse().forEach((each) => this.addTransitionTo(each));
        }
        let mapNode = this.addSubtransitions(map);
        // Mark and show the selected map.
        mapNode.classList.add("selected");
        if (this.selectedMap == map) {
          setTimeout(
            () =>
              mapNode.scrollIntoView({
                behavior: "smooth",
                block: "nearest",
                inline: "nearest",
              }),
            1
          );
        }
      }

      addMapNode(map) {
        let node = this.div("map");
        if (map.edge) node.style.backgroundColor = map.edge.getColor();
        node.map = map;
        node.addEventListener("click", () => this.selectMap(map));
        node.addEventListener("dblclick", () => this.dblClickSelectMap(map));
        if (map.children.length > 1) {
          node.innerText = map.children.length;
          let showSubtree = this.div("showSubtransitions");
          showSubtree.addEventListener("click", (e) =>
            this.toggleSubtree(e, node)
          );
          node.appendChild(showSubtree);
        } else if (map.children.length == 0) {
          node.innerHTML = "&#x25CF;";
        }
        this.currentNode.appendChild(node);
        return node;
      }

      addSubtransitions(map) {
        let mapNode = this.addTransitionTo(map);
        // Draw outgoing linear transition line.
        let current = map;
        while (current.children.length == 1) {
          current = current.children[0].to;
          this.addTransitionTo(current);
        }
        return mapNode;
      }

      addTransitionEdge(map) {
        let classes = ["transitionEdge"];
        let edge = this.div(classes);
        edge.style.backgroundColor = map.edge.getColor();
        let labelNode = this.div("transitionLabel");
        labelNode.innerText = map.edge.toString();
        edge.appendChild(labelNode);
        return edge;
      }

      addTransitionTo(map) {
        // transition[ transitions[ transition[...], transition[...], ...]];

        let transition = this.div("transition");
        if (map.isDeprecated()) transition.classList.add("deprecated");
        if (map.edge) {
          transition.appendChild(this.addTransitionEdge(map));
        }
        let mapNode = this.addMapNode(map);
        transition.appendChild(mapNode);

        let subtree = this.div("transitions");
        transition.appendChild(subtree);

        this.currentNode.appendChild(transition);
        this.currentNode = subtree;

        return mapNode;
      }

      toggleSubtree(event, node) {
        let map = node.map;
        event.target.classList.toggle("opened");
        let transitionsNode = node.parentElement.querySelector(".transitions");
        let subtransitionNodes = transitionsNode.children;
        if (subtransitionNodes.length <= 1) {
          // Add subtransitions excepth the one that's already shown.
          let visibleTransitionMap =
            subtransitionNodes.length == 1
              ? transitionsNode.querySelector(".map").map
              : void 0;
          map.children.forEach((edge) => {
            if (edge.to != visibleTransitionMap) {
              this.currentNode = transitionsNode;
              this.addSubtransitions(edge.to);
            }
          });
        } else {
          // remove all but the first (currently selected) subtransition
          for (let i = subtransitionNodes.length - 1; i > 0; i--) {
            transitionsNode.removeChild(subtransitionNodes[i]);
          }
        }
      }
    }
);
