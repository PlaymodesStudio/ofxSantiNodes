#pragma once

#include "ofxOceanodeNodeModel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <random>
#include <set>
#include <climits>
#include <cmath>

class melodicMutation : public ofxOceanodeNodeModel {
public:
    melodicMutation() : ofxOceanodeNodeModel("Melodic Mutation") {}

    void setup() override {
        description = "Seed-based melodic sequencer with step-wise musical mutation. "
                      "MelodySeed generates the base melody; MutSeed governs mutation flavour. "
                      "Mutate/Demutate navigate a branching history applied at the next loop boundary.";

        addSeparator("HARMONY", ofColor(180, 120, 60));
        addParameter(scale.set("Scale",   {0,2,4,5,7,9,11}, {0}, {11}));
        addParameter(chord.set("Chord",   {0,4,7},           {0}, {11}));
        addParameter(chordStr.set("ChordStr", 0.6f, 0.0f, 1.0f));

        addSeparator("RANGE & MELODY", ofColor(80, 140, 200));
        addParameter(minPitch.set("MinPitch",    48, 0, 127));
        addParameter(maxPitch.set("MaxPitch",    84, 0, 127));
        addParameter(melodySeed.set("MelSeed",  0, 0, 9999));
        addParameter(mutationSeed.set("MutSeed",  42, 0, 9999));
        addParameter(length.set("Length", 8, 1, 64));
        addParameter(lengthInBeats.set("LenBeats", false));
        addParameter(maxJump.set("MaxJump", 3, 1, 12));
        addParameter(durations.set("Durs",     {0.25f, 0.5f, 0.5f, 1.0f}, {0.01f}, {16.0f}));
        addParameter(transpose.set("Transpose", 0.0f, -48.0f, 48.0f));

        addSeparator("DYNAMICS", ofColor(180, 80, 120));
        addParameter(minVel.set("MinVel",       0.4f, 0.0f, 1.0f));
        addParameter(maxVel.set("MaxVel",       0.9f, 0.0f, 1.0f));
        addParameter(silenceProb.set("SilProb", 0.0f, 0.0f, 1.0f));
        addParameter(noteChance.set("Note%",    1.0f, 0.0f, 1.0f));
        addParameter(seqChance.set("Seq%",      1.0f, 0.0f, 1.0f));

        addSeparator("MUTATION", ofColor(100, 200, 120));
        addParameter(mutation.set("Mutation",  0.3f, 0.0f, 1.0f));
        addParameter(mutPitch.set("PitchMut",  0.7f, 0.0f, 1.0f));
        addParameter(mutDur.set("DurMut",      0.4f, 0.0f, 1.0f));
        addParameter(mutVel.set("VelMut",      0.3f, 0.0f, 1.0f));
        addParameter(mutSilence.set("SilMut",  0.1f, 0.0f, 1.0f));
        addParameter(mutateParam.set("Mutate"));
        addParameter(demutateParam.set("Demutate"));
        addCustomRegion(melodyRegion, [this]() { drawMelodyViz(); });
        addInspectorParameter(widgetWidth.set("Widget Width",   240.0f, 80.0f, 600.0f));
        addInspectorParameter(widgetHeight.set("Widget Height",  90.0f, 50.0f, 300.0f));

        addSeparator("TRANSPORT", ofColor(120, 120, 120));
        addParameter(phasorIn.set("1beatPh", {}, {0.0f}, {1.0f}));
        addParameter(loop.set("Loop", true));
        addParameter(triggerParam.set("Trigger"));
        addParameter(resetParam.set("Reset"));

        addOutputParameter(pitchOut.set("Pitch",   60,   0, 127));
        addOutputParameter(velGate.set("VelGate",  0.0f, 0.0f, 1.0f));

        // Scale/range/seed changes rebuild the whole melody and clear history
        auto rebuildVec   = [this](vector<int>&) { expandScales(); rebuildFromSeed(); };
        auto rebuildInt   = [this](int&)         { expandScales(); rebuildFromSeed(); };
        auto rebuildFloat = [this](float&)       { rebuildFromSeed(); };
        listeners.push(scale.newListener(rebuildVec));
        listeners.push(chord.newListener(rebuildVec));
        listeners.push(minPitch.newListener(rebuildInt));
        listeners.push(maxPitch.newListener(rebuildInt));
        listeners.push(melodySeed.newListener(rebuildInt));
        listeners.push(length.newListener(rebuildInt));
        listeners.push(lengthInBeats.newListener([this](bool&) { rebuildFromSeed(); }));
        listeners.push(silenceProb.newListener(rebuildFloat));

        listeners.push(phasorIn.newListener([this](vector<float>& phasor) {
            if (phasor.empty()) return;
            // Keep lastPhasorVal updated even when not playing
            if (!isPlaying || history.empty()) {
                lastPhasorVal = phasor.back();
                return;
            }
            for (float phase : phasor) {
                if (lastPhasorVal >= 0.0f) {
                    float delta = phase - lastPhasorVal;
                    if (delta < -0.5f) delta += 1.0f;  // wrap-around
                    beatAccum += delta;
                }
                lastPhasorVal = phase;
                while (isPlaying && beatAccum >= nextNoteBeat) {
                    fireCurrentNote();
                }
            }
        }));

        listeners.push(triggerParam.newListener([this]()  { onTrigger();  }));
        listeners.push(resetParam.newListener([this]()    { onReset();    }));
        listeners.push(mutateParam.newListener([this]()   { onMutate();   }));
        listeners.push(demutateParam.newListener([this]() { onDemutate(); }));

        // Initialize chance RNG with a different seed
        chanceRng.seed(std::random_device{}());
        
        expandScales();
        rebuildFromSeed();
    }

    void update(ofEventArgs&) override {
        // Wall-clock fallback: only used when no phasor is connected
        if (!phasorIn.get().empty()) return;
        if (!isPlaying || history.empty()) return;
        uint64_t now = ofGetElapsedTimeMillis();
        if (now >= nextNoteTime) fireCurrentNote();
    }

    void setBpm(float bpm) override {
        currentBPM = (bpm > 0.0f) ? bpm : 120.0f;
    }

private:
    // ── Parameters ─────────────────────────────────────────────────────────────
    ofParameter<vector<int>>   scale;
    ofParameter<vector<int>>   chord;
    ofParameter<float>         chordStr;
    ofParameter<int>           minPitch;
    ofParameter<int>           maxPitch;
    ofParameter<float>         transpose;
    ofParameter<int>           melodySeed;
    ofParameter<int>           mutationSeed;
    ofParameter<int>           length;
    ofParameter<bool>          lengthInBeats;
    ofParameter<vector<float>> durations;
    ofParameter<float>         minVel;
    ofParameter<float>         maxVel;
    ofParameter<float>         silenceProb;
    ofParameter<float>         noteChance;
    ofParameter<float>         seqChance;
    ofParameter<float>         mutation;
    ofParameter<float>         mutPitch;
    ofParameter<float>         mutDur;
    ofParameter<float>         mutVel;
    ofParameter<float>         mutSilence;
    ofParameter<int>           maxJump;
    ofParameter<void>          mutateParam;
    ofParameter<void>          demutateParam;
    ofParameter<bool>          loop;
    ofParameter<void>          triggerParam;
    ofParameter<void>          resetParam;
    ofParameter<float>         widgetWidth;
    ofParameter<float>         widgetHeight;
    customGuiRegion            melodyRegion;

    // ── Phasor input (optional — gives sample-accurate timing) ────────────────
    ofParameter<vector<float>> phasorIn;

    // ── Outputs ────────────────────────────────────────────────────────────────
    ofParameter<int>   pitchOut;
    ofParameter<float> velGate;

    ofEventListeners listeners;

    // ── Scale state ────────────────────────────────────────────────────────────
    vector<int>  expandedScale;
    vector<int>  expandedChord;

    // ── Melody history ─────────────────────────────────────────────────────────
    struct MelodyNote {
        int   scaleIndex;
        float duration;
        float velocity;
    };
    vector<vector<MelodyNote>> history;   // [0] = seed state, always preserved
    int historyIndex      = 0;
    int mutationCallCount = 0;            // monotonic — never decremented

    // ── Pending changes (applied at loop boundary while playing) ───────────────
    bool pendingMutate   = false;
    bool pendingDemutate = false;

    // ── Playback ───────────────────────────────────────────────────────────────
    bool     isPlaying        = false;
    int      currentNoteIndex = 0;
    uint64_t nextNoteTime     = 0;
    float    currentBPM       = 120.0f;
    // Phasor-based timing (sample-accurate when phasor is connected)
    float    beatAccum        = 0.0f;   // monotonic total beats from phasor
    float    nextNoteBeat     = 0.0f;   // beat position to fire the next note
    float    lastPhasorVal    = -1.0f;  // previous phasor sample for delta/wrap detection
    
    // Chance control state
    std::mt19937 chanceRng;             // separate RNG for chance calculations
    bool         seqGateActive = true;  // current sequence gate state

    // ── Low-level helpers ──────────────────────────────────────────────────────

    float beatToMs(float beats) const {
        return (60.0f / currentBPM) * beats * 1000.0f;
    }

    bool isChordTone(int midiNote) const {
        int pc = midiNote % 12;
        for (int c : chord.get()) if (c % 12 == pc) return true;
        return false;
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
    }

    // Chord-biased duration pick with a seeded rng (same formula as jazzWalk)
    float pickDuration(int scaleIdx, std::mt19937& rng) const {
        const auto& durs = durations.get();
        if (durs.empty()) return 0.5f;
        float cs = chordStr.get();
        bool  ic = (!expandedScale.empty() && scaleIdx >= 0) &&
                   isChordTone(expandedScale[scaleIdx]);
        std::uniform_real_distribution<float> d(0.0f, 1.0f);
        vector<float> w;
        w.reserve(durs.size());
        for (float dur : durs) {
            float wt = ic ? (1.0f*(1.0f-cs) + std::sqrt(dur)*cs)
                          : (1.0f*(1.0f-cs) + (1.0f/(dur+0.01f))*cs);
            w.push_back(std::max(wt, 0.01f));
        }
        float total = 0.0f; for (float wt : w) total += wt;
        float r = d(rng) * total, acc = 0.0f;
        for (int i = 0; i < (int)durs.size(); i++) {
            acc += w[i]; if (r <= acc) return durs[i];
        }
        return durs.back();
    }

    // ── Melody generation ──────────────────────────────────────────────────────

    void rebuildFromSeed() {
        if (expandedScale.empty()) return;

        std::mt19937 rng((uint32_t)melodySeed.get());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        auto randF = [&]() { return dist(rng); };

        bool  byBeats     = lengthInBeats.get();
        int   n           = std::clamp(length.get(), 1, 64);
        float targetBeats = (float)n;
        // Safety cap in beats mode: enough room for all-shortest-note case, max 1024
        float minDur = 0.25f;
        for (float d : durations.get()) if (d > 0.0f && d < minDur) minDur = d;
        int   maxNotes    = byBeats ? std::min((int)(targetBeats / minDur) + 4, 1024) : n;
        int   size       = (int)expandedScale.size();
        float cs         = chordStr.get();
        int   idx        = size / 2;
        int   mom        = 0;
        float totalBeats = 0.0f;

        vector<MelodyNote> melody;
        melody.reserve(byBeats ? 32 : n);

        for (int i = 0; byBeats ? (totalBeats < targetBeats && i < maxNotes) : (i < n); i++) {
            // Nearest chord tone for harmonic gravity
            int nearestChord = idx, minDist = INT_MAX;
            for (int si = 0; si < size; si++) {
                if (isChordTone(expandedScale[si])) {
                    int d = abs(si - idx);
                    if (d > 0 && d < minDist) { minDist = d; nearestChord = si; }
                }
            }
            int gravDir = (nearestChord > idx) ? 1 : (nearestChord < idx) ? -1 : 0;

            // Weighted step candidates — leaps (>=3 steps) must land on a chord tone.
            // Two-pass: first with the leap constraint, then without if it yields nothing.
            int mj = std::clamp(maxJump.get(), 1, 12);
            auto buildCands = [&](bool leapConstraint) {
                vector<pair<int,float>> out;
                for (int delta = -mj; delta <= mj; delta++) {
                    if (delta == 0) continue;
                    int ni = idx + delta;
                    if (ni < 0 || ni >= size) continue;
                    bool ic = isChordTone(expandedScale[ni]);
                    if (leapConstraint && std::abs(delta) >= 3 && !ic) continue;
                    float wt = 1.0f;
                    if (mom != 0) {
                        int ms = (mom > 0) ? 1 : -1;
                        if ((delta > 0) == (ms > 0)) wt *= 1.0f + std::min(abs(mom), 3) * 0.25f;
                    }
                    if (ic)                                            wt *= 1.0f + cs * 3.0f;
                    if (gravDir != 0 && (delta > 0) == (gravDir > 0)) wt *= 1.0f + cs * 1.5f;
                    out.push_back({ni, wt});
                }
                return out;
            };
            vector<pair<int,float>> cands = buildCands(true);
            if (cands.empty()) cands = buildCands(false);  // fallback: relax leap constraint

            int newIdx = idx;
            if (!cands.empty()) {
                float total = 0.0f; for (auto& c : cands) total += c.second;
                float r = randF() * total, acc = 0.0f;
                newIdx = cands.back().first;
                for (auto& c : cands) { acc += c.second; if (r <= acc) { newIdx = c.first; break; } }
            }

            // Update momentum
            int d = newIdx - idx;
            if      (d > 0) mom = std::min(mom + 1,  3);
            else if (d < 0) mom = std::max(mom - 1, -3);
            if (randF() < 0.25f) { if (mom > 0) mom--; else if (mom < 0) mom++; }
            idx = newIdx;

            float dur = pickDuration(idx, rng);
            totalBeats += dur;
            float lo  = minVel.get(), hi = maxVel.get();
            float vel = lo + randF() * (hi - lo);
            if (isChordTone(expandedScale[idx]))
                vel = std::min(vel + cs * 0.1f * (hi - lo), hi);

            // SilProb: rest probability baked into the seed melody
            // The walk position still advances so the contour stays coherent
            bool isSilence = (silenceProb.get() > 0.0f && randF() < silenceProb.get());
            melody.push_back({isSilence ? -1 : idx, dur, isSilence ? 0.0f : vel});
        }

        // In beats mode: trim the last note so the total is exactly targetBeats.
        // Without this, the last note always overshoots by up to max(Durs) beats.
        if (byBeats && !melody.empty() && totalBeats > targetBeats) {
            float excess = totalBeats - targetBeats;
            melody.back().duration = std::max(melody.back().duration - excess, 0.01f);
        }

        // Clear all history: old mutation chains are meaningless after a rebuild
        history.clear();
        history.push_back(std::move(melody));
        historyIndex      = 0;
        mutationCallCount = 0;
        pendingMutate     = false;
        pendingDemutate   = false;

        if (isPlaying) {
            currentNoteIndex = 0;
            nextNoteTime     = ofGetElapsedTimeMillis();
        }
    }

    // ── Mutation ───────────────────────────────────────────────────────────────

    // Core mutation logic, separated so it can be called immediately or deferred
    void applyMutation() {
        if (history.empty()) return;
        int size = (int)expandedScale.size();
        if (size == 0) return;

        vector<MelodyNote> newMelody = history[historyIndex];
        int n = (int)newMelody.size();
        if (n == 0) return;

        // Each Mutate press uses MutSeed + a monotonic counter:
        // — same MutSeed from the same history position → reproducible mutations
        // — change MutSeed → different flavour; demutate still walks back through
        //   the stored snapshots, so the original melody is always recoverable
        std::mt19937 rng((uint32_t)((uint64_t)mutationSeed.get() * 1000003ULL
                                    + (uint64_t)mutationCallCount));
        mutationCallCount++;   // never decremented — ensures branching uniqueness

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        auto randF = [&]() { return dist(rng); };

        float mut = mutation.get();
        float cs  = chordStr.get();
        int numChanges = 1 + (int)(mut * 2.0f);                                          // 1..3
        int maxDrift   = std::min(1 + (int)(mutPitch.get() * 3.0f), maxJump.get());  // ±1..±4, capped by MaxJump

        // Harmonic instability weights: non-chord tones mutate preferentially
        vector<float> weights(n);
        for (int i = 0; i < n; i++) {
            int si = newMelody[i].scaleIndex;
            bool ic = (si >= 0 && si < size) && isChordTone(expandedScale[si]);
            weights[i] = ic ? (1.0f - cs * 0.5f) : (1.0f + cs * 0.5f);
        }

        // Weighted unique-index selection
        std::set<int> changed;
        for (int c = 0; c < numChanges && (int)changed.size() < n; c++) {
            float total = 0.0f;
            for (int i = 0; i < n; i++) if (!changed.count(i)) total += weights[i];
            float r = randF() * total, acc = 0.0f;
            for (int i = 0; i < n; i++) {
                if (changed.count(i)) continue;
                acc += weights[i];
                if (r <= acc) { changed.insert(i); break; }
            }
        }

        for (int i : changed) {
            MelodyNote& note = newMelody[i];

            // Pitch: drift ±maxDrift, biased toward chord tones on landing
            int drift  = (int)(randF() * (float)(2 * maxDrift + 1)) - maxDrift;
            if (drift == 0) drift = (randF() < 0.5f) ? 1 : -1;
            int newIdx = std::clamp(note.scaleIndex + drift, 0, size - 1);
            bool newIc = isChordTone(expandedScale[newIdx]);
            float ap   = newIc ? (0.7f + cs * 0.3f) : (1.0f - cs * 0.4f);
            // Leaps of ≥3 scale steps must land on a chord tone — reject otherwise
            if (std::abs(newIdx - note.scaleIndex) >= 3 && !newIc) ap = 0.0f;
            if (randF() < ap) note.scaleIndex = newIdx;

            // Duration: step ±1 in the palette (probability controlled by DurMut)
            if (randF() < mutDur.get()) {
                const auto& durs = durations.get();
                if ((int)durs.size() > 1) {
                    int di = 0;
                    float best = std::abs(durs[0] - note.duration);
                    for (int j = 1; j < (int)durs.size(); j++) {
                        float dist2 = std::abs(durs[j] - note.duration);
                        if (dist2 < best) { best = dist2; di = j; }
                    }
                    int newDi = std::clamp(di + (randF() < 0.5f ? -1 : 1),
                                           0, (int)durs.size() - 1);
                    note.duration = durs[newDi];
                }
            }

            // Velocity: nudge within [minVel, maxVel], magnitude controlled by VelMut
            if (note.scaleIndex >= 0) {
                float lo = minVel.get(), hi = maxVel.get();
                note.velocity = std::clamp(note.velocity
                                           + (randF() * 2.0f - 1.0f) * mutVel.get() * (hi - lo) * 0.4f,
                                           lo, hi);
            }

            // Silence: toggle between rest and note with SilMut probability
            if (randF() < mutSilence.get()) {
                if (note.scaleIndex < 0) {
                    // Restore rest to a note: place near the current register centre
                    int centre = std::clamp(size / 2 + (int)(randF() * 5.0f) - 2, 0, size - 1);
                    note.scaleIndex = centre;
                } else {
                    note.scaleIndex = -1;   // mute this note
                }
            }
        }

        // Branching model: new mutation discards any forward history
        if (historyIndex < (int)history.size() - 1)
            history.resize(historyIndex + 1);

        history.push_back(std::move(newMelody));
        historyIndex++;
    }

    // ── Transport handlers ─────────────────────────────────────────────────────

    void onTrigger() {
        // Phase reset only: restart melody from note 0, history untouched
        isPlaying        = true;
        currentNoteIndex = 0;
        nextNoteTime     = ofGetElapsedTimeMillis();
        beatAccum        = 0.0f;
        nextNoteBeat     = 0.0f;
    }

    void onReset() {
        // Demutation only: revert to seed state, playback position untouched
        historyIndex      = 0;
        mutationCallCount = 0;
        pendingMutate     = false;
        pendingDemutate   = false;
    }

    void onMutate() {
        if (!isPlaying) { applyMutation(); return; }  // immediate if stopped
        pendingMutate   = true;
        pendingDemutate = false;  // only one pending change at a time
    }

    void onDemutate() {
        if (!isPlaying) { if (historyIndex > 0) historyIndex--; return; }
        pendingDemutate = true;
        pendingMutate   = false;
        // mutationCallCount is NOT decremented: next Mutate from this position
        // uses a fresh sub-seed, giving a different flavour without losing history
    }

    // ── Visualization ──────────────────────────────────────────────────────────

    void drawMelodyViz() {
        if (expandedScale.empty() || history.empty()) return;

        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        const float W   = widgetWidth.get();
        const float H   = widgetHeight.get();
        const float pad = 5.0f;
        const float dW  = W - pad * 2.0f;
        const float dH  = H - pad * 2.0f;

        // Background + border
        dl->AddRectFilled(pos, ImVec2(pos.x + W, pos.y + H), IM_COL32(18, 18, 22, 255), 3.0f);
        dl->AddRect      (pos, ImVec2(pos.x + W, pos.y + H), IM_COL32(55, 55, 65, 255),  3.0f);

        // Reserve ImGui layout space
        ImGui::Dummy(ImVec2(W, H));

        // Pitch → Y  (higher pitch = lower Y = higher on screen)
        int   pitchLo = minPitch.get(), pitchHi = maxPitch.get();
        if (pitchLo > pitchHi) std::swap(pitchLo, pitchHi);
        float pitchRange = std::max(1.0f, (float)(pitchHi - pitchLo));

        auto getY = [&](int si) -> float {
            if (si < 0)  return pos.y + pad + dH + 1.0f;          // silences: bottom rail
            if (si >= (int)expandedScale.size()) return pos.y + pad + dH * 0.5f;
            float t = std::clamp((float)(expandedScale[si] - pitchLo) / pitchRange, 0.0f, 1.0f);
            return pos.y + pad + dH * (1.0f - t);
        };

        // Returns {xLeft, xRight} per note — duration-proportional spans
        auto computeSpans = [&](const vector<MelodyNote>& mel) -> vector<pair<float,float>> {
            float total = 0.0f; for (auto& n : mel) total += n.duration;
            if (total <= 0.0f) total = 1.0f;
            vector<pair<float,float>> spans;
            spans.reserve(mel.size());
            float cum = 0.0f;
            for (auto& n : mel) {
                float xL = pos.x + pad + cum / total * dW;
                float xR = pos.x + pad + (cum + n.duration) / total * dW;
                spans.push_back({xL, xR});
                cum += n.duration;
            }
            return spans;
        };

        // Faint chord-tone guide lines (behind everything)
        for (int si = 0; si < (int)expandedScale.size(); si++) {
            if (isChordTone(expandedScale[si])) {
                float y = getY(si);
                dl->AddLine(ImVec2(pos.x + pad, y), ImVec2(pos.x + W - pad, y),
                            IM_COL32(255, 200, 80, 18));
            }
        }

        // Stepped melody contour: horizontal bar per note + vertical connector to next
        auto drawMelody = [&](const vector<MelodyNote>& mel,
                              ImU32 lineCol, ImU32 dotCol, ImU32 chordDotCol,
                              float thick, float dotR) {
            if (mel.empty()) return;
            auto spans = computeSpans(mel);
            int  sz    = (int)mel.size();
            for (int i = 0; i < sz; i++) {
                float y  = getY(mel[i].scaleIndex);
                float xL = spans[i].first;
                float xR = spans[i].second;
                // Horizontal bar for this note's duration
                dl->AddLine(ImVec2(xL, y), ImVec2(xR, y), lineCol, thick);
                // Vertical step connector to next note
                if (i + 1 < sz) {
                    float yNext = getY(mel[i + 1].scaleIndex);
                    dl->AddLine(ImVec2(xR, y), ImVec2(xR, yNext), lineCol, thick * 0.55f);
                }
            }
            // Dot at the left (onset) of each note
            for (int i = 0; i < sz; i++) {
                bool  ic = (mel[i].scaleIndex >= 0 && mel[i].scaleIndex < (int)expandedScale.size())
                           && isChordTone(expandedScale[mel[i].scaleIndex]);
                float y  = getY(mel[i].scaleIndex);
                dl->AddCircleFilled(ImVec2(spans[i].first, y),
                                    ic ? dotR * 1.5f : dotR,
                                    ic ? chordDotCol : dotCol);
            }
        };

        // Layer 1 — intermediate history: dim grey, alpha fades with age
        for (int h = 1; h < historyIndex; h++) {
            int   age = historyIndex - h;
            int   a   = std::max(10, 55 - age * 7);
            ImU32 col = IM_COL32(140, 140, 155, a);
            drawMelody(history[h], col, col, col, 0.8f, 1.8f);
        }

        // Layer 2 — seed melody: steel blue, fades as mutations accumulate
        if (historyIndex > 0) {
            int   a     = std::max(45, 165 - historyIndex * 10);
            ImU32 lineC = IM_COL32(70,  120, 200, a);
            ImU32 dotC  = IM_COL32(90,  145, 220, a);
            ImU32 cdotC = IM_COL32(120, 180, 255, a);
            drawMelody(history[0], lineC, dotC, cdotC, 1.2f, 2.5f);
        }

        // Layer 3 — current state: warm gold (brightest)
        drawMelody(history[historyIndex],
                   IM_COL32(235, 175, 55, 200),
                   IM_COL32(240, 190, 70, 255),
                   IM_COL32(255, 220, 110, 255),
                   1.5f, 3.2f);

        // Layer 4 — playback cursor: white ring at the onset of the playing note
        if (isPlaying && historyIndex < (int)history.size()) {
            const auto& mel = history[historyIndex];
            if (!mel.empty()) {
                int   melSz = (int)mel.size();
                int   ci    = ((currentNoteIndex - 1) % melSz + melSz) % melSz;
                auto  spans = computeSpans(mel);
                float y     = getY(mel[ci].scaleIndex);
                dl->AddCircle(ImVec2(spans[ci].first, y),
                              5.5f, IM_COL32(255, 255, 255, 210), 12, 1.5f);
            }
        }

        // History depth readout: "idx/total" top-right
        {
            char   buf[16];
            snprintf(buf, sizeof(buf), "%d/%d", historyIndex, (int)history.size() - 1);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(pos.x + W - ts.x - pad - 2.0f, pos.y + pad + 1.0f),
                        IM_COL32(130, 130, 145, 180), buf);
        }

        // Pending change indicator: bottom-left
        if (pendingMutate || pendingDemutate) {
            const char* lbl = pendingMutate ? "* mut" : "* demut";
            dl->AddText(ImVec2(pos.x + pad + 2.0f, pos.y + H - 14.0f),
                        IM_COL32(235, 210, 80, 200), lbl);
        }
    }

    // ── Playback ───────────────────────────────────────────────────────────────

    void fireCurrentNote() {
        if (history.empty() || historyIndex >= (int)history.size()) return;
        const auto& melody = history[historyIndex];
        if (melody.empty()) return;

        currentNoteIndex %= (int)melody.size();
        const MelodyNote& note = melody[currentNoteIndex];

        // Check sequence chance at the beginning of each loop
        if (currentNoteIndex == 0) {
            std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
            seqGateActive = (chanceDist(chanceRng) < seqChance.get());
        }

        // Calculate final gate output based on both sequence and note chances
        bool shouldGate = seqGateActive;
        if (shouldGate && noteChance.get() < 1.0f) {
            std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
            shouldGate = (chanceDist(chanceRng) < noteChance.get());
        }

        if (note.scaleIndex >= 0 && note.scaleIndex < (int)expandedScale.size()) {
            int raw      = expandedScale[note.scaleIndex];
            pitchOut = std::clamp((int)std::round(raw + transpose.get()), 0, 127);
            velGate  = shouldGate ? note.velocity : 0.0f;
        } else {
            velGate = 0.0f;
        }

        nextNoteBeat += note.duration;                                   // phasor mode
        uint64_t now = ofGetElapsedTimeMillis();
        nextNoteTime = now + (uint64_t)beatToMs(note.duration);          // wall-clock fallback

        currentNoteIndex++;
        if (currentNoteIndex >= (int)melody.size()) {
            currentNoteIndex = 0;

            // Apply any pending mutation/demutation cleanly at the loop boundary
            if (pendingMutate) {
                applyMutation();
                pendingMutate = false;
            } else if (pendingDemutate) {
                if (historyIndex > 0) historyIndex--;
                pendingDemutate = false;
            }

            if (!loop.get()) {
                isPlaying = false;
                velGate   = 0.0f;
            }
        }
    }
};
