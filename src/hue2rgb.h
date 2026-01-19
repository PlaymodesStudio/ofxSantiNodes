#ifndef hue2rgb_h
#define hue2rgb_h

#include "ofxOceanodeNodeModel.h"

class hsv2rgb : public ofxOceanodeNodeModel {
public:
	hsv2rgb() : ofxOceanodeNodeModel("HSV to RGB") {}

	void setup() override {
		description = "Converts HSV (all normalized 0-1) to RGB (0-1). Inputs can be single values or vectors.";
		
		addParameter(hueInput.set("H", {0.0f}, {0.0f}, {1.0f}));
		addParameter(satInput.set("S", {1.0f}, {0.0f}, {1.0f}));
		addParameter(valInput.set("V", {1.0f}, {0.0f}, {1.0f}));
		addOutputParameter(redOutput.set("R", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(greenOutput.set("G", {0.0f}, {0.0f}, {1.0f}));
		addOutputParameter(blueOutput.set("B", {0.0f}, {0.0f}, {1.0f}));

		listeners.push(hueInput.newListener([this](vector<float> &) { convert(); }));
		listeners.push(satInput.newListener([this](vector<float> &) { convert(); }));
		listeners.push(valInput.newListener([this](vector<float> &) { convert(); }));
	}

private:
	void convert() {
		const vector<float>& hues = hueInput.get();
		const vector<float>& sats = satInput.get();
		const vector<float>& vals = valInput.get();
		
		size_t size = std::max({hues.size(), sats.size(), vals.size()});
		
		vector<float> r(size);
		vector<float> g(size);
		vector<float> b(size);

		for (size_t i = 0; i < size; ++i) {
			float h = hues[i % hues.size()];
			float s = sats[i % sats.size()];
			float v = vals[i % vals.size()];
			
			// Wrap hue
			h = fmod(h, 1.0f);
			if (h < 0) h += 1.0f;
			
			// Clamp S and V
			s = std::max(0.0f, std::min(1.0f, s));
			v = std::max(0.0f, std::min(1.0f, v));
			
			// HSV to RGB conversion
			float h6 = h * 6.0f;
			int sector = static_cast<int>(h6) % 6;
			float f = h6 - floor(h6);
			
			float p = v * (1.0f - s);
			float q = v * (1.0f - s * f);
			float t = v * (1.0f - s * (1.0f - f));

			switch (sector) {
				case 0: r[i] = v; g[i] = t; b[i] = p; break;
				case 1: r[i] = q; g[i] = v; b[i] = p; break;
				case 2: r[i] = p; g[i] = v; b[i] = t; break;
				case 3: r[i] = p; g[i] = q; b[i] = v; break;
				case 4: r[i] = t; g[i] = p; b[i] = v; break;
				case 5: r[i] = v; g[i] = p; b[i] = q; break;
			}
		}

		redOutput = r;
		greenOutput = g;
		blueOutput = b;
	}

	ofParameter<vector<float>> hueInput;
	ofParameter<vector<float>> satInput;
	ofParameter<vector<float>> valInput;
	ofParameter<vector<float>> redOutput;
	ofParameter<vector<float>> greenOutput;
	ofParameter<vector<float>> blueOutput;
	
	ofEventListeners listeners;
};

#endif /* hue2rgb_h */
