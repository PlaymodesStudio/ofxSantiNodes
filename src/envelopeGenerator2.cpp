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

	// Hold: auto-releases the envelope this many phasor-periods after gate onset,
	// regardless of whether the gate is still high. 0 = disabled (gate controls release).
	// Note: this is NOT an AHDSR "hold" stage between A and D — it caps the total
	// envelope duration from the moment the gate opens.
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

	// Poly mode: when enabled, a new gate onset at an already-active index does NOT
	// kill the ongoing envelope. Instead a new envelope voice is spawned and its output
	// is summed with any currently-running voices for that index.
	addParameter(polyMode.set("Poly", false));

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
// computeVoice: advances one EnvelopeVoice by one phasor frame.
// Returns true while the voice is still alive, false when it has ended.
// ---------------------------------------------------------------------------
bool envelopeGenerator2::computeVoice(EnvelopeVoice &v, float f, int i, float &outSample) {
	float phase = f - v.phasorValueOnChange;
	if(phase < 0) phase = 1 + phase;
	if(phase < v.lastPhase) v.reachedMax = true;
	else v.lastPhase = phase;

	// Hold timeout: auto-release after a fixed phasor duration from gate onset
	if(getValueForIndex(hold, i) > 0) {
		float holdPhase = f - v.initialPhase;
		if(holdPhase < 0) holdPhase += 1;

		if(holdPhase > getValueForIndex(hold, i) &&
		   v.stage != envelopeRelease2 &&
		   v.stage != envelopeEnd2) {
			v.phasorValueOnChange = f;
			v.stage = (getValueForIndex(release, i) > 0) ? envelopeRelease2 : envelopeEnd2;
			v.reachedMax = false;
			v.lastPhase = 0;
			phase = 0;
		}
	}

	switch(v.stage) {
		case envelopeAttack2: {
			if(phase > getValueForIndex(attack, i) || v.reachedMax) {
				v.phasorValueOnChange = f;
				v.reachedMax = false;
				v.lastPhase = 0;
				if(getValueForIndex(decay, i) == 0) {
					v.stage = envelopeSustain2;
					outSample = v.maxValue * getValueForIndex(sustain, i);
					v.lastSustainValue = outSample;
				} else {
					v.stage = envelopeDecay2;
					outSample = v.maxValue;
					v.lastSustainValue = outSample;
				}
			} else {
				float p = ofMap(phase, 0, getValueForIndex(attack, i), 0, 1, true);
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
			if(phase > getValueForIndex(decay, i) || v.reachedMax) {
				v.phasorValueOnChange = f;
				v.stage = envelopeSustain2;
				v.reachedMax = false;
				v.lastPhase = 0;
				outSample = v.maxValue * getValueForIndex(sustain, i);
				v.lastSustainValue = outSample;
			} else {
				float p = ofMap(phase, 0, getValueForIndex(decay, i), 0, 1, true);
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
			if(phase > getValueForIndex(release, i) || v.reachedMax) {
				v.phasorValueOnChange = f;
				v.stage = envelopeEnd2;
				v.reachedMax = false;
				v.lastPhase = 0;
				outSample = 0;
				return false; // voice ended
			} else {
				float p = ofMap(phase, 0, getValueForIndex(release, i), 0, 1, true);
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
// phasorListener — called every frame by the phasor
// ---------------------------------------------------------------------------
void envelopeGenerator2::phasorListener(vector<float> &vf) {
	int inputSize = gateIn.get().size();

	if(inputSize != lastGateInSize) {
		output = vector<float>(inputSize, 0);
		lastInput = vector<float>(inputSize, 0);
		phasorValueOnValueChange = vector<float>(inputSize, 0);
		lastPhase = vector<float>(inputSize, 0);
		reachedMax = vector<bool>(inputSize, false);
		envelopeStage = vector<int>(inputSize, envelopeEnd2);
		maxValue = vector<float>(inputSize, 0);
		initialPhase = vector<float>(inputSize, 0);
		lastSustainValue = vector<float>(inputSize, 0);
		targetValue = vector<float>(inputSize, 0);
		voices         = vector<vector<EnvelopeVoice>>(inputSize);
		pendingOnsets  = vector<vector<float>>(inputSize);
		pendingRelease = vector<bool>(inputSize, false);
		lastGate       = vector<float>(inputSize, 0);
		lastGateInSize = inputSize;
	}

	vector<float> tempOutput(inputSize, 0);

	if(polyMode.get()) {
		// ---------------------------------------------------------------
		// POLY MODE: multiple simultaneous voices per index, summed.
		//
		// Gate onsets and releases are captured by gateInListener()
		// (which fires on every gateIn parameter change) and stored in
		// pendingOnsets / pendingRelease.  This way a trigger pulse that
		// rises and falls entirely between two phasor ticks is never lost.
		// ---------------------------------------------------------------
		for(int i = 0; i < inputSize; i++) {
			float f = getValueForIndex(vf, i);

			// --- consume pending onsets: spawn one new voice per onset ---
			for(float gateAmp : pendingOnsets[i]) {
				EnvelopeVoice v;
				v.maxValue            = gateAmp;
				v.phasorValueOnChange = f;
				v.initialPhase        = f;
				v.lastPhase           = 0;
				v.reachedMax          = false;
				v.lastSustainValue    = gateAmp;
				v.gated               = true;

				if(getValueForIndex(attack, i) == 0) {
					v.stage = (getValueForIndex(decay, i) == 0) ? envelopeSustain2 : envelopeDecay2;
				} else {
					v.stage = envelopeAttack2;
				}
				voices[i].push_back(v);
			}
			pendingOnsets[i].clear();

			// --- consume pending release: send gated voices to release ---
			if(pendingRelease[i]) {
				for(auto &v : voices[i]) {
					if(v.gated && (v.stage == envelopeAttack2 ||
					               v.stage == envelopeDecay2  ||
					               v.stage == envelopeSustain2)) {
						v.phasorValueOnChange = f;
						v.reachedMax = false;
						v.lastPhase  = 0;
						v.gated      = false;
						v.stage = (getValueForIndex(release, i) > 0)
						          ? envelopeRelease2 : envelopeEnd2;
					}
				}
				pendingRelease[i] = false;
			}

			// --- advance all voices and sum ---
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
			tempOutput[i] = ofClamp(sum, 0, 1);
		}

	} else {
		// ---------------------------------------------------------------
		// MONO MODE: original single-envelope behaviour
		// ---------------------------------------------------------------
		for(int i = 0; i < inputSize; i++) {
			float currentGate = gateIn.get()[i];
			float f = getValueForIndex(vf, i);

			bool wasOff = lastInput[i] <= gateThreshold;
			bool isOn   = currentGate > gateThreshold;

			// New gate onset — restart envelope from scratch
			if(wasOff && isOn) {
				targetValue[i] = currentGate;
				maxValue[i]    = currentGate;

				if(getValueForIndex(attack, i) == 0) {
					envelopeStage[i] = (getValueForIndex(decay, i) == 0)
					                   ? envelopeSustain2 : envelopeDecay2;
				} else {
					envelopeStage[i] = envelopeAttack2;
				}

				phasorValueOnValueChange[i] = f;
				reachedMax[i] = false;
				lastPhase[i]  = 0;
				initialPhase[i] = f;
				lastSustainValue[i] = currentGate;
			}
			// Gate off → go to release
			else if(!wasOff && currentGate <= gateThreshold) {
				if(getValueForIndex(hold, i) == 0) {
					envelopeStage[i] = (getValueForIndex(release, i) > 0)
					                   ? envelopeRelease2 : envelopeEnd2;
					phasorValueOnValueChange[i] = f;
					reachedMax[i] = false;
					lastPhase[i]  = 0;
				}
				// If hold > 0, the hold timeout below will handle the release transition
			}
			// Gate amplitude changed while active
			else if(isOn && abs(currentGate - targetValue[i]) > gateThreshold) {
				targetValue[i] = currentGate;
				maxValue[i]    = currentGate;
				if(envelopeStage[i] == envelopeDecay2 || envelopeStage[i] == envelopeSustain2) {
					lastSustainValue[i] = currentGate * getValueForIndex(sustain, i);
				}
			}

			// Build a temporary EnvelopeVoice from the mono per-index state
			// so we can reuse computeVoice() without duplicating logic.
			EnvelopeVoice v;
			v.stage               = envelopeStage[i];
			v.maxValue            = maxValue[i];
			v.phasorValueOnChange = phasorValueOnValueChange[i];
			v.lastPhase           = lastPhase[i];
			v.reachedMax          = reachedMax[i];
			v.lastSustainValue    = lastSustainValue[i];
			v.initialPhase        = initialPhase[i];

			float sample = 0;
			computeVoice(v, f, i, sample);
			tempOutput[i] = sample;

			// Write back updated state
			envelopeStage[i]              = v.stage;
			maxValue[i]                   = v.maxValue;
			phasorValueOnValueChange[i]   = v.phasorValueOnChange;
			lastPhase[i]                  = v.lastPhase;
			reachedMax[i]                 = v.reachedMax;
			lastSustainValue[i]           = v.lastSustainValue;

			lastInput[i] = currentGate;
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
// gateInListener — fires on every gateIn parameter update.
// Records rising and falling edges into pendingOnsets / pendingRelease so
// that phasorListener can consume them even if the gate pulsed and returned
// to zero between two phasor ticks.
// Also keeps lastInput up to date for mono mode (mono mode reads it directly
// in phasorListener; poly mode uses the pending queues instead).
// ---------------------------------------------------------------------------
void envelopeGenerator2::gateInListener(vector<float> &vf) {
	if(!polyMode.get()) return;   // mono mode does its own edge detection in phasorListener

	int sz = (int)vf.size();

	// Grow lastGate / pending queues if needed (phasorListener is the
	// authoritative resizer, but gateInListener can fire first).
	if(sz > (int)lastGate.size())     lastGate.resize(sz, 0);
	if(sz > (int)pendingOnsets.size()) {
		pendingOnsets.resize(sz);
		pendingRelease.resize(sz, false);
	}

	for(int i = 0; i < sz; i++) {
		float cur  = vf[i];
		float prev = lastGate[i];

		bool wasOff = prev <= gateThreshold;
		bool isOn   = cur  >  gateThreshold;

		// Rising edge → queue a new voice onset with this gate amplitude
		if(wasOff && isOn) {
			pendingOnsets[i].push_back(cur);
		}
		// Falling edge → mark that gated voices should be released
		if(!wasOff && !isOn) {
			pendingRelease[i] = true;
		}

		lastGate[i] = cur;
	}
}

// ---------------------------------------------------------------------------
// Preview curve (unchanged logic, just uses index 0 of each parameter)
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

	// Attack
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

	// Decay
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

	// Sustain
	int sustainSize = maxSize / 2;
	int sustainPos = tempOutput.size();
	tempOutput.resize(sustainPos + sustainSize);
	fill(tempOutput.begin() + sustainPos, tempOutput.end(), previewGateValue * sustain->at(0));

	// Release
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
