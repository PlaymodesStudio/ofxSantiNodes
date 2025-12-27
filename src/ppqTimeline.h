#ifndef ppqTimeline_h
#define ppqTimeline_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>

class ppqTimeline : public ofxOceanodeNodeModel {
public:
	ppqTimeline() : ofxOceanodeNodeModel("PPQ Timeline") {}

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
		addParameter(zoomBars.set("Zoom Bars", 8, 1, 128));
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

		resetTransport();
	}

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
			if(play.get() == 0) return;

			float dt = ofGetLastFrameTime();
			if(dt <= 0.f) return;

			// Accumulate in BEATS (quarter notes)
			beatAcc += dt * (bpm.get() / 60.f);
		}

		handleLoop(prev);

		// --- wrap whole timeline at the end ---
		if(wrapAtEnd.get() == 1){
			double totBeats = totalBeats();
			if(totBeats > 0 && beatAcc >= totBeats){
				beatAcc = std::fmod(beatAcc, totBeats);
			}
		}

		updateOutputs();

	}


	// ---------- GUI ----------
	void draw(ofEventArgs &) override {
		if(!showWindow.get()) return;

		std::string title =
			(canvasID == "Canvas" ? "" : canvasID + "/") +
			"PPQ Timeline " + ofToString(getNumIdentifier());

		bool show = showWindow.get();
		if(ImGui::Begin(title.c_str(), &show)){
			drawTimeline();
		}
		ImGui::End();
		
		if(show != showWindow.get()){
			showWindow = show;
		}
	}

private:

	// =====================================================
	// Interaction state
	// =====================================================

	enum DragMode {
		DRAG_NONE = 0,
		DRAG_LOOP_START,
		DRAG_LOOP_END,
		DRAG_LOOP_MOVE
	};

	DragMode dragMode = DRAG_NONE;
	double dragAnchorBeat = 0.0;

	// =====================================================
	// Timing helpers
	// =====================================================

	int ticksPerBeat() const {
		switch(denominator.get()){
			case 1:  return 96;
			case 2:  return 48;
			case 4:  return 24;
			case 8:  return 12;
			case 16: return 6;
			default: return std::max(1, int(96.f / denominator.get()));
		}
	}

	int ticksPerBar() const {
		return ticksPerBeat() * numerator.get();
	}

	int totalTicks() const {
		return ticksPerBar() * totalBars.get();
	}

	double totalBeats() const {
		return totalTicks() / 24.0;
	}

	// =====================================================
	// Core logic
	// =====================================================

	void resetTransport(){
		beatAcc = 0.0;
		ppq24 = 0;
		ppq24f = 0.f;
		phasor = 0.f;
		beatTransport = 0.f;
		bar = 0;
		barBeat = 0.f;
		jumpTrig = 0;
		jumpTrigFramesRemaining = 0;
		lastExternalBeat = -1.0;
	}

	void handleLoop(double prevBeats){
		if(loopEnabled.get() == 0) return;

		double startBeats = loopStartBeat.get();
		double endBeats = loopEndBeat.get();

		double len = endBeats - startBeats;
		if(len <= 0.0) return;

		// Only wrap if playback advanced across the loop end.
		// This prevents immediate wrap when user seeks beyond loop end.
		if(prevBeats < endBeats && beatAcc >= endBeats){
			beatAcc = startBeats + std::fmod(beatAcc - startBeats, len);
			if(beatAcc < startBeats) beatAcc += len; // safety
			
			// Loop wrap is a jump - trigger for 3 frames
			jumpTrigFramesRemaining = 3;
		}
	}


	void updateOutputs(){
		double totBeats = totalBeats();
		if(totBeats <= 0) return;

		// Wrap to timeline length
		double wrappedBeats = std::fmod(beatAcc, totBeats);
		if(wrappedBeats < 0) wrappedBeats += totBeats;

		// beatTransport is primary output (wrapped to timeline)
		beatTransport = float(wrappedBeats);

		// PPQ outputs derived from beatTransport
		ppq24f = beatTransport * 24.0f;
		ppq24 = int(std::floor(ppq24f));

		// Phasor (wrapped 0-1 over timeline)
		phasor = float(wrappedBeats / totBeats);
		
		// Calculate bar and barBeat
		// beatsPerBar calculated directly from time signature
		double beatsPerBar = double(numerator.get()) * (4.0 / double(denominator.get()));
		if(beatsPerBar > 0){
			bar = int(std::floor(wrappedBeats / beatsPerBar));
			barBeat = float(wrappedBeats - (bar * beatsPerBar));
		}
	}

	// =====================================================
	// Timeline UI
	// =====================================================

	int gridTicks() const {
		int base;

		switch(gridDiv.get()){
			case 0: return 0;                    // None
			case 1: base = ticksPerBar(); break; // Bar
			case 2: base = 96; break; // 1st (whole note)
			case 3: base = 48; break; // 2nd (half note)
			case 4: base = 24; break; // 4th
			case 5: base = 12; break; // 8th
			case 6: base = 6;  break; // 16th
			case 7: base = 3;  break; // 32nd
			case 8: base = 1;  break; // 64th (actually 96th in PPQ24, but close enough)
			default: base = 24;
		}

		switch(gridMode.get()){
			case 0: return base;          // straight
			case 1: return base * 3 / 2;  // dotted
			case 2: return base * 2 / 3;  // triplet
			default: return base;
		}
	}

	void drawTransportHeader(){
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6,4));

		// ---- Clock Mode ----
		ImGui::Text("Clock");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90);
		ImGui::Combo("##clockmode", (int*)&clockMode.get(),
			"Internal\0""External\0");

		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();

		// Only show transport controls in Internal mode
		if(clockMode.get() == 0){
			// ---- Play / Stop ----
			if(play.get() == 1){
				if(ImGui::Button("Stop")){
					play = 0;
				}
			}else{
				if(ImGui::Button("Play")){
					play = 1;
				}
			}

			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();

			// ---- BPM ----
			float bpmVal = bpm.get();
			ImGui::Text("BPM");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80);
			if(ImGui::InputFloat("##bpm", &bpmVal, 0.f, 0.f, "%.2f")){
				bpm = ofClamp(bpmVal, 1.f, 999.f);
			}

			ImGui::SameLine();
			ImGui::Spacing();
			ImGui::SameLine();
		}

		// ---- Time signature ----
		int num = numerator.get();
		int den = denominator.get();

		ImGui::Text("Time");
		ImGui::SameLine();

		ImGui::SetNextItemWidth(40);
		if(ImGui::InputInt("##num", &num, 0, 0)){
			numerator = ofClamp(num, 1, 32);
		}

		ImGui::SameLine();
		ImGui::Text("/");
		ImGui::SameLine();

		ImGui::SetNextItemWidth(40);
		if(ImGui::InputInt("##den", &den, 0, 0)){
			denominator = ofClamp(den, 1, 32);
		}
		
		// ---- Grid settings ----
		ImGui::SameLine();
		ImGui::Text("Grid");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90);
		ImGui::Combo("##griddiv", (int*)&gridDiv.get(),
			"None\0""Bar\0""1st\0""2nd\0""4th\0""8th\0""16th\0""32nd\0""64th\0");

		ImGui::SameLine();
		ImGui::SetNextItemWidth(90);
		ImGui::Combo("##gridmode", (int*)&gridMode.get(),
			"Straight\0""Dotted\0""Triplet\0");

		ImGui::PopStyleVar();
	}

	
	void drawTimeline(){
		drawTransportHeader();
		ImGui::Separator();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 size = ImGui::GetContentRegionAvail();

		if(size.x < 400) size.x = 800;
		if(size.y < 100) size.y = 180;

		float x0 = pos.x;
		float y0 = pos.y;
		float x1 = pos.x + size.x;
		float y1 = pos.y + size.y;

		// Background
		dl->AddRectFilled(
			ImVec2(x0, y0),
			ImVec2(x1, y1),
			IM_COL32(30, 30, 30, 255)
		);

		int barsVisible = zoomBars.get();
		int viewStartBar = 0;
		int viewEndBar   = viewStartBar + barsVisible;

		// Calculate beats per bar directly from time signature
		// In PPQ24 system, a quarter note (denominator=4) = 1 beat
		// Other denominators scale accordingly
		double beatsPerBar = double(numerator.get()) * (4.0 / double(denominator.get()));

		auto barToX = [&](int bar){
			return x0 + (float(bar - viewStartBar) / barsVisible) * size.x;
		};

		auto xToBar = [&](float x){
			float t = ofClamp((x - x0) / size.x, 0.f, 1.f);
			return viewStartBar + int(std::floor(t * barsVisible));
		};

		auto xToBeat = [&](float x){
			float t = ofClamp((x - x0) / size.x, 0.f, 1.f);
			double totalViewBeats = barsVisible * beatsPerBar;
			double beatOffset = t * totalViewBeats;
			return viewStartBar * beatsPerBar + beatOffset;
		};

		auto xToBeatSnapped = [&](float x){
			double beat = xToBeat(x);
			int gTicks = gridTicks();
			if(gTicks > 0){
				// Snap to grid divisions
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

			// Bar lines - prominent
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

			// Grid lines - subtle, darker and thinner
			if(bar < viewEndBar){
				int gTicks = gridTicks();
				// Only draw grid if not "None" (gTicks == 0)
				if(gTicks > 0){
					double gridBeats = gTicks / 24.0;
					double viewStartBeats = viewStartBar * beatsPerBar;
					double viewEndBeats = viewStartBeats + (barsVisible * beatsPerBar);
					double barStartBeats = bar * beatsPerBar;
					double barEndBeats = (bar + 1) * beatsPerBar;

					for(double b = barStartBeats + gridBeats; b < barEndBeats; b += gridBeats){
						if(b < viewStartBeats || b > viewEndBeats) continue;
						
						float x = x0 + float((b - viewStartBeats) / (barsVisible * beatsPerBar)) * size.x;

						// Grid lines - darker and thinner than bar lines
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
			
			float lx1 = x0 + float((loopStartBeat.get() - viewStartBeats) / totalViewBeats) * size.x;
			float lx2 = x0 + float((loopEndBeat.get() - viewStartBeats) / totalViewBeats) * size.x;

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

		float playX = x0 + float((beatAcc - viewStartBeats) / totalViewBeats) * size.x;

		dl->AddLine(
			ImVec2(playX, y0),
			ImVec2(playX, y1),
			IM_COL32(255, 80, 80, 255),
			2.5f
		);


		// ---------- INTERACTION ----------
		ImGui::InvisibleButton("timeline", size);
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
				jumpTrigFramesRemaining = 3; // User-initiated seek is a jump - stay high for 3 frames
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

		ImGui::Dummy(size);
	}

	// =====================================================
	// State
	// =====================================================

	double beatAcc = 0.0;
	double lastExternalBeat = -1.0; // For jump detection in external mode
	int jumpTrigFramesRemaining = 0; // Frame counter for jump trigger duration

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
