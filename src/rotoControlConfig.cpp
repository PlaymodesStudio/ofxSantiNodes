#include "rotoControlConfig.h"
#include "imgui.h"

// Serial command constants based on API docs
const unsigned char CMD_START_MARKER = 0x5A; // Command start marker
const unsigned char RESP_START_MARKER = 0xA5; // Response start marker

// Command types
const unsigned char CMD_GENERAL = 0x01;
const unsigned char CMD_MIDI = 0x02;
const unsigned char CMD_PLUGIN = 0x03;

// General commands
const unsigned char CMD_START_CONFIG_UPDATE = 0x04;
const unsigned char CMD_END_CONFIG_UPDATE = 0x05;

// MIDI commands
const unsigned char CMD_SET_KNOB_CONTROL_CONFIG = 0x07;
const unsigned char CMD_CLEAR_CONTROL_CONFIG = 0x09;

// Response codes
const unsigned char RESP_SUCCESS = 0x00;

// ROTO-CONTROL device path
const std::string ROTO_CONTROL_DEVICE_PREFIX = "cu.usbmodem"; // Look for any devices with this prefix

void rotoControlConfig::setup() {
	// Description for node
	description = "Configure ROTO-CONTROL display and control settings. Set names, colors, MIDI channels, CC numbers, and step counts for each control across multiple pages.";
	
	controlColor.set("Color", 0, 0, 82);
	
	// Initialize control configurations
	controlConfigs.resize(TOTAL_CONTROLS);
	for (int i = 0; i < TOTAL_CONTROLS; i++) {
		controlConfigs[i].name = "Control " + ofToString((i % NUM_PHYSICAL_CONTROLS) + 1);
		controlConfigs[i].color = 0;
		controlConfigs[i].midiChannel = 1;
		controlConfigs[i].midiCC = i % 128; // Default CC based on control number
		controlConfigs[i].steps = 0;
		controlConfigs[i].configured = false;
	}
	
	// Control and page selection
	addParameter(selectedControl.set("Control", 0, 0, NUM_PHYSICAL_CONTROLS - 1));
	addParameter(selectedPage.set("Page", 0, 0, NUM_PAGES - 1));
	
	// Parameters for the selected control
	addParameter(controlName.set("Name", "Control 1"));
	
	// Other parameters
	addParameter(controlMidiChannel.set("MIDI Ch", 1, 1, 16));
	addParameter(controlMidiCC.set("MIDI CC", 0, 0, 127));
	addParameter(controlSteps.set("Steps", 0, 0, 10));
	
	// Add custom region for preview
	addCustomRegion(previewRegion.set("Preview", [this](){
		drawControlPreview();
	}), [this](){
		drawControlPreview();
	});
	
	// Listen for control/page selection changes
	listeners.push(selectedControl.newListener([this](int &){
		updateSelectedControlParameters();
	}));
	
	listeners.push(selectedPage.newListener([this](int &){
		updateSelectedControlParameters();
	}));
	
	// Listen for control parameter changes
	listeners.push(controlName.newListener([this](string &){
		if (ignoreListeners) return;
		storeCurrentControlSettings();
		applyControlConfiguration(getAbsoluteControlIndex());
	}));
	
	listeners.push(controlColor.newListener([this](int &){
		if (ignoreListeners) return;
		storeCurrentControlSettings();
		applyControlConfiguration(getAbsoluteControlIndex());
	}));
	
	listeners.push(controlMidiChannel.newListener([this](int &){
		if (ignoreListeners) return;
		storeCurrentControlSettings();
		applyControlConfiguration(getAbsoluteControlIndex());
	}));
	
	listeners.push(controlMidiCC.newListener([this](int &){
		if (ignoreListeners) return;
		storeCurrentControlSettings();
		applyControlConfiguration(getAbsoluteControlIndex());
	}));
	
	listeners.push(controlSteps.newListener([this](int &){
		if (ignoreListeners) return;
		storeCurrentControlSettings();
		applyControlConfiguration(getAbsoluteControlIndex());
	}));
	
	// Initialize with first control's settings
	updateSelectedControlParameters();
	
	// Connect to the ROTO-CONTROL device
	setupSerialPort();
}

void rotoControlConfig::update(ofEventArgs &args) {
	// Read and handle any responses from the device
	if (serialConnected) {
		readSerialResponses();
	}
}

void rotoControlConfig::setupSerialPort() {
	// Get list of all available serial devices
	vector<ofSerialDeviceInfo> devices = serial.getDeviceList();
	bool connected = false;
	
	// Try to find and connect to any ROTO-CONTROL device
	for (auto& device : devices) {
		string devicePath = device.getDevicePath();
		ofLogNotice("rotoControlConfig") << "Found serial device: " << devicePath;
		
		// Check if the device path contains the ROTO-CONTROL prefix
		if (devicePath.find(ROTO_CONTROL_DEVICE_PREFIX) != string::npos) {
			ofLogNotice("rotoControlConfig") << "Attempting to connect to ROTO-CONTROL on port: " << devicePath;
			
			if (serial.setup(devicePath, 115200)) {
				ofLogNotice("rotoControlConfig") << "Connected to ROTO-CONTROL on port: " << devicePath;
				serialConnected = true;
				connected = true;
				
				// Give the device a moment to initialize
				ofSleepMillis(100);
				break;
			} else {
				ofLogError("rotoControlConfig") << "Failed to connect to ROTO-CONTROL on " << devicePath;
			}
		}
	}
	
	if (!connected) {
		ofLogError("rotoControlConfig") << "Could not find any ROTO-CONTROL device. Available devices:";
		for (auto& device : devices) {
			ofLogError("rotoControlConfig") << "  - " << device.getDevicePath();
		}
	}
}

void rotoControlConfig::closeSerialPort() {
	if (serialConnected) {
		serial.close();
		serialConnected = false;
		ofLogNotice("rotoControlConfig") << "Closed serial connection";
	}
}

void rotoControlConfig::readSerialResponses() {
	// Check if there's data to read
	if (serial.available() > 0) {
		// Buffer for reading
		unsigned char buffer[256];
		int numBytes = serial.readBytes(buffer, MIN(256, serial.available()));
		
		// Process the response only if we got enough bytes
		if (numBytes > 0) {
			// Look for start marker
			for (int i = 0; i < numBytes; i++) {
				if (buffer[i] == RESP_START_MARKER && i+1 < numBytes) {
					// Found a response header
					unsigned char responseCode = buffer[i+1];
					
					if (responseCode == RESP_SUCCESS) {
						ofLogNotice("rotoControlConfig") << "Received successful response from ROTO-CONTROL";
					} else {
						ofLogError("rotoControlConfig") << "Received error response from ROTO-CONTROL: " << (int)responseCode;
					}
					
					// Skip ahead to avoid reprocessing the same header
					i++;
				}
			}
		}
	}
}

int rotoControlConfig::getAbsoluteControlIndex() {
	return selectedPage * NUM_PHYSICAL_CONTROLS + selectedControl;
}

void rotoControlConfig::updateSelectedControlParameters() {
	int index = getAbsoluteControlIndex();
	if (index < 0 || index >= controlConfigs.size()) return;
	
	// Set flag to ignore listener callbacks
	ignoreListeners = true;
	
	// Update parameters with the selected control's config
	controlName = controlConfigs[index].name;
	controlColor = controlConfigs[index].color;
	controlMidiChannel = controlConfigs[index].midiChannel;
	controlMidiCC = controlConfigs[index].midiCC;
	controlSteps = controlConfigs[index].steps;
	
	// Re-enable listener callbacks
	ignoreListeners = false;
}

void rotoControlConfig::storeCurrentControlSettings() {
	int index = getAbsoluteControlIndex();
	if (index < 0 || index >= controlConfigs.size()) return;
	
	// Store current parameter values in the config
	controlConfigs[index].name = controlName;
	controlConfigs[index].color = controlColor;
	controlConfigs[index].midiChannel = controlMidiChannel;
	controlConfigs[index].midiCC = controlMidiCC;
	controlConfigs[index].steps = controlSteps;
	controlConfigs[index].configured = true;
}

void rotoControlConfig::sendSerialCommand(unsigned char commandType, unsigned char subType, vector<unsigned char> payload) {
	if (!serialConnected) {
		setupSerialPort();
		if (!serialConnected) return;
	}
	
	// Calculate payload length (2 bytes, MSB first)
	unsigned short dataLength = payload.size();
	unsigned char lengthMSB = (dataLength >> 8) & 0xFF;
	unsigned char lengthLSB = dataLength & 0xFF;
	
	// Construct the command
	vector<unsigned char> message;
	message.push_back(CMD_START_MARKER);
	message.push_back(commandType);
	message.push_back(subType);
	message.push_back(lengthMSB);
	message.push_back(lengthLSB);
	
	// Add payload bytes
	for (auto& byte : payload) {
		message.push_back(byte);
	}
	
	// Send the command
	serial.writeBytes(message.data(), message.size());
	
	// Wait a moment to ensure command is processed
	ofSleepMillis(20);
	
	ofLogNotice("rotoControlConfig") << "Sent command: Type=" << (int)commandType
		<< ", SubType=" << (int)subType
		<< ", Length=" << dataLength;
}

void rotoControlConfig::applyControlConfiguration(int controlIndex) {
	if (!serialConnected) {
		setupSerialPort();
	}
	
	if (!serialConnected) {
		ofLogError("rotoControlConfig") << "Cannot apply configuration: Serial device not connected";
		return;
	}
	
	if (controlIndex < 0 || controlIndex >= controlConfigs.size()) {
		ofLogError("rotoControlConfig") << "Invalid control index: " << controlIndex;
		return;
	}
	
	// Get the control configuration
	ControlConfig& config = controlConfigs[controlIndex];
	
	// Calculate the physical control (0-15) and page (0-7)
	int physicalControl = controlIndex % NUM_PHYSICAL_CONTROLS;
	int page = controlIndex / NUM_PHYSICAL_CONTROLS;
	
	// Start config update session
	sendSerialCommand(CMD_GENERAL, CMD_START_CONFIG_UPDATE);
	
	// Based on the image, it appears controls are linearly indexed in groups of 8 across pages
	// Page 0: Controls 0-7 are 1-8 in UI
	// Page 0: Controls 8-15 are 9-16 in UI
	// Page 1: Controls 0-7 are 17-24 in UI
	// Page 1: Controls 8-15 are 25-32 in UI
	// Etc.
	
	// Calculate the "absolute" control index for the API
	// This mapping assumes linear ordering based on the UI tabs (1-8, 9-16, 17-24, etc.)
	int absoluteControlIndex = page * NUM_PHYSICAL_CONTROLS + physicalControl;
	
	// Configure control using the SET KNOB CONTROL CONFIG command
	vector<unsigned char> configPayload;
	
	// First parameter is the setup index - always 0 for current setup
	configPayload.push_back(0);
	
	// Second parameter is the absolute control index
	configPayload.push_back(absoluteControlIndex);
	
	// Control mode (0 = CC 7-bit)
	configPayload.push_back(0);
	
	// MIDI channel (1-16)
	configPayload.push_back(config.midiChannel);
	
	// Control parameter (CC number)
	configPayload.push_back(config.midiCC);
	
	// NRPN address (2 bytes - not used for CC mode)
	configPayload.push_back(0);
	configPayload.push_back(0);
	
	// Min value (2 bytes)
	configPayload.push_back(0); // MSB
	configPayload.push_back(0); // LSB
	
	// Max value (2 bytes)
	configPayload.push_back(0); // MSB
	configPayload.push_back(127); // LSB
	
	// Control name (13 bytes, NULL terminated)
	string displayName = config.name;
	
	for (int j = 0; j < 13; j++) {
		if (j < displayName.length()) {
			configPayload.push_back(displayName[j]);
		} else {
			configPayload.push_back(0); // NULL pad the rest
		}
	}
	
	// Color scheme
	configPayload.push_back(config.color);
	
	// Haptic mode (0 = KNOB_300 for continuous, 1 = KNOB_N_STEP for stepped)
	bool useSteppedMode = config.steps >= 2;
	configPayload.push_back(useSteppedMode ? 1 : 0);
	
	// Indent positions (only for KNOB_300) - not using indents for now
	configPayload.push_back(0xFF); // Unused
	configPayload.push_back(0xFF); // Unused
	
	// Haptic steps (2-10, only for KNOB_N_STEP)
	int hapticSteps = useSteppedMode ? config.steps : 0;
	configPayload.push_back(hapticSteps);
	
	// If we're using stepped mode, add step labels
	if (useSteppedMode && hapticSteps >= 2) {
		for (int i = 0; i < hapticSteps; i++) {
			string stepLabel = config.name + " " + ofToString(i + 1);
			
			for (int j = 0; j < 13; j++) {
				if (j < stepLabel.length()) {
					configPayload.push_back(stepLabel[j]);
				} else {
					configPayload.push_back(0); // NULL pad the rest
				}
			}
		}
	}
	
	// Log the indexing for debugging
	ofLogNotice("rotoControlConfig") << "Control mapping: GUI Index " << controlIndex
									<< " (P" << page << " C" << physicalControl << ")"
									<< " -> Absolute Index " << absoluteControlIndex;
	
	// Send the configuration command
	sendSerialCommand(CMD_MIDI, CMD_SET_KNOB_CONTROL_CONFIG, configPayload);
	
	// End config update session
	sendSerialCommand(CMD_GENERAL, CMD_END_CONFIG_UPDATE);
}

void rotoControlConfig::drawControlPreview() {
	int index = getAbsoluteControlIndex();
	if (index < 0 || index >= controlConfigs.size()) return;
	
	ControlConfig& config = controlConfigs[index];
	
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
	
	// Fixed palette colors exactly matching the ROTO-CONTROL official app
	const ImVec4 palette[] = {
		// Row 1 (14 colors)
		ImVec4(0.95f, 0.70f, 0.75f, 1.0f), // 0: Pink
		ImVec4(0.97f, 0.72f, 0.35f, 1.0f), // 1: Light Orange
		ImVec4(0.85f, 0.65f, 0.30f, 1.0f), // 2: Gold
		ImVec4(0.97f, 0.97f, 0.50f, 1.0f), // 3: Light Yellow
		ImVec4(0.85f, 0.97f, 0.40f, 1.0f), // 4: Light Lime
		ImVec4(0.60f, 0.95f, 0.40f, 1.0f), // 5: Light Green
		ImVec4(0.40f, 0.95f, 0.65f, 1.0f), // 6: Light Mint
		ImVec4(0.40f, 0.95f, 0.95f, 1.0f), // 7: Light Cyan
		ImVec4(0.60f, 0.80f, 0.95f, 1.0f), // 8: Light Blue
		ImVec4(0.50f, 0.60f, 0.95f, 1.0f), // 9: Medium Blue
		ImVec4(0.60f, 0.50f, 0.95f, 1.0f), // 10: Light Purple
		ImVec4(0.80f, 0.50f, 0.95f, 1.0f), // 11: Light Magenta
		ImVec4(0.95f, 0.50f, 0.75f, 1.0f), // 12: Light Pink
		ImVec4(1.00f, 1.00f, 1.00f, 1.0f), // 13: White
		
		// Row 2 (14 colors)
		ImVec4(0.95f, 0.30f, 0.30f, 1.0f), // 14: Red
		ImVec4(0.95f, 0.50f, 0.20f, 1.0f), // 15: Orange
		ImVec4(0.65f, 0.40f, 0.20f, 1.0f), // 16: Brown
		ImVec4(0.95f, 0.95f, 0.20f, 1.0f), // 17: Yellow
		ImVec4(0.65f, 0.95f, 0.20f, 1.0f), // 18: Lime
		ImVec4(0.30f, 0.80f, 0.20f, 1.0f), // 19: Green
		ImVec4(0.20f, 0.80f, 0.60f, 1.0f), // 20: Teal
		ImVec4(0.20f, 0.85f, 0.95f, 1.0f), // 21: Cyan
		ImVec4(0.30f, 0.60f, 0.95f, 1.0f), // 22: Blue
		ImVec4(0.20f, 0.40f, 0.80f, 1.0f), // 23: Medium Blue
		ImVec4(0.40f, 0.30f, 0.80f, 1.0f), // 24: Purple
		ImVec4(0.65f, 0.30f, 0.80f, 1.0f), // 25: Magenta
		ImVec4(0.95f, 0.20f, 0.60f, 1.0f), // 26: Pink
		ImVec4(0.80f, 0.80f, 0.80f, 1.0f), // 27: Light Gray
		
		// Row 3 (14 colors)
		ImVec4(0.80f, 0.50f, 0.40f, 1.0f), // 28: Salmon
		ImVec4(0.90f, 0.65f, 0.50f, 1.0f), // 29: Peach
		ImVec4(0.75f, 0.65f, 0.50f, 1.0f), // 30: Tan
		ImVec4(0.90f, 0.95f, 0.70f, 1.0f), // 31: Pale Yellow
		ImVec4(0.75f, 0.85f, 0.60f, 1.0f), // 32: Pale Green
		ImVec4(0.65f, 0.80f, 0.40f, 1.0f), // 33: Olive Green
		ImVec4(0.60f, 0.75f, 0.60f, 1.0f), // 34: Sage
		ImVec4(0.85f, 0.95f, 0.90f, 1.0f), // 35: Pale Cyan
		ImVec4(0.80f, 0.90f, 0.95f, 1.0f), // 36: Pale Blue
		ImVec4(0.70f, 0.75f, 0.90f, 1.0f), // 37: Lavender
		ImVec4(0.80f, 0.70f, 0.90f, 1.0f), // 38: Pale Purple
		ImVec4(0.85f, 0.70f, 0.95f, 1.0f), // 39: Pale Magenta
		ImVec4(0.95f, 0.85f, 0.90f, 1.0f), // 40: Pale Pink
		ImVec4(0.60f, 0.60f, 0.60f, 1.0f), // 41: Medium Gray
		
		// Row 4 (14 colors)
		ImVec4(0.70f, 0.50f, 0.50f, 1.0f), // 42: Dusty Rose
		ImVec4(0.65f, 0.45f, 0.30f, 1.0f), // 43: Copper
		ImVec4(0.60f, 0.55f, 0.45f, 1.0f), // 44: Taupe
		ImVec4(0.65f, 0.65f, 0.35f, 1.0f), // 45: Olive
		ImVec4(0.55f, 0.65f, 0.25f, 1.0f), // 46: Moss Green
		ImVec4(0.45f, 0.65f, 0.45f, 1.0f), // 47: Forest Green
		ImVec4(0.40f, 0.65f, 0.60f, 1.0f), // 48: Sea Green
		ImVec4(0.50f, 0.65f, 0.70f, 1.0f), // 49: Steel Blue
		ImVec4(0.50f, 0.55f, 0.70f, 1.0f), // 50: Slate Blue
		ImVec4(0.45f, 0.45f, 0.70f, 1.0f), // 51: Navy Blue
		ImVec4(0.60f, 0.45f, 0.70f, 1.0f), // 52: Plum
		ImVec4(0.70f, 0.50f, 0.65f, 1.0f), // 53: Mauve
		ImVec4(0.70f, 0.40f, 0.50f, 1.0f), // 54: Raspberry
		ImVec4(0.40f, 0.40f, 0.40f, 1.0f), // 55: Dark Gray
		
		// Row 5 (14 colors)
		ImVec4(0.70f, 0.20f, 0.20f, 1.0f), // 56: Dark Red
		ImVec4(0.70f, 0.35f, 0.15f, 1.0f), // 57: Burnt Orange
		ImVec4(0.50f, 0.30f, 0.15f, 1.0f), // 58: Dark Brown
		ImVec4(0.70f, 0.70f, 0.15f, 1.0f), // 59: Dark Yellow
		ImVec4(0.50f, 0.65f, 0.15f, 1.0f), // 60: Dark Lime
		ImVec4(0.20f, 0.60f, 0.15f, 1.0f), // 61: Dark Green
		ImVec4(0.15f, 0.60f, 0.45f, 1.0f), // 62: Dark Teal
		ImVec4(0.20f, 0.50f, 0.60f, 1.0f), // 63: Dark Cyan
		ImVec4(0.10f, 0.30f, 0.60f, 1.0f), // 64: Dark Blue
		ImVec4(0.15f, 0.25f, 0.50f, 1.0f), // 65: Navy
		ImVec4(0.30f, 0.20f, 0.60f, 1.0f), // 66: Dark Purple
		ImVec4(0.50f, 0.20f, 0.60f, 1.0f), // 67: Dark Magenta
		ImVec4(0.70f, 0.15f, 0.45f, 1.0f), // 68: Dark Pink
		ImVec4(0.20f, 0.20f, 0.20f, 1.0f), // 69: Very Dark Gray
		
		// Row 6 (13 colors) - High Contrast Colors
		ImVec4(0.00f, 0.00f, 0.00f, 1.0f), // 70: Black
		ImVec4(1.00f, 0.00f, 0.00f, 1.0f), // 71: Pure Red
		ImVec4(0.00f, 1.00f, 0.00f, 1.0f), // 72: Pure Green
		ImVec4(1.00f, 1.00f, 0.00f, 1.0f), // 73: Pure Yellow
		ImVec4(0.00f, 0.00f, 1.00f, 1.0f), // 74: Pure Blue
		ImVec4(1.00f, 0.00f, 1.00f, 1.0f), // 75: Pure Magenta
		ImVec4(0.00f, 1.00f, 1.00f, 1.0f), // 76: Pure Cyan
		ImVec4(0.50f, 0.00f, 0.00f, 1.0f), // 77: Dark Red
		ImVec4(0.50f, 0.50f, 0.00f, 1.0f), // 78: Olive
		ImVec4(0.00f, 0.50f, 0.00f, 1.0f), // 79: Dark Green
		ImVec4(0.00f, 0.50f, 0.50f, 1.0f), // 80: Dark Teal
		ImVec4(0.00f, 0.00f, 0.50f, 1.0f), // 81: Dark Blue
		ImVec4(0.50f, 0.00f, 0.50f, 1.0f), // 82: Dark Purple
	};
	
	const int TOTAL_COLORS = sizeof(palette) / sizeof(palette[0]);
	
	// Color names for dropdown
	const char* colorNames[] = {
		// Row 1
		"0: Light Pink", "1: Light Orange", "2: Gold", "3: Light Yellow",
		"4: Light Lime", "5: Light Green", "6: Light Mint", "7: Light Cyan",
		"8: Light Blue", "9: Medium Blue", "10: Light Purple", "11: Light Magenta",
		"12: Light Pink", "13: White",
		
		// Row 2
		"14: Red", "15: Orange", "16: Brown", "17: Yellow",
		"18: Lime", "19: Green", "20: Teal", "21: Cyan",
		"22: Blue", "23: Medium Blue", "24: Purple", "25: Magenta",
		"26: Pink", "27: Light Gray",
		
		// Row 3
		"28: Salmon", "29: Peach", "30: Tan", "31: Pale Yellow",
		"32: Pale Green", "33: Olive Green", "34: Sage", "35: Pale Cyan",
		"36: Pale Blue", "37: Lavender", "38: Pale Purple", "39: Pale Magenta",
		"40: Pale Pink", "41: Medium Gray",
		
		// Row 4
		"42: Dusty Rose", "43: Copper", "44: Taupe", "45: Olive",
		"46: Moss Green", "47: Forest Green", "48: Sea Green", "49: Steel Blue",
		"50: Slate Blue", "51: Navy Blue", "52: Plum", "53: Mauve",
		"54: Raspberry", "55: Dark Gray",
		
		// Row 5
		"56: Dark Red", "57: Burnt Orange", "58: Dark Brown", "59: Dark Yellow",
		"60: Dark Lime", "61: Dark Green", "62: Dark Teal", "63: Dark Cyan",
		"64: Dark Blue", "65: Navy", "66: Dark Purple", "67: Dark Magenta",
		"68: Dark Pink", "69: Very Dark Gray",
		
		// Row 6
		"70: Black", "71: Pure Red", "72: Pure Green", "73: Pure Yellow",
		"74: Pure Blue", "75: Pure Magenta", "76: Pure Cyan", "77: Dark Red",
		"78: Olive", "79: Dark Green", "80: Dark Teal", "81: Dark Blue",
		"82: Dark Purple"
	};
	
	// Get current color index (with bounds checking)
	int colorIndex = config.color;
	if (colorIndex < 0) colorIndex = 0;
	if (colorIndex >= TOTAL_COLORS) colorIndex = TOTAL_COLORS - 1;
	
	// Color selector dropdown
	const char* currentColorName = colorNames[colorIndex];
	
	ImGui::Text("Color: ");
	ImGui::SameLine();
	
	// Display color preview square
	ImGui::ColorButton("##colorPreview", palette[colorIndex], 0, ImVec2(20, 20));
	ImGui::SameLine();
	
	// Create a dropdown using ImGui::Combo
	ImGui::PushItemWidth(150); // Set dropdown width
	if (ImGui::BeginCombo("##colorDropdown", currentColorName)) {
		for (int i = 0; i < TOTAL_COLORS; i++) {
			bool isSelected = (i == colorIndex);
			// Draw a color swatch and name
			ImGui::PushID(i);
			ImGui::ColorButton("##colorSwatch", palette[i], 0, ImVec2(15, 15));
			ImGui::SameLine();
			
			if (ImGui::Selectable(colorNames[i], isSelected)) {
				controlColor = i;
				storeCurrentControlSettings();
				applyControlConfiguration(getAbsoluteControlIndex());
			}
			
			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();
	
	ImGui::Spacing();
	
	// Preview label as it would appear on device
	string displayName = config.name;
	
	// Draw control preview with background color
	ImGui::PushStyleColor(ImGuiCol_Button, palette[colorIndex]);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));

	// Create button with label
	ImVec2 buttonSize(240, 30);
	ImGui::Button("##invisibleButton", buttonSize); // Invisible button for background
	   
	// Get button position for text overlay
	ImVec2 buttonPos = ImGui::GetItemRectMin();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	   
	ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
	ImVec2 textPos(
		buttonPos.x + (buttonSize.x - textSize.x) * 0.5f,
		buttonPos.y + (buttonSize.y - textSize.y) * 0.5f
	);
	   
	// Draw the center text
	drawList->AddText(textPos, IM_COL32(0, 0, 0, 255), displayName.c_str());
	   
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2); // Pop both color styles (button and text)
	
	// Display information about the step configuration
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	
	ImGui::PopStyleColor();
	
	ImGui::PopStyleVar();
}

void rotoControlConfig::initializeColorMap() {
	// Initialize the 52 colors with names and RGB color values
	rotoColors.clear();
	
	// Basic colors (black, white, grays)
	rotoColors.push_back({"Black", 0.0f, 0.0f, 0.0f});
	rotoColors.push_back({"White", 1.0f, 1.0f, 1.0f});
	rotoColors.push_back({"Gray 25%", 0.25f, 0.25f, 0.25f});
	rotoColors.push_back({"Gray 50%", 0.5f, 0.5f, 0.5f});
	rotoColors.push_back({"Gray 75%", 0.75f, 0.75f, 0.75f});
	
	// Reds
	rotoColors.push_back({"Red 100%", 1.0f, 0.0f, 0.0f});
	rotoColors.push_back({"Red 75%", 0.75f, 0.0f, 0.0f});
	rotoColors.push_back({"Red 50%", 0.5f, 0.0f, 0.0f});
	rotoColors.push_back({"Red 25%", 0.25f, 0.0f, 0.0f});
	
	// Greens
	rotoColors.push_back({"Green 100%", 0.0f, 1.0f, 0.0f});
	rotoColors.push_back({"Green 75%", 0.0f, 0.75f, 0.0f});
	rotoColors.push_back({"Green 50%", 0.0f, 0.5f, 0.0f});
	rotoColors.push_back({"Green 25%", 0.0f, 0.25f, 0.0f});
	
	// Blues
	rotoColors.push_back({"Blue 100%", 0.0f, 0.0f, 1.0f});
	rotoColors.push_back({"Blue 75%", 0.0f, 0.0f, 0.75f});
	rotoColors.push_back({"Blue 50%", 0.0f, 0.0f, 0.5f});
	rotoColors.push_back({"Blue 25%", 0.0f, 0.0f, 0.25f});
	
	// Yellows
	rotoColors.push_back({"Yellow 100%", 1.0f, 1.0f, 0.0f});
	rotoColors.push_back({"Yellow 75%", 0.75f, 0.75f, 0.0f});
	rotoColors.push_back({"Yellow 50%", 0.5f, 0.5f, 0.0f});
	rotoColors.push_back({"Yellow 25%", 0.25f, 0.25f, 0.0f});
	
	// Cyans
	rotoColors.push_back({"Cyan 100%", 0.0f, 1.0f, 1.0f});
	rotoColors.push_back({"Cyan 75%", 0.0f, 0.75f, 0.75f});
	rotoColors.push_back({"Cyan 50%", 0.0f, 0.5f, 0.5f});
	rotoColors.push_back({"Cyan 25%", 0.0f, 0.25f, 0.25f});
	
	// Magentas
	rotoColors.push_back({"Magenta 100%", 1.0f, 0.0f, 1.0f});
	rotoColors.push_back({"Magenta 75%", 0.75f, 0.0f, 0.75f});
	rotoColors.push_back({"Magenta 50%", 0.5f, 0.0f, 0.5f});
	rotoColors.push_back({"Magenta 25%", 0.25f, 0.0f, 0.25f});
	
	// Oranges
	rotoColors.push_back({"Orange 100%", 1.0f, 0.5f, 0.0f});
	rotoColors.push_back({"Orange 75%", 0.75f, 0.375f, 0.0f});
	rotoColors.push_back({"Orange 50%", 0.5f, 0.25f, 0.0f});
	rotoColors.push_back({"Orange 25%", 0.25f, 0.125f, 0.0f});
	
	// Purples
	rotoColors.push_back({"Purple 100%", 0.5f, 0.0f, 1.0f});
	rotoColors.push_back({"Purple 75%", 0.375f, 0.0f, 0.75f});
	rotoColors.push_back({"Purple 50%", 0.25f, 0.0f, 0.5f});
	rotoColors.push_back({"Purple 25%", 0.125f, 0.0f, 0.25f});
	
	// Lime greens
	rotoColors.push_back({"Lime 100%", 0.5f, 1.0f, 0.0f});
	rotoColors.push_back({"Lime 75%", 0.375f, 0.75f, 0.0f});
	rotoColors.push_back({"Lime 50%", 0.25f, 0.5f, 0.0f});
	rotoColors.push_back({"Lime 25%", 0.125f, 0.25f, 0.0f});
	
	// Teals
	rotoColors.push_back({"Teal 100%", 0.0f, 1.0f, 0.5f});
	rotoColors.push_back({"Teal 75%", 0.0f, 0.75f, 0.375f});
	rotoColors.push_back({"Teal 50%", 0.0f, 0.5f, 0.25f});
	rotoColors.push_back({"Teal 25%", 0.0f, 0.25f, 0.125f});
	
	// Pink/rose
	rotoColors.push_back({"Pink 100%", 1.0f, 0.0f, 0.5f});
	rotoColors.push_back({"Pink 75%", 0.75f, 0.0f, 0.375f});
	rotoColors.push_back({"Pink 50%", 0.5f, 0.0f, 0.25f});
	rotoColors.push_back({"Pink 25%", 0.25f, 0.0f, 0.125f});
}

void rotoControlConfig::applyConfiguration() {
	// Start config update session
	sendSerialCommand(CMD_GENERAL, CMD_START_CONFIG_UPDATE);
	
	// Apply configuration for all controls
	for (int i = 0; i < TOTAL_CONTROLS; i++) {
		if (controlConfigs[i].configured) {
			// Use a shortened version of applyControlConfiguration without opening/closing the session
			ControlConfig& config = controlConfigs[i];
			int physicalControl = i % NUM_PHYSICAL_CONTROLS;
			int page = i / NUM_PHYSICAL_CONTROLS;
			
			// Configure control using MIDI SET KNOB CONTROL CONFIG command
			vector<unsigned char> configPayload;
			
			// Setup index (0-63) - we'll always use setup 0
			configPayload.push_back(0);
			
			// Control index (0-31) - this is the physical control
			configPayload.push_back(physicalControl);
			
			// Control mode (0 = CC 7-bit)
			configPayload.push_back(0);
			
			// MIDI channel (1-16)
			configPayload.push_back(config.midiChannel);
			
			// Control parameter (CC number)
			configPayload.push_back(config.midiCC);
			
			// NRPN address (2 bytes - not used for CC mode)
			configPayload.push_back(0);
			configPayload.push_back(0);
			
			// Min value (2 bytes)
			configPayload.push_back(0); // MSB
			configPayload.push_back(0); // LSB
			
			// Max value (2 bytes)
			configPayload.push_back(0); // MSB
			configPayload.push_back(127); // LSB
			
			// Control name (13 bytes, NULL terminated)
			string displayName = config.name;
			
			for (int j = 0; j < 13; j++) {
				if (j < displayName.length()) {
					configPayload.push_back(displayName[j]);
				} else {
					configPayload.push_back(0); // NULL pad the rest
				}
			}
			
			// Color scheme
			configPayload.push_back(config.color);
			
			// Haptic mode (0 = KNOB_300 for continuous, 1 = KNOB_N_STEP for stepped)
			bool useSteppedMode = config.steps >= 2;
			configPayload.push_back(useSteppedMode ? 1 : 0);
			
			// Indent positions (only for KNOB_300) - not using indents for now
			configPayload.push_back(0xFF); // Unused
			configPayload.push_back(0xFF); // Unused
			
			// Haptic steps (2-10, only for KNOB_N_STEP)
			int hapticSteps = useSteppedMode ? config.steps : 0;
			configPayload.push_back(hapticSteps);
			
			// If we're using stepped mode, add step labels
			if (useSteppedMode && hapticSteps >= 2) {
				for (int i = 0; i < hapticSteps; i++) {
					string stepLabel = config.name + " " + ofToString(i + 1);
					
					for (int j = 0; j < 13; j++) {
						if (j < stepLabel.length()) {
							configPayload.push_back(stepLabel[j]);
						} else {
							configPayload.push_back(0); // NULL pad the rest
						}
					}
				}
			}
			
			// Send the configuration command
			sendSerialCommand(CMD_MIDI, CMD_SET_KNOB_CONTROL_CONFIG, configPayload);
			
			// Small delay between commands to avoid overwhelming the device
			ofSleepMillis(30);
		}
	}
	
	// End config update session
	sendSerialCommand(CMD_GENERAL, CMD_END_CONFIG_UPDATE);
	
	ofLogNotice("rotoControlConfig") << "All configurations applied to ROTO-CONTROL device";
}

void rotoControlConfig::presetRecallBeforeSettingParameters(ofJson &json) {
	// Clear any existing configuration
	controlConfigs.clear();
	controlConfigs.resize(TOTAL_CONTROLS);
	
	// Initialize with default values
	for (int i = 0; i < TOTAL_CONTROLS; i++) {
		controlConfigs[i].name = "Control " + ofToString((i % NUM_PHYSICAL_CONTROLS) + 1);
		controlConfigs[i].color = 0;
		controlConfigs[i].midiChannel = 1;
		controlConfigs[i].midiCC = i % 128;
		controlConfigs[i].steps = 0;
		controlConfigs[i].configured = false;
	}
}

void rotoControlConfig::presetRecallAfterSettingParameters(ofJson &json) {
	// Load controls from preset if available
	if (json.contains("controlConfigs")) {
		ofJson configJson = json["controlConfigs"];
		
		for (int i = 0; i < TOTAL_CONTROLS && i < configJson.size(); i++) {
			ofJson controlJson = configJson[i];
			
			if (controlJson.contains("name")) {
				controlConfigs[i].name = controlJson["name"];
			}
			
			if (controlJson.contains("color")) {
				controlConfigs[i].color = controlJson["color"];
			}
			
			if (controlJson.contains("midiChannel")) {
				controlConfigs[i].midiChannel = controlJson["midiChannel"];
			}
			
			if (controlJson.contains("midiCC")) {
				controlConfigs[i].midiCC = controlJson["midiCC"];
			}
			
			if (controlJson.contains("steps")) {
				controlConfigs[i].steps = controlJson["steps"];
			}
			
			if (controlJson.contains("configured")) {
				controlConfigs[i].configured = controlJson["configured"];
			}
		}
	}
	
	// Update the UI with the current control's settings
	updateSelectedControlParameters();
	
	// Apply all configurations to the device
	if (serialConnected) {
		applyConfiguration();
	} else {
		setupSerialPort();
		if (serialConnected) {
			applyConfiguration();
		}
	}
}

void rotoControlConfig::presetSave(ofJson &json) {
	// Save all control configurations
	ofJson configJson = ofJson::array();
	
	for (int i = 0; i < controlConfigs.size(); i++) {
		ofJson controlJson;
		controlJson["name"] = controlConfigs[i].name;
		controlJson["color"] = controlConfigs[i].color;
		controlJson["midiChannel"] = controlConfigs[i].midiChannel;
		controlJson["midiCC"] = controlConfigs[i].midiCC;
		controlJson["steps"] = controlConfigs[i].steps;
		controlJson["configured"] = controlConfigs[i].configured;
		
		configJson.push_back(controlJson);
	}
	
	json["controlConfigs"] = configJson;
}

