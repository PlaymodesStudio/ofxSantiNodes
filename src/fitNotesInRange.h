#ifndef fitNotesInRange_h
#define fitNotesInRange_h

#include "ofxOceanodeNodeModel.h"

class fitNotesInRange : public ofxOceanodeNodeModel {
public:
	fitNotesInRange() : ofxOceanodeNodeModel("Fit Notes In Range") {}
	
	void setup() override {
		description = "Maps input notes to fit within a specified range while preserving pitch classes (note names). Notes outside the range are transposed to their enharmonic equivalent within the range.";
		
		addParameter(notesInput.set("Notes", {60}, {0}, {127}));
		addParameter(rangeMin.set("Range Min", 60, 0, 127));
		addParameter(rangeMax.set("Range Max", 72, 0, 127));
		addOutputParameter(notesOutput.set("Output", {60}, {0}, {127}));
		
		// Add listeners to process when inputs change
		inputListener = notesInput.newListener([this](vector<float> &vf){
			processNotes();
		});
		
		minListener = rangeMin.newListener([this](int &i){
			processNotes();
		});
		
		maxListener = rangeMax.newListener([this](int &i){
			processNotes();
		});
	}
	
private:
	ofParameter<vector<float>> notesInput;
	ofParameter<int> rangeMin;
	ofParameter<int> rangeMax;
	ofParameter<vector<float>> notesOutput;
	
	ofEventListener inputListener, minListener, maxListener;
	
	void processNotes() {
		vector<float> output;
		int minRange = rangeMin.get();
		int maxRange = rangeMax.get();
		
		// Ensure valid range
		if (maxRange < minRange) {
			// Swap if range is inverted
			int temp = minRange;
			minRange = maxRange;
			maxRange = temp;
		}
		
		for (float inputNote : notesInput.get()) {
			float mappedNote = fitNoteInRange(inputNote, minRange, maxRange);
			output.push_back(mappedNote);
		}
		
		notesOutput = output;
	}
	
	float fitNoteInRange(float inputNote, int minRange, int maxRange) {
		int roundedInput = (int)round(inputNote);
		
		// If already in range, return as is
		if (roundedInput >= minRange && roundedInput <= maxRange) {
			return inputNote;
		}
		
		// Calculate pitch class (0-11)
		int pitchClass = roundedInput % 12;
		if (pitchClass < 0) pitchClass += 12; // Handle negative values
		
		// Find the note within range that has the same pitch class
		for (int candidate = minRange; candidate <= maxRange; candidate++) {
			int candidatePitchClass = candidate % 12;
			if (candidatePitchClass < 0) candidatePitchClass += 12;
			
			if (candidatePitchClass == pitchClass) {
				return (float)candidate;
			}
		}
		
		// Fallback: if no exact pitch class match found in range,
		// transpose by octaves until within range
		float result = inputNote;
		
		if (roundedInput > maxRange) {
			// Note is too high, transpose down by octaves
			while (result > maxRange) {
				result -= 12.0f;
			}
		} else if (roundedInput < minRange) {
			// Note is too low, transpose up by octaves
			while (result < minRange) {
				result += 12.0f;
			}
		}
		
		return result;
	}
};

#endif /* fitNotesInRange_h */
