#ifndef portalSelector_h
#define portalSelector_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "portal.h"
#include <set>

// Template class for specific data types
template<typename T>
class portalSelector : public ofxOceanodeNodeModel {
public:
	portalSelector(string typelabel, T defaultVal) : ofxOceanodeNodeModel("Portal Selector " + typelabel) {
		description = "Selects a " + typelabel + " portal from the patch and outputs its data. Global portals are marked with *.";
		this->defaultValue = defaultVal;
		this->selectedPortalInstance = nullptr;
	}

	void setup() override {
		// Initialize portal list first
		updatePortalListOnly();

		// Add dropdown parameter with initial options
		addParameterDropdown(selectedPortalIndex, "Portal", 0, portalNames);

		// Add hidden parameter to store the selected portal name for presets
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));

		// Add output parameter with proper bounds for numeric types
		if constexpr (std::is_arithmetic<T>::value) {
			addOutputParameter(output.set("Output", defaultValue,
				std::numeric_limits<T>::lowest(),
				std::numeric_limits<T>::max()));
		} else {
			addOutputParameter(output.set("Output", defaultValue));
		}

		// Listen for dropdown changes
		dropdownListener = selectedPortalIndex.newListener([this](int &index){
			// Don't update during preset loading to avoid crashes
			if (!ofxOceanodeShared::isPresetLoading()) {
				updateSelectedPortalInstance();
				updateOutputFromSelectedPortal();
			}
		});

		// Listen for preset loading completion to restore portal selection
		presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
			// After preset loading, update portal list and restore selection
			updatePortalList();
			maintainPortalSelectionByInstance();
			updateOutputFromSelectedPortal();
		});

		// IMPORTANT: Initialize the selection properly after setup
		updateSelectedPortalInstance();
		updateOutputFromSelectedPortal();
	}

	void update(ofEventArgs &args) override {
		// Check if we need to update the portal list
		updatePortalList();
		updateOutputFromSelectedPortal();
	}

protected:
	ofParameter<int> selectedPortalIndex;
	ofParameter<T> output;
	ofParameter<string> selectedPortalName; // For preset save/load
	ofEventListener dropdownListener;
	ofEventListener presetLoadedListener;
	vector<string> portalNames;
	vector<portal<T>*> compatiblePortals;
	T defaultValue;
	portal<T>* selectedPortalInstance; // Track the actual portal instance
	
	void updatePortalListOnly() {
		vector<string> newPortalNames;
		vector<portal<T>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		// Get all portals of this specific type from the shared instance
		vector<portal<T>*> typedPortals = ofxOceanodeShared::getAllPortals<T>();

		// Iterate through all portals of this type
		for (auto* portalPtr : typedPortals) {
			if (portalPtr != nullptr) {
				string portalName = portalPtr->getName();
				
				// Check if we've already added this portal name
				if (uniquePortalNames.find(portalName) == uniquePortalNames.end()) {
					string displayName = portalName;

					// Add asterisk for global portals
					if (!portalPtr->isLocal()) {
						displayName += " *";
					}

					newPortalNames.push_back(displayName);
					newCompatiblePortals.push_back(portalPtr);
					uniquePortalNames.insert(portalName);
				}
			}
		}

		// Update the lists
		portalNames = newPortalNames;
		compatiblePortals = newCompatiblePortals;

		// Handle empty portal list
		if (portalNames.empty()) {
			portalNames.push_back("No Compatible Portals");
			selectedPortalInstance = nullptr;
		}
	}
	
	void updatePortalList() {
		vector<string> newPortalNames;
		vector<portal<T>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		// Get all portals of this specific type from the shared instance
		vector<portal<T>*> typedPortals = ofxOceanodeShared::getAllPortals<T>();

		// Iterate through all portals of this type
		for (auto* portalPtr : typedPortals) {
			if (portalPtr != nullptr) {
				string portalName = portalPtr->getName();
				
				// Check if we've already added this portal name
				if (uniquePortalNames.find(portalName) == uniquePortalNames.end()) {
					string displayName = portalName;

					// Add asterisk for global portals
					if (!portalPtr->isLocal()) {
						displayName += " *";
					}

					newPortalNames.push_back(displayName);
					newCompatiblePortals.push_back(portalPtr);
					uniquePortalNames.insert(portalName);
				}
			}
		}

		// Check if the portal list has changed
		if (newPortalNames != portalNames) {
			portalNames = newPortalNames;
			compatiblePortals = newCompatiblePortals;

			// Update dropdown options
			if (portalNames.empty()) {
				portalNames.push_back("No Compatible Portals");
				selectedPortalInstance = nullptr;
			}

			// Update the dropdown parameter options and range
			getOceanodeParameter(selectedPortalIndex).setDropdownOptions(portalNames);
			
			// Update the parameter's min/max range to match the new options
			selectedPortalIndex.setMin(0);
			selectedPortalIndex.setMax(std::max(0, (int)portalNames.size() - 1));

			// Try to maintain the same portal selection by instance
			maintainPortalSelectionByInstance();
		}
	}
	
	void maintainPortalSelectionByInstance() {
		// First try to restore from saved name (for preset loading)
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
						// Portal is invalid, continue searching
						continue;
					}
				}
			}
		}
		
		// Then try to maintain by instance (for runtime changes)
		if (selectedPortalInstance != nullptr && !compatiblePortals.empty()) {
			// Look for the same portal instance (even if its name changed)
			for (int i = 0; i < compatiblePortals.size(); i++) {
				if (compatiblePortals[i] == selectedPortalInstance) {
					selectedPortalIndex = i;
					try {
						selectedPortalName = selectedPortalInstance->getName(); // Update saved name
					} catch (...) {
						// Portal is invalid, reset
						selectedPortalInstance = nullptr;
						selectedPortalName = "";
					}
					return; // Found it! No need to update the instance since it's the same object
				}
			}
		}
		
		// If we can't find either, try to select the first available portal
		if (!compatiblePortals.empty() && compatiblePortals[0] != nullptr) {
			try {
				selectedPortalIndex = 0;
				selectedPortalInstance = compatiblePortals[0];
				selectedPortalName = compatiblePortals[0]->getName();
			} catch (...) {
				// Even the first portal is invalid, stay disconnected
				selectedPortalIndex = 0;
				selectedPortalInstance = nullptr;
				selectedPortalName = "";
			}
		} else {
			// No portals available
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
					selectedPortalName = selectedPortalInstance->getName(); // Save name for presets
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
	
	void updateOutputFromSelectedPortal() {
		// Simplified validation - if we have a valid portal instance, use it
		if (selectedPortalInstance != nullptr) {
			try {
				output = selectedPortalInstance->getValue();
				return; // Success, exit early
			} catch (...) {
				// If we can't access the portal, it's probably been destroyed
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
				output = selectedPortalInstance->getValue();
				return; // Success
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName = "";
			}
		}
		
		// Fallback: use default value only if no portal is available
		output = defaultValue;
	}
};

// Template specialization for vector types
template<typename T>
class portalSelector<vector<T>> : public ofxOceanodeNodeModel {
public:
	portalSelector(string typelabel, T defaultVal) : ofxOceanodeNodeModel("Portal Selector " + typelabel) {
		description = "Selects a " + typelabel + " portal from the patch and outputs its data. Global portals are marked with *.";
		this->defaultValue = vector<T>{defaultVal};
		this->selectedPortalInstance = nullptr;
	}
	
	portalSelector(string typelabel, vector<T> defaultVal) : ofxOceanodeNodeModel("Portal Selector " + typelabel) {
		description = "Selects a " + typelabel + " portal from the patch and outputs its data. Global portals are marked with *.";
		this->defaultValue = defaultVal;
		this->selectedPortalInstance = nullptr;
	}

	void setup() override {
		// Initialize portal list first
		updatePortalListOnly();

		// Add dropdown parameter with initial options
		addParameterDropdown(selectedPortalIndex, "Portal", 0, portalNames);

		// Add hidden parameter to store the selected portal name for presets
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));

		// Add output parameter with appropriate min/max for vectors
		if constexpr (std::is_arithmetic<T>::value) {
			addOutputParameter(output.set("Output", defaultValue,
				vector<T>{std::numeric_limits<T>::lowest()},
				vector<T>{std::numeric_limits<T>::max()}));
		} else {
			addOutputParameter(output.set("Output", defaultValue));
		}

		// Listen for dropdown changes
		dropdownListener = selectedPortalIndex.newListener([this](int &index){
			// Don't update during preset loading to avoid crashes
			if (!ofxOceanodeShared::isPresetLoading()) {
				updateSelectedPortalInstance();
				updateOutputFromSelectedPortal();
			}
		});

		// Listen for preset loading completion to restore portal selection
		presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
			// After preset loading, update portal list and restore selection
			updatePortalList();
			maintainPortalSelectionByInstance();
			updateOutputFromSelectedPortal();
		});

		// IMPORTANT: Initialize the selection properly after setup
		updateSelectedPortalInstance();
		updateOutputFromSelectedPortal();
	}

	void update(ofEventArgs &args) override {
		// Check if we need to update the portal list
		updatePortalList();
		updateOutputFromSelectedPortal();
	}

protected:
	ofParameter<int> selectedPortalIndex;
	ofParameter<vector<T>> output;
	ofParameter<string> selectedPortalName; // For preset save/load
	ofEventListener dropdownListener;
	ofEventListener presetLoadedListener;
	vector<string> portalNames;
	vector<portal<vector<T>>*> compatiblePortals;
	vector<T> defaultValue;
	portal<vector<T>>* selectedPortalInstance; // Track the actual portal instance
	
	void updatePortalListOnly() {
		vector<string> newPortalNames;
		vector<portal<vector<T>>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		// Get all portals of this specific type
		vector<portal<vector<T>>*> typedPortals = ofxOceanodeShared::getAllPortals<vector<T>>();

		for (auto* portalPtr : typedPortals) {
			if (portalPtr != nullptr) {
				string portalName = portalPtr->getName();
				
				if (uniquePortalNames.find(portalName) == uniquePortalNames.end()) {
					string displayName = portalName;
					if (!portalPtr->isLocal()) {
						displayName += " *";
					}
					newPortalNames.push_back(displayName);
					newCompatiblePortals.push_back(portalPtr);
					uniquePortalNames.insert(portalName);
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
		vector<portal<vector<T>>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		vector<portal<vector<T>>*> typedPortals = ofxOceanodeShared::getAllPortals<vector<T>>();

		for (auto* portalPtr : typedPortals) {
			if (portalPtr != nullptr) {
				string portalName = portalPtr->getName();
				
				if (uniquePortalNames.find(portalName) == uniquePortalNames.end()) {
					string displayName = portalName;
					if (!portalPtr->isLocal()) {
						displayName += " *";
					}
					newPortalNames.push_back(displayName);
					newCompatiblePortals.push_back(portalPtr);
					uniquePortalNames.insert(portalName);
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

			getOceanodeParameter(selectedPortalIndex).setDropdownOptions(portalNames);
			selectedPortalIndex.setMin(0);
			selectedPortalIndex.setMax(std::max(0, (int)portalNames.size() - 1));

			maintainPortalSelectionByInstance();
		}
	}
	
	void maintainPortalSelectionByInstance() {
		// First try to restore from saved name (for preset loading)
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
		
		// Then try to maintain by instance (for runtime changes)
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
	
	void updateOutputFromSelectedPortal() {
		// Simplified validation - if we have a valid portal instance, use it
		if (selectedPortalInstance != nullptr) {
			try {
				output = selectedPortalInstance->getValue();
				return; // Success, exit early
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
				output = selectedPortalInstance->getValue();
				return; // Success
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName = "";
			}
		}
		
		// Fallback: use default value only if no portal is available
		output = defaultValue;
	}
};

// Template specialization for void type
template<>
class portalSelector<void> : public ofxOceanodeNodeModel {
public:
	portalSelector(string typelabel) : ofxOceanodeNodeModel("Portal Selector " + typelabel) {
		description = "Selects a " + typelabel + " portal from the patch and outputs its data. Global portals are marked with *.";
		this->selectedPortalInstance = nullptr;
	}

	void setup() override {
		// Initialize portal list first
		updatePortalListOnly();

		// Add dropdown parameter with initial options
		addParameterDropdown(selectedPortalIndex, "Portal", 0, portalNames);

		// Add hidden parameter to store the selected portal name for presets
		addInspectorParameter(selectedPortalName.set("Selected Portal", ""));

		// Add output parameter for void (trigger)
		addOutputParameter(output.set("Output"));

		// Listen for dropdown changes
		dropdownListener = selectedPortalIndex.newListener([this](int &index){
			// Don't update during preset loading to avoid crashes
			if (!ofxOceanodeShared::isPresetLoading()) {
				updateSelectedPortalInstance();
				updateOutputFromSelectedPortal();
			}
		});

		// Listen for preset loading completion to restore portal selection
		presetLoadedListener = ofxOceanodeShared::getPresetHasLoadedEvent().newListener([this](){
			// After preset loading, update portal list and restore selection
			updatePortalList();
			maintainPortalSelectionByInstance();
			updateOutputFromSelectedPortal();
		});

		// IMPORTANT: Initialize the selection properly after setup
		updateSelectedPortalInstance();
		updateOutputFromSelectedPortal();
	}

	void update(ofEventArgs &args) override {
		// Check if we need to update the portal list
		updatePortalList();
		updateOutputFromSelectedPortal();
	}

protected:
	ofParameter<int> selectedPortalIndex;
	ofParameter<void> output;
	ofParameter<string> selectedPortalName; // For preset save/load
	ofEventListener dropdownListener;
	ofEventListener presetLoadedListener;
	vector<string> portalNames;
	vector<portal<void>*> compatiblePortals;
	portal<void>* selectedPortalInstance; // Track the actual portal instance
	
	void updatePortalListOnly() {
		vector<string> newPortalNames;
		vector<portal<void>*> newCompatiblePortals;
		set<string> uniquePortalNames;

		// Get all void portals
		vector<portal<void>*> typedPortals = ofxOceanodeShared::getAllPortals<void>();

		for (auto* portalPtr : typedPortals) {
			if (portalPtr != nullptr) {
				string portalName = portalPtr->getName();
				
				if (uniquePortalNames.find(portalName) == uniquePortalNames.end()) {
					string displayName = portalName;
					if (!portalPtr->isLocal()) {
						displayName += " *";
					}
					newPortalNames.push_back(displayName);
					newCompatiblePortals.push_back(portalPtr);
					uniquePortalNames.insert(portalName);
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

		for (auto* portalPtr : typedPortals) {
			if (portalPtr != nullptr) {
				string portalName = portalPtr->getName();
				
				if (uniquePortalNames.find(portalName) == uniquePortalNames.end()) {
					string displayName = portalName;
					if (!portalPtr->isLocal()) {
						displayName += " *";
					}
					newPortalNames.push_back(displayName);
					newCompatiblePortals.push_back(portalPtr);
					uniquePortalNames.insert(portalName);
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

			getOceanodeParameter(selectedPortalIndex).setDropdownOptions(portalNames);
			selectedPortalIndex.setMin(0);
			selectedPortalIndex.setMax(std::max(0, (int)portalNames.size() - 1));

			maintainPortalSelectionByInstance();
		}
	}
	
	void maintainPortalSelectionByInstance() {
		// First try to restore from saved name (for preset loading)
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
		
		// Then try to maintain by instance (for runtime changes)
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
	
	void updateOutputFromSelectedPortal() {
		// Only trigger if we have a valid portal instance
		if (selectedPortalInstance != nullptr) {
			try {
				output.trigger();
				return; // Success
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
				output.trigger();
			} catch (...) {
				selectedPortalInstance = nullptr;
				selectedPortalName = "";
			}
		}
		// For void portals, we don't output anything when no portal is selected
	}
};

#endif /* portalSelector_h */
