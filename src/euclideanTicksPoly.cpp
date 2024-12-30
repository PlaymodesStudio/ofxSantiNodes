#include "euclideanTicksPoly.h"
#include <vector>
#include <algorithm>

void euclideanTicksPoly::setup() {
    addParameter(inCount.set("In Count", {0}, {0}, {INT_MAX}));
    addParameter(length.set("Length", {1}, {1}, {INT_MAX}));
    addParameter(onsets.set("Onsets", {1}, {0}, {INT_MAX}));
    addParameter(offset.set("Offset", {0}, {0}, {INT_MAX}));
    addOutputParameter(gatesOut.set("Gates Out", {0.0f}, {0.0f}, {1.0f}));
    addOutputParameter(outCount.set("Out Count", {0}, {0}, {INT_MAX}));
    addOutputParameter(tick.set("Tick"));
    addParameter(resetButton.set("Reset"));
    addParameter(resetNext.set("Reset Next"));

    listener = inCount.newListener([this](std::vector<int> &value) {
        calculate();
    });

    resetButtonListener = resetButton.newListener([this]() {
        reset();
    });

    resetNextListener = resetNext.newListener([this]() {
        shouldResetNext = true;
    });

    outCountVector.resize(1, 0); // Initial size of 1
    prevInCount.resize(1, -1); // Initial size of 1
}

void euclideanTicksPoly::calculate() {
    std::vector<int> inCountVal = inCount.get();
    std::vector<int> lengthVal = length.get();
    std::vector<int> onsetsVal = onsets.get();
    std::vector<int> offsetVal = offset.get();

    if(inCountVal == prevInCount) return; // No changes, so return
    
    // Resize and fill the length, onsets, and offset vectors if they are scalars and inCount is a vector
    if(inCountVal.size() > 1) {
        if(lengthVal.size() == 1) lengthVal.resize(inCountVal.size(), lengthVal[0]);
        if(onsetsVal.size() == 1) onsetsVal.resize(inCountVal.size(), onsetsVal[0]);
        if(offsetVal.size() == 1) offsetVal.resize(inCountVal.size(), offsetVal[0]);
    }

    size_t maxSize = std::max({inCountVal.size(), lengthVal.size(), onsetsVal.size(), offsetVal.size()});
    inCountVal.resize(maxSize, 0);
    lengthVal.resize(maxSize, 1);
    onsetsVal.resize(maxSize, 1);
    offsetVal.resize(maxSize, 0);
    outCountVector.resize(maxSize, 0);
    prevInCount.resize(maxSize, -1);

    std::vector<float> gatesOutVector(maxSize, 0.0f);

    for(size_t i = 0; i < maxSize; i++) {
            int length = std::max(lengthVal[i], 1);  // Ensure length is at least 1
            int onsets = std::min(onsetsVal[i], length);
            int offset = offsetVal[i];
            int inCount = length > 0 ? (inCountVal[i] + offset) % length : 0;
            int onsetmod = std::max(onsetsVal[i], 1);  // Ensure onsetmod is at least 1

            std::vector<bool> rhythm(length, false);
            
            if (length > 0) {  // Only calculate rhythm if length is valid
                for(int j = 0; j < onsets; j++) {
                    int idx = (j * length) / onsets;
                    if (idx >= 0 && idx < length) {  // Add bounds check
                        rhythm[idx] = true;
                    }
                }

                if (inCount >= 0 && inCount < length && rhythm[inCount]) {  // Add bounds check
                    gatesOutVector[i] = 1.0f;
                    if (shouldResetNext) {
                        outCountVector[i] = 0;
                        shouldResetNext = false;
                    } else {
                        outCountVector[i] = (outCountVector[i] + 1) % onsetmod;
                    }
                    tick.trigger();
                }
            }
        }

    gatesOut = gatesOutVector;
    outCount = outCountVector;
    prevInCount = inCountVal;
}

void euclideanTicksPoly::reset() {
    size_t vecSize = inCount.get().size();
    std::vector<int> zeroVector(vecSize, 0);

    inCount = zeroVector; // Set all the inCount values to zero
    prevInCount = zeroVector; // Also reset the previous values
    outCountVector = zeroVector; // Reset the output count vector to zero

    calculate();  // Optionally, if you want to force the calculation again
}
