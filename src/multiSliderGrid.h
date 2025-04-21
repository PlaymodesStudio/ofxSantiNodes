#ifndef multiSliderGrid_h
#define multiSliderGrid_h

#include "ofxOceanodeNodeModel.h"
#include "imgui_internal.h"

class multiSliderGrid : public ofxOceanodeNodeModel {
public:
	multiSliderGrid() : ofxOceanodeNodeModel("MultiSlider Grid") {
		description = "A customizable grid of sliders with quantization. Each slider value can be set interactively "
					 "and outputs as a vector. The grid displays both slider positions and quantization steps. "
					 "Multiple slots allow saving and recalling different patterns.";
		
		// Initialize slot tracking
		previousSlot = 0;
	}

	void setup() override {
		// Setup main parameters
		addParameter(size.set("Size", 16, 2, 64));
		addParameter(q.set("Q", 8, 2, 64));
		addParameter(minVal.set("Min", 0.0f, -FLT_MAX, FLT_MAX));
		addParameter(maxVal.set("Max", 1.0f, -FLT_MAX, FLT_MAX));
		addParameter(shift.set("Shift", 0, 0, 63));  // Add shift parameter
		addParameter(setIncremental.set("Set++"));  // Add button for incremental setup
		
		// Slot management parameters
		addParameter(slot.set("Slot", 0, 0, NUM_SLOTS - 1));
		addInspectorParameter(resetSlot.set("Reset Slot"));
		addInspectorParameter(resetAll.set("Reset All"));
		
		// File handling parameters
		addInspectorParameter(filename.set("Filename", ""));
		addInspectorParameter(openFile.set("Open"));
		addInspectorParameter(saveFile.set("Save"));
		
		// Output parameter
		addOutputParameter(output.set("Output", vector<float>(16, 0.0f), vector<float>(16, 0.0f), vector<float>(16, 1.0f)));
		
		// Inspector parameters for widget dimensions
		addInspectorParameter(width.set("Width", 240, 240, 800));
		addInspectorParameter(height.set("Height", 120, 50, 500));
		
		// Initialize storage for all slots
		initializeAllSlots();
		
		// Set values to point to current slot (0)
		values = &storage[0];
		
		// Add the custom region for drawing the grid
		addCustomRegion(customWidget, [this]() {
			drawMultiSliderGrid();
		});
		
		// Set up parameter listeners
		setupListeners();
	}
	
	void setupListeners() {
		// Listen for changes in parameter values
		listeners.push(size.newListener([this](int &s) {
			resizeValues();
		}));
		
		listeners.push(q.newListener([this](int &steps) {
			// If quantization steps change, update output values
			updateOutputValues();
		}));
		
		listeners.push(minVal.newListener([this](float &min) {
			// If min value changes, update output values
			updateOutputValues();
		}));
		
		listeners.push(maxVal.newListener([this](float &max) {
			// If max value changes, update output values
			updateOutputValues();
		}));
		
		// Add listener for shift parameter
		listeners.push(shift.newListener([this](int &newShift) {
			// When shift changes, update output values with the shift applied
			updateOutputValues();
		}));
		
		// Add listener for slot changes
		listeners.push(slot.newListener([this](int &newSlot){
			// Ignore negative slot values
			if (newSlot < 0) {
				// Restore to last valid slot without triggering the listener recursively
				slot.setWithoutEventNotifications(previousSlot);
				return;
			}
			
			// When slot changes, call switchSlot with old and new slot values
			int oldSlot = previousSlot;
			switchSlot(oldSlot, newSlot);
			previousSlot = newSlot; // Update previousSlot after the switch
		}));
		
		// Add listener for reset slot button
		resetListener = resetSlot.newListener([this](){
			resetCurrentSlot();
		});
		
		// Add listener for reset all button
		resetAllListener = resetAll.newListener([this](){
			resetAllSlots();
		});
		
		// Add listener for set incremental button
		setIncrementalListener = setIncremental.newListener([this](){
			setIncrementalValues();
		});
		
		// Add listeners for file operations
		filenameListener = filename.newListener([this](string &path){
			if (!path.empty()) {
				loadFromFile(path);
			}
		});
		
		openFileListener = openFile.newListener([this](){
			openFileDialog();
		});
		
		saveFileListener = saveFile.newListener([this](){
			if (!filename.get().empty()) {
				saveToFile(filename.get());
			} else {
				saveFileDialog();
			}
		});
	}
	
	// Initialize all slots with empty vectors
	void initializeAllSlots() {
		for (int i = 0; i < NUM_SLOTS; i++) {
			vector<float> emptyValues(size, 0.0f);
			storage[i] = emptyValues;
		}
	}
	
	void switchSlot(int oldSlotIndex, int newSlotIndex) {
		// Switch pointer to current slot's data
		values = &storage[newSlotIndex];
		
		// Update the output with data from the newly selected slot
		updateOutputValues();
	}
	
	void resizeValues() {
		// Resize all slots in storage
		for (auto& pair : storage) {
			auto& slotValues = pair.second;
			
			// Store old size for initialization
			int oldSize = slotValues.size();
			slotValues.resize(size);
			
			// Initialize new values if size increased
			for (int i = oldSize; i < size; i++) {
				slotValues[i] = 0.0f;
			}
			
			// Make sure all values are quantized
			for (int i = 0; i < size; i++) {
				slotValues[i] = round(slotValues[i] * (q - 1)) / (q - 1);
			}
		}
		
		// Update output parameter
		updateOutputValues();
	}
	
	void updateOutputValues() {
		// First, apply the shift if needed
		vector<float> shiftedValues;
		shiftedValues.resize(size);
		
		// Apply shift rotation
		int currentShift = shift.get() % size; // Ensure shift is within bounds
		if (currentShift > 0) {
			// Copy with rotation
			for (int i = 0; i < size; i++) {
				int sourceIdx = (i + size - currentShift) % size;
				shiftedValues[i] = (*values)[sourceIdx];
			}
		} else {
			// No shift, just copy
			shiftedValues = *values;
		}
		
		// Apply range mapping to shifted values
		vector<float> outputValues;
		outputValues.resize(size);
		
		for (int i = 0; i < size; i++) {
			// Map to output range
			outputValues[i] = ofMap(shiftedValues[i], 0.0f, 1.0f, minVal, maxVal);
		}
		
		// Set the output parameter with range-mapped values
		output = outputValues;
	}
	
	void resetCurrentSlot() {
		// Create a new empty vector with all zeros
		vector<float> emptyValues(size, 0.0f);
		
		// Replace the current slot's data with the empty vector
		storage[slot.get()] = emptyValues;
		
		// Since values is a pointer to storage[slot.get()],
		// it's automatically updated
		
		// Update the output to reflect the reset
		updateOutputValues();
	}
	
	void resetAllSlots() {
		// Reset all slots in storage to empty vectors
		for (int i = 0; i < NUM_SLOTS; i++) {
			storage[i] = vector<float>(size, 0.0f);
		}
		
		// Update the output to reflect the reset
		updateOutputValues();
	}
	
	void setIncrementalValues() {
		// Set values to incremental pattern (0, 1, 2, ... size-1)
		vector<float> incrementalValues(size);
		
		for (int i = 0; i < size; i++) {
			// Calculate normalized value between 0 and 1
			float normalizedValue = (float)i / (size - 1);
			
			// Quantize to the nearest allowed step if needed
			normalizedValue = round(normalizedValue * (q - 1)) / (q - 1);
			
			incrementalValues[i] = normalizedValue;
		}
		
		// Update the current slot with the incremental values
		storage[slot.get()] = incrementalValues;
		
		// Update the output
		updateOutputValues();
	}
	
	// File operation methods
	void ensureDirectoryExists() {
		string dir = ofToDataPath("MultiSliderGrids");
		if (!ofDirectory::doesDirectoryExist(dir)) {
			ofDirectory::createDirectory(dir);
		}
	}
	
	void openFileDialog() {
		ensureDirectoryExists();
		ofFileDialogResult result = ofSystemLoadDialog("Select a JSON file", false, ofToDataPath("MultiSliderGrids"));
		if (result.bSuccess) {
			filename = result.getPath();
			// loadFromFile will be called via filename listener
		}
	}
	
	void saveFileDialog() {
		ensureDirectoryExists();
		ofFileDialogResult result = ofSystemSaveDialog("multiSliderGrid.json", "Save JSON file");
		if (result.bSuccess) {
			filename = result.getPath();
			saveToFile(filename.get());
		}
	}
	
	void loadFromFile(const string &path) {
		if (path.empty()) return;
		
		// Convert relative path if needed
		string fullPath = path;
		if (!ofFilePath::isAbsolute(path)) {
			fullPath = ofToDataPath("MultiSliderGrids/" + path);
		}
		
		ofFile file(fullPath);
		if (file.exists()) {
			ofJson json = ofLoadJson(fullPath);
			loadFromJson(json);
		}
	}
	
	void saveToFile(const string &path) {
		if (path.empty()) return;
		
		ensureDirectoryExists();
		
		// Convert relative path if needed
		string fullPath = path;
		if (!ofFilePath::isAbsolute(path)) {
			fullPath = ofToDataPath("MultiSliderGrids/" + path);
		}
		
		ofJson json;
		saveToJson(json);
		ofSavePrettyJson(fullPath, json);
	}
	
	void loadFromJson(const ofJson &json) {
		// Clear existing storage
		storage.clear();
		
		// Load parameters if they exist
		if (json.count("size") > 0) {
			try {
				size = json["size"].get<int>();
			} catch (...) {
				// If failed, keep current value
			}
		}
		
		if (json.count("q") > 0) {
			try {
				q = json["q"].get<int>();
			} catch (...) {
				// If failed, keep current value
			}
		}
		
		if (json.count("shift") > 0) {
			try {
				shift = json["shift"].get<int>();
			} catch (...) {
				// If failed, keep current value
			}
		}
		
		// Extract current slot
		int currentSlotValue = 0;
		if (json.count("currentSlot") > 0) {
			currentSlotValue = json["currentSlot"].get<int>();
		}
		
		// Initialize all slots as empty vectors first
		initializeAllSlots();
		
		// Load slot data from the file
		for (auto it = json.begin(); it != json.end(); ++it) {
			if (it.key().substr(0, 9) == "slotData_") {
				int slotIndex = ofToInt(it.key().substr(9));
				auto &jsonValues = it.value();
				
				if (jsonValues.is_array()) {
					// Load the values for this slot
					vector<float> slotValues = jsonValues.get<vector<float>>();
					
					// Resize if needed
					if (slotValues.size() != size) {
						slotValues.resize(size, 0.0f);
					}
					
					// Ensure all values are quantized
					for (int i = 0; i < slotValues.size(); i++) {
						slotValues[i] = round(slotValues[i] * (q - 1)) / (q - 1);
					}
					
					// Store the loaded data in our storage map
					storage[slotIndex] = slotValues;
				}
			}
		}
		
		// Set values to point to the current slot's data
		values = &storage[currentSlotValue];
		
		// Set the slot parameter (bypass if needed)
		if (slot.get() == currentSlotValue && currentSlotValue > 0) {
			previousSlot = slot.get();
			slot = 0;
		} else if (slot.get() == currentSlotValue && currentSlotValue == 0) {
			previousSlot = slot.get();
			slot = 1;
		}
		
		// Set to the actual target value
		slot = currentSlotValue;
		
		// Update the output
		updateOutputValues();
	}
	
	void saveToJson(ofJson &json) {
		// Save parameters
		json["size"] = size.get();
		json["q"] = q.get();
		json["shift"] = shift.get();
		
		// Save which slot is currently active
		json["currentSlot"] = slot.get();
		
		// Make sure all slots are initialized
		for (int i = 0; i < NUM_SLOTS; i++) {
			if (storage.find(i) == storage.end()) {
				// Initialize any missing slots
				storage[i] = vector<float>(size, 0.0f);
			}
		}
		
		// Save ALL slots to JSON
		for (int i = 0; i < NUM_SLOTS; i++) {
			json["slotData_" + ofToString(i)] = storage[i];
		}
	}
	
	void presetSave(ofJson &json) {
		// 1. Save relevant parameters
		json["size"] = size.get();
		json["q"] = q.get();
		json["shift"] = shift.get();
		json["currentSlot"] = slot.get();
		json["filename"] = filename.get();
		
		// 2. Save ALL slots
		for (int i = 0; i < NUM_SLOTS; i++) {
			if (storage.find(i) != storage.end()) {
				json["slotData_" + ofToString(i)] = storage[i];
			}
		}
	}
	
	void presetRecallAfterSettingParameters(ofJson &json) {
		// 1. Handle the filename parameter if it exists
		if (json.count("filename") > 0) {
			string savedFilename = json["filename"].get<string>();
			
			// Update the filename parameter without triggering auto-load
			filename = savedFilename;
			
			// If filename is not empty, try to load the file
			if (!savedFilename.empty()) {
				string fullPath;
				if (!ofFilePath::isAbsolute(savedFilename)) {
					fullPath = ofToDataPath("MultiSliderGrids/" + savedFilename);
				} else {
					fullPath = savedFilename;
				}
				
				ofFile file(fullPath);
				if (file.exists()) {
					// Load from the external file instead of the preset
					ofJson fileJson = ofLoadJson(fullPath);
					loadFromJson(fileJson);
					
					// Skip the rest of preset loading since we've loaded from file
					return;
				}
			}
		}
		
		// 2. Clear existing storage to start fresh (if not loading from file)
		storage.clear();
		
		// 3. Initialize all slots with empty vectors
		initializeAllSlots();
		
		// 4. Extract which slot was active when the preset was saved
		int currentSlotValue = 0;
		if (json.count("currentSlot") > 0) {
			currentSlotValue = json["currentSlot"].get<int>();
		}
		
		// 5. Load all slot data from the JSON
		for (auto it = json.begin(); it != json.end(); ++it) {
			// Look for keys that start with "slotData_"
			if (it.key().substr(0, 9) == "slotData_") {
				// Extract the slot number from the key
				int slotIndex = ofToInt(it.key().substr(9));
				auto &jsonValues = it.value();
				
				if (jsonValues.is_array()) {
					// Load the values for this slot
					vector<float> slotValues = jsonValues.get<vector<float>>();
					
					// Resize if needed
					if (slotValues.size() != size) {
						slotValues.resize(size, 0.0f);
					}
					
					// Ensure all values are quantized
					for (int i = 0; i < slotValues.size(); i++) {
						slotValues[i] = round(slotValues[i] * (q - 1)) / (q - 1);
					}
					
					// Store the loaded data in our storage map
					storage[slotIndex] = slotValues;
				}
			}
		}
		
		// 6. Set values to point to the current slot's data
		values = &storage[currentSlotValue];
		
		// 7. Now set the slot parameter to trigger the UI update
		// Set it to a different value first if it's already at the target value
		if (slot.get() == currentSlotValue && currentSlotValue > 0) {
			// Force a slot change to trigger the listener
			previousSlot = slot.get();
			slot = 0;
		} else if (slot.get() == currentSlotValue && currentSlotValue == 0) {
			// Force a slot change to trigger the listener
			previousSlot = slot.get();
			slot = 1;
		}
		
		// Now set to the actual target value
		slot = currentSlotValue;
		
		// 8. Force an output update
		updateOutputValues();
	}

private:
	ofEventListeners listeners;
	
	// Main parameters
	ofParameter<int> size;
	ofParameter<int> q;
	ofParameter<float> minVal;
	ofParameter<float> maxVal;
	ofParameter<int> shift;  // Added shift parameter
	ofParameter<vector<float>> output;
	
	// Slot management parameters
	ofParameter<int> slot;
	ofParameter<void> resetSlot;
	ofParameter<void> resetAll;
	ofParameter<void> setIncremental;  // Button to set incremental pattern
	ofEventListener resetListener;
	ofEventListener resetAllListener;
	ofEventListener setIncrementalListener;
	
	// File handling parameters
	ofParameter<string> filename;
	ofParameter<void> openFile;
	ofParameter<void> saveFile;
	ofEventListener filenameListener;
	ofEventListener openFileListener;
	ofEventListener saveFileListener;
	
	// Inspector parameters
	ofParameter<int> width;
	ofParameter<int> height;
	
	// Internal values and slot management
	vector<float>* values;  // Pointer to current slot's data
	static const int NUM_SLOTS = 16;
	std::map<int, vector<float>> storage;
	int previousSlot;  // Tracks previously selected slot
	
	// Custom widget
	customGuiRegion customWidget;
	
	void drawMultiSliderGrid() {
		// Get ImGui IO to check key states
		ImGuiIO& io = ImGui::GetIO();
		
		// Get cursor position and calculate widget dimensions
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		ImVec2 widgetSize(width, height);
		
		// Create invisible button for interaction area
		ImGui::InvisibleButton("##MultiSliderGrid", widgetSize);
		bool isActive = ImGui::IsItemActive();
		bool isHovered = ImGui::IsItemHovered();
		
		// Get the draw list for rendering
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		// Draw the background
		drawList->AddRectFilled(
			cursorPos,
			ImVec2(cursorPos.x + widgetSize.x, cursorPos.y + widgetSize.y),
			ImGui::GetColorU32(ImGuiCol_FrameBg)
		);
		
		// Draw the border
		drawList->AddRect(
			cursorPos,
			ImVec2(cursorPos.x + widgetSize.x, cursorPos.y + widgetSize.y),
			ImGui::GetColorU32(ImGuiCol_Border)
		);
		
		// Removed slot text display as requested
		
		// Calculate dimensions for drawing with spacing between sliders
		const float sliderSpacing = 2.0f; // Spacing between sliders
		float sliderWidth = (widgetSize.x - (sliderSpacing * (size - 1))) / size;
		
		// Draw the horizontal grid lines only at quantization levels
		for (int i = 0; i < q; i++) {
			// Draw only at the quantization levels
			float normalizedY = (float)i / (q - 1);
			float y = cursorPos.y + (1.0f - normalizedY) * widgetSize.y;
			
			ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
			// Make the bottom and top lines thicker
			float thickness = (i == 0 || i == q-1) ? 1.5f : 0.75f;
			
			drawList->AddLine(
				ImVec2(cursorPos.x, y),
				ImVec2(cursorPos.x + widgetSize.x, y),
				lineColor,
				thickness
			);
		}
		
		// Handle mouse interaction to update slider values
		if (isActive && ImGui::IsMouseDragging(0, 0)) {
			const ImVec2 mousePos = ImGui::GetIO().MousePos;
			
			// Find which slider the mouse is over (considering spacing)
			float totalSliderWidth = sliderWidth + sliderSpacing;
			int sliderIdx = int((mousePos.x - cursorPos.x) / totalSliderWidth);
			sliderIdx = ofClamp(sliderIdx, 0, size - 1);
			
			// Calculate the value based on mouse Y position
			float rawValue = 1.0f - ((mousePos.y - cursorPos.y) / widgetSize.y);
			rawValue = ofClamp(rawValue, 0.0f, 1.0f);
			
			// Always quantize the value to snap to grid
			float quantizedValue = round(rawValue * (q - 1)) / (q - 1);
			
			// Update the value with the quantized value
			(*values)[sliderIdx] = quantizedValue;
			
			// Update the output
			updateOutputValues();
		}
		
		// Apply shift for display purposes (non-destructive to the stored values)
		vector<float> displayValues(size);
		int currentShift = shift.get() % size;
		if (currentShift > 0) {
			for (int i = 0; i < size; i++) {
				int sourceIdx = (i + size - currentShift) % size;
				displayValues[i] = (*values)[sourceIdx];
			}
		} else {
			displayValues = *values;
		}
		
		// Draw each slider bar
		for (int i = 0; i < size && i < displayValues.size(); i++) {
			// Calculate the position of this slider
			float x = cursorPos.x + i * (sliderWidth + sliderSpacing);
			
			// Draw background for this slider column
			drawList->AddRectFilled(
				ImVec2(x, cursorPos.y),
				ImVec2(x + sliderWidth, cursorPos.y + widgetSize.y),
				ImGui::GetColorU32(ImGuiCol_FrameBg, 0.5f)
			);
			
			// Get the quantized value for this slider
			float quantizedValue = displayValues[i]; // Values are already quantized
			float barHeight = quantizedValue * widgetSize.y;
			
			// Draw the slider bar
			drawList->AddRectFilled(
				ImVec2(x, cursorPos.y + widgetSize.y - barHeight),
				ImVec2(x + sliderWidth, cursorPos.y + widgetSize.y),
				ImGui::GetColorU32(ImGuiCol_PlotHistogram)
			);
			
			// Draw a marker at the quantized value for better visibility
			float quantizedY = cursorPos.y + widgetSize.y - (quantizedValue * widgetSize.y);
			
			// Draw horizontal marker line at the current value
			drawList->AddLine(
				ImVec2(x, quantizedY),
				ImVec2(x + sliderWidth, quantizedY),
				ImGui::GetColorU32(ImGuiCol_Text),
				2.0f
			);
		}
		
		// Show tooltips when hovering over sliders
		if (isHovered) {
			const ImVec2 mousePos = ImGui::GetIO().MousePos;
			float totalSliderWidth = sliderWidth + sliderSpacing;
			int hoverSliderIdx = int((mousePos.x - cursorPos.x) / totalSliderWidth);
			
			if (hoverSliderIdx >= 0 && hoverSliderIdx < size && hoverSliderIdx < displayValues.size()) {
				// Get the quantized value (from display values which include shift)
				float quantizedValue = displayValues[hoverSliderIdx]; // Already quantized
				float outputValue = ofMap(quantizedValue, 0.0f, 1.0f, minVal, maxVal);
				
				// Calculate the actual step this represents
				int step = round(quantizedValue * (q - 1));
				
				// Show tooltip with values
				ImGui::BeginTooltip();
				ImGui::Text("Slider: %d", hoverSliderIdx);
				ImGui::Text("Step: %d of %d", step, q - 1);
				ImGui::Text("Value: %.3f", outputValue);
				ImGui::Text("Slot: %d", slot.get());
				ImGui::EndTooltip();
			}
		}
		
		// Slot keyboard shortcuts
		if (isHovered || isActive) {
			// Number keys 0-9 select slots 0-9
			for (int i = 0; i <= 9; i++) {
				if (io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_0) + i] && io.KeyCtrl) {
					if (io.KeyShift && i == 0) {
						// Ctrl+Shift+0 resets current slot
						resetCurrentSlot();
					} else if (i < NUM_SLOTS) {
						// Ctrl+Number selects slot
						slot = i;
					}
				}
			}
		}
	}
};

#endif /* multiSliderGrid_h */
