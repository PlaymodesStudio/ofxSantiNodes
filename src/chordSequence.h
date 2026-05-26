#pragma once

#include "ofxOceanodeNodeModel.h"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

struct chordSequenceLibraryItem {
    std::string name;
    std::vector<float> values;
};

struct chordSequenceEntry {
    enum Mode {
        Chord = 0,
        Scale = 1,
        Cypher = 2
    };

    int mode = Chord;
    int itemIndex = 0;
    std::string itemName;
    int transpose = 0;
    int inversion = 0;

    ofJson toJson() const;
    static chordSequenceEntry fromJson(const ofJson &json);
};

struct chordSequenceSnapshot {
    std::vector<chordSequenceEntry> entries;
    std::string name;
    int outputSize = 4;
    bool expandOutput = false;
    bool sortOutput = false;
    int globalTranspose = 0;
    int globalInvert = 0;
    float pitchBend = 0.0f;
    float glideMs = 0.0f;
    bool hasData = false;

    ofJson toJson() const;
    static chordSequenceSnapshot fromJson(const ofJson &json);
};

class chordSequence : public ofxOceanodeNodeModel {
public:
    chordSequence();

    void setup() override;
    void update(ofEventArgs &e) override;
    void draw(ofEventArgs &e) override;
    void presetSave(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;
    void presetHasLoaded() override;

private:
    static constexpr int SnapshotSlots = 16;
    static constexpr int DefaultSequenceSize = 4;

    ofParameter<int> indexInput;
    ofParameter<std::vector<float>> chordOut;

    ofParameter<bool> showEditor;
    ofParameter<float> editorWidth;
    ofParameter<float> editorHeight;

    ofEventListeners listeners;

    std::vector<chordSequenceLibraryItem> chordLibrary;
    std::vector<chordSequenceLibraryItem> scaleLibrary;
    std::unordered_map<std::string, std::string> chordAliases;
    std::vector<chordSequenceEntry> progression;
    ofJson importedProgressionDatabase;
    ofJson jazzStandardsDatabase;
    std::vector<std::string> importedProgressionNames;
    std::vector<std::string> jazzStandardNames;
    int selectedImportedProgression = 0;
    int selectedJazzStandard = 0;
    std::array<char, 2048> importChordBuffer{};
    std::array<char, 128> importProgressionNameBuffer{};

    std::array<chordSequenceSnapshot, SnapshotSlots> snapshotSlots;

    int defaultChordIndex = 0;
    int defaultScaleIndex = 0;
    int activeSnapshotSlot = -1;

    int outputSize = 4;
    bool expandOutput = false;
    bool sortOutput = false;
    int globalTranspose = 0;
    int globalInvert = 0;
    float pitchBend = 0.0f;
    float glideMs = 0.0f;

    std::vector<float> currentOutput;
    std::vector<float> targetOutput;
    std::vector<float> glideStartOutput;
    bool isGliding = false;
    uint64_t glideStartTimeMs = 0;

    void loadLibraries();
    void loadCypherAliases();
    std::string resolvePitchClassFile(const std::string &fileName) const;
    std::vector<chordSequenceLibraryItem> parsePitchClassFile(const std::string &path) const;
    void loadImportSources();
    void reloadLibraries();

    void initializeDefaultProgression();
    void resizeProgression(int newSize);
    void sanitizeProgression();
    void sanitizeEntry(chordSequenceEntry &entry);
    bool parseChordTokenToEntry(const std::string &token, chordSequenceEntry &entry) const;
    std::string normalizeChordQuality(const std::string &quality) const;
    int getNoteValue(const std::string &note) const;
    std::vector<std::string> parseChordSequenceString(const std::string &chordString) const;
    std::vector<std::string> extractJazzStandardChords(int songIndex) const;
    void applyImportedChordList(const std::vector<std::string> &chords);
    void refreshImportedProgressionNames();
    int getNextImportedProgressionIndex() const;
    std::string getDefaultTimingStringForChords(const std::vector<std::string> &chords) const;
    bool appendImportedProgression(const std::string &name, const std::string &chordString);
    int findItemIndexByName(const std::vector<chordSequenceLibraryItem> &library, const std::string &name) const;
    int getDefaultIndexForMode(int mode) const;
    std::string getItemLabel(const chordSequenceEntry &entry) const;
    const std::vector<chordSequenceLibraryItem> &getLibraryForMode(int mode) const;
    std::vector<float> buildEntryCoreOutput(const chordSequenceEntry &entry) const;
    std::vector<float> buildEntryOutput(const chordSequenceEntry &entry) const;
    std::vector<float> adaptOutputSize(const std::vector<float> &values) const;
    std::vector<float> applyInversion(const std::vector<float> &values, int inversion) const;
    int resolveActiveIndex() const;
    float getSingleColumnWidth(float availableWidth) const;

    void refreshTargetOutput(bool forceInstant = false);
    void beginGlideTo(const std::vector<float> &nextTarget, bool forceInstant);
    std::vector<float> getInterpolatedOutput(float progress) const;
    float getGlideProgress() const;
    float sampleVector(const std::vector<float> &values, size_t index) const;

    void drawEditor();
    void drawGlobalControls();
    void drawImportTools();
    void drawEntries();
    void drawEntryEditor(int index, float width);
    void drawSnapshotManager();
    void drawOutputPreview();

    ofJson serializeCurrentState() const;
    void deserializeState(const ofJson &json, bool forceInstant);
    void storeToSlot(int slot);
    void recallSlot(int slot);
    void saveSnapshotToDisk(int slot) const;
    void loadSnapshotFromDisk(int slot);
    void loadAllSnapshotsFromDisk();
    void deleteSnapshotFromDisk(int slot);
    std::string getSnapshotsFolderPath() const;
    std::string getSnapshotFilePath(int slot) const;
    std::string getChordProgressionsFilePath() const;
};
