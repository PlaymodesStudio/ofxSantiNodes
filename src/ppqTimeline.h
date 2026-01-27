#ifndef ppqTimeline_h
#define ppqTimeline_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "transportTrack.h"
#include <cmath>
#include <algorithm>
#include <vector>

class ppqTimeline : public ofxOceanodeNodeModel {
public:
	
	// --- STATIC REGISTRY: Allows tracks to find timelines ---
	static std::vector<ppqTimeline*>& getTimelines() {
		static std::vector<ppqTimeline*> instances;
		return instances;
	}
	
	ppqTimeline() : ofxOceanodeNodeModel("PPQ Timeline") {
		getTimelines().push_back(this);
	}
	
	virtual ~ppqTimeline() {
		auto& tls = getTimelines();
		tls.erase(std::remove(tls.begin(), tls.end(), this), tls.end());
	}
	
	// --- TRACK MANAGEMENT ---
	void subscribeTrack(transportTrack* track) {
		subscribedTracks.push_back(track);
	}
	
	void unsubscribeTrack(transportTrack* track) {
		subscribedTracks.erase(std::remove(subscribedTracks.begin(), subscribedTracks.end(), track), subscribedTracks.end());
	}

	void setup() override {

		// ---------- Clock Mode ----------
		addSeparator("CLOCK",ofColor(240,240,240));
		addParameterDropdown(clockMode, "Clock Mode", 0, {
			"Internal", "External"
		});

		// ---------- Transport ----------
		addSeparator("TRANSPORT",ofColor(240,240,240));
		addParameter(play.set("Play", 0, 0, 1));
		addParameter(bpm.set("BPM", 120.f, 1.f, 999.f));
		addParameter(reset.set("Reset"));

		// ---------- External Inputs ----------
		addSeparator("EXTERNAL INPUT",ofColor(240,240,240));
		addParameter(ppqInput.set("PPQ In", 0, 0, INT_MAX));
		addParameter(beatTransportInput.set("Beat In", 0.f, 0.f, FLT_MAX));

		// ---------- Meter ----------
		addSeparator("TIME MEASURE",ofColor(240,240,240));
		addParameter(numerator.set("Numerator", 4, 1, 32));
		addParameter(denominator.set("Denominator", 4, 1, 32));

		// ---------- Structure ----------
		addSeparator("LENGTH",ofColor(240,240,240));
		addParameter(totalBars.set("Bars", 8, 1, 2048));

		// ---------- Loop ----------
		addSeparator("LOOP",ofColor(240,240,240));
		addParameter(loopEnabled.set("Loop", 0, 0, 1));
		addParameter(loopStartBeat.set("Loop Start", 0.f, 0.f, FLT_MAX));
		addParameter(loopEndBeat.set("Loop End", 4.f, 0.f, FLT_MAX));
		addParameter(wrapAtEnd.set("Wrap End", 1, 0, 1));

		// ---------- UI ----------
		addSeparator("GUI",ofColor(240,240,240));
		addParameter(showWindow.set("Show", false));
		addParameter(zoomBars.set("Zoom Bars", 8, 1, totalBars.get())); // Max = totalBars
		addParameterDropdown(gridDiv, "Grid", 5, {
			"None", "Bar", "1st", "2nd", "4th", "8th", "16th", "32nd", "64th"
		});

		addParameterDropdown(gridMode, "Grid Mode", 0, {
			"Straight", "Dotted", "Triplet"
		});


		// ---------- Outputs ----------
		addSeparator("OUT",ofColor(240,240,240));
		addOutputParameter(jumpTrig.set("Jump", 0, 0, 1));
		jumpTrig.setSerializable(false);
		
		addOutputParameter(ppq24.set("PPQ 24", 0, 0, INT_MAX));
		addOutputParameter(ppq24f.set("PPQ 24f", 0.f, 0.f, FLT_MAX));
		addOutputParameter(phasor.set("Phasor", 0.f, 0.f, 1.f));
		addOutputParameter(beatTransport.set("Beat Transport", 0.f, 0.f, FLT_MAX));
		addOutputParameter(bar.set("Bar", 0, 0, INT_MAX));
		addOutputParameter(barBeat.set("Bar Beat", 0.f, 0.f, FLT_MAX));

		listeners.push(reset.newListener([this]() {
			resetTransport();
		}));
		
		// Update zoom constraint when totalBars changes
		listeners.push(totalBars.newListener([this](int &bars) {
			int currentZoom = zoomBars.get();
			zoomBars.setMax(bars);
			// Clamp current zoom if it exceeds new max
			if(currentZoom > bars) {
				zoomBars = bars;
			}
		}));

		resetTransport();
	}
	
	// --- Public Accessors for Tracks ---
	double getBeatPosition() const { return beatAcc; }
	int getNumerator() const { return numerator; }
	int getDenominator() const { return denominator; }
	int getGridTicks() const { return gridTicks(); }
	
	// Loop accessors
	bool isLoopEnabled() const { return loopEnabled.get() == 1; }
	double getLoopStart() const { return loopStartBeat.get(); }
	double getLoopEnd() const { return loopEndBeat.get(); }
		

	// ---------- Clock ----------
	void update(ofEventArgs &) override {
		double prev = beatAcc;

		// Decrement jump trigger counter
		if(jumpTrigFramesRemaining > 0){
			jumpTrigFramesRemaining--;
			jumpTrig = 1;
		} else {
			jumpTrig = 0;
		}

		// External or Internal clock mode
		if(clockMode.get() == 1){ // External
			// Prefer beatTransport input if available, otherwise use PPQ
			float beatIn = beatTransportInput.get();
			int ppqIn = ppqInput.get();
			
			if(beatIn > 0.0001f || (beatIn == 0.0f && ppqIn == 0)){
				// Use beat transport input
				beatAcc = beatIn;
			} else {
				// Convert PPQ input to beats
				beatAcc = ppqIn / 24.0;
			}
			
			// Detect jumps in external input
			if(lastExternalBeat >= 0){
				double delta = std::abs(beatAcc - lastExternalBeat);
				// Jump if moved more than expected for one frame at current BPM
				double expectedDelta = (bpm.get() / 60.0) * 0.1; // 100ms worth
				if(delta > expectedDelta){
					jumpTrigFramesRemaining = 3; // Stay high for 3 frames
				}
			}
			lastExternalBeat = beatAcc;
		}
		else{ // Internal
			if(play.get() == 0){
				// Transport stopped - reset timing state when stopped
				transportRunning = false;
				return;
			}

			uint64_t now = ofGetElapsedTimeMillis();
			
			// First frame after play starts
			if(!transportRunning){
				lastTimeMs = now;
				beatAccBase = beatAcc; // Start from current position (preserves position on resume)
				lastBpm = bpm.get();
				transportRunning = true;
			}
			
			// Handle BPM changes
			if(std::abs(bpm.get() - lastBpm) > 0.001f){
				// BPM changed - snapshot current position as new base
				beatAccBase = beatAcc;
				lastTimeMs = now;
				lastBpm = bpm.get();
			}
			
			// Calculate beats from elapsed time (drift-free)
			double elapsedSeconds = (now - lastTimeMs) / 1000.0;
			beatAcc = beatAccBase + (elapsedSeconds * (bpm.get() / 60.0));
		}

		handleLoop(prev);

		// --- wrap whole timeline at the end ---
		if(wrapAtEnd.get() == 1){
			double totBeats = totalBeats();
			if(totBeats > 0 && beatAcc >= totBeats){
				beatAcc = std::fmod(beatAcc, totBeats);
				// Update base to reflect the wrap
				if(transportRunning){
					beatAccBase = beatAcc;
					lastTimeMs = ofGetElapsedTimeMillis();
				}
			}
		}

		updateOutputs();

	}
	


	// ---------- GUI ----------
	void draw(ofEventArgs &) override {
		if(!showWindow.get()) return;

		std::string title = "Timeline " + ofToString(getNumIdentifier());
		bool show = showWindow.get();

		// Fixed layout constants
		const float RULER_HEIGHT = 40.0f;  // Reduced from 120
		const float SEPARATOR_HEIGHT = 4.0f;

		// Calculate total required height dynamically based on track heights
		float totalH = RULER_HEIGHT + 40; // Ruler + margin
		if(!subscribedTracks.empty()) {
			totalH += 60; // Header space
			for(auto* track : subscribedTracks) {
				if(!track->isCollapsed()) {
					totalH += track->getHeight() + SEPARATOR_HEIGHT;
				} else {
					totalH += 20; // Just space for collapsed header
				}
			}
		}

		ImGui::SetNextWindowSize(ImVec2(800, totalH), ImGuiCond_Always);

		if(ImGui::Begin(title.c_str(), &show)) {
			
			// 1. Draw Master Ruler
			drawTimeline(RULER_HEIGHT);

			// 2. Draw Tracks
			if(!subscribedTracks.empty()) {
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0.7,0.7,0.7,1), "TRACKS");
				ImGui::Separator();

				// Calculate shared view parameters
				ImDrawList* dl = ImGui::GetWindowDrawList();
				
				int barsVisible = zoomBars.get();
				double beatsPerBar = double(numerator.get()) * (4.0 / double(denominator.get()));
				
				// Match the ruler's view - start from bar 0
				int viewStartBar = 0;
				double viewStartBeat = viewStartBar * beatsPerBar;
				double viewEndBeat = viewStartBeat + (barsVisible * beatsPerBar);

				for(auto* track : subscribedTracks) {
					ImGui::PushID(track);
					
					// Track header with collapse button
					ImVec2 headerStart = ImGui::GetCursorScreenPos();
					float availW = ImGui::GetContentRegionAvail().x;
					
					// Collapse/expand triangle button
					bool isCollapsed = track->isCollapsed();
					const char* arrowIcon = isCollapsed ? ">" : "v";
					
					if(ImGui::SmallButton(arrowIcon)) {
						track->setCollapsed(!isCollapsed);
					}
					
					// Track name next to button
					ImGui::SameLine();
					ImGui::Text("%s", track->getTrackName().c_str());
					
					// Only draw track content if not collapsed
					if(!isCollapsed) {
						// Get available width and this track's height
						float trackHeight = track->getHeight();
						ImVec2 trackSize(availW, trackHeight);
						
						// Let track draw itself - it handles its own InvisibleButton
						track->drawInTimeline(dl, ImVec2(0,0), trackSize, viewStartBeat, viewEndBeat);
						
						// Draggable separator
						ImVec2 sepPos = ImGui::GetCursorScreenPos();
						ImGui::InvisibleButton("##resize", ImVec2(availW, SEPARATOR_HEIGHT));
						
						// Visual feedback
						bool isHovered = ImGui::IsItemHovered();
						if(isHovered) {
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
						}
						
						// Draw separator line
						ImU32 sepColor = isHovered ? IM_COL32(150, 150, 150, 255) : IM_COL32(80, 80, 80, 255);
						float sepThickness = isHovered ? 3.0f : 1.0f;
						dl->AddLine(
							ImVec2(sepPos.x, sepPos.y + SEPARATOR_HEIGHT/2),
							ImVec2(sepPos.x + availW, sepPos.y + SEPARATOR_HEIGHT/2),
							sepColor,
							sepThickness
						);
						
						// Handle drag
						if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
							float deltaY = ImGui::GetIO().MouseDelta.y;
							float newHeight = track->getHeight() + deltaY;
							track->setHeight(newHeight);
						}
					} else {
						// Collapsed - just show a thin separator
						ImGui::Spacing();
					}
					
					ImGui::PopID();
				}
			}
		}
		ImGui::End();

		if(show != showWindow.get()) showWindow = show;
	}

private:
	
	std::vector<transportTrack*> subscribedTracks;

	// --- Drag state for ruler ---
	enum DragMode { DRAG_NONE, DRAG_LOOP_START, DRAG_LOOP_END, DRAG_LOOP_MOVE };
	DragMode dragMode = DRAG_NONE;
	double dragAnchorBeat = 0.0;

	// --- Grid helpers ---
	int gridTicks() const {
		int div = gridDiv.get();
		int mode = gridMode.get();
		if(div == 0) return 0; // None
		
		int baseTicks = 0;
		if(div == 1) baseTicks = 96;      // Bar
		else if(div == 2) baseTicks = 48; // 1st
		else if(div == 3) baseTicks = 24; // 2nd
		else if(div == 4) baseTicks = 12; // 4th
		else if(div == 5) baseTicks = 6;  // 8th
		else if(div == 6) baseTicks = 3;  // 16th
		else if(div == 7) baseTicks = 1.5;// 32nd
		else if(div == 8) baseTicks = 0.75; // 64th
		
		if(mode == 1) baseTicks = baseTicks * 1.5; // Dotted
		else if(mode == 2) baseTicks = baseTicks / 1.5; // Triplet (approximate)
		
		return baseTicks;
	}

	double totalBeats() const {
		double beatsPerBar = double(numerator.get()) * (4.0 / double(denominator.get()));
		return totalBars.get() * beatsPerBar;
	}

	void handleLoop(double prev){
		if(loopEnabled.get() == 1){
			double ls = loopStartBeat.get();
			double le = loopEndBeat.get();
			if(ls < le){
				// If playhead crossed loop end
				if(prev < le && beatAcc >= le){
					beatAcc = ls + (beatAcc - le);
					// Update base to reflect the loop jump
					if(transportRunning){
						beatAccBase = beatAcc;
						lastTimeMs = ofGetElapsedTimeMillis();
					}
				}
			}
		}
	}

	void resetTransport(){
		beatAcc = 0.0;
		beatAccBase = 0.0;
		lastExternalBeat = -1.0;
		jumpTrigFramesRemaining = 0;
		updateOutputs();
	}

	void updateOutputs(){
		double beatsPerBar = double(numerator.get()) * (4.0 / double(denominator.get()));
		
		int currentBar = int(beatAcc / beatsPerBar);
		double beatInBar = beatAcc - (currentBar * beatsPerBar);
		
		ppq24 = int(beatAcc * 24.0);
		ppq24f = float(beatAcc * 24.0);
		phasor = float(std::fmod(beatAcc, 1.0));
		beatTransport = float(beatAcc);
		bar = currentBar;
		barBeat = float(beatInBar);
	}

	// --- Transport Header Controls ---
	void drawTransportHeader(){
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8,4));
		
		// Play/Stop
		if(ImGui::Button(play.get() ? "Stop" : "Play")){
			play = !play.get();
		}
		
		ImGui::SameLine();
		if(ImGui::Button("Reset")){
			resetTransport();
		}

		// BPM
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		float bpmVal = bpm.get();
		if(ImGui::DragFloat("BPM", &bpmVal, 1.0f, 1.0f, 999.0f, "%.1f")){
			bpm = bpmVal;
		}

		// Time Signature
		ImGui::SameLine();
		ImGui::SetNextItemWidth(50);
		int numVal = numerator.get();
		if(ImGui::DragInt("##num", &numVal, 0.1f, 1, 32)){
			numerator = numVal;
		}
		
		ImGui::SameLine();
		ImGui::Text("/");
		
		ImGui::SameLine();
		ImGui::SetNextItemWidth(50);
		int denVal = denominator.get();
		if(ImGui::DragInt("##den", &denVal, 0.1f, 1, 32)){
			denominator = denVal;
		}

		// Zoom
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		int zoomVal = zoomBars.get();
		if(ImGui::DragInt("Zoom", &zoomVal, 0.1f, 1, 128)){
			zoomBars = zoomVal;
		}

		// Grid
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90);
		ImGui::Combo("##grid", (int*)&gridDiv.get(),
			"None\0""Bar\0""1st\0""2nd\0""4th\0""8th\0""16th\0""32nd\0""64th\0");

		ImGui::SameLine();
		ImGui::SetNextItemWidth(90);
		ImGui::Combo("##gridmode", (int*)&gridMode.get(),
			"Straight\0""Dotted\0""Triplet\0");

		ImGui::PopStyleVar();
	}

	
	void drawTimeline(float height){
		drawTransportHeader();
		
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		float availW = ImGui::GetContentRegionAvail().x;
		ImVec2 size(availW, height);
		
		// Create interactive button - this advances the cursor
		if(size.x==0) size.x = 100;
		ImGui::InvisibleButton("##rulerArea", size);
		
		// Capture the actual rect for drawing
		ImVec2 p = ImGui::GetItemRectMin();
		ImVec2 s = ImGui::GetItemRectSize();

		float x0 = p.x;
		float y0 = p.y;
		float x1 = p.x + s.x;
		float y1 = p.y + s.y;

		int barsVisible = zoomBars.get();
		int viewStartBar = 0;
		int viewEndBar   = viewStartBar + barsVisible;

		double beatsPerBar = double(numerator.get()) * (4.0 / double(denominator.get()));

		// Background
		// Calculate the split X coordinate where the timeline ends (totalBars)
		// Formula: x0 + (totalBars / barsVisible) * width
		float splitX = x0 + (float(totalBars.get()) / float(barsVisible)) * s.x;

		// Clamp splitX to the right edge (x1) to handle cases where totalBars > barsVisible
		float drawSplitX = std::min(splitX, x1);

		// Section 1: Active timeline area (from start to totalBars)
		// Color: Standard dark gray (30, 30, 30)
		dl->AddRectFilled(ImVec2(x0, y0), ImVec2(drawSplitX, y1), IM_COL32(30, 30, 30, 255));

		// Section 2: Beyond timeline area (from totalBars to end of view)
		// Only draw if the split point is visible (i.e., totalBars < barsVisible)
		if (drawSplitX < x1) {
			// Color: 20% darker (24, 24, 24)
			dl->AddRectFilled(ImVec2(drawSplitX, y0), ImVec2(x1, y1), IM_COL32(12, 12, 12, 255));
		}

		auto barToX = [&](int bar){
			return x0 + (float(bar - viewStartBar) / barsVisible) * s.x;
		};

		auto xToBar = [&](float x){
			float t = ofClamp((x - x0) / s.x, 0.f, 1.f);
			return viewStartBar + int(std::floor(t * barsVisible));
		};

		auto xToBeat = [&](float x){
			float t = ofClamp((x - x0) / s.x, 0.f, 1.f);
			double totalViewBeats = barsVisible * beatsPerBar;
			double beatOffset = t * totalViewBeats;
			return viewStartBar * beatsPerBar + beatOffset;
		};

		auto xToBeatSnapped = [&](float x){
			double beat = xToBeat(x);
			int gTicks = gridTicks();
			if(gTicks > 0){
				double gridBeats = gTicks / 24.0;
				beat = std::round(beat / gridBeats) * gridBeats;
			}
			return beat;
		};

		auto beatToBar = [&](double beat){
			return int(beat / beatsPerBar);
		};

		// ---------- RULER ----------
		for(int bar = viewStartBar; bar <= viewEndBar; ++bar){
			float bx = barToX(bar);

			// Bar lines
			dl->AddLine(
				ImVec2(bx, y0),
				ImVec2(bx, y1),
				IM_COL32(120,120,120,255),
				2.0f
			);

			// Display bars 1-based
			char buf[16];
			snprintf(buf, 16, "%d", bar + 1);
			dl->AddText(ImVec2(bx + 4, y0 + 4),
				IM_COL32(220,220,220,220), buf);

			// Grid lines
			if(bar < viewEndBar){
				int gTicks = gridTicks();
				if(gTicks > 0){
					double gridBeats = gTicks / 24.0;
					double viewStartBeats = viewStartBar * beatsPerBar;
					double viewEndBeats = viewStartBeats + (barsVisible * beatsPerBar);
					double barStartBeats = bar * beatsPerBar;
					double barEndBeats = (bar + 1) * beatsPerBar;

					for(double b = barStartBeats + gridBeats; b < barEndBeats; b += gridBeats){
						if(b < viewStartBeats || b > viewEndBeats) continue;
						
						float x = x0 + float((b - viewStartBeats) / (barsVisible * beatsPerBar)) * s.x;

						dl->AddLine(
							ImVec2(x, y0 + 26),
							ImVec2(x, y1),
							IM_COL32(70,70,70,100),
							0.5f
						);
					}
				}
			}
		}

		// ---------- LOOP ----------
		if(loopEnabled.get() == 1){
			double viewStartBeats = viewStartBar * beatsPerBar;
			double totalViewBeats = barsVisible * beatsPerBar;
			
			float lx1 = x0 + float((loopStartBeat.get() - viewStartBeats) / totalViewBeats) * s.x;
			float lx2 = x0 + float((loopEndBeat.get() - viewStartBeats) / totalViewBeats) * s.x;

			// Body
			dl->AddRectFilled(
				ImVec2(lx1, y0),
				ImVec2(lx2, y1),
				IM_COL32(80,80,160,70)
			);

			// Handles
			dl->AddRectFilled(ImVec2(lx1-4, y0), ImVec2(lx1+4, y1),
				IM_COL32(160,160,255,220));
			dl->AddRectFilled(ImVec2(lx2-4, y0), ImVec2(lx2+4, y1),
				IM_COL32(160,160,255,220));
		}

		// ---------- PLAYHEAD ----------
		double viewStartBeats = viewStartBar * beatsPerBar;
		double totalViewBeats = barsVisible * beatsPerBar;

		float playX = x0 + float((beatAcc - viewStartBeats) / totalViewBeats) * s.x;

		dl->AddLine(
			ImVec2(playX, y0),
			ImVec2(playX, y1),
			IM_COL32(255, 80, 80, 255),
			2.5f
		);


		// ---------- INTERACTION ----------
		ImVec2 mouse = ImGui::GetIO().MousePos;

		if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
			double beatAtMouse = xToBeatSnapped(mouse.x);
			dragMode = DRAG_NONE;

			if(loopEnabled.get() == 1){
				double loopStart = loopStartBeat.get();
				double loopEnd = loopEndBeat.get();
				double thresholdBeats = std::max(1, gridTicks()) / 24.0;
				
				if(abs(beatAtMouse - loopStart) <= thresholdBeats)
					dragMode = DRAG_LOOP_START;
				else if(abs(beatAtMouse - loopEnd) <= thresholdBeats)
					dragMode = DRAG_LOOP_END;
				else if(beatAtMouse > loopStart && beatAtMouse < loopEnd){
					dragMode = DRAG_LOOP_MOVE;
					dragAnchorBeat = beatAtMouse;
				}
			}

			// If not dragging loop → move playhead (only in Internal mode)
			if(dragMode == DRAG_NONE && clockMode.get() == 0){
				beatAcc = beatAtMouse;
				jumpTrigFramesRemaining = 3;
				
				if(transportRunning){
					beatAccBase = beatAcc;
					lastTimeMs = ofGetElapsedTimeMillis();
				}
				
				updateOutputs();
			}
		}

		if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
			double beatAtMouse = xToBeatSnapped(mouse.x);

			if(dragMode == DRAG_LOOP_START){
				double newBeat = ofClamp(beatAtMouse, 0.0, loopEndBeat.get() - (1.0/24.0));
				loopStartBeat = float(newBeat);
			}
			else if(dragMode == DRAG_LOOP_END){
				double totBeats = totalBeats();
				double newBeat = ofClamp(beatAtMouse, loopStartBeat.get() + (1.0/24.0), totBeats);
				loopEndBeat = float(newBeat);
			}
			else if(dragMode == DRAG_LOOP_MOVE){
				double delta = beatAtMouse - dragAnchorBeat;
				double len = loopEndBeat.get() - loopStartBeat.get();
				double totBeats = totalBeats();

				double newStart = ofClamp(loopStartBeat.get() + delta, 0.0, totBeats - len);
				loopStartBeat = float(newStart);
				loopEndBeat = float(newStart + len);
				dragAnchorBeat = beatAtMouse;
			}
		}

		if(ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
			dragMode = DRAG_NONE;
		}
		
		// Cursor already advanced by InvisibleButton, no Dummy needed!
	}

	// =====================================================
	// State
	// =====================================================

	double beatAcc = 0.0;
	double lastExternalBeat = -1.0;
	int jumpTrigFramesRemaining = 0;
	
	bool transportRunning = false;
	uint64_t lastTimeMs = 0;
	double beatAccBase = 0.0;
	double lastBpm = 120.0;

	ofParameter<int> clockMode;
	ofParameter<int> ppqInput;
	ofParameter<float> beatTransportInput;

	ofParameter<int> play;
	ofParameter<float> bpm;
	ofParameter<void> reset;

	ofParameter<int> numerator;
	ofParameter<int> denominator;
	ofParameter<int> totalBars;

	ofParameter<int> loopEnabled;
	ofParameter<float> loopStartBeat;
	ofParameter<float> loopEndBeat;
	ofParameter<int> wrapAtEnd;

	ofParameter<bool> showWindow;
	ofParameter<int> zoomBars;
	
	ofParameter<int> gridDiv;
	ofParameter<int> gridMode;


	ofParameter<int>   ppq24;
	ofParameter<float> ppq24f;
	ofParameter<float> phasor;
	ofParameter<float> beatTransport;
	ofParameter<int>   bar;
	ofParameter<float> barBeat;
	ofParameter<int>   jumpTrig;

	ofEventListeners listeners;
};

#endif /* ppqTimeline_h */
