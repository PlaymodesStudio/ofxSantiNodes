#pragma once

#include "ofxOceanodeNodeModel.h"
#include "blobBirthVoicesCore.h"

class blobBirthVoices : public ofxOceanodeNodeModel {
public:
    blobBirthVoices() : ofxOceanodeNodeModel("Blob Birth Voices") {}

    void setup() override {
        description = "Fires one voice for every new blob, carrying that blob's own values. "
            "Feed it the per-blob lists of a blob source (one entry per blob). When the number of blobs grows, "
            "it works out which entries are the new blobs, even when they land in the middle of the lists, "
            "gives each one the next voice, writes its X, Size and Aux on that voice and opens the voice's gate "
            "for one frame. Values stay on a voice until it is used again, so a sound can ring out with them.\n\n"
            "**[ Count]** Number of blobs, optional. Leave at -1 to use the length of X. Frames where it disagrees "
            "with the length of X are skipped, except 0, which always means no blobs.\n\n"
            "**[ X]** One value per blob. Used, with Size, to tell the blobs apart.\n\n"
            "**[ Size]** One value per blob. Used, with X, to tell the blobs apart.\n\n"
            "**[ Aux]** One value per blob, carried along only.\n\n"
            "**[ Voices]** Length of the outputs. Voices are taken in turn.\n\n"
            "**[ Frames]** Frames in a row the blob count must hold before it is believed. Filters contours that "
            "flicker; it also delays the gate by that many frames.\n\n"
            "**[ Merges]** On, a blob that disappears (absorbed by another) also fires a voice, with the X, Size and "
            "Aux it had just before it went.\n\n"
            "**[ Gate]** One entry per voice: 1 for one frame on the voice of a new (or, with Merges, gone) blob.\n\n"
            "**[ Trigger]** 1 for one frame whenever any voice fires.\n\n"
            "X Out, Size Out and Aux Out are published before Gate. "
            "A blob that dies while another is born within the same Frames window leaves the count unchanged and "
            "is not seen.";

        addParameter(count.set("Count", {-1.0f}, {-1.0f}, {FLT_MAX}));
        addParameter(x.set("X", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(size.set("Size", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(aux.set("Aux", {0.0f}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(voices.set("Voices", 8, 1, 128));
        addParameter(frames.set("Frames", 3, 1, 200));
        addParameter(merges.set("Merges", true));

        addOutputParameter(xOut.set("X Out", vector<float>(8, 0.0f), {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(sizeOut.set("Size Out", vector<float>(8, 0.0f), {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(auxOut.set("Aux Out", vector<float>(8, 0.0f), {-FLT_MAX}, {FLT_MAX}));
        addOutputParameter(gate.set("Gate", vector<float>(8, 0.0f), {0.0f}, {1.0f}));
        addOutputParameter(trigger.set("Trigger", {0.0f}, {0.0f}, {1.0f}));

        listener = voices.newListener([this](int& n) {
            core.resize(n);
            publishValues();
            gate = core.gate;
        });
        core.resize(voices);
    }

    void update(ofEventArgs&) override {
        const auto& xs = x.get();
        const auto& sizes = size.get();
        const auto& auxes = aux.get();
        // Count and the lists come by different routes and can be a frame
        // apart. A frame where they disagree is not read at all: cutting the
        // lists to Count would drop the wrong blob. Count at 0 is the one
        // exception, since an empty source can leave a stale entry in X.
        size_t n = xs.size();
        bool readable = true;
        const float counted = count.get().empty() ? -1.0f : count.get()[0];
        if (counted >= 0.0f) {
            const size_t rounded = size_t(std::lround(counted));
            if (rounded == 0) n = 0;
            else if (rounded != n) readable = false;
        }
        blobBirth::Voices::Step step;
        if (readable) {
            blobs.resize(n);
            for (size_t i = 0; i < n; ++i) {
                blobs[i].x = xs[i];
                blobs[i].size = i < sizes.size() ? sizes[i] : 0.0f;
                blobs[i].aux = i < auxes.size() ? auxes[i] : 0.0f;
            }
            step = core.step(blobs, frames, merges);
        } else {
            step = core.idle();
        }
        // Values first: the synth must hold them when the gate arrives.
        if (step.valuesChanged) publishValues();
        if (step.gateChanged) gate = core.gate;
        if (step.fired) {
            trigger = vector<float>(1, 1.0f);
            triggerHigh = true;
        } else if (triggerHigh) {
            trigger = vector<float>(1, 0.0f);
            triggerHigh = false;
        }
    }

    void presetHasLoaded() override {
        // The blobs of a freshly loaded preset are not births.
        core.reset();
    }

private:
    void publishValues() {
        xOut = core.x;
        sizeOut = core.size;
        auxOut = core.aux;
    }

    ofParameter<vector<float>> count, x, size, aux;
    ofParameter<int> voices, frames;
    ofParameter<bool> merges;
    ofParameter<vector<float>> xOut, sizeOut, auxOut, gate, trigger;
    ofEventListener listener;

    blobBirth::Voices core;
    std::vector<blobBirth::Blob> blobs;
    bool triggerHigh = false;
};
