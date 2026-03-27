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

// Per-voice envelope state used in poly mode
struct EnvelopeVoice {
	int   stage = envelopeEnd2;
	float maxValue = 0;
	float lastSustainValue = 0;
	bool  gated = true;   // true while the originating gate is still high

	float accumulatedHold = 0;
	float lastPhasorForHold = 0;
	
	// Pure continuous accumulator to preserve mathematical timing
	float stagePhase = 0;
};

class envelopeGenerator2 : public ofxOceanodeNodeModel {
public:
	envelopeGenerator2();
	~envelopeGenerator2(){};

private:
	float getValueForIndex(const vector<float> &vf, int i) {
		if(i < (int)vf.size()) {
			return vf[i];
		} else {
			return vf[0];
		}
	}

	void customPow(float & value, float pow);
	float smoothinterpolate(float start, float end, float pos);
	void phasorListener(vector<float> &vf);
	void gateInListener(vector<float> &vf);
	void recalculatePreviewCurve();

	bool computeVoice(EnvelopeVoice &v, float phasorValue, int i, float &outSample);

	ofEventListener listener;
	ofEventListener gateListener;

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
	ofParameter<bool>          polyMode;
	ofParameter<bool>          clipOut;

	// Mono-mode per-index state
	vector<float> lastInput;
	vector<int>   envelopeStage;
	vector<float> maxValue;
	vector<float> lastSustainValue;
	vector<float> targetValue;
	
	// Phase tracking for mono-mode
	vector<float> accumulatedHoldVec;
	vector<float> lastPhasorForHoldVec;
	vector<float> stagePhaseVec;

	// Poly-mode per-index voice lists
	vector<vector<EnvelopeVoice>> voices;

	vector<float> lastGate;
	vector<vector<float>> pendingOnsets;
	vector<bool> pendingRelease;

	vector<string> easeStringFuncs;

	vector<float> oldGateIn;
	vector<float> outputComputeVec;

	// Set by gateInListener in mono mode; consumed once per phasor tick.
	// Lets phasorListener detect retriggering even when the new gate value
	// equals the previous one (e.g. two consecutive {1,1} notes).
	vector<bool> monoNewGateEvent;

	int lastGateInSize;
	float gateThreshold;
};

#endif /* envelopeGenerator2_h */
