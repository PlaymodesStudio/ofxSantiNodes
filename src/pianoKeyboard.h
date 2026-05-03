#ifndef pianoKeyboard_h
#define pianoKeyboard_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <set>

class pianoKeyboard : public ofxOceanodeNodeModel {
public:
	pianoKeyboard() : ofxOceanodeNodeModel("Piano Keyboard"),
		width(400),    // Initialize with default values
		height(100),
		loNote(48),
		hiNote(72)
	{}
	
	void setup() {
		description = "Displays a piano keyboard that highlights keys based on pitch and gate inputs. Click keys to create chords.";

		// Add all parameters before creating GUI
		addParameter(pitch.set("Pitch[]", {60}, {0}, {127}));
		addParameter(gate.set("Gate[]", {0}, {0}, {1}));
		addParameter(width.set("Width", 400, 100, 1000));
		addParameter(height.set("Height", 100, 50, 300));
		addParameter(loNote.set("Lo Note", 48, 0, 127));
		addParameter(hiNote.set("Hi Note", 72, 0, 127));
		addParameter(showWindow.set("Show", false));

		// Make sure outputNotes is properly configured to be saved with presets
		// Don't use the DisableSavePreset flag for this parameter
		addOutputParameter(outputNotes.set("Output", {0}, {0}, {127}));
		
		// Add listeners
		listeners.push(width.newListener([this](int &){
			updateKeyboardGeometry();
		}));
		
		listeners.push(height.newListener([this](int &){
			updateKeyboardGeometry();
		}));
		
		listeners.push(loNote.newListener([this](int &){
			updateKeyboardGeometry();
		}));
		
		listeners.push(hiNote.newListener([this](int &){
			updateKeyboardGeometry();
		}));
		
		// Add listener for outputNotes to update selectedNoteSet
		listeners.push(outputNotes.newListener([this](vector<int> &notes){
			selectedNoteSet.clear();
			for(auto note : notes) {
				selectedNoteSet.insert(note);
			}
		}));
		
		// Initialize keyboard layout
		updateKeyboardGeometry();
		
		// Add custom region last
		addCustomRegion(keyboardRegion.set("Keyboard Region", [this](){
			drawKeyboard();
		}), [this](){
			drawKeyboard();
		});
	}
	
	// Override presetSave to add selected notes to the JSON
	void presetSave(ofJson &json) override {
		// Convert the set to a vector for JSON storage
		vector<int> selectedNotes(selectedNoteSet.begin(), selectedNoteSet.end());
		json["selectedNotes"] = selectedNotes;
	}
	
	// Override presetRecallAfterSettingParameters to restore selected notes
	void presetRecallAfterSettingParameters(ofJson &json) override {
		if(json.count("selectedNotes") > 0) {
			selectedNoteSet.clear();
			vector<int> selectedNotes = json["selectedNotes"];
			for(auto note : selectedNotes) {
				selectedNoteSet.insert(note);
			}
			
			// Update the output parameter to match the loaded notes
			vector<int> notes(selectedNoteSet.begin(), selectedNoteSet.end());
			outputNotes = notes;
		}
	}
	
private:
	ofParameter<vector<float>> pitch;
	ofParameter<vector<float>> gate;
	ofParameter<vector<int>> outputNotes;
	ofParameter<int> width;
	ofParameter<int> height;
	ofParameter<int> loNote;
	ofParameter<int> hiNote;
	ofParameter<bool> showWindow;
	ofEventListeners listeners;
	customGuiRegion keyboardRegion;
	std::set<int> selectedNoteSet;

	struct KeyGeometry {
		bool isBlack;
		float x;
		float w;
	};

	vector<KeyGeometry> keyGeometry;
	vector<KeyGeometry> fullKeyGeometry;
	
	void buildKeyboardGeometryForRange(vector<KeyGeometry>& geom, int startNote, int endNote) {
		geom.clear();

		int whiteKeyCount = 0;
		for(int note = startNote; note <= endNote; note++) {
			int noteInOctave = note % 12;
			if(noteInOctave == 0 || noteInOctave == 2 || noteInOctave == 4 ||
			   noteInOctave == 5 || noteInOctave == 7 || noteInOctave == 9 ||
			   noteInOctave == 11) {
				whiteKeyCount++;
			}
		}

		float whiteKeyWidth = width / (float)whiteKeyCount;
		float blackKeyWidth = whiteKeyWidth * 0.6f;

		float currentX = 0;

		for(int note = startNote; note <= endNote; note++) {
			int noteInOctave = note % 12;
			KeyGeometry key;

			switch(noteInOctave) {
				case 0: // C
				case 2: // D
				case 4: // E
				case 5: // F
				case 7: // G
				case 9: // A
				case 11: // B
					key.isBlack = false;
					key.x = currentX;
					key.w = whiteKeyWidth;
					currentX += whiteKeyWidth;
					break;

				case 1: // C#
				case 3: // D#
				case 6: // F#
				case 8: // G#
				case 10: // A#
					key.isBlack = true;
					key.x = currentX - (blackKeyWidth / 2);
					key.w = blackKeyWidth;
					break;
			}
			geom.push_back(key);
		}
	}

	void updateKeyboardGeometry() {
		int startNote = std::max(0, std::min(127, loNote.get()));
		int endNote = std::max(0, std::min(127, hiNote.get()));
		if(endNote < startNote) {
			std::swap(startNote, endNote);
		}

		// Build geometry for the embedded keyboard display (loNote-hiNote range)
		buildKeyboardGeometryForRange(keyGeometry, startNote, endNote);

		// Build geometry for the full scopable keyboard (0-127)
		buildKeyboardGeometryForRange(fullKeyGeometry, 0, 127);
	}
	
	void drawKeyboard() {
		int startNote = std::max(0, std::min(127, loNote.get()));
		int endNote = std::max(0, std::min(127, hiNote.get()));
		if(endNote < startNote) {
			std::swap(startNote, endNote);
		}
		drawKeyboardForRange(keyGeometry, startNote, endNote);
	}

	void drawFullKeyboard() {
		// For floating window, use the available width
		drawKeyboardForRange(fullKeyGeometry, 0, 127, true);
	}

	void drawKeyboardForRange(const vector<KeyGeometry>& geom, int startNote, int endNote, bool useAvailableWidth = false) {
		float zoom = ofxOceanodeShared::getZoomLevel();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		float scaledHeight = height * zoom;
		float scaledWidth = width * zoom;

		// If using available width (for floating window), get it from ImGui
		if(useAvailableWidth) {
			ImVec2 availableSize = ImGui::GetContentRegionAvail();
			scaledWidth = availableSize.x;
		}

		// Standard 88-key piano range: A0 (21) to C8 (108)
		const int PIANO_MIN = 21;
		const int PIANO_MAX = 108;

		// Create unique button ID to avoid conflicts
		string buttonID = "KeyboardArea_" + std::to_string(startNote) + "_" + std::to_string(endNote);
		ImGui::InvisibleButton(buttonID.c_str(), ImVec2(std::max(1.0f, scaledWidth), std::max(1.0f, scaledHeight)));

		// Calculate scale factor for geometry if using available width
		float geomScale = 1.0f;
		if(useAvailableWidth && width > 0) {
			geomScale = scaledWidth / (width * zoom);
		}

		// Handle mouse clicks
		if(ImGui::IsItemClicked()) {
			ImVec2 mousePos = ImGui::GetIO().MousePos;
			int clickedNote = getNoteFromPosition(mousePos.x, mousePos.y, geom, startNote, geomScale);
			if(clickedNote >= 0) {
				if(selectedNoteSet.find(clickedNote) != selectedNoteSet.end()) {
					selectedNoteSet.erase(clickedNote);
				} else {
					selectedNoteSet.insert(clickedNote);
				}
				vector<int> notes(selectedNoteSet.begin(), selectedNoteSet.end());
				outputNotes.set(notes);
			}
		}

		// Draw white keys first
		for(int i = 0; i < geom.size(); i++) {
			if(!geom[i].isBlack) {
				int note = startNote + i;
				ImVec2 keyPos(pos.x + geom[i].x * zoom * geomScale, pos.y);
				ImVec2 keyPosEnd(keyPos.x + geom[i].w * zoom * geomScale, pos.y + scaledHeight);

				// Check if note is within standard piano range
				bool isInPianoRange = (note >= PIANO_MIN && note <= PIANO_MAX);

				if(isInPianoRange) {
					drawList->AddRectFilled(keyPos, keyPosEnd, IM_COL32(255, 255, 255, 255));
				} else {
					// Downlighted - outside piano range
					drawList->AddRectFilled(keyPos, keyPosEnd, IM_COL32(240, 240, 240, 120));
				}
				drawList->AddRect(keyPos, keyPosEnd, IM_COL32(100, 100, 100, 255));

				if(selectedNoteSet.find(note) != selectedNoteSet.end()) {
					drawList->AddRectFilled(keyPos, keyPosEnd, IM_COL32(0, 255, 0, 100));
				}

				float keyGate = 0.0f;
				for(size_t j = 0; j < pitch->size() && j < gate->size(); j++) {
					int pitchValue = static_cast<int>(pitch->at(j) + 0.5f);
					if(pitchValue == note) {
						keyGate = std::max(keyGate, gate->at(j));
					}
				}

				if(keyGate > 0.0f) {
					uint8_t alpha = static_cast<uint8_t>(255 * (1.0f - keyGate));
					drawList->AddRectFilled(keyPos, keyPosEnd, IM_COL32(255, 0, 0, 255 - alpha));
				}
			}
		}

		// Then draw black keys
		float blackKeyHeight = scaledHeight * 0.6f;
		for(int i = 0; i < geom.size(); i++) {
			if(geom[i].isBlack) {
				int note = startNote + i;
				ImVec2 keyPos(pos.x + geom[i].x * zoom * geomScale, pos.y);
				ImVec2 keyPosEnd(keyPos.x + geom[i].w * zoom * geomScale, pos.y + blackKeyHeight);

				// Check if note is within standard piano range
				bool isInPianoRange = (note >= PIANO_MIN && note <= PIANO_MAX);

				if(isInPianoRange) {
					drawList->AddRectFilled(keyPos, keyPosEnd, IM_COL32(0, 0, 0, 255));
				} else {
					// Downlighted - outside piano range
					drawList->AddRectFilled(keyPos, keyPosEnd, IM_COL32(80, 80, 80, 120));
				}
				drawList->AddRect(keyPos, keyPosEnd, IM_COL32(100, 100, 100, 255));

				if(selectedNoteSet.find(note) != selectedNoteSet.end()) {
					drawList->AddRectFilled(keyPos, keyPosEnd, IM_COL32(0, 255, 0, 100));
				}

				float keyGate = 0.0f;
				for(size_t j = 0; j < pitch->size() && j < gate->size(); j++) {
					int pitchValue = static_cast<int>(pitch->at(j) + 0.5f);
					if(pitchValue == note) {
						keyGate = std::max(keyGate, gate->at(j));
					}
				}

				if(keyGate > 0.0f) {
					uint8_t alpha = static_cast<uint8_t>(255 * (1.0f - keyGate));
					drawList->AddRectFilled(keyPos, keyPosEnd, IM_COL32(255, 0, 0, 255 - alpha));
				}
			}
		}
	}
	
	int getNoteFromPosition(float mouseX, float mouseY, const vector<KeyGeometry>& geom, int startNote, float geomScale = 1.0f) {
		float zoom = ofxOceanodeShared::getZoomLevel();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		float relativeX = mouseX - pos.x;
		float relativeY = mouseY - pos.y;

		float blackKeyHeight = height * 0.6f * zoom;
		if(relativeY <= blackKeyHeight) {
			for(int i = 0; i < geom.size(); i++) {
				if(geom[i].isBlack) {
					if(relativeX >= geom[i].x * zoom * geomScale &&
					   relativeX <= geom[i].x * zoom * geomScale + geom[i].w * zoom * geomScale) {
						return startNote + i;
					}
				}
			}
		}

		for(int i = 0; i < geom.size(); i++) {
			if(!geom[i].isBlack) {
				if(relativeX >= geom[i].x * zoom * geomScale &&
				   relativeX <= geom[i].x * zoom * geomScale + geom[i].w * zoom * geomScale) {
					return startNote + i;
				}
			}
		}

		return -1;
	}

	void draw(ofEventArgs &e) override {
		bool windowOpen = showWindow.get();
		if(ImGui::Begin("Piano Keyboard", &windowOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
			drawFullKeyboard();
			ImGui::End();
		}
		showWindow.set(windowOpen);
	}
};

#endif /* pianoKeyboard_h */
