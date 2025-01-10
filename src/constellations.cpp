#include "constellations.h"
#include <fstream>
#include <algorithm>

void constellations::setupParams() {
    // Set up parameters with proper types and defaults
    constellationInput.set("Constell", "");
    descriptionOutput.set("Output", "");
    currentLanguage.set("Lang", 0, 0, 1);  // 0 for Catalan, 1 for English
    
    // Add parameters to the node
    addParameter(constellationInput);
    addParameter(descriptionOutput);
    addParameter(currentLanguage);
    
    // Set up listeners
    listeners.push(constellationInput.newListener([this](string &){
        updateOutput();
    }));
    
    listeners.push(currentLanguage.newListener([this](int &){
        updateOutput();
    }));
}

void constellations::loadConstellationData() {
    // Load constellation narratives
    std::string jsonPath = ofToDataPath("catalog/narratives/constellatio.json");
    ofJson json = ofLoadJson(jsonPath);
    
    if(json.empty()) {
        ofLogError("constellations") << "Cannot open/parse constellation data at: " << jsonPath;
        return;
    }
    
    // Process each constellation
    for(auto& [abbr, constData] : json["constellations"].items()) {
        ConstellationData data;
        
        // Get all names
        if(constData.contains("names")) {
            data.abbr = constData["names"]["abbr"].get<std::string>();
            data.latin = constData["names"]["latin"].get<std::string>();
            data.spanish = constData["names"]["spanish"].get<std::string>();
            data.genitive = constData["names"]["genitive"].get<std::string>();
        }
        
        // Get narratives
        if(constData.contains("narratives")) {
            if(constData["narratives"].contains("ca")) {
                data.narratives["ca"] = constData["narratives"]["ca"].get<std::string>();
            }
            if(constData["narratives"].contains("en")) {
                data.narratives["en"] = constData["narratives"]["en"].get<std::string>();
            }
        }
        
        // Store main data
        constellationData[abbr] = data;
        
        // Create lookup mappings for all name variants
        nameToAbbrMap[normalizeConstellationName(data.abbr)] = abbr;
        nameToAbbrMap[normalizeConstellationName(data.latin)] = abbr;
        nameToAbbrMap[normalizeConstellationName(data.spanish)] = abbr;
        nameToAbbrMap[normalizeConstellationName(data.genitive)] = abbr;
    }
    
    ofLogNotice("constellations") << "Loaded " << constellationData.size()
                                 << " constellation narratives with "
                                 << nameToAbbrMap.size() << " name mappings";
}

std::string constellations::normalizeConstellationName(const std::string& name) {
    std::string normalized = name;
    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    return normalized;
}

std::string constellations::findConstellationNarrative(const std::string& name) {
    if(name.empty()) return "";
    
    std::string normalizedName = normalizeConstellationName(name);
    
    // Get current language code based on parameter
    std::string langCode = (currentLanguage.get() == 0) ? "ca" : "en";
    
    // Look up the abbreviation for this name
    auto nameIt = nameToAbbrMap.find(normalizedName);
    if(nameIt != nameToAbbrMap.end()) {
        // Use the abbreviation to find the full constellation data
        auto constIt = constellationData.find(nameIt->second);
        if(constIt != constellationData.end()) {
            // Try to get narrative in selected language
            auto narrative = constIt->second.narratives.find(langCode);
            if(narrative != constIt->second.narratives.end()) {
                return narrative->second;
            }
            
            // If English requested but not available, fall back to Catalan
            if(langCode == "en") {
                auto catalanNarrative = constIt->second.narratives.find("ca");
                if(catalanNarrative != constIt->second.narratives.end()) {
                    return catalanNarrative->second + "\n[Translation not available]";
                }
            }
        }
    }
    
    return "Constellation narrative not found.";
}

void constellations::updateOutput() {
    std::string input = constellationInput.get();
    std::string narrative = findConstellationNarrative(input);
    descriptionOutput.set(narrative);
}
