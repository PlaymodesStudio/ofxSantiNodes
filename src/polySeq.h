#ifndef polySeq_h
#define polySeq_h

#include "ofxOceanodeNodeModel.h"
#include "imgui_internal.h"
#include <fstream>


class polySeq : public ofxOceanodeNodeModel {
public:
    static inline const int NUM_SLIDERS = 8;
    static inline const int NUM_SLOTS = 10;
    
    polySeq() : ofxOceanodeNodeModel("Poly Step Sequencer") {}

    void setup() {
        description = "An 8 track traditional or probabilistic sequencer, capable of storing slots of sequence data. It features adjustable sequence lengths, value ranges, and offset for each track. Responds to an index input for playback control and provides both individual track outputs and a combined vector output. The vectorial slot system enables switching between different sequence configurations per individual track";
        addParameter(size.set("Size[]", vector<int>(1, 10), vector<int>(1, 2), vector<int>(1, INT_MAX)));
        addParameter(minVal.set("Min[]", vector<float>(1, 0), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));
        addParameter(maxVal.set("Max[]", vector<float>(1, 1), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));
        addParameter(indexInput.set("Idx[]", vector<int>(1, 0), vector<int>(1, 0), vector<int>(1, INT_MAX)));
        addParameter(offsetInput.set("Offset[]", vector<int>(1, 0), vector<int>(1, -INT_MAX), vector<int>(1, INT_MAX)));
        
        vector<int> initialSlots(NUM_SLIDERS, 0);
        vector<int> minSlots(NUM_SLIDERS, 0);
        vector<int> maxSlots(NUM_SLIDERS, NUM_SLOTS - 1);
        addParameter(slot.set("Slot[]", vector<int>(1, 0), vector<int>(1, 0), vector<int>(1, NUM_SLOTS - 1)));
       
        addParameter(probabilistic.set("Probabilistic", false));
        lastSteps.resize(NUM_SLIDERS, -1);  // Initialize with -1 to ensure first step is always calculated

        addOutputParameter(vecOutput.set("Vec Output",vector<float>(8, 0.0f),vector<float>(8, -FLT_MAX),vector<float>(8, FLT_MAX)));
        
        const int NUM_SLIDERS = 8;
        vectorValues.resize(NUM_SLIDERS);
        for (auto &v : vectorValues) {
            v.resize(10, 0);
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
        
        initializeAllSlotData();

        listeners.push(minVal.newListener([this](vector<float> &f) {
            updateMinMaxValues();
        }));

        listeners.push(maxVal.newListener([this](vector<float> &f) {
            updateMinMaxValues();
        }));
        
        listeners.push(indexInput.newListener([this](vector<int> &i) {
            updateOutputs();
        }));
        
        listeners.push(offsetInput.newListener([this](vector<int> &o) {
                   updateOutputs();
        }));

        currentSlots.resize(NUM_SLIDERS, 0);

        listeners.push(slot.newListener([this](vector<int> &s) {
            switchSlot(s);
            updateSlotParameter();
        }));
        
        listeners.push(size.newListener([this](vector<int> &s) {
                initializeAllSlotData();
                updateSizes();
            }));
        
        listeners.push(probabilistic.newListener([this](bool &b) {
                std::fill(lastSteps.begin(), lastSteps.end(), -1);  // Reset all last steps
                updateMinMaxValues();
                updateOutputs();
            }));
    }
    
    void initializeAllSlotData() {
        vector<vector<vector<float>>> tempAllSlotData = allSlotData;
        
        allSlotData.clear();
        allSlotData.resize(NUM_SLOTS);
        for (int slot = 0; slot < NUM_SLOTS; ++slot) {
            allSlotData[slot].resize(NUM_SLIDERS);
            for (int track = 0; track < NUM_SLIDERS; ++track) {
                int trackSize = getValueForIndex(size, track);
                allSlotData[slot][track].resize(trackSize, 0.0f);
                
                // Copy existing data if available
                if (slot < tempAllSlotData.size() && track < tempAllSlotData[slot].size()) {
                    int oldSize = tempAllSlotData[slot][track].size();
                    for (int i = 0; i < std::min(oldSize, trackSize); i++) {
                        allSlotData[slot][track][i] = tempAllSlotData[slot][track][i];
                    }
                }
            }
        }
    }

    void update(ofEventArgs &a) {
        updateOutputs();

    }

    void presetSave(ofJson &json) {
        //logBuffer += "Saving preset...\n";
        //logBuffer += "Current slots: " + ofToString(currentSlots) + "\n";
        
        saveCurrentSlotData();
        
        for (int slot = 0; slot < NUM_SLOTS; ++slot) {
            json["SlotData_" + ofToString(slot)] = allSlotData[slot];
            //logBuffer += "Saving Slot " + ofToString(slot) + " data\n";
        }
        json["CurrentSlots"] = currentSlots;
        json["SliderSizes"] = size.get();
        
        //logSlotData("After saving preset");
    }
    
    

    void presetRecallAfterSettingParameters(ofJson &json) override {
        // Load all data first
        if (json.count("CurrentSlots") == 1) {
            currentSlots = json["CurrentSlots"].get<vector<int>>();
        } else {
            currentSlots = vector<int>(NUM_SLIDERS, 0);
        }

        for (int slot = 0; slot < NUM_SLOTS; ++slot) {
            string slotKey = "SlotData_" + ofToString(slot);
            if (json.count(slotKey) == 1) {
                allSlotData[slot] = json[slotKey].get<vector<vector<float>>>();
            }
        }

        // Now set the size and update
        if (json.count("SliderSizes") == 1) {
            size.set(json["SliderSizes"].get<vector<int>>());
        }
        updateSizes();

        // Finally, switch to the correct slot
        switchSlot(currentSlots);
        updateSlotParameter();
        updateMinMaxValues();
        updateOutputs();
    }
    
    void presetHasLoaded() override {
        for (int i = 0; i < NUM_SLIDERS; i++) {
            if (currentSlots[i] < allSlotData.size() && i < allSlotData[currentSlots[i]].size()) {
                vectorValues[i] = allSlotData[currentSlots[i]][i];
                vectorValueParams[i] = vectorValues[i];
            }
        }
        updateOutputs();
    }
    
    void logSlotData(const string& context) {
        appendLog("Logging slot data for context: " + context);
        for (int slot = 0; slot < NUM_SLOTS; ++slot) {
            for (int track = 0; track < NUM_SLIDERS; ++track) {
                appendLog("Slot " + ofToString(slot) + ", Track " + ofToString(track) + ": " + ofToString(allSlotData[slot][track]));
            }
        }
    }
    
    void logCurrentState() {
        appendLog("Current State:");
        appendLog("Current Slots: " + ofToString(currentSlots));
        for (int i = 0; i < NUM_SLIDERS; ++i) {
            appendLog("Track " + ofToString(i) + ": " + ofToString(vectorValues[i]));
        }
    }
    
    void appendLog(const string& message) {
        logBuffer += message + "\n";
        ofLogNotice("polySeq") << message;  // This will also output to the console
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
    ofParameter<vector<int>> indexInput;
    vector<float> currentOutputs;
    ofParameter<vector<int>> offsetInput;
    ofParameter<vector<float>> vecOutput;
    ofParameter<vector<int>> slot;
    vector<vector<vector<float>>> allSlotData;
    int currentSlot = 0;
    vector<int> currentSlots;
    string logBuffer;
    ofParameter<bool> probabilistic;
    vector<int> lastSteps;



    void saveCurrentSlotData() {
        //appendLog("Saving current slot data:");
        for (int i = 0; i < NUM_SLIDERS; ++i) {
            if (currentSlots[i] < allSlotData.size()) {
                allSlotData[currentSlots[i]][i] = vectorValues[i];
                //appendLog("Slot " + ofToString(currentSlots[i]) + ", Track " + ofToString(i) + ": " + ofToString(vectorValues[i]));
            }
        }
    }
    
    bool isSlotVectorial() {
        return slot.get().size() > 1;
    }

    void updateSlotParameter() {
        if (!isSlotVectorial()) {
            int currentSlot = slot.get().size() > 0 ? slot.get()[0] : 0;
            slot.set(vector<int>(1, currentSlot));
        }
    }
    
    void switchSlot(const vector<int>& newSlots) {
        saveCurrentSlotData();

        for (int i = 0; i < NUM_SLIDERS; ++i) {
            int newSlot = isSlotVectorial() ?
                (i < newSlots.size() ? newSlots[i] : newSlots.back()) :
                newSlots[0];
            currentSlots[i] = std::max(0, std::min(newSlot, NUM_SLOTS - 1));
        }
        
        for (int i = 0; i < NUM_SLIDERS; ++i) {
            if (currentSlots[i] < allSlotData.size() && i < allSlotData[currentSlots[i]].size()) {
                vectorValues[i] = allSlotData[currentSlots[i]][i];
                vectorValueParams[i] = vectorValues[i];
            }
        }
        
        updateMinMaxValues();
        updateSlotParameter();
        std::fill(lastSteps.begin(), lastSteps.end(), -1);  // Reset all last steps
        updateOutputs();
    }
    
    void updateOutputs() {
        int numSliders = NUM_SLIDERS;
        currentOutputs.resize(numSliders);
        vector<float> newVecOutput(numSliders, 0.0f);

        for (int i = 0; i < numSliders; i++) {
            int currentSize = vectorValues[i].size();
            if (currentSize > 0) {
                int index = (i < indexInput->size()) ? indexInput->at(i) : indexInput->back();
                int offset = (i < offsetInput->size()) ? offsetInput->at(i) : offsetInput->back();
                int step = (index + offset) % currentSize;
                if (step < 0) step += currentSize; // Handle negative values
                
                if (probabilistic) {
                    if (step != lastSteps[i]) {  // Only recalculate if the step has changed
                        float probability = vectorValues[i][step];
                        float randomValue = ofRandom(1.0f); // Generate a random float between 0 and 1
                        currentOutputs[i] = (randomValue < probability) ? 1.0f : 0.0f;
                        lastSteps[i] = step;  // Update the last step
                    }
                    // If the step hasn't changed, currentOutputs[i] retains its previous value
                } else {
                    currentOutputs[i] = vectorValues[i][step];
                    lastSteps[i] = step;  // Update the last step even in non-probabilistic mode
                }
                
                vectorValueParams[i] = vector<float>(1, currentOutputs[i]);
                newVecOutput[i] = currentOutputs[i];
            }
        }

        vecOutput.set(newVecOutput);
    }

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
        int numSliders = NUM_SLIDERS;
        vector<vector<vector<float>>> tempAllSlotData = allSlotData;

        for (int slot = 0; slot < NUM_SLOTS; ++slot) {
            if (slot >= allSlotData.size()) {
                allSlotData.resize(slot + 1);
            }
            allSlotData[slot].resize(numSliders);

            for (int i = 0; i < numSliders; i++) {
                int newSize = getValueForIndex(size, i);
                vector<float> newValues(newSize, 0.0f);

                if (slot < tempAllSlotData.size() && i < tempAllSlotData[slot].size()) {
                    int oldSize = tempAllSlotData[slot][i].size();
                    for (int j = 0; j < std::min(oldSize, newSize); j++) {
                        newValues[j] = tempAllSlotData[slot][i][j];
                    }
                }

                allSlotData[slot][i] = newValues;
            }
        }

        for (int i = 0; i < numSliders; i++) {
            vectorValues[i] = allSlotData[currentSlots[i]][i];
            vectorValueParams[i] = vectorValues[i];
        }

        updateMinMaxValues();
        std::fill(lastSteps.begin(), lastSteps.end(), -1);  // Reset all last steps
        updateOutputs();
    }

    void updateMinMaxValues() {
        int numSliders = NUM_SLIDERS;
        for (int i = 0; i < numSliders; i++) {
            float min = probabilistic ? 0.0f : getValueForIndex(minVal, i);
            float max = probabilistic ? 1.0f : getValueForIndex(maxVal, i);
            for (auto &val : vectorValues[i]) {
                val = ofClamp(val, min, max);
            }
            if (i < vectorValueParams.size()) {
                vectorValueParams[i].setMin(vector<float>(1, min));
                vectorValueParams[i].setMax(vector<float>(1, max));
                vectorValueParams[i] = vectorValues[i];
            }
        }

        // Update min and max for vecOutput
        vector<float> minVec(numSliders, probabilistic ? 0.0f : -FLT_MAX);
        vector<float> maxVec(numSliders, probabilistic ? 1.0f : FLT_MAX);
        for (int i = 0; i < numSliders; i++) {
            if (!probabilistic) {
                if (!minVal.get().empty()) minVec[i] = getValueForIndex(minVal, i);
                if (!maxVal.get().empty()) maxVec[i] = getValueForIndex(maxVal, i);
            }
        }
        vecOutput.setMin(minVec);
        vecOutput.setMax(maxVec);
    }

    void drawMultiSlider(int index) {
        if (index >= NUM_SLIDERS || index >= vectorValueParams.size()) return;

        auto values_getter = [](void* data, int idx) -> float {
            const float v = *(const float*)(const void*)((const unsigned char*)data + (size_t)idx * sizeof(float));
            return v;
        };

        auto cursorPos = ImGui::GetCursorScreenPos();
        
        ImGui::PushID(index);
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
                saveCurrentSlotData();
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
                            float newValue = ofMap(ofLerp(nVal0, nVal1, pctPos), 0, 1, scale_min, scale_max, true);
                            if (ImGui::GetIO().KeyShift) newValue = round(newValue);
                            
                    vectorValues[index][v_idx] = newValue;
                    allSlotData[currentSlots[index]][index][v_idx] = newValue;


                            }

                        
                        vectorValueParams[index] = vectorValues[index];
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
                    
                    // Correctly derive the alternate background color
                    ImVec4 frame_bg_color = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
                    ImVec4 alt_bg_color = ImVec4(
                        frame_bg_color.x * 1.1f,
                        frame_bg_color.y * 1.1f,
                        frame_bg_color.z * 1.1f,
                        frame_bg_color.w
                    );
                    const ImU32 col_bg_alt = ImGui::ColorConvertFloat4ToU32(alt_bg_color);
                    
                    // New color for highlighting the current step
                    const ImU32 col_highlight = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        1.0f, 1.0f, 1.0f, 0.5f  // White with 50% opacity
                    ));

                    // Calculate current step for this multislider
                    int rawIndex = indexInput->at(index % indexInput->size());
                    int offset = offsetInput->at(index % offsetInput->size());
                    int currentStep = (rawIndex + offset) % values_count;
                    if (currentStep < 0) currentStep += values_count;


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

                        // Determine the color for this bar
                        ImU32 barColor;
                                if (v1_idx == currentStep) {
                                    barColor = col_highlight;  // Highlight current step
                                } else if (idx_hovered == v1_idx) {
                                    barColor = col_hovered;
                                } else {
                                    barColor = col_base;
                                }

                                drawList->AddRectFilled(pos0, pos1, barColor);

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

#endif /* polySeq_h */
