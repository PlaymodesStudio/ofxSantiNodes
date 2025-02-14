#ifndef prepend_h
#define prepend_h

#include "ofxOceanodeNodeModel.h"

class prepend : public ofxOceanodeNodeModel {
public:
    prepend() : ofxOceanodeNodeModel("Prepend") {}
    
    void setup() override {
        description = "Prepends a specified string to the input string, adding a space between them";
        
        addParameter(inputString.set("Input", ""));
        addParameter(prependString.set("Prepend", ""));
        addOutputParameter(outputString.set("Output", ""));
        
        listener = inputString.newListener([this](string &){
            process();
        });
        
        prependListener = prependString.newListener([this](string &){
            process();
        });
    }
    
private:
    void process() {
        string prefix = prependString.get();
        string input = inputString.get();
        
        // Handle empty strings
        if(prefix.empty()) {
            outputString = input;
            return;
        }
        if(input.empty()) {
            outputString = prefix;
            return;
        }
        
        // Combine strings with a space in between
        outputString = prefix + " " + input;
    }
    
    ofParameter<string> inputString;
    ofParameter<string> prependString;
    ofParameter<string> outputString;
    ofEventListener listener;
    ofEventListener prependListener;
};

#endif /* prepend_h */
