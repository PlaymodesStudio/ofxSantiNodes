#ifndef midiNoteQuantizer_h
#define midiNoteQuantizer_h

#include "ofxOceanodeNodeModel.h"
#include <vector>
#include <algorithm>
#include <cmath>

class midiNoteQuantizer : public ofxOceanodeNodeModel {
public:
	midiNoteQuantizer() : ofxOceanodeNodeModel("MIDI Note Quantizer") {}

	void setup() override {
		// ---- INPUTS ----
		addSeparator("INPUTS", ofColor(240, 240, 240));
		
		addParameter(pitchIn.set("Pitch In", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(gateIn.set("Gate In", {0.0f}, {0.0f}, {FLT_MAX}));
		addParameter(beatTransport.set("Beat Transport", 0.0f, 0.0f, FLT_MAX));
		
		// ---- PARAMETERS ----
		addSeparator("PARAMETERS", ofColor(240, 240, 240));
		
		addParameter(qGrid.set("Q Grid", 0.25f, 0.001f, 16.0f));
		addParameter(noteOffQuant.set("Quantize Note Off", false));
		
		// ---- OUTPUTS ----
		addSeparator("OUTPUTS", ofColor(240, 240, 240));
		
		addOutputParameter(pitchOut.set("Pitch Out", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
		addOutputParameter(gateOut.set("Gate Out", {0.0f}, {0.0f}, {FLT_MAX}));
		
		// ---- LISTENERS ----
		listeners.push(gateIn.newListener([this](vector<float> &v) {
			resizeState(v.size());
		}));
		
		resizeState(1);
	}

	void update(ofEventArgs &) override {
		const auto& pIn = pitchIn.get();
		const auto& gIn = gateIn.get();
		const float beat = beatTransport.get();
		const float grid = std::max(0.001f, qGrid.get());
		const bool quantizeOff = noteOffQuant.get();
		
		// Ensure inputs match size (polyphony safety)
		size_t n = gIn.size();
		if(pIn.size() < n) n = pIn.size();
		resizeState(n);
		
		vector<float> pOut(n, 0.0f);
		vector<float> gOut(n, 0.0f);
		
		// Calculate Grid
		float currentGrid = std::floor(beat / grid);
		
		for(size_t i = 0; i < n; ++i) {
			float inPitch = pIn[i];
			float inGate = gIn[i];
			
			// --- 1. Detect Input Edges ---
			bool isHigh = (std::abs(inGate) > 0.01f);
			bool wasHigh = (std::abs(lastGateIn[i]) > 0.01f);
			
			// RISING EDGE (Note On Request)
			if(isHigh && !wasHigh) {
				// User pressed key:
				// 1. Cancel any pending Stop request (re-trigger)
				stopRequested[i] = false;
				// 2. Start waiting for grid to Start
				isWaiting[i] = true;
				// 3. Capture immediate values
				pendingPitch[i] = inPitch;
				pendingGate[i] = inGate;
			}
			
			// FALLING EDGE (Note Off Request)
			if(!isHigh && wasHigh) {
				// If we were waiting to start, but released early -> Cancel Start
				if(isWaiting[i]) {
					isWaiting[i] = false;
				}
				
				// If we are currently playing...
				if(isActive[i]) {
					if(quantizeOff) {
						// MODE A: Quantized Off -> Request stop at next grid
						stopRequested[i] = true;
					} else {
						// MODE B: Immediate Off -> Stop now
						isActive[i] = false;
					}
				}
			}
			
			// Continuous Pitch Tracking (While waiting)
			// Allows correcting finger position before the beat hits
			if(isWaiting[i]) {
				pendingPitch[i] = inPitch;
				// Optional: update pendingGate if you want pressure to update velocity
				// pendingGate[i] = inGate;
			}
			
			// --- 2. Grid Logic ---
			if(currentGrid != lastGridPos[i]) {
				// Grid Crossing!
				
				// A. Handle Starts
				if(isWaiting[i]) {
					isActive[i] = true;
					isWaiting[i] = false;
					
					// Latch values for playback
					latchedPitch[i] = pendingPitch[i];
					latchedGate[i] = pendingGate[i];
				}
				
				// B. Handle Stops
				if(stopRequested[i]) {
					isActive[i] = false;
					stopRequested[i] = false;
				}
				
				lastGridPos[i] = currentGrid;
			}
			
			// --- 3. Output Logic ---
			if(isActive[i]) {
				pOut[i] = latchedPitch[i];
				
				if(stopRequested[i]) {
					// If we are waiting to stop (finger released), sustain the note
					// using the latched velocity
					gOut[i] = latchedGate[i];
				} else {
					// If finger is still pressing, pass the LIVE gate value
					// This preserves PolyAftertouch / Pressure expression
					gOut[i] = inGate;
				}
			} else {
				// Silence
				pOut[i] = latchedPitch[i]; // Keep last pitch to avoid jumps
				gOut[i] = 0.0f;
			}
			
			// Store history
			lastGateIn[i] = inGate;
		}
		
		pitchOut = pOut;
		gateOut = gOut;
	}

private:
	void resizeState(size_t n) {
		if(n == 0) n = 1;
		
		if(lastGateIn.size() != n) {
			lastGateIn.assign(n, 0.0f);
			lastGridPos.assign(n, -1.0f);
			
			isWaiting.assign(n, false);
			isActive.assign(n, false);
			stopRequested.assign(n, false);
			
			pendingPitch.assign(n, 0.0f);
			pendingGate.assign(n, 0.0f);
			latchedPitch.assign(n, 0.0f);
			latchedGate.assign(n, 0.0f);
			
			vector<float> zeros(n, 0.0f);
			pitchOut = zeros;
			gateOut = zeros;
		}
	}

	// Parameters
	ofParameter<vector<float>> pitchIn;
	ofParameter<vector<float>> gateIn;
	ofParameter<float> beatTransport;
	ofParameter<float> qGrid;
	ofParameter<bool> noteOffQuant;
	
	ofParameter<vector<float>> pitchOut;
	ofParameter<vector<float>> gateOut;
	
	ofEventListeners listeners;
	
	// State (Per Channel)
	vector<float> lastGateIn;
	vector<float> lastGridPos;
	
	vector<bool> isWaiting;     // Waiting for Grid to Start
	vector<bool> isActive;      // Currently Playing
	vector<bool> stopRequested; // Waiting for Grid to Stop (only if noteOffQuant is true)
	
	vector<float> pendingPitch; // Pitch detected while waiting
	vector<float> pendingGate;  // Velocity detected while waiting
	
	vector<float> latchedPitch; // Pitch currently being output
	vector<float> latchedGate;  // Velocity to use if sustaining after release
};

#endif /* midiNoteQuantizer_h */
