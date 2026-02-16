#ifndef polyphonicArpeggiator_h
#define polyphonicArpeggiator_h

#include "ofxOceanodeNodeModel.h"
#include <random>
#include <algorithm>

// Struct to hold a single snapshot state
struct ArpeggiatorSnapshot {
    // Sequence
    int seqSize;

    // Scale and Pattern
    vector<float> scale;
    int patternMode;
    vector<int> idxPattern;
    int degStart;
    int stepInterval;
    int transpose;

    // Polyphony
    int polyphony;
    int polyInterval;
    float strum;
    float strumRndm;
    int strumDir;

    // Deviation
    float octaveDev;
    int octaveDevRng;
    float idxDev;
    int idxDevRng;
    float pitchDev;
    int pitchDevRng;

    // Velocity
    float velBase;
    float velRndm;
    float eucAccStrength;

    // Duration
    int durBase;
    int durRndm;
    int durEucStrength;

    // Euclidean
    int eucLen;
    int eucHits;
    int eucOff;
    int eucAccLen;
    int eucAccHits;
    int eucAccOff;
    int eucDurLen;
    int eucDurHits;
    int eucDurOff;

    // Probability
    float stepChance;
    float noteChance;

    bool hasData = false;
};

class polyphonicArpeggiator : public ofxOceanodeNodeModel {
public:
    polyphonicArpeggiator();
    ~polyphonicArpeggiator();

    void setup() override;
    void update(ofEventArgs &e) override;
    void presetSave(ofJson &json) override;
    void presetRecallAfterSettingParameters(ofJson &json) override;

private:
    // --- Core Trigger Inputs ---
    ofParameter<void> trigger;
    ofParameter<void> resetNext;

    // --- Euclidean Rhythm Parameters ---
    ofParameter<int> eucLen;
    ofParameter<int> eucHits;
    ofParameter<int> eucOff;
    ofParameter<float> stepChance;   // probability of the whole step firing
    ofParameter<float> noteChance;   // probability per polyphony voice within a step

    // --- Scale and Pattern Parameters ---
    ofParameter<vector<float>> scale;
    ofParameter<int> patternMode;  // 0=Ascending, 1=Descending, 2=Random, 3=User
    ofParameter<vector<int>> idxPattern;
    ofParameter<int> seqSize;
    ofParameter<int> degStart;
    ofParameter<int> stepInterval;

    // --- Polyphony Parameters ---
    ofParameter<int> polyphony;
    ofParameter<int> polyInterval;
    ofParameter<float> strum;
    ofParameter<float> strumRndm;
    ofParameter<int> strumDir;       // dropdown: 0=Ascending, 1=Descending, 2=Random

    // --- Pitch Deviation Parameters ---
    ofParameter<float> octaveDev;
    ofParameter<int> octaveDevRng;
    ofParameter<float> idxDev;
    ofParameter<int> idxDevRng;
    ofParameter<float> pitchDev;
    ofParameter<int> pitchDevRng;
    ofParameter<int> transpose;

    // --- Velocity Parameters ---
    ofParameter<float> velBase;
    ofParameter<float> velRndm;
    ofParameter<int> eucAccLen;
    ofParameter<int> eucAccHits;
    ofParameter<int> eucAccOff;
    ofParameter<float> eucAccStrength;

    // --- Duration Parameters ---
    ofParameter<int> durBase;
    ofParameter<int> durRndm;
    ofParameter<int> eucDurLen;
    ofParameter<int> eucDurHits;
    ofParameter<int> eucDurOff;
    ofParameter<int> durEucStrength;  // How much to increase duration on euclidean accent

    // --- Output Parameters ---
    ofParameter<vector<float>> pitchOut;
    ofParameter<vector<int>> gateOut;
    ofParameter<vector<float>> velocityOut;
    ofParameter<vector<float>> durOut;
    ofParameter<vector<float>> gateVelOut;  // gate * velocity combined output
    ofParameter<vector<int>> eucGateOut;    // euclidean gate pattern mapped to seqSize
    ofParameter<vector<int>> eucAccOut;     // euclidean accent pattern mapped to seqSize
    ofParameter<vector<int>> eucDurOut;     // euclidean duration pattern mapped to seqSize

    // --- GUI Parameters ---
    ofParameter<float> guiWidth;
    ofParameter<float> patternHeight;
    ofParameter<float> euclideanHeight;

    // --- Custom GUI Regions ---
    customGuiRegion uiPattern;
    customGuiRegion uiEuclidean;
    customGuiRegion uiVelocity;
    customGuiRegion uiSnapshots;

    // --- Internal State ---
    int currentStep;
    bool shouldReset;
    vector<bool> euclideanPattern;
    vector<bool> euclideanAccents;
    vector<bool> euclideanDurations;
    vector<float> expandedScale;

    // Persistent output state vectors (size = seqSize)
    vector<float> currentPitches;
    vector<int> currentGates;
    vector<float> currentVelocities;
    vector<float> currentDurations;
    vector<int> noteDurationsMs;          // per-slot computed duration in ms
    vector<uint64_t> noteStartTimes;      // per-slot gate-on timestamp (ms)

    // Pre-calculated deviation values (regenerated only when deviation params change)
    vector<float> deviationValues;        // stores the additive pitch deviation per slot

    // --- Random Number Generation ---
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist01;

    // --- Helper Functions ---
    void onTrigger();
    void onResetNext();
    void generateEuclideanPattern(vector<bool>& pattern, int length, int hits, int offset);
    void rebuildExpandedScale();
    float getScaleDegree(int index);
    void rebuildPitchSequence();
    void rebuildDeviations();             // Regenerate random deviations
    void rebuildVelocitySequence();
    void rebuildEuclideanOutputs();       // Rebuild euclidean pattern output vectors
    void processStep();
    void applyPitchDeviations(float& pitch, int scaleIndex);
    float computeStepVelocity(int stepIndex);
    int computeStepDuration(int stepIndex);
    float computeStrumOffset(int voiceIndex, int totalVoices);
    void updateOutputs();

    // --- GUI Drawing Functions ---
    void drawPatternDisplay();
    void drawEuclideanDisplay();
    void drawVelocityDisplay();
    void drawSnapshotSlots();

    // --- Snapshot Functions ---
    void storeToSlot(int slot);
    void recallSlot(int slot);
    void updateMorph();
    void saveSnapshotToDisk(int slot);
    void loadSnapshotFromDisk(int slot);
    void loadAllSnapshotsFromDisk();
    void deleteSnapshotFromDisk(int slot);
    string getSnapshotsFolderPath();
    string getSnapshotFilePath(int slot);

    // --- Event Listeners ---
    ofEventListeners listeners;

    // --- Timing ---
    uint64_t lastTriggerTime;

    // --- Pattern Visualization State ---
    int highlightedStep;
    vector<float> stepVelocities;    // last computed velocity per slot (for viz)
    vector<bool> stepGates;          // last gate-on state per slot (for viz)

    // --- Snapshot System ---
    vector<ArpeggiatorSnapshot> snapshotSlots;
    int activeSnapshotSlot;
    ofParameter<float> morphTime;

    // Morphing State
    bool isMorphing;
    float morphStartTime;
    ArpeggiatorSnapshot startSnapshot;
    ArpeggiatorSnapshot targetSnapshot;

    // --- Constants ---
    static constexpr int MAX_SEQUENCE_SIZE = 128;
    static constexpr int MAX_POLYPHONY = 16;
};

#endif /* polyphonicArpeggiator_h */
