#pragma once

#include "ofxOceanodeNodeModel.h"
#include "ofJson.h"
#include <map>
#include <string>

class constellations : public ofxOceanodeNodeModel {
public:
    constellations() : ofxOceanodeNodeModel("Constellations") {
        setupParams();
        loadConstellationData();
    }
    
private:
    void setupParams();
    void loadConstellationData();
    void updateOutput();
    std::string findConstellationNarrative(const std::string& name);
    std::string normalizeConstellationName(const std::string& name);
    
    struct ConstellationData {
        std::string abbr;
        std::string latin;
        std::string spanish;
        std::string genitive;
        std::map<std::string, std::string> narratives;
    };
    
    ofParameter<string> constellationInput;
    ofParameter<string> descriptionOutput;
    ofParameter<int> currentLanguage;  // 0 for Catalan, 1 for English
    
    // Main data storage
    std::map<std::string, ConstellationData> constellationData;
    // Name-to-abbr mapping for quick lookups
    std::map<std::string, std::string> nameToAbbrMap;
    
    ofEventListeners listeners;
};
