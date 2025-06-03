#include "rotoControlConfig.h"
#include "imgui.h"

// Serial command constants based on API docs
const unsigned char CMD_START_MARKER = 0x5A;
const unsigned char RESP_START_MARKER = 0xA5;

// Command types
const unsigned char CMD_GENERAL = 0x01;
const unsigned char CMD_MIDI = 0x02;

// General commands
const unsigned char CMD_START_CONFIG_UPDATE = 0x04;
const unsigned char CMD_END_CONFIG_UPDATE = 0x05;
const unsigned char CMD_SET_MODE = 0x03;

// MIDI commands
const unsigned char CMD_SET_KNOB_CONTROL_CONFIG = 0x07;
const unsigned char CMD_SET_SWITCH_CONTROL_CONFIG = 0x08;

// Response codes
const unsigned char RESP_SUCCESS = 0x00;

// ROTO-CONTROL device path
const std::string ROTO_CONTROL_DEVICE_PREFIX = "cu.usbmodem";

static void drawThickSeparator() {
	// Get the current cursor position in screen coordinates
	ImVec2 p = ImGui::GetCursorScreenPos();

	// Draw a 2px-thick horizontal line exactly 240px long
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(p.x,     p.y),
		ImVec2(p.x + 240, p.y),
		IM_COL32(200, 200, 200, 255),
		2.0f
	);

	// Add a little vertical spacing so subsequent widgets aren’t jammed against the line
	ImGui::Dummy(ImVec2(0, 4));
}



void rotoControlConfig::setup() {
	description = "Configure ROTO-CONTROL knobs, switches, and setups. Set names, colors, MIDI channels, CC numbers, and step counts for each control across multiple pages. Manage device setups for different configurations.";
	
	// Initialize knob and switch configurations
	knobConfigs.resize(TOTAL_KNOBS);
	switchConfigs.resize(TOTAL_SWITCHES);
	
	for (int i = 0; i < TOTAL_KNOBS; i++) {
		knobConfigs[i].name = "Knob " + ofToString((i % NUM_KNOBS_PER_PAGE) + 1);
		knobConfigs[i].color = 0;
		knobConfigs[i].midiChannel = 1;
		knobConfigs[i].midiCC = i % 128;
		knobConfigs[i].steps = 0;
		knobConfigs[i].configured = false;
	}
	
	for (int i = 0; i < TOTAL_SWITCHES; i++) {
		switchConfigs[i].name = "Switch " + ofToString((i % NUM_SWITCHES_PER_PAGE) + 1);
		switchConfigs[i].color = 0;
		switchConfigs[i].midiChannel = 1;
		switchConfigs[i].midiCC = (64 + i) % 128; // Start switches at CC 64
		switchConfigs[i].configured = false;
	}
	
	// Initialize setup management
	availableSetups.resize(64); // Setups 0-63 (0 is current)
	for (int i = 0; i < 64; i++) {
		availableSetups[i].index = i;
		availableSetups[i].name = (i == 0) ? "Current Setup" : "Setup " + ofToString(i);
		availableSetups[i].exists = false;
	}
	
	// Setup management parameters
	addParameter(selectedSetupIndex.set("Setup Slot", 0, 0, 63));
	addParameter(setupName.set("Setup Name", "Current Setup"));

	// Thicker separator after Setup section
	addCustomRegion(
		ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); }),
		ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); })
	);
	
	// Page selection
	addParameter(selectedPage.set("Page", 0, 0, NUM_PAGES - 1));
	
	// Thicker separator after Page section
	addCustomRegion(
		ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); }),
		ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); })
	);

	// Knob parameters
	addParameter(selectedKnob.set("Knob", 0, 0, NUM_KNOBS_PER_PAGE - 1));
	addParameter(knobName.set("Name", "Knob 1"));
	addParameter(knobMidiChannel.set("MIDI Ch", 1, 1, 16));
	addParameter(knobMidiCC.set("MIDI CC", 0, 0, 127));
	addParameter(knobSteps.set("Steps", 0, 0, 10));

	// Knob color dropdown at end of knob block
	addCustomRegion(
		knobColorRegion.set("Knob Colors", [this](){
			// Draw "Knob Color" label + dropdown
			const ImVec4 palette[] = {
				ImVec4(0.95f, 0.70f, 0.75f, 1.0f), ImVec4(0.97f, 0.72f, 0.35f, 1.0f),
				ImVec4(0.85f, 0.65f, 0.30f, 1.0f), ImVec4(0.97f, 0.97f, 0.50f, 1.0f),
				ImVec4(0.85f, 0.97f, 0.40f, 1.0f), ImVec4(0.60f, 0.95f, 0.40f, 1.0f),
				ImVec4(0.40f, 0.95f, 0.65f, 1.0f), ImVec4(0.40f, 0.95f, 0.95f, 1.0f),
				ImVec4(0.60f, 0.80f, 0.95f, 1.0f), ImVec4(0.50f, 0.60f, 0.95f, 1.0f),
				ImVec4(0.60f, 0.50f, 0.95f, 1.0f), ImVec4(0.80f, 0.50f, 0.95f, 1.0f),
				ImVec4(0.95f, 0.50f, 0.75f, 1.0f), ImVec4(1.00f, 1.00f, 1.00f, 1.0f),
				ImVec4(0.95f, 0.30f, 0.30f, 1.0f), ImVec4(0.95f, 0.50f, 0.20f, 1.0f),
				ImVec4(0.65f, 0.40f, 0.20f, 1.0f), ImVec4(0.95f, 0.95f, 0.20f, 1.0f),
				ImVec4(0.65f, 0.95f, 0.20f, 1.0f), ImVec4(0.30f, 0.80f, 0.20f, 1.0f),
				ImVec4(0.20f, 0.80f, 0.60f, 1.0f), ImVec4(0.20f, 0.85f, 0.95f, 1.0f),
				ImVec4(0.30f, 0.60f, 0.95f, 1.0f), ImVec4(0.20f, 0.40f, 0.80f, 1.0f),
				ImVec4(0.40f, 0.30f, 0.80f, 1.0f), ImVec4(0.65f, 0.30f, 0.80f, 1.0f),
				ImVec4(0.95f, 0.20f, 0.60f, 1.0f), ImVec4(0.80f, 0.80f, 0.80f, 1.0f),
				ImVec4(0.80f, 0.50f, 0.40f, 1.0f), ImVec4(0.90f, 0.65f, 0.50f, 1.0f),
				ImVec4(0.75f, 0.65f, 0.50f, 1.0f), ImVec4(0.90f, 0.95f, 0.70f, 1.0f),
				ImVec4(0.75f, 0.85f, 0.60f, 1.0f), ImVec4(0.65f, 0.80f, 0.40f, 1.0f),
				ImVec4(0.60f, 0.75f, 0.60f, 1.0f), ImVec4(0.85f, 0.95f, 0.90f, 1.0f),
				ImVec4(0.80f, 0.90f, 0.95f, 1.0f), ImVec4(0.70f, 0.75f, 0.90f, 1.0f),
				ImVec4(0.80f, 0.70f, 0.90f, 1.0f), ImVec4(0.85f, 0.70f, 0.95f, 1.0f),
				ImVec4(0.95f, 0.85f, 0.90f, 1.0f), ImVec4(0.60f, 0.60f, 0.60f, 1.0f),
				ImVec4(0.70f, 0.50f, 0.50f, 1.0f), ImVec4(0.65f, 0.45f, 0.30f, 1.0f),
				ImVec4(0.60f, 0.55f, 0.45f, 1.0f), ImVec4(0.65f, 0.65f, 0.35f, 1.0f),
				ImVec4(0.55f, 0.65f, 0.25f, 1.0f), ImVec4(0.45f, 0.65f, 0.45f, 1.0f),
				ImVec4(0.40f, 0.65f, 0.60f, 1.0f), ImVec4(0.50f, 0.65f, 0.70f, 1.0f),
				ImVec4(0.50f, 0.55f, 0.70f, 1.0f), ImVec4(0.45f, 0.45f, 0.70f, 1.0f),
				ImVec4(0.60f, 0.45f, 0.70f, 1.0f), ImVec4(0.70f, 0.50f, 0.65f, 1.0f),
				ImVec4(0.70f, 0.40f, 0.50f, 1.0f), ImVec4(0.40f, 0.40f, 0.40f, 1.0f),
				ImVec4(0.70f, 0.20f, 0.20f, 1.0f), ImVec4(0.70f, 0.35f, 0.15f, 1.0f),
				ImVec4(0.50f, 0.30f, 0.15f, 1.0f), ImVec4(0.70f, 0.70f, 0.15f, 1.0f),
				ImVec4(0.50f, 0.65f, 0.15f, 1.0f), ImVec4(0.20f, 0.60f, 0.15f, 1.0f),
				ImVec4(0.15f, 0.60f, 0.45f, 1.0f), ImVec4(0.20f, 0.50f, 0.60f, 1.0f),
				ImVec4(0.10f, 0.30f, 0.60f, 1.0f), ImVec4(0.15f, 0.25f, 0.50f, 1.0f),
				ImVec4(0.30f, 0.20f, 0.60f, 1.0f), ImVec4(0.50f, 0.20f, 0.60f, 1.0f),
				ImVec4(0.70f, 0.15f, 0.45f, 1.0f), ImVec4(0.20f, 0.20f, 0.20f, 1.0f),
				ImVec4(0.00f, 0.00f, 0.00f, 1.0f), ImVec4(1.00f, 0.00f, 0.00f, 1.0f),
				ImVec4(0.00f, 1.00f, 0.00f, 1.0f), ImVec4(1.00f, 1.00f, 0.00f, 1.0f),
				ImVec4(0.00f, 0.00f, 1.00f, 1.0f), ImVec4(1.00f, 0.00f, 1.00f, 1.0f),
				ImVec4(0.00f, 1.00f, 1.00f, 1.0f), ImVec4(0.50f, 0.00f, 0.00f, 1.0f),
				ImVec4(0.50f, 0.50f, 0.00f, 1.0f), ImVec4(0.00f, 0.50f, 0.00f, 1.0f),
				ImVec4(0.00f, 0.50f, 0.50f, 1.0f), ImVec4(0.00f, 0.00f, 0.50f, 1.0f),
				ImVec4(0.50f, 0.00f, 0.50f, 1.0f)
			};
			const char* colorNames[] = {
				"0: Light Pink", "1: Light Orange", "2: Gold", "3: Light Yellow",
				"4: Light Lime", "5: Light Green", "6: Light Mint", "7: Light Cyan",
				"8: Light Blue", "9: Medium Blue", "10: Light Purple", "11: Light Magenta",
				"12: Light Pink", "13: White", "14: Red", "15: Orange",
				"16: Brown", "17: Yellow", "18: Lime", "19: Green",
				"20: Teal", "21: Cyan", "22: Blue", "23: Medium Blue",
				"24: Purple", "25: Magenta", "26: Pink", "27: Light Gray",
				"28: Salmon", "29: Peach", "30: Tan", "31: Pale Yellow",
				"32: Pale Green", "33: Olive Green", "34: Sage", "35: Pale Cyan",
				"36: Pale Blue", "37: Lavender", "38: Pale Purple", "39: Pale Magenta",
				"40: Pale Pink", "41: Medium Gray", "42: Dusty Rose", "43: Copper",
				"44: Taupe", "45: Olive", "46: Moss Green", "47: Forest Green",
				"48: Sea Green", "49: Steel Blue", "50: Slate Blue", "51: Navy Blue",
				"52: Plum", "53: Mauve", "54: Raspberry", "55: Dark Gray",
				"56: Dark Red", "57: Burnt Orange", "58: Dark Brown", "59: Dark Yellow",
				"60: Dark Lime", "61: Dark Green", "62: Dark Teal", "63: Dark Cyan",
				"64: Dark Blue", "65: Navy", "66: Dark Purple", "67: Dark Magenta",
				"68: Dark Pink", "69: Very Dark Gray", "70: Black", "71: Pure Red",
				"72: Pure Green", "73: Pure Yellow", "74: Pure Blue", "75: Pure Magenta",
				"76: Pure Cyan", "77: Dark Red", "78: Olive", "79: Dark Green",
				"80: Dark Teal", "81: Dark Blue", "82: Dark Purple"
			};

			int idx = getAbsoluteKnobIndex();
			int currColor = 0;
			if (idx >= 0 && idx < knobConfigs.size()) {
				currColor = knobConfigs[idx].color;
			}
			if (currColor < 0) currColor = 0;
			if (currColor >= IM_ARRAYSIZE(colorNames)) currColor = IM_ARRAYSIZE(colorNames) - 1;

			ImGui::Text("Color:      ");
			ImGui::SameLine();
			ImGui::ColorButton("##knobColorPreview", palette[currColor], 0, ImVec2(20, 20));
			ImGui::SameLine();
			ImGui::PushItemWidth(120);
			if (ImGui::BeginCombo("##knobColorDropdown", colorNames[currColor])) {
				for (int i = 0; i < IM_ARRAYSIZE(colorNames); i++) {
					bool isSelected = (i == currColor);
					ImGui::PushID(i);
					ImGui::ColorButton("##knobColorSwatch", palette[i], 0, ImVec2(15, 15));
					ImGui::SameLine();
					if (ImGui::Selectable(colorNames[i], isSelected)) {
						knobConfigs[idx].color = i;
						storeCurrentKnobSettings();
						applyKnobConfiguration(idx);
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();
		}),
		knobColorRegion
	);

	// Thicker separator after Knob section
	addCustomRegion(
		ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); }),
		ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); })
	);

	// Switch parameters
	addParameter(selectedSwitch.set("Switch", 0, 0, NUM_SWITCHES_PER_PAGE - 1));
	addParameter(switchName.set("Name", "Switch 1"));
	addParameter(switchMidiChannel.set("MIDI Ch", 1, 1, 16));
	addParameter(switchMidiCC.set("MIDI CC", 64, 0, 127));

	// Switch color dropdown at end of switch block
	addCustomRegion(
		switchColorRegion.set("Switch Colors", [this](){
			const ImVec4 palette[] = {
				ImVec4(0.95f, 0.70f, 0.75f, 1.0f), ImVec4(0.97f, 0.72f, 0.35f, 1.0f),
				ImVec4(0.85f, 0.65f, 0.30f, 1.0f), ImVec4(0.97f, 0.97f, 0.50f, 1.0f),
				ImVec4(0.85f, 0.97f, 0.40f, 1.0f), ImVec4(0.60f, 0.95f, 0.40f, 1.0f),
				ImVec4(0.40f, 0.95f, 0.65f, 1.0f), ImVec4(0.40f, 0.95f, 0.95f, 1.0f),
				ImVec4(0.60f, 0.80f, 0.95f, 1.0f), ImVec4(0.50f, 0.60f, 0.95f, 1.0f),
				ImVec4(0.60f, 0.50f, 0.95f, 1.0f), ImVec4(0.80f, 0.50f, 0.95f, 1.0f),
				ImVec4(0.95f, 0.50f, 0.75f, 1.0f), ImVec4(1.00f, 1.00f, 1.00f, 1.0f),
				ImVec4(0.95f, 0.30f, 0.30f, 1.0f), ImVec4(0.95f, 0.50f, 0.20f, 1.0f),
				ImVec4(0.65f, 0.40f, 0.20f, 1.0f), ImVec4(0.95f, 0.95f, 0.20f, 1.0f),
				ImVec4(0.65f, 0.95f, 0.20f, 1.0f), ImVec4(0.30f, 0.80f, 0.20f, 1.0f),
				ImVec4(0.20f, 0.80f, 0.60f, 1.0f), ImVec4(0.20f, 0.85f, 0.95f, 1.0f),
				ImVec4(0.30f, 0.60f, 0.95f, 1.0f), ImVec4(0.20f, 0.40f, 0.80f, 1.0f),
				ImVec4(0.40f, 0.30f, 0.80f, 1.0f), ImVec4(0.65f, 0.30f, 0.80f, 1.0f),
				ImVec4(0.95f, 0.20f, 0.60f, 1.0f), ImVec4(0.80f, 0.80f, 0.80f, 1.0f),
				ImVec4(0.80f, 0.50f, 0.40f, 1.0f), ImVec4(0.90f, 0.65f, 0.50f, 1.0f),
				ImVec4(0.75f, 0.65f, 0.50f, 1.0f), ImVec4(0.90f, 0.95f, 0.70f, 1.0f),
				ImVec4(0.75f, 0.85f, 0.60f, 1.0f), ImVec4(0.65f, 0.80f, 0.40f, 1.0f),
				ImVec4(0.60f, 0.75f, 0.60f, 1.0f), ImVec4(0.85f, 0.95f, 0.90f, 1.0f),
				ImVec4(0.80f, 0.90f, 0.95f, 1.0f), ImVec4(0.70f, 0.75f, 0.90f, 1.0f),
				ImVec4(0.80f, 0.70f, 0.90f, 1.0f), ImVec4(0.85f, 0.70f, 0.95f, 1.0f),
				ImVec4(0.95f, 0.85f, 0.90f, 1.0f), ImVec4(0.60f, 0.60f, 0.60f, 1.0f),
				ImVec4(0.70f, 0.50f, 0.50f, 1.0f), ImVec4(0.65f, 0.45f, 0.30f, 1.0f),
				ImVec4(0.60f, 0.55f, 0.45f, 1.0f), ImVec4(0.65f, 0.65f, 0.35f, 1.0f),
				ImVec4(0.55f, 0.65f, 0.25f, 1.0f), ImVec4(0.45f, 0.65f, 0.45f, 1.0f),
				ImVec4(0.40f, 0.65f, 0.60f, 1.0f), ImVec4(0.50f, 0.65f, 0.70f, 1.0f),
				ImVec4(0.50f, 0.55f, 0.70f, 1.0f), ImVec4(0.45f, 0.45f, 0.70f, 1.0f),
				ImVec4(0.60f, 0.45f, 0.70f, 1.0f), ImVec4(0.70f, 0.50f, 0.65f, 1.0f),
				ImVec4(0.70f, 0.40f, 0.50f, 1.0f), ImVec4(0.40f, 0.40f, 0.40f, 1.0f),
				ImVec4(0.70f, 0.20f, 0.20f, 1.0f), ImVec4(0.70f, 0.35f, 0.15f, 1.0f),
				ImVec4(0.50f, 0.30f, 0.15f, 1.0f), ImVec4(0.70f, 0.70f, 0.15f, 1.0f),
				ImVec4(0.50f, 0.65f, 0.15f, 1.0f), ImVec4(0.20f, 0.60f, 0.15f, 1.0f),
				ImVec4(0.15f, 0.60f, 0.45f, 1.0f), ImVec4(0.20f, 0.50f, 0.60f, 1.0f),
				ImVec4(0.10f, 0.30f, 0.60f, 1.0f), ImVec4(0.15f, 0.25f, 0.50f, 1.0f),
				ImVec4(0.30f, 0.20f, 0.60f, 1.0f), ImVec4(0.50f, 0.20f, 0.60f, 1.0f),
				ImVec4(0.70f, 0.15f, 0.45f, 1.0f), ImVec4(0.20f, 0.20f, 0.20f, 1.0f),
				ImVec4(0.00f, 0.00f, 0.00f, 1.0f), ImVec4(1.00f, 0.00f, 0.00f, 1.0f),
				ImVec4(0.00f, 1.00f, 0.00f, 1.0f), ImVec4(1.00f, 1.00f, 0.00f, 1.0f),
				ImVec4(0.00f, 0.00f, 1.00f, 1.0f), ImVec4(1.00f, 0.00f, 1.00f, 1.0f),
				ImVec4(0.00f, 1.00f, 1.00f, 1.0f), ImVec4(0.50f, 0.00f, 0.00f, 1.0f),
				ImVec4(0.50f, 0.50f, 0.00f, 1.0f), ImVec4(0.00f, 0.50f, 0.00f, 1.0f),
				ImVec4(0.00f, 0.50f, 0.50f, 1.0f), ImVec4(0.00f, 0.00f, 0.50f, 1.0f),
				ImVec4(0.50f, 0.00f, 0.50f, 1.0f)
			};
			const char* colorNames[] = {
				"0: Light Pink", "1: Light Orange", "2: Gold", "3: Light Yellow",
				"4: Light Lime", "5: Light Green", "6: Light Mint", "7: Light Cyan",
				"8: Light Blue", "9: Medium Blue", "10: Light Purple", "11: Light Magenta",
				"12: Light Pink", "13: White", "14: Red", "15: Orange",
				"16: Brown", "17: Yellow", "18: Lime", "19: Green",
				"20: Teal", "21: Cyan", "22: Blue", "23: Medium Blue",
				"24: Purple", "25: Magenta", "26: Pink", "27: Light Gray",
				"28: Salmon", "29: Peach", "30: Tan", "31: Pale Yellow",
				"32: Pale Green", "33: Olive Green", "34: Sage", "35: Pale Cyan",
				"36: Pale Blue", "37: Lavender", "38: Pale Purple", "39: Pale Magenta",
				"40: Pale Pink", "41: Medium Gray", "42: Dusty Rose", "43: Copper",
				"44: Taupe", "45: Olive", "46: Moss Green", "47: Forest Green",
				"48: Sea Green", "49: Steel Blue", "50: Slate Blue", "51: Navy Blue",
				"52: Plum", "53: Mauve", "54: Raspberry", "55: Dark Gray",
				"56: Dark Red", "57: Burnt Orange", "58: Dark Brown", "59: Dark Yellow",
				"60: Dark Lime", "61: Dark Green", "62: Dark Teal", "63: Dark Cyan",
				"64: Dark Blue", "65: Navy", "66: Dark Purple", "67: Dark Magenta",
				"68: Dark Pink", "69: Very Dark Gray", "70: Black", "71: Pure Red",
				"72: Pure Green", "73: Pure Yellow", "74: Pure Blue", "75: Pure Magenta",
				"76: Pure Cyan", "77: Dark Red", "78: Olive", "79: Dark Green",
				"80: Dark Teal", "81: Dark Blue", "82: Dark Purple"
			};

			int idx = getAbsoluteSwitchIndex();
			int currColor = 0;
			if (idx >= 0 && idx < switchConfigs.size()) {
				currColor = switchConfigs[idx].color;
			}
			if (currColor < 0) currColor = 0;
			if (currColor >= IM_ARRAYSIZE(colorNames)) currColor = IM_ARRAYSIZE(colorNames) - 1;

			ImGui::Text("Color:      ");
			ImGui::SameLine();
			ImGui::ColorButton("##switchColorPreview", palette[currColor], 0, ImVec2(20, 20));
			ImGui::SameLine();
			ImGui::PushItemWidth(120);
			if (ImGui::BeginCombo("##switchColorDropdown", colorNames[currColor])) {
				for (int i = 0; i < IM_ARRAYSIZE(colorNames); i++) {
					bool isSelected = (i == currColor);
					ImGui::PushID(1000 + i);
					ImGui::ColorButton("##switchColorSwatch", palette[i], 0, ImVec2(15, 15));
					ImGui::SameLine();
					if (ImGui::Selectable(colorNames[i], isSelected)) {
						switchConfigs[idx].color = i;
						storeCurrentSwitchSettings();
						applySwitchConfiguration(idx);
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();
		}),
		switchColorRegion
	);

	// Thicker separator after Switch section
	addCustomRegion(
		ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); }),
		ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); })
	);
	
	// Listen for page changes
	listeners.push(selectedPage.newListener([this](int &){
		onPageChanged();
		updateSelectedKnobParameters();
		updateSelectedSwitchParameters();
	}));
	
	listeners.push(selectedSetupIndex.newListener([this](int &index){
		if (ignoreListeners) return;

		// Si no existeix encara, posem nom per defecte
		if (index < availableSetups.size()) {
			if (availableSetups[index].exists) {
				setupName = availableSetups[index].name;
			} else {
				setupName = (index == 0) ? "Current Setup" : "Setup " + ofToString(index);
			}
		}

		// Carreguem sempre el setup seleccionat al dispositiu
		loadSelectedSetup();
	}));

	listeners.push(setupName.newListener([this](string &newName){
		if (ignoreListeners) return;
		int slot = selectedSetupIndex.get();
		if (slot < 0 || slot >= availableSetups.size()) return;
	
		// 1) Iniciem sessió de config‐update
		sendSerialCommand(CMD_GENERAL, CMD_START_CONFIG_UPDATE, {});
	
		// 2) Enviem SET SETUP NAME
		setSetupName(slot, newName);
	
		// 3) Tanquem sessió de config‐update
		sendSerialCommand(CMD_GENERAL, CMD_END_CONFIG_UPDATE, {});
	
		// 4) Actualitzem localment la llista de noms i marquem que aquest setup existeix
		availableSetups[slot].name = newName;
		availableSetups[slot].exists = true;
	
		// 5) Tornem a demanar al dispositiu el nom del setup actual perquè confirmi
		getCurrentSetup();
	}));
	
	// Listen for knob selection changes
	listeners.push(selectedKnob.newListener([this](int &){
		updateSelectedKnobParameters();
	}));
	
	// Listen for switch selection changes
	listeners.push(selectedSwitch.newListener([this](int &){
		updateSelectedSwitchParameters();
	}));
	
	// Listen for knob parameter changes
	listeners.push(knobName.newListener([this](string &){
		if (ignoreListeners) return;
		storeCurrentKnobSettings();
		applyKnobConfiguration(getAbsoluteKnobIndex());
	}));
	
	listeners.push(knobMidiChannel.newListener([this](int &){
		if (ignoreListeners) return;
		storeCurrentKnobSettings();
		applyKnobConfiguration(getAbsoluteKnobIndex());
	}));
	
	listeners.push(knobMidiCC.newListener([this](int &){
		if (ignoreListeners) return;
		storeCurrentKnobSettings();
		applyKnobConfiguration(getAbsoluteKnobIndex());
	}));
	
	listeners.push(knobSteps.newListener([this](int &){
		if (ignoreListeners) return;
		storeCurrentKnobSettings();
		applyKnobConfiguration(getAbsoluteKnobIndex());
	}));
	
	// Listen for switch parameter changes
	listeners.push(switchName.newListener([this](string &){
		if (ignoreListeners) return;
		storeCurrentSwitchSettings();
		applySwitchConfiguration(getAbsoluteSwitchIndex());
	}));
	
	listeners.push(switchMidiChannel.newListener([this](int &){
		if (ignoreListeners) return;
		storeCurrentSwitchSettings();
		applySwitchConfiguration(getAbsoluteSwitchIndex());
	}));
	
	listeners.push(switchMidiCC.newListener([this](int &){
		if (ignoreListeners) return;
		storeCurrentSwitchSettings();
		applySwitchConfiguration(getAbsoluteSwitchIndex());
	}));
	
	// Initialize with first knob/switch settings
	updateSelectedKnobParameters();
	updateSelectedSwitchParameters();
	
	// Connect to the ROTO-CONTROL device
	setupSerialPort();
	
	// Get current setup info on startup
	if (serialConnected) {
		getCurrentSetup();
		refreshAvailableSetups();
	}
}



void rotoControlConfig::update(ofEventArgs &args) {
	if (serialConnected) {
		readSerialResponses();
	}
}

void rotoControlConfig::setupSerialPort() {
	vector<ofSerialDeviceInfo> devices = serial.getDeviceList();
	bool connected = false;
	
	for (auto& device : devices) {
		string devicePath = device.getDevicePath();
		ofLogNotice("rotoControlConfig") << "Found serial device: " << devicePath;
		
		if (devicePath.find(ROTO_CONTROL_DEVICE_PREFIX) != string::npos) {
			ofLogNotice("rotoControlConfig") << "Attempting to connect to ROTO-CONTROL on port: " << devicePath;
			
			if (serial.setup(devicePath, 115200)) {
				ofLogNotice("rotoControlConfig") << "Connected to ROTO-CONTROL on port: " << devicePath;
				serialConnected = true;
				connected = true;
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
	if (serial.available() > 0) {
		unsigned char buffer[256];
		int numBytes = serial.readBytes(buffer, MIN(256, serial.available()));
		
		if (numBytes > 0) {
			for (int i = 0; i < numBytes; i++) {
				if (buffer[i] == RESP_START_MARKER && i+1 < numBytes) {
					unsigned char responseCode = buffer[i+1];
					
					if (responseCode == RESP_SUCCESS) {
						ofLogNotice("rotoControlConfig") << "Received successful response from ROTO-CONTROL";
						
						// Podria ser GET_CURRENT_SETUP o GET_SETUP <i>
						if (i+2 < numBytes) {
							// Comprovem que tenim prou bytes per llegir SI + nom (13 bytes)
							if (i+3+13 <= numBytes) {
								unsigned char setupIndex = buffer[i+2];
								
								// Extreiem el nom (13 bytes, null-terminated)
								string setupNameFromDevice = "";
								for (int j = 0; j < 13; j++) {
									if (i+3+j < numBytes && buffer[i+3+j] != 0) {
										setupNameFromDevice += (char)buffer[i+3+j];
									}
								}
								
								// 1) Actualitzem always availableSetups[i]: nom i existeix=true
								if (setupIndex < availableSetups.size()) {
									availableSetups[setupIndex].name   = setupNameFromDevice;
									availableSetups[setupIndex].exists = true;
								}
								
								// 2) Si aquest setupIndex coincideix amb l'actual selectedSetupIndex,
								//    actualitzem el nom al UI (sense canviar l'index)
								if (setupIndex == selectedSetupIndex.get()) {
									ignoreListeners = true;
									setupName = setupNameFromDevice;
									ignoreListeners = false;
								}
								
								ofLogNotice("rotoControlConfig") << "Updated setup " << (int)setupIndex
																 << " name: " << setupNameFromDevice;
								
								// Avancem l'i per saltejar els 2+13 bytes ja processats
								i += 2 + 13;
							}
						}
					} else {
						ofLogError("rotoControlConfig") << "Received error response from ROTO-CONTROL: " << (int)responseCode;
					}
					i++;
				}
			}
		}
	}
}

void rotoControlConfig::sendSerialCommand(unsigned char commandType, unsigned char subType, vector<unsigned char> payload) {
	if (!serialConnected) {
		setupSerialPort();
		if (!serialConnected) return;
	}
	
	unsigned short dataLength = payload.size();
	unsigned char lengthMSB = (dataLength >> 8) & 0xFF;
	unsigned char lengthLSB = dataLength & 0xFF;
	
	vector<unsigned char> message;
	message.push_back(CMD_START_MARKER);
	message.push_back(commandType);
	message.push_back(subType);
	message.push_back(lengthMSB);
	message.push_back(lengthLSB);
	
	for (auto& byte : payload) {
		message.push_back(byte);
	}
	
	serial.writeBytes(message.data(), message.size());
	ofSleepMillis(20);
	
	ofLogNotice("rotoControlConfig") << "Sent command: Type=" << (int)commandType
		<< ", SubType=" << (int)subType << ", Length=" << dataLength;
}

void rotoControlConfig::setHardwarePage(int page) {
	if (!serialConnected) {
		setupSerialPort();
		if (!serialConnected) return;
	}
	
	// Set device to MIDI mode and specific page
	// Page index is in multiples of 8 (00 = Page 1, 08 = Page 2, etc.)
	vector<unsigned char> modePayload;
	modePayload.push_back(0x00); // MIDI mode
	modePayload.push_back(page * 8); // Page index
	
	sendSerialCommand(CMD_GENERAL, CMD_SET_MODE, modePayload);
	
	ofLogNotice("rotoControlConfig") << "Set hardware page to: " << page;
}

void rotoControlConfig::onPageChanged() {
	// Update hardware page when GUI page changes
	setHardwarePage(selectedPage);
}

int rotoControlConfig::getAbsoluteKnobIndex() {
	return selectedPage * NUM_KNOBS_PER_PAGE + selectedKnob;
}

int rotoControlConfig::getAbsoluteSwitchIndex() {
	return selectedPage * NUM_SWITCHES_PER_PAGE + selectedSwitch;
}

void rotoControlConfig::updateSelectedKnobParameters() {
	int index = getAbsoluteKnobIndex();
	if (index < 0 || index >= knobConfigs.size()) return;
	
	ignoreListeners = true;
	
	knobName = knobConfigs[index].name;
	knobMidiChannel = knobConfigs[index].midiChannel;
	knobMidiCC = knobConfigs[index].midiCC;
	knobSteps = knobConfigs[index].steps;
	
	ignoreListeners = false;
}

void rotoControlConfig::updateSelectedSwitchParameters() {
	int index = getAbsoluteSwitchIndex();
	if (index < 0 || index >= switchConfigs.size()) return;
	
	ignoreListeners = true;
	
	switchName = switchConfigs[index].name;
	switchMidiChannel = switchConfigs[index].midiChannel;
	switchMidiCC = switchConfigs[index].midiCC;
	
	ignoreListeners = false;
}

void rotoControlConfig::storeCurrentKnobSettings() {
	int index = getAbsoluteKnobIndex();
	if (index < 0 || index >= knobConfigs.size()) return;
	
	knobConfigs[index].name = knobName;
	knobConfigs[index].midiChannel = knobMidiChannel;
	knobConfigs[index].midiCC = knobMidiCC;
	knobConfigs[index].steps = knobSteps;
	knobConfigs[index].configured = true;
}

void rotoControlConfig::storeCurrentSwitchSettings() {
	int index = getAbsoluteSwitchIndex();
	if (index < 0 || index >= switchConfigs.size()) return;
	
	switchConfigs[index].name = switchName;
	switchConfigs[index].midiChannel = switchMidiChannel;
	switchConfigs[index].midiCC = switchMidiCC;
	switchConfigs[index].configured = true;
}

void rotoControlConfig::applyKnobConfiguration(int knobIndex) {
	if (!serialConnected) {
		setupSerialPort();
		if (!serialConnected) {
			ofLogError("rotoControlConfig") << "Cannot apply knob configuration: Serial device not connected";
			return;
		}
	}
	
	if (knobIndex < 0 || knobIndex >= knobConfigs.size()) {
		ofLogError("rotoControlConfig") << "Invalid knob index: " << knobIndex;
		return;
	}
	
	KnobConfig& config = knobConfigs[knobIndex];
	
	// Start config update session
	sendSerialCommand(CMD_GENERAL, CMD_START_CONFIG_UPDATE);
	
	// According to API: knob control index is 0-31 directly
	vector<unsigned char> configPayload;
	
	// ← Ara posem el setup seleccionat en lloc de 0
	   unsigned char si = static_cast<unsigned char>(selectedSetupIndex.get());
	   configPayload.push_back(si);        // Setup index (00–3F)
	   
	configPayload.push_back(static_cast<unsigned char>(knobIndex)); // Control index (00–1F)

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
			configPayload.push_back(0);
		}
	}
	
	// Color scheme
	configPayload.push_back(config.color);
	
	// Haptic mode (0 = KNOB_300 for continuous, 1 = KNOB_N_STEP for stepped)
	bool useSteppedMode = config.steps >= 2;
	configPayload.push_back(useSteppedMode ? 1 : 0);
	
	// Indent positions (only for KNOB_300)
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
					configPayload.push_back(0);
				}
			}
		}
	}
	
	sendSerialCommand(CMD_MIDI, CMD_SET_KNOB_CONTROL_CONFIG, configPayload);
	
	// End config update session
	sendSerialCommand(CMD_GENERAL, CMD_END_CONFIG_UPDATE);
	
	ofLogNotice("rotoControlConfig") << "Applied knob config for index " << knobIndex
										 << " on setup " << static_cast<int>(si);
}

void rotoControlConfig::applySwitchConfiguration(int switchIndex) {
	if (!serialConnected) {
		setupSerialPort();
		if (!serialConnected) {
			ofLogError("rotoControlConfig") << "Cannot apply switch configuration: Serial device not connected";
			return;
		}
	}
	
	if (switchIndex < 0 || switchIndex >= switchConfigs.size()) {
		ofLogError("rotoControlConfig") << "Invalid switch index: " << switchIndex;
		return;
	}
	
	SwitchConfig& config = switchConfigs[switchIndex];
	
	// Start config update session
	sendSerialCommand(CMD_GENERAL, CMD_START_CONFIG_UPDATE);
	
	// According to API: switch control index is 0-31 directly
	vector<unsigned char> configPayload;
	
	// ← Ara usem el setup seleccionat
	   unsigned char si = static_cast<unsigned char>(selectedSetupIndex.get());
	   configPayload.push_back(si);           // Setup index (00–3F)
	   
	configPayload.push_back(static_cast<unsigned char>(switchIndex)); // Control index (00–1F)
	
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
			configPayload.push_back(0);
		}
	}
	
	// Color scheme for the switch display
	configPayload.push_back(config.color);
	
	// LED ON colour (same as display color)
	configPayload.push_back(config.color);
	
	// LED OFF colour (black - color index 70)
	configPayload.push_back(70);
	
	// Haptic mode (0 = PUSH, 1 = TOGGLE) - Use PUSH so LED only lights when pressed
	configPayload.push_back(0);
	
	// Haptic steps (0 for normal two position switch)
	configPayload.push_back(0);
	
	sendSerialCommand(CMD_MIDI, CMD_SET_SWITCH_CONTROL_CONFIG, configPayload);
	
	// End config update session
	sendSerialCommand(CMD_GENERAL, CMD_END_CONFIG_UPDATE);
	
	ofLogNotice("rotoControlConfig") << "Applied switch config for index " << switchIndex
									  << " on setup " << static_cast<int>(si);
}


void rotoControlConfig::refreshAvailableSetups() {
	if (!serialConnected) {
		ofLogWarning("rotoControlConfig") << "Cannot refresh setups: Serial device not connected";
		return;
	}
	
	ofLogNotice("rotoControlConfig") << "Refreshing available setups...";
	
	// Query each setup to see if it exists and get its name
	for (int i = 0; i < 64; i++) {
		vector<unsigned char> payload;
		payload.push_back(i); // Setup index
		
		sendSerialCommand(CMD_MIDI, 0x02, payload); // GET SETUP command
		ofSleepMillis(20); // Small delay between commands
	}
}

void rotoControlConfig::getCurrentSetup() {
	if (!serialConnected) {
		ofLogWarning("rotoControlConfig") << "Cannot get current setup: Serial device not connected";
		return;
	}
	
	// GET CURRENT SETUP command
	sendSerialCommand(CMD_MIDI, 0x01);
}

void rotoControlConfig::saveCurrentSetup() {
	if (!serialConnected) {
		ofLogWarning("rotoControlConfig") << "Cannot save setup: Serial device not connected";
		return;
	}
	
	int setupIndex = selectedSetupIndex.get();
	ofLogNotice("rotoControlConfig") << "Saving current configuration to setup slot: " << setupIndex;
	
	// Start config update session
	sendSerialCommand(CMD_GENERAL, CMD_START_CONFIG_UPDATE);
	ofSleepMillis(50);
	
	// Set the setup name first (if not slot 0)
	if (setupIndex > 0) {
		setSetupName(setupIndex, setupName.get());
		ofSleepMillis(50);
	}
	
	// Save all current knob configurations to the setup
	for (int i = 0; i < TOTAL_KNOBS; i++) {
		if (knobConfigs[i].configured) {
			// The knob configuration commands will automatically save to current setup
			applyKnobConfiguration(i);
			ofSleepMillis(30);
		}
	}
	
	// Save all current switch configurations to the setup
	for (int i = 0; i < TOTAL_SWITCHES; i++) {
		if (switchConfigs[i].configured) {
			// The switch configuration commands will automatically save to current setup
			applySwitchConfiguration(i);
			ofSleepMillis(30);
		}
	}
	
	// If saving to a different slot, set that setup as current
	if (setupIndex > 0) {
		setCurrentSetup(setupIndex);
		ofSleepMillis(50);
	}
	
	// End config update session
	sendSerialCommand(CMD_GENERAL, CMD_END_CONFIG_UPDATE);
	
	// Mark setup as existing and update name
	if (setupIndex < availableSetups.size()) {
		availableSetups[setupIndex].exists = true;
		availableSetups[setupIndex].name = setupName.get();
	}
	
	ofLogNotice("rotoControlConfig") << "Setup saved successfully to slot " << setupIndex;
}

void rotoControlConfig::loadSelectedSetup() {
	if (!serialConnected) {
		ofLogWarning("rotoControlConfig") << "Cannot load setup: Serial device not connected";
		return;
	}

	int setupIndex = selectedSetupIndex.get();
	if (setupIndex < 0 || setupIndex >= availableSetups.size()) {
		ofLogWarning("rotoControlConfig") << "Invalid setup slot: " << setupIndex;
		return;
	}

	ofLogNotice("rotoControlConfig") << "Loading setup slot: " << setupIndex;

	// Enviem SET SETUP per indicar al dispositiu que canviï de slot
	setCurrentSetup(static_cast<unsigned char>(setupIndex));
	ofSleepMillis(100); // Deixem temps al dispositiu per processar

	// Demanem el CURRENT SETUP per refrescar nom i existència
	getCurrentSetup();
}


void rotoControlConfig::setSetupName(int setupIndex, const string& name) {
	if (!serialConnected) return;

	vector<unsigned char> payload;
	payload.push_back(static_cast<unsigned char>(setupIndex)); // Slot

	// Afegir 13 bytes (nom + nulls)
	for (int i = 0; i < 13; i++) {
		if (i < name.length()) payload.push_back(name[i]);
		else payload.push_back(0);
	}

	sendSerialCommand(CMD_MIDI, 0x04, payload); // CMD = SET SETUP NAME
}


void rotoControlConfig::setCurrentSetup(int setupIndex) {
	if (!serialConnected) return;
	
	vector<unsigned char> payload;
	payload.push_back(setupIndex); // Setup index
	
	sendSerialCommand(CMD_MIDI, 0x03, payload); // SET SETUP command
}

void rotoControlConfig::presetRecallBeforeSettingParameters(ofJson &json) {
	// Clear existing configurations
	knobConfigs.clear();
	switchConfigs.clear();
	knobConfigs.resize(TOTAL_KNOBS);
	switchConfigs.resize(TOTAL_SWITCHES);
	
	// Initialize with default values
	for (int i = 0; i < TOTAL_KNOBS; i++) {
		knobConfigs[i].name = "Knob " + ofToString((i % NUM_KNOBS_PER_PAGE) + 1);
		knobConfigs[i].color = 0;
		knobConfigs[i].midiChannel = 1;
		knobConfigs[i].midiCC = i % 128;
		knobConfigs[i].steps = 0;
		knobConfigs[i].configured = false;
	}
	
	for (int i = 0; i < TOTAL_SWITCHES; i++) {
		switchConfigs[i].name = "Switch " + ofToString((i % NUM_SWITCHES_PER_PAGE) + 1);
		switchConfigs[i].color = 0;
		switchConfigs[i].midiChannel = 1;
		switchConfigs[i].midiCC = (64 + i) % 128;
		switchConfigs[i].configured = false;
	}
	
	// Initialize setup management if not already done
	if (availableSetups.size() != 64) {
		availableSetups.clear();
		availableSetups.resize(64);
		for (int i = 0; i < 64; i++) {
			availableSetups[i].index = i;
			availableSetups[i].name = (i == 0) ? "Current Setup" : "Setup " + ofToString(i);
			availableSetups[i].exists = false;
		}
	}
}

void rotoControlConfig::presetRecallAfterSettingParameters(ofJson &json) {
	// Load knob configs from preset if available
	if (json.contains("knobConfigs")) {
		ofJson knobJson = json["knobConfigs"];
		
		for (int i = 0; i < TOTAL_KNOBS && i < knobJson.size(); i++) {
			ofJson configJson = knobJson[i];
			
			if (configJson.contains("name")) {
				knobConfigs[i].name = configJson["name"];
			}
			if (configJson.contains("color")) {
				knobConfigs[i].color = configJson["color"];
			}
			if (configJson.contains("midiChannel")) {
				knobConfigs[i].midiChannel = configJson["midiChannel"];
			}
			if (configJson.contains("midiCC")) {
				knobConfigs[i].midiCC = configJson["midiCC"];
			}
			if (configJson.contains("steps")) {
				knobConfigs[i].steps = configJson["steps"];
			}
			if (configJson.contains("configured")) {
				knobConfigs[i].configured = configJson["configured"];
			}
		}
	}
	
	// Load switch configs from preset if available
	if (json.contains("switchConfigs")) {
		ofJson switchJson = json["switchConfigs"];
		
		for (int i = 0; i < TOTAL_SWITCHES && i < switchJson.size(); i++) {
			ofJson configJson = switchJson[i];
			
			if (configJson.contains("name")) {
				switchConfigs[i].name = configJson["name"];
			}
			if (configJson.contains("color")) {
				switchConfigs[i].color = configJson["color"];
			}
			if (configJson.contains("midiChannel")) {
				switchConfigs[i].midiChannel = configJson["midiChannel"];
			}
			if (configJson.contains("midiCC")) {
				switchConfigs[i].midiCC = configJson["midiCC"];
			}
			if (configJson.contains("configured")) {
				switchConfigs[i].configured = configJson["configured"];
			}
		}
	}
	
	// Load setup management info from preset if available
	if (json.contains("availableSetups")) {
		ofJson setupJson = json["availableSetups"];
		
		for (int i = 0; i < availableSetups.size() && i < setupJson.size(); i++) {
			ofJson setupInfoJson = setupJson[i];
			
			if (setupInfoJson.contains("index")) {
				availableSetups[i].index = setupInfoJson["index"];
			}
			if (setupInfoJson.contains("name")) {
				availableSetups[i].name = setupInfoJson["name"];
			}
			if (setupInfoJson.contains("exists")) {
				availableSetups[i].exists = setupInfoJson["exists"];
			}
		}
	}
	
	// Load selected setup index
	if (json.contains("selectedSetupIndex")) {
		int loadedIndex = json["selectedSetupIndex"];
		if (loadedIndex >= 0 && loadedIndex < availableSetups.size()) {
			selectedSetupIndex = loadedIndex;
			setupName = availableSetups[loadedIndex].name;
		}
	}
	
	// Update the UI with the current control settings
	updateSelectedKnobParameters();
	updateSelectedSwitchParameters();
	
	// Set hardware page
	setHardwarePage(selectedPage);
	
	// Apply all configurations to the device
	if (serialConnected) {
		// Apply knob configurations
		for (int i = 0; i < TOTAL_KNOBS; i++) {
			if (knobConfigs[i].configured) {
				applyKnobConfiguration(i);
				ofSleepMillis(50); // Small delay between commands
			}
		}
		
		// Apply switch configurations
		for (int i = 0; i < TOTAL_SWITCHES; i++) {
			if (switchConfigs[i].configured) {
				applySwitchConfiguration(i);
				ofSleepMillis(50); // Small delay between commands
			}
		}
		
		// Refresh setup information from device
		getCurrentSetup();
		ofSleepMillis(100);
		refreshAvailableSetups();
	}
}

void rotoControlConfig::presetSave(ofJson &json) {
	// Save knob configurations
	ofJson knobJson = ofJson::array();
	
	for (int i = 0; i < knobConfigs.size(); i++) {
		ofJson configJson;
		configJson["name"] = knobConfigs[i].name;
		configJson["color"] = knobConfigs[i].color;
		configJson["midiChannel"] = knobConfigs[i].midiChannel;
		configJson["midiCC"] = knobConfigs[i].midiCC;
		configJson["steps"] = knobConfigs[i].steps;
		configJson["configured"] = knobConfigs[i].configured;
		
		knobJson.push_back(configJson);
	}
	
	json["knobConfigs"] = knobJson;
	
	// Save switch configurations
	ofJson switchJson = ofJson::array();
	
	for (int i = 0; i < switchConfigs.size(); i++) {
		ofJson configJson;
		configJson["name"] = switchConfigs[i].name;
		configJson["color"] = switchConfigs[i].color;
		configJson["midiChannel"] = switchConfigs[i].midiChannel;
		configJson["midiCC"] = switchConfigs[i].midiCC;
		configJson["configured"] = switchConfigs[i].configured;
		
		switchJson.push_back(configJson);
	}
	
	json["switchConfigs"] = switchJson;
	
	// Save setup management info
	ofJson setupJson = ofJson::array();
	for (int i = 0; i < availableSetups.size(); i++) {
		ofJson setupInfoJson;
		setupInfoJson["index"] = availableSetups[i].index;
		setupInfoJson["name"] = availableSetups[i].name;
		setupInfoJson["exists"] = availableSetups[i].exists;
		setupJson.push_back(setupInfoJson);
	}
	json["availableSetups"] = setupJson;
	json["selectedSetupIndex"] = selectedSetupIndex.get();
}
