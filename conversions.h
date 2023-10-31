#ifndef CONVERSIONS_H
#define CONVERSIONS_H

#include "ofxOceanodeNodeModel.h"

class conversions : public ofxOceanodeNodeModel {
public:
    conversions() : ofxOceanodeNodeModel("Conversions") {
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameterDropdown(operation, "Op", 0, {"ms-hz", "hz-ms", "beat-ms", "ms-beat", "frame-beat", "beat-frame", "soundMeters-ms", "ms-soundMeters", "pitch-hz", "hz-pitch", "speed-semitones", "semitones-speed"});
        addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        
        description = "Converts between various units or scales.";
        
        listeners.push(input.newListener([this](vector<float> &vf){
            processInput(vf);
            
        }));
    }
    void setBpm(float bpm) override {
            currentBPM = bpm;
            processInput(input);  // Recalculate when BPM changes
        }
    
private:
    ofParameter<vector<float>> input;
    ofParameter<int> operation;
    ofParameter<vector<float>> output;
    
    ofEventListeners listeners;
    
    float currentBPM;
    
    void processInput(const vector<float>& vf) {
        vector<float> out;
        vector<string> operations = {"ms-hz", "hz-ms", "beat-ms", "ms-beat", "frame-beat", "beat-frame", "soundMeters-ms", "ms-soundMeters", "pitch-hz", "hz-pitch", "speed-semitones", "semitones-speed"};
            int opIndex = operation.get(); // Retrieve the integer value
            string op = operations[opIndex]; // Convert the integer index to its corresponding string
            
        for(const auto& value : vf){
            if(op == "ms-hz") {
                out.push_back(1000.0 / value);
            }
            else if(op == "hz-ms") {
                out.push_back(1000.0 / value);
            }
            else if(op == "beat-ms") {
                float bpm = getOceanodeBPM();
                out.push_back((1.0 / (bpm / 60.0)) * 1000.0 * value);
            }
            else if(op == "ms-beat") {
                float bpm = getOceanodeBPM();
                out.push_back(value / ((1.0 / (bpm / 60.0)) * 1000.0));
            }
            else if(op == "frame-beat") {
                float bpm = getOceanodeBPM();
                float bps = bpm / 60.0;  // Beats per second
                float fpb = ofGetFrameRate() / bps;  // Frames per beat
                out.push_back(value / fpb);  // Convert frames to beats
            }
            else if(op == "beat-frame") {
                float bpm = getOceanodeBPM();
                float bps = bpm / 60.0;  // Beats per second
                float fpb = ofGetFrameRate() / bps;  // Frames per beat
                out.push_back(value * fpb);  // Convert beats to frames
            }
            else if(op == "soundMeters-ms") {
                // Assuming speed of sound is 343 meters per second
                out.push_back(value / 343.0 * 1000.0);
            }
            else if(op == "ms-soundMeters") {
                out.push_back(value * 343.0 / 1000.0);
            }
            else if(op == "pitch-hz") {
                out.push_back(440.0 * pow(2.0, (value - 69.0) / 12.0));
            }
            else if(op == "hz-pitch") {
                out.push_back(69.0 + 12.0 * log2(value / 440.0));
            }
            else if(op == "speed-semitones") {
                out.push_back(12.0 * log2(value));
            }
            else if(op == "semitones-speed") {
                out.push_back(pow(2.0, value / 12.0));
            }
        }
        output = out;
       }
    
    float getOceanodeBPM() {
        return currentBPM;
    }
};

#endif /* CONVERSIONS_H */
