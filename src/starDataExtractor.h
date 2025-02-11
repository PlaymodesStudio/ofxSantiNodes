#pragma once

#include "ofxOceanodeNodeModel.h"
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include <regex>


class starDataExtractor : public ofxOceanodeNodeModel {
public:
    starDataExtractor() : ofxOceanodeNodeModel("Star Data Extractor"){
        setupParams();
        loadCatalogs();
    }
    
    void setupParams(){
        hipInput.set("HIP", 0, 0, 200000);
        hdInput.set("HD", 0, 0, 1000000);
        parallax.set("Parallax", 0.0f, -100.0f, 100.0f);
        magnitude.set("Magnitude", 0.0f, -27.0f, 20.0f);
        sunX.set("Sun Mass X", 1.0f, 0.1f, 150.0f);
        bvColor.set("Color", 0.0f, -1.0f, 3.0f);
        spectralType.set("SpecType", "");
        multipleCount.set("Multiple", 1, 1, 10);
        starName.set("Name", "");
        constellation.set("Constel", "");
        continuousMode.set("Continuous", true);
                
        addParameter(hipInput);
        addParameter(hdInput);
        addParameter(parallax);
        addParameter(magnitude);
        addParameter(bvColor);
        addParameter(spectralType);
        addParameter(multipleCount);
        addParameter(starName);
        addParameter(constellation);
        addParameter(sunX);
        addParameter(continuousMode);
        
        lastUpdateTime = 0;
        
        listeners.push(hipInput.newListener([this](int &){
					if(hipInput>0)
					{
						updateDataFromHIP();
					}
                }));
        
        listeners.push(hdInput.newListener([this](int &){
            updateData();
        }));
    }
    
    void update(ofEventArgs &e) override {
            if(continuousMode) {
                uint64_t currentTime = ofGetElapsedTimeMillis();
                if(currentTime - lastUpdateTime > 16) {
                    if(hipInput > 0) {
                        updateDataFromHIP();
                    } else {
                        updateData();
                    }
                    lastUpdateTime = currentTime;
                }
            }
        }
    
private:
    void loadCatalogs() {
           loadConstellationNames();  // Load this first
           loadBSC5P();
           loadIAUNames();
        loadHIPtoHDMapping();

       }
    
    void loadHIPtoHDMapping() {
        std::string appDataPath = ofToDataPath("catalog/catalogIV27A.dat");
        std::ifstream file(appDataPath);
        if(!file.is_open()) {
            ofLogError("starDataExtractor") << "Cannot open catalog IV27A at: " << appDataPath;
            return;
        }
        
        std::string line;
        while(std::getline(file, line)) {
            try {
                // Skip empty lines
                if(line.length() < 10) continue;
                
                // Split line into fields based on whitespace
                std::vector<std::string> fields;
                std::istringstream iss(line);
                std::string field;
                while(iss >> field) {
                    fields.push_back(field);
                }
                
                // Need at least 6 fields
                if(fields.size() < 6) continue;
                
                // First field is HD number
                int hdNum = std::stoi(fields[0]);
                
                // Sixth field is HIP number
                int hipNum = std::stoi(fields[5]);
                
                hipToHdMap[hipNum] = hdNum;
                //ofLogVerbose("starDataExtractor") << "Mapped HIP" << hipNum << " to HD" << hdNum;
                
            } catch(const std::exception& e) {
                //ofLogWarning("starDataExtractor") << "Failed to process IV27A catalog line: " << e.what();
                continue;
            }
        }
        
        ofLogNotice("starDataExtractor") << "Loaded " << hipToHdMap.size() << " HIP to HD mappings";
    }
    
    void updateDataFromHIP() {
            int hipNum = hipInput;
            //ofLogNotice("starDataExtractor") << "\nLooking up HIP" << hipNum;
            
            // Find corresponding HD number
            auto hdIt = hipToHdMap.find(hipNum);
            if(hdIt != hipToHdMap.end()) {
                int hdNum = hdIt->second;
                //ofLogNotice("starDataExtractor") << "Found corresponding HD" << hdNum;
                hdInput.set(hdNum);  // This will trigger updateData() through the listener
            } else {
                //ofLogNotice("starDataExtractor") << "No HD number found for HIP" << hipNum;
                // Clear all fields if no mapping found
                hdInput.set(0);
                parallax.set(0);
                magnitude.set(0);
                bvColor.set(0);
                spectralType.set("");
                multipleCount.set(1);
                starName.set("Unknown");
                constellation.set("");
                sunX.set(1.0f);
            }
        }
    
    void loadIAUNames() {
        std::string appDataPath = ofToDataPath("catalog/IAU-CSN.json");
        ofJson json = ofLoadJson(appDataPath);
        if(json.empty()) {
            ofLogError("starDataExtractor") << "Cannot open/parse IAU-CSN catalog at: " << appDataPath;
            return;
        }
        
        //ofLogNotice("starDataExtractor") << "Loading IAU-CSN catalog...";
        
        for(auto& star : json) {
            try {
                // Check required fields exist and are strings
                if(star.contains("Name/ASCII") && star.contains("Designation") &&
                   star.contains("HD") && star.contains("Con")) {
                    
                    std::string properName = star["Name/ASCII"].get<std::string>();
                    std::string designation = star["Designation"].get<std::string>();
                    std::string constAbbr = star["Con"].get<std::string>();
                    std::string hdString = star["HD"].get<std::string>();
                    
                    // Get full constellation name
                    std::string fullConstName = constAbbr;  // Default to abbreviation
                    if(constellationNames.find(constAbbr) != constellationNames.end()) {
                        fullConstName = constellationNames[constAbbr];
                    }
                    
                    // Debug output
                    //ofLogNotice("starDataExtractor") << "Processing star: " << properName
                     //   << " (HD" << hdString << ", " << designation << ", " << fullConstName << ")";
                    
                    // Extract HR number from designation
                    if(designation.find("HR") != std::string::npos) {
                        size_t numStart = designation.find_first_of("0123456789");
                        if(numStart != std::string::npos) {
                            std::string numStr = "";
                            for(size_t i = numStart; i < designation.length(); i++) {
                                if(isdigit(designation[i])) {
                                    numStr += designation[i];
                                }
                            }
                            
                            int hr = std::stoi(numStr);
                            
                            // Create the name data
                            StarNameData nameData;
                            nameData.properName = properName;
                            nameData.constellation = fullConstName;
                            iauNameData[hr] = nameData;
                            
                            // If HD is present and valid, create mapping
                            if(hdString != "_" && !hdString.empty()) {
                                try {
                                    int hdNum = std::stoi(hdString);
                                    hdToHrMap[hdNum] = hr;
                                    //ofLogNotice("starDataExtractor") << "Mapped HD" << hdNum
                                      //  << " to HR" << hr;
                                } catch(...) {}
                            }
                            
                            //ofLogNotice("starDataExtractor") << "Successfully loaded: HR" << hr
                              //  << " -> " << nameData.properName << " (" << nameData.constellation << ")";
                        }
                    }
                }
            }
            catch(const std::exception& e) {
                ofLogWarning("starDataExtractor") << "Failed to process entry: " << e.what();
                continue;
            }
        }
        
        //ofLogNotice("starDataExtractor") << "IAU-CSN catalog loaded: " << iauNameData.size()
          //  << " entries, " << hdToHrMap.size() << " HD-HR mappings";
    }
    
    void loadBSC5P() {
        std::string appDataPath = ofToDataPath("catalog/bsc5p.json");
        ofJson json = ofLoadJson(appDataPath);
        if(json.empty()) {
            ofLogError("starDataExtractor") << "Cannot open/parse BSC5P catalog at: " << appDataPath;
            return;
        }

        //ofLogNotice("starDataExtractor") << "Loading BSC5P catalog...";
        int count = 0;

        for(auto& star : json) {
            if(!star["hdId"].empty()) {
                try {
                    int hdId = std::stoi(star["hdId"].get<std::string>());
                    StarData data;
                    
                    // Parse parallax
                    if(!star["trigParallax"].empty()) {
                        std::string parStr = star["trigParallax"].get<std::string>();
                        if(parStr[0] == '+') parStr = parStr.substr(1);
                        if(!parStr.empty()) {
                            try {
                                data.parallax = std::stof(parStr);
                            } catch(...) {
                                data.parallax = 0;
                            }
                        }
                    }
                    
                    // Parse magnitude
                    if(!star["visualMagnitude"].empty()) {
                        std::string magStr = star["visualMagnitude"].get<std::string>();
                        if(!magStr.empty()) {
                            try {
                                data.magnitude = std::stof(magStr);
                            } catch(...) {
                                data.magnitude = 0;
                            }
                        }
                    }
                    
                    // Fixed B-V color parsing
                    if(!star["bvColorUbv"].empty() && star["bvColorUbv"].is_string()) {
                        std::string colorStr = star["bvColorUbv"].get<std::string>();
                        if(!colorStr.empty()) {
                            try {
                                // Handle both positive and negative values
                                if(colorStr[0] == '+') {
                                    colorStr = colorStr.substr(1);
                                }
                                data.bvColor = std::stof(colorStr);
                                //ofLogVerbose("starDataExtractor") << "Parsed B-V color for HD" << hdId
                                 //                               << ": " << colorStr << " -> " << data.bvColor;
                            } catch(const std::exception& e) {
                                ofLogWarning("starDataExtractor") << "Failed to parse B-V color for HD"
                                                                << hdId << ": " << colorStr;
                                data.bvColor = 0;
                            }
                        }
                    }
                    
                    if(!star["spectralType"].empty())
                        data.spectralType = star["spectralType"].get<std::string>();
                    
                    if(!star["componentsAssignedToMultiple"].empty()) {
                       std::string compStr = star["componentsAssignedToMultiple"].get<std::string>();
                       if(!compStr.empty()) {
                           try {
                               data.multipleCount = std::stoi(compStr);
                           } catch(...) {
                               data.multipleCount = compStr.empty() ? 1 : 2;
                           }
                       }
                    }

                    // Store HR number if present
                    bool foundHR = false;
                    if(!star["hrId"].empty()) {
                        try {
                            std::string hrStr = star["hrId"].get<std::string>();
                            data.hrNumber = std::stoi(hrStr);
                            foundHR = true;
                        } catch(...) {}
                    }
                    if(!foundHR && !star["HR"].empty()) {
                        try {
                            std::string hrStr = star["HR"].get<std::string>();
                            data.hrNumber = std::stoi(hrStr);
                            foundHR = true;
                        } catch(...) {}
                    }
                    if(!foundHR && !star["hr"].empty()) {
                        try {
                            std::string hrStr = star["hr"].get<std::string>();
                            data.hrNumber = std::stoi(hrStr);
                            foundHR = true;
                        } catch(...) {}
                    }
                    if(!foundHR && !star["saoId"].empty()) {
                        // Some catalogs use SAO numbers for cross-reference
                        //ofLogNotice("starDataExtractor") << "Found SAO ID for HD" << hdId << ": " << star["saoId"].get<std::string>();
                    }

                    if(foundHR) {
                        //ofLogNotice("starDataExtractor") << "BSC5P: HD" << hdId << " -> HR" << data.hrNumber;
                    } else {
                        data.hrNumber = 0;
                        //ofLogNotice("starDataExtractor") << "No HR number found for HD" << hdId;
                    }
                    
                    // Store the data regardless of whether we found an HR number
                    bsc5pData[hdId] = data;
                    count++;
                    
                } catch(const std::exception& e) {
                    ofLogWarning("starDataExtractor") << "Failed to process catalog entry: " << e.what();

                    continue;
                }
            }
        }

        //ofLogNotice("starDataExtractor") << "BSC5P catalog loaded: " << count << " entries";
    }
    
    void loadConstellationNames() {
            std::string appDataPath = ofToDataPath("catalog/constellation_mapping.txt");
            std::ifstream file(appDataPath);
            if(!file.is_open()) {
                ofLogError("starDataExtractor") << "Cannot open constellation mapping at: " << appDataPath;
                return;
            }
            
            std::string line;
            while(std::getline(file, line)) {
                size_t pos = line.find("|");
                if(pos != std::string::npos) {
                    std::string abbr = line.substr(0, pos);
                    std::string fullName = line.substr(pos + 1);
                    constellationNames[abbr] = fullName;
                    //ofLogVerbose("starDataExtractor") << "Loaded constellation: " << abbr << " -> " << fullName;
                }
            }
            
            //ofLogNotice("starDataExtractor") << "Loaded " << constellationNames.size() << " constellation names";
        }
    
    float inferMass(const std::string &spectralType, float bvColor) {
        if (spectralType.empty()) return 1.0f; // Valor per defecte
        
        char mainClass = spectralType[0];
        bool isSuperGiant = spectralType.find("I") != std::string::npos;
        bool isGiant = spectralType.find("III") != std::string::npos;
        bool isDwarf = spectralType.find("V") != std::string::npos;

        // Cas especial per supergegants (classe I)
        if (isSuperGiant) {
            switch (mainClass) {
                case 'O': return 40.0f; // Supergegants O: 20-60 masses solars
                case 'B': return 25.0f; // Supergegants B: ~15-25 masses solars
                case 'A': return 15.0f; // Supergegants A: ~10-15 masses solars (ex.: Deneb)
                case 'F': return 10.0f; // Supergegants F: ~8-12 masses solars
                case 'G': return 8.0f;  // Supergegants G: ~5-8 masses solars
                case 'K': return 6.0f;  // Supergegants K: ~3-6 masses solars
                case 'M': return 3.0f;  // Supergegants M: ~1-3 masses solars
                default: return 1.0f;   // Cas desconegut, valor per defecte
            }
        }

        // Casos per nanes (classe V) i gegants normals (classe III)
        if (isDwarf || isGiant) {
            switch (mainClass) {
                case 'O': return 16.0f;
                case 'B': return 5.0f;
                case 'A': return 2.5f;
                case 'F': return 1.5f;
                case 'G': return 1.0f; // Similar al Sol
                case 'K': return 0.8f;
                case 'M': return 0.2f;
                default: return 1.0f;
            }
        }

        // Refinament amb B-V per estrelles de la seqüència principal
        if (bvColor > -0.3 && bvColor <= 0.0) return 2.5f;  // Estrelles calentes
        if (bvColor > 0.0 && bvColor <= 0.65) return 1.0f;  // Estrelles com el Sol
        if (bvColor > 0.65) return 0.5f;                    // Estrelles fredes

        return 1.0f; // Valor per defecte
    }


    
    void updateData() {
        int hdNum = hdInput;
        //ofLogNotice("starDataExtractor") << "\nLooking up HD" << hdNum;

        if (bsc5pData.find(hdNum) != bsc5pData.end()) {
            StarData &data = bsc5pData[hdNum];
            parallax.set(data.parallax);
            magnitude.set(data.magnitude);
            bvColor.set(data.bvColor);
            spectralType.set(data.spectralType);
            multipleCount.set(data.multipleCount);

            // Infer mass and calculate sunX
            float inferredMass = inferMass(data.spectralType, data.bvColor);
            sunX.set(inferredMass);

            // Try to get HR number from HD-HR map
            if (hdToHrMap.find(hdNum) != hdToHrMap.end()) {
                int hr = hdToHrMap[hdNum];
               // ofLogNotice("starDataExtractor") << "Found HR" << hr << " for HD" << hdNum;

                if (iauNameData.find(hr) != iauNameData.end()) {
                    StarNameData &nameData = iauNameData[hr];
                    //ofLogNotice("starDataExtractor") << "Found IAU data: " << nameData.properName
                       //                              << " in " << nameData.constellation;
                    starName.set(nameData.properName);
                    constellation.set(nameData.constellation);
                    return;
                }
            }

            starName.set("HD " + ofToString(hdNum));
            constellation.set("");
        } else {
            //ofLogNotice("starDataExtractor") << "No BSC5P data for HD" << hdNum;
            parallax.set(0);
            magnitude.set(0);
            bvColor.set(0);
            spectralType.set("");
            multipleCount.set(1);
            starName.set("Unknown");
            constellation.set("");
            sunX.set(1.0f); // Default to Sun's mass for unknown stars
        }
    }
    
    struct StarData {
        float parallax = 0;
        float magnitude = 0;
        float bvColor = 0;
        std::string spectralType;
        int multipleCount = 1;
        int hrNumber = 0;  // Added to store HR number for IAU lookup
    };
    
    struct StarNameData {
        std::string properName;
        std::string constellation;
    };
    
    ofParameter<int> hipInput;
    ofParameter<int> hdInput;
    ofParameter<float> parallax;
    ofParameter<float> magnitude;
    ofParameter<float> bvColor;
    ofParameter<string> spectralType;
    ofParameter<int> multipleCount;
    ofParameter<string> starName;
    ofParameter<string> constellation;
    ofParameter<float> sunX;
    ofParameter<bool> continuousMode;
    
    uint64_t lastUpdateTime;
    
    std::map<int, int> hipToHdMap;
    std::map<int, StarData> bsc5pData;
    std::map<int, StarNameData> iauNameData;
    std::map<int, int> hdToHrMap;
    std::map<std::string, std::string> constellationNames;
    ofEventListeners listeners;
};
