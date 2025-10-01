#ifndef radialIndexer_h
#define radialIndexer_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <numeric>
#include <cmath>
#include <random>
#include <algorithm>

class radialIndexer : public ofxOceanodeNodeModel {
public:
	radialIndexer() : ofxOceanodeNodeModel("Radial Indexer") {
		color = ofColor::orange;
	}
	
	void setup() override {
		description = "Creates radial index patterns similar to Indexer Texture 2 but outputs vector data";
		
		// Size parameters
		addParameter(width.set("Width", 100, 1, 5120));
		addParameter(height.set("Height", 100, 1, 2880));
		addParameter(radiusResolution.set("Res.R", 100, 1, INT_MAX));
		addParameter(angleResolution.set("Res.A", 360, 1, INT_MAX));
		
		// Center parameters
		addParameter(xCenter.set("Center.X", 0.5, 0, 1));
		addParameter(yCenter.set("Center.Y", 0.5, 0, 1));
		
		// Indexer parameters - Radius (.R) and Angle (.A) variants
		addParameter(numWaves[0].set("NumW.R", 1.0f, 0.0f, 100.0f));
		addParameter(numWaves[1].set("NumW.A", 1.0f, 0.0f, 100.0f));
		
		addParameter(indexInvert[0].set("Inv.R", 0.0f, 0.0f, 1.0f));
		addParameter(indexInvert[1].set("Inv.A", 0.0f, 0.0f, 1.0f));
		
		addParameter(indexSymmetry[0].set("Sym.R", 0, 0, radiusResolution/2));
		addParameter(indexSymmetry[1].set("Sym.A", 0, 0, angleResolution/2));
		
		addParameter(indexRandom[0].set("Rndm.R", 0.0f, 0.0f, 1.0f));
		addParameter(indexRandom[1].set("Rndm.A", 0.0f, 0.0f, 1.0f));
		
		addParameter(indexOffset[0].set("Offs.R", 0.0f, -50.0f, 50.0f));
		addParameter(indexOffset[1].set("Offs.A", 0.0f, -50.0f, 50.0f));
		
		addParameter(indexQuantization[0].set("Quant.R", radiusResolution, 1, radiusResolution));
		addParameter(indexQuantization[1].set("Quant.A", angleResolution, 1, angleResolution));
		
		addParameter(indexCombination[0].set("Comb.R", 0.0f, 0.0f, 1.0f));
		addParameter(indexCombination[1].set("Comb.A", 0.0f, 0.0f, 1.0f));
		
		addParameter(indexModulo[0].set("Mod.R", radiusResolution, 1, radiusResolution));
		addParameter(indexModulo[1].set("Mod.A", angleResolution, 1, angleResolution));
		
		addParameter(normalize.set("Normalize", true));
		addParameter(discrete.set("Discrete", false));
		
		// Output
		addOutputParameter(indexsOut.set("Output", {0.0f}, {0.0f}, {1.0f}));
		
		// Initialize random vectors
		initializeRandomVectors();
		
		// Set up listeners
		setupListeners();
		
		// Initial computation
		recompute();
	}
	
private:
	// Parameters
	ofParameter<int> width, height;
	ofParameter<int> radiusResolution, angleResolution;
	ofParameter<float> xCenter, yCenter;
	
	ofParameter<float> numWaves[2];
	ofParameter<float> indexInvert[2];
	ofParameter<int> indexSymmetry[2];
	ofParameter<float> indexRandom[2];
	ofParameter<float> indexOffset[2];
	ofParameter<int> indexQuantization[2];
	ofParameter<float> indexCombination[2];
	ofParameter<int> indexModulo[2];
	
	ofParameter<bool> normalize;
	ofParameter<bool> discrete;
	
	ofParameter<vector<float>> indexsOut;
	
	// Random vectors for shuffle functionality
	vector<int> randomR, randomA;
	float previousRandomR = -1.0f;
	float previousRandomA = -1.0f;
	
	ofEventListeners listeners;
	
	void setupListeners() {
		listeners.push(width.newListener([this](int&){ recompute(); }));
		listeners.push(height.newListener([this](int&){ recompute(); }));
		listeners.push(radiusResolution.newListener([this](int& val){
			updateParameterRanges();
			initializeRandomVectors();
			recompute();
		}));
		listeners.push(angleResolution.newListener([this](int& val){
			updateParameterRanges();
			initializeRandomVectors();
			recompute();
		}));
		
		listeners.push(xCenter.newListener([this](float&){ recompute(); }));
		listeners.push(yCenter.newListener([this](float&){ recompute(); }));
		
		for(int i = 0; i < 2; i++) {
			listeners.push(numWaves[i].newListener([this](float&){ recompute(); }));
			listeners.push(indexInvert[i].newListener([this](float&){ recompute(); }));
			listeners.push(indexSymmetry[i].newListener([this](int&){ recompute(); }));
			listeners.push(indexRandom[i].newListener([this, i](float& val){
				if(i == 0 && val != previousRandomR && val == 0.0f) {
					regenerateRandomVector(0);
				} else if(i == 1 && val != previousRandomA && val == 0.0f) {
					regenerateRandomVector(1);
				}
				if(i == 0) previousRandomR = val;
				else previousRandomA = val;
				recompute();
			}));
			listeners.push(indexOffset[i].newListener([this](float&){ recompute(); }));
			listeners.push(indexQuantization[i].newListener([this](int&){ recompute(); }));
			listeners.push(indexCombination[i].newListener([this](float&){ recompute(); }));
			listeners.push(indexModulo[i].newListener([this](int&){ recompute(); }));
		}
		
		listeners.push(normalize.newListener([this](bool&){ recompute(); }));
		listeners.push(discrete.newListener([this](bool&){ recompute(); }));
	}
	
	void updateParameterRanges() {
		indexSymmetry[0].setMax(radiusResolution/2);
		indexSymmetry[1].setMax(angleResolution/2);
		
		indexQuantization[0].setMax(radiusResolution);
		indexQuantization[1].setMax(angleResolution);
		
		indexModulo[0].setMax(radiusResolution);
		indexModulo[1].setMax(angleResolution);
		
		// Clamp current values to new ranges
		indexSymmetry[0] = ofClamp(indexSymmetry[0], 0, radiusResolution/2);
		indexSymmetry[1] = ofClamp(indexSymmetry[1], 0, angleResolution/2);
		
		indexQuantization[0] = ofClamp(indexQuantization[0], 1, radiusResolution);
		indexQuantization[1] = ofClamp(indexQuantization[1], 1, angleResolution);
		
		indexModulo[0] = ofClamp(indexModulo[0], 1, radiusResolution);
		indexModulo[1] = ofClamp(indexModulo[1], 1, angleResolution);
	}
	
	void initializeRandomVectors() {
		randomR.resize(radiusResolution);
		std::iota(randomR.begin(), randomR.end(), 0);
		
		randomA.resize(angleResolution);
		std::iota(randomA.begin(), randomA.end(), 0);
		
		regenerateRandomVector(0);
		regenerateRandomVector(1);
	}
	
	void regenerateRandomVector(int dim) {
		std::mt19937 g(static_cast<uint32_t>(time(0)));
		if(dim == 0) {
			std::shuffle(randomR.begin(), randomR.end(), g);
		} else {
			std::shuffle(randomA.begin(), randomA.end(), g);
		}
	}
	
	void recompute() {
		vector<float> output(width * height);
		
		for(int y = 0; y < height; y++) {
			for(int x = 0; x < width; x++) {
				int index = y * width + x;
				
				// Convert pixel coordinates to normalized coordinates
				float normX = (float)x / (float)(width - 1);
				float normY = (float)y / (float)(height - 1);
				
				// Convert to polar coordinates relative to center
				float dx = normX - xCenter;
				float dy = normY - yCenter;
				
				float radius = sqrt(dx * dx + dy * dy);
				float angle = atan2(dy, dx);
				
				// Normalize angle to [0, 1] range
				angle = (angle + M_PI) / (2.0f * M_PI);
				
				// Map to resolution indices
				float rIndex = radius * (radiusResolution - 1);
				float aIndex = angle * (angleResolution - 1);
				
				// Apply indexer math to both dimensions
				float rValue = computeIndexerValue(rIndex, 0); // radius dimension
				float aValue = computeIndexerValue(aIndex, 1); // angle dimension
				
				// Combine results (simple average for now)
				float finalValue = (rValue + aValue) * 0.5f;
				
				output[index] = finalValue;
			}
		}
		
		indexsOut = output;
	}
	
	float computeIndexerValue(float index, int dimension) {
		if(radiusResolution == 1 && dimension == 0) return 0.0f;
		if(angleResolution == 1 && dimension == 1) return 0.0f;
		
		int maxRes = (dimension == 0) ? radiusResolution : angleResolution;
		
		// QUANTIZE
		int validQuant = std::min(maxRes, indexQuantization[dimension].get());
		int newNumOfPixels = validQuant < 1 ? 1 : validQuant;
		
		int idx = (int)(index / ((float)maxRes / (float)newNumOfPixels));
		
		// SYMMETRY
		int sym = indexSymmetry[dimension];
		while(sym > newNumOfPixels - 1) sym--;
		
		bool odd = false;
		if((int)((idx) / (newNumOfPixels / (sym + 1))) % 2 == 1) odd = true;
		
		int veusSym = newNumOfPixels / (sym + 1);
		idx = veusSym - abs((((int)(idx / veusSym) % 2) * veusSym) - (idx % veusSym));
		
		if(newNumOfPixels % 2 == 0) {
			idx += odd ? 1 : 0;
		} else if(sym > 0) {
			idx += indexInvert[dimension] > 0.5f ? 0 : 1;
			idx %= newNumOfPixels;
		}
		
		// COMBINATION
		idx = std::abs(((idx % 2) * maxRes * indexCombination[dimension]) - idx);
		
		// RANDOM
		double indexf = idx;
		if(indexRandom[dimension] > 0) {
			const vector<int>& randomVec = (dimension == 0) ? randomR : randomA;
			int randomIdx = std::min((int)idx, (int)randomVec.size() - 1);
			indexf = double(idx) * (1 - indexRandom[dimension]) +
					 (double(randomVec[randomIdx]) * indexRandom[dimension]);
		}
		
		// INVERSE
		double nonInvertIndex = indexf;
		double invertedIndex = ((double)newNumOfPixels / double(sym + 1)) - indexf - 1;
		indexf = indexInvert[dimension] * invertedIndex + (1 - indexInvert[dimension]) * nonInvertIndex;
		
		// OFFSET
		indexf += indexOffset[dimension];
		
		// MODULO
		if(indexModulo[dimension] != maxRes) {
			indexf = std::fmod(indexf, indexModulo[dimension]);
		}
		
		// NORMALIZE
		if(discrete) {
			return indexf;
		} else {
			int toDivide = normalize ? maxRes - 1 : maxRes;
			if(!normalize) indexf += 0.5f;
			
			float value = float(((indexf) / (double)(toDivide)) *
							  ((double)numWaves[dimension] * ((double)maxRes / (double)newNumOfPixels) *
							   ((double)sym + 1)));
			
			if(value > 1) {
				int trunc = std::trunc(value);
				value -= (trunc == value) ? trunc - 1 : trunc;
			}
			
			return ofClamp(value, 0.0f, 1.0f);
		}
	}
};

#endif /* radialIndexer_h */
