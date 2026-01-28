#ifndef valueTrack_h
#define valueTrack_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "ppqTimeline.h"
#include "transportTrack.h"

#ifndef ValueRegion_DEFINED
#define ValueRegion_DEFINED
struct ValueRegion {
	float start;
	float length;
	float value;
	float end() const { return start + length; }
};
#endif

class valueTrack : public ofxOceanodeNodeModel, public transportTrack {
public:
	valueTrack() : ofxOceanodeNodeModel("Multi Value Track") {
		color = ofColor(100, 255, 100); // Different color than gateTrack
	}

	~valueTrack() {
		if(currentTimeline) currentTimeline->unsubscribeTrack(this);
	}

	void setup() override {
		refreshTimelineList();
		addParameterDropdown(timelineSelect, "Timeline", 0, timelineOptions);
		addParameter(trackName.set("Track Name", "MultiValue " + ofToString(getNumIdentifier())));
		addParameter(numLanes.set("Num Lanes", 4, 1, 16));
		
		// Vector output for multiple values
		addOutputParameter(valueOutput.set("Value[]", {0}, {-FLT_MAX}, {FLT_MAX}));
		addOutputParameter(trigOutput.set("Trig[]", {0}, {0}, {1}));

		listeners.push(timelineSelect.newListener([this](int &val){ updateSubscription(); }));
		listeners.push(numLanes.newListener([this](int &val){
			resizeLanes(val);
		}));
		
		valueOutput.setSerializable(false);
		trigOutput.setSerializable(false);
		
		// Initialize with default number of lanes
		resizeLanes(numLanes.get());
		
		updateSubscription();
	}

	void update(ofEventArgs &args) override {
		float currentBeat = 0.0f;
		if(currentTimeline) currentBeat = currentTimeline->getBeatPosition();

		// Prepare output vectors
		vector<float> valueOutputs(numLanes.get(), 0.0f);
		vector<float> trigOutputs(numLanes.get(), 0.0f);
		
		// Check each lane
		for(int lane = 0; lane < numLanes.get() && lane < valueLanes.size(); lane++) {
			float val = 0.0f;
			bool active = false;
			bool newTrig = false;
			
			for(const auto& r : valueLanes[lane]) {
				if(currentBeat >= r.start && currentBeat < r.end()) {
					val = r.value;
					active = true;
					if(lane < lastActiveState.size() && !lastActiveState[lane]) newTrig = true;
					break;
				}
			}
			
			valueOutputs[lane] = val;
			trigOutputs[lane] = newTrig ? 1.0f : 0.0f;
			
			if(lane < lastActiveState.size()) {
				lastActiveState[lane] = active;
			}
		}
		
		valueOutput = valueOutputs;
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
		
		// Calculate lane height
		float laneHeight = trackHeight / numLanes.get();
		float minLaneHeight = 15.0f;
		
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
		bool isDoubleClicked = isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
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
			
			// Draw regions for this lane
			if(lane < valueLanes.size()) {
				for(const auto& r : valueLanes[lane]) {
					float x1 = beatToX(r.start);
					float x2 = beatToX(r.end());
					
					if(x2 < p.x || x1 > endP.x) continue;
					
					float drawX1 = std::max(x1, p.x);
					float drawX2 = std::min(x2, endP.x);
					
					// Use different colors for each lane
					ImU32 regionColor = getLaneColor(lane, 200);
					
					dl->AddRectFilled(
						ImVec2(drawX1, laneY + 2),
						ImVec2(drawX2, laneEndY - 2),
						regionColor,
						10.0f
					);

	                   // Draw value text
	                   char valBuf[16];
	                   if(std::floor(r.value) == r.value) {
	                       snprintf(valBuf, 16, "%.0f", r.value);
	                   } else {
	                       snprintf(valBuf, 16, "%.2f", r.value);
	                   }
	                   ImVec2 textSize = ImGui::CalcTextSize(valBuf);
	                   float textX = drawX1 + (drawX2 - drawX1 - textSize.x) * 0.5f;
	                   float textY = laneY + (laneHeight - textSize.y) * 0.5f;
	                   
	                   // Only draw text if it fits
	                   if (drawX2 - drawX1 > textSize.x + 4) {
	                       dl->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 255), valBuf);
	                   }
				}
			}
			
			// Draw preview region while dragging in this lane
			if(isDragging && isCreatingRegion && dragLane == lane) {
				double currentBeat = snap(xToBeat(mousePos.x));
				
				// Calculate bounds for overlap prevention
				double minStart = -100000.0;
				double maxEnd = 100000.0;
				
				if(dragLane < valueLanes.size()) {
					for(const auto& r : valueLanes[dragLane]) {
						if(r.end() <= dragStartBeat) {
							if(r.end() > minStart) minStart = r.end();
						}
						if(r.start >= dragStartBeat) {
							if(r.start < maxEnd) maxEnd = r.start;
						}
					}
				}
				
				double start = std::min(dragStartBeat, currentBeat);
				double end = std::max(dragStartBeat, currentBeat);
				
				// Clamp to neighbors
				if(start < minStart) start = minStart;
				if(end > maxEnd) end = maxEnd;
				
				float x1 = beatToX(start);
				float x2 = beatToX(end);
				
				ImU32 previewColor = getLaneColor(lane, 120);
				
				dl->AddRectFilled(
					ImVec2(x1, laneY + 2),
					ImVec2(x2, laneEndY - 2),
					previewColor,
					5.0f
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
		
		// Double Click to Edit
		if (isDoubleClicked && hoveredLane >= 0) {
			double clickBeat = xToBeat(mousePos.x);
			if(hoveredLane < valueLanes.size()) {
				for(int i = 0; i < valueLanes[hoveredLane].size(); ++i) {
					auto& r = valueLanes[hoveredLane][i];
					if(clickBeat >= r.start && clickBeat < r.end()) {
						// Open popup
						ImGui::OpenPopup(("Edit Value##" + ofToString(getNumIdentifier())).c_str());
						editLaneIndex = hoveredLane;
						editRegionIndex = i;
						editValueTemp = r.value;
						
						// Cancel any move that might have started
						isMovingRegion = false;
						break;
					}
				}
			}
		}
		// Left Click Down (Start Move or Create)
		else if (isLeftClick && hoveredLane >= 0) {
			double clickBeat = xToBeat(mousePos.x);
			bool clickedOnRegion = false;
			
			if(hoveredLane < valueLanes.size()) {
				for(int i = 0; i < valueLanes[hoveredLane].size(); ++i) {
					auto& r = valueLanes[hoveredLane][i];
					if(clickBeat >= r.start && clickBeat < r.end()) {
						clickedOnRegion = true;
						// Start Moving
						isMovingRegion = true;
						moveLane = hoveredLane;
						moveRegionIndex = i;
						moveOffset = clickBeat - r.start;
						originalRegionStart = r.start;
						break;
					}
				}
			}
			
			// If not clicking on region, start creating new one
			if(!clickedOnRegion) {
				isCreatingRegion = true;
				dragStartBeat = snap(clickBeat);
				dragLane = hoveredLane;
			}
		}
		
		// Dragging
		if (isDragging) {
			double currentBeat = xToBeat(mousePos.x);
			
			if (isMovingRegion && moveLane >= 0 && moveRegionIndex >= 0 &&
				moveLane < valueLanes.size() && moveRegionIndex < valueLanes[moveLane].size()) {
				
				// Calculate new start
				double newStart = snap(currentBeat - moveOffset);
				auto& r = valueLanes[moveLane][moveRegionIndex];
				
				// Allow free dragging (no clamping against neighbors)
				r.start = newStart;
			}
			
			// Creation drag logic is handled in the drawing loop for preview
		}
		
		// Release
		if (isReleased) {
			if (isCreatingRegion) {
				double endBeat = snap(xToBeat(mousePos.x));
				
				// Calculate bounds for overlap prevention (same as preview)
				double minStart = -100000.0;
				double maxEnd = 100000.0;
				
				if(dragLane < valueLanes.size()) {
					for(const auto& r : valueLanes[dragLane]) {
						if(r.end() <= dragStartBeat) {
							if(r.end() > minStart) minStart = r.end();
						}
						if(r.start >= dragStartBeat) {
							if(r.start < maxEnd) maxEnd = r.start;
						}
					}
				}
				
				double start = std::min(dragStartBeat, endBeat);
				double end = std::max(dragStartBeat, endBeat);
				
				// Clamp
				if(start < minStart) start = minStart;
				if(end > maxEnd) end = maxEnd;
				
				double length = end - start;
				
				if(length > 0.001 && dragLane >= 0 && dragLane < valueLanes.size()) {
					valueLanes[dragLane].push_back({(float)start, (float)length, 0.0f});
					
					// Sort lane after insertion
					std::sort(valueLanes[dragLane].begin(), valueLanes[dragLane].end(),
						[](const ValueRegion& a, const ValueRegion& b){
							return a.start < b.start;
						});
				}
			}
			
			isCreatingRegion = false;
			
			if (isMovingRegion && moveLane >= 0 && moveRegionIndex >= 0 &&
				moveLane < valueLanes.size() && moveRegionIndex < valueLanes[moveLane].size()) {
				
				auto& r = valueLanes[moveLane][moveRegionIndex];
				bool overlap = false;
				
				// Check for overlaps with other regions in the same lane
				for(int i = 0; i < valueLanes[moveLane].size(); ++i) {
					if(i == moveRegionIndex) continue;
					
					const auto& other = valueLanes[moveLane][i];
					// Check intersection: start1 < end2 && start2 < end1
					if(r.start < other.end() && other.start < r.end()) {
						overlap = true;
						break;
					}
				}
				
				if(overlap) {
					// Revert
					r.start = originalRegionStart;
				} else {
					// Commit and sort
					std::sort(valueLanes[moveLane].begin(), valueLanes[moveLane].end(),
						[](const ValueRegion& a, const ValueRegion& b){
							return a.start < b.start;
						});
				}
			}

			isMovingRegion = false;
			moveLane = -1;
			moveRegionIndex = -1;
		}
		
		// Right Click (Delete)
		if (isRightClick && hoveredLane >= 0) {
			double clickBeat = xToBeat(mousePos.x);
			if(hoveredLane < valueLanes.size()) {
				valueLanes[hoveredLane].erase(
					std::remove_if(valueLanes[hoveredLane].begin(), valueLanes[hoveredLane].end(),
						[&](const ValueRegion& r) {
							return clickBeat >= r.start && clickBeat < r.end();
						}),
					valueLanes[hoveredLane].end()
				);
			}
		}

	       // Popup for editing value
	       if (ImGui::BeginPopup(("Edit Value##" + ofToString(getNumIdentifier())).c_str())) {
	           if (editLaneIndex >= 0 && editLaneIndex < valueLanes.size() &&
	               editRegionIndex >= 0 && editRegionIndex < valueLanes[editLaneIndex].size()) {
	               
	               ImGui::Text("Edit Region Value");
	               if (ImGui::DragFloat("Value", &editValueTemp, 0.01f)) {
	                   valueLanes[editLaneIndex][editRegionIndex].value = editValueTemp;
	               }
	               
	               if (ImGui::Button("Delete Region")) {
	                   valueLanes[editLaneIndex].erase(valueLanes[editLaneIndex].begin() + editRegionIndex);
	                   ImGui::CloseCurrentPopup();
	               }
	           } else {
	               ImGui::CloseCurrentPopup();
	           }
	           ImGui::EndPopup();
	       }
	}
	

	// --- Preset Saving ---
	void presetSave(ofJson &json) override {
		// Save all lanes
		std::vector<std::vector<std::vector<float>>> allLanes;
		for(auto &lane : valueLanes) {
			std::vector<std::vector<float>> laneRegions;
			for(auto &r : lane) {
				laneRegions.push_back({r.start, r.length, r.value});
			}
			allLanes.push_back(laneRegions);
		}
		json["valueLanes"] = allLanes;
		json["trackHeight"] = trackHeight;
		json["numLanes"] = numLanes.get();
		json["collapsed"] = collapsed;
	}

	void presetRecallAfterSettingParameters(ofJson &json) override {
		if(json.count("numLanes") > 0) {
			numLanes = json["numLanes"];
			resizeLanes(numLanes.get());
		}
		
		if(json.count("valueLanes") > 0) {
			valueLanes.clear();
			for(auto &laneData : json["valueLanes"]) {
				std::vector<ValueRegion> lane;
				for(auto &rData : laneData) {
					if(rData.size() >= 2) {
                        float val = 0.0f;
                        if (rData.size() >= 3) val = rData[2];
						lane.push_back({rData[0], rData[1], val});
					}
				}
				valueLanes.push_back(lane);
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
	ofParameter<int> numLanes;
	ofParameter<vector<float>> valueOutput;
	ofParameter<vector<float>> trigOutput;

	ppqTimeline* currentTimeline = nullptr;
	std::vector<std::vector<ValueRegion>> valueLanes;  // Vector of lanes, each containing regions
	std::vector<std::string> timelineOptions;
	
	std::vector<bool> lastActiveState;  // One per lane
	ofEventListeners listeners;
	
	// Drag state for creating regions
	bool isCreatingRegion = false;
	double dragStartBeat = 0.0;
	int dragLane = -1;  // Which lane is being edited
	
	// Drag state for moving regions
	bool isMovingRegion = false;
	int moveLane = -1;
	int moveRegionIndex = -1;
	double moveOffset = 0.0;
	double originalRegionStart = 0.0;

	   // Edit state
    int editLaneIndex = -1;
    int editRegionIndex = -1;
    float editValueTemp = 0.0f;

	// Track height management
	float trackHeight = 120.0f;  // Taller default for multiple lanes
	bool collapsed = false;
	static constexpr float MIN_TRACK_HEIGHT = 20.0f;
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
		// Resize valueLanes vector
		if(newNumLanes > valueLanes.size()) {
			// Add empty lanes
			while(valueLanes.size() < newNumLanes) {
				valueLanes.push_back(std::vector<ValueRegion>());
			}
		} else if(newNumLanes < valueLanes.size()) {
			// Remove lanes (data is lost)
			valueLanes.resize(newNumLanes);
		}
		
		// Resize lastActiveState
		lastActiveState.resize(newNumLanes, false);
	}
};

#endif
