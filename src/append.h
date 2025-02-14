#ifndef append_h
#define append_h

#include "ofxOceanodeNodeModel.h"

class append : public ofxOceanodeNodeModel {
public:
    append() : ofxOceanodeNodeModel("Append") {}
    
    void setup() override {
        description = "Appends a specified string to the input string, adding a space between them";
        
        addParameter(inputString.set("Input", ""));
        addParameter(appendString.set("Append", ""));
        addOutputParameter(outputString.set("Output", ""));
        
        listener = inputString.newListener([this](string &){
            process();
        });
        
        appendListener = appendString.newListener([this](string &){
            process();
        });
    }
    
private:
    void process() {
        string input = inputString.get();
        string suffix = appendString.get();
        
        // Handle empty strings
        if(suffix.empty()) {
            outputString = input;
            return;
        }
        if(input.empty()) {
            outputString = suffix;
            return;
        }
        
        // Combine strings with a space in between
        outputString = input + " " + suffix;
    }
    
    ofParameter<string> inputString;
    ofParameter<string> appendString;
    ofParameter<string> outputString;
    ofEventListener listener;
    ofEventListener appendListener;
};

#endif /* append_h */
