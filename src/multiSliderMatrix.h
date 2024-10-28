#ifndef multiSliderMatrix_h
#define multiSliderMatrix_h

#include "ofxOceanodeNodeModel.h"
#include "imgui_internal.h"

class multiSliderMatrix : public ofxOceanodeNodeModel {
public:
    multiSliderMatrix() : ofxOceanodeNodeModel("Multi Slider Matrix") {
        description = "A matrix of multiple sliders that output vector values. "
                     "Each slider can be independently controlled and outputs its own vector.";
                     
        // Add inspector parameter for number of sliders
        addInspectorParameter(numSliders.set("Num Sliders", 8, 1, 16));
    }

    void setup() {
        addParameter(size.set("Size[]", vector<int>(1, 10), vector<int>(1, 2), vector<int>(1, INT_MAX)));
        addParameter(minVal.set("Min[]", vector<float>(1, 0), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));
        addParameter(maxVal.set("Max[]", vector<float>(1, 1), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));
        addParameter(currentSlot.set("Slot", 0, 0, 31));
        
        
        vectorValues.resize(32);
               for(auto &slot : vectorValues) {
                   slot.resize(numSliders);
                   for(auto &sliderValues : slot) {
                       sliderValues.resize(10, 0.0f);
                   }
               }
        
        vectorValueParams.resize(numSliders);
        currentToEditValues.resize(numSliders);
        
        // Add output parameters before custom region
                for (int i = 0; i < numSliders; i++) {
                    vectorValueParams[i].set("Out " + ofToString(i + 1),
                                           vectorValues[currentSlot][i],
                                           vector<float>(1, getValueForIndex(minVal, i)),
                                           vector<float>(1, getValueForIndex(maxVal, i)));
                    addParameter(vectorValueParams[i],
                               ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_DisplayMinimized);
                }

                // Add custom region last (will be at the bottom)
                addCustomRegion(customWidget, [this]() {
                    for (int i = 0; i < numSliders; i++) {
                        drawMultiSlider(i);
                    }
                });
        
        setupListeners();
    }


    void setupListeners() {
           // Listen for changes in numSliders
           listeners.push(numSliders.newListener([this](int &){
               updateSliderCount();
           }));

           listeners.push(size.newListener([this](vector<int> &s) {
               updateSizes();
           }));

           listeners.push(minVal.newListener([this](vector<float> &f) {
               updateMinMaxValues();
           }));

           listeners.push(maxVal.newListener([this](vector<float> &f) {
               updateMinMaxValues();
           }));
           
           listeners.push(currentSlot.newListener([this](int &s) {
               updateOutputs();
           }));
       }

    void update(ofEventArgs &a) {
            updateOutputs();
        }

    void updateOutputs() {
            updateSizes(); // Make sure sizes are correct before updating outputs
            for (int i = 0; i < numSliders; i++) {
                if(i < vectorValues[currentSlot].size()) {
                    vectorValueParams[i].set(vectorValues[currentSlot][i]);
                }
            }
        }

    void presetSave(ofJson &json) {
           // Save all slots
           for (int slot = 0; slot < 32; slot++) {
               for (int slider = 0; slider < vectorValues[slot].size(); slider++) {
                   json["Values"][ofToString(slot)][ofToString(slider)] = vectorValues[slot][slider];
               }
           }
       }

    void presetRecallAfterSettingParameters(ofJson &json) {
            // First ensure sizes are properly set
            updateSizes();
            
            if (json.count("Values") == 1) {
                try {
                    // Store current values temporarily
                    auto tempValues = vectorValues;
                    
                    // Load new values
                    for (int slot = 0; slot < 32; slot++) {
                        for (int slider = 0; slider < vectorValues[slot].size(); slider++) {
                            // Check if this slot/slider combination exists in the json
                            if(json["Values"].contains(ofToString(slot)) &&
                               json["Values"][ofToString(slot)].contains(ofToString(slider))) {
                                
                                auto newValues = json["Values"][ofToString(slot)][ofToString(slider)].get<vector<float>>();
                                
                                // Resize if necessary
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
            
            // Force update sizes again to ensure everything is correct
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
    ofParameter<int> numSliders;
    ofParameter<vector<int>> size;
    ofParameter<vector<float>> minVal;
    ofParameter<vector<float>> maxVal;
    ofParameter<int> currentSlot;

    vector<vector<vector<float>>> vectorValues; // [slot][slider][values]
       vector<ofParameter<vector<float>>> vectorValueParams;
       
    customGuiRegion customWidget;
    vector<int> currentToEditValues;
    
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

    void updateSliderCount() {
            // Store old size to handle removal
            int oldSize = vectorValues[currentSlot].size();
            
            // Resize vectors for all slots
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
            
            // If we're reducing the number of sliders
            if (numSliders < oldSize) {
                // Remove parameters for sliders that are no longer needed
                for (int i = numSliders; i < oldSize; i++) {
                    removeParameter("Out " + ofToString(i + 1));
                }
            }
            
            // If we're adding new sliders
            if (numSliders > oldSize) {
                // First remove all parameters
                for (int i = 0; i < oldSize; i++) {
                    removeParameter("Out " + ofToString(i + 1));
                }
                
                // Re-add all parameters
                for (int i = 0; i < numSliders; i++) {
                    vectorValueParams[i].set("Out " + ofToString(i + 1),
                                           vectorValues[currentSlot][i],
                                           vector<float>(1, getValueForIndex(minVal, i)),
                                           vector<float>(1, getValueForIndex(maxVal, i)));
                    addParameter(vectorValueParams[i],
                               ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_DisplayMinimized);
                }
            }
            
            updateSizes();
            updateMinMaxValues();
        }
    
    void updateSizes() {
           for(auto &slot : vectorValues) {
               slot.resize(numSliders);
               for (int i = 0; i < slot.size(); i++) {
                   int targetSize = getValueForIndex(size, i);
                   if(targetSize <= 0) targetSize = 1; // Prevent zero-size vectors
                   
                   // Only resize if necessary
                   if(slot[i].size() != targetSize) {
                       vector<float> temp = slot[i];
                       slot[i].resize(targetSize, 0.0f);
                       
                       // Preserve existing values
                       for(int j = 0; j < std::min(targetSize, (int)temp.size()); j++) {
                           slot[i][j] = temp[j];
                       }
                   }
               }
           }
       }

        void updateMinMaxValues() {
            for(auto &slot : vectorValues) {
                for (int i = 0; i < slot.size(); i++) {
                    float min = getValueForIndex(minVal, i);
                    float max = getValueForIndex(maxVal, i);
                    for (auto &val : slot[i]) {
                        val = ofClamp(val, min, max);
                    }
                    if(i < vectorValueParams.size()) {
                        vectorValueParams[i].setMin(vector<float>(1, min));
                        vectorValueParams[i].setMax(vector<float>(1, max));
                    }
                }
            }
            updateOutputs();
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


    void drawMultiSlider(int index) {
        if (index >= vectorValues[currentSlot].size()) return;


        auto values_getter = [](void* data, int idx) -> float {
            const float v = *(const float*)(const void*)((const unsigned char*)data + (size_t)idx * sizeof(float));
            return v;
        };

        auto cursorPos = ImGui::GetCursorScreenPos();
        
        ImGui::PushID(index);  // Push ID to avoid conflicts with other sliders
        ImGui::InvisibleButton(("##InvBox" + ofToString(index)).c_str(), ImVec2(250, ImGui::GetFrameHeight() * 2));
        
        auto drawList = ImGui::GetWindowDrawList();
        
        void* data = (void*)vectorValues[currentSlot][index].data();
                float scale_min = getValueForIndex(minVal, index);
                float scale_max = getValueForIndex(maxVal, index);
                int values_count = getValueForIndex(size, index);
                ImVec2 frame_size = ImVec2(250, ImGui::GetFrameHeight() * 2);
        
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImRect frame_bb(cursorPos, cursorPos + frame_size);
        const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
        
        ImGui::RenderFrame(inner_bb.Min, inner_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

        bool hovered = ImGui::IsItemHovered();
        bool press = ImGui::IsMouseDown(0);
        
        int idx_hovered = -1;
        if (values_count >= 1) {
            int res_w = ImMin((int)frame_size.x, values_count);
            int item_count = values_count;

            auto mousePos = ImGui::GetIO().MousePos;
            auto mousePosPrev = mousePos - ImGui::GetIO().MouseDelta;
        
            // Modify on mouse drag
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0)) {
                const float t0 = ImClamp((mousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
                const float t1 = ImClamp((mousePosPrev.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
                float nVal0 = 1 - ImClamp((mousePos.y - inner_bb.Min.y) / (inner_bb.Max.y - inner_bb.Min.y), 0.0f, 1.0f);
                float nVal1 = 1 - ImClamp((mousePosPrev.y - inner_bb.Min.y) / (inner_bb.Max.y - inner_bb.Min.y), 0.0f, 1.0f);
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
                                    float newValue = ofMap(ofLerp(nVal0, nVal1, pctPos), 0, 1, scale_min, scale_max, true);
                                    if(ImGui::GetIO().KeyShift) {
                                        newValue = std::round(newValue);
                                    }
                                    vectorValues[currentSlot][index][v_idx] = newValue;
                                }
                                
                                idx_hovered = v_idx0;
                            }
            
            // Handle right-click popup
            if(ImGui::IsItemClicked(1) || (ImGui::IsPopupOpen(("Value Popup " + ofToString(index)).c_str()) && ImGui::IsMouseClicked(1))){
                            ImGui::OpenPopup(("Value Popup " + ofToString(index)).c_str());
                            const float t = ImClamp((mousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
                            const int v_idx = (int)(t * item_count);
                            IM_ASSERT(v_idx >= 0 && v_idx < values_count);
                            currentToEditValues[index] = v_idx;
                        }

            // Draw histogram
            const float t_step = 1.0f / (float)res_w;
            const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));

            float v0 = values_getter(data, 0);
            float t0 = 0.0f;
            ImVec2 tp0 = ImVec2(t0, 1.0f - ImSaturate((v0 - scale_min) * inv_scale));
            float histogram_zero_line_t = (scale_min * scale_max < 0.0f) ? (-scale_min * inv_scale) : (scale_min < 0.0f ? 0.0f : 1.0f);

            const ImU32 col_base = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
            const ImU32 col_hovered = ImGui::GetColorU32(ImGuiCol_PlotHistogramHovered);
            // Create a custom color for the alternate background
            ImVec4 base_color = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
            ImVec4 alt_color = ImVec4(
                base_color.x * 1.1f,
                base_color.y * 1.1f,
                base_color.z * 1.1f,
                base_color.w
            );
            const ImU32 col_bg_alt = ImGui::ColorConvertFloat4ToU32(alt_color);

            for (int n = 0; n < res_w; n++) {
                const float t1 = t0 + t_step;
                const int v1_idx = (int)(t0 * item_count + 0.5f);
                IM_ASSERT(v1_idx >= 0 && v1_idx < values_count);
                const float v1 = values_getter(data, (v1_idx + 1) % values_count);
                const ImVec2 tp1 = ImVec2(t1, 1.0f - ImSaturate((v1 - scale_min) * inv_scale));

                ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
                ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, ImVec2(tp1.x, histogram_zero_line_t));
                
                if (pos1.x >= pos0.x + 2.0f)
                    pos1.x -= 1.0f;

                // Draw alternating background
                if (n % 2 == 0) {
                    ImVec2 bg_pos0 = ImVec2(pos0.x, inner_bb.Min.y);
                    ImVec2 bg_pos1 = ImVec2(pos1.x, inner_bb.Max.y);
                    drawList->AddRectFilled(bg_pos0, bg_pos1, col_bg_alt);
                }

                drawList->AddRectFilled(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);

                t0 = t1;
                tp0 = tp1;
            }
            
            // Handle popup
            if (ImGui::BeginPopup(("Value Popup " + ofToString(index)).c_str())) {
                            ImGui::Text("Edit item %d on slot %d", currentToEditValues[index], currentSlot.get());
                            float currentValue = vectorValues[currentSlot][index][currentToEditValues[index]];
                            if (ImGui::SliderFloat("##edit", &currentValue, scale_min, scale_max, "%.4f")) {
                                vectorValues[currentSlot][index][currentToEditValues[index]] = currentValue;
                            }
                            if (ImGui::Button("Close")) {
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
        }
        
        ImGui::PopID();  // Pop ID
            }
        };

        #endif /* multiSliderMatrix_h */
