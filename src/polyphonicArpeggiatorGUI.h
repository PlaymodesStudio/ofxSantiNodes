#pragma once
#include "ofxOceanodeNodeModel.h"

#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

struct polyphonicArpeggiatorGUISnapshot {
    int sourceMode = 0;
    bool internalClockMode = false;
    float beatDiv = 1.0f;
    int seqSize = 16;
    std::vector<float> scale;
    int patternMode = 0;
    std::vector<int> idxPattern;
    int sourceStart = 0;
    int sourceStride = 1;
    bool pitchExpand = false;
    int expandStep = 12;
    int transpose = 0;
    bool sortPool = true;
    int sourceChangeMode = 0;

    int polyphony = 1;
    int polyInterval = 2;
    int skipSteps = 0;
    float strum = 0.0f;
    float strumRndm = 0.0f;
    int strumDir = 0;

    float octaveDev = 0.0f;
    int octaveDevRng = 1;
    float idxDev = 0.0f;
    int idxDevRng = 2;
    float pitchDev = 0.0f;
    int pitchDevRng = 2;

    float velBase = 0.8f;
    float velRndm = 0.1f;
    float eucAccStrength = 0.2f;

    int durBase = 100;
    int durRndm = 20;
    int durEucStrength = 50;

    int eucLen = 8;
    int eucHits = 8;
    int eucOff = 0;
    int eucAccLen = 4;
    int eucAccHits = 1;
    int eucAccOff = 0;
    int eucDurLen = 4;
    int eucDurHits = 4;
    int eucDurOff = 0;

    float stepChance = 1.0f;
    float noteChance = 1.0f;

    bool dynamicMode = false;
    bool accentOnsetMode = true;
    bool hasData = false;
    std::string name;
};

class polyphonicArpeggiatorGUI : public ofxOceanodeNodeModel {
public:
    polyphonicArpeggiatorGUI();
    ~polyphonicArpeggiatorGUI() override;

    void setup() override;
    void update(ofEventArgs &e) override;
    void draw(ofEventArgs &e) override;
    void setBpm(float bpm) override;
    void presetSave(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;

private:
    struct StepPreviewInfo {
        std::vector<float> notes;
        float velocity = 0.0f;
        int duration = 0;
        bool gate = false;
        bool accent = false;
    };

    static constexpr int MaxSequenceSize = 128;
    static constexpr int MaxPolyphony = 16;
    static constexpr int SnapshotSlots = 16;

    enum SourceMode {
        Scale = 0,
        ChordPool = 1
    };

    enum SourceChangeMode {
        KeepPhase = 0,
        ResetPattern = 1
    };

    ofParameter<void> trigger;
    ofParameter<void> reset;
    ofParameter<void> resetNext;

    ofParameter<bool> showEditor;
    ofParameter<float> editorWidth;
    ofParameter<float> editorHeight;
    ofParameter<int> snapshotRecall;

    ofParameter<int> sourceMode;
    ofParameter<std::vector<float>> notePoolIn;
    ofParameter<std::vector<float>> scale;
    ofParameter<bool> sortPool;
    ofParameter<int> sourceChangeMode;

    ofParameter<int> patternMode;
    ofParameter<std::vector<int>> idxPattern;
    ofParameter<bool> internalClockMode;
    ofParameter<float> beatDiv;
    ofParameter<int> seqSize;
    ofParameter<int> sourceStart;
    ofParameter<int> sourceStride;
    ofParameter<bool> pitchExpand;
    ofParameter<int> expandStep;
    ofParameter<int> transpose;
    ofParameter<bool> dynamicMode;
    ofParameter<bool> accentOnsetMode;

    ofParameter<int> polyphony;
    ofParameter<int> polyInterval;
    ofParameter<int> skipSteps;
    ofParameter<float> strum;
    ofParameter<float> strumRndm;
    ofParameter<int> strumDir;

    ofParameter<float> octaveDev;
    ofParameter<int> octaveDevRng;
    ofParameter<float> idxDev;
    ofParameter<int> idxDevRng;
    ofParameter<float> pitchDev;
    ofParameter<int> pitchDevRng;

    ofParameter<int> eucLen;
    ofParameter<int> eucHits;
    ofParameter<int> eucOff;
    ofParameter<float> stepChance;
    ofParameter<float> noteChance;

    ofParameter<float> velBase;
    ofParameter<float> velRndm;
    ofParameter<int> eucAccLen;
    ofParameter<int> eucAccHits;
    ofParameter<int> eucAccOff;
    ofParameter<float> eucAccStrength;

    ofParameter<int> durBase;
    ofParameter<int> durRndm;
    ofParameter<int> eucDurLen;
    ofParameter<int> eucDurHits;
    ofParameter<int> eucDurOff;
    ofParameter<int> durEucStrength;

    ofParameter<std::vector<float>> pitchOut;
    ofParameter<std::vector<int>> gateOut;
    ofParameter<std::vector<float>> velocityOut;
    ofParameter<std::vector<float>> durOut;
    ofParameter<std::vector<float>> gateVelOut;
    ofParameter<std::vector<int>> eucGateOut;
    ofParameter<std::vector<int>> eucAccOut;
    ofParameter<std::vector<int>> eucDurOut;

    ofParameter<float> morphTime;

    ofEventListeners listeners;

    int currentStep = 0;
    int highlightedStep = 0;
    bool shouldReset = false;
    int onsetCounter = 0;
    int absoluteStepCounter = 0;
    bool isMorphing = false;
    float morphStartTime = 0.0f;
    int activeSnapshotSlot = -1;
    float currentBpm = 120.0f;
    bool internalClockNeedsSync = true;

    std::vector<bool> euclideanPattern;
    std::vector<bool> euclideanAccents;
    std::vector<bool> euclideanDurations;
    std::vector<float> activeSourceValues;
    std::vector<float> currentPitches;
    std::vector<int> currentGates;
    std::vector<float> currentVelocities;
    std::vector<float> currentDurations;
    std::vector<int> noteDurationsMs;
    std::vector<uint64_t> noteStartTimes;
    std::vector<float> deviationValues;
    std::vector<bool> stepGates;

    std::array<polyphonicArpeggiatorGUISnapshot, SnapshotSlots> snapshotSlots;
    polyphonicArpeggiatorGUISnapshot startSnapshot;
    polyphonicArpeggiatorGUISnapshot targetSnapshot;

    bool snapshotsSectionExpanded = true;
    bool sourceSectionExpanded = true;
    bool patternSectionExpanded = true;
    bool polyphonySectionExpanded = true;
    bool euclideanSectionExpanded = true;
    bool velocityDurationSectionExpanded = true;
    bool visualizationSectionExpanded = true;
    bool outputSectionExpanded = true;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist01;

    void setupListeners();
    void resizeStateVectors(int size);
    void clearActiveVoices(bool resetCounters);
    void onTrigger();
    void onReset();
    void onResetNext();
    void processStep();
    void syncUserPatternToSequenceSize();
    std::string describeBeatDiv(float beatDivision) const;

    void generateEuclideanPattern(std::vector<bool> &pattern, int length, int hits, int offset);
    void rebuildSourceMaterial();
    void handleSourceMaterialChange();
    float getSourceValue(int index) const;
    void rebuildDeviations();
    void rebuildPitchSequence();
    void rebuildEuclideanOutputs();
    void updateOutputs();

    float computeStepVelocity(int stepIndex);
    int computeStepDuration(int stepIndex);
    float computePreviewVelocity(int stepIndex) const;
    int computePreviewDuration(int stepIndex) const;
    float computeStrumOffset(int voiceIndex, int totalVoices);
    StepPreviewInfo buildStepPreview(int stepIndex) const;

    void drawEditor();
    void drawSnapshotsSection();
    void drawSourceSection();
    void drawPatternSection();
    void drawPolyphonySection();
    void drawEuclideanSection();
    void drawVelocityDurationSection();
    void drawVisualizationSection();
    void drawOutputSection();
    void drawUserPatternEditor(float width, float height);
    void drawSourcePoolPreview(float width, float height) const;
    void drawEuclideanPreview(float width, float height) const;
    void drawArpGrid(float width, float height) const;

    std::vector<float> getSourcePreviewNotes() const;
    std::vector<float> getActiveOutputPreviewNotes() const;
    std::string summarizeVector(const std::vector<float> &values, int maxItems, int precision = 1) const;
    std::string summarizeIntVector(const std::vector<int> &values, int maxItems) const;

    void storeToSlot(int slot);
    void recallSlot(int slot);
    void updateMorph();
    void saveSnapshotToDisk(int slot) const;
    void loadSnapshotFromDisk(int slot);
    void loadAllSnapshotsFromDisk();
    void deleteSnapshotFromDisk(int slot);
    std::string getSnapshotsFolderPath() const;
    std::string getSnapshotFilePath(int slot) const;
};

#endif
