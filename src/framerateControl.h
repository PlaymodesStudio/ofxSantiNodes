#ifndef framerateControl_h
#define framerateControl_h

#include "ofxOceanodeNodeModel.h"

class framerateControl : public ofxOceanodeNodeModel {
public:
	framerateControl() : ofxOceanodeNodeModel("Framerate Control") {}

	void setup() override {
		description = "Controls the application framerate. Use the target FPS parameter to set the desired framerate, toggle to enable/disable framerate control, and monitor the actual FPS.";
		
		// Add parameters
		addParameter(toggle.set("Active", true));
		addParameter(targetFPS.set("Target FPS", 60, 1, 300));
		addParameter(vsync.set("VSync", true));
		addOutputParameter(actualFPS.set("Actual FPS", 0, 0, 300));
		
		// Add listeners
		toggleListener = toggle.newListener([this](bool &val){
			updateFramerate();
		});
		
		targetFPSListener = targetFPS.newListener([this](int &val){
			updateFramerate();
		});
		
		vsyncListener = vsync.newListener([this](bool &val){
			updateFramerate();
		});
		
		// Initialize framerate
		updateFramerate();
	}
	
	void update(ofEventArgs &args) override {
		// Update the actual FPS output
		actualFPS = ofGetFrameRate();
	}
	
private:
	void updateFramerate() {
		if(toggle) {
			ofSetVerticalSync(vsync);
			ofSetFrameRate(targetFPS);
		} else {
			// Reset to default settings when not active
			ofSetVerticalSync(true);
			ofSetFrameRate(60);
		}
	}

	ofParameter<bool> toggle;
	ofParameter<int> targetFPS;
	ofParameter<bool> vsync;
	ofParameter<float> actualFPS;
	
	ofEventListener toggleListener;
	ofEventListener targetFPSListener;
	ofEventListener vsyncListener;
};

#endif /* framerateControl_h */
