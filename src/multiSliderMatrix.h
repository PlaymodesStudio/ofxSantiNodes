#ifndef multiSliderMatrix_h
#define multiSliderMatrix_h

#include "ofxOceanodeNodeModel.h"
#include "imgui_internal.h"

class multiSliderMatrix : public ofxOceanodeNodeModel {
public:
    multiSliderMatrix() : ofxOceanodeNodeModel("Multi Slider Matrix") {}

    void setup() {
        addParameter(size.set("Size[]", vector<int>(1, 10), vector<int>(1, 2), vector<int>(1, INT_MAX)));
        addParameter(minVal.set("Min[]", vector<float>(1, 0), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));
        addParameter(maxVal.set("Max[]", vector<float>(1, 1), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));

        const int NUM_SLIDERS = 8;
        vectorValues.resize(NUM_SLIDERS);
        for (auto &v : vectorValues) {
            v.resize(10, 0);  // Default size
        }

        addCustomRegion(customWidget, [this]() {
            for (int i = 0; i < NUM_SLIDERS; i++) {
                drawMultiSlider(i);
            }
        });
        
        vectorValueParams.resize(NUM_SLIDERS);
        for (int i = 0; i < NUM_SLIDERS; ++i) {
            vectorValueParams[i].set("Out " + ofToString(i + 1), vectorValues[i], vector<float>(1, 0), vector<float>(1, 1));
            addParameter(vectorValueParams[i],
                         ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_DisplayMinimized);
        }

        listeners.push(size.newListener([this](vector<int> &s) {
            updateSizes();
        }));

        listeners.push(minVal.newListener([this](vector<float> &f) {
            updateMinMaxValues();
        }));

        listeners.push(maxVal.newListener([this](vector<float> &f) {
            updateMinMaxValues();
        }));
    }

    void update(ofEventArgs &a) {
        for (int i = 0; i < vectorValues.size(); i++) {
            vectorValueParams[i] = vectorValues[i];
        }
    }

    void presetSave(ofJson &json) {
        json["Values"] = vectorValues;
    }

    void presetRecallAfterSettingParameters(ofJson &json) {
        if (json.count("Values") == 1) {
            vectorValues = json["Values"].get<vector<vector<float>>>();
            updateSizes();
            updateMinMaxValues();
        }
    }

    void presetHasLoaded() override {
        for (int i = 0; i < vectorValues.size(); i++) {
            vectorValueParams[i] = vectorValues[i];
        }
    }

private:
    ofEventListeners listeners;

    ofParameter<vector<int>> size;
    ofParameter<vector<float>> minVal;
    ofParameter<vector<float>> maxVal;
    vector<vector<float>> vectorValues;
    vector<ofParameter<vector<float>>> vectorValueParams;
    customGuiRegion customWidget;

    vector<int> currentToEditValues;

    int getValueForIndex(const vector<int>& vec, int index) {
        if (vec.size() == 1) return vec[0];
        if (index < vec.size()) return vec[index];
        return vec.back();
    }

    float getValueForIndex(const vector<float>& vec, int index) {
        if (vec.size() == 1) return vec[0];
        if (index < vec.size()) return vec[index];
        return vec.back();
    }

    void updateSizes() {
        for (int i = 0; i < vectorValues.size(); i++) {
            int newSize = getValueForIndex(size, i);
            vectorValues[i].resize(newSize, 0);
        }
        updateMinMaxValues();
    }

    void updateMinMaxValues() {
        for (int i = 0; i < vectorValues.size(); i++) {
            float min = getValueForIndex(minVal, i);
            float max = getValueForIndex(maxVal, i);
            for (auto &val : vectorValues[i]) {
                val = ofClamp(val, min, max);
            }
            vectorValueParams[i].setMin(vector<float>(1, min));
            vectorValueParams[i].setMax(vector<float>(1, max));
            vectorValueParams[i] = vectorValues[i];
        }
    }

    void drawMultiSlider(int index) {
        if (index >= vectorValues.size() || index >= vectorValueParams.size()) return;

        auto values_getter = [](void* data, int idx) -> float {
            const float v = *(const float*)(const void*)((const unsigned char*)data + (size_t)idx * sizeof(float));
            return v;
        };

        auto cursorPos = ImGui::GetCursorScreenPos();
        
        ImGui::PushID(index);  // Push ID to avoid conflicts with other sliders
        ImGui::InvisibleButton(("##InvBox" + ofToString(index)).c_str(), ImVec2(250, ImGui::GetFrameHeight() * 2));
        
        auto drawList = ImGui::GetWindowDrawList();
        
        void* data = (void*)vectorValues[index].data();
        float scale_min = getValueForIndex(minVal, index);
        float scale_max = getValueForIndex(maxVal, index);
        int values_count = vectorValues[index].size();
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

                if (v_idx1 < v_idx0) {
                    std::swap(v_idx0, v_idx1);
                    std::swap(nVal0, nVal1);
                }
                
                for (int v_idx = v_idx0; v_idx <= v_idx1; v_idx++) {
                    float pctPos = 0;
                    if (v_idx0 != v_idx1) {
                        pctPos = float(v_idx-v_idx0) / float(v_idx1-v_idx0);
                    }
                    vectorValues[index][v_idx] = ofMap(ofLerp(nVal0, nVal1, pctPos), 0, 1, scale_min, scale_max, true);
                    if (ImGui::GetIO().KeyShift) vectorValues[index][v_idx] = round(vectorValues[index][v_idx]);
                }
                
                idx_hovered = v_idx0;
            }
            
            // Handle right-click popup
            if (ImGui::IsItemClicked(1) || (ImGui::IsPopupOpen(("Value Popup " + ofToString(index)).c_str()) && ImGui::IsMouseClicked(1))) {
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
                ImGui::Text("%s", ("Edit item " + ofToString(currentToEditValues[index]) + " of vector " + ofToString(index)).c_str());
                if (currentToEditValues[index] > 0) {
                    ImGui::SameLine();
                    if (ImGui::Button("<<")) {
                        currentToEditValues[index]--;
                    }
                }
                if (currentToEditValues[index] < getValueForIndex(size, index) - 1) {
                    ImGui::SameLine();
                    if (ImGui::Button(">>")) {
                        currentToEditValues[index]++;
                    }
                }
                ImGui::SliderFloat("##edit", &vectorValues[index][currentToEditValues[index]], scale_min, scale_max, "%.4f");
                if (ImGui::Button("Close"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        
        ImGui::PopID();  // Pop ID
    }
};

#endif /* multiSliderMatrix_h */
