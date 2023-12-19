#ifndef vectorFeedback_h
#define vectorFeedback_h

#include "ofxOceanodeNodeModel.h"

class vectorFeedback : public ofxOceanodeNodeModel {
public:
    vectorFeedback() : ofxOceanodeNodeModel("Vector Feedback") {}

    void setup() override {
        description = "Vector Feedback takes an input vector and applies feedback to it. The feedback is a product of the previous value and a feedback factor.";

        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(feedback.set("Feedback", {0.5f}, {0.0f}, {1.0f}));
        addParameter(clip.set("Clip", false));

        addOutputParameter(output.set("Output", {0}, {0}, {1}));

        inputListener = input.newListener([this](vector<float> &values) {
            processFeedback();
        });
    }

    void processFeedback() {
        const auto& in = input.get();
        const auto& fb = feedback.get();
        const bool shouldClip = clip.get();

        // Initialize previousInput if empty or if its size differs from the current input
        if (previousInput.empty() || previousInput.size() != in.size()) {
            previousInput.resize(in.size(), 0.0f);
        }

        vector<float> out(in.size(), 0.0f);

        for (size_t i = 0; i < in.size(); i++) {
            // Calculate feedback value based on previous output
            float feedbackValue = previousInput[i] * (i < fb.size() ? fb[i] : fb.back());
            
            // Add the feedback value to the current input
            float result = in[i] + feedbackValue;

            // Clip the result between 0 and 1 if clip is enabled
            if (shouldClip) {
                result = ofClamp(result, 0.0f, 1.0f);
            }

            out[i] = result;

            // Update previousInput with the result (clipped if necessary) for the next iteration
            previousInput[i] = result;
        }

        output = out;
    }


private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> feedback;
    ofParameter<bool> clip;
    ofParameter<vector<float>> output;

    vector<float> previousInput;

    ofEventListener inputListener;
};

#endif /* vectorFeedback_h */
