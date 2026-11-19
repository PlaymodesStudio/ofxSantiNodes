#pragma once

// Only model registration is replaced. Parameters/events, JSON, ImGui and the node are real.
#include "ofMain.h"
#include <map>

class ofxOceanodeNodeModel {
public:
    explicit ofxOceanodeNodeModel(const string&) {}
    virtual ~ofxOceanodeNodeModel() = default;
    virtual void setup() {}
    virtual void update(ofEventArgs&) {}
    virtual void draw(ofEventArgs&) {}
    virtual void presetSave(ofJson&) {}
    virtual void presetRecallBeforeSettingParameters(ofJson&) {}
    virtual void presetRecallAfterSettingParameters(ofJson&) {}
    virtual void presetHasLoaded() {}
    virtual void resetPhase() {}
    int getNumIdentifier() const { return 0; }
    template<class T> void addParameter(ofParameter<T>& p) { parameters_[p.getName()] = &p; }
    template<class T> void addOutputParameter(ofParameter<T>& p) { addParameter(p); }
    template<class T> void addInspectorParameter(ofParameter<T>& p) { addParameter(p); }
    void addSeparator(string = "", ofColor = ofColor(200, 200, 200, 255)) {}
    void addParameterDropdown(ofParameter<int>& p, const string& name, int value, const vector<string>& options) {
        addParameter(p.set(name,value,0,static_cast<int>(options.size())-1));
    }
    template<class T> ofParameter<T>& parameter(const string& name) {
        return *static_cast<ofParameter<T>*>(parameters_.at(name));
    }
    string description;
private:
    std::map<string,void*> parameters_;
};
