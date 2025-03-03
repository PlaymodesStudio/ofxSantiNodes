#ifndef markovVector_h
#define markovVector_h

#include "ofxOceanodeNodeModel.h"
#include "imgui_internal.h"
#include <random>

class markovVector : public ofxOceanodeNodeModel {
public:
	markovVector() : ofxOceanodeNodeModel("Markov Vector") {
		description = "Generates sequences based on Markov transition probabilities. "
					 "Each slider row defines probabilities of moving from one state to others. "
					 "The 'duplicates' vector defines how many times each state is repeated in output.";
					 
		// Add inspector parameter for number of states
		addInspectorParameter(numStates.set("Num States", 5, 2, 16));
	}

	void setup() {
		// Input parameters
		addParameter(initialState.set("Initial State", 0, 0, 15));
		addParameter(duplicates.set("Duplicates", {1}, {0}, {INT_MAX}));
		addParameter(outputSize.set("Output Size", 16, 1, INT_MAX));
		addParameter(noRepeats.set("No Repeats", false));
		addParameter(seed.set("Seed", 0, 0, INT_MAX));
		addParameter(recalculate.set("Recalculate"));
		
		// Output parameter
		addOutputParameter(output.set("Output", {0}, {0}, {INT_MAX}));
		
		// Initialize transition matrices with default values
		transitionMatrices.resize(16);
		for(auto &matrix : transitionMatrices) {
			matrix.resize(16, 0.0f);
		}
		
		// Initialize with deterministic transitions (i → i+1)
		for(int i = 0; i < numStates; i++) {
			for(int j = 0; j < numStates; j++) {
				// Set everything to 0 first
				transitionMatrices[i][j] = 0.0f;
			}
			// Set the next state to 1.0 (or loop back to 0 if at the end)
			int nextState = (i + 1) % numStates;
			transitionMatrices[i][nextState] = 1.0f;
		}
		
		// Add custom region for matrix sliders
		addCustomRegion(customWidget, [this]() {
			drawTransitionMatrix();
		});
		
		setupListeners();
		
		// Initial calculation
		calculateOutput();
	}

	void setupListeners() {
		// Listen for changes in numStates
		listeners.push(numStates.newListener([this](int &n) {
			updateStateCount();
		}));
		
		// Listen for recalculate button
		listeners.push(recalculate.newListener([this]() {
			calculateOutput();
		}));
		
		// Listen for changes in other parameters
		listeners.push(initialState.newListener([this](int &) {
			calculateOutput();
		}));
		
		listeners.push(duplicates.newListener([this](vector<int> &) {
			calculateOutput();
		}));
		
		listeners.push(outputSize.newListener([this](int &) {
			calculateOutput();
		}));
		
		listeners.push(noRepeats.newListener([this](bool &) {
			calculateOutput();
		}));
		
		listeners.push(seed.newListener([this](int &) {
			calculateOutput();
		}));
	}
	
	void presetSave(ofJson &json) {
		// Save transition matrices
		for (int from = 0; from < numStates; from++) {
			for (int to = 0; to < numStates; to++) {
				json["Transitions"][ofToString(from)][ofToString(to)] = transitionMatrices[from][to];
			}
		}
	}

	void presetRecallAfterSettingParameters(ofJson &json) {
		if (json.count("Transitions") == 1) {
			try {
				for (int from = 0; from < numStates; from++) {
					for (int to = 0; to < numStates; to++) {
						if(json["Transitions"].contains(ofToString(from)) &&
						   json["Transitions"][ofToString(from)].contains(ofToString(to))) {
							
							float value = json["Transitions"][ofToString(from)][ofToString(to)];
							transitionMatrices[from][to] = value;
						}
					}
				}
			}
			catch(const std::exception& e) {
				ofLogError("markovVector") << "Error loading preset: " << e.what();
			}
		}
		
		// Recalculate output with loaded values
		calculateOutput();
	}

private:
	ofEventListeners listeners;
	ofParameter<int> numStates;
	ofParameter<int> initialState;
	ofParameter<vector<int>> duplicates;
	ofParameter<int> outputSize;
	ofParameter<bool> noRepeats;
	ofParameter<int> seed;
	ofParameter<void> recalculate;
	ofParameter<vector<int>> output;
	
	vector<vector<float>> transitionMatrices;
	customGuiRegion customWidget;
	
	int hoveredState = -1;
	
	void updateStateCount() {
		// Preserve existing values when possible
		vector<vector<float>> oldMatrix = transitionMatrices;
		
		// Resize matrices
		transitionMatrices.resize(16);
		for(auto &matrix : transitionMatrices) {
			matrix.resize(16, 0.0f);
		}
		
		// Initialize with uniform distribution for new states
		for(int i = 0; i < numStates; i++) {
			if(i < oldMatrix.size()) {
				for(int j = 0; j < numStates; j++) {
					if(j < oldMatrix[i].size()) {
						transitionMatrices[i][j] = oldMatrix[i][j];
					} else {
						// For new states, set to 0
						transitionMatrices[i][j] = 0.0f;
					}
				}
			} else {
				// New row, initialize with deterministic transitions
				for(int j = 0; j < numStates; j++) {
					transitionMatrices[i][j] = 0.0f;
				}
				// Set the next state to 1.0 (or loop back to 0 if at the end)
				int nextState = (i + 1) % numStates;
				transitionMatrices[i][nextState] = 1.0f;
			}
		}
		
		// Recalculate output
		calculateOutput();
	}
	
	vector<float> getNormalizedRow(int row) {
		vector<float> normalizedRow = transitionMatrices[row];
		float sum = 0.0f;
		
		// Calculate sum
		for(int i = 0; i < numStates; i++) {
			sum += normalizedRow[i];
		}
		
		// Normalize
		if(sum > 0.0f) {
			for(int i = 0; i < numStates; i++) {
				normalizedRow[i] /= sum;
			}
		} else {
			// If sum is zero, distribute evenly
			for(int i = 0; i < numStates; i++) {
				normalizedRow[i] = 1.0f / numStates;
			}
		}
		
		return normalizedRow;
	}
	
	void calculateOutput() {
		vector<int> result;
		
		// Ensure initial state is valid
		int currentState = ofClamp(initialState, 0, numStates - 1);
		
		// Set up random generator
		std::mt19937 gen;
		if(seed > 0) {
			gen.seed(seed);
		} else {
			std::random_device rd;
			gen.seed(rd());
		}
		
		if(noRepeats) {
			// Track available states
			vector<bool> availableStates(numStates, true);
			int statesRemaining = numStates;
			
			while(statesRemaining > 0) {
				// Add current state to result with duplication
				int duplicateCount = 1;
				if(currentState < duplicates.get().size()) {
					duplicateCount = duplicates.get()[currentState];
				} else if(!duplicates.get().empty()) {
					duplicateCount = duplicates.get().back();
				}
				
				for(int i = 0; i < duplicateCount; i++) {
					result.push_back(currentState);
				}
				
				// Mark current state as used
				availableStates[currentState] = false;
				statesRemaining--;
				
				if(statesRemaining > 0) {
					// Get normalized probabilities for current state
					vector<float> tempDistribution = getNormalizedRow(currentState);
					
					// Zero out unavailable states and re-normalize
					float sum = 0.0f;
					for(int i = 0; i < numStates; i++) {
						if(!availableStates[i]) {
							tempDistribution[i] = 0.0f;
						} else {
							sum += tempDistribution[i];
						}
					}
					
					// Re-normalize available states
					if(sum > 0.0f) {
						for(int i = 0; i < numStates; i++) {
							tempDistribution[i] /= sum;
						}
					} else {
						// If all probabilities are zero, distribute evenly among available states
						float evenProb = 1.0f / statesRemaining;
						for(int i = 0; i < numStates; i++) {
							if(availableStates[i]) {
								tempDistribution[i] = evenProb;
							}
						}
					}
					
					// Select next state based on temporary distribution
					std::discrete_distribution<> dist(tempDistribution.begin(), tempDistribution.end());
					currentState = dist(gen);
				}
			}
		} else {
			// Normal Markov chain with repeats allowed
			int targetSize = outputSize;
			
			while(result.size() < targetSize) {
				// Add current state to result with duplication
				int duplicateCount = 1;
				if(currentState < duplicates.get().size()) {
					duplicateCount = duplicates.get()[currentState];
				} else if(!duplicates.get().empty()) {
					duplicateCount = duplicates.get().back();
				}
				
				// Ensure we don't exceed target size
				duplicateCount = std::min(duplicateCount, targetSize - (int)result.size());
				
				for(int i = 0; i < duplicateCount; i++) {
					result.push_back(currentState);
					if(result.size() >= targetSize) break;
				}
				
				if(result.size() >= targetSize) break;
				
				// Get normalized probabilities for current state
				vector<float> normalizedRow = getNormalizedRow(currentState);
				
				// Select next state based on normalized probabilities
				std::discrete_distribution<> dist(normalizedRow.begin(), normalizedRow.begin() + numStates);
				currentState = dist(gen);
			}
		}
		
		// Update output
		output = result;
	}
	
	void drawTransitionMatrix() {
		// Title and spacing
		ImGui::Text("Transition Probabilities");
		ImGui::Spacing();
		
		// Draw multisliders for each source state
		for(int from = 0; from < numStates; from++) {
			ImGui::PushID(from);
			
			// Label for current state
			ImGui::Text("From %d", from);
			
			float* values = transitionMatrices[from].data();
			
			// Using the same approach as multiSliderMatrix
			auto values_getter = [](void* data, int idx) -> float {
				const float v = ((float*)data)[idx];
				return v;
			};
			
			auto cursorPos = ImGui::GetCursorScreenPos();
			
			// Use fixed width instead of GetContentRegionAvail
			// This is the key change to fix the sizing issue
			const float sliderWidth = 250.0f;
			const ImVec2 frame_size = ImVec2(sliderWidth, ImGui::GetFrameHeight() * 2);
			
			ImGui::InvisibleButton(("##slider" + ofToString(from)).c_str(), frame_size);
			
			bool hovered = ImGui::IsItemHovered();
			bool press = ImGui::IsMouseDown(0);
			
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const ImRect frame_bb(cursorPos, cursorPos + frame_size);
			const ImRect inner_bb(frame_bb.Min + ImGui::GetStyle().FramePadding,
								 frame_bb.Max - ImGui::GetStyle().FramePadding);
			
			// Draw frame background
			ImGui::RenderFrame(inner_bb.Min, inner_bb.Max,
							  ImGui::GetColorU32(ImGuiCol_FrameBg), true, ImGui::GetStyle().FrameRounding);
			
			if (values != NULL && numStates >= 1) {
				int res_w = (int)frame_bb.GetWidth() < (int)numStates ? (int)frame_bb.GetWidth() : (int)numStates;
				int item_count = numStates;
				
				// Handle mouse interaction
				if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0)) {
					ImVec2 mousePos = ImGui::GetIO().MousePos;
					ImVec2 mousePosLast = mousePos - ImGui::GetIO().MouseDelta;
					
					const float t0 = ImClamp((mousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
					const float t1 = ImClamp((mousePosLast.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
					float nVal0 = 1 - ImClamp((mousePos.y - inner_bb.Min.y) / (inner_bb.Max.y - inner_bb.Min.y), 0.0f, 1.0f);
					float nVal1 = 1 - ImClamp((mousePosLast.y - inner_bb.Min.y) / (inner_bb.Max.y - inner_bb.Min.y), 0.0f, 1.0f);
					int v_idx0 = (int)(t0 * item_count);
					int v_idx1 = (int)(t1 * item_count);
					v_idx0 = ImClamp(v_idx0, 0, numStates - 1);
					v_idx1 = ImClamp(v_idx1, 0, numStates - 1);
					
					if(v_idx1 < v_idx0) {
						std::swap(v_idx0, v_idx1);
						std::swap(nVal0, nVal1);
					}
					
					for(int v_idx = v_idx0; v_idx <= v_idx1; v_idx++) {
						float pctPos = 0;
						if(v_idx0 != v_idx1) {
							pctPos = float(v_idx-v_idx0) / float(v_idx1-v_idx0);
						}
						float newValue = ofLerp(nVal0, nVal1, pctPos);
						if(ImGui::GetIO().KeyShift) {
							newValue = std::round(newValue * 10.0f) / 10.0f; // Snap to 0.1 increments with shift
						}
						transitionMatrices[from][v_idx] = newValue;
					}
					
					// Recalculate output with new values
					calculateOutput();
				}
				
				// Get normalized values for visualization
				vector<float> normalizedRow = getNormalizedRow(from);
				
				// Get hover info
				if(hovered) {
					const float t = ImClamp((ImGui::GetIO().MousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
					const int v_idx = (int)(t * item_count);
					hoveredState = v_idx;
				}
				
				// Draw histogram bars
				const float t_step = 1.0f / (float)res_w;
				
				for (int n = 0; n < res_w; n++) {
					const int v_idx = (int)(t_step * n * item_count);
					if (v_idx >= numStates) continue;
					
					// Calculate bar geometry
					const float val = values_getter((void*)values, v_idx);
					const float normalized_val = normalizedRow[v_idx]; // Normalized value for coloring
					
					const float x0 = inner_bb.Min.x + inner_bb.GetWidth() * t_step * n;
					const float x1 = inner_bb.Min.x + inner_bb.GetWidth() * t_step * (n + 1);
					const float y0 = inner_bb.Min.y;
					const float y1 = inner_bb.Max.y;
					const float y_normalized = inner_bb.Min.y + (inner_bb.Max.y - inner_bb.Min.y) * (1.0f - normalized_val);
					
					// Alternate background color for better visibility
					if (n % 2 == 0) {
						ImVec4 altBgColor = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
						altBgColor.x *= 1.1f; // Slightly lighter
						altBgColor.y *= 1.1f;
						altBgColor.z *= 1.1f;
						drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
											  ImGui::ColorConvertFloat4ToU32(altBgColor));
					}
					
					// Generate color based on normalized value
					ImU32 barColor;
					if(normalized_val > 0.95f) {
						// Almost 1.0 - use orange/gold
						barColor = IM_COL32(255, 165, 0, 255);
					} else if(normalized_val < 0.5f) {
						// Blue to purple gradient
						barColor = ImColor::HSV(0.66f, 0.7f, 0.4f + normalized_val);
					} else {
						// Purple to red gradient
						barColor = ImColor::HSV(0.66f - (normalized_val - 0.5f) * 1.32f, 0.7f, 0.7f);
					}
					
					// Draw bar for current value
					drawList->AddRectFilled(ImVec2(x0, y_normalized), ImVec2(x1, y1),
										  hoveredState == v_idx ?
										  ImGui::GetColorU32(ImGuiCol_PlotHistogramHovered) : barColor);
					
					// Add state number at the top of each bar
					char label[8];
					snprintf(label, sizeof(label), "%d", v_idx);
					ImVec2 textSize = ImGui::CalcTextSize(label);
					drawList->AddText(ImVec2(x0 + (x1 - x0 - textSize.x) * 0.5f, y0 + 2),
									 IM_COL32(255, 255, 255, 255), label);
				}
			}
			
			ImGui::PopID();
			ImGui::Spacing();
		}
	}
};

#endif /* markovVector_h */
