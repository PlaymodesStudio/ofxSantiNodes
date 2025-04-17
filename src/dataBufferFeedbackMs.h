#ifndef DATABUFFERFEEDBACKMS_H
#define DATABUFFERFEEDBACKMS_H

#include "ofxOceanodeNodeModel.h"

class dataBufferFeedbackMs : public ofxOceanodeNodeModel {
public:
	dataBufferFeedbackMs() : ofxOceanodeNodeModel("Data Buffer Feedback ms") {}
	
	void setup() override {
		description = "Delays input by milliseconds and applies feedback. Output adds the delayed input combined with feedback.";
		
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(delayMs.set("Delay ms", {500}, {0}, {10000}));
		addParameter(feedback.set("Feedback", 0.5f, 0.0f, 0.999f));
		addParameter(bufferMaxSize.set("Buffer Max Size", 1000, 10, INT_MAX));
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		// Initialize time
		lastUpdateTime = ofGetElapsedTimeMillis();
	}
	
	void update(ofEventArgs &a) override {
		// Get current time
		uint64_t currentTime = ofGetElapsedTimeMillis();
		
		// Store current input with timestamp
		TimeStampedData newEntry;
		newEntry.timestamp = currentTime;
		newEntry.data = input.get();
		inputBuffer.push_back(newEntry);
		
		// Maintain buffer size limit
		while (inputBuffer.size() > bufferMaxSize) {
			inputBuffer.pop_front();
		}
		
		// Get current delay values in milliseconds
		vector<float> currentDelayMs = delayMs.get();
		
		// Ensure delay vector is same size as input
		while (currentDelayMs.size() < input.get().size()) {
			currentDelayMs.push_back(currentDelayMs.empty() ? 0 : currentDelayMs.back());
		}
		
		// Initialize output if needed
		if (currentOutput.empty() || currentOutput.size() != input.get().size()) {
			currentOutput.resize(input.get().size(), 0.0f);
		}
		
		// Process each element in the vector
		for (size_t i = 0; i < input.get().size(); i++) {
			// Calculate target timestamp for this element
			uint64_t targetTime = currentTime - static_cast<uint64_t>(currentDelayMs[i]);
			
			// Get delayed input value from buffer
			float delayedInput = 0.0f;
			bool foundDelayedValue = false;
			
			// Find the entry with timestamp closest to target time
			for (int j = inputBuffer.size() - 1; j >= 0; j--) {
				if (inputBuffer[j].timestamp <= targetTime) {
					if (i < inputBuffer[j].data.size()) {
						delayedInput = inputBuffer[j].data[i];
						foundDelayedValue = true;
						break;
					}
				}
			}
			
			if (!foundDelayedValue) {
				// If no delayed value found, use zero as the delayed input
				delayedInput = 0.0f;
			}
			
			// Apply feedback formula: new output = delayed input + (feedback * previous output)
			currentOutput[i] = delayedInput + (feedback * currentOutput[i]);
		}
		
		// Set output parameter
		output = currentOutput;
		
		// Update last update time
		lastUpdateTime = currentTime;
	}
	
private:
	// Structure to store timestamped data
	struct TimeStampedData {
		uint64_t timestamp;
		vector<float> data;
	};
	
	ofParameter<vector<float>> input;
	ofParameter<vector<float>> delayMs;
	ofParameter<float> feedback;
	ofParameter<int> bufferMaxSize;
	ofParameter<vector<float>> output;
	
	deque<TimeStampedData> inputBuffer;  // Buffer to store input values
	vector<float> currentOutput;         // Current output values
	uint64_t lastUpdateTime;             // Last time update was called
};

#endif /* DATABUFFERFEEDBACKMS_H */
