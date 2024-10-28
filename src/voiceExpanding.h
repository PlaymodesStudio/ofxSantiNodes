#pragma once

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <map>

class voiceExpanding : public ofxOceanodeNodeModel {
public:
    voiceExpanding() : ofxOceanodeNodeModel("Voice Expanding") {}
    
    void setup() override {
        description = "Expands a small number of input voices (pitch + gate) over a larger vector. "
                     "Pitches are duplicated to fill the output slots, and incoming gates are distributed sequentially "
                     "to the corresponding pitch slots in the output.";

        addParameter(inputPitch.set("Input Pitch", {}, {0}, {127}));
        addParameter(inputGate.set("Input Gate", {}, {0}, {1}));
        addParameter(outputSize.set("Output Size", 8, 1, 128));
        addParameter(outputPitch.set("Output Pitch", {}, {0}, {127}));
        addParameter(outputGate.set("Output Gate", {}, {0}, {1}));

        listeners.push(outputSize.newListener([this](int &size){
            resizeOutputs(size);
        }));

        resizeOutputs(outputSize);
    }

    void update(ofEventArgs &args) override {
        const auto& inPitch = inputPitch.get();
        const auto& inGate = inputGate.get();
        
        // Ensure input vectors are valid
        size_t inSize = std::min(inPitch.size(), inGate.size());
        if (inSize == 0) {
            return;
        }

        // Get current output vectors
        auto outPitch = outputPitch.get();
        auto outGate = outputGate.get();
        size_t outSize = outPitch.size();
        
        if (outSize == 0) {
            return;
        }

        // Resize state vectors if needed
        if (previousGates.size() != inSize) {
            previousGates.resize(inSize, 0.0f);
            currentSlotIndices.resize(inSize, 0);
        }

        // Step 1: Duplicate input pitches to fill the output slots
        for (size_t i = 0; i < outSize; ++i) {
            outPitch[i] = inPitch[i % inSize];
        }

        // Step 2: Clear all output gates
        std::fill(outGate.begin(), outGate.end(), 0.0f);

        // Step 3: Create a map of pitch to available slots
        std::map<int, std::vector<size_t>> pitchToSlots;
        for (size_t i = 0; i < outSize; ++i) {
            int pitch = static_cast<int>(outPitch[i]);
            pitchToSlots[pitch].push_back(i);
        }

        // Step 4: Process each input gate
        for (size_t i = 0; i < inSize; ++i) {
            int pitch = static_cast<int>(inPitch[i]);
            auto& slots = pitchToSlots[pitch];
            
            if (!slots.empty()) {
                // Check for rising edge
                bool risingEdge = (inGate[i] > 0.01f) && (previousGates[i] <= 0.01f);
                
                // Advance to next slot on rising edge
                if (risingEdge) {
                    currentSlotIndices[i] = (currentSlotIndices[i] + 1) % slots.size();
                }
                
                // If gate is active, use current slot
                if (inGate[i] > 0.01f) {
                    size_t targetSlot = slots[currentSlotIndices[i]];
                    outGate[targetSlot] = inGate[i];
                }
            }
            
            // Update previous gate state
            previousGates[i] = inGate[i];
        }

        // Update output parameters
        outputPitch.set(outPitch);
        outputGate.set(outGate);
    }

private:
    ofParameter<std::vector<float>> inputPitch;
    ofParameter<std::vector<float>> inputGate;
    ofParameter<int> outputSize;
    ofParameter<std::vector<float>> outputPitch;
    ofParameter<std::vector<float>> outputGate;
    
    std::vector<float> previousGates;
    std::vector<size_t> currentSlotIndices;
    ofEventListeners listeners;

    void resizeOutputs(int size) {
        outputPitch.set(std::vector<float>(size, 0.0f));
        outputGate.set(std::vector<float>(size, 0.0f));
    }
};
