#ifndef value_h
#define value_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "portal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <set>

class value : public ofxOceanodeNodeModel {
public:
	value() : ofxOceanodeNodeModel("Value") {
		selectedPortalInstance = nullptr;
	}
	
	void setup() override {
		description = "A numeric input field that connects to a float portal.";
		
		// Set node flags to indicate this should be rendered transparently
		setFlags(ofxOceanodeNodeModelFlags_TransparentNode);
		
		// Inspector parameters for value appearance and connection
		addInspectorParameter(valueName.set("Name", "Value"));
		addInspectorParameter(inputWidth.set("Width", 120.0f, 50.0f, 300.0f));
		addInspectorParameter(fontSize.set("Font Size", 14.0f, 8.0f, 24.0f));
		addInspectorParameter(precision.set("Precision", 3, 0, 10));
		addInspectorParameter(globalSearch.set("Global Search", false));
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));
		
		// Initialize portal list
		updatePortalListOnly();
		
		// Register dropdown options with inspector system
		ofxOceanodeInspectorController::registerInspectorDropdown("Value", "Portal", portalNames);
		
		// Create as inspector parameter with dropdown support
		selectedPortalIndex.set("Portal", 0, 0, std::max(0, (int)portalNames.size() - 1));
		addInspectorParameter(selectedPortalIndex);
		
		// Initialize value state
		currentValue = 0.0f;
		inputBuffer[0] = '\0';
		
		// Add custom GUI region for the value input
		addCustomRegion(valueRegion.set("Value", [this](){
			drawValue();
		}), [this](){
			drawValue();
		});
		
		// Listen for dropdown changes
		dropdownListener = selectedPortalIndex.newListener([this](int &index){
			if (!ofxOceanodeShared::isPresetLoading()) {
				updateSelectedPortalInstance();
				updateValueFromPortal();
			}
		});
		
		// Listen for global search changes
		globalSearchListener = globalSearch.newListener([this](bool &){
			updatePortalList();
			updateSelectedPortalInstance();
			updateValueFromPortal();
		});
		
		// Listen for preset loading completion
		presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
			updatePortalList();
			maintainPortalSelectionByInstance();
			updateValueFromPortal();
		});
		
		// Initialize the selection properly after setup
		updateSelectedPortalInstance();
		updateValueFromPortal();
	}
	
	void update(ofEventArgs &args) override {
		// Check if we need to update the portal list
		updatePortalList();
		
		// Handle delayed restoration from preset loading
		if (needsDelayedRestore) {
			updatePortalListOnly();
			updatePortalList();
			maintainPortalSelectionByInstance();
			updateValueFromPortal();
			needsDelayedRestore = false;
		}
		
		// Update value state from portal
		updateValueFromPortal();
	}
	
	void presetRecallAfterSettingParameters(ofJson &json) override {
		needsDelayedRestore = true;
	}
	
private:
	// Parameters
	ofParameter<string> valueName;
	ofParameter<float> inputWidth;
	ofParameter<float> fontSize;
	ofParameter<int> precision;
	ofParameter<bool> globalSearch;
	ofParameter<string> selectedPortalName;
	ofParameter<int> selectedPortalIndex;
	
	// Event listeners
	ofEventListeners listeners;
	ofEventListener dropdownListener;
	ofEventListener presetLoadedListener;
	ofEventListener globalSearchListener;
	customGuiRegion valueRegion;
	
	// Portal management
	vector<string> portalNames;
	vector<portal<float>*> compatiblePortals;
	portal<float>* selectedPortalInstance;
	bool needsDelayedRestore = false;
	
	// Value state
	float currentValue;
	char inputBuffer[64];
	
	void updatePortalListOnly() {
		vector<string> newPortalNames;
		vector<portal<float>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		// Get all float portals
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
			portalNames = newPortalNames;
			compatiblePortals = newCompatiblePortals;

			if (portalNames.empty()) {
				portalNames.push_back("No Compatible Portals");
				selectedPortalInstance = nullptr;
			}

			// Update dropdown options in inspector system
			try {
				ofxOceanodeInspectorController::registerInspectorDropdown("Value", "Portal", portalNames);
				selectedPortalIndex.setMin(0);
				selectedPortalIndex.setMax(std::max(0, (int)portalNames.size() - 1));
			} catch (...) {
				// Ignore errors when updating dropdown options
			}

			maintainPortalSelectionByInstance();
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
						selectedPortalName = selectedPortalInstance->getName();
					} catch (...) {
						selectedPortalInstance = nullptr;
						selectedPortalName = "";
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
				selectedPortalName = compatiblePortals[0]->getName();
			} catch (...) {
				selectedPortalIndex = 0;
				selectedPortalInstance = nullptr;
				selectedPortalName = "";
			}
		} else {
			selectedPortalIndex = 0;
			selectedPortalInstance = nullptr;
			selectedPortalName = "";
		}
	}
	
	void updateSelectedPortalInstance() {
		if (selectedPortalIndex >= 0 && selectedPortalIndex < compatiblePortals.size() &&
			compatiblePortals[selectedPortalIndex] != nullptr) {
			selectedPortalInstance = compatiblePortals[selectedPortalIndex];
			if (selectedPortalInstance != nullptr) {
				try {
					selectedPortalName = selectedPortalInstance->getName();
				} catch (...) {
					selectedPortalInstance = nullptr;
					selectedPortalName = "";
				}
			}
		} else {
			selectedPortalInstance = nullptr;
			selectedPortalName = "";
		}
	}
	
	void updateValueFromPortal() {
		if (selectedPortalInstance != nullptr) {
			try {
				currentValue = selectedPortalInstance->getValue();
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName = "";
			}
		}
		
		// If no valid portal instance, try to get one from the current index
		if (selectedPortalIndex >= 0 && selectedPortalIndex < compatiblePortals.size() &&
			compatiblePortals[selectedPortalIndex] != nullptr) {
			try {
				selectedPortalInstance = compatiblePortals[selectedPortalIndex];
				selectedPortalName = selectedPortalInstance->getName();
				currentValue = selectedPortalInstance->getValue();
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName = "";
			}
		}
		
		// Fallback: use 0.0 as default
		currentValue = 0.0f;
	}
	
	void setPortalValue(float value) {
		if (selectedPortalInstance != nullptr) {
			try {
				selectedPortalInstance->setValue(value);
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName = "";
			}
		}
	}
	
	void drawValue() {
		// Draw the value name above the input if it's not empty
		string name = valueName.get();
		if (!name.empty()) {
			ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
			ImVec2 pos = ImGui::GetCursorPos();
			
			// Center the text above the input
			float inputW = inputWidth.get();
			ImGui::SetCursorPosX(pos.x + (inputW - textSize.x) * 0.5f);
			ImGui::Text("%s", name.c_str());
			ImGui::Spacing();
		}
		
		float width = inputWidth.get();
		ImGui::SetNextItemWidth(width);
		
		// Set font size
		ImFont* font = ImGui::GetIO().Fonts->Fonts[0]; // Default font
		float fontScale = fontSize.get() / ImGui::GetFontSize();
		ImGui::SetWindowFontScale(fontScale);
		
		// Update input buffer if value changed externally
		string format = "%." + ofToString(precision.get()) + "f";
		snprintf(inputBuffer, sizeof(inputBuffer), format.c_str(), currentValue);
		
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
		
		// Center the text in the input field
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 oldFramePadding = style.FramePadding;
		
		if (ImGui::InputText("##value_input", inputBuffer, sizeof(inputBuffer),
							ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal)) {
			// Parse the input and set portal value
			float newValue = ofToFloat(inputBuffer);
			setPortalValue(newValue);
			currentValue = newValue; // Update immediately for responsiveness
		}
		
		// Reset font scale
		ImGui::SetWindowFontScale(1.0f);
		
		ImGui::PopStyleColor(3);
		
		// Show tooltip with current value and portal info
		if (ImGui::IsItemHovered()) {
			string tooltipText = "Value: " + ofToString(currentValue, precision.get());
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
	
	void presetSave(ofJson &json) override {
		// Save the current portal name for preset loading
		if (selectedPortalInstance != nullptr) {
			try {
				json["selectedPortalName"] = selectedPortalInstance->getName();
			} catch (...) {
				json["selectedPortalName"] = "";
			}
		} else {
			json["selectedPortalName"] = "";
		}
	}
	void presetRecallBeforeSettingParameters(ofJson &json) override {
		try {
			// Handle string-to-number conversions for parameters that changed types
			vector<string> intParams = {"Portal", "Precision"};
			vector<string> floatParams = {"Width", "Font_Size"};
			
			// Convert string values to proper numbers
			for (const string& param : intParams) {
				if (json.contains(param) && json[param].is_string()) {
					try {
						int value = ofToInt(json[param].get<string>());
						json[param] = value;
					} catch (...) {
						json.erase(param);
					}
				}
			}
			
			for (const string& param : floatParams) {
				if (json.contains(param) && json[param].is_string()) {
					try {
						float value = ofToFloat(json[param].get<string>());
						json[param] = value;
					} catch (...) {
						json.erase(param);
					}
				}
			}
			
			// Handle portal name restoration
			if (json.contains("Selected_Portal") && json["Selected_Portal"].is_string()) {
				selectedPortalName.set(json["Selected_Portal"].get<string>());
			}
			
		} catch (...) {
			// If anything fails, just continue with defaults
		}
	}
};

#endif /* value_h */
