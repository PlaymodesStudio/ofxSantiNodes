#include "euclideanTicksPoly.h"
#include <vector>
#include <algorithm>

void euclideanTicksPoly::setup() {
    addParameter(inCount.set("In Count", {0}, {0}, {INT_MAX}));
    addParameter(length.set("Length", {1}, {1}, {INT_MAX}));
    addParameter(onsets.set("Onsets", {1}, {0}, {INT_MAX}));
    addParameter(offset.set("Offset", {0}, {0}, {INT_MAX}));
    addParameter(retrigger.set("Retrigger", false));
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
    
    // Add update listener
    updateListener = ofEvents().update.newListener(this, &euclideanTicksPoly::update);

    outCountVector.resize(1, 0);
    prevInCount.resize(1, -1);
    previousGatesOut.resize(1, 0.0f);
    insertZeroNextFrame.resize(1, false);
}

void euclideanTicksPoly::update(ofEventArgs &args){
    calculate();
}


void euclideanTicksPoly::calculate() {
    std::vector<int> inCountVal = inCount.get();
    std::vector<int> lengthVal = length.get();
    std::vector<int> onsetsVal = onsets.get();
    std::vector<int> offsetVal = offset.get();
    
    // Resize and fill vectors
    size_t maxSize = std::max({inCountVal.size(), lengthVal.size(), onsetsVal.size(), offsetVal.size()});
    inCountVal.resize(maxSize, 0);
    lengthVal.resize(maxSize, 1);
    onsetsVal.resize(maxSize, 1);
    offsetVal.resize(maxSize, 0);
    outCountVector.resize(maxSize, 0);
    prevInCount.resize(maxSize, -1);
    previousGatesOut.resize(maxSize, 0.0f);
    insertZeroNextFrame.resize(maxSize, false);
    
    std::vector<float> gatesOutVector(maxSize, 0.0f);
    
    for(size_t i = 0; i < maxSize; i++) {
        int length = lengthVal[i];
        int onsets = std::min(onsetsVal[i], length);
        int offset = offsetVal[i];
        int inCountValue = inCountVal[i];
        int inCountModulo = (inCountValue + offset) % length;
        int onsetmod = onsetsVal[i];
        
        // Build the rhythm pattern
        std::vector<bool> rhythm(length, false);
        for(int j = 0; j < onsets; j++) {
            rhythm[(j * length) / onsets] = true;
        }
        
        bool currentGate = rhythm[inCountModulo];
        bool prevGate = rhythm[(inCountModulo - 1 + length) % length];
        
        // Handle retrigger logic
        if (insertZeroNextFrame[i]) {
            gatesOutVector[i] = 0.0f;
            insertZeroNextFrame[i] = false;
        } else if (retrigger && currentGate && previousGatesOut[i] == 1.0f) {
            gatesOutVector[i] = 0.0f;
            insertZeroNextFrame[i] = true;
        } else if (currentGate) {
            gatesOutVector[i] = 1.0f;
            if (shouldResetNext) {
                outCountVector[i] = 0;
                shouldResetNext = false;
            } else {
                outCountVector[i] = (outCountVector[i] + 1) % onsetmod;
            }
            tick.trigger();  // Trigger the tick output
        } else {
            gatesOutVector[i] = 0.0f;
        }
        
        previousGatesOut[i] = gatesOutVector[i];
        prevInCount[i] = inCountVal[i];
    }
    
    gatesOut = gatesOutVector;
    outCount = outCountVector;
}


void euclideanTicksPoly::reset() {
    size_t vecSize = inCount.get().size();
    std::vector<int> zeroVector(vecSize, 0);
    
    inCount = zeroVector;
    prevInCount = zeroVector;
    outCountVector = zeroVector;
    previousGatesOut.assign(vecSize, 0.0f);
    insertZeroNextFrame.assign(vecSize, false);
    
    calculate();
}

