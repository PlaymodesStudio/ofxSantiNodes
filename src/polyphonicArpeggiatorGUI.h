#pragma once
#include "santiNodesTransportCompat.h"
#include "ofxOceanodeNodeModel.h"

#ifdef OFX_OCEANODE_HAS_GLOBAL_TRANSPORT

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <vector>

struct polyphonicArpeggiatorGUISnapshot {
    int sourceMode = 0;
    bool internalClockMode = false;
    bool oneShotMode = false;
    float beatDiv = 1.0f;
    int pulseMode = 0;
    std::vector<int> pulseStepPattern;
    float geigerSpeed = 1.0f;
    float geigerDensity = 0.45f;
    float geigerPeriodicity = 0.75f;
    float geigerChaos = 0.35f;
    int seqSize = 16;
    std::vector<float> scale;
    int patternMode = 0;
    int patternSeed = 0;
    bool patternRandomizeSeedEachCycle = false;
    std::vector<int> idxPattern;
    int modulo = 0;
    int sourceStart = 0;
    int sourceStride = 1;
    int stepShift = 0;
    float rndShiftChance = 0.0f;
    int rndShiftRange = 0;
    int rndShiftQuant = 1;
    int octave = 0;
    int octaveFold = 0;
    bool foldPoly = true;
    float root = 0.0f;
    bool pitchExpand = true;
    int expandStep = 12;
    int transpose = 0;
    bool sortPool = true;
    bool removeDuplicates = false;
    int sourceChangeMode = 0;

    int polyphony = 1;
    int polyInterval = 2;
    int polyAccent = 0;
    bool addBass = false;
    int bassPatternMode = 0;
    int bassAlternateSteps = 2;
    int bassAlternateShift = 0;
    int bassEucLen = 8;
    int bassEucHits = 2;
    int bassEucOff = 0;
    int bassOctave = -2;
    float bassProb = 1.0f;
    bool bassOnAccent = false;
    int bassOctRnd = 0;
    float bassOctRndChance = 0.0f;
    int bassOctRndSeed = 0;
    bool bassOctRndRandomizeSeedEachCycle = false;
    int bassPolyphony = 1;
    float bassVelBase = 0.75f;
    float bassVelRndm = 0.0f;
    bool bassPitchModified = true;
    int skipSteps = 0;
    float strum = 0.0f;
    float strumRndm = 0.0f;
    int strumRndSeed = 0;
    bool strumRndRandomizeSeedEachCycle = false;
    float swing = 0.0f;
    int strumDir = 0;
    int strumDirSeed = 0;
    bool strumDirRandomizeSeedEachCycle = false;
    float positionRndm = 0.0f;
    int positionRndSeed = 0;
    bool positionRndRandomizeSeedEachCycle = false;

    float octaveDev = 0.0f;
    int octaveDevRng = 1;
    float idxDev = 0.0f;
    int idxDevRng = 2;
    float pitchDev = 0.0f;
    int pitchDevRng = 2;

    float velBase = 0.8f;
    float velRndm = 0.1f;
    int velRndSeed = 0;
    bool velRndRandomizeSeedEachCycle = false;
    float velStepRndm = 0.0f;
    int velStepRndSeed = 0;
    bool velStepRndRandomizeSeedEachCycle = false;
    std::vector<float> velocityCurve;
    float velocitySlowRndm = 0.0f;
    float velocitySlowSpeed = 0.25f;
    int velocitySlowSeed = 0;
    int velocityLfoShape = 4;
    float velocityLfoPhase = 0.0f;
    float velocityLfoPulseWidth = 0.5f;
    int accentPatternMode = 1;
    int accentAlternateSteps = 2;
    int accentAlternateShift = 0;
    float accentChance = 0.5f;
    int accentSeed = 0;
    bool accentRandomizeSeedEachCycle = false;
    std::vector<int> accentStepPattern;
    float eucAccStrength = 0.2f;

    float durBase = 1.0f;
    float durRndm = 0.0f;
    int durationPatternMode = 1;
    int durationAlternateSteps = 2;
    int durationAlternateShift = 0;
    float durationChance = 0.5f;
    int durationSeed = 0;
    bool durationRandomizeSeedEachCycle = false;
    std::vector<int> durationStepPattern;
    float durEucStrength = 0.0f;
    bool durationRndPerStep = true;
    int outputHistoryWindowMs = 4000;

    int eucLen = 8;
    int eucHits = 8;
    int eucOff = 0;
    int eucAccLen = 4;
    int eucAccHits = 1;
    int eucAccOff = 0;
    int eucDurLen = 4;
    int eucDurHits = 4;
    int eucDurOff = 0;

    float seqProb = 1.0f;
    int seqProbCycles = 1;
    float runGateBeats = 16.0f;
    float runGateChance = 1.0f;
    float runGatePhase = 0.0f;
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
    void presetRecallBeforeSettingParameters(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;

private:
    struct EditorPublishAction {
        std::string key;
        std::function<bool()> publish;
        std::function<bool()> unpublish;
        std::function<bool()> isPublished;
        std::function<bool()> isAvailableInNode;
    };

    struct StepPreviewInfo {
        std::vector<float> notes;
        std::vector<float> noteVelocities;
        std::vector<int> noteDurations;
        std::vector<bool> noteAccents;
        std::vector<bool> noteDurationAccents;
        float velocity = 0.0f;
        int duration = 0;
        bool gate = false;
        bool accent = false;
    };

    struct OutputHistoryEvent {
        float pitch = 60.0f;
        float velocity = 0.0f;
        int durationMs = 100;
        uint64_t startTimeMs = 0;
    };

    static constexpr int MaxSequenceSize = 128;
    static constexpr int MaxPolyphony = 16;
    static constexpr int MaxSnapshotRecall = 4096;
    static constexpr int VelocityCurveResolution = 64;

    enum SourceMode {
        Scale = 0,
        ChordPool = 1
    };

    enum SourceChangeMode {
        KeepPhase = 0,
        ResetPattern = 1
    };

    enum PulseMode {
        PeriodicPulse = 0,
        EuclideanPulse = 1,
        GeigerPulse = 2,
        StepSeqPulse = 3
    };

    enum EventPatternMode {
        AlternateEventPattern = 0,
        EuclideanEventPattern = 1,
        RandomStepEventPattern = 2,
        RandomNoteEventPattern = 3,
        StepSeqEventPattern = 4
    };

    enum BassPatternMode {
        BassAlternatePattern = 0,
        BassEuclideanPattern = 1,
        BassRandomPattern = 2,
        BassVelAccentedPattern = 3,
        BassDurAccentedPattern = 4,
        BassStartPattern = 5
    };

    enum VelocityLfoShape {
        VelocityLfoSin = 0,
        VelocityLfoSaw = 1,
        VelocityLfoInvSaw = 2,
        VelocityLfoPulse = 3,
        VelocityLfoSlowRandom = 4
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
    ofParameter<bool> removeDuplicates;
    ofParameter<int> sourceChangeMode;

    ofParameter<int> patternMode;
    ofParameter<int> patternSeed;
    ofParameter<bool> patternRandomizeSeedEachCycle;
    ofParameter<std::vector<int>> idxPattern;
    ofParameter<int> modulo;
    ofParameter<bool> internalClockMode;
    ofParameter<bool> oneShotMode;
    ofParameter<float> beatDiv;
    ofParameter<int> pulseMode;
    ofParameter<std::vector<int>> pulseStepPattern;
    ofParameter<float> geigerSpeed;
    ofParameter<float> geigerDensity;
    ofParameter<float> geigerPeriodicity;
    ofParameter<float> geigerChaos;
    ofParameter<int> seqSize;
    ofParameter<int> sourceStart;
    ofParameter<int> sourceStride;
    ofParameter<int> stepShift;
    ofParameter<float> rndShiftChance;
    ofParameter<int> rndShiftRange;
    ofParameter<int> rndShiftQuant;
    ofParameter<int> octave;
    ofParameter<int> octaveFold;
    ofParameter<bool> foldPoly;
    ofParameter<float> root;
    ofParameter<bool> pitchExpand;
    ofParameter<int> expandStep;
    ofParameter<int> transpose;
    ofParameter<bool> dynamicMode;
    ofParameter<bool> accentOnsetMode;

    ofParameter<int> polyphony;
    ofParameter<int> polyInterval;
    ofParameter<int> polyAccent;
    ofParameter<bool> addBass;
    ofParameter<int> bassPatternMode;
    ofParameter<int> bassAlternateSteps;
    ofParameter<int> bassAlternateShift;
    ofParameter<int> bassEucLen;
    ofParameter<int> bassEucHits;
    ofParameter<int> bassEucOff;
    ofParameter<int> bassOctave;
    ofParameter<float> bassProb;
    ofParameter<bool> bassOnAccent;
    ofParameter<int> bassOctRnd;
    ofParameter<float> bassOctRndChance;
    ofParameter<int> bassOctRndSeed;
    ofParameter<bool> bassOctRndRandomizeSeedEachCycle;
    ofParameter<int> bassPolyphony;
    ofParameter<float> bassVelBase;
    ofParameter<float> bassVelRndm;
    ofParameter<bool> bassPitchModified;
    ofParameter<int> skipSteps;
    ofParameter<float> strum;
    ofParameter<float> strumRndm;
    ofParameter<int> strumRndSeed;
    ofParameter<bool> strumRndRandomizeSeedEachCycle;
    ofParameter<float> swing;
    ofParameter<int> strumDir;
    ofParameter<int> strumDirSeed;
    ofParameter<bool> strumDirRandomizeSeedEachCycle;
    ofParameter<float> positionRndm;
    ofParameter<int> positionRndSeed;
    ofParameter<bool> positionRndRandomizeSeedEachCycle;

    ofParameter<float> octaveDev;
    ofParameter<int> octaveDevRng;
    ofParameter<float> idxDev;
    ofParameter<int> idxDevRng;
    ofParameter<float> pitchDev;
    ofParameter<int> pitchDevRng;

    ofParameter<int> eucLen;
    ofParameter<int> eucHits;
    ofParameter<int> eucOff;
    ofParameter<float> seqProb;
    ofParameter<int> seqProbCycles;
    ofParameter<float> runGateBeats;
    ofParameter<float> runGateChance;
    ofParameter<float> runGatePhase;
    ofParameter<float> stepChance;
    ofParameter<float> noteChance;

    ofParameter<float> velBase;
    ofParameter<float> velRndm;
    ofParameter<int> velRndSeed;
    ofParameter<bool> velRndRandomizeSeedEachCycle;
    ofParameter<float> velStepRndm;
    ofParameter<int> velStepRndSeed;
    ofParameter<bool> velStepRndRandomizeSeedEachCycle;
    ofParameter<std::vector<float>> velocityCurve;
    ofParameter<float> velocitySlowRndm;
    ofParameter<float> velocitySlowSpeed;
    ofParameter<int> velocitySlowSeed;
    ofParameter<int> velocityLfoShape;
    ofParameter<float> velocityLfoPhase;
    ofParameter<float> velocityLfoPulseWidth;
    ofParameter<int> accentPatternMode;
    ofParameter<int> accentAlternateSteps;
    ofParameter<int> accentAlternateShift;
    ofParameter<float> accentChance;
    ofParameter<int> accentSeed;
    ofParameter<bool> accentRandomizeSeedEachCycle;
    ofParameter<std::vector<int>> accentStepPattern;
    ofParameter<int> eucAccLen;
    ofParameter<int> eucAccHits;
    ofParameter<int> eucAccOff;
    ofParameter<float> eucAccStrength;

    ofParameter<float> durBase;
    ofParameter<float> durRndm;
    ofParameter<int> durationPatternMode;
    ofParameter<int> durationAlternateSteps;
    ofParameter<int> durationAlternateShift;
    ofParameter<float> durationChance;
    ofParameter<int> durationSeed;
    ofParameter<bool> durationRandomizeSeedEachCycle;
    ofParameter<std::vector<int>> durationStepPattern;
    ofParameter<int> eucDurLen;
    ofParameter<int> eucDurHits;
    ofParameter<int> eucDurOff;
    ofParameter<float> durEucStrength;
    ofParameter<bool> durationRndPerStep;

    ofParameter<std::vector<float>> pitchOut;
    ofParameter<std::vector<int>> gateOut;
    ofParameter<std::vector<float>> velocityOut;
    ofParameter<std::vector<float>> durOut;
    ofParameter<std::vector<float>> gateVelOut;
    ofParameter<std::vector<int>> eucGateOut;
    ofParameter<std::vector<int>> eucAccOut;
    ofParameter<std::vector<int>> eucDurOut;
    ofParameter<int> outputHistoryWindowMs;

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
    double currentTransportBeatPosition = 0.0;
    bool internalClockNeedsSync = true;
    bool oneShotCycleActive = false;
    bool sourceChangePending = false;
    bool currentSequenceCycleShouldPlay = true;
    bool sequenceCycleDecisionPending = true;
    int skippedSequenceCyclesRemaining = 0;
    int currentCycleRandomStepShift = 0;
    int64_t runGateWindowIndex = 0;
    bool runGateWindowStateValid = false;
    bool runGateCurrentShouldPlay = true;
    bool geigerTransportPulseActive = false;
    float pendingTransportOffsetMs = 0.0f;
    float editorZoom = 1.0f;
    uint64_t observedSnapshotStorageRevision = 0;
    uint64_t lastExternalTriggerTimeMs = 0;
    int oneShotStepsRemaining = 0;
    int velocityCurveActiveHandle = 0;
    float velocityCurveEditorStart = 0.0f;
    float velocityCurveEditorEnd = 0.0f;
    float velocityCurveEditorInflection = 0.5f;
    float velocityCurveEditorSteepness = 1.0f;
    float velocityCurveDragStartMouseX = 0.0f;
    float velocityCurveDragStartMouseY = 0.0f;
    float velocityCurveDragStartInflection = 0.5f;
    float velocityCurveDragStartSteepness = 1.0f;

    std::vector<bool> euclideanPattern;
    std::vector<bool> euclideanAccents;
    std::vector<bool> euclideanDurations;
    std::vector<bool> euclideanBass;
    std::vector<float> activeSourceValues;
    std::vector<float> pendingSourceValues;
    std::vector<float> currentPitches;
    std::vector<int> currentGates;
    std::vector<float> currentVelocities;
    std::vector<float> currentDurations;
    std::vector<int> noteDurationsMs;
    std::vector<uint64_t> noteStartTimes;
    std::vector<float> deviationValues;
    std::vector<bool> stepGates;
    std::vector<float> recentExternalStepDurationsMs;
    std::vector<OutputHistoryEvent> outputHistory;

    std::map<int, polyphonicArpeggiatorGUISnapshot> snapshotSlots;
    polyphonicArpeggiatorGUISnapshot startSnapshot;
    polyphonicArpeggiatorGUISnapshot targetSnapshot;
    std::vector<EditorPublishAction> publishableEditorParameters;
    std::vector<std::string> publishedEditorParameterKeys;
    std::map<std::string, std::shared_ptr<ofxOceanodeAbstractParameter>> publishedEditorParameterHandles;
    bool publishedEditorSeparatorAdded = false;

    bool snapshotsSectionExpanded = true;
    bool topSectionExpanded = true;
    bool tabsSectionExpanded = true;
    bool rhythmSectionExpanded = true;
    bool visualizationSectionExpanded = true;
    bool outputSectionExpanded = true;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist01;

    void setupListeners();
    void initializePublishableEditorParameters();
    void resizeStateVectors(int size);
    void clearActiveVoices(bool resetCounters);
    void onTrigger();
    void onReset();
    void onResetNext();
    void processStep();
    int getOutputSlotCount() const;
    void syncUserPatternToSequenceSize();
    void syncPulseStepPatternToSequenceSize();
    void syncAccentStepPatternToSequenceSize();
    void syncDurationStepPatternToSequenceSize();
    void syncVelocityCurveShape();
    void syncVelocityCurveEditorStateFromCurve();
    void rebuildVelocityCurveFromEditorState();
    std::string describeBeatDiv(float beatDivision) const;
    const char *getPulseModeLabel() const;
    const char *getVelocityLfoShapeLabel() const;

    void generateEuclideanPattern(std::vector<bool> &pattern, int length, int hits, int offset);
    std::vector<float> buildSourceMaterialFromParameters() const;
    void rebuildSourceMaterial();
    void handleSourceMaterialChange();
    void applyPendingSourceMaterialChange();
    float getSourceValue(int index) const;
    float mapOutputPitch(float pitch, bool applyFold = true) const;
    float mapPitchWithDeviation(float basePitch, float deviation, bool applyFold = true) const;
    float mapBassPitch(int octaveStackOffset = 0, int randomOctaveOffset = 0) const;
    int findAvailableOutputIndex(int preferredIndex, std::vector<bool> &claimedSlots) const;
    int getEffectiveStepShift() const;
    int getShiftedSequenceStepIndex(int stepIndex) const;
    int getPatternTraversalSize() const;
    int getBidirectionalPatternOffset(int stepIndex, bool startAscending) const;
    int getPatternOffsetForStepLive(int stepIndex);
    int getPatternOffsetForStepPreview(int stepIndex) const;
    float generateDeviationForSourceIndex(int sourceIndex, std::mt19937 &generator) const;
    void rebuildDeviations();
    void rebuildPitchSequence();
    void rebuildEuclideanOutputs();
    void updateOutputs();
    float computeGeigerPulseProbability(double beatPosition) const;
    bool isGeigerGridLockedPulseForStep(int stepIndex) const;
    bool isPulseActiveForStepLive(int stepIndex);
    bool isPulseActiveForStepPreview(int stepIndex) const;
    bool isStepPatternAlternateActive(int stepIndex, int every, int shift = 0) const;
    float computePatternRandomUnit(int seed, int stepIndex, int voiceIndex, int streamTag) const;
    bool isAccentStepActiveLive(int stepIndex);
    bool isAccentStepActivePreview(int stepIndex) const;
    bool isDurationStepActiveLive(int stepIndex);
    bool isDurationStepActivePreview(int stepIndex) const;
    bool isBassStepActiveLive(int stepIndex, bool stepAccentActive, bool stepDurationAccent);
    bool isBassStepActivePreview(int stepIndex, bool stepAccentActive, bool stepDurationAccent) const;
    bool isAccentNoteActiveLive(int stepIndex, int voiceIndex, bool stepAccentActive);
    bool isAccentNoteActivePreview(int stepIndex, int voiceIndex, bool stepAccentActive) const;
    bool isDurationNoteActiveLive(int stepIndex, int voiceIndex, bool stepAccentActive);
    bool isDurationNoteActivePreview(int stepIndex, int voiceIndex, bool stepAccentActive) const;
    bool isAccentPreviewLaneActive(int stepIndex) const;
    bool isDurationPreviewLaneActive(int stepIndex) const;
    void randomizePatternSeedsForNewCycle();
    void randomizeCycleStepShift();
    double getRunGateBeatPosition() const;
    float getRunGatePhaseNormalized() const;
    void updateRunGateWindowState();
    void recordOutputHistoryEvent(float pitch, float velocity, int durationMs, uint64_t startTimeMs);
    void pruneOutputHistory(uint64_t nowMs);
    float getVisualizationStepDurationMs() const;

    float evaluateVelocityCurve(float normalizedPosition) const;
    float evaluateVelocitySigmoid(float normalizedPosition, float inflection, float steepness) const;
    float computeVelocityCurveOffsetForStep(int stepIndex) const;
    float evaluateVelocityLfoWaveform(double beatPosition) const;
    float computeLiveVelocityLfoOffset() const;
    float computePreviewVelocityLfoOffset(double sequencePosition) const;
    float computeLiveStepVelocityRandomOffset(int modulationStepIndex) const;
    float computePreviewStepVelocityRandomOffset(int modulationStepIndex) const;
    float computeLiveNoteVelocity(int modulationStepIndex, int voiceIndex, bool accentActive, float stepRandomOffset);
    float computePreviewNoteVelocity(int eventStepIndex, int modulationStepIndex, int voiceIndex, bool accentActive, float stepRandomOffset) const;
    int computeLiveBassOctaveOffset(int stepIndex) const;
    int computePreviewBassOctaveOffset(int stepIndex) const;
    float computeLiveBassVelocity();
    float computePreviewBassVelocity(int stepIndex, int voiceIndex) const;
    float computeLiveDurationRandomOffset();
    float computePreviewDurationRandomOffset(int stepIndex, int voiceIndex) const;
    int computeNoteDuration(bool accentActive, float randomOffset) const;
    float computeStrumOffset(int stepIndex, int voiceIndex, int totalVoices) const;
    float computePreviewStrumOffset(int stepIndex, int voiceIndex, int totalVoices) const;
    float computeLivePositionOffset(int stepIndex, int voiceIndex, int totalVoices) const;
    float computePreviewPositionOffset(int stepIndex, int voiceIndex, int totalVoices) const;
    StepPreviewInfo buildStepPreview(int stepIndex) const;

    void drawEditor();
    void drawTopBarSection();
    void drawSnapshotsSection();
    void drawSourceSection();
    void drawPatternSection();
    void drawPolyphonySection();
    void drawBassSection();
    void drawPositionSection();
    void drawEuclideanSection();
    void drawVelocitySection();
    void drawVelocityDurationSection();
    void drawDurationSection();
    void drawRhythmSection();
    void drawVisualizationSection();
    void drawOutputSection();
    void drawOutputHistoryRoll(float width, float height) const;
    void drawUserPatternEditor(float width, float height);
    void drawPulseStepPatternEditor(float width, float height);
    void drawRunGatePhasor(float width, float height) const;
    void drawAccentStepPatternEditor(float width, float height);
    void drawDurationStepPatternEditor(float width, float height);
    void drawVelocityCurveEditor(float width, float height);
    void drawVelocityLfoScope(float width, float height) const;
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
    int getNextAvailableSnapshotSlot() const;
    void refreshSnapshotsFromSharedStorage(bool force = false);
    bool isEditorParameterPublished(const std::string& key) const;
    const EditorPublishAction* findPublishableEditorParameter(const std::string& key) const;
    bool publishEditorParameterToNode(const std::string& key);
    bool unpublishEditorParameterFromNode(const std::string& key);
    void syncPublishedEditorParameters(const std::vector<std::string>& keys);
    void drawPublishedLabelUnderline(const std::string& key,
                                     const char* label,
                                     float frameWidth = -1.0f,
                                     bool checkbox = false) const;
    void drawPublishedCurrentItemUnderline(const std::string& key) const;
    void drawNodePublishMenuItems(const std::string& key);
    void drawNodePublishContextMenu(const std::string& key,
                                    const char* label = nullptr,
                                    float frameWidth = -1.0f,
                                    bool checkbox = false);

    template<typename ParameterType>
    bool publishEditorParameterToNode(const std::string& key,
                                      ofParameter<ParameterType>& parameter,
                                      ofxOceanodeParameterFlags flags = 0) {
        if(getParameterGroup().contains(parameter.getEscapedName())) return false;
        if(!publishedEditorSeparatorAdded) {
            addSeparator("Published", ofColor(200));
            publishedEditorSeparatorAdded = true;
        }
        publishedEditorParameterHandles[key] = addParameter(parameter, flags);
        if(std::find(publishedEditorParameterKeys.begin(), publishedEditorParameterKeys.end(), key) == publishedEditorParameterKeys.end()) {
            publishedEditorParameterKeys.push_back(key);
        }
        return true;
    }

    template<typename ParameterType>
    bool publishEditorDropdownParameterToNode(const std::string& key,
                                              ofParameter<ParameterType>& parameter,
                                              const std::vector<std::string>& options,
                                              ofxOceanodeParameterFlags flags = 0) {
        if(getParameterGroup().contains(parameter.getEscapedName())) return false;
        if(!publishedEditorSeparatorAdded) {
            addSeparator("Published", ofColor(200));
            publishedEditorSeparatorAdded = true;
        }
        auto published = addParameter(parameter, flags);
        published->setDropdownOptions(options);
        publishedEditorParameterHandles[key] = published;
        if(std::find(publishedEditorParameterKeys.begin(), publishedEditorParameterKeys.end(), key) == publishedEditorParameterKeys.end()) {
            publishedEditorParameterKeys.push_back(key);
        }
        return true;
    }

    template<typename ParameterType>
    bool isManagedByMainParameterGroup(const ofParameter<ParameterType>& parameter) {
        return getParameterGroup().contains(parameter.getEscapedName());
    }

    template<typename ParameterType>
    void saveEditorStateValue(ofJson& state, const char* key, const ofParameter<ParameterType>& parameter) {
        if(isManagedByMainParameterGroup(parameter)) return;
        state[key] = parameter.get();
    }

    template<typename ParameterType>
    void loadEditorStateValue(const ofJson& state, const char* key, ofParameter<ParameterType>& parameter) {
        if(isManagedByMainParameterGroup(parameter)) return;
        if(!state.contains(key)) return;
        try {
            parameter = state.at(key).get<ParameterType>();
        } catch(...) {
        }
    }
};

#endif
