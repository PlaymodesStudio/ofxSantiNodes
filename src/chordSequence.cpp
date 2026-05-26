#include "chordSequence.h"
#include "chordCypherAliases.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

namespace {
    constexpr int indexParameterMin = -1000000;
    constexpr int indexParameterMax = 1000000;
    constexpr int keyboardDisplayBaseNote = 60;

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
        std::vector<int> notes;
        notes.reserve(values.size());
        for(float value : values) {
            int note = ofClamp(static_cast<int>(std::round(value)) + keyboardDisplayBaseNote, 0, 127);
            notes.push_back(note);
        }
        return notes;
    }

    std::pair<int, int> computeKeyboardRange(const std::vector<int> &notes, int defaultLow, int defaultHigh, int minSemitones) {
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
        std::set<int> uniqueNotes;
        for(int note : resizedNotes) {
            noteCounts[note]++;
            uniqueNotes.insert(note);
        }

        std::vector<int> displayNotes(uniqueNotes.begin(), uniqueNotes.end());
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
                    drawList->AddText(ImVec2(keyPos.x + 4.0f, keyPos.y + height - 16.0f), IM_COL32(20, 20, 20, 220), countLabel.c_str());
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
                    drawList->AddText(ImVec2(keyPos.x + 2.0f, keyPos.y + blackKeyHeight - 14.0f), IM_COL32(255, 255, 255, 220), countLabel.c_str());
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

        std::string popupId = std::string(label) + "Popup";
        if(ImGui::BeginPopupContextItem(popupId.c_str())) {
            float tempValue = value;
            ImGui::SetNextItemWidth(120.0f);
            if(ImGui::InputFloat("##value", &tempValue, 0.0f, 0.0f, format, ImGuiInputTextFlags_EnterReturnsTrue)) {
                value = ofClamp(tempValue, minValue, maxValue);
                changed = true;
            }
            ImGui::EndPopup();
        }

        return changed;
    }
}

ofJson chordSequenceEntry::toJson() const {
    return {
        {"mode", mode},
        {"itemIndex", itemIndex},
        {"itemName", itemName},
        {"transpose", transpose},
        {"inversion", inversion}
    };
}

chordSequenceEntry chordSequenceEntry::fromJson(const ofJson &json) {
    chordSequenceEntry entry;
    entry.mode = json.value("mode", Chord);
    entry.itemIndex = json.value("itemIndex", 0);
    entry.itemName = json.value("itemName", std::string());
    entry.transpose = json.value("transpose", 0);
    entry.inversion = json.value("inversion", 0);
    return entry;
}

ofJson chordSequenceSnapshot::toJson() const {
    ofJson json;
    json["name"] = name;
    json["outputSize"] = outputSize;
    json["expandOutput"] = expandOutput;
    json["sortOutput"] = sortOutput;
    json["globalTranspose"] = globalTranspose;
    json["globalInvert"] = globalInvert;
    json["pitchBend"] = pitchBend;
    json["glideMs"] = glideMs;
    json["hasData"] = hasData;
    json["entries"] = ofJson::array();

    for(const auto &entry : entries) {
        json["entries"].push_back(entry.toJson());
    }

    return json;
}

chordSequenceSnapshot chordSequenceSnapshot::fromJson(const ofJson &json) {
    chordSequenceSnapshot snapshot;
    snapshot.name = json.value("name", std::string());
    snapshot.outputSize = std::max(1, json.value("outputSize", 4));
    snapshot.expandOutput = json.value("expandOutput", false);
    snapshot.sortOutput = json.value("sortOutput", false);
    snapshot.globalTranspose = json.value("globalTranspose", 0);
    snapshot.globalInvert = json.value("globalInvert", 0);
    snapshot.pitchBend = json.value("pitchBend", 0.0f);
    snapshot.glideMs = json.value("glideMs", 0.0f);
    snapshot.hasData = json.value("hasData", false);

    if(json.contains("entries") && json["entries"].is_array()) {
        for(const auto &entryJson : json["entries"]) {
            snapshot.entries.push_back(chordSequenceEntry::fromJson(entryJson));
        }
    }

    snapshot.hasData = snapshot.hasData || !snapshot.entries.empty();
    return snapshot;
}

chordSequence::chordSequence() : ofxOceanodeNodeModel("Chord Sequence") {
    description = "Builds chord and scale progressions from the Pitchclass text files, "
                  "with per-step transpose/inversion, global transpose/invert, pitch bend, "
                  "glide, and snapshot recall.";
}

void chordSequence::setup() {
    addParameter(indexInput.set("Index", 0, indexParameterMin, indexParameterMax));
    addParameter(showEditor.set("Show", false));
    addOutputParameter(chordOut.set("ChordOut", std::vector<float>{0.0f}, std::vector<float>{-FLT_MAX}, std::vector<float>{FLT_MAX}));

    addInspectorParameter(editorWidth.set("Editor Width", 760.0f, 420.0f, 1600.0f));
    addInspectorParameter(editorHeight.set("Editor Height", 860.0f, 360.0f, 1600.0f));

    chordOut.setSerializable(false);

    loadLibraries();
    loadImportSources();
    initializeDefaultProgression();
    loadAllSnapshotsFromDisk();

    listeners.push(indexInput.newListener([this](int &) {
        refreshTargetOutput();
    }));

    refreshTargetOutput(true);
}

void chordSequence::update(ofEventArgs &) {
    if(!isGliding) return;

    float progress = getGlideProgress();
    std::vector<float> interpolated = getInterpolatedOutput(progress);
    chordOut.set(interpolated);

    if(progress >= 1.0f) {
        isGliding = false;
        currentOutput = targetOutput;
        chordOut.set(currentOutput);
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
    }

    activeSnapshotSlot = json.value("activeSnapshotSlot", -1);
    refreshTargetOutput(true);
}

void chordSequence::presetHasLoaded() {
    refreshTargetOutput(true);
}

void chordSequence::loadLibraries() {
    chordLibrary = parsePitchClassFile(resolvePitchClassFile("chords.txt"));
    scaleLibrary = parsePitchClassFile(resolvePitchClassFile("scales.txt"));
    loadCypherAliases();

    if(chordLibrary.empty()) {
        chordLibrary.push_back({"Unavailable", {0.0f}});
    }

    if(scaleLibrary.empty()) {
        scaleLibrary.push_back({"Unavailable", {0.0f}});
    }

    defaultChordIndex = std::max(0, findItemIndexByName(chordLibrary, "M"));
    defaultScaleIndex = std::max(0, findItemIndexByName(scaleLibrary, "major"));
}

void chordSequence::loadCypherAliases() {
    chordAliases.clear();
    chordCypherAliases::mergeAliasesFromFile(chordAliases, "chordSequence");
}

void chordSequence::loadImportSources() {
    importedProgressionDatabase = ofJson::object();
    importedProgressionDatabase["progressions"] = ofJson::object();
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

std::string chordSequence::resolvePitchClassFile(const std::string &fileName) const {
    const std::string relativePath = ofToDataPath("Supercollider/Pitchclass/" + fileName, true);
    ofFile relativeFile(relativePath);
    if(relativeFile.exists()) {
        return relativePath;
    }

    return relativePath;
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

void chordSequence::reloadLibraries() {
    loadLibraries();
    sanitizeProgression();
    refreshTargetOutput(true);
}

void chordSequence::initializeDefaultProgression() {
    progression.clear();
    progression.resize(DefaultSequenceSize);

    for(auto &entry : progression) {
        entry.mode = chordSequenceEntry::Chord;
        entry.itemIndex = defaultChordIndex;
        entry.itemName = chordLibrary[defaultChordIndex].name;
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
        progression[i].transpose = 0;
        progression[i].inversion = 0;
    }

    sanitizeProgression();
    refreshTargetOutput();
}

void chordSequence::sanitizeProgression() {
    if(progression.empty()) {
        initializeDefaultProgression();
    }

    for(auto &entry : progression) {
        sanitizeEntry(entry);
    }
}

void chordSequence::sanitizeEntry(chordSequenceEntry &entry) {
    if(entry.mode == chordSequenceEntry::Cypher) {
        if(entry.itemName.empty()) entry.itemName = "C";
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
        entry.itemName = library[entry.itemIndex].name;
    }

    entry.itemName = library[entry.itemIndex].name;
}

bool chordSequence::parseChordTokenToEntry(const std::string &token, chordSequenceEntry &entry) const {
    std::string trimmed = ofTrim(token);
    if(trimmed.empty()) return false;

    entry.mode = chordSequenceEntry::Cypher;
    entry.itemName = trimmed;
    entry.itemIndex = defaultChordIndex;
    entry.transpose = 0;
    entry.inversion = 0;
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
    refreshTargetOutput(true);
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
    if(finalName.empty()) {
        finalName = "user";
    }

    importedProgressionDatabase["progressions"][ofToString(nextIndex)] = {
        {"name", finalName},
        {"chords", ofJoinString(chords, ", ")},
        {"timing", getDefaultTimingStringForChords(chords)}
    };

    ofSavePrettyJson(getChordProgressionsFilePath(), importedProgressionDatabase);
    refreshImportedProgressionNames();
    selectedImportedProgression = static_cast<int>(importedProgressionNames.size()) - 1;
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
    const auto &library = getLibraryForMode(entry.mode);
    if(library.empty()) return "---";
    int index = ofClamp(entry.itemIndex, 0, static_cast<int>(library.size()) - 1);
    return library[index].name;
}

const std::vector<chordSequenceLibraryItem> &chordSequence::getLibraryForMode(int mode) const {
    return mode == chordSequenceEntry::Scale ? scaleLibrary : chordLibrary;
}

std::vector<float> chordSequence::buildEntryCoreOutput(const chordSequenceEntry &entry) const {
    if(entry.mode == chordSequenceEntry::Cypher) {
        std::string input = ofTrim(entry.itemName);
        if(input.empty()) return {0.0f};

        size_t slashPos = input.find_first_of("/\\");
        if(slashPos != std::string::npos) {
            input = input.substr(0, slashPos);
        }

        if(input.empty()) return {0.0f};

        std::string root;
        root += static_cast<char>(std::toupper(input[0]));
        size_t offset = 1;
        if(input.size() > 1 && (input[1] == '#' || input[1] == 'b')) {
            root += input[1];
            offset = 2;
        }

        std::string quality = normalizeChordQuality(input.substr(offset));
        int qualityIndex = findItemIndexByName(chordLibrary, quality);
        if(qualityIndex < 0) {
            qualityIndex = defaultChordIndex;
        }

        std::vector<float> values = chordLibrary[qualityIndex].values;
        values = applyInversion(values, entry.inversion + globalInvert);

        int rootValue = getNoteValue(root);
        for(auto &value : values) {
            value += rootValue + entry.transpose + globalTranspose + pitchBend;
        }

        return values;
    }

    const auto &library = getLibraryForMode(entry.mode);
    if(library.empty()) return {0.0f};

    int safeIndex = ofClamp(entry.itemIndex, 0, static_cast<int>(library.size()) - 1);
    std::vector<float> values = library[safeIndex].values;

    values = applyInversion(values, entry.inversion + globalInvert);

    for(auto &value : values) {
        value += entry.transpose + globalTranspose + pitchBend;
    }

    return values;
}

std::vector<float> chordSequence::buildEntryOutput(const chordSequenceEntry &entry) const {
    return adaptOutputSize(buildEntryCoreOutput(entry));
}

std::vector<float> chordSequence::adaptOutputSize(const std::vector<float> &values) const {
    if(values.empty()) return {0.0f};

    int requestedSize = std::max(1, outputSize);
    if(requestedSize == static_cast<int>(values.size())) return values;

    if(requestedSize < static_cast<int>(values.size())) {
        return std::vector<float>(values.begin(), values.begin() + requestedSize);
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
    return wrapIndex(indexInput.get(), static_cast<int>(progression.size()));
}

void chordSequence::refreshTargetOutput(bool forceInstant) {
    sanitizeProgression();

    int activeIndex = resolveActiveIndex();
    if(activeIndex < 0 || activeIndex >= static_cast<int>(progression.size())) {
        beginGlideTo({0.0f}, true);
        return;
    }

    beginGlideTo(buildEntryOutput(progression[activeIndex]), forceInstant);
}

void chordSequence::beginGlideTo(const std::vector<float> &nextTarget, bool forceInstant) {
    if(isGliding) {
        currentOutput = getInterpolatedOutput(getGlideProgress());
    } else if(currentOutput.empty()) {
        currentOutput = chordOut.get();
    }

    targetOutput = nextTarget;

    if(forceInstant || glideMs <= 0.001f || currentOutput.empty()) {
        isGliding = false;
        currentOutput = targetOutput;
        chordOut.set(currentOutput);
        return;
    }

    glideStartOutput = currentOutput;
    glideStartTimeMs = ofGetElapsedTimeMillis();
    isGliding = true;
}

std::vector<float> chordSequence::getInterpolatedOutput(float progress) const {
    size_t maxSize = std::max(glideStartOutput.size(), targetOutput.size());
    if(maxSize == 0) return {};

    std::vector<float> output(maxSize, 0.0f);
    for(size_t i = 0; i < maxSize; i++) {
        float start = sampleVector(glideStartOutput, i);
        float end = sampleVector(targetOutput, i);
        output[i] = ofLerp(start, end, progress);
    }
    return output;
}

float chordSequence::getGlideProgress() const {
    if(!isGliding || glideMs <= 0.001f) return 1.0f;
    uint64_t elapsed = ofGetElapsedTimeMillis() - glideStartTimeMs;
    return ofClamp(static_cast<float>(elapsed) / glideMs, 0.0f, 1.0f);
}

float chordSequence::sampleVector(const std::vector<float> &values, size_t index) const {
    if(values.empty()) return 0.0f;
    if(index < values.size()) return values[index];
    return values.back();
}

float chordSequence::getSingleColumnWidth(float availableWidth) const {
    float gap = 10.0f;
    return std::max(180.0f, (availableWidth - gap * 3.0f) / 4.0f);
}

void chordSequence::drawEditor() {
    drawSnapshotManager();
    ImGui::Separator();

    float availableWidth = ImGui::GetContentRegionAvail().x;
    float outputColumnWidth = getSingleColumnWidth(availableWidth);
    float gap = 10.0f;
    float controlsWidth = std::max(220.0f, availableWidth - outputColumnWidth - gap);
    float topSectionHeight = 230.0f;

    ImGui::BeginChild("GlobalControlsColumn",
                      ImVec2(controlsWidth, topSectionHeight),
                      false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawGlobalControls();
    drawImportTools();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);

    ImGui::BeginChild("GlobalOutputColumn",
                      ImVec2(outputColumnWidth, topSectionHeight),
                      false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawOutputPreview();
    ImGui::EndChild();

    ImGui::Separator();
    drawEntries();
}

void chordSequence::drawGlobalControls() {
    int numChords = static_cast<int>(progression.size());
    int activeIndex = resolveActiveIndex();
    int displayIndex = activeIndex < 0 ? 0 : activeIndex;
    int requestedOutputSize = outputSize;

    if(ImGui::BeginTable("ChordSequenceGlobals", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(ImGui::InputInt("Number of Chords", &numChords)) {
            resizeProgression(numChords);
        }

        ImGui::TableSetColumnIndex(1);
        if(ImGui::InputInt("Active Index", &displayIndex)) {
            indexInput = ofClamp(displayIndex, 0, std::max(0, static_cast<int>(progression.size()) - 1));
        }

        ImGui::TableSetColumnIndex(2);
        if(drawDraggableFloatWithPopup("Pitch Bend", pitchBend, 0.05f, -24.0f, 24.0f, "%.3f")) {
            refreshTargetOutput();
        }

        ImGui::TableSetColumnIndex(3);
        if(ImGui::Button("Reload Lists")) {
            reloadLibraries();
        }

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(drawDraggableFloatWithPopup("Glide (ms)", glideMs, 2.0f, 0.0f, 10000.0f, "%.1f")) {
        }

        ImGui::TableSetColumnIndex(1);
        if(ImGui::InputInt("Global Transpose", &globalTranspose)) {
            refreshTargetOutput();
        }

        ImGui::TableSetColumnIndex(2);
        if(ImGui::InputInt("Global Invert", &globalInvert)) {
            refreshTargetOutput();
        }

        ImGui::TableSetColumnIndex(3);
        if(ImGui::InputInt("Output Size", &requestedOutputSize)) {
            outputSize = std::max(1, requestedOutputSize);
            refreshTargetOutput();
        }

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(ImGui::Checkbox("Expand", &expandOutput)) {
            refreshTargetOutput();
        }

        ImGui::TableSetColumnIndex(1);
        if(ImGui::Checkbox("Sort", &sortOutput)) {
            refreshTargetOutput();
        }

        ImGui::EndTable();
    }
}

void chordSequence::drawImportTools() {
    if(!ImGui::CollapsingHeader("Import", ImGuiTreeNodeFlags_DefaultOpen)) return;

    if(ImGui::InputText("Chord List", importChordBuffer.data(), importChordBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
        applyImportedChordList(parseChordSequenceString(importChordBuffer.data()));
    }
    ImGui::SameLine();
    if(ImGui::Button("Apply Chords")) {
        applyImportedChordList(parseChordSequenceString(importChordBuffer.data()));
    }
    ImGui::SameLine();
    std::vector<std::string> importChords = parseChordSequenceString(importChordBuffer.data());
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
        ImGui::SetNextItemWidth(240.0f);
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
        ImGui::SetNextItemWidth(240.0f);
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
    ImGui::Text("Progression");
    float availableWidth = ImGui::GetContentRegionAvail().x;
    int visibleColumns = std::min(std::max(1, static_cast<int>(progression.size())), 6);
    float gap = 6.0f;
    float slotWidth = std::max(170.0f, (availableWidth - gap * (visibleColumns - 1)) / static_cast<float>(visibleColumns));
    float slotHeight = 320.0f;

    ImGui::BeginChild("ChordSequenceEntries", ImVec2(0, slotHeight + 18.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    for(int i = 0; i < static_cast<int>(progression.size()); i++) {
        if(i > 0) ImGui::SameLine(0.0f, gap);
        drawEntryEditor(i, slotWidth);
    }
    ImGui::EndChild();
}

void chordSequence::drawEntryEditor(int index, float width) {
    chordSequenceEntry &entry = progression[index];
    int activeIndex = resolveActiveIndex();
    bool isActive = index == activeIndex;

    ImGui::PushID(index);
    ImVec4 headerColor = isActive ? ImVec4(0.18f, 0.30f, 0.16f, 0.85f) : ImVec4(0.12f, 0.12f, 0.12f, 0.85f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, headerColor);
    ImGui::BeginChild("Entry",
                      ImVec2(width, 320),
                      true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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
        ImGui::CalcTextSize("Transpose").x,
        ImGui::CalcTextSize("Inversion").x
    });
    float controlWidth = std::max(80.0f, rowWidth - maxLabelWidth - labelGap);
    float keyboardWidth = rowWidth;
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
    if(ImGui::Combo("##Type", &mode, "Chord\0Scale\0Cypher\0")) {
        entry.mode = mode;
        if(entry.mode == chordSequenceEntry::Cypher) {
            entry.itemName = entry.itemName.empty() ? "C" : entry.itemName;
        } else {
            entry.itemIndex = getDefaultIndexForMode(mode);
            entry.itemName.clear();
        }
        sanitizeEntry(entry);
        refreshTargetOutput();
    }
    drawRowLabel(rowStartX, "Type");

    if(entry.mode == chordSequenceEntry::Cypher) {
        char cypherBuf[128];
        std::snprintf(cypherBuf, sizeof(cypherBuf), "%s", entry.itemName.c_str());
        rowStartX = ImGui::GetCursorPosX();
        ImGui::SetNextItemWidth(controlWidth);
        if(ImGui::InputText("##ChordText", cypherBuf, sizeof(cypherBuf))) {
            entry.itemName = cypherBuf;
            refreshTargetOutput();
        }
        drawRowLabel(rowStartX, "Chord Text");
    } else {
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
                    refreshTargetOutput();
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        drawRowLabel(rowStartX, "Selection");
    }

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##Transpose", &entry.transpose)) {
        refreshTargetOutput();
    }
    drawRowLabel(rowStartX, "Transpose");

    rowStartX = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(controlWidth);
    if(ImGui::InputInt("##Inversion", &entry.inversion)) {
        refreshTargetOutput();
    }
    drawRowLabel(rowStartX, "Inversion");

    std::vector<float> textPreview = buildEntryCoreOutput(entry);
    drawKeyboardDisplay("StepKeyboard", textPreview, keyboardWidth, 62.0f, isActive, true);

    std::string previewText;
    for(size_t i = 0; i < textPreview.size(); i++) {
        if(i > 0) previewText += "  ";
        previewText += ofToString(static_cast<int>(std::round(textPreview[i])));
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + keyboardWidth);
    ImGui::TextWrapped("%s", previewText.c_str());
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

    auto getSnapshotDropdownLabel = [this, &getSnapshotDisplayName](int slot) {
        return ofToString(slot + 1) + ": " + getSnapshotDisplayName(slot);
    };

    ImGui::Text("Snapshots");
    ImGui::SameLine();
    ImGui::TextDisabled("Shift+Click store, Right-Click delete");

    float slotSize = 22.0f;

    for(int i = 0; i < SnapshotSlots; i++) {
        if(i > 0) ImGui::SameLine(0.0f, 4.0f);

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
    ImGui::SetNextItemWidth(260.0f);
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
        ImGui::SameLine();
        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", snapshotSlots[activeSnapshotSlot].name.c_str());
        ImGui::SetNextItemWidth(260.0f);
        if(ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            snapshotSlots[activeSnapshotSlot].name = nameBuf;
            saveSnapshotToDisk(activeSnapshotSlot);
        }
    }
}

void chordSequence::drawOutputPreview() {
    std::vector<float> displayedOutput = isGliding ? getInterpolatedOutput(getGlideProgress()) : targetOutput;
    drawKeyboardDisplay("GlobalKeyboard", displayedOutput, ImGui::GetContentRegionAvail().x, 64.0f, true, false);

    std::string outputText;
    for(size_t i = 0; i < displayedOutput.size(); i++) {
        if(i > 0) outputText += "  ";
        outputText += ofToString(displayedOutput[i], 2);
    }

    ImGui::Text("Current Output");
    ImGui::TextWrapped("%s", outputText.c_str());
}

ofJson chordSequence::serializeCurrentState() const {
    ofJson json;
    json["outputSize"] = outputSize;
    json["expandOutput"] = expandOutput;
    json["sortOutput"] = sortOutput;
    json["globalTranspose"] = globalTranspose;
    json["globalInvert"] = globalInvert;
    json["pitchBend"] = pitchBend;
    json["glideMs"] = glideMs;
    json["entries"] = ofJson::array();

    for(const auto &entry : progression) {
        json["entries"].push_back(entry.toJson());
    }

    return json;
}

void chordSequence::deserializeState(const ofJson &json, bool forceInstant) {
    outputSize = std::max(1, json.value("outputSize", 4));
    expandOutput = json.value("expandOutput", false);
    sortOutput = json.value("sortOutput", false);
    globalTranspose = json.value("globalTranspose", 0);
    globalInvert = json.value("globalInvert", 0);
    pitchBend = json.value("pitchBend", 0.0f);
    glideMs = json.value("glideMs", 0.0f);

    progression.clear();
    if(json.contains("entries") && json["entries"].is_array()) {
        for(const auto &entryJson : json["entries"]) {
            progression.push_back(chordSequenceEntry::fromJson(entryJson));
        }
    }

    if(progression.empty()) {
        initializeDefaultProgression();
    }

    sanitizeProgression();
    refreshTargetOutput(forceInstant);
}

void chordSequence::storeToSlot(int slot) {
    if(slot < 0 || slot >= SnapshotSlots) return;

    chordSequenceSnapshot snapshot;
    snapshot.entries = progression;
    snapshot.name = snapshotSlots[slot].name.empty() ? "Snapshot " + ofToString(slot + 1) : snapshotSlots[slot].name;
    snapshot.outputSize = outputSize;
    snapshot.expandOutput = expandOutput;
    snapshot.sortOutput = sortOutput;
    snapshot.globalTranspose = globalTranspose;
    snapshot.globalInvert = globalInvert;
    snapshot.pitchBend = pitchBend;
    snapshot.glideMs = glideMs;
    snapshot.hasData = true;

    snapshotSlots[slot] = snapshot;
    activeSnapshotSlot = slot;
    saveSnapshotToDisk(slot);
}

void chordSequence::recallSlot(int slot) {
    if(slot < 0 || slot >= SnapshotSlots) return;
    if(!snapshotSlots[slot].hasData) return;

    activeSnapshotSlot = slot;
    outputSize = snapshotSlots[slot].outputSize;
    expandOutput = snapshotSlots[slot].expandOutput;
    sortOutput = snapshotSlots[slot].sortOutput;
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
