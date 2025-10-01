#ifndef multislider_h
#define multislider_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "ofxOceanodeInspectorController.h"
#include "portal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <set>

class multislider : public ofxOceanodeNodeModel {
public:
	multislider() : ofxOceanodeNodeModel("Multislider") {
		selectedPortalInstance = nullptr;
	}
	
	void setup() override {
		description = "A multislider with transparent background, bindable to vector<float> portals.";
		
		setFlags(ofxOceanodeNodeModelFlags_TransparentNode);
		
		// Inspector parameters
		addInspectorParameter(sliderName.set("Name", "Multislider"));
		addInspectorParameter(sliderWidth.set("Width", 200.0f, 100.0f, 500.0f));
		addInspectorParameter(sliderHeight.set("Height", 80.0f, 50.0f, 200.0f));
		addInspectorParameter(numSliders.set("Num Sliders", 8, 1, 32));
		addInspectorParameter(minValue.set("Min Value", 0.0f, -FLT_MAX, FLT_MAX));
		addInspectorParameter(maxValue.set("Max Value", 1.0f, -FLT_MAX, FLT_MAX));
		addInspectorParameter(globalSearch.set("Global Search", false));
		
		// Portal name and values for preset save/load
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));
		addInspectorParameter(storedValues.set("Stored Values", vector<float>(8, 0.0f), vector<float>(32, -FLT_MAX), vector<float>(32, FLT_MAX)));
		
		// Initialize portal list
		updatePortalListOnly();
		
		// Register dropdown options with inspector system
		ofxOceanodeInspectorController::registerInspectorDropdown("Multislider", "Portal", portalNames);
		
		// Create as inspector parameter with dropdown support
		selectedPortalIndex.set("Portal", 0, 0, std::max(0, (int)portalNames.size() - 1));
		addInspectorParameter(selectedPortalIndex);
		
		// Initialize slider values
		sliderValues.resize(numSliders.get(), 0.0f);
		activeSlider = -1;
		
		// Add custom GUI region for the multislider
		addCustomRegion(multisliderRegion.set("Multislider", [this](){
			drawMultislider();
		}), [this](){
			drawMultislider();
		});
		
		// Listeners
		dropdownListener = selectedPortalIndex.newListener([this](int &index){
			if (!ofxOceanodeShared::isPresetLoading()) {
				updateSelectedPortalInstance();
				updateMultisliderFromPortal();
			}
		});
		
		globalSearchListener = globalSearch.newListener([this](bool &){
			updatePortalList();
			updateSelectedPortalInstance();
			updateMultisliderFromPortal();
		});
		
		numSlidersListener = numSliders.newListener([this](int &num){
			resizeSliderValues(num);
			pushValuesToPortal();
		});
		
		presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
			updatePortalList();
			restoreSelectionByName(selectedPortalName.get());
			// Restore values from stored parameters after preset loading
			restoreValuesFromStored();
			pushValuesToPortal();
		});
		
		// Listen for stored values changes to sync with working values
		storedValuesListener = storedValues.newListener([this](vector<float> &values){
			if (!ofxOceanodeShared::isPresetLoading()) {
				syncFromStoredValues();
				pushValuesToPortal();
			}
		});
		
		updateSelectedPortalInstance();
		updateMultisliderFromPortal();
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
		
		// Only pull values from portal if we're not actively dragging
		if (activeSlider == -1) {
			updateMultisliderFromPortal();
		}
	}
	
	void presetRecallAfterSettingParameters(ofJson &json) override {
		needsDelayedRestore = true;
	}
	
private:
	// Parameters
	ofParameter<string> sliderName;
	ofParameter<float> sliderWidth;
	ofParameter<float> sliderHeight;
	ofParameter<int> numSliders;
	ofParameter<float> minValue;
	ofParameter<float> maxValue;
	ofParameter<bool> globalSearch;
	ofParameter<string> selectedPortalName;
	ofParameter<int> selectedPortalIndex;
	ofParameter<vector<float>> storedValues; // For preset save/load
	
	// Event listeners
	ofEventListeners listeners;
	ofEventListener dropdownListener;
	ofEventListener presetLoadedListener;
	ofEventListener globalSearchListener;
	ofEventListener numSlidersListener;
	ofEventListener storedValuesListener;
	customGuiRegion multisliderRegion;
	
	// Portal management
	vector<string> portalNames;
	vector<portal<vector<float>>*> compatiblePortals;
	portal<vector<float>>* selectedPortalInstance;
	bool needsDelayedRestore = false;
	
	// Multislider state
	vector<float> sliderValues;
	int activeSlider;
	
	void resizeSliderValues(int newSize) {
		sliderValues.resize(newSize, 0.0f);
		// Also resize stored values to match
		vector<float> newStoredValues = storedValues.get();
		newStoredValues.resize(newSize, 0.0f);
		storedValues.set(newStoredValues);
	}
	
	void syncToStoredValues() {
		// Update stored values to match current slider values
		vector<float> values = sliderValues;
		values.resize(numSliders.get(), 0.0f);
		storedValues.set(values);
	}
	
	void syncFromStoredValues() {
		// Update slider values from stored values
		vector<float> values = storedValues.get();
		int numSlider = numSliders.get();
		sliderValues.resize(numSlider, 0.0f);
		for (int i = 0; i < numSlider && i < values.size(); i++) {
			sliderValues[i] = values[i];
		}
	}
	
	void restoreValuesFromStored() {
		syncFromStoredValues();
	}
	
	void pushValuesToPortal() {
		if (selectedPortalInstance != nullptr) {
			try {
				selectedPortalInstance->setValue(sliderValues);
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
	}
	
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
		
		maintainPortalSelectionByInstance();
	}
	
	void updatePortalListOnly() {
		vector<string> newPortalNames;
		vector<portal<vector<float>>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		vector<portal<vector<float>>*> typedPortals = ofxOceanodeShared::getAllPortals<vector<float>>();
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
		vector<portal<vector<float>>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		vector<portal<vector<float>>*> typedPortals = ofxOceanodeShared::getAllPortals<vector<float>>();
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
			if (selectedPortalIndex >= 0 && selectedPortalIndex < portalNames.size()) {
				currentlySelectedPortalName = getActualPortalNameFromDisplayName(portalNames[selectedPortalIndex]);
			}
			
			portalNames = newPortalNames;
			compatiblePortals = newCompatiblePortals;

			if (portalNames.empty()) {
				portalNames.push_back("No Compatible Portals");
				selectedPortalInstance = nullptr;
			}

			try {
				ofxOceanodeInspectorController::registerInspectorDropdown("Multislider", "Portal", portalNames);
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
	
	void updateMultisliderFromPortal() {
		if (selectedPortalInstance != nullptr) {
			try {
				vector<float> portalValues = selectedPortalInstance->getValue();
				int numSlider = numSliders.get();
				sliderValues.resize(numSlider, 0.0f);
				
				for (int i = 0; i < numSlider && i < portalValues.size(); i++) {
					sliderValues[i] = portalValues[i];
				}
				
				// Update stored values to stay in sync (except during preset loading)
				if (!ofxOceanodeShared::isPresetLoading()) {
					syncToStoredValues();
				}
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
				vector<float> portalValues = selectedPortalInstance->getValue();
				int numSlider = numSliders.get();
				sliderValues.resize(numSlider, 0.0f);
				
				for (int i = 0; i < numSlider && i < portalValues.size(); i++) {
					sliderValues[i] = portalValues[i];
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
		
		int numSlider = numSliders.get();
		sliderValues.resize(numSlider, minValue.get());
	}
	
	void setPortalValue(const vector<float>& values) {
		sliderValues = values;
		syncToStoredValues(); // Update stored values
		pushValuesToPortal(); // Push to portal
	}
	
	void drawMultislider() {
		// Draw the multislider name above if not empty
		string name = sliderName.get();
		if (!name.empty()) {
			ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
			ImVec2 pos = ImGui::GetCursorPos();
			
			float multisliderW = sliderWidth.get();
			ImGui::SetCursorPosX(pos.x + (multisliderW - textSize.x) * 0.5f);
			ImGui::Text("%s", name.c_str());
			ImGui::Spacing();
		}
		
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		float width = sliderWidth.get();
		float height = sliderHeight.get();
		float min_val = minValue.get();
		float max_val = maxValue.get();
		int num_sliders = numSliders.get();
		
		if (min_val >= max_val) {
			max_val = min_val + 1.0f;
		}
		
		if (sliderValues.size() != num_sliders) {
			sliderValues.resize(num_sliders, min_val);
		}
		
		ImGui::InvisibleButton("MultisliderButton", ImVec2(width, height));
		
		bool isActive = ImGui::IsItemActive();
		bool isHovered = ImGui::IsItemHovered();
		
		if (isHovered || isActive) {
			ImVec2 mousePos = ImGui::GetIO().MousePos;
			float mouseX = mousePos.x - pos.x;
			float mouseY = mousePos.y - pos.y;
			
			float sliderWidth = width / (float)num_sliders;
			int hoveredSlider = (int)(mouseX / sliderWidth);
			hoveredSlider = ofClamp(hoveredSlider, 0, num_sliders - 1);
			
			if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
				activeSlider = hoveredSlider;
				float normalizedValue = 1.0f - ofClamp(mouseY / height, 0.0f, 1.0f);
				float newValue = min_val + normalizedValue * (max_val - min_val);
				sliderValues[activeSlider] = newValue;
				setPortalValue(sliderValues);
			}
		}
		
		if (!isActive) {
			activeSlider = -1;
		}
		
		// Colors
		ImU32 bgColor = IM_COL32(50, 50, 50, 255);
		ImU32 sliderColor = IM_COL32(0, 150, 255, 255);
		ImU32 borderColor = IM_COL32(100, 100, 100, 255);
		ImU32 gridColor = IM_COL32(80, 80, 80, 255);
		
		// Draw background
		ImVec2 bgMin(pos.x, pos.y);
		ImVec2 bgMax(pos.x + width, pos.y + height);
		drawList->AddRectFilled(bgMin, bgMax, bgColor);
		drawList->AddRect(bgMin, bgMax, borderColor);
		
		// Draw grid lines
		float sliderWidth = width / (float)num_sliders;
		for (int i = 1; i < num_sliders; i++) {
			float x = pos.x + i * sliderWidth;
			drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + height), gridColor);
		}
		
		for (int i = 1; i < 4; i++) {
			float y = pos.y + (height / 4.0f) * i;
			drawList->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + width, y), gridColor);
		}
		
		// Draw sliders
		for (int i = 0; i < num_sliders; i++) {
			float normalizedValue = (sliderValues[i] - min_val) / (max_val - min_val);
			normalizedValue = ofClamp(normalizedValue, 0.0f, 1.0f);
			
			float sliderX = pos.x + i * sliderWidth;
			float sliderHeight = height * normalizedValue;
			
			ImVec2 sliderMin(sliderX + 2, pos.y + height - sliderHeight);
			ImVec2 sliderMax(sliderX + sliderWidth - 2, pos.y + height);
			drawList->AddRectFilled(sliderMin, sliderMax, sliderColor);
			
			if (i == activeSlider) {
				ImU32 highlightColor = IM_COL32(255, 255, 255, 100);
				ImVec2 highlightMin(sliderX, pos.y);
				ImVec2 highlightMax(sliderX + sliderWidth, pos.y + height);
				drawList->AddRectFilled(highlightMin, highlightMax, highlightColor);
			}
		}
		
		if (isHovered) {
			ImVec2 mousePos = ImGui::GetIO().MousePos;
			float mouseX = mousePos.x - pos.x;
			float sliderWidth = width / (float)num_sliders;
			int hoveredSlider = (int)(mouseX / sliderWidth);
			hoveredSlider = ofClamp(hoveredSlider, 0, num_sliders - 1);
			
			string tooltipText = "Slider " + ofToString(hoveredSlider) + ": " + ofToString(sliderValues[hoveredSlider], 3);
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

#endif /* multislider_h */
