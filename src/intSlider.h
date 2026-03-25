#ifndef intSlider_h
#define intSlider_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeInspectorController.h"
#include "portal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <set>

class intSlider : public ofxOceanodeNodeModel {
public:
	intSlider() : ofxOceanodeNodeModel("Int Slider") {
		selectedPortalInstance = nullptr;
	}

	void setup() override {
		description = "A slider with transparent background, bindable to int portals.";

		setFlags(ofxOceanodeNodeModelFlags_TransparentNode);

		addInspectorParameter(sliderName.set("Name", "Int Slider"));
		addInspectorParameter(sliderWidth.set("Width", 150.0f, 50.0f, 300.0f));
		addInspectorParameter(sliderHeight.set("Height", 20.0f, 15.0f, 50.0f));
		addInspectorParameter(minValue.set("Min Value", 0, INT_MIN, INT_MAX));
		addInspectorParameter(maxValue.set("Max Value", 100, INT_MIN, INT_MAX));
		addInspectorParameter(globalSearch.set("Global Search", false));
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));

		updatePortalListOnly();

		ofxOceanodeInspectorController::registerInspectorDropdown("Int Slider", "Portal", portalNames);

		selectedPortalIndex.set("Portal", 0, 0, std::max(0, (int)portalNames.size() - 1));
		addInspectorParameter(selectedPortalIndex);

		sliderValue = 0;

		addCustomRegion(sliderRegion.set("Slider", [this](){
			drawSlider();
		}), [this](){
			drawSlider();
		});

		dropdownListener = selectedPortalIndex.newListener([this](int &index){
			if (!ofxOceanodeShared::isPresetLoading()) {
				updateSelectedPortalInstance();
				updateSliderFromPortal();
			}
		});

		globalSearchListener = globalSearch.newListener([this](bool &){
			updatePortalList();
			updateSelectedPortalInstance();
			updateSliderFromPortal();
		});

		presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
			// Capture name BEFORE updatePortalList, which can corrupt selectedPortalName
			// via the dropdownListener cascade if it picks the wrong portal by index.
			string nameToRestore = selectedPortalName.get();
			updatePortalList();
			restoreSelectionByName(nameToRestore);
			updateSliderFromPortal();
		});

		updateSelectedPortalInstance();
		updateSliderFromPortal();
	}

	void update(ofEventArgs &args) override {
		if (ofxOceanodeShared::isPresetLoading()) return;

		if (needsDelayedRestore) {
			// Capture name first — updatePortalList can overwrite selectedPortalName
			// through the dropdownListener if it picks the wrong portal by stale index.
			string nameToRestore = selectedPortalName.get();
			updatePortalListOnly();
			updatePortalList();
			restoreSelectionByName(nameToRestore);
			updateSliderFromPortal();
			needsDelayedRestore = false;
			return;
		}

		updatePortalList();
		updateSliderFromPortal();
	}

	void presetRecallAfterSettingParameters(ofJson &json) override {
		needsDelayedRestore = true;
	}

private:
	ofParameter<string> sliderName;
	ofParameter<float>  sliderWidth;
	ofParameter<float>  sliderHeight;
	ofParameter<int>    minValue;
	ofParameter<int>    maxValue;
	ofParameter<bool>   globalSearch;
	ofParameter<string> selectedPortalName;
	ofParameter<int>    selectedPortalIndex;

	ofEventListeners listeners;
	ofEventListener  dropdownListener;
	ofEventListener  presetLoadedListener;
	ofEventListener  globalSearchListener;
	customGuiRegion  sliderRegion;

	vector<string>        portalNames;
	vector<portal<int>*>  compatiblePortals;
	portal<int>*          selectedPortalInstance;
	bool                  needsDelayedRestore = false;

	int sliderValue;

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
			// Always restore by name, never by index. The index is unreliable across
			// sessions (portal list order may change); the name is always the stable key.
			string nameToRestore = selectedPortalName.get();

			portalNames      = newNames;
			compatiblePortals = newPortals;

			if (portalNames.empty()) {
				portalNames.push_back("No Compatible Portals");
				selectedPortalInstance = nullptr;
			}

			try {
				ofxOceanodeInspectorController::registerInspectorDropdown("Int Slider", "Portal", portalNames);
				selectedPortalIndex.setMin(0);
				selectedPortalIndex.setMax(std::max(0, (int)portalNames.size() - 1));
			} catch (...) {}

			restoreSelectionByName(nameToRestore);
		}
	}

	// ── selection helpers ───────────────────────────────────────────────

	string getActualName(const string &display) {
		string s = display;
		auto slash = s.find_last_of('/');
		if (slash != string::npos) s = s.substr(slash + 1);
		if (s.size() >= 2 && s.substr(s.size() - 2) == " *")
			s = s.substr(0, s.size() - 2);
		return s;
	}

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
						if (selectedPortalName.get() != n) selectedPortalName.set(n);
					} catch (...) {
						selectedPortalInstance = nullptr;
						selectedPortalName.set("");
					}
					return;
				}
			}
		}
		if (!compatiblePortals.empty() && compatiblePortals[0]) {
			try {
				selectedPortalIndex    = 0;
				selectedPortalInstance = compatiblePortals[0];
				selectedPortalName.set(compatiblePortals[0]->getName());
			} catch (...) {
				selectedPortalIndex    = 0;
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		} else {
			selectedPortalIndex    = 0;
			selectedPortalInstance = nullptr;
			selectedPortalName.set("");
		}
	}

	void updateSelectedPortalInstance() {
		if (selectedPortalIndex >= 0 && selectedPortalIndex < (int)compatiblePortals.size()
				&& compatiblePortals[selectedPortalIndex]) {
			selectedPortalInstance = compatiblePortals[selectedPortalIndex];
			try {
				string n = selectedPortalInstance->getName();
				if (selectedPortalName.get() != n) selectedPortalName.set(n);
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		} else {
			selectedPortalInstance = nullptr;
			selectedPortalName.set("");
		}
	}

	// ── portal value access ─────────────────────────────────────────────

	void updateSliderFromPortal() {
		if (selectedPortalInstance) {
			try { sliderValue = selectedPortalInstance->getValue(); return; }
			catch (...) { selectedPortalInstance = nullptr; selectedPortalName.set(""); }
		}
		if (selectedPortalIndex >= 0 && selectedPortalIndex < (int)compatiblePortals.size()
				&& compatiblePortals[selectedPortalIndex]) {
			try {
				selectedPortalInstance = compatiblePortals[selectedPortalIndex];
				selectedPortalName.set(selectedPortalInstance->getName());
				sliderValue = selectedPortalInstance->getValue();
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
		sliderValue = minValue.get();
	}

	void setPortalValue(int v) {
		if (selectedPortalInstance) {
			try { selectedPortalInstance->setValue(v); }
			catch (...) { selectedPortalInstance = nullptr; selectedPortalName.set(""); }
		}
	}

	// ── drawing ──────────────────────────────────────────────────────────

	void drawSlider() {
		string name = sliderName.get();
		if (!name.empty()) {
			ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
			ImVec2 pos = ImGui::GetCursorPos();
			float  w   = sliderWidth.get();
			ImGui::SetCursorPosX(pos.x + (w - textSize.x) * 0.5f);
			ImGui::Text("%s", name.c_str());
			ImGui::Spacing();
		}

		ImVec2 pos      = ImGui::GetCursorScreenPos();
		ImDrawList *dl  = ImGui::GetWindowDrawList();
		float width     = sliderWidth.get();
		float height    = sliderHeight.get();
		int   minV      = minValue.get();
		int   maxV      = maxValue.get();
		if (minV >= maxV) maxV = minV + 1;

		ImGui::InvisibleButton("IntSliderButton", ImVec2(width, height));
		bool isActive  = ImGui::IsItemActive();
		bool isHovered = ImGui::IsItemHovered();

		if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			float mouseX  = ImGui::GetIO().MousePos.x - pos.x;
			float norm    = ofClamp(mouseX / width, 0.0f, 1.0f);
			int   newVal  = minV + (int)round(norm * (float)(maxV - minV));
			setPortalValue(newVal);
			sliderValue = newVal;
		}

		float norm       = ofClamp((float)(sliderValue - minV) / (float)(maxV - minV), 0.0f, 1.0f);
		float knobRadius = height * 0.4f;

		// track
		ImVec2 trackMin(pos.x, pos.y + height * 0.5f - knobRadius);
		ImVec2 trackMax(pos.x + width, pos.y + height * 0.5f + knobRadius);
		dl->AddRectFilled(trackMin, trackMax, IM_COL32(100, 100, 100, 255), knobRadius);

		// fill
		if (norm > 0.0f) {
			ImVec2 fillMax(pos.x + width * norm, pos.y + height * 0.5f + knobRadius);
			dl->AddRectFilled(trackMin, fillMax, IM_COL32(0, 200, 120, 255), knobRadius);
		}

		// knob
		float knobX = pos.x + width * norm;
		float knobY = pos.y + height * 0.5f;
		dl->AddCircleFilled(ImVec2(knobX, knobY), knobRadius,
			isActive ? IM_COL32(220, 220, 220, 255) : IM_COL32(255, 255, 255, 255));
		dl->AddCircle(ImVec2(knobX, knobY), knobRadius, IM_COL32(150, 150, 150, 255), 0, 1.5f);

		if (isHovered) {
			string tip = ofToString(sliderValue);
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

#endif /* intSlider_h */
