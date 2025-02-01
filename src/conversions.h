#ifndef CONVERSIONS_H
#define CONVERSIONS_H

#include "ofxOceanodeNodeModel.h"
#include <cmath>
#include <algorithm>

class conversions : public ofxOceanodeNodeModel {
public:
    conversions() : ofxOceanodeNodeModel("Conversions") {
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameterDropdown(operation, "Op", 0, {"ms-hz", "hz-ms", "beat-ms", "ms-beat", "frame-beat", "beat-frame", "soundMeters-ms", "ms-soundMeters", "pitch-hz", "hz-pitch", "speed-semitones", "semitones-speed", "beat-hz", "hz-beat", "frame-ms", "ms-frame", "frame-pitch", "pitch-fps", "pitch-ms", "ms-pitch", "pitch-cm", "hz-cm", "cm-pitch", "cm-hz", "amp-dB", "dB-amp", "beat-pitch", "pitch-beat", "bpm-pitch", "pitch-bpm", "luminance-volume", "volume-luminance", "lin-log", "log-lin"});
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
    
    // Convert linear 0-1 to logarithmic 0-1 with psychoacoustic curve
    float linearToLog(float value) {
        // Ensure input is in 0-1 range
        value = std::clamp(value, 0.0f, 1.0f);
        
        // Constants for psychoacoustic curve
        const float minDb = -60.0f;  // Minimum dB (silence threshold)
        const float epsilon = 1e-6f;  // Small value to prevent log(0)
        
        // Convert to dB scale with smooth transition near zero
        float dbValue = 20.0f * log10(value + epsilon);
        
        // Normalize to 0-1 range
        return (dbValue - minDb) / (-minDb);
    }
    
    // Convert logarithmic 0-1 to linear 0-1 with psychoacoustic curve
    float logToLinear(float value) {
        // Ensure input is in 0-1 range
        value = std::clamp(value, 0.0f, 1.0f);
        
        // Constants for psychoacoustic curve
        const float minDb = -60.0f;  // Minimum dB (silence threshold)
        
        // Convert from normalized to dB
        float dbValue = value * (-minDb) + minDb;
        
        // Convert from dB to linear scale
        return pow(10.0f, dbValue / 20.0f);
    }
    
    void processInput(const vector<float>& vf) {
        vector<float> out;
        vector<string> operations = {"ms-hz", "hz-ms", "beat-ms", "ms-beat", "frame-beat", "beat-frame", "soundMeters-ms", "ms-soundMeters", "pitch-hz", "hz-pitch", "speed-semitones", "semitones-speed", "beat-hz", "hz-beat", "frame-ms", "ms-frame", "frame-pitch", "pitch-fps", "pitch-ms", "ms-pitch", "pitch-cm", "hz-cm", "cm-pitch", "cm-hz", "amp-dB", "dB-amp", "beat-pitch", "pitch-beat", "bpm-pitch", "pitch-bpm", "luminance-volume", "volume-luminance", "lin-log", "log-lin"};
        
        int opIndex = operation.get();
        string op = operations[opIndex];
        
        for(const auto& value : vf){
            if(op == "lin-log") {
                out.push_back(linearToLog(value));
            }
            else if(op == "log-lin") {
                out.push_back(logToLinear(value));
            }
            else if(op == "ms-hz") {
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
            else if(op == "beat-hz") {
                float bpm = getOceanodeBPM();
                out.push_back(bpm / 60.0 * value);  // Convert beats to hertz (cycles per second)
            }
            else if(op == "hz-beat") {
                float bpm = getOceanodeBPM();
                out.push_back(value / (bpm / 60.0));  // Convert hertz to beats
            }
            else if(op == "frame-ms") {
                out.push_back((value / ofGetFrameRate()) * 1000.0);  // Convert frame count to milliseconds
            }
            else if(op == "ms-frame") {
                out.push_back(1000.0 / value);  // Convert milliseconds to frames per second
            }
            else if(op == "pitch-ms") {
                float hz = 440.0 * pow(2.0, (value - 69) / 12.0);
                float period_seconds = 1.0 / hz;
                float milliseconds = period_seconds * 1000.0;
                out.push_back(milliseconds);
            }
            else if(op == "ms-pitch") {
                float period_seconds = value / 1000.0;
                float hz = 1.0 / period_seconds;
                float pitch = 69 + 12 * log2(hz / 440.0);
                out.push_back(pitch);
            }
            else if(op == "frame-pitch") {
                float milliseconds = (value / ofGetFrameRate()) * 1000.0;
                float period_seconds = milliseconds / 1000.0;
                float hz = 1.0 / period_seconds;
                float pitch = 69 + 12 * log2(hz / 440.0);
                out.push_back(pitch);
            }
            else if(op == "pitch-fps") {
                float hz = 440.0 * pow(2.0, (value - 69) / 12.0);
                float period_seconds = 1.0 / hz;
                float milliseconds = period_seconds * 1000.0;
                float fps = 1000.0 / milliseconds;
                out.push_back(fps);
            }
            else if(op == "pitch-cm") {
                float hz = 440.0 * pow(2.0, (value - 69) / 12.0);
                float wavelength_cm = 34300.0 / hz;
                out.push_back(wavelength_cm);
            }
            else if(op == "hz-cm") {
                float wavelength_cm = 34300.0 / value;
                out.push_back(wavelength_cm);
            }
            else if(op == "cm-pitch") {
                float hz = 34300.0 / value;
                float pitch = 69 + 12 * log2(hz / 440.0);
                out.push_back(pitch);
            }
            else if(op == "cm-hz") {
                float hz = 34300.0 / value;
                out.push_back(hz);
            }
            else if(op == "amp-dB") {
                if (value <= 0) {
                    out.push_back(-INFINITY);
                } else {
                    float dB = 20 * log10(value);
                    out.push_back(dB);
                }
            }
            else if(op == "dB-amp") {
                float amp = pow(10, value / 20);
                out.push_back(amp);
            }
            else if (op == "beat-pitch") {
                float bpm = getOceanodeBPM();
                float hz = bpm / 60.0 * value;
                float pitch = 69 + 12 * log2(hz / 440.0);
                out.push_back(pitch);
            }
            else if (op == "pitch-beat") {
                float hz = 440.0 * pow(2.0, (value - 69) / 12.0);
                float bpm = getOceanodeBPM();
                float beat = hz / (bpm / 60.0);
                out.push_back(beat);
            }
            else if (op == "bpm-pitch") {
                float hz = value / 60.0; // Convert BPM to Hz
                float pitch = 69.0 + 12.0 * log2(hz / 440.0);
                while (pitch < 60.0) pitch += 12.0;
                while (pitch > 72.0) pitch -= 12.0;
                out.push_back(pitch);
            }
            else if (op == "pitch-bpm") {
                float hz = 440.0 * pow(2.0, (value - 69.0) / 12.0);
                float bpm = hz * 60.0;
                while (bpm < 50.0) bpm *= 2.0;
                while (bpm > 250.0) bpm /= 2.0;
                out.push_back(bpm);
            }
            else if (op == "luminance-volume") {
                float luminance = std::clamp(value, 0.0f, 1.0f);
                float gamma = 2.2f;
                float corrected_luminance = pow(luminance, 1.0f/gamma);
                float epsilon = 1e-10f;
                float volume_db = 20.0f * log10(corrected_luminance + epsilon);
                float amplitude_multiplier = pow(10.0f, volume_db / 20.0f);
                out.push_back(amplitude_multiplier);
            }
            else if (op == "volume-luminance") {
                float volume = std::clamp(value, 0.0f, 1.0f);
                float epsilon = 1e-10f;
                float volume_db = 20.0f * log10(volume + epsilon);
                float linear_luminance = pow(10.0f, volume_db / 20.0f);
                float gamma = 2.2f;
                float luminance = pow(linear_luminance, gamma);
                luminance = std::clamp(luminance, 0.0f, 1.0f);
                out.push_back(luminance);
            }
        }
        output = out;
    }
    
    float getOceanodeBPM() {
        return currentBPM;
    }
};

#endif /* CONVERSIONS_H */
