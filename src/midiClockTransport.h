#ifndef midiClockTransport_h
#define midiClockTransport_h

#include "ofxOceanodeNodeModel.h"
#include "ofxMidi.h"
#include "ofThreadChannel.h"
#include <cmath>
#include <cstdint>
#include <deque>

/*
	MIDI Clock Transport (thread-safe, tick-based BPM)

	Key features:
	1) BPM calculated using sliding window of tick intervals (24 PPQ)
	2) SPP handling with toggle for REAPER vs standard MIDI
	3) "Clock Only Mode" for devices without transport signals
	4) Configurable smoothing window size
*/

class midiClockTransport :
	public ofxOceanodeNodeModel,
	public ofxMidiListener {

public:
	midiClockTransport()
	: ofxOceanodeNodeModel("MIDI Clock Transport") {}

	~midiClockTransport() override {
		stopMidi();
		snapToMain.close();
	}

	void setup() override {
		/* -------- Inputs -------- */
		addSeparator("INPUTS", ofColor(240, 240, 240));
		addParameterDropdown(midiPort, "Port", 0, midiIn.getInPortList());
		addParameter(enable.set("Enable", 0, 0, 1));
		addParameter(clockOnlyMode.set("Clock Only", 0, 0, 1));
		addParameter(reaperMode.set("REAPER Mode", 1, 0, 1));  // Default ON for REAPER compatibility
		addParameter(bpmWindowSize.set("BPM Window", 24, 12, 96));  // Window size in ticks (12-96, default 24 = 1 beat)
		addParameter(bpmDecimals.set("BPM Decimals", 1, 0, 2));  // Rounding precision (0, 1, or 2 decimal places)
		addParameter(listPorts.set("List Ports"));

		/* -------- Transport outputs -------- */
		addSeparator("TRANSPORT", ofColor(240, 240, 240));
		addOutputParameter(playState.set("Play", 0, 0, 1));
		playState.setSerializable(false);

		addOutputParameter(stopState.set("Stop", 0, 0, 1));
		stopState.setSerializable(false);

		addOutputParameter(jumpTrig.set("Jump", 0, 0, 1));
		jumpTrig.setSerializable(false);

		/* -------- MIDI Clock outputs -------- */
		addSeparator("CLOCK OUTPUTS", ofColor(240, 240, 240));

		addOutputParameter(beat.set("Beat", 0.f, 0.f, FLT_MAX));
		beat.setSerializable(false);

		addOutputParameter(ppq24.set("PPQ 24", 0, 0, INT_MAX));
		ppq24.setSerializable(false);

		addOutputParameter(ppq24f.set("PPQ 24f", 0.f, 0.f, FLT_MAX));
		ppq24f.setSerializable(false);

		addOutputParameter(beatTransport.set("Beat Transport", 0.f, 0.f, FLT_MAX));
		beatTransport.setSerializable(false);

		addOutputParameter(timeSeconds.set("Time(s)", 0.f, 0.f, FLT_MAX));
		timeSeconds.setSerializable(false);

		addOutputParameter(bpm.set("BPM", 120.f, 1.f, 999.f));
		bpm.setSerializable(false);

		/* -------- Listeners -------- */
		listeners.push(midiPort.newListener([this](int &device){
			restartMidi(device);
		}));

		listeners.push(enable.newListener([this](int &e){
			(e == 1) ? startMidi() : stopMidi();
		}));

		listeners.push(clockOnlyMode.newListener([this](int &mode){
			if(mode == 1) {
				playing_midi = true;
				stopped_midi = false;
			}
		}));

		listeners.push(listPorts.newListener([this](){
			midiIn.listInPorts();
		}));

		resetMainThreadState();
		resetMidiThreadState();
	}

	/* ================= MIDI CALLBACK (MIDI THREAD) ================= */

	void newMidiMessage(ofxMidiMessage &msg) override {
		const MidiStatus st = msg.status;

		if(st == MIDI_TIME_CLOCK) {
			const uint64_t now = ofGetElapsedTimeMicros();

			if(!playing_midi && clockOnlyMode.get() == 0) {
				return;
			}

			if(clockOnlyMode.get() == 1 && !playing_midi) {
				playing_midi = true;
				stopped_midi = false;
				startCounter_midi++;
			}

			tickCount_midi++;
			
			// Store tick timestamp for BPM calculation (microseconds)
			tickTimestamps_midi.push_back(now);
			
			// Trim to max window size (keep some extra for flexibility)
			const size_t maxSize = 128;
			while(tickTimestamps_midi.size() > maxSize) {
				tickTimestamps_midi.pop_front();
			}

			lastClockMs_midi = now / 1000;  // Keep ms version for other uses
			stopped_midi = false;

			pushSnapshot(now);
		}
		else if(st == MIDI_SONG_POS_POINTER) {
			if(msg.bytes.size() >= 2) {
				const uint8_t byte0 = static_cast<uint8_t>(msg.bytes[0]);
				const uint8_t byte1 = static_cast<uint8_t>(msg.bytes[1]);
				
				int newTickCount = 0;
				
				if(reaperMode.get() == 1) {
					// REAPER mode: byte1 encodes position, byte0 is always 242 (ignored)
					// byte1 = beat * 4 (i.e., sixteenth notes)
					// ticks = byte1 * 6 (since 24 PPQ / 4 sixteenths = 6 ticks per sixteenth)
					newTickCount = static_cast<int>(byte1) * 6;
					
					ofLogNotice("MIDI SPP [REAPER]") << "byte1=" << (int)byte1
													 << " -> ticks=" << newTickCount
													 << " (beat " << (newTickCount / 24.0f) << ")";
				} else {
					// Standard MIDI SPP: 14-bit value in units of 1/16th notes
					const int sppValue = (static_cast<int>(byte1) << 7) | static_cast<int>(byte0);
					newTickCount = sppValue * 6;
					
					ofLogNotice("MIDI SPP [Standard]") << "sppValue=" << sppValue
													   << " -> ticks=" << newTickCount
													   << " (beat " << (newTickCount / 24.0f) << ")";
				}
				
				// Detect jump
				if(std::abs(newTickCount - tickCount_midi) > 12) {
					jumpCounter_midi++;
				}
				
				tickCount_midi = newTickCount;
				lastSpp_midi = (static_cast<int>(byte1) << 7) | static_cast<int>(byte0);
				
				// Clear tick timestamps on jump (they're no longer valid for BPM calc)
				tickTimestamps_midi.clear();

				pushSnapshot(ofGetElapsedTimeMicros());
			}
		}
		else if(st == MIDI_START) {
			resetMidiThreadState();
			playing_midi = true;
			stopped_midi = false;

			startCounter_midi++;
			jumpCounter_midi++;

			pushSnapshot(ofGetElapsedTimeMicros());
		}
		else if(st == MIDI_CONTINUE) {
			playing_midi = true;
			stopped_midi = false;

			contCounter_midi++;
			jumpCounter_midi++;
			
			// Clear tick timestamps on continue (timing discontinuity)
			tickTimestamps_midi.clear();

			pushSnapshot(ofGetElapsedTimeMicros());
		}
		else if(st == MIDI_STOP) {
			playing_midi = false;
			stopped_midi = true;

			stopCounter_midi++;

			pushSnapshot(ofGetElapsedTimeMicros());
		}
	}

	/* ================= MAIN THREAD (Oceanode update loop) ================= */

	void update(ofEventArgs &args) override {
		ClockSnapshot s;
		bool got = false;
		while(snapToMain.tryReceive(s)) {
			got = true;
		}

		if(jumpTrigFramesRemaining > 0){
			jumpTrigFramesRemaining--;
			jumpTrig = 1;
		} else {
			jumpTrig = 0;
		}

		if(!got) return;

		const bool jumpEdge  = (s.jumpCounter  != lastJumpCounter_main);
		const bool startEdge = (s.startCounter != lastStartCounter_main);
		const bool stopEdge  = (s.stopCounter  != lastStopCounter_main);
		const bool contEdge  = (s.contCounter  != lastContCounter_main);
		
		lastJumpCounter_main  = s.jumpCounter;
		lastStartCounter_main = s.startCounter;
		lastStopCounter_main  = s.stopCounter;
		lastContCounter_main  = s.contCounter;

		playState = s.playing ? 1 : 0;
		stopState = s.stopped ? 1 : 0;

		if(jumpEdge){
			jumpTrigFramesRemaining = 3;
		}

		ppq24  = s.tickCount;
		ppq24f = static_cast<float>(s.tickCount);
		
		const float currentBeat = static_cast<float>(s.tickCount) / 24.f;
		beat = currentBeat;
		beatTransport = currentBeat;

		// ---------- BPM CALCULATION (tick-based sliding window, microsecond precision) ----------
		// Use the last N tick timestamps to calculate average tick interval
		// BPM = 60000000 / (avg_us_per_tick * 24)
		
		const int windowSize = bpmWindowSize.get();
		const int decimals = bpmDecimals.get();
		
		if(s.playing && s.tickTimestamps.size() >= 2) {
			// Use up to windowSize ticks
			const size_t available = s.tickTimestamps.size();
			const size_t useCount = std::min(static_cast<size_t>(windowSize), available);
			
			if(useCount >= 2) {
				// Get timestamps from the end of the deque (most recent)
				const uint64_t newestUs = s.tickTimestamps.back();
				const uint64_t oldestUs = s.tickTimestamps[available - useCount];
				
				const uint64_t durationUs = newestUs - oldestUs;
				const size_t tickIntervals = useCount - 1;  // N timestamps = N-1 intervals
				
				if(durationUs > 0 && tickIntervals > 0) {
					// Average microseconds per tick
					const double avgUsPerTick = static_cast<double>(durationUs) / static_cast<double>(tickIntervals);
					
					// BPM = 60000000 / (us_per_tick * 24)
					const double instantBpm = 60000000.0 / (avgUsPerTick * 24.0);
					
					if(std::isfinite(instantBpm) && instantBpm >= 20.0 && instantBpm <= 400.0) {
						// Light smoothing to avoid micro-jitter
						bpmSmooth_main = bpmSmooth_main * 0.8 + instantBpm * 0.2;
					}
				}
			}
		}
		
		// Round to specified decimal places
		float bpmRounded = bpmSmooth_main;
		if(decimals == 0) {
			bpmRounded = std::round(bpmSmooth_main);
		} else if(decimals == 1) {
			bpmRounded = std::round(bpmSmooth_main * 10.0f) / 10.0f;
		} else if(decimals == 2) {
			bpmRounded = std::round(bpmSmooth_main * 100.0f) / 100.0f;
		}

		bpm = ofClamp(bpmRounded, 1.f, 999.f);

		const float bpmVal = bpm.get();
		timeSeconds = (bpmVal > 0.f) ? (currentBeat / bpmVal) * 60.f : 0.f;
	}

private:
	struct ClockSnapshot {
		uint64_t ms = 0;
		int tickCount = 0;

		bool playing = false;
		bool stopped = true;

		uint32_t jumpCounter = 0;
		uint32_t startCounter = 0;
		uint32_t stopCounter = 0;
		uint32_t contCounter = 0;

		int lastSpp = -1;
		
		// Tick timestamps for BPM calculation (copied from MIDI thread)
		std::deque<uint64_t> tickTimestamps;
	};

	ofThreadChannel<ClockSnapshot> snapToMain;

	/* ================= MIDI THREAD STATE ================= */
	uint64_t lastClockMs_midi = 0;
	int tickCount_midi = 0;

	bool playing_midi = false;
	bool stopped_midi = true;

	uint32_t jumpCounter_midi = 0;
	uint32_t startCounter_midi = 0;
	uint32_t stopCounter_midi = 0;
	uint32_t contCounter_midi = 0;

	int lastSpp_midi = -1;
	
	// Tick timestamps for BPM calculation
	std::deque<uint64_t> tickTimestamps_midi;

	/* ================= MAIN THREAD STATE ================= */
	uint32_t lastJumpCounter_main = 0;
	uint32_t lastStartCounter_main = 0;
	uint32_t lastStopCounter_main  = 0;
	uint32_t lastContCounter_main  = 0;

	double bpmSmooth_main = 120.0;
	
	int jumpTrigFramesRemaining = 0;

	void pushSnapshot(uint64_t nowMs) {
		ClockSnapshot old;
		while(snapToMain.tryReceive(old)) {}

		ClockSnapshot s;
		s.ms = nowMs;
		s.tickCount = tickCount_midi;

		s.playing = playing_midi;
		s.stopped = stopped_midi;

		s.jumpCounter = jumpCounter_midi;
		s.startCounter = startCounter_midi;
		s.stopCounter = stopCounter_midi;
		s.contCounter = contCounter_midi;

		s.lastSpp = lastSpp_midi;
		
		// Copy tick timestamps for BPM calculation on main thread
		s.tickTimestamps = tickTimestamps_midi;

		snapToMain.send(s);
	}

	/* ================= MIDI MANAGEMENT ================= */

	void startMidi() {
		if(midiIn.isOpen()) return;

		midiIn.openPort(midiPort);

		midiIn.ignoreTypes(
			false, // sysex
			false, // timing
			false  // active sensing
		);

		midiIn.addListener(this);

		resetMidiThreadState();
		resetMainThreadState();
		
		if(clockOnlyMode.get() == 1) {
			playing_midi = true;
			stopped_midi = false;
		}
	}

	void stopMidi() {
		if(midiIn.isOpen()) {
			midiIn.removeListener(this);
			midiIn.closePort();
		}
	}

	void restartMidi(int device) {
		stopMidi();
		if(enable.get() == 1) startMidi();
	}

	/* ================= RESET ================= */

	void resetMidiThreadState() {
		lastClockMs_midi = 0;
		tickCount_midi = 0;

		playing_midi = false;
		stopped_midi = true;

		jumpCounter_midi = 0;
		startCounter_midi = 0;
		stopCounter_midi = 0;
		contCounter_midi = 0;

		lastSpp_midi = -1;
		
		tickTimestamps_midi.clear();
	}

	void resetMainThreadState() {
		playState = 0;
		stopState = 1;
		jumpTrig = 0;

		beat = 0.f;
		ppq24 = 0;
		ppq24f = 0.f;
		beatTransport = 0.f;
		timeSeconds = 0.f;
		bpm = 120.f;

		ClockSnapshot old;
		while(snapToMain.tryReceive(old)) {}

		lastJumpCounter_main = 0;
		lastStartCounter_main = 0;
		lastStopCounter_main  = 0;
		lastContCounter_main  = 0;

		bpmSmooth_main = 120.0;
		
		jumpTrigFramesRemaining = 0;
	}

	/* -------- MIDI -------- */
	ofxMidiIn midiIn;

	/* -------- Parameters -------- */
	ofParameter<int> midiPort;
	ofParameter<int> enable;
	ofParameter<int> clockOnlyMode;
	ofParameter<int> reaperMode;
	ofParameter<int> bpmWindowSize;
	ofParameter<int> bpmDecimals;
	ofParameter<void> listPorts;

	ofParameter<int> playState;
	ofParameter<int> stopState;
	ofParameter<int> jumpTrig;

	ofParameter<float> beat;
	ofParameter<int> ppq24;
	ofParameter<float> ppq24f;
	ofParameter<float> beatTransport;
	ofParameter<float> timeSeconds;
	ofParameter<float> bpm;

	ofEventListeners listeners;
};

#endif /* midiClockTransport_h */
