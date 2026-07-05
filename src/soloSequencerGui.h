#ifndef soloSequencerGui_h
#define soloSequencerGui_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <numeric>
#include <random>
#include <string>
#include <vector>

class soloSequencerGui : public ofxOceanodeNodeModel {
public:
    soloSequencerGui() : ofxOceanodeNodeModel("Solo Sequencer GUI"), dist(0.0f, 1.0f) {
        description =
            "Weighted solo sequencer with editable probability tracks.\n"
            "Keeps the soloSequencer behavior, but tracks are created inside a custom GUI instead of fixed vector inputs.";
        rng.seed(std::random_device{}());
    }

    ~soloSequencerGui() override {
        listeners.unsubscribeAll();
    }

    void setup() override {
        addParameter(step.set("Step", 0, 0, INT_MAX));
        addParameter(shift.set("Shift", 0, -INT_MAX, INT_MAX));
        addParameter(holdMode.set("Hold Mode", false));
        addParameter(seed.set("Seed", 0, 0, INT_MAX));
        addParameter(showWindow.set("Show", false));

        addInspectorParameter(defaultTrackSteps.set("Default Steps", 8, 1, 128));
        addInspectorParameter(trackHeight.set("Track Height", 26.0f, 12.0f, 160.0f));
        addInspectorParameter(editorWidth.set("Editor Width", 920.0f, 420.0f, 2200.0f));
        addInspectorParameter(editorHeight.set("Editor Height", 620.0f, 240.0f, 1800.0f));

        addOutputParameter(solo.set("Solo", 0, 0, INT_MAX));
        addCustomRegion(customWidget.set("Solo Sequencer", [this]() {
            drawDockableGui();
        }), [this]() {
            drawDockableGui();
        });

        tracks.assign(4, std::vector<float>(defaultTrackSteps.get(), 0.0f));
        syncTrackState();
        resetGenerator();
        updateSolo();

        listeners.push(step.newListener([this](int &value) {
            if(value == 0) resetGenerator();
            updateSolo();
        }));
        listeners.push(shift.newListener([this](int &){ updateSolo(); }));
        listeners.push(holdMode.newListener([this](bool &){ updateSolo(); }));
        listeners.push(seed.newListener([this](int &){ resetGenerator(); }));
        listeners.push(defaultTrackSteps.newListener([this](int &value){
            defaultTrackSteps = std::max(1, value);
        }));
    }

    void draw(ofEventArgs &) override {
        if(!showWindow.get()) return;

        std::string title =
            (canvasID == "Canvas" ? "" : canvasID + "/") +
            "Solo Sequencer GUI " + ofToString(getNumIdentifier());

        bool open = showWindow.get();
        if(ImGui::Begin(title.c_str(), &open)) {
            drawEditorContents(std::max(200.0f, editorWidth.get()), true, true, true);
        }
        ImGui::End();
        if(!open) showWindow = false;
    }

    void presetSave(ofJson &json) override {
        json["tracks"] = ofJson::array();
        for(const auto &track : tracks) {
            ofJson trackJson = ofJson::array();
            for(float value : track) trackJson.push_back(value);
            json["tracks"].push_back(trackJson);
        }
        json["trackNames"] = ofJson::array();
        for(const auto &name : trackNames) json["trackNames"].push_back(name);
        json["trackSoloStates"] = trackSoloStates;
    }

    void presetRecallAfterSettingParameters(ofJson &json) override {
        if(!json.contains("tracks") || !json["tracks"].is_array()) return;

        tracks.clear();
        for(const auto &trackJson : json["tracks"]) {
            if(!trackJson.is_array()) continue;
            std::vector<float> track;
            for(const auto &value : trackJson) {
                track.push_back(ofClamp((float)value, 0.0f, 1.0f));
            }
            if(track.empty()) track.assign(defaultTrackSteps.get(), 0.0f);
            tracks.push_back(track);
        }

        if(tracks.empty()) tracks.assign(1, std::vector<float>(defaultTrackSteps.get(), 0.0f));
        trackNames.clear();
        if(json.contains("trackNames") && json["trackNames"].is_array()) {
            for(const auto &nameJson : json["trackNames"]) {
                trackNames.push_back(nameJson.is_string() ? (std::string)nameJson : "");
            }
        }
        trackSoloStates.clear();
        if(json.contains("trackSoloStates") && json["trackSoloStates"].is_array()) {
            trackSoloStates = json["trackSoloStates"].get<std::vector<bool>>();
        }
        syncTrackState();
        updateSolo();
    }

private:
    ofParameter<int> step;
    ofParameter<int> shift;
    ofParameter<bool> holdMode;
    ofParameter<int> seed;
    ofParameter<bool> showWindow;

    ofParameter<int> defaultTrackSteps;
    ofParameter<float> trackHeight;
    ofParameter<float> editorWidth;
    ofParameter<float> editorHeight;

    ofParameter<int> solo;

    std::vector<std::vector<float>> tracks;
    std::vector<std::string> trackNames;
    std::vector<bool> trackSoloStates;
    std::vector<float> lastValues;
    bool anyTrackSoloed = false;
    int lastActiveTrack = 0;
    int popupTrackIndex = -1;
    int popupStepIndex = -1;
    ofEventListeners listeners;
    customGuiRegion customWidget;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist;

    void syncTrackState() {
        for(auto &track : tracks) {
            if(track.empty()) track.assign(defaultTrackSteps.get(), 0.0f);
        }
        trackNames.resize(tracks.size());
        for(size_t i = 0; i < trackNames.size(); i++) {
            if(trackNames[i].empty()) trackNames[i] = "Track " + ofToString(i + 1);
        }
        trackSoloStates.resize(tracks.size(), false);
        refreshTrackSoloState();
        lastValues.resize(tracks.size(), 0.0f);
        if(popupTrackIndex >= (int)tracks.size()) {
            popupTrackIndex = -1;
            popupStepIndex = -1;
        }
    }

    void resetGenerator() {
        if(seed.get() != 0) rng.seed(seed.get());
        else rng.seed(std::random_device{}());
    }

    int wrapIndex(int value, int size) const {
        if(size <= 0) return 0;
        int index = value % size;
        if(index < 0) index += size;
        return index;
    }

    void updateSolo() {
        if(tracks.empty()) {
            solo = 0;
            lastActiveTrack = 0;
            return;
        }

        refreshTrackSoloState();
        std::vector<float> currentValues(tracks.size(), 0.0f);
        bool hasChanged = false;

        for(size_t i = 0; i < tracks.size(); i++) {
            if(tracks[i].empty()) continue;
            int index = wrapIndex(step.get() + shift.get(), (int)tracks[i].size());
            const bool trackEnabled = !anyTrackSoloed || (i < trackSoloStates.size() && trackSoloStates[i]);
            currentValues[i] = trackEnabled ? tracks[i][index] : 0.0f;
            if(i >= lastValues.size() || currentValues[i] != lastValues[i]) hasChanged = true;
        }

        float sum = std::accumulate(currentValues.begin(), currentValues.end(), 0.0f);
        if(sum <= 0.0f) {
            if(anyTrackSoloed) {
                int fallbackTrack = lastActiveTrack;
                if(fallbackTrack <= 0 ||
                   fallbackTrack > (int)trackSoloStates.size() ||
                   !trackSoloStates[fallbackTrack - 1]) {
                    fallbackTrack = 0;
                    for(size_t i = 0; i < trackSoloStates.size(); i++) {
                        if(trackSoloStates[i]) {
                            fallbackTrack = (int)i + 1;
                            break;
                        }
                    }
                }
                if(fallbackTrack > 0) {
                    solo = fallbackTrack;
                    lastValues = currentValues;
                    return;
                }
            }
            solo = 0;
            lastActiveTrack = 0;
            lastValues = currentValues;
            return;
        }

        if(hasChanged || !holdMode.get()) {
            float target = dist(rng);
            float cumulative = 0.0f;
            int selectedTrack = 0;
            for(size_t i = 0; i < currentValues.size(); i++) {
                cumulative += currentValues[i] / sum;
                if(target <= cumulative) {
                    selectedTrack = (int)i + 1;
                    break;
                }
            }
            solo = selectedTrack;
            if(selectedTrack > 0) lastActiveTrack = selectedTrack;
        }

        lastValues = currentValues;
    }

    void refreshTrackSoloState() {
        anyTrackSoloed = std::any_of(trackSoloStates.begin(), trackSoloStates.end(), [](bool isSoloed) {
            return isSoloed;
        });
    }

    void toggleTrackSolo(int index) {
        if(index < 0 || index >= (int)trackSoloStates.size()) return;
        trackSoloStates[index] = !trackSoloStates[index];
        if(trackSoloStates[index]) lastActiveTrack = index + 1;
        refreshTrackSoloState();
        updateSolo();
    }

    void addTrack() {
        tracks.emplace_back(std::max(1, defaultTrackSteps.get()), 0.0f);
        trackNames.push_back("");
        trackSoloStates.push_back(false);
        syncTrackState();
        updateSolo();
    }

    void removeTrack(int index) {
        if(index < 0 || index >= (int)tracks.size()) return;
        tracks.erase(tracks.begin() + index);
        if(index < (int)trackNames.size()) trackNames.erase(trackNames.begin() + index);
        if(index < (int)trackSoloStates.size()) trackSoloStates.erase(trackSoloStates.begin() + index);
        if(tracks.empty()) tracks.assign(1, std::vector<float>(std::max(1, defaultTrackSteps.get()), 0.0f));
        syncTrackState();
        updateSolo();
    }

    void clearTrack(int index) {
        if(index < 0 || index >= (int)tracks.size()) return;
        std::fill(tracks[index].begin(), tracks[index].end(), 0.0f);
        updateSolo();
    }

    void resizeTrack(int index, int newSize) {
        if(index < 0 || index >= (int)tracks.size()) return;
        newSize = ofClamp(newSize, 1, 256);
        tracks[index].resize(newSize, 0.0f);
        if(popupStepIndex >= newSize) popupStepIndex = newSize - 1;
        syncTrackState();
        updateSolo();
    }

    void drawShiftControl(float width) {
        const float sliderWidth = std::max(120.0f, std::min(width * 0.45f, 240.0f));
        int shiftValue = shift.get();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Shift");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(sliderWidth);
        if(ImGui::DragInt("##soloSequencerShift", &shiftValue, 0.2f)) shift = shiftValue;
        ImGui::SameLine();
        if(ImGui::SmallButton("Reset##soloSequencerShift")) shift = 0;
    }

    void drawToolbar() {
        if(ImGui::Button("+ Track")) addTrack();
        ImGui::SameLine();
        if(ImGui::Button("- Last")) removeTrack((int)tracks.size() - 1);
        ImGui::SameLine();
        if(ImGui::Button("Clear All")) {
            for(size_t i = 0; i < tracks.size(); i++) clearTrack((int)i);
        }
        ImGui::SameLine();
        ImGui::Text("Tracks: %d", (int)tracks.size());
        ImGui::SameLine();
        ImGui::Text("Playhead: %d", std::max(0, step.get() + shift.get()));
        if(anyTrackSoloed) {
            ImGui::SameLine();
            ImGui::TextDisabled("Solo Filter");
        }
    }

    void drawTrackHeader(int trackIndex, float width) {
        const int trackSize = tracks[trackIndex].size();
        char nameBuffer[128];
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", trackNames[trackIndex].c_str());
        ImGui::SetNextItemWidth(std::max(120.0f, std::min(width * 0.3f, 220.0f)));
        if(ImGui::InputText(("##trackName" + ofToString(trackIndex)).c_str(), nameBuffer, sizeof(nameBuffer))) {
            trackNames[trackIndex] = nameBuffer;
            if(trackNames[trackIndex].empty()) trackNames[trackIndex] = "Track " + ofToString(trackIndex + 1);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("[%d]", trackSize);
        ImGui::SameLine();
        const bool isSoloed = trackIndex < (int)trackSoloStates.size() && trackSoloStates[trackIndex];
        if(isSoloed) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 125, 215, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 145, 235, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(45, 105, 195, 255));
        }
        if(ImGui::SmallButton(("Solo##" + ofToString(trackIndex)).c_str())) toggleTrackSolo(trackIndex);
        if(isSoloed) ImGui::PopStyleColor(3);
        ImGui::SameLine();
        if(ImGui::SmallButton(("S+##" + ofToString(trackIndex)).c_str())) resizeTrack(trackIndex, trackSize + 1);
        ImGui::SameLine();
        if(ImGui::SmallButton(("S-##" + ofToString(trackIndex)).c_str())) resizeTrack(trackIndex, trackSize - 1);
        ImGui::SameLine();
        if(ImGui::SmallButton(("Clr##" + ofToString(trackIndex)).c_str())) clearTrack(trackIndex);
        ImGui::SameLine();
        if(ImGui::SmallButton(("X##" + ofToString(trackIndex)).c_str())) removeTrack(trackIndex);
    }

    void drawTrackRow(int trackIndex, float width, float height, bool editable) {
        auto &track = tracks[trackIndex];
        if(track.empty()) return;

        const int trackSize = (int)track.size();
        const float stepWidth = width / std::max(1, trackSize);
        ImVec2 rowStart = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(("##trackRow" + ofToString(trackIndex)).c_str(), ImVec2(width, height));
        ImDrawList *drawList = ImGui::GetWindowDrawList();

        const int currentStep = wrapIndex(step.get() + shift.get(), trackSize);
        const bool isSelectedTrack = (solo.get() == trackIndex + 1);
        const bool isSoloed = trackIndex < (int)trackSoloStates.size() && trackSoloStates[trackIndex];

        for(int j = 0; j < trackSize; j++) {
            ImVec2 stepStart(rowStart.x + j * stepWidth, rowStart.y);
            ImVec2 stepEnd(stepStart.x + stepWidth, rowStart.y + height);
            ImU32 bgColor = (j % 2 == 0) ? IM_COL32(52, 52, 52, 255) : IM_COL32(44, 44, 44, 255);
            drawList->AddRectFilled(stepStart, stepEnd, bgColor);

            float barHeight = ofClamp(track[j], 0.0f, 1.0f) * height;
            ImVec2 barStart(stepStart.x + 1.0f, rowStart.y + height - barHeight);
            ImVec2 barEnd(stepEnd.x - 1.0f, rowStart.y + height);
            ImU32 barColor = IM_COL32(120, 120, 120, 255);
            if(isSoloed) barColor = IM_COL32(95, 145, 225, 255);
            if(isSelectedTrack) barColor = IM_COL32(110, 210, 140, 255);
            if(j == currentStep) barColor = IM_COL32(220, 200, 90, 255);
            drawList->AddRectFilled(barStart, barEnd, barColor);
        }

        const float playheadX = rowStart.x + currentStep * stepWidth;
        drawList->AddLine(ImVec2(playheadX, rowStart.y), ImVec2(playheadX, rowStart.y + height), IM_COL32(255, 210, 90, 255), 2.0f);

        const ImU32 borderColor = isSoloed ? IM_COL32(95, 145, 225, 255) : IM_COL32(96, 96, 96, 255);
        drawList->AddRect(rowStart, ImVec2(rowStart.x + width, rowStart.y + height), borderColor);

        if(editable && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            int index = ofClamp((int)((mousePos.x - rowStart.x) / stepWidth), 0, trackSize - 1);
            float value = 1.0f - (mousePos.y - rowStart.y) / std::max(height, 1.0f);
            track[index] = ofClamp(value, 0.0f, 1.0f);
            updateSolo();
        }

        if(editable && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            popupTrackIndex = trackIndex;
            popupStepIndex = ofClamp((int)((mousePos.x - rowStart.x) / stepWidth), 0, trackSize - 1);
            ImGui::OpenPopup(("Track Step Popup##" + ofToString(trackIndex)).c_str());
        }

        if(editable && ImGui::BeginPopup(("Track Step Popup##" + ofToString(trackIndex)).c_str())) {
            popupTrackIndex = ofClamp(popupTrackIndex, 0, (int)tracks.size() - 1);
            popupStepIndex = ofClamp(popupStepIndex, 0, (int)tracks[popupTrackIndex].size() - 1);
            ImGui::Text("Track %d, Step %d", popupTrackIndex + 1, popupStepIndex + 1);
            ImGui::SliderFloat("Value", &tracks[popupTrackIndex][popupStepIndex], 0.0f, 1.0f, "%.4f");
            if(ImGui::Button("Prev") && popupStepIndex > 0) popupStepIndex--;
            ImGui::SameLine();
            if(ImGui::Button("Next") && popupStepIndex < (int)tracks[popupTrackIndex].size() - 1) popupStepIndex++;
            ImGui::SameLine();
            if(ImGui::Button("Close")) ImGui::CloseCurrentPopup();
            updateSolo();
            ImGui::EndPopup();
        }
    }

    void drawDockableGui() {
        float zoom = ofxOceanodeShared::getZoomLevel();
        const auto &customRegionContext = ofxOceanodeShared::getCustomRegionRenderContext();
        const float defaultWidth = (ofxOceanodeShared::getNodeWidthWidget() + ofxOceanodeShared::getNodeWidthText()) * zoom;
        const ImVec2 childSize(
            customRegionContext.active ? std::max(1.0f, customRegionContext.width) : std::max(220.0f, defaultWidth),
            customRegionContext.active ? std::max(1.0f, customRegionContext.height) : std::max(220.0f, 260.0f * zoom)
        );

        ImGui::BeginChild("SoloSequencerDockableGui", childSize, true, ImGuiWindowFlags_HorizontalScrollbar);
        const float contentWidth = std::max(160.0f, ImGui::GetContentRegionAvail().x);
        drawShiftControl(contentWidth);
        ImGui::Separator();
        drawToolbar();
        ImGui::Separator();
        drawEditorContents(contentWidth, false, true, true);
        ImGui::EndChild();
    }

    void drawEditorContents(float requestedWidth, bool showToolbarControls, bool editable, bool showHeaders) {
        float zoom = ofxOceanodeShared::getZoomLevel();
        const float width = std::max(120.0f, requestedWidth);
        const float rowHeight = std::max(12.0f, trackHeight.get() * zoom);
        const float trackSpacing = 4.0f * zoom;

        if(showToolbarControls) {
            drawShiftControl(width);
            ImGui::Separator();
            drawToolbar();
            ImGui::Separator();
        }

        for(size_t i = 0; i < tracks.size(); i++) {
            ImGui::PushID((int)i);
            if(showHeaders) drawTrackHeader((int)i, width);
            drawTrackRow((int)i, width, rowHeight, editable);
            if(i + 1 < tracks.size()) ImGui::Dummy(ImVec2(0, trackSpacing));
            ImGui::PopID();
        }
    }
};

#endif /* soloSequencerGui_h */
