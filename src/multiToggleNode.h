#ifndef multiToggleNode_h
#define multiToggleNode_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "imgui.h"
#include "imgui_internal.h"

class multiToggleNode : public ofxOceanodeNodeModel {
public:
    multiToggleNode() : ofxOceanodeNodeModel("Multi Toggle") {}

    void setup() override {
        description = "Row of toggle buttons outputting a vector<float>. Each press sets output[i]=1, unpress sets 0. Tick Mode makes each press a one-frame pulse.";

        addParameter(size.set("Size", 4, 1, 32));
        addParameter(tickMode.set("Tick Mode", false));
        addOutputParameter(output.set("Output",
            vector<float>(4, 0.0f),
            vector<float>(1, 0.0f),
            vector<float>(1, 1.0f)));

        toggleStates.assign(4, 0.0f);
        shouldReset.assign(4, false);

        listeners.push(size.newListener([this](int &n) {
            resizeToggles(n);
        }));
        listeners.push(tickMode.newListener([this](bool &tm) {
            int n = size.get();
            toggleStates.assign(n, 0.0f);
            shouldReset.assign(n, false);
            output = vector<float>(n, 0.0f);
        }));

        addCustomRegion(guiRegion, [this]() {
            drawGui();
        });
    }

    void update(ofEventArgs &a) override {
        int n = size.get();
        bool anyReset = false;
        vector<float> v = output.get();
        v.resize(n, 0.0f);
        for (int i = 0; i < n; i++) {
            if (i < (int)shouldReset.size() && shouldReset[i]) {
                v[i] = 0.0f;
                if (i < (int)toggleStates.size()) toggleStates[i] = 0.0f;
                shouldReset[i] = false;
                anyReset = true;
            }
        }
        if (anyReset) output = v;
    }

    void presetSave(ofJson &json) override {
        json["toggleStates"] = toggleStates;
    }

    void presetRecallAfterSettingParameters(ofJson &json) override {
        if (json.count("toggleStates") && json["toggleStates"].is_array()) {
            int n = size.get();
            toggleStates.assign(n, 0.0f);
            auto &arr = json["toggleStates"];
            for (int i = 0; i < n && i < (int)arr.size(); i++) {
                if (arr[i].is_number()) toggleStates[i] = arr[i].get<float>();
            }
            if (!tickMode.get()) output = toggleStates;
        }
    }

private:
    ofParameter<int>           size;
    ofParameter<bool>          tickMode;
    ofParameter<vector<float>> output;

    vector<float> toggleStates;
    vector<bool>  shouldReset;
    ofEventListeners listeners;
    customGuiRegion  guiRegion;

    void resizeToggles(int n) {
        toggleStates.resize(n, 0.0f);
        shouldReset.resize(n, false);
        vector<float> v(n, 0.0f);
        if (!tickMode.get()) {
            for (int i = 0; i < n; i++) v[i] = toggleStates[i];
        }
        output = v;
    }

    void onTogglePressed(int index) {
        int n = size.get();
        if (index < 0 || index >= n) return;

        toggleStates.resize(n, 0.0f);
        shouldReset.resize(n, false);

        if (tickMode.get()) {
            vector<float> v(n, 0.0f);
            v[index]            = 1.0f;
            toggleStates[index] = 1.0f;
            shouldReset[index]  = true;
            output = v;
        } else {
            toggleStates[index] = (toggleStates[index] > 0.5f) ? 0.0f : 1.0f;
            output = toggleStates;
        }
    }

    void drawGui() {
		float zoom = ofxOceanodeShared::getZoomLevel();
        int n = size.get();
        if (n <= 0) return;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const float totalWidth = (float)(ofxOceanodeShared::getNodeWidthWidget() + ofxOceanodeShared::getNodeWidthText());
        const float btnH       = 25.0f;
        const float gap        = 1.0f;
        const float cellW      = totalWidth / n;

        // Single invisible button covering the full row
        ImGui::InvisibleButton("##multiToggleRow", ImVec2(totalWidth, btnH));
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        bool hovered = ImGui::IsItemHovered();

        if (clicked) {
            float mx = ImGui::GetIO().MousePos.x - pos.x;
            int idx = (int)(mx / cellW);
            onTogglePressed(idx);
        }

        // Draw each cell
        for (int i = 0; i < n; i++) {
            ImVec2 bMin(pos.x + i * cellW,             pos.y);
            ImVec2 bMax(pos.x + (i + 1) * cellW - gap, pos.y + btnH - gap);

            bool isOn = (i < (int)toggleStates.size() && toggleStates[i] > 0.5f);

            bool isHoveredCell = false;
            if (hovered) {
                float mx = ImGui::GetIO().MousePos.x - pos.x;
                isHoveredCell = (mx >= i * cellW && mx < (i + 1) * cellW);
            }

            ImU32 fillColor;
            if (isOn)              fillColor = IM_COL32(0,   150, 255, 255);
            else if (isHoveredCell) fillColor = IM_COL32(110, 110, 110, 255);
            else                   fillColor = IM_COL32(80,   80,  80, 255);

            drawList->AddRectFilled(bMin, bMax, fillColor, 2.0f);
            drawList->AddRect(bMin, bMax, IM_COL32(150, 150, 150, 200), 2.0f, 0, 1.0f);
        }

        if (hovered) {
            const char* modeStr = tickMode.get() ? "Tick" : "Toggle";
            ImGui::SetTooltip("Multi Toggle | %s mode | %d buttons", modeStr, n);
        }
    }
};

#endif /* multiToggleNode_h */
