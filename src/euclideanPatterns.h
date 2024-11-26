#ifndef euclideanPatterns_h
#define euclideanPatterns_h

#include "ofxOceanodeNodeModel.h"
#include <algorithm>

class euclideanPatterns : public ofxOceanodeNodeModel {
public:
    euclideanPatterns() : ofxOceanodeNodeModel("Euclidean Patterns") {
        description = "Generates Euclidean rhythmic patterns for each output sequence, distributing onsets evenly across a defined length with optional offset control. "
                      "Additionally, accents can be applied to the onsets using a secondary Euclidean pattern, with accent positions offset and accent strength customizable. "
        "Accented onsets have a value of 1, while non-accented onsets are modulated by the accent strength parameter.";

        addInspectorParameter(numOutputs.set("Num Outputs", 1, 1, 16));
    }

    void setup() override {
        addParameter(length.set("Length[]", vector<int>(1, 1), vector<int>(1, 1), vector<int>(1, INT_MAX)));
        addParameter(onsets.set("Onsets[]", vector<int>(1, 1), vector<int>(1, 0), vector<int>(1, INT_MAX)));
        addParameter(offset.set("Offset[]", vector<int>(1, 0), vector<int>(1, 0), vector<int>(1, INT_MAX)));

        // New accent-related parameters
        addParameter(accents.set("Accents[]", vector<int>(1, 0), vector<int>(1, 0), vector<int>(1, INT_MAX)));
        addParameter(accOffset.set("AccOffset[]", vector<int>(1, 0), vector<int>(1, 0), vector<int>(1, INT_MAX)));
        addParameter(accStrength.set("AccStrength[]", vector<float>(1, 0.5f), vector<float>(1, 0.0f), vector<float>(1, 1.0f)));

        // Initialize first output
        outputs.emplace_back();
        outputs[0].set("Output 1", vector<float>(1, 0.0f), vector<float>(1, 0.0f), vector<float>(1, 1.0f));
        addOutputParameter(outputs[0]);

        setupListeners();
        calculate();
    }

private:
    void setupListeners() {
        listeners.push(numOutputs.newListener([this](int &) {
            updateNumOutputs();
        }));

        listeners.push(length.newListener([this](vector<int> &value) {
            calculate();
        }));

        listeners.push(onsets.newListener([this](vector<int> &value) {
            calculate();
        }));

        listeners.push(offset.newListener([this](vector<int> &value) {
            calculate();
        }));

        // Listeners for new parameters
        listeners.push(accents.newListener([this](vector<int> &value) {
            calculate();
        }));

        listeners.push(accOffset.newListener([this](vector<int> &value) {
            calculate();
        }));

        listeners.push(accStrength.newListener([this](vector<float> &value) {
            calculate();
        }));
    }

    void updateNumOutputs() {
        int oldSize = outputs.size();
        int newSize = numOutputs;

        // If reducing size
        if (newSize < oldSize) {
            for (int i = oldSize - 1; i >= newSize; i--) {
                removeParameter("Output " + ofToString(i + 1));
            }
            outputs.resize(newSize);
        }
        // If increasing size
        else if (newSize > oldSize) {
            outputs.resize(newSize);
            for (int i = oldSize; i < newSize; i++) {
                outputs[i].set("Output " + ofToString(i + 1),
                               vector<float>(1, 0.0f),
                               vector<float>(1, 0.0f),
                               vector<float>(1, 1.0f));
                addParameter(outputs[i]);
            }
        }

        calculate();
    }

    void calculate() {
        for (size_t i = 0; i < outputs.size(); i++) {
            int lengthVal = getValueForIndex(length, i);
            int onsetsVal = getValueForIndex(onsets, i);
            int offsetVal = getValueForIndex(offset, i);

            // Accent-related values
            int accentsVal = getValueForIndex(accents, i);
            int accOffsetVal = getValueForIndex(accOffset, i);
            float accStrengthVal = getValueForIndex(accStrength, i);

            // Ensure valid values
            lengthVal = std::max(1, lengthVal);
            onsetsVal = std::max(0, onsetsVal);
            accentsVal = std::min(accentsVal, onsetsVal);  // Ensure accents do not exceed the number of onsets

            // Calculate the base pattern (onsets)
            vector<float> pattern(lengthVal, 0.0f);
            vector<int> onsetIndices; // Keep track of onset positions

            if (onsetsVal > 0) {
                for (int j = 0; j < onsetsVal; j++) {
                    int index = ((j * lengthVal) / onsetsVal + offsetVal) % lengthVal;
                    if (index < 0) index += lengthVal;
                    pattern[index] = 1.0f; // Mark onsets
                    onsetIndices.push_back(index); // Save onset positions
                }
            }

            // Apply accents (Euclidean distribution within the onsets)
            if (accentsVal > 0 && !onsetIndices.empty()) {
                vector<int> accentedOnsetIndices;

                for (int j = 0; j < accentsVal; j++) {
                    int accIndex = ((j * onsetIndices.size()) / accentsVal + accOffsetVal) % onsetIndices.size();
                    if (accIndex < 0) accIndex += onsetIndices.size();
                    accentedOnsetIndices.push_back(onsetIndices[accIndex]); // Mark accented onsets
                }

                // Apply strength to non-accented onsets
                for (int idx : onsetIndices) {
                    if (std::find(accentedOnsetIndices.begin(), accentedOnsetIndices.end(), idx) == accentedOnsetIndices.end()) {
                        // Non-accented onsets get the reduced strength
                        pattern[idx] = 1.0f - accStrengthVal;
                    }
                }
            }

            outputs[i].set(pattern);
        }
    }


    int getValueForIndex(const ofParameter<vector<int>> &param, size_t index) {
        const auto &vec = param.get();
        if (vec.empty()) return 0;
        if (index < vec.size()) return vec[index];
        return vec.back();
    }

    float getValueForIndex(const ofParameter<vector<float>> &param, size_t index) {
        const auto &vec = param.get();
        if (vec.empty()) return 0.0f;
        if (index < vec.size()) return vec[index];
        return vec.back();
    }

    ofEventListeners listeners;
    ofParameter<int> numOutputs;
    ofParameter<vector<int>> length;
    ofParameter<vector<int>> onsets;
    ofParameter<vector<int>> offset;
    ofParameter<vector<int>> accents;
    ofParameter<vector<int>> accOffset;
    ofParameter<vector<float>> accStrength;
    vector<ofParameter<vector<float>>> outputs;
};

#endif /* euclideanPatterns_h */
