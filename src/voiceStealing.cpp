#include "voiceStealing.h"

void voiceStealing::setup() {
    addParameter(pitchIn.set("Pitch In", {}, {0}, {127}));
    addParameter(gateIn.set("Gate In", {}, {0}, {1}));
    addParameter(outputSize.set("Output Size", 8, 1, 128));
    addParameter(pitchOut.set("Pitch Out", {}, {0}, {127}));
    addParameter(gateOut.set("Gate Out", {}, {0}, {1}));

    // Setup initial size for the lastGateValue vector
    lastGateValue.resize(outputSize.get(), 0);

    // Listener for outputSize changes to adjust the size of our vectors
    listeners.push(outputSize.newListener([&](int &size){
        // Adjust vectors to the new size
        vector<float> tempPitchOut(size, 0);
        vector<float> tempGateOut(size, 0);

        pitchOut = tempPitchOut;
        gateOut = tempGateOut;

        voiceAge.resize(size, 0);
        lastGateValue.resize(size, 0);
    }));
}

void voiceStealing::update(ofEventArgs &args) {
    vector<float> tempPitchOut = pitchOut.get(); // Start with current pitch values
    vector<float> tempGateOut(outputSize, 0);

    if(voiceAge.size() != outputSize) {
        voiceAge.resize(outputSize, 0);
    }

    vector<bool> slotUpdated(outputSize, false);
    size_t nextAvailableSlot = 0; // Keep track of the next slot to fill
    // First pass: Try to keep incoming voices in their original slots
    for (size_t i = 0; i < std::min(gateIn.get().size(), static_cast<size_t>(outputSize)); ++i) {
        float gateValue = gateIn.get()[i];
        if (gateValue > 0.01) { // Active note
            if (tempGateOut[i] <= 0.01) { // Original slot is free
                tempPitchOut[i] = pitchIn.get()[i];
                tempGateOut[i] = gateValue;
                voiceAge[i] = 1;
                slotUpdated[i] = true;
                nextAvailableSlot = std::max(nextAvailableSlot, i + 1);
            }
        }
    }
    // Second pass: Process remaining active incoming voices
    for (size_t i = 0; i < gateIn.get().size(); ++i) {
        float gateValue = gateIn.get()[i];
        if (gateValue > 0.01 && !slotUpdated[i]) { // Active note that wasn't processed in first pass
            int slotIndex = -1;
            // Try to allocate to the next available slot
            if (nextAvailableSlot < outputSize) {
                slotIndex = nextAvailableSlot++;
            } else {
                // If we've reached the end, find the oldest slot
                slotIndex = findOldestSlot();
            }
            // Allocate or update the voice if a valid slot is found
            if (slotIndex != -1) {
                tempPitchOut[slotIndex] = pitchIn.get()[i];
                tempGateOut[slotIndex] = gateValue;
                voiceAge[slotIndex] = 1;
                slotUpdated[slotIndex] = true;
            }
        }
    }
    // Update ages for active slots and turn off gates for unused slots
    for (size_t i = 0; i < outputSize; ++i) {
        if (slotUpdated[i]) {
            voiceAge[i]++;
        } else {
            tempGateOut[i] = 0;
            voiceAge[i] = 0;
            // We don't modify tempPitchOut[i] here, keeping its last value
        }
    }
    // Update the actual outputs
    pitchOut = tempPitchOut;
    gateOut = tempGateOut;
    // Remember the gate values for the next cycle
    lastGateValue = gateIn.get();
}

int voiceStealing::findOldestSlot() {
    int oldestIndex = -1;
    int oldestAge = -1;
    for (size_t i = 0; i < voiceAge.size(); ++i) {
        if (voiceAge[i] > oldestAge) {
            oldestAge = voiceAge[i];
            oldestIndex = i;
        }
    }
    return oldestIndex;
}






int voiceStealing::findAvailableOrOldestSlot(const vector<float>& gateOutVector) {
    int oldestSlotIndex = -1;
    int maxAge = -1;

    for (size_t i = 0; i < gateOutVector.size(); ++i) {
        if (gateOutVector[i] <= 0.01) return i; // Available slot found

        if (voiceAge[i] > maxAge) {
            maxAge = voiceAge[i];
            oldestSlotIndex = i;
        }
    }

    return oldestSlotIndex; // Return the oldest slot if no available slot found
}
