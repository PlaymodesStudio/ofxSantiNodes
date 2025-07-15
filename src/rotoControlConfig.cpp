#include "rotoControlConfig.h"
#include "imgui.h"
#include <thread>
#include <atomic>

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
// ROTO-CONTROL device patterns: cu.usbmodem101, cu.usbmodem1101, cu.usbmodem2101

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
	description = "Configure ROTO-CONTROL knobs, switches, and setups. "
	"Set names, colors, MIDI channels, CC numbers, and step counts for each control "
	"across multiple pages. Manage device setups for different configurations.";
	
	// ─────────────────────────────────────────────────────────────────────────
	// Resize 2D arrays so each of the 64 setups has 32 knobs & 32 switches.
	allKnobConfigs.resize(MAX_SETUPS);
	allSwitchConfigs.resize(MAX_SETUPS);
	
	for (int s = 0; s < MAX_SETUPS; ++s) {
		allKnobConfigs[s].resize(TOTAL_KNOBS);
		allSwitchConfigs[s].resize(TOTAL_SWITCHES);
		
		// Calculate MIDI channel for this setup (1-16)
		// Every 4 setups get a new channel: setups 0-3=ch1, 4-7=ch2, etc.
		int midiChannel = (s / 4) + 1;
		
		// Calculate base CC for this setup within its channel
		// Each setup gets 32 CCs: setup 0=CCs 0-31, setup 1=CCs 32-63, etc.
		// But within each channel, we cycle: setup 0&4&8...=CCs 0-31, setup 1&5&9...=CCs 32-63
		int baseCCForSetup = (s % 4) * 32;
		
		// Initialize knobs (32 total: 16 per page)
		for (int i = 0; i < TOTAL_KNOBS; i++) {
			int page = i / NUM_KNOBS_PER_PAGE;           // Which page (0-1)
			int knobOnPage = i % NUM_KNOBS_PER_PAGE;     // Which knob on that page (0-7)
			
			// Page 0: knobs get CCs 0-7, Page 1: knobs get CCs 16-23
			int pageOffset = page * 16;                  // Page 0=0, Page 1=16
			int midiCC = baseCCForSetup + pageOffset + knobOnPage;
			
			//allKnobConfigs[s][i].name = "Knob " + ofToString(knobOnPage + 1);
			allKnobConfigs[s][i].name = "";
			allKnobConfigs[s][i].color = 70;
			allKnobConfigs[s][i].midiChannel = midiChannel;
			allKnobConfigs[s][i].midiCC = midiCC;
			allKnobConfigs[s][i].steps = 0;
			allKnobConfigs[s][i].configured = false;
		}
		
		// Initialize switches (32 total: 16 per page)
		for (int i = 0; i < TOTAL_SWITCHES; i++) {
			int page = i / NUM_SWITCHES_PER_PAGE;        // Which page (0-1)
			int switchOnPage = i % NUM_SWITCHES_PER_PAGE; // Which switch on that page (0-7)
			
			// Page 0: switches get CCs 8-15, Page 1: switches get CCs 24-31
			int pageOffset = page * 16;                  // Page 0=0, Page 1=16
			int midiCC = baseCCForSetup + pageOffset + 8 + switchOnPage;
			
			//allSwitchConfigs[s][i].name = "Switch " + ofToString(switchOnPage + 1);
			allSwitchConfigs[s][i].name = "";
			allSwitchConfigs[s][i].momentary = true;
			allSwitchConfigs[s][i].color = 70;
			allSwitchConfigs[s][i].midiChannel = midiChannel;
			allSwitchConfigs[s][i].midiCC = midiCC;
			allSwitchConfigs[s][i].configured = false;
		}
	}
	
	// Initialize setup management
	availableSetups.resize(MAX_SETUPS);
	for (int i = 0; i < MAX_SETUPS; i++) {
		availableSetups[i].index = i;
		availableSetups[i].name = (i == 0) ? "Current Setup" : "Setup " + ofToString(i);
		availableSetups[i].exists = false;
	}
	
	// UI parameters: select which slot (0..63) is active
	addParameter(selectedSetupIndex.set("Setup Slot", 0, 0, MAX_SETUPS - 1));
	addParameter(setupName.set("Setup Name", "Current Setup"));
	
	// Thicker separator after Setup section
	addCustomRegion(
					ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); }),
					ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); })
					);
	
	listeners.push(selectedSetupIndex.newListener([this](int &index){
		if(ignoreListeners) return;
		
		// CRITICAL: Add bounds checking for selectedSetupIndex
		if(index < 0 || index >= MAX_SETUPS) {
			ofLogError("rotoControlConfig") << "Invalid setup index: " << index << " (valid range: 0-" << (MAX_SETUPS-1) << ")";
			// Reset to safe value
			ignoreListeners = true;
			selectedSetupIndex = 0;
			ignoreListeners = false;
			return;
		}
		
		// Update setupName based on what we know about this slot
		if(index < availableSetups.size()) {
			ignoreListeners = true;
			if(availableSetups[index].exists) {
				setupName = availableSetups[index].name;
			} else {
				setupName = (index == 0) ? "Current Setup" : "Setup " + ofToString(index);
			}
			ignoreListeners = false;
		}
		
		// Load the selected setup
		loadSelectedSetup();
		
		// ADD THESE LINES: Update GUI to show the new setup's knob/switch values
		updateSelectedKnobParameters();
		updateSelectedSwitchParameters();
	}));
	
	listeners.push(selectedPage.newListener([this](int &newPage){
		if (ignoreListeners) return;
		onPageChanged();
		updateSelectedKnobParameters();
		updateSelectedSwitchParameters();
	}));
	
	setupSerialPort();
	if(serialConnected){
		// Query current setup - this will update selectedSetupIndex AND setupName
		getCurrentSetup();
		// Queue up all setup queries
		refreshAvailableSetups();
	}
	
	addInspectorParameter(retriggerMidiBounds.set("Retrigger MIDI Bounds", false));

	
	// Page selection
	addParameter(selectedPage.set("Page", 0, 0, NUM_PAGES - 1));
	
	// Thicker separator after Page section
	addCustomRegion(
					ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); }),
					ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); })
					);
	
	// Knob parameters
	addParameter(selectedKnob.set("Knob", 0, 0, NUM_KNOBS_PER_PAGE - 1));
	addParameter(knobName.set("K Name", "Knob 1"));
	addParameter(knobMidiChannel.set("K MIDI Ch", 1, 1, 16));
	addParameter(knobMidiCC.set("K MIDI CC", 0, 0, 127));
	addParameter(knobSteps.set("K Steps", 0, 0, 10));
	
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
						int setupIndex = selectedSetupIndex.get();
						
						// CRITICAL: Add bounds checking
						if (setupIndex < 0 || setupIndex >= MAX_SETUPS || idx < 0 || idx >= TOTAL_KNOBS) {
							ImGui::Text("Invalid knob selection");
							return;
						}
						
						int currColor = allKnobConfigs[setupIndex][idx].color;
						
						if (idx >= 0 && idx < TOTAL_KNOBS) {
							currColor = allKnobConfigs[selectedSetupIndex.get()][idx].color;
						}
						if (currColor < 0) currColor = 0;
						if (currColor >= IM_ARRAYSIZE(colorNames)) currColor = IM_ARRAYSIZE(colorNames) - 1;
						
						ImGui::Text("K Color    ");
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
									// Add bounds check before assignment
									if (setupIndex >= 0 && setupIndex < MAX_SETUPS && idx >= 0 && idx < TOTAL_KNOBS) {
										allKnobConfigs[setupIndex][idx].color = i;
										storeCurrentKnobSettings();
										applyKnobConfiguration(idx);
									}
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
	addParameter(switchName.set("S Name", "Switch 1"));
	addParameter(switchMomentary.set("Push", true));
	addParameter(switchMidiChannel.set("S MIDI Ch", 1, 1, 16));
	addParameter(switchMidiCC.set("S MIDI CC", 64, 0, 127));
	
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
						int setupIndex = selectedSetupIndex.get();
						
						// CRITICAL: Add bounds checking
						if (setupIndex < 0 || setupIndex >= MAX_SETUPS || idx < 0 || idx >= TOTAL_SWITCHES) {
							ImGui::Text("Invalid switch selection");
							return;
						}
						
						int currColor = allSwitchConfigs[setupIndex][idx].color;
						
						if (idx >= 0 && idx < TOTAL_SWITCHES) {
							currColor = allSwitchConfigs[selectedSetupIndex.get()][idx].color;
						}
						if (currColor < 0) currColor = 0;
						if (currColor >= IM_ARRAYSIZE(colorNames)) currColor = IM_ARRAYSIZE(colorNames) - 1;
						
						ImGui::Text("S Color    ");
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
									// Add bounds check before assignment
									if (setupIndex >= 0 && setupIndex < MAX_SETUPS && idx >= 0 && idx < TOTAL_SWITCHES) {
										allSwitchConfigs[setupIndex][idx].color = i;
										storeCurrentSwitchSettings();
										applySwitchConfiguration(idx);
									}
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
	
	// Add configuration save/load controls at the end
	   addCustomRegion(
		   ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); }),
		   ofParameter<std::function<void()>>().set("", [](){ drawThickSeparator(); })
	   );
	   
		addParameter(configStatus.set("Status", "Ready"));
	   addParameter(saveConfigButton.set("Save Config"));
	   addParameter(loadConfigButton.set("Load Config"));
	   
	   // Setup listeners for save/load buttons
	   listeners.push(saveConfigButton.newListener([this](void){
		   saveConfigurationFile();
	   }));
	   
	   listeners.push(loadConfigButton.newListener([this](void){
		   loadConfigurationFile();
	   }));
	
	
	// Setup name change listener
	listeners.push(setupName.newListener([this](string &newName){
		if (ignoreListeners) return;
		int slot = selectedSetupIndex.get();
		if (slot < 0 || slot >= availableSetups.size()) return;
		
		// Start config update session
		sendSerialCommand(CMD_GENERAL, CMD_START_CONFIG_UPDATE, {});
		
		// Send SET SETUP NAME
		setSetupName(slot, newName);
		
		// End config update session
		sendSerialCommand(CMD_GENERAL, CMD_END_CONFIG_UPDATE, {});
		
		// Update local cache
		availableSetups[slot].name = newName;
		availableSetups[slot].exists = true;
		
		// Re-query current setup to confirm
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
	
	listeners.push(switchMomentary.newListener([this](bool &){
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
	
	
}

rotoControlConfig::~rotoControlConfig() {
	// Stop any running configuration thread
	if (configThread.joinable()) {
		isConfiguring = false;
		configThread.join();
	}
	closeSerialPort();
}



void rotoControlConfig::update(ofEventArgs &args) {
	if (serialConnected) {
		readSerialResponses();
	}
}

void rotoControlConfig::setupSerialPort() {
	// If we already had a serial connection, close it first
	if(serialConnected) {
		serial.close();
		serialConnected = false;
		// give the OS a moment to actually release the port
		ofSleepMillis(50);
	}
	
	// Now do a fresh scan and open
	vector<ofSerialDeviceInfo> devices = serial.getDeviceList();
	bool connected = false;
	
	ofLogNotice("rotoControlConfig") << "Scanning for ROTO-CONTROL devices...";
	
	for(auto & device : devices) {
		string devicePath = device.getDevicePath();
		ofLogNotice("rotoControlConfig") << "Found serial device: " << devicePath;
		
		// Look for specific ROTO-CONTROL device patterns
		if(devicePath.find("cu.usbmodem101") != string::npos ||
		   devicePath.find("cu.usbmodem1101") != string::npos ||
		   devicePath.find("cu.usbmodem2101") != string::npos) {
			ofLogNotice("rotoControlConfig") << "Attempting to connect to ROTO-CONTROL on port: " << devicePath;
			if(serial.setup(devicePath, 115200)) {
				ofLogNotice("rotoControlConfig") << "Connected to ROTO-CONTROL on port: " << devicePath;
				serialConnected = true;
				connected = true;
				
				// Drain any junk that may be sitting in the input buffer
				ofSleepMillis(20);
				while(serial.available() > 0) {
					serial.readByte();
				}
				break;
			} else {
				ofLogError("rotoControlConfig") << "Failed to connect to ROTO-CONTROL on " << devicePath;
			}
		}
	}
	
	if(!connected) {
		ofLogError("rotoControlConfig") << "Could not find any ROTO-CONTROL device. Available devices:";
		for(auto & device : devices) {
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
	if (!serialConnected) return;
	
	// If there is data waiting on the serial port, read up to 256 bytes
	if (serial.available() > 0) {
		unsigned char buffer[256];
		int numBytes = serial.readBytes(buffer, MIN(256, serial.available()));
		if (numBytes <= 0) return;
		
		for (int i = 0; i < numBytes; ++i) {
			// ────────────────────────────────────────────────────────────────────────
			// 1) Handle any "A5"-prefixed responses (GET_CURRENT_SETUP / GET_SETUP)
			// ────────────────────────────────────────────────────────────────────────
			if (buffer[i] == RESP_START_MARKER) {
				// ────────────────────────────────────────────────────────────────────
				// 1a) TRY TO PARSE GET_CURRENT_SETUP reply:
				//     A5 02 01 <CL=0x000D> <SI> <13-byte ASCII name>
				//     (length == 13) :contentReference[oaicite:0]{index=0}
				// ────────────────────────────────────────────────────────────────────
				if ((i + 4) < numBytes) {
					unsigned char cmdType = buffer[i + 1];  // should be CMD_MIDI (0x02)
					unsigned char subType = buffer[i + 2];  // 0x01 = GET_CURRENT_SETUP
					unsigned short length = (static_cast<unsigned short>(buffer[i + 3]) << 8)
					| static_cast<unsigned short>(buffer[i + 4]);
					
					if (cmdType == CMD_MIDI && subType == 0x01 && length == 13) {
						// Make sure we have all 13 bytes of payload in the buffer
						if ((i + 5 + 12) < numBytes) {
							unsigned char setupIndex = buffer[i + 5];
							
							// CRITICAL: Add bounds checking for setup index
							if (setupIndex >= MAX_SETUPS) {
								ofLogError("rotoControlConfig") << "Invalid setup index received: " << (int)setupIndex << " (max: " << (MAX_SETUPS-1) << ")";
								i += 5 + 13 - 1;
								continue;
							}
							
							std::string nameFromDevice;
							nameFromDevice.reserve(13);
							for (int j = 0; j < 13; ++j) {
								unsigned char c = buffer[i + 6 + j];
								if (c != 0) nameFromDevice.push_back(static_cast<char>(c));
							}
							
							// 1) Cache into our local list
							if (setupIndex < availableSetups.size()) {
								availableSetups[setupIndex].name   = nameFromDevice;
								availableSetups[setupIndex].exists = true;
							}
							
							// 2) Update GUI fields
							ignoreListeners = true;
							selectedSetupIndex = setupIndex;
							setupName = nameFromDevice;
							ignoreListeners = false;
							
							updateSelectedKnobParameters();
							updateSelectedSwitchParameters();
							
							ofLogNotice("rotoControlConfig")
							<< "GET_CURRENT_SETUP reply: slot=" << (int)setupIndex
							<< ", name=\"" << nameFromDevice << "\"";
							
							// Skip exactly (1 + 1 + 1 + 2 + 1 + 13) = 19 bytes
							i += 5 + 13 - 1;
							continue;
						} else {
							// Not enough bytes yet; wait for more on next update()
							break;
						}
					}
				}
				
				// ────────────────────────────────────────────────────────────────────
				// 1b) TRY TO PARSE GET_SETUP reply:
				//     A5 <RC> <SI> <SN:0D>  (no length field) :contentReference[oaicite:1]{index=1}
				//     RC = Response code (00 = SUCCESS)
				//     SI = Setup index (00–3F)
				//     SN = 13-byte NULL-terminated ASCII name
				// ────────────────────────────────────────────────────────────────────
				// We need at least: [A5][RC][SI] + 13 bytes of name = 1 + 1 + 1 + 13 = 16 bytes
				if ((i + 1 + 1 + 13) < numBytes) {
					unsigned char rc    = buffer[i + 1];
					unsigned char slot  = buffer[i + 2];
					
					// CRITICAL: Add bounds checking for setup slot
					if (slot >= MAX_SETUPS) {
						ofLogError("rotoControlConfig") << "Invalid setup slot received: " << (int)slot << " (max: " << (MAX_SETUPS-1) << ")";
						i += 1 + 1 + 1 + 13 - 1;
						continue;
					}
					
					// If the RC is non-zero, it’s an error—skip it (still consume 2 bytes + 13 name bytes)
					// Otherwise, read the 13-byte name
					if (rc == RESP_SUCCESS) {
						std::string nameFromDevice;
						nameFromDevice.reserve(13);
						for (int j = 0; j < 13; ++j) {
							unsigned char c = buffer[i + 3 + j];
							if (c != 0) nameFromDevice.push_back(static_cast<char>(c));
						}
						
						// 1) Store into our local array
						if (slot < availableSetups.size()) {
							availableSetups[slot].name   = nameFromDevice;
							availableSetups[slot].exists = true;
						}
						
						// 2) If this is the currently selected slot in the GUI, update the text field
						if (slot == selectedSetupIndex.get()) {
							ignoreListeners = true;
							setupName = nameFromDevice;
							ignoreListeners = false;
							
							updateSelectedKnobParameters();
							updateSelectedSwitchParameters();
						}
						
						ofLogNotice("rotoControlConfig")
						<< "GET_SETUP reply: slot=" << (int)slot
						<< ", name=\"" << nameFromDevice << "\"";
					} else {
						ofLogWarning("rotoControlConfig")
						<< "GET_SETUP reply returned error code: " << (int)rc;
					}
					
					// Skip exactly 16 bytes: [A5][RC][SI][13-byte SN]
					i += 1 + 1 + 1 + 13 - 1;
					continue;
				} else {
					// Not enough bytes yet; wait for more on next update()
					break;
				}
			}
			
			// ────────────────────────────────────────────────────────────────────────
			// 2) Handle any "5A"-prefixed commands coming *FROM* ROTO (asynchronous)
			//    a) SET MODE   (page change on hardware)
			//    b) SET SETUP  (setup change on hardware)
			// ────────────────────────────────────────────────────────────────────────
			if (buffer[i] == CMD_START_MARKER && (i + 4) < numBytes) {
				unsigned char cmdType = buffer[i + 1];
				unsigned char subType = buffer[i + 2];
				unsigned short length = (static_cast<unsigned short>(buffer[i + 3]) << 8)
				| static_cast<unsigned short>(buffer[i + 4]);
				
				// ───────────────
				// a) SET MODE (page change on hardware)
				//    5A 01 03 <CL=0x0002> <AM> <PI>
				// ───────────────
				if (cmdType == CMD_GENERAL && subType == CMD_SET_MODE
					&& length == 2 && (i + 5 + 1) < numBytes)
				{
					unsigned char modeByte = buffer[i + 5];  // (ignored)
					unsigned char pageByte = buffer[i + 6];  // PI
					int newPage = (pageByte / 8);            // each page is a multiple of 8
					
					// CRITICAL: Add bounds checking for page
					if (newPage >= NUM_PAGES || newPage < 0) {
						ofLogError("rotoControlConfig") << "Invalid page received: " << newPage << " (max: " << (NUM_PAGES-1) << ")";
						i += 5 + 2 - 1;
						continue;
					}
					
					ignoreListeners = true;
					selectedPage = newPage;
					ignoreListeners = false;
					
					ofLogNotice("rotoControlConfig")
					<< "Device switched to page " << newPage;
					
					// Immediately refresh GUI knobs/switches
					updateSelectedKnobParameters();
					updateSelectedSwitchParameters();
					
					// Skip exactly (1+1+1+2+2) = 7 bytes: [5A][01][03][MSB][LSB][AM][PI]
					i += 5 + 2 - 1;
					continue;
				}
				
				// ───────────────
				// b) SET SETUP (setup change on hardware)
				//    5A 02 03 <CL=0x0001> <SI>
				// ───────────────
				if (cmdType == CMD_MIDI && subType == 0x03
					&& length == 1 && (i + 5) < numBytes)
				{
					unsigned char newSetup = buffer[i + 5];
					
					// CRITICAL: Add bounds checking for setup
					if (newSetup >= MAX_SETUPS) {
						ofLogError("rotoControlConfig") << "Invalid setup received: " << (int)newSetup << " (max: " << (MAX_SETUPS-1) << ")";
						i += 5 + 1 - 1;
						continue;
					}
					
					
					ignoreListeners = true;
					selectedSetupIndex = newSetup;
					ignoreListeners = false;
					
					ofLogNotice("rotoControlConfig")
					<< "Device switched to setup " << (int)newSetup;
					
					// Immediately request GET_SETUP for that slot so we repopulate name (and controls)
					{
						std::vector<unsigned char> payload;
						payload.push_back(newSetup);
						sendSerialCommand(CMD_MIDI, 0x02 /* GET_SETUP */, payload);
					}
					
					updateSelectedKnobParameters();
					updateSelectedSwitchParameters();
					
					// Skip exactly (1+1+1+2+1) = 6 bytes: [5A][02][03][MSB][LSB][SI]
					i += 5 + 1 - 1;
					continue;
				}
			}
			
			// ────────────────────────────────────────────────────────────────────────
			// Not an A5 or 5A packet we care about—advance to next byte
			// ────────────────────────────────────────────────────────────────────────
		}
	}
}


void rotoControlConfig::sendSerialCommand(unsigned char commandType, unsigned char subType, vector<unsigned char> payload) {
	if (!serialConnected) {
		return; // Don't try to reconnect during bulk operations
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
	ofSleepMillis(10); // Reduced from 20ms to 10ms for faster bulk operations
	
	ofLogVerbose("rotoControlConfig") << "Sent fast command: Type=" << (int)commandType
									 << ", SubType=" << (int)subType << ", Length=" << dataLength;
}

void rotoControlConfig::setHardwarePage(int page) {
	if (!serialConnected) {
		return; // Don't try to reconnect during loading
	}
	
	// Set device to MIDI mode and specific page
	vector<unsigned char> modePayload;
	modePayload.push_back(0x00); // MIDI mode
	modePayload.push_back(page * 8); // Page index
	
	sendSerialCommand(CMD_GENERAL, CMD_SET_MODE, modePayload);
	
	ofLogNotice("rotoControlConfig") << "Set hardware page to: " << page;
}

void rotoControlConfig::onPageChanged() {
	// Update hardware page when GUI page changes
	setHardwarePage(selectedPage);
	
	// Only sync bound parameters if retrigger option is enabled
		if (retriggerMidiBounds.get()) {
			ofSleepMillis(50); // Give page change time to complete
			syncBoundParametersToHardware();
		}
}

int rotoControlConfig::getAbsoluteKnobIndex() {
	return selectedPage * NUM_KNOBS_PER_PAGE + selectedKnob;
}

int rotoControlConfig::getAbsoluteSwitchIndex() {
	return selectedPage * NUM_SWITCHES_PER_PAGE + selectedSwitch;
}

void rotoControlConfig::updateSelectedKnobParameters() {
	int setupIndex = selectedSetupIndex.get();
	
	// CRITICAL: Add bounds checking
	if (setupIndex < 0 || setupIndex >= MAX_SETUPS) {
		ofLogError("rotoControlConfig") << "Invalid setup index in updateSelectedKnobParameters: " << setupIndex;
		return;
	}
	
	int index = getAbsoluteKnobIndex();
	if (index < 0 || index >= TOTAL_KNOBS) {
		ofLogError("rotoControlConfig") << "Invalid knob index: " << index;
		return;
	}
	ignoreListeners = true;
	
	auto &kc = allKnobConfigs[selectedSetupIndex.get()][index];
	knobName = kc.name;
	knobMidiChannel = kc.midiChannel;
	knobMidiCC = kc.midiCC;
	knobSteps = kc.steps;
	
	ignoreListeners = false;
}

void rotoControlConfig::updateSelectedSwitchParameters() {
	int setupIndex = selectedSetupIndex.get();
	
	// CRITICAL: Add bounds checking
	if (setupIndex < 0 || setupIndex >= MAX_SETUPS) {
		ofLogError("rotoControlConfig") << "Invalid setup index in updateSelectedSwitchParameters: " << setupIndex;
		return;
	}
	
	int index = getAbsoluteSwitchIndex();
	if (index < 0 || index >= TOTAL_SWITCHES) {
		ofLogError("rotoControlConfig") << "Invalid switch index: " << index;
		return;
	}
	
	ignoreListeners = true;
	
	auto &sc = allSwitchConfigs[selectedSetupIndex.get()][index];
	switchName = sc.name;
	switchMidiChannel = sc.midiChannel;
	switchMidiCC = sc.midiCC;
	switchMomentary = sc.momentary;
	
	ignoreListeners = false;
}

void rotoControlConfig::storeCurrentKnobSettings() {
	int setupIndex = selectedSetupIndex.get();
	
	// CRITICAL: Add bounds checking
	if (setupIndex < 0 || setupIndex >= MAX_SETUPS) {
		ofLogError("rotoControlConfig") << "Invalid setup index in storeCurrentKnobSettings: " << setupIndex;
		return;
	}
	
	int index = getAbsoluteKnobIndex();
	if (index < 0 || index >= TOTAL_KNOBS) {
		ofLogError("rotoControlConfig") << "Invalid knob index in storeCurrentKnobSettings: " << index;
		return;
	}
	
	auto &kc = allKnobConfigs[selectedSetupIndex.get()][index];
	kc.name = knobName;
	kc.midiChannel = knobMidiChannel;
	kc.midiCC = knobMidiCC;
	kc.steps = knobSteps;
	kc.configured = true;
}

void rotoControlConfig::storeCurrentSwitchSettings() {
	int setupIndex = selectedSetupIndex.get();
	
	// CRITICAL: Add bounds checking
	if (setupIndex < 0 || setupIndex >= MAX_SETUPS) {
		ofLogError("rotoControlConfig") << "Invalid setup index in storeCurrentSwitchSettings: " << setupIndex;
		return;
	}
	
	int index = getAbsoluteSwitchIndex();
	if (index < 0 || index >= TOTAL_SWITCHES) {
		ofLogError("rotoControlConfig") << "Invalid switch index in storeCurrentSwitchSettings: " << index;
		return;
	}
	
	auto &sc = allSwitchConfigs[selectedSetupIndex.get()][index];
	sc.name = switchName;
	sc.momentary = switchMomentary;
	sc.midiChannel = switchMidiChannel;
	sc.midiCC = switchMidiCC;
	sc.configured = true;
}

void rotoControlConfig::applyKnobConfiguration(int knobIndex) {
	if (!serialConnected) {
		setupSerialPort();
		if (!serialConnected) {
			ofLogError("rotoControlConfig") << "Cannot apply knob configuration: Serial device not connected";
			return;
		}
	}
	
	if (knobIndex < 0 || knobIndex >= TOTAL_KNOBS) {
		ofLogError("rotoControlConfig") << "Invalid knob index: " << knobIndex;
		return;
	}
	
	auto &config = allKnobConfigs[selectedSetupIndex.get()][knobIndex];
	
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
	
	if (switchIndex < 0 || switchIndex >= TOTAL_SWITCHES) {
		ofLogError("rotoControlConfig") << "Invalid switch index: " << switchIndex;
		return;
	}
	
	auto &config = allSwitchConfigs[selectedSetupIndex.get()][switchIndex];
	
	// Start config update session
	sendSerialCommand(CMD_GENERAL, CMD_START_CONFIG_UPDATE);
	
	// According to API: switch control index is 0-31 directly
	vector<unsigned char> configPayload;
	
	// Use the setup selected
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
	
	// Haptic mode - UPDATED TO USE THE MOMENTARY SETTING
	// 0 = PUSH (momentary), 1 = TOGGLE
	configPayload.push_back(config.momentary ? 0 : 1);
	
	// Haptic steps (0 for normal two position switch)
	configPayload.push_back(0);
	
	sendSerialCommand(CMD_MIDI, CMD_SET_SWITCH_CONTROL_CONFIG, configPayload);
	
	// End config update session
	sendSerialCommand(CMD_GENERAL, CMD_END_CONFIG_UPDATE);
	
	ofLogNotice("rotoControlConfig") << "Applied switch config for index " << switchIndex
	<< " on setup " << static_cast<int>(si)
	<< " (mode: " << (config.momentary ? "push" : "toggle") << ")";
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
		if (allKnobConfigs[setupIndex][i].configured) {
			applyKnobConfiguration(i);
			ofSleepMillis(30);
		}
	}
	// 4) Save all switches of the current setup:
	for (int i = 0; i < TOTAL_SWITCHES; i++) {
		if (allSwitchConfigs[setupIndex][i].configured) {
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
	std::vector<unsigned char> payload;
	payload.push_back(static_cast<unsigned char>(setupIndex));
	sendSerialCommand(CMD_MIDI, 0x02 /* GET_SETUP */, payload);
	
	updateSelectedKnobParameters();
	updateSelectedSwitchParameters();
	
	// Only sync bound parameters if retrigger option is enabled
		if (retriggerMidiBounds.get()) {
			ofSleepMillis(50); // Give setup change time to complete
			syncBoundParametersToHardware();
		}
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
	
}

void rotoControlConfig::presetRecallAfterSettingParameters(ofJson &json) {
	// 1) Clear out everything to defaults, in case some setups aren't present
	for (int s = 0; s < MAX_SETUPS; ++s) {
		availableSetups[s].index  = s;
		availableSetups[s].name   = (s == 0) ? "Current Setup" : "Setup " + ofToString(s);
		availableSetups[s].exists = false;
		
		// Calculate MIDI channel and base CC for this setup
		int midiChannel = (s / 4) + 1;                   // Channels 1-16
		int baseCCForSetup = (s % 4) * 32;               // Base CC within channel
		
		// Reset knobConfigs[s] to sequential defaults (2 pages)
		for (int i = 0; i < TOTAL_KNOBS; i++) {
			int page = i / NUM_KNOBS_PER_PAGE;           // Which page (0-1)
			int knobOnPage = i % NUM_KNOBS_PER_PAGE;     // Which knob on that page (0-7)
			
			int pageOffset = page * 16;                  // Page 0=0, Page 1=16
			int midiCC = baseCCForSetup + pageOffset + knobOnPage;
			
			auto &kc = allKnobConfigs[s][i];
			//kc.name        = "Knob " + ofToString(knobOnPage + 1);
			kc.name        = "";
			kc.color       = 70;
			kc.midiChannel = midiChannel;
			kc.midiCC      = midiCC;
			kc.steps       = 0;
			kc.configured  = false;
		}
		
		// Reset switchConfigs[s] to sequential defaults (2 pages)
		for (int i = 0; i < TOTAL_SWITCHES; i++) {
			int page = i / NUM_SWITCHES_PER_PAGE;        // Which page (0-1)
			int switchOnPage = i % NUM_SWITCHES_PER_PAGE; // Which switch on that page (0-7)
			
			int pageOffset = page * 16;                  // Page 0=0, Page 1=16
			int midiCC = baseCCForSetup + pageOffset + 8 + switchOnPage;
			
			auto &sc = allSwitchConfigs[s][i];
			//sc.name        = "Switch " + ofToString(switchOnPage + 1);
			sc.name        = "";
			sc.momentary   = true;
			sc.color       = 70;
			sc.midiChannel = midiChannel;
			sc.midiCC      = midiCC;
			sc.configured  = false;
		}
	}
	
	// 2) Load each saved setup from json["allSetups"]
	if (json.contains("allSetups")) {
		ofJson setupsJson = json["allSetups"];
		for (auto &oneSetupJson : setupsJson) {
			int idx = oneSetupJson["index"];
			if (idx < 0 || idx >= MAX_SETUPS) continue;
			
			availableSetups[idx].exists = oneSetupJson["exists"].get<bool>();
			availableSetups[idx].name   = oneSetupJson["name"].get<string>();
			
			// Load knobConfigs for this slot
			if (oneSetupJson.contains("knobConfigs")) {
				ofJson knobArray = oneSetupJson["knobConfigs"];
				for (int i = 0; i < TOTAL_KNOBS && i < knobArray.size(); i++) {
					ofJson &cfg = knobArray[i];
					auto &kc = allKnobConfigs[idx][i];
					if (cfg.contains("name"))        kc.name        = cfg["name"].get<string>();
					if (cfg.contains("color"))       kc.color       = cfg["color"].get<int>();
					if (cfg.contains("midiChannel")) kc.midiChannel = cfg["midiChannel"].get<int>();
					if (cfg.contains("midiCC"))      kc.midiCC      = cfg["midiCC"].get<int>();
					if (cfg.contains("steps"))       kc.steps       = cfg["steps"].get<int>();
					if (cfg.contains("configured"))  kc.configured  = cfg["configured"].get<bool>();
				}
			}
			
			// Load switchConfigs for this slot
			if (oneSetupJson.contains("switchConfigs")) {
				ofJson switchArray = oneSetupJson["switchConfigs"];
				for (int i = 0; i < TOTAL_SWITCHES && i < switchArray.size(); i++) {
					ofJson &cfg = switchArray[i];
					auto &sc = allSwitchConfigs[idx][i];
					if (cfg.contains("name"))        sc.name        = cfg["name"].get<string>();
					if (cfg.contains("momentary"))   sc.momentary   = cfg["momentary"].get<bool>();
					if (cfg.contains("color"))       sc.color       = cfg["color"].get<int>();
					if (cfg.contains("midiChannel")) sc.midiChannel = cfg["midiChannel"].get<int>();
					if (cfg.contains("midiCC"))      sc.midiCC      = cfg["midiCC"].get<int>();
					if (cfg.contains("configured"))  sc.configured  = cfg["configured"].get<bool>();
				}
			}
		}
	}
	
	// 3) Restore which setup slot was active
	if (json.contains("selectedSetupIndex")) {
		int loadedIndex = json["selectedSetupIndex"];
		if (loadedIndex >= 0 && loadedIndex < MAX_SETUPS) {
			selectedSetupIndex = loadedIndex;
			setupName = availableSetups[loadedIndex].name;
		}
	}
	
	// 4) Update the UI immediately - NO BLOCKING OPERATIONS HERE
	updateSelectedKnobParameters();
	updateSelectedSwitchParameters();
	setHardwarePage(selectedPage.get());
	
	// 5) REMOVED ALL BLOCKING SERIAL OPERATIONS
	// The threaded hardware configuration will handle this instead
	// No more blocking calls to:
	// - applyKnobConfiguration()
	// - applySwitchConfiguration()
	// - getCurrentSetup()
	// - refreshAvailableSetups()
}


void rotoControlConfig::presetSave(ofJson &json) {
	// ─────────────────────────────────────────────────────────────────────────
	// 1) Save setup management info
	ofJson setupListJson = ofJson::array();
	for (int s = 0; s < MAX_SETUPS; ++s) {
		if (!availableSetups[s].exists) continue; // skip unused slots
		
		ofJson oneSetupJson;
		oneSetupJson["index"]  = availableSetups[s].index;
		oneSetupJson["name"]   = availableSetups[s].name;
		oneSetupJson["exists"] = availableSetups[s].exists;
		
		// 2) Save this setup’s knobConfigs (32 entries)
		ofJson knobArrayJson = ofJson::array();
		for (int i = 0; i < TOTAL_KNOBS; i++) {
			ofJson cfg;
			KnobConfig &kc = allKnobConfigs[s][i];
			cfg["name"]        = kc.name;
			cfg["color"]       = kc.color;
			cfg["midiChannel"] = kc.midiChannel;
			cfg["midiCC"]      = kc.midiCC;
			cfg["steps"]       = kc.steps;
			cfg["configured"]  = kc.configured;
			knobArrayJson.push_back(cfg);
		}
		oneSetupJson["knobConfigs"] = knobArrayJson;
		
		// 3) Save this setup’s switchConfigs (32 entries)
		ofJson switchArrayJson = ofJson::array();
		for (int i = 0; i < TOTAL_SWITCHES; i++) {
			ofJson cfg;
			SwitchConfig &sc = allSwitchConfigs[s][i];
			cfg["name"]        = sc.name;
			cfg["momentary"]   = sc.momentary;
			cfg["color"]       = sc.color;
			cfg["midiChannel"] = sc.midiChannel;
			cfg["midiCC"]      = sc.midiCC;
			cfg["configured"]  = sc.configured;
			switchArrayJson.push_back(cfg);
		}
		oneSetupJson["switchConfigs"] = switchArrayJson;
		
		setupListJson.push_back(oneSetupJson);
	}
	json["allSetups"] = setupListJson;
	
	// 4) Save which setup is currently selected
	json["selectedSetupIndex"] = selectedSetupIndex.get();
}

void rotoControlConfig::setContainer(ofxOceanodeContainer* container) {
	// Call parent implementation first
	ofxOceanodeNodeModel::setContainer(container);
	
	// Store reference for our sync functionality
	containerRef = container;
}

void rotoControlConfig::syncBoundParametersToHardware() {
	if (!containerRef) {
		ofLogWarning("rotoControlConfig") << "No container reference - cannot sync parameters";
		return;
	}
	
	ofLogNotice("rotoControlConfig") << "Syncing bound parameters to ROTO hardware...";
	
	int currentSetup = selectedSetupIndex.get();
	int syncCount = 0;
	
	// Sync all configured knobs on current setup
	for (int i = 0; i < TOTAL_KNOBS; i++) {
		if (!allKnobConfigs[currentSetup][i].configured) continue;
		
		int midiChannel = allKnobConfigs[currentSetup][i].midiChannel;
		int midiCC = allKnobConfigs[currentSetup][i].midiCC;
		
		// Find corresponding binding
		auto binding = findBindingForMidiMapping(midiChannel, midiCC);
		if (binding) {
			forceParameterResend(binding);
			syncCount++;
			
			ofLogVerbose("rotoControlConfig")
				<< "Synced knob " << (i % NUM_KNOBS_PER_PAGE + 1)
				<< " on page " << (i / NUM_KNOBS_PER_PAGE)
				<< " (Ch" << midiChannel << " CC" << midiCC << ")";
		}
	}
	
	// Sync all configured switches on current setup
	for (int i = 0; i < TOTAL_SWITCHES; i++) {
		if (!allSwitchConfigs[currentSetup][i].configured) continue;
		
		int midiChannel = allSwitchConfigs[currentSetup][i].midiChannel;
		int midiCC = allSwitchConfigs[currentSetup][i].midiCC;
		
		// Find corresponding binding
		auto binding = findBindingForMidiMapping(midiChannel, midiCC);
		if (binding) {
			forceParameterResend(binding);
			syncCount++;
			
			ofLogVerbose("rotoControlConfig")
				<< "Synced switch " << (i % NUM_SWITCHES_PER_PAGE + 1)
				<< " on page " << (i / NUM_SWITCHES_PER_PAGE)
				<< " (Ch" << midiChannel << " CC" << midiCC << ")";
		}
	}
	
	ofLogNotice("rotoControlConfig")
		<< "Sync complete - " << syncCount << " parameters synced";
}

shared_ptr<ofxOceanodeAbstractMidiBinding> rotoControlConfig::findBindingForMidiMapping(int channel, int cc) {
	if (!containerRef) return nullptr;
	
	// Get the ROTO-Control's MIDI port name for filtering
	string rotoPortName = getRotoMidiPortName();
	if (rotoPortName.empty()) {
		ofLogWarning("rotoControlConfig") << "Cannot sync - ROTO MIDI port name not found";
		return nullptr;
	}
	
	// Search current preset bindings
	auto& currentBindings = containerRef->getMidiBindings();
	for (auto& bindingPair : currentBindings) {
		for (auto& binding : bindingPair.second) {
			// Check if this binding matches our ROTO device, channel, and CC
			if (binding->getPortName() == rotoPortName &&
				binding->getChannel().get() == channel &&
				binding->getControl().get() == cc) {
				return binding;
			}
		}
	}
	
	// Search persistent bindings
	auto& persistentBindings = containerRef->getPersistentMidiBindings();
	for (auto& bindingPair : persistentBindings) {
		for (auto& binding : bindingPair.second) {
			// Check if this binding matches our ROTO device, channel, and CC
			if (binding->getPortName() == rotoPortName &&
				binding->getChannel().get() == channel &&
				binding->getControl().get() == cc) {
				return binding;
			}
		}
	}
	
	return nullptr;
}

void rotoControlConfig::forceParameterResend(shared_ptr<ofxOceanodeAbstractMidiBinding> binding) {
	if (!binding) return;
	
	// Force parameter to resend its current value by triggering its change event
	// We do this by casting to the specific binding type and accessing the parameter
	
	if (binding->type() == typeid(ofxOceanodeMidiBinding<float>).name()) {
		auto floatBinding = dynamic_pointer_cast<ofxOceanodeMidiBinding<float>>(binding);
		if (floatBinding) {
			auto& param = floatBinding->getMinParameter(); // This gets us access to the binding's parameter structure
			// Get the actual parameter through the binding name and container
			string bindingName = binding->getName();
			vector<string> nameParts = ofSplitString(bindingName, "-|-");
			if (nameParts.size() >= 2) {
				string moduleName = nameParts[0];
				string paramName = ofSplitString(nameParts[1], " ")[0]; // Remove the _id suffix
				
				// Force resend by accessing parameter through container's node system
				forceParameterResendByName(moduleName, paramName);
			}
		}
	}
	else if (binding->type() == typeid(ofxOceanodeMidiBinding<int>).name()) {
		auto intBinding = dynamic_pointer_cast<ofxOceanodeMidiBinding<int>>(binding);
		if (intBinding) {
			string bindingName = binding->getName();
			vector<string> nameParts = ofSplitString(bindingName, "-|-");
			if (nameParts.size() >= 2) {
				string moduleName = nameParts[0];
				string paramName = ofSplitString(nameParts[1], " ")[0];
				forceParameterResendByName(moduleName, paramName);
			}
		}
	}
	else if (binding->type() == typeid(ofxOceanodeMidiBinding<bool>).name()) {
		auto boolBinding = dynamic_pointer_cast<ofxOceanodeMidiBinding<bool>>(binding);
		if (boolBinding) {
			string bindingName = binding->getName();
			vector<string> nameParts = ofSplitString(bindingName, "-|-");
			if (nameParts.size() >= 2) {
				string moduleName = nameParts[0];
				string paramName = ofSplitString(nameParts[1], " ")[0];
				forceParameterResendByName(moduleName, paramName);
			}
		}
	}
	else if (binding->type() == typeid(ofxOceanodeMidiBinding<vector<float>>).name()) {
		auto vecFloatBinding = dynamic_pointer_cast<ofxOceanodeMidiBinding<vector<float>>>(binding);
		if (vecFloatBinding) {
			string bindingName = binding->getName();
			vector<string> nameParts = ofSplitString(bindingName, "-|-");
			if (nameParts.size() >= 2) {
				string moduleName = nameParts[0];
				string paramName = ofSplitString(nameParts[1], " ")[0];
				forceParameterResendByName(moduleName, paramName);
			}
		}
	}
	else if (binding->type() == typeid(ofxOceanodeMidiBinding<vector<int>>).name()) {
		auto vecIntBinding = dynamic_pointer_cast<ofxOceanodeMidiBinding<vector<int>>>(binding);
		if (vecIntBinding) {
			string bindingName = binding->getName();
			vector<string> nameParts = ofSplitString(bindingName, "-|-");
			if (nameParts.size() >= 2) {
				string moduleName = nameParts[0];
				string paramName = ofSplitString(nameParts[1], " ")[0];
				forceParameterResendByName(moduleName, paramName);
			}
		}
	}
	else if (binding->type() == typeid(ofxOceanodeMidiBinding<void>).name()) {
		// Void parameters don't need syncing as they're triggers
		ofLogVerbose("rotoControlConfig") << "Skipping void parameter (trigger type)";
	}
}

void rotoControlConfig::forceParameterResendByName(const string& moduleName, const string& paramName) {
	if (!containerRef) return;
	
	// Find the node by module name and force its parameter to resend
	auto allNodes = containerRef->getAllModules();
	for (auto node : allNodes) {
		if (node->getParameters().getEscapedName() == moduleName) {
			if (node->getParameters().contains(paramName)) {
				ofAbstractParameter& param = node->getParameters().get(paramName);
				
				// Force parameter to notify listeners by setting it to itself
				// This works for all parameter types through the base class interface
				ofxOceanodeAbstractParameter& oceanodeParam = static_cast<ofxOceanodeAbstractParameter&>(param);
				
				if (oceanodeParam.valueType() == typeid(float).name()) {
					auto& floatParam = oceanodeParam.cast<float>().getParameter();
					floatParam = floatParam.get(); // This triggers the listener
				}
				else if (oceanodeParam.valueType() == typeid(int).name()) {
					auto& intParam = oceanodeParam.cast<int>().getParameter();
					intParam = intParam.get();
				}
				else if (oceanodeParam.valueType() == typeid(bool).name()) {
					auto& boolParam = oceanodeParam.cast<bool>().getParameter();
					boolParam = boolParam.get();
				}
				else if (oceanodeParam.valueType() == typeid(vector<float>).name()) {
					auto& vecFloatParam = oceanodeParam.cast<vector<float>>().getParameter();
					vecFloatParam = vecFloatParam.get();
				}
				else if (oceanodeParam.valueType() == typeid(vector<int>).name()) {
					auto& vecIntParam = oceanodeParam.cast<vector<int>>().getParameter();
					vecIntParam = vecIntParam.get();
				}
				
				ofLogVerbose("rotoControlConfig")
					<< "Forced resend for parameter: " << moduleName << "/" << paramName;
				return;
			}
		}
	}
	
	ofLogVerbose("rotoControlConfig")
		<< "Could not find parameter to resend: " << moduleName << "/" << paramName;
}

string rotoControlConfig::getRotoMidiPortName() const {
	// The ROTO-Control MIDI device appears as "Roto-Control" in macOS
	return "Roto-Control";
}

void rotoControlConfig::saveConfigurationFile() {
	// Open save dialog
	ofFileDialogResult result = ofSystemSaveDialog("roto_config.json", "Save ROTO-Control Configuration");
	
	if (!result.bSuccess) {
		ofLogNotice("rotoControlConfig") << "Save cancelled by user";
		return;
	}
	
	string filepath = result.getPath();
	
	// Ensure .json extension
	if (filepath.find(".json") == string::npos) {
		filepath += ".json";
	}
	
	// Use the existing preset save logic to create JSON
	ofJson configJson;
	presetSave(configJson);
	
	// Add some metadata to identify this as a config file
	configJson["_rotoConfigMetadata"] = {
		{"configType", "rotoControlConfig"},
		{"version", "1.0"},
		{"savedAt", ofGetTimestampString()},
		{"description", "ROTO-Control Configuration File"},
		{"filename", ofFilePath::getFileName(filepath)}
	};
	
	// Save to file
	if (ofSavePrettyJson(filepath, configJson)) {
		ofLogNotice("rotoControlConfig") << "Configuration saved to: " << filepath;
		configStatus = "Saved: " + ofFilePath::getFileName(filepath);
	} else {
		ofLogError("rotoControlConfig") << "Failed to save configuration to: " << filepath;
		configStatus = "Save failed!";
	}
}

void rotoControlConfig::loadConfigurationFile() {
	// Check if already configuring
	if (isConfiguring.load()) {
		ofLogWarning("rotoControlConfig") << "Configuration already in progress, please wait...";
		configStatus = "Busy - please wait...";
		return;
	}
	
	// Get default config directory for dialog
	string defaultDir = getDefaultConfigDir();
	
	// Open load dialog
	ofFileDialogResult result = ofSystemLoadDialog("Load ROTO-Control Configuration", false, defaultDir);
	
	if (!result.bSuccess) {
		ofLogNotice("rotoControlConfig") << "Load cancelled by user";
		return;
	}
	
	string filepath = result.getPath();
	
	// Check if file exists
	if (!ofFile::doesFileExist(filepath)) {
		ofLogError("rotoControlConfig") << "Selected file does not exist: " << filepath;
		configStatus = "File not found!";
		return;
	}
	
	// Load JSON
	ofJson configJson = ofLoadJson(filepath);
	if (configJson.empty()) {
		ofLogError("rotoControlConfig") << "Failed to load or parse configuration file: " << filepath;
		configStatus = "Invalid file format!";
		return;
	}
	
	// Update status
	configStatus = "Loading configuration...";
	
	// Apply the configuration immediately (UI updates)
	presetRecallAfterSettingParameters(configJson);
	
	// Start threaded hardware configuration
	if (serialConnected) {
		// Wait for any existing thread to finish
		if (configThread.joinable()) {
			configThread.join();
		}
		
		isConfiguring = true;
		configThread = std::thread([this]() {
			this->threadedConfigurationApply();
		});
	} else {
		configStatus = "Loaded (no hardware)";
	}
	
	ofLogNotice("rotoControlConfig") << "Configuration loaded from: " << filepath;
}

// Replace threadedConfigurationApply() method:
void rotoControlConfig::threadedConfigurationApply() {
	try {
		applyConfigurationsToHardware();
		
		// Mark as complete
		isConfiguring = false;
		
		// Update status on main thread
		configStatus = "Configuration applied";
		
		ofLogNotice("rotoControlConfig") << "Threaded configuration apply completed successfully";
	}
	catch (const std::exception& e) {
		ofLogError("rotoControlConfig") << "Error in threaded configuration: " << e.what();
		isConfiguring = false;
		configStatus = "Configuration failed!";
	}
}

void rotoControlConfig::applyConfigurationsToHardware() {
	if (!serialConnected) {
		ofLogWarning("rotoControlConfig") << "Cannot apply configurations - serial not connected";
		configStatus = "No hardware connection";
		return;
	}
	
	int currentSetup = selectedSetupIndex.get();
	int configurationsApplied = 0;
	int setupsToProcess = 0;
	
	// First, count how many setups need processing
	for (int s = 0; s < MAX_SETUPS; s++) {
		if (shouldSendSetupToHardware(s)) {
			setupsToProcess++;
		}
	}
	
	ofLogNotice("rotoControlConfig") << "Applying configurations to hardware... "
									<< setupsToProcess << " setups to process";
	
	if (setupsToProcess == 0) {
		configStatus = "No configurations to apply";
		return;
	}
	
	int processedSetups = 0;
	
	// Only configure setups that have actual data
	for (int s = 0; s < MAX_SETUPS; s++) {
		// Check if thread should stop
		if (!isConfiguring.load()) {
			ofLogNotice("rotoControlConfig") << "Configuration cancelled by user";
			return;
		}
		
		if (!shouldSendSetupToHardware(s)) continue;
		
		processedSetups++;
		
		// Update status
		configStatus = "Configuring setup " + ofToString(s) + " (" +
					  ofToString(processedSetups) + "/" + ofToString(setupsToProcess) + ")";
		
		ofLogNotice("rotoControlConfig") << "Processing setup " << s << " (" << processedSetups << "/" << setupsToProcess << ")";
		
		// Start config update session for this setup - USE FAST VERSION
		sendSerialCommand(CMD_GENERAL, CMD_START_CONFIG_UPDATE);
		ofSleepMillis(15); // Reduced delay
		
		// Apply knob configurations for this setup
		for (int i = 0; i < TOTAL_KNOBS; i++) {
			if (!isConfiguring.load()) return; // Check for cancellation
			
			if (allKnobConfigs[s][i].configured) {
				// Temporarily switch to this setup for configuration
				int originalSetup = selectedSetupIndex.get();
				{
					std::lock_guard<std::mutex> lock(configMutex);
					selectedSetupIndex = s;
				}
				
				applyKnobConfiguration(i); // Use fast version
				configurationsApplied++;
				
				// Restore original setup
				{
					std::lock_guard<std::mutex> lock(configMutex);
					selectedSetupIndex = originalSetup;
				}
				
				ofSleepMillis(10); // Further reduced delay
			}
		}
		
		// Apply switch configurations for this setup
		for (int i = 0; i < TOTAL_SWITCHES; i++) {
			if (!isConfiguring.load()) return; // Check for cancellation
			
			if (allSwitchConfigs[s][i].configured) {
				// Temporarily switch to this setup for configuration
				int originalSetup = selectedSetupIndex.get();
				{
					std::lock_guard<std::mutex> lock(configMutex);
					selectedSetupIndex = s;
				}
				
				applySwitchConfiguration(i); // Use fast version
				configurationsApplied++;
				
				// Restore original setup
				{
					std::lock_guard<std::mutex> lock(configMutex);
					selectedSetupIndex = originalSetup;
				}
				
				ofSleepMillis(10); // Further reduced delay
			}
		}
		
		// End config update session for this setup - USE FAST VERSION
		sendSerialCommand(CMD_GENERAL, CMD_END_CONFIG_UPDATE);
		ofSleepMillis(15); // Reduced delay
		
		ofLogNotice("rotoControlConfig") << "Completed setup " << s;
	}
	
	if (!isConfiguring.load()) return; // Final cancellation check
	
	// Switch back to the selected setup
	setCurrentSetup(currentSetup);
	ofSleepMillis(30); // Reduced delay
	
	// Update hardware page
	setHardwarePage(selectedPage.get());
	ofSleepMillis(30); // Reduced delay
	
	// Only sync bound parameters if retrigger option is enabled
		if (retriggerMidiBounds.get()) {
			syncBoundParametersToHardware();
		}
	
	ofLogNotice("rotoControlConfig") << "Hardware configuration complete. Applied "
									<< configurationsApplied << " configurations across "
									<< processedSetups << " setups.";
}

bool rotoControlConfig::shouldSendSetupToHardware(int setupIndex) {
	if (setupIndex < 0 || setupIndex >= MAX_SETUPS) return false;
	
	bool hasConfiguredControls = false;
	
	// Check if any knobs are configured for this setup
	for (int i = 0; i < TOTAL_KNOBS; i++) {
		if (allKnobConfigs[setupIndex][i].configured) {
			hasConfiguredControls = true;
			break;
		}
	}
	
	// Check if any switches are configured for this setup (only if no knobs found)
	if (!hasConfiguredControls) {
		for (int i = 0; i < TOTAL_SWITCHES; i++) {
			if (allSwitchConfigs[setupIndex][i].configured) {
				hasConfiguredControls = true;
				break;
			}
		}
	}
	
	if (hasConfiguredControls) {
		ofLogVerbose("rotoControlConfig") << "Setup " << setupIndex << " has configured controls - will send to hardware";
	}
	
	return hasConfiguredControls;
}

string rotoControlConfig::getDefaultConfigDir() {
	string configDir = ofToDataPath("roto_configurations", true);
	
	ofDirectory dir(configDir);
	if (!dir.exists()) {
		dir.create(true);
	}
	
	return configDir;
}
