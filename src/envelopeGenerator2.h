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
	float phasorValueOnChange = 0;
	float lastPhase = 0;
	bool  reachedMax = false;
	float lastSustainValue = 0;
	float initialPhase = 0;
	bool  gated = true;   // true while the originating gate is still high
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
	void gateInListener(vector<float> &vf);   // records onsets/releases between phasor ticks
	void recalculatePreviewCurve();

	// Compute one envelope voice for a given phasor value and index.
	// Returns the output sample and updates the voice state.
	// Returns false if the voice has ended (caller should remove it).
	bool computeVoice(EnvelopeVoice &v, float phasorValue, int i, float &outSample);

	ofEventListener listener;
	ofEventListener gateListener;  // catches gate onsets between phasor ticks

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

	// Mono-mode per-index state
	vector<float> lastInput;
	vector<float> phasorValueOnValueChange;
	vector<float> lastPhase;
	vector<bool>  reachedMax;
	vector<int>   envelopeStage;
	vector<float> maxValue;
	vector<float> initialPhase;
	vector<float> lastSustainValue;
	vector<float> targetValue;

	// Poly-mode per-index voice lists
	// voices[i] is the list of active envelopes for index i.
	vector<vector<EnvelopeVoice>> voices;

	// lastGate is used exclusively by gateInListener for edge detection.
	// It is separate from lastInput (which is owned by the mono phasorListener
	// path) so the two paths don't interfere with each other.
	vector<float> lastGate;

	// pendingOnsets[i] holds gate amplitudes for gate-on events detected
	// by the gateListener that have not yet been consumed by phasorListener.
	// This ensures fast trigger pulses (shorter than one phasor tick) are
	// never missed even if gateIn already returned to 0 by the time the
	// phasor fires.
	vector<vector<float>> pendingOnsets;

	// pendingRelease[i]: true if a gate-off was detected since the last
	// phasor tick and all gated voices should be released.
	vector<bool> pendingRelease;

	vector<string> easeStringFuncs;

	vector<float> oldGateIn;
	vector<float> outputComputeVec;

	int lastGateInSize;
	float gateThreshold;
};

#endif /* envelopeGenerator2_h */
