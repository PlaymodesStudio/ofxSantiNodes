#ifndef slider_h
#define slider_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeInspectorController.h"
#include "portal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <set>

class slider : public ofxOceanodeNodeModel {
public:
	slider() : ofxOceanodeNodeModel("Slider") {
		selectedPortalInstance = nullptr;
	}
	
	void setup() override {
		description = "A slider with transparent background, bindable to float portals.";
		
		setFlags(ofxOceanodeNodeModelFlags_TransparentNode);
		
		// Inspector parameters
		addInspectorParameter(sliderName.set("Name", "Slider"));
		addInspectorParameter(sliderWidth.set("Width", 150.0f, 50.0f, 300.0f));
		addInspectorParameter(sliderHeight.set("Height", 20.0f, 15.0f, 50.0f));
		addInspectorParameter(minValue.set("Min Value", 0.0f, -FLT_MAX, FLT_MAX));
		addInspectorParameter(maxValue.set("Max Value", 1.0f, -FLT_MAX, FLT_MAX));
		addInspectorParameter(globalSearch.set("Global Search", false));
		
		// Portal name parameter for preset save/load
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));
		
		// Initialize portal list
		updatePortalListOnly();
		
		// Register dropdown options with inspector system
		ofxOceanodeInspectorController::registerInspectorDropdown("Slider", "Portal", portalNames);
		
		// Create as inspector parameter with dropdown support
		selectedPortalIndex.set("Portal", 0, 0, std::max(0, (int)portalNames.size() - 1));
		addInspectorParameter(selectedPortalIndex);
		
		// Initialize slider value
		sliderValue = 0.0f;
		
		// Add custom GUI region for the slider
		addCustomRegion(sliderRegion.set("Slider", [this](){
			drawSlider();
		}), [this](){
			drawSlider();
		});
		
		// Listeners
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
			updatePortalList();
			restoreSelectionByName(selectedPortalName.get());
			updateSliderFromPortal();
		});
		
		updateSelectedPortalInstance();
		updateSliderFromPortal();
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
			updateSliderFromPortal();
			needsDelayedRestore = false;
		}
		
		updateSliderFromPortal();
	}
	
	void presetRecallAfterSettingParameters(ofJson &json) override {
		needsDelayedRestore = true;
	}
	
private:
	// Parameters
	ofParameter<string> sliderName;
	ofParameter<float> sliderWidth;
	ofParameter<float> sliderHeight;
	ofParameter<float> minValue;
	ofParameter<float> maxValue;
	ofParameter<bool> globalSearch;
	ofParameter<string> selectedPortalName;
	ofParameter<int> selectedPortalIndex;
	
	// Event listeners
	ofEventListeners listeners;
	ofEventListener dropdownListener;
	ofEventListener presetLoadedListener;
	ofEventListener globalSearchListener;
	customGuiRegion sliderRegion;
	
	// Portal management
	vector<string> portalNames;
	vector<portal<float>*> compatiblePortals;
	portal<float>* selectedPortalInstance;
	bool needsDelayedRestore = false;
	
	// Slider state
	float sliderValue;
	
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
		vector<portal<float>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		vector<portal<float>*> typedPortals = ofxOceanodeShared::getAllPortals<float>();
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
		vector<portal<float>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		vector<portal<float>*> typedPortals = ofxOceanodeShared::getAllPortals<float>();
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
				ofxOceanodeInspectorController::registerInspectorDropdown("Slider", "Portal", portalNames);
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
	
	void updateSliderFromPortal() {
		if (selectedPortalInstance != nullptr) {
			try {
				sliderValue = selectedPortalInstance->getValue();
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
		
		if (selectedPortalIndex >= 0 && selectedPortalIndex < compatiblePortals.size() &&
			compatiblePortals[selectedPortalIndex] != nullptr) {
			try {
				selectedPortalInstance = compatiblePortals[selectedPortalIndex];
				string portalName = selectedPortalInstance->getName();
				if (selectedPortalName.get() != portalName) {
					selectedPortalName.set(portalName);
				}
				sliderValue = selectedPortalInstance->getValue();
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
		
		// Fallback
		sliderValue = minValue.get();
	}
	
	void setPortalValue(float value) {
		if (selectedPortalInstance != nullptr) {
			try {
				selectedPortalInstance->setValue(value);
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
	}
	
	void drawSlider() {
		// Draw the slider name above if not empty
		string name = sliderName.get();
		if (!name.empty()) {
			ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
			ImVec2 pos = ImGui::GetCursorPos();
			
			float sliderW = sliderWidth.get();
			ImGui::SetCursorPosX(pos.x + (sliderW - textSize.x) * 0.5f);
			ImGui::Text("%s", name.c_str());
			ImGui::Spacing();
		}
		
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		float width = sliderWidth.get();
		float height = sliderHeight.get();
		float min_val = minValue.get();
		float max_val = maxValue.get();
		
		// Ensure min < max
		if (min_val >= max_val) {
			max_val = min_val + 1.0f;
		}
		
		// Create invisible button for interaction
		ImGui::InvisibleButton("SliderButton", ImVec2(width, height));
		
		bool isActive = ImGui::IsItemActive();
		bool isHovered = ImGui::IsItemHovered();
		
		// Handle dragging
		if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			ImVec2 mousePos = ImGui::GetIO().MousePos;
			float mouseX = mousePos.x - pos.x;
			float normalizedValue = ofClamp(mouseX / width, 0.0f, 1.0f);
			float newValue = min_val + normalizedValue * (max_val - min_val);
			setPortalValue(newValue);
			sliderValue = newValue; // Update immediately for responsiveness
		}
		
		// Calculate slider position
		float normalizedValue = (sliderValue - min_val) / (max_val - min_val);
		normalizedValue = ofClamp(normalizedValue, 0.0f, 1.0f);
		
		// Colors
		ImU32 trackColor = IM_COL32(100, 100, 100, 255);
		ImU32 fillColor = IM_COL32(0, 150, 255, 255);
		ImU32 knobColor = IM_COL32(255, 255, 255, 255);
		ImU32 knobColorActive = IM_COL32(220, 220, 220, 255);
		
		// Draw track
		float knobRadius = height * 0.4f;
		ImVec2 trackMin(pos.x, pos.y + height * 0.5f - knobRadius);
		ImVec2 trackMax(pos.x + width, pos.y + height * 0.5f + knobRadius);
		drawList->AddRectFilled(trackMin, trackMax, trackColor, knobRadius);
		
		// Draw fill
		ImVec2 fillMax(pos.x + width * normalizedValue, pos.y + height * 0.5f + knobRadius);
		if (normalizedValue > 0.0f) {
			drawList->AddRectFilled(trackMin, fillMax, fillColor, knobRadius);
		}
		
		// Draw knob
		float knobX = pos.x + width * normalizedValue;
		float knobY = pos.y + height * 0.5f;
		knobRadius = height * 0.4f;
		// Draw knob
		ImU32 currentKnobColor = isActive ? knobColorActive : knobColor;
		drawList->AddCircleFilled(ImVec2(knobX, knobY), knobRadius, currentKnobColor);
		
		// Draw knob border
		ImU32 borderColor = IM_COL32(150, 150, 150, 255);
		drawList->AddCircle(ImVec2(knobX, knobY), knobRadius, borderColor, 0, 1.5f);
		
		// Show tooltip
		if (isHovered) {
			string tooltipText = ofToString(sliderValue, 3);
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

#endif /* slider_h */
