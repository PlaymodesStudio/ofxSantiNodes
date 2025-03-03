#ifndef voidCounter_h
#define voidCounter_h

#include "ofxOceanodeNodeModel.h"

class voidCounter : public ofxOceanodeNodeModel {
public:
	voidCounter() : ofxOceanodeNodeModel("Void Counter")
	{
		description = "Counts incoming void events and outputs the count. Can be reset manually or automatically after N events.";
		
		// Initialize counter and flags
		count = 0;
		resetNextFlag = false;
		
		// Add parameters
		addParameter(voidIn.set("Void In"));
		addParameter(reset.set("Reset"));
		addParameter(resetNext.set("Reset Next"));
		addParameter(everyN.set("Every N", 1, 1, INT_MAX));
		addParameter(countOut.set("Count", 0, 0, INT_MAX));
		addParameter(everyNOut.set("Every N Out"));
		
		// Set up event listeners
		listeners.push(voidIn.newListener([this](){
			// If resetNext flag is set, reset counter and clear flag
			if (resetNextFlag) {
				count = 0;
				resetNextFlag = false;
			} else {
				// Increment counter
				count++;
				
				// Check if we should trigger everyNOut
				if (count % everyN == 0) {
					everyNOut.trigger();
				}
			}
			
			// Update output parameter
			countOut = count;
		}));
		
		listeners.push(reset.newListener([this](){
			// Reset counter immediately
			count = 0;
			countOut = 0;
			resetNextFlag = false;
		}));
		
		listeners.push(resetNext.newListener([this](){
			// Set flag to reset on next void input
			resetNextFlag = true;
		}));
	}
	
private:
	// Parameters
	ofParameter<void> voidIn;
	ofParameter<void> reset;
	ofParameter<void> resetNext;
	ofParameter<int> everyN;
	ofParameter<int> countOut;
	ofParameter<void> everyNOut;
	
	// Internal state
	int count;
	bool resetNextFlag;
	
	// Event listeners
	ofEventListeners listeners;
};

#endif /* voidCounter_h */
