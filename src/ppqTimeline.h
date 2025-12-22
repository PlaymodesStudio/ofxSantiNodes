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
		addParameter(play.set("Play", false));
		addParameter(bpm.set("BPM", 120.f, 1.f, 999.f));
		addParameter(reset.set("Reset"));

		// ---------- External PPQ Input ----------
		addSeparator("PPQ  INPUT",ofColor(240,240,240));
		addParameter(ppqInput.set("PPQ In", 0, 0, INT_MAX));

		// ---------- Meter ----------
		addSeparator("TIME MEASURE",ofColor(240,240,240));
		addParameter(numerator.set("Numerator", 4, 1, 32));
		addParameter(denominator.set("Denominator", 4, 1, 32));

		// ---------- Structure ----------
		addSeparator("LENGTH",ofColor(240,240,240));
		addParameter(totalBars.set("Bars", 8, 1, 2048));

		// ---------- Loop ----------
		addSeparator("LOOP",ofColor(240,240,240));
		addParameter(loopEnabled.set("Loop", false));
		addParameter(loopStartBar.set("Loop Start", 0, 0, 2047)); // 0-based
		addParameter(loopEndBar.set("Loop End", 4, 1, 2048));     // exclusive
		addParameter(wrapAtEnd.set("Wrap End", true));

		// ---------- UI ----------
		addSeparator("GUI",ofColor(240,240,240));
		addParameter(showWindow.set("Show", false));
		addParameter(zoomBars.set("Zoom Bars", 8, 1, 128));
		addParameterDropdown(gridDiv, "Grid", 2, {
			"4th", "8th", "16th", "32nd"
		});

		addParameterDropdown(gridMode, "Grid Mode", 0, {
			"Straight", "Dotted", "Triplet"
		});


		// ---------- Outputs ----------
		addSeparator("OUT",ofColor(240,240,240));
		addOutputParameter(ppq24.set("PPQ 24", 0, 0, INT_MAX));
		addOutputParameter(phasor.set("Phasor", 0.f, 0.f, 1.f));

		listeners.push(reset.newListener([this]() {
			resetTransport();
		}));

		resetTransport();
	}

	// ---------- Clock ----------
	void update(ofEventArgs &) override {
		double prev = ppqAcc;

		// External or Internal clock mode
		if(clockMode.get() == 1){ // External
			ppqAcc = ppqInput.get();
		}
		else{ // Internal
			if(!play) return;

			float dt = ofGetLastFrameTime();
			if(dt <= 0.f) return;

			ppqAcc += dt * (bpm.get() * 24.f / 60.f);
		}

		handleLoop(prev);

		// --- wrap whole timeline ---
		if(!loopEnabled.get() && wrapAtEnd.get()){
			double tot = double(totalTicks());
			if(tot > 0.0 && ppqAcc >= tot){
				ppqAcc = std::fmod(ppqAcc, tot);
			}
		}

		updateOutputs();

	}


	// ---------- GUI ----------
	void draw(ofEventArgs &) override {
		if(!showWindow) return;

		std::string title =
			(canvasID == "Canvas" ? "" : canvasID + "/") +
			"PPQ Timeline " + ofToString(getNumIdentifier());

		if(ImGui::Begin(title.c_str(), (bool *)&showWindow.get())){
			drawTimeline();
		}
		ImGui::End();
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
	int dragAnchorBar = 0;

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

	// =====================================================
	// Core logic
	// =====================================================

	void resetTransport(){
		ppqAcc = 0.0;
		ppq24 = 0;
		phasor = 0.f;
	}

	void handleLoop(double prevPPQ){
		if(!loopEnabled) return;

		int tpb = ticksPerBar();
		double startPPQ = double(loopStartBar.get()) * double(tpb);
		double endPPQ   = double(loopEndBar.get())   * double(tpb);

		double len = endPPQ - startPPQ;
		if(len <= 0.0) return;

		// Only wrap if playback advanced across the loop end.
		// This prevents immediate wrap when user seeks beyond loop end.
		if(prevPPQ < endPPQ && ppqAcc >= endPPQ){
			ppqAcc = startPPQ + std::fmod(ppqAcc - startPPQ, len);
			if(ppqAcc < startPPQ) ppqAcc += len; // safety
		}
	}


	void updateOutputs(){
		int tot = totalTicks();
		if(tot <= 0) return;

		int ip = std::max(0, int(std::floor(ppqAcc)));
		ppq24 = ip;

		int wrapped = ip % tot;
		if(wrapped < 0) wrapped += tot;

		phasor = float(wrapped) / float(tot);
	}

	// =====================================================
	// Timeline UI
	// =====================================================

	int gridTicks() const {
		int base;

		switch(gridDiv.get()){
			case 0: base = 24; break; // 4th
			case 1: base = 12; break; // 8th
			case 2: base = 6;  break; // 16th
			case 3: base = 3;  break; // 32nd
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
			if(play.get()){
				if(ImGui::Button("Stop")){
					play = false;
				}
			}else{
				if(ImGui::Button("Play")){
					play = true;
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
		ImGui::SetNextItemWidth(70);
		ImGui::Combo("##griddiv", (int*)&gridDiv.get(),
			"4th\0""8th\0""16th\0""32nd\0");

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

		int tpb = ticksPerBar();
		int barsVisible = zoomBars.get();
		int viewStartBar = 0;
		int viewEndBar   = viewStartBar + barsVisible;

		auto barToX = [&](int bar){
			return x0 + (float(bar - viewStartBar) / barsVisible) * size.x;
		};

		auto xToBar = [&](float x){
			float t = ofClamp((x - x0) / size.x, 0.f, 1.f);
			return viewStartBar + int(std::floor(t * barsVisible));
		};

		// ---------- RULER ----------
		for(int bar = viewStartBar; bar <= viewEndBar; ++bar){
			float bx = barToX(bar);

			dl->AddLine(
				ImVec2(bx, y0),
				ImVec2(bx, y1),
				IM_COL32(100,100,100,200),
				2.0f
			);

			// Display bars 1-based
			char buf[16];
			snprintf(buf, 16, "%d", bar + 1);
			dl->AddText(ImVec2(bx + 4, y0 + 4),
				IM_COL32(220,220,220,220), buf);

			// Beats
			if(bar < viewEndBar){
				int gTicks = gridTicks();
				if(gTicks > 0){
					int viewStartPPQ = viewStartBar * tpb;
					int viewEndPPQ   = viewStartPPQ + barsVisible * tpb;

					for(int t = viewStartPPQ; t <= viewEndPPQ; t += gTicks){
						float x = x0 +
							(float(t - viewStartPPQ) / (barsVisible * tpb)) * size.x;

						dl->AddLine(
							ImVec2(x, y0 + 26),
							ImVec2(x, y1),
							IM_COL32(80,80,80,120),
							1.0f
						);
					}
				}
			}
		}

		// ---------- LOOP ----------
		if(loopEnabled){
			float lx1 = barToX(loopStartBar.get());
			float lx2 = barToX(loopEndBar.get());

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
		float viewStartPPQ = viewStartBar * tpb;
		float viewPPQ      = barsVisible * tpb;

		float playX = x0 +
			(float(ppqAcc - viewStartPPQ) / viewPPQ) * size.x;

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
			int barAtMouse = xToBar(mouse.x);
			dragMode = DRAG_NONE;

			if(loopEnabled){
				if(abs(barAtMouse - loopStartBar.get()) <= 0)
					dragMode = DRAG_LOOP_START;
				else if(abs(barAtMouse - loopEndBar.get()) <= 0)
					dragMode = DRAG_LOOP_END;
				else if(barAtMouse > loopStartBar.get() &&
						barAtMouse < loopEndBar.get()){
					dragMode = DRAG_LOOP_MOVE;
					dragAnchorBar = barAtMouse;
				}
			}

			// If not dragging loop → move playhead (only in Internal mode)
			if(dragMode == DRAG_NONE && clockMode.get() == 0){
				ppqAcc = barAtMouse * tpb;
				updateOutputs();
			}
		}

		if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
			int barAtMouse = xToBar(mouse.x);

			if(dragMode == DRAG_LOOP_START){
				loopStartBar = ofClamp(barAtMouse, 0, loopEndBar.get()-1);
			}
			else if(dragMode == DRAG_LOOP_END){
				loopEndBar = ofClamp(barAtMouse, loopStartBar.get()+1, totalBars.get());
			}
			else if(dragMode == DRAG_LOOP_MOVE){
				int delta = barAtMouse - dragAnchorBar;
				int len = loopEndBar.get() - loopStartBar.get();

				int newStart = ofClamp(loopStartBar.get()+delta, 0, totalBars.get()-len);
				loopStartBar = newStart;
				loopEndBar   = newStart + len;
				dragAnchorBar = barAtMouse;
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

	double ppqAcc = 0.0;

	ofParameter<int> clockMode;
	ofParameter<int> ppqInput;

	ofParameter<bool> play;
	ofParameter<float> bpm;
	ofParameter<void> reset;

	ofParameter<int> numerator;
	ofParameter<int> denominator;
	ofParameter<int> totalBars;

	ofParameter<bool> loopEnabled;
	ofParameter<int> loopStartBar;
	ofParameter<int> loopEndBar;
	ofParameter<bool> wrapAtEnd;

	ofParameter<bool> showWindow;
	ofParameter<int> zoomBars;
	
	ofParameter<int> gridDiv;
	ofParameter<int> gridMode;


	ofParameter<int>   ppq24;
	ofParameter<float> phasor;

	ofEventListeners listeners;
};

#endif /* ppqTimeline_h */
