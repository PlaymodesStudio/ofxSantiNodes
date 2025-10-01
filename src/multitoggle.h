#ifndef multitoggle_h
#define multitoggle_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeInspectorController.h"
#include "portal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <set>

class multitoggle : public ofxOceanodeNodeModel {
public:
	multitoggle() : ofxOceanodeNodeModel("Multitoggle") {
		selectedPortalInstance = nullptr;
	}
	
	void setup() override {
		description = "A multitoggle with transparent background, bindable to vector<int> portals.";
		
		setFlags(ofxOceanodeNodeModelFlags_TransparentNode);
		
		// Inspector parameters
		addInspectorParameter(toggleName.set("Name", "Multitoggle"));
		addInspectorParameter(toggleWidth.set("Width", 200.0f, 100.0f, 400.0f));
		addInspectorParameter(toggleHeight.set("Height", 25.0f, 20.0f, 100.0f));
		addInspectorParameter(numToggles.set("Num Toggles", 8, 1, 32));
		addInspectorParameter(rows.set("Rows", 1, 1, 8));
		addInspectorParameter(globalSearch.set("Global Search", false));
		
		// Portal name and stored values for preset save/load
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));
		addInspectorParameter(storedValues.set(
			"Stored Values",
			vector<int>(8, 0),            // default
			vector<int>(32, 0),           // min per element
			vector<int>(32, 1)            // max per element
		));
		
		// Initialize portal list
		updatePortalListOnly();
		
		// Register dropdown options with inspector system
		ofxOceanodeInspectorController::registerInspectorDropdown("Multitoggle", "Portal", portalNames);
		
		// Create as inspector parameter with dropdown support
		selectedPortalIndex.set("Portal", 0, 0, std::max(0, (int)portalNames.size() - 1));
		addInspectorParameter(selectedPortalIndex);
		
		// Initialize toggle values
		toggleValues.resize(numToggles.get(), 0);
		
		// Add custom GUI region for the multitoggle
		addCustomRegion(multitoggleRegion.set("Multitoggle", [this](){
			drawMultitoggle();
		}), [this](){
			drawMultitoggle();
		});
		
		// Listeners
		dropdownListener = selectedPortalIndex.newListener([this](int &index){
			if (!ofxOceanodeShared::isPresetLoading()) {
				updateSelectedPortalInstance();
				updateMultitoggleFromPortal();
			}
		});
		
		globalSearchListener = globalSearch.newListener([this](bool &){
			updatePortalList();
			updateSelectedPortalInstance();
			updateMultitoggleFromPortal();
		});
		
		numTogglesListener = numToggles.newListener([this](int &num){
			resizeToggleValues(num);
			pushValuesToPortal();
		});
		
		// Keep stored values and working values in sync (and push to portal)
		storedValuesListener = storedValues.newListener([this](vector<int> &values){
			if (!ofxOceanodeShared::isPresetLoading()) {
				syncFromStoredValues();
				pushValuesToPortal();
			}
		});
		
		presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
			updatePortalList();
			restoreSelectionByName(selectedPortalName.get());
			restoreValuesFromStored();
			pushValuesToPortal();
		});
		
		updateSelectedPortalInstance();
		updateMultitoggleFromPortal();
	}
	
	void update(ofEventArgs &args) override {
		// Update portal list, but less frequently to avoid crashes
		static int updateCounter = 0;
		updateCounter++;
		
		// Only update portal list every 60 frames (once per second at 60fps)
		if (updateCounter % 60 == 0) {
			updatePortalList();
		}
		
		if (needsDelayedRestore) {
			updatePortalListOnly();
			restoreSelectionByName(selectedPortalName.get());
			restoreValuesFromStored();
			pushValuesToPortal();
			needsDelayedRestore = false;
		}
		
		// Pull values from portal (keeps UI in sync when others write to the portal)
		updateMultitoggleFromPortal();
	}
	
	void presetRecallAfterSettingParameters(ofJson &json) override {
		needsDelayedRestore = true;
	}
	
private:
	// Parameters
	ofParameter<string> toggleName;
	ofParameter<float> toggleWidth;
	ofParameter<float> toggleHeight;
	ofParameter<int> numToggles;
	ofParameter<int> rows;
	ofParameter<bool> globalSearch;
	ofParameter<string> selectedPortalName;
	ofParameter<int> selectedPortalIndex;
	ofParameter<vector<int>> storedValues; // For preset save/load
	
	// Event listeners
	ofEventListeners listeners;
	ofEventListener dropdownListener;
	ofEventListener presetLoadedListener;
	ofEventListener globalSearchListener;
	ofEventListener numTogglesListener;
	ofEventListener storedValuesListener;
	customGuiRegion multitoggleRegion;
	
	// Portal management
	vector<string> portalNames;
	vector<portal<vector<int>>*> compatiblePortals;
	portal<vector<int>>* selectedPortalInstance;
	bool needsDelayedRestore = false;
	
	// Multitoggle state
	vector<int> toggleValues;
	
	// --- value syncing helpers (like multislider) ---
	void resizeToggleValues(int newSize) {
		toggleValues.resize(newSize, 0);
		// Mirror resize into stored values so presets capture the new size
		vector<int> sv = storedValues.get();
		sv.resize(newSize, 0);
		storedValues.set(sv);
	}
	
	void syncToStoredValues() {
		vector<int> v = toggleValues;
		v.resize(numToggles.get(), 0);
		storedValues.set(v);
	}
	
	void syncFromStoredValues() {
		vector<int> v = storedValues.get();
		int n = numToggles.get();
		toggleValues.resize(n, 0);
		for (int i = 0; i < n && i < (int)v.size(); ++i) {
			toggleValues[i] = ofClamp(v[i], 0, 1);
		}
	}
	
	void restoreValuesFromStored() {
		syncFromStoredValues();
	}
	
	void pushValuesToPortal() {
		if (selectedPortalInstance != nullptr) {
			try {
				selectedPortalInstance->setValue(toggleValues);
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
	}
	// ------------------------------------------------
	
	string getActualPortalNameFromDisplayName(const string& displayName) {
		string actualName = displayName;
		size_t slashPos = actualName.find_last_of("/");
		if (slashPos != string::npos) {
			actualName = actualName.substr(slashPos + 1);
		}
		if (!actualName.empty() && actualName.length() >= 2 && actualName.substr(actualName.length() - 2) == " *") {
			actualName = actualName.substr(0, actualName.length() - 2);
		}
		return actualName;
	}
	
	void restoreSelectionByName(const string& portalName) {
		if (portalName.empty()) {
			maintainPortalSelectionByInstance();
			return;
		}
		
		for (int i = 0; i < (int)compatiblePortals.size(); i++) {
			if (compatiblePortals[i] != nullptr) {
				try {
					if (compatiblePortals[i]->getName() == portalName) {
						selectedPortalIndex = i;
						selectedPortalInstance = compatiblePortals[i];
						return;
					}
				} catch (...) {
					continue;
				}
			}
		}
		
		maintainPortalSelectionByInstance();
	}
	
	void updatePortalListOnly() {
		vector<string> newPortalNames;
		vector<portal<vector<int>>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		vector<portal<vector<int>>*> typedPortals = ofxOceanodeShared::getAllPortals<vector<int>>();
		string currentScope = getParents();

		for (auto* portalPtr : typedPortals) {
			if (portalPtr != nullptr) {
				bool scopeMatches = false;
				
				if (globalSearch.get()) {
					scopeMatches = true;
				} else {
					if (portalPtr->isLocal()) {
						scopeMatches = (portalPtr->getParents() == currentScope);
					} else {
						scopeMatches = true;
					}
				}
				
				if (scopeMatches) {
					string portalName = portalPtr->getName();
					
					if (uniquePortalNames.find(portalName) == uniquePortalNames.end()) {
						string displayName = portalName;

						if (globalSearch.get()) {
							string portalScope = portalPtr->getParents();
							if (!portalScope.empty() && portalScope != currentScope) {
								displayName = portalScope + "/" + portalName;
							}
						}

						if (!portalPtr->isLocal()) {
							displayName += " *";
						}

						newPortalNames.push_back(displayName);
						newCompatiblePortals.push_back(portalPtr);
						uniquePortalNames.insert(portalName);
					}
				}
			}
		}

		portalNames = newPortalNames;
		compatiblePortals = newCompatiblePortals;

		if (portalNames.empty()) {
			portalNames.push_back("No Compatible Portals");
			selectedPortalInstance = nullptr;
		}
	}
	
	void updatePortalList() {
		vector<string> newPortalNames;
		vector<portal<vector<int>>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		vector<portal<vector<int>>*> typedPortals = ofxOceanodeShared::getAllPortals<vector<int>>();
		string currentScope = getParents();

		for (auto* portalPtr : typedPortals) {
			if (portalPtr != nullptr) {
				bool scopeMatches = false;
				
				if (globalSearch.get()) {
					scopeMatches = true;
				} else {
					if (portalPtr->isLocal()) {
						scopeMatches = (portalPtr->getParents() == currentScope);
					} else {
						scopeMatches = true;
					}
				}
				
				if (scopeMatches) {
					string portalName = portalPtr->getName();
					
					if (uniquePortalNames.find(portalName) == uniquePortalNames.end()) {
						string displayName = portalName;

						if (globalSearch.get()) {
							string portalScope = portalPtr->getParents();
							if (!portalScope.empty() && portalScope != currentScope) {
								displayName = portalScope + "/" + portalName;
							}
						}

						if (!portalPtr->isLocal()) {
							displayName += " *";
						}

						newPortalNames.push_back(displayName);
						newCompatiblePortals.push_back(portalPtr);
						uniquePortalNames.insert(portalName);
					}
				}
			}
		}

		if (newPortalNames != portalNames) {
			string currentlySelectedPortalName = "";
			if (selectedPortalIndex >= 0 && selectedPortalIndex < (int)portalNames.size()) {
				currentlySelectedPortalName = getActualPortalNameFromDisplayName(portalNames[selectedPortalIndex]);
			}
			
			portalNames = newPortalNames;
			compatiblePortals = newCompatiblePortals;

			if (portalNames.empty()) {
				portalNames.push_back("No Compatible Portals");
				selectedPortalInstance = nullptr;
			}

			try {
				ofxOceanodeInspectorController::registerInspectorDropdown("Multitoggle", "Portal", portalNames);
				selectedPortalIndex.setMin(0);
				selectedPortalIndex.setMax(std::max(0, (int)portalNames.size() - 1));
			} catch (...) {
				// Ignore errors when updating dropdown options
			}

			if (!currentlySelectedPortalName.empty()) {
				restoreSelectionByName(currentlySelectedPortalName);
			} else {
				restoreSelectionByName(selectedPortalName.get());
			}
		}
	}
	
	void maintainPortalSelectionByInstance() {
		if (!selectedPortalName.get().empty()) {
			for (int i = 0; i < (int)compatiblePortals.size(); i++) {
				if (compatiblePortals[i] != nullptr) {
					try {
						if (compatiblePortals[i]->getName() == selectedPortalName.get()) {
							selectedPortalIndex = i;
							selectedPortalInstance = compatiblePortals[i];
							return;
						}
					} catch (...) {
						continue;
					}
				}
			}
		}
		
		if (selectedPortalInstance != nullptr && !compatiblePortals.empty()) {
			for (int i = 0; i < (int)compatiblePortals.size(); i++) {
				if (compatiblePortals[i] == selectedPortalInstance) {
					selectedPortalIndex = i;
					try {
						string portalName = selectedPortalInstance->getName();
						if (selectedPortalName.get() != portalName) {
							selectedPortalName.set(portalName);
						}
					} catch (...) {
						selectedPortalInstance = nullptr;
						selectedPortalName.set("");
					}
					return;
				}
			}
		}
		
		if (!compatiblePortals.empty() && compatiblePortals[0] != nullptr) {
			try {
				selectedPortalIndex = 0;
				selectedPortalInstance = compatiblePortals[0];
				selectedPortalName.set(compatiblePortals[0]->getName());
			} catch (...) {
				selectedPortalIndex = 0;
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		} else {
			selectedPortalIndex = 0;
			selectedPortalInstance = nullptr;
			selectedPortalName.set("");
		}
	}
	
	void updateSelectedPortalInstance() {
		if (selectedPortalIndex >= 0 && selectedPortalIndex < (int)compatiblePortals.size() &&
			compatiblePortals[selectedPortalIndex] != nullptr) {
			selectedPortalInstance = compatiblePortals[selectedPortalIndex];
			if (selectedPortalInstance != nullptr) {
				try {
					string portalName = selectedPortalInstance->getName();
					if (selectedPortalName.get() != portalName) {
						selectedPortalName.set(portalName);
					}
				} catch (...) {
					selectedPortalInstance = nullptr;
					selectedPortalName.set("");
				}
			}
		} else {
			selectedPortalInstance = nullptr;
			selectedPortalName.set("");
		}
	}
	
	void updateMultitoggleFromPortal() {
		if (selectedPortalInstance != nullptr) {
			try {
				vector<int> portalValues = selectedPortalInstance->getValue();
				int n = numToggles.get();
				toggleValues.resize(n, 0);
				
				for (int i = 0; i < n && i < (int)portalValues.size(); i++) {
					toggleValues[i] = ofClamp(portalValues[i], 0, 1);
				}
				
				// Keep stored values in sync (except during preset loading)
				if (!ofxOceanodeShared::isPresetLoading()) {
					syncToStoredValues();
				}
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
		
		if (selectedPortalIndex >= 0 && selectedPortalIndex < (int)compatiblePortals.size() &&
			compatiblePortals[selectedPortalIndex] != nullptr) {
			try {
				selectedPortalInstance = compatiblePortals[selectedPortalIndex];
				string portalName = selectedPortalInstance->getName();
				if (selectedPortalName.get() != portalName) {
					selectedPortalName.set(portalName);
				}
				vector<int> portalValues = selectedPortalInstance->getValue();
				int n = numToggles.get();
				toggleValues.resize(n, 0);
				
				for (int i = 0; i < n && i < (int)portalValues.size(); i++) {
					toggleValues[i] = ofClamp(portalValues[i], 0, 1);
				}
				
				if (!ofxOceanodeShared::isPresetLoading()) {
					syncToStoredValues();
				}
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
		
		int n = numToggles.get();
		toggleValues.resize(n, 0);
	}
	
	void setPortalValue(const vector<int>& values) {
		// Update local + stored + portal (like multislider)
		toggleValues = values;
		for (auto &v : toggleValues) v = ofClamp(v, 0, 1);
		syncToStoredValues();
		pushValuesToPortal();
	}
	
	void drawMultitoggle() {
		// Draw the multitoggle name above if not empty
		string name = toggleName.get();
		if (!name.empty()) {
			ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
			ImVec2 pos = ImGui::GetCursorPos();
			
			float multitoggleW = toggleWidth.get();
			ImGui::SetCursorPosX(pos.x + (multitoggleW - textSize.x) * 0.5f);
			ImGui::Text("%s", name.c_str());
			ImGui::Spacing();
		}
		
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		float width = toggleWidth.get();
		float height = toggleHeight.get();
		int num_toggles = numToggles.get();
		int num_rows = rows.get();
		
		if ((int)toggleValues.size() != num_toggles) {
			toggleValues.resize(num_toggles, 0);
		}
		
		// Calculate grid layout - columns based on rows
		int columns = (num_toggles + num_rows - 1) / num_rows; // Ceiling division
		float cellW = width / columns;
		float cellH = height / num_rows;
		
		ImGui::InvisibleButton("MultitoggleButton", ImVec2(width, height));
		
		bool isHovered = ImGui::IsItemHovered();
		
		if (ImGui::IsItemClicked()) {
			ImVec2 mousePos = ImGui::GetIO().MousePos;
			float mouseX = mousePos.x - pos.x;
			float mouseY = mousePos.y - pos.y;
			
			int col = (int)(mouseX / cellW);
			int row = (int)(mouseY / cellH);
			int toggleIndex = row * columns + col;
			
			if (toggleIndex >= 0 && toggleIndex < num_toggles) {
				toggleValues[toggleIndex] = (toggleValues[toggleIndex] == 0) ? 1 : 0;
				// Persist like multislider
				syncToStoredValues();
				pushValuesToPortal();
			}
		}
		
		// Colors
		ImU32 offColor   = IM_COL32(100, 100, 100, 255);
		ImU32 onColor    = IM_COL32(0, 150, 255, 255);
		ImU32 borderColor= IM_COL32(150, 150, 150, 255);
		ImU32 hoverColor = IM_COL32(255, 255, 255, 50);
		
		// Draw toggles
		for (int i = 0; i < num_toggles; i++) {
			int row = i / columns;
			int col = i % columns;
			
			float toggleX = pos.x + col * cellW;
			float toggleY = pos.y + row * cellH;
			
			ImVec2 toggleMin(toggleX, toggleY);
			ImVec2 toggleMax(toggleX + cellW, toggleY + cellH);
			
			ImU32 currentColor = (toggleValues[i] == 1) ? onColor : offColor;
			drawList->AddRectFilled(toggleMin, toggleMax, currentColor);
			drawList->AddRect(toggleMin, toggleMax, borderColor, 0, 0, 1.0f);
			
			if (isHovered) {
				ImVec2 mousePos = ImGui::GetIO().MousePos;
				float mouseX = mousePos.x - pos.x;
				float mouseY = mousePos.y - pos.y;
				
				if (mouseX >= col * cellW && mouseX <= (col + 1) * cellW &&
					mouseY >= row * cellH && mouseY <= (row + 1) * cellH) {
					drawList->AddRectFilled(toggleMin, toggleMax, hoverColor);
				}
			}
		}
		
		if (isHovered) {
			string tooltipText = "Multitoggle (" + ofToString(num_toggles) + " toggles)";
			if (selectedPortalInstance != nullptr) {
				try {
					tooltipText += "\nConnected to: " + selectedPortalInstance->getName();
				} catch (...) {
					tooltipText += "\nNo portal connected";
				}
			} else {
				tooltipText += "\nNo portal connected";
			}
			ImGui::SetTooltip("%s", tooltipText.c_str());
		}
	}
};

#endif /* multitoggle_h */
