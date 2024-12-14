#ifndef JAZZ_STANDARDS_H
#define JAZZ_STANDARDS_H

#include "ofxOceanodeNodeModel.h"
#include "ofJson.h"

class jazzStandards : public ofxOceanodeNodeModel {
public:
    jazzStandards() : ofxOceanodeNodeModel("Jazz Standards") {}
    
    void setup() {
            // Initialize default vectors with bounds
            std::vector<float> initTimings = {0.0f};
            std::vector<float> minTimings = {0.0f};
            std::vector<float> maxTimings = {4.0f};
            
            // Load database first to setup dropdown
            loadSongDatabase();
            if(songTitles.empty()) {
                songTitles.push_back("No songs loaded");
            }
            
            // Top parameters
            selectedSong.set("Song", 0, 0, songTitles.size()-1);
            auto songParam = addParameterDropdown(selectedSong, "Song", 0, songTitles);
            timeSignature.set("Signature", "");
            composer.set("Composer", "");
            
            addParameter(timeSignature);
            addParameter(composer);
            
            // Section A parameters
            sections[0].chords.set("A Chords", "");
            sections[0].timings.set("A Time", initTimings, minTimings, maxTimings);
            sections[0].repeats.set("A Repeat", 0, 0, 16);
            
            // Section B parameters
            sections[1].chords.set("B Chords", "");
            sections[1].timings.set("B Time", initTimings, minTimings, maxTimings);
            sections[1].repeats.set("B Repeat", 0, 0, 16);
            
            // Section endings
            endings[0].chords.set("A End", "");
            endings[0].timings.set("A End Time", initTimings, minTimings, maxTimings);
            endings[1].chords.set("B End", "");
            endings[1].timings.set("B End Time", initTimings, minTimings, maxTimings);
            
            // Add all parameters in desired order
            addParameter(sections[0].chords);
            addParameter(sections[0].timings);
            addParameter(sections[0].repeats);
            addParameter(endings[0].chords);
            addParameter(endings[0].timings);
            
            addParameter(sections[1].chords);
            addParameter(sections[1].timings);
            addParameter(sections[1].repeats);
            addParameter(endings[1].chords);
            addParameter(endings[1].timings);
            
            listeners.push(selectedSong.newListener([this](int &i){
                updateSong();
            }));
        }
    
private:
    struct Section {
            ofParameter<string> chords;
            ofParameter<std::vector<float>> timings;
            ofParameter<int> repeats;
        };
        
        struct Ending {
            ofParameter<string> chords;
            ofParameter<std::vector<float>> timings;
        };
        
        Section sections[2];
        Ending endings[2];
        
        ofParameter<string> timeSignature;
        ofParameter<string> composer;
        ofParameter<int> selectedSong;
        
        ofEventListeners listeners;
        std::vector<string> songTitles;
        ofJson songDatabase;
    
    void loadSongDatabase() {
        string path = ofToDataPath("Supercollider/Pitchclass/JazzStandards.json");
        ofFile file(path);
        
        if(file.exists()) {
            songDatabase = ofLoadJson(path);
            for(auto & song : songDatabase) {
                songTitles.push_back(song["Title"].get<string>());
            }
        } else {
            ofLogError("JazzStandards") << "Could not load database at: " << path;
        }
    }
    
    float getBarLength(const string& ts) {
        // Convert time signature to bar length in 4/4 terms
        std::vector<string> parts = ofSplitString(ts, "/");
        if(parts.size() != 2) return 1.0;
        
        float numerator = ofToFloat(parts[0]);
        float denominator = ofToFloat(parts[1]);
        
        return (numerator / denominator) * 4.0 / 4.0;
    }
    
    std::pair<std::vector<string>, std::vector<float>> parseChordString(const string& chordStr, float barLength) {
        std::vector<string> chords;
        std::vector<float> timings;
        
        // Split into bars
        std::vector<string> bars = ofSplitString(chordStr, "|");
        
        for(const auto& bar : bars) {
            if(bar.empty()) continue;
            
            // Split chords within bar
            std::vector<string> barChords = ofSplitString(bar, ",");
            float chordLength = barLength / barChords.size();
            
            for(const auto& chord : barChords) {
                if(!chord.empty()) {
                    chords.push_back(ofTrim(chord));
                    timings.push_back(chordLength);
                }
            }
        }
        
        return {chords, timings};
    }
    
    void updateSong() {
            if(selectedSong < 0 || selectedSong >= songDatabase.size()) return;
            
            const auto& song = songDatabase[selectedSong];
            
            // Clear all outputs first
            for(int i = 0; i < 2; i++) {
                sections[i].chords.set("");
                sections[i].timings.set(std::vector<float>{0});
                sections[i].repeats.set(0);
                
                endings[i].chords.set("");
                endings[i].timings.set(std::vector<float>{0});
            }
            
            // Set basic info
            timeSignature.set(song["TimeSignature"].get<string>());
            composer.set(song["Composer"].get<string>());
            
            float barLength = getBarLength(song["TimeSignature"].get<string>());
            
            // Process sections
            if(song.contains("Sections")) {
                const auto& jsonSections = song["Sections"];
                for(size_t i = 0; i < std::min(size_t(2), jsonSections.size()); i++) {
                    const auto& section = jsonSections[i];
                    
                    // Process main segment
                    if(section.contains("MainSegment")) {
                        auto [chords, timings] = parseChordString(section["MainSegment"]["Chords"], barLength);
                        
                        string chordStr = "";
                        for(size_t j = 0; j < chords.size(); j++) {
                            if(j > 0) chordStr += ",";
                            chordStr += chords[j];
                        }
                        
                        sections[i].chords.set(chordStr);
                        sections[i].timings.set(timings);
                    }
                    
                    // Set repeats if present
                    if(section.contains("Repeats")) {
                        sections[i].repeats.set(section["Repeats"].get<int>());
                    }
                    
                    // Process endings if present
                    if(section.contains("Endings") && i < 2) {
                        const auto& jsonEndings = section["Endings"];
                        for(size_t j = 0; j < std::min(size_t(1), jsonEndings.size()); j++) {
                            auto [chords, timings] = parseChordString(jsonEndings[j]["Chords"], barLength);
                            
                            string chordStr = "";
                            for(size_t k = 0; k < chords.size(); k++) {
                                if(k > 0) chordStr += ",";
                                chordStr += chords[k];
                            }
                            
                            endings[i].chords.set(chordStr);
                            endings[i].timings.set(timings);
                        }
                    }
                }
            }
        }
};

#endif /* JAZZ_STANDARDS_H */
