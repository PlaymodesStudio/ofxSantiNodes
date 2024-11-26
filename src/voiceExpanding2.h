#pragma once

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <queue>

class voiceExpanding2 : public ofxOceanodeNodeModel {
public:
    voiceExpanding2() : ofxOceanodeNodeModel("Voice Expanding 2") {}
    
    void setup() override {
        description = "Expands input voices sequentially over a larger vector. "
                     "New values are allocated to the next available output slot. "
                     "Pitch and aux values are retained after gate off.";

        // Input parameters
        addParameter(inputPitch.set("Input Pitch", vector<float>(1, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 127.0f)));
        addParameter(inputGate.set("Input Gate", vector<float>(1, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 1.0f)));
        addParameter(inputAux1.set("Input Aux1", vector<float>(1, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 1.0f)));
        addParameter(inputAux2.set("Input Aux2", vector<float>(1, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 1.0f)));
        addParameter(inputAux3.set("Input Aux3", vector<float>(1, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 1.0f)));
        
        // Output size control
        addParameter(outputSize.set("Output Size", 8, 1, 128));
        
        // Output parameters
        addOutputParameter(outputPitch.set("Output Pitch", vector<float>(8, 0.0f), vector<float>(8, 0.0f), vector<float>(8, 127.0f)));
        addOutputParameter(outputGate.set("Output Gate", vector<float>(8, 0.0f), vector<float>(8, 0.0f), vector<float>(8, 1.0f)));
        addOutputParameter(outputAux1.set("Output Aux1", vector<float>(8, 0.0f), vector<float>(8, 0.0f), vector<float>(8, 1.0f)));
        addOutputParameter(outputAux2.set("Output Aux2", vector<float>(8, 0.0f), vector<float>(8, 0.0f), vector<float>(8, 1.0f)));
        addOutputParameter(outputAux3.set("Output Aux3", vector<float>(8, 0.0f), vector<float>(8, 0.0f), vector<float>(8, 1.0f)));

        listeners.push(outputSize.newListener([this](int &size){
            handleSizeChange(size);
        }));

        lastOutputSize = outputSize;
        handleSizeChange(outputSize);
    }

    void update(ofEventArgs &args) override {
        try {
            // Get input vectors
            const vector<float>& inPitch = inputPitch.get();
            const vector<float>& inGate = inputGate.get();
            const vector<float>& inAux1 = inputAux1.get();
            const vector<float>& inAux2 = inputAux2.get();
            const vector<float>& inAux3 = inputAux3.get();
            
            // Safety check for mandatory inputs
            if(inPitch.empty() || inGate.empty()) {
                return;
            }
            
            // Get working size from pitch and gate
            size_t inSize = std::min(inPitch.size(), inGate.size());
            
            // Get output vectors (maintaining their current values)
            vector<float> outPitch = outputPitch.get();
            vector<float> outGate = outputGate.get();
            vector<float> outAux1 = outputAux1.get();
            vector<float> outAux2 = outputAux2.get();
            vector<float> outAux3 = outputAux3.get();
            
            size_t outSize = outPitch.size();

            // Ensure state vectors are properly sized
            if(previousGates.size() != inSize) {
                previousGates = vector<float>(inSize, 0.0f);
                inputToOutputMapping.clear();
            }

            // Only clear gate outputs, maintain pitch and aux values
            fill(outGate.begin(), outGate.end(), 0.0f);

            // Remove any mappings that point to invalid indices after resize
            for(auto it = inputToOutputMapping.begin(); it != inputToOutputMapping.end();) {
                if(it->second >= outSize) {
                    it = inputToOutputMapping.erase(it);
                } else {
                    ++it;
                }
            }

            // Ensure currentOutputSlot is valid
            if(currentOutputSlot >= outSize) {
                currentOutputSlot = outSize - 1;
            }

            // Process inputs and update mappings
            for(size_t i = 0; i < inSize; ++i) {
                // Detect rising edge
                bool risingEdge = (inGate[i] > 0.01f) && (previousGates[i] <= 0.01f);
                
                // On rising edge, allocate new output slot
                if(risingEdge) {
                    // If this input index doesn't have a mapping or its gate was off
                    if(inputToOutputMapping.find(i) == inputToOutputMapping.end()) {
                        // Assign next available slot
                        currentOutputSlot = (currentOutputSlot + 1) % outSize;
                        inputToOutputMapping[i] = currentOutputSlot;
                    }
                }
                
                // If this input has a mapping and gate is on, update outputs
                auto mappingIt = inputToOutputMapping.find(i);
                if(mappingIt != inputToOutputMapping.end() && inGate[i] > 0.01f) {
                    size_t outputIndex = mappingIt->second;
                    if(outputIndex < outSize) { // Additional safety check
                        // Update pitch and gate
                        outPitch[outputIndex] = inPitch[i];
                        outGate[outputIndex] = inGate[i];
                        
                        // Update aux values if they exist for this index
                        if(i < inAux1.size()) outAux1[outputIndex] = inAux1[i];
                        if(i < inAux2.size()) outAux2[outputIndex] = inAux2[i];
                        if(i < inAux3.size()) outAux3[outputIndex] = inAux3[i];
                    }
                }
                
                // Remove mapping when gate goes low
                if(inGate[i] <= 0.01f && previousGates[i] > 0.01f) {
                    inputToOutputMapping.erase(i);
                }
                
                previousGates[i] = inGate[i];
            }

            // Set output parameters
            outputPitch.set(outPitch);
            outputGate.set(outGate);
            outputAux1.set(outAux1);
            outputAux2.set(outAux2);
            outputAux3.set(outAux3);
            
        } catch(const std::exception& e) {
            ofLogError("voiceExpanding2") << "Error in update: " << e.what();
        }
    }

private:
    // Input parameters
    ofParameter<vector<float>> inputPitch;
    ofParameter<vector<float>> inputGate;
    ofParameter<vector<float>> inputAux1;
    ofParameter<vector<float>> inputAux2;
    ofParameter<vector<float>> inputAux3;
    
    // Output size control
    ofParameter<int> outputSize;
    int lastOutputSize;
    
    // Output parameters
    ofParameter<vector<float>> outputPitch;
    ofParameter<vector<float>> outputGate;
    ofParameter<vector<float>> outputAux1;
    ofParameter<vector<float>> outputAux2;
    ofParameter<vector<float>> outputAux3;
    
    // State tracking
    vector<float> previousGates;
    map<size_t, size_t> inputToOutputMapping;
    size_t currentOutputSlot = -1;
    ofEventListeners listeners;

    void handleSizeChange(int newSize) {
        if(newSize > 0) {
            // Create new vectors with the new size
            vector<float> newPitch(newSize, 0.0f);
            vector<float> newGate(newSize, 0.0f);
            vector<float> newAux1(newSize, 0.0f);
            vector<float> newAux2(newSize, 0.0f);
            vector<float> newAux3(newSize, 0.0f);

            // Copy existing values up to the minimum of old and new sizes
            const vector<float>& oldPitch = outputPitch.get();
            const vector<float>& oldAux1 = outputAux1.get();
            const vector<float>& oldAux2 = outputAux2.get();
            const vector<float>& oldAux3 = outputAux3.get();

            size_t copySize = std::min(static_cast<size_t>(newSize), oldPitch.size());
            
            std::copy_n(oldPitch.begin(), copySize, newPitch.begin());
            std::copy_n(oldAux1.begin(), copySize, newAux1.begin());
            std::copy_n(oldAux2.begin(), copySize, newAux2.begin());
            std::copy_n(oldAux3.begin(), copySize, newAux3.begin());

            // Set the new vectors
            outputPitch.set(newPitch);
            outputGate.set(newGate);
            outputAux1.set(newAux1);
            outputAux2.set(newAux2);
            outputAux3.set(newAux3);

            // Update lastOutputSize
            lastOutputSize = newSize;
        }
    }
};
