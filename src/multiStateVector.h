//
//  multistateVector.h
//  Oceanode
//
//  Created by Santi Vilanova on 23/7/2024.
//

#ifndef multistateVector_h
#define multistateVector_h

#include "ofxOceanodeNodeModel.h"
#include "imgui_internal.h"

class multistateVector : public ofxOceanodeNodeModel {
public:
    multistateVector() : ofxOceanodeNodeModel("Multistate Vector") {
        description = "A vector-based node that allows the user to manipulate multiple pages of vector values. "
                      "Each page can store a vector of values with customizable size, minimum, and maximum range. "
        "The current page can be selected, and the vector values can be edited in a custom graphical interface.";

    }
    
    void setup() {
        addParameter(size.set("Size", 10, 2, INT_MAX));
        addParameter(minVal.set("Min", 0, -FLT_MAX, FLT_MAX));
        addParameter(maxVal.set("Max", 1, -FLT_MAX, FLT_MAX));
        addParameter(numPages.set("Num Pages", 2, 1, 10));
        addParameter(currentPage.set("Current Page", 0, 0, numPages-1));
        
        addCustomRegion(customWidget, [this]() {
            drawCustomWidget();
        });
        
        vectorValues.resize(numPages, vector<float>(size, 0.0f));
        
        addParameter(vectorValueParam.set("Out", vector<float>(size, 0.0f), vector<float>(1, minVal), vector<float>(1, maxVal)),
                     ofxOceanodeParameterFlags_DisableInConnection | ofxOceanodeParameterFlags_DisplayMinimized);
        
        setupListeners();
    }
    
    void update(ofEventArgs &a) {
        vectorValueParam = vectorValues[currentPage];
    }
    
    void presetSave(ofJson &json) {
        for (int i = 0; i < numPages; i++) {
            json["Values"][ofToString(i)] = vectorValues[i];
        }
    }

    void presetRecallAfterSettingParameters(ofJson &json) {
        if (json.count("Values") == 1) {
            for (int i = 0; i < numPages; i++) {
                vectorValues[i] = json["Values"][ofToString(i)].get<vector<float>>();
            }
        }
    }

    void presetHasLoaded() override {
        vectorValueParam = vectorValues[currentPage];
    }
    
private:
    void setupListeners() {
        listeners.push(size.newListener([this](int &s) {
            for (auto &v : vectorValues) {
                v.resize(s, 0.0f);  // Initialize new elements to 0.5f
            }
            vectorValueParam = vectorValues[currentPage];
        }));
        
        listeners.push(minVal.newListener([this](float &f) {
            for (auto &page : vectorValues) {
                for (auto &v : page) v = ofClamp(v, minVal, maxVal);
            }
            vectorValueParam.setMin(vector<float>(1, f));
            vectorValueParam = vectorValues[currentPage];
        }));
        
        listeners.push(maxVal.newListener([this](float &f) {
            for (auto &page : vectorValues) {
                for (auto &v : page) v = ofClamp(v, minVal, maxVal);
            }
            vectorValueParam.setMax(vector<float>(1, f));
            vectorValueParam = vectorValues[currentPage];
        }));
        
        listeners.push(currentPage.newListener([this](int &p) {
            vectorValueParam = vectorValues[p];
        }));
        
        listeners.push(numPages.newListener([this](int &n) {
            // Resize vectorValues to the new number of pages
            if (n > vectorValues.size()) {
                // If increasing, add new pages
                while (vectorValues.size() < n) {
                    vectorValues.push_back(vector<float>(size, 0.0f));
                }
            } else if (n < vectorValues.size()) {
                // If decreasing, remove excess pages
                vectorValues.resize(n);
            }
            currentPage.setMax(n - 1);
            // Ensure current page is within valid range
            if (currentPage > n - 1) {
                currentPage = n - 1;
            }
        }));
    }
    
    void drawCustomWidget() {
        
        
        auto cursorPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##InvBox", ImVec2(210, ImGui::GetFrameHeight() * 2));
        auto drawList = ImGui::GetWindowDrawList();
        
        void* data = (void*)vectorValues[currentPage].data();
        int values_offset = 0;
        float scale_min = minVal;
        float scale_max = maxVal;
        int values_count = vectorValues[currentPage].size();
        ImVec2 frame_size = ImVec2(210, ImGui::GetFrameHeight() * 2);
        
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImRect frame_bb(cursorPos, cursorPos + frame_size);
        const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
        
        ImGui::RenderFrame(inner_bb.Min, inner_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

        bool hovered = ImGui::IsItemHovered();
        bool press = ImGui::IsMouseDown(0);
        
        int idx_hovered = -1;
        if (values_count >= 1)
        {
            int res_w = ImMin((int)frame_size.x, values_count);
            int item_count = values_count;

            auto mousePos = ImGui::GetIO().MousePos;
            auto mousePosPrev = mousePos - ImGui::GetIO().MouseDelta;
        
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0))
            {
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
                    vectorValues[currentPage][v_idx] = ofMap(ofLerp(nVal0, nVal1, pctPos), 0, 1, scale_min, scale_max, true);
                    if(ImGui::GetIO().KeyShift) vectorValues[currentPage][v_idx] = round(vectorValues[currentPage][v_idx]);
                }
                
                idx_hovered = v_idx0;
            }
            
            if(ImGui::IsItemClicked(1) || (ImGui::IsPopupOpen("Value Popup") && ImGui::IsMouseClicked(1))){
                ImGui::OpenPopup("Value Popup");
                const float t = ImClamp((mousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
                const int v_idx = (int)(t * item_count);
                IM_ASSERT(v_idx >= 0 && v_idx < values_count);
                currentToEditValue = v_idx;
            }

            const float t_step = 1.0f / (float)res_w;
            const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));

            float v0 = vectorValues[currentPage][(0 + values_offset) % values_count];
            float t0 = 0.0f;
            ImVec2 tp0 = ImVec2( t0, 1.0f - ImSaturate((v0 - scale_min) * inv_scale) );
            float histogram_zero_line_t = (scale_min * scale_max < 0.0f) ? (-scale_min * inv_scale) : (scale_min < 0.0f ? 0.0f : 1.0f);

            const ImU32 col_base = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
            const ImU32 col_hovered = ImGui::GetColorU32(ImGuiCol_PlotHistogramHovered);

            for (int n = 0; n < res_w; n++)
            {
                const float t1 = t0 + t_step;
                const int v1_idx = (int)(t0 * item_count + 0.5f);
                IM_ASSERT(v1_idx >= 0 && v1_idx < values_count);
                const float v1 = vectorValues[currentPage][(v1_idx + values_offset + 1) % values_count];
                const ImVec2 tp1 = ImVec2( t1, 1.0f - ImSaturate((v1 - scale_min) * inv_scale) );

                ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
                ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, ImVec2(tp1.x, histogram_zero_line_t));
                
                if (pos1.x >= pos0.x + 2.0f)
                    pos1.x -= 1.0f;
                drawList->AddRectFilled(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);

                t0 = t1;
                tp0 = tp1;
            }

            if (ImGui::BeginPopupContextItem("Value Popup"))
            {
                ImGui::Text("Edit item %d on page %d", currentToEditValue, currentPage.get());
                float currentValue = vectorValues[currentPage][currentToEditValue];
                if (ImGui::SliderFloat("##edit", &currentValue, minVal, maxVal, "%.4f")) {
                    vectorValues[currentPage][currentToEditValue] = currentValue;
                }
                if (ImGui::Button("Close"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
    }

    ofEventListeners listeners;
    
    ofParameter<int> size;
    ofParameter<float> minVal;
    ofParameter<float> maxVal;
    ofParameter<int> numPages;
    ofParameter<int> currentPage;
    vector<vector<float>> vectorValues;
    ofParameter<vector<float>> vectorValueParam;
    customGuiRegion customWidget;
    
    int lastEditValue;
    int currentToEditValue;
};

#endif /* multistateVector_h */
