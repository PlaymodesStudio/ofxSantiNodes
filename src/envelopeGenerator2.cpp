#include "envelopeGenerator2.h"

envelopeGenerator2::envelopeGenerator2() : ofxOceanodeNodeModel("Envelope Generator2")
{
	easeStringFuncs = {"EASE_LINEAR", "EASE_IN_QUAD", "EASE_OUT_QUAD", "EASE_IN_OUT_QUAD",
					   "EASE_IN_CUBIC", "EASE_OUT_CUBIC", "EASE_IN_OUT_CUBIC", "EASE_IN_QUART",
					   "EASE_OUT_QUART", "EASE_IN_OUT_QUART", "EASE_IN_QUINT", "EASE_OUT_QUINT",
					   "EASE_IN_OUT_QUINT", "EASE_IN_SINE", "EASE_OUT_SINE", "EASE_IN_OUT_SINE",
					   "EASE_IN_EXPO", "EASE_OUT_EXPO", "EASE_IN_OUT_EXPO", "EASE_IN_CIRC",
					   "EASE_OUT_CIRC", "EASE_IN_OUT_CIRC"};

	addParameter(phasor.set("Phase", {0}, {0}, {1}));
	addParameter(gateIn.set("GateIn", {0}, {0}, {1}));

	addParameter(hold.set("Hold", {0}, {0}, {1}));
	addParameter(attack.set("A", {0}, {0}, {1}));
	addParameter(decay.set("D", {0}, {0}, {1}));
	addParameter(sustain.set("S", {1}, {0}, {1}));
	addParameter(release.set("R", {0}, {0}, {1}));

	addParameter(attackPow.set("A.Pow", {0}, {-1}, {1}));
	addParameter(attackBiPow.set("A.BiPow", {0}, {-1}, {1}));
	addParameter(decayPow.set("D.Pow", {0}, {-1}, {1}));
	addParameter(decayBiPow.set("D.BiPow", {0}, {-1}, {1}));
	addParameter(releasePow.set("R.Pow", {0}, {-1}, {1}));
	addParameter(releaseBiPow.set("R.BiPow", {0}, {-1}, {1}));

	addParameter(polyMode.set("Poly", false));
	addParameter(clipOut.set("ClipOut", false));

	addParameter(curvePreview.set("Curve", {0}, {0}, {1}));
	addOutputParameter(output.set("Output", {0}, {0}, {1}));

	oldGateIn = {0};
	outputComputeVec = {0};
	gateThreshold = 0.001f;

	listener     = phasor.newListener(this, &envelopeGenerator2::phasorListener);
	gateListener = gateIn.newListener(this, &envelopeGenerator2::gateInListener);

	curvePreviewListeners.push(hold.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));
	curvePreviewListeners.push(attack.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));
	curvePreviewListeners.push(decay.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));
	curvePreviewListeners.push(sustain.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));
	curvePreviewListeners.push(release.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));
	curvePreviewListeners.push(attackPow.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));
	curvePreviewListeners.push(attackBiPow.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));
	curvePreviewListeners.push(decayPow.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));
	curvePreviewListeners.push(decayBiPow.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));
	curvePreviewListeners.push(releasePow.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));
	curvePreviewListeners.push(releaseBiPow.newListener([this](vector<float> &vf){recalculatePreviewCurve();}));

	recalculatePreviewCurve();
	lastGateInSize = -1;
}

// ---------------------------------------------------------------------------
// computeVoice
// ---------------------------------------------------------------------------
bool envelopeGenerator2::computeVoice(EnvelopeVoice &v, float f, int i, float &outSample) {
	float deltaPhase = f - v.lastPhasorForHold;
	
	// Robust anti-jitter for the phasor wrap
	if(deltaPhase < -0.5f) {
		deltaPhase += 1.0f; // Legitimate wrap around
	} else if(deltaPhase < 0.0f) {
		deltaPhase = 0.0f;  // Minor phasor jitter backward, discard
	}
	v.lastPhasorForHold = f;

	v.accumulatedHold += deltaPhase;
	v.stagePhase += deltaPhase;

	// Hold timeout
	if(getValueForIndex(hold, i) > 0) {
		if(v.accumulatedHold >= getValueForIndex(hold, i) &&
		   v.stage != envelopeRelease2 &&
		   v.stage != envelopeEnd2) {
			v.stage = (getValueForIndex(release, i) > 0) ? envelopeRelease2 : envelopeEnd2;
			v.stagePhase = 0; // Reset for release
		}
	}

	switch(v.stage) {
		case envelopeAttack2: {
			float aTime = getValueForIndex(attack, i);
			if(v.stagePhase >= aTime) {
				v.stagePhase -= aTime; // Retain mathematical overshoot
				if(getValueForIndex(decay, i) == 0) {
					v.stage = envelopeSustain2;
					outSample = v.maxValue * getValueForIndex(sustain, i);
				} else {
					v.stage = envelopeDecay2;
					outSample = v.maxValue;
				}
				v.lastSustainValue = outSample;
			} else {
				float p = (aTime > 0) ? (v.stagePhase / aTime) : 1.0f;
				if(getValueForIndex(attackPow, i) != 0)    customPow(p, getValueForIndex(attackPow, i));
				if(getValueForIndex(attackBiPow, i) != 0) {
					p = (p * 2) - 1;
					customPow(p, getValueForIndex(attackBiPow, i));
					p = (p + 1) / 2.0f;
				}
				outSample = smoothinterpolate(0, v.maxValue, p);
				if(p != 0) v.lastSustainValue = outSample;
			}
			break;
		}

		case envelopeDecay2: {
			float dTime = getValueForIndex(decay, i);
			if(v.stagePhase >= dTime) {
				v.stagePhase -= dTime;
				v.stage = envelopeSustain2;
				outSample = v.maxValue * getValueForIndex(sustain, i);
				v.lastSustainValue = outSample;
			} else {
				float p = (dTime > 0) ? (v.stagePhase / dTime) : 1.0f;
				if(getValueForIndex(decayPow, i) != 0)    customPow(p, getValueForIndex(decayPow, i));
				if(getValueForIndex(decayBiPow, i) != 0) {
					p = (p * 2) - 1;
					customPow(p, getValueForIndex(decayBiPow, i));
					p = (p + 1) / 2.0f;
				}
				outSample = smoothinterpolate(v.maxValue, v.maxValue * getValueForIndex(sustain, i), p);
				v.lastSustainValue = outSample;
			}
			break;
		}

		case envelopeSustain2: {
			outSample = v.maxValue * getValueForIndex(sustain, i);
			v.lastSustainValue = outSample;
			break;
		}

		case envelopeRelease2: {
			float rTime = getValueForIndex(release, i);
			if(v.stagePhase >= rTime) {
				v.stage = envelopeEnd2;
				outSample = 0;
				return false; // voice ended
			} else {
				float p = (rTime > 0) ? (v.stagePhase / rTime) : 1.0f;
				if(getValueForIndex(releasePow, i) != 0)    customPow(p, getValueForIndex(releasePow, i));
				if(getValueForIndex(releaseBiPow, i) != 0) {
					p = (p * 2) - 1;
					customPow(p, getValueForIndex(releaseBiPow, i));
					p = (p + 1) / 2.0f;
				}
				outSample = smoothinterpolate(v.lastSustainValue, 0, p);
			}
			break;
		}

		case envelopeEnd2:
		default:
			outSample = 0;
			return false;
	}

	return true;
}

// ---------------------------------------------------------------------------
// phasorListener
// ---------------------------------------------------------------------------
void envelopeGenerator2::phasorListener(vector<float> &vf) {
	int inputSize = gateIn.get().size();

	if(inputSize != lastGateInSize) {
		output = vector<float>(inputSize, 0);
		lastInput = vector<float>(inputSize, 0);
		envelopeStage = vector<int>(inputSize, envelopeEnd2);
		maxValue = vector<float>(inputSize, 0);
		lastSustainValue = vector<float>(inputSize, 0);
		targetValue = vector<float>(inputSize, 0);
		
		accumulatedHoldVec   = vector<float>(inputSize, 0);
		lastPhasorForHoldVec = vector<float>(inputSize, 0);
		stagePhaseVec        = vector<float>(inputSize, 0);

		voices            = vector<vector<EnvelopeVoice>>(inputSize);
		pendingOnsets     = vector<vector<float>>(inputSize);
		pendingRelease    = vector<bool>(inputSize, false);
		lastGate          = vector<float>(inputSize, 0);
		monoNewGateEvent  = vector<bool>(inputSize, false);
		lastGateInSize    = inputSize;
	}

	vector<float> tempOutput(inputSize, 0);

	if(polyMode.get()) {
		for(int i = 0; i < inputSize; i++) {
			float f = getValueForIndex(vf, i);

			// Consume pending release FIRST to avoid killing newly spawned voices in the same frame
			if(pendingRelease[i]) {
				for(auto &v : voices[i]) {
					if(v.gated) {
						v.gated = false;
						if(getValueForIndex(hold, i) == 0 &&
						  (v.stage == envelopeAttack2 || v.stage == envelopeDecay2 || v.stage == envelopeSustain2)) {
							v.stagePhase = 0;
							v.stage = (getValueForIndex(release, i) > 0) ? envelopeRelease2 : envelopeEnd2;
						}
					}
				}
				pendingRelease[i] = false;
			}

			// Spawn new voices after releasing old ones
			for(float gateAmp : pendingOnsets[i]) {
				EnvelopeVoice v;
				v.maxValue            = gateAmp;
				v.lastSustainValue    = gateAmp;
				v.gated               = true;
				
				v.accumulatedHold     = 0;
				v.lastPhasorForHold   = f;
				v.stagePhase          = 0;

				if(getValueForIndex(attack, i) == 0) {
					v.stage = (getValueForIndex(decay, i) == 0) ? envelopeSustain2 : envelopeDecay2;
				} else {
					v.stage = envelopeAttack2;
				}
				voices[i].push_back(v);
			}
			pendingOnsets[i].clear();

			float sum = 0;
			auto it = voices[i].begin();
			while(it != voices[i].end()) {
				float sample = 0;
				bool alive = computeVoice(*it, f, i, sample);
				sum += sample;
				if(!alive) {
					it = voices[i].erase(it);
				} else {
					++it;
				}
			}
			tempOutput[i] = sum;
		}

	} else {
		for(int i = 0; i < inputSize; i++) {
			float currentGate = gateIn.get()[i];
			float f = getValueForIndex(vf, i);

			bool wasOff = lastInput[i] <= gateThreshold;
			bool isOn   = currentGate > gateThreshold;

			// Consume the new-gate event flag set by gateInListener
			bool newEvt = (i < (int)monoNewGateEvent.size()) && monoNewGateEvent[i];
			if(newEvt) monoNewGateEvent[i] = false;

			bool doOnset = false;
			if(wasOff && isOn) {
				doOnset = true;
			} else if(!wasOff && currentGate <= gateThreshold) {
				if(getValueForIndex(hold, i) == 0) {
					envelopeStage[i] = (getValueForIndex(release, i) > 0) ? envelopeRelease2 : envelopeEnd2;
					stagePhaseVec[i] = 0;
				}
			} else if(isOn && (newEvt || abs(currentGate - targetValue[i]) > gateThreshold)) {
				// New gate event (same or different value) or amplitude changed:
				// restart the envelope (equivalent to synthetic 0 → new onset).
				doOnset = true;
			}

			if(doOnset) {
				targetValue[i] = currentGate;
				maxValue[i]    = currentGate;

				if(getValueForIndex(attack, i) == 0) {
					envelopeStage[i] = (getValueForIndex(decay, i) == 0) ? envelopeSustain2 : envelopeDecay2;
				} else {
					envelopeStage[i] = envelopeAttack2;
				}

				accumulatedHoldVec[i]   = 0;
				lastPhasorForHoldVec[i] = f;
				stagePhaseVec[i]        = 0;
				lastSustainValue[i]     = currentGate;
			}

			EnvelopeVoice v;
			v.stage               = envelopeStage[i];
			v.maxValue            = maxValue[i];
			v.lastSustainValue    = lastSustainValue[i];
			v.accumulatedHold     = accumulatedHoldVec[i];
			v.lastPhasorForHold   = lastPhasorForHoldVec[i];
			v.stagePhase          = stagePhaseVec[i];

			float sample = 0;
			computeVoice(v, f, i, sample);
			tempOutput[i] = sample;

			envelopeStage[i]              = v.stage;
			maxValue[i]                   = v.maxValue;
			lastSustainValue[i]           = v.lastSustainValue;
			accumulatedHoldVec[i]         = v.accumulatedHold;
			lastPhasorForHoldVec[i]       = v.lastPhasorForHold;
			stagePhaseVec[i]              = v.stagePhase;

			lastInput[i] = currentGate;
		}
	}

	if(clipOut.get()) {
		for(int i = 0; i < inputSize; i++) {
			tempOutput[i] = ofClamp(tempOutput[i], 0.0f, 1.0f);
		}
	}

	output = tempOutput;
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
void envelopeGenerator2::customPow(float & value, float pow) {
	float k1 = 2 * pow * 0.99999f;
	float k2 = (k1 / ((-pow * 0.999999f) + 1));
	float k3 = k2 * abs(value) + 1;
	value = value * (k2 + 1) / k3;
}

float envelopeGenerator2::smoothinterpolate(float start, float end, float pos) {
	float oldRandom    = start;
	float pastRandom   = start;
	float newRandom    = end;
	float futureRandom = end;
	float L0 = (newRandom - pastRandom) * 0.5f;
	float L1 = L0 + (oldRandom - newRandom);
	float L2 = L1 + ((futureRandom - oldRandom) * 0.5f) + (oldRandom - newRandom);
	return oldRandom + (pos * (L0 + (pos * ((pos * L2) - (L1 + L2)))));
}

// ---------------------------------------------------------------------------
// gateInListener
// ---------------------------------------------------------------------------
void envelopeGenerator2::gateInListener(vector<float> &vf) {
	// Handles both poly and mono — no early return on mono.
	int sz = (int)vf.size();

	if(sz > (int)lastGate.size())          lastGate.resize(sz, 0);
	if(sz > (int)monoNewGateEvent.size())  monoNewGateEvent.resize(sz, false);
	if(sz > (int)pendingOnsets.size()) {
		pendingOnsets.resize(sz);
		pendingRelease.resize(sz, false);
	}

	for(int i = 0; i < sz; i++) {
		float cur  = vf[i];
		float prev = lastGate[i];

		bool wasOff = prev <= gateThreshold;
		bool isOn   = cur  >  gateThreshold;

		if(polyMode.get()) {
			if(wasOff && isOn) {
				// Zero → non-zero: normal onset
				pendingOnsets[i].push_back(cur);
			} else if(!wasOff && isOn) {
				// Non-zero → non-zero (same OR different value): synthetic 0 + new onset.
				// No abs check: if the listener fired the send was intentional, even at same value.
				pendingRelease[i] = true;
				pendingOnsets[i].push_back(cur);
			}
			if(!wasOff && !isOn) {
				pendingRelease[i] = true;
			}
		} else {
			// Mono: set a one-shot flag so phasorListener can retrigger even on same-value gates.
			// phasorListener reads gate continuously so it can't tell apart a held gate from a
			// deliberate same-value re-send without this out-of-band signal.
			if(isOn) monoNewGateEvent[i] = true;
		}

		lastGate[i] = cur;
	}
}

// ---------------------------------------------------------------------------
// Preview curve
// ---------------------------------------------------------------------------
void envelopeGenerator2::recalculatePreviewCurve() {
	int maxSize = 100;
	vector<float> tempOutput;
	float previewGateValue = 1.0f;

	if(attack->empty() || decay->empty() || sustain->empty() || release->empty() ||
	   attackPow->empty() || attackBiPow->empty() || decayPow->empty() ||
	   decayBiPow->empty() || releasePow->empty() || releaseBiPow->empty()) {
		return;
	}

	int attackSize = std::max(0, std::min(maxSize, static_cast<int>(attack->at(0) * maxSize)));
	tempOutput.resize(attackSize);
	for(int i = 0; i < attackSize; i++) {
		float phase = float(i) / float(attackSize);
		if(attackPow->at(0) != 0)    customPow(phase, attackPow->at(0));
		if(attackBiPow->at(0) != 0) {
			phase = (phase * 2) - 1;
			customPow(phase, attackBiPow->at(0));
			phase = (phase + 1) / 2.0f;
		}
		tempOutput[i] = smoothinterpolate(0, previewGateValue, phase);
	}

	int decaySize = std::max(0, std::min(maxSize, static_cast<int>(decay->at(0) * maxSize)));
	int decayPos = tempOutput.size();
	tempOutput.resize(decayPos + decaySize);
	for(int i = 0; i < decaySize; i++) {
		float phase = float(i) / float(decaySize);
		if(decayPow->at(0) != 0)    customPow(phase, decayPow->at(0));
		if(decayBiPow->at(0) != 0) {
			phase = (phase * 2) - 1;
			customPow(phase, decayBiPow->at(0));
			phase = (phase + 1) / 2.0f;
		}
		tempOutput[decayPos + i] = smoothinterpolate(previewGateValue, previewGateValue * sustain->at(0), phase);
	}

	int sustainSize = maxSize / 2;
	int sustainPos = tempOutput.size();
	tempOutput.resize(sustainPos + sustainSize);
	fill(tempOutput.begin() + sustainPos, tempOutput.end(), previewGateValue * sustain->at(0));

	int releaseSize = std::max(0, std::min(maxSize, static_cast<int>(release->at(0) * maxSize)));
	int releasePos = tempOutput.size();
	tempOutput.resize(releasePos + releaseSize);
	float sustainLevel = previewGateValue * sustain->at(0);
	for(int i = 0; i < releaseSize; i++) {
		float phase = float(i) / float(releaseSize);
		if(releasePow->at(0) != 0)    customPow(phase, releasePow->at(0));
		if(releaseBiPow->at(0) != 0) {
			phase = (phase * 2) - 1;
			customPow(phase, releaseBiPow->at(0));
			phase = (phase + 1) / 2.0f;
		}
		tempOutput[releasePos + i] = smoothinterpolate(sustainLevel, 0, phase);
	}

	curvePreview = tempOutput;
}


