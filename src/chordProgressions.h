#ifndef CHORD_PROGRESSIONS_H
#define CHORD_PROGRESSIONS_H

#include "ofxOceanodeNodeModel.h"
#include "ofJson.h"

class chordProgressions : public ofxOceanodeNodeModel {
public:
    chordProgressions() : ofxOceanodeNodeModel("Chord Progressions") {}
    
    void setup() {
        // Initialize parameters
        name.set("Name", "");
        input.set("Input", "");
        timeIn.set("TimeIn", "");
        
        // Initialize default vectors with bounds
        std::vector<float> initTimings = {0.0f};
        std::vector<float> minTimings = {0.0f};
        std::vector<float> maxTimings = {4.0f};
        timeOut.set("Time Out", initTimings, minTimings, maxTimings);
        
        output.set("Output", "");
        selectedIdx.set("Index", 0, 0, 100);
        idxOut.set("idx out", 0, 0, 100);
        
        // Load progressions first to setup dropdown
        loadProgressions();
        if(progressionNames.empty()) {
            progressionNames.push_back("No progressions");
        }
        
        // Setup dropdown
        selectedProgression.set("Progression", 0, 0, progressionNames.size()-1);
        auto progParam = addParameterDropdown(selectedProgression, "Progression", 0, progressionNames);
        
        // Add parameters
        addParameter(name);
        addParameter(input);
        addParameter(timeIn);
        addParameter(selectedIdx);
        addParameter(output);
        addParameter(timeOut);
        addParameter(idxOut);
        
        // Add button parameters
        addParameter(add.set("Add"));
        addParameter(replace.set("Replace"));
        addParameter(remove.set("Delete"));
        
        // Setup listeners
        listeners.push(selectedProgression.newListener([this](int &i){
            loadSelectedProgression();
        }));
        
        listeners.push(selectedIdx.newListener([this](int &i){
            selectProgressionByIndex(i);
        }));
        
        listeners.push(add.newListener([this](void){
            addProgression();
        }));
        
        listeners.push(replace.newListener([this](void){
            replaceProgression();
        }));
        
        listeners.push(remove.newListener([this](void){
            deleteProgression();
        }));
    }
    
private:
    ofParameter<string> name;
    ofParameter<string> input;
    ofParameter<string> timeIn;
    ofParameter<string> output;
    ofParameter<std::vector<float>> timeOut;
    ofParameter<int> selectedProgression;
    ofParameter<int> selectedIdx;
    ofParameter<int> idxOut;
    ofParameter<void> add;
    ofParameter<void> replace;
    ofParameter<void> remove;
    
    ofEventListeners listeners;
    std::vector<string> progressionNames;
    ofJson database;
    
    void loadProgressions() {
        string path = ofToDataPath("Supercollider/Pitchclass/chord_progressions.json");
        ofFile file(path);
        
        if(file.exists()) {
            database = ofLoadJson(path);
            updateProgressionList();
        } else {
            // Create empty database
            database["progressions"] = ofJson::object();
            saveDatabase();
        }
    }
    
    void saveDatabase() {
        string path = ofToDataPath("Supercollider/Pitchclass/chord_progressions.json");
        ofFile file(path, ofFile::WriteOnly);
        if(file.is_open()) {
            string jsonStr = database.dump(4);
            file.write(jsonStr.c_str(), jsonStr.length());
            file.close();
        } else {
            ofLogError("ChordProgressions") << "Could not open file for writing at: " << path;
        }
        updateProgressionList();
    }
    
    void updateProgressionList() {
        progressionNames.clear();
        
        for(auto& [key, value] : database["progressions"].items()) {
            progressionNames.push_back(value["name"]);
        }
        
        if(progressionNames.empty()) {
            progressionNames.push_back("No progressions");
        }
        
        // Update dropdown bounds
        selectedProgression.setMax(progressionNames.size()-1);
    }
    
    int getNextIndex() {
        int maxIndex = -1;
        for(auto& [key, value] : database["progressions"].items()) {
            maxIndex = std::max(maxIndex, std::stoi(key));
        }
        return maxIndex + 1;
    }
    
    void loadSelectedProgression() {
        if(database["progressions"].empty()) return;
        
        int index = selectedProgression;
        if(index < 0 || index >= progressionNames.size()) return;
        
        // Find the progression with this index in order
        int currentIndex = 0;
        for(auto& [key, value] : database["progressions"].items()) {
            if(currentIndex == index) {
                output.set(value["chords"]);
                
                // Parse timing string to vector
                string timingStr = value["timing"];
                std::vector<string> timings = ofSplitString(timingStr, ",");
                std::vector<float> timeValues;
                
                for(const auto& t : timings) {
                    timeValues.push_back(ofToFloat(ofTrim(t)));
                }
                
                timeOut.set(timeValues);
                idxOut.set(std::stoi(key));
                return;
            }
            currentIndex++;
        }
    }
    
    void selectProgressionByIndex(int idx) {
        if(database["progressions"].empty()) return;
        
        // Find progression with this index and select it in dropdown
        int dropdownIndex = 0;
        for(auto& [key, value] : database["progressions"].items()) {
            if(std::stoi(key) == idx) {
                selectedProgression.set(dropdownIndex);
                return;
            }
            dropdownIndex++;
        }
    }
    
    string getDefaultTiming(const string& chordStr) {
        int numChords = 1;
        for(char c : chordStr) {
            if(c == ',') numChords++;
        }
        std::vector<string> times(numChords, "1");
        return ofJoinString(times, ", ");
    }
    
    void addProgression() {
        if(input.get().empty()) {
            ofLogWarning("ChordProgressions") << "Input chords required to add progression";
            return;
        }
        
        int newIndex = getNextIndex();
        string nameStr = name.get().empty() ? "user" : name.get();
        string timingStr = timeIn.get().empty() ? getDefaultTiming(input.get()) : timeIn.get();
        
        database["progressions"][std::to_string(newIndex)] = {
            {"name", nameStr},
            {"chords", input.get()},
            {"timing", timingStr}
        };
        
        saveDatabase();
        selectedProgression.set(progressionNames.size()-1);
    }
    
    void replaceProgression() {
        if(database["progressions"].empty()) return;
        
        if(input.get().empty()) {
            ofLogWarning("ChordProgressions") << "Input chords required to replace progression";
            return;
        }
        
        // Get current index
        int currentIndex = idxOut.get();
        string nameStr = name.get().empty() ? "user" : name.get();
        string timingStr = timeIn.get().empty() ? getDefaultTiming(input.get()) : timeIn.get();
        
        database["progressions"][std::to_string(currentIndex)] = {
            {"name", nameStr},
            {"chords", input.get()},
            {"timing", timingStr}
        };
        
        saveDatabase();
        loadSelectedProgression();
    }
    
    void deleteProgression() {
        if(database["progressions"].empty()) return;
        
        // Get current index
        int currentIndex = idxOut.get();
        
        // Remove the progression
        database["progressions"].erase(std::to_string(currentIndex));
        
        saveDatabase();
        
        // Select first progression if any remain
        if(!progressionNames.empty()) {
            selectedProgression.set(0);
        }
    }
};

#endif /* CHORD_PROGRESSIONS_H */
