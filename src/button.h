#ifndef button_h
#define button_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeInspectorController.h"
#include "portal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <set>

class button : public ofxOceanodeNodeModel {
public:
	button() : ofxOceanodeNodeModel("Button") {
		selectedPortalInstance = nullptr;
	}
	
	void setup() override {
		description = "A void button with transparent background, bindable to portals.";
		
		setFlags(ofxOceanodeNodeModelFlags_TransparentNode);
		
		// Inspector parameters
		addInspectorParameter(buttonName.set("Name", "Button"));
		addInspectorParameter(buttonWidth.set("Width", 80.0f, 30.0f, 200.0f));
		addInspectorParameter(buttonHeight.set("Height", 30.0f, 20.0f, 100.0f));
		addInspectorParameter(cornerRadius.set("Corner Radius", 5.0f, 0.0f, 20.0f));
		addInspectorParameter(globalSearch.set("Global Search", false));
		
		// Portal name parameter for preset save/load
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));
		
		// Initialize portal list
		updatePortalListOnly();
		
		// Register dropdown options with inspector system
		ofxOceanodeInspectorController::registerInspectorDropdown("Button", "Portal", portalNames);
		
		// Create as inspector parameter with dropdown support
		selectedPortalIndex.set("Portal", 0, 0, std::max(0, (int)portalNames.size() - 1));
		addInspectorParameter(selectedPortalIndex);
		
		// Add custom GUI region for the button
		addCustomRegion(buttonRegion.set("Button", [this](){
			drawButton();
		}), [this](){
			drawButton();
		});
		
		// Listeners
		dropdownListener = selectedPortalIndex.newListener([this](int &index){
			if (!ofxOceanodeShared::isPresetLoading()) {
				updateSelectedPortalInstance();
			}
		});
		
		globalSearchListener = globalSearch.newListener([this](bool &){
			updatePortalList();
			updateSelectedPortalInstance();
		});
		
		presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
			string nameToRestore = selectedPortalName.get();
			updatePortalList();
			restoreSelectionByName(nameToRestore);
		});
		
		updateSelectedPortalInstance();
	}
	
	void update(ofEventArgs &args) override {
		if (ofxOceanodeShared::isPresetLoading()) return;

		if (needsDelayedRestore) {
			string nameToRestore = selectedPortalName.get();
			updatePortalListOnly();
			updatePortalList();
			restoreSelectionByName(nameToRestore);
			needsDelayedRestore = false;
			return;
		}

		updatePortalList();
	}
	
	void presetRecallAfterSettingParameters(ofJson &json) override {
		needsDelayedRestore = true;
	}
	
private:
	// Parameters
	ofParameter<string> buttonName;
	ofParameter<float> buttonWidth;
	ofParameter<float> buttonHeight;
	ofParameter<float> cornerRadius;
	ofParameter<bool> globalSearch;
	ofParameter<string> selectedPortalName;
	ofParameter<int> selectedPortalIndex;
	
	// Event listeners
	ofEventListeners listeners;
	ofEventListener dropdownListener;
	ofEventListener presetLoadedListener;
	ofEventListener globalSearchListener;
	customGuiRegion buttonRegion;
	
	// Portal management
	vector<string> portalNames;
	vector<portal<void>*> compatiblePortals;
	portal<void>* selectedPortalInstance;
	bool needsDelayedRestore = false;
	
	string getActualPortalNameFromDisplayName(const string& displayName) {
		// Remove scope prefix and asterisk suffix
		string actualName = displayName;
		
		// Remove scope prefix (everything before and including "/")
		size_t slashPos = actualName.find_last_of("/");
		if (slashPos != string::npos) {
			actualName = actualName.substr(slashPos + 1);
		}
		
		// Remove asterisk suffix for global portals
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
		
		// Try to find by the stored portal name
		for (int i = 0; i < compatiblePortals.size(); i++) {
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
		
		// Fallback to maintaining by instance
		maintainPortalSelectionByInstance();
	}
	
	void updatePortalListOnly() {
		vector<string> newPortalNames;
		vector<portal<void>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		vector<portal<void>*> typedPortals = ofxOceanodeShared::getAllPortals<void>();
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
		vector<portal<void>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		vector<portal<void>*> typedPortals = ofxOceanodeShared::getAllPortals<void>();
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

		if (newPortalNames != portalNames || newCompatiblePortals != compatiblePortals) {
			string nameToRestore = selectedPortalName.get();

			portalNames = newPortalNames;
			compatiblePortals = newCompatiblePortals;

			if (portalNames.empty()) {
				portalNames.push_back("No Compatible Portals");
				selectedPortalInstance = nullptr;
			}

			try {
				ofxOceanodeInspectorController::registerInspectorDropdown("Button", "Portal", portalNames);
				selectedPortalIndex.setMin(0);
				selectedPortalIndex.setMax(std::max(0, (int)portalNames.size() - 1));
			} catch (...) {}

			restoreSelectionByName(nameToRestore);
		}
	}
	
	void maintainPortalSelectionByInstance() {
		// First try to restore from saved name
		if (!selectedPortalName.get().empty()) {
			for (int i = 0; i < compatiblePortals.size(); i++) {
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
			for (int i = 0; i < compatiblePortals.size(); i++) {
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
		if (selectedPortalIndex >= 0 && selectedPortalIndex < compatiblePortals.size() &&
			compatiblePortals[selectedPortalIndex] != nullptr) {
			selectedPortalInstance = compatiblePortals[selectedPortalIndex];
			if (selectedPortalInstance != nullptr) {
				try {
					// Keep the selectedPortalName parameter synchronized
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
	
	void triggerPortal() {
		if (selectedPortalInstance != nullptr) {
			try {
				selectedPortalInstance->trigger();
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
	}
	
	void drawButton() {
		float zoom = ofxOceanodeShared::getZoomLevel();
		// Draw the button name above if not empty
		string name = buttonName.get();
		if (!name.empty()) {
			ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
			ImVec2 pos = ImGui::GetCursorPos();
			
			float buttonW = buttonWidth.get();
			ImGui::SetCursorPosX(pos.x + (buttonW - textSize.x) * 0.5f);
			ImGui::Text("%s", name.c_str());
			ImGui::Spacing();
		}
		
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		float width = buttonWidth.get();
		float height = buttonHeight.get();
		float radius = cornerRadius.get();
		
		// Create invisible button for interaction
		ImGui::InvisibleButton("VoidButton", ImVec2(width, height));
		
		bool isPressed = ImGui::IsItemActive();
		bool isHovered = ImGui::IsItemHovered();
		
		// Handle click
		if (ImGui::IsItemClicked()) {
			triggerPortal();
		}
		
		// Colors
		ImU32 bgColor = IM_COL32(80, 80, 80, 255);
		ImU32 bgColorHover = IM_COL32(100, 100, 100, 255);
		ImU32 bgColorPressed = IM_COL32(60, 60, 60, 255);
		ImU32 borderColor = IM_COL32(150, 150, 150, 255);
		
		ImU32 currentBgColor = bgColor;
		if (isPressed) {
			currentBgColor = bgColorPressed;
		} else if (isHovered) {
			currentBgColor = bgColorHover;
		}
		
		// Draw button
		ImVec2 buttonMin(pos.x, pos.y);
		ImVec2 buttonMax(pos.x + width, pos.y + height);
		drawList->AddRectFilled(buttonMin, buttonMax, currentBgColor, radius);
		drawList->AddRect(buttonMin, buttonMax, borderColor, radius, 0, 1.0f * zoom);
		
		// Draw button text
		string buttonText = "";
		ImVec2 textSize = ImGui::CalcTextSize(buttonText.c_str());
		ImVec2 textPos(
			pos.x + (width - textSize.x) * 0.5f,
			pos.y + (height - textSize.y) * 0.5f
		);
		drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), buttonText.c_str());
		
		// Show tooltip
		if (isHovered) {
			string tooltipText = "Void Button";
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

#endif /* button_h */
