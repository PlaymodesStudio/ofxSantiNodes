#ifndef scalaTuning_h
#define scalaTuning_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <regex>

class scalaTuning : public ofxOceanodeNodeModel {
public:
	scalaTuning() : ofxOceanodeNodeModel("Scala Tuning") {}

	void setup() override {
		description = "Converts MIDI notes to microtonal frequencies using Scala (.scl) tuning files or custom ratios. Supports both direct (scale degree based) and modular (12-TET based) mapping modes.";
		
		// Add input parameters
		addParameter(midiNotes.set("MIDI In", {60}, {0}, {127}));
		addParameter(rootNote.set("Root", 60, 0, 127));
		addParameter(referenceFreq.set("Reference Hz", 440.0f, 220.0f, 880.0f));
		addParameter(useModularMapping.set("Modular", true));
		addParameter(useCustomRatios.set("Custom", false));
		addParameter(customRatios.set("Ratios", {1.0f, 1.059463f, 1.122462f, 1.189207f, 1.259921f, 1.334840f, 1.414214f, 1.498307f, 1.587401f, 1.681793f, 1.781797f, 1.887749f, 2.0f}, {0.001f}, {100.0f}));
		addParameter(bypass.set("Bypass", false));
		
		// Output parameters
		addOutputParameter(frequencies.set("Frequencies", {440.0f}, {0.0f}, {20000.0f}));
		addOutputParameter(pitches.set("Pitches", {60.0f}, {0.0f}, {127.0f}));
		
		// Description text display settings
		addInspectorParameter(descBoxWidth.set("Desc Width", 240, 240, 800));
		addInspectorParameter(descBoxHeight.set("Desc Height", 300, 50, 300));
		addInspectorParameter(descFontSize.set("Desc Font", 14, 8, 24));
		
		// Visualization settings - only keep octaves control
		addInspectorParameter(showVisualization.set("Show Viz", true));
		addInspectorParameter(numOctaves.set("Octaves", 2, 1, 4));
		
		// Add scale description as internal state (not directly displayed)
		scaleDescriptionText = "No scale loaded yet";
		
		// Create custom region for text display
		addCustomRegion(descriptionRegion.set("Description", [this](){
			drawDescriptionBox();
		}), [this](){
			drawDescriptionBox();
		});
		
		// Create custom region for scale visualization
		addCustomRegion(visualizationRegion.set("Scale Visualization", [this](){
			if(showVisualization) {
				drawScaleVisualization();
			}
		}), [this](){
			if(showVisualization) {
				drawScaleVisualization();
			}
		});
		
		// Scan for .scl files and populate the dropdown
		scanScalaFiles();
		
		// Create dropdown for scale selection
		vector<string> scaleOptions;
		for (const auto& scale : scaleFiles) {
			scaleOptions.push_back(scale.first);
		}
		
		if (!scaleOptions.empty()) {
			// Sort scale options alphabetically for easier navigation
			std::sort(scaleOptions.begin(), scaleOptions.end());
			
			addParameterDropdown(selectedScale, "Scale", 0, scaleOptions);
			
			// Load the first scale by default
			std::string firstScaleName = scaleOptions[0];
			ofLogNotice("scalaTuning") << "Loading default scale: " << firstScaleName;
			loadScaleFile(scaleFiles[firstScaleName]);
			updateOutput();  // Initialize output with the default scale
		} else {
			ofLogWarning("scalaTuning") << "No .scl files found in the directory!";
			addParameter(selectedScale.set("Scale", 0, 0, 0));
		}
		
		// Add listeners for all parameters
		listeners.push(selectedScale.newListener([this](int &val){
			if (!useCustomRatios) {
				vector<string> keys;
				for (const auto& pair : scaleFiles) {
					keys.push_back(pair.first);
				}
				
				// Sort the keys to match the dropdown order
				std::sort(keys.begin(), keys.end());
				
				if (val >= 0 && val < keys.size()) {
					string selectedScaleName = keys[val];
					ofLogNotice("scalaTuning") << "Loading scale: " << selectedScaleName;
					loadScaleFile(scaleFiles[selectedScaleName]);
					updateOutput();
				}
			}
		}));
		
		listeners.push(midiNotes.newListener([this](vector<int> &){
			updateOutput();
		}));
		
		listeners.push(rootNote.newListener([this](int &){
			updateOutput();
		}));
		
		listeners.push(referenceFreq.newListener([this](float &){
			updateOutput();
		}));
		
		listeners.push(useModularMapping.newListener([this](bool &){
			updateOutput();
		}));
		
		listeners.push(useCustomRatios.newListener([this](bool &){
			updateCurrentScale();
			updateOutput();
		}));
		
		listeners.push(customRatios.newListener([this](vector<float> &){
			if (useCustomRatios) {
				updateCurrentScale();
				updateOutput();
			}
		}));
		
		listeners.push(bypass.newListener([this](bool &){
			updateOutput();
		}));
		
		// Force an initial update of the output
		updateOutput();
	}
	
	// Update the current scale based on mode (custom or scala)
	void updateCurrentScale() {
		if (useCustomRatios) {
			// Use custom ratios
			vector<float> ratios = customRatios.get();
			
			// Use the custom ratios directly
			scaleRatios = ratios;
			
			// Create a description for custom ratios
			string customDesc = "Custom Ratios: " + ofToString(ratios.size()) + " values\n\n";
			int maxRatiosToShow = std::min((int)ratios.size(), 10);
			for (int i = 0; i < maxRatiosToShow; i++) {
				customDesc += "Ratio " + ofToString(i) + ": " + ofToString(ratios[i], 6) + "\n";
			}
			
			if (ratios.size() > maxRatiosToShow) {
				customDesc += "...\n";
			}
			
			// Compare with 12-TET
			customDesc += "\nComparison with 12-TET:\n";
			vector<float> tetRatios = {1.0f, 1.059463f, 1.122462f, 1.189207f, 1.259921f, 1.334840f,
				1.414214f, 1.498307f, 1.587401f, 1.681793f, 1.781797f, 1.887749f, 2.0f};
			
			if (ratios.size() <= tetRatios.size()) {
				for (int i = 0; i < std::min((int)ratios.size(), 13); i++) {
					if (i < tetRatios.size()) {
						float centsDiff = 1200.0f * log2(ratios[i] / tetRatios[i]);
						customDesc += "Step " + ofToString(i) + ": " + ofToString(centsDiff, 2) + " cents\n";
					}
				}
			} else {
				customDesc += "Custom scale has more than 12 steps per octave\n";
			}
			
			scaleDescriptionText = customDesc;
			ofLogNotice("scalaTuning") << "Updated to custom ratios with " << ratios.size() << " values";
			
		} else {
			// Use the selected Scala file
			if (!scaleFiles.empty()) {
				vector<string> keys;
				for (const auto& pair : scaleFiles) {
					keys.push_back(pair.first);
				}
				
				// Sort the keys to match the dropdown order
				std::sort(keys.begin(), keys.end());
				
				int val = selectedScale;
				if (val >= 0 && val < keys.size()) {
					string selectedScaleName = keys[val];
					loadScaleFile(scaleFiles[selectedScaleName]);
				}
			}
		}
	}
	
	// Draw description box using ImGui
	void drawDescriptionBox() {
		// Set font size for this widget
		float fontScale = descFontSize/14.0f; // 14 is ImGui's default font size
		ImGui::SetWindowFontScale(fontScale);
		
		// Create child window for scrolling
		ImGui::BeginChild("ScalaDescription", ImVec2(descBoxWidth, descBoxHeight), true,
						 ImGuiWindowFlags_HorizontalScrollbar);
		
		// Display the text
		ImGui::TextWrapped("%s", scaleDescriptionText.c_str());
		
		ImGui::EndChild();
		
		// Reset font scale to default
		ImGui::SetWindowFontScale(1.0f);
	}
	
	// Draw scale pitch visualization
	void drawScaleVisualization() {
		if ((scaleRatios.empty() || (!useCustomRatios && !scaleFiles.size())) && !bypass) {
			ImGui::Text("No scale loaded");
			return;
		}
		
		// Fixed dimensions
		const float width = 240.0f;
		const float height = 50.0f;
		
		// Set up drawing canvas with fixed size
		ImGui::BeginChild("ScaleVisualization", ImVec2(width, height), true, ImGuiWindowFlags_NoScrollbar);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		// Get the canvas position
		ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		
		// Calculate dimensions
		float padding = 1.0f; // Minimal padding
		float octaveWidth = (width - 2 * padding) / numOctaves;
		float noteWidth = octaveWidth / 12.0f;
		
		// If in bypass mode, don't show scale pitches
		if (bypass) {
			// Draw black background
			drawList->AddRectFilled(
				canvasPos,
				ImVec2(canvasPos.x + width, canvasPos.y + height),
				IM_COL32(0, 0, 0, 255)
			);
			
			// Just draw 12-TET reference lines (all in bright red to indicate bypass)
			for (int oct = 0; oct < numOctaves; oct++) {
				for (int i = 0; i < 12; i++) {
					float x = canvasPos.x + padding + oct * octaveWidth + i * noteWidth;
					
					// Round to nearest pixel for crisp lines
					float xRounded = round(x);
					
					// Vertical line - full height, 1px crisp
					drawList->AddLine(
						ImVec2(xRounded, canvasPos.y),
						ImVec2(xRounded, canvasPos.y + height),
						IM_COL32(255, 50, 50, 255),
						1.0f
					);
				}
			}
			
			// Draw 12-TET note for octave boundary
			float endX = canvasPos.x + padding + numOctaves * octaveWidth;
			float endXRounded = round(endX);
			drawList->AddLine(
				ImVec2(endXRounded, canvasPos.y),
				ImVec2(endXRounded, canvasPos.y + height),
				IM_COL32(255, 50, 50, 255),
				1.0f
			);
			
			ImGui::EndChild();
			return;
		}
		
		// Calculate scale notes from ratios
		std::vector<float> scalePitches;
		calculateScalePitches(scalePitches);
		
		// Draw black background
		drawList->AddRectFilled(
			canvasPos,
			ImVec2(canvasPos.x + width, canvasPos.y + height),
			IM_COL32(0, 0, 0, 255)
		);
		
		// Draw 12-TET reference lines (grey) - all the way from top to bottom with no labels
		for (int oct = 0; oct < numOctaves; oct++) {
			for (int i = 0; i < 12; i++) {
				float x = canvasPos.x + padding + oct * octaveWidth + i * noteWidth;
				
				// Round to nearest pixel for crisp lines
				float xRounded = round(x);
				
				// Vertical line - full height, 1px crisp
				drawList->AddLine(
					ImVec2(xRounded, canvasPos.y),
					ImVec2(xRounded, canvasPos.y + height),
					IM_COL32(120, 120, 120, 255),
					1.0f
				);
			}
		}
		
		// Draw 12-TET note for octave boundary
		float endX = canvasPos.x + padding + numOctaves * octaveWidth;
		float endXRounded = round(endX);
		drawList->AddLine(
			ImVec2(endXRounded, canvasPos.y),
			ImVec2(endXRounded, canvasPos.y + height),
			IM_COL32(120, 120, 120, 255),
			1.0f
		);
		
		// Draw scale pitches (red) - full height with no labels
		for (float pitch : scalePitches) {
			int octave = floor(pitch / 12);
			float x = canvasPos.x + padding + (octave - 4) * octaveWidth + (pitch - octave * 12) * noteWidth;
			
			// Round to nearest pixel for crisp lines
			float xRounded = round(x);
			
			// Only draw if within the visible range
			if (xRounded >= canvasPos.x + padding && xRounded <= canvasPos.x + padding + numOctaves * octaveWidth) {
				drawList->AddLine(
					ImVec2(xRounded, canvasPos.y),
					ImVec2(xRounded, canvasPos.y + height),
					IM_COL32(255, 50, 50, 255),
					1.0f  // Crisp 1px line
				);
			}
		}
		
		ImGui::EndChild();
	}
	
	// Explicitly call the update on the output periodically
	void update(ofEventArgs &e) override {
		// Check if we need to update (e.g., if the MIDI notes changed but the output didn't)
		if (frequencies.get().size() != midiNotes.get().size()) {
			updateOutput();
		}
	}

private:
	// Input parameters
	ofParameter<vector<int>> midiNotes;
	ofParameter<int> rootNote;
	ofParameter<float> referenceFreq;
	ofParameter<bool> useModularMapping;
	ofParameter<bool> useCustomRatios;
	ofParameter<vector<float>> customRatios;
	ofParameter<bool> bypass;
	ofParameter<int> selectedScale;
	
	// Description display parameters
	ofParameter<float> descBoxWidth;
	ofParameter<float> descBoxHeight;
	ofParameter<float> descFontSize;
	string scaleDescriptionText;
	customGuiRegion descriptionRegion;
	
	// Visualization parameters
	ofParameter<bool> showVisualization;
	ofParameter<int> numOctaves;
	customGuiRegion visualizationRegion;
	
	// Output parameters
	ofParameter<vector<float>> frequencies;
	ofParameter<vector<float>> pitches;
	
	// Event listeners
	ofEventListeners listeners;
	
	// Scale data
	vector<float> scaleRatios;
	map<string, string> scaleFiles; // Map of scale name to file path
	
	void scanScalaFiles() {
		// Clear any existing entries
		scaleFiles.clear();
		
		// Paths to check for scala files (try multiple locations)
		vector<string> scalaPaths = {
			"Supercollider/scl/",
			"data/Supercollider/scl/",
			"scl/",
			"data/scl/"
		};
		
		bool foundAnyFiles = false;
		
		for (const auto& scalaPath : scalaPaths) {
			// Check if the directory exists
			ofDirectory dir(scalaPath);
			if (!dir.exists()) {
				ofLogNotice("scalaTuning") << "Directory not found: " << scalaPath;
				continue;
			}
			
			// List all .scl files
			dir.allowExt("scl");
			dir.listDir();
			
			for (int i = 0; i < dir.size(); i++) {
				string filePath = dir.getPath(i);
				string fileName = ofFilePath::getBaseName(filePath);
				scaleFiles[fileName] = filePath;
				foundAnyFiles = true;
			}
			
			if (foundAnyFiles) {
				ofLogNotice("scalaTuning") << "Found " << dir.size() << " scala files in " << scalaPath;
				break;
			}
		}
		
		if (scaleFiles.empty()) {
			ofLogWarning("scalaTuning") << "No .scl files found in any of the search directories!";
		} else {
			ofLogNotice("scalaTuning") << "Found a total of " << scaleFiles.size() << " scala files";
		}
	}
	
	bool loadScaleFile(const string& filePath) {
		// Clear previous scale data
		scaleRatios.clear();
		
		// Open and read the file content
		ofBuffer buffer = ofBufferFromFile(filePath);
		if (buffer.size() <= 0) {
			ofLogError("scalaTuning") << "Failed to load file: " << filePath;
			scaleDescriptionText = "Error: Failed to load file";
			return false;
		}
		
		// Read line by line to find description and notes
		string description = "";
		bool foundDescription = false;
		int numNotes = 0;
		int notesRead = 0;
		
		// First, parse the file to find the description
		for (auto line : buffer.getLines()) {
			// Skip empty lines
			if (line.empty()) continue;
			
			// Trim whitespace
			line = ofTrim(line);
			
			// Skip comment lines at first
			if (line[0] == '!') continue;
			
			// First non-comment line should be the description
			if (!foundDescription) {
				description = line;
				foundDescription = true;
				continue;
			}
			
			// Second non-comment line should be the number of notes
			if (numNotes == 0) {
				try {
					numNotes = stoi(ofTrim(line));
					break;
				} catch (std::exception& e) {
					ofLogError("scalaTuning") << "Invalid number of notes: " << line;
					return false;
				}
			}
		}
		
		if (!foundDescription) {
			ofLogWarning("scalaTuning") << "No description found in file";
			description = "No description";
		}
		
		if (numNotes <= 0) {
			ofLogError("scalaTuning") << "Invalid or missing note count";
			return false;
		}
		
		// Now parse the actual note values
		bool processingNotes = false;
		for (auto line : buffer.getLines()) {
			// Skip empty lines
			if (line.empty()) continue;
			
			// Trim whitespace
			line = ofTrim(line);
			
			// Skip comment lines
			if (line[0] == '!') continue;
			
			// Skip until we've seen the note count
			if (!processingNotes) {
				if (line == ofToString(numNotes)) {
					processingNotes = true;
				}
				continue;
			}
			
			// We're now processing note values
			if (notesRead < numNotes) {
				// Clean up the line (remove comments)
				size_t commentPos = line.find('!');
				if (commentPos != string::npos) {
					line = line.substr(0, commentPos);
				}
				line = ofTrim(line);
				
				float ratio = 0.0f;
				
				// Handle ratio notation (e.g. "3/2")
				if (line.find('/') != string::npos) {
					vector<string> parts = ofSplitString(line, "/");
					if (parts.size() == 2) {
						float num = ofToFloat(ofTrim(parts[0]));
						float denom = ofToFloat(ofTrim(parts[1]));
						if (denom != 0) {
							ratio = num / denom;
						}
					}
				}
				// Handle cents notation (e.g. "701.955")
				else {
					float cents = ofToFloat(line);
					ratio = pow(2.0f, cents / 1200.0f);
				}
				
				if (ratio > 0) {
					scaleRatios.push_back(ratio);
					notesRead++;
				}
			}
		}
		
		// Always add unison ratio if not present
		if (scaleRatios.empty() || scaleRatios[0] != 1.0f) {
			scaleRatios.insert(scaleRatios.begin(), 1.0f);
		}
		
		// Create a detailed scale description
		string fileName = ofFilePath::getBaseName(filePath);
		
		// Build a more detailed description for the text box
		string scaleDetails = fileName + "\n\n" + description + "\n\nScale contains "
			+ ofToString(scaleRatios.size()) + " ratios:\n\n";
			
		// Add the first few ratios to the description
		int maxRatiosToShow = std::min((int)scaleRatios.size(), 10);
		for (int i = 0; i < maxRatiosToShow; i++) {
			scaleDetails += "Ratio " + ofToString(i) + ": " + ofToString(scaleRatios[i], 6) + "\n";
		}
		
		if (scaleRatios.size() > maxRatiosToShow) {
			scaleDetails += "...\n";
		}
		
		// Add comparison with 12-TET
		scaleDetails += "\nComparison with 12-TET:\n";
		vector<float> tetRatios = {1.0f, 1.059463f, 1.122462f, 1.189207f, 1.259921f, 1.334840f,
			1.414214f, 1.498307f, 1.587401f, 1.681793f, 1.781797f, 1.887749f, 2.0f};
		
		if (scaleRatios.size() <= tetRatios.size()) {
			for (int i = 0; i < std::min((int)scaleRatios.size(), 13); i++) {
				if (i < tetRatios.size()) {
					float centsDiff = 1200.0f * log2(scaleRatios[i] / tetRatios[i]);
					scaleDetails += "Step " + ofToString(i) + ": " + ofToString(centsDiff, 2) + " cents\n";
				}
			}
		} else {
			scaleDetails += "Scale has more than 12 steps per octave\n";
		}
		
		// Set the description text
		scaleDescriptionText = scaleDetails;
		
		ofLogNotice("scalaTuning") << "Loaded scale with " << scaleRatios.size() << " ratios";
		return true;
	}
	
	// Calculate scale pitches in MIDI note values for visualization
	void calculateScalePitches(std::vector<float>& scalePitches) {
		if (scaleRatios.empty()) return;
		
		// Calculate scale pitches for visualization (across multiple octaves)
		int startOctave = 4;  // Start at middle C
		int endOctave = startOctave + numOctaves;
		
		// Calculate microtonal pitches - make sure to generate enough so we don't miss any
		int scaleSize = scaleRatios.size();
		float octaveRatio = scaleRatios.back(); // Usually 2/1 for most scales
		
		// Calculate all scale notes across a wider range to ensure we catch all
		for (int octave = startOctave - 2; octave <= endOctave + 2; octave++) {
			for (int degree = 0; degree < scaleSize; degree++) {
				// Calculate the MIDI pitch based on reference frequency
				// Standard formula: midiNote = 69 + 12 * log2(freq / 440)
				float freq = referenceFreq * pow(octaveRatio, octave - 4) * scaleRatios[degree];
				float midiPitch = 69.0f + 12.0f * log2(freq / 440.0f);
				
				// Only add if within the visible range
				if (midiPitch >= startOctave * 12 - 1 && midiPitch <= (endOctave + 1) * 12 + 1) {
					scalePitches.push_back(midiPitch);
				}
			}
		}
		
		// Sort the pitches for better visualization
		std::sort(scalePitches.begin(), scalePitches.end());
	}
	
	void updateOutput() {
		// Get input values
		vector<int> inputNotes = midiNotes.get();
		
		// Prepare output vectors
		vector<float> outputFreqs;
		vector<float> outputPitches;
		outputFreqs.reserve(inputNotes.size());
		outputPitches.reserve(inputNotes.size());
		
		// Check if in bypass mode
		if (bypass) {
			// In bypass mode, pass MIDI notes directly to both outputs
			for (int midiNote : inputNotes) {
				if (midiNote < 0 || midiNote > 127) {
					// Invalid note handling
					outputFreqs.push_back(0);
					outputPitches.push_back(0);
				} else {
					// Calculate standard 12-TET frequency (A4 = 440Hz)
					// Formula: freq = 440 * 2^((midiNote - 69) / 12)
					float frequency = 440.0f * pow(2.0f, (midiNote - 69.0f) / 12.0f);
					
					// In bypass mode, the pitch is exactly the MIDI note
					outputFreqs.push_back(frequency);
					outputPitches.push_back(midiNote);
				}
			}
			
			// Update the output parameters
			frequencies.set(outputFreqs);
			pitches.set(outputPitches);
			return;
		}
		
		// Handle custom ratio mode or scala file mode
		if (useCustomRatios) {
			// Make sure we have valid custom ratios
			vector<float> ratios = customRatios.get();
			if (ratios.empty()) {
				// If no custom ratios, fall back to 12-TET
				for (int midiNote : inputNotes) {
					if (midiNote < 0 || midiNote > 127) {
						// Invalid note handling
						outputFreqs.push_back(0);
						outputPitches.push_back(0);
					} else {
						// Calculate standard 12-TET frequency (A4 = 440Hz)
						float frequency = 440.0f * pow(2.0f, (midiNote - 69.0f) / 12.0f);
						outputFreqs.push_back(frequency);
						outputPitches.push_back(midiNote);
					}
				}
				
				// Update the output parameters
				frequencies.set(outputFreqs);
				pitches.set(outputPitches);
				return;
			}
			
			// Use the custom ratios
			scaleRatios = ratios;
		}
		
		// Not in bypass mode, proceed with normal tuning calculation
		if (scaleRatios.empty()) {
			ofLogNotice("scalaTuning") << "No scale loaded or empty scale";
			
			// Output standard 12-TET frequencies as fallback
			for (int midiNote : inputNotes) {
				if (midiNote < 0 || midiNote > 127) {
					// Invalid note handling
					outputFreqs.push_back(0);
					outputPitches.push_back(0);
				} else {
					// Calculate standard 12-TET frequency (A4 = 440Hz)
					float frequency = 440.0f * pow(2.0f, (midiNote - 69.0f) / 12.0f);
					outputFreqs.push_back(frequency);
					outputPitches.push_back(midiNote);
				}
			}
			
			// Update the output parameters
			frequencies.set(outputFreqs);
			pitches.set(outputPitches);
			return;
		}
		
		// Regular scale tuning calculation
		int root = rootNote.get();
		float refFreq = referenceFreq.get();
		bool modularMode = useModularMapping.get();
		
		// Scale properties
		int scaleSize = scaleRatios.size();
		float octaveRatio = scaleRatios.back(); // Usually 2/1 for most scales
		
		// Process each MIDI note
		for (int midiNote : inputNotes) {
			// Skip invalid notes
			if (midiNote < 0 || midiNote > 127) {
				outputFreqs.push_back(0);
				outputPitches.push_back(0);
				continue;
			}
			
			// Calculate note offset from root
			int noteOffset = midiNote - root;
			
			int octaves = 0;
			int scaleDegree = 0;
			
			if (modularMode) {
				// Modular mapping (12-TET to scale degrees)
				octaves = noteOffset / 12;
				int semitones = noteOffset % 12;
				
				// Handle negative notes
				if (semitones < 0) {
					semitones += 12;
					octaves--;
				}
				
				// Map 12 semitones proportionally to scale degrees
				scaleDegree = static_cast<int>(round((semitones * (scaleSize - 1)) / 12.0));
			} else {
				// Direct mapping (note offset maps directly to scale degrees)
				octaves = noteOffset / scaleSize;
				scaleDegree = noteOffset % scaleSize;
				
				// Handle negative notes
				if (scaleDegree < 0) {
					scaleDegree += scaleSize;
					octaves--;
				}
			}
			
			// Keep scale degree in valid range
			scaleDegree = std::max(0, std::min(scaleSize - 1, scaleDegree));
			
			// Calculate the frequency
			float frequency = refFreq * pow(octaveRatio, octaves) * scaleRatios[scaleDegree];
			
			// Calculate MIDI pitch (with cents)
			// Standard formula: midiNote = 69 + 12 * log2(freq / 440)
			float midiPitch = 69.0f + 12.0f * log2(frequency / 440.0f);
			
			// Add to outputs
			outputFreqs.push_back(frequency);
			outputPitches.push_back(midiPitch);
		}
		
		// Update the output parameters
		frequencies.set(outputFreqs);
		pitches.set(outputPitches);
	}
};

#endif /* scalaTuning_h */
