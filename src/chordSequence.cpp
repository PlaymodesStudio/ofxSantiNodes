#include "chordSequence.h"
#include "chordCypherAliases.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <set>

namespace {
    constexpr int indexParameterMin = 0;
    constexpr int indexParameterMax = 32;
    constexpr int keyboardDisplayBaseNote = 60;
    const char *keyNames[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    const char *functionalGroupKeys[] = {"tonic", "subdominant", "dominant"};
    const char *functionalGroupLabels[] = {"Tonic", "Subdominant", "Dominant"};
    const char *voicingLabels[] = {"None", "Close", "Open", "Drop 2", "Drop 3", "Shell"};

    bool modeSupportsDiatonicDeviation(int mode) {
        return mode == chordSequenceEntry::Scale ||
               mode == chordSequenceEntry::Degree ||
               mode == chordSequenceEntry::Functional;
    }

    const ImVec4 snapshotsBg = ImVec4(0.12f, 0.17f, 0.24f, 0.94f);
    const ImVec4 snapshotsTitle = ImVec4(0.80f, 0.90f, 1.00f, 1.00f);
    const ImVec4 globalBg = ImVec4(0.20f, 0.18f, 0.11f, 0.94f);
    const ImVec4 globalTitle = ImVec4(1.00f, 0.92f, 0.66f, 1.00f);
    const ImVec4 cypherBg = ImVec4(0.25f, 0.15f, 0.10f, 0.94f);
    const ImVec4 cypherTitle = ImVec4(1.00f, 0.80f, 0.60f, 1.00f);
    const ImVec4 stepsBg = ImVec4(0.11f, 0.19f, 0.11f, 0.94f);
    const ImVec4 stepsTitle = ImVec4(0.70f, 1.00f, 0.72f, 1.00f);
    const ImVec4 outputsBg = ImVec4(0.08f, 0.18f, 0.20f, 0.94f);
    const ImVec4 outputsTitle = ImVec4(0.72f, 0.96f, 1.00f, 1.00f);

    struct ChordSequenceKeyGeometry {
        bool isBlack;
        float x;
        float w;
    };

    int wrapIndex(int value, int size) {
        if(size <= 0) return -1;
        int wrapped = value % size;
        if(wrapped < 0) wrapped += size;
        return wrapped;
    }

    bool isWhiteKey(int note) {
        int noteInOctave = note % 12;
        return noteInOctave == 0 || noteInOctave == 2 || noteInOctave == 4 ||
               noteInOctave == 5 || noteInOctave == 7 || noteInOctave == 9 ||
               noteInOctave == 11;
    }

    std::vector<ChordSequenceKeyGeometry> buildKeyboardGeometryForRange(int startNote, int endNote, float width) {
        std::vector<ChordSequenceKeyGeometry> geometry;
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
            ChordSequenceKeyGeometry key;
            key.isBlack = !isWhiteKey(note);

            if(!key.isBlack) {
                key.x = currentX;
                key.w = whiteKeyWidth;
                currentX += whiteKeyWidth;
            } else {
                key.x = currentX - (blackKeyWidth / 2.0f);
                key.w = blackKeyWidth;
            }

            geometry.push_back(key);
        }

        return geometry;
    }

    std::vector<int> toDisplayNotes(const std::vector<float> &values) {
        if(values.empty()) return {};

        std::vector<int> roundedValues;
        roundedValues.reserve(values.size());
        int minValue = std::numeric_limits<int>::max();
        int maxValue = std::numeric_limits<int>::min();
        for(float value : values) {
            int rounded = static_cast<int>(std::round(value));
            roundedValues.push_back(rounded);
            minValue = std::min(minValue, rounded);
            maxValue = std::max(maxValue, rounded);
        }

        int displayBase = keyboardDisplayBaseNote;
        if(displayBase + minValue < 0) {
            displayBase += -(displayBase + minValue);
        }
        if(displayBase + maxValue > 127) {
            displayBase -= (displayBase + maxValue - 127);
        }

        if(maxValue - minValue > 127) {
            int centerValue = static_cast<int>(std::round((minValue + maxValue) * 0.5f));
            displayBase = 60 - centerValue;
        }

        std::vector<int> notes;
        notes.reserve(values.size());
        for(int rounded : roundedValues) {
            notes.push_back(ofClamp(rounded + displayBase, 0, 127));
        }
        return notes;
    }

    std::vector<int> centerNotesToPitchClassDisplay(const std::vector<int> &notes) {
        std::vector<int> centeredNotes;
        centeredNotes.reserve(notes.size());
        constexpr int centeredDisplayBase = 60; // C4

        for(int note : notes) {
            int pitchClass = ((note % 12) + 12) % 12;
            centeredNotes.push_back(centeredDisplayBase + pitchClass);
        }

        return centeredNotes;
    }

    std::pair<int, int> computeKeyboardRange(const std::vector<int> &notes,
                                             int defaultLow,
                                             int defaultHigh,
                                             int minSemitones) {
        if(notes.empty()) {
            return {defaultLow, defaultHigh};
        }

        int minNote = *std::min_element(notes.begin(), notes.end());
        int maxNote = *std::max_element(notes.begin(), notes.end());
        int startNote = minNote - 2;
        int endNote = maxNote + 2;

        if(endNote - startNote + 1 < minSemitones) {
            int center = static_cast<int>(std::round((minNote + maxNote) * 0.5f));
            startNote = center - (minSemitones / 2);
            endNote = startNote + minSemitones - 1;
        }

        startNote = std::max(0, startNote);
        endNote = std::min(127, endNote);

        if(endNote - startNote + 1 < minSemitones) {
            startNote = std::max(0, endNote - minSemitones + 1);
            endNote = std::min(127, startNote + minSemitones - 1);
        }

        startNote = (startNote / 12) * 12;
        endNote = std::min(127, ((endNote / 12) * 12) + 11);
        if(endNote <= startNote) {
            endNote = std::min(127, startNote + std::max(11, minSemitones - 1));
        }

        return {startNote, endNote};
    }

    void drawKeyboardDisplay(const char *id,
                             const std::vector<float> &values,
                             float width,
                             float height,
                             bool active,
                             bool compact = false,
                             bool centerPitchClasses = false) {
        std::vector<int> resizedNotes = toDisplayNotes(values);
        if(centerPitchClasses) {
            resizedNotes = centerNotesToPitchClassDisplay(resizedNotes);
        }
        std::map<int, int> noteCounts;
        for(int note : resizedNotes) {
            noteCounts[note]++;
        }

        auto range = computeKeyboardRange(resizedNotes, compact ? 48 : 36, compact ? 72 : 84, compact ? 18 : 24);
        std::vector<ChordSequenceKeyGeometry> geometry = buildKeyboardGeometryForRange(range.first, range.second, width);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImGui::InvisibleButton(id, ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

        ImU32 frameBg = active ? IM_COL32(28, 42, 28, 255) : IM_COL32(24, 24, 24, 255);
        drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), frameBg, 4.0f);
        drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(70, 70, 70, 255), 4.0f);

        if(geometry.empty()) return;

        float blackKeyHeight = height * 0.62f;
        ImU32 whiteKeyColor = IM_COL32(242, 242, 242, 255);
        ImU32 blackKeyColor = IM_COL32(15, 15, 15, 255);
        ImU32 highlightColor = active ? IM_COL32(90, 235, 120, 180) : IM_COL32(80, 170, 235, 170);

        for(size_t i = 0; i < geometry.size(); i++) {
            if(geometry[i].isBlack) continue;

            int note = range.first + static_cast<int>(i);
            ImVec2 keyPos(pos.x + geometry[i].x, pos.y);
            ImVec2 keyEnd(keyPos.x + geometry[i].w, pos.y + height);
            drawList->AddRectFilled(keyPos, keyEnd, whiteKeyColor);
            drawList->AddRect(keyPos, keyEnd, IM_COL32(110, 110, 110, 255));

            auto countIt = noteCounts.find(note);
            if(countIt != noteCounts.end()) {
                drawList->AddRectFilled(keyPos, keyEnd, highlightColor);
                if(countIt->second > 1) {
                    std::string countLabel = ofToString(countIt->second);
                    drawList->AddText(ImVec2(keyPos.x + 4.0f, keyPos.y + height - 16.0f),
                                      IM_COL32(20, 20, 20, 220),
                                      countLabel.c_str());
                }
            }
        }

        for(size_t i = 0; i < geometry.size(); i++) {
            if(!geometry[i].isBlack) continue;

            int note = range.first + static_cast<int>(i);
            ImVec2 keyPos(pos.x + geometry[i].x, pos.y);
            ImVec2 keyEnd(keyPos.x + geometry[i].w, pos.y + blackKeyHeight);
            drawList->AddRectFilled(keyPos, keyEnd, blackKeyColor);
            drawList->AddRect(keyPos, keyEnd, IM_COL32(70, 70, 70, 255));

            auto countIt = noteCounts.find(note);
            if(countIt != noteCounts.end()) {
                drawList->AddRectFilled(keyPos, keyEnd, highlightColor);
                if(countIt->second > 1) {
                    std::string countLabel = ofToString(countIt->second);
                    drawList->AddText(ImVec2(keyPos.x + 2.0f, keyPos.y + blackKeyHeight - 14.0f),
                                      IM_COL32(255, 255, 255, 220),
                                      countLabel.c_str());
                }
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
                             ImGuiWindowFlags flags = 0) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        float collapsedHeight = 30.0f;
        ImGui::BeginChild(id, ImVec2(size.x, expanded ? size.y : collapsedHeight), true, flags);
        ImGui::PopStyleColor();
        std::string arrowId = std::string("##") + id + "Arrow";
        if(ImGui::ArrowButton(arrowId.c_str(), expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
            expanded = !expanded;
        }
        ImGui::SameLine();
        ImGui::TextColored(titleColor, "%s", title);
        if(expanded) {
            ImGui::Separator();
        }
    }

    float getSectionHeaderHeight() {
        return ImGui::GetFrameHeightWithSpacing() + 10.0f;
    }

    float getChordSequenceEntryCardHeight(int mode, bool markovEnabled) {
        float rowHeight = ImGui::GetFrameHeightWithSpacing();
        float textHeight = ImGui::GetTextLineHeightWithSpacing();
        float height = 24.0f; // header
        height += rowHeight; // type

        if(mode == chordSequenceEntry::Cypher) {
            height += rowHeight; // chord text
        } else if(mode == chordSequenceEntry::Degree) {
            height += rowHeight * 3.0f; // degree, chord size, step interval
        } else if(mode == chordSequenceEntry::Functional) {
            height += rowHeight * 5.0f; // function, variant, degree, chord size, step interval
        } else {
            height += rowHeight; // selection
        }

        if(modeSupportsDiatonicDeviation(mode)) {
            height += rowHeight * 2.0f; // diatonic deviation
        }
        height += rowHeight; // beats
        if(markovEnabled) {
            height += 57.0f; // inline multislider area
            height += rowHeight; // helper buttons
        }
        height += rowHeight * 2.0f; // transpose, inversion
        height += 62.0f; // keyboard
        height += std::max(18.0f, textHeight); // preview text
        height += 8.0f; // bottom padding

        return height;
    }

    float getOutputCardHeight(const chordSequenceOutputConfig &config) {
        float rowHeight = ImGui::GetFrameHeightWithSpacing();
        float textHeight = ImGui::GetTextLineHeightWithSpacing();
        float height = 24.0f; // header
        float controlRows = 12.0f; // always-visible rows
        if(!config.scaleOnly) {
            controlRows += 4.0f; // root/key, inversion, voicing, spread
            if(config.addBass) controlRows += 1.0f; // bass octave
        }
        height += rowHeight * controlRows;
        height += 64.0f; // keyboard
        height += std::max(20.0f, textHeight * 1.1f); // value preview
        height += 10.0f; // bottom padding
        return height;
    }

    float getSnapshotsSectionHeight(float availableWidth, bool hasActiveSnapshot) {
        float contentWidth = std::max(1.0f, availableWidth - 18.0f);
        float textHeight = ImGui::GetTextLineHeightWithSpacing();
        float rowHeight = ImGui::GetFrameHeightWithSpacing();
        float slotSize = 22.0f;
        float slotGap = 4.0f;
        int columns = std::max(1, static_cast<int>((contentWidth + slotGap) / (slotSize + slotGap)));
        int rows = (16 + columns - 1) / columns;

        float height = getSectionHeaderHeight();
        height += textHeight;
        height += rows * slotSize;
        height += std::max(0, rows - 1) * slotGap;
        height += 8.0f;
        height += rowHeight; // recall combo
        if(hasActiveSnapshot) {
            height += rowHeight; // name field
        }
        height += 8.0f;
        return height;
    }

    float getGlobalSectionHeight() {
        float rowHeight = ImGui::GetFrameHeightWithSpacing();
        float height = getSectionHeaderHeight();
        height += rowHeight * 6.0f;
        height += 8.0f;
        return height;
    }

    float getCypherSectionHeight(bool hasImportedProgressions, bool hasJazzStandards) {
        float rowHeight = ImGui::GetFrameHeightWithSpacing();
        float height = getSectionHeaderHeight();
        height += rowHeight; // chord list
        height += rowHeight * 2.0f; // apply + append buttons
        if(hasImportedProgressions) {
            height += rowHeight; // progression combo + load button row
        }
        if(hasJazzStandards) {
            height += rowHeight; // jazz combo + load button row
        }
        height += 8.0f;
        return height;
    }
}

ofJson chordSequenceEntry::toJson() const {
    return {
        {"mode", mode},
        {"itemIndex", itemIndex},
        {"itemName", itemName},
        {"functionalGroup", functionalGroup},
        {"functionalVariantIndex", functionalVariantIndex},
        {"functionalVariantLabel", functionalVariantLabel},
        {"degree", degree},
        {"chordSize", chordSize},
        {"stepInterval", stepInterval},
        {"diatonicDeviationProbability", diatonicDeviationProbability},
        {"diatonicDeviationRange", diatonicDeviationRange},
        {"beatDuration", beatDuration},
        {"markovWeights", markovWeights},
        {"transpose", transpose},
        {"inversion", inversion}
    };
}

chordSequenceEntry chordSequenceEntry::fromJson(const ofJson &json) {
    chordSequenceEntry entry;
    entry.mode = json.value("mode", Chord);
    entry.itemIndex = json.value("itemIndex", 0);
    entry.itemName = json.value("itemName", std::string());
    entry.functionalGroup = ofClamp(json.value("functionalGroup", 0), 0, 2);
    entry.functionalVariantIndex = std::max(0, json.value("functionalVariantIndex", 0));
    entry.functionalVariantLabel = json.value("functionalVariantLabel", std::string());
    entry.degree = std::max(1, json.value("degree", 1));
    entry.chordSize = std::max(1, json.value("chordSize", 4));
    entry.stepInterval = std::max(1, json.value("stepInterval", 2));
    entry.diatonicDeviationProbability = ofClamp(json.value("diatonicDeviationProbability", 0.0f), 0.0f, 100.0f);
    entry.diatonicDeviationRange = std::max(0, json.value("diatonicDeviationRange", 0));
    entry.beatDuration = std::max(0.001f, json.value("beatDuration", 1.0f));
    if(json.contains("markovWeights") && json["markovWeights"].is_array()) {
        entry.markovWeights.clear();
        for(const auto &weight : json["markovWeights"]) {
            entry.markovWeights.push_back(std::max(0.0f, weight.get<float>()));
        }
    }
    entry.transpose = json.value("transpose", 0);
    entry.inversion = json.value("inversion", 0);
    return entry;
}

ofJson chordSequenceOutputConfig::toJson() const {
    return {
        {"octave", octave},
        {"transpose", transpose},
        {"pitchBend", pitchBend},
        {"perNoteDetune", perNoteDetune},
        {"octaveRandomProbability", octaveRandomProbability},
        {"octaveRandomRange", octaveRandomRange},
        {"chromaticDeviationProbability", chromaticDeviationProbability},
        {"chromaticDeviationRange", chromaticDeviationRange},
        {"scaleOnly", scaleOnly},
        {"addBass", addBass},
        {"bassOct", bassOct},
        {"inversion", inversion},
        {"voicingMode", voicingMode},
        {"voicingSpread", voicingSpread},
        {"rootOnly", rootOnly},
        {"keyOnly", keyOnly},
        {"fold12", fold12},
        {"glideMs", glideMs},
        {"outputSize", outputSize},
        {"expandOutput", expandOutput},
        {"sortOutput", sortOutput}
    };
}

chordSequenceOutputConfig chordSequenceOutputConfig::fromJson(const ofJson &json) {
    chordSequenceOutputConfig config;
    config.octave = json.value("octave", 0);
    config.transpose = json.value("transpose", 0);
    config.pitchBend = json.value("pitchBend", 0.0f);
    config.perNoteDetune = std::max(0.0f, json.value("perNoteDetune", 0.0f));
    config.octaveRandomProbability = ofClamp(json.value("octaveRandomProbability", 0.0f), 0.0f, 100.0f);
    config.octaveRandomRange = std::max(0, json.value("octaveRandomRange", 0));
    config.chromaticDeviationProbability = ofClamp(json.value("chromaticDeviationProbability", 0.0f), 0.0f, 100.0f);
    config.chromaticDeviationRange = std::max(0, json.value("chromaticDeviationRange", 0));
    config.scaleOnly = json.value("scaleOnly", false);
    config.addBass = json.value("addBass", false);
    config.bassOct = json.value("bassOct", -2);
    config.inversion = json.value("inversion", 0);
    config.voicingMode = ofClamp(json.value("voicingMode", chordSequenceOutputConfig::None),
                                 chordSequenceOutputConfig::None,
                                 chordSequenceOutputConfig::Shell);
    config.voicingSpread = std::max(0.0f, json.value("voicingSpread", 0.0f));
    config.rootOnly = json.value("rootOnly", false);
    config.keyOnly = json.value("keyOnly", false);
    config.fold12 = json.value("fold12", false);
    config.glideMs = json.value("glideMs", 0.0f);
    config.outputSize = std::max(1, json.value("outputSize", 4));
    config.expandOutput = json.value("expandOutput", false);
    config.sortOutput = json.value("sortOutput", false);
    return config;
}

ofJson chordSequenceSnapshot::toJson() const {
    ofJson json;
    json["name"] = name;
    json["numOutputs"] = numOutputs;
    json["globalKey"] = globalKey;
    json["globalScaleIndex"] = globalScaleIndex;
    json["globalScaleName"] = globalScaleName;
    json["globalTranspose"] = globalTranspose;
    json["globalInvert"] = globalInvert;
    json["globalPitchBend"] = globalPitchBend;
    json["internalTimingEnabled"] = internalTimingEnabled;
    json["markovEnabled"] = markovEnabled;
    json["internalActiveStep"] = internalActiveStep;
    json["hasData"] = hasData;
    json["entries"] = ofJson::array();
    json["outputs"] = ofJson::array();

    for(const auto &entry : entries) {
        json["entries"].push_back(entry.toJson());
    }

    for(const auto &output : outputs) {
        json["outputs"].push_back(output.toJson());
    }

    return json;
}

chordSequenceSnapshot chordSequenceSnapshot::fromJson(const ofJson &json) {
    chordSequenceSnapshot snapshot;
    snapshot.name = json.value("name", std::string());
    snapshot.numOutputs = std::max(1, json.value("numOutputs", 1));
    snapshot.globalKey = ofClamp(json.value("globalKey", 0), 0, 11);
    snapshot.globalScaleIndex = std::max(0, json.value("globalScaleIndex", 0));
    snapshot.globalScaleName = json.value("globalScaleName", std::string());
    snapshot.globalTranspose = json.value("globalTranspose", 0);
    snapshot.globalInvert = json.value("globalInvert", 0);
    snapshot.globalPitchBend = json.value("globalPitchBend", json.value("pitchBend", 0.0f));
    snapshot.internalTimingEnabled = json.value("internalTimingEnabled", false);
    snapshot.markovEnabled = json.value("markovEnabled", false);
    snapshot.internalActiveStep = std::max(0, json.value("internalActiveStep", 0));
    snapshot.hasData = json.value("hasData", false);

    if(json.contains("entries") && json["entries"].is_array()) {
        for(const auto &entryJson : json["entries"]) {
            snapshot.entries.push_back(chordSequenceEntry::fromJson(entryJson));
        }
    }

    if(json.contains("outputs") && json["outputs"].is_array()) {
        for(const auto &outputJson : json["outputs"]) {
            snapshot.outputs.push_back(chordSequenceOutputConfig::fromJson(outputJson));
        }
    } else {
        chordSequenceOutputConfig fallback;
        fallback.outputSize = std::max(1, json.value("outputSize", 4));
        fallback.expandOutput = json.value("expandOutput", false);
        fallback.sortOutput = json.value("sortOutput", false);
        fallback.glideMs = json.value("glideMs", 0.0f);
        snapshot.outputs.push_back(fallback);
    }

    snapshot.numOutputs = std::max(snapshot.numOutputs, static_cast<int>(snapshot.outputs.size()));
    snapshot.hasData = snapshot.hasData || !snapshot.entries.empty();
    return snapshot;
}

chordSequence::chordSequence() : ofxOceanodeNodeModel("Chord Sequence") {
    description = "Builds chord and scale progressions with cypher import and per-output shaping.";
    randomEngine.seed(std::random_device{}());
}

void chordSequence::setup() {
    addParameter(indexInput.set("Index", 0, indexParameterMin, indexParameterMax));
    addParameter(numChordsParameter.set("NumChords", DefaultSequenceSize, 1, 32));
    addParameter(transposeParameter.set("Transpose", 0, -48, 48));
    addParameter(pitchBendParameter.set("Pitchbend", 0.0f, -24.0f, 24.0f));
    addParameter(inversionParameter.set("Inversion", 0, -16, 16));
    addParameter(resetSequenceParameter.set("Reset"));
    addParameter(showEditor.set("Show", false));

    addInspectorParameter(editorWidth.set("Editor Width", 980.0f, 560.0f, 1800.0f));
    addInspectorParameter(editorHeight.set("Editor Height", 980.0f, 420.0f, 1800.0f));

    loadLibraries();
    loadImportSources();
    initializeDefaultProgression();
    ensureOutputCount(numOutputs);
    syncNodeGuiParametersFromState();
    loadAllSnapshotsFromDisk();

    listeners.push(indexInput.newListener([this](int &) {
        refreshAllOutputs();
    }));
    listeners.push(numChordsParameter.newListener([this](int &value) {
        resizeProgression(ofClamp(value, numChordsParameter.getMin(), numChordsParameter.getMax()));
    }));
    listeners.push(transposeParameter.newListener([this](int &value) {
        globalTranspose = value;
        refreshAllOutputs(true);
    }));
    listeners.push(pitchBendParameter.newListener([this](float &value) {
        globalPitchBend = value;
        refreshAllOutputs(true);
    }));
    listeners.push(inversionParameter.newListener([this](int &value) {
        globalInvert = value;
        refreshAllOutputs(true);
    }));
    listeners.push(resetSequenceParameter.newListener([this]() {
        resetInternalSequence(true);
    }));

    refreshAllOutputs(true);
}

void chordSequence::setBpm(float bpm) {
    currentBPM = std::max(1.0f, bpm);
    if(internalTimingEnabled) {
        resetInternalSequence(true);
    }
}

void chordSequence::loadBeforeConnections(ofJson &json) {
    int desiredOutputs = 1;
    if(json.contains("state")) {
        const auto &state = json["state"];
        if(state.contains("outputs") && state["outputs"].is_array()) {
            desiredOutputs = std::max(1, static_cast<int>(state["outputs"].size()));
        } else {
            desiredOutputs = std::max(1, state.value("numOutputs", 1));
        }
    }
    ensureOutputCount(desiredOutputs);
}

void chordSequence::update(ofEventArgs &) {
    if(internalTimingEnabled && !progression.empty()) {
        if(nextInternalStepTimeMs == 0) {
            resetInternalSequence(true);
        } else {
            uint64_t now = ofGetElapsedTimeMillis();
            int safety = 0;
            while(now >= nextInternalStepTimeMs && safety < 128) {
                advanceInternalSequence();
                now = ofGetElapsedTimeMillis();
                safety++;
            }
        }
    }

    for(int i = 0; i < static_cast<int>(outputs.size()); i++) {
        if(!outputIsGliding[i]) continue;

        float progress = getGlideProgress(i);
        outputs[i].set(getInterpolatedOutput(i, progress));

        if(progress >= 1.0f) {
            outputIsGliding[i] = false;
            currentOutputs[i] = targetOutputs[i];
            outputs[i].set(currentOutputs[i]);
        }
    }
}

void chordSequence::draw(ofEventArgs &) {
    if(!showEditor.get()) return;

    ImGui::SetNextWindowSize(ImVec2(editorWidth.get(), editorHeight.get()), ImGuiCond_FirstUseEver);

    std::string windowName = "Chord Sequence##" + ofToString(getNumIdentifier());
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

void chordSequence::presetSave(ofJson &json) {
    json["state"] = serializeCurrentState();
    json["activeSnapshotSlot"] = activeSnapshotSlot;
}

void chordSequence::presetRecallAfterSettingParameters(ofJson &json) {
    if(json.contains("state")) {
        deserializeState(json["state"], true);
    } else {
        sanitizeProgression();
        refreshAllOutputs(true);
    }

    activeSnapshotSlot = json.value("activeSnapshotSlot", -1);
}

void chordSequence::presetHasLoaded() {
    refreshAllOutputs(true);
}

void chordSequence::loadLibraries() {
    chordLibrary = parsePitchClassFile(resolvePitchClassFile("chords.txt"));
    scaleLibrary = parsePitchClassFile(resolvePitchClassFile("scales.txt"));
    loadCypherAliases();
    loadFunctionalHarmony();

    if(chordLibrary.empty()) {
        chordLibrary.push_back({"Unavailable", {0.0f}});
    }

    if(scaleLibrary.empty()) {
        scaleLibrary.push_back({"Unavailable", {0.0f}});
    }

    defaultChordIndex = std::max(0, findItemIndexByName(chordLibrary, "M"));
    defaultScaleIndex = std::max(0, findItemIndexByName(scaleLibrary, "major"));
    if(globalScaleName.empty() && globalScaleIndex == 0 && defaultScaleIndex >= 0 && defaultScaleIndex < static_cast<int>(scaleLibrary.size())) {
        globalScaleIndex = defaultScaleIndex;
        globalScaleName = scaleLibrary[defaultScaleIndex].name;
    }
    sanitizeGlobalScaleSelection();
}

void chordSequence::loadCypherAliases() {
    chordAliases.clear();
    chordCypherAliases::mergeAliasesFromFile(chordAliases, "chordSequence");
}

void chordSequence::loadFunctionalHarmony() {
    functionalHarmonyLibrary.clear();

    ofFile file(resolvePitchClassFile("functional_harmony.json"));
    if(!file.exists()) {
        ofLogWarning("chordSequence") << "Functional harmony file not found: " << file.getAbsolutePath();
        return;
    }

    ofJson json = ofLoadJson(file.getAbsolutePath());
    if(!json.is_object()) return;

    for(auto &[scaleName, scaleValue] : json.items()) {
        if(!scaleValue.is_object()) continue;

        std::array<std::vector<chordSequenceFunctionalVariant>, 3> groups;
        for(int groupIndex = 0; groupIndex < 3; groupIndex++) {
            const char *groupKey = functionalGroupKeys[groupIndex];
            if(!scaleValue.contains(groupKey) || !scaleValue[groupKey].is_array()) continue;

            for(const auto &variantJson : scaleValue[groupKey]) {
                if(!variantJson.is_object()) continue;
                chordSequenceFunctionalVariant variant;
                variant.label = variantJson.value("label", std::string());
                variant.degree = std::max(1, variantJson.value("degree", 1));
                if(!variant.label.empty()) {
                    groups[groupIndex].push_back(variant);
                }
            }
        }

        functionalHarmonyLibrary[scaleName] = groups;
    }
}

std::string chordSequence::resolvePitchClassFile(const std::string &fileName) const {
    return ofToDataPath("Supercollider/Pitchclass/" + fileName, true);
}

std::vector<chordSequenceLibraryItem> chordSequence::parsePitchClassFile(const std::string &path) const {
    std::vector<chordSequenceLibraryItem> result;
    ofFile file(path);
    if(!file.exists()) {
        ofLogWarning("chordSequence") << "Pitchclass file not found: " << path;
        return result;
    }

    ofBuffer buffer = file.readToBuffer();
    for(const auto &rawLine : buffer.getLines()) {
        std::string line = ofTrim(rawLine);
        if(line.empty()) continue;

        auto beforeSemicolon = ofSplitString(line, ";", true, true);
        if(beforeSemicolon.empty()) continue;

        auto commaSplit = ofSplitString(beforeSemicolon[0], ",", true, true);
        if(commaSplit.size() < 2) continue;

        std::string body = ofTrim(commaSplit[1]);
        auto tokens = ofSplitString(body, " ", true, true);
        if(tokens.empty()) continue;

        chordSequenceLibraryItem item;
        item.name = tokens[0];
        for(size_t i = 1; i < tokens.size(); i++) {
            item.values.push_back(ofToFloat(tokens[i]));
        }
        result.push_back(item);
    }

    return result;
}

void chordSequence::loadImportSources() {
    importedProgressionDatabase = ofJson::object();
    importedProgressionDatabase["progressions"] = ofJson::object();
    jazzStandardsDatabase = ofJson();
    jazzStandardNames.clear();

    std::string progressionPath = getChordProgressionsFilePath();
    ofFile progressionFile(progressionPath);
    if(progressionFile.exists()) {
        importedProgressionDatabase = ofLoadJson(progressionPath);
        if(!importedProgressionDatabase.contains("progressions") || !importedProgressionDatabase["progressions"].is_object()) {
            importedProgressionDatabase["progressions"] = ofJson::object();
        }
    }
    refreshImportedProgressionNames();

    std::string jazzPath = ofToDataPath("Supercollider/Pitchclass/JazzStandards.json");
    ofFile jazzFile(jazzPath);
    if(jazzFile.exists()) {
        jazzStandardsDatabase = ofLoadJson(jazzPath);
        if(jazzStandardsDatabase.is_array()) {
            for(const auto &song : jazzStandardsDatabase) {
                jazzStandardNames.push_back(song.value("Title", "Untitled"));
            }
        }
    }
}

void chordSequence::reloadLibraries() {
    loadLibraries();
    sanitizeProgression();
    refreshAllOutputs(true);
}

void chordSequence::ensureOutputCount(int newCount) {
    newCount = ofClamp(newCount, 1, MaxOutputs);
    int oldCount = static_cast<int>(outputs.size());
    if(newCount == oldCount) {
        numOutputs = newCount;
        return;
    }

    if(newCount < oldCount) {
        for(int i = oldCount - 1; i >= newCount; i--) {
            removeParameter(outputName(i));
        }
        outputs.resize(newCount);
        outputConfigs.resize(newCount);
        currentOutputs.resize(newCount);
        targetOutputs.resize(newCount);
        glideStartOutputs.resize(newCount);
        outputIsGliding.resize(newCount);
        outputGlideStartTimeMs.resize(newCount);
    } else {
        outputs.resize(newCount);
        outputConfigs.resize(newCount);
        currentOutputs.resize(newCount, std::vector<float>{0.0f});
        targetOutputs.resize(newCount, std::vector<float>{0.0f});
        glideStartOutputs.resize(newCount, std::vector<float>{0.0f});
        outputIsGliding.resize(newCount, false);
        outputGlideStartTimeMs.resize(newCount, 0);

        for(int i = oldCount; i < newCount; i++) {
            outputs[i].set(outputName(i), std::vector<float>{0.0f}, std::vector<float>{-FLT_MAX}, std::vector<float>{FLT_MAX});
            outputs[i].setSerializable(false);
            addOutputParameter(outputs[i]);
        }
    }

    numOutputs = newCount;
    rebuildOutputSizeParameters();
    syncNodeGuiParametersFromState();
}

std::string chordSequence::outputName(int index) const {
    return "Output" + ofToString(index + 1);
}

std::string chordSequence::outputSizeParameterName(int index) const {
    return "out" + ofToString(index + 1) + "size";
}

void chordSequence::rebuildOutputSizeParameters() {
    int desiredCount = static_cast<int>(outputs.size());
    int oldCount = static_cast<int>(outputSizeParameters.size());

    if(desiredCount < oldCount) {
        for(int i = oldCount - 1; i >= desiredCount; i--) {
            if(i < static_cast<int>(outputSizeParameterListeners.size())) {
                outputSizeParameterListeners[i].reset();
            }
            removeParameter(outputSizeParameterName(i));
        }
        outputSizeParameters.resize(desiredCount);
        outputSizeParameterListeners.resize(desiredCount);
    } else if(desiredCount > oldCount) {
        outputSizeParameters.resize(desiredCount);
        outputSizeParameterListeners.resize(desiredCount);
    }

    for(int i = 0; i < desiredCount; i++) {
        int initialSize = (i < static_cast<int>(outputConfigs.size())) ? outputConfigs[i].outputSize : 4;

        if(!outputSizeParameters[i]) {
            auto parameter = std::make_shared<ofParameter<int>>();
            parameter->set(outputSizeParameterName(i), initialSize, 1, 32);
            outputSizeParameters[i] = parameter;
            addParameter(*parameter);
            outputSizeParameterListeners[i] = std::make_unique<ofEventListener>(
                parameter->newListener([this, i](int &value) {
                    if(i < static_cast<int>(outputConfigs.size())) {
                        outputConfigs[i].outputSize = std::max(1, value);
                        refreshAllOutputs(true);
                    }
                })
            );
        } else if(outputSizeParameters[i]->get() != initialSize) {
            outputSizeParameters[i]->set(initialSize);
        }

        if(i >= oldCount && !outputSizeParameterListeners[i]) {
            outputSizeParameterListeners[i] = std::make_unique<ofEventListener>(
                outputSizeParameters[i]->newListener([this, i](int &value) {
                    if(i < static_cast<int>(outputConfigs.size())) {
                        outputConfigs[i].outputSize = std::max(1, value);
                        refreshAllOutputs(true);
                    }
                })
            );
        }
    }
}

void chordSequence::syncNodeGuiParametersFromState() {
    int progressionSize = std::max(1, static_cast<int>(progression.size()));
    if(numChordsParameter.get() != progressionSize) numChordsParameter = progressionSize;
    if(transposeParameter.get() != globalTranspose) transposeParameter = globalTranspose;
    if(std::abs(pitchBendParameter.get() - globalPitchBend) > 0.0001f) pitchBendParameter = globalPitchBend;
    if(inversionParameter.get() != globalInvert) inversionParameter = globalInvert;

    for(int i = 0; i < static_cast<int>(outputSizeParameters.size()) && i < static_cast<int>(outputConfigs.size()); i++) {
        if(outputSizeParameters[i] && outputSizeParameters[i]->get() != outputConfigs[i].outputSize) {
            outputSizeParameters[i]->set(outputConfigs[i].outputSize);
        }
    }
}

void chordSequence::initializeDefaultProgression() {
    progression.assign(DefaultSequenceSize, chordSequenceEntry());
    for(auto &entry : progression) {
        entry.mode = chordSequenceEntry::Chord;
        entry.itemIndex = defaultChordIndex;
        entry.itemName = chordLibrary[defaultChordIndex].name;
        entry.functionalGroup = 0;
        entry.functionalVariantIndex = 0;
        entry.functionalVariantLabel = "I";
        entry.degree = 1;
        entry.chordSize = 4;
        entry.stepInterval = 2;
        entry.transpose = 0;
        entry.inversion = 0;
    }
}

void chordSequence::resizeProgression(int newSize) {
    newSize = std::max(1, newSize);
    int oldSize = static_cast<int>(progression.size());
    progression.resize(newSize);

    for(int i = oldSize; i < newSize; i++) {
        progression[i].mode = chordSequenceEntry::Chord;
        progression[i].itemIndex = defaultChordIndex;
        progression[i].itemName = chordLibrary[defaultChordIndex].name;
        progression[i].functionalGroup = 0;
        progression[i].functionalVariantIndex = 0;
        progression[i].functionalVariantLabel = "I";
        progression[i].degree = 1;
        progression[i].chordSize = 4;
        progression[i].stepInterval = 2;
        progression[i].transpose = 0;
        progression[i].inversion = 0;
    }

    sanitizeProgression();
    if(numChordsParameter.get() != newSize) numChordsParameter = newSize;
    if(internalTimingEnabled) {
        internalActiveStep = ofClamp(internalActiveStep, 0, std::max(0, newSize - 1));
        nextInternalStepTimeMs = ofGetElapsedTimeMillis() + static_cast<uint64_t>(std::max(1.0f, getStepDurationMs(internalActiveStep)));
    }
    refreshAllOutputs(true);
}

void chordSequence::sanitizeProgression() {
    if(progression.empty()) {
        initializeDefaultProgression();
    }

    sanitizeGlobalScaleSelection();

    for(auto &entry : progression) {
        sanitizeEntry(entry);
    }
    sanitizeMarkovRows();
    if(!progression.empty()) {
        internalActiveStep = ofClamp(internalActiveStep, 0, static_cast<int>(progression.size()) - 1);
    } else {
        internalActiveStep = 0;
    }
}

void chordSequence::sanitizeEntry(chordSequenceEntry &entry) {
    entry.degree = std::max(1, entry.degree);
    entry.chordSize = std::max(1, entry.chordSize);
    entry.stepInterval = std::max(1, entry.stepInterval);
    entry.diatonicDeviationProbability = ofClamp(entry.diatonicDeviationProbability, 0.0f, 100.0f);
    entry.diatonicDeviationRange = std::max(0, entry.diatonicDeviationRange);
    entry.beatDuration = std::max(0.001f, entry.beatDuration);
    for(auto &weight : entry.markovWeights) {
        weight = std::max(0.0f, weight);
    }

    if(entry.mode == chordSequenceEntry::Cypher) {
        if(entry.itemName.empty()) entry.itemName = "C";
        return;
    }

    if(entry.mode == chordSequenceEntry::Functional) {
        entry.functionalGroup = ofClamp(entry.functionalGroup, 0, 2);
        applyFunctionalVariantToEntry(entry);
        entry.itemIndex = 0;
        entry.itemName = entry.functionalVariantLabel.empty()
                             ? std::string(functionalGroupLabels[entry.functionalGroup])
                             : entry.functionalVariantLabel;
        return;
    }

    if(entry.mode == chordSequenceEntry::Degree) {
        entry.itemIndex = 0;
        entry.itemName = "Degree " + ofToString(entry.degree);
        return;
    }

    entry.mode = (entry.mode == chordSequenceEntry::Scale) ? chordSequenceEntry::Scale : chordSequenceEntry::Chord;

    const auto &library = getLibraryForMode(entry.mode);
    if(library.empty()) {
        entry.itemIndex = 0;
        entry.itemName.clear();
        return;
    }

    int nameIndex = findItemIndexByName(library, entry.itemName);
    if(nameIndex >= 0) {
        entry.itemIndex = nameIndex;
    } else {
        entry.itemIndex = ofClamp(entry.itemIndex, 0, static_cast<int>(library.size()) - 1);
    }

    entry.itemName = library[entry.itemIndex].name;
}

void chordSequence::sanitizeMarkovRows() {
    int sequenceSize = static_cast<int>(progression.size());
    for(int i = 0; i < sequenceSize; i++) {
        std::vector<float> oldWeights = progression[i].markovWeights;
        progression[i].markovWeights.assign(sequenceSize, 0.0f);
        for(int j = 0; j < std::min(sequenceSize, static_cast<int>(oldWeights.size())); j++) {
            progression[i].markovWeights[j] = std::max(0.0f, oldWeights[j]);
        }

        bool hasPositiveWeight = false;
        for(float weight : progression[i].markovWeights) {
            if(weight > 0.0001f) {
                hasPositiveWeight = true;
                break;
            }
        }

        if(!hasPositiveWeight && sequenceSize > 0) {
            progression[i].markovWeights[(i + 1) % sequenceSize] = 1.0f;
        }
    }
}

void chordSequence::sanitizeGlobalScaleSelection() {
    if(scaleLibrary.empty()) {
        globalScaleIndex = 0;
        globalScaleName.clear();
        return;
    }

    if(globalScaleName.empty() && globalScaleIndex == 0 && defaultScaleIndex >= 0 && defaultScaleIndex < static_cast<int>(scaleLibrary.size())) {
        globalScaleIndex = defaultScaleIndex;
    }

    int nameIndex = findItemIndexByName(scaleLibrary, globalScaleName);
    if(nameIndex >= 0) {
        globalScaleIndex = nameIndex;
    } else {
        globalScaleIndex = ofClamp(globalScaleIndex, 0, static_cast<int>(scaleLibrary.size()) - 1);
    }

    globalScaleName = scaleLibrary[globalScaleIndex].name;
}

bool chordSequence::parseChordTokenToEntry(const std::string &token, chordSequenceEntry &entry) const {
    std::string trimmed = ofTrim(token);
    if(trimmed.empty()) return false;

    entry.mode = chordSequenceEntry::Cypher;
    entry.itemName = trimmed;
    entry.itemIndex = defaultChordIndex;
    entry.functionalGroup = 0;
    entry.functionalVariantIndex = 0;
    entry.functionalVariantLabel = "I";
    entry.degree = 1;
    entry.chordSize = 4;
    entry.stepInterval = 2;
    entry.transpose = 0;
    entry.inversion = 0;
    return true;
}

bool chordSequence::parseCypherRootAndQuality(const std::string &input, float &rootValue, std::string &quality) const {
    std::string trimmed = ofTrim(input);
    if(trimmed.empty()) {
        rootValue = 0.0f;
        quality = "M";
        return false;
    }

    size_t slashPos = trimmed.find_first_of("/\\");
    if(slashPos != std::string::npos) {
        trimmed = trimmed.substr(0, slashPos);
    }
    if(trimmed.empty()) {
        rootValue = 0.0f;
        quality = "M";
        return false;
    }

    std::string root;
    root += static_cast<char>(std::toupper(trimmed[0]));
    size_t offset = 1;
    if(trimmed.size() > 1 && (trimmed[1] == '#' || trimmed[1] == 'b')) {
        root += trimmed[1];
        offset = 2;
    }

    rootValue = static_cast<float>(getNoteValue(root));
    quality = normalizeChordQuality(trimmed.substr(offset));
    return true;
}

std::string chordSequence::normalizeChordQuality(const std::string &quality) const {
    std::string q = ofTrim(quality);
    q.erase(std::remove(q.begin(), q.end(), ' '), q.end());
    if(q.empty()) return "M";

    for(const auto &item : chordLibrary) {
        if(item.name == q) return item.name;
    }

    std::string resolved = chordCypherAliases::resolveAlias(chordAliases, q);
    for(const auto &item : chordLibrary) {
        if(item.name == resolved) return item.name;
    }

    return resolved;
}

int chordSequence::getNoteValue(const std::string &note) const {
    if(note == "C") return 0;
    if(note == "C#" || note == "Db") return 1;
    if(note == "D") return 2;
    if(note == "D#" || note == "Eb") return 3;
    if(note == "E" || note == "Fb") return 4;
    if(note == "F" || note == "E#") return 5;
    if(note == "F#" || note == "Gb") return 6;
    if(note == "G") return 7;
    if(note == "G#" || note == "Ab") return 8;
    if(note == "A") return 9;
    if(note == "A#" || note == "Bb") return 10;
    if(note == "B" || note == "Cb") return 11;
    return 0;
}

std::vector<std::string> chordSequence::parseChordSequenceString(const std::string &chordString) const {
    std::string flattened = chordString;
    std::replace(flattened.begin(), flattened.end(), '|', ',');

    std::vector<std::string> result;
    auto tokens = ofSplitString(flattened, ",", true, true);
    for(const auto &token : tokens) {
        std::string trimmed = ofTrim(token);
        if(!trimmed.empty()) result.push_back(trimmed);
    }
    return result;
}

std::vector<std::string> chordSequence::extractJazzStandardChords(int songIndex) const {
    std::vector<std::string> result;
    if(!jazzStandardsDatabase.is_array()) return result;
    if(songIndex < 0 || songIndex >= static_cast<int>(jazzStandardsDatabase.size())) return result;

    const auto &song = jazzStandardsDatabase[songIndex];
    if(!song.contains("Sections") || !song["Sections"].is_array()) return result;

    for(const auto &section : song["Sections"]) {
        if(section.contains("MainSegment") && section["MainSegment"].contains("Chords")) {
            auto sectionChords = parseChordSequenceString(section["MainSegment"]["Chords"].get<std::string>());
            result.insert(result.end(), sectionChords.begin(), sectionChords.end());

            int repeats = section.value("Repeats", 0);
            for(int i = 0; i < repeats; i++) {
                result.insert(result.end(), sectionChords.begin(), sectionChords.end());
            }
        }

        if(section.contains("Endings") && section["Endings"].is_array() && !section["Endings"].empty()) {
            const auto &ending = section["Endings"][0];
            if(ending.contains("Chords")) {
                auto endingChords = parseChordSequenceString(ending["Chords"].get<std::string>());
                result.insert(result.end(), endingChords.begin(), endingChords.end());
            }
        }
    }

    return result;
}

void chordSequence::applyImportedChordList(const std::vector<std::string> &chords) {
    if(chords.empty()) return;

    resizeProgression(static_cast<int>(chords.size()));
    for(size_t i = 0; i < chords.size(); i++) {
        parseChordTokenToEntry(chords[i], progression[i]);
    }

    sanitizeProgression();
    refreshAllOutputs(true);
}

void chordSequence::refreshImportedProgressionNames() {
    importedProgressionNames.clear();
    if(importedProgressionDatabase.contains("progressions") && importedProgressionDatabase["progressions"].is_object()) {
        for(auto &[key, value] : importedProgressionDatabase["progressions"].items()) {
            importedProgressionNames.push_back(value.value("name", "Progression " + key));
        }
    }

    if(!importedProgressionNames.empty()) {
        selectedImportedProgression = ofClamp(selectedImportedProgression, 0, static_cast<int>(importedProgressionNames.size()) - 1);
    } else {
        selectedImportedProgression = 0;
    }
}

int chordSequence::getNextImportedProgressionIndex() const {
    if(!importedProgressionDatabase.contains("progressions") || !importedProgressionDatabase["progressions"].is_object()) {
        return 0;
    }

    int maxIndex = -1;
    for(auto &[key, value] : importedProgressionDatabase["progressions"].items()) {
        try {
            maxIndex = std::max(maxIndex, std::stoi(key));
        } catch(...) {
        }
    }

    return maxIndex + 1;
}

std::string chordSequence::getDefaultTimingStringForChords(const std::vector<std::string> &chords) const {
    if(chords.empty()) return "1";
    std::vector<std::string> timings(chords.size(), "1");
    return ofJoinString(timings, ", ");
}

bool chordSequence::appendImportedProgression(const std::string &name, const std::string &chordString) {
    std::vector<std::string> chords = parseChordSequenceString(chordString);
    if(chords.empty()) return false;

    if(!importedProgressionDatabase.contains("progressions") || !importedProgressionDatabase["progressions"].is_object()) {
        importedProgressionDatabase["progressions"] = ofJson::object();
    }

    int nextIndex = getNextImportedProgressionIndex();
    std::string finalName = ofTrim(name);
    if(finalName.empty()) finalName = "user";

    importedProgressionDatabase["progressions"][ofToString(nextIndex)] = {
        {"name", finalName},
        {"chords", ofJoinString(chords, ", ")},
        {"timing", getDefaultTimingStringForChords(chords)}
    };

    ofSavePrettyJson(getChordProgressionsFilePath(), importedProgressionDatabase);
    refreshImportedProgressionNames();
    selectedImportedProgression = std::max(0, static_cast<int>(importedProgressionNames.size()) - 1);
    return true;
}

int chordSequence::findItemIndexByName(const std::vector<chordSequenceLibraryItem> &library, const std::string &name) const {
    for(size_t i = 0; i < library.size(); i++) {
        if(library[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

int chordSequence::getDefaultIndexForMode(int mode) const {
    return mode == chordSequenceEntry::Scale ? defaultScaleIndex : defaultChordIndex;
}

std::string chordSequence::getItemLabel(const chordSequenceEntry &entry) const {
    if(entry.mode == chordSequenceEntry::Cypher) {
        return entry.itemName.empty() ? "C" : entry.itemName;
    }
    if(entry.mode == chordSequenceEntry::Functional) {
        int safeFunctionalGroup = std::max(0, std::min(entry.functionalGroup, 2));
        return entry.functionalVariantLabel.empty() ? functionalGroupLabels[safeFunctionalGroup]
                                                    : entry.functionalVariantLabel;
    }
    if(entry.mode == chordSequenceEntry::Degree) {
        return "Degree " + ofToString(std::max(1, entry.degree));
    }

    const auto &library = getLibraryForMode(entry.mode);
    if(library.empty()) return "---";
    int index = ofClamp(entry.itemIndex, 0, static_cast<int>(library.size()) - 1);
    return library[index].name;
}

const std::vector<chordSequenceLibraryItem> &chordSequence::getLibraryForMode(int mode) const {
    return mode == chordSequenceEntry::Scale ? scaleLibrary : chordLibrary;
}

int chordSequence::getGlobalScaleSafeIndex() const {
    if(scaleLibrary.empty()) return -1;
    return ofClamp(globalScaleIndex, 0, static_cast<int>(scaleLibrary.size()) - 1);
}

int chordSequence::getResolvedEntryDegree(const chordSequenceEntry &entry) const {
    if(entry.mode == chordSequenceEntry::Functional) {
        const std::vector<chordSequenceFunctionalVariant> &variants = getFunctionalVariants(entry.functionalGroup);
        if(!variants.empty()) {
            int safeIndex = ofClamp(entry.functionalVariantIndex, 0, static_cast<int>(variants.size()) - 1);
            return std::max(1, variants[safeIndex].degree);
        }
    }
    return std::max(1, entry.degree);
}

const std::array<std::vector<chordSequenceFunctionalVariant>, 3> *chordSequence::getCurrentFunctionalGroups() const {
    int safeScaleIndex = getGlobalScaleSafeIndex();
    if(safeScaleIndex < 0 || safeScaleIndex >= static_cast<int>(scaleLibrary.size())) return nullptr;

    auto it = functionalHarmonyLibrary.find(scaleLibrary[safeScaleIndex].name);
    if(it == functionalHarmonyLibrary.end()) return nullptr;
    return &it->second;
}

const std::vector<chordSequenceFunctionalVariant> &chordSequence::getFunctionalVariants(int functionalGroup) const {
    static const std::vector<chordSequenceFunctionalVariant> emptyVariants;

    const auto *groups = getCurrentFunctionalGroups();
    if(groups == nullptr) return emptyVariants;

    int safeGroup = ofClamp(functionalGroup, 0, 2);
    return (*groups)[safeGroup];
}

void chordSequence::applyFunctionalVariantToEntry(chordSequenceEntry &entry) const {
    const std::vector<chordSequenceFunctionalVariant> &variants = getFunctionalVariants(entry.functionalGroup);
    if(variants.empty()) {
        entry.functionalVariantIndex = 0;
        if(entry.functionalVariantLabel.empty()) {
            int safeFunctionalGroup = std::max(0, std::min(entry.functionalGroup, 2));
            entry.functionalVariantLabel = functionalGroupLabels[safeFunctionalGroup];
        }
        return;
    }

    int nameIndex = -1;
    if(!entry.functionalVariantLabel.empty()) {
        for(int i = 0; i < static_cast<int>(variants.size()); i++) {
            if(variants[i].label == entry.functionalVariantLabel) {
                nameIndex = i;
                break;
            }
        }
    }

    if(nameIndex >= 0) {
        entry.functionalVariantIndex = nameIndex;
    } else {
        entry.functionalVariantIndex = ofClamp(entry.functionalVariantIndex, 0, static_cast<int>(variants.size()) - 1);
    }

    entry.functionalVariantLabel = variants[entry.functionalVariantIndex].label;
}

std::vector<float> chordSequence::buildDegreeValues(const chordSequenceEntry &entry) const {
    int safeScaleIndex = getGlobalScaleSafeIndex();
    if(safeScaleIndex < 0) return {0.0f};

    const std::vector<float> &scaleValues = scaleLibrary[safeScaleIndex].values;
    if(scaleValues.empty()) return {0.0f};

    int scaleSize = static_cast<int>(scaleValues.size());
    int startDegree = getResolvedEntryDegree(entry) - 1;
    int noteCount = std::max(1, entry.chordSize);
    int interval = std::max(1, entry.stepInterval);

    std::vector<float> degreeValues;
    degreeValues.reserve(noteCount);

    for(int i = 0; i < noteCount; i++) {
        int scalarIndex = startDegree + (i * interval);
        int wrappedIndex = wrapIndex(scalarIndex, scaleSize);
        int octaveOffset = static_cast<int>(std::floor(static_cast<float>(scalarIndex) / static_cast<float>(scaleSize)));
        degreeValues.push_back(scaleValues[wrappedIndex] + static_cast<float>(octaveOffset * 12));
    }

    return degreeValues;
}

std::vector<float> chordSequence::buildEntryIntervals(const chordSequenceEntry &entry) const {
    if(entry.mode == chordSequenceEntry::Cypher) {
        float rootValue = 0.0f;
        std::string quality;
        parseCypherRootAndQuality(entry.itemName, rootValue, quality);

        int qualityIndex = findItemIndexByName(chordLibrary, quality);
        if(qualityIndex < 0) qualityIndex = defaultChordIndex;
        return chordLibrary[qualityIndex].values;
    }

    if(entry.mode == chordSequenceEntry::Degree || entry.mode == chordSequenceEntry::Functional) {
        return buildDegreeValues(entry);
    }

    const auto &library = getLibraryForMode(entry.mode);
    if(library.empty()) return {0.0f};

    int safeIndex = ofClamp(entry.itemIndex, 0, static_cast<int>(library.size()) - 1);
    return library[safeIndex].values;
}

std::vector<float> chordSequence::getDiatonicReferenceScale(const chordSequenceEntry &entry) const {
    if(entry.mode == chordSequenceEntry::Scale) {
        const auto &library = getLibraryForMode(entry.mode);
        if(library.empty()) return {};

        int safeIndex = ofClamp(entry.itemIndex, 0, static_cast<int>(library.size()) - 1);
        std::vector<float> reference = library[safeIndex].values;
        for(auto &value : reference) {
            value += static_cast<float>(entry.transpose);
        }
        return reference;
    }

    if(entry.mode == chordSequenceEntry::Degree || entry.mode == chordSequenceEntry::Functional) {
        int safeScaleIndex = getGlobalScaleSafeIndex();
        if(safeScaleIndex < 0 || safeScaleIndex >= static_cast<int>(scaleLibrary.size())) return {};

        std::vector<float> reference = scaleLibrary[safeScaleIndex].values;
        for(auto &value : reference) {
            value += static_cast<float>(globalKey + entry.transpose);
        }
        return reference;
    }

    return {};
}

std::vector<float> chordSequence::applyDiatonicDeviation(const std::vector<float> &values,
                                                         const chordSequenceEntry &entry) const {
    if(!modeSupportsDiatonicDeviation(entry.mode) ||
       entry.diatonicDeviationProbability <= 0.0f ||
       entry.diatonicDeviationRange <= 0 ||
       values.empty()) {
        return values;
    }

    std::vector<float> reference = getDiatonicReferenceScale(entry);
    if(reference.empty()) return values;

    int scaleSize = static_cast<int>(reference.size());
    if(scaleSize <= 0) return values;

    std::vector<float> deviated = values;
    for(auto &value : deviated) {
        if(ofRandom(100.0f) >= entry.diatonicDeviationProbability) continue;

        int centerOctave = static_cast<int>(std::floor(value / 12.0f));
        int closestScalarIndex = 0;
        float closestDistance = std::numeric_limits<float>::max();

        for(int octave = centerOctave - 2; octave <= centerOctave + 2; octave++) {
            for(int i = 0; i < scaleSize; i++) {
                float candidate = reference[i] + static_cast<float>(octave * 12);
                float distance = std::abs(candidate - value);
                if(distance < closestDistance) {
                    closestDistance = distance;
                    closestScalarIndex = octave * scaleSize + i;
                }
            }
        }

        int deviation = 0;
        while(deviation == 0) {
            deviation = static_cast<int>(std::floor(ofRandom(static_cast<float>(entry.diatonicDeviationRange * 2 + 1)))) -
                        entry.diatonicDeviationRange;
        }

        int deviatedScalarIndex = closestScalarIndex + deviation;
        int octaveOffset = static_cast<int>(std::floor(static_cast<float>(deviatedScalarIndex) /
                                                       static_cast<float>(scaleSize)));
        int wrappedIndex = wrapIndex(deviatedScalarIndex, scaleSize);
        value = reference[wrappedIndex] + static_cast<float>(octaveOffset * 12);
    }

    return deviated;
}

std::vector<float> chordSequence::applyChromaticDeviation(const std::vector<float> &values,
                                                          float probability,
                                                          int range) const {
    if(probability <= 0.0f || range <= 0 || values.empty()) return values;

    std::vector<float> deviated = values;
    for(auto &value : deviated) {
        if(ofRandom(100.0f) >= probability) continue;

        int deviation = 0;
        while(deviation == 0) {
            deviation = static_cast<int>(std::floor(ofRandom(static_cast<float>(range * 2 + 1)))) - range;
        }
        value += static_cast<float>(deviation);
    }

    return deviated;
}

float chordSequence::getEntryRootValue(const chordSequenceEntry &entry) const {
    if(entry.mode == chordSequenceEntry::Cypher) {
        float rootValue = 0.0f;
        std::string quality;
        parseCypherRootAndQuality(entry.itemName, rootValue, quality);
        return rootValue + static_cast<float>(entry.transpose);
    }

    if(entry.mode == chordSequenceEntry::Degree || entry.mode == chordSequenceEntry::Functional) {
        std::vector<float> degreeValues = buildDegreeValues(entry);
        if(degreeValues.empty()) return static_cast<float>(globalKey + entry.transpose);
        return static_cast<float>(globalKey + entry.transpose) + degreeValues.front();
    }

    return static_cast<float>(entry.transpose);
}

std::vector<float> chordSequence::buildEntryPreviewOutput(const chordSequenceEntry &entry) const {
    if(entry.mode == chordSequenceEntry::Degree || entry.mode == chordSequenceEntry::Functional) {
        std::vector<float> values = buildDegreeValues(entry);
        for(auto &value : values) {
            value += static_cast<float>(globalKey + entry.transpose);
        }
        return applyInversion(values, entry.inversion);
    }

    std::vector<float> values = buildEntryIntervals(entry);
    values = applyInversion(values, entry.inversion);

    float rootValue = getEntryRootValue(entry);
    for(auto &value : values) {
        value += rootValue;
    }

    return values;
}

std::vector<float> chordSequence::applyVoicing(const std::vector<float> &values, int voicingMode) const {
    if(values.size() <= 1) return values;

    std::vector<float> voiced = values;
    int safeVoicingMode = ofClamp(voicingMode,
                                  chordSequenceOutputConfig::None,
                                  chordSequenceOutputConfig::Shell);
    if(safeVoicingMode == chordSequenceOutputConfig::None) {
        return voiced;
    }

    std::sort(voiced.begin(), voiced.end());

    if(safeVoicingMode == chordSequenceOutputConfig::Close) {
        return voiced;
    }

    if(safeVoicingMode == chordSequenceOutputConfig::Open) {
        for(size_t i = 1; i < voiced.size(); i += 2) {
            voiced[i] += 12.0f;
        }
        std::sort(voiced.begin(), voiced.end());
        return voiced;
    }

    if(safeVoicingMode == chordSequenceOutputConfig::Drop2) {
        if(voiced.size() >= 2) {
            voiced[voiced.size() - 2] -= 12.0f;
            std::sort(voiced.begin(), voiced.end());
        }
        return voiced;
    }

    if(safeVoicingMode == chordSequenceOutputConfig::Drop3) {
        if(voiced.size() >= 3) {
            voiced[voiced.size() - 3] -= 12.0f;
            std::sort(voiced.begin(), voiced.end());
        }
        return voiced;
    }

    if(safeVoicingMode == chordSequenceOutputConfig::Shell) {
        if(voiced.size() <= 3) {
            return voiced;
        }

        std::vector<float> shell = {voiced.front(), voiced[1], voiced.back()};
        std::sort(shell.begin(), shell.end());
        return shell;
    }

    return voiced;
}

std::vector<float> chordSequence::applyVoicingSpread(const std::vector<float> &values, float spread) const {
    if(values.size() <= 1 || spread <= 0.0001f) return values;

    std::vector<float> spreadValues = values;
    float denominator = static_cast<float>(spreadValues.size() - 1);
    for(size_t i = 1; i < spreadValues.size(); i++) {
        float normalizedIndex = static_cast<float>(i) / denominator;
        spreadValues[i] += spread * normalizedIndex;
    }
    return spreadValues;
}

std::vector<float> chordSequence::buildOutputValues(const chordSequenceEntry &entry,
                                                    const chordSequenceOutputConfig &config) const {
    std::vector<float> values;
    float outputRoot = 0.0f;
    if(config.scaleOnly) {
        outputRoot = static_cast<float>(globalKey);
        int safeScaleIndex = getGlobalScaleSafeIndex();
        if(safeScaleIndex >= 0 && safeScaleIndex < static_cast<int>(scaleLibrary.size())) {
            values = scaleLibrary[safeScaleIndex].values;
            for(auto &value : values) {
                value += static_cast<float>(globalKey);
            }
        } else {
            values = std::vector<float>{outputRoot};
        }
    } else if(config.keyOnly) {
        outputRoot = static_cast<float>(globalKey);
        values = std::vector<float>{outputRoot};
    } else if(config.rootOnly) {
        outputRoot = getEntryRootValue(entry);
        values = std::vector<float>{outputRoot};
    } else {
        values = buildEntryPreviewOutput(entry);
        outputRoot = getEntryRootValue(entry);
        values = applyDiatonicDeviation(values, entry);
    }

    if(!config.scaleOnly) {
        values = applyInversion(values, globalInvert + config.inversion);
    }

    if(config.fold12) {
        for(auto &value : values) {
            value = std::fmod(value, 12.0f);
            if(value < 0.0f) value += 12.0f;
        }
    }

    if(!config.scaleOnly) {
        values = applyVoicing(values, config.voicingMode);
        values = applyVoicingSpread(values, config.voicingSpread);
    }

    int requestedBodySize = std::max(0, config.outputSize - ((!config.scaleOnly && config.addBass) ? 1 : 0));
    if(requestedBodySize == 0) {
        values.clear();
    } else {
        values = adaptOutputSize(values, requestedBodySize, config.expandOutput, false);
    }

    if(config.octaveRandomProbability > 0.0f && config.octaveRandomRange > 0) {
        for(auto &value : values) {
            if(ofRandom(100.0f) < config.octaveRandomProbability) {
                int options = config.octaveRandomRange * 2;
                int octaveOffset = static_cast<int>(std::floor(ofRandom(static_cast<float>(options)))) - config.octaveRandomRange;
                if(octaveOffset >= 0) octaveOffset += 1;
                value += static_cast<float>(octaveOffset * 12);
            }
        }
    }

    values = applyChromaticDeviation(values,
                                     config.chromaticDeviationProbability,
                                     config.chromaticDeviationRange);

    if(!config.scaleOnly && config.addBass) {
        float rootPitchClass = std::fmod(outputRoot, 12.0f);
        if(rootPitchClass < 0.0f) rootPitchClass += 12.0f;
        values.insert(values.begin(), rootPitchClass + static_cast<float>(config.bassOct * 12));
    }

    float pitchOffset = globalPitchBend + config.pitchBend;
    float noteOffset = static_cast<float>(globalTranspose + config.transpose + config.octave * 12);
    for(auto &value : values) {
        value += noteOffset + pitchOffset;
        if(config.perNoteDetune > 0.0f) {
            value += ofRandom(-config.perNoteDetune, config.perNoteDetune);
        }
    }

    if(config.sortOutput) {
        std::sort(values.begin(), values.end());
    }

    return values;
}

std::vector<float> chordSequence::adaptOutputSize(const std::vector<float> &values,
                                                  int requestedSize,
                                                  bool expandOutput,
                                                  bool sortOutput) const {
    if(values.empty()) return {0.0f};

    requestedSize = std::max(1, requestedSize);
    if(requestedSize == static_cast<int>(values.size())) {
        std::vector<float> output = values;
        if(sortOutput) std::sort(output.begin(), output.end());
        return output;
    }

    if(requestedSize < static_cast<int>(values.size())) {
        std::vector<float> output(values.begin(), values.begin() + requestedSize);
        if(sortOutput) std::sort(output.begin(), output.end());
        return output;
    }

    std::vector<float> output;
    output.reserve(requestedSize);
    int baseSize = static_cast<int>(values.size());

    for(int i = 0; i < requestedSize; i++) {
        int sourceIndex = i % baseSize;
        float value = values[sourceIndex];
        if(expandOutput && i >= baseSize) {
            int octaveCycle = i / baseSize;
            value += 12.0f * octaveCycle;
        }
        output.push_back(value);
    }

    if(sortOutput) {
        std::sort(output.begin(), output.end());
    }

    return output;
}

std::vector<float> chordSequence::applyInversion(const std::vector<float> &values, int inversion) const {
    if(values.empty() || inversion == 0) return values;

    std::vector<float> inverted = values;
    if(inversion > 0) {
        for(int i = 0; i < inversion; i++) {
            float moved = inverted.front() + 12.0f;
            inverted.erase(inverted.begin());
            inverted.push_back(moved);
        }
    } else {
        for(int i = 0; i < std::abs(inversion); i++) {
            float moved = inverted.back() - 12.0f;
            inverted.pop_back();
            inverted.insert(inverted.begin(), moved);
        }
    }

    return inverted;
}

int chordSequence::resolveActiveIndex() const {
    if(internalTimingEnabled) {
        return wrapIndex(internalActiveStep, static_cast<int>(progression.size()));
    }
    return wrapIndex(indexInput.get(), static_cast<int>(progression.size()));
}

float chordSequence::getStepDurationMs(int stepIndex) const {
    if(stepIndex < 0 || stepIndex >= static_cast<int>(progression.size())) return 0.0f;
    float bpm = std::max(1.0f, currentBPM);
    return progression[stepIndex].beatDuration * (60000.0f / bpm);
}

void chordSequence::resetInternalSequence(bool forceInstant) {
    sanitizeProgression();
    internalActiveStep = 0;
    if(!progression.empty()) {
        internalActiveStep = ofClamp(internalActiveStep, 0, static_cast<int>(progression.size()) - 1);
        nextInternalStepTimeMs = ofGetElapsedTimeMillis() + static_cast<uint64_t>(std::max(1.0f, getStepDurationMs(internalActiveStep)));
    } else {
        nextInternalStepTimeMs = 0;
    }
    outputBuildDirty = true;
    lastRefreshedActiveIndex = -1;
    refreshAllOutputs(forceInstant);
}

int chordSequence::chooseNextInternalStep(int currentStep) const {
    int sequenceSize = static_cast<int>(progression.size());
    if(sequenceSize <= 0) return 0;
    if(!markovEnabled || currentStep < 0 || currentStep >= sequenceSize) {
        return (currentStep + 1) % sequenceSize;
    }

    const std::vector<float> &weights = progression[currentStep].markovWeights;
    std::vector<float> normalized(sequenceSize, 0.0f);
    float sum = 0.0f;
    for(int i = 0; i < sequenceSize && i < static_cast<int>(weights.size()); i++) {
        normalized[i] = std::max(0.0f, weights[i]);
        sum += normalized[i];
    }

    if(sum <= 0.0001f) {
        return (currentStep + 1) % sequenceSize;
    }

    float threshold = ofRandom(sum);
    float cumulative = 0.0f;
    for(int i = 0; i < sequenceSize; i++) {
        cumulative += normalized[i];
        if(threshold <= cumulative) return i;
    }

    return (currentStep + 1) % sequenceSize;
}

void chordSequence::advanceInternalSequence() {
    if(progression.empty()) {
        nextInternalStepTimeMs = 0;
        return;
    }

    internalActiveStep = chooseNextInternalStep(internalActiveStep);
    outputBuildDirty = true;
    refreshAllOutputs(false);
    nextInternalStepTimeMs = ofGetElapsedTimeMillis() + static_cast<uint64_t>(std::max(1.0f, getStepDurationMs(internalActiveStep)));
}

void chordSequence::refreshAllOutputs(bool forceInstant) {
    sanitizeProgression();
    int activeIndex = resolveActiveIndex();

    if(forceInstant) {
        outputBuildDirty = true;
    }

    if(!outputBuildDirty && activeIndex == lastRefreshedActiveIndex) {
        return;
    }

    for(int i = 0; i < static_cast<int>(outputs.size()); i++) {
        if(activeIndex < 0 || activeIndex >= static_cast<int>(progression.size())) {
            beginGlideTo(i, {0.0f}, true);
        } else {
            beginGlideTo(i, buildOutputValues(progression[activeIndex], outputConfigs[i]), forceInstant);
        }
    }

    lastRefreshedActiveIndex = activeIndex;
    outputBuildDirty = false;
}

void chordSequence::beginGlideTo(int outputIndex, const std::vector<float> &nextTarget, bool forceInstant) {
    if(outputIndex < 0 || outputIndex >= static_cast<int>(outputs.size())) return;

    if(outputIsGliding[outputIndex]) {
        currentOutputs[outputIndex] = getInterpolatedOutput(outputIndex, getGlideProgress(outputIndex));
    } else if(currentOutputs[outputIndex].empty()) {
        currentOutputs[outputIndex] = outputs[outputIndex].get();
    }

    targetOutputs[outputIndex] = nextTarget;

    if(forceInstant || outputConfigs[outputIndex].glideMs <= 0.001f || currentOutputs[outputIndex].empty()) {
        outputIsGliding[outputIndex] = false;
        currentOutputs[outputIndex] = targetOutputs[outputIndex];
        outputs[outputIndex].set(currentOutputs[outputIndex]);
        return;
    }

    glideStartOutputs[outputIndex] = currentOutputs[outputIndex];
    outputGlideStartTimeMs[outputIndex] = ofGetElapsedTimeMillis();
    outputIsGliding[outputIndex] = true;
}

std::vector<float> chordSequence::getInterpolatedOutput(int outputIndex, float progress) const {
    size_t maxSize = std::max(glideStartOutputs[outputIndex].size(), targetOutputs[outputIndex].size());
    if(maxSize == 0) return {};

    std::vector<float> output(maxSize, 0.0f);
    for(size_t i = 0; i < maxSize; i++) {
        float start = sampleVector(glideStartOutputs[outputIndex], i);
        float end = sampleVector(targetOutputs[outputIndex], i);
        output[i] = ofLerp(start, end, progress);
    }
    return output;
}

float chordSequence::getGlideProgress(int outputIndex) const {
    if(!outputIsGliding[outputIndex] || outputConfigs[outputIndex].glideMs <= 0.001f) return 1.0f;
    uint64_t elapsed = ofGetElapsedTimeMillis() - outputGlideStartTimeMs[outputIndex];
    return ofClamp(static_cast<float>(elapsed) / outputConfigs[outputIndex].glideMs, 0.0f, 1.0f);
}

std::vector<float> chordSequence::getDisplayedOutput(int outputIndex) const {
    if(outputIndex < 0 || outputIndex >= static_cast<int>(outputs.size())) return {0.0f};
    return outputIsGliding[outputIndex] ? getInterpolatedOutput(outputIndex, getGlideProgress(outputIndex))
                                        : targetOutputs[outputIndex];
}

float chordSequence::sampleVector(const std::vector<float> &values, size_t index) const {
    if(values.empty()) return 0.0f;
    if(index < values.size()) return values[index];
    return values.back();
}

void chordSequence::drawEditor() {
    float gap = 6.0f;
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float snapshotsWidth = std::max(320.0f, availableWidth * 0.27f);
    float globalWidth = std::max(360.0f, availableWidth * 0.38f);
    float cypherWidth = std::max(240.0f, availableWidth - snapshotsWidth - globalWidth - gap * 2.0f);
    bool hasActiveSnapshot = activeSnapshotSlot >= 0 &&
                             activeSnapshotSlot < SnapshotSlots &&
                             snapshotSlots[activeSnapshotSlot].hasData;
    float snapshotsHeight = getSnapshotsSectionHeight(snapshotsWidth, hasActiveSnapshot);
    float globalHeight = getGlobalSectionHeight();
    float cypherHeight = getCypherSectionHeight(!importedProgressionNames.empty(), !jazzStandardNames.empty());

    float maxStepCardHeight = 0.0f;
    for(const auto &entry : progression) {
        maxStepCardHeight = std::max(maxStepCardHeight, getChordSequenceEntryCardHeight(entry.mode, markovEnabled));
    }
    float stepsSectionHeight = getSectionHeaderHeight() + maxStepCardHeight + 10.0f;
    float maxOutputCardHeight = 0.0f;
    for(const auto &config : outputConfigs) {
        maxOutputCardHeight = std::max(maxOutputCardHeight, getOutputCardHeight(config));
    }
    if(maxOutputCardHeight <= 0.0f) {
        chordSequenceOutputConfig defaultConfig;
        maxOutputCardHeight = getOutputCardHeight(defaultConfig);
    }
    float outputsSectionHeight = getSectionHeaderHeight() + maxOutputCardHeight + 10.0f;

    beginColoredSection("SnapshotsSection",
                        "Snapshots",
                        ImVec2(snapshotsWidth, snapshotsHeight),
                        snapshotsBg,
                        snapshotsTitle,
                        snapshotsSectionExpanded,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(snapshotsSectionExpanded) drawSnapshotManager();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);

    beginColoredSection("GlobalSection",
                        "Global",
                        ImVec2(globalWidth, globalHeight),
                        globalBg,
                        globalTitle,
                        globalSectionExpanded,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(globalSectionExpanded) drawGlobalControls();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);

    beginColoredSection("CypherSection",
                        "Cyphered Progressions",
                        ImVec2(cypherWidth, cypherHeight),
                        cypherBg,
                        cypherTitle,
                        cypherSectionExpanded,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(cypherSectionExpanded) drawImportTools();
    ImGui::EndChild();

    beginColoredSection("StepsSection",
                        "Step Chords",
                        ImVec2(0, stepsSectionHeight),
                        stepsBg,
                        stepsTitle,
                        stepsSectionExpanded,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(stepsSectionExpanded) drawEntries();
    ImGui::EndChild();

    beginColoredSection("OutputsSection",
                        "Outputs",
                        ImVec2(0, outputsSectionHeight),
                        outputsBg,
                        outputsTitle,
                        outputsSectionExpanded,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(outputsSectionExpanded) drawOutputs();
    ImGui::EndChild();
}

void chordSequence::drawGlobalControls() {
    int numChords = static_cast<int>(progression.size());
    int activeIndex = resolveActiveIndex();
    int displayIndex = activeIndex < 0 ? 0 : activeIndex;
    int requestedOutputs = numOutputs;
    float bpmDisplay = currentBPM;

    if(ImGui::BeginTable("ChordSequenceGlobals", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(ImGui::InputInt("Number of Chords", &numChords)) {
            numChordsParameter = ofClamp(numChords, numChordsParameter.getMin(), numChordsParameter.getMax());
        }

        ImGui::TableSetColumnIndex(1);
        if(ImGui::InputInt("Active Index", &displayIndex)) {
            int clampedIndex = ofClamp(displayIndex, 0, std::max(0, static_cast<int>(progression.size()) - 1));
            if(internalTimingEnabled) {
                internalActiveStep = clampedIndex;
                nextInternalStepTimeMs = ofGetElapsedTimeMillis() + static_cast<uint64_t>(std::max(1.0f, getStepDurationMs(internalActiveStep)));
                outputBuildDirty = true;
                lastRefreshedActiveIndex = -1;
                refreshAllOutputs(true);
            } else {
                indexInput = clampedIndex;
            }
        }

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(ImGui::InputInt("Num Outputs", &requestedOutputs)) {
            ensureOutputCount(requestedOutputs);
            refreshAllOutputs(true);
        }

        ImGui::TableSetColumnIndex(1);
        if(ImGui::Checkbox("Internal Timing", &internalTimingEnabled)) {
            if(internalTimingEnabled) {
                resetInternalSequence(true);
            } else {
                indexInput = ofClamp(internalActiveStep, 0, std::max(0, static_cast<int>(progression.size()) - 1));
                nextInternalStepTimeMs = 0;
                outputBuildDirty = true;
                lastRefreshedActiveIndex = -1;
                refreshAllOutputs(true);
            }
        }

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-FLT_MIN);
        int clampedGlobalKey = std::max(0, std::min(globalKey, 11));
        if(ImGui::BeginCombo("Key Root", keyNames[clampedGlobalKey])) {
            for(int i = 0; i < 12; i++) {
                bool selected = i == globalKey;
                if(ImGui::Selectable(keyNames[i], selected)) {
                    globalKey = i;
                    refreshAllOutputs(true);
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TableSetColumnIndex(1);
        sanitizeGlobalScaleSelection();
        std::string scalePreview = scaleLibrary.empty() ? "---" : scaleLibrary[getGlobalScaleSafeIndex()].name;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::BeginCombo("Key Scale", scalePreview.c_str())) {
            for(int i = 0; i < static_cast<int>(scaleLibrary.size()); i++) {
                bool selected = i == getGlobalScaleSafeIndex();
                if(ImGui::Selectable(scaleLibrary[i].name.c_str(), selected)) {
                    globalScaleIndex = i;
                    globalScaleName = scaleLibrary[i].name;
                    refreshAllOutputs(true);
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(drawDraggableFloatWithPopup("Pitch Bend", globalPitchBend, 0.05f, -24.0f, 24.0f, "%.3f")) {
            pitchBendParameter = globalPitchBend;
        }

        ImGui::TableSetColumnIndex(1);
        if(ImGui::InputInt("Transpose", &globalTranspose)) {
            transposeParameter = globalTranspose;
        }

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(ImGui::InputInt("Invert", &globalInvert)) {
            inversionParameter = globalInvert;
        }

        ImGui::TableSetColumnIndex(1);
        if(ImGui::Checkbox("Markov", &markovEnabled)) {
            refreshAllOutputs(true);
        }

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::BeginDisabled();
        ImGui::InputFloat("BPM", &bpmDisplay, 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();

        ImGui::TableSetColumnIndex(1);
        if(ImGui::Button("Reset Sequence")) {
            resetInternalSequence(true);
        }

        ImGui::EndTable();
    }
}

void chordSequence::drawImportTools() {
    ImGui::SetNextItemWidth(std::min(220.0f, ImGui::GetContentRegionAvail().x));
    if(ImGui::InputText("Chord List", importChordBuffer.data(), importChordBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
        applyImportedChordList(parseChordSequenceString(importChordBuffer.data()));
    }

    std::vector<std::string> importChords = parseChordSequenceString(importChordBuffer.data());
    if(ImGui::Button("Apply Chords")) {
        applyImportedChordList(parseChordSequenceString(importChordBuffer.data()));
    }
    if(importChords.empty()) ImGui::BeginDisabled();
    if(ImGui::Button("Append Progression")) {
        std::snprintf(importProgressionNameBuffer.data(), importProgressionNameBuffer.size(), "%s", "user");
        ImGui::OpenPopup("Append Progression Popup");
    }
    if(importChords.empty()) ImGui::EndDisabled();

    if(ImGui::BeginPopupModal("Append Progression Popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Save the current chord list to the progression library.");
        ImGui::SetNextItemWidth(280.0f);
        ImGui::InputText("Name", importProgressionNameBuffer.data(), importProgressionNameBuffer.size());
        ImGui::Spacing();
        ImGui::TextWrapped("%s", ofJoinString(importChords, ", ").c_str());
        ImGui::Spacing();
        if(ImGui::Button("Save", ImVec2(100.0f, 0.0f))) {
            if(appendImportedProgression(importProgressionNameBuffer.data(), importChordBuffer.data())) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if(!importedProgressionNames.empty()) {
        std::string preview = importedProgressionNames[ofClamp(selectedImportedProgression, 0, static_cast<int>(importedProgressionNames.size()) - 1)];
        ImGui::SetNextItemWidth(std::max(120.0f, std::min(260.0f, ImGui::GetContentRegionAvail().x - 110.0f)));
        if(ImGui::BeginCombo("Progression", preview.c_str())) {
            for(int i = 0; i < static_cast<int>(importedProgressionNames.size()); i++) {
                bool selected = i == selectedImportedProgression;
                if(ImGui::Selectable(importedProgressionNames[i].c_str(), selected)) {
                    selectedImportedProgression = i;
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if(ImGui::Button("Load Progression")) {
            int currentIndex = 0;
            if(importedProgressionDatabase.contains("progressions")) {
                for(auto &[key, value] : importedProgressionDatabase["progressions"].items()) {
                    if(currentIndex == selectedImportedProgression) {
                        applyImportedChordList(parseChordSequenceString(value.value("chords", "")));
                        break;
                    }
                    currentIndex++;
                }
            }
        }
    }

    if(!jazzStandardNames.empty()) {
        std::string preview = jazzStandardNames[ofClamp(selectedJazzStandard, 0, static_cast<int>(jazzStandardNames.size()) - 1)];
        ImGui::SetNextItemWidth(260.0f);
        if(ImGui::BeginCombo("Jazz Standard", preview.c_str())) {
            for(int i = 0; i < static_cast<int>(jazzStandardNames.size()); i++) {
                bool selected = i == selectedJazzStandard;
                if(ImGui::Selectable(jazzStandardNames[i].c_str(), selected)) {
                    selectedJazzStandard = i;
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if(ImGui::Button("Load Standard")) {
            applyImportedChordList(extractJazzStandardChords(selectedJazzStandard));
        }
    }
}

void chordSequence::drawEntries() {
    float availableWidth = ImGui::GetContentRegionAvail().x;
    int visibleColumns = std::min(std::max(1, static_cast<int>(progression.size())), 6);
    float gap = 6.0f;
    float slotWidth = std::max(170.0f, (availableWidth - gap * (visibleColumns - 1)) / static_cast<float>(visibleColumns));
    float slotHeight = 0.0f;
    for(const auto &entry : progression) {
        slotHeight = std::max(slotHeight, getChordSequenceEntryCardHeight(entry.mode, markovEnabled));
    }

    ImGui::BeginChild("ChordSequenceEntriesScroller", ImVec2(0, slotHeight + 4.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    for(int i = 0; i < static_cast<int>(progression.size()); i++) {
        if(i > 0) ImGui::SameLine(0.0f, gap);
        drawEntryEditor(i, slotWidth);
    }
    ImGui::EndChild();
}

void chordSequence::drawEntryEditor(int index, float width) {
    chordSequenceEntry &entry = progression[index];
    bool isActive = index == resolveActiveIndex();
    float cardHeight = getChordSequenceEntryCardHeight(entry.mode, markovEnabled);

    ImGui::PushID(index);
    ImVec4 headerColor = isActive ? ImVec4(0.15f, 0.33f, 0.15f, 0.88f) : ImVec4(0.12f, 0.12f, 0.12f, 0.86f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, headerColor);
    ImGui::BeginChild("StepEntryCard", ImVec2(width, cardHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImGui::Text("Step %d", index);
    ImGui::SameLine();
    ImGui::TextColored(isActive ? ImVec4(0.50f, 1.0f, 0.50f, 1.0f) : ImVec4(0.60f, 0.60f, 0.60f, 1.0f), isActive ? "ACTIVE" : "idle");

    float rowWidth = std::max(140.0f, ImGui::GetContentRegionAvail().x * 0.94f);
    float labelGap = 10.0f;
    float maxLabelWidth = std::max({
        ImGui::CalcTextSize("Type").x,
        ImGui::CalcTextSize("Selection").x,
        ImGui::CalcTextSize("Chord Text").x,
        ImGui::CalcTextSize("Function").x,
        ImGui::CalcTextSize("Variant").x,
        ImGui::CalcTextSize("Degree").x,
        ImGui::CalcTextSize("Chord Size").x,
        ImGui::CalcTextSize("Step Interval").x,
        ImGui::CalcTextSize("Diat Dev %").x,
        ImGui::CalcTextSize("Diat Dev Range").x,
        ImGui::CalcTextSize("Beats").x,
        ImGui::CalcTextSize("Transitions").x,
        ImGui::CalcTextSize("Transpose").x,
        ImGui::CalcTextSize("Inversion").x
    });
    float controlWidth = std::max(80.0f, rowWidth - maxLabelWidth - labelGap);

    auto drawRowLabel = [rowWidth](float rowStartX, const char *label) {
        float textWidth = ImGui::CalcTextSize(label).x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(rowStartX + std::max(0.0f, rowWidth - textWidth));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
    };

    int mode = entry.mode;
    float rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::Combo("##Type", &mode, "Chord\0Scale\0Cypher\0Degree\0Functional\0")) {
        entry.mode = mode;
        if(entry.mode == chordSequenceEntry::Cypher) {
            entry.itemName = entry.itemName.empty() ? "C" : entry.itemName;
        } else if(entry.mode == chordSequenceEntry::Degree) {
            entry.itemIndex = 0;
            entry.itemName = "Degree " + ofToString(std::max(1, entry.degree));
        } else if(entry.mode == chordSequenceEntry::Functional) {
            entry.itemIndex = 0;
            entry.functionalGroup = 0;
            entry.functionalVariantIndex = 0;
            if(entry.functionalVariantLabel.empty()) entry.functionalVariantLabel = "I";
        } else {
            entry.itemIndex = getDefaultIndexForMode(mode);
            entry.itemName.clear();
        }
        sanitizeEntry(entry);
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Type");

    if(entry.mode == chordSequenceEntry::Cypher) {
        char cypherBuf[128];
        std::snprintf(cypherBuf, sizeof(cypherBuf), "%s", entry.itemName.c_str());
        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(ImGui::InputText("##ChordText", cypherBuf, sizeof(cypherBuf))) {
            entry.itemName = cypherBuf;
            refreshAllOutputs(true);
        }
        drawRowLabel(rowStartX, "Chord Text");
    } else if(entry.mode != chordSequenceEntry::Degree && entry.mode != chordSequenceEntry::Functional) {
        const auto &library = getLibraryForMode(entry.mode);
        std::string previewLabel = getItemLabel(entry);
        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(ImGui::BeginCombo("##Selection", previewLabel.c_str())) {
            for(int i = 0; i < static_cast<int>(library.size()); i++) {
                bool selected = entry.itemIndex == i;
                if(ImGui::Selectable(library[i].name.c_str(), selected)) {
                    entry.itemIndex = i;
                    entry.itemName = library[i].name;
                    refreshAllOutputs(true);
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        drawRowLabel(rowStartX, "Selection");
    }

    if(entry.mode == chordSequenceEntry::Functional) {
        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        int safeFunctionalGroup = std::max(0, std::min(entry.functionalGroup, 2));
        if(ImGui::BeginCombo("##Function", functionalGroupLabels[safeFunctionalGroup])) {
            for(int i = 0; i < 3; i++) {
                bool selected = entry.functionalGroup == i;
                if(ImGui::Selectable(functionalGroupLabels[i], selected)) {
                    entry.functionalGroup = i;
                    entry.functionalVariantIndex = 0;
                    entry.functionalVariantLabel.clear();
                    sanitizeEntry(entry);
                    refreshAllOutputs(true);
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        drawRowLabel(rowStartX, "Function");

        const std::vector<chordSequenceFunctionalVariant> &variants = getFunctionalVariants(entry.functionalGroup);
        if(variants.empty()) {
            rowStartX = ImGui::GetCursorPosX();
            ImGui::SetNextItemWidth(controlWidth);
            ImGui::BeginDisabled();
            char unavailableBuf[128];
            std::snprintf(unavailableBuf, sizeof(unavailableBuf), "%s", ("No map for " + globalScaleName).c_str());
            ImGui::InputText("##FunctionalUnavailable", unavailableBuf, sizeof(unavailableBuf), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndDisabled();
            drawRowLabel(rowStartX, "Variant");
        } else {
            rowStartX = ImGui::GetCursorPosX();
            ImGui::SetNextItemWidth(controlWidth);
            std::string previewLabel = getItemLabel(entry);
            if(ImGui::BeginCombo("##FunctionalVariant", previewLabel.c_str())) {
                for(int i = 0; i < static_cast<int>(variants.size()); i++) {
                    bool selected = entry.functionalVariantIndex == i;
                    if(ImGui::Selectable(variants[i].label.c_str(), selected)) {
                        entry.functionalVariantIndex = i;
                        entry.functionalVariantLabel = variants[i].label;
                        sanitizeEntry(entry);
                        refreshAllOutputs(true);
                    }
                    if(selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            drawRowLabel(rowStartX, "Variant");
        }
    }

    if(entry.mode == chordSequenceEntry::Degree || entry.mode == chordSequenceEntry::Functional) {
        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(entry.mode == chordSequenceEntry::Degree) {
            if(ImGui::InputInt("##Degree", &entry.degree)) {
                entry.degree = std::max(1, entry.degree);
                sanitizeEntry(entry);
                refreshAllOutputs(true);
            }
        } else {
            int resolvedDegree = getResolvedEntryDegree(entry);
            ImGui::BeginDisabled();
            ImGui::InputInt("##FunctionalDegree", &resolvedDegree, 0, 0, ImGuiInputTextFlags_ReadOnly);
            ImGui::EndDisabled();
        }
        drawRowLabel(rowStartX, "Degree");

        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(ImGui::InputInt("##ChordSize", &entry.chordSize)) {
            entry.chordSize = std::max(1, entry.chordSize);
            sanitizeEntry(entry);
            refreshAllOutputs(true);
        }
        drawRowLabel(rowStartX, "Chord Size");

        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(ImGui::InputInt("##StepInterval", &entry.stepInterval)) {
            entry.stepInterval = std::max(1, entry.stepInterval);
            sanitizeEntry(entry);
            refreshAllOutputs(true);
        }
        drawRowLabel(rowStartX, "Step Interval");
    }

    if(modeSupportsDiatonicDeviation(entry.mode)) {
        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(drawDraggableFloatWithPopup("##DiatonicDeviationProbability",
                                       entry.diatonicDeviationProbability,
                                       0.5f,
                                       0.0f,
                                       100.0f,
                                       "%.1f")) {
            sanitizeEntry(entry);
            refreshAllOutputs(true);
        }
        drawRowLabel(rowStartX, "Diat Dev %");

        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(ImGui::InputInt("##DiatonicDeviationRange", &entry.diatonicDeviationRange)) {
            sanitizeEntry(entry);
            refreshAllOutputs(true);
        }
        drawRowLabel(rowStartX, "Diat Dev Range");
    }

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(drawDraggableFloatWithPopup("##BeatDuration", entry.beatDuration, 0.05f, 0.001f, 64.0f, "%.3f")) {
        outputBuildDirty = true;
        if(internalTimingEnabled) {
            nextInternalStepTimeMs = ofGetElapsedTimeMillis() + static_cast<uint64_t>(std::max(1.0f, getStepDurationMs(resolveActiveIndex())));
        }
    }
    drawRowLabel(rowStartX, "Beats");

    if(markovEnabled) {
        ImGui::TextUnformatted("Transitions");

        float sliderRegionWidth = rowWidth;
        float sliderHeight = 37.0f;
        float innerGap = 3.0f;
        int transitionCount = std::max(1, static_cast<int>(progression.size()));
        float sliderWidth = std::max(8.0f, (sliderRegionWidth - innerGap * (transitionCount - 1)) / static_cast<float>(transitionCount));

        ImGui::BeginChild("MarkovSliders", ImVec2(sliderRegionWidth, sliderHeight + 20.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        for(int j = 0; j < transitionCount; j++) {
            if(j > 0) ImGui::SameLine(0.0f, innerGap);
            ImGui::BeginGroup();
            ImVec2 sliderSize(sliderWidth, sliderHeight);
            if(ImGui::VSliderFloat(("##To" + ofToString(j)).c_str(), sliderSize, &entry.markovWeights[j], 0.0f, 1.0f, "")) {
                entry.markovWeights[j] = std::max(0.0f, entry.markovWeights[j]);
            }
            std::string stepLabel = ofToString(j);
            float textWidth = ImGui::CalcTextSize(stepLabel.c_str()).x;
            float centeredX = ImGui::GetCursorPosX() + std::max(0.0f, (sliderWidth - textWidth) * 0.5f);
            ImGui::SetCursorPosX(centeredX);
            ImGui::TextUnformatted(stepLabel.c_str());
            ImGui::EndGroup();
        }
        ImGui::EndChild();

        if(ImGui::Button("Seq##Markov")) {
            std::fill(entry.markovWeights.begin(), entry.markovWeights.end(), 0.0f);
            if(!entry.markovWeights.empty()) {
                entry.markovWeights[(index + 1) % entry.markovWeights.size()] = 1.0f;
            }
        }
        ImGui::SameLine();
        if(ImGui::Button("Clr##Markov")) {
            std::fill(entry.markovWeights.begin(), entry.markovWeights.end(), 0.0f);
        }
    }

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##Transpose", &entry.transpose)) {
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Transpose");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##Inversion", &entry.inversion)) {
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Inversion");

    std::vector<float> preview = buildEntryPreviewOutput(entry);
    drawKeyboardDisplay("StepKeyboard", preview, rowWidth, 62.0f, isActive, true);

    std::string previewText;
    for(size_t i = 0; i < preview.size(); i++) {
        if(i > 0) previewText += "  ";
        previewText += ofToString(static_cast<int>(std::round(preview[i])));
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + rowWidth);
    ImGui::TextWrapped("%s", previewText.c_str());
    ImGui::PopTextWrapPos();

    ImGui::EndChild();
    ImGui::PopID();
}

void chordSequence::drawOutputs() {
    float availableWidth = ImGui::GetContentRegionAvail().x;
    int visibleColumns = std::min(std::max(1, static_cast<int>(outputs.size())), 4);
    float gap = 8.0f;
    float slotWidth = std::max(230.0f, std::min(320.0f, (availableWidth - gap * (visibleColumns - 1)) / static_cast<float>(visibleColumns)));
    float slotHeight = 0.0f;
    for(const auto &config : outputConfigs) {
        slotHeight = std::max(slotHeight, getOutputCardHeight(config));
    }
    if(slotHeight <= 0.0f) {
        chordSequenceOutputConfig defaultConfig;
        slotHeight = getOutputCardHeight(defaultConfig);
    }

    ImGui::BeginChild("ChordSequenceOutputsScroller", ImVec2(0, slotHeight + 4.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    for(int i = 0; i < static_cast<int>(outputs.size()); i++) {
        if(i > 0) ImGui::SameLine(0.0f, gap);
        drawOutputEditor(i, slotWidth);
    }
    ImGui::EndChild();
}

void chordSequence::drawOutputEditor(int index, float width) {
    chordSequenceOutputConfig &config = outputConfigs[index];
    std::vector<float> displayedOutput = getDisplayedOutput(index);
    float cardHeight = getOutputCardHeight(config);

    ImGui::PushID(index);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.14f, 0.15f, 0.88f));
    ImGui::BeginChild("OutputCard", ImVec2(width, cardHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImGui::Text("%s", outputName(index).c_str());

    float rowWidth = std::max(180.0f, ImGui::GetContentRegionAvail().x * 0.94f);
    float labelGap = 10.0f;
    float maxLabelWidth = std::max({
        ImGui::CalcTextSize("Octave").x,
        ImGui::CalcTextSize("Transpose").x,
        ImGui::CalcTextSize("Pitch Bend").x,
        ImGui::CalcTextSize("Detune").x,
        ImGui::CalcTextSize("Oct Rand %").x,
        ImGui::CalcTextSize("Oct Rand Range").x,
        ImGui::CalcTextSize("Chrom Dev %").x,
        ImGui::CalcTextSize("Chrom Dev Range").x,
        ImGui::CalcTextSize("Scale").x,
        ImGui::CalcTextSize("AddBass").x,
        ImGui::CalcTextSize("BassOct").x,
        ImGui::CalcTextSize("Inversion").x,
        ImGui::CalcTextSize("Voicing").x,
        ImGui::CalcTextSize("Spread").x,
        ImGui::CalcTextSize("Key").x,
        ImGui::CalcTextSize("Root").x,
        ImGui::CalcTextSize("Fold12").x,
        ImGui::CalcTextSize("Glide").x,
        ImGui::CalcTextSize("Output Size").x,
        ImGui::CalcTextSize("Expand").x,
        ImGui::CalcTextSize("Sort").x
    });
    float controlWidth = std::max(84.0f, rowWidth - maxLabelWidth - labelGap);

    auto drawRowLabel = [rowWidth](float rowStartX, const char *label) {
        float textWidth = ImGui::CalcTextSize(label).x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(rowStartX + std::max(0.0f, rowWidth - textWidth));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
    };

    auto drawBoolPairRow = [&](const char *leftId,
                               bool &leftValue,
                               const char *leftLabel,
                               const char *rightId,
                               bool &rightValue,
                               const char *rightLabel) {
        float rowStartX = ImGui::GetCursorPosX();
        float pairGap = 12.0f;
        float pairWidth = std::max(70.0f, (rowWidth - pairGap) * 0.5f);

        ImGui::SetCursorPosX(rowStartX);
        if(ImGui::Checkbox(leftId, &leftValue)) {
            refreshAllOutputs(true);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(leftLabel);

        ImGui::SameLine(rowStartX + pairWidth + pairGap);
        if(ImGui::Checkbox(rightId, &rightValue)) {
            refreshAllOutputs(true);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(rightLabel);
    };

    auto drawSingleToggleRow = [&](const char *id, bool &value, const char *label) {
        if(ImGui::Checkbox(id, &value)) {
            refreshAllOutputs(true);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
    };

    float rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##Octave", &config.octave)) {
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Octave");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##Transpose", &config.transpose)) {
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Transpose");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(drawDraggableFloatWithPopup("##PitchBend", config.pitchBend, 0.05f, -24.0f, 24.0f, "%.3f")) {
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Pitch Bend");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(drawDraggableFloatWithPopup("##Detune", config.perNoteDetune, 0.01f, 0.0f, 2.0f, "%.3f")) {
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Detune");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(drawDraggableFloatWithPopup("##OctRandProbability", config.octaveRandomProbability, 0.5f, 0.0f, 100.0f, "%.1f")) {
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Oct Rand %");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##OctRandRange", &config.octaveRandomRange)) {
        config.octaveRandomRange = std::max(0, config.octaveRandomRange);
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Oct Rand Range");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(drawDraggableFloatWithPopup("##ChromaticDeviationProbability",
                                   config.chromaticDeviationProbability,
                                   0.5f,
                                   0.0f,
                                   100.0f,
                                   "%.1f")) {
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Chrom Dev %");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##ChromaticDeviationRange", &config.chromaticDeviationRange)) {
        config.chromaticDeviationRange = std::max(0, config.chromaticDeviationRange);
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Chrom Dev Range");

    {
        float rowStartX = ImGui::GetCursorPosX();
        float pairGap = 12.0f;
        float pairWidth = std::max(70.0f, (rowWidth - pairGap) * 0.5f);

        ImGui::SetCursorPosX(rowStartX);
        if(ImGui::Checkbox("##Scale", &config.scaleOnly)) {
            if(config.scaleOnly) {
                config.rootOnly = false;
                config.keyOnly = false;
            }
            refreshAllOutputs(true);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Scale");

        ImGui::SameLine(rowStartX + pairWidth + pairGap);
        if(ImGui::Checkbox("##Fold12", &config.fold12)) {
            refreshAllOutputs(true);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Fold12");
    }

    if(!config.scaleOnly) {
        drawBoolPairRow("##Root", config.rootOnly, "Root",
                        "##Key", config.keyOnly, "Key");
        if(config.rootOnly && config.keyOnly) {
            config.rootOnly = false;
        }
        drawSingleToggleRow("##AddBass", config.addBass, "AddBass");

        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(ImGui::InputInt("##Inversion", &config.inversion)) {
            refreshAllOutputs(true);
        }
        drawRowLabel(rowStartX, "Inversion");

        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        int safeVoicingMode = ofClamp(config.voicingMode,
                                      chordSequenceOutputConfig::None,
                                      chordSequenceOutputConfig::Shell);
        if(ImGui::BeginCombo("##Voicing", voicingLabels[safeVoicingMode])) {
            for(int i = chordSequenceOutputConfig::None; i <= chordSequenceOutputConfig::Shell; i++) {
                bool selected = safeVoicingMode == i;
                if(ImGui::Selectable(voicingLabels[i], selected)) {
                    config.voicingMode = i;
                    refreshAllOutputs(true);
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        drawRowLabel(rowStartX, "Voicing");

        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(drawDraggableFloatWithPopup("##Spread", config.voicingSpread, 0.1f, 0.0f, 24.0f, "%.2f")) {
            refreshAllOutputs(true);
        }
        drawRowLabel(rowStartX, "Spread");

        if(config.addBass) {
            rowStartX = ImGui::GetCursorPosX();
            ImGui::SetNextItemWidth(controlWidth);
            if(ImGui::InputInt("##BassOct", &config.bassOct)) {
                refreshAllOutputs(true);
            }
            drawRowLabel(rowStartX, "BassOct");
        }
    }

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    drawDraggableFloatWithPopup("##Glide", config.glideMs, 2.0f, 0.0f, 10000.0f, "%.1f");
    drawRowLabel(rowStartX, "Glide");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##OutputSize", &config.outputSize)) {
        config.outputSize = std::max(1, config.outputSize);
        if(index < static_cast<int>(outputSizeParameters.size()) && outputSizeParameters[index]) {
            outputSizeParameters[index]->set(config.outputSize);
        }
        refreshAllOutputs(true);
    }
    drawRowLabel(rowStartX, "Output Size");

    drawBoolPairRow("##Expand", config.expandOutput, "Expand",
                    "##Sort", config.sortOutput, "Sort");

    drawKeyboardDisplay("OutputKeyboard", displayedOutput, rowWidth, 64.0f, true, false, false);

    std::string outputText;
    for(size_t i = 0; i < displayedOutput.size(); i++) {
        if(i > 0) outputText += "  ";
        outputText += ofToString(displayedOutput[i], 2);
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + rowWidth);
    ImGui::TextWrapped("%s", outputText.c_str());
    ImGui::PopTextWrapPos();

    ImGui::EndChild();
    ImGui::PopID();
}

void chordSequence::drawSnapshotManager() {
    auto getSnapshotDisplayName = [this](int slot) {
        if(slot < 0 || slot >= SnapshotSlots) return std::string("Snapshot");
        const auto &snapshot = snapshotSlots[slot];
        if(!snapshot.name.empty()) return snapshot.name;
        return "Snapshot " + ofToString(slot + 1);
    };

    auto getSnapshotDropdownLabel = [&](int slot) {
        return ofToString(slot + 1) + ": " + getSnapshotDisplayName(slot);
    };

    ImGui::TextDisabled("Shift+Click store, Right-Click delete");

    float slotSize = 22.0f;
    float slotGap = 4.0f;
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
            if(ImGui::GetIO().KeyShift) {
                storeToSlot(i);
            } else {
                recallSlot(i);
            }
        }

        if(ImGui::BeginPopupContextItem("SnapshotSlotContext")) {
            if(hasData && ImGui::MenuItem("Delete Snapshot")) {
                deleteSnapshotFromDisk(i);
            }
            ImGui::EndPopup();
        }

        if(ImGui::IsItemHovered() && hasData) {
            ImGui::SetTooltip("%s", getSnapshotDropdownLabel(i).c_str());
        }

        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    ImGui::Spacing();

    int dropdownSlot = activeSnapshotSlot;
    if(dropdownSlot < 0 || dropdownSlot >= SnapshotSlots || !snapshotSlots[dropdownSlot].hasData) {
        dropdownSlot = -1;
        for(int i = 0; i < SnapshotSlots; i++) {
            if(snapshotSlots[i].hasData) {
                dropdownSlot = i;
                break;
            }
        }
    }

    std::string dropdownPreview = dropdownSlot >= 0 ? getSnapshotDropdownLabel(dropdownSlot) : std::string("Select snapshot");
    ImGui::SetNextItemWidth(std::min(260.0f, ImGui::GetContentRegionAvail().x));
    if(ImGui::BeginCombo("Recall", dropdownPreview.c_str())) {
        for(int i = 0; i < SnapshotSlots; i++) {
            if(!snapshotSlots[i].hasData) continue;
            bool selected = i == activeSnapshotSlot;
            std::string label = getSnapshotDropdownLabel(i);
            if(ImGui::Selectable(label.c_str(), selected)) {
                recallSlot(i);
            }
            if(selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if(activeSnapshotSlot >= 0 && activeSnapshotSlot < SnapshotSlots && snapshotSlots[activeSnapshotSlot].hasData) {
        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", snapshotSlots[activeSnapshotSlot].name.c_str());
        ImGui::SetNextItemWidth(std::min(260.0f, ImGui::GetContentRegionAvail().x));
        if(ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            snapshotSlots[activeSnapshotSlot].name = nameBuf;
            saveSnapshotToDisk(activeSnapshotSlot);
        }
    }
}

ofJson chordSequence::serializeCurrentState() const {
    ofJson json;
    json["numOutputs"] = numOutputs;
    json["globalKey"] = globalKey;
    json["globalScaleIndex"] = globalScaleIndex;
    json["globalScaleName"] = globalScaleName;
    json["globalTranspose"] = globalTranspose;
    json["globalInvert"] = globalInvert;
    json["globalPitchBend"] = globalPitchBend;
    json["internalTimingEnabled"] = internalTimingEnabled;
    json["markovEnabled"] = markovEnabled;
    json["internalActiveStep"] = internalActiveStep;
    json["entries"] = ofJson::array();
    json["outputs"] = ofJson::array();

    for(const auto &entry : progression) {
        json["entries"].push_back(entry.toJson());
    }
    for(const auto &output : outputConfigs) {
        json["outputs"].push_back(output.toJson());
    }

    return json;
}

void chordSequence::deserializeState(const ofJson &json, bool forceInstant) {
    globalKey = ofClamp(json.value("globalKey", 0), 0, 11);
    globalScaleIndex = std::max(0, json.value("globalScaleIndex", defaultScaleIndex));
    globalScaleName = json.value("globalScaleName", std::string());
    globalTranspose = json.value("globalTranspose", 0);
    globalInvert = json.value("globalInvert", 0);
    globalPitchBend = json.value("globalPitchBend", json.value("pitchBend", 0.0f));
    internalTimingEnabled = json.value("internalTimingEnabled", false);
    markovEnabled = json.value("markovEnabled", false);
    internalActiveStep = std::max(0, json.value("internalActiveStep", 0));

    progression.clear();
    if(json.contains("entries") && json["entries"].is_array()) {
        for(const auto &entryJson : json["entries"]) {
            progression.push_back(chordSequenceEntry::fromJson(entryJson));
        }
    }
    if(progression.empty()) {
        initializeDefaultProgression();
    }

    int desiredOutputs = 1;
    if(json.contains("outputs") && json["outputs"].is_array()) {
        desiredOutputs = std::max(1, static_cast<int>(json["outputs"].size()));
    } else {
        desiredOutputs = std::max(1, json.value("numOutputs", 1));
    }
    ensureOutputCount(desiredOutputs);

    for(auto &config : outputConfigs) {
        config = chordSequenceOutputConfig();
    }

    if(json.contains("outputs") && json["outputs"].is_array()) {
        for(size_t i = 0; i < outputConfigs.size() && i < json["outputs"].size(); i++) {
            outputConfigs[i] = chordSequenceOutputConfig::fromJson(json["outputs"][i]);
        }
    } else {
        outputConfigs[0].outputSize = std::max(1, json.value("outputSize", 4));
        outputConfigs[0].expandOutput = json.value("expandOutput", false);
        outputConfigs[0].sortOutput = json.value("sortOutput", false);
        outputConfigs[0].glideMs = json.value("glideMs", 0.0f);
    }

    sanitizeProgression();
    sanitizeGlobalScaleSelection();
    syncNodeGuiParametersFromState();
    outputBuildDirty = true;
    lastRefreshedActiveIndex = -1;
    if(internalTimingEnabled) {
        if(!progression.empty()) {
            internalActiveStep = ofClamp(internalActiveStep, 0, static_cast<int>(progression.size()) - 1);
            nextInternalStepTimeMs = ofGetElapsedTimeMillis() + static_cast<uint64_t>(std::max(1.0f, getStepDurationMs(internalActiveStep)));
        } else {
            internalActiveStep = 0;
            nextInternalStepTimeMs = 0;
        }
        refreshAllOutputs(forceInstant);
    } else {
        nextInternalStepTimeMs = 0;
        refreshAllOutputs(forceInstant);
    }
}

void chordSequence::storeToSlot(int slot) {
    if(slot < 0 || slot >= SnapshotSlots) return;

    chordSequenceSnapshot snapshot;
    snapshot.entries = progression;
    snapshot.outputs = outputConfigs;
    snapshot.name = snapshotSlots[slot].name.empty() ? "Snapshot " + ofToString(slot + 1) : snapshotSlots[slot].name;
    snapshot.numOutputs = numOutputs;
    snapshot.globalKey = globalKey;
    snapshot.globalScaleIndex = globalScaleIndex;
    snapshot.globalScaleName = globalScaleName;
    snapshot.globalTranspose = globalTranspose;
    snapshot.globalInvert = globalInvert;
    snapshot.globalPitchBend = globalPitchBend;
    snapshot.internalTimingEnabled = internalTimingEnabled;
    snapshot.markovEnabled = markovEnabled;
    snapshot.internalActiveStep = internalActiveStep;
    snapshot.hasData = true;

    snapshotSlots[slot] = snapshot;
    activeSnapshotSlot = slot;
    saveSnapshotToDisk(slot);
}

void chordSequence::recallSlot(int slot) {
    if(slot < 0 || slot >= SnapshotSlots) return;
    if(!snapshotSlots[slot].hasData) return;

    activeSnapshotSlot = slot;
    deserializeState(snapshotSlots[slot].toJson(), false);
}

void chordSequence::saveSnapshotToDisk(int slot) const {
    if(slot < 0 || slot >= SnapshotSlots) return;
    if(!snapshotSlots[slot].hasData) return;

    ofDirectory directory(getSnapshotsFolderPath());
    if(!directory.exists()) {
        directory.create(true);
    }

    ofSavePrettyJson(getSnapshotFilePath(slot), snapshotSlots[slot].toJson());
}

void chordSequence::loadSnapshotFromDisk(int slot) {
    if(slot < 0 || slot >= SnapshotSlots) return;

    ofFile file(getSnapshotFilePath(slot));
    if(!file.exists()) return;

    ofJson json = ofLoadJson(file.getAbsolutePath());
    if(json.empty()) return;

    snapshotSlots[slot] = chordSequenceSnapshot::fromJson(json);
}

void chordSequence::loadAllSnapshotsFromDisk() {
    for(auto &slot : snapshotSlots) {
        slot = chordSequenceSnapshot();
    }

    for(int i = 0; i < SnapshotSlots; i++) {
        loadSnapshotFromDisk(i);
    }
}

void chordSequence::deleteSnapshotFromDisk(int slot) {
    if(slot < 0 || slot >= SnapshotSlots) return;

    ofFile file(getSnapshotFilePath(slot));
    if(file.exists()) {
        file.remove();
    }

    snapshotSlots[slot] = chordSequenceSnapshot();
    if(activeSnapshotSlot == slot) {
        activeSnapshotSlot = -1;
    }
}

std::string chordSequence::getSnapshotsFolderPath() const {
    return ofToDataPath("nodeSnapshots/ChordSequence/", true);
}

std::string chordSequence::getSnapshotFilePath(int slot) const {
    return getSnapshotsFolderPath() + "snapshot_" + ofToString(slot) + ".json";
}

std::string chordSequence::getChordProgressionsFilePath() const {
    return ofToDataPath("Supercollider/Pitchclass/chord_progressions.json", true);
}
