#include "chordSequence.h"

#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
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
    const char *progressionOrderLabels[] = {"Input Idx", "Ascendent", "Descendent", "Random", "Markov"};
    const char *functionalGroupKeys[] = {"tonic", "subdominant", "dominant"};
    const char *functionalGroupLabels[] = {"Tonic", "Subdominant", "Dominant"};
    const char *voicingLabels[] = {"None", "Close", "Open", "Drop 2", "Drop 3", "Shell"};
    const char *outputSourceLabels[] = {"Chord", "Scale", "Root", "Key", "Chord Sum"};
    constexpr double transportResetBeatWindow = 0.05;
    float chordSequenceLayoutZoom = 1.0f;
    float chordSequenceFontZoom = 1.0f;

    float scaledUi(float value) {
        return value * chordSequenceLayoutZoom;
    }

    bool modeSupportsDiatonicDeviation(int mode) {
        return mode == chordSequenceEntry::Scale ||
               mode == chordSequenceEntry::Degree ||
               mode == chordSequenceEntry::Functional;
    }

    bool outputSourceUsesScaleLikeMaterial(int sourceMode) {
        return sourceMode == chordSequenceOutputConfig::Scale ||
               sourceMode == chordSequenceOutputConfig::ChordSum;
    }

    const ImVec4 snapshotsBg = ImVec4(0.24f, 0.46f, 0.28f, 0.97f);
    const ImVec4 snapshotsTitle = ImVec4(0.92f, 1.00f, 0.94f, 1.00f);
    const ImVec4 globalBg = ImVec4(0.21f, 0.42f, 0.25f, 0.97f);
    const ImVec4 globalTitle = ImVec4(0.89f, 0.99f, 0.91f, 1.00f);
    const ImVec4 randomationBg = ImVec4(0.18f, 0.38f, 0.23f, 0.97f);
    const ImVec4 randomationTitle = ImVec4(0.85f, 0.98f, 0.88f, 1.00f);
    const ImVec4 cypherBg = ImVec4(0.15f, 0.34f, 0.21f, 0.97f);
    const ImVec4 cypherTitle = ImVec4(0.82f, 0.96f, 0.85f, 1.00f);
    const ImVec4 stepsBg = ImVec4(0.12f, 0.30f, 0.18f, 0.97f);
    const ImVec4 stepsTitle = ImVec4(0.78f, 0.94f, 0.82f, 1.00f);
    const ImVec4 outputsBg = ImVec4(0.09f, 0.26f, 0.16f, 0.97f);
    const ImVec4 outputsTitle = ImVec4(0.75f, 0.93f, 0.79f, 1.00f);

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
                             bool compact = false) {
        std::vector<int> resizedNotes = toDisplayNotes(values);
        std::map<int, int> noteCounts;
        for(int note : resizedNotes) {
            noteCounts[note]++;
        }

        auto range = computeKeyboardRange(resizedNotes, compact ? 48 : 36, compact ? 72 : 84, compact ? 18 : 24);
        std::vector<ChordSequenceKeyGeometry> geometry = buildKeyboardGeometryForRange(range.first, range.second, width);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImGui::InvisibleButton(id, ImVec2(std::max(1.0f, width), std::max(1.0f, height)));

        ImU32 frameBg = active ? IM_COL32(16, 30, 28, 255) : IM_COL32(16, 18, 24, 255);
        drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), frameBg, 6.0f);
        drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(66, 86, 96, 220), 6.0f);

        if(geometry.empty()) return;

        float blackKeyHeight = height * 0.62f;
        ImU32 whiteKeyColor = IM_COL32(244, 239, 232, 255);
        ImU32 blackKeyColor = IM_COL32(28, 30, 38, 255);
        ImU32 highlightColor = active ? IM_COL32(255, 182, 96, 200) : IM_COL32(96, 216, 224, 185);

        for(size_t i = 0; i < geometry.size(); i++) {
            if(geometry[i].isBlack) continue;

            int note = range.first + static_cast<int>(i);
            ImVec2 keyPos(pos.x + geometry[i].x, pos.y);
            ImVec2 keyEnd(keyPos.x + geometry[i].w, pos.y + height);
            drawList->AddRectFilled(keyPos, keyEnd, whiteKeyColor);
            drawList->AddRect(keyPos, keyEnd, IM_COL32(102, 104, 110, 255));

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
            drawList->AddRect(keyPos, keyEnd, IM_COL32(78, 80, 88, 255));

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

            ImGui::SetNextItemWidth(scaledUi(120.0f));
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
                             float fontScale,
                             ImGuiWindowFlags flags = 0) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(titleColor.x * 0.55f, titleColor.y * 0.55f, titleColor.z * 0.55f, 0.30f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, scaledUi(9.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        float collapsedHeight = std::max(ImGui::GetTextLineHeightWithSpacing() + scaledUi(10.0f), scaledUi(30.0f));
        ImGui::BeginChild(id, ImVec2(size.x, expanded ? size.y : collapsedHeight), true, flags);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::SetWindowFontScale(fontScale);

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImVec2 childPos = ImGui::GetWindowPos();
        ImVec2 childSize = ImGui::GetWindowSize();
        float headerHeight = std::max(scaledUi(24.0f), ImGui::GetTextLineHeightWithSpacing() + scaledUi(10.0f));
        ImU32 headerFill = IM_COL32(static_cast<int>(titleColor.x * 255.0f),
                                    static_cast<int>(titleColor.y * 255.0f),
                                    static_cast<int>(titleColor.z * 255.0f),
                                    34);
        ImU32 headerLine = IM_COL32(static_cast<int>(titleColor.x * 255.0f),
                                    static_cast<int>(titleColor.y * 255.0f),
                                    static_cast<int>(titleColor.z * 255.0f),
                                    170);
        drawList->AddRectFilled(childPos, ImVec2(childPos.x + childSize.x, childPos.y + headerHeight), headerFill, scaledUi(9.0f), ImDrawFlags_RoundCornersTop);
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
        if(expanded) {
            ImGui::Separator();
        }
    }

    float getSectionHeaderHeight() {
        return ImGui::GetFrameHeightWithSpacing() + scaledUi(10.0f);
    }

    float getChordSequenceEntryCardHeight(int mode, bool markovEnabled) {
        float rowHeight = ImGui::GetFrameHeightWithSpacing();
        float textHeight = ImGui::GetTextLineHeightWithSpacing();
        float height = scaledUi(24.0f); // header
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
            height += scaledUi(57.0f); // inline multislider area
            height += rowHeight; // helper buttons
        }
        height += rowHeight * 2.0f; // transpose, inversion
        height += scaledUi(62.0f); // keyboard
        height += std::max(scaledUi(18.0f), textHeight); // preview text
        height += scaledUi(8.0f); // bottom padding

        return height;
    }

    float getOutputCardHeight(const chordSequenceOutputConfig &config) {
        float rowHeight = ImGui::GetFrameHeightWithSpacing();
        float textHeight = ImGui::GetTextLineHeightWithSpacing();
        float height = scaledUi(24.0f); // header
        float controlRows = 15.0f; // always-visible rows
        if(!outputSourceUsesScaleLikeMaterial(config.sourceMode)) {
            controlRows += 5.0f; // root/key, addBass, inversion, voicing, spread
            if(config.addBass) controlRows += 1.0f; // bass octave
        }
        height += rowHeight * controlRows;
        height += scaledUi(64.0f); // keyboard
        height += std::max(scaledUi(20.0f), textHeight * 1.1f); // value preview
        height += scaledUi(10.0f); // bottom padding
        return height;
    }

    float getSnapshotsSectionHeight(float availableWidth, bool hasActiveSnapshot) {
        float contentWidth = std::max(1.0f, availableWidth - scaledUi(18.0f));
        float textHeight = ImGui::GetTextLineHeightWithSpacing();
        float rowHeight = ImGui::GetFrameHeightWithSpacing();
        float slotSize = scaledUi(22.0f);
        float slotGap = scaledUi(4.0f);
        int columns = std::max(1, static_cast<int>((contentWidth + slotGap) / (slotSize + slotGap)));
        int rows = (16 + columns - 1) / columns;

        float height = getSectionHeaderHeight();
        height += textHeight;
        height += rows * slotSize;
        height += std::max(0, rows - 1) * slotGap;
        height += scaledUi(8.0f);
        height += rowHeight; // recall combo
        if(hasActiveSnapshot) {
            height += rowHeight; // name field
        }
        height += scaledUi(8.0f);
        return height;
    }

    float getGlobalSectionHeight() {
        float rowHeight = ImGui::GetFrameHeightWithSpacing();
        float height = getSectionHeaderHeight();
        height += rowHeight * 6.0f;
        height += scaledUi(8.0f);
        return height;
    }

    float getRandomationSectionHeight() {
        float rowHeight = ImGui::GetFrameHeightWithSpacing();
        float textHeight = ImGui::GetTextLineHeightWithSpacing();
        float height = getSectionHeaderHeight();
        height += textHeight * 2.0f;
        height += rowHeight * 6.0f;
        height += scaledUi(12.0f);
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
        height += scaledUi(8.0f);
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
        {"voiceLeading", voiceLeading},
        {"minNote", minNote},
        {"maxNote", maxNote},
        {"sourceMode", sourceMode},
        {"addBass", addBass},
        {"bassOct", bassOct},
        {"inversion", inversion},
        {"voicingMode", voicingMode},
        {"voicingSpread", voicingSpread},
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
    config.voiceLeading = json.value("voiceLeading", false);
    config.minNote = ofClamp(json.value("minNote", 0), 0, 127);
    config.maxNote = ofClamp(json.value("maxNote", 127), 0, 127);
    if(config.minNote > config.maxNote) std::swap(config.minNote, config.maxNote);
    if(json.contains("sourceMode")) {
        config.sourceMode = ofClamp(json.value("sourceMode", chordSequenceOutputConfig::Chord),
                                    chordSequenceOutputConfig::Chord,
                                    chordSequenceOutputConfig::ChordSum);
    } else if(json.value("scaleOnly", false)) {
        config.sourceMode = chordSequenceOutputConfig::Scale;
    } else if(json.value("keyOnly", false)) {
        config.sourceMode = chordSequenceOutputConfig::Key;
    } else if(json.value("rootOnly", false)) {
        config.sourceMode = chordSequenceOutputConfig::Root;
    } else {
        config.sourceMode = chordSequenceOutputConfig::Chord;
    }
    config.addBass = json.value("addBass", false);
    config.bassOct = json.value("bassOct", -2);
    config.inversion = json.value("inversion", 0);
    config.voicingMode = ofClamp(json.value("voicingMode", chordSequenceOutputConfig::None),
                                 chordSequenceOutputConfig::None,
                                 chordSequenceOutputConfig::Shell);
    config.voicingSpread = std::max(0.0f, json.value("voicingSpread", 0.0f));
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
    json["transposeRandomRange"] = transposeRandomRange;
    json["transposeRandomQuantization"] = transposeRandomQuantization;
    json["transposeRandomStep"] = transposeRandomStep;
    json["inversionRandomRange"] = inversionRandomRange;
    json["inversionRandomQuantization"] = inversionRandomQuantization;
    json["inversionRandomStep"] = inversionRandomStep;
    json["globalPitchBend"] = globalPitchBend;
    json["progressionOrder"] = progressionOrder;
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
    snapshot.transposeRandomRange = std::max(0, json.value("transposeRandomRange", 0));
    snapshot.transposeRandomQuantization = std::max(1, json.value("transposeRandomQuantization", 1));
    snapshot.transposeRandomStep = json.value("transposeRandomStep", false);
    snapshot.inversionRandomRange = std::max(0, json.value("inversionRandomRange", 0));
    snapshot.inversionRandomQuantization = std::max(1, json.value("inversionRandomQuantization", 1));
    snapshot.inversionRandomStep = json.value("inversionRandomStep", false);
    snapshot.globalPitchBend = json.value("globalPitchBend", json.value("pitchBend", 0.0f));
    if(json.contains("progressionOrder")) {
        snapshot.progressionOrder = ofClamp(json.value("progressionOrder", 0), 0, 4);
    } else {
        bool legacyInternalTiming = json.value("internalTimingEnabled", false);
        bool legacyMarkov = json.value("markovEnabled", false);
        snapshot.progressionOrder = legacyInternalTiming
                                  ? (legacyMarkov ? 4 : 1)
                                  : 0;
    }
    snapshot.internalTimingEnabled = snapshot.progressionOrder != 0;
    snapshot.markovEnabled = snapshot.progressionOrder == 4;
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
    addOutputParameter(rootOutput.set("Root", 0.0f, 0.0f, 11.0f));
    addParameter(showEditor.set("Show", false));

    addInspectorParameter(editorWidth.set("Editor Width", 980.0f, 560.0f, 1800.0f));
    addInspectorParameter(editorHeight.set("Editor Height", 980.0f, 420.0f, 1800.0f));

    loadLibraries();
    loadImportSources();
    initializeDefaultProgression();
    ensureOutputCount(numOutputs);
    initializePublishableEditorParameters();
    syncNodeGuiParametersFromState();
    loadAllSnapshotsFromDisk();
    setProgressionOrder(progressionOrder, false);

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

    effectiveGlobalTranspose = globalTranspose;
    effectiveGlobalInvert = globalInvert;
    refreshAllOutputs(true);
}

void chordSequence::setBpm(float bpm) {
    currentBPM = std::max(1.0f, bpm);
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
    const auto frameState = getFrameTransportState();
    currentBPM = std::max(1.0f, frameState.current.bpm);

    if(usesInternalProgressionOrder() && !progression.empty()) {
        if(ofxOceanodeTransportUtils::didTransportDiscontinuity(frameState)) {
            const bool returnedToStart = frameState.current.beatPosition <= transportResetBeatWindow &&
                                         (ofxOceanodeTransportUtils::didGenerationChange(frameState) ||
                                          ofxOceanodeTransportUtils::didBeatRewind(frameState, transportResetBeatWindow));
            resetInternalSequence(true, returnedToStart ? 0.0 : frameState.current.beatPosition);
        } else if(frameState.current.isPlaying) {
            if(nextInternalStepBeat < 0.0) {
                resetInternalSequence(true);
            }
            int safety = 0;
            while(nextInternalStepBeat >= 0.0 &&
                  frameState.current.beatPosition + ofxOceanodeTransportUtils::StepEpsilon >= nextInternalStepBeat &&
                  safety < 128) {
                advanceInternalSequence();
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
    json["publishedEditorParameters"] = publishedEditorParameterKeys;
}

void chordSequence::presetRecallBeforeSettingParameters(ofJson &json) {
    std::vector<std::string> publishedKeys;
    if(json.contains("publishedEditorParameters") && json["publishedEditorParameters"].is_array()) {
        try {
            publishedKeys = json["publishedEditorParameters"].get<std::vector<std::string>>();
        } catch(...) {
        }
    }
    syncPublishedEditorParameters(publishedKeys);
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
    initializePublishableEditorParameters();
    syncPublishedEditorParameters(publishedEditorParameterKeys);
    sanitizeProgression();
    refreshAllOutputs(true);
}

void chordSequence::ensureOutputCount(int newCount) {
    newCount = ofClamp(newCount, 1, MaxOutputs);
    int oldCount = static_cast<int>(outputs.size());
    if(newCount == oldCount) {
        numOutputs = newCount;
        initializePublishableEditorParameters();
        syncNodeGuiParametersFromState();
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
    initializePublishableEditorParameters();
    syncPublishedEditorParameters(publishedEditorParameterKeys);
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

    syncPublishedEditorProxyValuesFromState();
}

void chordSequence::initializePublishableEditorParameters() {
    publishableEditorParameters.clear();

    auto registerNative = [this](const std::string &key, auto &parameter) {
        EditorPublishAction action;
        action.key = key;
        action.publish = [this, key, &parameter]() { return publishEditorParameterToNode(key, parameter); };
        action.unpublish = [this, key]() { return unpublishEditorParameterFromNode(key); };
        action.isPublished = [this, key]() { return isEditorParameterPublished(key); };
        action.isAvailableInNode = [this, &parameter]() { return getParameterGroup().contains(parameter.getEscapedName()); };
        publishableEditorParameters.push_back(std::move(action));
    };

    auto registerExistingOnly = [this](const std::string &key, const std::function<bool()> &availableInNode) {
        EditorPublishAction action;
        action.key = key;
        action.publish = []() { return false; };
        action.unpublish = []() { return false; };
        action.isPublished = []() { return false; };
        action.isAvailableInNode = availableInNode;
        publishableEditorParameters.push_back(std::move(action));
    };

    auto registerIntProxy = [this](const std::string &key,
                                   const std::string &name,
                                   int minValue,
                                   int maxValue,
                                   const std::function<int()> &getter,
                                   const std::function<void(int)> &setter,
                                   const std::vector<std::string> &options = std::vector<std::string>()) {
        EditorPublishAction action;
        action.key = key;
        action.publish = [this, key, name, minValue, maxValue, getter, setter, options]() {
            return publishEditorIntProxyToNode(key, name, minValue, maxValue, getter, setter, options);
        };
        action.unpublish = [this, key]() { return unpublishEditorParameterFromNode(key); };
        action.isPublished = [this, key]() { return isEditorParameterPublished(key); };
        action.isAvailableInNode = []() { return false; };
        action.syncFromState = [this, key, getter]() { syncPublishedEditorIntProxyValue(key, getter()); };
        publishableEditorParameters.push_back(std::move(action));
    };

    auto registerFloatProxy = [this](const std::string &key,
                                     const std::string &name,
                                     float minValue,
                                     float maxValue,
                                     const std::function<float()> &getter,
                                     const std::function<void(float)> &setter) {
        EditorPublishAction action;
        action.key = key;
        action.publish = [this, key, name, minValue, maxValue, getter, setter]() {
            return publishEditorFloatProxyToNode(key, name, minValue, maxValue, getter, setter);
        };
        action.unpublish = [this, key]() { return unpublishEditorParameterFromNode(key); };
        action.isPublished = [this, key]() { return isEditorParameterPublished(key); };
        action.isAvailableInNode = []() { return false; };
        action.syncFromState = [this, key, getter]() { syncPublishedEditorFloatProxyValue(key, getter()); };
        publishableEditorParameters.push_back(std::move(action));
    };

    auto registerBoolProxy = [this](const std::string &key,
                                    const std::string &name,
                                    const std::function<bool()> &getter,
                                    const std::function<void(bool)> &setter) {
        EditorPublishAction action;
        action.key = key;
        action.publish = [this, key, name, getter, setter]() {
            return publishEditorBoolProxyToNode(key, name, getter, setter);
        };
        action.unpublish = [this, key]() { return unpublishEditorParameterFromNode(key); };
        action.isPublished = [this, key]() { return isEditorParameterPublished(key); };
        action.isAvailableInNode = []() { return false; };
        action.syncFromState = [this, key, getter]() { syncPublishedEditorBoolProxyValue(key, getter()); };
        publishableEditorParameters.push_back(std::move(action));
    };

    registerNative("index", indexInput);
    registerNative("numChords", numChordsParameter);
    registerNative("transpose", transposeParameter);
    registerNative("pitchBend", pitchBendParameter);
    registerNative("inversion", inversionParameter);
    registerNative("resetSequence", resetSequenceParameter);

    registerIntProxy("numOutputs", "OutputNum", 1, MaxOutputs,
                     [this]() { return numOutputs; },
                     [this](int value) {
                         ensureOutputCount(ofClamp(value, 1, MaxOutputs));
                         refreshAllOutputs(true);
                     });

    registerIntProxy("progressionOrder", "Progression", InputIdx, Markov,
                     [this]() { return progressionOrder; },
                     [this](int value) { setProgressionOrder(ofClamp(value, InputIdx, Markov), true); },
                     {"Input Idx", "Ascendent", "Descendent", "Random", "Markov"});

    std::vector<std::string> keyOptions(std::begin(keyNames), std::end(keyNames));
    registerIntProxy("globalKey", "Key Root", 0, 11,
                     [this]() { return globalKey; },
                     [this](int value) {
                         globalKey = ofClamp(value, 0, 11);
                         refreshAllOutputs(true);
                     },
                     keyOptions);

    std::vector<std::string> scaleOptions;
    scaleOptions.reserve(scaleLibrary.size());
    for(const auto &item : scaleLibrary) scaleOptions.push_back(item.name);
    registerIntProxy("globalScale", "Key Scale", 0, std::max(0, static_cast<int>(scaleOptions.size()) - 1),
                     [this]() { return getGlobalScaleSafeIndex(); },
                     [this](int value) {
                         if(scaleLibrary.empty()) return;
                         int index = ofClamp(value, 0, static_cast<int>(scaleLibrary.size()) - 1);
                         globalScaleIndex = index;
                         globalScaleName = scaleLibrary[index].name;
                         sanitizeProgression();
                         refreshAllOutputs(true);
                     },
                     scaleOptions);

    registerIntProxy("transposeRandomRange", "Transpose Random Range", 0, 48,
                     [this]() { return transposeRandomRange; },
                     [this](int value) {
                         transposeRandomRange = std::max(0, value);
                         updateEffectiveGlobalModifiers(false, false, true);
                         refreshAllOutputs(true);
                     });
    registerIntProxy("transposeRandomQuantization", "Transpose Random Q", 1, 48,
                     [this]() { return transposeRandomQuantization; },
                     [this](int value) {
                         transposeRandomQuantization = std::max(1, value);
                         updateEffectiveGlobalModifiers(false, false, true);
                         refreshAllOutputs(true);
                     });
    registerBoolProxy("transposeRandomStep", "Transpose Random Step",
                      [this]() { return transposeRandomStep; },
                      [this](bool value) {
                          transposeRandomStep = value;
                          updateEffectiveGlobalModifiers(false, false, true);
                          refreshAllOutputs(true);
                      });
    registerIntProxy("inversionRandomRange", "Inversion Random Range", 0, 24,
                     [this]() { return inversionRandomRange; },
                     [this](int value) {
                         inversionRandomRange = std::max(0, value);
                         updateEffectiveGlobalModifiers(false, false, true);
                         refreshAllOutputs(true);
                     });
    registerIntProxy("inversionRandomQuantization", "Inversion Random Q", 1, 24,
                     [this]() { return inversionRandomQuantization; },
                     [this](int value) {
                         inversionRandomQuantization = std::max(1, value);
                         updateEffectiveGlobalModifiers(false, false, true);
                         refreshAllOutputs(true);
                     });
    registerBoolProxy("inversionRandomStep", "Inversion Random Step",
                      [this]() { return inversionRandomStep; },
                      [this](bool value) {
                          inversionRandomStep = value;
                          updateEffectiveGlobalModifiers(false, false, true);
                          refreshAllOutputs(true);
                      });

    for(int i = 0; i < static_cast<int>(progression.size()); i++) {
        const std::string prefix = stepPublishPrefix(i);
        const std::string labelPrefix = "Step " + ofToString(i + 1) + " ";
        registerIntProxy(prefix + "mode", labelPrefix + "Type", chordSequenceEntry::Chord, chordSequenceEntry::Functional,
                         [this, i]() { return progression[i].mode; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(progression.size())) return;
                             chordSequenceEntry &entry = progression[i];
                             entry.mode = ofClamp(value, chordSequenceEntry::Chord, chordSequenceEntry::Functional);
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
                                 entry.itemIndex = getDefaultIndexForMode(entry.mode);
                                 entry.itemName.clear();
                             }
                             sanitizeEntry(entry);
                             refreshAllOutputs(true);
                         },
                         {"Chord", "Scale", "Cypher", "Degree", "Functional"});

        registerIntProxy(prefix + "functionalGroup", labelPrefix + "Function", 0, 2,
                         [this, i]() { return progression[i].functionalGroup; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(progression.size())) return;
                             chordSequenceEntry &entry = progression[i];
                             entry.functionalGroup = ofClamp(value, 0, 2);
                             entry.functionalVariantIndex = 0;
                             entry.functionalVariantLabel.clear();
                             sanitizeEntry(entry);
                             refreshAllOutputs(true);
                         },
                         {"Tonic", "Subdominant", "Dominant"});

        registerIntProxy(prefix + "degree", labelPrefix + "Degree", 1, 32,
                         [this, i]() { return progression[i].degree; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(progression.size())) return;
                             progression[i].degree = std::max(1, value);
                             sanitizeEntry(progression[i]);
                             refreshAllOutputs(true);
                         });
        registerIntProxy(prefix + "chordSize", labelPrefix + "Chord Size", 1, 16,
                         [this, i]() { return progression[i].chordSize; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(progression.size())) return;
                             progression[i].chordSize = std::max(1, value);
                             sanitizeEntry(progression[i]);
                             refreshAllOutputs(true);
                         });
        registerIntProxy(prefix + "stepInterval", labelPrefix + "Step Interval", 1, 16,
                         [this, i]() { return progression[i].stepInterval; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(progression.size())) return;
                             progression[i].stepInterval = std::max(1, value);
                             sanitizeEntry(progression[i]);
                             refreshAllOutputs(true);
                         });
        registerFloatProxy(prefix + "diatonicDeviationProbability", labelPrefix + "Diat Dev %",
                           0.0f, 100.0f,
                           [this, i]() { return progression[i].diatonicDeviationProbability; },
                           [this, i](float value) {
                               if(i >= static_cast<int>(progression.size())) return;
                               progression[i].diatonicDeviationProbability = ofClamp(value, 0.0f, 100.0f);
                               sanitizeEntry(progression[i]);
                               refreshAllOutputs(true);
                           });
        registerIntProxy(prefix + "diatonicDeviationRange", labelPrefix + "Diat Dev Range", 0, 24,
                         [this, i]() { return progression[i].diatonicDeviationRange; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(progression.size())) return;
                             progression[i].diatonicDeviationRange = std::max(0, value);
                             sanitizeEntry(progression[i]);
                             refreshAllOutputs(true);
                         });
        registerFloatProxy(prefix + "beatDuration", labelPrefix + "Beats",
                           0.001f, 64.0f,
                           [this, i]() { return progression[i].beatDuration; },
                           [this, i](float value) {
                               if(i >= static_cast<int>(progression.size())) return;
                               progression[i].beatDuration = std::max(0.001f, value);
                               outputBuildDirty = true;
                               if(usesInternalProgressionOrder() && !progression.empty()) {
                                   int activeStep = ofClamp(resolveActiveIndex(), 0, static_cast<int>(progression.size()) - 1);
                                   nextInternalStepBeat = getFrameTransportState().current.beatPosition +
                                                          std::max(0.001f, progression[activeStep].beatDuration);
                               }
                           });
        registerIntProxy(prefix + "transpose", labelPrefix + "Transpose", -48, 48,
                         [this, i]() { return progression[i].transpose; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(progression.size())) return;
                             progression[i].transpose = value;
                             refreshAllOutputs(true);
                         });
        registerIntProxy(prefix + "inversion", labelPrefix + "Inversion", -16, 16,
                         [this, i]() { return progression[i].inversion; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(progression.size())) return;
                             progression[i].inversion = value;
                             refreshAllOutputs(true);
                         });
    }

    for(int i = 0; i < static_cast<int>(outputConfigs.size()); i++) {
        const std::string prefix = outputPublishPrefix(i);
        const std::string labelPrefix = "Output " + ofToString(i + 1) + " ";
        registerIntProxy(prefix + "octave", labelPrefix + "Octave", -8, 8,
                         [this, i]() { return outputConfigs[i].octave; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(outputConfigs.size())) return;
                             outputConfigs[i].octave = value;
                             refreshAllOutputs(true);
                         });
        registerIntProxy(prefix + "transpose", labelPrefix + "Transpose", -48, 48,
                         [this, i]() { return outputConfigs[i].transpose; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(outputConfigs.size())) return;
                             outputConfigs[i].transpose = value;
                             refreshAllOutputs(true);
                         });
        registerFloatProxy(prefix + "pitchBend", labelPrefix + "Pitch Bend", -24.0f, 24.0f,
                           [this, i]() { return outputConfigs[i].pitchBend; },
                           [this, i](float value) {
                               if(i >= static_cast<int>(outputConfigs.size())) return;
                               outputConfigs[i].pitchBend = ofClamp(value, -24.0f, 24.0f);
                               refreshAllOutputs(true);
                           });
        registerFloatProxy(prefix + "detune", labelPrefix + "Detune", 0.0f, 2.0f,
                           [this, i]() { return outputConfigs[i].perNoteDetune; },
                           [this, i](float value) {
                               if(i >= static_cast<int>(outputConfigs.size())) return;
                               outputConfigs[i].perNoteDetune = std::max(0.0f, value);
                               refreshAllOutputs(true);
                           });
        registerFloatProxy(prefix + "octRandProbability", labelPrefix + "Oct Rand %", 0.0f, 100.0f,
                           [this, i]() { return outputConfigs[i].octaveRandomProbability; },
                           [this, i](float value) {
                               if(i >= static_cast<int>(outputConfigs.size())) return;
                               outputConfigs[i].octaveRandomProbability = ofClamp(value, 0.0f, 100.0f);
                               refreshAllOutputs(true);
                           });
        registerIntProxy(prefix + "octRandRange", labelPrefix + "Oct Rand Range", 0, 8,
                         [this, i]() { return outputConfigs[i].octaveRandomRange; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(outputConfigs.size())) return;
                             outputConfigs[i].octaveRandomRange = std::max(0, value);
                             refreshAllOutputs(true);
                         });
        registerFloatProxy(prefix + "chromDevProbability", labelPrefix + "Chrom Dev %", 0.0f, 100.0f,
                           [this, i]() { return outputConfigs[i].chromaticDeviationProbability; },
                           [this, i](float value) {
                               if(i >= static_cast<int>(outputConfigs.size())) return;
                               outputConfigs[i].chromaticDeviationProbability = ofClamp(value, 0.0f, 100.0f);
                               refreshAllOutputs(true);
                           });
        registerIntProxy(prefix + "chromDevRange", labelPrefix + "Chrom Dev Range", 0, 12,
                         [this, i]() { return outputConfigs[i].chromaticDeviationRange; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(outputConfigs.size())) return;
                             outputConfigs[i].chromaticDeviationRange = std::max(0, value);
                             refreshAllOutputs(true);
                         });
        registerIntProxy(prefix + "source", labelPrefix + "Source",
                         chordSequenceOutputConfig::Chord, chordSequenceOutputConfig::ChordSum,
                         [this, i]() { return outputConfigs[i].sourceMode; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(outputConfigs.size())) return;
                             outputConfigs[i].sourceMode = ofClamp(value,
                                                                   chordSequenceOutputConfig::Chord,
                                                                   chordSequenceOutputConfig::ChordSum);
                             refreshAllOutputs(true);
                         },
                         {"Chord", "Scale", "Root", "Key", "Chord Sum"});
        registerBoolProxy(prefix + "fold12", labelPrefix + "Fold12",
                          [this, i]() { return outputConfigs[i].fold12; },
                          [this, i](bool value) {
                              if(i >= static_cast<int>(outputConfigs.size())) return;
                              outputConfigs[i].fold12 = value;
                              refreshAllOutputs(true);
                          });
        registerBoolProxy(prefix + "addBass", labelPrefix + "AddBass",
                          [this, i]() { return outputConfigs[i].addBass; },
                          [this, i](bool value) {
                              if(i >= static_cast<int>(outputConfigs.size())) return;
                              outputConfigs[i].addBass = value;
                              refreshAllOutputs(true);
                          });
        registerIntProxy(prefix + "inversion", labelPrefix + "Inversion", -16, 16,
                         [this, i]() { return outputConfigs[i].inversion; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(outputConfigs.size())) return;
                             outputConfigs[i].inversion = value;
                             refreshAllOutputs(true);
                         });
        registerIntProxy(prefix + "voicing", labelPrefix + "Voicing",
                         chordSequenceOutputConfig::None, chordSequenceOutputConfig::Shell,
                         [this, i]() { return outputConfigs[i].voicingMode; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(outputConfigs.size())) return;
                             outputConfigs[i].voicingMode = ofClamp(value,
                                                                    chordSequenceOutputConfig::None,
                                                                    chordSequenceOutputConfig::Shell);
                             refreshAllOutputs(true);
                         },
                         {"None", "Close", "Open", "Drop 2", "Drop 3", "Shell"});
        registerFloatProxy(prefix + "spread", labelPrefix + "Spread", 0.0f, 24.0f,
                           [this, i]() { return outputConfigs[i].voicingSpread; },
                           [this, i](float value) {
                               if(i >= static_cast<int>(outputConfigs.size())) return;
                               outputConfigs[i].voicingSpread = std::max(0.0f, value);
                               refreshAllOutputs(true);
                           });
        registerIntProxy(prefix + "bassOct", labelPrefix + "BassOct", -8, 8,
                         [this, i]() { return outputConfigs[i].bassOct; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(outputConfigs.size())) return;
                             outputConfigs[i].bassOct = value;
                             refreshAllOutputs(true);
                         });
        registerFloatProxy(prefix + "glide", labelPrefix + "Glide", 0.0f, 10000.0f,
                           [this, i]() { return outputConfigs[i].glideMs; },
                           [this, i](float value) {
                               if(i >= static_cast<int>(outputConfigs.size())) return;
                               outputConfigs[i].glideMs = std::max(0.0f, value);
                           });
        registerBoolProxy(prefix + "voiceLeading", labelPrefix + "Voice Lead",
                          [this, i]() { return outputConfigs[i].voiceLeading; },
                          [this, i](bool value) {
                              if(i >= static_cast<int>(outputConfigs.size())) return;
                              outputConfigs[i].voiceLeading = value;
                              refreshAllOutputs(true);
                          });
        registerIntProxy(prefix + "minNote", labelPrefix + "Min Note", 0, 127,
                         [this, i]() { return outputConfigs[i].minNote; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(outputConfigs.size())) return;
                             outputConfigs[i].minNote = ofClamp(value, 0, 127);
                             if(outputConfigs[i].minNote > outputConfigs[i].maxNote) outputConfigs[i].maxNote = outputConfigs[i].minNote;
                             refreshAllOutputs(true);
                         });
        registerIntProxy(prefix + "maxNote", labelPrefix + "Max Note", 0, 127,
                         [this, i]() { return outputConfigs[i].maxNote; },
                         [this, i](int value) {
                             if(i >= static_cast<int>(outputConfigs.size())) return;
                             outputConfigs[i].maxNote = ofClamp(value, 0, 127);
                             if(outputConfigs[i].maxNote < outputConfigs[i].minNote) outputConfigs[i].minNote = outputConfigs[i].maxNote;
                             refreshAllOutputs(true);
                         });
        registerBoolProxy(prefix + "expand", labelPrefix + "Expand",
                          [this, i]() { return outputConfigs[i].expandOutput; },
                          [this, i](bool value) {
                              if(i >= static_cast<int>(outputConfigs.size())) return;
                              outputConfigs[i].expandOutput = value;
                              refreshAllOutputs(true);
                          });
        registerBoolProxy(prefix + "sort", labelPrefix + "Sort",
                          [this, i]() { return outputConfigs[i].sortOutput; },
                          [this, i](bool value) {
                              if(i >= static_cast<int>(outputConfigs.size())) return;
                              outputConfigs[i].sortOutput = value;
                              refreshAllOutputs(true);
                          });

        registerExistingOnly(prefix + "outputSize", [this, i]() {
            return i < static_cast<int>(outputSizeParameters.size()) &&
                   outputSizeParameters[i] &&
                   getParameterGroup().contains(outputSizeParameters[i]->getEscapedName());
        });
    }
}

void chordSequence::syncPublishedEditorProxyValuesFromState() {
    for(const auto &action : publishableEditorParameters) {
        if(action.syncFromState && action.isPublished && action.isPublished()) {
            action.syncFromState();
        }
    }
}

std::string chordSequence::stepPublishPrefix(int index) const {
    return "step" + ofToString(index + 1) + ".";
}

std::string chordSequence::outputPublishPrefix(int index) const {
    return "output" + ofToString(index + 1) + ".";
}

bool chordSequence::isEditorParameterPublished(const std::string &key) const {
    return publishedEditorParameterHandles.find(key) != publishedEditorParameterHandles.end();
}

const chordSequence::EditorPublishAction *chordSequence::findPublishableEditorParameter(const std::string &key) const {
    auto it = std::find_if(publishableEditorParameters.begin(), publishableEditorParameters.end(),
                           [&](const EditorPublishAction &action) { return action.key == key; });
    return it == publishableEditorParameters.end() ? nullptr : &(*it);
}

bool chordSequence::publishEditorIntProxyToNode(const std::string &key,
                                                const std::string &name,
                                                int minValue,
                                                int maxValue,
                                                const std::function<int()> &getter,
                                                const std::function<void(int)> &setter,
                                                const std::vector<std::string> &options,
                                                ofxOceanodeParameterFlags flags) {
    if(isEditorParameterPublished(key)) return false;
    if(!publishedEditorSeparatorAdded) {
        addSeparator("Published", ofColor(200));
        publishedEditorSeparatorAdded = true;
    }

    auto parameter = std::make_shared<ofParameter<int>>();
    int initialValue = getter();
    if(maxValue < minValue) maxValue = minValue;
    parameter->set(name, ofClamp(initialValue, minValue, maxValue), minValue, maxValue);

    auto published = addParameter(parameter, flags);
    if(!options.empty()) published->setDropdownOptions(options);
    publishedEditorParameterHandles[key] = published;
    publishedEditorIntProxyParameters[key] = parameter;
    publishedEditorProxyListeners[key] = std::make_unique<ofEventListener>(parameter->newListener([this, key, setter](int &value) {
        if(publishedEditorProxySyncKeys.count(key) > 0) return;
        setter(value);
        syncPublishedEditorProxyValuesFromState();
    }));

    if(std::find(publishedEditorParameterKeys.begin(), publishedEditorParameterKeys.end(), key) == publishedEditorParameterKeys.end()) {
        publishedEditorParameterKeys.push_back(key);
    }
    return true;
}

bool chordSequence::publishEditorFloatProxyToNode(const std::string &key,
                                                  const std::string &name,
                                                  float minValue,
                                                  float maxValue,
                                                  const std::function<float()> &getter,
                                                  const std::function<void(float)> &setter,
                                                  ofxOceanodeParameterFlags flags) {
    if(isEditorParameterPublished(key)) return false;
    if(!publishedEditorSeparatorAdded) {
        addSeparator("Published", ofColor(200));
        publishedEditorSeparatorAdded = true;
    }

    auto parameter = std::make_shared<ofParameter<float>>();
    float initialValue = getter();
    if(maxValue < minValue) maxValue = minValue;
    parameter->set(name, ofClamp(initialValue, minValue, maxValue), minValue, maxValue);

    publishedEditorParameterHandles[key] = addParameter(parameter, flags);
    publishedEditorFloatProxyParameters[key] = parameter;
    publishedEditorProxyListeners[key] = std::make_unique<ofEventListener>(parameter->newListener([this, key, setter](float &value) {
        if(publishedEditorProxySyncKeys.count(key) > 0) return;
        setter(value);
        syncPublishedEditorProxyValuesFromState();
    }));

    if(std::find(publishedEditorParameterKeys.begin(), publishedEditorParameterKeys.end(), key) == publishedEditorParameterKeys.end()) {
        publishedEditorParameterKeys.push_back(key);
    }
    return true;
}

bool chordSequence::publishEditorBoolProxyToNode(const std::string &key,
                                                 const std::string &name,
                                                 const std::function<bool()> &getter,
                                                 const std::function<void(bool)> &setter,
                                                 ofxOceanodeParameterFlags flags) {
    if(isEditorParameterPublished(key)) return false;
    if(!publishedEditorSeparatorAdded) {
        addSeparator("Published", ofColor(200));
        publishedEditorSeparatorAdded = true;
    }

    auto parameter = std::make_shared<ofParameter<bool>>();
    parameter->set(name, getter());

    publishedEditorParameterHandles[key] = addParameter(parameter, flags);
    publishedEditorBoolProxyParameters[key] = parameter;
    publishedEditorProxyListeners[key] = std::make_unique<ofEventListener>(parameter->newListener([this, key, setter](bool &value) {
        if(publishedEditorProxySyncKeys.count(key) > 0) return;
        setter(value);
        syncPublishedEditorProxyValuesFromState();
    }));

    if(std::find(publishedEditorParameterKeys.begin(), publishedEditorParameterKeys.end(), key) == publishedEditorParameterKeys.end()) {
        publishedEditorParameterKeys.push_back(key);
    }
    return true;
}

void chordSequence::syncPublishedEditorIntProxyValue(const std::string &key, int value) {
    auto it = publishedEditorIntProxyParameters.find(key);
    if(it == publishedEditorIntProxyParameters.end() || !it->second) return;
    if(it->second->get() == value) return;
    publishedEditorProxySyncKeys.insert(key);
    it->second->setWithoutEventNotifications(value);
    publishedEditorProxySyncKeys.erase(key);
}

void chordSequence::syncPublishedEditorFloatProxyValue(const std::string &key, float value) {
    auto it = publishedEditorFloatProxyParameters.find(key);
    if(it == publishedEditorFloatProxyParameters.end() || !it->second) return;
    if(std::abs(it->second->get() - value) < 0.0001f) return;
    publishedEditorProxySyncKeys.insert(key);
    it->second->setWithoutEventNotifications(value);
    publishedEditorProxySyncKeys.erase(key);
}

void chordSequence::syncPublishedEditorBoolProxyValue(const std::string &key, bool value) {
    auto it = publishedEditorBoolProxyParameters.find(key);
    if(it == publishedEditorBoolProxyParameters.end() || !it->second) return;
    if(it->second->get() == value) return;
    publishedEditorProxySyncKeys.insert(key);
    it->second->setWithoutEventNotifications(value);
    publishedEditorProxySyncKeys.erase(key);
}

bool chordSequence::publishEditorParameterToNode(const std::string &key) {
    const EditorPublishAction *action = findPublishableEditorParameter(key);
    if(action == nullptr) return false;
    bool changed = action->publish();
    if(changed) parameterGroupChanged.notify(this);
    return changed;
}

bool chordSequence::unpublishEditorParameterFromNode(const std::string &key) {
    auto it = publishedEditorParameterHandles.find(key);
    if(it == publishedEditorParameterHandles.end()) return false;

    removeParameter(it->second->getEscapedName());
    publishedEditorParameterHandles.erase(it);
    publishedEditorIntProxyParameters.erase(key);
    publishedEditorFloatProxyParameters.erase(key);
    publishedEditorBoolProxyParameters.erase(key);
    publishedEditorProxyListeners.erase(key);
    publishedEditorProxySyncKeys.erase(key);
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

void chordSequence::syncPublishedEditorParameters(const std::vector<std::string> &keys) {
    std::vector<std::string> uniqueKeys;
    uniqueKeys.reserve(keys.size());
    for(const std::string &key : keys) {
        if(findPublishableEditorParameter(key) == nullptr) continue;
        if(std::find(uniqueKeys.begin(), uniqueKeys.end(), key) == uniqueKeys.end()) uniqueKeys.push_back(key);
    }

    bool changed = false;
    for(auto &entry : publishedEditorParameterHandles) {
        removeParameter(entry.second->getEscapedName());
        changed = true;
    }
    publishedEditorParameterHandles.clear();
    publishedEditorIntProxyParameters.clear();
    publishedEditorFloatProxyParameters.clear();
    publishedEditorBoolProxyParameters.clear();
    publishedEditorProxyListeners.clear();
    publishedEditorProxySyncKeys.clear();

    if(publishedEditorSeparatorAdded) {
        removeSeparator("Published");
        publishedEditorSeparatorAdded = false;
        changed = true;
    }

    publishedEditorParameterKeys.clear();
    for(const std::string &key : uniqueKeys) {
        const EditorPublishAction *action = findPublishableEditorParameter(key);
        if(action != nullptr) changed = action->publish() || changed;
    }

    if(publishedEditorParameterHandles.empty()) {
        publishedEditorSeparatorAdded = false;
    }

    syncPublishedEditorProxyValuesFromState();
    if(changed) parameterGroupChanged.notify(this);
}

void chordSequence::drawPublishedLabelUnderline(const std::string &key,
                                                const char *label,
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

void chordSequence::drawPublishedCurrentItemUnderline(const std::string &key) const {
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

void chordSequence::drawNodePublishMenuItems(const std::string &key) {
    const EditorPublishAction *action = findPublishableEditorParameter(key);
    if(action == nullptr) {
        ImGui::TextDisabled("Not publishable");
    } else if(action->isPublished()) {
        ImGui::TextDisabled("Published in node");
        if(ImGui::MenuItem("Unpublish from Node")) {
            unpublishEditorParameterFromNode(key);
        }
    } else if(action->isAvailableInNode && action->isAvailableInNode()) {
        ImGui::TextDisabled("Already in node");
    } else {
        if(ImGui::MenuItem("Publish to Node")) {
            publishEditorParameterToNode(key);
        }
    }
}

void chordSequence::drawNodePublishContextMenu(const std::string &key,
                                               const char *label,
                                               float frameWidth,
                                               bool checkbox) {
    drawPublishedLabelUnderline(key, label, frameWidth, checkbox);
    if(!ImGui::BeginPopupContextItem(("##PublishToNode_" + key).c_str())) return;
    drawNodePublishMenuItems(key);
    ImGui::EndPopup();
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
    initializePublishableEditorParameters();
    syncPublishedEditorParameters(publishedEditorParameterKeys);
    if(numChordsParameter.get() != newSize) numChordsParameter = newSize;
    if(usesInternalProgressionOrder()) {
        internalActiveStep = ofClamp(internalActiveStep, 0, std::max(0, newSize - 1));
        nextInternalStepBeat = getFrameTransportState().current.beatPosition +
                               std::max(0.001f, progression[internalActiveStep].beatDuration);
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

std::vector<float> chordSequence::buildChordSumValues() const {
    std::set<int> distinctPitchClasses;
    for(const auto &progressionEntry : progression) {
        std::vector<float> entryValues = buildEntryPreviewOutput(progressionEntry);
        for(float value : entryValues) {
            int pitchClass = wrapIndex(static_cast<int>(std::round(value)), 12);
            if(pitchClass >= 0) distinctPitchClasses.insert(pitchClass);
        }
    }

    if(distinctPitchClasses.empty()) {
        return {static_cast<float>(globalKey)};
    }

    std::vector<float> values;
    values.reserve(distinctPitchClasses.size());
    for(int pitchClass : distinctPitchClasses) {
        values.push_back(static_cast<float>(pitchClass));
    }
    return values;
}

std::vector<float> chordSequence::buildOutputSourceValues(const chordSequenceEntry &entry,
                                                          const chordSequenceOutputConfig &config,
                                                          float &outputRoot) const {
    if(config.sourceMode == chordSequenceOutputConfig::Scale) {
        outputRoot = static_cast<float>(globalKey);
        int safeScaleIndex = getGlobalScaleSafeIndex();
        if(safeScaleIndex >= 0 && safeScaleIndex < static_cast<int>(scaleLibrary.size())) {
            std::vector<float> values = scaleLibrary[safeScaleIndex].values;
            for(auto &value : values) {
                value += static_cast<float>(globalKey);
            }
            return values;
        }
        return std::vector<float>{outputRoot};
    }

    if(config.sourceMode == chordSequenceOutputConfig::ChordSum) {
        outputRoot = static_cast<float>(globalKey);
        return buildChordSumValues();
    }

    if(config.sourceMode == chordSequenceOutputConfig::Key) {
        outputRoot = static_cast<float>(globalKey);
        return std::vector<float>{outputRoot};
    }

    if(config.sourceMode == chordSequenceOutputConfig::Root) {
        outputRoot = getEntryRootValue(entry);
        return std::vector<float>{outputRoot};
    }

    outputRoot = getEntryRootValue(entry);
    std::vector<float> values = buildEntryPreviewOutput(entry);
    return applyDiatonicDeviation(values, entry);
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

float chordSequence::getEntryDisplayRootPitchClass(const chordSequenceEntry &entry) const {
    float rootPitchClass = std::fmod(getEntryRootValue(entry) + static_cast<float>(effectiveGlobalTranspose), 12.0f);
    if(rootPitchClass < 0.0f) rootPitchClass += 12.0f;
    return rootPitchClass;
}

std::string chordSequence::getEntryDisplayRootLabel(const chordSequenceEntry &entry) const {
    int pitchClass = ofClamp(static_cast<int>(std::round(getEntryDisplayRootPitchClass(entry))), 0, 11);
    return keyNames[pitchClass];
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

std::vector<float> chordSequence::applyVoiceLeading(const std::vector<float> &previousValues,
                                                    const std::vector<float> &nextValues,
                                                    int minNote,
                                                    int maxNote) const {
    if(previousValues.empty() || nextValues.empty()) return nextValues;

    minNote = ofClamp(minNote, 0, 127);
    maxNote = ofClamp(maxNote, 0, 127);
    if(minNote > maxNote) std::swap(minNote, maxNote);

    std::vector<float> voiced = nextValues;
    for(size_t i = 0; i < voiced.size(); i++) {
        float reference = sampleVector(previousValues, i);
        float bestValue = voiced[i];
        float bestDistance = std::numeric_limits<float>::max();

        // Find the octave displacement that keeps each note closest to the
        // previous emitted output while respecting the configured register.
        for(int octaveShift = -8; octaveShift <= 8; octaveShift++) {
            float candidate = voiced[i] + static_cast<float>(octaveShift * 12);
            if(candidate < minNote || candidate > maxNote) continue;

            float distance = std::abs(candidate - reference);
            if(distance < bestDistance) {
                bestDistance = distance;
                bestValue = candidate;
            }
        }

        if(bestDistance == std::numeric_limits<float>::max()) {
            for(int octaveShift = -8; octaveShift <= 8; octaveShift++) {
                float candidate = voiced[i] + static_cast<float>(octaveShift * 12);
                float distance = std::abs(candidate - reference);
                if(distance < bestDistance) {
                    bestDistance = distance;
                    bestValue = candidate;
                }
            }
        }

        voiced[i] = bestValue;
    }

    return voiced;
}

std::vector<float> chordSequence::applyRangeConstraints(const std::vector<float> &values,
                                                        int minNote,
                                                        int maxNote) const {
    if(values.empty()) return values;

    minNote = ofClamp(minNote, 0, 127);
    maxNote = ofClamp(maxNote, 0, 127);
    if(minNote > maxNote) std::swap(minNote, maxNote);

    std::vector<float> constrained = values;
    for(auto &value : constrained) {
        float bestValue = value;
        float bestDistance = std::numeric_limits<float>::max();

        // Prefer octave-equivalent placements inside the range before falling
        // back to a direct clamp when no octave placement can fit.
        for(int octaveShift = -8; octaveShift <= 8; octaveShift++) {
            float candidate = value + static_cast<float>(octaveShift * 12);
            if(candidate < minNote || candidate > maxNote) continue;

            float distance = std::abs(candidate - value);
            if(distance < bestDistance) {
                bestDistance = distance;
                bestValue = candidate;
            }
        }

        if(bestDistance == std::numeric_limits<float>::max()) {
            bestValue = ofClamp(value, static_cast<float>(minNote), static_cast<float>(maxNote));
        }

        value = bestValue;
    }

    return constrained;
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
                                                    const chordSequenceOutputConfig &config,
                                                    const std::vector<float> &previousValues) const {
    float outputRoot = 0.0f;
    std::vector<float> values = buildOutputSourceValues(entry, config, outputRoot);

    // Output processing intentionally flows from harmonic source -> global
    // transpose/fold -> voicing/size -> stochastic color -> absolute register
    // -> optional continuity/range cleanup.
    if(!outputSourceUsesScaleLikeMaterial(config.sourceMode)) {
        values = applyInversion(values, effectiveGlobalInvert + config.inversion);
    }

    if(std::abs(effectiveGlobalTranspose) > 0) {
        for(auto &value : values) {
            value += static_cast<float>(effectiveGlobalTranspose);
        }
        outputRoot += static_cast<float>(effectiveGlobalTranspose);
    }

    if(config.fold12) {
        for(auto &value : values) {
            value = std::fmod(value, 12.0f);
            if(value < 0.0f) value += 12.0f;
        }
    }

    if(!outputSourceUsesScaleLikeMaterial(config.sourceMode)) {
        values = applyVoicing(values, config.voicingMode);
        values = applyVoicingSpread(values, config.voicingSpread);
    }

    int requestedBodySize = std::max(0, config.outputSize - ((!outputSourceUsesScaleLikeMaterial(config.sourceMode) && config.addBass) ? 1 : 0));
    values = requestedBodySize == 0
                 ? std::vector<float>{}
                 : adaptOutputSize(values, requestedBodySize, config.expandOutput, false);

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

    if(!outputSourceUsesScaleLikeMaterial(config.sourceMode) && config.addBass) {
        float rootPitchClass = std::fmod(outputRoot, 12.0f);
        if(rootPitchClass < 0.0f) rootPitchClass += 12.0f;
        values.insert(values.begin(), rootPitchClass + static_cast<float>(config.bassOct * 12));
    }

    float pitchOffset = globalPitchBend + config.pitchBend;
    float noteOffset = static_cast<float>(config.transpose + config.octave * 12);
    for(auto &value : values) {
        value += noteOffset + pitchOffset;
        if(config.perNoteDetune > 0.0f) {
            value += ofRandom(-config.perNoteDetune, config.perNoteDetune);
        }
    }

    if(config.voiceLeading) {
        values = applyVoiceLeading(previousValues, values, config.minNote, config.maxNote);
    }
    values = applyRangeConstraints(values, config.minNote, config.maxNote);

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

bool chordSequence::usesInternalProgressionOrder() const {
    return progressionOrder != InputIdx;
}

void chordSequence::setProgressionOrder(int order, bool refreshSequence) {
    int clampedOrder = ofClamp(order, InputIdx, Markov);
    int activeIndex = resolveActiveIndex();
    progressionOrder = clampedOrder;
    internalTimingEnabled = usesInternalProgressionOrder();
    markovEnabled = progressionOrder == Markov;

    if(!progression.empty()) {
        internalActiveStep = ofClamp(activeIndex, 0, static_cast<int>(progression.size()) - 1);
    } else {
        internalActiveStep = 0;
    }

    if(!internalTimingEnabled) {
        nextInternalStepBeat = -1.0;
        if(indexInput.get() != internalActiveStep) {
            indexInput = internalActiveStep;
        }
    } else {
        const auto transportState = getFrameTransportState().current;
        if(!progression.empty()) {
            nextInternalStepBeat = transportState.beatPosition + std::max(0.001f, progression[internalActiveStep].beatDuration);
        } else {
            nextInternalStepBeat = -1.0;
        }
    }

    outputBuildDirty = true;
    lastRefreshedActiveIndex = -1;
    if(refreshSequence) {
        refreshAllOutputs(true);
    }
}

int chordSequence::resolveActiveIndex() const {
    if(usesInternalProgressionOrder()) {
        return wrapIndex(internalActiveStep, static_cast<int>(progression.size()));
    }
    return wrapIndex(indexInput.get(), static_cast<int>(progression.size()));
}

float chordSequence::getStepDurationMs(int stepIndex) const {
    if(stepIndex < 0 || stepIndex >= static_cast<int>(progression.size())) return 0.0f;
    float bpm = std::max(1.0f, currentBPM);
    return progression[stepIndex].beatDuration * (60000.0f / bpm);
}

void chordSequence::resetInternalSequence(bool forceInstant, double anchorBeat) {
    sanitizeProgression();
    pendingRandomationSequenceRestart = true;
    pendingRandomationStepAdvance = true;
    internalActiveStep = 0;
    if(usesInternalProgressionOrder() && !progression.empty()) {
        internalActiveStep = ofClamp(internalActiveStep, 0, static_cast<int>(progression.size()) - 1);
        const double scheduleBeat = anchorBeat >= 0.0 ? anchorBeat : getFrameTransportState().current.beatPosition;
        nextInternalStepBeat = scheduleBeat + std::max(0.001f, progression[internalActiveStep].beatDuration);
    } else {
        nextInternalStepBeat = -1.0;
    }
    outputBuildDirty = true;
    lastRefreshedActiveIndex = -1;
    refreshAllOutputs(forceInstant);
}

int chordSequence::chooseNextInternalStep(int currentStep) {
    int sequenceSize = static_cast<int>(progression.size());
    if(sequenceSize <= 0) return 0;
    if(currentStep < 0 || currentStep >= sequenceSize) {
        return (currentStep + 1) % sequenceSize;
    }

    if(progressionOrder == Ascendent) {
        return (currentStep + 1) % sequenceSize;
    }
    if(progressionOrder == Descendent) {
        return wrapIndex(currentStep - 1, sequenceSize);
    }
    if(progressionOrder == Random) {
        return std::uniform_int_distribution<int>(0, sequenceSize - 1)(randomEngine);
    }
    if(progressionOrder != Markov) {
        return currentStep;
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
        nextInternalStepBeat = -1.0;
        return;
    }

    int previousStep = internalActiveStep;
    double currentBoundaryBeat = nextInternalStepBeat;
    if(currentBoundaryBeat < 0.0) {
        currentBoundaryBeat = getFrameTransportState().current.beatPosition;
    }
    internalActiveStep = chooseNextInternalStep(internalActiveStep);
    pendingRandomationStepAdvance = true;
    pendingRandomationSequenceRestart = (progressionOrder == Ascendent) &&
                                        (progression.size() == 1 ||
                                         (previousStep == static_cast<int>(progression.size()) - 1 && internalActiveStep == 0));
    outputBuildDirty = true;
    refreshAllOutputs(false);
    nextInternalStepBeat = currentBoundaryBeat + std::max(0.001f, progression[internalActiveStep].beatDuration);
}

int chordSequence::generateRandomizedModifier(int range, int quantization) {
    range = std::max(0, range);
    quantization = std::max(1, quantization);
    if(range <= 0) return 0;

    int maxStep = range / quantization;
    if(maxStep <= 0) return 0;
    return std::uniform_int_distribution<int>(0, maxStep)(randomEngine) * quantization;
}

void chordSequence::updateEffectiveGlobalModifiers(bool sequenceRestart, bool stepAdvance, bool forceReroll) {
    bool rerollTranspose = forceReroll || (transposeRandomStep ? stepAdvance : sequenceRestart);
    bool rerollInvert = forceReroll || (inversionRandomStep ? stepAdvance : sequenceRestart);

    if(rerollTranspose) {
        currentTransposeRandomOffset = generateRandomizedModifier(transposeRandomRange, transposeRandomQuantization);
    }
    if(rerollInvert) {
        currentInversionRandomOffset = generateRandomizedModifier(inversionRandomRange, inversionRandomQuantization);
    }

    effectiveGlobalTranspose = globalTranspose + currentTransposeRandomOffset;
    effectiveGlobalInvert = globalInvert + currentInversionRandomOffset;
}

void chordSequence::refreshAllOutputs(bool forceInstant) {
    sanitizeProgression();
    int activeIndex = resolveActiveIndex();
    bool stepAdvance = pendingRandomationStepAdvance || lastRefreshedActiveIndex < 0 || activeIndex != lastRefreshedActiveIndex;
    bool sequenceRestart = pendingRandomationSequenceRestart ||
                           lastRefreshedActiveIndex < 0 ||
                           (activeIndex == 0 && activeIndex != lastRefreshedActiveIndex);
    updateEffectiveGlobalModifiers(sequenceRestart, stepAdvance, false);
    pendingRandomationStepAdvance = false;
    pendingRandomationSequenceRestart = false;

    if(activeIndex < 0 || activeIndex >= static_cast<int>(progression.size())) {
        rootOutput = 0.0f;
    } else {
        rootOutput = getEntryDisplayRootPitchClass(progression[activeIndex]);
    }

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
            std::vector<float> previousValues =
                (forceInstant || lastRefreshedActiveIndex < 0) ? std::vector<float>{} : targetOutputs[i];
            beginGlideTo(i,
                         buildOutputValues(progression[activeIndex], outputConfigs[i], previousValues),
                         forceInstant);
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
    syncPublishedEditorProxyValuesFromState();

    ImVec2 availableRegion = ImGui::GetContentRegionAvail();
    float baseWidth = 1320.0f;
    float widthScale = availableRegion.x > 1.0f ? availableRegion.x / baseWidth : 1.0f;
    editorZoom = ofClamp(widthScale * manualEditorZoom, 0.42f, 2.0f);
    editorFontZoom = ofClamp(editorZoom + 0.08f, 0.50f, 2.0f);
    chordSequenceLayoutZoom = editorZoom;
    chordSequenceFontZoom = editorFontZoom;

    ImGui::SetWindowFontScale(editorFontZoom);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(scaledUi(8.0f), scaledUi(4.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(scaledUi(4.0f), scaledUi(3.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(scaledUi(8.0f), scaledUi(8.0f)));

    float toolbarRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    float buttonWidth = scaledUi(28.0f);
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    std::string zoomLabel = "Zoom " + ofToString(static_cast<int>(std::round(editorZoom * 100.0f))) + "%";
    float labelWidth = ImGui::CalcTextSize(zoomLabel.c_str()).x;
    float toolbarWidth = buttonWidth * 2.0f + labelWidth + spacing * 2.0f;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), toolbarRight - toolbarWidth));
    if(ImGui::Button("-##ChordSequenceZoom", ImVec2(buttonWidth, 0.0f))) {
        manualEditorZoom = std::max(0.5f, manualEditorZoom - 0.1f);
    }
    ImGui::SameLine(0.0f, spacing);
    ImGui::TextUnformatted(zoomLabel.c_str());
    ImGui::SameLine(0.0f, spacing);
    if(ImGui::Button("+##ChordSequenceZoom", ImVec2(buttonWidth, 0.0f))) {
        manualEditorZoom = std::min(2.0f, manualEditorZoom + 0.1f);
    }
    ImGui::Spacing();

    float gap = scaledUi(6.0f);
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float snapshotsWidth = std::max(scaledUi(280.0f), availableWidth * 0.20f);
    float globalWidth = std::max(scaledUi(320.0f), availableWidth * 0.23f);
    float randomationWidth = std::max(scaledUi(280.0f), availableWidth * 0.22f);
    float cypherWidth = std::max(scaledUi(240.0f), availableWidth - snapshotsWidth - globalWidth - randomationWidth - gap * 3.0f);
    bool hasActiveSnapshot = activeSnapshotSlot >= 0 &&
                             activeSnapshotSlot < SnapshotSlots &&
                             snapshotSlots[activeSnapshotSlot].hasData;
    float snapshotsHeight = getSnapshotsSectionHeight(snapshotsWidth, hasActiveSnapshot);
    float globalHeight = getGlobalSectionHeight();
    float randomationHeight = getRandomationSectionHeight();
    float cypherHeight = getCypherSectionHeight(!importedProgressionNames.empty(), !jazzStandardNames.empty());

    float maxStepCardHeight = 0.0f;
    for(const auto &entry : progression) {
        maxStepCardHeight = std::max(maxStepCardHeight, getChordSequenceEntryCardHeight(entry.mode, markovEnabled));
    }
    float stepsSectionHeight = getSectionHeaderHeight() + maxStepCardHeight + scaledUi(10.0f);
    float maxOutputCardHeight = 0.0f;
    for(const auto &config : outputConfigs) {
        maxOutputCardHeight = std::max(maxOutputCardHeight, getOutputCardHeight(config));
    }
    if(maxOutputCardHeight <= 0.0f) {
        chordSequenceOutputConfig defaultConfig;
        maxOutputCardHeight = getOutputCardHeight(defaultConfig);
    }
    float outputsSectionHeight = getSectionHeaderHeight() + maxOutputCardHeight + scaledUi(10.0f);

    beginColoredSection("SnapshotsSection",
                        "Snapshots",
                        ImVec2(snapshotsWidth, snapshotsHeight),
                        snapshotsBg,
                        snapshotsTitle,
                        snapshotsSectionExpanded,
                        editorFontZoom,
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
                        editorFontZoom,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(globalSectionExpanded) drawGlobalControls();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);

    beginColoredSection("RandomationSection",
                        "Randomation",
                        ImVec2(randomationWidth, randomationHeight),
                        randomationBg,
                        randomationTitle,
                        randomationSectionExpanded,
                        editorFontZoom,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(randomationSectionExpanded) drawRandomationControls();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);

    beginColoredSection("CypherSection",
                        "Cyphered Progressions",
                        ImVec2(cypherWidth, cypherHeight),
                        cypherBg,
                        cypherTitle,
                        cypherSectionExpanded,
                        editorFontZoom,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(cypherSectionExpanded) drawImportTools();
    ImGui::EndChild();

    beginColoredSection("StepsSection",
                        "Step Chords",
                        ImVec2(0, stepsSectionHeight),
                        stepsBg,
                        stepsTitle,
                        stepsSectionExpanded,
                        editorFontZoom,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(stepsSectionExpanded) drawEntries();
    ImGui::EndChild();

    beginColoredSection("OutputsSection",
                        "Outputs",
                        ImVec2(0, outputsSectionHeight),
                        outputsBg,
                        outputsTitle,
                        outputsSectionExpanded,
                        editorFontZoom,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(outputsSectionExpanded) drawOutputs();
    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::SetWindowFontScale(1.0f);
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
        if(ImGui::InputInt("ChordNum", &numChords)) {
            numChordsParameter = ofClamp(numChords, numChordsParameter.getMin(), numChordsParameter.getMax());
        }
        drawNodePublishContextMenu("numChords");
        drawPublishedCurrentItemUnderline("numChords");

        ImGui::TableSetColumnIndex(1);
        if(ImGui::InputInt("Index", &displayIndex)) {
            int clampedIndex = ofClamp(displayIndex, 0, std::max(0, static_cast<int>(progression.size()) - 1));
            if(usesInternalProgressionOrder()) {
                internalActiveStep = clampedIndex;
                nextInternalStepBeat = getFrameTransportState().current.beatPosition +
                                       std::max(0.001f, progression[internalActiveStep].beatDuration);
                outputBuildDirty = true;
                lastRefreshedActiveIndex = -1;
                refreshAllOutputs(true);
            } else {
                indexInput = clampedIndex;
            }
        }
        drawNodePublishContextMenu("index");
        drawPublishedCurrentItemUnderline("index");

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(ImGui::InputInt("OutputNum", &requestedOutputs)) {
            ensureOutputCount(requestedOutputs);
            refreshAllOutputs(true);
        }
        drawNodePublishContextMenu("numOutputs");
        drawPublishedCurrentItemUnderline("numOutputs");

        ImGui::TableSetColumnIndex(1);
        int orderValue = progressionOrder;
        int safeOrderValue = std::max(static_cast<int>(InputIdx), std::min(orderValue, static_cast<int>(Markov)));
        const char *orderLabel = progressionOrderLabels[safeOrderValue];
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::BeginCombo("Progression", orderLabel)) {
            for(int i = InputIdx; i <= Markov; i++) {
                bool selected = orderValue == i;
                if(ImGui::Selectable(progressionOrderLabels[i], selected)) {
                    setProgressionOrder(i, true);
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        drawNodePublishContextMenu("progressionOrder");
        drawPublishedCurrentItemUnderline("progressionOrder");

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
        drawNodePublishContextMenu("globalKey");
        drawPublishedCurrentItemUnderline("globalKey");

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
        drawNodePublishContextMenu("globalScale");
        drawPublishedCurrentItemUnderline("globalScale");

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(drawDraggableFloatWithPopup("Pitch Bend", globalPitchBend, 0.05f, -24.0f, 24.0f, "%.3f",
                                       [this]() { drawNodePublishMenuItems("pitchBend"); })) {
            pitchBendParameter = globalPitchBend;
        }
        drawPublishedCurrentItemUnderline("pitchBend");

        ImGui::TableSetColumnIndex(1);
        if(ImGui::InputInt("Transpose", &globalTranspose)) {
            transposeParameter = globalTranspose;
        }
        drawNodePublishContextMenu("transpose");
        drawPublishedCurrentItemUnderline("transpose");

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(ImGui::InputInt("Invert", &globalInvert)) {
            inversionParameter = globalInvert;
        }
        drawNodePublishContextMenu("inversion");
        drawPublishedCurrentItemUnderline("inversion");

        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("%s", usesInternalProgressionOrder() ? "Transport paced" : "External index paced");

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::BeginDisabled();
        ImGui::InputFloat("BPM", &bpmDisplay, 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();

        ImGui::TableSetColumnIndex(1);
        if(ImGui::Button("Reset Sequence")) {
            resetInternalSequence(true);
        }
        drawNodePublishContextMenu("resetSequence");
        drawPublishedCurrentItemUnderline("resetSequence");

        ImGui::EndTable();
    }
}

void chordSequence::drawRandomationControls() {
    auto rerollAndRefresh = [this]() {
        updateEffectiveGlobalModifiers(false, false, true);
        outputBuildDirty = true;
        lastRefreshedActiveIndex = -1;
        refreshAllOutputs(true);
    };

    ImGui::TextDisabled("Transpose");
    if(ImGui::InputInt("randomRange##TransposeRandomRange", &transposeRandomRange)) {
        transposeRandomRange = std::max(0, transposeRandomRange);
        rerollAndRefresh();
    }
    drawNodePublishContextMenu("transposeRandomRange");
    drawPublishedCurrentItemUnderline("transposeRandomRange");
    if(ImGui::InputInt("Q##TransposeRandomQ", &transposeRandomQuantization)) {
        transposeRandomQuantization = std::max(1, transposeRandomQuantization);
        rerollAndRefresh();
    }
    drawNodePublishContextMenu("transposeRandomQuantization");
    drawPublishedCurrentItemUnderline("transposeRandomQuantization");
    if(ImGui::Checkbox("step##TransposeRandomStep", &transposeRandomStep)) {
        rerollAndRefresh();
    }
    drawNodePublishContextMenu("transposeRandomStep");
    drawPublishedCurrentItemUnderline("transposeRandomStep");

    ImGui::Separator();
    ImGui::TextDisabled("Inversion");
    if(ImGui::InputInt("randomRange##InversionRandomRange", &inversionRandomRange)) {
        inversionRandomRange = std::max(0, inversionRandomRange);
        rerollAndRefresh();
    }
    drawNodePublishContextMenu("inversionRandomRange");
    drawPublishedCurrentItemUnderline("inversionRandomRange");
    if(ImGui::InputInt("Q##InversionRandomQ", &inversionRandomQuantization)) {
        inversionRandomQuantization = std::max(1, inversionRandomQuantization);
        rerollAndRefresh();
    }
    drawNodePublishContextMenu("inversionRandomQuantization");
    drawPublishedCurrentItemUnderline("inversionRandomQuantization");
    if(ImGui::Checkbox("step##InversionRandomStep", &inversionRandomStep)) {
        rerollAndRefresh();
    }
    drawNodePublishContextMenu("inversionRandomStep");
    drawPublishedCurrentItemUnderline("inversionRandomStep");
}

void chordSequence::drawImportTools() {
    ImGui::SetNextItemWidth(std::min(scaledUi(220.0f), ImGui::GetContentRegionAvail().x));
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
        ImGui::SetNextItemWidth(std::max(scaledUi(120.0f), std::min(scaledUi(260.0f), ImGui::GetContentRegionAvail().x - scaledUi(110.0f))));
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
        ImGui::SetNextItemWidth(scaledUi(260.0f));
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
    const int visibleColumns = 8;
    float gap = scaledUi(6.0f);
    float slotWidth = std::max(1.0f, (availableWidth - gap * (visibleColumns - 1)) / static_cast<float>(visibleColumns));
    float slotHeight = 0.0f;
    for(const auto &entry : progression) {
        slotHeight = std::max(slotHeight, getChordSequenceEntryCardHeight(entry.mode, markovEnabled));
    }

    ImGui::BeginChild("ChordSequenceEntriesScroller", ImVec2(0, slotHeight + scaledUi(4.0f)), false, ImGuiWindowFlags_HorizontalScrollbar);
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
    const std::string publishPrefix = stepPublishPrefix(index);

    ImGui::PushID(index);
    ImVec4 headerColor = isActive ? ImVec4(0.23f, 0.52f, 0.29f, 0.96f) : ImVec4(0.08f, 0.22f, 0.13f, 0.92f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, headerColor);
    ImGui::BeginChild("StepEntryCard", ImVec2(width, cardHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImGui::Text("Step %d", index);
    ImGui::SameLine();
    ImGui::TextColored(isActive ? ImVec4(0.92f, 1.00f, 0.94f, 1.0f) : ImVec4(0.70f, 0.88f, 0.74f, 1.0f), isActive ? "ACTIVE" : "idle");

    float rowWidth = std::max(scaledUi(140.0f), ImGui::GetContentRegionAvail().x * 0.94f);
    float labelGap = scaledUi(10.0f);
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
    float controlWidth = std::max(scaledUi(80.0f), rowWidth - maxLabelWidth - labelGap);

    auto drawRowLabel = [controlWidth, labelGap](float rowStartX, const char *label) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(rowStartX + controlWidth + labelGap);
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
    drawNodePublishContextMenu(publishPrefix + "mode", "Type", controlWidth);
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
        drawNodePublishContextMenu(publishPrefix + "functionalGroup", "Function", controlWidth);
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
            drawNodePublishContextMenu(publishPrefix + "degree", "Degree", controlWidth);
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
        drawNodePublishContextMenu(publishPrefix + "chordSize", "Chord Size", controlWidth);
        drawRowLabel(rowStartX, "Chord Size");

        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(ImGui::InputInt("##StepInterval", &entry.stepInterval)) {
            entry.stepInterval = std::max(1, entry.stepInterval);
            sanitizeEntry(entry);
            refreshAllOutputs(true);
        }
        drawNodePublishContextMenu(publishPrefix + "stepInterval", "Step Interval", controlWidth);
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
                                       "%.1f",
                                       [this, publishPrefix]() {
                                           drawNodePublishMenuItems(publishPrefix + "diatonicDeviationProbability");
                                       })) {
            sanitizeEntry(entry);
            refreshAllOutputs(true);
        }
        drawPublishedLabelUnderline(publishPrefix + "diatonicDeviationProbability", "Diat Dev %", controlWidth);
        drawRowLabel(rowStartX, "Diat Dev %");

        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(ImGui::InputInt("##DiatonicDeviationRange", &entry.diatonicDeviationRange)) {
            sanitizeEntry(entry);
            refreshAllOutputs(true);
        }
        drawNodePublishContextMenu(publishPrefix + "diatonicDeviationRange", "Diat Dev Range", controlWidth);
        drawRowLabel(rowStartX, "Diat Dev Range");
    }

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(drawDraggableFloatWithPopup("##BeatDuration", entry.beatDuration, 0.05f, 0.001f, 64.0f, "%.3f",
                                   [this, publishPrefix]() {
                                       drawNodePublishMenuItems(publishPrefix + "beatDuration");
                                   })) {
        outputBuildDirty = true;
        if(usesInternalProgressionOrder() && !progression.empty()) {
            int activeStep = ofClamp(resolveActiveIndex(), 0, static_cast<int>(progression.size()) - 1);
            nextInternalStepBeat = getFrameTransportState().current.beatPosition +
                                   std::max(0.001f, progression[activeStep].beatDuration);
        }
    }
    drawPublishedLabelUnderline(publishPrefix + "beatDuration", "Beats", controlWidth);
    drawRowLabel(rowStartX, "Beats");

    if(markovEnabled) {
        ImGui::TextUnformatted("Transitions");

        float sliderRegionWidth = rowWidth;
        float sliderHeight = scaledUi(37.0f);
        float innerGap = scaledUi(3.0f);
        int transitionCount = std::max(1, static_cast<int>(progression.size()));
        float sliderWidth = std::max(scaledUi(8.0f), (sliderRegionWidth - innerGap * (transitionCount - 1)) / static_cast<float>(transitionCount));

        ImGui::BeginChild("MarkovSliders", ImVec2(sliderRegionWidth, sliderHeight + scaledUi(20.0f)), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        for(int j = 0; j < transitionCount; j++) {
            if(j > 0) ImGui::SameLine(0.0f, innerGap);
            ImGui::BeginGroup();
            ImVec2 sliderSize(sliderWidth, sliderHeight);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, IM_COL32(0, 0, 0, 0));
            if(ImGui::VSliderFloat(("##To" + ofToString(j)).c_str(), sliderSize, &entry.markovWeights[j], 0.0f, 1.0f, "")) {
                entry.markovWeights[j] = std::max(0.0f, entry.markovWeights[j]);
            }
            ImVec2 sliderMin = ImGui::GetItemRectMin();
            ImVec2 sliderMax = ImGui::GetItemRectMax();
            float normalizedWeight = ofClamp(entry.markovWeights[j], 0.0f, 1.0f);
            float fillTop = sliderMax.y - (sliderMax.y - sliderMin.y) * normalizedWeight;
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            ImU32 fillColor = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
            ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Border);
            drawList->AddRectFilled(ImVec2(sliderMin.x, fillTop),
                                    sliderMax,
                                    fillColor,
                                    2.0f);
            drawList->AddRect(sliderMin, sliderMax, borderColor, 2.0f);
            ImGui::PopStyleColor(2);
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
    drawNodePublishContextMenu(publishPrefix + "transpose", "Transpose", controlWidth);
    drawRowLabel(rowStartX, "Transpose");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    std::string rootLabel = getEntryDisplayRootLabel(entry);
    char rootBuf[32];
    std::snprintf(rootBuf, sizeof(rootBuf), "%s", rootLabel.c_str());
    ImGui::BeginDisabled();
    ImGui::InputText("##Root", rootBuf, sizeof(rootBuf), ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    drawRowLabel(rowStartX, "Root");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##Inversion", &entry.inversion)) {
        refreshAllOutputs(true);
    }
    drawNodePublishContextMenu(publishPrefix + "inversion", "Inversion", controlWidth);
    drawRowLabel(rowStartX, "Inversion");

    std::vector<float> preview = buildEntryPreviewOutput(entry);
    drawKeyboardDisplay("StepKeyboard", preview, rowWidth, scaledUi(62.0f), isActive, true);

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
    float gap = scaledUi(8.0f);
    float slotWidth = std::max(scaledUi(230.0f), std::min(scaledUi(320.0f), (availableWidth - gap * (visibleColumns - 1)) / static_cast<float>(visibleColumns)));
    float slotHeight = 0.0f;
    for(const auto &config : outputConfigs) {
        slotHeight = std::max(slotHeight, getOutputCardHeight(config));
    }
    if(slotHeight <= 0.0f) {
        chordSequenceOutputConfig defaultConfig;
        slotHeight = getOutputCardHeight(defaultConfig);
    }

    ImGui::BeginChild("ChordSequenceOutputsScroller", ImVec2(0, slotHeight + scaledUi(4.0f)), false, ImGuiWindowFlags_HorizontalScrollbar);
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
    const std::string publishPrefix = outputPublishPrefix(index);

    ImGui::PushID(index);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.20f, 0.12f, 0.92f));
    ImGui::BeginChild("OutputCard", ImVec2(width, cardHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImGui::Text("%s", outputName(index).c_str());

    float rowWidth = std::max(scaledUi(180.0f), ImGui::GetContentRegionAvail().x * 0.94f);
    float labelGap = scaledUi(10.0f);
    float maxLabelWidth = std::max({
        ImGui::CalcTextSize("Octave").x,
        ImGui::CalcTextSize("Transpose").x,
        ImGui::CalcTextSize("Pitch Bend").x,
        ImGui::CalcTextSize("Detune").x,
        ImGui::CalcTextSize("Oct Rand %").x,
        ImGui::CalcTextSize("Oct Rand Range").x,
        ImGui::CalcTextSize("Chrom Dev %").x,
        ImGui::CalcTextSize("Chrom Dev Range").x,
        ImGui::CalcTextSize("Voice Lead").x,
        ImGui::CalcTextSize("Min Note").x,
        ImGui::CalcTextSize("Max Note").x,
        ImGui::CalcTextSize("Chord Sum").x,
        ImGui::CalcTextSize("Source").x,
        ImGui::CalcTextSize("AddBass").x,
        ImGui::CalcTextSize("BassOct").x,
        ImGui::CalcTextSize("Inversion").x,
        ImGui::CalcTextSize("Voicing").x,
        ImGui::CalcTextSize("Spread").x,
        ImGui::CalcTextSize("Fold12").x,
        ImGui::CalcTextSize("Glide").x,
        ImGui::CalcTextSize("Output Size").x,
        ImGui::CalcTextSize("Expand").x,
        ImGui::CalcTextSize("Sort").x
    });
    float controlWidth = std::max(scaledUi(84.0f), rowWidth - maxLabelWidth - labelGap);

    auto drawRowLabel = [controlWidth, labelGap](float rowStartX, const char *label) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(rowStartX + controlWidth + labelGap);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
    };

    auto drawSingleToggleRow = [&](const char *id, bool &value, const char *label, const std::string &key = std::string()) {
        if(ImGui::Checkbox(id, &value)) {
            refreshAllOutputs(true);
        }
        if(!key.empty()) drawNodePublishContextMenu(key, label, 0.0f, true);
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
    };

    auto drawBoolPairRow = [&](const char *leftId,
                               bool &leftValue,
                               const char *leftLabel,
                               const char *rightId,
                               bool &rightValue,
                               const char *rightLabel,
                               const std::string &leftKey = std::string(),
                               const std::string &rightKey = std::string()) {
        float rowStartX = ImGui::GetCursorPosX();
        float pairGap = scaledUi(12.0f);
        float pairWidth = std::max(scaledUi(70.0f), (rowWidth - pairGap) * 0.5f);

        ImGui::SetCursorPosX(rowStartX);
        if(ImGui::Checkbox(leftId, &leftValue)) {
            refreshAllOutputs(true);
        }
        if(!leftKey.empty()) drawNodePublishContextMenu(leftKey, leftLabel, 0.0f, true);
        ImGui::SameLine();
        ImGui::TextUnformatted(leftLabel);

        ImGui::SameLine(rowStartX + pairWidth + pairGap);
        if(ImGui::Checkbox(rightId, &rightValue)) {
            refreshAllOutputs(true);
        }
        if(!rightKey.empty()) drawNodePublishContextMenu(rightKey, rightLabel, 0.0f, true);
        ImGui::SameLine();
        ImGui::TextUnformatted(rightLabel);
    };

    float rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##Octave", &config.octave)) {
        refreshAllOutputs(true);
    }
    drawNodePublishContextMenu(publishPrefix + "octave", "Octave", controlWidth);
    drawRowLabel(rowStartX, "Octave");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##Transpose", &config.transpose)) {
        refreshAllOutputs(true);
    }
    drawNodePublishContextMenu(publishPrefix + "transpose", "Transpose", controlWidth);
    drawRowLabel(rowStartX, "Transpose");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(drawDraggableFloatWithPopup("##PitchBend", config.pitchBend, 0.05f, -24.0f, 24.0f, "%.3f",
                                   [this, publishPrefix]() { drawNodePublishMenuItems(publishPrefix + "pitchBend"); })) {
        refreshAllOutputs(true);
    }
    drawPublishedLabelUnderline(publishPrefix + "pitchBend", "Pitch Bend", controlWidth);
    drawRowLabel(rowStartX, "Pitch Bend");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(drawDraggableFloatWithPopup("##Detune", config.perNoteDetune, 0.01f, 0.0f, 2.0f, "%.3f",
                                   [this, publishPrefix]() { drawNodePublishMenuItems(publishPrefix + "detune"); })) {
        refreshAllOutputs(true);
    }
    drawPublishedLabelUnderline(publishPrefix + "detune", "Detune", controlWidth);
    drawRowLabel(rowStartX, "Detune");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(drawDraggableFloatWithPopup("##OctRandProbability", config.octaveRandomProbability, 0.5f, 0.0f, 100.0f, "%.1f",
                                   [this, publishPrefix]() { drawNodePublishMenuItems(publishPrefix + "octRandProbability"); })) {
        refreshAllOutputs(true);
    }
    drawPublishedLabelUnderline(publishPrefix + "octRandProbability", "Oct Rand %", controlWidth);
    drawRowLabel(rowStartX, "Oct Rand %");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##OctRandRange", &config.octaveRandomRange)) {
        config.octaveRandomRange = std::max(0, config.octaveRandomRange);
        refreshAllOutputs(true);
    }
    drawNodePublishContextMenu(publishPrefix + "octRandRange", "Oct Rand Range", controlWidth);
    drawRowLabel(rowStartX, "Oct Rand Range");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(drawDraggableFloatWithPopup("##ChromaticDeviationProbability",
                                   config.chromaticDeviationProbability,
                                   0.5f,
                                   0.0f,
                                   100.0f,
                                   "%.1f",
                                   [this, publishPrefix]() { drawNodePublishMenuItems(publishPrefix + "chromDevProbability"); })) {
        refreshAllOutputs(true);
    }
    drawPublishedLabelUnderline(publishPrefix + "chromDevProbability", "Chrom Dev %", controlWidth);
    drawRowLabel(rowStartX, "Chrom Dev %");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##ChromaticDeviationRange", &config.chromaticDeviationRange)) {
        config.chromaticDeviationRange = std::max(0, config.chromaticDeviationRange);
        refreshAllOutputs(true);
    }
    drawNodePublishContextMenu(publishPrefix + "chromDevRange", "Chrom Dev Range", controlWidth);
    drawRowLabel(rowStartX, "Chrom Dev Range");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    int safeSourceMode = ofClamp(config.sourceMode,
                                 chordSequenceOutputConfig::Chord,
                                 chordSequenceOutputConfig::ChordSum);
    if(ImGui::BeginCombo("##Source", outputSourceLabels[safeSourceMode])) {
        for(int i = chordSequenceOutputConfig::Chord; i <= chordSequenceOutputConfig::ChordSum; i++) {
            bool selected = safeSourceMode == i;
            if(ImGui::Selectable(outputSourceLabels[i], selected)) {
                config.sourceMode = i;
                refreshAllOutputs(true);
            }
            if(selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    drawNodePublishContextMenu(publishPrefix + "source", "Source", controlWidth);
    drawRowLabel(rowStartX, "Source");

    drawSingleToggleRow("##Fold12", config.fold12, "Fold12", publishPrefix + "fold12");

    if(!outputSourceUsesScaleLikeMaterial(config.sourceMode)) {
        drawSingleToggleRow("##AddBass", config.addBass, "AddBass", publishPrefix + "addBass");

        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(ImGui::InputInt("##Inversion", &config.inversion)) {
            refreshAllOutputs(true);
        }
        drawNodePublishContextMenu(publishPrefix + "inversion", "Inversion", controlWidth);
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
        drawNodePublishContextMenu(publishPrefix + "voicing", "Voicing", controlWidth);
        drawRowLabel(rowStartX, "Voicing");

        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(drawDraggableFloatWithPopup("##Spread", config.voicingSpread, 0.1f, 0.0f, 24.0f, "%.2f",
                                       [this, publishPrefix]() { drawNodePublishMenuItems(publishPrefix + "spread"); })) {
            refreshAllOutputs(true);
        }
        drawPublishedLabelUnderline(publishPrefix + "spread", "Spread", controlWidth);
        drawRowLabel(rowStartX, "Spread");

        if(config.addBass) {
            rowStartX = ImGui::GetCursorPosX();
            ImGui::SetNextItemWidth(controlWidth);
            if(ImGui::InputInt("##BassOct", &config.bassOct)) {
                refreshAllOutputs(true);
            }
            drawNodePublishContextMenu(publishPrefix + "bassOct", "BassOct", controlWidth);
            drawRowLabel(rowStartX, "BassOct");
        }
    }

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    drawDraggableFloatWithPopup("##Glide", config.glideMs, 2.0f, 0.0f, 10000.0f, "%.1f",
                                [this, publishPrefix]() { drawNodePublishMenuItems(publishPrefix + "glide"); });
    drawPublishedLabelUnderline(publishPrefix + "glide", "Glide", controlWidth);
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
    drawNodePublishContextMenu(publishPrefix + "outputSize", "Output Size", controlWidth);
    drawRowLabel(rowStartX, "Output Size");

    drawSingleToggleRow("##VoiceLeading", config.voiceLeading, "Voice Lead", publishPrefix + "voiceLeading");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##MinNote", &config.minNote)) {
        config.minNote = ofClamp(config.minNote, 0, 127);
        if(config.minNote > config.maxNote) config.maxNote = config.minNote;
        refreshAllOutputs(true);
    }
    drawNodePublishContextMenu(publishPrefix + "minNote", "Min Note", controlWidth);
    drawRowLabel(rowStartX, "Min Note");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##MaxNote", &config.maxNote)) {
        config.maxNote = ofClamp(config.maxNote, 0, 127);
        if(config.maxNote < config.minNote) config.minNote = config.maxNote;
        refreshAllOutputs(true);
    }
    drawNodePublishContextMenu(publishPrefix + "maxNote", "Max Note", controlWidth);
    drawRowLabel(rowStartX, "Max Note");

    drawBoolPairRow("##Expand", config.expandOutput, "Expand",
                    "##Sort", config.sortOutput, "Sort",
                    publishPrefix + "expand", publishPrefix + "sort");

    drawKeyboardDisplay("OutputKeyboard", displayedOutput, rowWidth, scaledUi(64.0f), true, false);

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
        ImVec4 color = hasData ? ImVec4(0.17f, 0.28f, 0.35f, 1.0f) : ImVec4(0.11f, 0.12f, 0.15f, 1.0f);
        if(isActive) color = ImVec4(0.76f, 0.48f, 0.26f, 1.0f);

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
    ImGui::SetNextItemWidth(std::min(scaledUi(260.0f), ImGui::GetContentRegionAvail().x));
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
        ImGui::SetNextItemWidth(std::min(scaledUi(260.0f), ImGui::GetContentRegionAvail().x));
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
    json["transposeRandomRange"] = transposeRandomRange;
    json["transposeRandomQuantization"] = transposeRandomQuantization;
    json["transposeRandomStep"] = transposeRandomStep;
    json["inversionRandomRange"] = inversionRandomRange;
    json["inversionRandomQuantization"] = inversionRandomQuantization;
    json["inversionRandomStep"] = inversionRandomStep;
    json["globalPitchBend"] = globalPitchBend;
    json["progressionOrder"] = progressionOrder;
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
    transposeRandomRange = std::max(0, json.value("transposeRandomRange", 0));
    transposeRandomQuantization = std::max(1, json.value("transposeRandomQuantization", 1));
    transposeRandomStep = json.value("transposeRandomStep", false);
    inversionRandomRange = std::max(0, json.value("inversionRandomRange", 0));
    inversionRandomQuantization = std::max(1, json.value("inversionRandomQuantization", 1));
    inversionRandomStep = json.value("inversionRandomStep", false);
    currentTransposeRandomOffset = 0;
    currentInversionRandomOffset = 0;
    effectiveGlobalTranspose = globalTranspose;
    effectiveGlobalInvert = globalInvert;
    pendingRandomationStepAdvance = false;
    pendingRandomationSequenceRestart = false;
    globalPitchBend = json.value("globalPitchBend", json.value("pitchBend", 0.0f));
    int loadedProgressionOrder = InputIdx;
    if(json.contains("progressionOrder")) {
        loadedProgressionOrder = ofClamp(json.value("progressionOrder", InputIdx), InputIdx, Markov);
    } else {
        bool legacyInternalTiming = json.value("internalTimingEnabled", false);
        bool legacyMarkov = json.value("markovEnabled", false);
        loadedProgressionOrder = legacyInternalTiming ? (legacyMarkov ? Markov : Ascendent) : InputIdx;
    }
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
    initializePublishableEditorParameters();
    syncPublishedEditorParameters(publishedEditorParameterKeys);
    syncNodeGuiParametersFromState();
    setProgressionOrder(loadedProgressionOrder, false);
    outputBuildDirty = true;
    lastRefreshedActiveIndex = -1;
    refreshAllOutputs(forceInstant);
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
    snapshot.transposeRandomRange = transposeRandomRange;
    snapshot.transposeRandomQuantization = transposeRandomQuantization;
    snapshot.transposeRandomStep = transposeRandomStep;
    snapshot.inversionRandomRange = inversionRandomRange;
    snapshot.inversionRandomQuantization = inversionRandomQuantization;
    snapshot.inversionRandomStep = inversionRandomStep;
    snapshot.globalPitchBend = globalPitchBend;
    snapshot.progressionOrder = progressionOrder;
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

#endif
