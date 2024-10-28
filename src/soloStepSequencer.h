#ifndef soloStepSequencer_h
#define soloStepSequencer_h

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <random>
#include <array>
#include <vector>
#include <numeric>

class soloStepSequencer : public ofxOceanodeNodeModel {
public:
    soloStepSequencer() : ofxOceanodeNodeModel("Solo Step Sequencer"), gen(rd()), dist(0.0f, 1.0f) {
        description = "A sequencer node that outputs a number based on weighted probabilities from multiple tracks. "
                      "Each track has its own multislider for entering values. "
                      "'Hold Mode' allows the output to update only when there are changes in the steps' values.";

        // Inspector parameters
        addInspectorParameter(numTracks.set("Num Tracks", 4, 1, 16));
        addInspectorParameter(guiWidth.set("GUI Width", 240, 50, 500));
        addInspectorParameter(trackHeight.set("Track Height", 25, 10, 200));

        // Parameters
        addParameter(size.set("Size[]", vector<int>(1, 8), vector<int>(1, 2), vector<int>(1, INT_MAX)));
        addParameter(step.set("Step[]", vector<int>(1, 0), vector<int>(1, 0), vector<int>(1, INT_MAX)));
        addParameter(shift.set("Shift[]", vector<int>(1, 0), vector<int>(1, -INT_MAX), vector<int>(1, INT_MAX)));
        addParameter(holdMode.set("Hold Mode", false));
        addParameter(seed.set("Seed", 0, 0, INT_MAX));
        addOutputParameter(solo.set("Solo", 0, 0, 16));

        tracks.resize(16);
        for (auto &track : tracks) {
            track.resize(16, 0.0f);
        }

        updateTracksAndGUI();

        // Listeners
        listeners.push(numTracks.newListener([this](int &){
            updateTracksAndGUI();
        }));

        listeners.push(size.newListener([this](vector<int> &s){
                    if (s != lastSize) {
                        lastSize = s;
                        updateTracksAndGUI();
                    }
                }));

                listeners.push(step.newListener([this](vector<int> &s){
                    if (s != lastStep) {
                        lastStep = s;
                        updateSolo();
                    }
                }));

                listeners.push(shift.newListener([this](vector<int> &s){
                    if (s != lastShift) {
                        lastShift = s;
                        updateSolo();
                    }
                }));

        seedListener = seed.newListener([this](int &) {
            resetGenerator();
        });

        addCustomRegion(customWidget, [this]() {
            drawCustomWidget();
        });
        
        currentToEditTrack = -1;
        currentToEditStep = -1;
    }
    
    void presetSave(ofJson &json) override {
            // Save tracks data
            json["tracks"] = ofJson::array();
            for (const auto& track : tracks) {
                ofJson trackJson = ofJson::array();
                for (const auto& value : track) {
                    trackJson.push_back(value);
                }
                json["tracks"].push_back(trackJson);
            }
        }

        void presetRecallAfterSettingParameters(ofJson &json) override {
            // Load tracks data
            if (json.contains("tracks")) {
                tracks.clear();
                for (const auto& trackJson : json["tracks"]) {
                    std::vector<float> track;
                    for (const auto& value : trackJson) {
                        track.push_back(value);
                    }
                    tracks.push_back(track);
                }
                
                // Ensure tracks size matches numTracks
                tracks.resize(numTracks);
                for (auto& track : tracks) {
                    track.resize(getValueForIndex(size, 0), 0.0f);
                }

                updateSolo();
            }
        }

    void updateTracksAndGUI() {
        int newSize = numTracks;
        
        // Update track data
        tracks.resize(newSize);
        for (int i = 0; i < newSize; ++i) {
            tracks[i].resize(getValueForIndex(size, i), 0.0f);
        }

        lastValues.resize(newSize, 0.0f);

        updateSolo();
    }

    void updateSolo() {
        std::vector<float> currentValues(numTracks, 0.0f);
        bool hasChanged = false;
        std::vector<bool> changedTracks(numTracks, false);

        for (int i = 0; i < numTracks; ++i) {
            int trackSize = getValueForIndex(size, i);
            int trackStep = getValueForIndex(step, i);
            int trackShift = getValueForIndex(shift, i);
            int index = (trackStep + trackShift) % trackSize;
            if (index < 0) index += trackSize;
            currentValues[i] = tracks[i][index];
            if (currentValues[i] != lastValues[i]) {
                hasChanged = true;
                changedTracks[i] = true;
            }
        }

        float sum = std::accumulate(currentValues.begin(), currentValues.end(), 0.0f);
        if (sum == 0) {
            solo = 0; // All inputs are zero
            return;
        }

        std::vector<float> probabilities;
        for (auto value : currentValues) probabilities.push_back(value / sum);

        if (hasChanged || !holdMode) {
            performSelection(probabilities, changedTracks);
        }

        lastValues = currentValues;
    }

    void performSelection(const std::vector<float>& probabilities, const std::vector<bool>& changedTracks) {
        float rand = dist(gen);
        float cumulative = 0.0f;
        for (int i = 0; i < probabilities.size(); ++i) {
            cumulative += probabilities[i];
            if (rand <= cumulative) {
                solo = i + 1;
                break;
            }
        }
    }

    void drawCustomWidget() {
            ImGuiStyle& style = ImGui::GetStyle();
            float width = guiWidth;
            float height = trackHeight;
            ImVec2 p = ImGui::GetCursorScreenPos();

            float trackSpacing = 2.0f; // Spacing between tracks
            float totalHeight = 0;

            for (int i = 0; i < numTracks; ++i) {
                std::string label = "Track " + std::to_string(i + 1);
                ImGui::PushID(i);
                
                ImVec2 rowStart = ImVec2(p.x, p.y + totalHeight);
                ImVec2 rowSize = ImVec2(width, height);
                ImGui::SetCursorScreenPos(rowStart);
                ImGui::InvisibleButton(("##" + label).c_str(), rowSize);

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                
                // Background with checker pattern
                int trackSize = getValueForIndex(size, i);
                float stepWidth = width / trackSize;
                for (int j = 0; j < trackSize; ++j) {
                    ImU32 bgColor = (j % 2 == 0) ? IM_COL32(60, 60, 60, 255) : IM_COL32(50, 50, 50, 255);
                    ImVec2 stepStart = ImVec2(rowStart.x + j * stepWidth, rowStart.y);
                    ImVec2 stepEnd = ImVec2(stepStart.x + stepWidth, rowStart.y + height);
                    draw_list->AddRectFilled(stepStart, stepEnd, bgColor);
                }

                // Draw bars
                for (int j = 0; j < trackSize; ++j) {
                    float barHeight = tracks[i][j] * height;
                    ImVec2 barStart = ImVec2(rowStart.x + j * stepWidth, rowStart.y + height - barHeight);
                    ImVec2 barEnd = ImVec2(barStart.x + stepWidth - 1, rowStart.y + height);
                    
                    ImU32 barColor = IM_COL32(100, 100, 100, 255); // Default color
                    
                    // Highlight current step
                    int currentStep = (getValueForIndex(step, i) + getValueForIndex(shift, i)) % trackSize;
                    if (currentStep < 0) currentStep += trackSize;
                    if (j == currentStep) {
                        barColor = IM_COL32(200, 200, 100, 255); // Highlight color
                    }

                    draw_list->AddRectFilled(barStart, barEnd, barColor);
                }

                // Handle mouse interaction
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    int index = int((mousePos.x - rowStart.x) / stepWidth);
                    float value = 1.0f - (mousePos.y - rowStart.y) / height;
                    if (index >= 0 && index < trackSize) {
                        tracks[i][index] = std::max(0.0f, std::min(1.0f, value));
                        updateSolo();
                    }
                }

                // Right-click for numerical input
                if (ImGui::IsItemClicked(1) || (ImGui::IsPopupOpen(("Value Popup##" + ofToString(i)).c_str()) && ImGui::IsMouseClicked(1))) {
                    ImGui::OpenPopup(("Value Popup##" + ofToString(i)).c_str());
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    int index = int((mousePos.x - rowStart.x) / stepWidth);
                    if (index >= 0 && index < trackSize) {
                        currentToEditTrack = i;
                        currentToEditStep = index;
                    }
                }

                if (ImGui::BeginPopup(("Value Popup##" + ofToString(i)).c_str())) {
                    ImGui::Text("Edit Track %d, Step %d", currentToEditTrack + 1, currentToEditStep + 1);
                    if (currentToEditStep > 0) {
                        ImGui::SameLine();
                        if (ImGui::Button("<<")) {
                            currentToEditStep--;
                        }
                    }
                    if (currentToEditStep < trackSize - 1) {
                        ImGui::SameLine();
                        if (ImGui::Button(">>")) {
                            currentToEditStep++;
                        }
                    }
                    ImGui::SliderFloat("##edit", &tracks[currentToEditTrack][currentToEditStep], 0.0f, 1.0f, "%.4f");
                    if (ImGui::Button("Close")) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", label.c_str());
                    ImGui::EndTooltip();
                }

                ImGui::PopID();

                totalHeight += height + trackSpacing;
            }
        }

private:
    ofParameter<int> numTracks;
    ofParameter<float> guiWidth;
    ofParameter<float> trackHeight;
    ofParameter<vector<int>> size;
    ofParameter<vector<int>> step;
    ofParameter<vector<int>> shift;
    vector<int> lastSize;
    vector<int> lastStep;
    vector<int> lastShift;
    ofParameter<int> solo;
    ofParameter<bool> holdMode;
    ofParameter<int> seed;
    std::vector<std::vector<float>> tracks;
    std::vector<float> lastValues;
    ofEventListeners listeners;
    ofEventListener seedListener;
    customGuiRegion customWidget;
    int currentToEditTrack;
    int currentToEditStep;

    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist;

    void resetGenerator() {
        if (seed != 0) {
            gen.seed(seed);
        } else {
            gen.seed(rd());
        }
    }

    int getValueForIndex(const ofParameter<vector<int>>& param, int index) {
        const auto& vec = param.get();
        if (vec.empty()) return 0;
        if (vec.size() == 1) return vec[0];
        if (index < vec.size()) return vec[index];
        return vec.back();
    }
};

#endif /* soloStepSequencer_h */
