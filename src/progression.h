#ifndef progression_h
#define progression_h

#include "ofxOceanodeNodeModel.h"
#include "ofMain.h"   // ofRandomf
#include <cmath>
#include <algorithm>

class progression : public ofxOceanodeNodeModel {
public:
	progression() : ofxOceanodeNodeModel("Progression") {}

	void setup() override {
		description = "Generates numeric progressions from a vector input. "
					  "Size controls the output length. Any operation can run "
					  "in item-wise or vector-wise mode. Includes jitter and sorting.";

		// Vector input
		addParameter(input.set("Input",
							   std::vector<float>{0.0f},
							   std::vector<float>{-FLT_MAX},
							   std::vector<float>{FLT_MAX}));

		addParameter(step.set("Step", 1.0f, -FLT_MAX, FLT_MAX));
		addParameter(size.set("Size", 8, 1, 4096));

		// Jitter multiplicatiu
		addParameter(jitter.set("Jitter", 0.0f, 0.0f, 1.0f));

		// Sort final
		addParameter(sortOutput.set("Sort", false));

		// Dropdown d'operacions
		// 0 = Multiply (geom)
		// 1 = Add (arith)
		// 2 = Subtract
		// 3 = Divide
		// 4 = Sqrt(prev)
		// 5 = Harmonic series
		// 6 = Subharmonic series
		// 7 = Spectral sqrt
		addParameterDropdown(mode,
							 "Mode",
							 1, // per defecte Add
							 {
								 "Multiply (geom)",
								 "Add (arith)",
								 "Subtract",
								 "Divide",
								 "Sqrt(prev)",
								 "Harmonic series",
								 "Subharmonic series",
								 "Spectral sqrt"
							 });

		// Dropdown per triar com s'aplica la progressió
		// 0 = Item-wise → index global dins del vector de sortida
		// 1 = Vector-wise → progressió per "capes" del vector d'entrada
		addParameterDropdown(applyMode,
							 "Apply",
							 0,
							 { "Item-wise", "Vector-wise" });

		// Output
		addOutputParameter(output.set("Output",
									  std::vector<float>{0.0f},
									  std::vector<float>{-FLT_MAX},
									  std::vector<float>{FLT_MAX}));

		// Listeners
		listeners.push(input.newListener([this](std::vector<float> &) { recompute(); }));
		listeners.push(step.newListener([this](float &) { recompute(); }));
		listeners.push(size.newListener([this](int &) { recompute(); }));
		listeners.push(jitter.newListener([this](float &) { recompute(); }));
		listeners.push(sortOutput.newListener([this](bool &) { recompute(); }));
		listeners.push(mode.newListener([this](int &) { recompute(); }));
		listeners.push(applyMode.newListener([this](int &) { recompute(); }));

		recompute();
	}

private:
	float applyOperationItemWise(float base, int index, int opMode, float s, float stepNorm,
								 const std::vector<float> &tempOut) {
		// index = i global de 0..size-1
		switch(opMode){
			case 0: // Multiply (geom): base * step^i
				if(index == 0) return base;
				return base * std::pow(s, static_cast<float>(index));

			case 1: // Add (arith): base + i*step
				return base + static_cast<float>(index) * s;

			case 2: // Subtract: base - i*step
				return base - static_cast<float>(index) * s;

			case 3: // Divide: base / step^i
			{
				if(index == 0 || s == 0.0f) return base;
				float denom = std::pow(s, static_cast<float>(index));
				if(denom == 0.0f) return base;
				return base / denom;
			}

			case 4: // Sqrt(prev): seqüencial sobre la sortida
			{
				if(index == 0) return base;
				float prev = tempOut[index - 1];
				if(prev < 0.0f) prev = 0.0f;
				return std::sqrt(prev);
			}

			case 5: // Harmonic series: base * (1 + i*stepNorm)
			{
				float harmIndex = 1.0f + static_cast<float>(index) * stepNorm;
				if(harmIndex <= 0.0f) harmIndex = 1.0f;
				return base * harmIndex;
			}

			case 6: // Subharmonic series: base / (1 + i*stepNorm)
			{
				float harmIndex = 1.0f + static_cast<float>(index) * stepNorm;
				if(harmIndex == 0.0f) harmIndex = 1.0f;
				return base / harmIndex;
			}

			case 7: // Spectral sqrt: base * sqrt(1 + i*stepNorm)
			{
				float harmIndex = 1.0f + static_cast<float>(index) * stepNorm;
				if(harmIndex <= 0.0f) harmIndex = 1.0f;
				return base * std::sqrt(harmIndex);
			}

			default:
				return base;
		}
	}

	float applyOperationVectorWise(float base, int layer, int opMode, float s, float stepNorm,
								   float &currentSqrtState) {
		// layer = "capa" o segment index: 0,1,2,...
		switch(opMode){
			case 0: // Multiply (geom): base * step^layer
			{
				if(layer == 0) return base;
				return base * std::pow(s, static_cast<float>(layer));
			}

			case 1: // Add (arith): base + layer*step
				return base + static_cast<float>(layer) * s;

			case 2: // Subtract: base - layer*step
				return base - static_cast<float>(layer) * s;

			case 3: // Divide: base / step^layer
			{
				if(layer == 0 || s == 0.0f) return base;
				float denom = std::pow(s, static_cast<float>(layer));
				if(denom == 0.0f) return base;
				return base / denom;
			}

			case 4: // Sqrt(prev) per element (vector-wise)
			{
				if(layer == 0){
					currentSqrtState = base;
				}else{
					if(currentSqrtState < 0.0f) currentSqrtState = 0.0f;
					currentSqrtState = std::sqrt(currentSqrtState);
				}
				return currentSqrtState;
			}

			case 5: // Harmonic series: base * (1 + layer*stepNorm)
			{
				float harmIndex = 1.0f + static_cast<float>(layer) * stepNorm;
				if(harmIndex <= 0.0f) harmIndex = 1.0f;
				return base * harmIndex;
			}

			case 6: // Subharmonic series: base / (1 + layer*stepNorm)
			{
				float harmIndex = 1.0f + static_cast<float>(layer) * stepNorm;
				if(harmIndex == 0.0f) harmIndex = 1.0f;
				return base / harmIndex;
			}

			case 7: // Spectral sqrt: base * sqrt(1 + layer*stepNorm)
			{
				float harmIndex = 1.0f + static_cast<float>(layer) * stepNorm;
				if(harmIndex <= 0.0f) harmIndex = 1.0f;
				return base * std::sqrt(harmIndex);
			}

			default:
				return base;
		}
	}

	void recompute() {
		const std::vector<float> &inVec = input.get();
		int inSize = static_cast<int>(inVec.size());
		int outSize = std::max(1, size.get());

		if(inSize == 0){
			output = std::vector<float>{};
			return;
		}

		std::vector<float> tempOut;
		tempOut.resize(outSize, 0.0f);

		auto getValueForIndex = [&](int i) -> float {
			if(inSize == 0) return 0.0f;
			int idx = i % inSize;   // wrap
			return inVec[idx];
		};

		float s        = step.get();
		float stepNorm = (s == 0.0f ? 1.0f : s);
		int   opMode   = mode.get();
		int   applMode = applyMode.get();
		float j        = jitter.get();

		if(applMode == 0){
			// ------- ITEM-WISE -------
			for(int i = 0; i < outSize; ++i){
				float base  = getValueForIndex(i);
				float value = applyOperationItemWise(base, i, opMode, s, stepNorm, tempOut);

				if(j > 0.0f){
					float factor = 1.0f + ofRandomf() * j;
					value *= factor;
				}

				tempOut[i] = value;
			}
		}else{
			// ------- VECTOR-WISE -------
			// Capes del vector d'entrada repetides fins omplir Size
			int outIndex = 0;
			int layer    = 0;

			// Estat per al mode Sqrt(prev) per element
			std::vector<float> sqrtState(inSize, 0.0f);

			while(outIndex < outSize){
				for(int k = 0; k < inSize && outIndex < outSize; ++k){
					float base = inVec[k];
					float &stateRef = sqrtState[k]; // per al mode 4

					float value = applyOperationVectorWise(base, layer, opMode, s, stepNorm, stateRef);

					if(j > 0.0f){
						float factor = 1.0f + ofRandomf() * j;
						value *= factor;
					}

					tempOut[outIndex++] = value;
				}
				layer++;
			}
		}

		if(sortOutput.get()){
			std::sort(tempOut.begin(), tempOut.end());
		}

		output = tempOut;
	}

	// Paràmetres
	ofParameter<std::vector<float>> input;
	ofParameter<float> step;
	ofParameter<int>   size;
	ofParameter<float> jitter;
	ofParameter<bool>  sortOutput;
	ofParameter<int>   mode;      // operació
	ofParameter<int>   applyMode; // item-wise / vector-wise

	ofParameter<std::vector<float>> output;

	ofEventListeners listeners;
};

#endif /* progression_h */
