#ifndef ftos_h
#define ftos_h

#include "ofMain.h"
#include "ofxOceanodeNodeModel.h"

class ftos : public ofxOceanodeNodeModel {
public:
    ftos() : ofxOceanodeNodeModel("Float to String") {}
    
    void setup() override {
        description = "Converts a vector of floats to a string with a custom separator.";
        
        // Add input parameter for vector of floats
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        
        // Add parameter for separator
        addParameter(separator.set("Separator", " "));
        
        // Add output string parameter
        addParameter(output.set("Output", ""));
        
        // Add listener for input changes
        listener = input.newListener([this](vector<float> &vf){
            processInput();
        });
        
        // Add listener for separator changes
        separatorListener = separator.newListener([this](string &s){
            processInput();
        });
    }
    
private:
    void processInput() {
        const auto& inputVec = input.get();
        string result;
        
        // Early return if input is empty
        if(inputVec.empty()) {
            output = "";
            return;
        }
        
        // Process all numbers except the last one
        for(size_t i = 0; i < inputVec.size() - 1; ++i) {
            result += ofToString(inputVec[i]) + separator.get();
        }
        
        // Add the last number without separator
        result += ofToString(inputVec.back());
        
        output = result;
    }
    
    ofParameter<vector<float>> input;
    ofParameter<string> separator;
    ofParameter<string> output;
    ofEventListener listener;
    ofEventListener separatorListener;
};

#endif /* ftos_h */
