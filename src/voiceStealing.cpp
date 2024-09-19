#include "voiceStealing.h"
#include <algorithm>

voiceStealing::voiceStealing() : ofxOceanodeNodeModel("Voice Stealing") {}

void voiceStealing::setup() {
    description = "Manages polyphonic voice allocation. Assigns incoming notes to available voices, prioritizing original slots. When all voices are occupied, it intelligently steals the oldest voice. Outputs pitch and gate signals for each voice.";

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

void voiceStealing::update(ofEventArgs &args) {
    const auto& inPitch = inputPitch.get();
    const auto& inGate = inputGate.get();
    auto outPitch = outputPitch.get();
    auto outGate = outputGate.get();

    size_t inSize = std::min(inPitch.size(), inGate.size());
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

            // Allocate or update the voice
            outPitch[targetSlot] = inPitch[i];
            outGate[targetSlot] = inGate[i];
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
        }
    }

    outputPitch = outPitch;
    outputGate = outGate;
}

void voiceStealing::resizeOutputs(int size) {
    outputPitch.set(std::vector<float>(size, 0.0f));
    outputGate.set(std::vector<float>(size, 0.0f));
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
