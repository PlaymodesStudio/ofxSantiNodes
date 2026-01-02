//
//  circularCrossfade.h
//  ofxOceanode
//
//  Created by Santi Vilanova
//

#ifndef circularCrossfade_h
#define circularCrossfade_h

#include "ofxOceanodeNodeModel.h"

class circularCrossfade : public ofxOceanodeNodeModel{
public:
	circularCrossfade() : ofxOceanodeNodeModel("Circular Crossfade"){};
	
	void setup(){
		color = ofColor(0, 200, 255);
		description = "Crossfades the end of a vector to its beginning for seamless looping";
		
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(crossfadePercentage.set("Crossfade %", 0.1, 0, 1));
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		listeners.push(input.newListener([this](vector<float> &vf){
			computeCrossfade();
		}));
		
		listeners.push(crossfadePercentage.newListener([this](float &f){
			computeCrossfade();
		}));
	}
	
	void computeCrossfade(){
		const vector<float>& inputVec = input.get();
		int size = inputVec.size();
		
		if(size == 0){
			output = vector<float>();
			return;
		}
		
		if(size == 1){
			output = inputVec;
			return;
		}
		
		vector<float> tempOut = inputVec; // Copy input
		
		int crossfadeSize = crossfadePercentage * size;
		
		if(crossfadeSize > 0){
			float startValue = inputVec[0];
			float endValue = inputVec[size - 1];
			
			for(int i = 0; i < crossfadeSize && i < size; i++){
				int index = size - crossfadeSize + i;
				float interpolation = float(i) / float(crossfadeSize);
				
				float currentValue = inputVec[index];
				float loopedValue = currentValue - endValue + startValue;
				
				tempOut[index] = smoothInterpolate(currentValue, loopedValue, interpolation);
			}
		}
		
		output = tempOut;
	}
	
private:
	float smoothInterpolate(float start, float end, float pos){
		float oldRandom = start;
		float pastRandom = start;
		float newRandom = end;
		float futureRandom = end;
		
		float L0 = (newRandom - pastRandom) * 0.5;
		float L1 = L0 + (oldRandom - newRandom);
		float L2 = L1 + ((futureRandom - oldRandom) * 0.5) + (oldRandom - newRandom);
		
		return oldRandom + (pos * (L0 + (pos * ((pos * L2) - (L1 + L2)))));
	}
	
	ofParameter<vector<float>> input;
	ofParameter<float> crossfadePercentage;
	ofParameter<vector<float>> output;
	
	ofEventListeners listeners;
};

#endif /* circularCrossfade_h */
