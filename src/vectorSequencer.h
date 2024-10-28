#ifndef vectorSequencer_h
#define vectorSequencer_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include <memory>
#include <algorithm>
#include <cmath>
#include <random>

class vectorSequencer : public ofxOceanodeNodeModel {
public:
    vectorSequencer() : ofxOceanodeNodeModel("Vector Sequencer") {
        description = "A sequencer that takes multiple input vectors and selects values based on phasor and index input. "
                      "It can operate in probabilistic or deterministic mode."
        "Outputs scalar values and vector sequences, with customizable GUI display size.";
    }

    void setup() {
        // Add inspector parameters
        addInspectorParameter(numInputs.set("Num Inputs", 2, 1, 16));
        addInspectorParameter(guiWidth.set("GUI Width", 240, 50, 500));
        addInspectorParameter(guiHeight.set("GUI Height", 60, 50, 500));
        
        // Add common parameters
        addParameter(indexInput.set("idx[]", vector<int>(1, 0), vector<int>(1, 0), vector<int>(1, INT_MAX)));
        addParameter(phasorInput.set("ph[]", vector<float>(1, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 1.0f)));
        addParameter(probabilistic.set("Probabilistic", false));
        addParameter(vectorOutput.set("vec Out", vector<float>(2, 0.0f), vector<float>(2, -FLT_MAX), vector<float>(2, FLT_MAX)));

        // Initialize vectors with default size
        inputVectors.resize(2);
        scalarOutputs.resize(2);
        lastIndices.resize(2, -1);
        lastOutputs.resize(2, 0.0f);
        lastPhasorValues.resize(2, -1.0f);
        lastIndexValues.resize(2, -1);
        
        // Add input parameters first
        for(int i = 0; i < inputVectors.size(); i++) {
            addParameter(inputVectors[i].set("in" + ofToString(i + 1),
                vector<float>(1, 0.0f), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));
        }

        // Add custom region in the middle
        addCustomRegion(customWidget, [this]() {
            drawCustomWidget();
        });

        // Add output parameters last
        for(int i = 0; i < scalarOutputs.size(); i++) {
            addParameter(scalarOutputs[i].set("out" + ofToString(i + 1),
                0.0f, -FLT_MAX, FLT_MAX));
        }

        setupListeners();
    }
    
    void setupListeners() {
        listeners.push(numInputs.newListener([this](int &newSize) {
            if(inputVectors.size() != newSize) {
                int oldSize = inputVectors.size();
                
                // Remove excess parameters
                if(oldSize > newSize) {
                    for(int i = oldSize - 1; i >= newSize; i--) {
                        removeParameter("out" + ofToString(i + 1));
                        removeParameter("in" + ofToString(i + 1));
                    }
                }

                inputVectors.resize(newSize);
                scalarOutputs.resize(newSize);
                lastIndices.resize(newSize, -1);
                lastOutputs.resize(newSize, 0.0f);
                lastPhasorValues.resize(newSize, -1.0f);
                lastIndexValues.resize(newSize, -1);
                
                // Add new parameters in the right order
                if(oldSize < newSize) {
                    for(int i = oldSize; i < newSize; i++) {
                        addParameter(inputVectors[i].set("in" + ofToString(i + 1),
                            vector<float>(1, 0.0f), vector<float>(1, -FLT_MAX), vector<float>(1, FLT_MAX)));
                    }
                    for(int i = oldSize; i < newSize; i++) {
                        addParameter(scalarOutputs[i].set("out" + ofToString(i + 1),
                            0.0f, -FLT_MAX, FLT_MAX));
                    }
                }

                setupTriggerListeners();
            }
        }));

        listeners.push(probabilistic.newListener([this](bool &) {
            lastIndices.clear();
            lastIndices.resize(numInputs, -1);
            updateOutputs();
        }));

        setupTriggerListeners();
    }
    
    void loadBeforeConnections(ofJson &json) {
        deserializeParameter(json, numInputs);
    }
    
    void setupTriggerListeners() {
        triggerListeners.clear();
        
        // Add listener for index input
        triggerListeners.push_back(std::make_unique<ofEventListener>(
            indexInput.newListener([this](vector<int> &newIndices) {
                bool shouldUpdate = false;
                for(size_t i = 0; i < std::min(newIndices.size(), lastIndexValues.size()); i++) {
                    if(newIndices[i] != lastIndexValues[i]) {
                        shouldUpdate = true;
                        lastIndexValues[i] = newIndices[i];
                    }
                }
                if(shouldUpdate) {
                    updateOutputs();
                }
            })
        ));
        
        // Add listener for phasor input
        triggerListeners.push_back(std::make_unique<ofEventListener>(
            phasorInput.newListener([this](vector<float> &newPhasors) {
                bool shouldUpdate = false;
                for(size_t i = 0; i < std::min(newPhasors.size(), lastPhasorValues.size()); i++) {
                    if(newPhasors[i] != lastPhasorValues[i]) {
                        shouldUpdate = true;
                        lastPhasorValues[i] = newPhasors[i];
                    }
                }
                if(shouldUpdate) {
                    updateOutputs();
                }
            })
        ));
    }

private:
    ofEventListeners listeners;
    vector<std::unique_ptr<ofEventListener>> triggerListeners;
    vector<ofParameter<vector<float>>> inputVectors;
    vector<ofParameter<float>> scalarOutputs;
    ofParameter<vector<int>> indexInput;
    ofParameter<vector<float>> phasorInput;
    ofParameter<bool> probabilistic;
    ofParameter<vector<float>> vectorOutput;
    ofParameter<int> numInputs;
    ofParameter<float> guiWidth;
    ofParameter<float> guiHeight;
    customGuiRegion customWidget;
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_real_distribution<> dis{0.0, 1.0};
    vector<int> lastIndices;
    vector<float> lastOutputs;
    vector<float> lastPhasorValues;
    vector<int> lastIndexValues;
    
    void updateOutputs() {
        if(inputVectors.empty()) return;
        
        try {
            vector<float> newVectorOutput(numInputs, 0.0f);

            for (int i = 0; i < numInputs && i < inputVectors.size(); ++i) {
                try {
                    const vector<float>& inputVec = inputVectors[i].get();
                    if (inputVec.empty()) continue;
                    
                    int vectorSize = inputVec.size();
                    float phasor = 0.0f;
                    int indexOffset = 0;
                    
                    if(!phasorInput->empty()) {
                        phasor = (i < phasorInput->size()) ? phasorInput->at(i) : phasorInput->back();
                    }
                    
                    if(!indexInput->empty()) {
                        indexOffset = (i < indexInput->size()) ? indexInput->at(i) : indexInput->back();
                    }
                    
                    int index = static_cast<int>(floor(phasor * vectorSize + indexOffset)) % vectorSize;
                    if (index < 0) index += vectorSize;

                    float value = 0.0f;
                    if (probabilistic) {
                        if (i < lastIndices.size() && index != lastIndices[i]) {
                            float probability = normalizeValue(inputVec[index], inputVec);
                            value = (dis(gen) < probability) ? 1.0f : 0.0f;
                            
                            if(i < lastOutputs.size()) {
                                lastOutputs[i] = value;
                            }
                            if(i < lastIndices.size()) {
                                lastIndices[i] = index;
                            }
                        } else {
                            value = (i < lastOutputs.size()) ? lastOutputs[i] : 0.0f;
                        }
                    } else {
                        value = inputVec[index];
                        if(i < lastIndices.size()) {
                            lastIndices[i] = index;
                        }
                    }

                    if(i < scalarOutputs.size()) {
                        scalarOutputs[i] = value;
                    }
                    newVectorOutput[i] = value;
                }
                catch(const std::exception& e) {
                    ofLogWarning("vectorSequencer") << "Error processing input " << i << ": " << e.what();
                    continue;
                }
            }

            vectorOutput.set(newVectorOutput);
        }
        catch(const std::exception& e) {
            ofLogError("vectorSequencer") << "Error in updateOutputs: " << e.what();
        }
    }

    void drawCustomWidget() {
            ImGuiStyle& style = ImGui::GetStyle();
            float width = guiWidth;
            float height = guiHeight;
            ImVec2 p = ImGui::GetCursorScreenPos();

            float rowHeight = height / numInputs;

            for (int i = 0; i < numInputs; ++i) {
                const vector<float>& inputVec = inputVectors[i].get();
                if (!inputVec.empty()) {
                    float phasor = (i < phasorInput->size()) ? phasorInput->at(i) : phasorInput->back();
                    int indexOffset = (i < indexInput->size()) ? indexInput->at(i) : indexInput->back();
                    int vectorSize = inputVec.size();
                    
                    // Calculate index based on phasor and index offset
                    int index = static_cast<int>(floor(phasor * vectorSize + indexOffset)) % vectorSize;
                    if (index < 0) index += vectorSize;  // Handle negative indices

                    float stepWidth = width / vectorSize;
                    
                    // Get background colors - exact same as multiSliderMatrix
                    ImVec4 base_color = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
                    ImVec4 alt_color = ImVec4(
                        base_color.x * 1.1f,
                        base_color.y * 1.1f,
                        base_color.z * 1.1f,
                        base_color.w
                    );
                    const ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
                    const ImU32 col_bg_alt = ImGui::ColorConvertFloat4ToU32(alt_color);
                    
                    // Draw full row background first
                    ImVec2 row_bg_pos0 = ImVec2(p.x, p.y + rowHeight * i);
                    ImVec2 row_bg_pos1 = ImVec2(p.x + width, p.y + rowHeight * (i + 1));
                    ImGui::GetWindowDrawList()->AddRectFilled(row_bg_pos0, row_bg_pos1, col_bg);

                    // Draw alternating background pattern
                    for (int j = 0; j < vectorSize; j += 2) {
                        ImVec2 bg_pos0 = ImVec2(p.x + j * stepWidth, p.y + rowHeight * i);
                        ImVec2 bg_pos1 = ImVec2(bg_pos0.x + stepWidth, p.y + rowHeight * (i + 1) - 2);
                        ImGui::GetWindowDrawList()->AddRectFilled(bg_pos0, bg_pos1, col_bg_alt);
                    }
                    
                    // Draw highlighted background for current index
                    if(index >= 0 && index < vectorSize) {
                        ImVec2 hl_pos0 = ImVec2(p.x + index * stepWidth, p.y + rowHeight * i);
                        ImVec2 hl_pos1 = ImVec2(hl_pos0.x + stepWidth, p.y + rowHeight * (i + 1) - 2);
                        ImGui::GetWindowDrawList()->AddRectFilled(hl_pos0, hl_pos1, IM_COL32(100, 100, 0, 100));
                    }
                    
                    // Draw value bars
                    for (int j = 0; j < vectorSize; ++j) {
                        ImU32 color = (j == index) ? IM_COL32(255, 255, 0, 255) : IM_COL32(100, 100, 100, 255);
                        
                        float normalizedValue = normalizeValue(inputVec[j], inputVec);
                        float barHeight = normalizedValue * (rowHeight - 2);  // -2 for a small gap between rows
                        
                        if(barHeight > 0) {  // Only draw bar if there's a value
                            ImVec2 pos = ImVec2(p.x + j * stepWidth, p.y + rowHeight * i + rowHeight - barHeight);
                            ImGui::GetWindowDrawList()->AddRectFilled(
                                pos,
                                ImVec2(pos.x + stepWidth - 1, pos.y + barHeight),
                                color
                            );
                        }
                    }

                    // Draw phasor line on top
                    float phasorX = p.x + phasor * width;
                    ImGui::GetWindowDrawList()->AddLine(
                        ImVec2(phasorX, p.y + rowHeight * i),
                        ImVec2(phasorX, p.y + rowHeight * (i + 1) - 2),
                        IM_COL32(255, 0, 0, 255),
                        2.0f
                    );
                }
            }

            ImGui::Dummy(ImVec2(width, height));
        }

    float normalizeValue(float value, const vector<float>& vec) {
        if (probabilistic) {
            return ofClamp(value, 0.0f, 1.0f);
        } else {
            auto [min_it, max_it] = std::minmax_element(vec.begin(), vec.end());
            float min_val = *min_it;
            float max_val = *max_it;
            float value_range = max_val - min_val;
            return (value_range != 0) ? (value - min_val) / value_range : 0.5f;
        }
    }
};

#endif /* vectorSequencer_h */
