#ifndef harmonicPartials_h
#define harmonicPartials_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <cfloat>

class harmonicPartials : public ofxOceanodeNodeModel {
public:
	harmonicPartials() : ofxOceanodeNodeModel("Harmonic Partials") {}

	void setup() override {
		description = "Generates harmonic or subharmonic numbers from a root. "
					  "Input is a vector of harmonic indices; output is root * n "
					  "or root / n depending on the selected mode. If Root = 1, "
					  "the output are pure ratios.";

		// Root: can be 1.0 (for ratios) or any frequency in Hz
		addParameter(root.set("Root", 1.0f, 0.0f, FLT_MAX));

		// Harmonic indices (or multipliers) as a vector
		// Example: {1,2,3,4,5,11,13}
		addParameter(harmonics.set("Harmonics",
								   std::vector<float>{1.0f, 2.0f, 3.0f},
								   std::vector<float>{-FLT_MAX},
								   std::vector<float>{FLT_MAX}));

		// Mode:
		// 0 = Harmonics:   output[i] = root * n
		// 1 = Subharmonics:output[i] = root / n
		addParameterDropdown(mode,
							 "Mode",
							 0,
							 {
								 "Harmonics (root * n)",
								 "Subharmonics (root / n)"
							 });

		// Output vector
		addOutputParameter(output.set("Output",
									  std::vector<float>{1.0f},
									  std::vector<float>{-FLT_MAX},
									  std::vector<float>{FLT_MAX}));

		// Listeners
		listeners.push(root.newListener([this](float &) {
			recompute();
		}));
		listeners.push(harmonics.newListener([this](std::vector<float> &) {
			recompute();
		}));
		listeners.push(mode.newListener([this](int &) {
			recompute();
		}));

		// Initial compute
		recompute();
	}

private:
	void recompute() {
		float r = root.get();
		int   m = mode.get();
		const std::vector<float> &h = harmonics.get();

		if(h.empty()){
			output = std::vector<float>{};
			return;
		}

		std::vector<float> out;
		out.resize(h.size());

		if(m == 0){
			// Harmonics: root * n
			for(size_t i = 0; i < h.size(); ++i){
				out[i] = r * h[i];
			}
		}else{
			// Subharmonics: root / n
			for(size_t i = 0; i < h.size(); ++i){
				float n = h[i];
				if(n == 0.0f){
					out[i] = 0.0f; // avoid division by zero
				}else{
					out[i] = r / n;
				}
			}
		}

		output = out;
	}

	// Parameters
	ofParameter<float>              root;
	ofParameter<std::vector<float>> harmonics;
	ofParameter<int>                mode;

	ofParameter<std::vector<float>> output;

	ofEventListeners listeners;
};

#endif /* harmonicPartials */
