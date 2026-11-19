// Runs the BITXOS_BUBBLES chain as it was patched (debounce -> Change -> Event Gate ->
// Voice Expanding 2, with release/pitch sent per blob index) and the Blob Birth Voices
// node over the same chop sequence, with the real node classes.
#include "ofMain.h"
#include "../src/debounce.h"
#include "../src/change.h"
#include "../src/eventGate.h"
#include "../src/voiceExpanding2.h"
#include "../src/blobBirthVoices.h"
#include <cassert>
#include <iostream>

struct Frame { vector<float> x, size; };
struct Bubble { int frame; int voice; float pan; float size; };

static const int VOICES = 8;

// One blob, chopped twice. Lists are in blob-id order, as Bounding Box Aspect Ratio
// publishes them: the second new blob takes a freed id and lands in the middle.
static vector<Frame> scene(vector<std::pair<int, std::pair<float, float>>>& expected) {
    vector<Frame> frames;
    auto hold = [&](Frame f, int n) { for (int i = 0; i < n; ++i) frames.push_back(f); };
    hold({{.54f}, {.30f}}, 10);
    expected.push_back({int(frames.size()), {.18f, .006f}});
    hold({{.54f, .18f}, {.29f, .006f}}, 10);
    expected.push_back({int(frames.size()), {.80f, .04f}});
    hold({{.54f, .80f, .18f}, {.25f, .04f, .006f}}, 10);
    return frames;
}

template<class Node> static ofParameter<vector<float>>& vec(Node& node, const string& name) {
    return node.template parameter<vector<float>>(name);
}

// The original patch. The SC nodes get Release / Pitch / Curve straight from the
// per-blob list: /n_setn writes blob i's value on channel i, whatever voice fires.
static vector<Bubble> runOriginal(const vector<Frame>& frames) {
    debounce deb; deb.setup();
    change chg;
    eventGate evt;
    voiceExpanding2 exp; exp.setup();
    deb.parameter<int>("Frames") = 3;
    deb.parameter<float>("Tolerance") = 0.5f;
    exp.parameter<int>("Output Size") = VOICES;
    ofEventArgs e;
    vector<float> channelSize(VOICES, 0.0f);
    vector<Bubble> bubbles;
    // Connections, as Oceanode makes them: the sink is set whenever the source is.
    auto c1 = vec(deb, "Output").newListener([&](vector<float>& v) { vec(chg, "Input") = v; });
    auto c2 = vec(chg, "Output").newListener([&](vector<float>& v) { vec(evt, "Input") = v; });
    auto c3 = vec(evt, "Output").newListener([&](vector<float>& v) { vec(exp, "Input Gate") = v; });
    for (size_t f = 0; f < frames.size(); ++f) {
        vec(deb, "Input") = vector<float>(1, float(frames[f].x.size()));
        deb.update(e);
        evt.update(e);
        for (size_t i = 0; i < frames[f].size.size() && i < channelSize.size(); ++i) channelSize[i] = frames[f].size[i];
        vec(exp, "Input Aux1") = frames[f].x;
        exp.update(e);
        const auto gate = vec(exp, "Output Gate").get();
        for (int v = 0; v < int(gate.size()); ++v)
            if (gate[v] > 0) bubbles.push_back({int(f), v, vec(exp, "Output Aux1").get()[v], channelSize[v]});
    }
    return bubbles;
}

static vector<Bubble> runFixed(const vector<Frame>& frames) {
    blobBirthVoices node; node.setup();
    node.parameter<int>("Voices") = VOICES;
    ofEventArgs e;
    vector<Bubble> bubbles;
    int valueEvents = 0, gateSawValues = 0;
    // Downstream listeners on Gate must already see the new blob's values.
    auto onValues = vec(node, "Size Out").newListener([&](vector<float>&) { ++valueEvents; });
    auto onGate = vec(node, "Gate").newListener([&](vector<float>& g) {
        for (float v : g) if (v > 0) { gateSawValues = valueEvents; break; }
    });
    int triggers = 0;
    auto onTrigger = vec(node, "Trigger").newListener([&](vector<float>& t) { if (t[0] > 0) ++triggers; });
    for (size_t f = 0; f < frames.size(); ++f) {
        vec(node, "Count") = vector<float>(1, float(frames[f].x.size()));
        vec(node, "X") = frames[f].x;
        vec(node, "Size") = frames[f].size;
        node.update(e);
        const auto gate = vec(node, "Gate").get();
        assert(int(gate.size()) == VOICES && int(vec(node, "X Out").get().size()) == VOICES);
        for (int v = 0; v < int(gate.size()); ++v)
            if (gate[v] > 0) {
                assert(gateSawValues == valueEvents && valueEvents > 0);
                bubbles.push_back({int(f), v, vec(node, "X Out").get()[v], vec(node, "Size Out").get()[v]});
            }
    }
    assert(triggers == int(bubbles.size()));
    assert(vec(node, "Trigger").get()[0] == 0.0f);
    return bubbles;
}

static void print(const char* title, const vector<Bubble>& bubbles) {
    std::cout << title << std::endl;
    for (auto& b : bubbles)
        std::cout << "  frame " << b.frame << "  voice " << b.voice << "  pan " << b.pan << "  size " << b.size << std::endl;
}

static bool close(float a, float b) { return std::fabs(a - b) < 1e-5f; }

int main() {
    vector<std::pair<int, std::pair<float, float>>> expected;
    const auto frames = scene(expected);
    std::cout << "Expected bubbles (the new blob of each chop):" << std::endl;
    for (auto& x : expected) std::cout << "  chop at frame " << x.first << "  pan " << x.second.first << "  size " << x.second.second << std::endl;

    auto original = runOriginal(frames);
    print("Original chain:", original);
    // The chain also fires while its debounce settles at start-up; leave those out.
    original.erase(std::remove_if(original.begin(), original.end(), [](const Bubble& b) { return b.frame < 5; }), original.end());
    // Characterises the bug: one bubble per chop, but always with blob 0's pan, and with
    // the size (so pitch, release and curve) of whatever sits on the fired voice's channel,
    // which is the blob with that index if there is one and a stale value if not.
    assert(original.size() == expected.size());
    for (size_t i = 0; i < original.size(); ++i) {
        assert(close(original[i].pan, .54f));
        assert(!close(original[i].pan, expected[i].second.first));
        assert(!close(original[i].size, expected[i].second.second));
    }

    const auto fixed = runFixed(frames);
    print("Blob Birth Voices:", fixed);
    assert(fixed.size() == expected.size());
    for (size_t i = 0; i < fixed.size(); ++i) {
        assert(close(fixed[i].pan, expected[i].second.first));
        assert(close(fixed[i].size, expected[i].second.second));
        assert(fixed[i].voice == int(i));
        assert(fixed[i].frame == expected[i].first + 2); // Frames = 3
    }

    // Count left at -1: the length of X is the count.
    {
        blobBirthVoices node; node.setup();
        ofEventArgs e;
        node.parameter<int>("Frames") = 1;
        node.parameter<bool>("Merges") = false;
        vec(node, "X") = {.5f}; vec(node, "Size") = {.3f}; vec(node, "Aux") = {7.f};
        node.update(e);
        vec(node, "X") = {.5f, .2f}; vec(node, "Size") = {.3f, .01f}; vec(node, "Aux") = {7.f, 9.f};
        node.update(e);
        assert(vec(node, "Gate").get()[0] == 1 && close(vec(node, "X Out").get()[0], .2f) && close(vec(node, "Aux Out").get()[0], 9.f));
        // Count ahead of the lists right after a fire: frame not read, but the gate still closes.
        vec(node, "Count") = {3.f};
        node.update(e);
        assert(vec(node, "Gate").get()[0] == 0 && vec(node, "Trigger").get()[0] == 0);
        vec(node, "Count") = {-1.f}; node.update(e);
        // The lists grow a frame before Count, with the new blob in the middle: it is the
        // middle one that fires, not the last.
        vec(node, "Count") = {2.f}; node.update(e);
        vec(node, "X") = {.5f, .7f, .2f}; vec(node, "Size") = {.3f, .05f, .01f}; vec(node, "Aux") = {7.f, 8.f, 9.f};
        node.update(e);
        for (float g : vec(node, "Gate").get()) assert(g == 0);
        vec(node, "Count") = {3.f}; node.update(e);
        assert(vec(node, "Gate").get()[1] == 1 && close(vec(node, "X Out").get()[1], .7f) && close(vec(node, "Aux Out").get()[1], 8.f));
        // Count 0 with stale lists reads as no blobs; the next blob is a birth.
        vec(node, "Count") = {0.f}; node.update(e);
        for (float g : vec(node, "Gate").get()) assert(g == 0);
        vec(node, "X") = {.9f}; vec(node, "Size") = {.02f}; vec(node, "Count") = {1.f}; node.update(e);
        assert(vec(node, "Gate").get()[2] == 1 && close(vec(node, "X Out").get()[2], .9f));
        // Merges on: a blob going away fires with its last values.
        node.parameter<bool>("Merges") = true;
        vec(node, "X") = {.9f, .4f}; vec(node, "Size") = {.02f, .2f}; vec(node, "Aux") = {1.f, 2.f}; vec(node, "Count") = {2.f};
        node.update(e); node.update(e);
        vec(node, "X") = {.41f}; vec(node, "Size") = {.22f}; vec(node, "Aux") = {2.f}; vec(node, "Count") = {1.f};
        node.update(e);
        assert(vec(node, "Gate").get()[4] == 1 && close(vec(node, "X Out").get()[4], .9f) && close(vec(node, "Size Out").get()[4], .02f));
        assert(vec(node, "Trigger").get()[0] == 1);
        node.update(e);
        // Off, it does not.
        node.parameter<bool>("Merges") = false;
        vec(node, "Count") = {0.f}; node.update(e);
        for (float g : vec(node, "Gate").get()) assert(g == 0);
        node.parameter<bool>("Merges") = true;
        vec(node, "Count") = {1.f}; node.update(e); node.update(e);
        // Fewer voices: outputs follow, state survives.
        node.parameter<int>("Voices") = 2;
        assert(vec(node, "Gate").get().size() == 2 && vec(node, "X Out").get().size() == 2);
        node.update(e);
        // A preset load does not fire for the blobs it brings.
        vec(node, "X") = {.1f, .2f, .3f}; vec(node, "Size") = {.1f, .1f, .1f}; vec(node, "Count") = {3.f};
        node.presetHasLoaded();
        node.update(e);
        for (float g : vec(node, "Gate").get()) assert(g == 0);
    }
    std::cout << "Blob Birth Voices node tests passed" << std::endl;
    return 0;
}
