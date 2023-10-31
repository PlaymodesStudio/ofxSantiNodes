#include "ofxOceanodeNodeModel.h"

class merger : public ofxOceanodeNodeModel {
public:
    merger() : ofxOceanodeNodeModel("Merge") {
        description = "Merges event values from various inputs ";
        // Output setup
        output.set("Output", std::vector<float>{0}, std::vector<float>{-FLT_MAX}, std::vector<float>{FLT_MAX});
        addOutputParameter(output);

        // Inputs setup
        for(int i = 0; i < 4; i++) {
            ofParameter<std::vector<float>> newInput;
            newInput.set("In " + ofToString(i + 1), std::vector<float>{0}, std::vector<float>{-FLT_MAX}, std::vector<float>{FLT_MAX});
            addParameter(newInput);
            inputs.push_back(newInput);
            
            // Add listener
            auto listener = newInput.newListener([this](std::vector<float> &f) {
                mergeInputs(f);
            });
            inputListeners.push_back(std::make_shared<ofEventListener>(std::move(listener)));
        }
    }

    void mergeInputs(const std::vector<float> &newData) {
        output = newData; // This will trigger the event for the output, sending the new data.
    }

private:
    ofParameter<std::vector<float>> output;
    std::vector<ofParameter<std::vector<float>>> inputs;
    std::vector<std::shared_ptr<ofEventListener>> inputListeners;
};
