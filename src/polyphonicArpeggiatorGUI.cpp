#include "polyphonicArpeggiatorGUI.h"

#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT

#include "imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>

namespace {
    const ImVec4 snapshotsBg = ImVec4(0.27f, 0.42f, 0.62f, 0.97f);
    const ImVec4 snapshotsTitle = ImVec4(0.92f, 0.97f, 1.00f, 1.00f);
    const ImVec4 topBg = ImVec4(0.22f, 0.36f, 0.55f, 0.97f);
    const ImVec4 topTitle = ImVec4(0.89f, 0.96f, 1.00f, 1.00f);
    const ImVec4 tabsBg = ImVec4(0.17f, 0.30f, 0.47f, 0.97f);
    const ImVec4 tabsTitle = ImVec4(0.84f, 0.93f, 1.00f, 1.00f);
    const ImVec4 rhythmBg = ImVec4(0.14f, 0.25f, 0.40f, 0.97f);
    const ImVec4 rhythmTitle = ImVec4(0.82f, 0.92f, 1.00f, 1.00f);
    const ImVec4 sourceBg = ImVec4(0.24f, 0.39f, 0.58f, 0.97f);
    const ImVec4 sourceTitle = ImVec4(0.89f, 0.96f, 1.00f, 1.00f);
    const ImVec4 patternBg = ImVec4(0.21f, 0.35f, 0.53f, 0.97f);
    const ImVec4 patternTitle = ImVec4(0.86f, 0.94f, 1.00f, 1.00f);
    const ImVec4 polyBg = ImVec4(0.18f, 0.31f, 0.48f, 0.97f);
    const ImVec4 polyTitle = ImVec4(0.82f, 0.92f, 1.00f, 1.00f);
    const ImVec4 euclidBg = ImVec4(0.15f, 0.27f, 0.43f, 0.97f);
    const ImVec4 euclidTitle = ImVec4(0.79f, 0.90f, 1.00f, 1.00f);
    const ImVec4 velocityBg = ImVec4(0.12f, 0.23f, 0.38f, 0.97f);
    const ImVec4 velocityTitle = ImVec4(0.76f, 0.88f, 0.99f, 1.00f);
    const ImVec4 visualizationBg = ImVec4(0.20f, 0.33f, 0.50f, 0.98f);
    const ImVec4 visualizationTitle = ImVec4(0.84f, 0.93f, 1.00f, 1.00f);
    const ImVec4 outputBg = ImVec4(0.15f, 0.28f, 0.44f, 0.97f);
    const ImVec4 outputTitle = ImVec4(0.80f, 0.91f, 1.00f, 1.00f);
    constexpr double transportResetBeatWindow = 0.05;
    uint64_t sharedSnapshotStorageRevision = 1;

    struct KeyGeometry {
        bool isBlack;
        float x;
        float w;
    };

    struct BeatDivisionPreset {
        const char *label;
        float value;
    };

    constexpr std::array<BeatDivisionPreset, 21> beatDivisionPresets = {{
        {"2 bars", 0.125f},
        {"1 bar", 0.25f},
        {"1/2 dotted", 1.0f / 3.0f},
        {"1/2", 0.5f},
        {"1/2 triplet", 0.75f},
        {"4th dotted", 2.0f / 3.0f},
        {"4th", 1.0f},
        {"4th triplet", 1.5f},
        {"8th dotted", 4.0f / 3.0f},
        {"8th", 2.0f},
        {"8th triplet", 3.0f},
        {"16th dotted", 8.0f / 3.0f},
        {"16th", 4.0f},
        {"16th triplet", 6.0f},
        {"32nd dotted", 16.0f / 3.0f},
        {"32nd", 8.0f},
        {"32nd triplet", 12.0f},
        {"64th dotted", 32.0f / 3.0f},
        {"64th", 16.0f},
        {"64th triplet", 24.0f},
        {"128th", 32.0f},
    }};

    constexpr double geigerTransportStepsPerBeat = 96.0;

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
                             const std::vector<float> *velocities,
                             float width,
                             float height,
                             bool active,
                             bool fullRange = false) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImGui::InvisibleButton(id, ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

        drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                                active ? IM_COL32(16, 30, 28, 255) : IM_COL32(16, 18, 24, 255), 6.0f);
        drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(66, 86, 96, 220), 6.0f);

        if(values.empty()) return;

        std::map<int, int> noteCounts;
        std::map<int, float> noteVelocities;
        for(size_t i = 0; i < values.size(); i++) {
            int note = ofClamp(static_cast<int>(std::round(values[i])), 0, 127);
            noteCounts[note]++;
            float velocity = velocities != nullptr && i < velocities->size()
                           ? ofClamp((*velocities)[i], 0.0f, 1.0f)
                           : 1.0f;
            auto found = noteVelocities.find(note);
            if(found == noteVelocities.end()) noteVelocities[note] = velocity;
            else found->second = std::max(found->second, velocity);
        }

        auto range = fullRange ? std::pair<int, int>{0, 127} : computeKeyboardRange(values, 18);
        std::vector<KeyGeometry> geometry = buildKeyboardGeometry(range.first, range.second, width);
        float blackKeyHeight = height * 0.62f;
        auto getHighlightColor = [active, &noteVelocities](int note) {
            float velocity = 1.0f;
            auto found = noteVelocities.find(note);
            if(found != noteVelocities.end()) velocity = found->second;

            if(!active) return IM_COL32(96, 216, 224, 185);

            float emphasized = std::pow(ofClamp(velocity, 0.0f, 1.0f), 1.15f);
            int r = static_cast<int>(ofLerp(88.0f, 255.0f, emphasized));
            int g = static_cast<int>(ofLerp(98.0f, 198.0f, emphasized));
            int b = static_cast<int>(ofLerp(74.0f, 104.0f, emphasized));
            int a = static_cast<int>(ofLerp(120.0f, 235.0f, emphasized));
            return IM_COL32(r, g, b, a);
        };

        for(size_t i = 0; i < geometry.size(); i++) {
            if(geometry[i].isBlack) continue;
            int note = range.first + static_cast<int>(i);
            ImVec2 keyPos(pos.x + geometry[i].x, pos.y);
            ImVec2 keyEnd(keyPos.x + geometry[i].w, pos.y + height);
            drawList->AddRectFilled(keyPos, keyEnd, IM_COL32(244, 239, 232, 255));
            drawList->AddRect(keyPos, keyEnd, IM_COL32(102, 104, 110, 255));
            if(noteCounts.count(note) > 0) {
                drawList->AddRectFilled(keyPos, keyEnd, getHighlightColor(note));
            }
        }

        for(size_t i = 0; i < geometry.size(); i++) {
            if(!geometry[i].isBlack) continue;
            int note = range.first + static_cast<int>(i);
            ImVec2 keyPos(pos.x + geometry[i].x, pos.y);
            ImVec2 keyEnd(keyPos.x + geometry[i].w, pos.y + blackKeyHeight);
            drawList->AddRectFilled(keyPos, keyEnd, IM_COL32(28, 30, 38, 255));
            drawList->AddRect(keyPos, keyEnd, IM_COL32(78, 80, 88, 255));
            if(noteCounts.count(note) > 0) {
                drawList->AddRectFilled(keyPos, keyEnd, getHighlightColor(note));
            }
        }
    }

    bool drawDraggableFloatWithPopup(const char *label,
                                     float &value,
                                     float speed,
                                     float minValue,
                                     float maxValue,
                                     const char *format,
                                     const std::function<void()> &popupExtras = {}) {
        bool changed = ImGui::DragFloat(label, &value, speed, minValue, maxValue, format);
        value = ofClamp(value, minValue, maxValue);

        if(ImGui::BeginPopupContextItem()) {
            static char valueBuffer[64] = "";
            if(ImGui::IsWindowAppearing()) {
                std::snprintf(valueBuffer, sizeof(valueBuffer), format, value);
                ImGui::SetKeyboardFocusHere();
            }

            ImGui::SetNextItemWidth(120.0f);
            if(ImGui::InputText("##value", valueBuffer, sizeof(valueBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                try {
                    value = ofClamp(std::stof(std::string(valueBuffer)), minValue, maxValue);
                    changed = true;
                } catch(...) {
                }
                ImGui::CloseCurrentPopup();
            }

            if(popupExtras) {
                ImGui::Separator();
                popupExtras();
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
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(titleColor.x * 0.55f, titleColor.y * 0.55f, titleColor.z * 0.55f, 0.30f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 9.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        const float collapsedHeight = std::max(22.0f, ImGui::GetTextLineHeightWithSpacing() + 8.0f);
        ImGui::BeginChild(id, ImVec2(size.x, expanded ? size.y : collapsedHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::SetWindowFontScale(zoom);

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImVec2 childPos = ImGui::GetWindowPos();
        ImVec2 childSize = ImGui::GetWindowSize();
        float headerHeight = std::max(24.0f, ImGui::GetTextLineHeightWithSpacing() + 10.0f);
        ImU32 headerFill = IM_COL32(static_cast<int>(titleColor.x * 255.0f),
                                    static_cast<int>(titleColor.y * 255.0f),
                                    static_cast<int>(titleColor.z * 255.0f),
                                    34);
        ImU32 headerLine = IM_COL32(static_cast<int>(titleColor.x * 255.0f),
                                    static_cast<int>(titleColor.y * 255.0f),
                                    static_cast<int>(titleColor.z * 255.0f),
                                    170);
        drawList->AddRectFilled(childPos, ImVec2(childPos.x + childSize.x, childPos.y + headerHeight), headerFill, 9.0f, ImDrawFlags_RoundCornersTop);
        drawList->AddLine(ImVec2(childPos.x, childPos.y + headerHeight),
                          ImVec2(childPos.x + childSize.x, childPos.y + headerHeight),
                          headerLine,
                          2.0f);

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

    int findBeatDivisionPresetIndex(float value) {
        for(size_t i = 0; i < beatDivisionPresets.size(); i++) {
            if(std::abs(beatDivisionPresets[i].value - value) < 0.001f) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    float computeSequenceMuteBlockTriggerProbability(float playProbability, int muteCycles) {
        float clampedPlayProbability = ofClamp(playProbability, 0.0f, 1.0f);
        int clampedMuteCycles = std::max(1, muteCycles);
        float targetMuteFraction = 1.0f - clampedPlayProbability;
        if(targetMuteFraction <= 0.0f) return 0.0f;

        float denominator = 1.0f + clampedPlayProbability * static_cast<float>(clampedMuteCycles - 1);
        if(denominator <= 0.0f) return 0.0f;
        return ofClamp(targetMuteFraction / denominator, 0.0f, 1.0f);
    }

    float convertLegacyDurationMsToUnits(float durationMs, float beatDivision) {
        float clampedBeatDivision = std::max(0.001f, beatDivision);
        float stepDurationMs = 60000.0f / (120.0f * clampedBeatDivision);
        if(stepDurationMs <= 0.0f) return durationMs;
        return durationMs / stepDurationMs;
    }

    float convertLegacyPositionMsToUnits(float positionMs, float beatDivision) {
        float clampedBeatDivision = std::max(0.001f, beatDivision);
        float stepDurationMs = 60000.0f / (120.0f * clampedBeatDivision);
        if(stepDurationMs <= 0.0f) return positionMs;
        return ofClamp(positionMs / stepDurationMs, 0.0f, 1.0f);
    }

    void markSharedSnapshotStorageChanged() {
        sharedSnapshotStorageRevision++;
    }

    void deduplicateApproximatePitches(std::vector<float> &values) {
        std::vector<float> uniqueValues;
        uniqueValues.reserve(values.size());
        for(float note : values) {
            bool alreadyPresent = false;
            for(float existing : uniqueValues) {
                if(std::abs(existing - note) < 0.001f) {
                    alreadyPresent = true;
                    break;
                }
            }
            if(!alreadyPresent) uniqueValues.push_back(note);
        }
        values = std::move(uniqueValues);
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
    addParameter(snapshotRecall.set("Snapshot Recall", 0, 0, MaxSnapshotRecall));

    addSeparator("Trigger", ofColor(200));
    addParameter(trigger.set("Trigger"));
    addParameter(reset.set("Reset"));
    addParameter(resetNext.set("ResetNext"));
    addParameter(internalClockMode.set("Transport Clock", false));
    addParameter(oneShotMode.set("One Shot", false));
    addParameter(beatDiv.set("BeatDiv", 1.0f, 0.125f, 32.0f));
    pulseMode.set("PulseMode", PeriodicPulse, PeriodicPulse, StepSeqPulse);
    pulseStepPattern.set("PulseStepPatt", std::vector<int>(16, 100), std::vector<int>{0}, std::vector<int>{100});
    geigerSpeed.set("GeigerSpeed", 1.0f, 0.125f, 16.0f);
    geigerDensity.set("GeigerDensity", 0.45f, 0.0f, 1.0f);
    geigerPeriodicity.set("GeigerPeriodicity", 0.75f, 0.0f, 1.0f);
    geigerChaos.set("GeigerChaos", 0.35f, 0.0f, 1.0f);

    addSeparator("Source", ofColor(200));
    sourceMode.set("Source Mode", Scale, Scale, ChordPool);
    addParameter(notePoolIn.set("NotePool", std::vector<float>{60.0f, 64.0f, 67.0f}, std::vector<float>{0.0f}, std::vector<float>{127.0f}));
    addParameter(scale.set("Scale", std::vector<float>{0, 2, 4, 5, 7, 9, 11}, std::vector<float>{-24.0f}, std::vector<float>{127.0f}));
    sortPool.set("Sort Pool", true);
    removeDuplicates.set("Dedup Pool", false);
    sourceChangeMode.set("Src Change", KeepPhase, KeepPhase, ResetPattern);

    patternMode.set("Pattern", 0, 0, 5);
    patternSeed.set("PatternSeed", 0, 0, 999999);
    patternRandomizeSeedEachCycle.set("PatternRndSeed", false);
    idxPattern.set("IdxPatt", std::vector<int>{0, 1, 2, 3}, std::vector<int>{0}, std::vector<int>{127});
    modulo.set("Modulo", 0, 0, MaxSequenceSize);
    seqSize.set("SeqSize", 16, 1, MaxSequenceSize);
    sourceStart.set("Source Start", 0, 0, 127);
    sourceStride.set("Source Stride", 1, 0, 24);
    stepShift.set("StepShift", 0, -MaxSequenceSize, MaxSequenceSize);
    rndShiftChance.set("RndShift%", 0.0f, 0.0f, 1.0f);
    rndShiftRange.set("RndShiftRange", 0, 0, MaxSequenceSize);
    rndShiftQuant.set("RndShiftQ", 1, 1, MaxSequenceSize);
    octave.set("Octave", 0, -2, 8);
    octaveFold.set("Octavefold", 0, 0, 8);
    foldPoly.set("FoldPoly", true);
    addParameter(root.set("Root", 0.0f, -24.0f, 127.0f));
    pitchExpand.set("Pitch Expand", true);
    expandStep.set("Expand Step", 12, 1, 48);
    transpose.set("Transpose", 0, -96, 96);
    dynamicMode.set("Dynamic", false);
    accentOnsetMode.set("AccOnset", true);

    polyphony.set("Polyphony", 1, 1, MaxPolyphony);
    polyInterval.set("PolyInterval", 2, 1, 24);
    polyAccent.set("PolyAccent", 0, 0, MaxPolyphony - 1);
    addBass.set("AddBass", false);
    bassPatternMode.set("BassPatt", BassAlternatePattern, BassAlternatePattern, BassStartPattern);
    bassAlternateSteps.set("BassEvery", 2, 1, 64);
    bassAlternateShift.set("BassShift", 0, -MaxSequenceSize, MaxSequenceSize);
    bassEucLen.set("BassLen", 8, 1, 64);
    bassEucHits.set("BassHits", 2, 0, 64);
    bassEucOff.set("BassOff", 0, 0, 63);
    bassOctave.set("BassOctave", -2, -4, 4);
    bassProb.set("BassProb", 1.0f, 0.0f, 1.0f);
    bassOnAccent.set("BassOnAccent", false);
    bassOctRnd.set("BassOctRnd", 0, 0, 4);
    bassOctRndChance.set("BassOctRnd%", 0.0f, 0.0f, 1.0f);
    bassOctRndSeed.set("BassOctSeed", 0, 0, 999999);
    bassOctRndRandomizeSeedEachCycle.set("BassOctRndSeed", false);
    bassPolyphony.set("BassPoly", 1, 1, 4);
    bassVelBase.set("BassVel", 0.75f, 0.0f, 1.0f);
    bassVelRndm.set("BassVelRnd", 0.0f, 0.0f, 1.0f);
    bassPitchModified.set("BassPitchMod", true);
    skipSteps.set("SkipSteps", 0, 0, 32);
    strum.set("Strum", 0.0f, 0.0f, 1.0f);
    strumRndm.set("StrumRndm", 0.0f, 0.0f, 1.0f);
    strumRndSeed.set("StrRndSeed", 0, 0, 999999);
    strumRndRandomizeSeedEachCycle.set("StrRndRndSeed", false);
    swing.set("Swing", 0.0f, 0.0f, 1.0f);
    strumDir.set("StrumDir", 0, 0, 2);
    strumDirSeed.set("StrDirSeed", 0, 0, 999999);
    strumDirRandomizeSeedEachCycle.set("StrDirRndSeed", false);
    positionRndm.set("PosRndm", 0.0f, 0.0f, 1.0f);
    positionRndSeed.set("PosRndSeed", 0, 0, 999999);
    positionRndRandomizeSeedEachCycle.set("PosRndRndSeed", false);

    octaveDev.set("OctDev", 0.0f, 0.0f, 1.0f);
    octaveDevRng.set("OctDevRng", 1, 1, 4);
    idxDev.set("IdxDev", 0.0f, 0.0f, 1.0f);
    idxDevRng.set("IdxDevRng", 2, 1, 12);
    pitchDev.set("PitchDev", 0.0f, 0.0f, 1.0f);
    pitchDevRng.set("PitchDevRng", 2, 1, 12);

    eucLen.set("EucLen", 8, 1, 64);
    eucHits.set("EucHits", 8, 0, 64);
    eucOff.set("EucOff", 0, 0, 63);
    seqProb.set("Seq%", 1.0f, 0.0f, 1.0f);
    seqProbCycles.set("SeqCycles", 1, 1, 64);
    runGateBeats.set("RunBeats", 16.0f, 1.0f, 512.0f);
    runGateChance.set("Run%", 1.0f, 0.0f, 1.0f);
    runGatePhase.set("RunPhase", 0.0f, 0.0f, 1.0f);
    stepChance.set("Step%", 1.0f, 0.0f, 1.0f);
    noteChance.set("Note%", 1.0f, 0.0f, 1.0f);

    velBase.set("VelBase", 0.8f, 0.0f, 1.0f);
    velRndm.set("VelRndm", 0.1f, 0.0f, 1.0f);
    velRndSeed.set("VelRndSeed", 0, 0, 999999);
    velRndRandomizeSeedEachCycle.set("VelRndRndSeed", false);
    velStepRndm.set("VelStepRnd", 0.0f, 0.0f, 1.0f);
    velStepRndSeed.set("VelStepSeed", 0, 0, 999999);
    velStepRndRandomizeSeedEachCycle.set("VelStepRndSeed", false);
    velocityCurve.set("VelCurve", std::vector<float>(VelocityCurveResolution, 0.0f), std::vector<float>{0.0f}, std::vector<float>{1.0f});
    velocitySlowRndm.set("VelSlowRnd", 0.0f, 0.0f, 1.0f);
    velocitySlowSpeed.set("VelSlowSpd", 0.25f, 0.0f, 8.0f);
    velocitySlowSeed.set("VelSlowSeed", 0, 0, 999999);
    velocityLfoShape.set("VelLfoShape", VelocityLfoSlowRandom, VelocityLfoSin, VelocityLfoSlowRandom);
    velocityLfoPhase.set("VelLfoPhase", 0.0f, 0.0f, 1.0f);
    velocityLfoPulseWidth.set("VelLfoWidth", 0.5f, 0.05f, 0.95f);
    accentPatternMode.set("AccPatt", EuclideanEventPattern, AlternateEventPattern, StepSeqEventPattern);
    accentAlternateSteps.set("AccEvery", 2, 1, 64);
    accentAlternateShift.set("AccShift", 0, -MaxSequenceSize, MaxSequenceSize);
    accentChance.set("AccChance", 0.5f, 0.0f, 1.0f);
    accentSeed.set("AccSeed", 0, 0, 999999);
    accentRandomizeSeedEachCycle.set("AccRndSeed", false);
    accentStepPattern.set("AccStepSeq", std::vector<int>(16, 0), std::vector<int>{0}, std::vector<int>{1});
    eucAccLen.set("AccLen", 4, 1, 64);
    eucAccHits.set("AccHits", 1, 0, 64);
    eucAccOff.set("AccOff", 0, 0, 63);
    eucAccStrength.set("AccStr", 0.2f, 0.0f, 1.0f);

    durBase.set("DurBase", 1.0f, 0.0f, 4.0f);
    durRndm.set("DurRndm", 0.0f, 0.0f, 4.0f);
    durationPatternMode.set("DurPatt", EuclideanEventPattern, AlternateEventPattern, StepSeqEventPattern);
    durationAlternateSteps.set("DurEvery", 2, 1, 64);
    durationAlternateShift.set("DurShift", 0, -MaxSequenceSize, MaxSequenceSize);
    durationChance.set("DurChance", 0.5f, 0.0f, 1.0f);
    durationSeed.set("DurSeed", 0, 0, 999999);
    durationRandomizeSeedEachCycle.set("DurRndSeed", false);
    durationStepPattern.set("DurStepSeq", std::vector<int>(16, 0), std::vector<int>{0}, std::vector<int>{1});
    eucDurLen.set("DurEucLen", 4, 1, 64);
    eucDurHits.set("DurEucHits", 4, 0, 64);
    eucDurOff.set("DurEucOff", 0, 0, 63);
    durEucStrength.set("DurEucStr", 0.0f, 0.0f, 4.0f);
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

    initializePublishableEditorParameters();
    resizeStateVectors(seqSize.get());
    syncUserPatternToSequenceSize();
    syncPulseStepPatternToSequenceSize();
    syncAccentStepPatternToSequenceSize();
    syncDurationStepPatternToSequenceSize();
    syncVelocityCurveShape();
    setupListeners();

    rebuildSourceMaterial();
    generateEuclideanPattern(euclideanPattern, eucLen.get(), eucHits.get(), eucOff.get());
    generateEuclideanPattern(euclideanAccents, eucAccLen.get(), eucAccHits.get(), eucAccOff.get());
    generateEuclideanPattern(euclideanDurations, eucDurLen.get(), eucDurHits.get(), eucDurOff.get());
    generateEuclideanPattern(euclideanBass, bassEucLen.get(), bassEucHits.get(), bassEucOff.get());
    rebuildDeviations();
    rebuildPitchSequence();
    rebuildEuclideanOutputs();
    updateOutputs();
    refreshSnapshotsFromSharedStorage(true);
}

void polyphonicArpeggiatorGUI::initializePublishableEditorParameters() {
    publishableEditorParameters.clear();

    auto registerParam = [this](const std::string& key, auto& parameter) {
        EditorPublishAction action;
        action.key = key;
        action.publish = [this, key, &parameter]() { return publishEditorParameterToNode(key, parameter); };
        action.unpublish = [this, key]() { return unpublishEditorParameterFromNode(key); };
        action.isPublished = [this, key]() { return isEditorParameterPublished(key); };
        action.isAvailableInNode = [this, &parameter]() { return getParameterGroup().contains(parameter.getEscapedName()); };
        publishableEditorParameters.push_back(std::move(action));
    };

    auto registerDropdown = [this](const std::string& key, auto& parameter, std::vector<std::string> options) {
        EditorPublishAction action;
        action.key = key;
        action.publish = [this, key, &parameter, options]() { return publishEditorDropdownParameterToNode(key, parameter, options); };
        action.unpublish = [this, key]() { return unpublishEditorParameterFromNode(key); };
        action.isPublished = [this, key]() { return isEditorParameterPublished(key); };
        action.isAvailableInNode = [this, &parameter]() { return getParameterGroup().contains(parameter.getEscapedName()); };
        publishableEditorParameters.push_back(std::move(action));
    };

    registerDropdown("sourceMode", sourceMode, {"Scale", "Chord Pool"});
    registerParam("sortPool", sortPool);
    registerParam("removeDuplicates", removeDuplicates);
    registerDropdown("sourceChangeMode", sourceChangeMode, {"Keep Phase", "Reset Pattern"});
    registerDropdown("patternMode", patternMode, {"Ascending", "Descending", "Random", "User", "Asc+Desc", "Desc+Asc"});
    registerParam("patternSeed", patternSeed);
    registerParam("patternRandomizeSeedEachCycle", patternRandomizeSeedEachCycle);
    registerParam("snapshotRecall", snapshotRecall);
    registerParam("modulo", modulo);
    registerParam("internalClockMode", internalClockMode);
    registerParam("oneShotMode", oneShotMode);
    registerParam("trigger", trigger);
    registerParam("reset", reset);
    registerParam("resetNext", resetNext);
    registerParam("beatDiv", beatDiv);
    registerDropdown("pulseMode", pulseMode, {"Periodic", "Euclidean", "Geiger", "StepSeq"});
    registerParam("geigerSpeed", geigerSpeed);
    registerParam("geigerDensity", geigerDensity);
    registerParam("geigerPeriodicity", geigerPeriodicity);
    registerParam("seqSize", seqSize);
    registerParam("sourceStart", sourceStart);
    registerParam("sourceStride", sourceStride);
    registerParam("stepShift", stepShift);
    registerParam("rndShiftChance", rndShiftChance);
    registerParam("rndShiftRange", rndShiftRange);
    registerParam("rndShiftQuant", rndShiftQuant);
    registerParam("octave", octave);
    registerParam("octaveFold", octaveFold);
    registerParam("foldPoly", foldPoly);
    registerParam("pitchExpand", pitchExpand);
    registerParam("expandStep", expandStep);
    registerParam("transpose", transpose);
    registerParam("dynamicMode", dynamicMode);
    registerParam("accentOnsetMode", accentOnsetMode);
    registerParam("polyphony", polyphony);
    registerParam("polyInterval", polyInterval);
    registerParam("polyAccent", polyAccent);
    registerParam("addBass", addBass);
    registerDropdown("bassPatternMode", bassPatternMode, {"Alternate", "Euclidean", "Random", "VelAccented", "DurAccented", "Start"});
    registerParam("bassAlternateSteps", bassAlternateSteps);
    registerParam("bassAlternateShift", bassAlternateShift);
    registerParam("bassEucLen", bassEucLen);
    registerParam("bassEucHits", bassEucHits);
    registerParam("bassEucOff", bassEucOff);
    registerParam("bassOctave", bassOctave);
    registerParam("bassProb", bassProb);
    registerParam("bassOnAccent", bassOnAccent);
    registerParam("bassOctRnd", bassOctRnd);
    registerParam("bassOctRndChance", bassOctRndChance);
    registerParam("bassOctRndSeed", bassOctRndSeed);
    registerParam("bassOctRndRandomizeSeedEachCycle", bassOctRndRandomizeSeedEachCycle);
    registerParam("bassPolyphony", bassPolyphony);
    registerParam("bassVelBase", bassVelBase);
    registerParam("bassVelRndm", bassVelRndm);
    registerParam("bassPitchModified", bassPitchModified);
    registerParam("skipSteps", skipSteps);
    registerParam("strum", strum);
    registerParam("strumRndm", strumRndm);
    registerParam("strumRndSeed", strumRndSeed);
    registerParam("strumRndRandomizeSeedEachCycle", strumRndRandomizeSeedEachCycle);
    registerParam("swing", swing);
    registerDropdown("strumDir", strumDir, {"Ascending", "Descending", "Random"});
    registerParam("strumDirSeed", strumDirSeed);
    registerParam("strumDirRandomizeSeedEachCycle", strumDirRandomizeSeedEachCycle);
    registerParam("positionRndm", positionRndm);
    registerParam("positionRndSeed", positionRndSeed);
    registerParam("positionRndRandomizeSeedEachCycle", positionRndRandomizeSeedEachCycle);
    registerParam("octaveDev", octaveDev);
    registerParam("octaveDevRng", octaveDevRng);
    registerParam("idxDev", idxDev);
    registerParam("idxDevRng", idxDevRng);
    registerParam("pitchDev", pitchDev);
    registerParam("pitchDevRng", pitchDevRng);
    registerParam("eucLen", eucLen);
    registerParam("eucHits", eucHits);
    registerParam("eucOff", eucOff);
    registerParam("seqProb", seqProb);
    registerParam("seqProbCycles", seqProbCycles);
    registerParam("runGateBeats", runGateBeats);
    registerParam("runGateChance", runGateChance);
    registerParam("runGatePhase", runGatePhase);
    registerParam("stepChance", stepChance);
    registerParam("noteChance", noteChance);
    registerParam("velBase", velBase);
    registerParam("velRndm", velRndm);
    registerParam("velRndSeed", velRndSeed);
    registerParam("velRndRandomizeSeedEachCycle", velRndRandomizeSeedEachCycle);
    registerParam("velStepRndm", velStepRndm);
    registerParam("velStepRndSeed", velStepRndSeed);
    registerParam("velStepRndRandomizeSeedEachCycle", velStepRndRandomizeSeedEachCycle);
    registerParam("velocitySlowRndm", velocitySlowRndm);
    registerParam("velocitySlowSpeed", velocitySlowSpeed);
    registerParam("velocitySlowSeed", velocitySlowSeed);
    registerDropdown("velocityLfoShape", velocityLfoShape, {"Sin", "Saw", "InvSaw", "Pulse", "SlowRandom"});
    registerParam("velocityLfoPhase", velocityLfoPhase);
    registerParam("velocityLfoPulseWidth", velocityLfoPulseWidth);
    registerDropdown("accentPatternMode", accentPatternMode, {"Alternate", "Euclidean", "RandomStep", "RandomNote", "StepSeq"});
    registerParam("accentAlternateSteps", accentAlternateSteps);
    registerParam("accentAlternateShift", accentAlternateShift);
    registerParam("accentChance", accentChance);
    registerParam("accentSeed", accentSeed);
    registerParam("accentRandomizeSeedEachCycle", accentRandomizeSeedEachCycle);
    registerParam("eucAccLen", eucAccLen);
    registerParam("eucAccHits", eucAccHits);
    registerParam("eucAccOff", eucAccOff);
    registerParam("eucAccStrength", eucAccStrength);
    registerParam("durBase", durBase);
    registerParam("durRndm", durRndm);
    registerDropdown("durationPatternMode", durationPatternMode, {"Alternate", "Euclidean", "RandomStep", "RandomNote", "StepSeq"});
    registerParam("durationAlternateSteps", durationAlternateSteps);
    registerParam("durationAlternateShift", durationAlternateShift);
    registerParam("durationChance", durationChance);
    registerParam("durationSeed", durationSeed);
    registerParam("durationRandomizeSeedEachCycle", durationRandomizeSeedEachCycle);
    registerParam("eucDurLen", eucDurLen);
    registerParam("eucDurHits", eucDurHits);
    registerParam("eucDurOff", eucDurOff);
    registerParam("durEucStrength", durEucStrength);
    registerParam("durationRndPerStep", durationRndPerStep);
    registerParam("outputHistoryWindowMs", outputHistoryWindowMs);
}

const polyphonicArpeggiatorGUI::EditorPublishAction* polyphonicArpeggiatorGUI::findPublishableEditorParameter(const std::string& key) const {
    auto it = std::find_if(publishableEditorParameters.begin(), publishableEditorParameters.end(),
                           [&](const EditorPublishAction& action) { return action.key == key; });
    return it == publishableEditorParameters.end() ? nullptr : &(*it);
}

bool polyphonicArpeggiatorGUI::isEditorParameterPublished(const std::string& key) const {
    return publishedEditorParameterHandles.find(key) != publishedEditorParameterHandles.end();
}

bool polyphonicArpeggiatorGUI::publishEditorParameterToNode(const std::string& key) {
    const EditorPublishAction* action = findPublishableEditorParameter(key);
    if(action == nullptr) return false;
    bool changed = action->publish();
    if(changed) parameterGroupChanged.notify(this);
    return changed;
}

bool polyphonicArpeggiatorGUI::unpublishEditorParameterFromNode(const std::string& key) {
    auto it = publishedEditorParameterHandles.find(key);
    if(it == publishedEditorParameterHandles.end()) return false;

    const std::string parameterName = it->second->getEscapedName();
    removeParameter(parameterName);
    publishedEditorParameterHandles.erase(it);
    publishedEditorParameterKeys.erase(std::remove(publishedEditorParameterKeys.begin(),
                                                   publishedEditorParameterKeys.end(),
                                                   key),
                                       publishedEditorParameterKeys.end());
    if(publishedEditorParameterHandles.empty() && publishedEditorSeparatorAdded) {
        removeSeparator("Published");
        publishedEditorSeparatorAdded = false;
    }
    parameterGroupChanged.notify(this);
    return true;
}

void polyphonicArpeggiatorGUI::syncPublishedEditorParameters(const std::vector<std::string>& keys) {
    std::vector<std::string> uniqueKeys;
    uniqueKeys.reserve(keys.size());
    for(const std::string& key : keys) {
        if(findPublishableEditorParameter(key) == nullptr) continue;
        if(std::find(uniqueKeys.begin(), uniqueKeys.end(), key) == uniqueKeys.end()) uniqueKeys.push_back(key);
    }

    bool changed = false;
    for(auto& entry : publishedEditorParameterHandles) {
        removeParameter(entry.second->getEscapedName());
        changed = true;
    }
    publishedEditorParameterHandles.clear();

    if(publishedEditorSeparatorAdded) {
        removeSeparator("Published");
        publishedEditorSeparatorAdded = false;
        changed = true;
    }

    publishedEditorParameterKeys.clear();
    for(const std::string& key : uniqueKeys) {
        const EditorPublishAction* action = findPublishableEditorParameter(key);
        if(action != nullptr) changed = action->publish() || changed;
    }

    if(publishedEditorParameterHandles.empty()) {
        publishedEditorSeparatorAdded = false;
    }
    if(changed) parameterGroupChanged.notify(this);
}

void polyphonicArpeggiatorGUI::drawPublishedLabelUnderline(const std::string& key,
                                                           const char* label,
                                                           float frameWidth,
                                                           bool checkbox) const {
    if(!isEditorParameterPublished(key) || label == nullptr || label[0] == '\0' || label[0] == '#') return;

    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    const ImGuiStyle &style = ImGui::GetStyle();
    const float labelStartX = checkbox
        ? itemMin.x + ImGui::GetFrameHeight() + style.ItemInnerSpacing.x
        : itemMin.x + std::max(0.0f, frameWidth) + style.ItemInnerSpacing.x;
    const ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);
    const float labelEndX = std::min(labelStartX + textSize.x, itemMax.x);
    if(labelEndX <= labelStartX) return;

    const float lineY = itemMax.y - 2.0f;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(labelStartX, lineY),
                                        ImVec2(labelEndX, lineY),
                                        ImGui::GetColorU32(ImGuiCol_Text),
                                        1.0f);
}

void polyphonicArpeggiatorGUI::drawPublishedCurrentItemUnderline(const std::string& key) const {
    if(!isEditorParameterPublished(key)) return;

    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    if(itemMax.x <= itemMin.x) return;

    const float lineY = itemMax.y - 2.0f;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(itemMin.x, lineY),
                                        ImVec2(itemMax.x, lineY),
                                        ImGui::GetColorU32(ImGuiCol_Text),
                                        1.0f);
}

void polyphonicArpeggiatorGUI::drawNodePublishContextMenu(const std::string& key,
                                                          const char* label,
                                                          float frameWidth,
                                                          bool checkbox) {
    drawPublishedLabelUnderline(key, label, frameWidth, checkbox);
    if(!ImGui::BeginPopupContextItem(("##PublishToNode_" + key).c_str())) return;

    drawNodePublishMenuItems(key);

    ImGui::EndPopup();
}

void polyphonicArpeggiatorGUI::drawNodePublishMenuItems(const std::string& key) {
    const EditorPublishAction* action = findPublishableEditorParameter(key);
    if(action == nullptr) {
        ImGui::TextDisabled("Not publishable");
    } else if(action->isPublished()) {
        ImGui::TextDisabled("Published in node");
        if(ImGui::MenuItem("Unpublish from Node")) {
            unpublishEditorParameterFromNode(key);
        }
    } else if(action->isAvailableInNode != nullptr && action->isAvailableInNode()) {
        ImGui::TextDisabled("Already in node");
    } else {
        if(ImGui::MenuItem("Publish to Node")) {
            publishEditorParameterToNode(key);
        }
    }
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
        sequenceCycleDecisionPending = true;
        skippedSequenceCyclesRemaining = 0;
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
    listeners.push(pulseMode.newListener([this](int &){ updateOutputs(); }));
    listeners.push(geigerSpeed.newListener([this](float &){ updateOutputs(); }));
    listeners.push(geigerDensity.newListener([this](float &){ updateOutputs(); }));
    listeners.push(geigerPeriodicity.newListener([this](float &){ updateOutputs(); }));
    listeners.push(geigerChaos.newListener([this](float &){ updateOutputs(); }));
    listeners.push(snapshotRecall.newListener([this](int &value) {
        if(value > 0) {
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
        syncPulseStepPatternToSequenceSize();
        syncAccentStepPatternToSequenceSize();
        syncDurationStepPatternToSequenceSize();
        rebuildDeviations();
        rebuildPitchSequence();
        rebuildEuclideanOutputs();
        sequenceCycleDecisionPending = true;
        skippedSequenceCyclesRemaining = 0;
        updateOutputs();
    }));
    listeners.push(polyphony.newListener([this](int &) {
        resizeStateVectors(seqSize.get());
        rebuildDeviations();
        rebuildPitchSequence();
        updateOutputs();
    }));
    listeners.push(polyAccent.newListener([this](int &) {
        resizeStateVectors(seqSize.get());
        rebuildDeviations();
        rebuildPitchSequence();
        updateOutputs();
    }));
    listeners.push(addBass.newListener([this](bool &) {
        resizeStateVectors(seqSize.get());
        rebuildDeviations();
        rebuildPitchSequence();
        updateOutputs();
    }));
    listeners.push(bassPolyphony.newListener([this](int &) {
        resizeStateVectors(seqSize.get());
        rebuildDeviations();
        rebuildPitchSequence();
        updateOutputs();
    }));

    auto rebuildPitch = [this]() {
        rebuildDeviations();
        rebuildPitchSequence();
        updateOutputs();
    };

    listeners.push(sourceStart.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(sourceStride.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(stepShift.newListener([this](int &){
        rebuildEuclideanOutputs();
        updateOutputs();
    }));
    auto rebuildShiftOutputs = [this]() {
        rebuildEuclideanOutputs();
        updateOutputs();
    };
    listeners.push(rndShiftChance.newListener([rebuildShiftOutputs](float &){ rebuildShiftOutputs(); }));
    listeners.push(rndShiftRange.newListener([rebuildShiftOutputs](int &){ rebuildShiftOutputs(); }));
    listeners.push(rndShiftQuant.newListener([rebuildShiftOutputs](int &){ rebuildShiftOutputs(); }));
    listeners.push(octave.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(octaveFold.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(foldPoly.newListener([rebuildPitch](bool &){ rebuildPitch(); }));
    listeners.push(pitchExpand.newListener([rebuildPitch](bool &){ rebuildPitch(); }));
    listeners.push(expandStep.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(transpose.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(polyInterval.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(dynamicMode.newListener([rebuildPitch](bool &){ rebuildPitch(); }));
    listeners.push(root.newListener([this](float &){ updateOutputs(); }));
    listeners.push(bassPatternMode.newListener([this](int &){ updateOutputs(); }));
    listeners.push(bassAlternateSteps.newListener([this](int &){ updateOutputs(); }));
    listeners.push(bassAlternateShift.newListener([this](int &){ updateOutputs(); }));
    listeners.push(bassEucLen.newListener([this](int &){
        generateEuclideanPattern(euclideanBass, bassEucLen.get(), bassEucHits.get(), bassEucOff.get());
        updateOutputs();
    }));
    listeners.push(bassEucHits.newListener([this](int &){
        generateEuclideanPattern(euclideanBass, bassEucLen.get(), bassEucHits.get(), bassEucOff.get());
        updateOutputs();
    }));
    listeners.push(bassEucOff.newListener([this](int &){
        generateEuclideanPattern(euclideanBass, bassEucLen.get(), bassEucHits.get(), bassEucOff.get());
        updateOutputs();
    }));
    listeners.push(bassOctave.newListener([this](int &){ updateOutputs(); }));
    listeners.push(bassProb.newListener([this](float &){ updateOutputs(); }));
    listeners.push(bassOnAccent.newListener([this](bool &){ updateOutputs(); }));
    listeners.push(bassOctRnd.newListener([this](int &){ updateOutputs(); }));
    listeners.push(bassOctRndChance.newListener([this](float &){ updateOutputs(); }));
    listeners.push(bassOctRndSeed.newListener([this](int &){ updateOutputs(); }));
    listeners.push(bassOctRndRandomizeSeedEachCycle.newListener([this](bool &){ updateOutputs(); }));
    listeners.push(bassVelBase.newListener([this](float &){ updateOutputs(); }));
    listeners.push(bassVelRndm.newListener([this](float &){ updateOutputs(); }));
    listeners.push(bassPitchModified.newListener([this](bool &){ updateOutputs(); }));
    listeners.push(strum.newListener([this](float &){ updateOutputs(); }));
    listeners.push(strumRndm.newListener([this](float &){ updateOutputs(); }));
    listeners.push(strumRndSeed.newListener([this](int &){ updateOutputs(); }));
    listeners.push(strumRndRandomizeSeedEachCycle.newListener([this](bool &){ updateOutputs(); }));
    listeners.push(swing.newListener([this](float &){ updateOutputs(); }));
    listeners.push(strumDir.newListener([this](int &){ updateOutputs(); }));
    listeners.push(strumDirSeed.newListener([this](int &){ updateOutputs(); }));
    listeners.push(strumDirRandomizeSeedEachCycle.newListener([this](bool &){ updateOutputs(); }));
    listeners.push(positionRndm.newListener([this](float &){ updateOutputs(); }));
    listeners.push(positionRndSeed.newListener([this](int &){ updateOutputs(); }));
    listeners.push(positionRndRandomizeSeedEachCycle.newListener([this](bool &){ updateOutputs(); }));
    listeners.push(patternMode.newListener([this](int &){
        syncUserPatternToSequenceSize();
        updateOutputs();
    }));
    listeners.push(modulo.newListener([this](int &){ updateOutputs(); }));
    listeners.push(patternSeed.newListener([this](int &){ updateOutputs(); }));
    listeners.push(patternRandomizeSeedEachCycle.newListener([this](bool &){ updateOutputs(); }));
    listeners.push(idxPattern.newListener([this](std::vector<int> &){ updateOutputs(); }));
    listeners.push(pulseStepPattern.newListener([this](std::vector<int> &){ updateOutputs(); }));
    listeners.push(accentStepPattern.newListener([this](std::vector<int> &){ updateOutputs(); }));
    listeners.push(durationStepPattern.newListener([this](std::vector<int> &){ updateOutputs(); }));

    listeners.push(octaveDev.newListener([rebuildPitch](float &){ rebuildPitch(); }));
    listeners.push(octaveDevRng.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(idxDev.newListener([rebuildPitch](float &){ rebuildPitch(); }));
    listeners.push(idxDevRng.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(pitchDev.newListener([rebuildPitch](float &){ rebuildPitch(); }));
    listeners.push(pitchDevRng.newListener([rebuildPitch](int &){ rebuildPitch(); }));
    listeners.push(durationRndPerStep.newListener([this](bool &){ updateOutputs(); }));
    listeners.push(velBase.newListener([this](float &){ updateOutputs(); }));
    listeners.push(velRndm.newListener([this](float &){ updateOutputs(); }));
    listeners.push(velRndSeed.newListener([this](int &){ updateOutputs(); }));
    listeners.push(velRndRandomizeSeedEachCycle.newListener([this](bool &){ updateOutputs(); }));
    listeners.push(velStepRndm.newListener([this](float &){ updateOutputs(); }));
    listeners.push(velStepRndSeed.newListener([this](int &){ updateOutputs(); }));
    listeners.push(velStepRndRandomizeSeedEachCycle.newListener([this](bool &){ updateOutputs(); }));
    listeners.push(velocityCurve.newListener([this](std::vector<float> &){
        syncVelocityCurveShape();
        updateOutputs();
    }));
    listeners.push(velocitySlowRndm.newListener([this](float &){ updateOutputs(); }));
    listeners.push(velocitySlowSpeed.newListener([this](float &){ updateOutputs(); }));
    listeners.push(velocitySlowSeed.newListener([this](int &){ updateOutputs(); }));
    listeners.push(velocityLfoShape.newListener([this](int &){ updateOutputs(); }));
    listeners.push(velocityLfoPhase.newListener([this](float &){ updateOutputs(); }));
    listeners.push(velocityLfoPulseWidth.newListener([this](float &){ updateOutputs(); }));
    listeners.push(eucAccStrength.newListener([this](float &){ updateOutputs(); }));
    listeners.push(durBase.newListener([this](float &){ updateOutputs(); }));
    listeners.push(durRndm.newListener([this](float &){ updateOutputs(); }));
    listeners.push(durEucStrength.newListener([this](float &){ updateOutputs(); }));

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
    auto rebuildAccentOutputs = [this]() {
        rebuildEuclideanOutputs();
        updateOutputs();
    };
    auto rebuildDurationOutputs = [this]() {
        rebuildEuclideanOutputs();
        updateOutputs();
    };

    listeners.push(eucLen.newListener([rebuildGatePattern](int &){ rebuildGatePattern(); }));
    listeners.push(eucHits.newListener([rebuildGatePattern](int &){ rebuildGatePattern(); }));
    listeners.push(eucOff.newListener([rebuildGatePattern](int &){ rebuildGatePattern(); }));
    listeners.push(seqProb.newListener([this](float &){
        if(currentStep == 0) sequenceCycleDecisionPending = true;
    }));
    listeners.push(seqProbCycles.newListener([this](int &){
        if(currentStep == 0) sequenceCycleDecisionPending = true;
    }));
    auto resetRunGateWindow = [this]() {
        runGateWindowStateValid = false;
        updateOutputs();
    };
    listeners.push(runGateBeats.newListener([resetRunGateWindow](float &){ resetRunGateWindow(); }));
    listeners.push(runGateChance.newListener([resetRunGateWindow](float &){ resetRunGateWindow(); }));
    listeners.push(runGatePhase.newListener([resetRunGateWindow](float &){ resetRunGateWindow(); }));
    listeners.push(eucAccLen.newListener([rebuildAccentPattern](int &){ rebuildAccentPattern(); }));
    listeners.push(eucAccHits.newListener([rebuildAccentPattern](int &){ rebuildAccentPattern(); }));
    listeners.push(eucAccOff.newListener([rebuildAccentPattern](int &){ rebuildAccentPattern(); }));
    listeners.push(accentPatternMode.newListener([rebuildAccentOutputs](int &){ rebuildAccentOutputs(); }));
    listeners.push(accentAlternateSteps.newListener([rebuildAccentOutputs](int &){ rebuildAccentOutputs(); }));
    listeners.push(accentAlternateShift.newListener([rebuildAccentOutputs](int &){ rebuildAccentOutputs(); }));
    listeners.push(accentChance.newListener([rebuildAccentOutputs](float &){ rebuildAccentOutputs(); }));
    listeners.push(accentSeed.newListener([rebuildAccentOutputs](int &){ rebuildAccentOutputs(); }));
    listeners.push(accentRandomizeSeedEachCycle.newListener([rebuildAccentOutputs](bool &){ rebuildAccentOutputs(); }));
    listeners.push(eucDurLen.newListener([rebuildDurationPattern](int &){ rebuildDurationPattern(); }));
    listeners.push(eucDurHits.newListener([rebuildDurationPattern](int &){ rebuildDurationPattern(); }));
    listeners.push(eucDurOff.newListener([rebuildDurationPattern](int &){ rebuildDurationPattern(); }));
    listeners.push(durationPatternMode.newListener([rebuildDurationOutputs](int &){ rebuildDurationOutputs(); }));
    listeners.push(durationAlternateSteps.newListener([rebuildDurationOutputs](int &){ rebuildDurationOutputs(); }));
    listeners.push(durationAlternateShift.newListener([rebuildDurationOutputs](int &){ rebuildDurationOutputs(); }));
    listeners.push(durationChance.newListener([rebuildDurationOutputs](float &){ rebuildDurationOutputs(); }));
    listeners.push(durationSeed.newListener([rebuildDurationOutputs](int &){ rebuildDurationOutputs(); }));
    listeners.push(durationRandomizeSeedEachCycle.newListener([rebuildDurationOutputs](bool &){ rebuildDurationOutputs(); }));
}

int polyphonicArpeggiatorGUI::getOutputSlotCount() const {
    int sequenceSize = std::max(1, seqSize.get());
    int activePolyphony = std::max(1, std::min(polyphony.get() + polyAccent.get(), MaxPolyphony));
    if(addBass.get()) activePolyphony += std::max(1, bassPolyphony.get());
    return ofClamp(std::max(sequenceSize, activePolyphony), 1, MaxSequenceSize);
}

void polyphonicArpeggiatorGUI::resizeStateVectors(int size) {
    int maxPolyVoices = std::max(1, std::min(polyphony.get() + polyAccent.get(), MaxPolyphony));
    if(addBass.get()) maxPolyVoices += std::max(1, bassPolyphony.get());
    int clampedSize = ofClamp(std::max(size, maxPolyVoices), 1, MaxSequenceSize);
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
        currentCycleRandomStepShift = 0;
        oneShotCycleActive = false;
        oneShotStepsRemaining = 0;
        sequenceCycleDecisionPending = true;
        skippedSequenceCyclesRemaining = 0;
        runGateWindowStateValid = false;
        rebuildPitchSequence();
    }
    updateOutputs();
}

void polyphonicArpeggiatorGUI::randomizePatternSeedsForNewCycle() {
    if(patternRandomizeSeedEachCycle.get()) {
        patternSeed = std::uniform_int_distribution<int>(patternSeed.getMin(), patternSeed.getMax())(rng);
    }
    if(accentRandomizeSeedEachCycle.get()) {
        accentSeed = std::uniform_int_distribution<int>(accentSeed.getMin(), accentSeed.getMax())(rng);
    }
    if(durationRandomizeSeedEachCycle.get()) {
        durationSeed = std::uniform_int_distribution<int>(durationSeed.getMin(), durationSeed.getMax())(rng);
    }
    if(strumRndRandomizeSeedEachCycle.get()) {
        strumRndSeed = std::uniform_int_distribution<int>(strumRndSeed.getMin(), strumRndSeed.getMax())(rng);
    }
    if(strumDirRandomizeSeedEachCycle.get()) {
        strumDirSeed = std::uniform_int_distribution<int>(strumDirSeed.getMin(), strumDirSeed.getMax())(rng);
    }
    if(positionRndRandomizeSeedEachCycle.get()) {
        positionRndSeed = std::uniform_int_distribution<int>(positionRndSeed.getMin(), positionRndSeed.getMax())(rng);
    }
    if(velRndRandomizeSeedEachCycle.get()) {
        velRndSeed = std::uniform_int_distribution<int>(velRndSeed.getMin(), velRndSeed.getMax())(rng);
    }
    if(velStepRndRandomizeSeedEachCycle.get()) {
        velStepRndSeed = std::uniform_int_distribution<int>(velStepRndSeed.getMin(), velStepRndSeed.getMax())(rng);
    }
    if(bassOctRndRandomizeSeedEachCycle.get()) {
        bassOctRndSeed = std::uniform_int_distribution<int>(bassOctRndSeed.getMin(), bassOctRndSeed.getMax())(rng);
    }
}

void polyphonicArpeggiatorGUI::randomizeCycleStepShift() {
    currentCycleRandomStepShift = 0;

    float chance = ofClamp(rndShiftChance.get(), 0.0f, 1.0f);
    int range = std::max(0, rndShiftRange.get());
    int quant = std::max(1, rndShiftQuant.get());
    if(chance <= 0.0f || range <= 0) return;
    if(dist01(rng) > chance) return;

    int maxQuantizedSteps = range / quant;
    if(maxQuantizedSteps <= 0) return;

    int randomStep = std::uniform_int_distribution<int>(-maxQuantizedSteps, maxQuantizedSteps)(rng);
    currentCycleRandomStepShift = randomStep * quant;
}

double polyphonicArpeggiatorGUI::getRunGateBeatPosition() const {
    if(internalClockMode.get()) return currentTransportBeatPosition;
    return static_cast<double>(absoluteStepCounter) / std::max(0.001, static_cast<double>(beatDiv.get()));
}

float polyphonicArpeggiatorGUI::getRunGatePhaseNormalized() const {
    double beats = std::max(0.001, static_cast<double>(runGateBeats.get()));
    double shiftedBeats = getRunGateBeatPosition() + static_cast<double>(runGatePhase.get()) * beats;
    double normalized = shiftedBeats / beats;
    double phase = normalized - std::floor(normalized);
    if(phase < 0.0) phase += 1.0;
    return ofClamp(static_cast<float>(phase), 0.0f, 1.0f);
}

void polyphonicArpeggiatorGUI::updateRunGateWindowState() {
    double beats = std::max(0.001, static_cast<double>(runGateBeats.get()));
    double shiftedBeats = getRunGateBeatPosition() + static_cast<double>(runGatePhase.get()) * beats;
    int64_t windowIndex = static_cast<int64_t>(std::floor(shiftedBeats / beats));
    if(runGateWindowStateValid && runGateWindowIndex == windowIndex) return;

    runGateWindowIndex = windowIndex;
    runGateWindowStateValid = true;

    float chance = ofClamp(runGateChance.get(), 0.0f, 1.0f);
    if(chance <= 0.0f) runGateCurrentShouldPlay = false;
    else if(chance >= 1.0f) runGateCurrentShouldPlay = true;
    else runGateCurrentShouldPlay = dist01(rng) <= chance;
}

std::vector<float> polyphonicArpeggiatorGUI::buildSourceMaterialFromParameters() const {
    std::vector<float> sourceValues;

    if(sourceMode.get() == Scale) {
        const auto &scaleValues = scale.get();
        if(scaleValues.empty()) {
            sourceValues.push_back(0.0f);
        } else {
            sourceValues = scaleValues;
            std::sort(sourceValues.begin(), sourceValues.end());
            deduplicateApproximatePitches(sourceValues);
        }
    } else {
        sourceValues = notePoolIn.get();
        for(float &note : sourceValues) {
            note = ofClamp(note, 0.0f, 127.0f);
        }
        if(sortPool.get()) {
            std::stable_sort(sourceValues.begin(), sourceValues.end());
        }
        if(removeDuplicates.get()) {
            deduplicateApproximatePitches(sourceValues);
        }
        if(sourceValues.empty()) {
            sourceValues.push_back(60.0f);
        }
    }

    return sourceValues;
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

void polyphonicArpeggiatorGUI::syncPulseStepPatternToSequenceSize() {
    int size = std::max(1, seqSize.get());
    std::vector<int> pattern = pulseStepPattern.get();
    if(pattern.size() != static_cast<size_t>(size)) {
        size_t oldSize = pattern.size();
        pattern.resize(size, 100);
        for(size_t i = oldSize; i < pattern.size(); i++) {
            pattern[i] = 100;
        }
    }

    bool legacyBinaryPattern = !pattern.empty()
                            && std::all_of(pattern.begin(), pattern.end(), [](int value) { return value == 0 || value == 1; });
    if(legacyBinaryPattern) {
        for(int &value : pattern) value = value > 0 ? 100 : 0;
    }

    for(int &value : pattern) {
        value = ofClamp(value, 0, 100);
    }

    if(pattern != pulseStepPattern.get()) pulseStepPattern = pattern;
}

void polyphonicArpeggiatorGUI::syncAccentStepPatternToSequenceSize() {
    int size = std::max(1, seqSize.get());
    std::vector<int> pattern = accentStepPattern.get();
    if(pattern.size() != static_cast<size_t>(size)) {
        pattern.resize(size, 0);
    }

    for(int &value : pattern) value = value > 0 ? 1 : 0;

    if(pattern != accentStepPattern.get()) accentStepPattern = pattern;
}

void polyphonicArpeggiatorGUI::syncDurationStepPatternToSequenceSize() {
    int size = std::max(1, seqSize.get());
    std::vector<int> pattern = durationStepPattern.get();
    if(pattern.size() != static_cast<size_t>(size)) {
        pattern.resize(size, 0);
    }

    for(int &value : pattern) value = value > 0 ? 1 : 0;

    if(pattern != durationStepPattern.get()) durationStepPattern = pattern;
}

void polyphonicArpeggiatorGUI::syncVelocityCurveShape() {
    std::vector<float> curve = velocityCurve.get();
    if(curve.empty()) curve.assign(VelocityCurveResolution, 0.0f);

    if(curve.size() != static_cast<size_t>(VelocityCurveResolution)) {
        std::vector<float> resized(VelocityCurveResolution, 0.0f);
        if(curve.size() == 1) {
            std::fill(resized.begin(), resized.end(), ofClamp(curve.front(), 0.0f, 1.0f));
        } else {
            for(int i = 0; i < VelocityCurveResolution; i++) {
                float normalized = static_cast<float>(i) / static_cast<float>(VelocityCurveResolution - 1);
                float sourcePosition = normalized * static_cast<float>(curve.size() - 1);
                int leftIndex = ofClamp(static_cast<int>(std::floor(sourcePosition)), 0, static_cast<int>(curve.size()) - 1);
                int rightIndex = std::min(leftIndex + 1, static_cast<int>(curve.size()) - 1);
                float lerpAmount = sourcePosition - static_cast<float>(leftIndex);
                resized[i] = ofClamp(ofLerp(curve[leftIndex], curve[rightIndex], lerpAmount), 0.0f, 1.0f);
            }
        }
        curve = std::move(resized);
    } else {
        for(float &value : curve) value = ofClamp(value, 0.0f, 1.0f);
    }

    if(curve != velocityCurve.get()) velocityCurve = curve;
    syncVelocityCurveEditorStateFromCurve();
}

void polyphonicArpeggiatorGUI::syncVelocityCurveEditorStateFromCurve() {
    const std::vector<float> curve = velocityCurve.get();
    if(curve.empty()) {
        velocityCurveEditorStart = 0.0f;
        velocityCurveEditorEnd = 0.0f;
        velocityCurveEditorInflection = 0.5f;
        velocityCurveEditorSteepness = 1.0f;
        return;
    }

    velocityCurveEditorStart = ofClamp(curve.front(), 0.0f, 1.0f);
    velocityCurveEditorEnd = ofClamp(curve.back(), 0.0f, 1.0f);

    float amplitude = velocityCurveEditorEnd - velocityCurveEditorStart;
    if(std::abs(amplitude) <= 0.0001f) {
        velocityCurveEditorInflection = 0.5f;
        velocityCurveEditorSteepness = 1.0f;
        return;
    }

    int bestIndex = 0;
    float bestDistance = std::numeric_limits<float>::max();
    for(size_t i = 0; i < curve.size(); i++) {
        float normalizedValue = ofClamp((curve[i] - velocityCurveEditorStart) / amplitude, 0.0f, 1.0f);
        float distance = std::abs(normalizedValue - 0.5f);
        if(distance < bestDistance) {
            bestDistance = distance;
            bestIndex = static_cast<int>(i);
        }
    }

    velocityCurveEditorInflection = curve.size() <= 1
        ? 0.5f
        : ofClamp(static_cast<float>(bestIndex) / static_cast<float>(curve.size() - 1), 0.05f, 0.95f);

    int leftIndex = std::max(0, bestIndex - 1);
    int rightIndex = std::min(static_cast<int>(curve.size()) - 1, bestIndex + 1);
    float deltaX = std::max(1, rightIndex - leftIndex) / static_cast<float>(std::max(1, VelocityCurveResolution - 1));
    float normalizedLeft = ofClamp((curve[leftIndex] - velocityCurveEditorStart) / amplitude, 0.0f, 1.0f);
    float normalizedRight = ofClamp((curve[rightIndex] - velocityCurveEditorStart) / amplitude, 0.0f, 1.0f);
    float slope = std::abs(normalizedRight - normalizedLeft) / std::max(0.0001f, deltaX);
    velocityCurveEditorSteepness = ofClamp(slope * 4.0f, 0.1f, 8.0f);
}

void polyphonicArpeggiatorGUI::rebuildVelocityCurveFromEditorState() {
    std::vector<float> curve(VelocityCurveResolution, 0.0f);
    float inflection = ofClamp(velocityCurveEditorInflection, 0.05f, 0.95f);
    float steepness = ofClamp(velocityCurveEditorSteepness, 0.1f, 8.0f);
    float startValue = ofClamp(velocityCurveEditorStart, 0.0f, 1.0f);
    float endValue = ofClamp(velocityCurveEditorEnd, 0.0f, 1.0f);

    for(int i = 0; i < VelocityCurveResolution; i++) {
        float normalized = VelocityCurveResolution <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(VelocityCurveResolution - 1);
        float curveValue = evaluateVelocitySigmoid(normalized, inflection, steepness);
        curve[i] = ofClamp(ofLerp(startValue, endValue, curveValue), 0.0f, 1.0f);
    }

    velocityCurve = curve;
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

const char *polyphonicArpeggiatorGUI::getPulseModeLabel() const {
    switch(pulseMode.get()) {
        case PeriodicPulse: return "Periodic";
        case EuclideanPulse: return "Euclidean";
        case GeigerPulse: return "Geiger";
        case StepSeqPulse: return "StepSeq";
        default: return "Periodic";
    }
}

const char *polyphonicArpeggiatorGUI::getVelocityLfoShapeLabel() const {
    switch(velocityLfoShape.get()) {
        case VelocityLfoSin: return "Sin";
        case VelocityLfoSaw: return "Saw";
        case VelocityLfoInvSaw: return "InvSaw";
        case VelocityLfoPulse: return "Pulse";
        case VelocityLfoSlowRandom: return "SlowRandom";
        default: return "SlowRandom";
    }
}

void polyphonicArpeggiatorGUI::setBpm(float bpm) {
    currentBpm = std::max(1.0f, bpm);
}

void polyphonicArpeggiatorGUI::update(ofEventArgs &) {
    refreshSnapshotsFromSharedStorage();

    const auto frameState = getFrameTransportState();
    const auto &previousTransport = frameState.previous;
    const auto &currentTransport = frameState.current;
    currentBpm = std::max(1.0f, currentTransport.bpm);
    currentTransportBeatPosition = currentTransport.beatPosition;
    updateRunGateWindowState();

    uint64_t now = ofGetElapsedTimeMillis();
    bool needsUpdate = false;
    pruneOutputHistory(now);

    if(internalClockMode.get()) {
        if(pulseMode.get() == GeigerPulse && geigerPeriodicity.get() < 0.999f) {
            auto triggerGeigerPulse = [this](double beatPosition) {
                if(dist01(rng) > computeGeigerPulseProbability(beatPosition)) return;

                float microStepDurationMs = 60000.0f / (std::max(1.0f, currentBpm) * static_cast<float>(geigerTransportStepsPerBeat));
                geigerTransportPulseActive = true;
                pendingTransportOffsetMs = dist01(rng) * microStepDurationMs;
                onTrigger();
                pendingTransportOffsetMs = 0.0f;
                geigerTransportPulseActive = false;
            };

            if(ofxOceanodeTransportUtils::didTransportDiscontinuity(frameState)) {
                clearActiveVoices(true);
                internalClockNeedsSync = false;
                const bool returnedToStart = currentTransport.beatPosition <= transportResetBeatWindow &&
                                             (ofxOceanodeTransportUtils::didGenerationChange(frameState) ||
                                              ofxOceanodeTransportUtils::didBeatRewind(frameState, transportResetBeatWindow));
                if(currentTransport.isPlaying && returnedToStart) {
                    triggerGeigerPulse(currentTransport.beatPosition);
                }
            } else if(internalClockNeedsSync) {
                internalClockNeedsSync = false;
                if(currentTransport.isPlaying) {
                    triggerGeigerPulse(currentTransport.beatPosition);
                }
            } else if(currentTransport.isPlaying) {
                if(!previousTransport.isPlaying) {
                    triggerGeigerPulse(currentTransport.beatPosition);
                }

                const auto crossedTicks = ofxOceanodeTransportUtils::getCrossedStepRange(frameState, geigerTransportStepsPerBeat);
                if(crossedTicks.valid) {
                    for(int64_t tick = crossedTicks.firstStep; tick <= crossedTicks.lastStep; tick++) {
                        triggerGeigerPulse(static_cast<double>(tick) / geigerTransportStepsPerBeat);
                    }
                }
            }
        } else {
        const double stepsPerBeat = beatDiv.get();
        if(ofxOceanodeTransportUtils::didTransportDiscontinuity(frameState)) {
            clearActiveVoices(true);
            internalClockNeedsSync = false;
            const bool returnedToStart = currentTransport.beatPosition <= transportResetBeatWindow &&
                                         (ofxOceanodeTransportUtils::didGenerationChange(frameState) ||
                                          ofxOceanodeTransportUtils::didBeatRewind(frameState, transportResetBeatWindow));
            if(currentTransport.isPlaying &&
               (returnedToStart ||
                ofxOceanodeTransportUtils::isStepBoundary(currentTransport.beatPosition, stepsPerBeat))) {
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
    constexpr float baseTopRowHeight = 110.0f;
    constexpr float baseTabsHeight = 198.0f;
    constexpr float baseBottomHeight = 270.0f;
    constexpr float baseContentWidth = 1140.0f;
    ImVec2 available = ImGui::GetContentRegionAvail();
    float widthScale = available.x / std::max(1.0f, baseContentWidth);
    editorZoom = ofClamp(widthScale, 0.38f, 1.0f);
    float editorFontZoom = ofClamp(editorZoom + 0.08f, 0.46f, 1.0f);

    ImGui::SetWindowFontScale(editorFontZoom);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * editorZoom, 4.0f * editorZoom));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f * editorZoom, 3.0f * editorZoom));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * editorZoom, 8.0f * editorZoom));

    float gap = baseGap * editorZoom;
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float snapshotsWidth = std::max(400.0f * editorZoom, availableWidth * 0.40f);
    float bottomWidgetWidth = (availableWidth - gap) * 0.5f;
    float topRowHeight = baseTopRowHeight * editorZoom;
    float tabsHeight = baseTabsHeight * editorZoom;
    float bottomHeight = baseBottomHeight * editorZoom;
    float rhythmWidth = ofClamp(snapshotsWidth * 0.5f, 180.0f * editorZoom, std::max(180.0f * editorZoom, availableWidth * 0.28f));
    float editorWidth = std::max(220.0f * editorZoom, availableWidth - gap - rhythmWidth);

    beginColoredSection("ArpSnapshots", "Snapshots", ImVec2(snapshotsWidth, topRowHeight), snapshotsBg, snapshotsTitle, snapshotsSectionExpanded, editorFontZoom);
    if(snapshotsSectionExpanded) drawSnapshotsSection();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);
    beginColoredSection("ArpTop", "Transport", ImVec2(0, topRowHeight), topBg, topTitle, topSectionExpanded, editorFontZoom);
    if(topSectionExpanded) drawTopBarSection();
    ImGui::EndChild();

    beginColoredSection("ArpTabs", "Editor", ImVec2(editorWidth, tabsHeight), tabsBg, tabsTitle, tabsSectionExpanded, editorFontZoom);
    if(tabsSectionExpanded && ImGui::BeginTabBar("ArpEditorTabs")) {
        if(ImGui::BeginTabItem("SOURCE")) {
            drawSourceSection();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("GLOBAL")) {
            drawPatternSection();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("GATES")) {
            drawEuclideanSection();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("PITCH")) {
            drawPolyphonySection();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("BASS")) {
            drawBassSection();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("POSITION")) {
            drawPositionSection();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("VELOCITY")) {
            drawVelocitySection();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("ACCENTS")) {
            drawVelocityDurationSection();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("DURATIONS")) {
            drawDurationSection();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);
    beginColoredSection("ArpRhythm", "Rhythm", ImVec2(0, tabsHeight), rhythmBg, rhythmTitle, rhythmSectionExpanded, editorFontZoom);
    if(rhythmSectionExpanded) drawRhythmSection();
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

void polyphonicArpeggiatorGUI::drawTopBarSection() {
    float compactWidth = getCompactFieldWidth(84.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.22f);
    float beatPresetWidth = getCompactFieldWidth(138.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.30f);

    bool internalClock = internalClockMode.get();
    if(ImGui::Checkbox("Transport", &internalClock)) internalClockMode = internalClock;
    drawNodePublishContextMenu("internalClockMode", "Transport", 0.0f, true);
    ImGui::SameLine();

    bool oneShot = oneShotMode.get();
    if(ImGui::Checkbox("One Shot", &oneShot)) oneShotMode = oneShot;
    drawNodePublishContextMenu("oneShotMode", "One Shot", 0.0f, true);
    ImGui::SameLine();

    float division = beatDiv.get();
    ImGui::SetNextItemWidth(compactWidth);
    if(drawDraggableFloatWithPopup("Beat Div", division, 0.05f, beatDiv.getMin(), beatDiv.getMax(), "%.3f",
                                   [this]() { drawNodePublishMenuItems("beatDiv"); })) {
        beatDiv = division;
    }
    drawPublishedLabelUnderline("beatDiv", "Beat Div", compactWidth);
    ImGui::SameLine();

    const int beatPresetIndex = findBeatDivisionPresetIndex(beatDiv.get());
    const char *beatPresetLabel = beatPresetIndex >= 0 ? beatDivisionPresets[beatPresetIndex].label : "Custom";
    ImGui::SetNextItemWidth(beatPresetWidth);
    if(ImGui::BeginCombo("Beat Preset", beatPresetLabel)) {
        if(ImGui::Selectable("Custom", beatPresetIndex < 0)) {
        }
        for(size_t i = 0; i < beatDivisionPresets.size(); i++) {
            const bool selected = static_cast<int>(i) == beatPresetIndex;
            if(ImGui::Selectable(beatDivisionPresets[i].label, selected)) {
                beatDiv = beatDivisionPresets[i].value;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if(ImGui::Button("Trig")) onTrigger();
    drawNodePublishContextMenu("trigger", "Trig");
    ImGui::SameLine();
    if(ImGui::Button("Reset")) onReset();
    drawNodePublishContextMenu("reset", "Reset");
    ImGui::SameLine();
    if(ImGui::Button("Reset N")) onResetNext();
    drawNodePublishContextMenu("resetNext", "Reset N");

    std::string clockLabel = internalClockMode.get()
        ? "Transport " + describeBeatDiv(beatDiv.get())
        : "External trig";
    ImGui::TextDisabled("%s | BPM %.2f | Pulse %s", clockLabel.c_str(), currentBpm, getPulseModeLabel());
    if(oneShotMode.get()) {
        ImGui::TextDisabled("%s %d", oneShotCycleActive ? "One shot running:" : "One shot idle:", std::max(0, oneShotStepsRemaining));
    }
}

void polyphonicArpeggiatorGUI::drawSnapshotsSection() {
    refreshSnapshotsFromSharedStorage();

    float availableWidth = ImGui::GetContentRegionAvail().x;
    int selectedSlot = activeSnapshotSlot;
    auto selectedIt = snapshotSlots.find(selectedSlot);
    if(selectedIt == snapshotSlots.end() || !selectedIt->second.hasData) {
        selectedSlot = -1;
        for(const auto &entry : snapshotSlots) {
            if(entry.second.hasData) {
                selectedSlot = entry.first;
                break;
            }
        }
    }

    std::string preview = "Select";
    if(selectedSlot >= 0) {
        auto previewIt = snapshotSlots.find(selectedSlot);
        if(previewIt != snapshotSlots.end() && previewIt->second.hasData) {
            preview = ofToString(selectedSlot + 1) + ": " + previewIt->second.name;
        }
    }

    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float buttonWidth = std::max(28.0f * editorZoom, availableWidth * 0.06f);
    float saveWidth = std::max(48.0f * editorZoom, availableWidth * 0.10f);
    float loadWidth = std::max(150.0f * editorZoom, availableWidth * 0.44f - spacing);
    float nameWidth = std::max(100.0f * editorZoom, availableWidth * 0.24f - spacing);

    if(ImGui::Button("+", ImVec2(buttonWidth, 0.0f))) {
        storeToSlot(getNextAvailableSnapshotSlot());
    }
    ImGui::SameLine(0.0f, spacing);

    if(ImGui::Button("Save", ImVec2(saveWidth, 0.0f))) {
        if(activeSnapshotSlot >= 0 && snapshotSlots.count(activeSnapshotSlot) > 0) {
            storeToSlot(activeSnapshotSlot);
        } else {
            storeToSlot(getNextAvailableSnapshotSlot());
        }
    }
    ImGui::SameLine(0.0f, spacing);

    ImGui::SetNextItemWidth(loadWidth);
    if(ImGui::BeginCombo("Load", preview.c_str())) {
        for(const auto &entry : snapshotSlots) {
            if(!entry.second.hasData) continue;
            int slot = entry.first;
            bool selected = slot == activeSnapshotSlot;
            std::string label = ofToString(slot + 1) + ": " + entry.second.name;
            if(ImGui::Selectable(label.c_str(), selected)) recallSlot(slot);
            if(ImGui::BeginPopupContextItem(("SnapshotContext" + ofToString(slot)).c_str())) {
                if(ImGui::MenuItem("Delete Snapshot")) {
                    deleteSnapshotFromDisk(slot);
                }
                ImGui::EndPopup();
            }
            if(selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    drawNodePublishContextMenu("snapshotRecall", "Load", loadWidth);

    ImGui::SameLine(0.0f, spacing);

    auto activeIt = snapshotSlots.find(activeSnapshotSlot);
    if(activeIt != snapshotSlots.end() && activeIt->second.hasData) {
        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", activeIt->second.name.c_str());
        ImGui::SetNextItemWidth(nameWidth);
        if(ImGui::InputText("Nm", nameBuf, sizeof(nameBuf))) {
            activeIt->second.name = nameBuf;
            saveSnapshotToDisk(activeSnapshotSlot);
        }
    } else {
        ImGui::BeginDisabled();
        char emptyName[8] = "";
        ImGui::SetNextItemWidth(nameWidth);
        ImGui::InputText("Nm", emptyName, sizeof(emptyName), ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
    }

}

void polyphonicArpeggiatorGUI::drawSourceSection() {
    float compactWidth = getCompactFieldWidth(92.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.88f);
    int srcMode = sourceMode.get();
    if(ImGui::BeginTable("ArpSourceLayout", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::BeginCombo("Mode", srcMode == Scale ? "Scale" : "Pool")) {
            if(ImGui::Selectable("Scale", srcMode == Scale)) sourceMode = Scale;
            if(ImGui::Selectable("Chord Pool", srcMode == ChordPool)) sourceMode = ChordPool;
            ImGui::EndCombo();
        }
        drawNodePublishContextMenu("sourceMode", "Mode", compactWidth);

        bool resetOnSourceChange = sourceChangeMode.get() == ResetPattern;
        if(ImGui::Checkbox("Reset Seq", &resetOnSourceChange)) {
            sourceChangeMode = resetOnSourceChange ? ResetPattern : KeepPhase;
        }
        drawNodePublishContextMenu("sourceChangeMode", "Reset Seq", 0.0f, true);

        ImGui::TableNextColumn();
        if(srcMode == ChordPool) {
            bool sort = sortPool.get();
            if(ImGui::Checkbox("Sort", &sort)) sortPool = sort;
            drawNodePublishContextMenu("sortPool", "Sort", 0.0f, true);

            bool dedup = removeDuplicates.get();
            if(ImGui::Checkbox("Dedup", &dedup)) removeDuplicates = dedup;
            drawNodePublishContextMenu("removeDuplicates", "Dedup", 0.0f, true);
        } else {
            ImGui::TextWrapped("Scale degrees input.");
            ImGui::TextDisabled("Each value is treated as a degree/pitch source.");
        }

        ImGui::TableNextColumn();
        ImGui::TextDisabled(sourceChangePending ? "Pending source applies next step" : "Source applies on next step");
        size_t displayedSourceCount = (srcMode == ChordPool && sourceChangePending) ? pendingSourceValues.size() : activeSourceValues.size();
        ImGui::TextDisabled("%s n:%zu", srcMode == Scale ? "Src" : "Pool", displayedSourceCount);
        const std::vector<float> &displaySourceValues = (srcMode == ChordPool && sourceChangePending) ? pendingSourceValues : activeSourceValues;
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        ImGui::TextWrapped("%s", summarizeVector(srcMode == ChordPool ? displaySourceValues : scale.get(), 18).c_str());
        ImGui::PopTextWrapPos();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        std::vector<float> previewNotes = getSourcePreviewNotes();
        drawKeyboardDisplay("SourceKeyboard", previewNotes, nullptr, ImGui::GetContentRegionAvail().x, 60.0f * editorZoom, srcMode == ChordPool);

        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::EndTable();
    }
}

void polyphonicArpeggiatorGUI::drawPatternSection() {
    float compactWidth = getCompactFieldWidth(88.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.88f);
    int modeValue = patternMode.get();
    const char *modeLabel = modeValue == 0 ? "Asc"
                           : modeValue == 1 ? "Desc"
                           : modeValue == 2 ? "Rnd"
                           : modeValue == 3 ? "User"
                           : modeValue == 4 ? "Asc+Desc"
                           : "Desc+Asc";

    if(ImGui::BeginTable("ArpGlobalLayout", 5, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        int sizeValue = seqSize.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Seq", &sizeValue)) seqSize = ofClamp(sizeValue, seqSize.getMin(), seqSize.getMax());
        drawNodePublishContextMenu("seqSize", "Seq", compactWidth);

        int moduloValue = modulo.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Modulo", &moduloValue)) modulo = ofClamp(moduloValue, modulo.getMin(), modulo.getMax());
        drawNodePublishContextMenu("modulo", "Modulo", compactWidth);

        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::BeginCombo("Travel", modeLabel)) {
            if(ImGui::Selectable("Ascending", modeValue == 0)) patternMode = 0;
            if(ImGui::Selectable("Descending", modeValue == 1)) patternMode = 1;
            if(ImGui::Selectable("Random", modeValue == 2)) patternMode = 2;
            if(ImGui::Selectable("User", modeValue == 3)) patternMode = 3;
            if(ImGui::Selectable("Ascending + Descending", modeValue == 4)) patternMode = 4;
            if(ImGui::Selectable("Descending + Ascending", modeValue == 5)) patternMode = 5;
            ImGui::EndCombo();
        }
        drawNodePublishContextMenu("patternMode", "Travel", compactWidth);

        ImGui::TableNextColumn();
        int octaveValue = octave.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Oct", &octaveValue)) octave = ofClamp(octaveValue, octave.getMin(), octave.getMax());
        drawNodePublishContextMenu("octave", "Oct", compactWidth);

        int transposeValue = transpose.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Transp", &transposeValue)) transpose = ofClamp(transposeValue, transpose.getMin(), transpose.getMax());
        drawNodePublishContextMenu("transpose", "Transp", compactWidth);

        int octaveFoldValue = octaveFold.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Oct Fold", &octaveFoldValue)) octaveFold = ofClamp(octaveFoldValue, octaveFold.getMin(), octaveFold.getMax());
        drawNodePublishContextMenu("octaveFold", "Oct Fold", compactWidth);

        bool foldPolyValue = foldPoly.get();
        if(ImGui::Checkbox("Fold Poly", &foldPolyValue)) foldPoly = foldPolyValue;
        drawNodePublishContextMenu("foldPoly", "Fold Poly", 0.0f, true);

        ImGui::TableNextColumn();
        bool expand = pitchExpand.get();
        if(ImGui::Checkbox("Pitch Expand", &expand)) pitchExpand = expand;
        drawNodePublishContextMenu("pitchExpand", "Pitch Expand", 0.0f, true);

        int expandValue = expandStep.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Expand Step", &expandValue)) expandStep = ofClamp(expandValue, expandStep.getMin(), expandStep.getMax());
        drawNodePublishContextMenu("expandStep", "Expand Step", compactWidth);

        bool dynamic = dynamicMode.get();
        if(ImGui::Checkbox("Dynamic", &dynamic)) dynamicMode = dynamic;
        drawNodePublishContextMenu("dynamicMode", "Dynamic", 0.0f, true);

        ImGui::TextDisabled("Step: %d", wrapIndex(highlightedStep, std::max(1, seqSize.get())) + 1);

        ImGui::TableNextColumn();
        int stepShiftValue = stepShift.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Step Shift", &stepShiftValue)) stepShift = ofClamp(stepShiftValue, stepShift.getMin(), stepShift.getMax());
        drawNodePublishContextMenu("stepShift", "Step Shift", compactWidth);

        float rndShiftChanceValue = rndShiftChance.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::DragFloat("RndShift%", &rndShiftChanceValue, 0.01f, rndShiftChance.getMin(), rndShiftChance.getMax(), "%.2f")) {
            rndShiftChance = ofClamp(rndShiftChanceValue, rndShiftChance.getMin(), rndShiftChance.getMax());
        }
        drawNodePublishContextMenu("rndShiftChance", "RndShift%", compactWidth);

        int rndShiftRangeValue = rndShiftRange.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("RndShiftRange", &rndShiftRangeValue)) {
            rndShiftRange = ofClamp(rndShiftRangeValue, rndShiftRange.getMin(), rndShiftRange.getMax());
        }
        drawNodePublishContextMenu("rndShiftRange", "RndShiftRange", compactWidth);

        int rndShiftQuantValue = rndShiftQuant.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("RndShiftQ", &rndShiftQuantValue)) {
            rndShiftQuant = ofClamp(rndShiftQuantValue, rndShiftQuant.getMin(), rndShiftQuant.getMax());
        }
        drawNodePublishContextMenu("rndShiftQuant", "RndShiftQ", compactWidth);

        ImGui::TextDisabled("Active: %d", getEffectiveStepShift());
        ImGui::TextDisabled("+ right / - left");

        ImGui::TableNextColumn();
        if(patternMode.get() == 2) {
            int patternSeedValue = patternSeed.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Seed", &patternSeedValue)) patternSeed = ofClamp(patternSeedValue, patternSeed.getMin(), patternSeed.getMax());
            drawNodePublishContextMenu("patternSeed", "Seed", compactWidth);

            bool randomizeSeed = patternRandomizeSeedEachCycle.get();
            if(ImGui::Checkbox("Rndm Seed", &randomizeSeed)) patternRandomizeSeedEachCycle = randomizeSeed;
            drawNodePublishContextMenu("patternRandomizeSeedEachCycle", "Rndm Seed", 0.0f, true);

            ImGui::TextDisabled("Seeded random travel");
            ImGui::TextDisabled("repeats until reseeded.");
        } else if(patternMode.get() == 3) {
            ImGui::TextDisabled("User pattern");
            float availableHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y);
            float widgetHeight = std::max(1.0f, std::min(availableHeight, 126.0f * editorZoom));
            drawUserPatternEditor(ImGui::GetContentRegionAvail().x, widgetHeight);
        } else {
            ImGui::TextDisabled("User pattern editor");
            ImGui::TextDisabled("Select Travel = User");
            ImGui::TextDisabled("or Rnd for extra controls.");
        }
        ImGui::EndTable();
    }
}

void polyphonicArpeggiatorGUI::drawPolyphonySection() {
    const bool poolMode = sourceMode.get() == ChordPool;
    const char *startLabel = poolMode ? "Pool Start" : "Deg Start";
    const char *strideLabel = poolMode ? "Pool Stride" : "Deg Stride";

    if(ImGui::BeginTable("ArpPitchLayout", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("##PitchMain", ImGuiTableColumnFlags_WidthStretch, 1.55f);
        ImGui::TableSetupColumn("##PitchDev", ImGuiTableColumnFlags_WidthStretch, 1.25f);

        ImGui::TableNextColumn();
        float mainWidth = getCompactFieldWidth(84.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.84f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * editorZoom, 3.0f * editorZoom));
        if(ImGui::BeginTable("ArpPitchLayoutMain", 3, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            int startValue = sourceStart.get();
            ImGui::SetNextItemWidth(mainWidth);
            if(ImGui::InputInt(startLabel, &startValue)) sourceStart = ofClamp(startValue, sourceStart.getMin(), sourceStart.getMax());
            drawNodePublishContextMenu("sourceStart", startLabel, mainWidth);

            int strideValue = sourceStride.get();
            ImGui::SetNextItemWidth(mainWidth);
            if(ImGui::InputInt(strideLabel, &strideValue)) sourceStride = ofClamp(strideValue, sourceStride.getMin(), sourceStride.getMax());
            drawNodePublishContextMenu("sourceStride", strideLabel, mainWidth);

            int polyValue = polyphony.get();
            ImGui::SetNextItemWidth(mainWidth);
            if(ImGui::InputInt("Poly", &polyValue)) polyphony = ofClamp(polyValue, polyphony.getMin(), polyphony.getMax());
            drawNodePublishContextMenu("polyphony", "Poly", mainWidth);

            int intervalValue = polyInterval.get();
            ImGui::SetNextItemWidth(mainWidth);
            if(ImGui::InputInt("Voice Str", &intervalValue)) polyInterval = ofClamp(intervalValue, polyInterval.getMin(), polyInterval.getMax());
            drawNodePublishContextMenu("polyInterval", "Voice Str", mainWidth);

            ImGui::TableNextColumn();
            int polyAccentValue = polyAccent.get();
            ImGui::SetNextItemWidth(mainWidth);
            if(ImGui::InputInt("Poly Acc", &polyAccentValue)) polyAccent = ofClamp(polyAccentValue, polyAccent.getMin(), polyAccent.getMax());
            drawNodePublishContextMenu("polyAccent", "Poly Acc", mainWidth);
            ImGui::TextDisabled("Main note on");
            ImGui::TextDisabled("voice 0, extra");
            ImGui::TextDisabled("voices follow");
            ImGui::TextDisabled("Voice Str.");

            ImGui::TableNextColumn();
            int skipValue = skipSteps.get();
            ImGui::SetNextItemWidth(mainWidth);
            if(ImGui::InputInt("Skip", &skipValue)) skipSteps = ofClamp(skipValue, skipSteps.getMin(), skipSteps.getMax());
            drawNodePublishContextMenu("skipSteps", "Skip", mainWidth);
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();

        ImGui::TableNextColumn();
        float devWidth = getCompactFieldWidth(84.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.98f);
        ImGui::TextDisabled("Deviation");
        if(ImGui::BeginTable("ArpPitchLayoutDev", 3, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            float octProb = octaveDev.get();
            ImGui::SetNextItemWidth(devWidth);
            if(drawDraggableFloatWithPopup("Oct Prob", octProb, 0.01f, 0.0f, 1.0f, "%.2f",
                                           [this]() { drawNodePublishMenuItems("octaveDev"); })) octaveDev = octProb;
            drawPublishedLabelUnderline("octaveDev", "Oct Prob", devWidth);
            int octRange = octaveDevRng.get();
            ImGui::SetNextItemWidth(devWidth);
            if(ImGui::InputInt("Oct Range", &octRange)) octaveDevRng = ofClamp(octRange, octaveDevRng.getMin(), octaveDevRng.getMax());
            drawNodePublishContextMenu("octaveDevRng", "Oct Range", devWidth);

            ImGui::TableNextColumn();
            float idxProb = idxDev.get();
            ImGui::SetNextItemWidth(devWidth);
            if(drawDraggableFloatWithPopup("Idx Prob", idxProb, 0.01f, 0.0f, 1.0f, "%.2f",
                                           [this]() { drawNodePublishMenuItems("idxDev"); })) idxDev = idxProb;
            drawPublishedLabelUnderline("idxDev", "Idx Prob", devWidth);
            int idxRange = idxDevRng.get();
            ImGui::SetNextItemWidth(devWidth);
            if(ImGui::InputInt("Idx Range", &idxRange)) idxDevRng = ofClamp(idxRange, idxDevRng.getMin(), idxDevRng.getMax());
            drawNodePublishContextMenu("idxDevRng", "Idx Range", devWidth);

            ImGui::TableNextColumn();
            float pitchProb = pitchDev.get();
            ImGui::SetNextItemWidth(devWidth);
            if(drawDraggableFloatWithPopup("Pitch Prob", pitchProb, 0.01f, 0.0f, 1.0f, "%.2f",
                                           [this]() { drawNodePublishMenuItems("pitchDev"); })) pitchDev = pitchProb;
            drawPublishedLabelUnderline("pitchDev", "Pitch Prob", devWidth);
            int pitchRange = pitchDevRng.get();
            ImGui::SetNextItemWidth(devWidth);
            if(ImGui::InputInt("Pitch Rng", &pitchRange)) pitchDevRng = ofClamp(pitchRange, pitchDevRng.getMin(), pitchDevRng.getMax());
            drawNodePublishContextMenu("pitchDevRng", "Pitch Rng", devWidth);
            ImGui::EndTable();
        }

        ImGui::EndTable();
    }
}

void polyphonicArpeggiatorGUI::drawBassSection() {
    float compactWidth = getCompactFieldWidth(90.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.90f);
    if(ImGui::BeginTable("ArpBassLayout", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        bool addBassEnabled = addBass.get();
        if(ImGui::Checkbox("AddBass", &addBassEnabled)) addBass = addBassEnabled;
        drawNodePublishContextMenu("addBass", "AddBass", 0.0f, true);

        int patternValue = bassPatternMode.get();
        const char *patternLabel = patternValue == BassAlternatePattern ? "Alternate"
                                 : patternValue == BassEuclideanPattern ? "Euclidean"
                                 : patternValue == BassRandomPattern ? "Random"
                                 : patternValue == BassVelAccentedPattern ? "VelAccented"
                                 : patternValue == BassDurAccentedPattern ? "DurAccented"
                                 : "Start";
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::BeginCombo("Pattern", patternLabel)) {
            if(ImGui::Selectable("Alternate", patternValue == BassAlternatePattern)) bassPatternMode = BassAlternatePattern;
            if(ImGui::Selectable("Euclidean", patternValue == BassEuclideanPattern)) bassPatternMode = BassEuclideanPattern;
            if(ImGui::Selectable("Random", patternValue == BassRandomPattern)) bassPatternMode = BassRandomPattern;
            if(ImGui::Selectable("VelAccented", patternValue == BassVelAccentedPattern)) bassPatternMode = BassVelAccentedPattern;
            if(ImGui::Selectable("DurAccented", patternValue == BassDurAccentedPattern)) bassPatternMode = BassDurAccentedPattern;
            if(ImGui::Selectable("Start", patternValue == BassStartPattern)) bassPatternMode = BassStartPattern;
            ImGui::EndCombo();
        }
        drawNodePublishContextMenu("bassPatternMode", "Pattern", compactWidth);

        if(bassPatternMode.get() == BassAlternatePattern) {
            int everyValue = bassAlternateSteps.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Every", &everyValue)) bassAlternateSteps = ofClamp(everyValue, bassAlternateSteps.getMin(), bassAlternateSteps.getMax());
            drawNodePublishContextMenu("bassAlternateSteps", "Every", compactWidth);

            int shiftValue = bassAlternateShift.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Shift", &shiftValue)) bassAlternateShift = ofClamp(shiftValue, bassAlternateShift.getMin(), bassAlternateShift.getMax());
            drawNodePublishContextMenu("bassAlternateShift", "Shift", compactWidth);
        } else if(bassPatternMode.get() == BassEuclideanPattern) {
            int lenValue = bassEucLen.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Bass Len", &lenValue)) bassEucLen = ofClamp(lenValue, bassEucLen.getMin(), bassEucLen.getMax());
            drawNodePublishContextMenu("bassEucLen", "Bass Len", compactWidth);

            int hitsValue = bassEucHits.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Bass Hits", &hitsValue)) bassEucHits = ofClamp(hitsValue, bassEucHits.getMin(), bassEucHits.getMax());
            drawNodePublishContextMenu("bassEucHits", "Bass Hits", compactWidth);

            int offsetValue = bassEucOff.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Bass Off", &offsetValue)) bassEucOff = ofClamp(offsetValue, bassEucOff.getMin(), bassEucOff.getMax());
            drawNodePublishContextMenu("bassEucOff", "Bass Off", compactWidth);
        } else if(bassPatternMode.get() == BassRandomPattern) {
            ImGui::TextDisabled("Chance defines the");
            ImGui::TextDisabled("random bass density.");
        } else if(bassPatternMode.get() == BassVelAccentedPattern) {
            ImGui::TextDisabled("Bass follows the");
            ImGui::TextDisabled("velocity accents.");
        } else if(bassPatternMode.get() == BassDurAccentedPattern) {
            ImGui::TextDisabled("Bass follows the");
            ImGui::TextDisabled("duration accents.");
        } else {
            ImGui::TextDisabled("Bass only plays on");
            ImGui::TextDisabled("the first step.");
        }

        ImGui::TableNextColumn();
        float bassChance = bassProb.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Chance", bassChance, 0.01f, bassProb.getMin(), bassProb.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("bassProb"); })) bassProb = bassChance;
        drawPublishedLabelUnderline("bassProb", "Chance", compactWidth);

        int bassOctaveValue = bassOctave.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Bass Oct", &bassOctaveValue)) bassOctave = ofClamp(bassOctaveValue, bassOctave.getMin(), bassOctave.getMax());
        drawNodePublishContextMenu("bassOctave", "Bass Oct", compactWidth);

        int bassOctaveRandomRange = bassOctRnd.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Oct Rnd", &bassOctaveRandomRange)) bassOctRnd = ofClamp(bassOctaveRandomRange, bassOctRnd.getMin(), bassOctRnd.getMax());
        drawNodePublishContextMenu("bassOctRnd", "Oct Rnd", compactWidth);

        float bassOctaveRandomChance = bassOctRndChance.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Oct %", bassOctaveRandomChance, 0.01f, bassOctRndChance.getMin(), bassOctRndChance.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("bassOctRndChance"); })) bassOctRndChance = bassOctaveRandomChance;
        drawPublishedLabelUnderline("bassOctRndChance", "Oct %", compactWidth);

        int bassOctaveSeed = bassOctRndSeed.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Seed", &bassOctaveSeed)) bassOctRndSeed = ofClamp(bassOctaveSeed, bassOctRndSeed.getMin(), bassOctRndSeed.getMax());
        drawNodePublishContextMenu("bassOctRndSeed", "Seed", compactWidth);

        bool bassRandomizeSeed = bassOctRndRandomizeSeedEachCycle.get();
        if(ImGui::Checkbox("Rndm Seed", &bassRandomizeSeed)) bassOctRndRandomizeSeedEachCycle = bassRandomizeSeed;
        drawNodePublishContextMenu("bassOctRndRandomizeSeedEachCycle", "Rndm Seed", 0.0f, true);

        ImGui::TableNextColumn();
        int bassPolyphonyValue = bassPolyphony.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Poly", &bassPolyphonyValue)) bassPolyphony = ofClamp(bassPolyphonyValue, bassPolyphony.getMin(), bassPolyphony.getMax());
        drawNodePublishContextMenu("bassPolyphony", "Poly", compactWidth);

        float bassVelocityBase = bassVelBase.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Vel", bassVelocityBase, 0.01f, bassVelBase.getMin(), bassVelBase.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("bassVelBase"); })) bassVelBase = bassVelocityBase;
        drawPublishedLabelUnderline("bassVelBase", "Vel", compactWidth);

        float bassVelocityRandom = bassVelRndm.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Vel Rnd", bassVelocityRandom, 0.01f, bassVelRndm.getMin(), bassVelRndm.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("bassVelRndm"); })) bassVelRndm = bassVelocityRandom;
        drawPublishedLabelUnderline("bassVelRndm", "Vel Rnd", compactWidth);

        bool pitchModified = bassPitchModified.get();
        if(ImGui::Checkbox("Pitch Mod", &pitchModified)) bassPitchModified = pitchModified;
        drawNodePublishContextMenu("bassPitchModified", "Pitch Mod", 0.0f, true);

        ImGui::TextDisabled("Uses root + transpose.");
        ImGui::TextDisabled("Pitch Mod adds main octave.");
        ImGui::EndTable();
    }
}

void polyphonicArpeggiatorGUI::drawPositionSection() {
    float compactWidth = getCompactFieldWidth(90.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.90f);
    if(ImGui::BeginTable("ArpPositionLayout", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        float strumValue = strum.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Strum", strumValue, 0.01f, strum.getMin(), strum.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("strum"); })) strum = strumValue;
        drawPublishedLabelUnderline("strum", "Strum", compactWidth);

        float strumRndValue = strumRndm.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Str Rnd", strumRndValue, 0.01f, strumRndm.getMin(), strumRndm.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("strumRndm"); })) strumRndm = strumRndValue;
        drawPublishedLabelUnderline("strumRndm", "Str Rnd", compactWidth);

        ImGui::TableNextColumn();
        float swingValue = swing.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Swing", swingValue, 0.01f, swing.getMin(), swing.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("swing"); })) swing = swingValue;
        drawPublishedLabelUnderline("swing", "Swing", compactWidth);

        int direction = strumDir.get();
        const char *dirLabel = direction == 0 ? "Asc" : direction == 1 ? "Desc" : "Rnd";
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::BeginCombo("Str Dir", dirLabel)) {
            if(ImGui::Selectable("Ascending", direction == 0)) strumDir = 0;
            if(ImGui::Selectable("Descending", direction == 1)) strumDir = 1;
            if(ImGui::Selectable("Random", direction == 2)) strumDir = 2;
            ImGui::EndCombo();
        }
        drawNodePublishContextMenu("strumDir", "Str Dir", compactWidth);

        float positionRandomValue = positionRndm.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Rnd Offset", positionRandomValue, 0.01f, positionRndm.getMin(), positionRndm.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("positionRndm"); })) positionRndm = positionRandomValue;
        drawPublishedLabelUnderline("positionRndm", "Rnd Offset", compactWidth);

        int timingSeedValue = strumRndSeed.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Seed", &timingSeedValue)) strumRndSeed = ofClamp(timingSeedValue, strumRndSeed.getMin(), strumRndSeed.getMax());
        drawNodePublishContextMenu("strumRndSeed", "Seed", compactWidth);

        bool timingRandomizeSeed = strumRndRandomizeSeedEachCycle.get();
        if(ImGui::Checkbox("Rndm Seed", &timingRandomizeSeed)) strumRndRandomizeSeedEachCycle = timingRandomizeSeed;
        drawNodePublishContextMenu("strumRndRandomizeSeedEachCycle", "Rndm Seed", 0.0f, true);

        ImGui::TableNextColumn();
        ImGui::TextDisabled("Timing parameters use");
        ImGui::TextDisabled("step units.");
        ImGui::TextDisabled("1.00 = one full step.");
        ImGui::TextDisabled("Swing delays every");
        ImGui::TextDisabled("second step.");
        ImGui::TextDisabled("Random offset adds");
        ImGui::TextDisabled("an extra positive delay.");
        ImGui::EndTable();
    }
}

void polyphonicArpeggiatorGUI::drawVelocitySection() {
    float compactWidth = getCompactFieldWidth(92.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.92f);
    if(ImGui::BeginTable("ArpVelocityLayout", 4, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("##VelControlsA", ImGuiTableColumnFlags_WidthStretch, 0.56f);
        ImGui::TableSetupColumn("##VelLfo", ImGuiTableColumnFlags_WidthStretch, 0.72f);
        ImGui::TableSetupColumn("##VelScope", ImGuiTableColumnFlags_WidthStretch, 0.92f);
        ImGui::TableSetupColumn("##VelCurve", ImGuiTableColumnFlags_WidthStretch, 0.92f);

        ImGui::TableNextColumn();
        float baseVel = velBase.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Base", baseVel, 0.01f, velBase.getMin(), velBase.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("velBase"); })) velBase = baseVel;
        drawPublishedLabelUnderline("velBase", "Base", compactWidth);

        float noteRandomVel = velRndm.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Note Rnd", noteRandomVel, 0.01f, velRndm.getMin(), velRndm.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("velRndm"); })) velRndm = noteRandomVel;
        drawPublishedLabelUnderline("velRndm", "Note Rnd", compactWidth);

        float stepRandomVel = velStepRndm.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Step Rnd", stepRandomVel, 0.01f, velStepRndm.getMin(), velStepRndm.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("velStepRndm"); })) velStepRndm = stepRandomVel;
        drawPublishedLabelUnderline("velStepRndm", "Step Rnd", compactWidth);

        int velocitySeedValue = velRndSeed.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Seed", &velocitySeedValue)) velRndSeed = ofClamp(velocitySeedValue, velRndSeed.getMin(), velRndSeed.getMax());
        drawNodePublishContextMenu("velRndSeed", "Seed", compactWidth);

        bool velocityRandomizeSeed = velRndRandomizeSeedEachCycle.get();
        if(ImGui::Checkbox("Rndm Seed", &velocityRandomizeSeed)) velRndRandomizeSeedEachCycle = velocityRandomizeSeed;
        drawNodePublishContextMenu("velRndRandomizeSeedEachCycle", "Rndm Seed", 0.0f, true);

        ImGui::TableNextColumn();
        int lfoShapeValue = velocityLfoShape.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::BeginCombo("LFO", getVelocityLfoShapeLabel())) {
            if(ImGui::Selectable("Sin", lfoShapeValue == VelocityLfoSin)) velocityLfoShape = VelocityLfoSin;
            if(ImGui::Selectable("Saw", lfoShapeValue == VelocityLfoSaw)) velocityLfoShape = VelocityLfoSaw;
            if(ImGui::Selectable("InvSaw", lfoShapeValue == VelocityLfoInvSaw)) velocityLfoShape = VelocityLfoInvSaw;
            if(ImGui::Selectable("Pulse", lfoShapeValue == VelocityLfoPulse)) velocityLfoShape = VelocityLfoPulse;
            if(ImGui::Selectable("SlowRandom", lfoShapeValue == VelocityLfoSlowRandom)) velocityLfoShape = VelocityLfoSlowRandom;
            ImGui::EndCombo();
        }
        drawNodePublishContextMenu("velocityLfoShape", "LFO", compactWidth);

        float lfoAmount = velocitySlowRndm.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Amt", lfoAmount, 0.01f, velocitySlowRndm.getMin(), velocitySlowRndm.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("velocitySlowRndm"); })) velocitySlowRndm = lfoAmount;
        drawPublishedLabelUnderline("velocitySlowRndm", "Amt", compactWidth);

        float lfoSpeed = velocitySlowSpeed.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Speed", lfoSpeed, 0.01f, velocitySlowSpeed.getMin(), velocitySlowSpeed.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("velocitySlowSpeed"); })) velocitySlowSpeed = lfoSpeed;
        drawPublishedLabelUnderline("velocitySlowSpeed", "Speed", compactWidth);

        float lfoPhase = velocityLfoPhase.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Phase", lfoPhase, 0.01f, velocityLfoPhase.getMin(), velocityLfoPhase.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("velocityLfoPhase"); })) velocityLfoPhase = lfoPhase;
        drawPublishedLabelUnderline("velocityLfoPhase", "Phase", compactWidth);

        if(velocityLfoShape.get() == VelocityLfoPulse) {
            float pulseWidth = velocityLfoPulseWidth.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(drawDraggableFloatWithPopup("Width", pulseWidth, 0.01f, velocityLfoPulseWidth.getMin(), velocityLfoPulseWidth.getMax(), "%.2f",
                                           [this]() { drawNodePublishMenuItems("velocityLfoPulseWidth"); })) velocityLfoPulseWidth = pulseWidth;
            drawPublishedLabelUnderline("velocityLfoPulseWidth", "Width", compactWidth);
        }

        int lfoSeedValue = velocitySlowSeed.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::InputInt("Seed", &lfoSeedValue)) velocitySlowSeed = ofClamp(lfoSeedValue, velocitySlowSeed.getMin(), velocitySlowSeed.getMax());
        drawNodePublishContextMenu("velocitySlowSeed", "Seed", compactWidth);

        ImGui::TableNextColumn();
        float scopeWidth = ImGui::GetContentRegionAvail().x;
        drawVelocityLfoScope(scopeWidth,
                             std::max(124.0f * editorZoom, ImGui::GetContentRegionAvail().y));

        ImGui::TableNextColumn();
        float curveWidth = ImGui::GetContentRegionAvail().x;
        drawVelocityCurveEditor(curveWidth,
                                std::max(124.0f * editorZoom, ImGui::GetContentRegionAvail().y));
        ImGui::EndTable();
    }
}

void polyphonicArpeggiatorGUI::drawVelocityLfoScope(float width, float height) const {
    width = std::max(1.0f, width);
    height = std::max(72.0f * editorZoom, height);

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size(width, height);
    ImVec2 end(origin.x + size.x, origin.y + size.y);
    ImGui::InvisibleButton("##VelocityLfoScope", size);

    const ImU32 bg = IM_COL32(18, 28, 42, 255);
    const ImU32 border = IM_COL32(86, 124, 168, 230);
    const ImU32 guide = IM_COL32(112, 150, 190, 42);
    const ImU32 waveColor = IM_COL32(196, 236, 255, 255);
    const ImU32 playheadColor = IM_COL32(255, 208, 124, 220);

    drawList->AddRectFilled(origin, end, bg, 8.0f);
    drawList->AddRect(origin, end, border, 8.0f, 0, 1.3f);
    for(int i = 1; i < 4; i++) {
        float y = ofLerp(origin.y, end.y, static_cast<float>(i) / 4.0f);
        drawList->AddLine(ImVec2(origin.x, y), ImVec2(end.x, y), guide, 1.0f);
    }
    for(int i = 1; i < 8; i++) {
        float x = ofLerp(origin.x, end.x, static_cast<float>(i) / 8.0f);
        drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, end.y), guide, 1.0f);
    }

    std::vector<ImVec2> points;
    constexpr int ScopeSamples = 96;
    points.reserve(ScopeSamples);
    double startBeat = currentTransportBeatPosition;
    double beatWindow = 4.0 / std::max(0.001f, velocitySlowSpeed.get() <= 0.0f ? 1.0f : velocitySlowSpeed.get());
    for(int i = 0; i < ScopeSamples; i++) {
        float normalizedX = ScopeSamples <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(ScopeSamples - 1);
        double beat = startBeat + normalizedX * beatWindow;
        float value = evaluateVelocityLfoWaveform(beat);
        float x = ofLerp(origin.x + 4.0f, end.x - 4.0f, normalizedX);
        float y = ofLerp(end.y - 6.0f, origin.y + 6.0f, value);
        points.emplace_back(x, y);
    }

    if(!points.empty()) {
        drawList->AddPolyline(points.data(), static_cast<int>(points.size()), waveColor, false, 2.0f);
        drawList->AddLine(ImVec2(origin.x + 4.0f, origin.y + 4.0f),
                          ImVec2(origin.x + 4.0f, end.y - 4.0f),
                          playheadColor,
                          1.4f);
    }

    drawList->AddText(ImVec2(origin.x + 8.0f, origin.y + 6.0f), IM_COL32(220, 236, 252, 185), "LFO Scope");
}

void polyphonicArpeggiatorGUI::drawVelocityCurveEditor(float width, float height) {
    width = std::max(1.0f, width);
    height = std::max(110.0f * editorZoom, height);

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size(width, height);
    ImVec2 end(origin.x + size.x, origin.y + size.y);
    ImGui::InvisibleButton("##VelocityCurveEditor", size);

    const ImU32 bg = IM_COL32(18, 28, 42, 255);
    const ImU32 border = IM_COL32(86, 124, 168, 230);
    const ImU32 guide = IM_COL32(112, 150, 190, 48);
    const ImU32 curveColor = IM_COL32(216, 236, 255, 255);
    const ImU32 fillColor = IM_COL32(112, 176, 236, 38);
    const ImU32 pointColor = IM_COL32(255, 255, 255, 255);
    const ImU32 pointSelectedColor = IM_COL32(255, 200, 100, 255);
    const ImU32 tensionMarkerColor = IM_COL32(255, 208, 124, 220);
    const ImU32 tensionHintColor = IM_COL32(132, 234, 182, 120);

    drawList->AddRectFilled(origin, end, bg, 8.0f);
    drawList->AddRect(origin, end, border, 8.0f, 0, 1.4f);
    for(int i = 1; i < 4; i++) {
        float y = ofLerp(origin.y, end.y, static_cast<float>(i) / 4.0f);
        drawList->AddLine(ImVec2(origin.x, y), ImVec2(end.x, y), guide, 1.0f);
    }
    for(int i = 1; i < 8; i++) {
        float x = ofLerp(origin.x, end.x, static_cast<float>(i) / 8.0f);
        drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, end.y), guide, 1.0f);
    }

    auto valueToY = [&](float value) {
        return ofLerp(end.y - 6.0f, origin.y + 6.0f, ofClamp(value, 0.0f, 1.0f));
    };
    auto yToValue = [&](float y) {
        return ofClamp((end.y - 6.0f - y) / std::max(1.0f, (end.y - origin.y - 12.0f)), 0.0f, 1.0f);
    };
    auto posToX = [&](float normalizedPosition) {
        return ofLerp(origin.x + 6.0f, end.x - 6.0f, ofClamp(normalizedPosition, 0.0f, 1.0f));
    };
    auto xToPos = [&](float x) {
        return ofClamp((x - (origin.x + 6.0f)) / std::max(1.0f, (end.x - origin.x - 12.0f)), 0.0f, 1.0f);
    };

    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool isHovered = ImGui::IsItemHovered();
    bool isLeftClick = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool isDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    bool isReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    bool isAltHeld = ImGui::GetIO().KeyAlt;

    ImVec2 startPoint(origin.x + 6.0f, valueToY(velocityCurveEditorStart));
    ImVec2 endPoint(end.x - 6.0f, valueToY(velocityCurveEditorEnd));
    float inflection = ofClamp(velocityCurveEditorInflection, 0.05f, 0.95f);
    float steepness = ofClamp(velocityCurveEditorSteepness, 0.1f, 8.0f);
    float inflectionValue = ofLerp(velocityCurveEditorStart, velocityCurveEditorEnd, evaluateVelocitySigmoid(inflection, inflection, steepness));
    ImVec2 inflectionPoint(posToX(inflection), valueToY(inflectionValue));

    auto pointHovered = [&](const ImVec2 &point) {
        return std::abs(mouse.x - point.x) < 8.0f * editorZoom && std::abs(mouse.y - point.y) < 8.0f * editorZoom;
    };
    bool startHovered = pointHovered(startPoint);
    bool endHovered = pointHovered(endPoint);

    if(isLeftClick) {
        velocityCurveActiveHandle = 0;
        if(startHovered) {
            velocityCurveActiveHandle = 1;
        } else if(endHovered) {
            velocityCurveActiveHandle = 2;
        } else if(isAltHeld) {
            velocityCurveActiveHandle = 3;
            velocityCurveDragStartMouseX = mouse.x;
            velocityCurveDragStartMouseY = mouse.y;
            velocityCurveDragStartInflection = velocityCurveEditorInflection;
            velocityCurveDragStartSteepness = velocityCurveEditorSteepness;
        }
    }

    if(isDragging && velocityCurveActiveHandle != 0) {
        if(velocityCurveActiveHandle == 1) {
            velocityCurveEditorStart = yToValue(mouse.y);
        } else if(velocityCurveActiveHandle == 2) {
            velocityCurveEditorEnd = yToValue(mouse.y);
        } else if(velocityCurveActiveHandle == 3) {
            float deltaX = mouse.x - velocityCurveDragStartMouseX;
            float deltaY = mouse.y - velocityCurveDragStartMouseY;
            velocityCurveEditorInflection = ofClamp(velocityCurveDragStartInflection + deltaX / std::max(1.0f, width - 12.0f), 0.01f, 0.99f);
            float steepnessDelta = -deltaY / std::max(1.0f, height / 3.0f);
            float newSteepness = velocityCurveDragStartSteepness * std::exp(steepnessDelta * 0.5f);
            velocityCurveEditorSteepness = ofClamp(newSteepness, 0.1f, 8.0f);
        }
        rebuildVelocityCurveFromEditorState();
    }

    if(isReleased) {
        velocityCurveActiveHandle = 0;
    }

    std::vector<float> curve = velocityCurve.get();
    if(curve.empty()) return;

    std::vector<ImVec2> linePoints;
    std::vector<ImVec2> fillPoints;
    linePoints.reserve(curve.size());
    fillPoints.reserve(curve.size() + 2);
    fillPoints.push_back(ImVec2(origin.x + 6.0f, end.y - 6.0f));

    for(size_t i = 0; i < curve.size(); i++) {
        float normalized = curve.size() <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(curve.size() - 1);
        ImVec2 point(posToX(normalized), valueToY(curve[i]));
        linePoints.push_back(point);
        fillPoints.push_back(point);
    }
    fillPoints.push_back(ImVec2(end.x - 6.0f, end.y - 6.0f));

    drawList->AddConvexPolyFilled(fillPoints.data(), static_cast<int>(fillPoints.size()), fillColor);
    drawList->AddPolyline(linePoints.data(), static_cast<int>(linePoints.size()), curveColor, false, 2.2f);
    if(isAltHeld || velocityCurveActiveHandle == 3) {
        drawList->AddLine(ImVec2(inflectionPoint.x, origin.y + 6.0f),
                          ImVec2(inflectionPoint.x, end.y - 6.0f),
                          tensionHintColor,
                          1.0f);
    }
    drawList->AddCircleFilled(startPoint,
                              (startHovered || velocityCurveActiveHandle == 1) ? 6.0f * editorZoom : 4.5f * editorZoom,
                              velocityCurveActiveHandle == 1 ? pointSelectedColor : pointColor);
    drawList->AddCircle(startPoint, (startHovered || velocityCurveActiveHandle == 1) ? 6.0f * editorZoom : 4.5f * editorZoom,
                        IM_COL32(42, 50, 60, 255), 12, 1.4f);
    drawList->AddCircleFilled(endPoint,
                              (endHovered || velocityCurveActiveHandle == 2) ? 6.0f * editorZoom : 4.5f * editorZoom,
                              velocityCurveActiveHandle == 2 ? pointSelectedColor : pointColor);
    drawList->AddCircle(endPoint, (endHovered || velocityCurveActiveHandle == 2) ? 6.0f * editorZoom : 4.5f * editorZoom,
                        IM_COL32(42, 50, 60, 255), 12, 1.4f);
    drawList->AddCircleFilled(inflectionPoint, 4.0f * editorZoom, tensionMarkerColor);

    drawList->AddText(ImVec2(origin.x + 10.0f, origin.y + 8.0f), IM_COL32(220, 236, 252, 190), "Velocity Curve");
    drawList->AddText(ImVec2(origin.x + 10.0f, end.y - 18.0f),
                      IM_COL32(186, 208, 230, 150),
                      "Drag points. Alt+drag: X=Inflection Y=Steepness");

    if(isAltHeld && isHovered) {
        char tensionInfo[80];
        std::snprintf(tensionInfo, sizeof(tensionInfo), "p=%.2f  k=%.2f", velocityCurveEditorInflection, velocityCurveEditorSteepness);
        drawList->AddText(ImVec2(mouse.x + 10.0f, mouse.y - 18.0f), IM_COL32(255, 244, 182, 255), tensionInfo);
    }
}

float polyphonicArpeggiatorGUI::evaluateVelocitySigmoid(float normalizedPosition, float inflection, float steepness) const {
    float x = ofClamp(normalizedPosition, 0.0f, 1.0f);
    float p = ofClamp(inflection, 0.01f, 0.99f);
    float k = ofClamp(steepness, 0.1f, 8.0f);
    const float epsilon = 0.0001f;
    if(x < epsilon) return 0.0f;
    if(x > 1.0f - epsilon) return 1.0f;

    float xSafe = ofClamp(x, epsilon, 1.0f - epsilon);
    float pSafe = ofClamp(p, epsilon, 1.0f - epsilon);
    float a = std::pow(xSafe / pSafe, k);
    float b = std::pow((1.0f - xSafe) / (1.0f - pSafe), k);
    float denominator = a + b;
    if(denominator < epsilon) return 0.5f;
    return ofClamp(a / denominator, 0.0f, 1.0f);
}

void polyphonicArpeggiatorGUI::drawEuclideanSection() {
    float compactWidth = getCompactFieldWidth(88.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.88f);
    if(ImGui::BeginTable("ArpGatesLayout", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        int currentPulseMode = pulseMode.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::BeginCombo("Pulse", getPulseModeLabel())) {
            if(ImGui::Selectable("Periodic", currentPulseMode == PeriodicPulse)) pulseMode = PeriodicPulse;
            if(ImGui::Selectable("Euclidean", currentPulseMode == EuclideanPulse)) pulseMode = EuclideanPulse;
            if(ImGui::Selectable("Geiger", currentPulseMode == GeigerPulse)) pulseMode = GeigerPulse;
            if(ImGui::Selectable("StepSeq", currentPulseMode == StepSeqPulse)) pulseMode = StepSeqPulse;
            ImGui::EndCombo();
        }
        drawNodePublishContextMenu("pulseMode", "Pulse", compactWidth);

        if(pulseMode.get() == PeriodicPulse) {
            ImGui::TextDisabled("Periodic pulses at the current beat division.");
        } else if(pulseMode.get() == EuclideanPulse) {
            int gateLen = eucLen.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Gate Len", &gateLen)) eucLen = ofClamp(gateLen, eucLen.getMin(), eucLen.getMax());
            drawNodePublishContextMenu("eucLen", "Gate Len", compactWidth);
            int gateHits = eucHits.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Gate Hits", &gateHits)) eucHits = ofClamp(gateHits, eucHits.getMin(), eucHits.getMax());
            drawNodePublishContextMenu("eucHits", "Gate Hits", compactWidth);
            int gateOff = eucOff.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Gate Off", &gateOff)) eucOff = ofClamp(gateOff, eucOff.getMin(), eucOff.getMax());
            drawNodePublishContextMenu("eucOff", "Gate Off", compactWidth);
        } else if(pulseMode.get() == GeigerPulse) {
            float speed = geigerSpeed.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(drawDraggableFloatWithPopup("Speed", speed, 0.01f, geigerSpeed.getMin(), geigerSpeed.getMax(), "%.2f",
                                           [this]() { drawNodePublishMenuItems("geigerSpeed"); })) geigerSpeed = speed;
            drawPublishedLabelUnderline("geigerSpeed", "Speed", compactWidth);

            float density = geigerDensity.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(drawDraggableFloatWithPopup("Density", density, 0.01f, 0.0f, 1.0f, "%.2f",
                                           [this]() { drawNodePublishMenuItems("geigerDensity"); })) geigerDensity = density;
            drawPublishedLabelUnderline("geigerDensity", "Density", compactWidth);

            float periodicity = geigerPeriodicity.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(drawDraggableFloatWithPopup("Periodicity", periodicity, 0.01f, 0.0f, 1.0f, "%.2f",
                                           [this]() { drawNodePublishMenuItems("geigerPeriodicity"); })) geigerPeriodicity = periodicity;
            drawPublishedLabelUnderline("geigerPeriodicity", "Periodicity", compactWidth);
            ImGui::TextDisabled("Periodic = clustered");
            ImGui::TextDisabled("Low periodicity = diffuse");
        } else {
            ImGui::TextDisabled("Draw a per-step pulse pattern below.");
        }

        ImGui::TableNextColumn();
        float sequenceProb = seqProb.get();
        float cycleWidth = getCompactFieldWidth(54.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.34f);
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Seq%", sequenceProb, 0.01f, 0.0f, 1.0f, "%.2f",
                                       [this]() { drawNodePublishMenuItems("seqProb"); })) seqProb = sequenceProb;
        drawPublishedLabelUnderline("seqProb", "Seq%", compactWidth);
        ImGui::SameLine();
        int sequenceCycles = seqProbCycles.get();
        ImGui::SetNextItemWidth(cycleWidth);
        if(ImGui::InputInt("##SeqCycles", &sequenceCycles, 0, 0)) seqProbCycles = ofClamp(sequenceCycles, seqProbCycles.getMin(), seqProbCycles.getMax());
        drawNodePublishContextMenu("seqProbCycles");
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Mute-block length in full cycles while preserving the overall Seq%% rate");
        ImGui::SameLine();
        ImGui::TextDisabled("cyc");
        drawPublishedCurrentItemUnderline("seqProbCycles");

        float stepProb = stepChance.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Step%", stepProb, 0.01f, 0.0f, 1.0f, "%.2f",
                                       [this]() { drawNodePublishMenuItems("stepChance"); })) stepChance = stepProb;
        drawPublishedLabelUnderline("stepChance", "Step%", compactWidth);

        float noteProb = noteChance.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Note%", noteProb, 0.01f, 0.0f, 1.0f, "%.2f",
                                       [this]() { drawNodePublishMenuItems("noteChance"); })) noteChance = noteProb;
        drawPublishedLabelUnderline("noteChance", "Note%", compactWidth);

        ImGui::TableNextColumn();
        float runBeatsValue = runGateBeats.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Run Beats", runBeatsValue, 0.25f, runGateBeats.getMin(), runGateBeats.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("runGateBeats"); })) runGateBeats = runBeatsValue;
        drawPublishedLabelUnderline("runGateBeats", "Run Beats", compactWidth);

        float runChanceValue = runGateChance.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Run%", runChanceValue, 0.01f, 0.0f, 1.0f, "%.2f",
                                       [this]() { drawNodePublishMenuItems("runGateChance"); })) runGateChance = runChanceValue;
        drawPublishedLabelUnderline("runGateChance", "Run%", compactWidth);

        float runPhaseValue = runGatePhase.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Phase", runPhaseValue, 0.01f, 0.0f, 1.0f, "%.2f",
                                       [this]() { drawNodePublishMenuItems("runGatePhase"); })) runGatePhase = runPhaseValue;
        drawPublishedLabelUnderline("runGatePhase", "Phase", compactWidth);

        drawRunGatePhasor(ImGui::GetContentRegionAvail().x, 54.0f * editorZoom);

        if(pulseMode.get() == StepSeqPulse) {
            ImGui::Spacing();
            drawPulseStepPatternEditor(ImGui::GetContentRegionAvail().x, 70.0f * editorZoom);
        } else {
            ImGui::TextDisabled("StepSeq editor appears");
            ImGui::TextDisabled("when Pulse = StepSeq.");
        }
        ImGui::EndTable();
    }
}

void polyphonicArpeggiatorGUI::drawVelocityDurationSection() {
    float compactWidth = getCompactFieldWidth(88.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.88f);
    if(ImGui::BeginTable("ArpAccentsLayout", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        int accentModeValue = accentPatternMode.get();
        const char *accentModeLabel = accentModeValue == AlternateEventPattern ? "Alternate"
                                     : accentModeValue == EuclideanEventPattern ? "Euclidean"
                                     : accentModeValue == RandomStepEventPattern ? "RandomStep"
                                     : accentModeValue == RandomNoteEventPattern ? "RandomNote"
                                     : "StepSeq";
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::BeginCombo("Pattern", accentModeLabel)) {
            if(ImGui::Selectable("Alternate", accentModeValue == AlternateEventPattern)) accentPatternMode = AlternateEventPattern;
            if(ImGui::Selectable("Euclidean", accentModeValue == EuclideanEventPattern)) accentPatternMode = EuclideanEventPattern;
            if(ImGui::Selectable("RandomStep", accentModeValue == RandomStepEventPattern)) accentPatternMode = RandomStepEventPattern;
            if(ImGui::Selectable("RandomNote", accentModeValue == RandomNoteEventPattern)) accentPatternMode = RandomNoteEventPattern;
            if(ImGui::Selectable("StepSeq", accentModeValue == StepSeqEventPattern)) accentPatternMode = StepSeqEventPattern;
            ImGui::EndCombo();
        }
        drawNodePublishContextMenu("accentPatternMode", "Pattern", compactWidth);

        bool accentMode = accentOnsetMode.get();
        if(ImGui::Checkbox("Onset Acc", &accentMode)) accentOnsetMode = accentMode;
        drawNodePublishContextMenu("accentOnsetMode", "Onset Acc", 0.0f, true);

        if(accentPatternMode.get() == AlternateEventPattern) {
            int everyValue = accentAlternateSteps.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Every", &everyValue)) accentAlternateSteps = ofClamp(everyValue, accentAlternateSteps.getMin(), accentAlternateSteps.getMax());
            drawNodePublishContextMenu("accentAlternateSteps", "Every", compactWidth);

            int shiftValue = accentAlternateShift.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Shift", &shiftValue)) accentAlternateShift = ofClamp(shiftValue, accentAlternateShift.getMin(), accentAlternateShift.getMax());
            drawNodePublishContextMenu("accentAlternateShift", "Shift", compactWidth);
        } else if(accentPatternMode.get() == EuclideanEventPattern) {
            int accLenValue = eucAccLen.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Acc Len", &accLenValue)) eucAccLen = ofClamp(accLenValue, eucAccLen.getMin(), eucAccLen.getMax());
            drawNodePublishContextMenu("eucAccLen", "Acc Len", compactWidth);

            int accHitsValue = eucAccHits.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Acc Hits", &accHitsValue)) eucAccHits = ofClamp(accHitsValue, eucAccHits.getMin(), eucAccHits.getMax());
            drawNodePublishContextMenu("eucAccHits", "Acc Hits", compactWidth);

            int accOffValue = eucAccOff.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Acc Off", &accOffValue)) eucAccOff = ofClamp(accOffValue, eucAccOff.getMin(), eucAccOff.getMax());
            drawNodePublishContextMenu("eucAccOff", "Acc Off", compactWidth);
        } else if(accentPatternMode.get() == RandomStepEventPattern || accentPatternMode.get() == RandomNoteEventPattern) {
            float chanceValue = accentChance.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(drawDraggableFloatWithPopup("Chance", chanceValue, 0.01f, accentChance.getMin(), accentChance.getMax(), "%.2f",
                                           [this]() { drawNodePublishMenuItems("accentChance"); })) accentChance = chanceValue;
            drawPublishedLabelUnderline("accentChance", "Chance", compactWidth);

            int seedValue = accentSeed.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Seed", &seedValue)) accentSeed = ofClamp(seedValue, accentSeed.getMin(), accentSeed.getMax());
            drawNodePublishContextMenu("accentSeed", "Seed", compactWidth);

            bool randomizeSeed = accentRandomizeSeedEachCycle.get();
            if(ImGui::Checkbox("Rndm Seed", &randomizeSeed)) accentRandomizeSeedEachCycle = randomizeSeed;
            drawNodePublishContextMenu("accentRandomizeSeedEachCycle", "Rndm Seed", 0.0f, true);
        }

        ImGui::TableNextColumn();
        float accentStrength = eucAccStrength.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Acc Amt", accentStrength, 0.01f, 0.0f, 1.0f, "%.2f",
                                       [this]() { drawNodePublishMenuItems("eucAccStrength"); })) eucAccStrength = accentStrength;
        drawPublishedLabelUnderline("eucAccStrength", "Acc Amt", compactWidth);
        ImGui::TextDisabled("Velocity layers now");
        ImGui::TextDisabled("live in the Velocity tab.");

        ImGui::TableNextColumn();
        if(accentPatternMode.get() == StepSeqEventPattern) {
            drawAccentStepPatternEditor(ImGui::GetContentRegionAvail().x, 70.0f * editorZoom);
        } else {
            ImGui::TextDisabled("StepSeq editor appears");
            ImGui::TextDisabled("when Pattern = StepSeq.");
        }
        ImGui::EndTable();
    }
}

void polyphonicArpeggiatorGUI::drawDurationSection() {
    float compactWidth = getCompactFieldWidth(88.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.88f);
    if(ImGui::BeginTable("ArpDurationsLayout", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        int durationModeValue = durationPatternMode.get();
        const char *durationModeLabel = durationModeValue == AlternateEventPattern ? "Alternate"
                                       : durationModeValue == EuclideanEventPattern ? "Euclidean"
                                       : durationModeValue == RandomStepEventPattern ? "RandomStep"
                                       : durationModeValue == RandomNoteEventPattern ? "RandomNote"
                                       : "StepSeq";
        ImGui::SetNextItemWidth(compactWidth);
        if(ImGui::BeginCombo("Pattern", durationModeLabel)) {
            if(ImGui::Selectable("Alternate", durationModeValue == AlternateEventPattern)) durationPatternMode = AlternateEventPattern;
            if(ImGui::Selectable("Euclidean", durationModeValue == EuclideanEventPattern)) durationPatternMode = EuclideanEventPattern;
            if(ImGui::Selectable("RandomStep", durationModeValue == RandomStepEventPattern)) durationPatternMode = RandomStepEventPattern;
            if(ImGui::Selectable("RandomNote", durationModeValue == RandomNoteEventPattern)) durationPatternMode = RandomNoteEventPattern;
            if(ImGui::Selectable("StepSeq", durationModeValue == StepSeqEventPattern)) durationPatternMode = StepSeqEventPattern;
            ImGui::EndCombo();
        }
        drawNodePublishContextMenu("durationPatternMode", "Pattern", compactWidth);

        if(durationPatternMode.get() == AlternateEventPattern) {
            int everyValue = durationAlternateSteps.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Every", &everyValue)) durationAlternateSteps = ofClamp(everyValue, durationAlternateSteps.getMin(), durationAlternateSteps.getMax());
            drawNodePublishContextMenu("durationAlternateSteps", "Every", compactWidth);

            int shiftValue = durationAlternateShift.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Shift", &shiftValue)) durationAlternateShift = ofClamp(shiftValue, durationAlternateShift.getMin(), durationAlternateShift.getMax());
            drawNodePublishContextMenu("durationAlternateShift", "Shift", compactWidth);
        } else if(durationPatternMode.get() == EuclideanEventPattern) {
            int durLenValue = eucDurLen.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Dur Len", &durLenValue)) eucDurLen = ofClamp(durLenValue, eucDurLen.getMin(), eucDurLen.getMax());
            drawNodePublishContextMenu("eucDurLen", "Dur Len", compactWidth);

            int durHitsValue = eucDurHits.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Dur Hits", &durHitsValue)) eucDurHits = ofClamp(durHitsValue, eucDurHits.getMin(), eucDurHits.getMax());
            drawNodePublishContextMenu("eucDurHits", "Dur Hits", compactWidth);

            int durOffValue = eucDurOff.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Dur Off", &durOffValue)) eucDurOff = ofClamp(durOffValue, eucDurOff.getMin(), eucDurOff.getMax());
            drawNodePublishContextMenu("eucDurOff", "Dur Off", compactWidth);
        } else if(durationPatternMode.get() == RandomStepEventPattern || durationPatternMode.get() == RandomNoteEventPattern) {
            float chanceValue = durationChance.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(drawDraggableFloatWithPopup("Chance", chanceValue, 0.01f, durationChance.getMin(), durationChance.getMax(), "%.2f",
                                           [this]() { drawNodePublishMenuItems("durationChance"); })) durationChance = chanceValue;
            drawPublishedLabelUnderline("durationChance", "Chance", compactWidth);

            int seedValue = durationSeed.get();
            ImGui::SetNextItemWidth(compactWidth);
            if(ImGui::InputInt("Seed", &seedValue)) durationSeed = ofClamp(seedValue, durationSeed.getMin(), durationSeed.getMax());
            drawNodePublishContextMenu("durationSeed", "Seed", compactWidth);

            bool randomizeSeed = durationRandomizeSeedEachCycle.get();
            if(ImGui::Checkbox("Rndm Seed", &randomizeSeed)) durationRandomizeSeedEachCycle = randomizeSeed;
            drawNodePublishContextMenu("durationRandomizeSeedEachCycle", "Rndm Seed", 0.0f, true);
        }

        ImGui::TableNextColumn();
        float baseDuration = durBase.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Dur", baseDuration, 0.01f, durBase.getMin(), durBase.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("durBase"); })) durBase = baseDuration;
        drawPublishedLabelUnderline("durBase", "Dur", compactWidth);

        float randomDuration = durRndm.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Dur Rnd", randomDuration, 0.01f, durRndm.getMin(), durRndm.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("durRndm"); })) durRndm = randomDuration;
        drawPublishedLabelUnderline("durRndm", "Dur Rnd", compactWidth);

        float durationAccent = durEucStrength.get();
        ImGui::SetNextItemWidth(compactWidth);
        if(drawDraggableFloatWithPopup("Dur Acc", durationAccent, 0.01f, durEucStrength.getMin(), durEucStrength.getMax(), "%.2f",
                                       [this]() { drawNodePublishMenuItems("durEucStrength"); })) durEucStrength = durationAccent;
        drawPublishedLabelUnderline("durEucStrength", "Dur Acc", compactWidth);

        ImGui::TableNextColumn();
        bool randomByStep = durationRndPerStep.get();
        if(ImGui::Checkbox("Rnd by Step", &randomByStep)) durationRndPerStep = randomByStep;
        drawNodePublishContextMenu("durationRndPerStep", "Rnd by Step", 0.0f, true);

        ImGui::TableNextColumn();
        if(durationPatternMode.get() == StepSeqEventPattern) {
            drawDurationStepPatternEditor(ImGui::GetContentRegionAvail().x, 70.0f * editorZoom);
        } else {
            ImGui::TextDisabled(durationRndPerStep.get() ? "Duration randomization: per step" : "Duration randomization: per note");
        }
        ImGui::EndTable();
    }
}

void polyphonicArpeggiatorGUI::drawRhythmSection() {
    drawEuclideanPreview(ImGui::GetContentRegionAvail().x, 88.0f * editorZoom);
}

void polyphonicArpeggiatorGUI::drawVisualizationSection() {
    float width = ImGui::GetContentRegionAvail().x;
    drawArpGrid(width, 180.0f * editorZoom);
}

void polyphonicArpeggiatorGUI::drawOutputSection() {
    std::vector<float> notes;
    std::vector<float> velocities;
    for(size_t i = 0; i < currentPitches.size() && i < currentGates.size() && i < currentVelocities.size(); i++) {
        if(currentGates[i] > 0) {
            notes.push_back(currentPitches[i]);
            velocities.push_back(currentVelocities[i]);
        }
    }
    if(notes.empty()) {
        StepPreviewInfo info = buildStepPreview(wrapIndex(highlightedStep, std::max(1, seqSize.get())));
        notes = info.notes;
        velocities = info.noteVelocities;
    }
    if(notes.empty()) notes = getActiveOutputPreviewNotes();
    drawKeyboardDisplay("OutputKeyboard", notes, &velocities, ImGui::GetContentRegionAvail().x, 64.0f * editorZoom, true, true);

    ImGui::Spacing();
    int historyWindow = outputHistoryWindowMs.get();
    float compactWidth = getCompactFieldWidth(90.0f, ImGui::GetContentRegionAvail().x, editorZoom, 0.34f);
    ImGui::SetNextItemWidth(compactWidth);
    if(ImGui::InputInt("Hist ms", &historyWindow)) outputHistoryWindowMs = ofClamp(historyWindow, outputHistoryWindowMs.getMin(), outputHistoryWindowMs.getMax());
    drawNodePublishContextMenu("outputHistoryWindowMs", "Hist ms", compactWidth);
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
        float velocityNorm = ofClamp(event.velocity, 0.0f, 1.0f);
        float emphasized = std::pow(velocityNorm, 1.1f);
        int r = static_cast<int>(ofLerp(38.0f, 136.0f, emphasized));
        int g = static_cast<int>(ofLerp(78.0f, 242.0f, emphasized));
        int b = static_cast<int>(ofLerp(70.0f, 202.0f, emphasized));
        int alpha = static_cast<int>(ofLerp(92.0f, 248.0f, emphasized));
        ImU32 color = IM_COL32(r, g, b, alpha);
        drawList->AddRectFilled(ImVec2(x1, y + 1.0f), ImVec2(std::min(gridX + gridW - 1.0f, x2), y + laneHeight - 1.0f), color, 2.0f);
        drawList->AddRect(ImVec2(x1, y + 1.0f), ImVec2(std::min(gridX + gridW - 1.0f, x2), y + laneHeight - 1.0f),
                          IM_COL32(255, 255, 255, static_cast<int>(ofLerp(14.0f, 52.0f, emphasized))), 2.0f);
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

void polyphonicArpeggiatorGUI::drawPulseStepPatternEditor(float width, float height) {
    syncPulseStepPatternToSequenceSize();

    int size = std::max(1, seqSize.get());
    std::vector<int> pattern = pulseStepPattern.get();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##PulseStepPatternEditor", ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(18, 20, 24, 255), 4.0f);
    drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(70, 70, 74, 255), 4.0f);

    float stepWidth = width / static_cast<float>(size);
    int highlighted = wrapIndex(highlightedStep, size);
    float topPad = 18.0f;
    bool changed = false;

    if(ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float localX = ofClamp(mouse.x - pos.x, 0.0f, std::max(0.0f, width - 1.0f));
        float localY = ofClamp(mouse.y - (pos.y + topPad), 0.0f, std::max(1.0f, height - topPad - 4.0f));
        int step = ofClamp(static_cast<int>(localX / std::max(1.0f, stepWidth)), 0, size - 1);
        float normalized = 1.0f - (localY / std::max(1.0f, height - topPad - 4.0f));
        pattern[step] = ofClamp(static_cast<int>(std::round(normalized * 100.0f)), 0, 100);
        changed = true;
    }

    drawList->AddText(ImVec2(pos.x + 6.0f, pos.y + 3.0f), IM_COL32(230, 230, 230, 220), "StepSeq %");
    for(int step = 0; step < size; step++) {
        float x = pos.x + step * stepWidth;
        ImVec2 cellMin(x + 1.0f, pos.y + topPad);
        ImVec2 cellMax(x + stepWidth - 1.0f, pos.y + height - 4.0f);
        ImU32 borderColor = step == highlighted ? IM_COL32(252, 224, 132, 220) : IM_COL32(48, 48, 52, 180);
        drawList->AddRect(cellMin, cellMax, borderColor, 2.0f);
        float probability = ofClamp(static_cast<float>(pattern[step]) / 100.0f, 0.0f, 1.0f);
        if(probability > 0.0f) {
            ImU32 fillColor = step == highlighted ? IM_COL32(242, 146, 94, 255) : IM_COL32(210, 100, 96, 255);
            float fillTop = ofLerp(cellMax.y, cellMin.y, probability);
            drawList->AddRectFilled(ImVec2(cellMin.x, fillTop), cellMax, fillColor, 2.0f);
        }
        std::string label = ofToString(pattern[step]);
        ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        drawList->AddText(ImVec2(x + std::max(0.0f, (stepWidth - textSize.x) * 0.5f), pos.y + 3.0f),
                          IM_COL32(205, 214, 224, 180),
                          label.c_str());
    }

    if(changed) pulseStepPattern = pattern;
}

void polyphonicArpeggiatorGUI::drawRunGatePhasor(float width, float height) const {
    width = std::max(1.0f, width);
    height = std::max(34.0f * editorZoom, height);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##RunGatePhasor", ImVec2(width, height));

    const ImU32 bg = IM_COL32(16, 20, 26, 255);
    const ImU32 border = IM_COL32(66, 92, 122, 220);
    const ImU32 activeFill = IM_COL32(84, 176, 110, 220);
    const ImU32 mutedFill = IM_COL32(98, 58, 58, 220);
    const ImU32 playhead = IM_COL32(236, 234, 190, 255);
    const ImU32 text = IM_COL32(214, 224, 236, 220);

    ImVec2 end(pos.x + width, pos.y + height);
    drawList->AddRectFilled(pos, end, bg, 4.0f);
    drawList->AddRect(pos, end, border, 4.0f);

    float phase = getRunGatePhaseNormalized();
    float innerPad = 6.0f;
    float barTop = pos.y + 18.0f;
    float barBottom = end.y - 7.0f;
    float barLeft = pos.x + innerPad;
    float barRight = end.x - innerPad;
    float barWidth = std::max(1.0f, barRight - barLeft);
    float playheadX = barLeft + barWidth * phase;

    drawList->AddRectFilled(ImVec2(barLeft, barTop),
                            ImVec2(playheadX, barBottom),
                            runGateCurrentShouldPlay ? activeFill : mutedFill,
                            3.0f);
    drawList->AddLine(ImVec2(playheadX, barTop - 2.0f),
                      ImVec2(playheadX, barBottom + 2.0f),
                      playhead,
                      2.0f);

    std::string label = std::string(runGateCurrentShouldPlay ? "Run open" : "Run muted")
                      + "  p=" + ofToString(phase, 2);
    drawList->AddText(ImVec2(pos.x + innerPad, pos.y + 3.0f), text, label.c_str());
}

void polyphonicArpeggiatorGUI::drawAccentStepPatternEditor(float width, float height) {
    syncAccentStepPatternToSequenceSize();

    int size = std::max(1, seqSize.get());
    std::vector<int> pattern = accentStepPattern.get();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##AccentStepPatternEditor", ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(22, 20, 18, 255), 4.0f);
    drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(82, 70, 54, 255), 4.0f);

    float stepWidth = width / static_cast<float>(size);
    int highlighted = wrapIndex(highlightedStep, size);
    float topPad = 18.0f;
    bool changed = false;

    if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float localX = ofClamp(mouse.x - pos.x, 0.0f, std::max(0.0f, width - 1.0f));
        int step = ofClamp(static_cast<int>(localX / std::max(1.0f, stepWidth)), 0, size - 1);
        pattern[step] = pattern[step] > 0 ? 0 : 1;
        changed = true;
    }

    drawList->AddText(ImVec2(pos.x + 6.0f, pos.y + 3.0f), IM_COL32(230, 230, 230, 220), "Acc StepSeq");
    for(int step = 0; step < size; step++) {
        float x = pos.x + step * stepWidth;
        ImVec2 cellMin(x + 1.0f, pos.y + topPad);
        ImVec2 cellMax(x + stepWidth - 1.0f, pos.y + height - 4.0f);
        ImU32 borderColor = step == highlighted ? IM_COL32(252, 224, 132, 220) : IM_COL32(64, 58, 48, 180);
        drawList->AddRect(cellMin, cellMax, borderColor, 2.0f);
        if(pattern[step] > 0) {
            ImU32 fillColor = step == highlighted ? IM_COL32(252, 186, 76, 255) : IM_COL32(224, 144, 54, 255);
            drawList->AddRectFilled(cellMin, cellMax, fillColor, 2.0f);
        }
    }

    if(changed) accentStepPattern = pattern;
}

void polyphonicArpeggiatorGUI::drawDurationStepPatternEditor(float width, float height) {
    syncDurationStepPatternToSequenceSize();

    int size = std::max(1, seqSize.get());
    std::vector<int> pattern = durationStepPattern.get();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##DurationStepPatternEditor", ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(18, 20, 24, 255), 4.0f);
    drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(56, 72, 88, 255), 4.0f);

    float stepWidth = width / static_cast<float>(size);
    int highlighted = wrapIndex(highlightedStep, size);
    float topPad = 18.0f;
    bool changed = false;

    if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float localX = ofClamp(mouse.x - pos.x, 0.0f, std::max(0.0f, width - 1.0f));
        int step = ofClamp(static_cast<int>(localX / std::max(1.0f, stepWidth)), 0, size - 1);
        pattern[step] = pattern[step] > 0 ? 0 : 1;
        changed = true;
    }

    drawList->AddText(ImVec2(pos.x + 6.0f, pos.y + 3.0f), IM_COL32(230, 230, 230, 220), "Dur StepSeq");
    for(int step = 0; step < size; step++) {
        float x = pos.x + step * stepWidth;
        ImVec2 cellMin(x + 1.0f, pos.y + topPad);
        ImVec2 cellMax(x + stepWidth - 1.0f, pos.y + height - 4.0f);
        ImU32 borderColor = step == highlighted ? IM_COL32(252, 224, 132, 220) : IM_COL32(52, 64, 76, 180);
        drawList->AddRect(cellMin, cellMax, borderColor, 2.0f);
        if(pattern[step] > 0) {
            ImU32 fillColor = step == highlighted ? IM_COL32(120, 188, 255, 255) : IM_COL32(76, 148, 228, 255);
            drawList->AddRectFilled(cellMin, cellMax, fillColor, 2.0f);
        }
    }

    if(changed) durationStepPattern = pattern;
}

void polyphonicArpeggiatorGUI::drawEuclideanPreview(float width, float height) const {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##ArpEuclideanPreview", ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(23, 23, 25, 255), 4.0f);
    drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(70, 70, 70, 255), 4.0f);

    int previewLength = std::max(1, seqSize.get());
    std::vector<bool> pulsePreview(previewLength, true);
    std::vector<bool> accentPreview(previewLength, false);
    std::vector<bool> durationPreview(previewLength, false);
    for(int i = 0; i < previewLength; i++) {
        pulsePreview[i] = isPulseActiveForStepPreview(i);
        accentPreview[i] = isAccentPreviewLaneActive(i);
        durationPreview[i] = isDurationPreviewLaneActive(i);
    }

    struct LaneInfo {
        const std::vector<bool> *pattern = nullptr;
        ImU32 color;
        const char *label;
    };

    std::array<LaneInfo, 3> lanes = {{
        {&pulsePreview, IM_COL32(210, 100, 96, 255), "Gate"},
        {&accentPreview, IM_COL32(236, 182, 82, 255), "Vel"},
        {&durationPreview, IM_COL32(92, 156, 236, 255), "Dur"}
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
            if((*lane.pattern)[wrapIndex(i, len)]) drawList->AddRectFilled(cellMin, cellMax, lane.color, 2.0f);
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
        bool hasVisibleStrum = strum.get() > 0.001f || strumRndm.get() > 0.001f || swing.get() > 0.001f || positionRndm.get() > 0.001f;

        for(size_t voice = 0; voice < info.notes.size(); voice++) {
            int note = ofClamp(static_cast<int>(std::round(info.notes[voice])), 0, 127);
            float y = gridY + (maxNote - note) * laneHeight;
            float noteVelocity = voice < info.noteVelocities.size() ? info.noteVelocities[voice] : info.velocity;
            bool noteAccent = voice < info.noteAccents.size() ? info.noteAccents[voice] : info.accent;
            float velocityNorm = ofClamp(noteVelocity, 0.0f, 1.0f);
            float emphasized = std::pow(velocityNorm, 1.85f);
            ImU32 baseColor;
            if(noteAccent) {
                int r = static_cast<int>(ofLerp(42.0f, 255.0f, emphasized));
                int g = static_cast<int>(ofLerp(24.0f, 196.0f, emphasized));
                int b = static_cast<int>(ofLerp(12.0f, 104.0f, emphasized));
                int a = static_cast<int>(ofLerp(72.0f, 255.0f, emphasized));
                baseColor = IM_COL32(r, g, b, a);
            } else {
                int r = static_cast<int>(ofLerp(14.0f, 112.0f, emphasized));
                int g = static_cast<int>(ofLerp(28.0f, 240.0f, emphasized));
                int b = static_cast<int>(ofLerp(22.0f, 202.0f, emphasized));
                int a = static_cast<int>(ofLerp(58.0f, 245.0f, emphasized));
                baseColor = IM_COL32(r, g, b, a);
            }
            float onsetOffset = 0.0f;
            if(hasVisibleStrum) {
                float positionOffsetMs = computePreviewPositionOffset(step, static_cast<int>(voice), static_cast<int>(info.notes.size()));
                onsetOffset = ofClamp(positionOffsetMs / std::max(1.0f, stepDurationMs), 0.0f, 0.95f);
            }
            int noteDurationMs = voice < info.noteDurations.size() ? info.noteDurations[voice] : info.duration;
            float durationSteps = std::max(0.12f, static_cast<float>(noteDurationMs) / std::max(1.0f, stepDurationMs));
            float rectX = gridX + step * stepWidth + stepWidth * onsetOffset;
            float rectW = std::max(3.0f, stepWidth * durationSteps);
            drawList->AddRectFilled(ImVec2(rectX, y + 1.0f), ImVec2(std::min(gridX + gridW - 1.0f, rectX + rectW), y + laneHeight - 1.0f), baseColor, 2.0f);
            drawList->AddRect(ImVec2(rectX, y + 1.0f), ImVec2(std::min(gridX + gridW - 1.0f, rectX + rectW), y + laneHeight - 1.0f),
                              IM_COL32(255, 255, 255, static_cast<int>(ofLerp(8.0f, 64.0f, emphasized))), 2.0f);
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

    applyPendingSourceMaterialChange();

    if(shouldReset) {
        currentStep = 0;
        shouldReset = false;
        onsetCounter = 0;
        absoluteStepCounter = 0;
    }

    if(oneShotMode.get() && !oneShotCycleActive) {
        return;
    }

    processStep();
    absoluteStepCounter++;

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
    sequenceCycleDecisionPending = true;
    skippedSequenceCyclesRemaining = 0;
    if(oneShotMode.get()) {
        oneShotCycleActive = true;
        oneShotStepsRemaining = std::max(1, seqSize.get());
    }
}

void polyphonicArpeggiatorGUI::onResetNext() {
    shouldReset = true;
}

void polyphonicArpeggiatorGUI::processStep() {
    updateRunGateWindowState();
    int shiftedCurrentStep = getShiftedSequenceStepIndex(currentStep);
    bool usingTransportGeigerPulse = internalClockMode.get() && pulseMode.get() == GeigerPulse && geigerTransportPulseActive;
    if(sequenceCycleDecisionPending || currentStep == 0) {
        randomizePatternSeedsForNewCycle();
        randomizeCycleStepShift();
        if(skippedSequenceCyclesRemaining > 0) {
            currentSequenceCycleShouldPlay = false;
            skippedSequenceCyclesRemaining--;
        } else {
            float muteBlockTriggerProbability = computeSequenceMuteBlockTriggerProbability(seqProb.get(), seqProbCycles.get());
            bool startMutedBlock = muteBlockTriggerProbability > 0.0f && dist01(rng) <= muteBlockTriggerProbability;
            currentSequenceCycleShouldPlay = !startMutedBlock;
            if(startMutedBlock) {
                skippedSequenceCyclesRemaining = std::max(0, seqProbCycles.get() - 1);
            }
        }
        sequenceCycleDecisionPending = false;
    }
    if(!currentSequenceCycleShouldPlay) {
        highlightedStep = currentStep;
        return;
    }
    if(!runGateCurrentShouldPlay) {
        highlightedStep = currentStep;
        return;
    }
    if(!usingTransportGeigerPulse && !isPulseActiveForStepLive(shiftedCurrentStep)) return;
    if(stepChance.get() < 1.0f && dist01(rng) > stepChance.get()) return;

    int accentIndex = accentOnsetMode.get() ? onsetCounter : absoluteStepCounter;
    int shiftedAccentIndex = getShiftedSequenceStepIndex(accentIndex);
    bool stepAccented = isAccentStepActiveLive(shiftedAccentIndex);
    int poly = std::min(polyphony.get() + (stepAccented ? polyAccent.get() : 0), MaxPolyphony);
    int interval = std::max(1, polyInterval.get());
    uint64_t now = ofGetElapsedTimeMillis();
    int patternOffset = getPatternOffsetForStepLive(currentStep);
    int baseRelativeOffset = patternOffset * std::max(0, sourceStride.get());
    int baseSourceIndex = sourceStart.get() + baseRelativeOffset;

    std::fill(stepGates.begin(), stepGates.end(), false);
    rebuildPitchSequence();
    int outputSlotCount = std::max(1, static_cast<int>(currentPitches.size()));
    std::vector<bool> claimedOutputSlots(outputSlotCount, false);
    float previousTriggeredPitch = std::numeric_limits<float>::quiet_NaN();
    bool stepDurationAccent = isDurationStepActiveLive(shiftedAccentIndex);
    float sharedDurationRandomOffset = durationRndPerStep.get() ? computeLiveDurationRandomOffset() : 0.0f;
    float sharedStepVelocityOffset = computeLiveStepVelocityRandomOffset(currentStep);

    auto clampDurationMs = [](int durationMs) {
        return static_cast<uint64_t>(std::max(1, durationMs));
    };

    auto extendOutputHistoryEvent = [this](float pitch, uint64_t startTimeMs, int durationMs) {
        float clampedPitch = ofClamp(pitch, 0.0f, 127.0f);
        int clampedDuration = std::max(1, durationMs);
        for(auto it = outputHistory.rbegin(); it != outputHistory.rend(); ++it) {
            if(it->startTimeMs == startTimeMs && std::abs(it->pitch - clampedPitch) <= 0.001f) {
                it->durationMs = std::max(it->durationMs, clampedDuration);
                return;
            }
        }
    };

    auto findMergeableOutputSlot = [this, clampDurationMs](float pitch, uint64_t startTimeMs, int durationMs) {
        uint64_t endTimeMs = startTimeMs + clampDurationMs(durationMs);
        int bestIndex = -1;
        uint64_t bestStartTime = std::numeric_limits<uint64_t>::max();

        for(size_t i = 0; i < currentPitches.size(); i++) {
            if(noteStartTimes[i] == 0) continue;
            if(std::abs(currentPitches[i] - pitch) > 0.001f) continue;

            uint64_t existingStartTimeMs = noteStartTimes[i];
            uint64_t existingEndTimeMs = existingStartTimeMs + clampDurationMs(noteDurationsMs[i]);
            bool overlaps = startTimeMs <= existingEndTimeMs && existingStartTimeMs <= endTimeMs;
            if(!overlaps) continue;

            if(existingStartTimeMs < bestStartTime) {
                bestStartTime = existingStartTimeMs;
                bestIndex = static_cast<int>(i);
            }
        }

        return bestIndex;
    };

    auto triggerOutputSlot = [this, now, &claimedOutputSlots, clampDurationMs, &extendOutputHistoryEvent, &findMergeableOutputSlot]
                             (int preferredIndex, float pitch, float velocity, int noteDuration, float strumOffsetMs) {
        uint64_t scheduledStartTimeMs = strumOffsetMs <= 0.5f
                                      ? now
                                      : now + static_cast<uint64_t>(std::max(0.0f, strumOffsetMs));
        uint64_t scheduledDurationMs = clampDurationMs(noteDuration);
        uint64_t scheduledEndTimeMs = scheduledStartTimeMs + scheduledDurationMs;

        int mergeOutputIndex = findMergeableOutputSlot(pitch, scheduledStartTimeMs, noteDuration);
        if(mergeOutputIndex >= 0) {
            claimedOutputSlots[mergeOutputIndex] = true;

            uint64_t existingStartTimeMs = noteStartTimes[mergeOutputIndex];
            uint64_t existingEndTimeMs = existingStartTimeMs + clampDurationMs(noteDurationsMs[mergeOutputIndex]);
            uint64_t mergedStartTimeMs = std::min(existingStartTimeMs, scheduledStartTimeMs);
            uint64_t mergedEndTimeMs = std::max(existingEndTimeMs, scheduledEndTimeMs);
            bool newNoteStartsEarlier = scheduledStartTimeMs < existingStartTimeMs;

            currentPitches[mergeOutputIndex] = pitch;
            if(newNoteStartsEarlier) {
                currentVelocities[mergeOutputIndex] = velocity;
            }

            noteDurationsMs[mergeOutputIndex] = static_cast<int>(std::max<uint64_t>(1, mergedEndTimeMs - mergedStartTimeMs));
            currentDurations[mergeOutputIndex] = static_cast<float>(noteDurationsMs[mergeOutputIndex]);

            if(newNoteStartsEarlier) {
                noteStartTimes[mergeOutputIndex] = mergedStartTimeMs;
                if(mergedStartTimeMs <= now) {
                    currentGates[mergeOutputIndex] = 1;
                    stepGates[mergeOutputIndex] = true;
                    noteStartTimes[mergeOutputIndex] = now;
                    recordOutputHistoryEvent(currentPitches[mergeOutputIndex], currentVelocities[mergeOutputIndex], noteDurationsMs[mergeOutputIndex], now);
                } else {
                    currentGates[mergeOutputIndex] = 0;
                }
            } else if(currentGates[mergeOutputIndex] == 1) {
                extendOutputHistoryEvent(currentPitches[mergeOutputIndex], existingStartTimeMs, noteDurationsMs[mergeOutputIndex]);
            }

            return;
        }

        int outputIndex = findAvailableOutputIndex(preferredIndex, claimedOutputSlots);
        currentPitches[outputIndex] = pitch;
        currentVelocities[outputIndex] = velocity;
        currentDurations[outputIndex] = static_cast<float>(noteDuration);

        bool sustaining = currentGates[outputIndex] == 1 &&
                          noteStartTimes[outputIndex] > 0 &&
                          now < noteStartTimes[outputIndex] + clampDurationMs(noteDurationsMs[outputIndex]);

        if(!sustaining) {
            noteDurationsMs[outputIndex] = static_cast<int>(scheduledDurationMs);
            if(strumOffsetMs <= 0.5f) {
                currentGates[outputIndex] = 1;
                stepGates[outputIndex] = true;
                noteStartTimes[outputIndex] = now;
                recordOutputHistoryEvent(currentPitches[outputIndex], currentVelocities[outputIndex], noteDurationsMs[outputIndex], now);
            } else {
                currentGates[outputIndex] = 0;
                noteStartTimes[outputIndex] = scheduledStartTimeMs;
            }
        }
    };

    for(int voice = 0; voice < poly; voice++) {
        if(noteChance.get() < 1.0f && dist01(rng) > noteChance.get()) continue;

        int sourceIndex = baseSourceIndex + voice * interval;
        int preferredOutputIndex = baseRelativeOffset + voice * interval;
        float mappedPitch = 60.0f;
        int maxAttempts = std::max(1, static_cast<int>(activeSourceValues.size()));
        bool applyFold = (voice == 0) || foldPoly.get();

        for(int attempt = 0; attempt < maxAttempts; attempt++) {
            int candidateIndex = sourceIndex + attempt;
            float basePitch = getSourceValue(candidateIndex);
            float deviation = generateDeviationForSourceIndex(candidateIndex, rng);
            mappedPitch = mapPitchWithDeviation(basePitch, deviation, applyFold);
            if(!std::isfinite(previousTriggeredPitch) || std::abs(mappedPitch - previousTriggeredPitch) > 0.001f) {
                break;
            }
        }

        previousTriggeredPitch = mappedPitch;
        bool noteAccentActive = isAccentNoteActiveLive(shiftedAccentIndex, voice, stepAccented);
        bool noteDurationAccent = isDurationNoteActiveLive(shiftedAccentIndex, voice, stepDurationAccent);
        float noteDurationOffset = durationRndPerStep.get() ? sharedDurationRandomOffset : computeLiveDurationRandomOffset();
        int noteDuration = computeNoteDuration(noteDurationAccent, noteDurationOffset);
        float noteVelocity = computeLiveNoteVelocity(currentStep, voice, noteAccentActive, sharedStepVelocityOffset);
        triggerOutputSlot(preferredOutputIndex, mappedPitch, noteVelocity, noteDuration, computeLivePositionOffset(currentStep, voice, poly));
    }

    bool bassModeActive = addBass.get() && isBassStepActiveLive(shiftedCurrentStep, stepAccented, stepDurationAccent);
    bool shouldAddBass = bassModeActive;
    if(shouldAddBass && bassPatternMode.get() != BassRandomPattern) {
        shouldAddBass = bassProb.get() >= 1.0f || dist01(rng) <= bassProb.get();
    }
    if(shouldAddBass) {
        float bassDurationOffset = durationRndPerStep.get() ? sharedDurationRandomOffset : computeLiveDurationRandomOffset();
        int bassDuration = computeNoteDuration(stepDurationAccent, bassDurationOffset);
        int bassVoices = std::max(1, bassPolyphony.get());
        int bassOctaveOffset = computeLiveBassOctaveOffset(shiftedCurrentStep);
        for(int bassVoice = 0; bassVoice < bassVoices; bassVoice++) {
            float bassVelocity = computeLiveBassVelocity();
            int preferredOutputIndex = baseRelativeOffset + poly * interval + bassVoice;
            float bassPitch = mapBassPitch(bassVoice, bassOctaveOffset);
            triggerOutputSlot(preferredOutputIndex,
                              bassPitch,
                              bassVelocity,
                              bassDuration,
                              computeLivePositionOffset(currentStep, bassVoice, bassVoices));
        }
    }

    onsetCounter++;
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
    activeSourceValues = buildSourceMaterialFromParameters();
}

void polyphonicArpeggiatorGUI::handleSourceMaterialChange() {
    std::vector<float> nextSourceValues = buildSourceMaterialFromParameters();
    auto materiallyDifferent = [](const std::vector<float> &lhs, const std::vector<float> &rhs) {
        if(lhs.size() != rhs.size()) return true;
        for(size_t i = 0; i < lhs.size(); i++) {
            if(std::abs(lhs[i] - rhs[i]) > 0.001f) return true;
        }
        return false;
    };

    sourceChangePending = materiallyDifferent(activeSourceValues, nextSourceValues);
    if(sourceChangePending) {
        pendingSourceValues = std::move(nextSourceValues);
    } else {
        pendingSourceValues.clear();
        rebuildDeviations();
        rebuildPitchSequence();
    }
    updateOutputs();
}

void polyphonicArpeggiatorGUI::applyPendingSourceMaterialChange() {
    if(!sourceChangePending) return;

    activeSourceValues = pendingSourceValues;
    pendingSourceValues.clear();
    sourceChangePending = false;

    if(sourceChangeMode.get() == ResetPattern) {
        currentStep = 0;
        highlightedStep = 0;
        onsetCounter = 0;
        absoluteStepCounter = 0;
        shouldReset = false;
    }

    rebuildDeviations();
    rebuildPitchSequence();
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

float polyphonicArpeggiatorGUI::mapOutputPitch(float pitch, bool applyFold) const {
    float mappedPitch = pitch + static_cast<float>(transpose.get() + octave.get() * 12);
    int foldOctaves = octaveFold.get();
    if(applyFold && foldOctaves > 0) {
        float foldBase = static_cast<float>(octave.get() * 12);
        float foldRange = static_cast<float>(foldOctaves * 12);
        float localPitch = std::fmod(mappedPitch - foldBase, foldRange);
        if(localPitch < 0.0f) localPitch += foldRange;
        mappedPitch = foldBase + localPitch;
    }
    return ofClamp(mappedPitch, 0.0f, 127.0f);
}

float polyphonicArpeggiatorGUI::mapPitchWithDeviation(float basePitch, float deviation, bool applyFold) const {
    return ofClamp(mapOutputPitch(basePitch, applyFold) + deviation, 0.0f, 127.0f);
}

float polyphonicArpeggiatorGUI::mapBassPitch(int octaveStackOffset, int randomOctaveOffset) const {
    float bassPitch = root.get() + static_cast<float>(transpose.get());
    if(bassPitchModified.get()) bassPitch += static_cast<float>(octave.get() * 12);
    bassPitch += static_cast<float>((bassOctave.get() + randomOctaveOffset + octaveStackOffset) * 12);
    return ofClamp(bassPitch, 0.0f, 127.0f);
}

int polyphonicArpeggiatorGUI::getPatternOffsetForStepLive(int stepIndex) {
    int size = getPatternTraversalSize();
    int shiftedStepIndex = getShiftedSequenceStepIndex(stepIndex);
    if(patternMode.get() == 1) {
        return size - 1 - wrapIndex(shiftedStepIndex, size);
    }
    if(patternMode.get() == 2) {
        float randomUnit = computePatternRandomUnit(patternSeed.get(), shiftedStepIndex, 0, 11);
        return std::min(size - 1, static_cast<int>(randomUnit * static_cast<float>(size)));
    }
    if(patternMode.get() == 3) {
        const auto &pattern = idxPattern.get();
        if(!pattern.empty()) return wrapIndex(pattern[wrapIndex(shiftedStepIndex, static_cast<int>(pattern.size()))], size);
        return 0;
    }
    if(patternMode.get() == 4) {
        return getBidirectionalPatternOffset(shiftedStepIndex, true);
    }
    if(patternMode.get() == 5) {
        return getBidirectionalPatternOffset(shiftedStepIndex, false);
    }
    return wrapIndex(shiftedStepIndex, size);
}

int polyphonicArpeggiatorGUI::findAvailableOutputIndex(int preferredIndex, std::vector<bool> &claimedSlots) const {
    if(claimedSlots.empty()) return 0;

    int slotCount = static_cast<int>(claimedSlots.size());
    int wrappedIndex = wrapIndex(preferredIndex, slotCount);
    for(int offset = 0; offset < slotCount; offset++) {
        int candidate = wrapIndex(wrappedIndex + offset, slotCount);
        if(!claimedSlots[candidate]) {
            claimedSlots[candidate] = true;
            return candidate;
        }
    }

    return wrappedIndex;
}

int polyphonicArpeggiatorGUI::getEffectiveStepShift() const {
    return stepShift.get() + currentCycleRandomStepShift;
}

int polyphonicArpeggiatorGUI::getShiftedSequenceStepIndex(int stepIndex) const {
    return stepIndex - getEffectiveStepShift();
}

int polyphonicArpeggiatorGUI::getPatternTraversalSize() const {
    int sequenceSize = std::max(1, seqSize.get());
    int moduloSize = modulo.get();
    if(moduloSize <= 0) return sequenceSize;
    return std::max(1, std::min(sequenceSize, moduloSize));
}

int polyphonicArpeggiatorGUI::getBidirectionalPatternOffset(int stepIndex, bool startAscending) const {
    int size = getPatternTraversalSize();
    if(size <= 1) return 0;

    int wrappedStep = wrapIndex(stepIndex, size);
    int midpoint = size / 2;

    if(startAscending) {
        return wrappedStep <= midpoint ? wrappedStep : size - wrappedStep;
    }

    return wrappedStep <= midpoint ? midpoint - wrappedStep : wrappedStep - midpoint;
}

int polyphonicArpeggiatorGUI::getPatternOffsetForStepPreview(int stepIndex) const {
    int size = getPatternTraversalSize();
    int shiftedStepIndex = getShiftedSequenceStepIndex(stepIndex);
    if(patternMode.get() == 1) {
        return size - 1 - wrapIndex(shiftedStepIndex, size);
    }
    if(patternMode.get() == 2) {
        float randomUnit = computePatternRandomUnit(patternSeed.get(), shiftedStepIndex, 0, 11);
        return std::min(size - 1, static_cast<int>(randomUnit * static_cast<float>(size)));
    }
    if(patternMode.get() == 3) {
        const auto &pattern = idxPattern.get();
        if(!pattern.empty()) return wrapIndex(pattern[wrapIndex(shiftedStepIndex, static_cast<int>(pattern.size()))], size);
        return 0;
    }
    if(patternMode.get() == 4) {
        return getBidirectionalPatternOffset(shiftedStepIndex, true);
    }
    if(patternMode.get() == 5) {
        return getBidirectionalPatternOffset(shiftedStepIndex, false);
    }
    return wrapIndex(shiftedStepIndex, size);
}

float polyphonicArpeggiatorGUI::generateDeviationForSourceIndex(int sourceIndex, std::mt19937 &generator) const {
    float deviation = 0.0f;
    std::uniform_real_distribution<float> unitDist(0.0f, 1.0f);

    if(octaveDev.get() > 0.0f && unitDist(generator) < octaveDev.get()) {
        std::uniform_int_distribution<int> octDist(1, std::max(1, octaveDevRng.get()));
        deviation += octDist(generator) * 12.0f;
    }

    if(idxDev.get() > 0.0f && unitDist(generator) < idxDev.get()) {
        std::uniform_int_distribution<int> idxDist(1, std::max(1, idxDevRng.get()));
        float basePitch = getSourceValue(sourceIndex);
        float shiftedPitch = getSourceValue(sourceIndex + idxDist(generator));
        deviation += shiftedPitch - basePitch;
    }

    if(pitchDev.get() > 0.0f && unitDist(generator) < pitchDev.get()) {
        std::uniform_int_distribution<int> chromDist(1, std::max(1, pitchDevRng.get()));
        deviation += static_cast<float>(chromDist(generator));
    }

    return deviation;
}

void polyphonicArpeggiatorGUI::rebuildDeviations() {
    int size = getOutputSlotCount();
    deviationValues.assign(size, 0.0f);

    for(int i = 0; i < size; i++) {
        int sourceIndex = sourceStart.get() + i;
        deviationValues[i] = generateDeviationForSourceIndex(sourceIndex, rng);
    }
}

void polyphonicArpeggiatorGUI::rebuildPitchSequence() {
    int size = getOutputSlotCount();
    if(static_cast<int>(currentPitches.size()) != size) resizeStateVectors(size);

    for(int i = 0; i < size; i++) {
        int sourceIndex = sourceStart.get() + i;
        float basePitch = getSourceValue(sourceIndex);
        float deviation = i < static_cast<int>(deviationValues.size()) ? deviationValues[i] : 0.0f;
        currentPitches[i] = mapPitchWithDeviation(basePitch, deviation);
    }
}

void polyphonicArpeggiatorGUI::rebuildEuclideanOutputs() {
    int size = std::max(1, seqSize.get());
    std::vector<int> gateValues(size, 0);
    std::vector<int> accentValues(size, 0);
    std::vector<int> durationValues(size, 0);

    for(int i = 0; i < size; i++) gateValues[i] = isPulseActiveForStepPreview(i) ? 1 : 0;
    for(int i = 0; i < size; i++) accentValues[i] = isAccentPreviewLaneActive(i) ? 1 : 0;
    for(int i = 0; i < size; i++) durationValues[i] = isDurationPreviewLaneActive(i) ? 1 : 0;

    eucGateOut.set(gateValues);
    eucAccOut.set(accentValues);
    eucDurOut.set(durationValues);
}

float polyphonicArpeggiatorGUI::computeGeigerPulseProbability(double beatPosition) const {
    float density = ofClamp(geigerDensity.get(), 0.0f, 1.0f);
    float speed = std::max(0.001f, geigerSpeed.get());
    float periodicity = ofClamp(geigerPeriodicity.get(), 0.0f, 1.0f);
    if(density <= 0.0f) return 0.0f;

    double positiveBeatPosition = std::abs(beatPosition);
    double speedPhase = positiveBeatPosition * std::pow(static_cast<double>(speed), 1.18);
    double cycleLengthBeats = ofLerp(2.4, 0.09, density);
    double phase = std::fmod(speedPhase, cycleLengthBeats) / std::max(0.0001, cycleLengthBeats);
    float periodicSignal = 1.0f - std::abs(static_cast<float>(phase) * 2.0f - 1.0f);
    periodicSignal = std::pow(ofClamp(periodicSignal, 0.0f, 1.0f), ofLerp(4.2f, 0.75f, density));

    float diffuseEnvelope = ofLerp(0.82f, 0.16f, periodicity);
    float structuredEnvelope = ofLerp(0.12f, 1.0f, periodicity) * periodicSignal;
    float activity = density * ofLerp(diffuseEnvelope, structuredEnvelope + diffuseEnvelope * 0.15f, periodicity);

    float ratePerBeat = (0.35f + 26.0f * density) * std::pow(speed, 1.22f) * std::max(0.06f, activity);
    float probabilityPerMicroTick = 1.0f - std::exp(-ratePerBeat / static_cast<float>(geigerTransportStepsPerBeat));
    return ofClamp(probabilityPerMicroTick, 0.0f, 1.0f);
}

bool polyphonicArpeggiatorGUI::isGeigerGridLockedPulseForStep(int stepIndex) const {
    float density = ofClamp(geigerDensity.get(), 0.0f, 1.0f);
    if(density <= 0.0f) return false;

    double stepsPerBeat = std::max(0.001, static_cast<double>(beatDiv.get()));
    double effectiveRatePerBeat = std::max(0.001, static_cast<double>(density) * std::pow(static_cast<double>(std::max(0.001f, geigerSpeed.get())), 1.18));
    double effectiveRatePerStep = effectiveRatePerBeat / stepsPerBeat;
    if(effectiveRatePerStep >= 1.0) return true;

    int intervalSteps = std::max(1, static_cast<int>(std::round(1.0 / std::max(0.0001, effectiveRatePerStep))));
    return wrapIndex(stepIndex, intervalSteps) == 0;
}

bool polyphonicArpeggiatorGUI::isPulseActiveForStepLive(int stepIndex) {
    switch(pulseMode.get()) {
        case PeriodicPulse:
            return true;
        case EuclideanPulse:
            return euclideanPattern.empty() || euclideanPattern[wrapIndex(stepIndex, static_cast<int>(euclideanPattern.size()))];
        case GeigerPulse:
            if(geigerPeriodicity.get() >= 0.999f) return isGeigerGridLockedPulseForStep(stepIndex);
            return dist01(rng) <= computeGeigerPulseProbability(static_cast<double>(stepIndex) / std::max(0.001, static_cast<double>(beatDiv.get())));
        case StepSeqPulse: {
            const auto &pattern = pulseStepPattern.get();
            if(pattern.empty()) return true;
            float probability = ofClamp(static_cast<float>(pattern[wrapIndex(stepIndex, static_cast<int>(pattern.size()))]) / 100.0f, 0.0f, 1.0f);
            if(probability >= 1.0f) return true;
            if(probability <= 0.0f) return false;
            return dist01(rng) <= probability;
        }
        default:
            return true;
    }
}

bool polyphonicArpeggiatorGUI::isPulseActiveForStepPreview(int stepIndex) const {
    int shiftedStepIndex = getShiftedSequenceStepIndex(stepIndex);
    switch(pulseMode.get()) {
        case PeriodicPulse:
            return true;
        case EuclideanPulse:
            return euclideanPattern.empty() || euclideanPattern[wrapIndex(shiftedStepIndex, static_cast<int>(euclideanPattern.size()))];
        case GeigerPulse: {
            if(geigerPeriodicity.get() >= 0.999f) return isGeigerGridLockedPulseForStep(shiftedStepIndex);
            double stepStartBeat = static_cast<double>(shiftedStepIndex) / std::max(0.001, static_cast<double>(beatDiv.get()));
            double stepEndBeat = static_cast<double>(shiftedStepIndex + 1) / std::max(0.001, static_cast<double>(beatDiv.get()));
            int microTickCount = std::max(1, static_cast<int>(std::ceil((stepEndBeat - stepStartBeat) * geigerTransportStepsPerBeat)));
            for(int i = 0; i < microTickCount; i++) {
                double sampleBeat = stepStartBeat + (static_cast<double>(i) + 0.5) * (stepEndBeat - stepStartBeat) / static_cast<double>(microTickCount);
                int tickIndex = static_cast<int>(std::floor(sampleBeat * geigerTransportStepsPerBeat));
                std::mt19937 previewRng(4099 + tickIndex * 131 + seqSize.get() * 17);
                if(std::uniform_real_distribution<float>(0.0f, 1.0f)(previewRng) <= computeGeigerPulseProbability(sampleBeat)) {
                    return true;
                }
            }
            return false;
        }
        case StepSeqPulse: {
            const auto &pattern = pulseStepPattern.get();
            if(pattern.empty()) return true;
            float probability = ofClamp(static_cast<float>(pattern[wrapIndex(shiftedStepIndex, static_cast<int>(pattern.size()))]) / 100.0f, 0.0f, 1.0f);
            if(probability >= 1.0f) return true;
            if(probability <= 0.0f) return false;
            return computePatternRandomUnit(6001, shiftedStepIndex, 0, 504) <= probability;
        }
        default:
            return true;
    }
}

bool polyphonicArpeggiatorGUI::isStepPatternAlternateActive(int stepIndex, int every, int shift) const {
    int clampedEvery = std::max(1, every);
    return wrapIndex(stepIndex - shift, clampedEvery) == 0;
}

float polyphonicArpeggiatorGUI::computePatternRandomUnit(int seed, int stepIndex, int voiceIndex, int streamTag) const {
    uint32_t mixed = 2166136261u;
    auto mixValue = [&mixed](uint32_t value) {
        mixed ^= value + 0x9e3779b9u + (mixed << 6) + (mixed >> 2);
        mixed *= 16777619u;
    };

    mixValue(static_cast<uint32_t>(seed));
    mixValue(static_cast<uint32_t>(stepIndex + 0x4000));
    mixValue(static_cast<uint32_t>(voiceIndex + 0x2000));
    mixValue(static_cast<uint32_t>(streamTag));
    mixValue(static_cast<uint32_t>(seqSize.get()));
    mixValue(static_cast<uint32_t>(getEffectiveStepShift() + 0x1000));

    std::mt19937 generator(mixed);
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(generator);
}

bool polyphonicArpeggiatorGUI::isAccentStepActiveLive(int stepIndex) {
    switch(accentPatternMode.get()) {
        case AlternateEventPattern:
            return isStepPatternAlternateActive(stepIndex, accentAlternateSteps.get(), accentAlternateShift.get());
        case EuclideanEventPattern:
            return !euclideanAccents.empty() && euclideanAccents[wrapIndex(stepIndex, static_cast<int>(euclideanAccents.size()))];
        case RandomStepEventPattern:
            return accentChance.get() > 0.0f && computePatternRandomUnit(accentSeed.get(), stepIndex, 0, 101) <= accentChance.get();
        case RandomNoteEventPattern:
            return false;
        case StepSeqEventPattern: {
            const auto &pattern = accentStepPattern.get();
            return !pattern.empty() && pattern[wrapIndex(stepIndex, static_cast<int>(pattern.size()))] > 0;
        }
        default:
            return false;
    }
}

bool polyphonicArpeggiatorGUI::isAccentStepActivePreview(int stepIndex) const {
    switch(accentPatternMode.get()) {
        case AlternateEventPattern:
            return isStepPatternAlternateActive(stepIndex, accentAlternateSteps.get(), accentAlternateShift.get());
        case EuclideanEventPattern:
            return !euclideanAccents.empty() && euclideanAccents[wrapIndex(stepIndex, static_cast<int>(euclideanAccents.size()))];
        case RandomStepEventPattern:
            return accentChance.get() > 0.0f && computePatternRandomUnit(accentSeed.get(), stepIndex, 0, 101) <= accentChance.get();
        case RandomNoteEventPattern:
            return false;
        case StepSeqEventPattern: {
            const auto &pattern = accentStepPattern.get();
            return !pattern.empty() && pattern[wrapIndex(stepIndex, static_cast<int>(pattern.size()))] > 0;
        }
        default:
            return false;
    }
}

bool polyphonicArpeggiatorGUI::isDurationStepActiveLive(int stepIndex) {
    switch(durationPatternMode.get()) {
        case AlternateEventPattern:
            return isStepPatternAlternateActive(stepIndex, durationAlternateSteps.get(), durationAlternateShift.get());
        case EuclideanEventPattern:
            return !euclideanDurations.empty() && euclideanDurations[wrapIndex(stepIndex, static_cast<int>(euclideanDurations.size()))];
        case RandomStepEventPattern:
            return durationChance.get() > 0.0f && computePatternRandomUnit(durationSeed.get(), stepIndex, 0, 201) <= durationChance.get();
        case RandomNoteEventPattern:
            return false;
        case StepSeqEventPattern: {
            const auto &pattern = durationStepPattern.get();
            return !pattern.empty() && pattern[wrapIndex(stepIndex, static_cast<int>(pattern.size()))] > 0;
        }
        default:
            return false;
    }
}

bool polyphonicArpeggiatorGUI::isDurationStepActivePreview(int stepIndex) const {
    switch(durationPatternMode.get()) {
        case AlternateEventPattern:
            return isStepPatternAlternateActive(stepIndex, durationAlternateSteps.get(), durationAlternateShift.get());
        case EuclideanEventPattern:
            return !euclideanDurations.empty() && euclideanDurations[wrapIndex(stepIndex, static_cast<int>(euclideanDurations.size()))];
        case RandomStepEventPattern:
            return durationChance.get() > 0.0f && computePatternRandomUnit(durationSeed.get(), stepIndex, 0, 201) <= durationChance.get();
        case RandomNoteEventPattern:
            return false;
        case StepSeqEventPattern: {
            const auto &pattern = durationStepPattern.get();
            return !pattern.empty() && pattern[wrapIndex(stepIndex, static_cast<int>(pattern.size()))] > 0;
        }
        default:
            return false;
    }
}

bool polyphonicArpeggiatorGUI::isBassStepActiveLive(int stepIndex, bool stepAccentActive, bool stepDurationAccent) {
    switch(bassPatternMode.get()) {
        case BassAlternatePattern:
            return isStepPatternAlternateActive(stepIndex, bassAlternateSteps.get(), bassAlternateShift.get());
        case BassEuclideanPattern:
            return !euclideanBass.empty() && euclideanBass[wrapIndex(stepIndex, static_cast<int>(euclideanBass.size()))];
        case BassRandomPattern:
            return bassProb.get() > 0.0f && dist01(rng) <= bassProb.get();
        case BassVelAccentedPattern:
            return stepAccentActive;
        case BassDurAccentedPattern:
            return stepDurationAccent;
        case BassStartPattern:
            return wrapIndex(stepIndex, std::max(1, seqSize.get())) == 0;
        default:
            return false;
    }
}

bool polyphonicArpeggiatorGUI::isBassStepActivePreview(int stepIndex, bool stepAccentActive, bool stepDurationAccent) const {
    switch(bassPatternMode.get()) {
        case BassAlternatePattern:
            return isStepPatternAlternateActive(stepIndex, bassAlternateSteps.get(), bassAlternateShift.get());
        case BassEuclideanPattern:
            return !euclideanBass.empty() && euclideanBass[wrapIndex(stepIndex, static_cast<int>(euclideanBass.size()))];
        case BassRandomPattern:
            return bassProb.get() > 0.0f && computePatternRandomUnit(7001, stepIndex, 0, 301) <= bassProb.get();
        case BassVelAccentedPattern:
            return stepAccentActive;
        case BassDurAccentedPattern:
            return stepDurationAccent;
        case BassStartPattern:
            return wrapIndex(stepIndex, std::max(1, seqSize.get())) == 0;
        default:
            return false;
    }
}

bool polyphonicArpeggiatorGUI::isAccentNoteActiveLive(int stepIndex, int voiceIndex, bool stepAccentActive) {
    if(accentPatternMode.get() == RandomNoteEventPattern) {
        return accentChance.get() > 0.0f && computePatternRandomUnit(accentSeed.get(), stepIndex, voiceIndex, 102) <= accentChance.get();
    }
    return stepAccentActive;
}

bool polyphonicArpeggiatorGUI::isAccentNoteActivePreview(int stepIndex, int voiceIndex, bool stepAccentActive) const {
    if(accentPatternMode.get() == RandomNoteEventPattern) {
        return accentChance.get() > 0.0f && computePatternRandomUnit(accentSeed.get(), stepIndex, voiceIndex, 102) <= accentChance.get();
    }
    return stepAccentActive;
}

bool polyphonicArpeggiatorGUI::isDurationNoteActiveLive(int stepIndex, int voiceIndex, bool stepAccentActive) {
    if(durationPatternMode.get() == RandomNoteEventPattern) {
        return durationChance.get() > 0.0f && computePatternRandomUnit(durationSeed.get(), stepIndex, voiceIndex, 202) <= durationChance.get();
    }
    return stepAccentActive;
}

bool polyphonicArpeggiatorGUI::isDurationNoteActivePreview(int stepIndex, int voiceIndex, bool stepAccentActive) const {
    if(durationPatternMode.get() == RandomNoteEventPattern) {
        return durationChance.get() > 0.0f && computePatternRandomUnit(durationSeed.get(), stepIndex, voiceIndex, 202) <= durationChance.get();
    }
    return stepAccentActive;
}

bool polyphonicArpeggiatorGUI::isAccentPreviewLaneActive(int stepIndex) const {
    int shiftedStepIndex = getShiftedSequenceStepIndex(stepIndex);
    if(accentPatternMode.get() != RandomNoteEventPattern) {
        return isAccentStepActivePreview(shiftedStepIndex);
    }

    int previewPolyphony = std::max(1, polyphony.get());
    for(int voice = 0; voice < previewPolyphony; voice++) {
        if(isAccentNoteActivePreview(shiftedStepIndex, voice, false)) return true;
    }
    return false;
}

bool polyphonicArpeggiatorGUI::isDurationPreviewLaneActive(int stepIndex) const {
    int shiftedStepIndex = getShiftedSequenceStepIndex(stepIndex);
    if(durationPatternMode.get() != RandomNoteEventPattern) {
        return isDurationStepActivePreview(shiftedStepIndex);
    }

    int previewPolyphony = std::max(1, polyphony.get());
    for(int voice = 0; voice < previewPolyphony; voice++) {
        if(isDurationNoteActivePreview(shiftedStepIndex, voice, false)) return true;
    }
    return false;
}

float polyphonicArpeggiatorGUI::evaluateVelocityCurve(float normalizedPosition) const {
    const auto &curve = velocityCurve.get();
    if(curve.empty()) return 0.0f;
    if(curve.size() == 1) return ofClamp(curve.front(), 0.0f, 1.0f);

    float clampedPosition = ofClamp(normalizedPosition, 0.0f, 1.0f);
    float samplePosition = clampedPosition * static_cast<float>(curve.size() - 1);
    int leftIndex = ofClamp(static_cast<int>(std::floor(samplePosition)), 0, static_cast<int>(curve.size()) - 1);
    int rightIndex = std::min(leftIndex + 1, static_cast<int>(curve.size()) - 1);
    float interpolation = samplePosition - static_cast<float>(leftIndex);
    return ofClamp(ofLerp(curve[leftIndex], curve[rightIndex], interpolation), 0.0f, 1.0f);
}

float polyphonicArpeggiatorGUI::computeVelocityCurveOffsetForStep(int stepIndex) const {
    int size = std::max(1, seqSize.get());
    int wrappedStep = wrapIndex(stepIndex, size);
    float normalizedPosition = size <= 1 ? 0.0f : static_cast<float>(wrappedStep) / static_cast<float>(size - 1);
    return evaluateVelocityCurve(normalizedPosition);
}

float polyphonicArpeggiatorGUI::evaluateVelocityLfoWaveform(double beatPosition) const {
    double phaseOffset = static_cast<double>(velocityLfoPhase.get()) + static_cast<double>(velocitySlowSeed.get()) * 0.00091;
    double phase = beatPosition * std::max(0.0, static_cast<double>(velocitySlowSpeed.get())) + phaseOffset;
    double normalizedPhase = phase - std::floor(phase);

    switch(velocityLfoShape.get()) {
        case VelocityLfoSin:
            return static_cast<float>(0.5 + 0.5 * std::sin(TWO_PI * normalizedPhase));
        case VelocityLfoSaw:
            return static_cast<float>(normalizedPhase);
        case VelocityLfoInvSaw:
            return static_cast<float>(1.0 - normalizedPhase);
        case VelocityLfoPulse:
            return normalizedPhase < static_cast<double>(velocityLfoPulseWidth.get()) ? 1.0f : 0.0f;
        case VelocityLfoSlowRandom: {
            float seed = static_cast<float>(velocitySlowSeed.get()) * 0.00137f;
            return ofClamp(ofNoise(seed + 17.0f, static_cast<float>(phase) + seed), 0.0f, 1.0f);
        }
        default:
            return 0.0f;
    }
}

float polyphonicArpeggiatorGUI::computeLiveVelocityLfoOffset() const {
    if(velocitySlowRndm.get() <= 0.0f) return 0.0f;
    return ofClamp(evaluateVelocityLfoWaveform(currentTransportBeatPosition) * velocitySlowRndm.get(), 0.0f, 1.0f);
}

float polyphonicArpeggiatorGUI::computePreviewVelocityLfoOffset(double sequencePosition) const {
    if(velocitySlowRndm.get() <= 0.0f) return 0.0f;
    double beatsPerSequence = static_cast<double>(std::max(1, seqSize.get())) / std::max(0.001, static_cast<double>(beatDiv.get()));
    double previewBeatPosition = sequencePosition * beatsPerSequence;
    return ofClamp(evaluateVelocityLfoWaveform(previewBeatPosition) * velocitySlowRndm.get(), 0.0f, 1.0f);
}

float polyphonicArpeggiatorGUI::computeLiveStepVelocityRandomOffset(int modulationStepIndex) const {
    if(velStepRndm.get() <= 0.0f) return 0.0f;
    float randomUnit = computePatternRandomUnit(velRndSeed.get(),
                                                wrapIndex(modulationStepIndex, std::max(1, seqSize.get())),
                                                0,
                                                401);
    return ofClamp(velStepRndm.get() * randomUnit, 0.0f, 1.0f);
}

float polyphonicArpeggiatorGUI::computePreviewStepVelocityRandomOffset(int modulationStepIndex) const {
    if(velStepRndm.get() <= 0.0f) return 0.0f;
    float randomUnit = computePatternRandomUnit(velRndSeed.get(),
                                                wrapIndex(modulationStepIndex, std::max(1, seqSize.get())),
                                                0,
                                                401);
    return ofClamp(velStepRndm.get() * randomUnit, 0.0f, 1.0f);
}

float polyphonicArpeggiatorGUI::computeLiveNoteVelocity(int modulationStepIndex, int voiceIndex, bool accentActive, float stepRandomOffset) {
    float velocity = velBase.get();
    if(velRndm.get() > 0.0f) {
        float randomUnit = computePatternRandomUnit(velRndSeed.get(),
                                                    wrapIndex(modulationStepIndex, std::max(1, seqSize.get())),
                                                    voiceIndex,
                                                    402);
        velocity += velRndm.get() * randomUnit;
    }
    velocity += stepRandomOffset;
    velocity += computeVelocityCurveOffsetForStep(modulationStepIndex);
    velocity += computeLiveVelocityLfoOffset();
    if(accentActive) velocity += eucAccStrength.get();
    return ofClamp(velocity, 0.0f, 1.0f);
}

float polyphonicArpeggiatorGUI::computePreviewNoteVelocity(int eventStepIndex, int modulationStepIndex, int voiceIndex, bool accentActive, float stepRandomOffset) const {
    (void) eventStepIndex;

    float velocity = velBase.get();
    if(velRndm.get() > 0.0f) {
        float randomUnit = computePatternRandomUnit(velRndSeed.get(),
                                                    wrapIndex(modulationStepIndex, std::max(1, seqSize.get())),
                                                    voiceIndex,
                                                    402);
        velocity += velRndm.get() * randomUnit;
    }
    velocity += stepRandomOffset;
    velocity += computeVelocityCurveOffsetForStep(modulationStepIndex);
    velocity += computePreviewVelocityLfoOffset(static_cast<double>(wrapIndex(modulationStepIndex, std::max(1, seqSize.get()))) / static_cast<double>(std::max(1, seqSize.get())));
    if(accentActive) velocity += eucAccStrength.get();
    return ofClamp(velocity, 0.0f, 1.0f);
}

int polyphonicArpeggiatorGUI::computeLiveBassOctaveOffset(int stepIndex) const {
    int range = std::max(0, bassOctRnd.get());
    if(range <= 0 || bassOctRndChance.get() <= 0.0f) return 0;
    if(computePatternRandomUnit(bassOctRndSeed.get(), stepIndex, 0, 302) > bassOctRndChance.get()) return 0;

    int valueCount = range * 2 + 1;
    float randomUnit = computePatternRandomUnit(bassOctRndSeed.get(), stepIndex, 0, 303);
    int offsetIndex = ofClamp(static_cast<int>(std::floor(randomUnit * static_cast<float>(valueCount))), 0, valueCount - 1);
    return offsetIndex - range;
}

int polyphonicArpeggiatorGUI::computePreviewBassOctaveOffset(int stepIndex) const {
    int range = std::max(0, bassOctRnd.get());
    if(range <= 0 || bassOctRndChance.get() <= 0.0f) return 0;
    if(computePatternRandomUnit(bassOctRndSeed.get(), stepIndex, 0, 302) > bassOctRndChance.get()) return 0;

    int valueCount = range * 2 + 1;
    float randomUnit = computePatternRandomUnit(bassOctRndSeed.get(), stepIndex, 0, 303);
    int offsetIndex = ofClamp(static_cast<int>(std::floor(randomUnit * static_cast<float>(valueCount))), 0, valueCount - 1);
    return offsetIndex - range;
}

float polyphonicArpeggiatorGUI::computeLiveBassVelocity() {
    float velocity = bassVelBase.get();
    if(bassVelRndm.get() > 0.0f) velocity += bassVelRndm.get() * dist01(rng);
    return ofClamp(velocity, 0.0f, 1.0f);
}

float polyphonicArpeggiatorGUI::computePreviewBassVelocity(int stepIndex, int voiceIndex) const {
    float velocity = bassVelBase.get();
    if(bassVelRndm.get() > 0.0f) {
        std::mt19937 previewRng(13127 + stepIndex * 193 + voiceIndex * 37 + seqSize.get() * 17);
        velocity += bassVelRndm.get() * std::uniform_real_distribution<float>(0.0f, 1.0f)(previewRng);
    }
    return ofClamp(velocity, 0.0f, 1.0f);
}

float polyphonicArpeggiatorGUI::computeLiveDurationRandomOffset() {
    if(durRndm.get() <= 0.0f) return 0.0f;
    return durRndm.get() * dist01(rng);
}

float polyphonicArpeggiatorGUI::computePreviewDurationRandomOffset(int stepIndex, int voiceIndex) const {
    if(durRndm.get() <= 0.0f) return 0.0f;
    std::mt19937 previewRng(6131 + stepIndex * 227 + (voiceIndex + 2) * 41 + seqSize.get() * 29);
    return durRndm.get() * std::uniform_real_distribution<float>(0.0f, 1.0f)(previewRng);
}

int polyphonicArpeggiatorGUI::computeNoteDuration(bool accentActive, float randomOffset) const {
    float durationUnits = durBase.get();
    if(accentActive) durationUnits += durEucStrength.get();
    durationUnits += randomOffset;
    durationUnits = ofClamp(durationUnits, 0.0f, 16.0f);

    float stepDurationMs = getVisualizationStepDurationMs();
    int durationMs = static_cast<int>(std::round(durationUnits * stepDurationMs));
    return ofClamp(durationMs, 1, 60000);
}

float polyphonicArpeggiatorGUI::computeStrumOffset(int stepIndex, int voiceIndex, int totalVoices) const {
    if(totalVoices <= 1 || strum.get() <= 0.0f) return 0.0f;

    float stepDurationMs = getVisualizationStepDurationMs();
    float baseStrum = strum.get();
    if(strumRndm.get() > 0.0f) {
        float randomUnit = computePatternRandomUnit(strumRndSeed.get(),
                                                    wrapIndex(stepIndex, std::max(1, seqSize.get())),
                                                    voiceIndex,
                                                    501);
        float randomOffset = (randomUnit * 2.0f - 1.0f) * strumRndm.get();
        baseStrum = std::max(0.0f, baseStrum + randomOffset);
    }
    baseStrum *= stepDurationMs;

    if(strumDir.get() == 0) return voiceIndex * baseStrum;
    if(strumDir.get() == 1) return (totalVoices - 1 - voiceIndex) * baseStrum;
        float randomUnit = computePatternRandomUnit(strumRndSeed.get(),
                                                    wrapIndex(stepIndex, std::max(1, seqSize.get())),
                                                    voiceIndex,
                                                    502);
    return randomUnit * (totalVoices - 1) * baseStrum;
}

float polyphonicArpeggiatorGUI::computePreviewStrumOffset(int stepIndex, int voiceIndex, int totalVoices) const {
    if(totalVoices <= 1 || strum.get() <= 0.0f) return 0.0f;

    float stepDurationMs = getVisualizationStepDurationMs();
    float baseStrum = strum.get();
    if(strumRndm.get() > 0.0f) {
        float randomUnit = computePatternRandomUnit(strumRndSeed.get(),
                                                    wrapIndex(stepIndex, std::max(1, seqSize.get())),
                                                    voiceIndex,
                                                    501);
        float randomOffset = (randomUnit * 2.0f - 1.0f) * strumRndm.get();
        baseStrum = std::max(0.0f, baseStrum + randomOffset);
    }
    baseStrum *= stepDurationMs;

    if(strumDir.get() == 0) return voiceIndex * baseStrum;
    if(strumDir.get() == 1) return (totalVoices - 1 - voiceIndex) * baseStrum;
    float randomUnit = computePatternRandomUnit(strumRndSeed.get(),
                                                wrapIndex(stepIndex, std::max(1, seqSize.get())),
                                                voiceIndex,
                                                502);
    return randomUnit * (totalVoices - 1) * baseStrum;
}

float polyphonicArpeggiatorGUI::computeLivePositionOffset(int stepIndex, int voiceIndex, int totalVoices) const {
    float stepDurationMs = getVisualizationStepDurationMs();
    float offsetMs = computeStrumOffset(stepIndex, voiceIndex, totalVoices);
    if(geigerTransportPulseActive) {
        offsetMs += pendingTransportOffsetMs;
    }
    if(swing.get() > 0.0f && wrapIndex(stepIndex, 2) == 1) {
        offsetMs += swing.get() * 0.5f * stepDurationMs;
    }
    if(positionRndm.get() > 0.0f) {
        float randomUnit = computePatternRandomUnit(strumRndSeed.get(),
                                                    wrapIndex(stepIndex, std::max(1, seqSize.get())),
                                                    voiceIndex,
                                                    503);
        offsetMs += randomUnit * positionRndm.get() * stepDurationMs;
    }
    return std::max(0.0f, offsetMs);
}

float polyphonicArpeggiatorGUI::computePreviewPositionOffset(int stepIndex, int voiceIndex, int totalVoices) const {
    float stepDurationMs = getVisualizationStepDurationMs();
    float offsetMs = computePreviewStrumOffset(stepIndex, voiceIndex, totalVoices);
    if(swing.get() > 0.0f && wrapIndex(stepIndex, 2) == 1) {
        offsetMs += swing.get() * 0.5f * stepDurationMs;
    }
    if(positionRndm.get() > 0.0f) {
        float randomUnit = computePatternRandomUnit(strumRndSeed.get(),
                                                    wrapIndex(stepIndex, std::max(1, seqSize.get())),
                                                    voiceIndex,
                                                    503);
        offsetMs += randomUnit * positionRndm.get() * stepDurationMs;
    }
    return std::max(0.0f, offsetMs);
}

polyphonicArpeggiatorGUI::StepPreviewInfo polyphonicArpeggiatorGUI::buildStepPreview(int stepIndex) const {
    StepPreviewInfo info;
    int shiftedStepIndex = getShiftedSequenceStepIndex(stepIndex);
    info.gate = isPulseActiveForStepPreview(stepIndex);
    bool stepAccentActive = isAccentStepActivePreview(shiftedStepIndex);
    bool stepDurationAccent = isDurationStepActivePreview(shiftedStepIndex);
    float sharedDurationOffset = durationRndPerStep.get() ? computePreviewDurationRandomOffset(shiftedStepIndex, -1) : 0.0f;

    if(!info.gate) return info;

    int size = std::max(1, seqSize.get());
    int poly = std::min(polyphony.get() + (stepAccentActive ? polyAccent.get() : 0), MaxPolyphony);
    int interval = std::max(1, polyInterval.get());

    int patternOffset = getPatternOffsetForStepPreview(stepIndex);
    int baseRelativeOffset = patternOffset * std::max(0, sourceStride.get());
    int baseSourceIndex = sourceStart.get() + baseRelativeOffset;
    float sharedStepVelocityOffset = computePreviewStepVelocityRandomOffset(stepIndex);

    float previousPreviewPitch = std::numeric_limits<float>::quiet_NaN();
    for(int voice = 0; voice < poly; voice++) {
        int sourceIndex = baseSourceIndex + voice * interval;
        float mappedPitch = 60.0f;
        int maxAttempts = std::max(1, static_cast<int>(activeSourceValues.size()));
        bool applyFold = (voice == 0) || foldPoly.get();

        for(int attempt = 0; attempt < maxAttempts; attempt++) {
            int candidateIndex = sourceIndex + attempt;
            std::mt19937 previewPitchRng(5527 + shiftedStepIndex * 197 + voice * 31 + size * 7 + candidateIndex * 13);
            float basePitch = getSourceValue(candidateIndex);
            float deviation = generateDeviationForSourceIndex(candidateIndex, previewPitchRng);
            mappedPitch = mapPitchWithDeviation(basePitch, deviation, applyFold);
            if(!std::isfinite(previousPreviewPitch) || std::abs(mappedPitch - previousPreviewPitch) > 0.001f) {
                break;
            }
        }

        previousPreviewPitch = mappedPitch;
        bool noteAccentActive = isAccentNoteActivePreview(shiftedStepIndex, voice, stepAccentActive);
        bool noteDurationAccent = isDurationNoteActivePreview(shiftedStepIndex, voice, stepDurationAccent);
        float noteDurationOffset = durationRndPerStep.get() ? sharedDurationOffset : computePreviewDurationRandomOffset(shiftedStepIndex, voice);
        int previewDuration = computeNoteDuration(noteDurationAccent, noteDurationOffset);
        float previewVelocity = computePreviewNoteVelocity(shiftedStepIndex, stepIndex, voice, noteAccentActive, sharedStepVelocityOffset);
        info.notes.push_back(mappedPitch);
        info.noteVelocities.push_back(previewVelocity);
        info.noteDurations.push_back(previewDuration);
        info.noteAccents.push_back(noteAccentActive);
        info.noteDurationAccents.push_back(noteDurationAccent);
    }

    bool bassModeActive = addBass.get() && isBassStepActivePreview(shiftedStepIndex, stepAccentActive, stepDurationAccent);
    bool shouldAddBass = bassModeActive;
    if(shouldAddBass && bassPatternMode.get() != BassRandomPattern) {
        shouldAddBass = bassProb.get() >= 1.0f || computePatternRandomUnit(7919, shiftedStepIndex, 0, 303) <= bassProb.get();
    }
    if(shouldAddBass) {
        int bassVoices = std::max(1, bassPolyphony.get());
        int bassOctaveOffset = computePreviewBassOctaveOffset(shiftedStepIndex);
        float bassDurationOffset = durationRndPerStep.get() ? sharedDurationOffset : computePreviewDurationRandomOffset(shiftedStepIndex, poly);
        int bassDuration = computeNoteDuration(stepDurationAccent, bassDurationOffset);
        for(int bassVoice = bassVoices - 1; bassVoice >= 0; bassVoice--) {
            info.notes.insert(info.notes.begin(), mapBassPitch(bassVoice, bassOctaveOffset));
            info.noteVelocities.insert(info.noteVelocities.begin(), computePreviewBassVelocity(shiftedStepIndex, bassVoice));
            info.noteDurations.insert(info.noteDurations.begin(), bassDuration);
            info.noteAccents.insert(info.noteAccents.begin(), stepAccentActive);
            info.noteDurationAccents.insert(info.noteDurationAccents.begin(), stepDurationAccent);
        }
    }

    if(!info.noteAccents.empty()) {
        info.accent = std::any_of(info.noteAccents.begin(), info.noteAccents.end(), [](bool value) { return value; });
    } else {
        info.accent = stepAccentActive;
    }
    if(!info.noteVelocities.empty()) info.velocity = *std::max_element(info.noteVelocities.begin(), info.noteVelocities.end());
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
    if(sourceMode.get() == ChordPool) return sourceChangePending ? pendingSourceValues : activeSourceValues;

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

void polyphonicArpeggiatorGUI::refreshSnapshotsFromSharedStorage(bool force) {
    if(!force && observedSnapshotStorageRevision == sharedSnapshotStorageRevision) return;

    loadAllSnapshotsFromDisk();
    if(activeSnapshotSlot >= 0 && snapshotSlots.count(activeSnapshotSlot) == 0) {
        activeSnapshotSlot = -1;
    }

    observedSnapshotStorageRevision = sharedSnapshotStorageRevision;
}

std::string polyphonicArpeggiatorGUI::getSnapshotsFolderPath() const {
    return ofToDataPath("nodeSnapshots/PolyphonicArpeggiatorGUI/", true);
}

std::string polyphonicArpeggiatorGUI::getSnapshotFilePath(int slot) const {
    return getSnapshotsFolderPath() + "snapshot_" + ofToString(slot) + ".json";
}

int polyphonicArpeggiatorGUI::getNextAvailableSnapshotSlot() const {
    int slot = 0;
    while(snapshotSlots.count(slot) > 0) slot++;
    return slot;
}

void polyphonicArpeggiatorGUI::saveSnapshotToDisk(int slot) const {
    if(slot < 0) return;
    auto it = snapshotSlots.find(slot);
    if(it == snapshotSlots.end() || !it->second.hasData) return;

    ofDirectory dir(getSnapshotsFolderPath());
    if(!dir.exists()) dir.create(true);

    const auto &snap = it->second;
    ofJson json;
    json["name"] = snap.name;
    json["sourceMode"] = snap.sourceMode;
    json["internalClockMode"] = snap.internalClockMode;
    json["oneShotMode"] = snap.oneShotMode;
    json["beatDiv"] = snap.beatDiv;
    json["pulseMode"] = snap.pulseMode;
    json["pulseStepPattern"] = snap.pulseStepPattern;
    json["geigerSpeed"] = snap.geigerSpeed;
    json["geigerDensity"] = snap.geigerDensity;
    json["geigerPeriodicity"] = snap.geigerPeriodicity;
    json["geigerChaos"] = snap.geigerChaos;
    json["seqSize"] = snap.seqSize;
    json["scale"] = snap.scale;
    json["patternMode"] = snap.patternMode;
    json["patternSeed"] = snap.patternSeed;
    json["patternRandomizeSeedEachCycle"] = snap.patternRandomizeSeedEachCycle;
    json["idxPattern"] = snap.idxPattern;
    json["modulo"] = snap.modulo;
    json["sourceStart"] = snap.sourceStart;
    json["sourceStride"] = snap.sourceStride;
    json["stepShift"] = snap.stepShift;
    json["rndShiftChance"] = snap.rndShiftChance;
    json["rndShiftRange"] = snap.rndShiftRange;
    json["rndShiftQuant"] = snap.rndShiftQuant;
    json["octave"] = snap.octave;
    json["octaveFold"] = snap.octaveFold;
    json["foldPoly"] = snap.foldPoly;
    json["root"] = snap.root;
    json["pitchExpand"] = snap.pitchExpand;
    json["expandStep"] = snap.expandStep;
    json["transpose"] = snap.transpose;
    json["sortPool"] = snap.sortPool;
    json["removeDuplicates"] = snap.removeDuplicates;
    json["sourceChangeMode"] = snap.sourceChangeMode;
    json["polyphony"] = snap.polyphony;
    json["polyInterval"] = snap.polyInterval;
    json["polyAccent"] = snap.polyAccent;
    json["addBass"] = snap.addBass;
    json["bassPatternMode"] = snap.bassPatternMode;
    json["bassAlternateSteps"] = snap.bassAlternateSteps;
    json["bassAlternateShift"] = snap.bassAlternateShift;
    json["bassEucLen"] = snap.bassEucLen;
    json["bassEucHits"] = snap.bassEucHits;
    json["bassEucOff"] = snap.bassEucOff;
    json["bassOctave"] = snap.bassOctave;
    json["bassProb"] = snap.bassProb;
    json["bassOnAccent"] = snap.bassOnAccent;
    json["bassOctRnd"] = snap.bassOctRnd;
    json["bassOctRndChance"] = snap.bassOctRndChance;
    json["bassOctRndSeed"] = snap.bassOctRndSeed;
    json["bassOctRndRandomizeSeedEachCycle"] = snap.bassOctRndRandomizeSeedEachCycle;
    json["bassPolyphony"] = snap.bassPolyphony;
    json["bassVelBase"] = snap.bassVelBase;
    json["bassVelRndm"] = snap.bassVelRndm;
    json["bassPitchModified"] = snap.bassPitchModified;
    json["skipSteps"] = snap.skipSteps;
    json["strum"] = snap.strum;
    json["strumRndm"] = snap.strumRndm;
    json["strumRndSeed"] = snap.strumRndSeed;
    json["strumRndRandomizeSeedEachCycle"] = snap.strumRndRandomizeSeedEachCycle;
    json["swing"] = snap.swing;
    json["strumDir"] = snap.strumDir;
    json["strumDirSeed"] = snap.strumDirSeed;
    json["strumDirRandomizeSeedEachCycle"] = snap.strumDirRandomizeSeedEachCycle;
    json["positionRndm"] = snap.positionRndm;
    json["positionRndSeed"] = snap.positionRndSeed;
    json["positionRndRandomizeSeedEachCycle"] = snap.positionRndRandomizeSeedEachCycle;
    json["octaveDev"] = snap.octaveDev;
    json["octaveDevRng"] = snap.octaveDevRng;
    json["idxDev"] = snap.idxDev;
    json["idxDevRng"] = snap.idxDevRng;
    json["pitchDev"] = snap.pitchDev;
    json["pitchDevRng"] = snap.pitchDevRng;
    json["velBase"] = snap.velBase;
    json["velRndm"] = snap.velRndm;
    json["velRndSeed"] = snap.velRndSeed;
    json["velRndRandomizeSeedEachCycle"] = snap.velRndRandomizeSeedEachCycle;
    json["velStepRndm"] = snap.velStepRndm;
    json["velStepRndSeed"] = snap.velStepRndSeed;
    json["velStepRndRandomizeSeedEachCycle"] = snap.velStepRndRandomizeSeedEachCycle;
    json["velocityCurve"] = snap.velocityCurve;
    json["velocitySlowRndm"] = snap.velocitySlowRndm;
    json["velocitySlowSpeed"] = snap.velocitySlowSpeed;
    json["velocitySlowSeed"] = snap.velocitySlowSeed;
    json["velocityLfoShape"] = snap.velocityLfoShape;
    json["velocityLfoPhase"] = snap.velocityLfoPhase;
    json["velocityLfoPulseWidth"] = snap.velocityLfoPulseWidth;
    json["accentPatternMode"] = snap.accentPatternMode;
    json["accentAlternateSteps"] = snap.accentAlternateSteps;
    json["accentAlternateShift"] = snap.accentAlternateShift;
    json["accentChance"] = snap.accentChance;
    json["accentSeed"] = snap.accentSeed;
    json["accentRandomizeSeedEachCycle"] = snap.accentRandomizeSeedEachCycle;
    json["accentStepPattern"] = snap.accentStepPattern;
    json["eucAccStrength"] = snap.eucAccStrength;
    json["durBase"] = snap.durBase;
    json["durRndm"] = snap.durRndm;
    json["durationPatternMode"] = snap.durationPatternMode;
    json["durationAlternateSteps"] = snap.durationAlternateSteps;
    json["durationAlternateShift"] = snap.durationAlternateShift;
    json["durationChance"] = snap.durationChance;
    json["durationSeed"] = snap.durationSeed;
    json["durationRandomizeSeedEachCycle"] = snap.durationRandomizeSeedEachCycle;
    json["durationStepPattern"] = snap.durationStepPattern;
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
    json["seqProb"] = snap.seqProb;
    json["seqProbCycles"] = snap.seqProbCycles;
    json["runGateBeats"] = snap.runGateBeats;
    json["runGateChance"] = snap.runGateChance;
    json["runGatePhase"] = snap.runGatePhase;
    json["stepChance"] = snap.stepChance;
    json["noteChance"] = snap.noteChance;
    json["dynamicMode"] = snap.dynamicMode;
    json["accentOnsetMode"] = snap.accentOnsetMode;

    ofSavePrettyJson(getSnapshotFilePath(slot), json);
    markSharedSnapshotStorageChanged();
}

void polyphonicArpeggiatorGUI::loadSnapshotFromDisk(int slot) {
    if(slot < 0) return;
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
    snap.pulseMode = json.contains("pulseMode") ? json.value("pulseMode", PeriodicPulse) : EuclideanPulse;
    snap.geigerSpeed = json.value("geigerSpeed", 1.0f);
    snap.geigerDensity = json.value("geigerDensity", 0.45f);
    snap.geigerPeriodicity = json.value("geigerPeriodicity", 0.75f);
    snap.geigerChaos = json.value("geigerChaos", 0.35f);
    snap.seqSize = json.value("seqSize", 16);
    snap.pulseStepPattern = json.value("pulseStepPattern", std::vector<int>(std::max(1, snap.seqSize), 1));
    snap.scale = json.value("scale", std::vector<float>{0, 2, 4, 5, 7, 9, 11});
    snap.patternMode = json.value("patternMode", 0);
    snap.patternSeed = json.value("patternSeed", 0);
    snap.patternRandomizeSeedEachCycle = json.value("patternRandomizeSeedEachCycle", false);
    snap.idxPattern = json.value("idxPattern", std::vector<int>{0, 1, 2, 3});
    snap.modulo = json.value("modulo", 0);
    snap.sourceStart = json.value("sourceStart", 0);
    snap.sourceStride = json.value("sourceStride", 1);
    snap.stepShift = json.value("stepShift", 0);
    snap.rndShiftChance = json.value("rndShiftChance", 0.0f);
    snap.rndShiftRange = json.value("rndShiftRange", 0);
    snap.rndShiftQuant = json.value("rndShiftQuant", 1);
    snap.octave = json.value("octave", 0);
    snap.octaveFold = json.value("octaveFold", 0);
    snap.foldPoly = json.value("foldPoly", true);
    snap.root = json.value("root", 0.0f);
    snap.pitchExpand = json.value("pitchExpand", true);
    snap.expandStep = json.value("expandStep", 12);
    snap.transpose = json.value("transpose", 0);
    snap.sortPool = json.value("sortPool", true);
    snap.removeDuplicates = json.value("removeDuplicates", false);
    snap.sourceChangeMode = json.value("sourceChangeMode", KeepPhase);
    snap.polyphony = json.value("polyphony", 1);
    snap.polyInterval = json.value("polyInterval", 2);
    snap.polyAccent = json.value("polyAccent", 0);
    snap.addBass = json.value("addBass", false);
    snap.bassOnAccent = json.value("bassOnAccent", false);
    snap.bassPatternMode = json.contains("bassPatternMode")
                         ? json.value("bassPatternMode", BassAlternatePattern)
                         : (snap.bassOnAccent ? BassVelAccentedPattern : BassAlternatePattern);
    snap.bassAlternateSteps = json.value("bassAlternateSteps", 2);
    snap.bassAlternateShift = json.value("bassAlternateShift", 0);
    snap.bassEucLen = json.value("bassEucLen", 8);
    snap.bassEucHits = json.value("bassEucHits", 2);
    snap.bassEucOff = json.value("bassEucOff", 0);
    snap.bassOctave = json.value("bassOctave", -2);
    snap.bassProb = json.value("bassProb", 1.0f);
    snap.bassOctRnd = json.value("bassOctRnd", 0);
    snap.bassOctRndChance = json.value("bassOctRndChance", 0.0f);
    snap.bassOctRndSeed = json.value("bassOctRndSeed", 0);
    snap.bassOctRndRandomizeSeedEachCycle = json.value("bassOctRndRandomizeSeedEachCycle", false);
    snap.bassPolyphony = json.value("bassPolyphony", 1);
    snap.bassVelBase = json.value("bassVelBase", 0.75f);
    snap.bassVelRndm = json.value("bassVelRndm", 0.0f);
    snap.bassPitchModified = json.value("bassPitchModified", true);
    snap.skipSteps = json.value("skipSteps", 0);
    snap.strum = json.value("strum", 0.0f);
    snap.strumRndm = json.value("strumRndm", 0.0f);
    snap.strumRndSeed = json.value("strumRndSeed", 0);
    snap.strumRndRandomizeSeedEachCycle = json.value("strumRndRandomizeSeedEachCycle", false);
    snap.swing = json.value("swing", 0.0f);
    snap.strumDir = json.value("strumDir", 0);
    snap.strumDirSeed = json.value("strumDirSeed", 0);
    snap.strumDirRandomizeSeedEachCycle = json.value("strumDirRandomizeSeedEachCycle", false);
    snap.positionRndm = json.value("positionRndm", 0.0f);
    snap.positionRndSeed = json.value("positionRndSeed", 0);
    snap.positionRndRandomizeSeedEachCycle = json.value("positionRndRandomizeSeedEachCycle", false);
    snap.octaveDev = json.value("octaveDev", 0.0f);
    snap.octaveDevRng = json.value("octaveDevRng", 1);
    snap.idxDev = json.value("idxDev", 0.0f);
    snap.idxDevRng = json.value("idxDevRng", 2);
    snap.pitchDev = json.value("pitchDev", 0.0f);
    snap.pitchDevRng = json.value("pitchDevRng", 2);
    snap.velBase = json.value("velBase", 0.8f);
    snap.velRndm = json.value("velRndm", 0.1f);
    snap.velRndSeed = json.value("velRndSeed", 0);
    snap.velRndRandomizeSeedEachCycle = json.value("velRndRandomizeSeedEachCycle", false);
    snap.velStepRndm = json.value("velStepRndm", 0.0f);
    snap.velStepRndSeed = json.value("velStepRndSeed", 0);
    snap.velStepRndRandomizeSeedEachCycle = json.value("velStepRndRandomizeSeedEachCycle", false);
    if(snap.strumRndSeed == 0) {
        if(snap.strumDirSeed != 0) snap.strumRndSeed = snap.strumDirSeed;
        else if(snap.positionRndSeed != 0) snap.strumRndSeed = snap.positionRndSeed;
    }
    snap.strumRndRandomizeSeedEachCycle = snap.strumRndRandomizeSeedEachCycle
                                       || snap.strumDirRandomizeSeedEachCycle
                                       || snap.positionRndRandomizeSeedEachCycle;
    if(snap.velRndSeed == 0 && snap.velStepRndSeed != 0) {
        snap.velRndSeed = snap.velStepRndSeed;
    }
    snap.velRndRandomizeSeedEachCycle = snap.velRndRandomizeSeedEachCycle
                                     || snap.velStepRndRandomizeSeedEachCycle;
    snap.velocityCurve = json.value("velocityCurve", std::vector<float>(VelocityCurveResolution, 0.0f));
    snap.velocitySlowRndm = json.value("velocitySlowRndm", 0.0f);
    snap.velocitySlowSpeed = json.value("velocitySlowSpeed", 0.25f);
    snap.velocitySlowSeed = json.value("velocitySlowSeed", 0);
    snap.velocityLfoShape = json.value("velocityLfoShape", VelocityLfoSlowRandom);
    snap.velocityLfoPhase = json.value("velocityLfoPhase", 0.0f);
    snap.velocityLfoPulseWidth = json.value("velocityLfoPulseWidth", 0.5f);
    snap.accentPatternMode = json.value("accentPatternMode", EuclideanEventPattern);
    snap.accentAlternateSteps = json.value("accentAlternateSteps", 2);
    snap.accentAlternateShift = json.value("accentAlternateShift", 0);
    snap.accentChance = json.value("accentChance", 0.5f);
    snap.accentSeed = json.value("accentSeed", 0);
    snap.accentRandomizeSeedEachCycle = json.value("accentRandomizeSeedEachCycle", false);
    snap.accentStepPattern = json.value("accentStepPattern", std::vector<int>(std::max(1, snap.seqSize), 0));
    snap.eucAccStrength = json.value("eucAccStrength", 0.2f);
    snap.durBase = json.value("durBase", 1.0f);
    snap.durRndm = json.value("durRndm", 0.0f);
    snap.durationPatternMode = json.value("durationPatternMode", EuclideanEventPattern);
    snap.durationAlternateSteps = json.value("durationAlternateSteps", 2);
    snap.durationAlternateShift = json.value("durationAlternateShift", 0);
    snap.durationChance = json.value("durationChance", 0.5f);
    snap.durationSeed = json.value("durationSeed", 0);
    snap.durationRandomizeSeedEachCycle = json.value("durationRandomizeSeedEachCycle", false);
    snap.durationStepPattern = json.value("durationStepPattern", std::vector<int>(std::max(1, snap.seqSize), 0));
    snap.durEucStrength = json.value("durEucStrength", 0.0f);
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
    snap.seqProb = json.value("seqProb", 1.0f);
    snap.seqProbCycles = json.value("seqProbCycles", 1);
    snap.runGateBeats = json.value("runGateBeats", 16.0f);
    snap.runGateChance = json.value("runGateChance", 1.0f);
    snap.runGatePhase = json.value("runGatePhase", 0.0f);
    snap.stepChance = json.value("stepChance", 1.0f);
    snap.noteChance = json.value("noteChance", 1.0f);
    snap.dynamicMode = json.value("dynamicMode", false);
    snap.accentOnsetMode = json.value("accentOnsetMode", true);
    if(snap.durBase > 4.0f || snap.durRndm > 4.0f || snap.durEucStrength > 4.0f) {
        snap.durBase = convertLegacyDurationMsToUnits(snap.durBase, snap.beatDiv);
        snap.durRndm = convertLegacyDurationMsToUnits(snap.durRndm, snap.beatDiv);
        snap.durEucStrength = convertLegacyDurationMsToUnits(snap.durEucStrength, snap.beatDiv);
    }
    if(snap.strum > 1.0f) {
        snap.strum = convertLegacyPositionMsToUnits(snap.strum, snap.beatDiv);
    }
    if(snap.strumRndm > 1.0f) {
        snap.strumRndm = convertLegacyPositionMsToUnits(snap.strumRndm, snap.beatDiv);
    }
    if(snap.positionRndm > 1.0f) {
        snap.positionRndm = convertLegacyPositionMsToUnits(snap.positionRndm, snap.beatDiv);
    }
    if(snap.velocityCurve.empty()) snap.velocityCurve.assign(VelocityCurveResolution, 0.0f);
    if(snap.velocityCurve.size() != static_cast<size_t>(VelocityCurveResolution)) {
        std::vector<float> resized(VelocityCurveResolution, 0.0f);
        if(snap.velocityCurve.size() == 1) {
            std::fill(resized.begin(), resized.end(), ofClamp(snap.velocityCurve.front(), 0.0f, 1.0f));
        } else {
            for(int i = 0; i < VelocityCurveResolution; i++) {
                float normalized = static_cast<float>(i) / static_cast<float>(VelocityCurveResolution - 1);
                float sourcePosition = normalized * static_cast<float>(snap.velocityCurve.size() - 1);
                int leftIndex = ofClamp(static_cast<int>(std::floor(sourcePosition)), 0, static_cast<int>(snap.velocityCurve.size()) - 1);
                int rightIndex = std::min(leftIndex + 1, static_cast<int>(snap.velocityCurve.size()) - 1);
                float lerpAmount = sourcePosition - static_cast<float>(leftIndex);
                resized[i] = ofClamp(ofLerp(snap.velocityCurve[leftIndex], snap.velocityCurve[rightIndex], lerpAmount), 0.0f, 1.0f);
            }
        }
        snap.velocityCurve = std::move(resized);
    } else {
        for(float &value : snap.velocityCurve) value = ofClamp(value, 0.0f, 1.0f);
    }
    snap.hasData = true;
}

void polyphonicArpeggiatorGUI::loadAllSnapshotsFromDisk() {
    snapshotSlots.clear();

    ofDirectory dir(getSnapshotsFolderPath());
    if(!dir.exists()) return;

    dir.allowExt("json");
    dir.listDir();
    for(const auto &file : dir.getFiles()) {
        std::string fileName = file.getFileName();
        const std::string prefix = "snapshot_";
        const std::string suffix = ".json";
        if(fileName.size() <= prefix.size() + suffix.size()) continue;
        if(fileName.compare(0, prefix.size(), prefix) != 0) continue;
        if(fileName.compare(fileName.size() - suffix.size(), suffix.size(), suffix) != 0) continue;

        std::string slotString = fileName.substr(prefix.size(), fileName.size() - prefix.size() - suffix.size());
        if(slotString.empty()) continue;
        bool allDigits = std::all_of(slotString.begin(), slotString.end(), [](char c) { return c >= '0' && c <= '9'; });
        if(!allDigits) continue;

        loadSnapshotFromDisk(ofToInt(slotString));
    }
}

void polyphonicArpeggiatorGUI::deleteSnapshotFromDisk(int slot) {
    if(slot < 0) return;

    ofFile file(getSnapshotFilePath(slot));
    if(file.exists()) file.remove();
    snapshotSlots.erase(slot);
    if(activeSnapshotSlot == slot) activeSnapshotSlot = -1;
    markSharedSnapshotStorageChanged();
}

void polyphonicArpeggiatorGUI::storeToSlot(int slot) {
    if(slot < 0) return;

    auto &snap = snapshotSlots[slot];
    std::string existingName = snap.name;
    snap = polyphonicArpeggiatorGUISnapshot();
    snap.name = existingName.empty() ? "Snapshot " + ofToString(slot + 1) : existingName;
    snap.sourceMode = sourceMode.get();
    snap.internalClockMode = internalClockMode.get();
    snap.oneShotMode = oneShotMode.get();
    snap.beatDiv = beatDiv.get();
    snap.pulseMode = pulseMode.get();
    snap.pulseStepPattern = pulseStepPattern.get();
    snap.geigerSpeed = geigerSpeed.get();
    snap.geigerDensity = geigerDensity.get();
    snap.geigerPeriodicity = geigerPeriodicity.get();
    snap.geigerChaos = geigerChaos.get();
    snap.seqSize = seqSize.get();
    snap.scale = scale.get();
    snap.patternMode = patternMode.get();
    snap.patternSeed = patternSeed.get();
    snap.patternRandomizeSeedEachCycle = patternRandomizeSeedEachCycle.get();
    snap.idxPattern = idxPattern.get();
    snap.modulo = modulo.get();
    snap.sourceStart = sourceStart.get();
    snap.sourceStride = sourceStride.get();
    snap.stepShift = stepShift.get();
    snap.rndShiftChance = rndShiftChance.get();
    snap.rndShiftRange = rndShiftRange.get();
    snap.rndShiftQuant = rndShiftQuant.get();
    snap.octave = octave.get();
    snap.octaveFold = octaveFold.get();
    snap.foldPoly = foldPoly.get();
    snap.root = root.get();
    snap.pitchExpand = pitchExpand.get();
    snap.expandStep = expandStep.get();
    snap.transpose = transpose.get();
    snap.sortPool = sortPool.get();
    snap.removeDuplicates = removeDuplicates.get();
    snap.sourceChangeMode = sourceChangeMode.get();
    snap.polyphony = polyphony.get();
    snap.polyInterval = polyInterval.get();
    snap.polyAccent = polyAccent.get();
    snap.addBass = addBass.get();
    snap.bassPatternMode = bassPatternMode.get();
    snap.bassAlternateSteps = bassAlternateSteps.get();
    snap.bassAlternateShift = bassAlternateShift.get();
    snap.bassEucLen = bassEucLen.get();
    snap.bassEucHits = bassEucHits.get();
    snap.bassEucOff = bassEucOff.get();
    snap.bassOctave = bassOctave.get();
    snap.bassProb = bassProb.get();
    snap.bassOnAccent = bassOnAccent.get();
    snap.bassOctRnd = bassOctRnd.get();
    snap.bassOctRndChance = bassOctRndChance.get();
    snap.bassOctRndSeed = bassOctRndSeed.get();
    snap.bassOctRndRandomizeSeedEachCycle = bassOctRndRandomizeSeedEachCycle.get();
    snap.bassPolyphony = bassPolyphony.get();
    snap.bassVelBase = bassVelBase.get();
    snap.bassVelRndm = bassVelRndm.get();
    snap.bassPitchModified = bassPitchModified.get();
    snap.skipSteps = skipSteps.get();
    snap.strum = strum.get();
    snap.strumRndm = strumRndm.get();
    snap.strumRndSeed = strumRndSeed.get();
    snap.strumRndRandomizeSeedEachCycle = strumRndRandomizeSeedEachCycle.get();
    snap.swing = swing.get();
    snap.strumDir = strumDir.get();
    snap.strumDirSeed = snap.strumRndSeed;
    snap.strumDirRandomizeSeedEachCycle = snap.strumRndRandomizeSeedEachCycle;
    snap.positionRndm = positionRndm.get();
    snap.positionRndSeed = snap.strumRndSeed;
    snap.positionRndRandomizeSeedEachCycle = snap.strumRndRandomizeSeedEachCycle;
    snap.octaveDev = octaveDev.get();
    snap.octaveDevRng = octaveDevRng.get();
    snap.idxDev = idxDev.get();
    snap.idxDevRng = idxDevRng.get();
    snap.pitchDev = pitchDev.get();
    snap.pitchDevRng = pitchDevRng.get();
    snap.velBase = velBase.get();
    snap.velRndm = velRndm.get();
    snap.velRndSeed = velRndSeed.get();
    snap.velRndRandomizeSeedEachCycle = velRndRandomizeSeedEachCycle.get();
    snap.velStepRndm = velStepRndm.get();
    snap.velStepRndSeed = snap.velRndSeed;
    snap.velStepRndRandomizeSeedEachCycle = snap.velRndRandomizeSeedEachCycle;
    snap.velocityCurve = velocityCurve.get();
    snap.velocitySlowRndm = velocitySlowRndm.get();
    snap.velocitySlowSpeed = velocitySlowSpeed.get();
    snap.velocitySlowSeed = velocitySlowSeed.get();
    snap.velocityLfoShape = velocityLfoShape.get();
    snap.velocityLfoPhase = velocityLfoPhase.get();
    snap.velocityLfoPulseWidth = velocityLfoPulseWidth.get();
    snap.accentPatternMode = accentPatternMode.get();
    snap.accentAlternateSteps = accentAlternateSteps.get();
    snap.accentAlternateShift = accentAlternateShift.get();
    snap.accentChance = accentChance.get();
    snap.accentSeed = accentSeed.get();
    snap.accentRandomizeSeedEachCycle = accentRandomizeSeedEachCycle.get();
    snap.accentStepPattern = accentStepPattern.get();
    snap.eucAccStrength = eucAccStrength.get();
    snap.durBase = durBase.get();
    snap.durRndm = durRndm.get();
    snap.durationPatternMode = durationPatternMode.get();
    snap.durationAlternateSteps = durationAlternateSteps.get();
    snap.durationAlternateShift = durationAlternateShift.get();
    snap.durationChance = durationChance.get();
    snap.durationSeed = durationSeed.get();
    snap.durationRandomizeSeedEachCycle = durationRandomizeSeedEachCycle.get();
    snap.durationStepPattern = durationStepPattern.get();
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
    snap.seqProb = seqProb.get();
    snap.seqProbCycles = seqProbCycles.get();
    snap.runGateBeats = runGateBeats.get();
    snap.runGateChance = runGateChance.get();
    snap.runGatePhase = runGatePhase.get();
    snap.stepChance = stepChance.get();
    snap.noteChance = noteChance.get();
    snap.dynamicMode = dynamicMode.get();
    snap.accentOnsetMode = accentOnsetMode.get();
    snap.hasData = true;

    activeSnapshotSlot = slot;
    saveSnapshotToDisk(slot);
}

void polyphonicArpeggiatorGUI::recallSlot(int slot) {
    refreshSnapshotsFromSharedStorage();

    auto it = snapshotSlots.find(slot);
    if(slot < 0 || it == snapshotSlots.end() || !it->second.hasData) return;

    activeSnapshotSlot = slot;
    isMorphing = false;
    const auto &snap = it->second;
    sourceMode = snap.sourceMode;
    internalClockMode = snap.internalClockMode;
    oneShotMode = snap.oneShotMode;
    beatDiv = snap.beatDiv;
    pulseMode = snap.pulseMode;
    pulseStepPattern = snap.pulseStepPattern;
    geigerSpeed = snap.geigerSpeed;
    geigerDensity = snap.geigerDensity;
    geigerPeriodicity = snap.geigerPeriodicity;
    geigerChaos = snap.geigerChaos;
    seqSize = snap.seqSize;
    scale = snap.scale;
    patternMode = snap.patternMode;
    patternSeed = snap.patternSeed;
    patternRandomizeSeedEachCycle = snap.patternRandomizeSeedEachCycle;
    idxPattern = snap.idxPattern;
    modulo = snap.modulo;
    sourceStart = snap.sourceStart;
    sourceStride = snap.sourceStride;
    stepShift = snap.stepShift;
    rndShiftChance = snap.rndShiftChance;
    rndShiftRange = snap.rndShiftRange;
    rndShiftQuant = snap.rndShiftQuant;
    octave = snap.octave;
    octaveFold = snap.octaveFold;
    foldPoly = snap.foldPoly;
    root = snap.root;
    pitchExpand = snap.pitchExpand;
    expandStep = snap.expandStep;
    transpose = snap.transpose;
    sortPool = snap.sortPool;
    removeDuplicates = snap.removeDuplicates;
    sourceChangeMode = snap.sourceChangeMode;
    polyphony = snap.polyphony;
    polyInterval = snap.polyInterval;
    polyAccent = snap.polyAccent;
    addBass = snap.addBass;
    bassPatternMode = snap.bassPatternMode;
    bassAlternateSteps = snap.bassAlternateSteps;
    bassAlternateShift = snap.bassAlternateShift;
    bassEucLen = snap.bassEucLen;
    bassEucHits = snap.bassEucHits;
    bassEucOff = snap.bassEucOff;
    bassOctave = snap.bassOctave;
    bassProb = snap.bassProb;
    bassOnAccent = snap.bassOnAccent;
    bassOctRnd = snap.bassOctRnd;
    bassOctRndChance = snap.bassOctRndChance;
    bassOctRndSeed = snap.bassOctRndSeed;
    bassOctRndRandomizeSeedEachCycle = snap.bassOctRndRandomizeSeedEachCycle;
    bassPolyphony = snap.bassPolyphony;
    bassVelBase = snap.bassVelBase;
    bassVelRndm = snap.bassVelRndm;
    bassPitchModified = snap.bassPitchModified;
    skipSteps = snap.skipSteps;
    strum = snap.strum;
    strumRndm = snap.strumRndm;
    strumRndSeed = snap.strumRndSeed;
    strumRndRandomizeSeedEachCycle = snap.strumRndRandomizeSeedEachCycle;
    swing = snap.swing;
    strumDir = snap.strumDir;
    strumDirSeed = strumRndSeed;
    strumDirRandomizeSeedEachCycle = strumRndRandomizeSeedEachCycle;
    positionRndm = snap.positionRndm;
    positionRndSeed = strumRndSeed;
    positionRndRandomizeSeedEachCycle = strumRndRandomizeSeedEachCycle;
    octaveDev = snap.octaveDev;
    octaveDevRng = snap.octaveDevRng;
    idxDev = snap.idxDev;
    idxDevRng = snap.idxDevRng;
    pitchDev = snap.pitchDev;
    pitchDevRng = snap.pitchDevRng;
    velBase = snap.velBase;
    velRndm = snap.velRndm;
    velRndSeed = snap.velRndSeed;
    velRndRandomizeSeedEachCycle = snap.velRndRandomizeSeedEachCycle;
    velStepRndm = snap.velStepRndm;
    velStepRndSeed = velRndSeed;
    velStepRndRandomizeSeedEachCycle = velRndRandomizeSeedEachCycle;
    velocityCurve = snap.velocityCurve;
    velocitySlowRndm = snap.velocitySlowRndm;
    velocitySlowSpeed = snap.velocitySlowSpeed;
    velocitySlowSeed = snap.velocitySlowSeed;
    velocityLfoShape = snap.velocityLfoShape;
    velocityLfoPhase = snap.velocityLfoPhase;
    velocityLfoPulseWidth = snap.velocityLfoPulseWidth;
    accentPatternMode = snap.accentPatternMode;
    accentAlternateSteps = snap.accentAlternateSteps;
    accentAlternateShift = snap.accentAlternateShift;
    accentChance = snap.accentChance;
    accentSeed = snap.accentSeed;
    accentRandomizeSeedEachCycle = snap.accentRandomizeSeedEachCycle;
    accentStepPattern = snap.accentStepPattern;
    eucAccStrength = snap.eucAccStrength;
    durBase = snap.durBase;
    durRndm = snap.durRndm;
    durationPatternMode = snap.durationPatternMode;
    durationAlternateSteps = snap.durationAlternateSteps;
    durationAlternateShift = snap.durationAlternateShift;
    durationChance = snap.durationChance;
    durationSeed = snap.durationSeed;
    durationRandomizeSeedEachCycle = snap.durationRandomizeSeedEachCycle;
    durationStepPattern = snap.durationStepPattern;
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
    seqProb = snap.seqProb;
    seqProbCycles = snap.seqProbCycles;
    runGateBeats = snap.runGateBeats;
    runGateChance = snap.runGateChance;
    runGatePhase = snap.runGatePhase;
    stepChance = snap.stepChance;
    noteChance = snap.noteChance;
    dynamicMode = snap.dynamicMode;
    accentOnsetMode = snap.accentOnsetMode;
    syncUserPatternToSequenceSize();
    syncPulseStepPatternToSequenceSize();
    syncAccentStepPatternToSequenceSize();
    syncDurationStepPatternToSequenceSize();
    syncVelocityCurveShape();
    generateEuclideanPattern(euclideanBass, bassEucLen.get(), bassEucHits.get(), bassEucOff.get());
    internalClockNeedsSync = true;
    currentCycleRandomStepShift = 0;
    sequenceCycleDecisionPending = true;
    skippedSequenceCyclesRemaining = 0;
    runGateWindowStateValid = false;
}

void polyphonicArpeggiatorGUI::updateMorph() {
    float progress = (ofGetElapsedTimef() - morphStartTime) / std::max(0.001f, morphTime.get());
    if(progress >= 1.0f) {
        progress = 1.0f;
        isMorphing = false;
    }

    seqSize = static_cast<int>(ofLerp(startSnapshot.seqSize, targetSnapshot.seqSize, progress));
    beatDiv = ofLerp(startSnapshot.beatDiv, targetSnapshot.beatDiv, progress);
    geigerSpeed = ofLerp(startSnapshot.geigerSpeed, targetSnapshot.geigerSpeed, progress);
    geigerDensity = ofLerp(startSnapshot.geigerDensity, targetSnapshot.geigerDensity, progress);
    geigerPeriodicity = ofLerp(startSnapshot.geigerPeriodicity, targetSnapshot.geigerPeriodicity, progress);
    geigerChaos = ofLerp(startSnapshot.geigerChaos, targetSnapshot.geigerChaos, progress);
    modulo = static_cast<int>(ofLerp(startSnapshot.modulo, targetSnapshot.modulo, progress));
    sourceStart = static_cast<int>(ofLerp(startSnapshot.sourceStart, targetSnapshot.sourceStart, progress));
    sourceStride = static_cast<int>(ofLerp(startSnapshot.sourceStride, targetSnapshot.sourceStride, progress));
    stepShift = static_cast<int>(ofLerp(startSnapshot.stepShift, targetSnapshot.stepShift, progress));
    octave = static_cast<int>(ofLerp(startSnapshot.octave, targetSnapshot.octave, progress));
    octaveFold = static_cast<int>(ofLerp(startSnapshot.octaveFold, targetSnapshot.octaveFold, progress));
    root = ofLerp(startSnapshot.root, targetSnapshot.root, progress);
    expandStep = static_cast<int>(ofLerp(startSnapshot.expandStep, targetSnapshot.expandStep, progress));
    transpose = static_cast<int>(ofLerp(startSnapshot.transpose, targetSnapshot.transpose, progress));
    polyphony = static_cast<int>(ofLerp(startSnapshot.polyphony, targetSnapshot.polyphony, progress));
    polyInterval = static_cast<int>(ofLerp(startSnapshot.polyInterval, targetSnapshot.polyInterval, progress));
    polyAccent = static_cast<int>(ofLerp(startSnapshot.polyAccent, targetSnapshot.polyAccent, progress));
    bassAlternateSteps = static_cast<int>(ofLerp(startSnapshot.bassAlternateSteps, targetSnapshot.bassAlternateSteps, progress));
    bassAlternateShift = static_cast<int>(ofLerp(startSnapshot.bassAlternateShift, targetSnapshot.bassAlternateShift, progress));
    bassEucLen = static_cast<int>(ofLerp(startSnapshot.bassEucLen, targetSnapshot.bassEucLen, progress));
    bassEucHits = static_cast<int>(ofLerp(startSnapshot.bassEucHits, targetSnapshot.bassEucHits, progress));
    bassEucOff = static_cast<int>(ofLerp(startSnapshot.bassEucOff, targetSnapshot.bassEucOff, progress));
    bassOctave = static_cast<int>(ofLerp(startSnapshot.bassOctave, targetSnapshot.bassOctave, progress));
    bassProb = ofLerp(startSnapshot.bassProb, targetSnapshot.bassProb, progress);
    bassOctRnd = static_cast<int>(ofLerp(startSnapshot.bassOctRnd, targetSnapshot.bassOctRnd, progress));
    bassOctRndChance = ofLerp(startSnapshot.bassOctRndChance, targetSnapshot.bassOctRndChance, progress);
    bassOctRndSeed = static_cast<int>(ofLerp(startSnapshot.bassOctRndSeed, targetSnapshot.bassOctRndSeed, progress));
    bassPolyphony = static_cast<int>(ofLerp(startSnapshot.bassPolyphony, targetSnapshot.bassPolyphony, progress));
    bassVelBase = ofLerp(startSnapshot.bassVelBase, targetSnapshot.bassVelBase, progress);
    bassVelRndm = ofLerp(startSnapshot.bassVelRndm, targetSnapshot.bassVelRndm, progress);
    skipSteps = static_cast<int>(ofLerp(startSnapshot.skipSteps, targetSnapshot.skipSteps, progress));
    strum = ofLerp(startSnapshot.strum, targetSnapshot.strum, progress);
    strumRndm = ofLerp(startSnapshot.strumRndm, targetSnapshot.strumRndm, progress);
    strumRndSeed = static_cast<int>(ofLerp(startSnapshot.strumRndSeed, targetSnapshot.strumRndSeed, progress));
    swing = ofLerp(startSnapshot.swing, targetSnapshot.swing, progress);
    positionRndm = ofLerp(startSnapshot.positionRndm, targetSnapshot.positionRndm, progress);
    positionRndSeed = static_cast<int>(ofLerp(startSnapshot.positionRndSeed, targetSnapshot.positionRndSeed, progress));
    octaveDev = ofLerp(startSnapshot.octaveDev, targetSnapshot.octaveDev, progress);
    octaveDevRng = static_cast<int>(ofLerp(startSnapshot.octaveDevRng, targetSnapshot.octaveDevRng, progress));
    idxDev = ofLerp(startSnapshot.idxDev, targetSnapshot.idxDev, progress);
    idxDevRng = static_cast<int>(ofLerp(startSnapshot.idxDevRng, targetSnapshot.idxDevRng, progress));
    pitchDev = ofLerp(startSnapshot.pitchDev, targetSnapshot.pitchDev, progress);
    pitchDevRng = static_cast<int>(ofLerp(startSnapshot.pitchDevRng, targetSnapshot.pitchDevRng, progress));
    velBase = ofLerp(startSnapshot.velBase, targetSnapshot.velBase, progress);
    velRndm = ofLerp(startSnapshot.velRndm, targetSnapshot.velRndm, progress);
    velRndSeed = static_cast<int>(ofLerp(startSnapshot.velRndSeed, targetSnapshot.velRndSeed, progress));
    velStepRndm = ofLerp(startSnapshot.velStepRndm, targetSnapshot.velStepRndm, progress);
    velStepRndSeed = static_cast<int>(ofLerp(startSnapshot.velStepRndSeed, targetSnapshot.velStepRndSeed, progress));
    velocitySlowRndm = ofLerp(startSnapshot.velocitySlowRndm, targetSnapshot.velocitySlowRndm, progress);
    velocitySlowSpeed = ofLerp(startSnapshot.velocitySlowSpeed, targetSnapshot.velocitySlowSpeed, progress);
    velocitySlowSeed = static_cast<int>(ofLerp(startSnapshot.velocitySlowSeed, targetSnapshot.velocitySlowSeed, progress));
    velocityLfoPhase = ofLerp(startSnapshot.velocityLfoPhase, targetSnapshot.velocityLfoPhase, progress);
    velocityLfoPulseWidth = ofLerp(startSnapshot.velocityLfoPulseWidth, targetSnapshot.velocityLfoPulseWidth, progress);
    accentAlternateSteps = static_cast<int>(ofLerp(startSnapshot.accentAlternateSteps, targetSnapshot.accentAlternateSteps, progress));
    accentAlternateShift = static_cast<int>(ofLerp(startSnapshot.accentAlternateShift, targetSnapshot.accentAlternateShift, progress));
    accentChance = ofLerp(startSnapshot.accentChance, targetSnapshot.accentChance, progress);
    accentSeed = static_cast<int>(ofLerp(startSnapshot.accentSeed, targetSnapshot.accentSeed, progress));
    eucAccStrength = ofLerp(startSnapshot.eucAccStrength, targetSnapshot.eucAccStrength, progress);
    durBase = ofLerp(startSnapshot.durBase, targetSnapshot.durBase, progress);
    durRndm = ofLerp(startSnapshot.durRndm, targetSnapshot.durRndm, progress);
    durationAlternateSteps = static_cast<int>(ofLerp(startSnapshot.durationAlternateSteps, targetSnapshot.durationAlternateSteps, progress));
    durationAlternateShift = static_cast<int>(ofLerp(startSnapshot.durationAlternateShift, targetSnapshot.durationAlternateShift, progress));
    durationChance = ofLerp(startSnapshot.durationChance, targetSnapshot.durationChance, progress);
    durationSeed = static_cast<int>(ofLerp(startSnapshot.durationSeed, targetSnapshot.durationSeed, progress));
    durEucStrength = ofLerp(startSnapshot.durEucStrength, targetSnapshot.durEucStrength, progress);
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
    seqProb = ofLerp(startSnapshot.seqProb, targetSnapshot.seqProb, progress);
    seqProbCycles = static_cast<int>(ofLerp(startSnapshot.seqProbCycles, targetSnapshot.seqProbCycles, progress));
    stepChance = ofLerp(startSnapshot.stepChance, targetSnapshot.stepChance, progress);
    noteChance = ofLerp(startSnapshot.noteChance, targetSnapshot.noteChance, progress);

    if(progress >= 1.0f) {
        sourceMode = targetSnapshot.sourceMode;
        internalClockMode = targetSnapshot.internalClockMode;
        oneShotMode = targetSnapshot.oneShotMode;
        pulseMode = targetSnapshot.pulseMode;
        pulseStepPattern = targetSnapshot.pulseStepPattern;
        scale = targetSnapshot.scale;
        patternMode = targetSnapshot.patternMode;
        patternSeed = targetSnapshot.patternSeed;
        patternRandomizeSeedEachCycle = targetSnapshot.patternRandomizeSeedEachCycle;
        idxPattern = targetSnapshot.idxPattern;
        modulo = targetSnapshot.modulo;
        beatDiv = targetSnapshot.beatDiv;
        geigerSpeed = targetSnapshot.geigerSpeed;
        geigerDensity = targetSnapshot.geigerDensity;
        geigerPeriodicity = targetSnapshot.geigerPeriodicity;
        geigerChaos = targetSnapshot.geigerChaos;
        stepShift = targetSnapshot.stepShift;
        octave = targetSnapshot.octave;
        octaveFold = targetSnapshot.octaveFold;
        foldPoly = targetSnapshot.foldPoly;
        root = targetSnapshot.root;
        pitchExpand = targetSnapshot.pitchExpand;
        expandStep = targetSnapshot.expandStep;
        sortPool = targetSnapshot.sortPool;
        removeDuplicates = targetSnapshot.removeDuplicates;
        sourceChangeMode = targetSnapshot.sourceChangeMode;
        polyAccent = targetSnapshot.polyAccent;
        addBass = targetSnapshot.addBass;
        bassPatternMode = targetSnapshot.bassPatternMode;
        bassAlternateSteps = targetSnapshot.bassAlternateSteps;
        bassAlternateShift = targetSnapshot.bassAlternateShift;
        bassEucLen = targetSnapshot.bassEucLen;
        bassEucHits = targetSnapshot.bassEucHits;
        bassEucOff = targetSnapshot.bassEucOff;
        swing = targetSnapshot.swing;
        strumDir = targetSnapshot.strumDir;
        strumDirSeed = targetSnapshot.strumDirSeed;
        strumDirRandomizeSeedEachCycle = targetSnapshot.strumDirRandomizeSeedEachCycle;
        bassOctave = targetSnapshot.bassOctave;
        bassProb = targetSnapshot.bassProb;
        bassOnAccent = targetSnapshot.bassOnAccent;
        bassOctRnd = targetSnapshot.bassOctRnd;
        bassOctRndChance = targetSnapshot.bassOctRndChance;
        bassOctRndSeed = targetSnapshot.bassOctRndSeed;
        bassOctRndRandomizeSeedEachCycle = targetSnapshot.bassOctRndRandomizeSeedEachCycle;
        bassPolyphony = targetSnapshot.bassPolyphony;
        bassVelBase = targetSnapshot.bassVelBase;
        bassVelRndm = targetSnapshot.bassVelRndm;
        bassPitchModified = targetSnapshot.bassPitchModified;
        positionRndm = targetSnapshot.positionRndm;
        positionRndSeed = targetSnapshot.positionRndSeed;
        positionRndRandomizeSeedEachCycle = targetSnapshot.positionRndRandomizeSeedEachCycle;
        strumRndm = targetSnapshot.strumRndm;
        strumRndSeed = targetSnapshot.strumRndSeed;
        strumRndRandomizeSeedEachCycle = targetSnapshot.strumRndRandomizeSeedEachCycle;
        velRndm = targetSnapshot.velRndm;
        velRndSeed = targetSnapshot.velRndSeed;
        velRndRandomizeSeedEachCycle = targetSnapshot.velRndRandomizeSeedEachCycle;
        velStepRndm = targetSnapshot.velStepRndm;
        velStepRndSeed = targetSnapshot.velStepRndSeed;
        velStepRndRandomizeSeedEachCycle = targetSnapshot.velStepRndRandomizeSeedEachCycle;
        velocityCurve = targetSnapshot.velocityCurve;
        velocitySlowRndm = targetSnapshot.velocitySlowRndm;
        velocitySlowSpeed = targetSnapshot.velocitySlowSpeed;
        velocitySlowSeed = targetSnapshot.velocitySlowSeed;
        velocityLfoShape = targetSnapshot.velocityLfoShape;
        velocityLfoPhase = targetSnapshot.velocityLfoPhase;
        velocityLfoPulseWidth = targetSnapshot.velocityLfoPulseWidth;
        dynamicMode = targetSnapshot.dynamicMode;
        accentOnsetMode = targetSnapshot.accentOnsetMode;
        accentPatternMode = targetSnapshot.accentPatternMode;
        accentAlternateSteps = targetSnapshot.accentAlternateSteps;
        accentAlternateShift = targetSnapshot.accentAlternateShift;
        accentChance = targetSnapshot.accentChance;
        accentSeed = targetSnapshot.accentSeed;
        accentRandomizeSeedEachCycle = targetSnapshot.accentRandomizeSeedEachCycle;
        accentStepPattern = targetSnapshot.accentStepPattern;
        durationPatternMode = targetSnapshot.durationPatternMode;
        durationAlternateSteps = targetSnapshot.durationAlternateSteps;
        durationAlternateShift = targetSnapshot.durationAlternateShift;
        durationChance = targetSnapshot.durationChance;
        durationSeed = targetSnapshot.durationSeed;
        durationRandomizeSeedEachCycle = targetSnapshot.durationRandomizeSeedEachCycle;
        durationStepPattern = targetSnapshot.durationStepPattern;
        durationRndPerStep = targetSnapshot.durationRndPerStep;
        outputHistoryWindowMs = targetSnapshot.outputHistoryWindowMs;
        seqProb = targetSnapshot.seqProb;
        seqProbCycles = targetSnapshot.seqProbCycles;
        syncUserPatternToSequenceSize();
        syncPulseStepPatternToSequenceSize();
        syncAccentStepPatternToSequenceSize();
        syncDurationStepPatternToSequenceSize();
        syncVelocityCurveShape();
        generateEuclideanPattern(euclideanBass, bassEucLen.get(), bassEucHits.get(), bassEucOff.get());
        internalClockNeedsSync = true;
        sequenceCycleDecisionPending = true;
        skippedSequenceCyclesRemaining = 0;
    }
}

void polyphonicArpeggiatorGUI::presetSave(ofJson &json) {
    json["currentStep"] = currentStep;
    json["highlightedStep"] = highlightedStep;
    json["activeSnapshotSlot"] = activeSnapshotSlot;
    json["publishedEditorParameters"] = publishedEditorParameterKeys;

    ofJson state;
    state["editorWidth"] = editorWidth.get();
    state["editorHeight"] = editorHeight.get();
    saveEditorStateValue(state, "sourceMode", sourceMode);
    saveEditorStateValue(state, "sortPool", sortPool);
    saveEditorStateValue(state, "removeDuplicates", removeDuplicates);
    saveEditorStateValue(state, "sourceChangeMode", sourceChangeMode);
    saveEditorStateValue(state, "patternMode", patternMode);
    saveEditorStateValue(state, "patternSeed", patternSeed);
    saveEditorStateValue(state, "patternRandomizeSeedEachCycle", patternRandomizeSeedEachCycle);
    saveEditorStateValue(state, "modulo", modulo);
    saveEditorStateValue(state, "pulseMode", pulseMode);
    saveEditorStateValue(state, "pulseStepPattern", pulseStepPattern);
    saveEditorStateValue(state, "geigerSpeed", geigerSpeed);
    saveEditorStateValue(state, "geigerDensity", geigerDensity);
    saveEditorStateValue(state, "geigerPeriodicity", geigerPeriodicity);
    saveEditorStateValue(state, "geigerChaos", geigerChaos);
    saveEditorStateValue(state, "idxPattern", idxPattern);
    saveEditorStateValue(state, "seqSize", seqSize);
    saveEditorStateValue(state, "sourceStart", sourceStart);
    saveEditorStateValue(state, "sourceStride", sourceStride);
    saveEditorStateValue(state, "stepShift", stepShift);
    saveEditorStateValue(state, "rndShiftChance", rndShiftChance);
    saveEditorStateValue(state, "rndShiftRange", rndShiftRange);
    saveEditorStateValue(state, "rndShiftQuant", rndShiftQuant);
    saveEditorStateValue(state, "octave", octave);
    saveEditorStateValue(state, "octaveFold", octaveFold);
    saveEditorStateValue(state, "foldPoly", foldPoly);
    saveEditorStateValue(state, "pitchExpand", pitchExpand);
    saveEditorStateValue(state, "expandStep", expandStep);
    saveEditorStateValue(state, "transpose", transpose);
    saveEditorStateValue(state, "dynamicMode", dynamicMode);
    saveEditorStateValue(state, "accentOnsetMode", accentOnsetMode);
    saveEditorStateValue(state, "polyphony", polyphony);
    saveEditorStateValue(state, "polyInterval", polyInterval);
    saveEditorStateValue(state, "polyAccent", polyAccent);
    saveEditorStateValue(state, "addBass", addBass);
    saveEditorStateValue(state, "bassPatternMode", bassPatternMode);
    saveEditorStateValue(state, "bassAlternateSteps", bassAlternateSteps);
    saveEditorStateValue(state, "bassAlternateShift", bassAlternateShift);
    saveEditorStateValue(state, "bassEucLen", bassEucLen);
    saveEditorStateValue(state, "bassEucHits", bassEucHits);
    saveEditorStateValue(state, "bassEucOff", bassEucOff);
    saveEditorStateValue(state, "bassOctave", bassOctave);
    saveEditorStateValue(state, "bassProb", bassProb);
    saveEditorStateValue(state, "bassOnAccent", bassOnAccent);
    saveEditorStateValue(state, "bassOctRnd", bassOctRnd);
    saveEditorStateValue(state, "bassOctRndChance", bassOctRndChance);
    saveEditorStateValue(state, "bassOctRndSeed", bassOctRndSeed);
    saveEditorStateValue(state, "bassOctRndRandomizeSeedEachCycle", bassOctRndRandomizeSeedEachCycle);
    saveEditorStateValue(state, "bassPolyphony", bassPolyphony);
    saveEditorStateValue(state, "bassVelBase", bassVelBase);
    saveEditorStateValue(state, "bassVelRndm", bassVelRndm);
    saveEditorStateValue(state, "bassPitchModified", bassPitchModified);
    saveEditorStateValue(state, "skipSteps", skipSteps);
    saveEditorStateValue(state, "strum", strum);
    saveEditorStateValue(state, "strumRndm", strumRndm);
    saveEditorStateValue(state, "strumRndSeed", strumRndSeed);
    saveEditorStateValue(state, "strumRndRandomizeSeedEachCycle", strumRndRandomizeSeedEachCycle);
    saveEditorStateValue(state, "swing", swing);
    saveEditorStateValue(state, "strumDir", strumDir);
    saveEditorStateValue(state, "strumDirSeed", strumDirSeed);
    saveEditorStateValue(state, "strumDirRandomizeSeedEachCycle", strumDirRandomizeSeedEachCycle);
    saveEditorStateValue(state, "positionRndm", positionRndm);
    saveEditorStateValue(state, "positionRndSeed", positionRndSeed);
    saveEditorStateValue(state, "positionRndRandomizeSeedEachCycle", positionRndRandomizeSeedEachCycle);
    saveEditorStateValue(state, "octaveDev", octaveDev);
    saveEditorStateValue(state, "octaveDevRng", octaveDevRng);
    saveEditorStateValue(state, "idxDev", idxDev);
    saveEditorStateValue(state, "idxDevRng", idxDevRng);
    saveEditorStateValue(state, "pitchDev", pitchDev);
    saveEditorStateValue(state, "pitchDevRng", pitchDevRng);
    saveEditorStateValue(state, "eucLen", eucLen);
    saveEditorStateValue(state, "eucHits", eucHits);
    saveEditorStateValue(state, "eucOff", eucOff);
    saveEditorStateValue(state, "seqProb", seqProb);
    saveEditorStateValue(state, "seqProbCycles", seqProbCycles);
    saveEditorStateValue(state, "runGateBeats", runGateBeats);
    saveEditorStateValue(state, "runGateChance", runGateChance);
    saveEditorStateValue(state, "runGatePhase", runGatePhase);
    saveEditorStateValue(state, "stepChance", stepChance);
    saveEditorStateValue(state, "noteChance", noteChance);
    saveEditorStateValue(state, "velBase", velBase);
    saveEditorStateValue(state, "velRndm", velRndm);
    saveEditorStateValue(state, "velRndSeed", velRndSeed);
    saveEditorStateValue(state, "velRndRandomizeSeedEachCycle", velRndRandomizeSeedEachCycle);
    saveEditorStateValue(state, "velStepRndm", velStepRndm);
    saveEditorStateValue(state, "velStepRndSeed", velStepRndSeed);
    saveEditorStateValue(state, "velStepRndRandomizeSeedEachCycle", velStepRndRandomizeSeedEachCycle);
    saveEditorStateValue(state, "velocityCurve", velocityCurve);
    saveEditorStateValue(state, "velocitySlowRndm", velocitySlowRndm);
    saveEditorStateValue(state, "velocitySlowSpeed", velocitySlowSpeed);
    saveEditorStateValue(state, "velocitySlowSeed", velocitySlowSeed);
    saveEditorStateValue(state, "velocityLfoShape", velocityLfoShape);
    saveEditorStateValue(state, "velocityLfoPhase", velocityLfoPhase);
    saveEditorStateValue(state, "velocityLfoPulseWidth", velocityLfoPulseWidth);
    saveEditorStateValue(state, "accentPatternMode", accentPatternMode);
    saveEditorStateValue(state, "accentAlternateSteps", accentAlternateSteps);
    saveEditorStateValue(state, "accentAlternateShift", accentAlternateShift);
    saveEditorStateValue(state, "accentChance", accentChance);
    saveEditorStateValue(state, "accentSeed", accentSeed);
    saveEditorStateValue(state, "accentRandomizeSeedEachCycle", accentRandomizeSeedEachCycle);
    saveEditorStateValue(state, "accentStepPattern", accentStepPattern);
    saveEditorStateValue(state, "eucAccLen", eucAccLen);
    saveEditorStateValue(state, "eucAccHits", eucAccHits);
    saveEditorStateValue(state, "eucAccOff", eucAccOff);
    saveEditorStateValue(state, "eucAccStrength", eucAccStrength);
    saveEditorStateValue(state, "durBase", durBase);
    saveEditorStateValue(state, "durRndm", durRndm);
    saveEditorStateValue(state, "durationPatternMode", durationPatternMode);
    saveEditorStateValue(state, "durationAlternateSteps", durationAlternateSteps);
    saveEditorStateValue(state, "durationAlternateShift", durationAlternateShift);
    saveEditorStateValue(state, "durationChance", durationChance);
    saveEditorStateValue(state, "durationSeed", durationSeed);
    saveEditorStateValue(state, "durationRandomizeSeedEachCycle", durationRandomizeSeedEachCycle);
    saveEditorStateValue(state, "durationStepPattern", durationStepPattern);
    saveEditorStateValue(state, "eucDurLen", eucDurLen);
    saveEditorStateValue(state, "eucDurHits", eucDurHits);
    saveEditorStateValue(state, "eucDurOff", eucDurOff);
    saveEditorStateValue(state, "durEucStrength", durEucStrength);
    saveEditorStateValue(state, "durationRndPerStep", durationRndPerStep);
    saveEditorStateValue(state, "outputHistoryWindowMs", outputHistoryWindowMs);
    json["editorState"] = state;
}

void polyphonicArpeggiatorGUI::presetRecallBeforeSettingParameters(ofJson &json) {
    std::vector<std::string> publishedKeys;
    if(json.contains("publishedEditorParameters") && json["publishedEditorParameters"].is_array()) {
        try {
            publishedKeys = json["publishedEditorParameters"].get<std::vector<std::string>>();
        } catch(...) {
        }
    }
    syncPublishedEditorParameters(publishedKeys);
}

void polyphonicArpeggiatorGUI::presetRecallAfterSettingParameters(ofJson &json) {
    currentStep = json.value("currentStep", 0);
    highlightedStep = json.value("highlightedStep", currentStep);
    activeSnapshotSlot = json.value("activeSnapshotSlot", -1);

    if(json.contains("editorState") && json["editorState"].is_object()) {
        const ofJson &state = json["editorState"];
        editorWidth = state.value("editorWidth", editorWidth.get());
        editorHeight = state.value("editorHeight", editorHeight.get());
        loadEditorStateValue(state, "sourceMode", sourceMode);
        loadEditorStateValue(state, "sortPool", sortPool);
        loadEditorStateValue(state, "removeDuplicates", removeDuplicates);
        loadEditorStateValue(state, "sourceChangeMode", sourceChangeMode);
        loadEditorStateValue(state, "patternMode", patternMode);
        loadEditorStateValue(state, "patternSeed", patternSeed);
        loadEditorStateValue(state, "patternRandomizeSeedEachCycle", patternRandomizeSeedEachCycle);
        loadEditorStateValue(state, "modulo", modulo);
        if(state.contains("pulseMode")) {
            loadEditorStateValue(state, "pulseMode", pulseMode);
        } else {
            pulseMode = EuclideanPulse;
        }
        loadEditorStateValue(state, "pulseStepPattern", pulseStepPattern);
        loadEditorStateValue(state, "geigerSpeed", geigerSpeed);
        loadEditorStateValue(state, "geigerDensity", geigerDensity);
        loadEditorStateValue(state, "geigerPeriodicity", geigerPeriodicity);
        loadEditorStateValue(state, "geigerChaos", geigerChaos);
        loadEditorStateValue(state, "seqSize", seqSize);
        loadEditorStateValue(state, "idxPattern", idxPattern);
        loadEditorStateValue(state, "sourceStart", sourceStart);
        loadEditorStateValue(state, "sourceStride", sourceStride);
        loadEditorStateValue(state, "stepShift", stepShift);
        loadEditorStateValue(state, "rndShiftChance", rndShiftChance);
        loadEditorStateValue(state, "rndShiftRange", rndShiftRange);
        loadEditorStateValue(state, "rndShiftQuant", rndShiftQuant);
        loadEditorStateValue(state, "octave", octave);
        loadEditorStateValue(state, "octaveFold", octaveFold);
        loadEditorStateValue(state, "foldPoly", foldPoly);
        loadEditorStateValue(state, "pitchExpand", pitchExpand);
        loadEditorStateValue(state, "expandStep", expandStep);
        loadEditorStateValue(state, "transpose", transpose);
        loadEditorStateValue(state, "dynamicMode", dynamicMode);
        loadEditorStateValue(state, "accentOnsetMode", accentOnsetMode);
        loadEditorStateValue(state, "polyphony", polyphony);
        loadEditorStateValue(state, "polyInterval", polyInterval);
        loadEditorStateValue(state, "polyAccent", polyAccent);
        loadEditorStateValue(state, "addBass", addBass);
        loadEditorStateValue(state, "bassPatternMode", bassPatternMode);
        loadEditorStateValue(state, "bassAlternateSteps", bassAlternateSteps);
        loadEditorStateValue(state, "bassAlternateShift", bassAlternateShift);
        loadEditorStateValue(state, "bassEucLen", bassEucLen);
        loadEditorStateValue(state, "bassEucHits", bassEucHits);
        loadEditorStateValue(state, "bassEucOff", bassEucOff);
        loadEditorStateValue(state, "bassOctave", bassOctave);
        loadEditorStateValue(state, "bassProb", bassProb);
        loadEditorStateValue(state, "bassOnAccent", bassOnAccent);
        loadEditorStateValue(state, "bassOctRnd", bassOctRnd);
        loadEditorStateValue(state, "bassOctRndChance", bassOctRndChance);
        loadEditorStateValue(state, "bassOctRndSeed", bassOctRndSeed);
        loadEditorStateValue(state, "bassOctRndRandomizeSeedEachCycle", bassOctRndRandomizeSeedEachCycle);
        loadEditorStateValue(state, "bassPolyphony", bassPolyphony);
        loadEditorStateValue(state, "bassVelBase", bassVelBase);
        loadEditorStateValue(state, "bassVelRndm", bassVelRndm);
        loadEditorStateValue(state, "bassPitchModified", bassPitchModified);
        loadEditorStateValue(state, "skipSteps", skipSteps);
        loadEditorStateValue(state, "strum", strum);
        loadEditorStateValue(state, "strumRndm", strumRndm);
        loadEditorStateValue(state, "strumRndSeed", strumRndSeed);
        loadEditorStateValue(state, "strumRndRandomizeSeedEachCycle", strumRndRandomizeSeedEachCycle);
        loadEditorStateValue(state, "swing", swing);
        loadEditorStateValue(state, "strumDir", strumDir);
        loadEditorStateValue(state, "strumDirSeed", strumDirSeed);
        loadEditorStateValue(state, "strumDirRandomizeSeedEachCycle", strumDirRandomizeSeedEachCycle);
        loadEditorStateValue(state, "positionRndm", positionRndm);
        loadEditorStateValue(state, "positionRndSeed", positionRndSeed);
        loadEditorStateValue(state, "positionRndRandomizeSeedEachCycle", positionRndRandomizeSeedEachCycle);
        loadEditorStateValue(state, "octaveDev", octaveDev);
        loadEditorStateValue(state, "octaveDevRng", octaveDevRng);
        loadEditorStateValue(state, "idxDev", idxDev);
        loadEditorStateValue(state, "idxDevRng", idxDevRng);
        loadEditorStateValue(state, "pitchDev", pitchDev);
        loadEditorStateValue(state, "pitchDevRng", pitchDevRng);
        loadEditorStateValue(state, "eucLen", eucLen);
        loadEditorStateValue(state, "eucHits", eucHits);
        loadEditorStateValue(state, "eucOff", eucOff);
        loadEditorStateValue(state, "seqProb", seqProb);
        loadEditorStateValue(state, "seqProbCycles", seqProbCycles);
        loadEditorStateValue(state, "runGateBeats", runGateBeats);
        loadEditorStateValue(state, "runGateChance", runGateChance);
        loadEditorStateValue(state, "runGatePhase", runGatePhase);
        loadEditorStateValue(state, "stepChance", stepChance);
        loadEditorStateValue(state, "noteChance", noteChance);
        loadEditorStateValue(state, "velBase", velBase);
        loadEditorStateValue(state, "velRndm", velRndm);
        loadEditorStateValue(state, "velRndSeed", velRndSeed);
        loadEditorStateValue(state, "velRndRandomizeSeedEachCycle", velRndRandomizeSeedEachCycle);
        loadEditorStateValue(state, "velStepRndm", velStepRndm);
        loadEditorStateValue(state, "velStepRndSeed", velStepRndSeed);
        loadEditorStateValue(state, "velStepRndRandomizeSeedEachCycle", velStepRndRandomizeSeedEachCycle);
        loadEditorStateValue(state, "velocityCurve", velocityCurve);
        loadEditorStateValue(state, "velocitySlowRndm", velocitySlowRndm);
        loadEditorStateValue(state, "velocitySlowSpeed", velocitySlowSpeed);
        loadEditorStateValue(state, "velocitySlowSeed", velocitySlowSeed);
        if(state.contains("velocityLfoShape")) {
            loadEditorStateValue(state, "velocityLfoShape", velocityLfoShape);
        } else {
            velocityLfoShape = VelocityLfoSlowRandom;
        }
        if(state.contains("velocityLfoPhase")) {
            loadEditorStateValue(state, "velocityLfoPhase", velocityLfoPhase);
        } else {
            velocityLfoPhase = 0.0f;
        }
        if(state.contains("velocityLfoPulseWidth")) {
            loadEditorStateValue(state, "velocityLfoPulseWidth", velocityLfoPulseWidth);
        } else {
            velocityLfoPulseWidth = 0.5f;
        }
        loadEditorStateValue(state, "accentPatternMode", accentPatternMode);
        loadEditorStateValue(state, "accentAlternateSteps", accentAlternateSteps);
        loadEditorStateValue(state, "accentAlternateShift", accentAlternateShift);
        loadEditorStateValue(state, "accentChance", accentChance);
        loadEditorStateValue(state, "accentSeed", accentSeed);
        loadEditorStateValue(state, "accentRandomizeSeedEachCycle", accentRandomizeSeedEachCycle);
        loadEditorStateValue(state, "accentStepPattern", accentStepPattern);
        loadEditorStateValue(state, "eucAccLen", eucAccLen);
        loadEditorStateValue(state, "eucAccHits", eucAccHits);
        loadEditorStateValue(state, "eucAccOff", eucAccOff);
        loadEditorStateValue(state, "eucAccStrength", eucAccStrength);
        loadEditorStateValue(state, "durBase", durBase);
        loadEditorStateValue(state, "durRndm", durRndm);
        loadEditorStateValue(state, "durationPatternMode", durationPatternMode);
        loadEditorStateValue(state, "durationAlternateSteps", durationAlternateSteps);
        loadEditorStateValue(state, "durationAlternateShift", durationAlternateShift);
        loadEditorStateValue(state, "durationChance", durationChance);
        loadEditorStateValue(state, "durationSeed", durationSeed);
        loadEditorStateValue(state, "durationRandomizeSeedEachCycle", durationRandomizeSeedEachCycle);
        loadEditorStateValue(state, "durationStepPattern", durationStepPattern);
        loadEditorStateValue(state, "eucDurLen", eucDurLen);
        loadEditorStateValue(state, "eucDurHits", eucDurHits);
        loadEditorStateValue(state, "eucDurOff", eucDurOff);
        loadEditorStateValue(state, "durEucStrength", durEucStrength);
        loadEditorStateValue(state, "durationRndPerStep", durationRndPerStep);
        loadEditorStateValue(state, "outputHistoryWindowMs", outputHistoryWindowMs);
        if(strumRndSeed.get() == 0) {
            if(strumDirSeed.get() != 0) strumRndSeed = strumDirSeed.get();
            else if(positionRndSeed.get() != 0) strumRndSeed = positionRndSeed.get();
        }
        strumRndRandomizeSeedEachCycle = strumRndRandomizeSeedEachCycle.get()
                                      || strumDirRandomizeSeedEachCycle.get()
                                      || positionRndRandomizeSeedEachCycle.get();
        strumDirSeed = strumRndSeed.get();
        strumDirRandomizeSeedEachCycle = strumRndRandomizeSeedEachCycle.get();
        positionRndSeed = strumRndSeed.get();
        positionRndRandomizeSeedEachCycle = strumRndRandomizeSeedEachCycle.get();

        if(velRndSeed.get() == 0 && velStepRndSeed.get() != 0) {
            velRndSeed = velStepRndSeed.get();
        }
        velRndRandomizeSeedEachCycle = velRndRandomizeSeedEachCycle.get()
                                    || velStepRndRandomizeSeedEachCycle.get();
        velStepRndSeed = velRndSeed.get();
        velStepRndRandomizeSeedEachCycle = velRndRandomizeSeedEachCycle.get();
    }
    runGateWindowStateValid = false;

    if(durBase.get() > 4.0f || durRndm.get() > 4.0f || durEucStrength.get() > 4.0f) {
        durBase = convertLegacyDurationMsToUnits(durBase.get(), beatDiv.get());
        durRndm = convertLegacyDurationMsToUnits(durRndm.get(), beatDiv.get());
        durEucStrength = convertLegacyDurationMsToUnits(durEucStrength.get(), beatDiv.get());
    }
    if(strum.get() > 1.0f) {
        strum = convertLegacyPositionMsToUnits(strum.get(), beatDiv.get());
    }
    if(strumRndm.get() > 1.0f) {
        strumRndm = convertLegacyPositionMsToUnits(strumRndm.get(), beatDiv.get());
    }
    if(positionRndm.get() > 1.0f) {
        positionRndm = convertLegacyPositionMsToUnits(positionRndm.get(), beatDiv.get());
    }

    rebuildSourceMaterial();
    generateEuclideanPattern(euclideanPattern, eucLen.get(), eucHits.get(), eucOff.get());
    generateEuclideanPattern(euclideanAccents, eucAccLen.get(), eucAccHits.get(), eucAccOff.get());
    generateEuclideanPattern(euclideanDurations, eucDurLen.get(), eucDurHits.get(), eucDurOff.get());
    generateEuclideanPattern(euclideanBass, bassEucLen.get(), bassEucHits.get(), bassEucOff.get());
    resizeStateVectors(seqSize.get());
    syncUserPatternToSequenceSize();
    syncPulseStepPatternToSequenceSize();
    syncAccentStepPatternToSequenceSize();
    syncDurationStepPatternToSequenceSize();
    syncVelocityCurveShape();
    rebuildDeviations();
    rebuildPitchSequence();
    rebuildEuclideanOutputs();
    internalClockNeedsSync = true;
    currentCycleRandomStepShift = 0;
    sequenceCycleDecisionPending = true;
    skippedSequenceCyclesRemaining = 0;
    updateOutputs();
}

#endif
