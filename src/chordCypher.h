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
				
				// NEW - Major 7 with alterations
				{"maj7#11", "M7#11"},
				{"Maj7#11", "M7#11"},
				{"M7#11", "M7#11"},
				{"maj7+11", "M7#11"},
				{"Maj7+11", "M7#11"},
				{"M7+11", "M7#11"},
				{"maj7#4", "M7#11"},
				{"Maj7#4", "M7#11"},
				{"M7#4", "M7#11"},
				
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
				
				// NEW - Minor-Major combinations with extensions
				{"mM9", "mM9"},
				{"mMaj9", "mM9"},
				{"minMaj9", "mM9"},
				{"m/M9", "mM9"},
				{"m/maj9", "mM9"},
				{"mM11", "mM11"},
				{"mMaj11", "mM11"},
				{"minMaj11", "mM11"},
				{"m/M11", "mM11"},
				{"m/maj11", "mM11"},
				{"mM13", "mM13"},
				{"mMaj13", "mM13"},
				{"minMaj13", "mM13"},
				{"m/M13", "mM13"},
				{"m/maj13", "mM13"},
				
				// Augmented sevenths
				{"+M7", "augM7"},
				{"M7+", "augM7"},
				{"M7+5", "augM7"},
				{"M7#5", "augM7"}, // NEW - Alternative notation for M7+5
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
				{"sus4", "sus4"}, // NEW - Explicit sus4
				{"sus2", "sus2"}, // NEW - Explicit sus2
				{"m7sus4", "m7sus4"},
				{"min7sus4", "m7sus4"},
				{"m7sus", "m7sus4"},
				{"min7sus", "m7sus4"},
				{"9sus4", "9sus4"},
				{"9sus", "9sus4"},
				{"dom9sus4", "9sus4"},
				{"dom9sus", "9sus4"},
				
				// NEW - Sus chords with altered notes
				{"sus4b9", "sus4b9"},
				{"susb9", "sus4b9"},
				{"sus4#9", "sus4#9"},
				{"sus#9", "sus4#9"},
				{"sus4b5", "sus4b5"},
				{"susb5", "sus4b5"},
				{"7sus4b9", "7sus4b9"},
				{"7susb9", "7sus4b9"},
				{"9sus4b9", "9sus4b9"},
				{"9susb9", "9sus4b9"},
				{"sus2b9", "sus2b9"}, // NEW
				
				// NEW - 6/9 and other mixed extension chords
				{"6/9", "69"},
				{"69", "69"},
				{"m6/9", "m69"},
				{"m69", "m69"},
				{"min6/9", "m69"},
				{"min69", "m69"},
				{"M6/9", "M69"},
				{"M69", "M69"},
				{"maj6/9", "M69"},
				{"maj69", "M69"},
				
				// NEW - 13 chords with alterations
				{"13b9", "13b9"},
				{"13#9", "13#9"},
				{"13b5", "13b5"},
				{"13#5", "13#5"},
				{"13#11", "13#11"},
				{"m13b9", "m13b9"},
				{"min13b9", "m13b9"},
				{"M13b9", "M13b9"},
				{"maj13b9", "M13b9"},
			
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
				
				// NEW - Alt chords
				{"7alt", "7alt"},
				{"7Alt", "7alt"},
				{"alt", "7alt"},
				{"Alt", "7alt"},
				{"7altered", "7alt"},
				{"altered", "7alt"},
				
				// Add9 variations
				{"add9", "add9"},
				{"2", "add9"},
				{"add2", "add9"},
				{"madd9", "madd9"},
				{"m2", "madd9"},
				{"madd2", "madd9"},
				{"minadd9", "madd9"},
				{"min2", "madd9"},
				{"minadd2", "madd9"},
				{"Add9", "add9"}, // NEW - Handle capitalization variations
				{"ADD9", "add9"},
				{"Add2", "add9"},
				{"ADD2", "add9"},
				{"MAdd9", "madd9"}, // NEW - Handle capitalization variations
				{"MADD9", "madd9"},
				{"MAdd2", "madd9"},
				{"MADD2", "madd9"},
				
				// Add11 variations
				{"add11", "add11"},
				{"add4", "add11"},
				{"4", "add11"},
				{"madd11", "madd11"},
				{"madd4", "madd11"},
				{"m4", "madd11"},
				{"minadd11", "madd11"},
				{"minadd4", "madd11"},
				{"min4", "madd11"},
				{"Add11", "add11"}, // NEW - Handle capitalization variations
				{"ADD11", "add11"},
				{"Add4", "add11"},
				{"ADD4", "add11"},
				{"MAdd11", "madd11"}, // NEW - Handle capitalization variations
				{"MADD11", "madd11"},
				{"MAdd4", "madd11"},
				{"MADD4", "madd11"},
				
				// Compound chord types
				{"m9", "m9"},
				{"min9", "m9"},
				{"M9", "M9"},
				{"maj9", "M9"},
				{"9", "9"},
				{"dom9", "9"},
				{"m11", "m11"},
				{"min11", "m11"},
				{"M11", "M11"},
				{"maj11", "M11"},
				{"11", "11"},
				{"dom11", "11"},
				{"m13", "m13"},
				{"min13", "m13"},
				{"M13", "M13"},
				{"maj13", "M13"},
				{"13", "13"},
				{"dom13", "13"},
				
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
		
		// Add some fallback definitions in case the file is missing or incomplete
		if(chordDefinitions.find("add9") == chordDefinitions.end()) {
			chordDefinitions["add9"] = {0, 4, 7, 14};
		}
		if(chordDefinitions.find("madd9") == chordDefinitions.end()) {
			chordDefinitions["madd9"] = {0, 3, 7, 14};
		}
		if(chordDefinitions.find("add11") == chordDefinitions.end()) {
			chordDefinitions["add11"] = {0, 4, 7, 17};
		}
		if(chordDefinitions.find("madd11") == chordDefinitions.end()) {
			chordDefinitions["madd11"] = {0, 3, 7, 17};
		}
		
		// Add definitions for sus4b9 chord
		if(chordDefinitions.find("sus4b9") == chordDefinitions.end()) {
			chordDefinitions["sus4b9"] = {0, 5, 7, 13}; // Root, 4th, 5th, flat 9th
		}
		
		// Add definitions for sus2b9 chord
		if(chordDefinitions.find("sus2b9") == chordDefinitions.end()) {
			chordDefinitions["sus2b9"] = {0, 2, 7, 13}; // Root, 2nd, 5th, flat 9th
		}
		
		// Add definitions for 7alt chord (an altered dominant chord)
		if(chordDefinitions.find("7alt") == chordDefinitions.end()) {
			// Typically includes some combination of b5/#5 and b9/#9
			chordDefinitions["7alt"] = {0, 4, 8, 10, 15}; // Root, M3, #5, b7, #9
		}
		
		// Add definition for M7#5 chord
		if(chordDefinitions.find("augM7") == chordDefinitions.end()) {
			chordDefinitions["augM7"] = {0, 4, 8, 11}; // Root, M3, #5, M7
		}
		
		// Add definition for 6/9 chords
		if(chordDefinitions.find("69") == chordDefinitions.end()) {
			chordDefinitions["69"] = {0, 4, 7, 9, 14}; // Root, M3, 5, 6, 9
		}
		if(chordDefinitions.find("m69") == chordDefinitions.end()) {
			chordDefinitions["m69"] = {0, 3, 7, 9, 14}; // Root, m3, 5, 6, 9
		}
		if(chordDefinitions.find("M69") == chordDefinitions.end()) {
			chordDefinitions["M69"] = {0, 4, 7, 9, 14}; // Same as 69, but explicit major
		}
		
		// Add definition for 13b9 chords
		if(chordDefinitions.find("13b9") == chordDefinitions.end()) {
			chordDefinitions["13b9"] = {0, 4, 7, 10, 13, 21}; // Root, M3, 5, b7, b9, 13
		}
		if(chordDefinitions.find("m13b9") == chordDefinitions.end()) {
			chordDefinitions["m13b9"] = {0, 3, 7, 10, 13, 21}; // Root, m3, 5, b7, b9, 13
		}
		if(chordDefinitions.find("M13b9") == chordDefinitions.end()) {
			chordDefinitions["M13b9"] = {0, 4, 7, 11, 13, 21}; // Root, M3, 5, M7, b9, 13
		}
		
		// Add definition for minor-major chords with extensions
		if(chordDefinitions.find("mM7") == chordDefinitions.end()) {
			chordDefinitions["mM7"] = {0, 3, 7, 11}; // Root, m3, 5, M7
		}
		if(chordDefinitions.find("mM9") == chordDefinitions.end()) {
			chordDefinitions["mM9"] = {0, 3, 7, 11, 14}; // Root, m3, 5, M7, 9
		}
		if(chordDefinitions.find("mM11") == chordDefinitions.end()) {
			chordDefinitions["mM11"] = {0, 3, 7, 11, 14, 17}; // Root, m3, 5, M7, 9, 11
		}
		if(chordDefinitions.find("mM13") == chordDefinitions.end()) {
			chordDefinitions["mM13"] = {0, 3, 7, 11, 14, 17, 21}; // Root, m3, 5, M7, 9, 11, 13
		}
		
		// Add definition for major 7 with sharp 11
		if(chordDefinitions.find("M7#11") == chordDefinitions.end()) {
			chordDefinitions["M7#11"] = {0, 4, 7, 11, 18}; // Root, M3, 5, M7, #11
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
		
		// Modified regex to capture root and all modifiers - UPDATED REGEX
		std::regex chordRegex("([A-G][#b]?)(.*)");
		
		if(std::regex_match(processedInput, matches, chordRegex)) {
			string noteName = matches[1].str();
			string chordSuffix = matches[2].str();
			
			// Find the root note
			int intervalRoot = getNoteValue(noteName);
			int baseRoot;
			if(hasSlash && !bassNote.empty()) {
				baseRoot = getNoteValue(bassNote);
			} else {
				baseRoot = intervalRoot;
			}
			rootOut.set((baseRoot + transpose.get()) % 12);
			
			// Process chord type
			std::vector<int> intervals;
			
			// If no suffix, default to major
			if(chordSuffix.empty()) {
				intervals = chordDefinitions["M"];
			} else {
				// Try to find exact match in definitions
				bool foundMatch = false;
				
				// Normalize chord suffix - remove spaces
				chordSuffix.erase(std::remove(chordSuffix.begin(), chordSuffix.end(), ' '), chordSuffix.end());
				
				// Check for exact match in definitions
				if(chordDefinitions.find(chordSuffix) != chordDefinitions.end()) {
					intervals = chordDefinitions[chordSuffix];
					foundMatch = true;
				}
				
				// Check if it's in aliases
				if(!foundMatch && chordAliases.find(chordSuffix) != chordAliases.end()) {
					string aliasedType = chordAliases[chordSuffix];
					if(chordDefinitions.find(aliasedType) != chordDefinitions.end()) {
						intervals = chordDefinitions[aliasedType];
						foundMatch = true;
					}
				}
				
				// Handle compound chord types like "madd9"
				if(!foundMatch) {
					// Check for common prefixes like "m", "M", etc.
					string prefix = "";
					string suffix = chordSuffix;
					
					// Check for "sus" prefix
					if(chordSuffix.size() >= 3 && chordSuffix.substr(0, 3) == "sus") {
						prefix = "sus4"; // Default sus means sus4
						suffix = chordSuffix.substr(3);
						
						// Check if it's explicitly sus2 or sus4
						if(suffix.size() >= 1) {
							if(suffix[0] == '2') {
								prefix = "sus2";
								suffix = suffix.substr(1);
							} else if(suffix[0] == '4') {
								prefix = "sus4";
								suffix = suffix.substr(1);
							}
						}
					}
					// Try to extract prefix (m, M, dim, aug, etc.)
					else if(chordSuffix.size() >= 1 && (chordSuffix.substr(0, 1) == "m" ||
						   (chordSuffix.size() >= 3 && chordSuffix.substr(0, 3) == "min"))) {
						prefix = "m";
						if(chordSuffix.substr(0, 1) == "m") {
							suffix = chordSuffix.substr(1);
						} else {
							suffix = chordSuffix.substr(3);
						}
					} else if(chordSuffix.size() >= 1 && (chordSuffix.substr(0, 1) == "M" ||
							 (chordSuffix.size() >= 3 && chordSuffix.substr(0, 3) == "maj"))) {
						prefix = "M";
						if(chordSuffix.substr(0, 1) == "M") {
							suffix = chordSuffix.substr(1);
						} else {
							suffix = chordSuffix.substr(3);
						}
					} else if(chordSuffix.size() >= 3 && chordSuffix.substr(0, 3) == "dim") {
						prefix = "dim";
						suffix = chordSuffix.substr(3);
					} else if(chordSuffix.size() >= 3 && chordSuffix.substr(0, 3) == "aug") {
						prefix = "aug";
						suffix = chordSuffix.substr(3);
					} else if(chordSuffix.size() >= 3 &&
							 (chordSuffix.substr(0, 3) == "alt" || chordSuffix.substr(0, 3) == "Alt")) {
						// Handle alt chord
						prefix = "7";
						suffix = "alt";
					}
					
					// Try to match combined chord type (e.g., "madd9" -> "m" + "add9")
					if(!prefix.empty() && !suffix.empty()) {
						// Look for the suffix in definitions or aliases
						string aliasedSuffix = suffix;
						if(chordAliases.find(suffix) != chordAliases.end()) {
							aliasedSuffix = chordAliases[suffix];
						}
						
						// Try to find combined version first (e.g., "madd9")
						string combinedType = prefix + aliasedSuffix;
						if(chordDefinitions.find(combinedType) != chordDefinitions.end()) {
							intervals = chordDefinitions[combinedType];
							foundMatch = true;
						}
						// If not found, try to compose from prefix and suffix
						else if(chordDefinitions.find(prefix) != chordDefinitions.end() &&
								chordDefinitions.find(aliasedSuffix) != chordDefinitions.end()) {
							// Special handling for add chords - merge intervals
							if(aliasedSuffix == "add9" || aliasedSuffix == "add11" || aliasedSuffix == "add13") {
								intervals = chordDefinitions[prefix]; // Start with the basic chord
								std::vector<int> addIntervals = chordDefinitions[aliasedSuffix];
								
								// Find the added interval (usually the last one)
								if(!addIntervals.empty()) {
									int addedInterval = addIntervals.back();
									intervals.push_back(addedInterval);
								}
								
								foundMatch = true;
							}
						}
					}
				}
				
				// If still no match, try fallbacks based on components
				if(!foundMatch) {
					// Check for "add" pattern
					std::regex addRegex("add([0-9]+)");
					if(std::regex_search(chordSuffix, matches, addRegex)) {
						string addNum = matches[1].str();
						int addInterval = 0;
						
						if(addNum == "9") addInterval = 14;
						else if(addNum == "11") addInterval = 17;
						else if(addNum == "13") addInterval = 21;
						
						if(addInterval > 0) {
							// First see if the chord starts with 'm' for minor
							if(chordSuffix.length() > 3 && (chordSuffix[0] == 'm' || chordSuffix.substr(0, 3) == "min")) {
								intervals = chordDefinitions["m"];  // Minor triad
							} else {
								intervals = chordDefinitions["M"];  // Major triad
							}
							intervals.push_back(addInterval);  // Add the extension
							foundMatch = true;
						}
					}
				}
				
				// Last resort - try to parse extensions
				if(!foundMatch) {
					// Try to recognize some common patterns
					if(chordSuffix == "7" || chordSuffix == "dom7") {
						intervals = chordDefinitions["7"];
					} else if(chordSuffix == "9" || chordSuffix == "dom9") {
						intervals = chordDefinitions["9"];
					} else if(chordSuffix == "11" || chordSuffix == "dom11") {
						intervals = chordDefinitions["11"];
					} else if(chordSuffix == "13" || chordSuffix == "dom13") {
						intervals = chordDefinitions["13"];
					} else {
						// Default to major triad if we still can't figure it out
						ofLogWarning("ChordCypher") << "Unknown chord type: " << chordSuffix << " in chord: " << input;
						intervals = chordDefinitions["M"];
					}
				}
			}
			
			// Now we have intervals, generate the final notes
			std::vector<int> result;
			for(int interval : intervals) {
				int note = interval + intervalRoot;
				
				if(fold) {
					note = note % 12;
				}
				
				result.push_back(note);
			}
			
			// Add bass note if slash chord
			if(!bassNote.empty()) {
				int bass = getNoteValue(bassNote);
				if(fold) {
					bass = bass % 12;
				}
				result.insert(result.begin(), bass);
			}
			
			// Sort and deduplicate if folding
			if(fold) {
				std::sort(result.begin(), result.end());
				result.erase(std::unique(result.begin(), result.end()), result.end());
			}
			
			// Apply transposition
			for(int& note : result) {
				note += transpose;
			}
			
			// Ensure we have at least one note
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
