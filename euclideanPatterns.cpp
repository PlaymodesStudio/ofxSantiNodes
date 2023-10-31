//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#include "euclideanPatterns.h"
#include <algorithm>

void euclideanPatterns::calculate() {
    int lengthVal = length.get();
    int onsetsVal = onsets.get();
    int offsetVal = offset.get();

    // Reset output vector
    vector<float> outputVector(lengthVal, 0.0f);

    // Calculate Euclidean rhythm
    if (onsetsVal > 0 && lengthVal > 0) {
        for (int i = 0; i < onsetsVal; i++) {
            int index = ((i * lengthVal) / onsetsVal + offsetVal) % lengthVal;
            outputVector[index] = 1.0f;
        }
    }

    output = outputVector;
}
