#pragma once

#include "ofxOceanodeNodeModel.h"
#include <algorithm>
#include <vector>

class vectorSymmetry : public ofxOceanodeNodeModel {
public:
    vectorSymmetry() : ofxOceanodeNodeModel("Vector Symmetry") {}

    void setup() override {
        description = "Repeats the beginning of a vector in alternating forward and mirrored segments. Symmetry sets the number of additional segments; RepEdge repeats the endpoint at each reflection.";
        addParameter(input.set("Input", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(symmetry.set("Symmetry", 0, 0, 10));
        addParameter(repEdge.set("RepEdge", false));
        addOutputParameter(output.set("Output", {0.0f}, {-FLT_MAX}, {FLT_MAX}));

        listeners.push(input.newListener([this](vector<float> &) { process(); }));
        listeners.push(symmetry.newListener([this](int &) { process(); }));
        listeners.push(repEdge.newListener([this](bool &) { process(); }));
        process();
    }

private:
    void process() {
        const auto &values = input.get();
        const size_t count = values.size();
        const int segments = symmetry.get() + 1;
        if (count == 0 || segments <= 1) {
            output.set(values);
            return;
        }

        // The source segment stays nonempty even if the requested number of
        // segments exceeds the number of input values.
        const size_t segmentLength = std::max<size_t>(1, (count + segments - 1) / segments);
        vector<float> result(count);
        const size_t period = repEdge.get() ? 2 * segmentLength
                                            : (segmentLength == 1 ? 1 : 2 * segmentLength - 2);
        for (size_t i = 0; i < count; ++i) {
            const size_t position = i % period;
            const size_t source = position < segmentLength ? position
                : (repEdge.get() ? period - 1 - position : period - position);
            result[i] = values[source];
        }
        output.set(result);
    }

    ofParameter<vector<float>> input;
    ofParameter<int> symmetry;
    ofParameter<bool> repEdge;
    ofParameter<vector<float>> output;
    ofEventListeners listeners;
};
