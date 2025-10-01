#ifndef rangedSlider_h
#define rangedSlider_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "portal.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <set>

class rangedSlider : public ofxOceanodeNodeModel {
public:
	rangedSlider() : ofxOceanodeNodeModel("Ranged Slider") {
		selectedPortalInstance = nullptr;
	}
	
	void setup() override {
		description = "A range slider with two handles for min/max values, outputs vector<float> with size 2.";
		
		setFlags(ofxOceanodeNodeModelFlags_TransparentNode);
		
		// Inspector parameters for slider appearance and connection
		addInspectorParameter(sliderName.set("Name", "Range"));
		addInspectorParameter(sliderWidth.set("Width", 200.0f, 50.0f, 500.0f));
		addInspectorParameter(sliderHeight.set("Height", 20.0f, 10.0f, 50.0f));
		addInspectorParameter(absoluteMin.set("Absolute Min", 0.0f, -1000.0f, 1000.0f));
		addInspectorParameter(absoluteMax.set("Absolute Max", 1.0f, -1000.0f, 1000.0f));
		addInspectorParameter(quantization.set("Quantization", 0, 0, 100));
		addInspectorParameter(precision.set("Precision", 3, 0, 10));
		addInspectorParameter(globalSearch.set("Global Search", false));
		
		// Portal name and values for preset save/load
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));
		addInspectorParameter(currentMinValue.set("Current Min", 0.0f, -1000.0f, 1000.0f));
		addInspectorParameter(currentMaxValue.set("Current Max", 1.0f, -1000.0f, 1000.0f));
		
		// Initialize portal list
		updatePortalListOnly();
		
		// Register dropdown options with inspector system
		ofxOceanodeInspectorController::registerInspectorDropdown("Ranged Slider", "Portal", portalNames);
		
		// Create as inspector parameter with dropdown support
		selectedPortalIndex.set("Portal", 0, 0, std::max(0, (int)portalNames.size() - 1));
		addInspectorParameter(selectedPortalIndex);
		
		// Initialize slider state
		currentMinValue = absoluteMin.get();
		currentMaxValue = absoluteMax.get();
		activeHandle = -1; // -1 = none, 0 = min handle, 1 = max handle
		
		// Add custom GUI region for the slider
		addCustomRegion(sliderRegion.set("Range Slider", [this](){
			drawSlider();
		}), [this](){
			drawSlider();
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
			restoreSelectionByName(selectedPortalName.get());
			// After restoring portal connection, push our stored values to the portal
			pushValuesToPortal();
		});
		
		// Listen for value changes to push to portal
		minValueListener = currentMinValue.newListener([this](float &val){
			if (!ofxOceanodeShared::isPresetLoading()) {
				pushValuesToPortal();
			}
		});
		
		maxValueListener = currentMaxValue.newListener([this](float &val){
			if (!ofxOceanodeShared::isPresetLoading()) {
				pushValuesToPortal();
			}
		});
		
		// Initialize the selection properly after setup
		updateSelectedPortalInstance();
		updateValueFromPortal();
	}
	
	void update(ofEventArgs &args) override {
		// Update portal list
		updatePortalList();
		
		// Handle delayed restoration from preset loading
		if (needsDelayedRestore) {
			updatePortalListOnly();
			updatePortalList();
			restoreSelectionByName(selectedPortalName.get());
			// Push our stored values to the portal after restoration
			pushValuesToPortal();
			needsDelayedRestore = false;
		}
		
		// Only pull values from portal if we're not actively dragging
		if (activeHandle == -1) {
			updateValueFromPortal();
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
	ofParameter<float> absoluteMin;
	ofParameter<float> absoluteMax;
	ofParameter<int> quantization;
	ofParameter<int> precision;
	ofParameter<bool> globalSearch;
	ofParameter<string> selectedPortalName;
	ofParameter<int> selectedPortalIndex;
	ofParameter<float> currentMinValue;
	ofParameter<float> currentMaxValue;
	
	// Event listeners
	ofEventListeners listeners;
	ofEventListener dropdownListener;
	ofEventListener presetLoadedListener;
	ofEventListener globalSearchListener;
	ofEventListener minValueListener;
	ofEventListener maxValueListener;
	customGuiRegion sliderRegion;
	
	// Portal management - now for vector<float>
	vector<string> portalNames;
	vector<portal<vector<float>>*> compatiblePortals;
	portal<vector<float>>* selectedPortalInstance;
	bool needsDelayedRestore = false;
	
	// Slider state
	int activeHandle; // -1 = none, 0 = min handle, 1 = max handle
	
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
				ofxOceanodeInspectorController::registerInspectorDropdown("Ranged Slider", "Portal", portalNames);
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
	
	void updateValueFromPortal() {
		if (selectedPortalInstance != nullptr) {
			try {
				vector<float> values = selectedPortalInstance->getValue();
				if (values.size() >= 2) {
					// Only update our parameters if values actually changed
					if (currentMinValue.get() != values[0]) {
						currentMinValue.set(values[0]);
					}
					if (currentMaxValue.get() != values[1]) {
						currentMaxValue.set(values[1]);
					}
				} else if (values.size() == 1) {
					if (currentMinValue.get() != values[0]) {
						currentMinValue.set(values[0]);
					}
					if (currentMaxValue.get() != values[0]) {
						currentMaxValue.set(values[0]);
					}
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
				vector<float> values = selectedPortalInstance->getValue();
				if (values.size() >= 2) {
					if (currentMinValue.get() != values[0]) {
						currentMinValue.set(values[0]);
					}
					if (currentMaxValue.get() != values[1]) {
						currentMaxValue.set(values[1]);
					}
				}
				return;
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
	}
	
	void pushValuesToPortal() {
		if (selectedPortalInstance != nullptr) {
			try {
				vector<float> values = {currentMinValue.get(), currentMaxValue.get()};
				selectedPortalInstance->setValue(values);
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName.set("");
			}
		}
	}
	
	void setPortalValue(float minVal, float maxVal) {
		// Update our stored parameters
		currentMinValue.set(minVal);
		currentMaxValue.set(maxVal);
		
		// Push to portal
		pushValuesToPortal();
	}
	
	float quantizeValue(float value) {
		if (quantization.get() <= 0) {
			return value; // No quantization
		}
		
		float range = absoluteMax.get() - absoluteMin.get();
		float step = range / quantization.get();
		float normalizedValue = (value - absoluteMin.get()) / range;
		float quantizedNormalized = round(normalizedValue * quantization.get()) / quantization.get();
		return absoluteMin.get() + quantizedNormalized * range;
	}
	
	void drawSlider() {
		// Draw the slider name above the slider if it's not empty
		string name = sliderName.get();
		if (!name.empty()) {
			ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
			ImVec2 pos = ImGui::GetCursorPos();
			
			// Center the text above the slider
			float sliderW = sliderWidth.get();
			ImGui::SetCursorPosX(pos.x + (sliderW - textSize.x) * 0.5f);
			ImGui::Text("%s", name.c_str());
			ImGui::Spacing();
		}
		
		float width = sliderWidth.get();
		float height = sliderHeight.get();
		float absMin = absoluteMin.get();
		float absMax = absoluteMax.get();
		float minVal = currentMinValue.get();
		float maxVal = currentMaxValue.get();
		
		// Ensure absMin < absMax
		if (absMin >= absMax) {
			absMax = absMin + 0.001f;
		}
		
		// Ensure minVal <= maxVal
		if (minVal > maxVal) {
			float temp = minVal;
			minVal = maxVal;
			maxVal = temp;
			setPortalValue(minVal, maxVal);
		}
		
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		// Create invisible button for interaction
		ImGui::InvisibleButton("range_slider_area", ImVec2(width, height));
		
		bool isHovered = ImGui::IsItemHovered();
		bool isActive = ImGui::IsItemActive();
		
		// Calculate handle positions
		float minNormalized = ofClamp((minVal - absMin) / (absMax - absMin), 0.0f, 1.0f);
		float maxNormalized = ofClamp((maxVal - absMin) / (absMax - absMin), 0.0f, 1.0f);
		float minHandleX = minNormalized * width;
		float maxHandleX = maxNormalized * width;
		float handleRadius = height * 0.4f;
		
		// Check which handle is being interacted with
		if (isActive && ImGui::IsMouseDragging(0, 0.0f)) {
			ImVec2 mousePos = ImGui::GetMousePos();
			float localX = mousePos.x - pos.x;
			float normalizedValue = ofClamp(localX / width, 0.0f, 1.0f);
			float newValue = absMin + normalizedValue * (absMax - absMin);
			
			// Apply quantization if enabled
			newValue = quantizeValue(newValue);
			
			if (activeHandle == -1) {
				// Determine which handle is closer
				float distToMin = abs(localX - minHandleX);
				float distToMax = abs(localX - maxHandleX);
				activeHandle = (distToMin < distToMax) ? 0 : 1;
			}
			
			if (activeHandle == 0) { // Min handle
				float newMinValue = ofClamp(newValue, absMin, maxVal);
				setPortalValue(newMinValue, maxVal);
			} else { // Max handle
				float newMaxValue = ofClamp(newValue, minVal, absMax);
				setPortalValue(minVal, newMaxValue);
			}
		} else if (!isActive) {
			activeHandle = -1;
		}
		
		// Colors
		ImU32 trackColor = IM_COL32(100, 100, 100, 255);
		ImU32 rangeColor = IM_COL32(0, 150, 255, 255);
		ImU32 handleColor = IM_COL32(255, 255, 255, 255);
		ImU32 handleActiveColor = IM_COL32(220, 220, 220, 255);
		ImU32 handleShadow = IM_COL32(0, 0, 0, 100);
		
		// Draw track
		ImVec2 trackMin(pos.x, pos.y + height * 0.4f);
		ImVec2 trackMax(pos.x + width, pos.y + height * 0.6f);
		drawList->AddRectFilled(trackMin, trackMax, trackColor, height * 0.1f);
		
		// Draw range (between handles)
		ImVec2 rangeMin(pos.x + minHandleX, pos.y + height * 0.4f);
		ImVec2 rangeMax(pos.x + maxHandleX, pos.y + height * 0.6f);
		drawList->AddRectFilled(rangeMin, rangeMax, rangeColor, height * 0.1f);
		
		// Draw quantization marks if enabled
		if (quantization.get() > 0) {
			float step = width / quantization.get();
			for (int i = 1; i < quantization.get(); i++) {
				float markX = pos.x + i * step;
				ImVec2 markTop(markX, pos.y + height * 0.3f);
				ImVec2 markBottom(markX, pos.y + height * 0.7f);
				drawList->AddLine(markTop, markBottom, IM_COL32(200, 200, 200, 150), 1.0f);
			}
		}
		
		// Draw handles
		ImVec2 minHandleCenter(pos.x + minHandleX, pos.y + height * 0.5f);
		ImVec2 maxHandleCenter(pos.x + maxHandleX, pos.y + height * 0.5f);
		
		// Handle shadows
		ImVec2 shadowOffset(1.0f, 1.0f);
		drawList->AddCircleFilled(minHandleCenter + shadowOffset, handleRadius, handleShadow);
		drawList->AddCircleFilled(maxHandleCenter + shadowOffset, handleRadius, handleShadow);
		
		// Min handle
		ImU32 minHandleCol = (activeHandle == 0) ? handleActiveColor : handleColor;
		drawList->AddCircleFilled(minHandleCenter, handleRadius, minHandleCol);
		
		// Max handle
		ImU32 maxHandleCol = (activeHandle == 1) ? handleActiveColor : handleColor;
		drawList->AddCircleFilled(maxHandleCenter, handleRadius, maxHandleCol);
		
		// Handle highlights
		ImVec2 minHighlight(minHandleCenter.x - handleRadius * 0.3f, minHandleCenter.y - handleRadius * 0.3f);
		ImVec2 maxHighlight(maxHandleCenter.x - handleRadius * 0.3f, maxHandleCenter.y - handleRadius * 0.3f);
		drawList->AddCircleFilled(minHighlight, handleRadius * 0.3f, IM_COL32(255, 255, 255, 150));
		drawList->AddCircleFilled(maxHighlight, handleRadius * 0.3f, IM_COL32(255, 255, 255, 150));
		
		// Show value text below slider
		string format = "%." + ofToString(precision.get()) + "f";
		string minText = ofVAArgsToString(format.c_str(), minVal);
		string maxText = ofVAArgsToString(format.c_str(), maxVal);
		string valueText = minText + " - " + maxText;
		
		ImVec2 valueTextSize = ImGui::CalcTextSize(valueText.c_str());
		ImVec2 valueTextPos(pos.x + (width - valueTextSize.x) * 0.5f, pos.y + height + 2);
		drawList->AddText(valueTextPos, IM_COL32(200, 200, 200, 255), valueText.c_str());
		
		// Advance cursor past the slider and text
		ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + height + valueTextSize.y + 4));
		
		// Show tooltip with detailed info
		if (isHovered) {
			string tooltipText = "Range: " + valueText;
			tooltipText += "\nAbsolute limits: " + ofToString(absMin, precision.get()) + " - " + ofToString(absMax, precision.get());
			if (quantization.get() > 0) {
				tooltipText += "\nQuantization: " + ofToString(quantization.get()) + " steps";
			}
			tooltipText += "\nOutputs vector<float> with [min, max]";
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

#endif /* rangedSlider_h */
