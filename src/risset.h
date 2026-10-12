#ifndef risset_h
#define risset_h

#include "ofxOceanodeNodeModel.h"

// Risset-Shepard glissando: `Voices` voices `Interval` semitones apart move together
// and wrap from the top of the span to the bottom, where the bell-shaped amplitude
// window has silenced them. One cycle of the input phasor moves every voice by one interval;
// a phasor running backwards makes it fall.
class risset : public ofxOceanodeNodeModel {
public:
    risset() : ofxOceanodeNodeModel("Risset") {}

    void setup() override {
        description = "Endless rising or falling glissando: pitch and linear amplitude vectors for N voices. Driven by a phasor: one cycle moves every voice by one interval, backwards falls. Quantize snaps the pitches to Scale (pitch classes, 0 = C); Equal Loudness weights the amplitudes by an equal-loudness curve.";
        addParameter(phasor.set("Phasor", 0, 0, 1));
        addParameter(voices.set("Voices", 8, 1, 128));
        addParameter(center.set("Center", 66, 0, 130));
        addParameter(interval.set("Interval", 12, 1, 24));
        addParameter(width.set("Width", 1, 0.1, 2));
        addParameter(level.set("Level", 1, 0, 1));
        addParameter(equalLoudness.set("Equal Loudness", false));
        addParameter(quantize.set("Quantize", false));
        addParameter(scale.set("Scale", {0, 2, 4, 5, 7, 9, 11}, {0}, {11}));
        addOutputParameter(pitchOut.set("Pitch", {0}, {0}, {130}));
        addOutputParameter(ampOut.set("Amp", {0}, {0}, {1}));
        addOutputParameter(wrapOut.set("Wrap", {0}, {0}, {1}));
    }

    void update(ofEventArgs &a) override {
        int n = voices;
        float d = phasor - lastPhasor;
        if(d < -0.5f) d += 1.0f;
        if(d > 0.5f) d -= 1.0f;
        position += d;
        lastPhasor = phasor;
        position = fmod(position, (double)n);
        if(position < 0) position += n;

        float span = interval * n;
        auto pitchAt = [&](float x){ return center + (x - 0.5f) * span; };
        auto windowAt = [&](float x){ return (float)pow(sin(x * PI), 2.0f / width); };

        // Equal loudness: inverse B-weighting (close to the 70 phon contour), normalised so the
        // loudest point of the window stays at Level
        float norm = 1;
        if(equalLoudness){
            norm = 0;
            for(int k = 1; k < 64; k++){
                float x = k / 64.0f;
                norm = std::max(norm, windowAt(x) * loudnessGain(pitchAt(x)));
            }
            norm = std::max(norm, 1e-6f);
        }

        vector<float> pitches(n), amps(n), wraps(n, 0);
        if(lastX.size() != n) lastX.assign(n, -1);
        for(int i = 0; i < n; i++){
            float x = fmod(position + i, (double)n) / n;
            bool wrapped = lastX[i] >= 0 && std::abs(x - lastX[i]) > 0.5f;
            lastX[i] = x;
            float p = pitchAt(x);
            float amp = windowAt(x);
            if(equalLoudness) amp *= loudnessGain(p) / norm;
            pitches[i] = quantize ? snapToScale(p) : p;
            amps[i] = wrapped ? 0 : amp * level;
            wraps[i] = wrapped ? 1 : 0;
        }
        pitchOut = pitches;
        ampOut = amps;
        wrapOut = wraps;
    }

private:
    // Linear gain that compensates the ear's sensitivity at a MIDI pitch (inverse B-weighting)
    static float loudnessGain(float midi){
        double f = std::max(10.0, 440.0 * pow(2.0, (midi - 69.0) / 12.0));
        double f2 = f * f;
        double rb = (12194.0 * 12194.0 * f2 * f) / ((f2 + 20.6 * 20.6) * sqrt(f2 + 158.5 * 158.5) * (f2 + 12194.0 * 12194.0));
        double db = 20.0 * log10(rb) + 0.17;
        return (float)pow(10.0, -db / 20.0);
    }

    float snapToScale(float p) const {
        const vector<int>& s = scale.get();
        if(s.empty()) return p;
        float base = floor(p / 12.0f) * 12.0f;
        float best = p, bestDist = FLT_MAX;
        for(int octave = -1; octave <= 1; octave++){
            for(int pc : s){
                float candidate = base + octave * 12 + (((pc % 12) + 12) % 12);
                float dist = std::abs(candidate - p);
                if(dist < bestDist){ bestDist = dist; best = candidate; }
            }
        }
        return best;
    }

    ofParameter<int> voices;
    ofParameter<float> phasor, center, interval, width, level;
    ofParameter<bool> equalLoudness, quantize;
    ofParameter<vector<int>> scale;
    ofParameter<vector<float>> pitchOut, ampOut, wrapOut;

    double position = 0;
    float lastPhasor = 0;
    vector<float> lastX;
};

#endif /* risset_h */
