#pragma once

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeShared.h"
#include "pitchMapEngine.h"
#include "imgui.h"

// Control-rate scale expansion using PolyComb's oscillator index geometry.
// No server, synth or separate editor is needed.
class pitchMap : public ofxOceanodeNodeModel {
public:
    pitchMap() : ofxOceanodeNodeModel("Pitch Map") {}

    void setup() override {
        description = "Expand an ordered scale of offsets (0-11) into MIDI pitches using PolyComb's index geometry. "
                      "Size sets the output length; Root is the MIDI base. Step and Degree Shift select scale degrees. "
                      "Octave Jump sets the distance between scale repetitions; Repeat After wraps degrees (0 disables). "
                      "Start Index is zero-based. Group Size groups indices; Scatter is stable for each Seed.";
        addParameter(scale.set("Scale", {0, 2, 4, 5, 7, 9, 11}, {0}, {11}));
        addParameter(size.set("Size", 16, 1, 256));
        addParameter(root.set("Root", 60.f, 0.f, 127.f));
        addParameter(octaveJump.set("Octave Jump", 1.f, 0.f, 4.f));
        addParameter(start.set("Start Index", 0, 0, 255));
        addParameter(step.set("Step", 1, -24, 24));
        addParameter(degreeShift.set("Degree Shift", 0, -256, 256));
        addParameter(repeatAfter.set("Repeat After", 0, 0, 256));
        addParameter(skew.set("Skew", -1.f, -1.f, 1.f));
        addParameter(roundness.set("Roundness", 0.f, 0.f, 1.f));
        addParameter(pw.set("PW", .5f, .001f, 1.f));
        addParameter(waves.set("Num Waves", 1.f, 0.f, 16.f));
        addParameter(cycleShift.set("Cycle Shift", 0, -255, 255));
        addParameter(symmetry.set("Symmetry", 0, 0, 256));
        addParameter(comb.set("Comb", 0.f, 0.f, 1.f));
        addParameter(groupSize.set("Group Size", 0, 0, 256));
        addParameter(scatter.set("Scatter", 0.f, 0.f, 256.f));
        addParameter(seed.set("Seed", 0, 0, 9999));
        addParameter(reverse.set("Reverse", false));
        addParameter(pow.set("Pow", 0.f, -1.f, 1.f));
        addParameter(transpose.set("Transpose", 0, -72, 72));
        addParameter(fineTune.set("Fine Tune", 0.f, -1.f, 1.f));
        // Expanded scales may legitimately exceed MIDI's usual 0..127 range.
        addOutputParameter(pitches.set("Pitches", {60}, {-400000}, {400000}));
        addInspectorParameter(widgetWidth.set("Widget Width", 220.f, 100.f, 800.f));
        addInspectorParameter(widgetHeight.set("Widget Height", 70.f, 30.f, 300.f));
        addCustomRegion(preview.set("Pitch Vector", [this]() { drawPreview(); }),
                        [this]() { drawPreview(); });

        watch(scale); watch(size); watch(root); watch(octaveJump);
        watch(start); watch(step); watch(degreeShift); watch(repeatAfter);
        watch(skew); watch(roundness); watch(pw); watch(waves); watch(cycleShift);
        watch(symmetry); watch(comb); watch(groupSize); watch(scatter); watch(seed);
        watch(reverse); watch(pow); watch(transpose); watch(fineTune);
        rebuild();
    }

private:
    template<typename T> void watch(ofParameter<T>& parameter) {
        listeners.push(parameter.newListener([this](T&) { rebuild(); }));
    }

    void rebuild() {
        santipitchmap::Map map;
        map.mode = santipitchmap::ModeTable;
        map.morphGeometry = true;
        map.table = scale.get();
        map.transpose = root.get() - 60.f + transpose.get();
        map.octaveJump = octaveJump.get();
        map.first = 1.f + start.get();
        map.interval = (float)step.get();
        map.degreeOffset = (float)degreeShift.get();
        map.span = (float)repeatAfter.get();
        map.skew = skew.get();
        map.roundness = roundness.get();
        map.pulseWidth = pw.get();
        map.waves = waves.get();
        map.cycleShift = cycleShift.get();
        map.symmetry = symmetry.get();
        map.comb = comb.get();
        map.indexQuantize = (float)groupSize.get();
        map.indexRandom = scatter.get();
        map.seed = seed.get();
        map.invert = reverse.get();
        map.pow = pow.get();
        map.offset = fineTune.get();
        pitches = santipitchmap::generate(map, size.get());
    }

    void drawPreview() {
        const float zoom = ofxOceanodeShared::getZoomLevel();
        const auto& ctx = ofxOceanodeShared::getCustomRegionRenderContext();
        const float width = ctx.active ? std::max(1.f, ctx.width) : widgetWidth.get() * zoom;
        const float height = ctx.active ? std::max(1.f, ctx.height) : widgetHeight.get() * zoom;
        const auto& notes = pitches.get();
        const ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + width, a.y + height);
        auto* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(a, b, IM_COL32(18, 22, 29, 255), 3.f * zoom);
        if(notes.empty()) { ImGui::Dummy(ImVec2(width, height)); return; }
        const auto mm = std::minmax_element(notes.begin(), notes.end());
        const float lo = *mm.first - 1.f, range = std::max(12.f, *mm.second - lo + 1.f);
        const float cell = width / notes.size();
        const float inset = std::min(zoom, cell * .15f);
        for(size_t i = 0; i < notes.size(); ++i) {
            const float x = a.x + i * cell;
            const float y = b.y - 4.f * zoom - (notes[i] - lo) / range * std::max(1.f, height - 8.f * zoom);
            dl->AddRectFilled(ImVec2(x + inset, y), ImVec2(x + cell - inset, b.y - 2.f * zoom), IM_COL32(112, 214, 201, 55));
            dl->AddLine(ImVec2(x + inset, y), ImVec2(x + cell - inset, y), IM_COL32(112, 214, 201, 255), 2.f * zoom);
        }
        ImGui::InvisibleButton("##pitch-vector", ImVec2(width, height));
        if(ImGui::IsItemHovered()) {
            const int i = std::clamp((int)((ImGui::GetIO().MousePos.x - a.x) / cell), 0, (int)notes.size() - 1);
            ImGui::SetTooltip("Index %d | MIDI %.3f", i, notes[i]);
        }
    }

    ofParameter<std::vector<float>> scale, pitches;
    ofParameter<int> size, start, step, degreeShift, repeatAfter, cycleShift;
    ofParameter<int> symmetry, groupSize, seed, transpose;
    ofParameter<float> root, octaveJump, skew, roundness, pw, waves, comb, scatter, pow, fineTune;
    ofParameter<bool> reverse;
    ofParameter<float> widgetWidth, widgetHeight;
    customGuiRegion preview;
    ofEventListeners listeners;
};
