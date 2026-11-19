#include "../src/blobBirthVoicesCore.h"
#include <cassert>
#include <iostream>

using blobBirth::Blob;
using blobBirth::Voices;
using blobBirth::findBirths;

static std::vector<Blob> blobs(std::initializer_list<std::pair<float, float>> list) {
    std::vector<Blob> out;
    for (auto& p : list) out.push_back({p.first, p.second, p.first * 10.0f});
    return out;
}

static int firedVoice(const Voices& v) {
    int fired = -1, count = 0;
    for (size_t i = 0; i < v.gate.size(); ++i) if (v.gate[i] > 0) { fired = int(i); ++count; }
    return count == 1 ? fired : -1 - count;
}

int main() {
    // The new blob can be at the end, the start or the middle.
    assert((findBirths(blobs({{.5f, .3f}}), blobs({{.5f, .29f}, {.18f, .006f}})) == std::vector<int>{1}));
    assert((findBirths(blobs({{.5f, .3f}}), blobs({{.18f, .006f}, {.5f, .29f}})) == std::vector<int>{0}));
    assert((findBirths(blobs({{.5f, .3f}, {.8f, .05f}}), blobs({{.5f, .3f}, {.2f, .01f}, {.81f, .05f}})) == std::vector<int>{1}));
    // Two at once, and from nothing.
    assert((findBirths(blobs({{.5f, .3f}}), blobs({{.1f, .01f}, {.5f, .3f}, {.9f, .01f}})) == std::vector<int>{0, 2}));
    assert((findBirths({}, blobs({{.1f, .01f}, {.5f, .3f}})) == std::vector<int>{0, 1}));
    // Identical twins: the later one is the new one. Nothing born when the count does not grow.
    assert((findBirths(blobs({{.5f, .1f}}), blobs({{.5f, .1f}, {.5f, .1f}})) == std::vector<int>{1}));
    assert(findBirths(blobs({{.5f, .1f}, {.2f, .1f}}), blobs({{.5f, .1f}})).empty());

    Voices v(4);
    const int frames = 3;
    auto one = blobs({{.5f, .3f}});
    // Blobs present at the start are not births.
    for (int i = 0; i < 5; ++i) assert(!v.step(one, frames).fired);

    // A chop: the gate waits for the count to hold, then fires once, for one frame,
    // with the new blob's values on the fired voice.
    auto two = blobs({{.5f, .28f}, {.18f, .006f}});
    assert(!v.step(two, frames).fired);
    assert(!v.step(two, frames).fired);
    auto step = v.step(two, frames);
    assert(step.fired && step.valuesChanged && step.gateChanged);
    assert(firedVoice(v) == 0 && v.x[0] == .18f && v.size[0] == .006f && v.aux[0] == .18f * 10.0f);
    step = v.step(two, frames);
    assert(!step.fired && step.gateChanged && firedVoice(v) == -1);
    assert(!v.step(two, frames).gateChanged);

    // Next chop lands in the middle of the lists and takes the next voice; voice 0 keeps its values.
    auto three = blobs({{.5f, .2f}, {.7f, .05f}, {.18f, .006f}});
    v.step(three, frames); v.step(three, frames);
    assert(v.step(three, frames).fired);
    assert(firedVoice(v) == 1 && v.x[1] == .7f && v.size[1] == .05f && v.x[0] == .18f);
    v.step(three, frames);

    // A one-frame flicker of the contour fires nothing.
    auto four = blobs({{.5f, .2f}, {.7f, .05f}, {.18f, .006f}, {.9f, .001f}});
    assert(!v.step(four, frames).fired);
    for (int i = 0; i < 5; ++i) assert(!v.step(three, frames).fired);
    assert(!v.step(four, frames).fired);
    assert(!v.step(three, frames).fired);
    assert(!v.step(four, frames).fired);
    assert(!v.step(four, frames).fired); // only two in a row
    assert(!v.step(three, frames).fired);

    // Merging fires nothing, and the chop after it is still found.
    for (int i = 0; i < 5; ++i) assert(!v.step(two, frames).fired);
    for (int i = 0; i < 2; ++i) assert(!v.step(three, frames).fired);
    assert(v.step(three, frames).fired && firedVoice(v) == 2 && v.x[2] == .7f);
    v.step(three, frames);

    // Blobs drift between frames; the reference follows them up to the chop.
    auto drift = three;
    for (int i = 0; i < 20; ++i) {
        for (auto& b : drift) b.x += .01f;
        assert(!v.step(drift, frames).fired);
    }
    auto chopped = drift;
    chopped.insert(chopped.begin(), Blob{.05f, .002f, 0});
    v.step(chopped, frames); v.step(chopped, frames);
    assert(v.step(chopped, frames).fired && firedVoice(v) == 3 && v.x[3] == .05f);
    v.step(chopped, frames);

    // Two births in one go use two voices, wrapping round.
    auto six = chopped;
    six.push_back({.33f, .003f, 0});
    six.push_back({.44f, .004f, 0});
    v.step(six, frames); v.step(six, frames);
    assert(v.step(six, frames).fired);
    assert(v.gate[0] == 1 && v.gate[1] == 1 && v.x[0] == .33f && v.x[1] == .44f && firedVoice(v) == -3);

    // Frames at 1 reacts on the frame of the chop. More births than voices does not overrun.
    Voices fast(2);
    fast.step({}, 1);
    assert(fast.step(six, 1).fired && fast.gate.size() == 2);
    fast.resize(1);
    fast.step({}, 1);
    assert(fast.step(two, 1).fired && fast.gate[0] == 1 && fast.x[0] == .18f);

    // Merges on: a blob absorbed from the middle fires with the values it had; off: silent.
    Voices m(4);
    auto abc = blobs({{.5f, .3f}, {.7f, .05f}, {.18f, .006f}});
    auto ac = blobs({{.52f, .34f}, {.18f, .006f}});
    m.step(abc, frames, true);
    assert(!m.step(ac, frames, true).fired && !m.step(ac, frames, true).fired);
    assert(m.step(ac, frames, true).fired && firedVoice(m) == 0 && m.x[0] == .7f && m.size[0] == .05f);
    assert(!m.step(ac, frames, true).fired && firedVoice(m) == -1);
    // A chop after it still finds the new blob, on the next voice.
    m.step(abc, frames, true); m.step(abc, frames, true);
    assert(m.step(abc, frames, true).fired && firedVoice(m) == 1 && m.x[1] == .7f);
    m.step(abc, frames, true);
    // Everything gone at once fires every blob; a flicker down and back fires nothing.
    assert(!m.step(ac, frames, true).fired);
    for (int i = 0; i < 4; ++i) assert(!m.step(abc, frames, true).fired);
    m.step({}, frames, true); m.step({}, frames, true);
    assert(m.step({}, frames, true).fired && firedVoice(m) == -4);
    Voices quiet(4);
    quiet.step(abc, 1);
    assert(!quiet.step(ac, 1).fired);

    std::cout << "Blob Birth Voices core tests passed" << std::endl;
    return 0;
}
