#ifndef rgb2rgbw_h
#define rgb2rgbw_h

#include "ofxOceanodeNodeModel.h"

class rgb2rgbw : public ofxOceanodeNodeModel {
public:
	rgb2rgbw() : ofxOceanodeNodeModel("RGB to RGBW") {}

	void setup() override {
		description = "Converts RGB color values (normalized 0-1) to RGBW values (normalized 0-1).";
		
		// Input parameters
		addParameter(rgbInput.set("RGB Input", {0, 0, 0}, {0}, {1}));
		
		// Output parameters
		addOutputParameter(rgbwOutput.set("RGBW Output", {0, 0, 0, 0}, {0}, {1}));
		
		// Add listener to process whenever input changes
		listener = rgbInput.newListener([this](vector<float> &rgb){
			processRgbToRgbw();
		});
	}

private:
	void processRgbToRgbw() {
		vector<float> rgb = rgbInput.get();
		vector<float> rgbw(4, 0.0f); // Initialize with 4 zeros
		
		// Check if we have at least 3 values for RGB
		if (rgb.size() >= 3) {
			float r = rgb[0];
			float g = rgb[1];
			float b = rgb[2];
			
			// Find the minimum value among R, G, B
			float w = std::min(std::min(r, g), b);
			
			// Subtract white from RGB to get new RGB values
			r = r - w;
			g = g - w;
			b = b - w;
			
			// Set the RGBW output values
			rgbw[0] = r; // R
			rgbw[1] = g; // G
			rgbw[2] = b; // B
			rgbw[3] = w; // W
		}
		
		// Update the output parameter
		rgbwOutput = rgbw;
	}

	ofParameter<vector<float>> rgbInput;
	ofParameter<vector<float>> rgbwOutput;
	ofEventListener listener;
};

#endif /* rgb2rgbw_h */
