#ifndef intValue_h
#define intValue_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeInspectorController.h"
#include "portal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <set>

class intValue : public ofxOceanodeNodeModel {
public:
	intValue() : ofxOceanodeNodeModel("Int Value") {
		selectedPortalInstance = nullptr;
	}

	void setup() override {
		description = "A numeric input field that connects to an int portal.";

		setFlags(ofxOceanodeNodeModelFlags_TransparentNode);

		addInspectorParameter(valueName.set("Name", "Int Value"));
		addInspectorParameter(inputWidth.set("Width", 120.0f, 50.0f, 300.0f));
		addInspectorParameter(fontSize.set("Font Size", 14.0f, 8.0f, 24.0f));
		addInspectorParameter(globalSearch.set("Global Search", false));
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));

		updatePortalListOnly();

		ofxOceanodeInspectorController::registerInspectorDropdown("Int Value", "Portal", portalNames);

		selectedPortalIndex.set("Portal", 0, 0, std::max(0, (int)portalNames.size() - 1));
		addInspectorParameter(selectedPortalIndex);

		currentValue = 0;
		inputBuffer[0] = '\0';

		addCustomRegion(valueRegion.set("Value", [this](){
			drawValue();
		}), [this](){
			drawValue();
		});

		dropdownListener = selectedPortalIndex.newListener([this](int &index){
			if (!ofxOceanodeShared::isPresetLoading()) {
				updateSelectedPortalInstance();
				updateValueFromPortal();
			}
		});

		globalSearchListener = globalSearch.newListener([this](bool &){
			updatePortalList();
			updateSelectedPortalInstance();
			updateValueFromPortal();
		});

		presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
			string nameToRestore = selectedPortalName.get();
			updatePortalList();
			restoreSelectionByName(nameToRestore);
			updateValueFromPortal();
		});

		updateSelectedPortalInstance();
		updateValueFromPortal();
	}

	void update(ofEventArgs &args) override {
		if (ofxOceanodeShared::isPresetLoading()) return;

		if (needsDelayedRestore) {
			string nameToRestore = selectedPortalName.get();
			updatePortalListOnly();
			updatePortalList();
			restoreSelectionByName(nameToRestore);
			updateValueFromPortal();
			needsDelayedRestore = false;
			return;
		}

		updatePortalList();
		updateValueFromPortal();
	}

	void presetRecallAfterSettingParameters(ofJson &json) override {
		needsDelayedRestore = true;
	}

	void presetRecallBeforeSettingParameters(ofJson &json) override {
		try {
			// Convert string values to proper int if needed (legacy preset compat)
			for (const string &p : {"Portal"}) {
				if (json.contains(p) && json[p].is_string()) {
					try { json[p] = ofToInt(json[p].get<string>()); }
					catch (...) { json.erase(p); }
				}
			}
			for (const string &p : {"Width", "Font_Size"}) {
				if (json.contains(p) && json[p].is_string()) {
					try { json[p] = ofToFloat(json[p].get<string>()); }
					catch (...) { json.erase(p); }
				}
			}
			if (json.contains("Selected_Portal") && json["Selected_Portal"].is_string())
				selectedPortalName.set(json["Selected_Portal"].get<string>());
		} catch (...) {}
	}

	void presetSave(ofJson &json) override {
		if (selectedPortalInstance) {
			try { json["selectedPortalName"] = selectedPortalInstance->getName(); }
			catch (...) { json["selectedPortalName"] = ""; }
		} else {
			json["selectedPortalName"] = "";
		}
	}

private:
	ofParameter<string> valueName;
	ofParameter<float>  inputWidth;
	ofParameter<float>  fontSize;
	ofParameter<bool>   globalSearch;
	ofParameter<string> selectedPortalName;
	ofParameter<int>    selectedPortalIndex;

	ofEventListeners listeners;
	ofEventListener  dropdownListener;
	ofEventListener  presetLoadedListener;
	ofEventListener  globalSearchListener;
	customGuiRegion  valueRegion;

	vector<string>        portalNames;
	vector<portal<int>*>  compatiblePortals;
	portal<int>*          selectedPortalInstance;
	bool                  needsDelayedRestore = false;

	int  currentValue;
	char inputBuffer[64];

	// ── portal list helpers ─────────────────────────────────────────────

	void buildPortalList(vector<string> &outNames, vector<portal<int>*> &outPortals) {
		set<string> seen;
		string currentScope = getParents();

		for (auto *p : ofxOceanodeShared::getAllPortals<int>()) {
			if (!p) continue;

			bool inScope = globalSearch.get()
				? true
				: (!p->isLocal() || p->getParents() == currentScope);

			if (!inScope) continue;

			string pName = p->getName();
			if (seen.count(pName)) continue;
			seen.insert(pName);

			string display = pName;
			if (globalSearch.get()) {
				string scope = p->getParents();
				if (!scope.empty() && scope != currentScope)
					display = scope + "/" + pName;
			}
			if (!p->isLocal()) display += " *";

			outNames.push_back(display);
			outPortals.push_back(p);
		}
	}

	void updatePortalListOnly() {
		portalNames.clear();
		compatiblePortals.clear();
		buildPortalList(portalNames, compatiblePortals);
		if (portalNames.empty()) {
			portalNames.push_back("No Compatible Portals");
			selectedPortalInstance = nullptr;
		}
	}

	void updatePortalList() {
		vector<string>       newNames;
		vector<portal<int>*> newPortals;
		buildPortalList(newNames, newPortals);

		if (newNames != portalNames || newPortals != compatiblePortals) {
			portalNames       = newNames;
			compatiblePortals = newPortals;

			if (portalNames.empty()) {
				portalNames.push_back("No Compatible Portals");
				selectedPortalInstance = nullptr;
			}

			try {
				ofxOceanodeInspectorController::registerInspectorDropdown("Int Value", "Portal", portalNames);
				selectedPortalIndex.setMin(0);
				selectedPortalIndex.setMax(std::max(0, (int)portalNames.size() - 1));
			} catch (...) {}

			maintainSelectionByInstance();
		}
	}

	// ── selection helpers ───────────────────────────────────────────────

	void restoreSelectionByName(const string &name) {
		if (name.empty()) { maintainSelectionByInstance(); return; }
		for (int i = 0; i < (int)compatiblePortals.size(); i++) {
			if (!compatiblePortals[i]) continue;
			try {
				if (compatiblePortals[i]->getName() == name) {
					selectedPortalIndex    = i;
					selectedPortalInstance = compatiblePortals[i];
					return;
				}
			} catch (...) {}
		}
		maintainSelectionByInstance();
	}

	void maintainSelectionByInstance() {
		if (!selectedPortalName.get().empty()) {
			for (int i = 0; i < (int)compatiblePortals.size(); i++) {
				if (!compatiblePortals[i]) continue;
				try {
					if (compatiblePortals[i]->getName() == selectedPortalName.get()) {
						selectedPortalIndex    = i;
						selectedPortalInstance = compatiblePortals[i];
						return;
					}
				} catch (...) {}
			}
		}
		if (selectedPortalInstance && !compatiblePortals.empty()) {
			for (int i = 0; i < (int)compatiblePortals.size(); i++) {
				if (compatiblePortals[i] == selectedPortalInstance) {
					selectedPortalIndex = i;
					try {
						string n = selectedPortalInstance->getName();
						if (selectedPortalName.get() != n) selectedPortalName = n;
					} catch (...) {
						selectedPortalInstance = nullptr;
						selectedPortalName = "";
					}
					return;
				}
			}
		}
		if (!compatiblePortals.empty() && compatiblePortals[0]) {
			try {
				selectedPortalIndex    = 0;
				selectedPortalInstance = compatiblePortals[0];
				selectedPortalName     = compatiblePortals[0]->getName();
			} catch (...) {
				selectedPortalIndex    = 0;
				selectedPortalInstance = nullptr;
				selectedPortalName     = "";
			}
		} else {
			selectedPortalIndex    = 0;
			selectedPortalInstance = nullptr;
			selectedPortalName     = "";
		}
	}

	void updateSelectedPortalInstance() {
		if (selectedPortalIndex >= 0 && selectedPortalIndex < (int)compatiblePortals.size()
				&& compatiblePortals[selectedPortalIndex]) {
			selectedPortalInstance = compatiblePortals[selectedPortalIndex];
			try {
				string n = selectedPortalInstance->getName();
				if (selectedPortalName.get() != n) selectedPortalName = n;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName = "";
			}
		} else {
			selectedPortalInstance = nullptr;
			selectedPortalName = "";
		}
	}

	// ── portal value access ─────────────────────────────────────────────

	void updateValueFromPortal() {
		if (selectedPortalInstance) {
			try { currentValue = selectedPortalInstance->getValue(); return; }
			catch (...) { selectedPortalInstance = nullptr; selectedPortalName = ""; }
		}
		if (selectedPortalIndex >= 0 && selectedPortalIndex < (int)compatiblePortals.size()
				&& compatiblePortals[selectedPortalIndex]) {
			try {
				selectedPortalInstance = compatiblePortals[selectedPortalIndex];
				selectedPortalName     = selectedPortalInstance->getName();
				currentValue           = selectedPortalInstance->getValue();
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName     = "";
			}
		}
		currentValue = 0;
	}

	void setPortalValue(int v) {
		if (selectedPortalInstance) {
			try { selectedPortalInstance->setValue(v); }
			catch (...) { selectedPortalInstance = nullptr; selectedPortalName = ""; }
		}
	}

	// ── drawing ──────────────────────────────────────────────────────────

	void drawValue() {
		string name = valueName.get();
		if (!name.empty()) {
			ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
			ImVec2 pos = ImGui::GetCursorPos();
			float  w   = inputWidth.get();
			ImGui::SetCursorPosX(pos.x + (w - textSize.x) * 0.5f);
			ImGui::Text("%s", name.c_str());
			ImGui::Spacing();
		}

		float width = inputWidth.get();
		ImGui::SetNextItemWidth(width);

		float fontScale = fontSize.get() / ImGui::GetFontSize();
		ImGui::SetWindowFontScale(fontScale);

		snprintf(inputBuffer, sizeof(inputBuffer), "%d", currentValue);

		ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

		if (ImGui::InputText("##intvalue_input", inputBuffer, sizeof(inputBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal)) {
			int newVal = ofToInt(inputBuffer);
			setPortalValue(newVal);
			currentValue = newVal;
		}

		ImGui::SetWindowFontScale(1.0f);
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemHovered()) {
			string tip = "Value: " + ofToString(currentValue);
			if (selectedPortalInstance) {
				try { tip += "\nConnected to: " + selectedPortalInstance->getName(); }
				catch (...) { tip += "\nNo portal connected"; }
			} else {
				tip += "\nNo portal connected";
			}
			ImGui::SetTooltip("%s", tip.c_str());
		}
	}
};

#endif /* intValue_h */
