#ifndef harmonyDetector_h
#define harmonyDetector_h

#include "ofxOceanodeNodeModel.h"
#include <set>
#include <map>
#include <algorithm>
#include <sstream>

struct HarmonyPattern {
	string name;
	set<int> intervals; // Relative to root (0)
	
	HarmonyPattern(const string& n, const set<int>& i) : name(n), intervals(i) {}
};

class harmonyDetector : public ofxOceanodeNodeModel {
public:
	harmonyDetector() : ofxOceanodeNodeModel("Harmony Detector") {}
	
	void setup() override {
		description = "Detects chords and scales from incoming pitch values. Converts pitches to pitch classes (mod 12), finds root note, and identifies chord/scale quality. Reads definitions from chords.txt and scales.txt files.";
		
		addParameter(pitchInput.set("Pitch", {60}, {0}, {127}));
		addParameter(accumMode.set("Accum", false));
		addParameter(clearAccum.set("Clear"));
		addOutputParameter(detectedChord.set("Chord", "none"));
		addOutputParameter(detectedScale.set("Scale", "none"));
		addOutputParameter(pitchClasses.set("Pitch Classes", {0}, {0}, {11}));
		
		// Note names for root identification
		noteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		
		// Load chord and scale databases from files
		loadChordsFromFile();
		loadScalesFromFile();
		
		// Add listeners
		listener = pitchInput.newListener([this](vector<float> &vf){
			analyzeHarmony();
		});
		
		clearListener = clearAccum.newListener([this](){
			accumulatedPitchClasses.clear();
			analyzeHarmony();
		});
	}
	
private:
	ofParameter<vector<float>> pitchInput;
	ofParameter<bool> accumMode;
	ofParameter<void> clearAccum;
	ofParameter<string> detectedChord;
	ofParameter<string> detectedScale;
	ofParameter<vector<int>> pitchClasses;
	ofEventListener listener, clearListener;
	
	vector<HarmonyPattern> chords;
	vector<HarmonyPattern> scales;
	vector<string> noteNames;
	set<int> accumulatedPitchClasses; // For accumulation mode
	
	void loadChordsFromFile() {
		string filePath = ofToDataPath("Supercollider/Pitchclass/chords.txt");
		ofFile file(filePath);
		
		if (!file.exists()) {
			ofLogError("harmonyDetector") << "Could not find chords.txt at: " << filePath;
			return;
		}
		
		ofBuffer buffer = file.readToBuffer();
		
		for (auto line : buffer.getLines()) {
			if (line.empty()) continue;
			
			// Parse line format: "id, name interval1 interval2 interval3...;"
			size_t commaPos = line.find(',');
			if (commaPos == string::npos) continue;
			
			string remainder = line.substr(commaPos + 1);
			
			// Remove trailing semicolon if present
			if (!remainder.empty() && remainder.back() == ';') {
				remainder.pop_back();
			}
			
			// Trim whitespace
			remainder = ofTrim(remainder);
			
			// Find first space to separate name from intervals
			size_t spacePos = remainder.find(' ');
			if (spacePos == string::npos) continue;
			
			string chordName = remainder.substr(0, spacePos);
			string intervalString = remainder.substr(spacePos + 1);
			
			// Parse intervals
			set<int> intervalSet;
			stringstream ss(intervalString);
			string interval;
			
			while (ss >> interval) {
				int intervalValue = ofToInt(interval);
				intervalSet.insert(intervalValue % 12); // Apply mod 12
			}
			
			if (!intervalSet.empty()) {
				chords.emplace_back(chordName, intervalSet);
			}
		}
		
		// Sort chords by size (smaller first for exact match priority)
		std::sort(chords.begin(), chords.end(), [](const HarmonyPattern& a, const HarmonyPattern& b) {
			return a.intervals.size() < b.intervals.size();
		});
		
		ofLogNotice("harmonyDetector") << "Loaded " << chords.size() << " chord patterns";
	}
	
	void loadScalesFromFile() {
		string filePath = ofToDataPath("Supercollider/Pitchclass/scales.txt");
		ofFile file(filePath);
		
		if (!file.exists()) {
			ofLogError("harmonyDetector") << "Could not find scales.txt at: " << filePath;
			return;
		}
		
		ofBuffer buffer = file.readToBuffer();
		
		for (auto line : buffer.getLines()) {
			if (line.empty()) continue;
			
			// Parse line format: "id, name interval1 interval2 interval3...;"
			size_t commaPos = line.find(',');
			if (commaPos == string::npos) continue;
			
			string remainder = line.substr(commaPos + 1);
			
			// Remove trailing semicolon if present
			if (!remainder.empty() && remainder.back() == ';') {
				remainder.pop_back();
			}
			
			// Trim whitespace
			remainder = ofTrim(remainder);
			
			// Find first space to separate name from intervals
			size_t spacePos = remainder.find(' ');
			if (spacePos == string::npos) continue;
			
			string scaleName = remainder.substr(0, spacePos);
			string intervalString = remainder.substr(spacePos + 1);
			
			// Parse intervals
			set<int> intervalSet;
			stringstream ss(intervalString);
			string interval;
			
			while (ss >> interval) {
				int intervalValue = ofToInt(interval);
				intervalSet.insert(intervalValue % 12); // Apply mod 12
			}
			
			if (!intervalSet.empty()) {
				scales.emplace_back(scaleName, intervalSet);
			}
		}
		
		// Sort scales by size (smaller first for exact match priority)
		std::sort(scales.begin(), scales.end(), [](const HarmonyPattern& a, const HarmonyPattern& b) {
			return a.intervals.size() < b.intervals.size();
		});
		
		ofLogNotice("harmonyDetector") << "Loaded " << scales.size() << " scale patterns";
	}
	
	// Transpose a set of pitch classes by a given amount
	set<int> transposePitchClasses(const set<int>& pitchClasses, int transposition) {
		set<int> transposed;
		for (int pc : pitchClasses) {
			transposed.insert((pc - transposition + 12) % 12);
		}
		return transposed;
	}
	
	// Find the best root note for a given pattern against input pitch classes
	pair<int, bool> findBestRoot(const HarmonyPattern& pattern, const set<int>& inputPitchClasses, bool exactMatch) {
		for (int root = 0; root < 12; root++) {
			// Transpose the input to this potential root
			set<int> transposedInput = transposePitchClasses(inputPitchClasses, root);
			
			if (exactMatch) {
				if (pattern.intervals == transposedInput) {
					return make_pair(root, true);
				}
			} else {
				// Check if pattern is contained in transposed input
				if (std::includes(transposedInput.begin(), transposedInput.end(),
								pattern.intervals.begin(), pattern.intervals.end())) {
					return make_pair(root, true);
				}
			}
		}
		return make_pair(-1, false);
	}
	
	void analyzeHarmony() {
		if (pitchInput->empty()) {
			if (!accumMode) {
				detectedChord = "none";
				detectedScale = "none";
				pitchClasses = vector<int>();
			}
			return;
		}
		
		set<int> inputPitchClasses;
		
		if (accumMode) {
			// In accumulation mode, add new pitches to accumulated set
			for (float pitch : pitchInput.get()) {
				int pitchClass = ((int)round(pitch)) % 12;
				if (pitchClass < 0) pitchClass += 12; // Handle negative values
				accumulatedPitchClasses.insert(pitchClass);
			}
			inputPitchClasses = accumulatedPitchClasses;
		} else {
			// In normal mode, analyze current input only
			for (float pitch : pitchInput.get()) {
				int pitchClass = ((int)round(pitch)) % 12;
				if (pitchClass < 0) pitchClass += 12; // Handle negative values
				inputPitchClasses.insert(pitchClass);
			}
		}
		
		// Skip analysis if no pitch classes available
		if (inputPitchClasses.empty()) {
			detectedChord = "none";
			detectedScale = "none";
			pitchClasses = vector<int>();
			return;
		}
		
		// Output the pitch classes for visualization
		vector<int> pcVector(inputPitchClasses.begin(), inputPitchClasses.end());
		pitchClasses = pcVector;
		
		// Find best matching chord
		string bestChord = "none";
		
		// First try exact matches
		for (const auto& chord : chords) {
			auto result = findBestRoot(chord, inputPitchClasses, true);
			if (result.second) {
				bestChord = noteNames[result.first] + " " + chord.name;
				break;
			}
		}
		
		// If no exact match, try contained matches
		if (bestChord == "none") {
			HarmonyPattern* bestPattern = nullptr;
			int bestRoot = -1;
			
			for (const auto& chord : chords) {
				auto result = findBestRoot(chord, inputPitchClasses, false);
				if (result.second) {
					if (!bestPattern || chord.intervals.size() > bestPattern->intervals.size()) {
						bestPattern = const_cast<HarmonyPattern*>(&chord);
						bestRoot = result.first;
					}
				}
			}
			
			if (bestPattern && bestRoot != -1) {
				bestChord = noteNames[bestRoot] + " " + bestPattern->name;
			}
		}
		
		detectedChord = bestChord;
		
		// Find best matching scale
		string bestScale = "none";
		
		// First try exact matches
		for (const auto& scale : scales) {
			auto result = findBestRoot(scale, inputPitchClasses, true);
			if (result.second) {
				bestScale = noteNames[result.first] + " " + scale.name;
				break;
			}
		}
		
		// If no exact match, try contained matches
		if (bestScale == "none") {
			HarmonyPattern* bestPattern = nullptr;
			int bestRoot = -1;
			
			for (const auto& scale : scales) {
				auto result = findBestRoot(scale, inputPitchClasses, false);
				if (result.second) {
					if (!bestPattern || scale.intervals.size() > bestPattern->intervals.size()) {
						bestPattern = const_cast<HarmonyPattern*>(&scale);
						bestRoot = result.first;
					}
				}
			}
			
			if (bestPattern && bestRoot != -1) {
				bestScale = noteNames[bestRoot] + " " + bestPattern->name;
			}
		}
		
		detectedScale = bestScale;
	}
};

#endif /* harmonyDetector_h */
