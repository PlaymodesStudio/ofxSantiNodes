#include "voiceStealing.h"
#include <algorithm>

voiceStealing::voiceStealing() : ofxOceanodeNodeModel("Voice Stealing") {}

void voiceStealing::setup() {
    description = "Manages polyphonic voice allocation with auxiliary inputs. Assigns incoming notes and auxiliary signals to available voices.";

    // Main inputs
    addParameter(outputSize.set("Output Size", 8, 1, 128));
    
    addParameter(inputPitch.set("Input Pitch", {}, {0}, {127}));
    addParameter(inputGate.set("Input Gate", {}, {0}, {1}));
    
    // Auxiliary inputs
    addParameter(inputAux1.set("Input Aux1", {}, {-1}, {1}));
    addParameter(inputAux2.set("Input Aux2", {}, {-1}, {1}));
    addParameter(inputAux3.set("Input Aux3", {}, {-1}, {1}));
    addParameter(inputAux4.set("Input Aux4", {}, {-1}, {1}));
    
    // Main outputs
    addOutputParameter(outputPitch.set("Output Pitch", {}, {0}, {127}));
    addOutputParameter(outputGate.set("Output Gate", {}, {0}, {1}));
    
    // Auxiliary outputs
    addOutputParameter(outputAux1.set("Output Aux1", {}, {-1}, {1}));
    addOutputParameter(outputAux2.set("Output Aux2", {}, {-1}, {1}));
    addOutputParameter(outputAux3.set("Output Aux3", {}, {-1}, {1}));
    addOutputParameter(outputAux4.set("Output Aux4", {}, {-1}, {1}));

    listeners.push(outputSize.newListener([this](int &size){
        resizeOutputs(size);
    }));

    resizeOutputs(outputSize);
}

void voiceStealing::update(ofEventArgs &args) {
    const auto& inPitch = inputPitch.get();
    const auto& inGate = inputGate.get();
    const auto& inAux1 = inputAux1.get();
    const auto& inAux2 = inputAux2.get();
    const auto& inAux3 = inputAux3.get();
    const auto& inAux4 = inputAux4.get();
    
    auto outPitch = outputPitch.get();
    auto outGate = outputGate.get();
    auto outAux1 = outputAux1.get();
    auto outAux2 = outputAux2.get();
    auto outAux3 = outputAux3.get();
    auto outAux4 = outputAux4.get();

    size_t inSize = std::min({inPitch.size(), inGate.size()});
    size_t outSize = outPitch.size();

    // Update ages and release inactive voices
    for (size_t i = 0; i < outSize; ++i) {
        if (outGate[i] > 0.01f) {
            voiceAge[i]++;
        } else {
            voiceAge[i] = 0;
        }
    }

    // Process all input voices
    for (size_t i = 0; i < inSize; ++i) {
        if (inGate[i] > 0.01f) {  // Active input voice
            size_t targetSlot = i % outSize;
            
            // If the preferred slot is occupied, find the oldest voice
            if (outGate[targetSlot] > 0.01f && outPitch[targetSlot] != inPitch[i]) {
                targetSlot = findOldestVoice(outGate);
            }

            // Allocate or update the voice and auxiliary signals
            outPitch[targetSlot] = inPitch[i];
            outGate[targetSlot] = inGate[i];
            
            // Update auxiliary signals if they exist
            if (i < inAux1.size()) outAux1[targetSlot] = inAux1[i];
            if (i < inAux2.size()) outAux2[targetSlot] = inAux2[i];
            if (i < inAux3.size()) outAux3[targetSlot] = inAux3[i];
            if (i < inAux4.size()) outAux4[targetSlot] = inAux4[i];
            
            voiceAge[targetSlot] = 1;  // Reset age for newly allocated or updated voice
        }
    }

    // Release any remaining voices that are no longer active in the input
    for (size_t i = 0; i < outSize; ++i) {
        bool isActive = false;
        for (size_t j = 0; j < inSize; ++j) {
            if (inGate[j] > 0.01f && outPitch[i] == inPitch[j]) {
                isActive = true;
                break;
            }
        }
        if (!isActive) {
            outGate[i] = 0.0f;
            // Clear auxiliary outputs for inactive voices
            outAux1[i] = 0.0f;
            outAux2[i] = 0.0f;
            outAux3[i] = 0.0f;
            outAux4[i] = 0.0f;
        }
    }

    // Update all outputs
    outputPitch = outPitch;
    outputGate = outGate;
    outputAux1 = outAux1;
    outputAux2 = outAux2;
    outputAux3 = outAux3;
    outputAux4 = outAux4;
}

void voiceStealing::resizeOutputs(int size) {
    outputPitch.set(std::vector<float>(size, 0.0f));
    outputGate.set(std::vector<float>(size, 0.0f));
    outputAux1.set(std::vector<float>(size, 0.0f));
    outputAux2.set(std::vector<float>(size, 0.0f));
    outputAux3.set(std::vector<float>(size, 0.0f));
    outputAux4.set(std::vector<float>(size, 0.0f));
    voiceAge.resize(size, 0);
}

size_t voiceStealing::findOldestVoice(const std::vector<float>& gates) {
    size_t oldestSlot = 0;
    int maxAge = voiceAge[0];

    for (size_t i = 1; i < gates.size(); ++i) {
        if (voiceAge[i] > maxAge) {
            maxAge = voiceAge[i];
            oldestSlot = i;
        }
    }

    return oldestSlot;
}
