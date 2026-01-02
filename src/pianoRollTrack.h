#ifndef pianoRollTrack_h
#define pianoRollTrack_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "ppqTimeline.h"
#include "transportTrack.h"
#include <algorithm>
#include <cmath>

struct MidiNote {
	double startBeat;
	float length;
	int pitch;        // MIDI note number (0-127)
	float velocity;   // 0.0 to 1.0
	
	double endBeat() const { return startBeat + length; }
	
	bool operator<(const MidiNote& other) const {
		if(startBeat != other.startBeat) return startBeat < other.startBeat;
		return pitch < other.pitch;
	}
};

class pianoRollTrack : public ofxOceanodeNodeModel, public transportTrack {
public:
	pianoRollTrack() : ofxOceanodeNodeModel("Piano Roll Track") {
		color = ofColor(180, 255, 180);
	}

	~pianoRollTrack() {
		if(currentTimeline) currentTimeline->unsubscribeTrack(this);
	}

	void setup() override {
		refreshTimelineList();
		addParameterDropdown(timelineSelect, "Timeline", 0, timelineOptions);
		addParameter(trackName.set("Track Name", "Piano " + ofToString(getNumIdentifier())));
		
		addParameter(scrollOffset.set("Scroll", 48, 0, 127)); // Start at C3
		addParameter(visibleRange.set("Zoom", 36, 12, 88)); // 3 octaves visible
		
		addOutputParameter(pitchesOutput.set("Pitch[]", {0}, {0}, {127}));
		addOutputParameter(velocitiesOutput.set("Velocity[]", {0}, {0}, {1}));
		addOutputParameter(gatesOutput.set("Gate[]", {0}, {0}, {1}));
		addOutputParameter(numActiveOutput.set("Num Active", 0, 0, 128));

		listeners.push(timelineSelect.newListener([this](int &val){ updateSubscription(); }));
		
		pitchesOutput.setSerializable(false);
		velocitiesOutput.setSerializable(false);
		gatesOutput.setSerializable(false);
		numActiveOutput.setSerializable(false);
		
		updateSubscription();
	}

	void update(ofEventArgs &args) override {
		if(!currentTimeline) {
			pitchesOutput = vector<float>();
			velocitiesOutput = vector<float>();
			gatesOutput = vector<float>();
			numActiveOutput = 0;
			return;
		}
		
		double currentBeat = currentTimeline->getBeatPosition();
		
		vector<float> activePitches;
		vector<float> activeVelocities;
		vector<float> activeGates;
		
		// Find all notes active at current beat
		for(const auto& note : notes) {
			if(currentBeat >= note.startBeat && currentBeat < note.endBeat()) {
				activePitches.push_back(note.pitch);
				activeVelocities.push_back(note.velocity);
				activeGates.push_back(1.0f);
			}
		}
		
		pitchesOutput = activePitches;
		velocitiesOutput = activeVelocities;
		gatesOutput = activeGates;
		numActiveOutput = activePitches.size();
	}

	// --- TransportTrack Implementation ---
	std::string getTrackName() override {
		return trackName.get();
	}
	
	float getHeight() const override {
		return trackHeight;
	}
	
	void setHeight(float h) override {
		trackHeight = ofClamp(h, MIN_TRACK_HEIGHT, MAX_TRACK_HEIGHT);
	}
	
	bool isCollapsed() const override {
		return collapsed;
	}
	
	void setCollapsed(bool c) override {
		collapsed = c;
	}

	void drawInTimeline(ImDrawList* dl, ImVec2 pos, ImVec2 sz, double viewStart, double viewEnd) override {
		
		// 1. Create interaction button
		std::string buttonId = "##pianoBtn" + ofToString(getNumIdentifier());
		ImGui::InvisibleButton(buttonId.c_str(), sz);
		
		// 2. Capture screen rect
		ImVec2 p = ImGui::GetItemRectMin();
		ImVec2 s = ImGui::GetItemRectSize();
		ImVec2 endP = ImGui::GetItemRectMax();
		
		// 3. Calculate layout areas
		float pianoKeyWidth = s.x * 0.05f;
		float velocityLaneHeight = 60.0f; // Height of velocity editing lane
		
		// Piano keys area (left, upper portion)
		ImVec2 pianoKeysStart(p.x, p.y);
		ImVec2 pianoKeysEnd(p.x + pianoKeyWidth, endP.y - velocityLaneHeight);
		
		// Piano roll area (main, upper portion)
		ImVec2 rollStart(p.x + pianoKeyWidth, p.y);
		ImVec2 rollEnd(endP.x, endP.y - velocityLaneHeight);
		float rollWidth = rollEnd.x - rollStart.x;
		float rollHeight = rollEnd.y - rollStart.y;
		
		// Velocity lane area (bottom)
		ImVec2 velLaneStart(p.x + pianoKeyWidth, endP.y - velocityLaneHeight);
		ImVec2 velLaneEnd(endP.x, endP.y);
		
		// Velocity lane label area
		ImVec2 velLabelStart(p.x, endP.y - velocityLaneHeight);
		ImVec2 velLabelEnd(p.x + pianoKeyWidth, endP.y);
		
		// 4. Get interaction state
		ImVec2 mousePos = ImGui::GetMousePos();
		bool isHovered = ImGui::IsItemHovered();
		bool isLeftClick = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		bool isRightClick = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
		bool isDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
		bool isReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
		
		// 5. Calculate note range (all 128 notes, but only show visible range)
		int lowestVisibleNote = scrollOffset.get();
		int highestVisibleNote = std::min(127, lowestVisibleNote + visibleRange.get() - 1);
		int numVisibleNotes = highestVisibleNote - lowestVisibleNote + 1;
		float noteHeight = rollHeight / numVisibleNotes;
		
		// 6. Setup coordinate helpers
		double visibleLen = viewEnd - viewStart;
		if(visibleLen <= 0.001) return;
		
		int gridTicks = 0;
		double beatsPerBar = 4.0;
		double currentPlayheadBeat = 0.0;
		
		if(currentTimeline) {
			gridTicks = currentTimeline->getGridTicks();
			beatsPerBar = double(currentTimeline->getNumerator()) *
						  (4.0 / double(currentTimeline->getDenominator()));
			currentPlayheadBeat = currentTimeline->getBeatPosition();
		}
		
		auto beatToX = [&](double b) {
			return rollStart.x + float((b - viewStart) / visibleLen) * rollWidth;
		};
		
		auto xToBeat = [&](float x) {
			return viewStart + ((x - rollStart.x) / rollWidth) * visibleLen;
		};
		
		auto pitchToY = [&](int pitch) {
			// Higher pitches at top, within visible range
			if(pitch > highestVisibleNote) return rollStart.y - 100.0f; // Off screen top
			if(pitch < lowestVisibleNote) return rollEnd.y + 100.0f; // Off screen bottom
			int noteIndex = highestVisibleNote - pitch;
			return rollStart.y + noteIndex * noteHeight;
		};
		
		auto yToPitch = [&](float y) {
			int noteIndex = (int)((y - rollStart.y) / noteHeight);
			int pitch = highestVisibleNote - noteIndex;
			return ofClamp(pitch, 0, 127);
		};
		
		auto snap = [&](double b) {
			if(gridTicks <= 0) return b;
			double gridBeats = gridTicks / 24.0;
			return std::round(b / gridBeats) * gridBeats;
		};
		
		// 7. Draw backgrounds
		dl->AddRectFilled(pianoKeysStart, pianoKeysEnd, IM_COL32(30, 30, 30, 255));
		dl->AddRectFilled(rollStart, rollEnd, IM_COL32(40, 40, 40, 255));
		dl->AddRectFilled(velLabelStart, velLabelEnd, IM_COL32(30, 30, 30, 255));
		dl->AddRectFilled(velLaneStart, velLaneEnd, IM_COL32(35, 35, 35, 255));
		dl->AddRect(p, endP, IM_COL32(60, 60, 60, 255));
		
		// 8. Draw velocity lane label
		dl->AddText(
			ImVec2(velLabelStart.x + 2, velLabelStart.y + 20),
			IM_COL32(150, 150, 150, 255),
			"Vel"
		);
		
		// 9. Draw piano keys and note rows
		for(int pitch = lowestVisibleNote; pitch <= highestVisibleNote; pitch++) {
			float y = pitchToY(pitch);
			float nextY = pitchToY(pitch - 1);
			if(nextY > rollEnd.y) nextY = rollEnd.y;
			
			int pitchClass = pitch % 12;
			bool isBlackKey = (pitchClass == 1 || pitchClass == 3 || pitchClass == 6 ||
							   pitchClass == 8 || pitchClass == 10);
			
			// Piano key
			ImU32 keyColor = isBlackKey ? IM_COL32(20, 20, 20, 255) : IM_COL32(200, 200, 200, 255);
			dl->AddRectFilled(
				ImVec2(pianoKeysStart.x, y),
				ImVec2(pianoKeysEnd.x - 1, nextY),
				keyColor
			);
			
			// Note row background (alternating for white keys)
			if(!isBlackKey) {
				dl->AddRectFilled(
					ImVec2(rollStart.x, y),
					ImVec2(rollEnd.x, nextY),
					IM_COL32(45, 45, 45, 255)
				);
			}
			
			// Note row separator
			dl->AddLine(
				ImVec2(rollStart.x, y),
				ImVec2(rollEnd.x, y),
				IM_COL32(60, 60, 60, 255),
				0.5f
			);
			
			// Octave markers (C notes)
			if(pitchClass == 0) {
				char noteName[8];
				int octave = (pitch / 12) - 1;
				snprintf(noteName, 8, "C%d", octave);
				dl->AddText(
					ImVec2(pianoKeysStart.x + 2, y + 2),
					IM_COL32(100, 100, 100, 255),
					noteName
				);
				// Thicker line for octaves
				dl->AddLine(
					ImVec2(rollStart.x, y),
					ImVec2(rollEnd.x, y),
					IM_COL32(80, 80, 80, 255),
					1.0f
				);
			}
		}
		
		// 10. Draw vertical grid lines (bar lines) in both roll and velocity lane
		int viewStartBar = int(viewStart / beatsPerBar);
		int viewEndBar = int(viewEnd / beatsPerBar) + 1;
		
		for(int bar = viewStartBar; bar <= viewEndBar; ++bar) {
			double barBeat = bar * beatsPerBar;
			float barX = beatToX(barBeat);
			
			if(barX < rollStart.x - 5 || barX > rollEnd.x + 5) continue;
			
			// Grid in roll area
			dl->AddLine(
				ImVec2(barX, rollStart.y),
				ImVec2(barX, rollEnd.y),
				IM_COL32(120, 120, 120, 255),
				2.0f
			);
			
			// Grid in velocity lane
			dl->AddLine(
				ImVec2(barX, velLaneStart.y),
				ImVec2(barX, velLaneEnd.y),
				IM_COL32(80, 80, 80, 255),
				1.0f
			);
			
			// Grid subdivisions
			if(gridTicks > 0 && bar < viewEndBar) {
				double gridBeats = gridTicks / 24.0;
				double nextBarBeat = (bar + 1) * beatsPerBar;
				
				for(double b = barBeat + gridBeats; b < nextBarBeat; b += gridBeats) {
					if(b < viewStart || b > viewEnd) continue;
					
					float gridX = beatToX(b);
					
					// Roll subdivisions
					dl->AddLine(
						ImVec2(gridX, rollStart.y),
						ImVec2(gridX, rollEnd.y),
						IM_COL32(70, 70, 70, 100),
						0.5f
					);
					
					// Velocity lane subdivisions
					dl->AddLine(
						ImVec2(gridX, velLaneStart.y),
						ImVec2(gridX, velLaneEnd.y),
						IM_COL32(60, 60, 60, 100),
						0.5f
					);
				}
			}
		}
		
		// 10.5 Draw loop region
		if(currentTimeline) {
			double loopStart, loopEnd;
			bool loopEnabled = false;
			
			if(getLoopRegion(loopStart, loopEnd, loopEnabled) && loopEnabled) {
				float lx1 = beatToX(loopStart);
				float lx2 = beatToX(loopEnd);
				
				lx1 = std::max(lx1, rollStart.x);
				lx2 = std::min(lx2, rollEnd.x);
				
				// Roll area
				dl->AddRectFilled(
					ImVec2(lx1, rollStart.y),
					ImVec2(lx2, rollEnd.y),
					IM_COL32(80, 80, 160, 50)
				);
				
				// Velocity lane
				dl->AddRectFilled(
					ImVec2(lx1, velLaneStart.y),
					ImVec2(lx2, velLaneEnd.y),
					IM_COL32(80, 80, 160, 50)
				);
				
				dl->AddLine(
					ImVec2(lx1, rollStart.y),
					ImVec2(lx1, velLaneEnd.y),
					IM_COL32(160, 160, 255, 180),
					2.0f
				);
				dl->AddLine(
					ImVec2(lx2, rollStart.y),
					ImVec2(lx2, velLaneEnd.y),
					IM_COL32(160, 160, 255, 180),
					2.0f
				);
			}
		}
		
		// 11. Draw notes (without velocity bars - those go in velocity lane now)
		for(size_t i = 0; i < notes.size(); i++) {
			const MidiNote& note = notes[i];
			
			// Only draw if pitch is in visible range
			if(note.pitch < lowestVisibleNote || note.pitch > highestVisibleNote) continue;
			
			float x1 = beatToX(note.startBeat);
			float x2 = beatToX(note.endBeat());
			float y1 = pitchToY(note.pitch);
			float y2 = pitchToY(note.pitch - 1);
			
			// Skip if outside view
			if(x2 < rollStart.x || x1 > rollEnd.x) continue;
			
			// Clamp to visible area
			float drawX1 = std::max(x1, rollStart.x);
			float drawX2 = std::min(x2, rollEnd.x);
			float drawY1 = std::max(y1, rollStart.y);
			float drawY2 = std::min(y2, rollEnd.y);
			
			// Note body
			bool isSelected = (int)i == selectedNote;
			ImU32 noteColor = isSelected ?
				IM_COL32(180, 255, 180, 255) :
				IM_COL32(120, 200, 120, 220);
			
			dl->AddRectFilled(
				ImVec2(drawX1 + 1, drawY1 + 1),
				ImVec2(drawX2 - 1, drawY2 - 1),
				noteColor,
				2.0f
			);
			
			// Note border
			dl->AddRect(
				ImVec2(drawX1 + 1, drawY1 + 1),
				ImVec2(drawX2 - 1, drawY2 - 1),
				IM_COL32(80, 80, 80, 255),
				2.0f,
				0,
				1.5f
			);
		}
		
		// 12. Draw velocity bars in velocity lane
		for(size_t i = 0; i < notes.size(); i++) {
			const MidiNote& note = notes[i];
			
			float x1 = beatToX(note.startBeat);
			float x2 = beatToX(note.endBeat());
			
			// Skip if outside horizontal view
			if(x2 < rollStart.x || x1 > rollEnd.x) continue;
			
			float drawX1 = std::max(x1, rollStart.x);
			float drawX2 = std::min(x2, rollEnd.x);
			
			// Velocity bar height based on velocity value
			float velBarHeight = (velLaneEnd.y - velLaneStart.y) * note.velocity;
			float velBarY1 = velLaneEnd.y - velBarHeight;
			float velBarY2 = velLaneEnd.y;
			
			bool isSelected = (int)i == selectedNote;
			
			// Velocity bar color
			uint8_t brightness = (uint8_t)(100 + note.velocity * 155);
			ImU32 velColor = isSelected ?
				IM_COL32(180, 255, 180, 255) :
				IM_COL32(100, brightness, 100, 220);
			
			dl->AddRectFilled(
				ImVec2(drawX1 + 1, velBarY1),
				ImVec2(drawX2 - 1, velBarY2 - 1),
				velColor
			);
			
			// Border
			dl->AddRect(
				ImVec2(drawX1 + 1, velBarY1),
				ImVec2(drawX2 - 1, velBarY2 - 1),
				IM_COL32(80, 80, 80, 255),
				0,
				0,
				1.0f
			);
		}
		
		// 13. Draw preview note while dragging
		if(isDragging && isCreatingNote) {
			double currentBeat = snap(xToBeat(mousePos.x));
			
			double start = std::min(dragStartBeat, currentBeat);
			double end = std::max(dragStartBeat, currentBeat);
			
			float px1 = beatToX(start);
			float px2 = beatToX(end);
			float py1 = pitchToY(dragStartPitch);
			float py2 = pitchToY(dragStartPitch - 1);
			
			// Preview note in roll
			if(dragStartPitch >= lowestVisibleNote && dragStartPitch <= highestVisibleNote) {
				dl->AddRectFilled(
					ImVec2(px1 + 1, py1 + 1),
					ImVec2(px2 - 1, py2 - 1),
					IM_COL32(120, 200, 120, 120),
					2.0f
				);
			}
			
			// Preview velocity bar
			float defaultVel = 0.8f;
			float velBarHeight = (velLaneEnd.y - velLaneStart.y) * defaultVel;
			float velBarY1 = velLaneEnd.y - velBarHeight;
			
			dl->AddRectFilled(
				ImVec2(px1 + 1, velBarY1),
				ImVec2(px2 - 1, velLaneEnd.y - 1),
				IM_COL32(100, 200, 100, 120)
			);
		}
		
		// 14. Draw playhead
		float playheadX = beatToX(currentPlayheadBeat);
		if(playheadX >= rollStart.x && playheadX <= rollEnd.x) {
			// Playhead in roll
			dl->AddLine(
				ImVec2(playheadX, rollStart.y),
				ImVec2(playheadX, rollEnd.y),
				IM_COL32(255, 80, 80, 255),
				2.5f
			);
			
			// Playhead in velocity lane
			dl->AddLine(
				ImVec2(playheadX, velLaneStart.y),
				ImVec2(playheadX, velLaneEnd.y),
				IM_COL32(255, 80, 80, 255),
				2.5f
			);
		}
		
		// 15. Handle interactions
		bool inRollArea = (mousePos.x >= rollStart.x && mousePos.x <= rollEnd.x &&
						   mousePos.y >= rollStart.y && mousePos.y <= rollEnd.y);
		bool inVelLane = (mousePos.x >= velLaneStart.x && mousePos.x <= velLaneEnd.x &&
						  mousePos.y >= velLaneStart.y && mousePos.y <= velLaneEnd.y);
		
		if(isLeftClick) {
			if(inRollArea) {
				// Check if clicking on existing note
				selectedNote = -1;
				for(size_t i = 0; i < notes.size(); i++) {
					if(notes[i].pitch < lowestVisibleNote || notes[i].pitch > highestVisibleNote) continue;
					
					float x1 = beatToX(notes[i].startBeat);
					float x2 = beatToX(notes[i].endBeat());
					float y1 = pitchToY(notes[i].pitch);
					float y2 = pitchToY(notes[i].pitch - 1);
					
					if(mousePos.x >= x1 && mousePos.x <= x2 &&
					   mousePos.y >= y1 && mousePos.y <= y2) {
						selectedNote = i;
						isDraggingNote = true;
						dragOffsetBeat = notes[i].startBeat - xToBeat(mousePos.x);
						dragOffsetPitch = notes[i].pitch - yToPitch(mousePos.y);
						break;
					}
				}
				
				// If not clicking on note, create new one
				if(selectedNote == -1) {
					isCreatingNote = true;
					dragStartBeat = snap(xToBeat(mousePos.x));
					dragStartPitch = yToPitch(mousePos.y); // Lock pitch at start
				}
			}
			else if(inVelLane) {
				// Check if clicking on velocity bar
				selectedNote = -1;
				for(size_t i = 0; i < notes.size(); i++) {
					float x1 = beatToX(notes[i].startBeat);
					float x2 = beatToX(notes[i].endBeat());
					
					if(mousePos.x >= x1 && mousePos.x <= x2) {
						selectedNote = i;
						isDraggingVelocity = true;
						break;
					}
				}
			}
		}
		
		if(isDragging) {
			if(isDraggingNote && selectedNote >= 0 && selectedNote < notes.size()) {
				// Move note
				double newBeat = snap(xToBeat(mousePos.x) + dragOffsetBeat);
				int newPitch = ofClamp(yToPitch(mousePos.y) + dragOffsetPitch, 0, 127);
				
				notes[selectedNote].startBeat = newBeat;
				notes[selectedNote].pitch = newPitch;
				
				std::sort(notes.begin(), notes.end());
			}
			else if(isDraggingVelocity && selectedNote >= 0 && selectedNote < notes.size()) {
				// Adjust velocity based on Y position in velocity lane
				float velPercent = 1.0f - ((mousePos.y - velLaneStart.y) / (velLaneEnd.y - velLaneStart.y));
				notes[selectedNote].velocity = ofClamp(velPercent, 0.0f, 1.0f);
			}
		}
		
		if(isReleased) {
			if(isCreatingNote) {
				double endBeat = snap(xToBeat(mousePos.x));
				
				double start = std::min(dragStartBeat, endBeat);
				double end = std::max(dragStartBeat, endBeat);
				double length = end - start;
				
				// Create note if it has some length (pitch already locked from start)
				if(length > 0.001) {
					MidiNote newNote;
					newNote.startBeat = start;
					newNote.length = length;
					newNote.pitch = dragStartPitch; // Use locked pitch
					newNote.velocity = 0.8f; // Default velocity
					notes.push_back(newNote);
					std::sort(notes.begin(), notes.end());
				}
				
				isCreatingNote = false;
			}
			
			isDraggingNote = false;
			isDraggingVelocity = false;
		}
		
		if(isRightClick) {
			if(inRollArea) {
				// Delete note under cursor in roll
				for(size_t i = 0; i < notes.size(); i++) {
					if(notes[i].pitch < lowestVisibleNote || notes[i].pitch > highestVisibleNote) continue;
					
					float x1 = beatToX(notes[i].startBeat);
					float x2 = beatToX(notes[i].endBeat());
					float y1 = pitchToY(notes[i].pitch);
					float y2 = pitchToY(notes[i].pitch - 1);
					
					if(mousePos.x >= x1 && mousePos.x <= x2 &&
					   mousePos.y >= y1 && mousePos.y <= y2) {
						notes.erase(notes.begin() + i);
						break;
					}
				}
			}
			else if(inVelLane) {
				// Delete note by velocity bar
				for(size_t i = 0; i < notes.size(); i++) {
					float x1 = beatToX(notes[i].startBeat);
					float x2 = beatToX(notes[i].endBeat());
					
					if(mousePos.x >= x1 && mousePos.x <= x2) {
						notes.erase(notes.begin() + i);
						break;
					}
				}
			}
		}
	}
	

	// --- Preset Saving ---
	void presetSave(ofJson &json) override {
		std::vector<std::vector<float>> noteList;
		for(auto &n : notes) {
			noteList.push_back({(float)n.startBeat, n.length, (float)n.pitch, n.velocity});
		}
		json["notes"] = noteList;
		json["trackHeight"] = trackHeight;
		json["collapsed"] = collapsed;
	}

	void presetRecallAfterSettingParameters(ofJson &json) override {
		if(json.count("notes") > 0) {
			notes.clear();
			for(auto &nData : json["notes"]) {
				if(nData.size() >= 4) {
					MidiNote note;
					note.startBeat = nData[0];
					note.length = nData[1];
					note.pitch = (int)nData[2];
					note.velocity = nData[3];
					notes.push_back(note);
				}
			}
		}
		
		if(json.count("trackHeight") > 0) {
			trackHeight = json["trackHeight"];
			trackHeight = ofClamp(trackHeight, MIN_TRACK_HEIGHT, MAX_TRACK_HEIGHT);
		}
		
		if(json.count("collapsed") > 0) {
			collapsed = json["collapsed"];
		}
	}

private:
	ofParameter<int> timelineSelect;
	ofParameter<std::string> trackName;
	ofParameter<int> scrollOffset;     // Starting MIDI note (0-127)
	ofParameter<int> visibleRange;     // Number of visible notes
	ofParameter<vector<float>> pitchesOutput;
	ofParameter<vector<float>> velocitiesOutput;
	ofParameter<vector<float>> gatesOutput;
	ofParameter<int> numActiveOutput;

	ppqTimeline* currentTimeline = nullptr;
	std::vector<MidiNote> notes;
	std::vector<std::string> timelineOptions;
	
	ofEventListeners listeners;
	
	// Interaction state
	int selectedNote = -1;
	bool isCreatingNote = false;
	bool isDraggingNote = false;
	bool isDraggingVelocity = false;
	double dragStartBeat = 0.0;
	int dragStartPitch = 60;
	double dragOffsetBeat = 0.0;
	int dragOffsetPitch = 0;
	
	// Track height management
	float trackHeight = 260.0f; // Taller default for piano roll + velocity lane
	bool collapsed = false;
	static constexpr float MIN_TRACK_HEIGHT = 160.0f; // Minimum to fit velocity lane
	static constexpr float MAX_TRACK_HEIGHT = 600.0f;

	void refreshTimelineList() {
		timelineOptions.clear();
		timelineOptions.push_back("None");
		for(auto* tl : ppqTimeline::getTimelines()) {
			timelineOptions.push_back("Timeline " + ofToString(tl->getNumIdentifier()));
		}
		timelineSelect.set("Timeline", 0, 0, (int)timelineOptions.size()-1);
	}

	void updateSubscription() {
		if(currentTimeline) currentTimeline->unsubscribeTrack(this);
		int idx = timelineSelect.get() - 1;
		auto& tls = ppqTimeline::getTimelines();
		if(idx >= 0 && idx < tls.size()) {
			currentTimeline = tls[idx];
			currentTimeline->subscribeTrack(this);
		} else {
			currentTimeline = nullptr;
		}
	}
	
	bool getLoopRegion(double& loopStart, double& loopEnd, bool& enabled) {
		if(!currentTimeline) return false;
		
		enabled = currentTimeline->isLoopEnabled();
		loopStart = currentTimeline->getLoopStart();
		loopEnd = currentTimeline->getLoopEnd();
		
		return true;
	}
};

#endif
