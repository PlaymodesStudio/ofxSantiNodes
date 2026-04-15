#ifndef gateTrack_h
#define gateTrack_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"
#include "ppqTimeline.h"
#include "transportTrack.h"

// Use same struct as gateTrack - no need to redefine
// If gateTrack.h is included elsewhere, this will work
// Otherwise we give it a unique name
#ifndef GateRegion_DEFINED
#define GateRegion_DEFINED
struct GateRegion {
	float start;
	float length;
	float end() const { return start + length; }
};
#endif

class gateTrack : public ofxOceanodeNodeModel, public transportTrack {
public:
	gateTrack() : ofxOceanodeNodeModel("Multi Gate Track") {
		color = ofColor(255, 100, 100);
	}

	~gateTrack() {
		if(currentTimeline) currentTimeline->unsubscribeTrack(this);
	}

	void setup() override {
		refreshTimelineList();
		addParameterDropdown(timelineSelect, "Timeline", 0, timelineOptions);
		addParameter(trackName.set("Track Name", "MultiGate " + ofToString(getNumIdentifier())));
		addParameter(numLanes.set("Num Lanes", 4, 1, 16));
		addParameter(clipStart   .set("Clip Start",   0.f,   0.f, 999.f));
		addParameter(clipEnd     .set("Clip End",    999.f,  0.f, 999.f));
		addParameter(clipDuration.set("Clip Dur",      4.f, 0.25f, 64.f));
		addParameter(clipLoop    .set("Loop",         false));

		// Vector output for multiple gates
		addOutputParameter(gateOutput.set("Gate[]", {0}, {0}, {1}));
		addOutputParameter(trigOutput.set("Trig[]", {0}, {0}, {1}));

		listeners.push(timelineSelect.newListener([this](int &val){ updateSubscription(); }));
		listeners.push(numLanes.newListener([this](int &val){
			resizeLanes(val);
		}));
		
		gateOutput.setSerializable(false);
		trigOutput.setSerializable(false);
		
		// Initialize with default number of lanes
		resizeLanes(numLanes.get());
		
		updateSubscription();
	}

	void update(ofEventArgs &args) override {
		float currentBeat = 0.0f;
		bool inClip = false;
		if(currentTimeline) {
			double beat = currentTimeline->getBeatPosition();
			double dur  = (double)clipDuration.get();
			double cs0  = getClipStartAt(0); // primary clip start — data is in this space
			int nClips = getClipCount();
			for(int ci = 0; ci < nClips; ci++) {
				double cs = getClipStartAt(ci);
				double ce = getClipEndAt(ci);
				if(beat >= cs && beat < ce) {
					double local = beat - cs;
					if(clipLoop.get() && dur > 0.001) local = std::fmod(local, dur);
					currentBeat = (float)(cs0 + local); // remap to primary clip space
					inClip = true;
					break;
				}
			}
			if(!inClip) {
				gateOutput = vector<float>(numLanes.get(), 0.0f);
				trigOutput = vector<float>(numLanes.get(), 0.0f);
				std::fill(lastActiveState.begin(), lastActiveState.end(), false);
				return;
			}
		}

		// Prepare output vectors
		vector<float> gateOutputs(numLanes.get(), 0.0f);
		vector<float> trigOutputs(numLanes.get(), 0.0f);
		
		// Check each lane
		for(int lane = 0; lane < numLanes.get() && lane < gateLanes.size(); lane++) {
			bool active = false;
			bool newTrig = false;
			
			for(const auto& g : gateLanes[lane]) {
				if(currentBeat >= g.start && currentBeat < g.end()) {
					active = true;
					if(lane < lastActiveState.size() && !lastActiveState[lane]) newTrig = true;
					break;
				}
			}
			
			gateOutputs[lane] = active ? 1.0f : 0.0f;
			trigOutputs[lane] = newTrig ? 1.0f : 0.0f;
			
			if(lane < lastActiveState.size()) {
				lastActiveState[lane] = active;
			}
		}
		
		gateOutput = gateOutputs;
		trigOutput = trigOutputs;
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
		float zoom = ofxOceanodeShared::getZoomLevel();
		
		// Calculate lane height
		float laneHeight = trackHeight / numLanes.get();
		float minLaneHeight = 15.0f;  // Reduced from 30px to allow narrower lanes
		
		// If lanes are too small, adjust track height
		if(laneHeight < minLaneHeight) {
			trackHeight = minLaneHeight * numLanes.get();
			laneHeight = minLaneHeight;
			sz.y = trackHeight;
		}
		
		// 1. Create the interaction button for entire track
		std::string buttonId = "##trkBtn" + ofToString(getNumIdentifier());
		ImGui::InvisibleButton(buttonId.c_str(), sz);
		
		// 2. Capture the ACTUAL screen rect
		ImVec2 p = ImGui::GetItemRectMin();
		ImVec2 s = ImGui::GetItemRectSize();
		ImVec2 endP = ImGui::GetItemRectMax();
		
		// 3. Draw background
		dl->AddRectFilled(p, endP, IM_COL32(40, 40, 40, 255));
		dl->AddRect(p, endP, IM_COL32(60, 60, 60, 255));
		
		// 4. Get mouse info for interaction
		ImVec2 mousePos = ImGui::GetMousePos();
		bool isHovered = ImGui::IsItemHovered();
		bool isLeftClick = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		bool isRightClick = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
		bool isDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
		bool isReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
		
		// Determine which lane the mouse is in
		int hoveredLane = -1;
		if(isHovered) {
			float relY = mousePos.y - p.y;
			hoveredLane = (int)(relY / laneHeight);
			if(hoveredLane >= numLanes.get()) hoveredLane = numLanes.get() - 1;
			if(hoveredLane < 0) hoveredLane = 0;
		}
		
		// 5. Calculate helper functions
		double visibleLen = viewEnd - viewStart;
		if(visibleLen <= 0.001) return;
		
		// Get timeline info for grid
		int gridTicks = 0;
		double beatsPerBar = 4.0;
		double currentPlayheadBeat = 0.0;
		
		if(currentTimeline) {
			gridTicks = currentTimeline->getGridTicks();
			beatsPerBar = double(currentTimeline->getNumerator()) *
						  (4.0 / double(currentTimeline->getDenominator()));
			currentPlayheadBeat = currentTimeline->getBeatPosition();
		}
		
		// Coordinate conversion helpers
		auto beatToX = [&](double b) {
			return p.x + float((b - viewStart) / visibleLen) * s.x;
		};
		
		auto xToBeat = [&](float x) {
			return viewStart + ((x - p.x) / s.x) * visibleLen;
		};
		
		// Snapping function
		auto snap = [&](double b) {
			if(gridTicks <= 0) return b;
			double gridBeats = gridTicks / 24.0;
			return std::round(b / gridBeats) * gridBeats;
		};
		
		// 6. Draw grid lines (shared across all lanes)
		int viewStartBar = int(viewStart / beatsPerBar);
		int viewEndBar = int(viewEnd / beatsPerBar) + 1;
		
		for(int bar = viewStartBar; bar <= viewEndBar; ++bar) {
			double barBeat = bar * beatsPerBar;
			float barX = beatToX(barBeat);
			
			if(barX < p.x - 5 || barX > endP.x + 5) continue;
			
			// Bar line - extends through all lanes
			dl->AddLine(
				ImVec2(barX, p.y),
				ImVec2(barX, endP.y),
				IM_COL32(120, 120, 120, 255),
				2.0f
			);
			
			// Grid subdivisions
			if(gridTicks > 0 && bar < viewEndBar) {
				double gridBeats = gridTicks / 24.0;
				double nextBarBeat = (bar + 1) * beatsPerBar;
				
				for(double b = barBeat + gridBeats; b < nextBarBeat; b += gridBeats) {
					if(b < viewStart || b > viewEnd) continue;
					
					float gridX = beatToX(b);
					dl->AddLine(
						ImVec2(gridX, p.y),
						ImVec2(gridX, endP.y),
						IM_COL32(70, 70, 70, 100),
						0.5f
					);
				}
			}
		}
		
		// 6.5 Draw loop region
		if(currentTimeline) {
			double loopStart, loopEnd;
			bool loopEnabled = false;
			
			if(getLoopRegion(loopStart, loopEnd, loopEnabled) && loopEnabled) {
				float lx1 = beatToX(loopStart);
				float lx2 = beatToX(loopEnd);
				
				lx1 = std::max(lx1, p.x);
				lx2 = std::min(lx2, endP.x);
				
				dl->AddRectFilled(
					ImVec2(lx1, p.y),
					ImVec2(lx2, endP.y),
					IM_COL32(80, 80, 160, 50)
				);
				
				dl->AddLine(
					ImVec2(lx1, p.y),
					ImVec2(lx1, endP.y),
					IM_COL32(160, 160, 255, 180),
					2.0f
				);
				dl->AddLine(
					ImVec2(lx2, p.y),
					ImVec2(lx2, endP.y),
					IM_COL32(160, 160, 255, 180),
					2.0f
				);
			}
		}
		
		// 7. Draw each lane
		for(int lane = 0; lane < numLanes.get(); lane++) {
			float laneY = p.y + (lane * laneHeight);
			float laneEndY = laneY + laneHeight;
			
			// Lane separator line (except for last lane)
			if(lane < numLanes.get() - 1) {
				dl->AddLine(
					ImVec2(p.x, laneEndY),
					ImVec2(endP.x, laneEndY),
					IM_COL32(80, 80, 80, 150),
					1.0f
				);
			}
			
			// Highlight hovered lane
			if(isHovered && lane == hoveredLane) {
				dl->AddRectFilled(
					ImVec2(p.x, laneY),
					ImVec2(endP.x, laneEndY),
					IM_COL32(255, 255, 255, 10)
				);
			}
			
			// Lane number (small, left side)
			char laneBuf[8];
			snprintf(laneBuf, 8, "%d", lane + 1);
			dl->AddText(
				ImVec2(p.x + 4, laneY + 4),
				IM_COL32(150, 150, 150, 180),
				laneBuf
			);
			
			// Draw gates for this lane
			if(lane < gateLanes.size()) {
				for(const auto& g : gateLanes[lane]) {
					float x1 = beatToX(g.start);
					float x2 = beatToX(g.end());
					
					if(x2 < p.x || x1 > endP.x) continue;
					
					float drawX1 = std::max(x1, p.x);
					float drawX2 = std::min(x2, endP.x);
					
					// Use different colors for each lane
					ImU32 gateColor = getLaneColor(lane, 200);
					
					dl->AddRectFilled(
						ImVec2(drawX1, laneY + 2),
						ImVec2(drawX2, laneEndY - 2),
						gateColor,
						2.0f
					);
				}
			}
			
			// Draw preview gate while dragging in this lane
			if(isDragging && isCreatingGate && dragLane == lane) {
				double currentBeat = snap(xToBeat(mousePos.x));
				double start = std::min(dragStartBeat, currentBeat);
				double end = std::max(dragStartBeat, currentBeat);
				
				float x1 = beatToX(start);
				float x2 = beatToX(end);
				
				ImU32 previewColor = getLaneColor(lane, 120);
				
				dl->AddRectFilled(
					ImVec2(x1, laneY + 2),
					ImVec2(x2, laneEndY - 2),
					previewColor,
					2.0f
				);
			}
		}
		
		// 8. Draw playhead
		float playheadX = beatToX(currentPlayheadBeat);
		if(playheadX >= p.x && playheadX <= endP.x) {
			dl->AddLine(
				ImVec2(playheadX, p.y),
				ImVec2(playheadX, endP.y),
				IM_COL32(255, 80, 80, 255),
				2.5f
			);
		}
		
		// 9. Handle interactions
		if(isLeftClick && hoveredLane >= 0) {
			double clickBeat = xToBeat(mousePos.x);
			
			// Check if clicking on existing gate - if so, delete it
			bool clickedOnGate = false;
			if(hoveredLane < gateLanes.size()) {
				for(auto it = gateLanes[hoveredLane].begin(); it != gateLanes[hoveredLane].end(); ++it) {
					if(clickBeat >= it->start && clickBeat < it->end()) {
						gateLanes[hoveredLane].erase(it);
						clickedOnGate = true;
						break;
					}
				}
			}
			
			// If not clicking on gate, start creating new one
			if(!clickedOnGate) {
				isCreatingGate = true;
				dragStartBeat = snap(clickBeat);
				dragLane = hoveredLane;
			}
		}
		
		if(isReleased && isCreatingGate) {
			double endBeat = snap(xToBeat(mousePos.x));
			
			double start = std::min(dragStartBeat, endBeat);
			double end = std::max(dragStartBeat, endBeat);
			double length = end - start;
			
			if(length > 0.001 && dragLane >= 0 && dragLane < gateLanes.size()) {
				gateLanes[dragLane].push_back({(float)start, (float)length});
			}
			
			isCreatingGate = false;
		}
		
		if(isRightClick && hoveredLane >= 0) {
			double clickBeat = xToBeat(mousePos.x);
			if(hoveredLane < gateLanes.size()) {
				gateLanes[hoveredLane].erase(
					std::remove_if(gateLanes[hoveredLane].begin(), gateLanes[hoveredLane].end(),
						[&](const GateRegion& g) {
							return clickBeat >= g.start && clickBeat < g.end();
						}),
					gateLanes[hoveredLane].end()
				);
			}
		}
	}
	

	// Clip window virtuals
	bool   hasClipWindow()        const override { return true; }
	double getClipStart()         const override { return clipStart.get(); }
	double getClipEnd()           const override { return clipEnd.get(); }
	double getClipDuration()      const override { return clipDuration.get(); }
	bool   getClipLoop()          const override { return clipLoop.get(); }
	void   setClipStart(double v)    override { clipStart.set((float)v); }
	void   setClipEnd(double v)      override { clipEnd.set((float)v); }
	void   setClipDuration(double v) override { clipDuration.set((float)ofClamp(v, 0.25, 64.0)); }
	void   setClipLoop(bool v)       override { clipLoop.set(v); }

	// Mini content — draw gate bars inside clip bar
	void drawMiniContent(ImDrawList* dl, ImVec2 p1, ImVec2 p2,
	                     double viewBeat0, double viewBeat1,
	                     double clipOrigin = -1.0) override
	{
		float zoom = ofxOceanodeShared::getZoomLevel();
		double barLen = viewBeat1 - viewBeat0;
		if(barLen <= 0.001) return;
		float W = p2.x - p1.x;
		float H = p2.y - p1.y;
		if(W <= 1.f || H <= 1.f || gateLanes.empty()) return;

		// cs_primary: start of the primary clip — gate data lives relative to this
		double cs_primary = (double)clipStart.get();
		// cs: start of the clip being drawn (may be an extra clip)
		double cs  = (clipOrigin >= 0.0) ? clipOrigin : cs_primary;
		double dur = (double)clipDuration.get();
		bool   lp  = clipLoop.get();
		int    nL  = (int)gateLanes.size();
		float laneH = H / nL;

		int repStart = 0, repEnd = 0;
		if(lp && dur > 0.001) {
			repStart = std::max(0, (int)std::floor((viewBeat0 - cs) / dur) - 1);
			repEnd   = (int)std::ceil((viewBeat1 - cs) / dur);
		}

		dl->PushClipRect(p1, p2, true);
		for(int rep = repStart; rep <= repEnd; rep++) {
			double offset = rep * dur;
			for(int lane = 0; lane < nL && lane < (int)gateLanes.size(); lane++) {
				float ly1 = p1.y + lane * laneH;
				float ly2 = ly1 + laneH - 1.f;
				for(auto& g : gateLanes[lane]) {
					// When looping, only show gates within the primary clip period
					if(lp && dur > 0.001 &&
					   (g.start < cs_primary || g.start >= cs_primary + dur)) continue;
					// Position relative to cs_primary so extra clips show same content
					float gx1 = p1.x + float((g.start - cs_primary + offset) / barLen) * W;
					float gx2 = p1.x + float((g.end() - cs_primary + offset) / barLen) * W;
					if(gx2 < p1.x || gx1 > p2.x) continue;
					ImU32 c = getLaneColor(lane, 160);
					dl->AddRectFilled(ImVec2(std::max(gx1,p1.x), ly1+1),
					                  ImVec2(std::min(gx2,p2.x), ly2), c, 1.f);
				}
			}
		}
		dl->PopClipRect();
	}

	// Content stretching — scale all gate positions/lengths relative to primary clip start
	void beginContentStretch() override {
		gateStretchSnap.clear();
		for(auto& lane : gateLanes) {
			std::vector<std::pair<float,float>> snap;
			for(auto& g : lane) snap.push_back({g.start, g.length});
			gateStretchSnap.push_back(snap);
		}
	}
	void applyContentStretch(double factor) override {
		if(gateStretchSnap.size() != gateLanes.size()) return;
		factor = std::max(0.01, factor);
		double cs0 = (double)clipStart.get();
		for(int i = 0; i < (int)gateLanes.size(); i++) {
			for(int j = 0; j < (int)gateLanes[i].size() && j < (int)gateStretchSnap[i].size(); j++) {
				gateLanes[i][j].start  = (float)(cs0 + (gateStretchSnap[i][j].first  - cs0) * factor);
				gateLanes[i][j].length = (float)(gateStretchSnap[i][j].second * factor);
			}
		}
	}

	// --- Preset Saving ---
	void presetSave(ofJson &json) override {
		// Save all lanes
		std::vector<std::vector<std::vector<float>>> allLanes;
		for(auto &lane : gateLanes) {
			std::vector<std::vector<float>> laneGates;
			for(auto &g : lane) {
				laneGates.push_back({g.start, g.length});
			}
			allLanes.push_back(laneGates);
		}
		json["gateLanes"]      = allLanes;
		json["trackHeight"]    = trackHeight;
		json["numLanes"]       = numLanes.get();
		json["collapsed"]      = collapsed;
		json["clipStart"]      = clipStart.get();
		json["clipEnd"]        = clipEnd.get();
		json["clipDuration"]   = clipDuration.get();
		json["clipLoop"]       = clipLoop.get();
		saveExtraClips(json);
	}

	void presetRecallAfterSettingParameters(ofJson &json) override {
		if(json.count("numLanes") > 0) {
			numLanes = json["numLanes"];
			resizeLanes(numLanes.get());
		}
		
		if(json.count("gateLanes") > 0) {
			gateLanes.clear();
			for(auto &laneData : json["gateLanes"]) {
				std::vector<GateRegion> lane;
				for(auto &gData : laneData) {
					if(gData.size() >= 2) {
						lane.push_back({gData[0], gData[1]});
					}
				}
				gateLanes.push_back(lane);
			}
		}
		
		if(json.count("trackHeight") > 0) {
			trackHeight = json["trackHeight"];
			trackHeight = ofClamp(trackHeight, MIN_TRACK_HEIGHT, MAX_TRACK_HEIGHT);
		}
		if(json.count("collapsed") > 0) collapsed = json["collapsed"];
		if(json.count("clipStart"))    clipStart    = (float)json["clipStart"];
		if(json.count("clipEnd"))      clipEnd      = (float)json["clipEnd"];
		if(json.count("clipDuration")) clipDuration = (float)json["clipDuration"];
		if(json.count("clipLoop"))     clipLoop     = (bool) json["clipLoop"];
		loadExtraClips(json);
	}

private:
	ofParameter<int>         timelineSelect;
	ofParameter<std::string> trackName;
	ofParameter<int>         numLanes;
	ofParameter<float>       clipStart;
	ofParameter<float>       clipEnd;
	ofParameter<float>       clipDuration;
	ofParameter<bool>        clipLoop;
	ofParameter<vector<float>> gateOutput;
	ofParameter<vector<float>> trigOutput;

	ppqTimeline* currentTimeline = nullptr;
	std::vector<std::vector<GateRegion>> gateLanes;  // Vector of lanes, each containing gates
	std::vector<std::vector<std::pair<float,float>>> gateStretchSnap; // for beginContentStretch
	std::vector<std::string> timelineOptions;

	std::vector<bool> lastActiveState;  // One per lane
	ofEventListeners listeners;
	
	// Drag state for creating gates
	bool isCreatingGate = false;
	double dragStartBeat = 0.0;
	int dragLane = -1;  // Which lane is being edited
	
	// Track height management
	float trackHeight = 120.0f;  // Taller default for multiple lanes
	bool collapsed = false;
	static constexpr float MIN_TRACK_HEIGHT = 20.0f;  // Lower minimum (was 40)
	static constexpr float MAX_TRACK_HEIGHT = 600.0f;  // Higher max for multi-lane

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
	
	// Helper to get loop region from timeline
	bool getLoopRegion(double& loopStart, double& loopEnd, bool& enabled) {
		if(!currentTimeline) return false;
		
		enabled = currentTimeline->isLoopEnabled();
		loopStart = currentTimeline->getLoopStart();
		loopEnd = currentTimeline->getLoopEnd();
		
		return true;
	}
	
	// Helper to get color for each lane
	ImU32 getLaneColor(int lane, uint8_t alpha) {
		// Color palette for lanes
		static const ImVec4 laneColors[] = {
			ImVec4(200.0f/255, 80.0f/255, 80.0f/255, 1.0f),   // Red
			ImVec4(80.0f/255, 200.0f/255, 80.0f/255, 1.0f),   // Green
			ImVec4(80.0f/255, 80.0f/255, 200.0f/255, 1.0f),   // Blue
			ImVec4(200.0f/255, 200.0f/255, 80.0f/255, 1.0f),  // Yellow
			ImVec4(200.0f/255, 80.0f/255, 200.0f/255, 1.0f),  // Magenta
			ImVec4(80.0f/255, 200.0f/255, 200.0f/255, 1.0f),  // Cyan
			ImVec4(200.0f/255, 150.0f/255, 80.0f/255, 1.0f),  // Orange
			ImVec4(150.0f/255, 80.0f/255, 200.0f/255, 1.0f),  // Purple
		};
		
		int colorIdx = lane % 8;
		ImVec4 color = laneColors[colorIdx];
		
		return IM_COL32(
			(uint8_t)(color.x * 255),
			(uint8_t)(color.y * 255),
			(uint8_t)(color.z * 255),
			alpha
		);
	}
	
	// Resize lanes when parameter changes
	void resizeLanes(int newNumLanes) {
		// Resize gateLanes vector
		if(newNumLanes > gateLanes.size()) {
			// Add empty lanes
			while(gateLanes.size() < newNumLanes) {
				gateLanes.push_back(std::vector<GateRegion>());
			}
		} else if(newNumLanes < gateLanes.size()) {
			// Remove lanes (data is lost)
			gateLanes.resize(newNumLanes);
		}
		
		// Resize lastActiveState
		lastActiveState.resize(newNumLanes, false);
	}
};

#endif
