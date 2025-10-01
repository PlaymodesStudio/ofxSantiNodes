#ifndef toggle_h
#define toggle_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeInspectorController.h"
#include "portal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <set>

class toggle : public ofxOceanodeNodeModel {
public:
	toggle() : ofxOceanodeNodeModel("Toggle") {
		selectedPortalInstance = nullptr;
		animationValue = 0.0f;
		toggleValue = false;
	}
	
	void setup() override {
		description = "A resizable toggle switch that connects to a boolean portal.";
		
		setFlags(ofxOceanodeNodeModelFlags_TransparentNode);

		// Inspector parameters for toggle appearance and connection
		addInspectorParameter(toggleName.set("Name", "Toggle"));
		addInspectorParameter(toggleWidth.set("Width", 60.0f, 20.0f, 200.0f));
		addInspectorParameter(toggleHeight.set("Height", 30.0f, 15.0f, 100.0f));
		addInspectorParameter(cornerRadius.set("Corner Radius", 15.0f, 0.0f, 50.0f));
		addInspectorParameter(animationSpeed.set("Animation Speed", 8.0f, 1.0f, 20.0f));
		addInspectorParameter(globalSearch.set("Global Search", false));
		
		// Portal name parameter for preset save/load
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));
		
		// Initialize portal list first
		updatePortalListOnly();
		
		// Register dropdown options with inspector system
		ofxOceanodeInspectorController::registerInspectorDropdown("Toggle", "Portal", portalNames);
		
		// Create as inspector parameter with dropdown support
		selectedPortalIndex.set("Portal", 0, 0, std::max(0, (int)portalNames.size() - 1));
		addInspectorParameter(selectedPortalIndex);
		
		// Initialize animation state and toggle value
		toggleValue = false;
		animationValue = 0.0f;
		
		// Add custom GUI region for the toggle
		addCustomRegion(toggleRegion.set("Toggle", [this](){
			drawToggle();
		}), [this](){
			drawToggle();
		});
		
		// Listen for dropdown changes
		dropdownListener = selectedPortalIndex.newListener([this](int &index){
			if (!ofxOceanodeShared::isPresetLoading()) {
				updateSelectedPortalInstance();
				updateToggleFromPortal();
			}
		});
		
		// Listen for global search changes
		globalSearchListener = globalSearch.newListener([this](bool &){
			updatePortalList();
			updateSelectedPortalInstance();
			updateToggleFromPortal();
		});
		
		// Listen for preset loading completion
		presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
			updatePortalList();
			restoreSelectionByName(selectedPortalName.get());
			updateToggleFromPortal();
		});
		
		// Initialize the selection properly after setup
		updateSelectedPortalInstance();
		updateToggleFromPortal();
	}
	
	void update(ofEventArgs &args) override {
		// Update portal list, but less frequently to avoid crashes
		static int updateCounter = 0;
		updateCounter++;
		
		// Only update portal list every 60 frames (once per second at 60fps)
		if (updateCounter % 60 == 0) {
			updatePortalList();
		}
		
		// Handle delayed restoration from preset loading
		if (needsDelayedRestore) {
			updatePortalListOnly();
			restoreSelectionByName(selectedPortalName.get());
			updateToggleFromPortal();
			needsDelayedRestore = false;
		}
		
		// Update toggle state from portal
		updateToggleFromPortal();
	}
	
	void presetRecallAfterSettingParameters(ofJson &json) override {
		needsDelayedRestore = true;
	}
	
private:
	// Parameters
	ofParameter<string> toggleName;
	ofParameter<float> toggleWidth;
	ofParameter<float> toggleHeight;
	ofParameter<float> cornerRadius;
	ofParameter<float> animationSpeed;
	ofParameter<bool> globalSearch;
	ofParameter<string> selectedPortalName;
	ofParameter<int> selectedPortalIndex;
	
	// Event listeners
	ofEventListeners listeners;
	ofEventListener dropdownListener;
	ofEventListener presetLoadedListener;
	ofEventListener globalSearchListener;
	customGuiRegion toggleRegion;
	
	// Portal management
	vector<string> portalNames;
	vector<portal<bool>*> compatiblePortals;
	portal<bool>* selectedPortalInstance; // Track the actual portal instance
	bool needsDelayedRestore = false;
	
	// Toggle state
	bool toggleValue;
	float animationValue; // 0.0 = off, 1.0 = on
	
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
		vector<portal<bool>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		// Get all boolean portals
		vector<portal<bool>*> typedPortals = ofxOceanodeShared::getAllPortals<bool>();
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
		vector<portal<bool>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		vector<portal<bool>*> typedPortals = ofxOceanodeShared::getAllPortals<bool>();
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
			// Store the currently selected portal name before updating
			string currentlySelectedPortalName = "";
			if (selectedPortalIndex >= 0 && selectedPortalIndex < portalNames.size()) {
				currentlySelectedPortalName = getActualPortalNameFromDisplayName(portalNames[selectedPortalIndex]);
			}
			
			portalNames = newPortalNames;
			compatiblePortals = newCompatiblePortals;

			if (portalNames.empty()) {
				portalNames.push_back("No Compatible Portals");
				selectedPortalInstance = nullptr;
			}

			// Update dropdown options in inspector system
			try {
				ofxOceanodeInspectorController::registerInspectorDropdown("Toggle", "Portal", portalNames);
				selectedPortalIndex.setMin(0);
				selectedPortalIndex.setMax(std::max(0, (int)portalNames.size() - 1));
			} catch (...) {
				// Ignore errors when updating dropdown options
			}

			// Restore the selection by name
			if (!currentlySelectedPortalName.empty()) {
				restoreSelectionByName(currentlySelectedPortalName);
			} else {
				restoreSelectionByName(selectedPortalName.get());
			}
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
		
		// Then try to maintain by instance
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
		
		// Try to select the first available portal
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
	
	void updateToggleFromPortal() {
		if (selectedPortalInstance != nullptr) {
			try {
				toggleValue = selectedPortalInstance->getValue();
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
		
		// If no valid portal instance, try to get one from the current index
		if (selectedPortalIndex >= 0 && selectedPortalIndex < compatiblePortals.size() &&
			compatiblePortals[selectedPortalIndex] != nullptr) {
			try {
				selectedPortalInstance = compatiblePortals[selectedPortalIndex];
				string portalName = selectedPortalInstance->getName();
				if (selectedPortalName.get() != portalName) {
					selectedPortalName.set(portalName);
				}
				toggleValue = selectedPortalInstance->getValue();
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
		
		// Fallback: use false as default
		toggleValue = false;
	}
	
	void setPortalValue(bool value) {
		if (selectedPortalInstance != nullptr) {
			try {
				selectedPortalInstance->setValue(value);
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
	}
	
	void drawToggle() {
		// Draw the toggle name above the toggle if it's not empty
		string name = toggleName.get();
		if (!name.empty()) {
			ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
			ImVec2 pos = ImGui::GetCursorPos();
			
			// Center the text above the toggle
			float toggleW = toggleWidth.get();
			ImGui::SetCursorPosX(pos.x + (toggleW - textSize.x) * 0.5f);
			ImGui::Text("%s", name.c_str());
			ImGui::Spacing();
		}
		
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		float width = toggleWidth.get();
		float height = toggleHeight.get();
		float radius = std::min(cornerRadius.get(), height * 0.5f);
		
		// Animate the toggle
		float targetValue = toggleValue ? 1.0f : 0.0f;
		float deltaTime = ImGui::GetIO().DeltaTime;
		float speed = animationSpeed.get();
		
		if (animationValue < targetValue) {
			animationValue = std::min(targetValue, animationValue + speed * deltaTime);
		} else if (animationValue > targetValue) {
			animationValue = std::max(targetValue, animationValue - speed * deltaTime);
		}
		
		// Create invisible button for interaction
		ImGui::InvisibleButton("ToggleButton", ImVec2(width, height));
		
		// Handle click
		if (ImGui::IsItemClicked()) {
			bool newValue = !toggleValue;
			setPortalValue(newValue);
			toggleValue = newValue; // Update immediately for responsiveness
		}
		
		// Colors
		ImU32 bgColorOff = IM_COL32(100, 100, 100, 255);
		ImU32 bgColorOn = IM_COL32(0, 150, 255, 255);
		ImU32 knobColor = IM_COL32(255, 255, 255, 255);
		ImU32 knobShadow = IM_COL32(0, 0, 0, 50);
		
		// Interpolate background color
		float r1 = ((bgColorOff >> 24) & 0xFF) / 255.0f;
		float g1 = ((bgColorOff >> 16) & 0xFF) / 255.0f;
		float b1 = ((bgColorOff >> 8) & 0xFF) / 255.0f;
		float a1 = (bgColorOff & 0xFF) / 255.0f;
		
		float r2 = ((bgColorOn >> 24) & 0xFF) / 255.0f;
		float g2 = ((bgColorOn >> 16) & 0xFF) / 255.0f;
		float b2 = ((bgColorOn >> 8) & 0xFF) / 255.0f;
		float a2 = (bgColorOn & 0xFF) / 255.0f;
		
		float r = r1 + (r2 - r1) * animationValue;
		float g = g1 + (g2 - g1) * animationValue;
		float b = b1 + (b2 - b1) * animationValue;
		float a = a1 + (a2 - a1) * animationValue;
		
		ImU32 bgColor = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
		
		// Draw background
		ImVec2 toggleMin(pos.x, pos.y);
		ImVec2 toggleMax(pos.x + width, pos.y + height);
		drawList->AddRectFilled(toggleMin, toggleMax, bgColor, radius);
		
		// Add subtle border
		ImU32 borderColor = IM_COL32(200, 200, 200, 100);
		drawList->AddRect(toggleMin, toggleMax, borderColor, radius, 0, 1.0f);
		
		// Calculate knob position and size
		float knobRadius = (height * 0.4f);
		float knobMargin = (height - knobRadius * 2) * 0.5f;
		float knobTravel = width - height;
		float knobX = pos.x + knobMargin + knobRadius + (knobTravel * animationValue);
		float knobY = pos.y + height * 0.5f;
		
		// Draw knob shadow
		ImVec2 shadowOffset(1.0f, 1.0f);
		drawList->AddCircleFilled(
			ImVec2(knobX + shadowOffset.x, knobY + shadowOffset.y),
			knobRadius,
			knobShadow
		);
		
		// Draw knob
		drawList->AddCircleFilled(ImVec2(knobX, knobY), knobRadius, knobColor);
		
		// Add knob highlight
		ImU32 highlightColor = IM_COL32(255, 255, 255, 150);
		drawList->AddCircleFilled(
			ImVec2(knobX - knobRadius * 0.3f, knobY - knobRadius * 0.3f),
			knobRadius * 0.3f,
			highlightColor
		);
		
		// Hover effect
		if (ImGui::IsItemHovered()) {
			ImU32 hoverColor = IM_COL32(255, 255, 255, 30);
			drawList->AddRectFilled(toggleMin, toggleMax, hoverColor, radius);
		}
		
		// Show tooltip with current state and portal info
		if (ImGui::IsItemHovered()) {
			string tooltipText = toggleValue ? "ON" : "OFF";
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

#endif /* toggle_h */
