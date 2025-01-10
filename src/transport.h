#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "ofxOceanodeNodeModel.h"
#include "ofThread.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

class Transport : public ofxOceanodeNodeModel {
public:
    Transport() : ofxOceanodeNodeModel("Transport"), isPlaying(false), ppqCount(0) {
        description = "Transport node with sample-accurate timing using audio callbacks. Outputs bar phases (0-1), beat phases (0-1), bar position in PPQ and time in H:M:S:MS. Custom note divisions counts, including dotted and triplets noted as d and t.";
        
        setupAudioUnit();
        
        addParameter(playStop.set("Play/Stop", false));
        addParameter(pause.set("Pause", false));
        addParameter(timeNumerator.set("Time Num", 4, 1, 32));
        addParameter(timeDenominator.set("Time Den", 4, 2, 32));
        
        addOutputParameter(ppqOutput.set("PPQ", 0));
        addOutputParameter(barPhase.set("Bar", 0.0f, 0.0f, 1.0f));
        addOutputParameter(timeOutput.set("Time", vector<float>{0,0,0,0}, vector<float>{0,0,0,0}, vector<float>{INT_MAX,59,59,999}));
        
        vector<float> defaultAccents(4, 0.0f);
        addOutputParameter(beatAccents.set("Accents", defaultAccents, vector<float>(4, 0.0f), vector<float>(4, 1.0f)));
        beatAccents = calculateAccents(4, 4);
        
        addInspectorParameter(noteDivisions.set("Note Divisions", ""));
        addInspectorParameter(phasorDivisions.set("Phasor Divisions", ""));
        addInspectorParameter(tickDivisions.set("Tick Divisions", ""));
        
        setupListeners();
    }
    
    void presetWillBeLoaded() override {
        isPresetLoading = true;
        if(!noteDivisions.get().empty()) {
            createNoteOutputs(noteDivisions.get());
        }
        if(!phasorDivisions.get().empty()) {
            createPhasorOutputs(phasorDivisions.get());
        }
        if(!tickDivisions.get().empty()) {
            createTickOutputs(tickDivisions.get());
        }
    }

    void presetHasLoaded() override {
        isPresetLoading = false;
    }

    void presetSave(ofJson &json) override {
        // Just save the division strings
        if(!noteDivisions.get().empty()) json["noteDivisions"] = noteDivisions.get();
        if(!phasorDivisions.get().empty()) json["phasorDivisions"] = phasorDivisions.get();
        if(!tickDivisions.get().empty()) json["tickDivisions"] = tickDivisions.get();
    }

    void loadBeforeConnections(ofJson &json) override {
        // Just deserialize the parameters
        if(json.contains("noteDivisions")) {
            noteDivisions = json["noteDivisions"].get<string>();
        }
        if(json.contains("phasorDivisions")) {
            phasorDivisions = json["phasorDivisions"].get<string>();
        }
        if(json.contains("tickDivisions")) {
            tickDivisions = json["tickDivisions"].get<string>();
        }
    }
    
    ~Transport() {
            cleanup();
        }
        
        void setBpm(float bpm) override {
            currentBPM = bpm;
            updatePPQIncrement();
        }

private:
    // Audio components
       AudioComponent audioComponent{nullptr};
       AudioComponentInstance audioUnit{nullptr};
       double sampleRate{44100.0};
       double ppqIncrement{0.0};  // PPQ increment per sample
       double ppqAccumulator{0.0}; // Fractional PPQ counter
       std::atomic<bool> audioRunning{false};
    
    void setupAudioUnit() {
            AudioComponentDescription desc;
            desc.componentType = kAudioUnitType_Output;
            desc.componentSubType = kAudioUnitSubType_HALOutput;
            desc.componentManufacturer = kAudioUnitManufacturer_Apple;
            desc.componentFlags = 0;
            desc.componentFlagsMask = 0;
            
            audioComponent = AudioComponentFindNext(NULL, &desc);
            if(!audioComponent) return;
            
            OSStatus status = AudioComponentInstanceNew(audioComponent, &audioUnit);
            if(status != noErr) return;
            
            // Get the actual hardware sample rate
            AudioStreamBasicDescription asbd;
            UInt32 size = sizeof(asbd);
            status = AudioUnitGetProperty(audioUnit,
                                        kAudioUnitProperty_StreamFormat,
                                        kAudioUnitScope_Output,
                                        0,
                                        &asbd,
                                        &size);
            if(status == noErr) {
                sampleRate = asbd.mSampleRate;
            }
            
            // Set callback
            AURenderCallbackStruct callbackStruct;
            callbackStruct.inputProc = audioCallback;
            callbackStruct.inputProcRefCon = this;
            
            status = AudioUnitSetProperty(audioUnit,
                                        kAudioUnitProperty_SetRenderCallback,
                                        kAudioUnitScope_Input,
                                        0,
                                        &callbackStruct,
                                        sizeof(callbackStruct));
            
            if(status == noErr) {
                status = AudioUnitInitialize(audioUnit);
                if(status == noErr) {
                    audioRunning = true;
                    AudioOutputUnitStart(audioUnit);
                }
            }
        }
        
        static OSStatus audioCallback(void *inRefCon,
                                    AudioUnitRenderActionFlags *ioActionFlags,
                                    const AudioTimeStamp *inTimeStamp,
                                    UInt32 inBusNumber,
                                    UInt32 inNumberFrames,
                                    AudioBufferList *ioData) {
            Transport* transport = static_cast<Transport*>(inRefCon);
            if(!transport || !transport->isPlaying || transport->pause) return noErr;
            
            // Process each sample for maximum precision
            for(UInt32 frame = 0; frame < inNumberFrames; frame++) {
                transport->ppqAccumulator += transport->ppqIncrement;
                
                // When we accumulate a whole PPQ tick
                while(transport->ppqAccumulator >= 1.0) {
                    transport->ppqAccumulator -= 1.0;
                    transport->ppqCount++;
                    transport->updatePhases();
                }
            }
            
            // Clear audio output (we're just using the callback for timing)
            for(UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
                memset(ioData->mBuffers[i].mData, 0,
                      ioData->mBuffers[i].mDataByteSize);
            }
            
            return noErr;
        }
    
    void updatePPQIncrement() {
            // Calculate PPQ increment per sample
            // PPQ ticks per second = BPM * 96 / 60
            // PPQ ticks per sample = PPQ ticks per second / sample rate
            ppqIncrement = (currentBPM * 96.0) / (60.0 * sampleRate);
        }
        
        void cleanup() {
            if(audioUnit) {
                AudioOutputUnitStop(audioUnit);
                AudioUnitUninitialize(audioUnit);
                AudioComponentInstanceDispose(audioUnit);
                audioUnit = nullptr;
            }
        }
    
    void setupListeners() {
        listeners.push(playStop.newListener([this](bool& val){
            isPlaying = val && !pause;
            if (!val) {
                ppqCount = 0;
                accumulatedTime = 0;
                updatePhases();
                updateTimeOutput();
            } else if (!isPaused) {
                lastUpdateTime = ofGetElapsedTimeMillis();
            }
        }));
        
        listeners.push(pause.newListener([this](bool& val){
            isPlaying = playStop && !val;
            isPaused = val;
            if (!val && playStop) {
                lastUpdateTime = ofGetElapsedTimeMillis();
            }
        }));
        
        listeners.push(noteDivisions.newListener([this](string& val){
                if (!isPresetLoading) {  // Add this check
                    createNoteOutputs(val);
                }
            }));
            
            listeners.push(phasorDivisions.newListener([this](string& val){
                if (!isPresetLoading) {  // Add this check
                    createPhasorOutputs(val);
                }
            }));
            
            listeners.push(tickDivisions.newListener([this](string& val){
                if (!isPresetLoading) {  // Add this check
                    createTickOutputs(val);
                }
            }));
        
        listeners.push(timeNumerator.newListener([this](int& val){
            beatAccents = calculateAccents(val, timeDenominator);
        }));

        listeners.push(timeDenominator.newListener([this](int& val){
            beatAccents = calculateAccents(timeNumerator, val);
        }));
        
    }
    
    void createNoteOutputs(const string& val) {
            // Clear existing outputs first
            for(auto& output : customNoteOutputs) {
                removeParameter(output.getName());
            }
            customNoteOutputs.clear();
            noteDivisionValues.clear();
            
            vector<string> divisions = ofSplitString(val, ",");
            for(auto& div : divisions) {
                if(!div.empty()) {
                    parseAndAddOutput(div);
                }
            }
        }

        void createPhasorOutputs(const string& val) {
            // Clear existing phasors first
            for(auto& phasor : customPhasors) {
                removeParameter(phasor.getName());
            }
            customPhasors.clear();
            phasorDivisionValues.clear();
            
            vector<string> divisions = ofSplitString(val, ",");
            for(auto& div : divisions) {
                if(!div.empty()) {
                    parseAndAddPhasor(div);
                }
            }
        }

        void createTickOutputs(const string& val) {
            // Clear existing tick triggers first
            for(auto& tick : customTickTriggers) {
                removeParameter(tick.getName());
            }
            customTickTriggers.clear();
            tickDivisionValues.clear();
            
            vector<string> divisions = ofSplitString(val, ",");
            for(auto& div : divisions) {
                if(!div.empty()) {
                    parseAndAddTick(div);
                }
            }
        }


    ofParameter<bool> playStop, pause;
    ofParameter<int> timeNumerator, timeDenominator;
    ofParameter<int> ppqOutput;
    ofParameter<float> barPhase, beatPhasor;
    ofParameter<string> noteDivisions;
    vector<ofParameter<int>> customNoteOutputs;
    vector<float> noteDivisionValues;
    ofParameter<string> phasorDivisions;
    vector<ofParameter<float>> customPhasors;
    vector<float> phasorDivisionValues;
    ofParameter<vector<float>> beatAccents;
    vector<ofParameter<void>> customTickTriggers;
    vector<float> tickDivisionValues;
    ofParameter<string> tickDivisions;
    ofParameter<vector<float>> timeOutput;
    
    ofEventListeners listeners;
    bool isPlaying;
    bool isPaused;
    bool isPresetLoading = false;
    float currentBPM;
    unsigned long ppqCount;
    uint64_t lastUpdateTime;
    uint64_t accumulatedTime;

    void updateTimeOutput() {
        if(isPlaying && !isPaused) {
            uint64_t currentTime = ofGetElapsedTimeMillis();
            uint64_t deltaTime = currentTime - lastUpdateTime;
            lastUpdateTime = currentTime;
            accumulatedTime += deltaTime;
        }

        vector<float> time(4);
        uint64_t remainingTime = accumulatedTime;
        
        // Calculate hours
        time[0] = remainingTime / (1000 * 60 * 60);
        remainingTime %= (1000 * 60 * 60);
        
        // Calculate minutes
        time[1] = remainingTime / (1000 * 60);
        remainingTime %= (1000 * 60);
        
        // Calculate seconds
        time[2] = remainingTime / 1000;
        
        // Calculate milliseconds
        time[3] = remainingTime % 1000;
        
        timeOutput = time;
    }
    
    void processPPQ() {
        if(currentBPM <= 0) return;
        
        double msPerPulse = (60.0 * 1000.0) / (currentBPM * 96.0);
        static double accumulator = 0;
        
        accumulator += 1.0;
        
        if(accumulator >= msPerPulse) {
            ppqCount++;
            accumulator -= msPerPulse;
            updatePhases();
        }
    }
    
    void parseAndAddOutput(string division) {
        string baseDiv = division;
        float divValue;
        
        int baseDivision = ofToInt(baseDiv.back() == 'd' || baseDiv.back() == 't' ?
                                   baseDiv.substr(0, baseDiv.size()-1) : baseDiv);
                                   
        // Use 4.0f/baseDivision so that '4' represents a quarter note (1.0),
        // '8' represents an eighth note (0.5), and so forth.
        divValue = 4.0f / (float)baseDivision;

        // Adjust for dotted and triplet notes
        if(division.back() == 'd') {
            // Dotted notes are 1.5 times longer
            divValue *= 1.5f;
        }
        else if(division.back() == 't') {
            // Triplet notes are 2/3 the length of the base note
            divValue *= (2.0f / 3.0f);
        }
        
        if(baseDivision > 0) {
            string name = division + " Cnt";
            customNoteOutputs.push_back(ofParameter<int>());
            addOutputParameter(customNoteOutputs.back().set(name, 0));
            noteDivisionValues.push_back(divValue);
        }
    }

    void parseAndAddPhasor(string division) {
        string baseDiv = division;
        float divValue;
        
        int baseDivision = ofToInt(baseDiv.back() == 'd' || baseDiv.back() == 't' ?
                                   baseDiv.substr(0, baseDiv.size()-1) : baseDiv);
                                   
        // Use 4.0f/baseDivision so that '4' represents a quarter note (1.0)
        divValue = 4.0f / (float)baseDivision;

        // Adjust for dotted and triplet notes
        if(division.back() == 'd') {
            // Dotted notes are 1.5 times longer
            divValue *= 1.5f;
        }
        else if(division.back() == 't') {
            // Triplet notes are 2/3 the length of the base note
            divValue *= (2.0f / 3.0f);
        }
        
        if(baseDivision > 0) {
            string name = division + " Ph";
            customPhasors.push_back(ofParameter<float>());
            addOutputParameter(customPhasors.back().set(name, 0.0f, 0.0f, 1.0f));
            phasorDivisionValues.push_back(divValue);
        }
    }
    
    void parseAndAddTick(string division) {
        string baseDiv = division;
        float divValue;
        
        int baseDivision = ofToInt(baseDiv.back() == 'd' || baseDiv.back() == 't' ?
                                   baseDiv.substr(0, baseDiv.size()-1) : baseDiv);
                                   
        // Use 4.0f/baseDivision so that '4' maps to a quarter note
        divValue = 4.0f / (float)baseDivision;
        
        if(division.back() == 'd') {
            // Dotted notes are 1.5 times longer
            divValue *= 1.5f;
        } else if(division.back() == 't') {
            // Triplet notes are 2/3 the length of the base note
            divValue *= (2.0f / 3.0f);
        }
        
        if(baseDivision > 0) {
            string name = division + " Tick";
            customTickTriggers.push_back(ofParameter<void>());
            addOutputParameter(customTickTriggers.back().set(name));
            tickDivisionValues.push_back(divValue);
        }
    }

    void updatePhases() {
        int ppqPerBeat = 96 / (timeDenominator / 4);
        int ppqPerBar = ppqPerBeat * timeNumerator;
        
        float currentBar = (ppqCount % ppqPerBar) / float(ppqPerBar);
        
        barPhase = currentBar;
        ppqOutput = ppqCount % INT_MAX;
        
        // Update phasors
        for(size_t i = 0; i < customPhasors.size(); i++) {
            float ppqsPerNote = 96.0f * phasorDivisionValues[i];
            customPhasors[i] = (ppqCount % (int)ppqsPerNote) / ppqsPerNote;
        }
        
        // Update note counters
        for(size_t i = 0; i < customNoteOutputs.size(); i++) {
            float ppqsPerNote = 96.0f * noteDivisionValues[i];
            customNoteOutputs[i] = (int)(ppqCount / ppqsPerNote);
        }

        // Trigger void outputs (ticks)
        for(size_t i = 0; i < customTickTriggers.size(); i++) {
            float ppqsPerNote = 96.0f * tickDivisionValues[i];
            int noteLengthInPulses = (int)ppqsPerNote;
            
            if(noteLengthInPulses > 0 && (ppqCount % noteLengthInPulses) == 0) {
                customTickTriggers[i].trigger();
            }
        }
    }

    vector<float> calculateAccents(int numerator, int denominator) {
        vector<float> accents(numerator, 0.3f); // Initialize all beats as weak
        accents[0] = 1.0f; // First beat always strong
        
        if(denominator == 8) { // Compound meter
            for(int i = 3; i < numerator; i += 3) {
                accents[i] = 0.7f; // Medium accent on start of each group of 3
            }
        }
        else { // Simple meter
            switch(numerator) {
                case 4:
                    accents[2] = 0.7f; // Medium on 3 for 4/4
                    break;
                case 5:
                    accents[3] = 0.7f; // Common 3+2 grouping
                    break;
                case 7:
                    accents[2] = 0.7f; // Common 2+2+3 grouping
                    accents[4] = 0.7f;
                    break;
            }
        }
        return accents;
    }
};

#endif /* TRANSPORT_H */
