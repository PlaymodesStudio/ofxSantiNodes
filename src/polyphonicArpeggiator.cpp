#include "polyphonicArpeggiator.h"
#include "imgui.h"
#include <chrono>

// ═══════════════════════════════════════════════════════════
// CONSTRUCTOR / DESTRUCTOR
// ═══════════════════════════════════════════════════════════

polyphonicArpeggiator::polyphonicArpeggiator() : ofxOceanodeNodeModel("Polyphonic Arpeggiator") {
    rng = std::mt19937(std::chrono::steady_clock::now().time_since_epoch().count());
    dist01 = std::uniform_real_distribution<float>(0.0f, 1.0f);

    currentStep = 0;
    shouldReset = false;
    lastTriggerTime = 0;
    highlightedStep = -1;

    euclideanPattern.reserve(MAX_SEQUENCE_SIZE);
    euclideanAccents.reserve(MAX_SEQUENCE_SIZE);
    euclideanDurations.reserve(MAX_SEQUENCE_SIZE);
    expandedScale.reserve(128);
    currentPitches.reserve(MAX_SEQUENCE_SIZE);
    currentGates.reserve(MAX_SEQUENCE_SIZE);
    currentVelocities.reserve(MAX_SEQUENCE_SIZE);
    currentDurations.reserve(MAX_SEQUENCE_SIZE);
    noteDurationsMs.reserve(MAX_SEQUENCE_SIZE);
    noteStartTimes.reserve(MAX_SEQUENCE_SIZE);
    stepVelocities.reserve(MAX_SEQUENCE_SIZE);
    stepGates.reserve(MAX_SEQUENCE_SIZE);
    deviationValues.reserve(MAX_SEQUENCE_SIZE);
}

polyphonicArpeggiator::~polyphonicArpeggiator() {
    listeners.unsubscribeAll();
}

// ═══════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::setup() {
    description = "Polyphonic arpeggiator with euclidean gating, strum, "
                  "positive-only deviations (octave, index, chromatic), "
                  "euclidean accent and duration patterns. "
                  "Pitch sequence is precomputed from scale and pattern. "
                  "Each trigger advances one step, toggling gates at the "
                  "current position with per-voice strum and duration.";

    // ── TRIGGER & CONTROL ──
    addSeparator("Trigger", ofColor(200));
    addParameter(trigger.set("Trigger"));
    addParameter(resetNext.set("ResetNext"));
    addParameter(eucLen.set("EucLen", 8, 1, 64));
    addParameter(eucHits.set("EucHits", 8, 0, 64));
    addParameter(eucOff.set("EucOff", 0, 0, 63));
    addParameter(stepChance.set("Step%", 1.0f, 0.0f, 1.0f));
    addParameter(noteChance.set("Note%", 1.0f, 0.0f, 1.0f));

    // ── PITCH ──
    addSeparator("Pitch", ofColor(200));
    addParameter(scale.set("Scale", {0, 2, 4, 5, 7, 9, 11}, {-24}, {127}));
    addParameterDropdown(patternMode, "Pattern", 0, {"Ascending", "Descending", "Random", "User"});
    addParameter(idxPattern.set("IdxPatt", {0, 1, 2, 3}, {0}, {127}));
    addParameter(seqSize.set("SeqSize", 16, 1, MAX_SEQUENCE_SIZE));
    addParameter(degStart.set("IdxStart", 0, 0, 127));
    addParameter(stepInterval.set("StepInterval", 1, 1, 12));
    addParameter(transpose.set("Transpose", 0, 0, 96));
    addParameter(continuousPitch.set("Continuous", false));

    // ── POLYPHONY ──
    addSeparator("Polyphony", ofColor(200));
    addParameter(polyphony.set("Polyphony", 1, 1, MAX_POLYPHONY));
    addParameter(polyInterval.set("PolyInterval", 2, 1, 12));
    addParameter(strum.set("Strum", 0.0f, 0.0f, 500.0f));
    addParameter(strumRndm.set("StrumRndm", 0.0f, 0.0f, 200.0f));
    addParameterDropdown(strumDir, "StrumDir", 0, {"Ascending", "Descending", "Random"});

    // ── DEVIATION (all positive-only) ──
    addSeparator("Deviation", ofColor(200));
    addParameter(octaveDev.set("OctDev", 0.0f, 0.0f, 1.0f));
    addParameter(octaveDevRng.set("OctDevRng", 1, 1, 4));
    addParameter(idxDev.set("IdxDev", 0.0f, 0.0f, 1.0f));
    addParameter(idxDevRng.set("IdxDevRng", 2, 1, 12));
    addParameter(pitchDev.set("PitchDev", 0.0f, 0.0f, 1.0f));
    addParameter(pitchDevRng.set("PitchDevRng", 2, 1, 12));

    // ── VELOCITY ──
    addSeparator("Velocity", ofColor(200));
    addParameter(velBase.set("VelBase", 0.8f, 0.0f, 1.0f));
    addParameter(velRndm.set("VelRndm", 0.1f, 0.0f, 1.0f));
    addParameter(eucAccLen.set("AccLen", 4, 1, 64));
    addParameter(eucAccHits.set("AccHits", 1, 0, 64));
    addParameter(eucAccOff.set("AccOff", 0, 0, 63));
    addParameter(eucAccStrength.set("AccStr", 0.2f, 0.0f, 1.0f));

    // ── DURATION ──
    addSeparator("Duration", ofColor(200));
    addParameter(durBase.set("DurBase", 100, 1, 5000));
    addParameter(durRndm.set("DurRndm", 20, 0, 1000));
    addParameter(eucDurLen.set("DurEucLen", 4, 1, 64));
    addParameter(eucDurHits.set("DurEucHits", 4, 0, 64));
    addParameter(eucDurOff.set("DurEucOff", 0, 0, 63));
    addParameter(durEucStrength.set("DurEucStr", 50, -5000, 5000));  // Milliseconds to add on euclidean accent (can be negative)

    // ── OUTPUT ──
    addSeparator("Output", ofColor(200));
    addOutputParameter(pitchOut.set("PitchOut",
        vector<float>(16, 60.0f), vector<float>(1, 0.0f), vector<float>(1, 127.0f)));
    addOutputParameter(gateOut.set("GateOut",
        vector<int>(16, 0), vector<int>(1, 0), vector<int>(1, 1)));
    addOutputParameter(velocityOut.set("VelOut",
        vector<float>(16, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 1.0f)));
    addOutputParameter(durOut.set("DurOut",
        vector<float>(16, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 60000.0f)));
    addOutputParameter(gateVelOut.set("GateVelOut",
        vector<float>(16, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 1.0f)));

    // ── DISPLAY ──
    addSeparator("Display", ofColor(200));
    addInspectorParameter(guiWidth.set("GUI Width", 300.0f, 200.0f, 600.0f));
    addInspectorParameter(patternHeight.set("Pattern Height", 100.0f, 50.0f, 200.0f));
    addInspectorParameter(euclideanHeight.set("Euclidean Height", 80.0f, 40.0f, 150.0f));

    uiPattern.set("Pattern Display", [this](){ drawPatternDisplay(); });
    addCustomRegion(uiPattern, [this](){ drawPatternDisplay(); });

    uiEuclidean.set("Euclidean Display", [this](){ drawEuclideanDisplay(); });
    addCustomRegion(uiEuclidean, [this](){ drawEuclideanDisplay(); });

    uiVelocity.set("Velocity Display", [this](){ drawVelocityDisplay(); });
    addCustomRegion(uiVelocity, [this](){ drawVelocityDisplay(); });

    // ── EVENT LISTENERS ──
    listeners.push(trigger.newListener([this](void){ onTrigger(); }));
    listeners.push(resetNext.newListener([this](void){ onResetNext(); }));

    // Euclidean patterns
    listeners.push(eucLen.newListener([this](int&){
        generateEuclideanPattern(euclideanPattern, eucLen, eucHits, eucOff);
    }));
    listeners.push(eucHits.newListener([this](int&){
        generateEuclideanPattern(euclideanPattern, eucLen, eucHits, eucOff);
    }));
    listeners.push(eucOff.newListener([this](int&){
        generateEuclideanPattern(euclideanPattern, eucLen, eucHits, eucOff);
    }));

    listeners.push(eucAccLen.newListener([this](int&){
        generateEuclideanPattern(euclideanAccents, eucAccLen, eucAccHits, eucAccOff);
    }));
    listeners.push(eucAccHits.newListener([this](int&){
        generateEuclideanPattern(euclideanAccents, eucAccLen, eucAccHits, eucAccOff);
    }));
    listeners.push(eucAccOff.newListener([this](int&){
        generateEuclideanPattern(euclideanAccents, eucAccLen, eucAccHits, eucAccOff);
    }));

    listeners.push(eucDurLen.newListener([this](int&){
        generateEuclideanPattern(euclideanDurations, eucDurLen, eucDurHits, eucDurOff);
    }));
    listeners.push(eucDurHits.newListener([this](int&){
        generateEuclideanPattern(euclideanDurations, eucDurLen, eucDurHits, eucDurOff);
    }));
    listeners.push(eucDurOff.newListener([this](int&){
        generateEuclideanPattern(euclideanDurations, eucDurLen, eucDurHits, eucDurOff);
    }));

    // Pitch rebuild triggers
    listeners.push(scale.newListener([this](vector<float>&){
        rebuildExpandedScale();
        rebuildPitchSequence();
    }));
    listeners.push(idxPattern.newListener([this](vector<int>&){
        rebuildPitchSequence();
    }));
    listeners.push(degStart.newListener([this](int&){
        rebuildPitchSequence();
    }));
    listeners.push(stepInterval.newListener([this](int&){
        rebuildPitchSequence();
    }));
    listeners.push(polyphony.newListener([this](int&){
        rebuildPitchSequence();
    }));
    listeners.push(polyInterval.newListener([this](int&){
        rebuildPitchSequence();
    }));
    listeners.push(transpose.newListener([this](int&){
        rebuildPitchSequence();
    }));
    
    // Deviation parameter listeners - regenerate deviations and rebuild pitch sequence
    listeners.push(octaveDev.newListener([this](float&){
        rebuildDeviations();
        rebuildPitchSequence();
    }));
    listeners.push(octaveDevRng.newListener([this](int&){
        rebuildDeviations();
        rebuildPitchSequence();
    }));
    listeners.push(idxDev.newListener([this](float&){
        rebuildDeviations();
        rebuildPitchSequence();
    }));
    listeners.push(idxDevRng.newListener([this](int&){
        rebuildDeviations();
        rebuildPitchSequence();
    }));
    listeners.push(pitchDev.newListener([this](float&){
        rebuildDeviations();
        rebuildPitchSequence();
    }));
    listeners.push(pitchDevRng.newListener([this](int&){
        rebuildDeviations();
        rebuildPitchSequence();
    }));
    listeners.push(patternMode.newListener([this](int&){
        rebuildPitchSequence();
    }));
    listeners.push(continuousPitch.newListener([this](bool&){
        rebuildPitchSequence();
    }));

    // seqSize change
    listeners.push(seqSize.newListener([this](int& size){
        currentPitches.resize(size, 60.0f);
        currentGates.resize(size, 0);
        currentVelocities.resize(size, 0.0f);
        currentDurations.resize(size, 0.0f);
        noteDurationsMs.resize(size, 100);
        noteStartTimes.resize(size, 0);
        stepVelocities.resize(size, 0.0f);
        stepGates.resize(size, false);
        deviationValues.resize(size, 0.0f);
        rebuildDeviations();
        rebuildPitchSequence();
        updateOutputs();
    }));

    // Initialize euclidean patterns
    generateEuclideanPattern(euclideanPattern, eucLen, eucHits, eucOff);
    generateEuclideanPattern(euclideanAccents, eucAccLen, eucAccHits, eucAccOff);
    generateEuclideanPattern(euclideanDurations, eucDurLen, eucDurHits, eucDurOff);

    // Initialize state vectors
    int sz = seqSize;
    currentPitches.resize(sz, 60.0f);
    currentGates.resize(sz, 0);
    currentVelocities.resize(sz, 0.0f);
    currentDurations.resize(sz, 0.0f);
    noteDurationsMs.resize(sz, 100);
    noteStartTimes.resize(sz, 0);
    stepVelocities.resize(sz, 0.0f);
    stepGates.resize(sz, false);
    deviationValues.resize(sz, 0.0f);

    rebuildExpandedScale();
    rebuildDeviations();
    rebuildPitchSequence();
    rebuildVelocitySequence();
}

// ═══════════════════════════════════════════════════════════
// UPDATE (per frame — gate duration management + strum)
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::update(ofEventArgs &e) {
    auto currentTime = std::chrono::steady_clock::now().time_since_epoch();
    auto currentMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime).count();

    bool needsUpdate = false;
    int sz = seqSize;

    for(int i = 0; i < sz && i < (int)currentGates.size(); i++) {
        // Strummed notes waiting to start
        if(currentGates[i] == 0 && noteStartTimes[i] > 0) {
            if(currentMs >= (int64_t)noteStartTimes[i]) {
                currentGates[i] = 1;
                stepGates[i] = true;
                needsUpdate = true;
            }
        }

        // Active gates that should expire
        if(currentGates[i] == 1 && noteStartTimes[i] > 0) {
            if(currentMs >= (int64_t)(noteStartTimes[i] + noteDurationsMs[i])) {
                currentGates[i] = 0;
                noteStartTimes[i] = 0;
                stepGates[i] = false;
                needsUpdate = true;
            }
        }
    }

    if(needsUpdate) {
        updateOutputs();
    }
}

// ═══════════════════════════════════════════════════════════
// TRIGGER
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::onTrigger() {
    if(shouldReset) {
        currentStep = 0;  // Reset to beginning of sequence
        shouldReset = false;
    }

    processStep();

    // Advance currentStep by 1 (not by stepInterval - that's for pattern indexing)
    currentStep = (currentStep + 1) % seqSize;
}

void polyphonicArpeggiator::onResetNext() {
    shouldReset = true;
}

// ═══════════════════════════════════════════════════════════
// PROCESS STEP — the core per-trigger logic
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::processStep() {
    int sz = seqSize.get();
    if(sz <= 0) return;

    // Check euclidean gate
    if(!euclideanPattern.empty()) {
        int eucStep = currentStep % (int)euclideanPattern.size();
        if(!euclideanPattern[eucStep]) {
            return;
        }
    }

    // Check step chance
    if(stepChance.get() < 1.0f) {
        if(dist01(rng) > stepChance.get()) {
            return;
        }
    }

    auto currentTime = std::chrono::steady_clock::now().time_since_epoch();
    auto currentMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime).count();

    int poly = polyphony.get();

    // Clear previous gates at positions we'll use
    for(int voice = 0; voice < poly; voice++) {
        int outputIndex = (currentStep + voice) % sz;
        currentGates[outputIndex] = 0;
        stepGates[outputIndex] = false;
    }

    // Calculate duration ONCE for all voices at this step
    int stepDuration = computeStepDuration(currentStep);

    // Turn on gates for all polyphonic voices
    for(int voice = 0; voice < poly; voice++) {
        // Check note chance per voice
        if(noteChance.get() < 1.0f) {
            if(dist01(rng) > noteChance.get()) {
                continue;
            }
        }

        int outputIndex = (currentStep + voice) % sz;

        // Pitch is already pre-calculated with deviations applied
        // No need to modify it here

        // Calculate velocity in real-time
        float finalVel = velBase.get();

        // Add random variation if enabled
        if(velRndm.get() > 0) {
            finalVel += velRndm.get() * dist01(rng);
        }

        // Apply accent if on accent beat - use currentStep directly for the accent pattern
        if(!euclideanAccents.empty()) {
            int accentStep = currentStep % (int)euclideanAccents.size();
            if(euclideanAccents[accentStep]) {
                finalVel += eucAccStrength.get();
            }
        }

        finalVel = ofClamp(finalVel, 0.0f, 1.0f);
        currentVelocities[outputIndex] = finalVel;

        // Calculate strum offset for this voice
        float strumOffset = computeStrumOffset(voice, poly);

        // Use the shared step duration for all voices
        noteDurationsMs[outputIndex] = stepDuration;
        currentDurations[outputIndex] = (float)stepDuration;
        
        if(strumOffset <= 0.5f) {
            // Turn on immediately
            currentGates[outputIndex] = 1;
            stepGates[outputIndex] = true;
            noteStartTimes[outputIndex] = currentMs;
        } else {
            // Schedule for later (handled in update())
            noteStartTimes[outputIndex] = currentMs + (uint64_t)strumOffset;
        }
    }

    highlightedStep = currentStep;
    updateOutputs();
}

// Calculate pitch for the next step (called one step before gate)
void polyphonicArpeggiator::calculateNextPitch() {
    int nextStep = (currentStep + 1) % seqSize;
    int sz = seqSize;
    if(sz <= 0) return;
    
    // Get pattern based on mode
    vector<int> pattern;
    int mode = patternMode.get();
    
    if(mode == 0) { // Ascending
        pattern.clear();
        int maxSteps = 128;
        for(int i = 0; i < maxSteps; i++) {
            pattern.push_back(i);
        }
    } else if(mode == 1) { // Descending
        pattern.clear();
        int maxSteps = 128;
        for(int i = maxSteps - 1; i >= 0; i--) {
            pattern.push_back(i);
        }
    } else if(mode == 2) { // Random
        // For random mode, we can't predict next pitch
        return;
    } else { // User
        pattern = idxPattern.get();
        if(pattern.empty()) pattern = {0};
    }
    
    // Get the pattern value for next position
    int patternIndex = nextStep % (int)pattern.size();
    int patternValue = pattern[patternIndex];
    
    // Apply stepInterval to the pattern value
    int scaleIndex = degStart + (patternValue * stepInterval.get());
    
    int poly = std::min((int)polyphony, (int)MAX_POLYPHONY);
    
    // Pre-calculate pitches for next step
    for(int voice = 0; voice < poly; voice++) {
        int outputIndex = nextStep;
        if(voice > 0 && polyphony > 1) {
            outputIndex = (nextStep + voice) % sz;
        }
        
        int noteIndex = scaleIndex + (voice * polyInterval);
        float pitch = getScaleDegree(noteIndex);
        
        // Don't apply random deviations in pre-calculation
        pitch += transpose;
        pitch = ofClamp(pitch, 0.0f, 127.0f);
        
        // Store pre-calculated pitch
        currentPitches[outputIndex] = pitch;
    }
}

// ═══════════════════════════════════════════════════════════
// PITCH SEQUENCE REBUILD
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::rebuildExpandedScale() {
    expandedScale.clear();

    if(scale->empty()) {
        expandedScale.push_back(60.0f);
        return;
    }

    for(int octave = -2; octave <= 8; octave++) {
        for(float note : scale.get()) {
            float expandedNote = note + (octave * 12);
            if(expandedNote >= 0 && expandedNote <= 127) {
                expandedScale.push_back(expandedNote);
            }
        }
    }

    std::sort(expandedScale.begin(), expandedScale.end());
}

float polyphonicArpeggiator::getScaleDegree(int index) {
    if(expandedScale.empty()) {
        rebuildExpandedScale();
        if(expandedScale.empty()) return 60.0f;
    }

    int sz = (int)expandedScale.size();
    int wrappedIndex = index % sz;
    if(wrappedIndex < 0) wrappedIndex += sz;

    return expandedScale[wrappedIndex];
}

// Generate random deviation values for the entire sequence
// This is only called when deviation parameters change
void polyphonicArpeggiator::rebuildDeviations() {
    int sz = seqSize.get();
    if(sz <= 0) return;

    deviationValues.resize(sz, 0.0f);

    int poly = polyphony.get();

    for(int pos = 0; pos < sz; pos++) {
        int logicalStep = pos / std::max(1, poly);
        int voice = pos % poly;

        // Start with no deviation
        float deviation = 0.0f;

        // We need the base noteIndex for index deviation
        // This is a simplified calculation - just get approximate noteIndex
        int noteIndex = degStart.get() + (logicalStep * stepInterval.get()) + (voice * polyInterval.get());

        // Octave deviation: probability of transposing up by 1 to octaveDevRng octaves
        if(octaveDev.get() > 0 && dist01(rng) < octaveDev.get()) {
            if(octaveDevRng.get() > 0) {
                std::uniform_int_distribution<int> octDist(1, octaveDevRng.get());
                deviation += octDist(rng) * 12;
            }
        }

        // Index deviation: probability of shifting up by 1 to idxDevRng scale degrees
        if(idxDev.get() > 0 && dist01(rng) < idxDev.get()) {
            if(idxDevRng.get() > 0) {
                std::uniform_int_distribution<int> idxDist(1, idxDevRng.get());
                int shift = idxDist(rng);
                // Calculate the pitch difference from shifting scale degrees
                float basePitch = getScaleDegree(noteIndex);
                float shiftedPitch = getScaleDegree(noteIndex + shift);
                deviation += (shiftedPitch - basePitch);
            }
        }

        // Chromatic pitch deviation: probability of adding 1 to pitchDevRng semitones
        if(pitchDev.get() > 0 && dist01(rng) < pitchDev.get()) {
            if(pitchDevRng.get() > 0) {
                std::uniform_int_distribution<int> chromDist(1, pitchDevRng.get());
                deviation += chromDist(rng);
            }
        }

        deviationValues[pos] = deviation;
    }
}

void polyphonicArpeggiator::rebuildPitchSequence() {
    int sz = seqSize.get();
    if(sz <= 0) return;

    currentPitches.resize(sz, 60.0f);

    // Generate pattern based on mode
    vector<int> pattern;
    int mode = patternMode.get();

    if(mode == 0) { // Ascending
        pattern.clear();
        // Pattern length should be based on how many unique steps we want
        int patternLength = sz / std::max(1, polyphony.get());
        for(int i = 0; i < patternLength; i++) {
            pattern.push_back(i);
        }
    } else if(mode == 1) { // Descending
        pattern.clear();
        int patternLength = sz / std::max(1, polyphony.get());
        for(int i = patternLength - 1; i >= 0; i--) {
            pattern.push_back(i);
        }
    } else if(mode == 2) { // Random
        // For random mode, generate a random pattern
        pattern.clear();
        int patternLength = sz / std::max(1, polyphony.get());
        // Maximum index should be based on a reasonable range
        int maxIndex = std::min(16, patternLength);  // Limit to reasonable range
        for(int i = 0; i < patternLength; i++) {
            std::uniform_int_distribution<int> randDist(0, maxIndex - 1);
            pattern.push_back(randDist(rng));
        }
    } else { // User (mode == 3)
        pattern = idxPattern.get();
        if(pattern.empty()) pattern = {0};
    }

    int poly = polyphony.get();
    int stepInt = stepInterval.get();
    int polyInt = polyInterval.get();
    int degreeStart = degStart.get();
    int transp = transpose.get();
    bool continuous = continuousPitch.get();

    // For continuous pitch mode, we need to track which notes will actually play
    int continuousPitchIndex = 0;

    // First, figure out which steps will actually sound (based on euclidean pattern)
    int numLogicalSteps = sz / std::max(1, poly);
    vector<bool> activeSteps(numLogicalSteps, true);  // logical steps that will sound
    if(!euclideanPattern.empty()) {
        for(int logicalStep = 0; logicalStep < numLogicalSteps; logicalStep++) {
            int eucStep = logicalStep % (int)euclideanPattern.size();
            activeSteps[logicalStep] = euclideanPattern[eucStep];
        }
    }

    // Calculate pitch for each position in the sequence
    // The sequence is organized as: step0voice0, step0voice1, step0voice2, step1voice0, step1voice1...
    for(int pos = 0; pos < sz; pos++) {
        // Determine which logical step and voice this position represents
        int logicalStep = pos / poly;  // Which step in the pattern
        int voice = pos % poly;         // Which polyphonic voice

        int noteIndex;

        if(continuous) {
            // In continuous mode, use consecutive pitches without gaps
            // Count how many active steps come before this one (for main voice)
            int activePitchIndex = 0;
            for(int i = 0; i < logicalStep && i < (int)activeSteps.size(); i++) {
                if(activeSteps[i]) activePitchIndex++;
            }

            // All voices at this position should sound if the step is active
            if(logicalStep < (int)activeSteps.size() && activeSteps[logicalStep]) {
                // In continuous mode, each active step gets the next consecutive pitch
                // The activePitchIndex tells us which consecutive pitch this active step should use
                int consecutiveIndex = activePitchIndex;

                // For descending mode, reverse the index
                if(mode == 1) { // Descending
                    int totalActive = 0;
                    for(bool active : activeSteps) if(active) totalActive++;
                    consecutiveIndex = totalActive - 1 - activePitchIndex;
                }
                // For random mode, we'll still use consecutive but could shuffle later

                // Apply continuous pitch with polyphonic intervals
                // IMPORTANT: The consecutive index advances the base pitch
                // Each voice adds its polyphonic interval on top
                noteIndex = degreeStart + (consecutiveIndex * stepInt) + (voice * polyInt);
            } else {
                // This step won't sound, set a default pitch
                noteIndex = degreeStart + voice * polyInt;
            }
        } else {
            // Normal mode: use pattern values with potential gaps
            // Get pattern value for this logical step
            int patternIndex = logicalStep % (int)pattern.size();
            int patternValue = pattern[patternIndex];

            // Apply stepInterval to pattern value
            int scaleIndex = degreeStart + (patternValue * stepInt);

            // Add polyphonic interval for additional voices
            noteIndex = scaleIndex + (voice * polyInt);
        }

        // Get pitch from expanded scale
        float pitch = getScaleDegree(noteIndex);

        // Apply pre-calculated deviation (only regenerated when deviation params change)
        if(pos < (int)deviationValues.size()) {
            pitch += deviationValues[pos];
        }

        pitch += transp;
        pitch = ofClamp(pitch, 0.0f, 127.0f);

        currentPitches[pos] = pitch;
    }

    pitchOut.set(currentPitches);
}

// Initialize velocity sequence (actual calculation happens in real-time)
void polyphonicArpeggiator::rebuildVelocitySequence() {
    int sz = seqSize.get();
    if(sz <= 0) return;
    
    // Just initialize to zero - actual velocities calculated per-trigger
    currentVelocities.resize(sz, 0.0f);
    velocityOut.set(currentVelocities);
}

// ═══════════════════════════════════════════════════════════
// PITCH DEVIATIONS (all positive-only)
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::applyPitchDeviations(float& pitch, int scaleIndex) {
    // Octave deviation: transposes UP by 1..octaveDevRng octaves
    if(octaveDev > 0 && dist01(rng) < octaveDev) {
        int range = octaveDevRng;
        if(range > 0) {
            std::uniform_int_distribution<int> octDist(1, range);
            pitch += octDist(rng) * 12;
        }
    }

    // Index deviation: shifts UP by 1..idxDevRng scale degrees
    if(idxDev > 0 && dist01(rng) < idxDev) {
        int range = idxDevRng;
        if(range > 0) {
            std::uniform_int_distribution<int> idxDistrib(1, range);
            int shift = idxDistrib(rng);
            pitch = getScaleDegree(scaleIndex + shift);
            pitch += transpose;
        }
    }

    // Chromatic pitch deviation: shifts UP by 1..pitchDevRng semitones
    if(pitchDev > 0 && dist01(rng) < pitchDev) {
        int range = pitchDevRng;
        if(range > 0) {
            std::uniform_int_distribution<int> chromDist(1, range);
            pitch += chromDist(rng);
        }
    }
}

// ═══════════════════════════════════════════════════════════
// VELOCITY COMPUTATION
// ═══════════════════════════════════════════════════════════

float polyphonicArpeggiator::computeStepVelocity(int stepIndex) {
    float velocity = velBase;

    if(velRndm > 0) {
        velocity += velRndm * dist01(rng);
    }

    if(!euclideanAccents.empty()) {
        int accentStep = stepIndex % (int)euclideanAccents.size();
        if(euclideanAccents[accentStep]) {
            velocity += eucAccStrength;
        }
    }

    return ofClamp(velocity, 0.0f, 1.0f);
}

// ═══════════════════════════════════════════════════════════
// DURATION COMPUTATION
// ═══════════════════════════════════════════════════════════

int polyphonicArpeggiator::computeStepDuration(int stepIndex) {
    int baseDur = durBase.get();
    int duration = baseDur;

    // Apply euclidean duration accent - add durEucStrength ms to the duration
    if(!euclideanDurations.empty()) {
        int durStep = stepIndex % (int)euclideanDurations.size();
        if(euclideanDurations[durStep]) {
            duration += durEucStrength.get();
        }
    }

    // Add randomization after euclidean accent
    if(durRndm.get() > 0) {
        duration += (int)(durRndm.get() * dist01(rng));
    }

    return ofClamp(duration, 1, 60000);
}

// ═══════════════════════════════════════════════════════════
// STRUM COMPUTATION
// ═══════════════════════════════════════════════════════════

float polyphonicArpeggiator::computeStrumOffset(int voiceIndex, int totalVoices) {
    if(totalVoices <= 1 || strum <= 0.0f) return 0.0f;

    float baseStrum = strum;

    if(strumRndm > 0.0f) {
        float rndOffset = (dist01(rng) * 2.0f - 1.0f) * strumRndm;
        baseStrum += rndOffset;
        baseStrum = std::max(0.0f, baseStrum);
    }

    int dir = strumDir;
    if(dir == 0) {
        return voiceIndex * baseStrum;
    } else if(dir == 1) {
        return (totalVoices - 1 - voiceIndex) * baseStrum;
    } else {
        return dist01(rng) * (totalVoices - 1) * baseStrum;
    }
}

// ═══════════════════════════════════════════════════════════
// EUCLIDEAN PATTERN GENERATION
// Uses the proven formula from euclideanPatterns.h:
//   index = ((j * length) / hits + offset) % length
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::generateEuclideanPattern(vector<bool>& pattern, int length, int hits, int offset) {
    pattern.clear();
    pattern.resize(length, false);

    if(hits <= 0 || length <= 0) return;
    if(hits > length) hits = length;

    for(int j = 0; j < hits; j++) {
        int index = ((j * length) / hits + offset) % length;
        if(index < 0) index += length;
        pattern[index] = true;
    }
}

// ═══════════════════════════════════════════════════════════
// OUTPUT UPDATE
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::updateOutputs() {
    pitchOut.set(currentPitches);
    gateOut.set(currentGates);
    velocityOut.set(currentVelocities);
    durOut.set(currentDurations);
    
    // Calculate gate * velocity output
    int sz = currentGates.size();
    vector<float> gateVel(sz, 0.0f);
    for(int i = 0; i < sz; i++) {
        gateVel[i] = currentGates[i] * currentVelocities[i];
    }
    gateVelOut.set(gateVel);
}

// ═══════════════════════════════════════════════════════════
// PRESET SAVE / LOAD
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::presetSave(ofJson &json) {
    json["currentStep"] = currentStep;
}

void polyphonicArpeggiator::presetRecallAfterSettingParameters(ofJson &json) {
    if(json.contains("currentStep")) currentStep = json["currentStep"];

    generateEuclideanPattern(euclideanPattern, eucLen, eucHits, eucOff);
    generateEuclideanPattern(euclideanAccents, eucAccLen, eucAccHits, eucAccOff);
    generateEuclideanPattern(euclideanDurations, eucDurLen, eucDurHits, eucDurOff);

    int sz = seqSize;
    currentPitches.resize(sz, 60.0f);
    currentGates.resize(sz, 0);
    currentVelocities.resize(sz, 0.0f);
    currentDurations.resize(sz, 0.0f);
    noteDurationsMs.resize(sz, 100);
    noteStartTimes.resize(sz, 0);
    stepVelocities.resize(sz, 0.0f);
    stepGates.resize(sz, false);
    deviationValues.resize(sz, 0.0f);

    rebuildExpandedScale();
    rebuildDeviations();
    rebuildPitchSequence();
}

// ═══════════════════════════════════════════════════════════
// GUI: PATTERN DISPLAY
//   Shows pitch bars per step (height = pitch, color = gate)
//   Current step highlighted, step numbers labeled
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::drawPatternDisplay() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = guiWidth.get();
    float height = patternHeight.get();

    ImGui::InvisibleButton("##pattern", ImVec2(width, height));

    // Background
    drawList->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(30, 30, 30, 255));
    drawList->AddRect(p, ImVec2(p.x + width, p.y + height), IM_COL32(80, 80, 80, 255));

    int sz = seqSize;
    if(sz <= 0) return;
    float stepWidth = width / sz;

    // Find pitch range
    float minPitch = 127.0f, maxPitch = 0.0f;
    for(int i = 0; i < sz && i < (int)currentPitches.size(); i++) {
        minPitch = std::min(minPitch, currentPitches[i]);
        maxPitch = std::max(maxPitch, currentPitches[i]);
    }
    if(maxPitch <= minPitch) { minPitch = 48; maxPitch = 84; }
    float pitchRange = std::max(12.0f, maxPitch - minPitch);

    for(int i = 0; i < sz; i++) {
        float x = p.x + i * stepWidth;

        // Step divider
        if(i > 0) {
            drawList->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + height), IM_COL32(50, 50, 55, 255));
        }

        // Highlight current step position
        if(i == highlightedStep) {
            drawList->AddRectFilled(ImVec2(x, p.y), ImVec2(x + stepWidth, p.y + height),
                                   IM_COL32(80, 80, 40, 80));
        }

        // Pitch bar
        if(i < (int)currentPitches.size()) {
            float normalizedPitch = (currentPitches[i] - minPitch) / pitchRange;
            normalizedPitch = ofClamp(normalizedPitch, 0.0f, 1.0f);
            float barHeight = normalizedPitch * height * 0.8f;
            float barY = p.y + height - barHeight - height * 0.05f;

            bool gateOn = (i < (int)currentGates.size() && currentGates[i] == 1);

            ImU32 barColor;
            if(gateOn) {
                barColor = IM_COL32(100, 220, 120, 255);
            } else {
                barColor = IM_COL32(60, 100, 80, 140);
            }

            drawList->AddRectFilled(ImVec2(x + 1, barY),
                                   ImVec2(x + stepWidth - 1, p.y + height - height * 0.05f),
                                   barColor);
        }

        // Step number every 4 steps
        if(i % 4 == 0) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", i);
            drawList->AddText(ImVec2(x + 2, p.y + 2), IM_COL32(140, 140, 140, 200), buf);
        }
    }

    // Info
    char info[80];
    snprintf(info, sizeof(info), "Step %d/%d | Poly %d | Trsp %d",
        currentStep,
        (int)idxPattern.get().size(),
        (int)polyphony, (int)transpose);
    ImVec2 infoSize = ImGui::CalcTextSize(info);
    drawList->AddText(ImVec2(p.x + width - infoSize.x - 4, p.y + height - infoSize.y - 2),
                     IM_COL32(160, 160, 170, 200), info);
}

// ═══════════════════════════════════════════════════════════
// GUI: EUCLIDEAN DISPLAY
//   Three rows: Gate / Accent / Duration
//   Each shows the euclidean pattern as colored blocks
//   Current playhead highlighted in gate row
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::drawEuclideanDisplay() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = guiWidth.get();
    float height = euclideanHeight.get();

    ImGui::InvisibleButton("##euclidean", ImVec2(width, height));

    drawList->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(30, 30, 30, 255));
    drawList->AddRect(p, ImVec2(p.x + width, p.y + height), IM_COL32(80, 80, 80, 255));

    float rowHeight = height / 3.0f;

    // Row 1: Gate euclidean
    {
        int len = std::max(1, (int)eucLen);
        float stepW = width / len;
        int eucPos = currentStep % len;
        for(int i = 0; i < len && i < (int)euclideanPattern.size(); i++) {
            float x = p.x + i * stepW;
            if(euclideanPattern[i]) {
                drawList->AddRectFilled(ImVec2(x + 1, p.y + 2),
                    ImVec2(x + stepW - 1, p.y + rowHeight - 2),
                    IM_COL32(200, 100, 100, 255));
            }
            if(i == eucPos) {
                drawList->AddRect(ImVec2(x, p.y + 1),
                    ImVec2(x + stepW, p.y + rowHeight - 1),
                    IM_COL32(255, 255, 200, 200), 0.0f, 0, 2.0f);
            }
        }
        drawList->AddText(ImVec2(p.x + 2, p.y + 2), IM_COL32(255, 255, 255, 180), "Gates");
    }

    // Row 2: Accent euclidean
    {
        float rowY = p.y + rowHeight;
        int len = std::max(1, (int)eucAccLen);
        float stepW = width / len;
        for(int i = 0; i < len && i < (int)euclideanAccents.size(); i++) {
            float x = p.x + i * stepW;
            if(euclideanAccents[i]) {
                drawList->AddRectFilled(ImVec2(x + 1, rowY + 2),
                    ImVec2(x + stepW - 1, rowY + rowHeight - 2),
                    IM_COL32(100, 200, 100, 255));
            }
        }
        drawList->AddText(ImVec2(p.x + 2, rowY + 2), IM_COL32(255, 255, 255, 180), "Accents");
    }

    // Row 3: Duration euclidean
    {
        float rowY = p.y + 2 * rowHeight;
        int len = std::max(1, (int)eucDurLen);
        float stepW = width / len;
        for(int i = 0; i < len && i < (int)euclideanDurations.size(); i++) {
            float x = p.x + i * stepW;
            if(euclideanDurations[i]) {
                drawList->AddRectFilled(ImVec2(x + 1, rowY + 2),
                    ImVec2(x + stepW - 1, rowY + rowHeight - 2),
                    IM_COL32(100, 100, 200, 255));
            }
        }
        drawList->AddText(ImVec2(p.x + 2, rowY + 2), IM_COL32(255, 255, 255, 180), "Duration");
    }
}

// ═══════════════════════════════════════════════════════════
// GUI: VELOCITY DISPLAY
//   Shows velocity bars per step, bright when gate on
// ═══════════════════════════════════════════════════════════

void polyphonicArpeggiator::drawVelocityDisplay() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = guiWidth.get();
    float height = 60.0f;

    ImGui::InvisibleButton("##velocity", ImVec2(width, height));

    drawList->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(30, 30, 30, 255));
    drawList->AddRect(p, ImVec2(p.x + width, p.y + height), IM_COL32(80, 80, 80, 255));

    int sz = seqSize;
    if(sz <= 0) return;
    float stepWidth = width / sz;

    for(int i = 0; i < sz; i++) {
        bool gateOn = (i < (int)currentGates.size() && currentGates[i] == 1);
        float vel = (i < (int)currentVelocities.size()) ? currentVelocities[i] : 0.0f;
        float barH = vel * (height - 6);
        float barY = p.y + height - barH - 3;

        if(gateOn) {
            drawList->AddRectFilled(
                ImVec2(p.x + i * stepWidth + 1, barY),
                ImVec2(p.x + (i + 1) * stepWidth - 1, p.y + height - 3),
                IM_COL32(200, 200, 80, 220));
        } else if(vel > 0.0f) {
            drawList->AddRectFilled(
                ImVec2(p.x + i * stepWidth + 1, barY),
                ImVec2(p.x + (i + 1) * stepWidth - 1, p.y + height - 3),
                IM_COL32(80, 80, 40, 80));
        }
    }

    drawList->AddText(ImVec2(p.x + 2, p.y + 2), IM_COL32(255, 255, 255, 180), "Velocity");
}
