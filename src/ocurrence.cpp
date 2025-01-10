#include "ocurrence.h"
#include <unordered_map>

void Ocurrence::setup() {
    addParameter(inputVector.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
    addOutputParameter(outputVector.set("Output", {0}, {0}, {INT_MAX}));

    // Using newListener to attach a listener for inputVector changes
    // Correctly store the listener in the 'listeners' container
    listeners.push(inputVector.newListener([this](std::vector<float> &v){
        this->processInput();
    }));
}

void Ocurrence::processInput() {
    std::unordered_map<float, int> occurrenceCount;
    std::vector<int> result;
    const auto &input = inputVector.get();

    // Count occurrences
    for (const auto &value : input) {
        occurrenceCount[value]++;
    }

    // Generate output vector based on the counts
    for (const auto &value : input) {
        result.push_back(occurrenceCount[value]);
    }

    // Update the output vector parameter
    outputVector = result;
}
