
#ifndef vectorOfVectorIndexedSampler_h
#define vectorOfVectorIndexedSampler_h

#include "ofxOceanodeNodeModel.h"
#include <vector>

class vectorOfVectorIndexedSampler : public ofxOceanodeNodeModel{
public:
	vectorOfVectorIndexedSampler() : ofxOceanodeNodeModel("VV Indexed Sampler"){
		addParameter(input.set("VV Input", {{0}}, {{-INT_MAX}}, {{INT_MAX}}));
		addParameter(vectorIndex.set("Vec Idx", {0}, {0}, {INT_MAX}));
		addParameter(elementIndex.set("Elem Idx", {0}, {0}, {INT_MAX}));
		addOutputParameter(output.set("Output", {0}, {-INT_MAX}, {INT_MAX}));
		
		inputListener = input.newListener([this](vector<vector<int>> &v){
			computeOutput();
		});
		
		vectorIndexListener = vectorIndex.newListener([this](vector<int> &idx){
			computeOutput();
		});
		
		elementIndexListener = elementIndex.newListener([this](vector<int> &idx){
			computeOutput();
		});
	};

	~vectorOfVectorIndexedSampler(){};

private:
	void computeOutput(){
		auto inputVV = input.get();
		auto vecIdx = vectorIndex.get();
		auto elemIdx = elementIndex.get();
		
		// Determine output size based on the maximum length of the two index vectors
		size_t outputSize = std::max(vecIdx.size(), elemIdx.size());
		vector<int> result(outputSize, 0);
		
		for(size_t i = 0; i < outputSize; i++){
			// Wrap indices if one vector is shorter than the other
			int currentVecIdx = vecIdx[i % vecIdx.size()];
			int currentElemIdx = elemIdx[i % elemIdx.size()];
			
			// Bounds checking
			if(currentVecIdx >= 0 && currentVecIdx < inputVV.size()){
				const auto& selectedVector = inputVV[currentVecIdx];
				if(currentElemIdx >= 0 && currentElemIdx < selectedVector.size()){
					result[i] = selectedVector[currentElemIdx];
				}
				// If element index out of bounds, result[i] stays 0
			}
			// If vector index out of bounds, result[i] stays 0
		}
		
		output = result;
	}

	ofEventListener inputListener;
	ofEventListener vectorIndexListener;
	ofEventListener elementIndexListener;

	ofParameter<vector<vector<int>>> input;
	ofParameter<vector<int>> vectorIndex;
	ofParameter<vector<int>> elementIndex;
	ofParameter<vector<int>> output;
};

#endif /* vectorOfVectorIndexedSampler_h */
