//
//  circularValueEaser.h
//  Espills_controller
//
//  Circular Value Easer - avoids easing on wrap-around transitions
//

#ifndef circularValueEaser_h
#define circularValueEaser_h

#include "ofxOceanodeNodeModel.h"

class circularValueEaser : public ofxOceanodeNodeModel{
public:
	circularValueEaser() : ofxOceanodeNodeModel("Circular Value Easer"){
		addParameter(phasor.set("Phase", {0}, {0}, {1}));
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(pow.set("Pow", {0}, {-1}, {1}));
		addParameter(bipow.set("BiPow", {0}, {-1}, {1}));
		addParameter(shortestPath.set("Shortest Path", false));
		addParameter(wrapThreshold.set("Wrap Threshold", 0.1, 0.0, 0.5));
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		
		color = ofColor::green;
		
		listener = phasor.newListener([this](vector<float> &vf){
			if(input.get().size() != lastInput.size() || input.get().size() != phasorValueOnValueChange.size()){
				output = input;
				lastInput = input;
				lastChangedValue = input;
				phasorValueOnValueChange = (vf.size() == 1) ? vector<float>(input.get().size(), vf[0]) : vf;
				lastPhase = vector<float>(input.get().size(), 0);
				reachedMax = vector<bool>(input.get().size(), false);
			}else{
				int inputSize = input.get().size();
				for(int i = 0; i < inputSize; i++){
					if(lastInput[i] != input.get()[i]){
						phasorValueOnValueChange[i] = (vf.size() < inputSize) ? vf[0] : vf[i];
						reachedMax[i] = false;
						lastPhase[i] = 0;
						lastChangedValue[i] = output.get()[i];
					}
				}
				vector<float> tempOutput = input.get();
				for(int i = 0; i < inputSize; i++){
					float phase = ((vf.size() < inputSize) ? vf[0] : vf[i]) - phasorValueOnValueChange[i];
					if(phase < 0) phase = 1+phase;
					
					// Check if this is a wrap-around transition
					bool isWrapTransition = detectWrapTransition(lastChangedValue[i], input.get()[i]);
					
					if(isWrapTransition){
						// Skip easing for wrap-around transitions
						tempOutput[i] = input.get()[i];
						reachedMax[i] = true;
					}else{
						// Apply normal easing
						if(getValueForPosition(pow, i) != 0){
							customPow(phase, getValueForPosition(pow, i));
						}
						if(getValueForPosition(bipow, i) != 0){
							phase = (phase*2) - 1;
							customPow(phase, getValueForPosition(bipow, i));
							phase = (phase + 1) / 2.0;
						}
						if(phase < lastPhase[i]) reachedMax[i] = true;
						else lastPhase[i] = phase;
						
						if(!reachedMax[i]){
							if(shortestPath && abs(lastChangedValue[i] - input.get()[i]) > 0.5){
								if(lastChangedValue[i] > input.get()[i]){
									tempOutput[i] = fmod(smoothinterpolate(lastChangedValue[i], input.get()[i]+1, phase), 1);
								}else{
									tempOutput[i] = fmod(smoothinterpolate(lastChangedValue[i]+1, input.get()[i], phase), 1);
								}
							}else{
								tempOutput[i] = smoothinterpolate(lastChangedValue[i], input.get()[i], phase);
							}
						}else{
							tempOutput[i] = input.get()[i];
						}
					}
				}
				lastInput = input;
				output = tempOutput;
			}
		});
	}
	
	~circularValueEaser(){};
	
private:
	bool detectWrapTransition(float from, float to){
		float threshold = wrapThreshold.get();
		float lowerBound = threshold;
		float upperBound = 1.0 - threshold;
		
		// Check if transitioning from high (>=upperBound) to low (<=lowerBound)
		bool highToLow = (from >= upperBound) && (to <= lowerBound);
		
		// Check if transitioning from low (<=lowerBound) to high (>=upperBound)
		bool lowToHigh = (from <= lowerBound) && (to >= upperBound);
		
		return highToLow || lowToHigh;
	}
	
	void customPow(float & value, float pow){
		float k1 = 2*pow*0.99999;
		float k2 = (k1/((-pow*0.999999)+1));
		float k3 = k2 * abs(value) + 1;
		value = value * (k2+1) / k3;
	}
	
	float smoothinterpolate(float start, float end, float pos){
		float oldRandom = start;
		float pastRandom = start;
		float newRandom = end;
		float futureRandom  = end;
		float L0 = (newRandom - pastRandom) * 0.5;
		float L1 = L0 + (oldRandom-newRandom);
		float L2 = L1 + ((futureRandom - oldRandom)*0.5) + (oldRandom - newRandom);
		return oldRandom + (pos * (L0 + (pos * ((pos * L2) - (L1 + L2)))));
	}
	
	auto getValueForPosition(const vector<float> &param, int index) -> float{
		if(param.size() == 1 || param.size() <= index){
			return param[0];
		}
		else{
			return param[index];
		}
	};
	
	ofParameter<vector<float>> input;
	ofParameter<vector<float>> phasor;
	ofParameter<vector<float>> pow;
	ofParameter<vector<float>> bipow;
	ofParameter<vector<float>> output;
	ofParameter<bool> shortestPath;
	ofParameter<float> wrapThreshold;
	
	ofEventListener listener;
	vector<float> lastInput;
	vector<float> lastChangedValue;
	vector<float> phasorValueOnValueChange;
	vector<float> lastPhase;
	vector<bool> reachedMax;
};

#endif /* circularValueEaser_h */
