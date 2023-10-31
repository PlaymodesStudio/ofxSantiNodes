//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#include "euclideanTicks.h"
#include <algorithm>

void euclideanTicks::calculate() {
    int inCountVal = inCount.get();
    int lengthVal = length.get();
    int onsetsVal = onsets.get();
    int offsetVal = offset.get();

    // Reset pulse
    vector<float> pulseVector(lengthVal, 0.0f);

    // Reset totalOnsets if inCount has reset to 0
    if (inCountVal < prevInCount) {
        totalOnsets = 0;
    }
    prevInCount = inCountVal;

    // Calculate Euclidean rhythm
    if (onsetsVal > 0 && lengthVal > 0) {
        for (int i = 0; i < onsetsVal; i++) {
            int index = ((i * lengthVal) / onsetsVal + offsetVal) % lengthVal;
            if (index == inCountVal % lengthVal) {
                pulseVector[index] = 1.0f;
                totalOnsets++;
            }
        }
    }

    pulse = pulseVector;
    outCount = totalOnsets;
}
