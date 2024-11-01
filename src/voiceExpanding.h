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

        addParameter(inputPitch.set("Input Pitch", vector<float>(1, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 127.0f)));
        addParameter(inputGate.set("Input Gate", vector<float>(1, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 1.0f)));
        addParameter(outputSize.set("Output Size", 8, 1, 128));
        addParameter(outputPitch.set("Output Pitch", vector<float>(8, 0.0f), vector<float>(8, 0.0f), vector<float>(8, 127.0f)));
        addParameter(outputGate.set("Output Gate", vector<float>(8, 0.0f), vector<float>(8, 0.0f), vector<float>(8, 1.0f)));

        listeners.push(outputSize.newListener([this](int &size){
            resizeOutputs(size);
        }));

        resizeOutputs(outputSize);
    }

    void update(ofEventArgs &args) override {
        try {
            // Get input vectors safely
            const vector<float>& inPitch = inputPitch.get();
            const vector<float>& inGate = inputGate.get();
            
            // Safety check for input vectors
            if(inPitch.empty() || inGate.empty()) {
                return;
            }
            
            // Get minimum size between input vectors
            size_t inSize = std::min(inPitch.size(), inGate.size());
            
            // Get and check output vectors
            vector<float> outPitch = outputPitch.get();
            vector<float> outGate = outputGate.get();
            
            // Safety check for output vectors
            if(outPitch.empty() || outGate.empty()) {
                return;
            }
            size_t outSize = outPitch.size();

            // Ensure state vectors are properly sized
            if(previousGates.size() != inSize) {
                previousGates = vector<float>(inSize, 0.0f);
                currentSlotIndices = vector<size_t>(inSize, 0);
            }

            // Duplicate input pitches safely
            for(size_t i = 0; i < outSize && !inPitch.empty(); ++i) {
                outPitch[i] = inPitch[i % inPitch.size()];
            }

            // Clear output gates
            fill(outGate.begin(), outGate.end(), 0.0f);

            // Create pitch-to-slots mapping
            map<int, vector<size_t>> pitchToSlots;
            for(size_t i = 0; i < outSize; ++i) {
                if(i < outPitch.size()) {
                    int pitch = static_cast<int>(outPitch[i]);
                    pitchToSlots[pitch].push_back(i);
                }
            }

            // Process gates
            for(size_t i = 0; i < inSize; ++i) {
                if(i >= inPitch.size() || i >= inGate.size() || i >= previousGates.size() || i >= currentSlotIndices.size()) {
                    continue;
                }

                int pitch = static_cast<int>(inPitch[i]);
                auto slotsIt = pitchToSlots.find(pitch);
                
                if(slotsIt != pitchToSlots.end() && !slotsIt->second.empty()) {
                    const auto& slots = slotsIt->second;
                    
                    // Detect rising edge
                    bool risingEdge = (inGate[i] > 0.01f) && (previousGates[i] <= 0.01f);
                    
                    // Update slot index on rising edge
                    if(risingEdge) {
                        currentSlotIndices[i] = (currentSlotIndices[i] + 1) % slots.size();
                    }
                    
                    // Set gate if active
                    if(inGate[i] > 0.01f) {
                        size_t slotIndex = currentSlotIndices[i] % slots.size();
                        size_t targetSlot = slots[slotIndex];
                        if(targetSlot < outGate.size()) {
                            outGate[targetSlot] = inGate[i];
                        }
                    }
                    
                    previousGates[i] = inGate[i];
                }
            }

            // Set output parameters safely
            outputPitch.set(outPitch);
            outputGate.set(outGate);
            
        } catch(const std::exception& e) {
            ofLogError("voiceExpanding") << "Error in update: " << e.what();
        }
    }

private:
    ofParameter<vector<float>> inputPitch;
    ofParameter<vector<float>> inputGate;
    ofParameter<int> outputSize;
    ofParameter<vector<float>> outputPitch;
    ofParameter<vector<float>> outputGate;
    
    vector<float> previousGates;
    vector<size_t> currentSlotIndices;
    ofEventListeners listeners;

    void resizeOutputs(int size) {
        if(size > 0) {
            vector<float> newPitch(size, 0.0f);
            vector<float> newGate(size, 0.0f);
            outputPitch.set(newPitch);
            outputGate.set(newGate);
        }
    }
};
