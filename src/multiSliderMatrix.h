#ifndef multiSliderMatrix_h
#define multiSliderMatrix_h

#include "ofxOceanodeNodeModel.h"
#include "imgui_internal.h"

class multiSliderMatrix : public ofxOceanodeNodeModel {
public:
	multiSliderMatrix() : ofxOceanodeNodeModel("Multi Slider Matrix") {
		description = "A matrix of multiple sliders that output vector values. "
					  "Each slider can be independently controlled and outputs its own vector.";
					 
		// Number of sliders in inspector
		addInspectorParameter(numSliders.set("Num Sliders", 8, 1, 16));
	}

	void setup() override {
		addParameter(size.set("Size[]", vector<int>(1, 10), vector<int>(1, 2), vector<int>(1, INT_MAX)));
		addParameter(minVal.set("Min[]", vector<float>(1, 0), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));
		addParameter(maxVal.set("Max[]", vector<float>(1, 1), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));
		addParameter(currentSlot.set("Slot", 0, 0, 31));

		// Quantization steps (Q). Q <= 1 => no quantization.
		addParameter(quantizationSteps.set("Q", 0, 0, 128));

		// Inspector parameters for widget dimensions (like multiSliderGrid)
		addInspectorParameter(width.set("Width", 240, 100, 800));
		addInspectorParameter(height.set("Height", 40, 20, 500));

		// Allocate storage: [slot][slider][values]
		vectorValues.resize(32);
		for(auto &slot : vectorValues) {
			slot.resize(numSliders);
			for(auto &sliderValues : slot) {
				sliderValues.resize(10, 0.0f);
			}
		}
		
		vectorValueParams.resize(numSliders);
		currentToEditValues.resize(numSliders);
		
		// Outputs per slider
		for (int i = 0; i < numSliders; i++) {
			vectorValueParams[i].set("Out " + ofToString(i + 1),
									 vectorValues[currentSlot][i],
									 vector<float>(1, getValueForIndex(minVal, i)),
									 vector<float>(1, getValueForIndex(maxVal, i)));
			addOutputParameter(vectorValueParams[i],
							   ofxOceanodeParameterFlags_DisableInConnection |
							   ofxOceanodeParameterFlags_DisplayMinimized);
		}

		// Chained output (concatenation of all slider vectors)
		chainedOutput.set("ChainedOut",
						  vector<float>(1, 0.0f),
						  vector<float>(1, getValueForIndex(minVal, 0)),
						  vector<float>(1, getValueForIndex(maxVal, 0)));
		addOutputParameter(chainedOutput,
						   ofxOceanodeParameterFlags_DisableInConnection |
						   ofxOceanodeParameterFlags_DisplayMinimized);

		// Custom GUI region
		addCustomRegion(customWidget, [this]() {
			for (int i = 0; i < numSliders; i++) {
				drawMultiSlider(i);
			}
		});
		
		setupListeners();
	}

	void setupListeners() {
		// Num sliders changes
		listeners.push(numSliders.newListener([this](int &){
			updateSliderCount();
		}));

		// Size changes
		listeners.push(size.newListener([this](vector<int> &s) {
			updateSizes();
		}));

		// Min/max changes
		listeners.push(minVal.newListener([this](vector<float> &f) {
			updateMinMaxValues();
		}));

		listeners.push(maxVal.newListener([this](vector<float> &f) {
			updateMinMaxValues();
		}));
		   
		// Slot changes
		listeners.push(currentSlot.newListener([this](int &s) {
			updateOutputs();
		}));

		// Quantization changes
		listeners.push(quantizationSteps.newListener([this](int &q) {
			applyQuantizationToAll();
		}));
	}

	void update(ofEventArgs &a) override {
		updateOutputs();
	}

	void updateOutputs() {
		updateSizes(); // keep sizes sane

		for (int i = 0; i < numSliders; i++) {
			if(i < vectorValues[currentSlot].size()) {
				vectorValueParams[i].set(vectorValues[currentSlot][i]);
			}
		}
		updateChainedOutput();
	}

	void presetSave(ofJson &json) override {
		// Save all slots
		for (int slot = 0; slot < 32; slot++) {
			for (int slider = 0; slider < vectorValues[slot].size(); slider++) {
				json["Values"][ofToString(slot)][ofToString(slider)] = vectorValues[slot][slider];
			}
		}
	}

	void presetRecallAfterSettingParameters(ofJson &json) override {
		updateSizes();
			
		if (json.count("Values") == 1) {
			try {
				for (int slot = 0; slot < 32; slot++) {
					for (int slider = 0; slider < vectorValues[slot].size(); slider++) {
						if(json["Values"].contains(ofToString(slot)) &&
						   json["Values"][ofToString(slot)].contains(ofToString(slider))) {
								
							auto newValues = json["Values"][ofToString(slot)][ofToString(slider)]
												.get<vector<float>>();
								
							int targetSize = getValueForIndex(size, slider);
							if(newValues.size() > targetSize) {
								newValues.resize(targetSize);
							}
							while(newValues.size() < targetSize) {
								newValues.push_back(0.0f);
							}
								
							vectorValues[slot][slider] = newValues;
						}
					}
				}
			}
			catch(const std::exception& e) {
				ofLogError("multiSliderMatrix") << "Error loading preset: " << e.what();
			}
		}
			
		updateSizes();
		updateMinMaxValues();
		updateOutputs();
	}

	void presetHasLoaded() override {
		updateSizes();
		updateMinMaxValues();
		updateOutputs();
	}

private:
	ofEventListeners listeners;

	// Main params
	ofParameter<int> numSliders;
	ofParameter<vector<int>> size;
	ofParameter<vector<float>> minVal;
	ofParameter<vector<float>> maxVal;
	ofParameter<int> currentSlot;
	ofParameter<int> quantizationSteps; // Q

	// Inspector-dimension params
	ofParameter<int> width;
	ofParameter<int> height;

	// Data
	vector<vector<vector<float>>> vectorValues; // [slot][slider][values]
	vector<ofParameter<vector<float>>> vectorValueParams;
	ofParameter<vector<float>> chainedOutput;

	customGuiRegion customWidget;
	vector<int> currentToEditValues;
	
	// --- helpers for indexing base vectors ---
	int getValueForIndex(const vector<int>& vec, int index) {
		if (vec.empty()) return 0;
		if (vec.size() == 1) return vec[0];
		if (index < vec.size()) return vec[index];
		return vec.back();
	}

	float getValueForIndex(const vector<float>& vec, int index) {
		if (vec.empty()) return 0;
		if (vec.size() == 1) return vec[0];
		if (index < vec.size()) return vec[index];
		return vec.back();
	}

	int getValueForIndex(const ofParameter<vector<int>>& param, int index) {
		const auto& vec = param.get();
		if (vec.empty()) return 0;
		if (vec.size() == 1) return vec[0];
		if (index < vec.size()) return vec[index];
		return vec.back();
	}

	float getValueForIndex(const ofParameter<vector<float>>& param, int index) {
		const auto& vec = param.get();
		if (vec.empty()) return 0;
		if (vec.size() == 1) return vec[0];
		if (index < vec.size()) return vec[index];
		return vec.back();
	}

	// --- quantization helpers ---
	float quantizeForSlider(float value, int sliderIndex) {
		int Q = quantizationSteps.get();
		if (Q <= 1) {
			// no quantization
			return value;
		}

		float min = getValueForIndex(minVal, sliderIndex);
		float max = getValueForIndex(maxVal, sliderIndex);
		if (max <= min) return min;

		float t = ofMap(value, min, max, 0.0f, 1.0f, true);
		float idx = std::round(t * float(Q - 1));
		t = idx / float(Q - 1);
		return ofLerp(min, max, t);
	}

	void applyQuantizationToAll() {
		int Q = quantizationSteps.get();
		if (Q <= 1) {
			updateOutputs();
			return;
		}

		for (int slot = 0; slot < (int)vectorValues.size(); ++slot) {
			for (int i = 0; i < (int)vectorValues[slot].size(); ++i) {
				for (int j = 0; j < (int)vectorValues[slot][i].size(); ++j) {
					vectorValues[slot][i][j] = quantizeForSlider(vectorValues[slot][i][j], i);
				}
			}
		}
		updateOutputs();
	}

	// --- slider count / size / min-max ---
	void updateSliderCount() {
		int oldSize = vectorValues[currentSlot].size();
			
		for(auto &slot : vectorValues) {
			slot.resize(numSliders);
			for(auto &sliderValues : slot) {
				if(sliderValues.empty()) {
					sliderValues.resize(getValueForIndex(size, 0), 0.0f);
				}
			}
		}
			
		vectorValueParams.resize(numSliders);
		currentToEditValues.resize(numSliders);
			
		if (numSliders < oldSize) {
			for (int i = numSliders; i < oldSize; i++) {
				removeParameter("Out " + ofToString(i + 1));
			}
		}
			
		if (numSliders > oldSize) {
			for (int i = 0; i < oldSize; i++) {
				removeParameter("Out " + ofToString(i + 1));
			}
				
			for (int i = 0; i < numSliders; i++) {
				vectorValueParams[i].set("Out " + ofToString(i + 1),
										 vectorValues[currentSlot][i],
										 vector<float>(1, getValueForIndex(minVal, i)),
										 vector<float>(1, getValueForIndex(maxVal, i)));
				addParameter(vectorValueParams[i],
							 ofxOceanodeParameterFlags_DisableInConnection |
							 ofxOceanodeParameterFlags_DisplayMinimized);
			}
		}
			
		updateSizes();
		updateMinMaxValues();
	}
	
	void updateSizes() {
		for(auto &slot : vectorValues) {
			slot.resize(numSliders);
			for (int i = 0; i < (int)slot.size(); i++) {
				int targetSize = getValueForIndex(size, i);
				if(targetSize <= 0) targetSize = 1;
				   
				if((int)slot[i].size() != targetSize) {
					vector<float> temp = slot[i];
					slot[i].resize(targetSize, 0.0f);
					   
					for(int j = 0; j < std::min(targetSize, (int)temp.size()); j++) {
						slot[i][j] = temp[j];
					}
				}
			}
		}
	}

	void updateChainedRange() {
		float globalMin = FLT_MAX;
		float globalMax = -FLT_MAX;

		for (int i = 0; i < numSliders; ++i) {
			float mn = getValueForIndex(minVal, i);
			float mx = getValueForIndex(maxVal, i);
			globalMin = std::min(globalMin, mn);
			globalMax = std::max(globalMax, mx);
		}

		if (globalMin == FLT_MAX || globalMax == -FLT_MAX) {
			globalMin = 0.0f;
			globalMax = 1.0f;
		}

		chainedOutput.setMin(vector<float>(1, globalMin));
		chainedOutput.setMax(vector<float>(1, globalMax));
	}

	void updateMinMaxValues() {
		for(auto &slot : vectorValues) {
			for (int i = 0; i < (int)slot.size(); i++) {
				float min = getValueForIndex(minVal, i);
				float max = getValueForIndex(maxVal, i);
				for (auto &val : slot[i]) {
					val = ofClamp(val, min, max);
					val = quantizeForSlider(val, i);
				}
				if(i < (int)vectorValueParams.size()) {
					vectorValueParams[i].setMin(vector<float>(1, min));
					vectorValueParams[i].setMax(vector<float>(1, max));
				}
			}
		}

		updateChainedRange();
		updateOutputs();
	}

	void updateChainedOutput() {
		vector<float> chained;
		if (currentSlot.get() >= 0 && currentSlot.get() < (int)vectorValues.size()) {
			auto &slot = vectorValues[currentSlot.get()];
			for (int i = 0; i < numSliders && i < (int)slot.size(); ++i) {
				auto &vec = slot[i];
				chained.insert(chained.end(), vec.begin(), vec.end());
			}
		}
		if (chained.empty()) {
			chained.push_back(0.0f);
		}
		chainedOutput.set(chained);
	}

	// --- GUI ---
	void drawMultiSlider(int index) {
		if (index >= (int)vectorValues[currentSlot].size()) return;

		auto values_getter = [](void* data, int idx) -> float {
			const float v = *(const float*)((const unsigned char*)data + (size_t)idx * sizeof(float));
			return v;
		};

		auto cursorPos = ImGui::GetCursorScreenPos();
		
		ImGui::PushID(index);

		// Use inspector width/height
		ImVec2 frame_size((float)width.get(), (float)height.get());

		ImGui::InvisibleButton(("##InvBox" + ofToString(index)).c_str(), frame_size);
		
		auto drawList = ImGui::GetWindowDrawList();
		
		void* data = (void*)vectorValues[currentSlot][index].data();
		float scale_min = getValueForIndex(minVal, index);
		float scale_max = getValueForIndex(maxVal, index);
		int values_count = getValueForIndex(size, index);
		
		const ImGuiStyle& style = ImGui::GetStyle();
		const ImRect frame_bb(cursorPos, cursorPos + frame_size);
		const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
		
		ImGui::RenderFrame(inner_bb.Min, inner_bb.Max,
						   ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

		bool hovered = ImGui::IsItemHovered(); (void)hovered;
		bool press = ImGui::IsMouseDown(0);   (void)press;
		
		int idx_hovered = -1;
		if (values_count >= 1) {
			int res_w = ImMin((int)frame_size.x, values_count);
			int item_count = values_count;

			auto mousePos = ImGui::GetIO().MousePos;
			auto mousePosPrev = mousePos - ImGui::GetIO().MouseDelta;
		
			// Drag edit
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0)) {
				const float t0 = ImClamp((mousePos.x - inner_bb.Min.x) /
										 (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
				const float t1 = ImClamp((mousePosPrev.x - inner_bb.Min.x) /
										 (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
				float nVal0 = 1 - ImClamp((mousePos.y - inner_bb.Min.y) /
										  (inner_bb.Max.y - inner_bb.Min.y), 0.0f, 1.0f);
				float nVal1 = 1 - ImClamp((mousePosPrev.y - inner_bb.Min.y) /
										  (inner_bb.Max.y - inner_bb.Min.y), 0.0f, 1.0f);
				int v_idx0 = (int)(t0 * item_count);
				int v_idx1 = (int)(t1 * item_count);
				IM_ASSERT(v_idx0 >= 0 && v_idx0 < values_count);
				IM_ASSERT(v_idx1 >= 0 && v_idx1 < values_count);

				if(v_idx1 < v_idx0){
					std::swap(v_idx0, v_idx1);
					std::swap(nVal0, nVal1);
				}
								
				for(int v_idx = v_idx0; v_idx <= v_idx1; v_idx++){
					float pctPos = 0;
					if(v_idx0 != v_idx1){
						pctPos = float(v_idx-v_idx0) / float(v_idx1-v_idx0);
					}
					float newValue = ofMap(ofLerp(nVal0, nVal1, pctPos), 0, 1,
										   scale_min, scale_max, true);
					if(ImGui::GetIO().KeyShift) {
						newValue = std::round(newValue);
					}
					newValue = quantizeForSlider(newValue, index);
					vectorValues[currentSlot][index][v_idx] = newValue;
				}
								
				idx_hovered = v_idx0;
			}
			
			// Right-click popup
			if(ImGui::IsItemClicked(1) ||
			   (ImGui::IsPopupOpen(("Value Popup " + ofToString(index)).c_str())
				&& ImGui::IsMouseClicked(1))) {
				ImGui::OpenPopup(("Value Popup " + ofToString(index)).c_str());
				const float t = ImClamp((mousePos.x - inner_bb.Min.x) /
										(inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
				const int v_idx = (int)(t * item_count);
				IM_ASSERT(v_idx >= 0 && v_idx < values_count);
				currentToEditValues[index] = v_idx;
			}

			// Histogram
			const float t_step = 1.0f / (float)res_w;
			const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));

			float v0 = values_getter(data, 0);
			float t0_draw = 0.0f;
			ImVec2 tp0 = ImVec2(t0_draw, 1.0f - ImSaturate((v0 - scale_min) * inv_scale));
			float histogram_zero_line_t =
				(scale_min * scale_max < 0.0f) ? (-scale_min * inv_scale)
											   : (scale_min < 0.0f ? 0.0f : 1.0f);

			const ImU32 col_base = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
			const ImU32 col_hovered = ImGui::GetColorU32(ImGuiCol_PlotHistogramHovered);

			ImVec4 base_color = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
			ImVec4 alt_color = ImVec4(
				base_color.x * 1.1f,
				base_color.y * 1.1f,
				base_color.z * 1.1f,
				base_color.w
			);
			const ImU32 col_bg_alt = ImGui::ColorConvertFloat4ToU32(alt_color);

			for (int n = 0; n < res_w; n++) {
				const float t1_draw = t0_draw + t_step;
				const int v1_idx = (int)(t0_draw * item_count + 0.5f);
				IM_ASSERT(v1_idx >= 0 && v1_idx < values_count);
				const float v1 = values_getter(data, (v1_idx + 1) % values_count);
				const ImVec2 tp1 = ImVec2(t1_draw, 1.0f - ImSaturate((v1 - scale_min) * inv_scale));

				ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
				ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max,
									 ImVec2(tp1.x, histogram_zero_line_t));
				
				if (pos1.x >= pos0.x + 2.0f)
					pos1.x -= 1.0f;

				if (n % 2 == 0) {
					ImVec2 bg_pos0 = ImVec2(pos0.x, inner_bb.Min.y);
					ImVec2 bg_pos1 = ImVec2(pos1.x, inner_bb.Max.y);
					drawList->AddRectFilled(bg_pos0, bg_pos1, col_bg_alt);
				}

				drawList->AddRectFilled(
					pos0, pos1,
					idx_hovered == v1_idx ? col_hovered : col_base);

				t0_draw = t1_draw;
				tp0 = tp1;
			}
			
			// Popup edit
			if (ImGui::BeginPopup(("Value Popup " + ofToString(index)).c_str())) {
				ImGui::Text("Edit item %d on slot %d",
							currentToEditValues[index],
							currentSlot.get());
				float currentValue =
					vectorValues[currentSlot][index][currentToEditValues[index]];
				if (ImGui::SliderFloat("##edit", &currentValue,
									   scale_min, scale_max, "%.4f")) {
					currentValue = quantizeForSlider(currentValue, index);
					vectorValues[currentSlot][index][currentToEditValues[index]] =
						currentValue;
				}
				if (ImGui::Button("Close")) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		
		ImGui::PopID();
	}
};

#endif /* multiSliderMatrix_h */
