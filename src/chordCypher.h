#ifndef CHORD_CYPHER_H
#define CHORD_CYPHER_H

#include "ofxOceanodeNodeModel.h"
#include <regex>
#include <unordered_map>

class chordCypher : public ofxOceanodeNodeModel {
public:
    chordCypher() : ofxOceanodeNodeModel("Chord Cypher"){
        setupChordMap();
    }
    
    void setup() {
            chordInput.set("Chord", "CM7");
            transpose.set("Transpose", 0, 0, 96);
            fold.set("Fold", true);
            rootOut.set("Root Out", 0, 0, 11);
            
            std::vector<int> initVec = {0};
            std::vector<int> minVec = {0};
            std::vector<int> maxVec = {130};
            output.set("Semitones", initVec, minVec, maxVec);
            
            addParameter(chordInput);
            addParameter(transpose);
            addParameter(fold);
            addParameter(output);
            addParameter(rootOut);
            
            listeners.push(chordInput.newListener([this](string &s){
                updateChord();
            }));
            
            listeners.push(transpose.newListener([this](int &i){
                updateChord();
            }));
            
            listeners.push(fold.newListener([this](bool &b){
                updateChord();
            }));
            
            loadChordDefinitions();
        }

private:
    ofParameter<string> chordInput;
    ofParameter<int> transpose;
    ofParameter<bool> fold;
    ofParameter<std::vector<int>> output;
    ofParameter<int> rootOut;
    ofEventListeners listeners;
    
    std::unordered_map<string, std::vector<int>> chordDefinitions;
    std::unordered_map<string, string> chordAliases;
    
    void setupChordMap() {
        chordAliases = {
                // Basic triads and qualities
                {"major", "M"},
                {"Major", "M"},
                {"maj", "M"},
                {"Maj", "M"},
                {"minor", "m"},
                {"Minor", "m"},
                {"min", "m"},
                {"Min", "m"},
                
                // Augmented variations (before sevenths)
                {"+5", "aug"},
                {"#5", "aug"},
                {"+", "aug"},
                
                // Diminished and half-diminished
                {"°", "dim"},
                {"o", "dim"},
                {"0", "dim"},
                {"ø", "m7b5"},
                {"φ", "m7b5"},
                {"Ø", "m7b5"},
                {"half-dim", "m7b5"},
                {"halfdim", "m7b5"},
                
                // Sevenths nomenclature
                {"dom", "7"},
                {"Dom", "7"},
                {"maj7", "M7"},
                {"Maj7", "M7"},
                {"major7", "M7"},
                {"min7", "m7"},
                {"Min7", "m7"},
                {"minor7", "m7"},
                
                // Special seventh combinations
                {"m/maj7", "mM7"},
                {"m/M7", "mM7"},
                {"m(maj7)", "mM7"},
                {"m(M7)", "mM7"},
                {"minmaj7", "mM7"},
                {"min/maj7", "mM7"},
                {"min(maj7)", "mM7"},
                {"/maj7", "M7"},
                {"(maj7)", "M7"},
                
                // Augmented sevenths
                {"+M7", "augM7"},
                {"M7+", "augM7"},
                {"M7+5", "augM7"},
                {"+m7", "m7aug5"},
                {"m7+", "m7aug5"},
                {"m7+5", "m7aug5"},
                {"+7", "aug7"},
                {"7+", "aug7"},
                {"7+5", "aug7"},
                {"7#5", "aug7"},
                {"7aug", "aug7"},
                
                // Diminished sevenths
                {"dim7°", "dim7"},
                {"°7", "dim7"},
                {"o7", "dim7"},
                {"07", "dim7"},
                {"ø7", "m7b5"},
                
                // Handle suspended chords
                {"6sus2", "6sus2"},
                {"6/2", "6sus2"},
                {"6sus", "6sus2"},
                {"7sus4", "7sus4"},
                {"7sus", "7sus4"},
                {"sus47", "7sus4"},
                {"7sus2", "7sus2"},
                {"sus", "sus4"},
                {"m7sus4", "m7sus4"},
                {"min7sus4", "m7sus4"},
                {"m7sus", "m7sus4"},
                {"min7sus", "m7sus4"},
                {"9sus4", "9sus4"},
                {"9sus", "9sus4"},
                {"dom9sus4", "9sus4"},
                {"dom9sus", "9sus4"},
            
                // b6 chords
                {"m7b6", "m7b6"},
                {"min7b6", "m7b6"},
                {"m7-6", "m7b6"},
                {"min7-6", "m7b6"},
                
                // Altered dominants
                {"7#5b9", "aug7b9"},
                {"7b9#5", "aug7b9"},
                {"7#5#9", "aug7#9"},
                {"7#9#5", "aug7#9"},
                {"aug7b9", "aug7b9"},
                {"aug7#9", "aug7#9"},
                
                // Super/subscript numbers
                {"⁷", "7"},
                {"⁹", "9"},
                {"¹¹", "11"},
                {"¹³", "13"}
            };
    }
    
    void loadChordDefinitions() {
        string path = ofToDataPath("Supercollider/Pitchclass/chords.txt");
        ofBuffer buffer = ofBufferFromFile(path);
        if(!buffer.size()) {
            ofLogError("ChordCypher") << "Could not load chord definitions file at path: " << path;
            output.set(std::vector<int>{0});
            return;
        }
        
        for(auto line : buffer.getLines()) {
            std::vector<string> tokens = ofSplitString(line, ",");
            if(tokens.size() < 2) continue;
            
            string chordName = ofTrim(tokens[1]);
            size_t spacePos = chordName.find(' ');
            if(spacePos != string::npos) {
                string name = chordName.substr(0, spacePos);
                string intervalsStr = chordName.substr(spacePos + 1);
                std::vector<string> intervalTokens = ofSplitString(intervalsStr, " ");
                std::vector<int> intervals;
                
                for(const auto& token : intervalTokens) {
                    if(!token.empty()) {
                        intervals.push_back(ofToInt(token));
                    }
                }
                
                if(!intervals.empty()) {
                    chordDefinitions[name] = intervals;
                }
            }
        }
    }
    
    void updateChord() {
                    string input = chordInput.get();
                    if(input.empty()) {
                        output.set(std::vector<int>{0});
                        rootOut.set(0);
                        return;
                    }
                    
                    // Handle substitutions vs extensions
                    std::regex subRegex("([A-Za-z][#b]?[^(]*)\\(([A-Za-z][#b]?.*?)\\)");
                    std::regex extRegex("([^(]*)\\(([^A-Za-z]*[#b0-9]*)\\)");
                    std::regex slashRegex("([^/\\\\]+)[/\\\\]([A-Za-z][#b]?)");
                    std::smatch matches;
                    
                    string processedInput;
                    string bassNote;
                    bool hasSlash = false;
                    
                    if(std::regex_search(input, matches, slashRegex)) {
                        processedInput = matches[1].str();
                        bassNote = matches[2].str();
                        if(!bassNote.empty()) {
                            bassNote[0] = toupper(bassNote[0]);
                            hasSlash = true;
                        }
                    }
                    else if(std::regex_search(input, matches, subRegex)) {
                        processedInput = matches[1].str();
                    }
                    else if(std::regex_search(input, matches, extRegex)) {
                        processedInput = matches[1].str() + matches[2].str();
                    }
                    else {
                        processedInput = input;
                    }
                    
                    if(!processedInput.empty()) {
                        processedInput[0] = toupper(processedInput[0]);
                    }
                    
                    // Modified regex to capture root and all modifiers
                    std::regex chordRegex("([A-G][#b]?)([^b#]*)([b#].*)?");
                    
                    if(std::regex_match(processedInput, matches, chordRegex)) {
                        string noteName = matches[1].str();
                        string baseChordType = matches[2].str();
                        string alterations = matches[3].str();
                        
                        // Try first with full chordType including alterations
                        string chordType = baseChordType;
                        if(!alterations.empty()) {
                            chordType += alterations;
                        }
                        chordType.erase(std::remove(chordType.begin(), chordType.end(), ' '), chordType.end());
                        
                        // Get the final chord type by comparing substrings
                        string finalChordType = "";
                        for(const auto& definition : chordDefinitions) {
                            if(chordType == definition.first) {
                                finalChordType = definition.first;
                                break;
                            }
                        }
                        
                        // If not found, try with aliases
                        if(finalChordType.empty()) {
                            for(const auto& alias : chordAliases) {
                                if(chordType == alias.first) {
                                    finalChordType = alias.second;
                                    break;
                                }
                            }
                        }
                        
                        // Set root out depending on slash presence
                        int baseRoot;
                        if(hasSlash && !bassNote.empty()) {
                            baseRoot = getNoteValue(bassNote);
                        } else {
                            baseRoot = getNoteValue(noteName);
                        }
                        rootOut.set((baseRoot + transpose.get()) % 12);
                        
                        // For intervals, still use the original note name
                        int intervalRoot = getNoteValue(noteName);
                        
                        std::vector<int> intervals;
                        if(!finalChordType.empty() && chordDefinitions.find(finalChordType) != chordDefinitions.end()) {
                            intervals = chordDefinitions[finalChordType];
                        } else {
                            // If no match found, try without alterations
                            if(baseChordType.empty()) {
                                intervals = chordDefinitions["M"];
                            } else {
                                ofLogWarning("ChordCypher") << "Unknown chord type: " << chordType;
                                output.set(std::vector<int>{baseRoot + transpose});
                                return;
                            }
                        }
                        
                        std::vector<int> result;
                        for(int interval : intervals) {
                            int note = interval + intervalRoot;
                            
                            if(fold) {
                                note = note % 12;
                            }
                            
                            result.push_back(note);
                        }
                        
                        if(!bassNote.empty()) {
                            int bass = getNoteValue(bassNote);
                            if(fold) {
                                bass = bass % 12;
                            }
                            result.insert(result.begin(), bass);
                        }
                        
                        if(fold) {
                            std::sort(result.begin(), result.end());
                            result.erase(std::unique(result.begin(), result.end()), result.end());
                        }
                        
                        for(int& note : result) {
                            note += transpose;
                        }
                        
                        if(result.empty()) {
                            result.push_back(baseRoot + transpose);
                        }
                        
                        output.set(result);
                    } else {
                        output.set(std::vector<int>{0});
                        rootOut.set(0);
                    }
                }
    
    int getNoteValue(const string& note) {
        static const std::unordered_map<string, int> noteValues = {
            {"C", 0}, {"C#", 1}, {"Db", 1},
            {"D", 2}, {"D#", 3}, {"Eb", 3},
            {"E", 4}, {"F", 5}, {"F#", 6},
            {"Gb", 6}, {"G", 7}, {"G#", 8},
            {"Ab", 8}, {"A", 9}, {"A#", 10},
            {"Bb", 10}, {"B", 11}
        };
        
        try {
                auto it = noteValues.find(note);
                return (it != noteValues.end()) ? it->second : 0;
            } catch(...) {
                return 0;
            }
    }
};

#endif /* CHORD_CYPHER_H */
