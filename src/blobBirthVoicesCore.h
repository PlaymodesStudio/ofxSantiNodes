#pragma once

// Core of the Blob Birth Voices node, free of openFrameworks so it can be
// tested on its own.
//
// A source lists one value per blob (position, size...). When a blob is
// chopped the lists grow, and the new entry can land anywhere in them, not
// only at the end. This finds which entries are the new blobs, gives each
// one a voice, and holds that blob's values on its voice while the gate
// fires, so a per-voice synth plays the new blob and not whatever happens to
// sit at the same index.

#include <algorithm>
#include <cmath>
#include <vector>

namespace blobBirth {

struct Blob {
    float x = 0.0f;
    float size = 0.0f;
    float aux = 0.0f;
};

inline float distance(const Blob& a, const Blob& b) {
    return std::fabs(a.x - b.x) + std::fabs(a.size - b.size);
}

// The entries of current that have no partner in previous. Both lists keep
// their surviving blobs in the same order, so every previous blob is paired
// with one current entry, in order, at the least total distance; what is
// left over was born. On a tie the later entry is the new one.
inline std::vector<int> findBirths(const std::vector<Blob>& previous, const std::vector<Blob>& current) {
    const int m = int(previous.size());
    const int n = int(current.size());
    std::vector<int> births;
    if (n <= m) return births;
    const double unreachable = 1e30;
    // cost[i][j]: the first i previous blobs paired within the first j current.
    std::vector<std::vector<double>> cost(m + 1, std::vector<double>(n + 1, unreachable));
    for (int j = 0; j <= n; ++j) cost[0][j] = 0.0;
    for (int i = 1; i <= m; ++i) {
        for (int j = i; j <= n; ++j) {
            const double paired = cost[i - 1][j - 1] + distance(previous[i - 1], current[j - 1]);
            const double skipped = j - 1 >= i ? cost[i][j - 1] : unreachable;
            cost[i][j] = std::min(paired, skipped);
        }
    }
    int i = m, j = n;
    while (j > 0) {
        const bool canSkip = j - 1 >= i;
        if (i == 0 || (canSkip && cost[i][j - 1] <= cost[i - 1][j - 1] + distance(previous[i - 1], current[j - 1]))) {
            births.push_back(j - 1);
            --j;
        } else {
            --i;
            --j;
        }
    }
    std::reverse(births.begin(), births.end());
    return births;
}

class Voices {
public:
    struct Step {
        bool valuesChanged = false; // x, size or aux of some voice changed
        bool gateChanged = false;   // gate differs from the previous step
        bool fired = false;         // at least one voice fired on this step
    };

    std::vector<float> x, size, aux, gate;

    explicit Voices(int voices = 8) { resize(voices); }

    void resize(int voices) {
        voices = std::max(1, voices);
        x.resize(voices, 0.0f);
        size.resize(voices, 0.0f);
        aux.resize(voices, 0.0f);
        gate.assign(voices, 0.0f);
        if (slot >= voices) slot = voices - 1;
    }

    void reset() {
        primed = false;
        counter = 0;
    }

    // A frame with nothing to read: only closes the gates left open.
    Step idle() {
        Step result;
        if (std::any_of(gate.begin(), gate.end(), [](float g) { return g != 0.0f; })) {
            std::fill(gate.begin(), gate.end(), 0.0f);
            result.gateChanged = true;
        }
        return result;
    }

    // One frame. The blob count has to hold for `frames` frames in a row
    // before it is believed, so a contour that flickers for a frame does
    // not fire. The blobs of the last believed frame are kept as they were
    // just before the count moved, and the new blobs are found against them.
    // A count that drops (a blob absorbed by another) fires only with merges on.
    Step step(const std::vector<Blob>& blobs, int frames, bool merges = false) {
        Step result = idle();
        const int n = int(blobs.size());
        if (!primed) {
            // Whatever is there when we start is not a birth.
            stable = blobs;
            primed = true;
            counter = 0;
            return result;
        }
        if (n == int(stable.size())) {
            stable = blobs;
            counter = 0;
            return result;
        }
        if (n == candidate && counter > 0) ++counter;
        else { candidate = n; counter = 1; }
        if (counter < std::max(1, frames)) return result;

        // Grown: the entries of blobs with no partner are new. Shrunk: the
        // entries of the last believed frame with no partner are gone, and
        // fire with the values they had then.
        const bool grown = n > int(stable.size());
        if (grown || merges) {
            const std::vector<Blob>& source = grown ? blobs : stable;
            const std::vector<int> changed = grown ? findBirths(stable, blobs) : findBirths(blobs, stable);
            for (int index : changed) {
                slot = (slot + 1) % int(gate.size());
                x[slot] = source[index].x;
                size[slot] = source[index].size;
                aux[slot] = source[index].aux;
                gate[slot] = 1.0f;
                result.valuesChanged = true;
                result.gateChanged = true;
                result.fired = true;
            }
        }
        stable = blobs;
        counter = 0;
        return result;
    }

private:
    std::vector<Blob> stable;
    bool primed = false;
    int candidate = -1;
    int counter = 0;
    int slot = -1;
};

} // namespace blobBirth
