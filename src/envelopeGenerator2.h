#ifndef envelopeGenerator2_h
#define envelopeGenerator2_h

#include "ofxOceanodeNodeModel.h"

enum envelopeStages2 {
	envelopeAttack2 = 0,
	envelopeDecay2 = 1,
	envelopeSustain2 = 2,
	envelopeRelease2 = 3,
	envelopeEnd2 = 4
};

class envelopeGenerator2 : public ofxOceanodeNodeModel {
public:
	envelopeGenerator2();
	~envelopeGenerator2(){};

private:
	float getValueForIndex(const vector<float> &vf, int i) {
		if(i < vf.size()) {
			return vf[i];
		} else {
			return vf[0];
		}
	}

	void customPow(float & value, float pow);
	float smoothinterpolate(float start, float end, float pos);
	void phasorListener(vector<float> &vf);
	void gateInChanged(vector<float> &vf);
	void recalculatePreviewCurve();

	ofEventListener listener;

	ofParameter<vector<float>> phasor;
	ofParameter<vector<float>> hold;
	ofParameter<vector<float>> attack;
	ofParameter<vector<float>> decay;
	ofParameter<vector<float>> sustain;
	ofParameter<vector<float>> release;

	ofParameter<vector<float>> attackPow;
	ofParameter<vector<float>> decayPow;
	ofParameter<vector<float>> releasePow;
	ofParameter<vector<float>> attackBiPow;
	ofParameter<vector<float>> decayBiPow;
	ofParameter<vector<float>> releaseBiPow;

	ofParameter<vector<float>> curvePreview;
	ofEventListeners curvePreviewListeners;

	ofParameter<vector<float>> gateIn;
	ofParameter<vector<float>> output;

	vector<float> lastInput;           // Previous gate input values
	vector<float> phasorValueOnValueChange; // Phasor value when envelope stage changes
	vector<float> lastPhase;          // Previous phase values
	vector<bool> reachedMax;          // Track if envelope reached max value
	vector<int> envelopeStage;        // Current stage of each envelope
	vector<float> maxValue;           // Target amplitude for each envelope
	vector<float> initialPhase;       // Initial phase when gate triggers
	vector<float> lastSustainValue;   // Last sustain level (for release)
	vector<float> targetValue;        // Current target value based on gate

	vector<string> easeStringFuncs;

	vector<float> oldGateIn;
	vector<float> outputComputeVec;

	int lastGateInSize;
	
	float gateThreshold;  // Threshold for considering a gate "off"
};

#endif /* envelopeGenerator2_h */
