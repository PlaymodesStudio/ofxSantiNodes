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
	
	addParameter(curvePreview.set("Curve", {0}, {0}, {1}));
	addOutputParameter(output.set("Output", {0}, {0}, {1}));
	
	oldGateIn = {0};
	outputComputeVec = {0};
	gateThreshold = 0.001f; // Threshold for considering gate "off"
	
	listener = phasor.newListener(this, &envelopeGenerator2::phasorListener);
	
	// Add preview curve update listeners
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

void envelopeGenerator2::phasorListener(vector<float> &vf) {
	int inputSize = gateIn.get().size();
	
	// Initialize or resize vectors if input size changes
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
		lastGateInSize = inputSize;
	}
	
	vector<float> tempOutput = gateIn.get();
	
	for(int i = 0; i < inputSize; i++) {
		float currentGate = gateIn.get()[i];
		float f = getValueForIndex(vf, i);
		
		// Gate transition detection with hysteresis
		bool wasOff = lastInput[i] <= gateThreshold;
		bool isOn = currentGate > gateThreshold;
		
		// New gate onset
		if(wasOff && isOn) {
			targetValue[i] = currentGate;
			maxValue[i] = currentGate;
			
			if(getValueForIndex(attack, i) == 0) {
				if(getValueForIndex(decay, i) == 0) {
					envelopeStage[i] = envelopeSustain2;
				} else {
					envelopeStage[i] = envelopeDecay2;
				}
			} else {
				envelopeStage[i] = envelopeAttack2;
			}
			
			phasorValueOnValueChange[i] = f;
			reachedMax[i] = false;
			lastPhase[i] = 0;
			initialPhase[i] = f;
			lastSustainValue[i] = currentGate;
		}
		// Gate off transition
		else if(!wasOff && currentGate <= gateThreshold) {
			if(getValueForIndex(hold, i) == 0) {
				if(getValueForIndex(release, i) > 0) {
					envelopeStage[i] = envelopeRelease2;
				} else {
					envelopeStage[i] = envelopeEnd2;
				}
				phasorValueOnValueChange[i] = f;
				reachedMax[i] = false;
				lastPhase[i] = 0;
			}
		}
		// Gate value change while active
		else if(isOn && abs(currentGate - targetValue[i]) > gateThreshold) {
			targetValue[i] = currentGate;
			maxValue[i] = currentGate;
			
			// Smoothly transition to new target value based on current stage
			switch(envelopeStage[i]) {
				case envelopeAttack2:
					// Continue attack to new target
					break;
				case envelopeDecay2:
				case envelopeSustain2:
					lastSustainValue[i] = currentGate * getValueForIndex(sustain, i);
					break;
				default:
					break;
			}
		}
		
		float phase = f - phasorValueOnValueChange[i];
		if(phase < 0) phase = 1 + phase;
		if(phase < lastPhase[i]) reachedMax[i] = true;
		else lastPhase[i] = phase;
		
		// Handle hold timeout
		if(getValueForIndex(hold, i) > 0) {
			float holdPhase = f - initialPhase[i];
			if(holdPhase < 0) holdPhase += 1;
			
			if(holdPhase > getValueForIndex(hold, i) &&
			   envelopeStage[i] != envelopeRelease2 &&
			   envelopeStage[i] != envelopeEnd2) {
				phasorValueOnValueChange[i] = f;
				if(getValueForIndex(release, i) > 0) {
					envelopeStage[i] = envelopeRelease2;
				} else {
					envelopeStage[i] = envelopeEnd2;
				}
				reachedMax[i] = false;
				lastPhase[i] = 0;
				phase = 0;
			}
		}
		
		// Process each envelope stage
		switch(envelopeStage[i]) {
			case envelopeAttack2: {
				if(phase > getValueForIndex(attack, i) || reachedMax[i]) {
					phasorValueOnValueChange[i] = f;
					reachedMax[i] = false;
					lastPhase[i] = 0;
					
					if(getValueForIndex(decay, i) == 0) {
						envelopeStage[i] = envelopeSustain2;
						// Set output to sustain value immediately
						tempOutput[i] = maxValue[i] * getValueForIndex(sustain, i);
						lastSustainValue[i] = tempOutput[i];
					} else {
						envelopeStage[i] = envelopeDecay2;
						// Set output to max value (start of decay)
						tempOutput[i] = maxValue[i];
						lastSustainValue[i] = tempOutput[i];
					}
				} else {
					phase = ofMap(phase, 0, getValueForIndex(attack, i), 0, 1, true);
					if(getValueForIndex(attackPow, i) != 0) {
						customPow(phase, getValueForIndex(attackPow, i));
					}
					if(getValueForIndex(attackBiPow, i) != 0) {
						phase = (phase * 2) - 1;
						customPow(phase, getValueForIndex(attackBiPow, i));
						phase = (phase + 1) / 2.0;
					}
					tempOutput[i] = smoothinterpolate(0, maxValue[i], phase);
					if(phase != 0) {
						lastSustainValue[i] = tempOutput[i];
					}
				}
				break;
			}
			
			case envelopeDecay2: {
				if(phase > getValueForIndex(decay, i) || reachedMax[i]) {
					phasorValueOnValueChange[i] = f;
					envelopeStage[i] = envelopeSustain2;
					reachedMax[i] = false;
					lastPhase[i] = 0;
					// Set output to sustain value immediately
					tempOutput[i] = maxValue[i] * getValueForIndex(sustain, i);
					lastSustainValue[i] = tempOutput[i];
				} else {
					phase = ofMap(phase, 0, getValueForIndex(decay, i), 0, 1, true);
					if(getValueForIndex(decayPow, i) != 0) {
						customPow(phase, getValueForIndex(decayPow, i));
					}
					if(getValueForIndex(decayBiPow, i) != 0) {
						phase = (phase * 2) - 1;
						customPow(phase, getValueForIndex(decayBiPow, i));
						phase = (phase + 1) / 2.0;
					}
					tempOutput[i] = smoothinterpolate(maxValue[i], maxValue[i] * getValueForIndex(sustain, i), phase);
					lastSustainValue[i] = tempOutput[i];
				}
				break;
			}
			
			case envelopeSustain2: {
				tempOutput[i] = maxValue[i] * getValueForIndex(sustain, i);
				lastSustainValue[i] = tempOutput[i];
				break;
			}
			
			case envelopeRelease2: {
				if(phase > getValueForIndex(release, i) || reachedMax[i]) {
					phasorValueOnValueChange[i] = f;
					envelopeStage[i] = envelopeEnd2;
					reachedMax[i] = false;
					lastPhase[i] = 0;
					// Set output to zero immediately
					tempOutput[i] = 0;
				} else {
					phase = ofMap(phase, 0, getValueForIndex(release, i), 0, 1, true);
					if(getValueForIndex(releasePow, i) != 0) {
						customPow(phase, getValueForIndex(releasePow, i));
					}
					if(getValueForIndex(releaseBiPow, i) != 0) {
						phase = (phase * 2) - 1;
						customPow(phase, getValueForIndex(releaseBiPow, i));
						phase = (phase + 1) / 2.0;
					}
					tempOutput[i] = smoothinterpolate(lastSustainValue[i], 0, phase);
				}
				break;
			}
			
			case envelopeEnd2: {
				tempOutput[i] = 0;
				break;
			}
		}
		
		lastInput[i] = currentGate;
	}
	
	output = tempOutput;
}

void envelopeGenerator2::customPow(float & value, float pow) {
	float k1 = 2 * pow * 0.99999;
	float k2 = (k1 / ((-pow * 0.999999) + 1));
	float k3 = k2 * abs(value) + 1;
	value = value * (k2 + 1) / k3;
}

float envelopeGenerator2::smoothinterpolate(float start, float end, float pos) {
	float oldRandom = start;
	float pastRandom = start;
	float newRandom = end;
	float futureRandom = end;
	float L0 = (newRandom - pastRandom) * 0.5;
	float L1 = L0 + (oldRandom - newRandom);
	float L2 = L1 + ((futureRandom - oldRandom) * 0.5) + (oldRandom - newRandom);
	return oldRandom + (pos * (L0 + (pos * ((pos * L2) - (L1 + L2)))));
}

void envelopeGenerator2::gateInChanged(vector<float> &vf) {
	for(int i = 0; i < vf.size(); i++) {
		float currentGateValue = vf[i];
		bool wasOff = lastInput[i] <= gateThreshold;
		bool isOn = currentGateValue > gateThreshold;
		
		if(wasOff && isOn) {
			// Gate onset
			if(getValueForIndex(attack, i) == 0) {
				if(getValueForIndex(decay, i) == 0) {
					envelopeStage[i] = envelopeSustain2;
				} else {
					envelopeStage[i] = envelopeDecay2;
				}
			} else {
				envelopeStage[i] = envelopeAttack2;
			}
			maxValue[i] = currentGateValue;
			targetValue[i] = currentGateValue;
			phasorValueOnValueChange[i] = 0;
			lastPhase[i] = 0;
			reachedMax[i] = false;
		} else if(!wasOff && currentGateValue <= gateThreshold) {
			// Gate release
			if(getValueForIndex(release, i) > 0) {
				envelopeStage[i] = envelopeRelease2;
			} else {
				envelopeStage[i] = envelopeEnd2;
			}
			phasorValueOnValueChange[i] = 0;
			lastPhase[i] = 0;
			reachedMax[i] = false;
		}
		lastInput[i] = currentGateValue;
	}
}

void envelopeGenerator2::recalculatePreviewCurve() {
	int maxSize = 100;
	vector<float> tempOutput;
	float previewGateValue = 1.0f; // Preview at full amplitude
	
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
		if(!attackPow->empty() && attackPow->at(0) != 0) {
			customPow(phase, attackPow->at(0));
		}
		if(!attackBiPow->empty() && attackBiPow->at(0) != 0) {
			phase = (phase * 2) - 1;
			customPow(phase, attackBiPow->at(0));
			phase = (phase + 1) / 2.0;
		}
		tempOutput[i] = smoothinterpolate(0, previewGateValue, phase);
	}

	// Decay
	int decaySize = std::max(0, std::min(maxSize, static_cast<int>(decay->at(0) * maxSize)));
	int decayPos = tempOutput.size();
	tempOutput.resize(decayPos + decaySize);
	for(int i = 0; i < decaySize; i++) {
		float phase = float(i) / float(decaySize);
		if(!decayPow->empty() && decayPow->at(0) != 0) {
			customPow(phase, decayPow->at(0));
		}
		if(!decayBiPow->empty() && decayBiPow->at(0) != 0) {
			phase = (phase * 2) - 1;
			customPow(phase, decayBiPow->at(0));
			phase = (phase + 1) / 2.0;
		}
		tempOutput[decayPos + i] = smoothinterpolate(previewGateValue, previewGateValue * sustain->at(0), phase);
	}
	
	// Sustain
	int sustainSize = maxSize/2;
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
		if(!releasePow->empty() && releasePow->at(0) != 0) {
			customPow(phase, releasePow->at(0));
		}
		if(!releaseBiPow->empty() && releaseBiPow->at(0) != 0) {
			phase = (phase * 2) - 1;
			customPow(phase, releaseBiPow->at(0));
			phase = (phase + 1) / 2.0;
		}
		tempOutput[releasePos + i] = smoothinterpolate(sustainLevel, 0, phase);
	}

	curvePreview = tempOutput;
}
