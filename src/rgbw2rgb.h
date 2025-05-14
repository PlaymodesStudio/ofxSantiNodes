#ifndef rgbw2rgb_h
#define rgbw2rgb_h

#include "ofxOceanodeNodeModel.h"

class rgbw2rgb : public ofxOceanodeNodeModel {
public:
	rgbw2rgb() : ofxOceanodeNodeModel("RGBW to RGB") {}

	void setup() override {
		description = "Converts RGBW color values (normalized 0-1) back to RGB values (normalized 0-1).";
		
		// Input parameters
		addParameter(rgbwInput.set("RGBW Input", {0, 0, 0, 0}, {0}, {1}));
		
		// Output parameters
		addOutputParameter(rgbOutput.set("RGB Output", {0, 0, 0}, {0}, {1}));
		
		// Add listener to process whenever input changes
		listener = rgbwInput.newListener([this](vector<float> &rgbw){
			processRgbwToRgb();
		});
	}

private:
	void processRgbwToRgb() {
		vector<float> rgbw = rgbwInput.get();
		vector<float> rgb(3, 0.0f); // Initialize with 3 zeros
		
		// Check if we have at least 4 values for RGBW
		if (rgbw.size() >= 4) {
			float r = rgbw[0];
			float g = rgbw[1];
			float b = rgbw[2];
			float w = rgbw[3];
			
			// Add the white component back to RGB
			rgb[0] = r + w; // R
			rgb[1] = g + w; // G
			rgb[2] = b + w; // B
			
			// Clamp values to the 0-1 range
			for (int i = 0; i < 3; i++) {
				rgb[i] = std::max(0.0f, std::min(1.0f, rgb[i]));
			}
		}
		
		// Update the output parameter
		rgbOutput = rgb;
	}

	ofParameter<vector<float>> rgbwInput;
	ofParameter<vector<float>> rgbOutput;
	ofEventListener listener;
};

#endif /* rgbw2rgb_h */
