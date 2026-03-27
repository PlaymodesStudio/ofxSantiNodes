#pragma once

#include "ofxOceanodeNodeModel.h"
#include <deque>
#include <set>
#include <climits>
#include <cmath>

class jazzWalk : public ofxOceanodeNodeModel {
public:
    jazzWalk() : ofxOceanodeNodeModel("Jazz Walk") {}

    void setup() override {
        description = "Jazz-style melodic random walk with harmonic awareness, phrase shaping, expressive dynamics, motif repetition, trills, and tremolos.";

        addSeparator("HARMONY", ofColor(180, 120, 60));
        addParameter(scale.set("Scale",    {0,2,4,5,7,9,11}, {0}, {11}));
        addParameter(chord.set("Chord",    {0,4,7},           {0}, {11}));
        addParameter(chordStrength.set("ChordStr",  0.6f,  0.0f, 1.0f));

        addSeparator("RANGE & WALK", ofColor(80, 140, 200));
        addParameter(minPitch.set("MinPitch", 48, 0, 127));
        addParameter(maxPitch.set("MaxPitch", 84, 0, 127));
        addParameter(transpose.set("Transpose",     0.0f, -48.0f, 48.0f));
        addParameter(chance.set("Chance",    0.9f,  0.0f, 1.0f));
        addParameter(maxStep.set("MaxStep",  3,     1,    12));

        addSeparator("RHYTHM", ofColor(80, 180, 120));
        addParameter(durations.set("Durs", {0.25f,0.5f,0.5f,1.0f,2.0f}, {0.01f}, {16.0f}));
        addParameter(durBias.set("DurBias",  0.0f, -1.0f, 1.0f));
        addParameter(runProb.set("RunProb",  0.35f, 0.0f, 1.0f));
        addParameter(activity.set("Activity",   0.8f, 0.0f, 1.0f));
        addParameter(silenceChance.set("SilenceCh", 0.15f, 0.0f, 1.0f));

        addSeparator("DYNAMICS", ofColor(180, 80, 120));
        addParameter(minVel.set("MinVel",    0.4f,  0.0f, 1.0f));
        addParameter(maxVel.set("MaxVel",    0.9f,  0.0f, 1.0f));
        addParameter(expression.set("Expr",  0.7f,  0.0f, 1.0f));

        addSeparator("MOTIF", ofColor(140, 80, 200));
        addParameter(repeatChance.set("RepChance",  0.25f, 0.0f, 1.0f));
        addParameter(motifMutation.set("Mutation",  0.5f,  0.0f, 1.0f));

        addSeparator("TRILL", ofColor(200, 160, 60));
        addParameter(trillChance.set("TrillCh",     0.2f,  0.0f, 1.0f));
        addParameterDropdown(trillInterval, "TrillInt", 0, {"1st", "2nd", "3rd", "4th", "5th"});
        addParameterDropdown(trillSpeed,    "TrillSpd", 0, {"1/8", "1/16", "1/32"});

        addSeparator("TREMOLO", ofColor(200, 100, 60));
        addParameter(tremoloChance.set("TremCh",    0.2f,  0.0f, 1.0f));
        addParameter(tremoloDepth.set("TremDepth",  0.5f,  0.0f, 1.0f));
        addParameterDropdown(tremoloSpeed, "TremSpd", 3, {"1/8", "1/16", "1/32", "rnd"});

        addSeparator("TRANSPORT", ofColor(120, 120, 120));
        addParameter(resetParam.set("Reset"));

        addOutputParameter(pitchOut.set("Pitch",   60,   0, 127));
        addOutputParameter(velGate.set("VelGate",  0.0f, 0.0f, 1.0f));

        expandScales();
        if (!expandedScale.empty()) {
            currentScaleIndex = (int)expandedScale.size() / 2;
            currentPitch = expandedScale[currentScaleIndex];
        }
        nextNoteTime = ofGetElapsedTimeMillis();

        listeners.push(scale.newListener([this](vector<int>&) { expandScales(); }));
        listeners.push(chord.newListener([this](vector<int>&) { expandScales(); }));
        listeners.push(minPitch.newListener([this](int&)      { expandScales(); }));
        listeners.push(maxPitch.newListener([this](int&)      { expandScales(); }));
        listeners.push(resetParam.newListener([this]()        { onReset(); }));
    }

    void update(ofEventArgs& args) override {
        if (expandedScale.empty()) return;
        uint64_t now = ofGetElapsedTimeMillis();
        if (now >= nextNoteTime) generateAndFireNote();
    }

    void setBpm(float bpm) override {
        currentBPM = (bpm > 0) ? bpm : 120.0f;
    }

private:
    // ── Parameters ───────────────────────────────────────────────────────────
    ofParameter<vector<int>>   scale;
    ofParameter<vector<int>>   chord;
    ofParameter<vector<float>> durations;
    ofParameter<int>           minPitch;
    ofParameter<int>           maxPitch;
    ofParameter<float>         chance;
    ofParameter<int>           maxStep;
    ofParameter<float>         minVel;
    ofParameter<float>         maxVel;
    ofParameter<float>         expression;
    ofParameter<float>         chordStrength;
    ofParameter<float>         durBias;
    ofParameter<float>         runProb;
    ofParameter<float>         activity;
    ofParameter<float>         silenceChance;
    ofParameter<float>         repeatChance;
    ofParameter<float>         motifMutation;
    ofParameter<float>         transpose;
    ofParameter<float>         trillChance;
    ofParameter<int>           trillInterval;  // 0=1st .. 4=5th scale step up
    ofParameter<int>           trillSpeed;     // 0=1/8, 1=1/16, 2=1/32 beat per alternation
    ofParameter<float>         tremoloChance;
    ofParameter<float>         tremoloDepth;
    ofParameter<int>           tremoloSpeed;   // 0=1/8, 1=1/16, 2=1/32, 3=rnd
    ofParameter<void>          resetParam;

    // ── Outputs ───────────────────────────────────────────────────────────────
    ofParameter<int>   pitchOut;
    ofParameter<float> velGate;

    ofEventListeners listeners;

    // ── Expanded note sets ────────────────────────────────────────────────────
    vector<int> expandedScale;
    vector<int> expandedChord;

    // ── Walk state ────────────────────────────────────────────────────────────
    int   currentScaleIndex = 0;
    int   currentPitch      = 60;
    int   momentum          = 0;   // [-3 .. +3]

    // ── Timing ────────────────────────────────────────────────────────────────
    uint64_t nextNoteTime = 0;
    float    currentBPM   = 120.0f;

    // ── Velocity noise ────────────────────────────────────────────────────────
    float velNoisePhase = 0.0f;

    // ── Silence state ─────────────────────────────────────────────────────────
    int   silenceStreak = 0;
    float lastDuration  = 0.5f;

    // ── Phrase activity state ─────────────────────────────────────────────────
    // A "phrase" is the span between two long notes (natural phrase boundaries).
    // Activity decides at each boundary whether the performer plays or rests.
    bool     phraseResting    = false;
    uint64_t phraseRestEndTime = 0;

    // ── Motif state ───────────────────────────────────────────────────────────
    struct NoteEvent {
        int   scaleIndex;  // -1 = silence
        float duration;    // beats
        float velocity;
    };
    deque<NoteEvent>  recentNotes;
    vector<NoteEvent> motifBuffer;
    int  motifIndex        = 0;
    int  motifRepeat       = 0;
    int  motifRepeatsTotal = 0;
    bool inMotif           = false;

    // ── Ornament state machine ────────────────────────────────────────────────
    enum class OrnamentMode { None, TrillWaiting, Trill, TremoloWaiting, Tremolo };
    OrnamentMode ornamentMode = OrnamentMode::None;

    uint64_t noteEndTime       = 0;   // end of the note slot containing the ornament
    uint64_t ornamentStartTime = 0;   // mid-note moment when trill/tremolo begins

    // Trill
    bool  trillAlternate   = false;   // false = base note, true = upper note
    int   trillTargetIndex = 0;
    float currentNoteVel   = 0.0f;

    // Tremolo
    float tremoloBaseVel = 0.0f;
    float tremoloSpeedMs = 0.0f;
    float tremoloTotalMs = 0.0f;

    // ── Low-level helpers ─────────────────────────────────────────────────────

    float beatToMs(float beats) const {
        return (60.0f / currentBPM) * beats * 1000.0f;
    }

    bool isChordTone(int midiNote) const {
        int pc = midiNote % 12;
        for (int c : chord.get()) if (c % 12 == pc) return true;
        return false;
    }

    int transposedPitch(int midiNote) const {
        return std::clamp((int)std::round(midiNote + transpose.get()), 0, 127);
    }

    // Beat-interval per trill alternation (beats)
    float trillSubdivBeats() const {
        switch (trillSpeed.get()) {
            case 0:  return 0.5f;    // 1/8
            case 1:  return 0.25f;   // 1/16
            default: return 0.125f;  // 1/32
        }
    }

    float tremoloSpeedBeats() const {
        switch (tremoloSpeed.get()) {
            case 0:  return 0.5f;                       // 1/8
            case 1:  return 0.25f;                      // 1/16
            case 2:  return 0.125f;                     // 1/32
            default: return ofRandom(0.125f, 0.5f);     // rnd between 1/32 and 1/8
        }
    }

    void expandScales() {
        expandedScale.clear();
        expandedChord.clear();
        std::set<int> scalePCs, chordPCs;
        for (int p : scale.get()) scalePCs.insert(p % 12);
        for (int p : chord.get()) chordPCs.insert(p % 12);
        int lo = minPitch.get(), hi = maxPitch.get();
        if (lo > hi) std::swap(lo, hi);
        for (int note = lo; note <= hi; note++) {
            int pc = note % 12;
            if (scalePCs.count(pc)) expandedScale.push_back(note);
            if (chordPCs.count(pc)) expandedChord.push_back(note);
        }
        if (!expandedScale.empty()) {
            currentScaleIndex = std::clamp(currentScaleIndex, 0, (int)expandedScale.size() - 1);
            currentPitch = expandedScale[currentScaleIndex];
        }
    }

    void onReset() {
        // Only correct the phase — momentum, motif, and silence flow are preserved.
        // Any active ornament will self-resolve on the next tick via noteEndTime check.
        nextNoteTime = ofGetElapsedTimeMillis();
    }

    // ── Note-selection helpers ────────────────────────────────────────────────

    bool shouldBeSilence() const {
        // Within-phrase breathing only — Activity is handled at phrase level, not note level.
        float prob = 0.05f;
        if (lastDuration >= 1.0f) prob += 0.25f;                              // post-phrase breath
        if (silenceStreak > 0)    prob += 0.30f * std::min(silenceStreak, 2); // clustering
        if (expandedScale.size() > 1) {
            float rel = (float)currentScaleIndex / (float)(expandedScale.size() - 1);
            if (rel < 0.1f || rel > 0.9f) prob += 0.15f;                      // register extremes
        }
        return ofRandom(1.0f) < (prob * silenceChance.get());
    }

    // Called at each phrase boundary (long note fired). If Activity says rest,
    // schedules a silent period whose length grows with lower activity.
    void tryEnterPhraseRest(uint64_t noteEndMs) {
        if (ofRandom(1.0f) < activity.get()) return;  // performer keeps playing
        float act = activity.get();
        // Rest duration: 1–5 beats, longer when activity is lower
        float restBeats = ofRandom(1.0f, 1.0f + 4.0f * (1.0f - act));
        phraseResting     = true;
        phraseRestEndTime = noteEndMs + (uint64_t)beatToMs(restBeats);
    }

    int selectNextScaleIndex() {
        if (expandedScale.empty()) return 0;
        if (ofRandom(1.0f) > chance.get()) return currentScaleIndex;

        int step = maxStep.get(), size = (int)expandedScale.size();
        float cs = chordStrength.get();

        int nearestChordIdx = currentScaleIndex, minDist = INT_MAX;
        for (int si = 0; si < size; si++) {
            if (isChordTone(expandedScale[si])) {
                int d = abs(si - currentScaleIndex);
                if (d > 0 && d < minDist) { minDist = d; nearestChordIdx = si; }
            }
        }
        int gravDir = (nearestChordIdx > currentScaleIndex) ?  1 :
                      (nearestChordIdx < currentScaleIndex) ? -1 : 0;

        vector<pair<int,float>> candidates;
        for (int delta = -step; delta <= step; delta++) {
            if (delta == 0) continue;
            int idx = currentScaleIndex + delta;
            if (idx < 0 || idx >= size) continue;
            float w = 1.0f;
            if (momentum != 0) {
                int ms = (momentum > 0) ? 1 : -1;
                if ((delta > 0) == (ms > 0))
                    w *= 1.0f + std::min(abs(momentum), 3) * 0.25f;
            }
            if (isChordTone(expandedScale[idx]))
                w *= 1.0f + cs * 3.0f;
            if (gravDir != 0 && cs > 0 && (delta > 0) == (gravDir > 0))
                w *= 1.0f + cs * 1.5f;
            candidates.push_back({idx, w});
        }
        if (candidates.empty()) return currentScaleIndex;

        float total = 0; for (auto& c : candidates) total += c.second;
        float r = ofRandom(total), acc = 0;
        for (auto& c : candidates) { acc += c.second; if (r <= acc) return c.first; }
        return candidates.back().first;
    }

    float selectDuration(int scaleIndex) {
        const auto& durs = durations.get();
        if (durs.empty()) return 0.5f;
        bool isChord = (!expandedScale.empty() && scaleIndex >= 0) && isChordTone(expandedScale[scaleIndex]);
        float cs   = chordStrength.get();
        float bias = durBias.get();

        vector<float> weights;
        for (float d : durs) {
            // Harmonic shaping: sqrt(d) softens the long-note bias for chord tones
            float w = isChord ? (1.0f*(1.0f-cs) + std::sqrt(d)*cs)
                              : (1.0f*(1.0f-cs) + (1.0f/(d+0.01f))*cs);
            // DurBias: negative = favour short (d^neg = large for small d), positive = favour long
            w *= std::pow(d + 0.01f, bias);
            weights.push_back(std::max(w, 0.01f));
        }

        // RunProb: after a short note, probabilistically lock into short durations
        if (lastDuration <= 0.5f && ofRandom(1.0f) < runProb.get()) {
            float total = 0;
            vector<pair<int,float>> shortCands;
            for (int i = 0; i < (int)durs.size(); i++) {
                if (durs[i] <= 0.5f) { shortCands.push_back({i, weights[i]}); total += weights[i]; }
            }
            if (!shortCands.empty()) {
                float r = ofRandom(total), acc = 0;
                for (auto& p : shortCands) { acc += p.second; if (r <= acc) return durs[p.first]; }
                return durs[shortCands.back().first];
            }
        }

        float total = 0; for (float w : weights) total += w;
        float r = ofRandom(total), acc = 0;
        for (int i = 0; i < (int)durs.size(); i++) { acc += weights[i]; if (r <= acc) return durs[i]; }
        return durs.back();
    }

    float selectSilenceDuration() const {
        const auto& durs = durations.get();
        if (durs.empty()) return 0.5f;
        vector<float> weights;
        for (float d : durs) weights.push_back(1.0f / (d + 0.1f));
        float total = 0; for (float w : weights) total += w;
        float r = ofRandom(total), acc = 0;
        for (int i = 0; i < (int)durs.size(); i++) { acc += weights[i]; if (r <= acc) return durs[i]; }
        return durs.front();
    }

    float selectVelocity(int scaleIndex) {
        velNoisePhase += 0.07f;
        float noiseVal = ofNoise(velNoisePhase);
        float lo = minVel.get(), hi = maxVel.get();
        float base = ofMap(noiseVal, 0.0f, 1.0f, lo, hi);
        float vel  = (lo+hi)*0.5f*(1.0f-expression.get()) + base*expression.get();
        if (!expandedScale.empty() && scaleIndex >= 0 && isChordTone(expandedScale[scaleIndex]))
            vel += chordStrength.get() * 0.15f * (hi - vel);
        return std::clamp(vel, lo, hi);
    }

    void updateMomentum(int oldIdx, int newIdx) {
        int d = newIdx - oldIdx;
        if      (d > 0) momentum = std::min(momentum + 1,  3);
        else if (d < 0) momentum = std::max(momentum - 1, -3);
        if (ofRandom(1.0f) < 0.25f) {
            if      (momentum > 0) momentum--;
            else if (momentum < 0) momentum++;
        }
    }

    void tryCaptureMotif() {
        if (inMotif || (int)recentNotes.size() < 2) return;
        if (ofRandom(1.0f) > repeatChance.get()) return;
        int len = std::min(2 + (int)ofRandom(4.0f), (int)recentNotes.size());
        motifBuffer.clear();
        for (int i = (int)recentNotes.size() - len; i < (int)recentNotes.size(); i++)
            motifBuffer.push_back(recentNotes[i]);
        motifIndex = 0; motifRepeat = 0;
        motifRepeatsTotal = 1 + (int)ofRandom(3.0f);
        inMotif = true;
    }

    // ── Ornament scheduling ───────────────────────────────────────────────────

    // Called after firing a new note. May override nextNoteTime to schedule a
    // mid-note ornament start. Only when not in a motif.
    void tryScheduleOrnament(float duration, float vel, uint64_t now) {
        float noteDurMs = beatToMs(duration);
        float tc  = trillChance.get();
        float trc = tremoloChance.get();
        float roll = ofRandom(1.0f);

        int   steps     = trillInterval.get() + 1;  // 1st=1 .. 5th=5
        int   trillTgt  = currentScaleIndex + steps;
        bool  canTrill  = (trillTgt < (int)expandedScale.size());
        bool  canTremolo = (duration >= 0.75f);

        if (roll < tc && canTrill) {
            // Trill: starts 30–60 % into the note
            float frac     = ofRandom(0.30f, 0.60f);
            ornamentStartTime = now + (uint64_t)(noteDurMs * frac);
            noteEndTime       = now + (uint64_t)noteDurMs;
            trillTargetIndex  = trillTgt;
            trillAlternate    = false;
            currentNoteVel    = vel;
            ornamentMode      = OrnamentMode::TrillWaiting;
            nextNoteTime      = ornamentStartTime;

        } else if (roll < tc + trc && canTremolo) {
            // Tremolo: starts 10–35 % into the note
            float frac     = ofRandom(0.10f, 0.35f);
            ornamentStartTime = now + (uint64_t)(noteDurMs * frac);
            noteEndTime       = now + (uint64_t)noteDurMs;
            tremoloBaseVel    = vel;
            tremoloTotalMs    = (float)(noteEndTime - ornamentStartTime);
            // Random speed: 1/16 – 1/5 of a beat per sub-event
            tremoloSpeedMs    = beatToMs(tremoloSpeedBeats());
            ornamentMode      = OrnamentMode::TremoloWaiting;
            nextNoteTime      = ornamentStartTime;
        }
    }

    // ── Trill ─────────────────────────────────────────────────────────────────

    void startTrill(uint64_t now) {
        if (now >= noteEndTime) { ornamentMode = OrnamentMode::None; generateNewNote(now); return; }
        ornamentMode   = OrnamentMode::Trill;
        trillAlternate = true;   // first event fires the upper note
        fireTrill(now);
    }

    void stepTrill(uint64_t now) {
        if (now >= noteEndTime) { ornamentMode = OrnamentMode::None; generateNewNote(now); return; }
        trillAlternate = !trillAlternate;
        fireTrill(now);
    }

    void fireTrill(uint64_t now) {
        int idx = std::clamp(trillAlternate ? trillTargetIndex : currentScaleIndex,
                             0, (int)expandedScale.size() - 1);
        pitchOut     = transposedPitch(expandedScale[idx]);
        velGate      = currentNoteVel;
        nextNoteTime = now + (uint64_t)beatToMs(trillSubdivBeats());
    }

    // ── Tremolo ───────────────────────────────────────────────────────────────

    void startTremolo(uint64_t now) {
        if (now >= noteEndTime) { ornamentMode = OrnamentMode::None; generateNewNote(now); return; }
        ornamentMode = OrnamentMode::Tremolo;
        stepTremolo(now);
    }

    void stepTremolo(uint64_t now) {
        if (now >= noteEndTime) {
            velGate      = tremoloBaseVel;   // restore before moving on
            ornamentMode = OrnamentMode::None;
            generateNewNote(now);
            return;
        }
        float elapsed  = (float)(now - ornamentStartTime);
        float envPhase = std::clamp(elapsed / tremoloTotalMs, 0.0f, 1.0f);
        float envelope = std::sin(envPhase * (float)M_PI);               // bell 0→1→0
        float osc      = std::sin(elapsed / tremoloSpeedMs * 2.0f * (float)M_PI); // ±1
        float swing    = (maxVel.get() - minVel.get()) * 0.5f;
        float vel      = tremoloBaseVel + osc * tremoloDepth.get() * envelope * swing;
        velGate      = std::clamp(vel, minVel.get(), maxVel.get());
        nextNoteTime = now + (uint64_t)tremoloSpeedMs;
    }

    // ── Main note generation ──────────────────────────────────────────────────

    void generateAndFireNote() {
        uint64_t now = ofGetElapsedTimeMillis();

        switch (ornamentMode) {
            case OrnamentMode::TrillWaiting:   startTrill(now);   return;
            case OrnamentMode::Trill:          stepTrill(now);    return;
            case OrnamentMode::TremoloWaiting: startTremolo(now); return;
            case OrnamentMode::Tremolo:        stepTremolo(now);  return;
            default: break;
        }

        if (inMotif && !motifBuffer.empty()) { playMotifNote(now); return; }

        generateNewNote(now);
    }

    void generateNewNote(uint64_t now) {
        // ── Phrase-level rest (Activity) ─────────────────────────────────────
        if (phraseResting) {
            if (now < phraseRestEndTime) {
                // Still resting: stay silent until rest period ends
                velGate      = 0.0f;
                nextNoteTime = phraseRestEndTime;
                return;
            }
            // Rest period over: roll again — performer may stay silent or return
            phraseResting = false;
            if (ofRandom(1.0f) >= activity.get()) {
                // Still not ready to play: schedule another rest
                float restBeats = ofRandom(1.0f, 1.0f + 4.0f * (1.0f - activity.get()));
                phraseResting     = true;
                phraseRestEndTime = now + (uint64_t)beatToMs(restBeats);
                velGate      = 0.0f;
                nextNoteTime = phraseRestEndTime;
                return;
            }
            // Performer re-enters: clear silence streak so next note fires cleanly
            silenceStreak = 0;
        }

        // ── Within-phrase silence (SilenceCh) ────────────────────────────────
        if (shouldBeSilence()) {
            float dur = selectSilenceDuration();
            velGate = 0.0f;
            silenceStreak++;
            lastDuration = dur;
            nextNoteTime = now + (uint64_t)beatToMs(dur);
            NoteEvent evt{ -1, dur, 0.0f };
            recentNotes.push_back(evt);
            if (recentNotes.size() > 8) recentNotes.pop_front();
            return;
        }

        silenceStreak = 0;
        int   newIdx = selectNextScaleIndex();
        float dur    = selectDuration(newIdx);
        float vel    = selectVelocity(newIdx);

        updateMomentum(currentScaleIndex, newIdx);
        currentScaleIndex = newIdx;
        currentPitch      = expandedScale[newIdx];

        pitchOut     = transposedPitch(currentPitch);
        velGate      = vel;
        lastDuration = dur;
        nextNoteTime = now + (uint64_t)beatToMs(dur);

        NoteEvent evt{ newIdx, dur, vel };
        recentNotes.push_back(evt);
        if (recentNotes.size() > 8) recentNotes.pop_front();

        // Long note = phrase boundary: Activity decides whether to rest next
        if (dur >= 1.0f) {
            tryCaptureMotif();
            if (!inMotif) tryEnterPhraseRest(now + (uint64_t)beatToMs(dur));
        }

        // Ornaments only on plain notes (not inside motif playback)
        if (!inMotif) tryScheduleOrnament(dur, vel, now);
    }

    void playMotifNote(uint64_t now) {
        NoteEvent evt = motifBuffer[motifIndex];

        if (evt.scaleIndex >= 0 && motifMutation.get() > 0.5f && momentum != 0) {
            int offset = motifRepeat * ((momentum > 0) ? 1 : -1);
            evt.scaleIndex = std::clamp(evt.scaleIndex + offset, 0, (int)expandedScale.size() - 1);
        }

        if (evt.scaleIndex >= 0 && !expandedScale.empty()) {
            currentScaleIndex = evt.scaleIndex;
            currentPitch      = expandedScale[currentScaleIndex];
            pitchOut = transposedPitch(currentPitch);
            velGate  = evt.velocity;
        } else {
            velGate = 0.0f;
        }

        lastDuration = evt.duration;
        nextNoteTime = now + (uint64_t)beatToMs(evt.duration);

        motifIndex++;
        if (motifIndex >= (int)motifBuffer.size()) {
            motifIndex = 0; motifRepeat++;
            if (motifRepeat >= motifRepeatsTotal) {
                inMotif  = false;
                momentum = -momentum;
            }
        }
    }
};
