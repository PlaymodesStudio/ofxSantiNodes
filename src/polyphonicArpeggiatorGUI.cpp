#include "polyphonicArpeggiatorGUI.h"

#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT

#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>

namespace {
    const ImVec4 snapshotsBg = ImVec4(0.12f, 0.17f, 0.24f, 0.94f);
    const ImVec4 snapshotsTitle = ImVec4(0.82f, 0.90f, 1.00f, 1.00f);
    const ImVec4 sourceBg = ImVec4(0.18f, 0.15f, 0.10f, 0.94f);
    const ImVec4 sourceTitle = ImVec4(1.00f, 0.87f, 0.64f, 1.00f);
    const ImVec4 patternBg = ImVec4(0.11f, 0.18f, 0.13f, 0.94f);
    const ImVec4 patternTitle = ImVec4(0.72f, 1.00f, 0.74f, 1.00f);
    const ImVec4 polyBg = ImVec4(0.17f, 0.12f, 0.20f, 0.94f);
    const ImVec4 polyTitle = ImVec4(0.93f, 0.80f, 1.00f, 1.00f);
    const ImVec4 euclidBg = ImVec4(0.14f, 0.18f, 0.22f, 0.94f);
    const ImVec4 euclidTitle = ImVec4(0.76f, 0.91f, 1.00f, 1.00f);
    const ImVec4 velocityBg = ImVec4(0.22f, 0.15f, 0.11f, 0.94f);
    const ImVec4 velocityTitle = ImVec4(1.00f, 0.78f, 0.62f, 1.00f);
    const ImVec4 visualizationBg = ImVec4(0.09f, 0.13f, 0.17f, 0.96f);
    const ImVec4 visualizationTitle = ImVec4(0.76f, 0.95f, 1.00f, 1.00f);
    const ImVec4 outputBg = ImVec4(0.11f, 0.15f, 0.12f, 0.94f);
    const ImVec4 outputTitle = ImVec4(0.79f, 1.00f, 0.81f, 1.00f);

    struct KeyGeometry {
        bool isBlack;
        float x;
        float w;
    };

    int wrapIndex(int value, int size) {
        return static_cast<int>(ofxOceanodeTransportUtils::positiveModulo(value, size));
    }

    bool isWhiteKey(int note) {
        int noteInOctave = wrapIndex(note, 12);
        return noteInOctave == 0 || noteInOctave == 2 || noteInOctave == 4 ||
               noteInOctave == 5 || noteInOctave == 7 || noteInOctave == 9 ||
               noteInOctave == 11;
    }

    std::vector<KeyGeometry> buildKeyboardGeometry(int startNote, int endNote, float width) {
        std::vector<KeyGeometry> geometry;
        if(endNote < startNote) return geometry;

        int whiteKeyCount = 0;
        for(int note = startNote; note <= endNote; note++) {
            if(isWhiteKey(note)) whiteKeyCount++;
        }
        if(whiteKeyCount <= 0) return geometry;

        float whiteKeyWidth = width / static_cast<float>(whiteKeyCount);
        float blackKeyWidth = whiteKeyWidth * 0.6f;
        float currentX = 0.0f;

        for(int note = startNote; note <= endNote; note++) {
            KeyGeometry key;
            key.isBlack = !isWhiteKey(note);
            if(!key.isBlack) {
                key.x = currentX;
                key.w = whiteKeyWidth;
                currentX += whiteKeyWidth;
            } else {
                key.x = currentX - blackKeyWidth * 0.5f;
                key.w = blackKeyWidth;
            }
            geometry.push_back(key);
        }

        return geometry;
    }

    std::pair<int, int> computeKeyboardRange(const std::vector<float> &values, int minSemitones) {
        if(values.empty()) {
            return {48, 72};
        }

        int minNote = 127;
        int maxNote = 0;
        for(float value : values) {
            int rounded = ofClamp(static_cast<int>(std::round(value)), 0, 127);
            minNote = std::min(minNote, rounded);
            maxNote = std::max(maxNote, rounded);
        }

        int startNote = std::max(0, minNote - 2);
        int endNote = std::min(127, maxNote + 2);
        if(endNote - startNote + 1 < minSemitones) {
            int center = (minNote + maxNote) / 2;
            startNote = std::max(0, center - minSemitones / 2);
            endNote = std::min(127, startNote + minSemitones - 1);
        }

        startNote = (startNote / 12) * 12;
        endNote = std::min(127, ((endNote / 12) * 12) + 11);
        if(endNote <= startNote) endNote = std::min(127, startNote + std::max(11, minSemitones - 1));
        return {startNote, endNote};
    }

    void drawKeyboardDisplay(const char *id,
                             const std::vector<float> &values,
                             float width,
                             float height,
                             bool active,
                             bool fullRange = false) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImGui::InvisibleButton(id, ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

        drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                                active ? IM_COL32(24, 38, 28, 255) : IM_COL32(24, 24, 24, 255), 4.0f);
        drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(74, 74, 74, 255), 4.0f);

        if(values.empty()) return;

        std::map<int, int> noteCounts;
        for(float value : values) {
            noteCounts[ofClamp(static_cast<int>(std::round(value)), 0, 127)]++;
        }

        auto range = fullRange ? std::pair<int, int>{0, 127} : computeKeyboardRange(values, 18);
        std::vector<KeyGeometry> geometry = buildKeyboardGeometry(range.first, range.second, width);
        float blackKeyHeight = height * 0.62f;
        ImU32 highlightColor = active ? IM_COL32(88, 220, 132, 180) : IM_COL32(80, 170, 235, 170);

        for(size_t i = 0; i < geometry.size(); i++) {
            if(geometry[i].isBlack) continue;
            int note = range.first + static_cast<int>(i);
            ImVec2 keyPos(pos.x + geometry[i].x, pos.y);
            ImVec2 keyEnd(keyPos.x + geometry[i].w, pos.y + height);
            drawList->AddRectFilled(keyPos, keyEnd, IM_COL32(242, 242, 242, 255));
            drawList->AddRect(keyPos, keyEnd, IM_COL32(110, 110, 110, 255));
            if(noteCounts.count(note) > 0) {
                drawList->AddRectFilled(keyPos, keyEnd, highlightColor);
            }
        }

        for(size_t i = 0; i < geometry.size(); i++) {
            if(!geometry[i].isBlack) continue;
            int note = range.first + static_cast<int>(i);
            ImVec2 keyPos(pos.x + geometry[i].x, pos.y);
            ImVec2 keyEnd(keyPos.x + geometry[i].w, pos.y + blackKeyHeight);
            drawList->AddRectFilled(keyPos, keyEnd, IM_COL32(15, 15, 15, 255));
            drawList->AddRect(keyPos, keyEnd, IM_COL32(70, 70, 70, 255));
            if(noteCounts.count(note) > 0) {
                drawList->AddRectFilled(keyPos, keyEnd, highlightColor);
            }
        }
    }

    bool drawDraggableFloatWithPopup(const char *label,
                                     float &value,
                                     float speed,
                                     float minValue,
                                     float maxValue,
                                     const char *format) {
        bool changed = ImGui::DragFloat(label, &value, speed, minValue, maxValue, format);
        value = ofClamp(value, minValue, maxValue);

        if(ImGui::BeginPopupContextItem()) {
            static char valueBuffer[64] = "";
            static ImGuiID activePopupItem = 0;
            ImGuiID currentItem = ImGui::GetItemID();
            if(activePopupItem != currentItem) {
                std::snprintf(valueBuffer, sizeof(valueBuffer), format, value);
                activePopupItem = currentItem;
            }

            ImGui::SetNextItemWidth(120.0f);
            ImGui::SetKeyboardFocusHere();
            if(ImGui::InputText("##value", valueBuffer, sizeof(valueBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                try {
                    value = ofClamp(std::stof(std::string(valueBuffer)), minValue, maxValue);
                    changed = true;
                } catch(...) {
                }
                activePopupItem = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        return changed;
    }

    void beginColoredSection(const char *id,
                             const char *title,
                             const ImVec2 &size,
                             const ImVec4 &bg,
                             const ImVec4 &titleColor,
                             bool &expanded,
                             float zoom) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        const float collapsedHeight = std::max(22.0f, ImGui::GetTextLineHeightWithSpacing() + 8.0f);
        ImGui::BeginChild(id, ImVec2(size.x, expanded ? size.y : collapsedHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(zoom);

        std::string arrowId = std::string("##") + id + "Arrow";
        if(ImGui::ArrowButton(arrowId.c_str(), expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
            expanded = !expanded;
        }
        ImGui::SameLine();
        ImGui::TextColored(titleColor, "%s", title);
        if(expanded) ImGui::Separator();
    }

    float getCompactFieldWidth(float baseWidth, float availableWidth, float zoom, float widthRatio = 0.50f) {
        return std::min(baseWidth * zoom, availableWidth * widthRatio);
    }
}

polyphonicArpeggiatorGUI::polyphonicArpeggiatorGUI()
: ofxOceanodeNodeModel("Polyphonic Arpeggiator GUI")
, dist01(0.0f, 1.0f) {
    description = "Dockable polyphonic arpeggiator with scale and chord-pool source modes.";
    rng.seed(std::random_device{}());
}

polyphonicArpeggiatorGUI::~polyphonicArpeggiatorGUI() {
    listeners.unsubscribeAll();
}

void polyphonicArpeggiatorGUI::setup() {
    addParameter(showEditor.set("Show", false));
    editorWidth.set("Editor Width", 1180.0f, 760.0f, 2200.0f);
    editorHeight.set("Editor Height", 920.0f, 520.0f, 1800.0f);
    addParameter(snapshotRecall.set("Snapshot Recall", 0, 0, SnapshotSlots));

    addSeparator("Trigger", ofColor(200));
    addParameter(trigger.set("Trigger"));
    addParameter(reset.set("Reset"));
    addParameter(resetNext.set("ResetNext"));
    addParameter(internalClockMode.set("Transport Clock", false));
    addParameter(oneShotMode.set("One Shot", false));
    addParameter(beatDiv.set("BeatDiv", 1.0f, 0.125f, 32.0f));

    addSeparator("Source", ofColor(200));
    sourceMode.set("Source Mode", Scale, Scale, ChordPool);
    addParameter(notePoolIn.set("NotePool", std::vector<float>{60.0f, 64.0f, 67.0f}, std::vector<float>{0.0f}, std::vector<float>{127.0f}));
    addParameter(scale.set("Scale", std::vector<float>{0, 2, 4, 5, 7, 9, 11}, std::vector<float>{-24.0f}, std::vector<float>{127.0f}));
    sortPool.set("Sort Pool", true);
    removeDuplicates.set("Dedup Pool", false);
    sourceChangeMode.set("Src Change", KeepPhase, KeepPhase, ResetPattern);

    patternMode.set("Pattern", 0, 0, 3);
    idxPattern.set("IdxPatt", std::vector<int>{0, 1, 2, 3}, std::vector<int>{0}, std::vector<int>{127});
    seqSize.set("SeqSize", 16, 1, MaxSequenceSize);
    sourceStart.set("Source Start", 0, 0, 127);
    sourceStride.set("Source Stride", 1, 0, 24);
    pitchExpand.set("Pitch Expand", false);
    expandStep.set("Expand Step", 12, 1, 48);
    transpose.set("Transpose", 0, -96, 96);
    dynamicMode.set("Dynamic", false);
    accentOnsetMode.set("AccOnset", true);

    polyphony.set("Polyphony", 1, 1, MaxPolyphony);
    polyInterval.set("PolyInterval", 2, 1, 24);
    skipSteps.set("SkipSteps", 0, 0, 32);
    strum.set("Strum", 0.0f, 0.0f, 500.0f);
    strumRndm.set("StrumRndm", 0.0f, 0.0f, 200.0f);
    strumDir.set("StrumDir", 0, 0, 2);

    octaveDev.set("OctDev", 0.0f, 0.0f, 1.0f);
    octaveDevRng.set("OctDevRng", 1, 1, 4);
    idxDev.set("IdxDev", 0.0f, 0.0f, 1.0f);
    idxDevRng.set("IdxDevRng", 2, 1, 12);
    pitchDev.set("PitchDev", 0.0f, 0.0f, 1.0f);
    pitchDevRng.set("PitchDevRng", 2, 1, 12);

    eucLen.set("EucLen", 8, 1, 64);
    eucHits.set("EucHits", 8, 0, 64);
    eucOff.set("EucOff", 0, 0, 63);
    stepChance.set("Step%", 1.0f, 0.0f, 1.0f);
    noteChance.set("Note%", 1.0f, 0.0f, 1.0f);

    velBase.set("VelBase", 0.8f, 0.0f, 1.0f);
    velRndm.set("VelRndm", 0.1f, 0.0f, 1.0f);
    eucAccLen.set("AccLen", 4, 1, 64);
    eucAccHits.set("AccHits", 1, 0, 64);
    eucAccOff.set("AccOff", 0, 0, 63);
    eucAccStrength.set("AccStr", 0.2f, 0.0f, 1.0f);

    durBase.set("DurBase", 100, 1, 5000);
    durRndm.set("DurRndm", 20, 0, 1000);
    eucDurLen.set("DurEucLen", 4, 1, 64);
    eucDurHits.set("DurEucHits", 4, 0, 64);
    eucDurOff.set("DurEucOff", 0, 0, 63);
    durEucStrength.set("DurEucStr", 50, -5000, 5000);
    durationRndPerStep.set("DurRndByStep", true);
    outputHistoryWindowMs.set("OutputHistMs", 4000, 250, 60000);

    morphTime.set("Morph Time", 0.0f, 0.0f, 10.0f);

    addSeparator("Output", ofColor(200));
    addOutputParameter(pitchOut.set("PitchOut", std::vector<float>(16, 60.0f), std::vector<float>{0.0f}, std::vector<float>{127.0f}));
    addOutputParameter(gateOut.set("GateOut", std::vector<int>(16, 0), std::vector<int>{0}, std::vector<int>{1}));
    addOutputParameter(velocityOut.set("VelOut", std::vector<float>(16, 0.0f), std::vector<float>{0.0f}, std::vector<float>{1.0f}));
    addOutputParameter(durOut.set("DurOut", std::vector<float>(16, 0.0f), std::vector<float>{0.0f}, std::vector<float>{60000.0f}));
    addOutputParameter(gateVelOut.set("GateVelOut", std::vector<float>(16, 0.0f), std::vector<float>{0.0f}, std::vector<float>{1.0f}));
    addOutputParameter(eucGateOut.set("EucGateOut", std::vector<int>(16, 0), std::vector<int>{0}, std::vector<int>{1}));
    addOutputParameter(eucAccOut.set("EucAccOut", std::vector<int>(16, 0), std::vector<int>{0}, std::vector<int>{1}));
    addOutputParameter(eucDurOut.set("EucDurOut", std::vector<int>(16, 0), std::vector<int>{0}, std::vector<int>{1}));

    resizeStateVectors(seqSize.get());
    syncUserPatternToSequenceSize();
    setupListeners();

    rebuildSourceMaterial();
    generateEuclideanPattern(euclideanPattern, eucLen.get(), eucHits.get(), eucOff.get());
    generateEuclideanPattern(euclideanAccents, eucAccLen.get(), eucAccHits.get(), eucAccOff.get());
    generateEuclideanPattern(euclideanDurations, eucDurLen.get(), eucDurHits.get(), eucDurOff.get());
    rebuildDeviations();
    rebuildPitchSequence();
    rebuildEuclideanOutputs();
    updateOutputs();
    loadAllSnapshotsFromDisk();
}

void polyphonicArpeggiatorGUI::setupListeners() {
    listeners.push(trigger.newListener([this](void){
        if(!internalClockMode.get()) onTrigger();
    }));
    listeners.push(reset.newListener([this](void){ onReset(); }));
    listeners.push(resetNext.newListener([this](void){ onResetNext(); }));
    listeners.push(internalClockMode.newListener([this](bool &){
        internalClockNeedsSync = true;
    }));
    listeners.push(oneShotMode.newListener([this](bool &enabled) {
        oneShotCycleActive = false;
        oneShotStepsRemaining = 0;
        shouldReset = false;
        if(enabled) {
            currentStep = 0;
            highlightedStep = 0;
            onsetCounter = 0;
            absoluteStepCounter = 0;
            updateOutputs();
        }
    }));
    listeners.push(beatDiv.newListener([this](float &){
        internalClockNeedsSync = true;
    }));
    listeners.push(snapshotRecall.newListener([this](int &value) {
        if(value > 0 && value <= SnapshotSlots) {
            recallSlot(value - 1);
            snapshotRecall = 0;
        }
    }));

    listeners.push(sourceMode.newListener([this](int &){ handleSourceMaterialChange(); }));
    listeners.push(notePoolIn.newListener([this](std::vector<float> &){ handleSourceMaterialChange(); }));
    listeners.push(scale.newListener([this](std::vector<float> &){ handleSourceMaterialChange(); }));
    listeners.push(sortPool.newListener([this](bool &){ handleSourceMaterialChange(); }));
    listeners.push(removeDuplicates.newListener([this](bool &){ handleSourceMaterialChange(); }));

    listeners.push(seqSize.newListener([this](int &size) {
        resizeStateVectors(size);
        syncUserPatternToSequenceSize();
        rebuildDeviations();
        rebuildPitchSequence();
        rebuildEuclideanOutputs();
        updateOutputs();
    }));

    auto rebuildPitch = [this]() {
        rebuildDeviations();
        rebuildPitchSequence();
        updateOutputs();
    };

    listeners.push(sourceStart.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(sourceStride.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(pitchExpand.newListener([rebuildPitch](bool &){ rebuildPitch(); }));
    listeners.push(expandStep.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(transpose.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(polyInterval.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(dynamicMode.newListener([rebuildPitch](bool &){ rebuildPitch(); }));
    listeners.push(patternMode.newListener([this](int &){
        syncUserPatternToSequenceSize();
        updateOutputs();
    }));
    listeners.push(idxPattern.newListener([this](std::vector<int> &){ updateOutputs(); }));

    listeners.push(octaveDev.newListener([rebuildPitch](float &){ rebuildPitch(); }));
    listeners.push(octaveDevRng.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(idxDev.newListener([rebuildPitch](float &){ rebuildPitch(); }));
    listeners.push(idxDevRng.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(pitchDev.newListener([rebuildPitch](float &){ rebuildPitch(); }));
    listeners.push(pitchDevRng.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(durationRndPerStep.newListener([this](bool &){ updateOutputs(); }));

    auto rebuildGatePattern = [this]() {
        generateEuclideanPattern(euclideanPattern, eucLen.get(), eucHits.get(), eucOff.get());
        rebuildEuclideanOutputs();
        updateOutputs();
    };
    auto rebuildAccentPattern = [this]() {
        generateEuclideanPattern(euclideanAccents, eucAccLen.get(), eucAccHits.get(), eucAccOff.get());
        rebuildEuclideanOutputs();
        updateOutputs();
    };
    auto rebuildDurationPattern = [this]() {
        generateEuclideanPattern(euclideanDurations, eucDurLen.get(), eucDurHits.get(), eucDurOff.get());
        rebuildEuclideanOutputs();
        updateOutputs();
    };

    listeners.push(eucLen.newListener([rebuildGatePattern](int &){ rebuildGatePattern(); }));
    listeners.push(eucHits.newListener([rebuildGatePattern](int &){ rebuildGatePattern(); }));
    listeners.push(eucOff.newListener([rebuildGatePattern](int &){ rebuildGatePattern(); }));
    listeners.push(eucAccLen.newListener([rebuildAccentPattern](int &){ rebuildAccentPattern(); }));
    listeners.push(eucAccHits.newListener([rebuildAccentPattern](int &){ rebuildAccentPattern(); }));
    listeners.push(eucAccOff.newListener([rebuildAccentPattern](int &){ rebuildAccentPattern(); }));
    listeners.push(eucDurLen.newListener([rebuildDurationPattern](int &){ rebuildDurationPattern(); }));
    listeners.push(eucDurHits.newListener([rebuildDurationPattern](int &){ rebuildDurationPattern(); }));
    listeners.push(eucDurOff.newListener([rebuildDurationPattern](int &){ rebuildDurationPattern(); }));
}

void polyphonicArpeggiatorGUI::resizeStateVectors(int size) {
    int clampedSize = ofClamp(size, 1, MaxSequenceSize);
    currentPitches.assign(clampedSize, 60.0f);
    currentGates.assign(clampedSize, 0);
    currentVelocities.assign(clampedSize, 0.0f);
    currentDurations.assign(clampedSize, 0.0f);
    noteDurationsMs.assign(clampedSize, 100);
    noteStartTimes.assign(clampedSize, 0);
    deviationValues.assign(clampedSize, 0.0f);
    stepGates.assign(clampedSize, false);
}

void polyphonicArpeggiatorGUI::clearActiveVoices(bool resetCounters) {
    std::fill(currentGates.begin(), currentGates.end(), 0);
    std::fill(currentVelocities.begin(), currentVelocities.end(), 0.0f);
    std::fill(currentDurations.begin(), currentDurations.end(), 0.0f);
    std::fill(stepGates.begin(), stepGates.end(), false);
    std::fill(noteStartTimes.begin(), noteStartTimes.end(), 0);
    if(resetCounters) {
        currentStep = 0;
        highlightedStep = 0;
        shouldReset = false;
        onsetCounter = 0;
        absoluteStepCounter = 0;
        oneShotCycleActive = false;
        oneShotStepsRemaining = 0;
        rebuildPitchSequence();
    }
    updateOutputs();
}

void polyphonicArpeggiatorGUI::syncUserPatternToSequenceSize() {
    int size = std::max(1, seqSize.get());
    std::vector<int> pattern = idxPattern.get();
    if(pattern.size() != static_cast<size_t>(size)) {
        size_t oldSize = pattern.size();
        pattern.resize(size);
        for(int i = static_cast<int>(oldSize); i < size; i++) {
            pattern[i] = wrapIndex(i, size);
        }
    }

    for(int &value : pattern) {
        value = wrapIndex(value, size);
    }

    if(pattern != idxPattern.get()) idxPattern = pattern;
}

std::string polyphonicArpeggiatorGUI::describeBeatDiv(float beatDivision) const {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(beatDivision < 1.0f ? 2 : (std::abs(beatDivision - std::round(beatDivision)) < 0.001f ? 0 : 2));

    if(std::abs(beatDivision - 1.0f) < 0.001f) return "1 beat";
    if(beatDivision > 1.0f) {
        if(std::abs(beatDivision - 2.0f) < 0.001f) return "8th";
        if(std::abs(beatDivision - 4.0f) < 0.001f) return "16th";
        if(std::abs(beatDivision - 8.0f) < 0.001f) return "32nd";
        stream << beatDivision;
        return stream.str() + " div/beat";
    }

    float beatsPerStep = 1.0f / std::max(0.001f, beatDivision);
    stream.str("");
    stream.clear();
    stream << std::fixed << std::setprecision(std::abs(beatsPerStep - std::round(beatsPerStep)) < 0.001f ? 0 : 2) << beatsPerStep;
    return stream.str() + " beats";
}

void polyphonicArpeggiatorGUI::setBpm(float bpm) {
    currentBpm = std::max(1.0f, bpm);
}

void polyphonicArpeggiatorGUI::update(ofEventArgs &) {
    const auto frameState = getFrameTransportState();
    const auto &previousTransport = frameState.previous;
    const auto &currentTransport = frameState.current;
    currentBpm = std::max(1.0f, currentTransport.bpm);

    uint64_t now = ofGetElapsedTimeMillis();
    bool needsUpdate = false;
    pruneOutputHistory(now);

    if(internalClockMode.get()) {
        const double stepsPerBeat = beatDiv.get();
        if(ofxOceanodeTransportUtils::didTransportDiscontinuity(frameState)) {
            clearActiveVoices(true);
            internalClockNeedsSync = false;
            if(currentTransport.isPlaying &&
               ofxOceanodeTransportUtils::isStepBoundary(currentTransport.beatPosition, stepsPerBeat)) {
                onTrigger();
            }
        } else if(internalClockNeedsSync) {
            internalClockNeedsSync = false;
            if(currentTransport.isPlaying &&
               ofxOceanodeTransportUtils::isStepBoundary(currentTransport.beatPosition, stepsPerBeat)) {
                onTrigger();
            }
        } else if(currentTransport.isPlaying) {
            if(!previousTransport.isPlaying &&
               ofxOceanodeTransportUtils::isStepBoundary(currentTransport.beatPosition, stepsPerBeat)) {
                onTrigger();
            }

            const auto crossedSteps = ofxOceanodeTransportUtils::getCrossedStepRange(frameState, stepsPerBeat);
            if(crossedSteps.valid) {
                for(int64_t step = crossedSteps.firstStep; step <= crossedSteps.lastStep; step++) {
                    onTrigger();
                }
            }
        }
    } else {
        internalClockNeedsSync = true;
    }

    for(size_t i = 0; i < currentGates.size(); i++) {
        if(currentGates[i] == 0 && noteStartTimes[i] > 0 && now >= noteStartTimes[i]) {
            currentGates[i] = 1;
            recordOutputHistoryEvent(currentPitches[i], currentVelocities[i], noteDurationsMs[i], now);
            noteStartTimes[i] = now;
            needsUpdate = true;
        }
        if(currentGates[i] == 1 && noteStartTimes[i] > 0 && now >= noteStartTimes[i] + static_cast<uint64_t>(std::max(1, noteDurationsMs[i]))) {
            currentGates[i] = 0;
            noteStartTimes[i] = 0;
            needsUpdate = true;
        }
    }

    if(isMorphing) updateMorph();
    if(needsUpdate) updateOutputs();
}

void polyphonicArpeggiatorGUI::draw(ofEventArgs &) {
    if(!showEditor.get()) return;

    ImGui::SetNextWindowSize(ImVec2(editorWidth.get(), editorHeight.get()), ImGuiCond_FirstUseEver);

    std::string windowName = "Polyphonic Arpeggiator GUI##" + ofToString(getNumIdentifier());
    bool open = showEditor.get();

    if(ImGui::Begin(windowName.c_str(), &open)) {
        ImVec2 size = ImGui::GetWindowSize();
        editorWidth = size.x;
        editorHeight = size.y;
        drawEditor();
    }

    ImGui::End();

    if(open != showEditor.get()) {
        showEditor = open;
    }
}

void polyphonicArpeggiatorGUI::drawEditor() {
    constexpr float baseGap = 6.0f;
    constexpr float baseTopHeight = 430.0f;
    constexpr float baseBottomHeight = 270.0f;
    constexpr float baseContentWidth = 1140.0f;
    ImVec2 available = ImGui::GetContentRegionAvail();
    float widthScale = available.x / std::max(1.0f, baseContentWidth);
    editorZoom = ofClamp(widthScale, 0.38f, 1.0f);
    float editorFontZoom = ofClamp(editorZoom + 0.08f, 0.46f, 1.0f);

    ImGui::SetWindowFontScale(editorFontZoom);

    ImGuiStyle &style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * editorZoom, 4.0f * editorZoom));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f * editorZoom, 3.0f * editorZoom));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * editorZoom, 8.0f * editorZoom));

    float gap = baseGap * editorZoom;
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float topWidgetWidth = (availableWidth - gap * 5.0f) / 6.0f;
    float bottomWidgetWidth = (availableWidth - gap) * 0.5f;
    float topHeight = baseTopHeight * editorZoom;
    float bottomHeight = baseBottomHeight * editorZoom;

    beginColoredSection("ArpSnapshots", "Snapshots", ImVec2(topWidgetWidth, topHeight), snapshotsBg, snapshotsTitle, snapshotsSectionExpanded, editorFontZoom);
    if(snapshotsSectionExpanded) drawSnapshotsSection();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);
    beginColoredSection("ArpSource", "Source", ImVec2(topWidgetWidth, topHeight), sourceBg, sourceTitle, sourceSectionExpanded, editorFontZoom);
    if(sourceSectionExpanded) drawSourceSection();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);
    beginColoredSection("ArpPattern", "Pattern", ImVec2(topWidgetWidth, topHeight), patternBg, patternTitle, patternSectionExpanded, editorFontZoom);
    if(patternSectionExpanded) drawPatternSection();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);
    beginColoredSection("ArpPoly", "Polyphony", ImVec2(topWidgetWidth, topHeight), polyBg, polyTitle, polyphonySectionExpanded, editorFontZoom);
    if(polyphonySectionExpanded) drawPolyphonySection();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);
    beginColoredSection("ArpEuclidean", "Euclid", ImVec2(topWidgetWidth, topHeight), euclidBg, euclidTitle, euclideanSectionExpanded, editorFontZoom);
    if(euclideanSectionExpanded) drawEuclideanSection();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);
    beginColoredSection("ArpVelDur", "Vel / Dur", ImVec2(topWidgetWidth, topHeight), velocityBg, velocityTitle, velocityDurationSectionExpanded, editorFontZoom);
    if(velocityDurationSectionExpanded) drawVelocityDurationSection();
    ImGui::EndChild();

    beginColoredSection("ArpVisualization", "Visualization", ImVec2(bottomWidgetWidth, bottomHeight), visualizationBg, visualizationTitle, visualizationSectionExpanded, editorFontZoom);
    if(visualizationSectionExpanded) drawVisualizationSection();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);
    beginColoredSection("ArpOutput", "Output", ImVec2(0, bottomHeight), outputBg, outputTitle, outputSectionExpanded, editorFontZoom);
    if(outputSectionExpanded) drawOutputSection();
    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::SetWindowFontScale(1.0f);
}

void polyphonicArpeggiatorGUI::drawSnapshotsSection() {
    float compactWidth = getCompactFieldWidth(80.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.46f);
    ImGui::TextDisabled("Shift saves");

    float slotSize = 22.0f * editorZoom;
    float slotGap = 4.0f * editorZoom;
    int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + slotGap) / (slotSize + slotGap)));

    for(int i = 0; i < SnapshotSlots; i++) {
        if(i > 0 && (i % columns) != 0) ImGui::SameLine(0.0f, slotGap);

        bool hasData = snapshotSlots[i].hasData;
        bool isActive = activeSnapshotSlot == i;
        ImVec4 color = hasData ? ImVec4(0.22f, 0.36f, 0.52f, 1.0f) : ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
        if(isActive) color = ImVec4(0.42f, 0.62f, 0.86f, 1.0f);

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x + 0.08f, color.y + 0.08f, color.z + 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x + 0.12f, color.y + 0.12f, color.z + 0.12f, 1.0f));

        if(ImGui::Button(ofToString(i + 1).c_str(), ImVec2(slotSize, slotSize))) {
            if(ImGui::GetIO().KeyShift) storeToSlot(i);
            else recallSlot(i);
        }

        if(ImGui::BeginPopupContextItem("SnapshotContext")) {
            if(hasData && ImGui::MenuItem("Delete Snapshot")) {
                deleteSnapshotFromDisk(i);
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    ImGui::Spacing();
    int selectedSlot = activeSnapshotSlot;
    if(selectedSlot < 0 || !snapshotSlots[selectedSlot].hasData) {
        selectedSlot = -1;
        for(int i = 0; i < SnapshotSlots; i++) {
            if(snapshotSlots[i].hasData) {
                selectedSlot = i;
                break;
            }
        }
    }

    std::string preview = selectedSlot >= 0
        ? ofToString(selectedSlot + 1) + ": " + snapshotSlots[selectedSlot].name
        : std::string("Select");

    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::BeginCombo("Load", preview.c_str())) {
        for(int i = 0; i < SnapshotSlots; i++) {
            if(!snapshotSlots[i].hasData) continue;
            bool selected = i == activeSnapshotSlot;
            std::string label = ofToString(i + 1) + ": " + snapshotSlots[i].name;
            if(ImGui::Selectable(label.c_str(), selected)) recallSlot(i);
            if(selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if(activeSnapshotSlot >= 0 && snapshotSlots[activeSnapshotSlot].hasData) {
        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", snapshotSlots[activeSnapshotSlot].name.c_str());
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputText("Nm", nameBuf, sizeof(nameBuf))) {
            snapshotSlots[activeSnapshotSlot].name = nameBuf;
            saveSnapshotToDisk(activeSnapshotSlot);
        }
    }

    float morph = morphTime.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Morph", morph, 0.02f, 0.0f, 10.0f, "%.2f")) {
        morphTime = morph;
    }
}

void polyphonicArpeggiatorGUI::drawSourceSection() {
    float compactWidth = getCompactFieldWidth(84.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.46f);
    int srcMode = sourceMode.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::BeginCombo("Mode", srcMode == Scale ? "Scale" : "Pool")) {
        if(ImGui::Selectable("Scale", srcMode == Scale)) sourceMode = Scale;
        if(ImGui::Selectable("Chord Pool", srcMode == ChordPool)) sourceMode = ChordPool;
        ImGui::EndCombo();
    }

    if(srcMode == ChordPool) {
        bool sort = sortPool.get();
        if(ImGui::Checkbox("Sort", &sort)) sortPool = sort;

        bool dedup = removeDuplicates.get();
        if(ImGui::Checkbox("Dedup", &dedup)) removeDuplicates = dedup;

        int changeMode = sourceChangeMode.get();
        const char *changeLabel = changeMode == KeepPhase ? "Keep" : "Reset";
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::BeginCombo("On Src", changeLabel)) {
            if(ImGui::Selectable("Keep Phase", changeMode == KeepPhase)) sourceChangeMode = KeepPhase;
            if(ImGui::Selectable("Reset Pattern", changeMode == ResetPattern)) sourceChangeMode = ResetPattern;
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextWrapped("Scale degrees.");
    }

    bool expand = pitchExpand.get();
    if(ImGui::Checkbox("Pitch Expand", &expand)) pitchExpand = expand;

    int expandValue = expandStep.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Expand Step", &expandValue)) expandStep = ofClamp(expandValue, expandStep.getMin(), expandStep.getMax());

    ImGui::TextDisabled("%s n:%zu", srcMode == Scale ? "Src" : "Pool", activeSourceValues.size());

    float previewHeight = 54.0f * editorZoom;
    drawSourcePoolPreview(ImGui::GetContentRegionAvail().x, previewHeight);
    ImGui::Spacing();

    std::vector<float> previewNotes = getSourcePreviewNotes();
    drawKeyboardDisplay("SourceKeyboard", previewNotes, ImGui::GetContentRegionAvail().x, 60.0f * editorZoom, srcMode == ChordPool);

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextWrapped("%s", summarizeVector(srcMode == ChordPool ? activeSourceValues : scale.get(), 18).c_str());
    ImGui::PopTextWrapPos();
}

void polyphonicArpeggiatorGUI::drawPatternSection() {
    float compactWidth = getCompactFieldWidth(78.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.46f);
    const bool poolMode = sourceMode.get() == ChordPool;
    const char *startLabel = poolMode ? "Pool Start" : "Deg Start";
    const char *strideLabel = poolMode ? "Pool Stride" : "Deg Stride";

    bool internalClock = internalClockMode.get();
    if(ImGui::Checkbox("Transport", &internalClock)) internalClockMode = internalClock;

    bool oneShot = oneShotMode.get();
    if(ImGui::Checkbox("One Shot", &oneShot)) oneShotMode = oneShot;

    float division = beatDiv.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Beat Div", division, 0.05f, beatDiv.getMin(), beatDiv.getMax(), "%.3f")) {
        beatDiv = division;
    }
    std::string clockLabel = internalClockMode.get()
        ? "Transport " + describeBeatDiv(beatDiv.get())
        : "External trig";
    ImGui::TextDisabled("%s | BPM %.2f", clockLabel.c_str(), currentBpm);
    if(oneShotMode.get()) {
        ImGui::TextDisabled("%s %d", oneShotCycleActive ? "One shot running:" : "One shot idle:", std::max(0, oneShotStepsRemaining));
    }

    ImGui::Separator();

    if(ImGui::Button("Trig")) onTrigger();
    ImGui::SameLine();
    if(ImGui::Button("Reset")) onReset();
    ImGui::SameLine();
    if(ImGui::Button("Reset N")) onResetNext();

    int sizeValue = seqSize.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Seq", &sizeValue)) seqSize = ofClamp(sizeValue, seqSize.getMin(), seqSize.getMax());

    int modeValue = patternMode.get();
    const char *modeLabel = modeValue == 0 ? "Asc" : modeValue == 1 ? "Desc" : modeValue == 2 ? "Rnd" : "User";
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::BeginCombo("Travel", modeLabel)) {
        if(ImGui::Selectable("Ascending", modeValue == 0)) patternMode = 0;
        if(ImGui::Selectable("Descending", modeValue == 1)) patternMode = 1;
        if(ImGui::Selectable("Random", modeValue == 2)) patternMode = 2;
        if(ImGui::Selectable("User", modeValue == 3)) patternMode = 3;
        ImGui::EndCombo();
    }

    int startValue = sourceStart.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt(startLabel, &startValue)) sourceStart = ofClamp(startValue, sourceStart.getMin(), sourceStart.getMax());

    int strideValue = sourceStride.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt(strideLabel, &strideValue)) sourceStride = ofClamp(strideValue, sourceStride.getMin(), sourceStride.getMax());

    int transposeValue = transpose.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Transp", &transposeValue)) transpose = ofClamp(transposeValue, transpose.getMin(), transpose.getMax());

    bool accentMode = accentOnsetMode.get();
    if(ImGui::Checkbox("Onset Acc", &accentMode)) accentOnsetMode = accentMode;

    if(patternMode.get() == 3) {
        ImGui::Spacing();
        drawUserPatternEditor(ImGui::GetContentRegionAvail().x, 124.0f * editorZoom);
    }

    ImGui::TextDisabled("Step: %d", wrapIndex(highlightedStep, std::max(1, seqSize.get())) + 1);
}

void polyphonicArpeggiatorGUI::drawPolyphonySection() {
    float compactWidth = getCompactFieldWidth(78.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.46f);
    int polyValue = polyphony.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Poly", &polyValue)) polyphony = ofClamp(polyValue, polyphony.getMin(), polyphony.getMax());

    int intervalValue = polyInterval.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Voice Str", &intervalValue)) polyInterval = ofClamp(intervalValue, polyInterval.getMin(), polyInterval.getMax());

    int skipValue = skipSteps.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Skip", &skipValue)) skipSteps = ofClamp(skipValue, skipSteps.getMin(), skipSteps.getMax());

    float strumValue = strum.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Strum", strumValue, 1.0f, 0.0f, 500.0f, "%.1f")) strum = strumValue;

    float strumRnd = strumRndm.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Str Rnd", strumRnd, 1.0f, 0.0f, 200.0f, "%.1f")) strumRndm = strumRnd;

    int direction = strumDir.get();
    const char *dirLabel = direction == 0 ? "Asc" : direction == 1 ? "Desc" : "Rnd";
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::BeginCombo("Str Dir", dirLabel)) {
        if(ImGui::Selectable("Ascending", direction == 0)) strumDir = 0;
        if(ImGui::Selectable("Descending", direction == 1)) strumDir = 1;
        if(ImGui::Selectable("Random", direction == 2)) strumDir = 2;
        ImGui::EndCombo();
    }

    ImGui::Separator();

    float octProb = octaveDev.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Oct Prob", octProb, 0.01f, 0.0f, 1.0f, "%.2f")) octaveDev = octProb;
    int octRange = octaveDevRng.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Oct Range", &octRange)) octaveDevRng = ofClamp(octRange, octaveDevRng.getMin(), octaveDevRng.getMax());

    float idxProb = idxDev.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Idx Prob", idxProb, 0.01f, 0.0f, 1.0f, "%.2f")) idxDev = idxProb;
    int idxRange = idxDevRng.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Idx Range", &idxRange)) idxDevRng = ofClamp(idxRange, idxDevRng.getMin(), idxDevRng.getMax());

    float pitchProb = pitchDev.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Pitch Prob", pitchProb, 0.01f, 0.0f, 1.0f, "%.2f")) pitchDev = pitchProb;
    int pitchRange = pitchDevRng.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Pitch Rng", &pitchRange)) pitchDevRng = ofClamp(pitchRange, pitchDevRng.getMin(), pitchDevRng.getMax());
}

void polyphonicArpeggiatorGUI::drawEuclideanSection() {
    float compactWidth = getCompactFieldWidth(78.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.46f);
    int gateLen = eucLen.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Gate Len", &gateLen)) eucLen = ofClamp(gateLen, eucLen.getMin(), eucLen.getMax());
    int gateHits = eucHits.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Gate Hits", &gateHits)) eucHits = ofClamp(gateHits, eucHits.getMin(), eucHits.getMax());
    int gateOff = eucOff.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Gate Off", &gateOff)) eucOff = ofClamp(gateOff, eucOff.getMin(), eucOff.getMax());

    int accLenValue = eucAccLen.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Acc Len", &accLenValue)) eucAccLen = ofClamp(accLenValue, eucAccLen.getMin(), eucAccLen.getMax());
    int accHitsValue = eucAccHits.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Acc Hits", &accHitsValue)) eucAccHits = ofClamp(accHitsValue, eucAccHits.getMin(), eucAccHits.getMax());
    int accOffValue = eucAccOff.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Acc Off", &accOffValue)) eucAccOff = ofClamp(accOffValue, eucAccOff.getMin(), eucAccOff.getMax());

    int durLenValue = eucDurLen.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Dur Len", &durLenValue)) eucDurLen = ofClamp(durLenValue, eucDurLen.getMin(), eucDurLen.getMax());
    int durHitsValue = eucDurHits.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Dur Hits", &durHitsValue)) eucDurHits = ofClamp(durHitsValue, eucDurHits.getMin(), eucDurHits.getMax());
    int durOffValue = eucDurOff.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Dur Off", &durOffValue)) eucDurOff = ofClamp(durOffValue, eucDurOff.getMin(), eucDurOff.getMax());

    float stepProb = stepChance.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Step%", stepProb, 0.01f, 0.0f, 1.0f, "%.2f")) stepChance = stepProb;
    float noteProb = noteChance.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Note%", noteProb, 0.01f, 0.0f, 1.0f, "%.2f")) noteChance = noteProb;

    ImGui::Spacing();
    drawEuclideanPreview(ImGui::GetContentRegionAvail().x, 86.0f * editorZoom);
}

void polyphonicArpeggiatorGUI::drawVelocityDurationSection() {
    float compactWidth = getCompactFieldWidth(78.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.46f);
    float baseVel = velBase.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Vel", baseVel, 0.01f, 0.0f, 1.0f, "%.2f")) velBase = baseVel;
    float randomVel = velRndm.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Vel Rnd", randomVel, 0.01f, 0.0f, 1.0f, "%.2f")) velRndm = randomVel;
    float accentStrength = eucAccStrength.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Acc Amt", accentStrength, 0.01f, 0.0f, 1.0f, "%.2f")) eucAccStrength = accentStrength;

    int baseDuration = durBase.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Dur", &baseDuration)) durBase = ofClamp(baseDuration, durBase.getMin(), durBase.getMax());
    int randomDuration = durRndm.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Dur Rnd", &randomDuration)) durRndm = ofClamp(randomDuration, durRndm.getMin(), durRndm.getMax());
    int durationAccent = durEucStrength.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Dur Acc", &durationAccent)) durEucStrength = ofClamp(durationAccent, durEucStrength.getMin(), durEucStrength.getMax());
    bool randomByStep = durationRndPerStep.get();
    if(ImGui::Checkbox("Rnd by Step", &randomByStep)) durationRndPerStep = randomByStep;

    ImGui::Separator();
    ImGui::TextDisabled("Prev");
    ImGui::Text("Vel %.2f..%.2f", velBase.get(), ofClamp(velBase.get() + velRndm.get() + eucAccStrength.get(), 0.0f, 1.0f));
    ImGui::Text("Dur %d..%d", std::max(1, durBase.get() + std::min(0, durEucStrength.get())), std::min(60000, durBase.get() + durRndm.get() + std::max(0, durEucStrength.get())));
    ImGui::TextDisabled(durationRndPerStep.get() ? "Dur random per step" : "Dur random per note");
}

void polyphonicArpeggiatorGUI::drawVisualizationSection() {
    float width = ImGui::GetContentRegionAvail().x;
    drawArpGrid(width, 180.0f * editorZoom);
}

void polyphonicArpeggiatorGUI::drawOutputSection() {
    std::vector<float> notes = getActiveOutputPreviewNotes();
    drawKeyboardDisplay("OutputKeyboard", notes, ImGui::GetContentRegionAvail().x, 64.0f * editorZoom, true, true);

    ImGui::Spacing();
    int historyWindow = outputHistoryWindowMs.get();
    float compactWidth = getCompactFieldWidth(90.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.34f);
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Hist ms", &historyWindow)) outputHistoryWindowMs = ofClamp(historyWindow, outputHistoryWindowMs.getMin(), outputHistoryWindowMs.getMax());
    drawOutputHistoryRoll(ImGui::GetContentRegionAvail().x, 104.0f * editorZoom);
}

void polyphonicArpeggiatorGUI::drawOutputHistoryRoll(float width, float height) const {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##ArpOutputHistory", ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(17, 18, 22, 255), 4.0f);
    drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(68, 72, 80, 255), 4.0f);

    uint64_t now = ofGetElapsedTimeMillis();
    uint64_t windowMs = static_cast<uint64_t>(std::max(1, outputHistoryWindowMs.get()));
    uint64_t windowStart = now > windowMs ? now - windowMs : 0;
    float leftPad = 28.0f;
    float topPad = 16.0f;
    float bottomPad = 14.0f;
    float gridX = pos.x + leftPad;
    float gridY = pos.y + topPad;
    float gridW = std::max(1.0f, width - leftPad - 4.0f);
    float gridH = std::max(1.0f, height - topPad - bottomPad);

    int minNote = 127;
    int maxNote = 0;
    bool hasVisibleEvents = false;
    for(const auto &event : outputHistory) {
        uint64_t eventEnd = event.startTimeMs + static_cast<uint64_t>(std::max(1, event.durationMs));
        if(eventEnd < windowStart || event.startTimeMs > now) continue;
        int rounded = ofClamp(static_cast<int>(std::round(event.pitch)), 0, 127);
        minNote = std::min(minNote, rounded);
        maxNote = std::max(maxNote, rounded);
        hasVisibleEvents = true;
    }

    if(!hasVisibleEvents) {
        drawList->AddText(ImVec2(pos.x + 8.0f, pos.y + 8.0f), IM_COL32(210, 210, 210, 180), "Output History");
        drawList->AddText(ImVec2(pos.x + 8.0f, pos.y + 30.0f), IM_COL32(160, 160, 160, 180), "No recent notes");
        return;
    }

    minNote = std::max(0, minNote - 2);
    maxNote = std::min(127, maxNote + 2);
    int noteSpan = std::max(1, maxNote - minNote + 1);
    float laneHeight = gridH / static_cast<float>(noteSpan);

    for(int note = minNote; note <= maxNote; note++) {
        float y = gridY + (maxNote - note) * laneHeight;
        ImU32 lineColor = isWhiteKey(note) ? IM_COL32(44, 48, 52, 120) : IM_COL32(30, 32, 36, 100);
        drawList->AddLine(ImVec2(gridX, y), ImVec2(gridX + gridW, y), lineColor);
    }

    for(int div = 0; div <= 4; div++) {
        float x = gridX + gridW * static_cast<float>(div) / 4.0f;
        drawList->AddLine(ImVec2(x, gridY), ImVec2(x, gridY + gridH), IM_COL32(42, 46, 52, 140));
    }

    for(const auto &event : outputHistory) {
        uint64_t eventEnd = event.startTimeMs + static_cast<uint64_t>(std::max(1, event.durationMs));
        if(eventEnd < windowStart || event.startTimeMs > now) continue;

        float startNorm = ofClamp(static_cast<float>(static_cast<int64_t>(event.startTimeMs) - static_cast<int64_t>(windowStart)) / static_cast<float>(windowMs), 0.0f, 1.0f);
        float endNorm = ofClamp(static_cast<float>(static_cast<int64_t>(eventEnd) - static_cast<int64_t>(windowStart)) / static_cast<float>(windowMs), 0.0f, 1.0f);
        float x1 = gridX + gridW * startNorm;
        float x2 = gridX + gridW * std::max(endNorm, startNorm + 0.004f);
        int note = ofClamp(static_cast<int>(std::round(event.pitch)), 0, 127);
        float y = gridY + (maxNote - note) * laneHeight;
        int alpha = static_cast<int>(90 + ofClamp(event.velocity, 0.0f, 1.0f) * 150.0f);
        ImU32 color = IM_COL32(108, 220, 186, alpha);
        drawList->AddRectFilled(ImVec2(x1, y + 1.0f), ImVec2(std::min(gridX + gridW - 1.0f, x2), y + laneHeight - 1.0f), color, 2.0f);
        drawList->AddRect(ImVec2(x1, y + 1.0f), ImVec2(std::min(gridX + gridW - 1.0f, x2), y + laneHeight - 1.0f), IM_COL32(255, 255, 255, 26), 2.0f);
    }

    drawList->AddText(ImVec2(pos.x + 6.0f, pos.y + 2.0f), IM_COL32(220, 220, 220, 220), "Output History");
    drawList->AddText(ImVec2(pos.x + 4.0f, gridY - 2.0f), IM_COL32(180, 180, 180, 180), ofToString(maxNote).c_str());
    drawList->AddText(ImVec2(pos.x + 4.0f, gridY + gridH - 14.0f), IM_COL32(180, 180, 180, 180), ofToString(minNote).c_str());
}

void polyphonicArpeggiatorGUI::drawUserPatternEditor(float width, float height) {
    syncUserPatternToSequenceSize();

    int size = std::max(1, seqSize.get());
    std::vector<int> pattern = idxPattern.get();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##UserPatternEditor", ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(18, 28, 20, 255), 4.0f);
    drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(70, 90, 74, 255), 4.0f);

    float stepWidth = width / static_cast<float>(size);
    float laneHeight = height / static_cast<float>(size);

    for(int lane = 0; lane <= size; lane++) {
        float y = pos.y + lane * laneHeight;
        drawList->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + width, y), IM_COL32(44, 60, 48, 120));
    }
    for(int step = 0; step <= size; step++) {
        float x = pos.x + step * stepWidth;
        drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + height), IM_COL32(44, 60, 48, 120));
    }

    int highlighted = wrapIndex(highlightedStep, size);
    float highlightX = pos.x + highlighted * stepWidth;
    drawList->AddRectFilled(ImVec2(highlightX, pos.y), ImVec2(highlightX + stepWidth, pos.y + height), IM_COL32(255, 255, 255, 18));

    bool changed = false;
    if((ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) || ImGui::IsItemActive()) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float localX = ofClamp(mouse.x - pos.x, 0.0f, std::max(0.0f, width - 1.0f));
        float localY = ofClamp(mouse.y - pos.y, 0.0f, std::max(0.0f, height - 1.0f));
        int step = ofClamp(static_cast<int>(localX / std::max(1.0f, stepWidth)), 0, size - 1);
        float normalized = 1.0f - localY / std::max(1.0f, height - 1.0f);
        int value = ofClamp(static_cast<int>(std::round(normalized * static_cast<float>(size - 1))), 0, size - 1);
        if(pattern[step] != value) {
            pattern[step] = value;
            changed = true;
        }
    }

    for(int step = 0; step < size; step++) {
        int value = wrapIndex(pattern[step], size);
        float x = pos.x + step * stepWidth;
        float barTop = pos.y + (size - 1 - value) * laneHeight;
        ImU32 fill = step == highlighted ? IM_COL32(136, 255, 156, 220) : IM_COL32(90, 210, 122, 200);
        drawList->AddRectFilled(ImVec2(x + 1.0f, barTop + 1.0f),
                                ImVec2(x + stepWidth - 1.0f, pos.y + height - 1.0f),
                                fill, 2.0f);
    }

    drawList->AddText(ImVec2(pos.x + 6.0f, pos.y + 4.0f), IM_COL32(215, 238, 220, 210), "User Pattern");
    drawList->AddText(ImVec2(pos.x + width - 22.0f, pos.y + 4.0f), IM_COL32(190, 210, 194, 180), ofToString(size - 1).c_str());
    drawList->AddText(ImVec2(pos.x + width - 14.0f, pos.y + height - 18.0f), IM_COL32(190, 210, 194, 180), "0");

    if(changed) idxPattern = pattern;
}

void polyphonicArpeggiatorGUI::drawSourcePoolPreview(float width, float height) const {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##SourcePoolPreview", ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(26, 26, 28, 255), 4.0f);
    drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(68, 68, 70, 255), 4.0f);

    std::vector<float> preview = sourceMode.get() == ChordPool ? activeSourceValues : getSourcePreviewNotes();
    if(preview.empty()) return;

    int visibleCount = std::min(static_cast<int>(preview.size()), 20);
    float gap = 4.0f;
    float chipWidth = std::max(28.0f, (width - gap * (visibleCount + 1)) / static_cast<float>(visibleCount));
    float chipHeight = height - 10.0f;

    for(int i = 0; i < visibleCount; i++) {
        float x = pos.x + gap + i * (chipWidth + gap);
        ImVec2 chipMin(x, pos.y + 5.0f);
        ImVec2 chipMax(x + chipWidth, chipMin.y + chipHeight);
        bool isPool = sourceMode.get() == ChordPool;
        ImU32 color = isPool ? IM_COL32(56, 108, 138, 220) : IM_COL32(82, 126, 80, 220);
        drawList->AddRectFilled(chipMin, chipMax, color, 3.0f);
        drawList->AddRect(chipMin, chipMax, IM_COL32(120, 140, 150, 200), 3.0f);

        std::string label = ofToString(preview[i], sourceMode.get() == ChordPool ? 1 : 0);
        ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        drawList->AddText(ImVec2(chipMin.x + std::max(2.0f, (chipWidth - textSize.x) * 0.5f),
                                 chipMin.y + std::max(2.0f, (chipHeight - textSize.y) * 0.5f)),
                          IM_COL32(245, 245, 245, 255),
                          label.c_str());
    }

    if(static_cast<int>(preview.size()) > visibleCount) {
        std::string extra = "+" + ofToString(static_cast<int>(preview.size()) - visibleCount);
        drawList->AddText(ImVec2(pos.x + width - 34.0f, pos.y + 7.0f), IM_COL32(210, 210, 210, 220), extra.c_str());
    }
}

void polyphonicArpeggiatorGUI::drawEuclideanPreview(float width, float height) const {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##ArpEuclideanPreview", ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(23, 23, 25, 255), 4.0f);
    drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(70, 70, 70, 255), 4.0f);

    struct LaneInfo {
        const std::vector<bool> *pattern = nullptr;
        ImU32 color;
        const char *label;
    };

    std::array<LaneInfo, 3> lanes = {{
        {&euclideanPattern, IM_COL32(210, 100, 96, 255), "Gate"},
        {&euclideanAccents, IM_COL32(236, 182, 82, 255), "Accent"},
        {&euclideanDurations, IM_COL32(92, 156, 236, 255), "Dur"}
    }};

    float rowHeight = height / 3.0f;
    for(int row = 0; row < 3; row++) {
        float rowY = pos.y + row * rowHeight;
        const auto &lane = lanes[row];
        int len = std::max(1, static_cast<int>(lane.pattern->size()));
        float stepWidth = width / static_cast<float>(len);

        drawList->AddText(ImVec2(pos.x + 4.0f, rowY + 2.0f), IM_COL32(230, 230, 230, 220), lane.label);
        for(int i = 0; i < len; i++) {
            float x = pos.x + i * stepWidth;
            ImVec2 cellMin(x + 1.0f, rowY + 1.0f);
            ImVec2 cellMax(x + stepWidth - 1.0f, rowY + rowHeight - 2.0f);
            drawList->AddRect(cellMin, cellMax, IM_COL32(48, 48, 52, 180));
            if((*lane.pattern)[i]) drawList->AddRectFilled(cellMin, cellMax, lane.color, 2.0f);
        }
    }
}

void polyphonicArpeggiatorGUI::drawArpGrid(float width, float height) const {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##ArpGrid", ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(18, 21, 24, 255), 5.0f);
    drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(72, 78, 84, 255), 5.0f);

    int steps = std::max(1, seqSize.get());
    std::vector<StepPreviewInfo> previewSteps;
    previewSteps.reserve(steps);

    int minNote = 127;
    int maxNote = 0;
    for(int step = 0; step < steps; step++) {
        StepPreviewInfo info = buildStepPreview(step);
        for(float note : info.notes) {
            int rounded = ofClamp(static_cast<int>(std::round(note)), 0, 127);
            minNote = std::min(minNote, rounded);
            maxNote = std::max(maxNote, rounded);
        }
        previewSteps.push_back(info);
    }

    if(minNote > maxNote) {
        minNote = 48;
        maxNote = 72;
    }
    if(maxNote - minNote < 7) maxNote = std::min(127, minNote + 7);

    float leftPad = 34.0f;
    float topPad = 20.0f;
    float bottomPad = 18.0f;
    float gridX = pos.x + leftPad;
    float gridY = pos.y + topPad;
    float gridW = std::max(1.0f, width - leftPad - 6.0f);
    float gridH = std::max(1.0f, height - topPad - bottomPad);
    float stepWidth = gridW / static_cast<float>(steps);
    float stepDurationMs = getVisualizationStepDurationMs();
    int noteSpan = std::max(1, maxNote - minNote + 1);
    float laneHeight = gridH / static_cast<float>(noteSpan);

    for(int note = minNote; note <= maxNote; note++) {
        float y = gridY + (maxNote - note) * laneHeight;
        ImU32 lineColor = isWhiteKey(note) ? IM_COL32(44, 48, 52, 140) : IM_COL32(32, 34, 38, 110);
        drawList->AddLine(ImVec2(gridX, y), ImVec2(gridX + gridW, y), lineColor);
    }

    for(int step = 0; step <= steps; step++) {
        float x = gridX + step * stepWidth;
        drawList->AddLine(ImVec2(x, gridY), ImVec2(x, gridY + gridH), IM_COL32(40, 44, 48, 150));
    }

    int playhead = wrapIndex(highlightedStep, steps);
    float playheadX = gridX + playhead * stepWidth;
    drawList->AddRectFilled(ImVec2(playheadX, gridY), ImVec2(playheadX + stepWidth, gridY + gridH), IM_COL32(255, 255, 255, 16));
    drawList->AddLine(ImVec2(playheadX, gridY), ImVec2(playheadX, gridY + gridH), IM_COL32(250, 240, 160, 210), 2.0f);

    for(int step = 0; step < steps; step++) {
        const StepPreviewInfo &info = previewSteps[step];
        if(!info.gate) continue;

        float velocityNorm = ofClamp(info.velocity, 0.0f, 1.0f);
        ImU32 baseColor = info.accent ? IM_COL32(255, 168, 84, static_cast<int>(90 + velocityNorm * 140))
                                      : IM_COL32(90, 220, 184, static_cast<int>(80 + velocityNorm * 120));
        bool hasVisibleStrum = strum.get() > 0.5f || strumRndm.get() > 0.5f;

        for(size_t voice = 0; voice < info.notes.size(); voice++) {
            int note = ofClamp(static_cast<int>(std::round(info.notes[voice])), 0, 127);
            float y = gridY + (maxNote - note) * laneHeight;
            float onsetOffset = 0.0f;
            if(hasVisibleStrum && info.notes.size() > 1) {
                float strumOffsetMs = computePreviewStrumOffset(step, static_cast<int>(voice), static_cast<int>(info.notes.size()));
                onsetOffset = ofClamp(strumOffsetMs / std::max(1.0f, stepDurationMs), 0.0f, 0.95f);
            }
            int noteDurationMs = voice < info.noteDurations.size() ? info.noteDurations[voice] : info.duration;
            float durationSteps = std::max(0.12f, static_cast<float>(noteDurationMs) / std::max(1.0f, stepDurationMs));
            float rectX = gridX + step * stepWidth + stepWidth * onsetOffset;
            float rectW = std::max(3.0f, stepWidth * durationSteps);
            drawList->AddRectFilled(ImVec2(rectX, y + 1.0f), ImVec2(std::min(gridX + gridW - 1.0f, rectX + rectW), y + laneHeight - 1.0f), baseColor, 2.0f);
            drawList->AddRect(ImVec2(rectX, y + 1.0f), ImVec2(std::min(gridX + gridW - 1.0f, rectX + rectW), y + laneHeight - 1.0f), IM_COL32(255, 255, 255, 34), 2.0f);
        }
    }

    drawList->AddText(ImVec2(pos.x + 6.0f, pos.y + 2.0f), IM_COL32(220, 220, 220, 220),
                      sourceMode.get() == Scale ? "Scale Source" : "Chord Pool Source");

    drawList->AddText(ImVec2(pos.x + 4.0f, gridY - 2.0f), IM_COL32(180, 180, 180, 180), ofToString(maxNote).c_str());
    drawList->AddText(ImVec2(pos.x + 4.0f, gridY + gridH - 14.0f), IM_COL32(180, 180, 180, 180), ofToString(minNote).c_str());
}

void polyphonicArpeggiatorGUI::onTrigger() {
    if(!internalClockMode.get()) {
        uint64_t now = ofGetElapsedTimeMillis();
        if(lastExternalTriggerTimeMs > 0 && now > lastExternalTriggerTimeMs) {
            recentExternalStepDurationsMs.push_back(static_cast<float>(now - lastExternalTriggerTimeMs));
            if(recentExternalStepDurationsMs.size() > 24) {
                recentExternalStepDurationsMs.erase(recentExternalStepDurationsMs.begin());
            }
        }
        lastExternalTriggerTimeMs = now;
    }

    if(shouldReset) {
        currentStep = 0;
        shouldReset = false;
        onsetCounter = 0;
        absoluteStepCounter = 0;
    }

    if(oneShotMode.get() && !oneShotCycleActive) {
        return;
    }

    absoluteStepCounter++;
    processStep();

    int stepAdvance = 1 + skipSteps.get();
    if(oneShotMode.get()) {
        oneShotStepsRemaining -= stepAdvance;
        if(oneShotStepsRemaining <= 0) {
            oneShotCycleActive = false;
            oneShotStepsRemaining = 0;
            updateOutputs();
            return;
        }
    }

    currentStep = wrapIndex(currentStep + stepAdvance, std::max(1, seqSize.get()));
}

void polyphonicArpeggiatorGUI::onReset() {
    clearActiveVoices(true);
    if(oneShotMode.get()) {
        oneShotCycleActive = true;
        oneShotStepsRemaining = std::max(1, seqSize.get());
    }
}

void polyphonicArpeggiatorGUI::onResetNext() {
    shouldReset = true;
}

void polyphonicArpeggiatorGUI::processStep() {
    int size = std::max(1, seqSize.get());
    if(!euclideanPattern.empty() && !euclideanPattern[wrapIndex(currentStep, static_cast<int>(euclideanPattern.size()))]) return;
    if(stepChance.get() < 1.0f && dist01(rng) > stepChance.get()) return;

    onsetCounter++;
    int accentIndex = accentOnsetMode.get() ? onsetCounter : absoluteStepCounter;
    int poly = std::min(polyphony.get(), MaxPolyphony);
    int interval = std::max(1, polyInterval.get());
    uint64_t now = ofGetElapsedTimeMillis();
    int patternOffset = getPatternOffsetForStepLive(currentStep);
    int baseRelativeOffset = patternOffset * std::max(0, sourceStride.get());
    int baseSourceIndex = sourceStart.get() + baseRelativeOffset;

    std::fill(stepGates.begin(), stepGates.end(), false);
    rebuildPitchSequence();

    int stepDuration = computeStepDuration(accentIndex);
    int randomizedStepDuration = durationRndPerStep.get() ? randomizeDurationValue(stepDuration) : stepDuration;
    float stepVelocity = computeStepVelocity(accentIndex);

    for(int voice = 0; voice < poly; voice++) {
        if(noteChance.get() < 1.0f && dist01(rng) > noteChance.get()) continue;

        int sourceIndex = baseSourceIndex + voice * interval;
        int outputIndex = wrapIndex(baseRelativeOffset + voice * interval, size);
        int noteDuration = durationRndPerStep.get() ? randomizedStepDuration : randomizeDurationValue(stepDuration);
        float pitch = getSourceValue(sourceIndex);
        if(outputIndex < static_cast<int>(deviationValues.size())) pitch += deviationValues[outputIndex];
        currentPitches[outputIndex] = ofClamp(pitch + transpose.get(), 0.0f, 127.0f);
        currentVelocities[outputIndex] = stepVelocity;
        currentDurations[outputIndex] = static_cast<float>(noteDuration);

        bool sustaining = currentGates[outputIndex] == 1 &&
                          noteStartTimes[outputIndex] > 0 &&
                          now < noteStartTimes[outputIndex] + static_cast<uint64_t>(std::max(1, noteDurationsMs[outputIndex]));

        if(!sustaining) {
            noteDurationsMs[outputIndex] = noteDuration;
            float strumOffset = computeStrumOffset(voice, poly);
            if(strumOffset <= 0.5f) {
                currentGates[outputIndex] = 1;
                stepGates[outputIndex] = true;
                noteStartTimes[outputIndex] = now;
                recordOutputHistoryEvent(currentPitches[outputIndex], currentVelocities[outputIndex], noteDurationsMs[outputIndex], now);
            } else {
                currentGates[outputIndex] = 0;
                noteStartTimes[outputIndex] = now + static_cast<uint64_t>(strumOffset);
            }
        }
    }

    highlightedStep = currentStep;
    updateOutputs();
}

void polyphonicArpeggiatorGUI::generateEuclideanPattern(std::vector<bool> &pattern, int length, int hits, int offset) {
    pattern.assign(std::max(0, length), false);
    if(length <= 0 || hits <= 0) return;

    hits = std::min(hits, length);
    for(int j = 0; j < hits; j++) {
        int index = ((j * length) / hits + offset) % length;
        if(index < 0) index += length;
        pattern[index] = true;
    }
}

void polyphonicArpeggiatorGUI::rebuildSourceMaterial() {
    activeSourceValues.clear();

    if(sourceMode.get() == Scale) {
        const auto &scaleValues = scale.get();
        if(scaleValues.empty()) {
            activeSourceValues.push_back(60.0f);
        } else {
            for(int octave = -2; octave <= 8; octave++) {
                for(float note : scaleValues) {
                    float expanded = note + static_cast<float>(octave * 12);
                    if(expanded >= 0.0f && expanded <= 127.0f) {
                        activeSourceValues.push_back(expanded);
                    }
                }
            }
            std::sort(activeSourceValues.begin(), activeSourceValues.end());
        }
    } else {
        activeSourceValues = notePoolIn.get();
        for(float &note : activeSourceValues) {
            note = ofClamp(note, 0.0f, 127.0f);
        }
        if(sortPool.get()) {
            std::stable_sort(activeSourceValues.begin(), activeSourceValues.end());
        }
        if(removeDuplicates.get()) {
            std::vector<float> uniqueValues;
            uniqueValues.reserve(activeSourceValues.size());
            for(float note : activeSourceValues) {
                bool alreadyPresent = false;
                for(float existing : uniqueValues) {
                    if(std::abs(existing - note) < 0.001f) {
                        alreadyPresent = true;
                        break;
                    }
                }
                if(!alreadyPresent) uniqueValues.push_back(note);
            }
            activeSourceValues = uniqueValues;
        }
        if(activeSourceValues.empty()) {
            activeSourceValues.push_back(60.0f);
        }
    }
}

void polyphonicArpeggiatorGUI::handleSourceMaterialChange() {
    std::vector<float> previous = activeSourceValues;
    rebuildSourceMaterial();

    bool changed = previous.size() != activeSourceValues.size();
    if(!changed) {
        for(size_t i = 0; i < previous.size(); i++) {
            if(std::abs(previous[i] - activeSourceValues[i]) > 0.001f) {
                changed = true;
                break;
            }
        }
    }

    if(changed) {
        std::fill(currentGates.begin(), currentGates.end(), 0);
        std::fill(noteStartTimes.begin(), noteStartTimes.end(), 0);
        if(sourceChangeMode.get() == ResetPattern) {
            currentStep = 0;
            highlightedStep = 0;
            onsetCounter = 0;
            absoluteStepCounter = 0;
            shouldReset = false;
        }
    }

    rebuildDeviations();
    rebuildPitchSequence();
    updateOutputs();
}

float polyphonicArpeggiatorGUI::getSourceValue(int index) const {
    if(activeSourceValues.empty()) return 60.0f;

    int sourceSize = static_cast<int>(activeSourceValues.size());
    int wrappedIndex = wrapIndex(index, sourceSize);
    float pitch = activeSourceValues[wrappedIndex];

    if(!pitchExpand.get() || sourceSize <= 0) {
        return pitch;
    }

    int cycle = 0;
    if(index >= 0) cycle = index / sourceSize;
    else cycle = -(((-index - 1) / sourceSize) + 1);

    return pitch + static_cast<float>(cycle * expandStep.get());
}

int polyphonicArpeggiatorGUI::getPatternOffsetForStepLive(int stepIndex) {
    int size = std::max(1, seqSize.get());
    if(patternMode.get() == 1) {
        return size - 1 - wrapIndex(stepIndex, size);
    }
    if(patternMode.get() == 2) {
        return std::uniform_int_distribution<int>(0, size - 1)(rng);
    }
    if(patternMode.get() == 3) {
        const auto &pattern = idxPattern.get();
        if(!pattern.empty()) return wrapIndex(pattern[wrapIndex(stepIndex, static_cast<int>(pattern.size()))], size);
        return 0;
    }
    return wrapIndex(stepIndex, size);
}

int polyphonicArpeggiatorGUI::getPatternOffsetForStepPreview(int stepIndex) const {
    int size = std::max(1, seqSize.get());
    if(patternMode.get() == 1) {
        return size - 1 - wrapIndex(stepIndex, size);
    }
    if(patternMode.get() == 2) {
        std::mt19937 previewRng(7919 + stepIndex * 131);
        return std::uniform_int_distribution<int>(0, size - 1)(previewRng);
    }
    if(patternMode.get() == 3) {
        const auto &pattern = idxPattern.get();
        if(!pattern.empty()) return wrapIndex(pattern[wrapIndex(stepIndex, static_cast<int>(pattern.size()))], size);
        return 0;
    }
    return wrapIndex(stepIndex, size);
}

void polyphonicArpeggiatorGUI::rebuildDeviations() {
    int size = std::max(1, seqSize.get());
    deviationValues.assign(size, 0.0f);

    for(int i = 0; i < size; i++) {
        float deviation = 0.0f;
        int sourceIndex = sourceStart.get() + i;

        if(octaveDev.get() > 0.0f && dist01(rng) < octaveDev.get()) {
            std::uniform_int_distribution<int> octDist(1, std::max(1, octaveDevRng.get()));
            deviation += octDist(rng) * 12.0f;
        }

        if(idxDev.get() > 0.0f && dist01(rng) < idxDev.get()) {
            std::uniform_int_distribution<int> idxDist(1, std::max(1, idxDevRng.get()));
            float basePitch = getSourceValue(sourceIndex);
            float shiftedPitch = getSourceValue(sourceIndex + idxDist(rng));
            deviation += shiftedPitch - basePitch;
        }

        if(pitchDev.get() > 0.0f && dist01(rng) < pitchDev.get()) {
            std::uniform_int_distribution<int> chromDist(1, std::max(1, pitchDevRng.get()));
            deviation += static_cast<float>(chromDist(rng));
        }

        deviationValues[i] = deviation;
    }
}

void polyphonicArpeggiatorGUI::rebuildPitchSequence() {
    int size = std::max(1, seqSize.get());
    if(static_cast<int>(currentPitches.size()) != size) resizeStateVectors(size);

    for(int i = 0; i < size; i++) {
        int sourceIndex = sourceStart.get() + i;
        float pitch = getSourceValue(sourceIndex);
        if(i < static_cast<int>(deviationValues.size())) pitch += deviationValues[i];
        currentPitches[i] = ofClamp(pitch + transpose.get(), 0.0f, 127.0f);
    }
}

void polyphonicArpeggiatorGUI::rebuildEuclideanOutputs() {
    int size = std::max(1, seqSize.get());
    std::vector<int> gateValues(size, 0);
    std::vector<int> accentValues(size, 0);
    std::vector<int> durationValues(size, 0);

    if(!euclideanPattern.empty()) {
        for(int i = 0; i < size; i++) gateValues[i] = euclideanPattern[wrapIndex(i, static_cast<int>(euclideanPattern.size()))] ? 1 : 0;
    }
    if(!euclideanAccents.empty()) {
        for(int i = 0; i < size; i++) accentValues[i] = euclideanAccents[wrapIndex(i, static_cast<int>(euclideanAccents.size()))] ? 1 : 0;
    }
    if(!euclideanDurations.empty()) {
        for(int i = 0; i < size; i++) durationValues[i] = euclideanDurations[wrapIndex(i, static_cast<int>(euclideanDurations.size()))] ? 1 : 0;
    }

    eucGateOut.set(gateValues);
    eucAccOut.set(accentValues);
    eucDurOut.set(durationValues);
}

float polyphonicArpeggiatorGUI::computeStepVelocity(int stepIndex) {
    float velocity = velBase.get();
    if(velRndm.get() > 0.0f) velocity += velRndm.get() * dist01(rng);
    if(!euclideanAccents.empty() && euclideanAccents[wrapIndex(stepIndex, static_cast<int>(euclideanAccents.size()))]) {
        velocity += eucAccStrength.get();
    }
    return ofClamp(velocity, 0.0f, 1.0f);
}

int polyphonicArpeggiatorGUI::computeStepDuration(int stepIndex) {
    int duration = durBase.get();
    if(!euclideanDurations.empty() && euclideanDurations[wrapIndex(stepIndex, static_cast<int>(euclideanDurations.size()))]) {
        duration += durEucStrength.get();
    }
    return ofClamp(duration, 1, 60000);
}

int polyphonicArpeggiatorGUI::randomizeDurationValue(int duration) {
    if(durRndm.get() > 0) duration += static_cast<int>(durRndm.get() * dist01(rng));
    return ofClamp(duration, 1, 60000);
}

float polyphonicArpeggiatorGUI::computePreviewVelocity(int stepIndex) const {
    float velocity = velBase.get() + velRndm.get() * 0.5f;
    if(!euclideanAccents.empty() && euclideanAccents[wrapIndex(stepIndex, static_cast<int>(euclideanAccents.size()))]) {
        velocity += eucAccStrength.get();
    }
    return ofClamp(velocity, 0.0f, 1.0f);
}

int polyphonicArpeggiatorGUI::computePreviewDuration(int stepIndex) const {
    int duration = durBase.get() + durRndm.get() / 2;
    if(!euclideanDurations.empty() && euclideanDurations[wrapIndex(stepIndex, static_cast<int>(euclideanDurations.size()))]) {
        duration += durEucStrength.get();
    }
    return ofClamp(duration, 1, 60000);
}

float polyphonicArpeggiatorGUI::computeStrumOffset(int voiceIndex, int totalVoices) {
    if(totalVoices <= 1 || strum.get() <= 0.0f) return 0.0f;

    float baseStrum = strum.get();
    if(strumRndm.get() > 0.0f) {
        float randomOffset = (dist01(rng) * 2.0f - 1.0f) * strumRndm.get();
        baseStrum = std::max(0.0f, baseStrum + randomOffset);
    }

    if(strumDir.get() == 0) return voiceIndex * baseStrum;
    if(strumDir.get() == 1) return (totalVoices - 1 - voiceIndex) * baseStrum;
    return dist01(rng) * (totalVoices - 1) * baseStrum;
}

float polyphonicArpeggiatorGUI::computePreviewStrumOffset(int stepIndex, int voiceIndex, int totalVoices) const {
    if(totalVoices <= 1 || strum.get() <= 0.0f) return 0.0f;

    float baseStrum = strum.get();
    if(strumRndm.get() > 0.0f) {
        std::mt19937 previewRng(9901 + stepIndex * 131 + voiceIndex * 17 + totalVoices * 7);
        float randomUnit = std::uniform_real_distribution<float>(0.0f, 1.0f)(previewRng);
        float randomOffset = (randomUnit * 2.0f - 1.0f) * strumRndm.get();
        baseStrum = std::max(0.0f, baseStrum + randomOffset);
    }

    if(strumDir.get() == 0) return voiceIndex * baseStrum;
    if(strumDir.get() == 1) return (totalVoices - 1 - voiceIndex) * baseStrum;

    std::mt19937 previewRng(10427 + stepIndex * 149 + voiceIndex * 23 + totalVoices * 11);
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(previewRng) * (totalVoices - 1) * baseStrum;
}

polyphonicArpeggiatorGUI::StepPreviewInfo polyphonicArpeggiatorGUI::buildStepPreview(int stepIndex) const {
    StepPreviewInfo info;
    info.gate = euclideanPattern.empty() || euclideanPattern[wrapIndex(stepIndex, static_cast<int>(euclideanPattern.size()))];
    info.accent = !euclideanAccents.empty() && euclideanAccents[wrapIndex(stepIndex, static_cast<int>(euclideanAccents.size()))];
    info.velocity = computePreviewVelocity(stepIndex);
    info.duration = computePreviewDuration(stepIndex);

    if(!info.gate) return info;

    int size = std::max(1, seqSize.get());
    int poly = std::min(polyphony.get(), MaxPolyphony);
    int interval = std::max(1, polyInterval.get());

    int patternOffset = getPatternOffsetForStepPreview(stepIndex);
    int baseRelativeOffset = patternOffset * std::max(0, sourceStride.get());
    int baseSourceIndex = sourceStart.get() + baseRelativeOffset;
    int baseDuration = durBase.get();
    if(!euclideanDurations.empty() && euclideanDurations[wrapIndex(stepIndex, static_cast<int>(euclideanDurations.size()))]) {
        baseDuration += durEucStrength.get();
    }
    baseDuration = ofClamp(baseDuration, 1, 60000);
    std::mt19937 durationRng(4253 + stepIndex * 197);
    int sharedDuration = baseDuration;
    if(durationRndPerStep.get() && durRndm.get() > 0) {
        sharedDuration = ofClamp(baseDuration + static_cast<int>(durRndm.get() * std::uniform_real_distribution<float>(0.0f, 1.0f)(durationRng)), 1, 60000);
    }

    for(int voice = 0; voice < poly; voice++) {
        int outputIndex = wrapIndex(baseRelativeOffset + voice * interval, size);
        float pitch = getSourceValue(baseSourceIndex + voice * interval);
        if(outputIndex < static_cast<int>(deviationValues.size())) pitch += deviationValues[outputIndex];
        info.notes.push_back(ofClamp(pitch + transpose.get(), 0.0f, 127.0f));
        int previewDuration = sharedDuration;
        if(!durationRndPerStep.get() && durRndm.get() > 0) {
            previewDuration = ofClamp(baseDuration + static_cast<int>(durRndm.get() * std::uniform_real_distribution<float>(0.0f, 1.0f)(durationRng)), 1, 60000);
        }
        info.noteDurations.push_back(previewDuration);
    }

    if(!info.noteDurations.empty()) info.duration = info.noteDurations.front();

    return info;
}

void polyphonicArpeggiatorGUI::updateOutputs() {
    pitchOut.set(currentPitches);
    velocityOut.set(currentVelocities);
    durOut.set(currentDurations);

    std::vector<float> gateVel(currentGates.size(), 0.0f);
    for(size_t i = 0; i < currentGates.size(); i++) {
        gateVel[i] = currentGates[i] * currentVelocities[i];
    }

    gateVelOut.set(gateVel);
    gateOut.set(currentGates);
}

void polyphonicArpeggiatorGUI::recordOutputHistoryEvent(float pitch, float velocity, int durationMs, uint64_t startTimeMs) {
    OutputHistoryEvent event;
    event.pitch = ofClamp(pitch, 0.0f, 127.0f);
    event.velocity = ofClamp(velocity, 0.0f, 1.0f);
    event.durationMs = std::max(1, durationMs);
    event.startTimeMs = startTimeMs;
    outputHistory.push_back(event);
}

void polyphonicArpeggiatorGUI::pruneOutputHistory(uint64_t nowMs) {
    uint64_t windowMs = static_cast<uint64_t>(std::max(1, outputHistoryWindowMs.get()));
    uint64_t keepStart = nowMs > windowMs * 2 ? nowMs - windowMs * 2 : 0;
    outputHistory.erase(std::remove_if(outputHistory.begin(), outputHistory.end(),
                                       [keepStart](const OutputHistoryEvent &event) {
                                           uint64_t eventEnd = event.startTimeMs + static_cast<uint64_t>(std::max(1, event.durationMs));
                                           return eventEnd < keepStart;
                                       }),
                        outputHistory.end());
}

float polyphonicArpeggiatorGUI::getVisualizationStepDurationMs() const {
    if(!internalClockMode.get() && !recentExternalStepDurationsMs.empty()) {
        float sum = 0.0f;
        for(float durationMs : recentExternalStepDurationsMs) sum += durationMs;
        return std::max(1.0f, sum / static_cast<float>(recentExternalStepDurationsMs.size()));
    }

    return 60000.0f / (std::max(1.0f, currentBpm) * std::max(0.001f, beatDiv.get()));
}

std::vector<float> polyphonicArpeggiatorGUI::getSourcePreviewNotes() const {
    if(sourceMode.get() == ChordPool) return activeSourceValues;

    std::vector<float> preview;
    preview.reserve(scale.get().size());
    for(float value : scale.get()) preview.push_back(ofClamp(value + 60.0f, 0.0f, 127.0f));
    return preview;
}

std::vector<float> polyphonicArpeggiatorGUI::getActiveOutputPreviewNotes() const {
    std::vector<float> notes;
    for(size_t i = 0; i < currentPitches.size() && i < currentGates.size(); i++) {
        if(currentGates[i] > 0) notes.push_back(currentPitches[i]);
    }
    if(notes.empty()) {
        StepPreviewInfo info = buildStepPreview(wrapIndex(highlightedStep, std::max(1, seqSize.get())));
        notes = info.notes;
    }
    if(notes.empty()) notes = currentPitches;
    return notes;
}

std::string polyphonicArpeggiatorGUI::summarizeVector(const std::vector<float> &values, int maxItems, int precision) const {
    if(values.empty()) return "[]";

    std::ostringstream stream;
    stream << "[";
    int count = std::min(static_cast<int>(values.size()), maxItems);
    for(int i = 0; i < count; i++) {
        if(i > 0) stream << ", ";
        stream << std::fixed << std::setprecision(precision) << values[i];
    }
    if(static_cast<int>(values.size()) > count) stream << ", ...";
    stream << "]";
    return stream.str();
}

std::string polyphonicArpeggiatorGUI::summarizeIntVector(const std::vector<int> &values, int maxItems) const {
    if(values.empty()) return "[]";

    std::ostringstream stream;
    stream << "[";
    int count = std::min(static_cast<int>(values.size()), maxItems);
    for(int i = 0; i < count; i++) {
        if(i > 0) stream << ", ";
        stream << values[i];
    }
    if(static_cast<int>(values.size()) > count) stream << ", ...";
    stream << "]";
    return stream.str();
}

std::string polyphonicArpeggiatorGUI::getSnapshotsFolderPath() const {
    return ofToDataPath("nodeSnapshots/PolyphonicArpeggiatorGUI/", true);
}

std::string polyphonicArpeggiatorGUI::getSnapshotFilePath(int slot) const {
    return getSnapshotsFolderPath() + "snapshot_" + ofToString(slot) + ".json";
}

void polyphonicArpeggiatorGUI::saveSnapshotToDisk(int slot) const {
    if(slot < 0 || slot >= SnapshotSlots || !snapshotSlots[slot].hasData) return;

    ofDirectory dir(getSnapshotsFolderPath());
    if(!dir.exists()) dir.create(true);

    const auto &snap = snapshotSlots[slot];
    ofJson json;
    json["name"] = snap.name;
    json["sourceMode"] = snap.sourceMode;
    json["internalClockMode"] = snap.internalClockMode;
    json["oneShotMode"] = snap.oneShotMode;
    json["beatDiv"] = snap.beatDiv;
    json["seqSize"] = snap.seqSize;
    json["scale"] = snap.scale;
    json["patternMode"] = snap.patternMode;
    json["idxPattern"] = snap.idxPattern;
    json["sourceStart"] = snap.sourceStart;
    json["sourceStride"] = snap.sourceStride;
    json["pitchExpand"] = snap.pitchExpand;
    json["expandStep"] = snap.expandStep;
    json["transpose"] = snap.transpose;
    json["sortPool"] = snap.sortPool;
    json["removeDuplicates"] = snap.removeDuplicates;
    json["sourceChangeMode"] = snap.sourceChangeMode;
    json["polyphony"] = snap.polyphony;
    json["polyInterval"] = snap.polyInterval;
    json["skipSteps"] = snap.skipSteps;
    json["strum"] = snap.strum;
    json["strumRndm"] = snap.strumRndm;
    json["strumDir"] = snap.strumDir;
    json["octaveDev"] = snap.octaveDev;
    json["octaveDevRng"] = snap.octaveDevRng;
    json["idxDev"] = snap.idxDev;
    json["idxDevRng"] = snap.idxDevRng;
    json["pitchDev"] = snap.pitchDev;
    json["pitchDevRng"] = snap.pitchDevRng;
    json["velBase"] = snap.velBase;
    json["velRndm"] = snap.velRndm;
    json["eucAccStrength"] = snap.eucAccStrength;
    json["durBase"] = snap.durBase;
    json["durRndm"] = snap.durRndm;
    json["durEucStrength"] = snap.durEucStrength;
    json["durationRndPerStep"] = snap.durationRndPerStep;
    json["outputHistoryWindowMs"] = snap.outputHistoryWindowMs;
    json["eucLen"] = snap.eucLen;
    json["eucHits"] = snap.eucHits;
    json["eucOff"] = snap.eucOff;
    json["eucAccLen"] = snap.eucAccLen;
    json["eucAccHits"] = snap.eucAccHits;
    json["eucAccOff"] = snap.eucAccOff;
    json["eucDurLen"] = snap.eucDurLen;
    json["eucDurHits"] = snap.eucDurHits;
    json["eucDurOff"] = snap.eucDurOff;
    json["stepChance"] = snap.stepChance;
    json["noteChance"] = snap.noteChance;
    json["dynamicMode"] = snap.dynamicMode;
    json["accentOnsetMode"] = snap.accentOnsetMode;

    ofSavePrettyJson(getSnapshotFilePath(slot), json);
}

void polyphonicArpeggiatorGUI::loadSnapshotFromDisk(int slot) {
    if(slot < 0 || slot >= SnapshotSlots) return;
    ofFile file(getSnapshotFilePath(slot));
    if(!file.exists()) return;

    ofJson json = ofLoadJson(file.getAbsolutePath());
    if(json.empty()) return;

    auto &snap = snapshotSlots[slot];
    snap = polyphonicArpeggiatorGUISnapshot();
    snap.name = json.value("name", "Snapshot " + ofToString(slot + 1));
    snap.sourceMode = json.value("sourceMode", Scale);
    snap.internalClockMode = json.value("internalClockMode", false);
    snap.oneShotMode = json.value("oneShotMode", false);
    snap.beatDiv = json.value("beatDiv", 1.0f);
    snap.seqSize = json.value("seqSize", 16);
    snap.scale = json.value("scale", std::vector<float>{0, 2, 4, 5, 7, 9, 11});
    snap.patternMode = json.value("patternMode", 0);
    snap.idxPattern = json.value("idxPattern", std::vector<int>{0, 1, 2, 3});
    snap.sourceStart = json.value("sourceStart", 0);
    snap.sourceStride = json.value("sourceStride", 1);
    snap.pitchExpand = json.value("pitchExpand", false);
    snap.expandStep = json.value("expandStep", 12);
    snap.transpose = json.value("transpose", 0);
    snap.sortPool = json.value("sortPool", true);
    snap.removeDuplicates = json.value("removeDuplicates", false);
    snap.sourceChangeMode = json.value("sourceChangeMode", KeepPhase);
    snap.polyphony = json.value("polyphony", 1);
    snap.polyInterval = json.value("polyInterval", 2);
    snap.skipSteps = json.value("skipSteps", 0);
    snap.strum = json.value("strum", 0.0f);
    snap.strumRndm = json.value("strumRndm", 0.0f);
    snap.strumDir = json.value("strumDir", 0);
    snap.octaveDev = json.value("octaveDev", 0.0f);
    snap.octaveDevRng = json.value("octaveDevRng", 1);
    snap.idxDev = json.value("idxDev", 0.0f);
    snap.idxDevRng = json.value("idxDevRng", 2);
    snap.pitchDev = json.value("pitchDev", 0.0f);
    snap.pitchDevRng = json.value("pitchDevRng", 2);
    snap.velBase = json.value("velBase", 0.8f);
    snap.velRndm = json.value("velRndm", 0.1f);
    snap.eucAccStrength = json.value("eucAccStrength", 0.2f);
    snap.durBase = json.value("durBase", 100);
    snap.durRndm = json.value("durRndm", 20);
    snap.durEucStrength = json.value("durEucStrength", 50);
    snap.durationRndPerStep = json.value("durationRndPerStep", true);
    snap.outputHistoryWindowMs = json.value("outputHistoryWindowMs", 4000);
    snap.eucLen = json.value("eucLen", 8);
    snap.eucHits = json.value("eucHits", 8);
    snap.eucOff = json.value("eucOff", 0);
    snap.eucAccLen = json.value("eucAccLen", 4);
    snap.eucAccHits = json.value("eucAccHits", 1);
    snap.eucAccOff = json.value("eucAccOff", 0);
    snap.eucDurLen = json.value("eucDurLen", 4);
    snap.eucDurHits = json.value("eucDurHits", 4);
    snap.eucDurOff = json.value("eucDurOff", 0);
    snap.stepChance = json.value("stepChance", 1.0f);
    snap.noteChance = json.value("noteChance", 1.0f);
    snap.dynamicMode = json.value("dynamicMode", false);
    snap.accentOnsetMode = json.value("accentOnsetMode", true);
    snap.hasData = true;
}

void polyphonicArpeggiatorGUI::loadAllSnapshotsFromDisk() {
    for(auto &slot : snapshotSlots) slot = polyphonicArpeggiatorGUISnapshot();
    for(int i = 0; i < SnapshotSlots; i++) loadSnapshotFromDisk(i);
}

void polyphonicArpeggiatorGUI::deleteSnapshotFromDisk(int slot) {
    if(slot < 0 || slot >= SnapshotSlots) return;

    ofFile file(getSnapshotFilePath(slot));
    if(file.exists()) file.remove();
    snapshotSlots[slot] = polyphonicArpeggiatorGUISnapshot();
    if(activeSnapshotSlot == slot) activeSnapshotSlot = -1;
}

void polyphonicArpeggiatorGUI::storeToSlot(int slot) {
    if(slot < 0 || slot >= SnapshotSlots) return;

    auto &snap = snapshotSlots[slot];
    std::string existingName = snap.name;
    snap = polyphonicArpeggiatorGUISnapshot();
    snap.name = existingName.empty() ? "Snapshot " + ofToString(slot + 1) : existingName;
    snap.sourceMode = sourceMode.get();
    snap.internalClockMode = internalClockMode.get();
    snap.oneShotMode = oneShotMode.get();
    snap.beatDiv = beatDiv.get();
    snap.seqSize = seqSize.get();
    snap.scale = scale.get();
    snap.patternMode = patternMode.get();
    snap.idxPattern = idxPattern.get();
    snap.sourceStart = sourceStart.get();
    snap.sourceStride = sourceStride.get();
    snap.pitchExpand = pitchExpand.get();
    snap.expandStep = expandStep.get();
    snap.transpose = transpose.get();
    snap.sortPool = sortPool.get();
    snap.removeDuplicates = removeDuplicates.get();
    snap.sourceChangeMode = sourceChangeMode.get();
    snap.polyphony = polyphony.get();
    snap.polyInterval = polyInterval.get();
    snap.skipSteps = skipSteps.get();
    snap.strum = strum.get();
    snap.strumRndm = strumRndm.get();
    snap.strumDir = strumDir.get();
    snap.octaveDev = octaveDev.get();
    snap.octaveDevRng = octaveDevRng.get();
    snap.idxDev = idxDev.get();
    snap.idxDevRng = idxDevRng.get();
    snap.pitchDev = pitchDev.get();
    snap.pitchDevRng = pitchDevRng.get();
    snap.velBase = velBase.get();
    snap.velRndm = velRndm.get();
    snap.eucAccStrength = eucAccStrength.get();
    snap.durBase = durBase.get();
    snap.durRndm = durRndm.get();
    snap.durEucStrength = durEucStrength.get();
    snap.durationRndPerStep = durationRndPerStep.get();
    snap.outputHistoryWindowMs = outputHistoryWindowMs.get();
    snap.eucLen = eucLen.get();
    snap.eucHits = eucHits.get();
    snap.eucOff = eucOff.get();
    snap.eucAccLen = eucAccLen.get();
    snap.eucAccHits = eucAccHits.get();
    snap.eucAccOff = eucAccOff.get();
    snap.eucDurLen = eucDurLen.get();
    snap.eucDurHits = eucDurHits.get();
    snap.eucDurOff = eucDurOff.get();
    snap.stepChance = stepChance.get();
    snap.noteChance = noteChance.get();
    snap.dynamicMode = dynamicMode.get();
    snap.accentOnsetMode = accentOnsetMode.get();
    snap.hasData = true;

    activeSnapshotSlot = slot;
    saveSnapshotToDisk(slot);
}

void polyphonicArpeggiatorGUI::recallSlot(int slot) {
    if(slot < 0 || slot >= SnapshotSlots || !snapshotSlots[slot].hasData) return;

    activeSnapshotSlot = slot;
    if(morphTime.get() <= 0.001f) {
        const auto &snap = snapshotSlots[slot];
        sourceMode = snap.sourceMode;
        internalClockMode = snap.internalClockMode;
        oneShotMode = snap.oneShotMode;
        beatDiv = snap.beatDiv;
        seqSize = snap.seqSize;
        scale = snap.scale;
        patternMode = snap.patternMode;
        idxPattern = snap.idxPattern;
        sourceStart = snap.sourceStart;
        sourceStride = snap.sourceStride;
        pitchExpand = snap.pitchExpand;
        expandStep = snap.expandStep;
        transpose = snap.transpose;
        sortPool = snap.sortPool;
        removeDuplicates = snap.removeDuplicates;
        sourceChangeMode = snap.sourceChangeMode;
        polyphony = snap.polyphony;
        polyInterval = snap.polyInterval;
        skipSteps = snap.skipSteps;
        strum = snap.strum;
        strumRndm = snap.strumRndm;
        strumDir = snap.strumDir;
        octaveDev = snap.octaveDev;
        octaveDevRng = snap.octaveDevRng;
        idxDev = snap.idxDev;
        idxDevRng = snap.idxDevRng;
        pitchDev = snap.pitchDev;
        pitchDevRng = snap.pitchDevRng;
        velBase = snap.velBase;
        velRndm = snap.velRndm;
        eucAccStrength = snap.eucAccStrength;
        durBase = snap.durBase;
        durRndm = snap.durRndm;
        durEucStrength = snap.durEucStrength;
        durationRndPerStep = snap.durationRndPerStep;
        outputHistoryWindowMs = snap.outputHistoryWindowMs;
        eucLen = snap.eucLen;
        eucHits = snap.eucHits;
        eucOff = snap.eucOff;
        eucAccLen = snap.eucAccLen;
        eucAccHits = snap.eucAccHits;
        eucAccOff = snap.eucAccOff;
        eucDurLen = snap.eucDurLen;
        eucDurHits = snap.eucDurHits;
        eucDurOff = snap.eucDurOff;
        stepChance = snap.stepChance;
        noteChance = snap.noteChance;
        dynamicMode = snap.dynamicMode;
        accentOnsetMode = snap.accentOnsetMode;
        syncUserPatternToSequenceSize();
        internalClockNeedsSync = true;
    } else {
        startSnapshot = polyphonicArpeggiatorGUISnapshot();
        startSnapshot.sourceMode = sourceMode.get();
        startSnapshot.internalClockMode = internalClockMode.get();
        startSnapshot.oneShotMode = oneShotMode.get();
        startSnapshot.beatDiv = beatDiv.get();
        startSnapshot.seqSize = seqSize.get();
        startSnapshot.scale = scale.get();
        startSnapshot.patternMode = patternMode.get();
        startSnapshot.idxPattern = idxPattern.get();
        startSnapshot.sourceStart = sourceStart.get();
        startSnapshot.sourceStride = sourceStride.get();
        startSnapshot.pitchExpand = pitchExpand.get();
        startSnapshot.expandStep = expandStep.get();
        startSnapshot.transpose = transpose.get();
        startSnapshot.sortPool = sortPool.get();
        startSnapshot.removeDuplicates = removeDuplicates.get();
        startSnapshot.sourceChangeMode = sourceChangeMode.get();
        startSnapshot.polyphony = polyphony.get();
        startSnapshot.polyInterval = polyInterval.get();
        startSnapshot.skipSteps = skipSteps.get();
        startSnapshot.strum = strum.get();
        startSnapshot.strumRndm = strumRndm.get();
        startSnapshot.strumDir = strumDir.get();
        startSnapshot.octaveDev = octaveDev.get();
        startSnapshot.octaveDevRng = octaveDevRng.get();
        startSnapshot.idxDev = idxDev.get();
        startSnapshot.idxDevRng = idxDevRng.get();
        startSnapshot.pitchDev = pitchDev.get();
        startSnapshot.pitchDevRng = pitchDevRng.get();
        startSnapshot.velBase = velBase.get();
        startSnapshot.velRndm = velRndm.get();
        startSnapshot.eucAccStrength = eucAccStrength.get();
        startSnapshot.durBase = durBase.get();
        startSnapshot.durRndm = durRndm.get();
        startSnapshot.durEucStrength = durEucStrength.get();
        startSnapshot.durationRndPerStep = durationRndPerStep.get();
        startSnapshot.outputHistoryWindowMs = outputHistoryWindowMs.get();
        startSnapshot.eucLen = eucLen.get();
        startSnapshot.eucHits = eucHits.get();
        startSnapshot.eucOff = eucOff.get();
        startSnapshot.eucAccLen = eucAccLen.get();
        startSnapshot.eucAccHits = eucAccHits.get();
        startSnapshot.eucAccOff = eucAccOff.get();
        startSnapshot.eucDurLen = eucDurLen.get();
        startSnapshot.eucDurHits = eucDurHits.get();
        startSnapshot.eucDurOff = eucDurOff.get();
        startSnapshot.stepChance = stepChance.get();
        startSnapshot.noteChance = noteChance.get();
        startSnapshot.dynamicMode = dynamicMode.get();
        startSnapshot.accentOnsetMode = accentOnsetMode.get();

        targetSnapshot = snapshotSlots[slot];
        morphStartTime = ofGetElapsedTimef();
        isMorphing = true;
    }
}

void polyphonicArpeggiatorGUI::updateMorph() {
    float progress = (ofGetElapsedTimef() - morphStartTime) / std::max(0.001f, morphTime.get());
    if(progress >= 1.0f) {
        progress = 1.0f;
        isMorphing = false;
    }

    seqSize = static_cast<int>(ofLerp(startSnapshot.seqSize, targetSnapshot.seqSize, progress));
    beatDiv = ofLerp(startSnapshot.beatDiv, targetSnapshot.beatDiv, progress);
    sourceStart = static_cast<int>(ofLerp(startSnapshot.sourceStart, targetSnapshot.sourceStart, progress));
    sourceStride = static_cast<int>(ofLerp(startSnapshot.sourceStride, targetSnapshot.sourceStride, progress));
    expandStep = static_cast<int>(ofLerp(startSnapshot.expandStep, targetSnapshot.expandStep, progress));
    transpose = static_cast<int>(ofLerp(startSnapshot.transpose, targetSnapshot.transpose, progress));
    polyphony = static_cast<int>(ofLerp(startSnapshot.polyphony, targetSnapshot.polyphony, progress));
    polyInterval = static_cast<int>(ofLerp(startSnapshot.polyInterval, targetSnapshot.polyInterval, progress));
    skipSteps = static_cast<int>(ofLerp(startSnapshot.skipSteps, targetSnapshot.skipSteps, progress));
    strum = ofLerp(startSnapshot.strum, targetSnapshot.strum, progress);
    strumRndm = ofLerp(startSnapshot.strumRndm, targetSnapshot.strumRndm, progress);
    octaveDev = ofLerp(startSnapshot.octaveDev, targetSnapshot.octaveDev, progress);
    octaveDevRng = static_cast<int>(ofLerp(startSnapshot.octaveDevRng, targetSnapshot.octaveDevRng, progress));
    idxDev = ofLerp(startSnapshot.idxDev, targetSnapshot.idxDev, progress);
    idxDevRng = static_cast<int>(ofLerp(startSnapshot.idxDevRng, targetSnapshot.idxDevRng, progress));
    pitchDev = ofLerp(startSnapshot.pitchDev, targetSnapshot.pitchDev, progress);
    pitchDevRng = static_cast<int>(ofLerp(startSnapshot.pitchDevRng, targetSnapshot.pitchDevRng, progress));
    velBase = ofLerp(startSnapshot.velBase, targetSnapshot.velBase, progress);
    velRndm = ofLerp(startSnapshot.velRndm, targetSnapshot.velRndm, progress);
    eucAccStrength = ofLerp(startSnapshot.eucAccStrength, targetSnapshot.eucAccStrength, progress);
    durBase = static_cast<int>(ofLerp(startSnapshot.durBase, targetSnapshot.durBase, progress));
    durRndm = static_cast<int>(ofLerp(startSnapshot.durRndm, targetSnapshot.durRndm, progress));
    durEucStrength = static_cast<int>(ofLerp(startSnapshot.durEucStrength, targetSnapshot.durEucStrength, progress));
    outputHistoryWindowMs = static_cast<int>(ofLerp(startSnapshot.outputHistoryWindowMs, targetSnapshot.outputHistoryWindowMs, progress));
    eucLen = static_cast<int>(ofLerp(startSnapshot.eucLen, targetSnapshot.eucLen, progress));
    eucHits = static_cast<int>(ofLerp(startSnapshot.eucHits, targetSnapshot.eucHits, progress));
    eucOff = static_cast<int>(ofLerp(startSnapshot.eucOff, targetSnapshot.eucOff, progress));
    eucAccLen = static_cast<int>(ofLerp(startSnapshot.eucAccLen, targetSnapshot.eucAccLen, progress));
    eucAccHits = static_cast<int>(ofLerp(startSnapshot.eucAccHits, targetSnapshot.eucAccHits, progress));
    eucAccOff = static_cast<int>(ofLerp(startSnapshot.eucAccOff, targetSnapshot.eucAccOff, progress));
    eucDurLen = static_cast<int>(ofLerp(startSnapshot.eucDurLen, targetSnapshot.eucDurLen, progress));
    eucDurHits = static_cast<int>(ofLerp(startSnapshot.eucDurHits, targetSnapshot.eucDurHits, progress));
    eucDurOff = static_cast<int>(ofLerp(startSnapshot.eucDurOff, targetSnapshot.eucDurOff, progress));
    stepChance = ofLerp(startSnapshot.stepChance, targetSnapshot.stepChance, progress);
    noteChance = ofLerp(startSnapshot.noteChance, targetSnapshot.noteChance, progress);

    if(progress >= 1.0f) {
        sourceMode = targetSnapshot.sourceMode;
        internalClockMode = targetSnapshot.internalClockMode;
        oneShotMode = targetSnapshot.oneShotMode;
        scale = targetSnapshot.scale;
        patternMode = targetSnapshot.patternMode;
        idxPattern = targetSnapshot.idxPattern;
        beatDiv = targetSnapshot.beatDiv;
        pitchExpand = targetSnapshot.pitchExpand;
        expandStep = targetSnapshot.expandStep;
        sortPool = targetSnapshot.sortPool;
        removeDuplicates = targetSnapshot.removeDuplicates;
        sourceChangeMode = targetSnapshot.sourceChangeMode;
        strumDir = targetSnapshot.strumDir;
        dynamicMode = targetSnapshot.dynamicMode;
        accentOnsetMode = targetSnapshot.accentOnsetMode;
        durationRndPerStep = targetSnapshot.durationRndPerStep;
        outputHistoryWindowMs = targetSnapshot.outputHistoryWindowMs;
        syncUserPatternToSequenceSize();
        internalClockNeedsSync = true;
    }
}

void polyphonicArpeggiatorGUI::presetSave(ofJson &json) {
    json["currentStep"] = currentStep;
    json["highlightedStep"] = highlightedStep;
    json["activeSnapshotSlot"] = activeSnapshotSlot;

    ofJson state;
    state["showEditor"] = showEditor.get();
    state["editorWidth"] = editorWidth.get();
    state["editorHeight"] = editorHeight.get();
    state["sourceMode"] = sourceMode.get();
    state["internalClockMode"] = internalClockMode.get();
    state["oneShotMode"] = oneShotMode.get();
    state["beatDiv"] = beatDiv.get();
    state["sortPool"] = sortPool.get();
    state["removeDuplicates"] = removeDuplicates.get();
    state["sourceChangeMode"] = sourceChangeMode.get();
    state["patternMode"] = patternMode.get();
    state["idxPattern"] = idxPattern.get();
    state["seqSize"] = seqSize.get();
    state["sourceStart"] = sourceStart.get();
    state["sourceStride"] = sourceStride.get();
    state["pitchExpand"] = pitchExpand.get();
    state["expandStep"] = expandStep.get();
    state["transpose"] = transpose.get();
    state["dynamicMode"] = dynamicMode.get();
    state["accentOnsetMode"] = accentOnsetMode.get();
    state["polyphony"] = polyphony.get();
    state["polyInterval"] = polyInterval.get();
    state["skipSteps"] = skipSteps.get();
    state["strum"] = strum.get();
    state["strumRndm"] = strumRndm.get();
    state["strumDir"] = strumDir.get();
    state["octaveDev"] = octaveDev.get();
    state["octaveDevRng"] = octaveDevRng.get();
    state["idxDev"] = idxDev.get();
    state["idxDevRng"] = idxDevRng.get();
    state["pitchDev"] = pitchDev.get();
    state["pitchDevRng"] = pitchDevRng.get();
    state["eucLen"] = eucLen.get();
    state["eucHits"] = eucHits.get();
    state["eucOff"] = eucOff.get();
    state["stepChance"] = stepChance.get();
    state["noteChance"] = noteChance.get();
    state["velBase"] = velBase.get();
    state["velRndm"] = velRndm.get();
    state["eucAccLen"] = eucAccLen.get();
    state["eucAccHits"] = eucAccHits.get();
    state["eucAccOff"] = eucAccOff.get();
    state["eucAccStrength"] = eucAccStrength.get();
    state["durBase"] = durBase.get();
    state["durRndm"] = durRndm.get();
    state["eucDurLen"] = eucDurLen.get();
    state["eucDurHits"] = eucDurHits.get();
    state["eucDurOff"] = eucDurOff.get();
    state["durEucStrength"] = durEucStrength.get();
    state["durationRndPerStep"] = durationRndPerStep.get();
    state["outputHistoryWindowMs"] = outputHistoryWindowMs.get();
    state["morphTime"] = morphTime.get();
    json["editorState"] = state;
}

void polyphonicArpeggiatorGUI::presetRecallAfterSettingParameters(ofJson &json) {
    currentStep = json.value("currentStep", 0);
    highlightedStep = json.value("highlightedStep", currentStep);
    activeSnapshotSlot = json.value("activeSnapshotSlot", -1);

    if(json.contains("editorState") && json["editorState"].is_object()) {
        const ofJson &state = json["editorState"];
        showEditor = state.value("showEditor", showEditor.get());
        editorWidth = state.value("editorWidth", editorWidth.get());
        editorHeight = state.value("editorHeight", editorHeight.get());
        sourceMode = state.value("sourceMode", sourceMode.get());
        internalClockMode = state.value("internalClockMode", internalClockMode.get());
        oneShotMode = state.value("oneShotMode", oneShotMode.get());
        beatDiv = state.value("beatDiv", beatDiv.get());
        sortPool = state.value("sortPool", sortPool.get());
        removeDuplicates = state.value("removeDuplicates", removeDuplicates.get());
        sourceChangeMode = state.value("sourceChangeMode", sourceChangeMode.get());
        patternMode = state.value("patternMode", patternMode.get());
        seqSize = state.value("seqSize", seqSize.get());
        idxPattern = state.value("idxPattern", idxPattern.get());
        sourceStart = state.value("sourceStart", sourceStart.get());
        sourceStride = state.value("sourceStride", sourceStride.get());
        pitchExpand = state.value("pitchExpand", pitchExpand.get());
        expandStep = state.value("expandStep", expandStep.get());
        transpose = state.value("transpose", transpose.get());
        dynamicMode = state.value("dynamicMode", dynamicMode.get());
        accentOnsetMode = state.value("accentOnsetMode", accentOnsetMode.get());
        polyphony = state.value("polyphony", polyphony.get());
        polyInterval = state.value("polyInterval", polyInterval.get());
        skipSteps = state.value("skipSteps", skipSteps.get());
        strum = state.value("strum", strum.get());
        strumRndm = state.value("strumRndm", strumRndm.get());
        strumDir = state.value("strumDir", strumDir.get());
        octaveDev = state.value("octaveDev", octaveDev.get());
        octaveDevRng = state.value("octaveDevRng", octaveDevRng.get());
        idxDev = state.value("idxDev", idxDev.get());
        idxDevRng = state.value("idxDevRng", idxDevRng.get());
        pitchDev = state.value("pitchDev", pitchDev.get());
        pitchDevRng = state.value("pitchDevRng", pitchDevRng.get());
        eucLen = state.value("eucLen", eucLen.get());
        eucHits = state.value("eucHits", eucHits.get());
        eucOff = state.value("eucOff", eucOff.get());
        stepChance = state.value("stepChance", stepChance.get());
        noteChance = state.value("noteChance", noteChance.get());
        velBase = state.value("velBase", velBase.get());
        velRndm = state.value("velRndm", velRndm.get());
        eucAccLen = state.value("eucAccLen", eucAccLen.get());
        eucAccHits = state.value("eucAccHits", eucAccHits.get());
        eucAccOff = state.value("eucAccOff", eucAccOff.get());
        eucAccStrength = state.value("eucAccStrength", eucAccStrength.get());
        durBase = state.value("durBase", durBase.get());
        durRndm = state.value("durRndm", durRndm.get());
        eucDurLen = state.value("eucDurLen", eucDurLen.get());
        eucDurHits = state.value("eucDurHits", eucDurHits.get());
        eucDurOff = state.value("eucDurOff", eucDurOff.get());
        durEucStrength = state.value("durEucStrength", durEucStrength.get());
        durationRndPerStep = state.value("durationRndPerStep", durationRndPerStep.get());
        outputHistoryWindowMs = state.value("outputHistoryWindowMs", outputHistoryWindowMs.get());
        morphTime = state.value("morphTime", morphTime.get());
    }

    rebuildSourceMaterial();
    generateEuclideanPattern(euclideanPattern, eucLen.get(), eucHits.get(), eucOff.get());
    generateEuclideanPattern(euclideanAccents, eucAccLen.get(), eucAccHits.get(), eucAccOff.get());
    generateEuclideanPattern(euclideanDurations, eucDurLen.get(), eucDurHits.get(), eucDurOff.get());
    resizeStateVectors(seqSize.get());
    syncUserPatternToSequenceSize();
    rebuildDeviations();
    rebuildPitchSequence();
    rebuildEuclideanOutputs();
    internalClockNeedsSync = true;
    updateOutputs();
}

#endif
