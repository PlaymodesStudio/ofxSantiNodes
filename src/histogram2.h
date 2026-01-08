#ifndef histogram2_h
#define histogram2_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <algorithm>
#include <functional>

class histogram2 : public ofxOceanodeNodeModel {
public:
	histogram2() : ofxOceanodeNodeModel("Histogram 2") {
		description =
			"Multi-lane scrolling waveform display with multiple input support.\n"
			"New data appears on the right, scrolls left over time.\n"
			"Multiple inputs are overlaid with progressive transparency.";
			
		// Maximum time we can store
		maxBufferTime = 240.0f; // 240 seconds maximum
		maxBufferSamples = 60 * 240; // Store at 60Hz for 240 seconds
		writeIndex = 0;
		maxInputs = 8; // Maximum number of inputs
		isLoadingPreset = false;
	}

	void setup() override {
		// Floating window toggle
		addParameter(showWindow.set("Show", false));

		addInspectorParameter(numInputs.set("Num Inputs", 1, 1, maxInputs));
		
		// Core parameters - start with inputs based on numInputs
		inputs.resize(numInputs);
		for(int i = 0; i < numInputs; i++){
			inputs[i] = make_shared<ofParameter<vector<float>>>();
			addParameter(inputs[i]->set("Input " + ofToString(i + 1),
								  std::vector<float>(4, 0.5f),
								  std::vector<float>(4, 0.0f),
								  std::vector<float>(4, 1.0f)));
		}

		addParameter(minVal.set("Min", 0.0f, -FLT_MAX, FLT_MAX));
		addParameter(maxVal.set("Max", 1.0f, -FLT_MAX, FLT_MAX));
		addParameter(timeWindow.set("Time Window", 2.0f, 0.1f, maxBufferTime));
		addParameter(freeze.set("Freeze", false));
		addParameter(gain.set("Gain", 1.0f, 0.1f, 10.0f));

		// Inspector-only UI controls
		addInspectorParameter(drawInNode.set("Draw In Node", false));
		addInspectorParameter(widgetWidth.set("Widget Width", 600.0f, 200.0f, 1200.0f));
		addInspectorParameter(widgetHeight.set("Widget Height", 300.0f, 100.0f, 1200.0f));
		addInspectorParameter(showGrid.set("Grid", true));
		addInspectorParameter(laneHeight.set("Lane Height", 80.0f, 40.0f, 200.0f));
		addInspectorParameter(lineThickness.set("Line Thickness", 1.5f, 0.5f, 5.0f));

		// Embedded node GUI region
		addCustomRegion(
			ofParameter<std::function<void()>>().set("Histogram", [this](){ drawWidget(); }),
			ofParameter<std::function<void()>>().set("Histogram", [this](){ drawWidget(); })
		);

		listeners.push(numInputs.newListener([this](int &i){
			if(!isLoadingPreset) {
				updateInputCount(i);
			}
		}));

		lastUpdateTime = ofGetElapsedTimef();
	}

	void updateInputCount(int count) {
		if(inputs.size() != count){
			int oldSize = inputs.size();
			bool remove = oldSize > count;
			
			inputs.resize(count);
			
			if(remove){
				// Remove excess parameters
				for(int j = oldSize-1; j >= count; j--){
					removeParameter("Input " + ofToString(j + 1));
				}
			}else{
				// Add new parameters
				for(int j = oldSize; j < count; j++){
					inputs[j] = make_shared<ofParameter<vector<float>>>();
					addParameter(inputs[j]->set("Input " + ofToString(j + 1),
										  std::vector<float>(numLanes > 0 ? numLanes : 4, 0.5f),
										  std::vector<float>(numLanes > 0 ? numLanes : 4, 0.0f),
										  std::vector<float>(numLanes > 0 ? numLanes : 4, 1.0f)));
				}
			}
			
			// Resize buffers
			slidingBuffers.resize(count);
			for(auto& buffer : slidingBuffers) {
				if(buffer.size() != maxBufferSamples * numLanes) {
					buffer.clear();
					buffer.resize(maxBufferSamples * numLanes, 0.0f);
				}
			}
		}
	}

	void loadBeforeConnections(ofJson &json) override {
		ofLogNotice("histogram") << "=== LOAD BEFORE CONNECTIONS ===";
		
		// Set flag to bypass listener during preset load
		isLoadingPreset = true;
		
		// Load numInputs first
		deserializeParameter(json, numInputs);
		
		// Now manually update inputs to match without triggering listener
		int targetInputs = numInputs.get();
		if(inputs.size() != targetInputs) {
			int oldSize = inputs.size();
			
			if(oldSize > targetInputs) {
				// Remove excess
				for(int j = oldSize-1; j >= targetInputs; j--){
					removeParameter("Input " + ofToString(j + 1));
				}
				inputs.resize(targetInputs);
			} else {
				// Add new
				inputs.resize(targetInputs);
				for(int j = oldSize; j < targetInputs; j++){
					inputs[j] = make_shared<ofParameter<vector<float>>>();
					addParameter(inputs[j]->set("Input " + ofToString(j + 1),
										  std::vector<float>(numLanes > 0 ? numLanes : 4, 0.5f),
										  std::vector<float>(numLanes > 0 ? numLanes : 4, 0.0f),
										  std::vector<float>(numLanes > 0 ? numLanes : 4, 1.0f)));
				}
			}
			
			// Resize buffers
			slidingBuffers.resize(targetInputs);
			for(auto& buffer : slidingBuffers) {
				buffer.clear();
				buffer.resize(maxBufferSamples * numLanes, 0.0f);
			}
		}
		
		isLoadingPreset = false;
		
		ofLogNotice("histogram") << "Inputs ready: " << inputs.size() << " inputs";
	}

	void update(ofEventArgs &) override {
		if(freeze.get()) return;
		
		float currentTime = ofGetElapsedTimef();
		
		// Get the size from the first input
		if(inputs.empty() || inputs[0]->get().empty()) return;
		
		const auto &firstInput = inputs[0]->get();
		
		// Initialize or resize buffers if channel count changed
		if(numLanes != firstInput.size()) {
			numLanes = firstInput.size();
			
			// Resize all buffers
			slidingBuffers.resize(inputs.size());
			for(auto& buffer : slidingBuffers) {
				buffer.clear();
				buffer.resize(maxBufferSamples * numLanes, 0.0f);
			}
			writeIndex = 0;
		}
		
		// Write samples from all inputs to their respective buffers
		for(size_t inputIdx = 0; inputIdx < inputs.size() && inputIdx < slidingBuffers.size(); ++inputIdx) {
			const auto &currentInput = inputs[inputIdx]->get();
			
			// Skip if this input doesn't match the expected size
			if(currentInput.size() != numLanes) continue;
			
			// Write new samples to this input's sliding buffer
			for(size_t ch = 0; ch < numLanes; ++ch) {
				int idx = writeIndex + (ch * maxBufferSamples);
				if(idx < slidingBuffers[inputIdx].size()) {
					slidingBuffers[inputIdx][idx] = currentInput[ch];
				}
			}
		}
		
		// Advance write index (circular buffer)
		writeIndex = (writeIndex + 1) % maxBufferSamples;
		
		lastUpdateTime = currentTime;
	}

	void draw(ofEventArgs &) override {
		if(!showWindow.get()) return;

		std::string title =
			(canvasID == "Canvas" ? "" : canvasID + "/") +
			"Histogram " + ofToString(getNumIdentifier());

		if(ImGui::Begin(title.c_str(), (bool*)&showWindow.get())) {
			ImVec2 avail = ImGui::GetContentRegionAvail();
			if(avail.x < 200) avail.x = 200;
			if(avail.y < 100) avail.y = 100;

			drawHistogramAtCursor(avail.x, avail.y);
		}
		ImGui::End();
	}

private:
	// Embedded widget wrapper
	void drawWidget() {
		if(!drawInNode.get()) return;

		float w = widgetWidth.get();
		float h = widgetHeight.get();

		drawHistogramAtCursor(w, h);
		ImGui::Dummy(ImVec2(0, 4));
	}

	// Core renderer - draws scrolling waveforms
	void drawHistogramAtCursor(float targetW, float targetH) {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();

		if(slidingBuffers.empty() || numLanes == 0) {
			ImGui::Text("No data");
			return;
		}

		const float minV = minVal.get();
		const float maxV = maxVal.get();
		const float range = maxV - minV;

		if(range <= 0.0f) {
			ImGui::Text("Invalid range (min >= max)");
			return;
		}

		// Calculate how many samples to display based on time window
		float timeWindowSeconds = ofClamp(timeWindow.get(), 0.1f, maxBufferTime);
		int samplesToDisplay = (int)(timeWindowSeconds * 60.0f);
		samplesToDisplay = ofClamp(samplesToDisplay, 10, maxBufferSamples);
		
		// Layout calculation
		const float laneH = laneHeight.get();
		const float totalHeight = laneH * numLanes;
		const float gainValue = gain.get();
		const float thickness = lineThickness.get();

		ImVec2 start = cursorPos;
		ImVec2 end = ImVec2(start.x + targetW, start.y + totalHeight);

		// Background + border
		drawList->AddRectFilled(start, end, IM_COL32(15, 15, 15, 255));
		drawList->AddRect(start, end, IM_COL32(100, 100, 100, 255), 0.0f, 0, 1.5f);

		// Draw each lane
		for(size_t lane = 0; lane < numLanes; ++lane) {
			const float laneY = start.y + lane * laneH;
			const float laneCenterY = laneY + laneH * 0.5f;
			
			// Thicker lane separator line
			if(lane > 0) {
				drawList->AddLine(
					ImVec2(start.x, laneY),
					ImVec2(end.x, laneY),
					IM_COL32(100, 100, 100, 200),
					2.0f
				);
			}

			// Center/zero line
			if(showGrid.get()) {
				drawList->AddLine(
					ImVec2(start.x, laneCenterY),
					ImVec2(end.x, laneCenterY),
					IM_COL32(80, 80, 80, 140),
					0.5f
				);
			}

			// Vertical grid lines
			if(showGrid.get()) {
				for(int i = 1; i < 10; ++i) {
					float gx = start.x + (targetW * i) / 10.0f;
					drawList->AddLine(
						ImVec2(gx, laneY),
						ImVec2(gx, laneY + laneH),
						IM_COL32(40, 40, 40, 100),
						0.5f
					);
				}
			}

			// Generate color for this lane (same color for all inputs on this lane)
			float hue = (float)lane / (float)numLanes;
			
			// Draw waveforms from all inputs with progressive transparency
			// First input: 100% opacity, second: 75%, third: 50%, etc.
			for(size_t inputIdx = 0; inputIdx < slidingBuffers.size(); ++inputIdx) {
				const auto& buffer = slidingBuffers[inputIdx];
				if(buffer.empty()) continue;
				
				const int channelOffset = lane * maxBufferSamples;
				
				// Calculate alpha: 100%, 75%, 50%, 25% for inputs 0, 1, 2, 3...
				// Formula: (numInputs - inputIdx) / numInputs
				float alpha = (float)(slidingBuffers.size() - inputIdx) / (float)slidingBuffers.size();
				
				// Same hue for same lane, but different alpha per input
				ImU32 color = ImColor::HSV(hue, 0.6f, 0.9f, alpha);

				// Draw waveform - one line segment per pixel
				for(int px = 0; px < (int)targetW - 1; ++px) {
					// Map pixel to sample index
					float t1 = (float)px / (float)(targetW - 1);
					float t2 = (float)(px + 1) / (float)(targetW - 1);
					
					// Calculate sample indices
					int recentIdx = (writeIndex - 1 + maxBufferSamples) % maxBufferSamples;
					int oldestIdx = (recentIdx - samplesToDisplay + 1 + maxBufferSamples) % maxBufferSamples;
					
					// Get samples for this pixel
					int sampleIdx1, sampleIdx2;
					if(oldestIdx < recentIdx) {
						// No wrap-around
						sampleIdx1 = oldestIdx + (int)(t1 * samplesToDisplay);
						sampleIdx2 = oldestIdx + (int)(t2 * samplesToDisplay);
					} else {
						// Circular buffer wraps around
						int idx1 = (int)(t1 * samplesToDisplay);
						int idx2 = (int)(t2 * samplesToDisplay);
						sampleIdx1 = (oldestIdx + idx1) % maxBufferSamples;
						sampleIdx2 = (oldestIdx + idx2) % maxBufferSamples;
					}
					
					// Read values from buffer
					float val1 = buffer[channelOffset + sampleIdx1];
					float val2 = buffer[channelOffset + sampleIdx2];
					
					// Apply gain and clamp
					val1 = ofClamp(val1 * gainValue, minV, maxV);
					val2 = ofClamp(val2 * gainValue, minV, maxV);
					
					// Normalize to [0,1] within range
					float norm1 = (val1 - minV) / range;
					float norm2 = (val2 - minV) / range;
					
					// Convert to screen Y coordinates (flip so high values are at top)
					float y1 = laneY + laneH - (norm1 * laneH * 0.9f) - (laneH * 0.05f);
					float y2 = laneY + laneH - (norm2 * laneH * 0.9f) - (laneH * 0.05f);
					
					float x1 = start.x + px;
					float x2 = start.x + px + 1;
					
					drawList->AddLine(
						ImVec2(x1, y1),
						ImVec2(x2, y2),
						color,
						thickness
					);
				}
			}
		}

		// Consume layout space
		ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + totalHeight));
		ImGui::Dummy(ImVec2(targetW, 1.0f));
	}

private:
	// Parameters
	ofParameter<bool> showWindow;
	std::vector<shared_ptr<ofParameter<vector<float>>>> inputs;
	ofParameter<int> numInputs;
	ofParameter<float> minVal;
	ofParameter<float> maxVal;
	ofParameter<float> timeWindow;
	ofParameter<bool> freeze;
	ofParameter<float> gain;

	// Inspector-only
	ofParameter<bool> drawInNode;
	ofParameter<float> widgetWidth;
	ofParameter<float> widgetHeight;
	ofParameter<bool> showGrid;
	ofParameter<float> laneHeight;
	ofParameter<float> lineThickness;

	// Internal state - multiple sliding buffers (one per input)
	std::vector<std::vector<float>> slidingBuffers;  // One buffer per input
	int maxBufferSamples;              // Maximum samples per channel
	float maxBufferTime;               // Maximum time (240 seconds)
	int writeIndex;                    // Current write position
	size_t numLanes = 0;
	int maxInputs;
	float lastUpdateTime = 0.0f;
	bool isLoadingPreset;
	
	ofEventListeners listeners;
};

#endif /* histogram2_h */
