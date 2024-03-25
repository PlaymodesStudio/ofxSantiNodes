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
    vector<float> tempPitchOut(outputSize, 0); // Initialize based on current output size.
    vector<float> tempGateOut(outputSize, 0);  // Initialize based on current output size.

    // Make sure the auxiliary vectors match the output size at the start of each update.
    if(voiceAge.size() != outputSize) {
        voiceAge.resize(outputSize, 0);
    }

    // A vector to mark which output slots have been updated in this cycle.
    vector<bool> slotUpdated(outputSize, false);

    // Process active notes in gateIn.
    for (size_t i = 0; i < gateIn.get().size(); ++i) {
        float gateValue = gateIn.get()[i];

        if (gateValue > 0.01) { // For active notes
            int slotIndex = -1;

            // First, try to find a free slot (prioritize lower indices).
            for (size_t j = 0; j < outputSize; ++j) {
                if (tempGateOut[j] <= 0.01) { // This checks if the slot is free.
                    slotIndex = j;
                    break; // Stop at the first free slot.
                }
            }

            // If no free slot is found, find the oldest active slot.
            if (slotIndex == -1) {
                slotIndex = findOldestSlot();
            }

            // Allocate or update the voice if a valid slot is found.
            if (slotIndex != -1) {
                tempPitchOut[slotIndex] = pitchIn.get()[i];
                tempGateOut[slotIndex] = gateValue;
                voiceAge[slotIndex] = 1; // Reset age for this voice.
                slotUpdated[slotIndex] = true;
            }
        }
    }

    // Turn off notes for slots that weren't updated (handle note offs).
    for (size_t i = 0; i < outputSize; ++i) {
        if (!slotUpdated[i]) {
            tempGateOut[i] = 0; // Implicit note off for slots not updated in this cycle.
            voiceAge[i] = 0; // Reset age as the slot is now free.
        }
        else {
            // Increment age for slots that were kept active.
            voiceAge[i]++;
        }
    }

    // Update the actual outputs.
    pitchOut = tempPitchOut;
    gateOut = tempGateOut;

    // Remember the gate values for the next cycle.
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
