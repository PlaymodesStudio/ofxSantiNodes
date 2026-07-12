#pragma once

#include "santiNodesTransportCompat.h"
#include "ofxOceanodeNodeModel.h"
#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT
#include <array>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

struct chordSequenceLibraryItem {
    std::string name;
    std::vector<float> values;
};

struct chordSequenceFunctionalVariant {
    std::string label;
    int degree = 1;
};

struct chordSequenceEntry {
    enum Mode {
        Chord = 0,
        Scale = 1,
        Cypher = 2,
        Degree = 3,
        Functional = 4
    };

    int mode = Chord;
    int itemIndex = 0;
    std::string itemName;
    int functionalGroup = 0;
    int functionalVariantIndex = 0;
    std::string functionalVariantLabel;
    int degree = 1;
    int chordSize = 4;
    int stepInterval = 2;
    float diatonicDeviationProbability = 0.0f;
    int diatonicDeviationRange = 0;
    float beatDuration = 1.0f;
    std::vector<float> markovWeights;
    int transpose = 0;
    int inversion = 0;

    ofJson toJson() const;
    static chordSequenceEntry fromJson(const ofJson &json);
};

struct chordSequenceOutputConfig {
    enum VoicingMode {
        None = 0,
        Close = 1,
        Open = 2,
        Drop2 = 3,
        Drop3 = 4,
        Shell = 5
    };

    enum SourceMode {
        Chord = 0,
        Scale = 1,
        Root = 2,
        Key = 3,
        ChordSum = 4
    };

    int octave = 0;
    int transpose = 0;
    float pitchBend = 0.0f;
    float perNoteDetune = 0.0f;
    float octaveRandomProbability = 0.0f;
    int octaveRandomRange = 0;
    float chromaticDeviationProbability = 0.0f;
    int chromaticDeviationRange = 0;
    bool voiceLeading = false;
    int minNote = 0;
    int maxNote = 127;
    int sourceMode = Chord;
    bool addBass = false;
    int bassOct = -2;
    int inversion = 0;
    int voicingMode = None;
    float voicingSpread = 0.0f;
    bool fold12 = false;
    float glideMs = 0.0f;
    int outputSize = 4;
    bool expandOutput = false;
    bool sortOutput = false;

    ofJson toJson() const;
    static chordSequenceOutputConfig fromJson(const ofJson &json);
};

struct chordSequenceSnapshot {
    std::vector<chordSequenceEntry> entries;
    std::vector<chordSequenceOutputConfig> outputs;
    std::string name;
    int numOutputs = 1;
    int globalKey = 0;
    int globalScaleIndex = 0;
    std::string globalScaleName;
    int globalTranspose = 0;
    int globalInvert = 0;
    int transposeRandomRange = 0;
    int transposeRandomQuantization = 1;
    bool transposeRandomStep = false;
    int inversionRandomRange = 0;
    int inversionRandomQuantization = 1;
    bool inversionRandomStep = false;
    float globalPitchBend = 0.0f;
    int progressionOrder = 0;
    bool internalTimingEnabled = false;
    bool markovEnabled = false;
    int internalActiveStep = 0;
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
    void setBpm(float bpm) override;
    void presetSave(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;
    void presetHasLoaded() override;
    void loadBeforeConnections(ofJson &json) override;

private:
    static constexpr int SnapshotSlots = 16;
    static constexpr int DefaultSequenceSize = 4;
    static constexpr int MaxOutputs = 16;

    enum ProgressionOrder {
        InputIdx = 0,
        Ascendent = 1,
        Descendent = 2,
        Random = 3,
        Markov = 4
    };

    ofParameter<int> indexInput;
    ofParameter<int> numChordsParameter;
    ofParameter<int> transposeParameter;
    ofParameter<float> pitchBendParameter;
    ofParameter<int> inversionParameter;
    ofParameter<void> resetSequenceParameter;
    ofParameter<float> rootOutput;
    ofParameter<bool> showEditor;
    ofParameter<float> editorWidth;
    ofParameter<float> editorHeight;

    ofEventListeners listeners;

    std::vector<chordSequenceLibraryItem> chordLibrary;
    std::vector<chordSequenceLibraryItem> scaleLibrary;
    std::unordered_map<std::string, std::array<std::vector<chordSequenceFunctionalVariant>, 3>> functionalHarmonyLibrary;
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

    int numOutputs = 1;
    int globalKey = 0;
    int globalScaleIndex = 0;
    std::string globalScaleName;
    int globalTranspose = 0;
    int globalInvert = 0;
    int transposeRandomRange = 0;
    int transposeRandomQuantization = 1;
    bool transposeRandomStep = false;
    int inversionRandomRange = 0;
    int inversionRandomQuantization = 1;
    bool inversionRandomStep = false;
    int currentTransposeRandomOffset = 0;
    int currentInversionRandomOffset = 0;
    int effectiveGlobalTranspose = 0;
    int effectiveGlobalInvert = 0;
    bool pendingRandomationStepAdvance = false;
    bool pendingRandomationSequenceRestart = false;
    float globalPitchBend = 0.0f;
    int progressionOrder = InputIdx;
    bool internalTimingEnabled = false;
    bool markovEnabled = false;
    float currentBPM = 120.0f;
    int internalActiveStep = 0;
    double nextInternalStepBeat = -1.0;
    int lastRefreshedActiveIndex = -1;
    bool outputBuildDirty = true;
    float editorZoom = 1.0f;
    float editorFontZoom = 1.0f;
    float manualEditorZoom = 1.0f;
    std::mt19937 randomEngine;
    bool snapshotsSectionExpanded = true;
    bool globalSectionExpanded = true;
    bool randomationSectionExpanded = true;
    bool cypherSectionExpanded = true;
    bool stepsSectionExpanded = true;
    bool outputsSectionExpanded = true;

    std::vector<ofParameter<std::vector<float>>> outputs;
    std::vector<std::shared_ptr<ofParameter<int>>> outputSizeParameters;
    std::vector<std::unique_ptr<ofEventListener>> outputSizeParameterListeners;
    std::vector<chordSequenceOutputConfig> outputConfigs;
    std::vector<std::vector<float>> currentOutputs;
    std::vector<std::vector<float>> targetOutputs;
    std::vector<std::vector<float>> glideStartOutputs;
    std::vector<bool> outputIsGliding;
    std::vector<uint64_t> outputGlideStartTimeMs;

    void loadLibraries();
    void loadCypherAliases();
    void loadFunctionalHarmony();
    std::string resolvePitchClassFile(const std::string &fileName) const;
    std::vector<chordSequenceLibraryItem> parsePitchClassFile(const std::string &path) const;
    void loadImportSources();
    void reloadLibraries();

    void ensureOutputCount(int newCount);
    std::string outputName(int index) const;
    std::string outputSizeParameterName(int index) const;
    void rebuildOutputSizeParameters();
    void syncNodeGuiParametersFromState();

    void initializeDefaultProgression();
    void resizeProgression(int newSize);
    void sanitizeProgression();
    void sanitizeEntry(chordSequenceEntry &entry);
    void sanitizeMarkovRows();
    void sanitizeGlobalScaleSelection();
    bool parseChordTokenToEntry(const std::string &token, chordSequenceEntry &entry) const;
    bool parseCypherRootAndQuality(const std::string &input, float &rootValue, std::string &quality) const;
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
    int getGlobalScaleSafeIndex() const;
    int getResolvedEntryDegree(const chordSequenceEntry &entry) const;
    const std::array<std::vector<chordSequenceFunctionalVariant>, 3> *getCurrentFunctionalGroups() const;
    const std::vector<chordSequenceFunctionalVariant> &getFunctionalVariants(int functionalGroup) const;
    void applyFunctionalVariantToEntry(chordSequenceEntry &entry) const;
    std::vector<float> buildDegreeValues(const chordSequenceEntry &entry) const;
    std::vector<float> buildEntryIntervals(const chordSequenceEntry &entry) const;
    std::vector<float> getDiatonicReferenceScale(const chordSequenceEntry &entry) const;
    std::vector<float> applyDiatonicDeviation(const std::vector<float> &values, const chordSequenceEntry &entry) const;
    std::vector<float> applyChromaticDeviation(const std::vector<float> &values,
                                               float probability,
                                               int range) const;
    std::vector<float> buildChordSumValues() const;
    std::vector<float> buildOutputSourceValues(const chordSequenceEntry &entry,
                                               const chordSequenceOutputConfig &config,
                                               float &outputRoot) const;
    float getEntryRootValue(const chordSequenceEntry &entry) const;
    float getEntryDisplayRootPitchClass(const chordSequenceEntry &entry) const;
    std::string getEntryDisplayRootLabel(const chordSequenceEntry &entry) const;
    std::vector<float> buildEntryPreviewOutput(const chordSequenceEntry &entry) const;
    std::vector<float> applyVoicing(const std::vector<float> &values, int voicingMode) const;
    std::vector<float> applyVoicingSpread(const std::vector<float> &values, float spread) const;
    std::vector<float> applyVoiceLeading(const std::vector<float> &previousValues,
                                         const std::vector<float> &nextValues,
                                         int minNote,
                                         int maxNote) const;
    std::vector<float> applyRangeConstraints(const std::vector<float> &values,
                                             int minNote,
                                             int maxNote) const;
    std::vector<float> buildOutputValues(const chordSequenceEntry &entry,
                                         const chordSequenceOutputConfig &config,
                                         const std::vector<float> &previousValues) const;
    std::vector<float> adaptOutputSize(const std::vector<float> &values,
                                       int requestedSize,
                                       bool expandOutput,
                                       bool sortOutput) const;
    std::vector<float> applyInversion(const std::vector<float> &values, int inversion) const;
    bool usesInternalProgressionOrder() const;
    void setProgressionOrder(int order, bool refreshSequence);
    int resolveActiveIndex() const;
    float getStepDurationMs(int stepIndex) const;
    void resetInternalSequence(bool forceInstant, double anchorBeat = -1.0);
    int chooseNextInternalStep(int currentStep);
    void advanceInternalSequence();
    int generateRandomizedModifier(int range, int quantization);
    void updateEffectiveGlobalModifiers(bool sequenceRestart, bool stepAdvance, bool forceReroll = false);

    void refreshAllOutputs(bool forceInstant = false);
    void beginGlideTo(int outputIndex, const std::vector<float> &nextTarget, bool forceInstant);
    std::vector<float> getInterpolatedOutput(int outputIndex, float progress) const;
    float getGlideProgress(int outputIndex) const;
    std::vector<float> getDisplayedOutput(int outputIndex) const;
    float sampleVector(const std::vector<float> &values, size_t index) const;

    void drawEditor();
    void drawGlobalControls();
    void drawRandomationControls();
    void drawImportTools();
    void drawEntries();
    void drawEntryEditor(int index, float width);
    void drawOutputs();
    void drawOutputEditor(int index, float width);
    void drawSnapshotManager();

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

#endif
