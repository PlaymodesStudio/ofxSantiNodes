#ifndef midiClockTransport_h
#define midiClockTransport_h

#include "ofxOceanodeNodeModel.h"
#include "ofxMidi.h"
#include "ofThreadChannel.h"
#include <cmath>
#include <cstdint>

/*
	MIDI Clock Transport (thread-safe + stable BPM)

	Changes vs previous "fixed" version:
	1) MIDI callback NEVER touches ofParameters (still true).
	2) Ignore MIDI_TIME_CLOCK until START/CONTINUE sets playing=true (REAPER-friendly).
	3) BPM is computed on the MAIN THREAD over a window of N ticks (default 24 = 1 beat),
	   and we suppress BPM updates for the first X ticks after start/continue/jump.
	4) Snapshot no longer relies on MIDI-thread BPM smoothing (optional, removed for stability).
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
		addParameter(enable.set("Enable", false));
		addParameter(listPorts.set("List Ports"));

		/* -------- Transport outputs -------- */
		addSeparator("TRANSPORT", ofColor(240, 240, 240));
		addOutputParameter(playState.set("Play", false));
		playState.setSerializable(false);

		addOutputParameter(stopState.set("Stop", false));
		stopState.setSerializable(false);

		addOutputParameter(jumpTrig.set("Jump", false));
		jumpTrig.setSerializable(false);

		/* -------- MIDI Clock outputs -------- */
		addSeparator("CLOCK OUTPUTS", ofColor(240, 240, 240));

		addOutputParameter(beat.set("Beat", 0.f, 0.f, FLT_MAX));
		beat.setSerializable(false);

		addOutputParameter(ppq24.set("PPQ 24", 0, 0, INT_MAX));
		ppq24.setSerializable(false);

		addOutputParameter(ppq24f.set("PPQ 24f", 0.f, 0.f, FLT_MAX));
		ppq24f.setSerializable(false);

		addOutputParameter(timeSeconds.set("Time(s)", 0.f, 0.f, FLT_MAX));
		timeSeconds.setSerializable(false);

		addOutputParameter(bpm.set("BPM", 120.f, 1.f, 999.f));
		bpm.setSerializable(false);

		/* -------- Listeners -------- */
		listeners.push(midiPort.newListener([this](int &device){
			restartMidi(device);
		}));

		listeners.push(enable.newListener([this](bool &e){
			e ? startMidi() : stopMidi();
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
			// REAPER / DAW safety: ignore clocks until transport says we're playing
			if(!playing_midi) return;

			const uint64_t now = ofGetElapsedTimeMillis();

			const int prevTick = tickCount_midi;
			tickCount_midi++;

			// Jump detection: if we ever see discontinuity in tick progression.
			// (In practice tickCount increments by 1 always here, but keep the logic
			// in case you later derive ticks differently.)
			if(prevTick > 0) {
				const int expected = prevTick + 1;
				const int delta = std::abs(tickCount_midi - expected);
				if(delta > 2) jumpCounter_midi++;
			}

			lastClockMs_midi = now;
			stopped_midi = false;

			pushSnapshot(now);
		}
		else if(st == MIDI_SONG_POS_POINTER) {
			if(msg.bytes.size() >= 2) {
				const int sppValue = (static_cast<int>(msg.bytes[1]) << 7) | static_cast<int>(msg.bytes[0]);

				const int REAPER_OFFSET = 242;
				const int SPP_MAX = 16384; // 14-bit max

				int basePPQ = ((sppValue - REAPER_OFFSET) * 3) / 64;
				if(basePPQ < 0) basePPQ = 0;

				const int MAX_PPQ_PER_WRAP = ((SPP_MAX - REAPER_OFFSET) * 3) / 64;

				if(playing_midi && tickCount_midi > 0 && MAX_PPQ_PER_WRAP > 0) {
					const int expectedWrap = tickCount_midi / MAX_PPQ_PER_WRAP;
					const int candidate1 = basePPQ + (expectedWrap * MAX_PPQ_PER_WRAP);
					const int candidate2 = basePPQ + ((expectedWrap + 1) * MAX_PPQ_PER_WRAP);
					tickCount_midi = (std::abs(candidate1 - tickCount_midi) < std::abs(candidate2 - tickCount_midi))
						? candidate1 : candidate2;
				} else {
					tickCount_midi = basePPQ;
				}

				// SPP = jump event
				jumpCounter_midi++;
				lastSpp_midi = sppValue;

				pushSnapshot(ofGetElapsedTimeMillis());
			}
		}
		else if(st == MIDI_START) {
			resetMidiThreadState();        // zero tick, timestamps, etc.
			playing_midi = true;
			stopped_midi = false;

			startCounter_midi++;
			jumpCounter_midi++;            // treat start as an edge/jump

			pushSnapshot(ofGetElapsedTimeMillis());
		}
		else if(st == MIDI_CONTINUE) {
			playing_midi = true;
			stopped_midi = false;

			contCounter_midi++;
			jumpCounter_midi++;            // treat continue as an edge/jump

			pushSnapshot(ofGetElapsedTimeMillis());
		}
		else if(st == MIDI_STOP) {
			playing_midi = false;
			stopped_midi = true;

			stopCounter_midi++;

			pushSnapshot(ofGetElapsedTimeMillis());
		}
	}

	/* ================= MAIN THREAD (Oceanode update loop) ================= */

	void update(ofEventArgs &args) override {
		// Drain snapshots; keep latest (resample to frame rate)
		ClockSnapshot s;
		bool got = false;
		while(snapToMain.tryReceive(s)) {
			got = true;
		}

		// Edge trigger default
		jumpTrig = false;

		if(!got) return;

		// Compute jump edge BEFORE updating playState parameter (we need previous too)
		const bool jumpEdge = (s.jumpCounter != lastJumpCounter_main);
		lastJumpCounter_main = s.jumpCounter;

		// Transport edges for “start of playing”
		const bool startEdge    = (s.startCounter != lastStartCounter_main);
		const bool stopEdge     = (s.stopCounter  != lastStopCounter_main);
		const bool contEdge     = (s.contCounter  != lastContCounter_main);
		lastStartCounter_main = s.startCounter;
		lastStopCounter_main  = s.stopCounter;
		lastContCounter_main  = s.contCounter;

		// Publish transport states (main thread)
		const bool wasPlaying = playState.get(); // previous frame value
		playState = s.playing;
		stopState = s.stopped;

		// One-frame trigger
		jumpTrig = jumpEdge;

		// Publish clock scalars (main thread)
		ppq24  = s.tickCount;
		ppq24f = static_cast<float>(s.tickCount);
		beat   = static_cast<float>(s.tickCount) / 24.f;

		// ---------- BPM STABILIZATION ----------
		// Strategy:
		// - Estimate BPM over a window of N ticks (default: 24 ticks = 1 beat)
		// - Reset/re-prime estimation on start/continue/jump/stop
		// - Suppress BPM updates for first X ticks after start/continue/jump
		//
		// This kills the big oscillations right after pressing play in REAPER.

		const bool playingNow = s.playing;
		const bool transportEdge = startEdge || contEdge || stopEdge || jumpEdge || (playingNow && !wasPlaying) || (!playingNow && wasPlaying);

		if(transportEdge) {
			haveBpmWindow_main = false;
			ticksSinceEdge_main = 0;
		}

		// Track tick advances to count "ticks since edge"
		if(playingNow) {
			if(!haveLastTick_main) {
				lastTickObserved_main = s.tickCount;
				haveLastTick_main = true;
			} else {
				if(s.tickCount != lastTickObserved_main) {
					// If tick jumped backwards/forwards a lot, treat as edge
					if(std::abs(s.tickCount - lastTickObserved_main) > 8) {
						haveBpmWindow_main = false;
						ticksSinceEdge_main = 0;
					} else {
						ticksSinceEdge_main += std::abs(s.tickCount - lastTickObserved_main);
					}
					lastTickObserved_main = s.tickCount;
				}
			}
		} else {
			haveLastTick_main = false;
		}

		// Only update BPM after we’ve seen enough ticks post-edge
		if(playingNow && ticksSinceEdge_main >= BPM_IGNORE_TICKS_AFTER_EDGE) {
			if(!haveBpmWindow_main) {
				bpmWindowStartMs_main = s.ms;
				bpmWindowStartTick_main = s.tickCount;
				haveBpmWindow_main = true;
			} else {
				const int dTick = s.tickCount - bpmWindowStartTick_main;
				const uint64_t dMs = s.ms - bpmWindowStartMs_main;

				if(dTick >= BPM_WINDOW_TICKS && dMs > 0) {
					float instBpm = (60000.f * static_cast<float>(dTick)) / (static_cast<float>(dMs) * 24.f);

					// Plausibility clamp (optional but helpful)
					if(std::isfinite(instBpm)) {
						instBpm = ofClamp(instBpm, 1.f, 999.f);

						// Prevent insane spikes relative to current smooth BPM
						instBpm = ofClamp(instBpm, bpmSmooth_main * 0.85f, bpmSmooth_main * 1.15f);

						// Smooth (since we update less frequently, use stronger alpha)
						bpmSmooth_main = bpmSmooth_main * (1.f - BPM_SMOOTH_ALPHA) + instBpm * BPM_SMOOTH_ALPHA;
					}

					// Advance window
					bpmWindowStartMs_main = s.ms;
					bpmWindowStartTick_main = s.tickCount;
				}
			}
		}

		bpm = ofClamp(bpmSmooth_main, 1.f, 999.f);

		// Time in seconds: (beats / bpm) * 60
		const float bpmVal = bpm.get();
		const float currentBeat = beat.get();
		timeSeconds = (bpmVal > 0.f) ? (currentBeat / bpmVal) * 60.f : 0.f;
	}

private:
	/* ================= CONFIG (MAIN THREAD BPM) ================= */
	static constexpr int   BPM_WINDOW_TICKS = 24;              // 24 = 1 beat; try 96 for extra smooth
	static constexpr int   BPM_IGNORE_TICKS_AFTER_EDGE = 48;   // ignore first 2 beats after start/jump
	static constexpr float BPM_SMOOTH_ALPHA = 0.20f;           // smoothing factor when a window update happens

	/* ================= SNAPSHOT / RESAMPLING ================= */

	struct ClockSnapshot {
		uint64_t ms = 0;
		int tickCount = 0;

		bool playing = false;
		bool stopped = true;

		uint32_t jumpCounter = 0;
		uint32_t startCounter = 0;
		uint32_t stopCounter = 0;
		uint32_t contCounter = 0;

		int lastSpp = -1; // debug/info
	};

	ofThreadChannel<ClockSnapshot> snapToMain;

	// MIDI-thread state (ONLY touched on MIDI thread)
	uint64_t lastClockMs_midi = 0;
	int tickCount_midi = 0;

	bool playing_midi = false;
	bool stopped_midi = true;

	uint32_t jumpCounter_midi = 0;
	uint32_t startCounter_midi = 0;
	uint32_t stopCounter_midi = 0;
	uint32_t contCounter_midi = 0;

	int lastSpp_midi = -1;

	// Main-thread edge tracking
	uint32_t lastJumpCounter_main = 0;
	uint32_t lastStartCounter_main = 0;
	uint32_t lastStopCounter_main  = 0;
	uint32_t lastContCounter_main  = 0;

	// Main-thread BPM estimation state
	float bpmSmooth_main = 120.f;

	bool haveBpmWindow_main = false;
	uint64_t bpmWindowStartMs_main = 0;
	int bpmWindowStartTick_main = 0;

	int ticksSinceEdge_main = 0;

	bool haveLastTick_main = false;
	int lastTickObserved_main = 0;

	void pushSnapshot(uint64_t nowMs) {
		// MIDI thread: publish latest snapshot; drop backlog to keep “latest-only”
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

		snapToMain.send(s);
	}

	/* ================= MIDI MANAGEMENT (MAIN THREAD) ================= */

	void startMidi() {
		if(midiIn.isOpen()) return;

		midiIn.openPort(midiPort);

		// Allow timing messages (MIDI Clock)
		midiIn.ignoreTypes(
			false, // sysex
			false, // timing
			false  // active sensing
		);

		midiIn.addListener(this);

		// Reset state when enabling
		resetMidiThreadState();
		resetMainThreadState();
	}

	void stopMidi() {
		if(midiIn.isOpen()) {
			midiIn.removeListener(this);
			midiIn.closePort();
		}
	}

	void restartMidi(int device) {
		stopMidi();
		if(enable) startMidi();
	}

	/* ================= RESET ================= */

	void resetMidiThreadState() {
		lastClockMs_midi = 0;
		tickCount_midi = 0;

		playing_midi = false;
		stopped_midi = true;

		// Reset counters (fresh run)
		jumpCounter_midi = 0;
		startCounter_midi = 0;
		stopCounter_midi = 0;
		contCounter_midi = 0;

		lastSpp_midi = -1;
	}

	void resetMainThreadState() {
		// clear output params
		playState = false;
		stopState = true;
		jumpTrig = false;

		beat = 0.f;
		ppq24 = 0;
		ppq24f = 0.f;
		timeSeconds = 0.f;
		bpm = 120.f;

		// clear pending snapshots
		ClockSnapshot old;
		while(snapToMain.tryReceive(old)) {}

		// edge trackers
		lastJumpCounter_main = 0;
		lastStartCounter_main = 0;
		lastStopCounter_main  = 0;
		lastContCounter_main  = 0;

		// bpm state
		bpmSmooth_main = 120.f;
		haveBpmWindow_main = false;
		bpmWindowStartMs_main = 0;
		bpmWindowStartTick_main = 0;

		ticksSinceEdge_main = 0;
		haveLastTick_main = false;
		lastTickObserved_main = 0;
	}

	/* -------- MIDI -------- */
	ofxMidiIn midiIn;

	/* -------- Parameters -------- */
	ofParameter<int> midiPort;
	ofParameter<bool> enable;
	ofParameter<void> listPorts;

	ofParameter<bool> playState;
	ofParameter<bool> stopState;
	ofParameter<bool> jumpTrig;

	ofParameter<float> beat;
	ofParameter<int> ppq24;
	ofParameter<float> ppq24f;
	ofParameter<float> timeSeconds;
	ofParameter<float> bpm;

	ofEventListeners listeners;
};

#endif /* midiClockTransport_h */
