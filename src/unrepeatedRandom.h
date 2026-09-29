#ifndef UnrepeatedRandom_h
#define UnrepeatedRandom_h

#include "ofxOceanodeNodeModel.h"
// Transport mode needs ofxOceanode's global transport (feature-globalTransport branch).
// Without it the node compiles exactly as before.
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
#include "ofxOceanodeDeterministicRandom.h"
#endif
#include <vector>
#include <random>
#include <algorithm>
#include <map>
#include <mutex>
#include <cfloat>
#include <climits>

class UnrepeatedRandom : public ofxOceanodeNodeModel {
public:
    UnrepeatedRandom() : ofxOceanodeNodeModel("Unrepeated Random") {
        description = "This module generates random numbers in a given range. "
                      "It has a sequential mode where it ensures all numbers within the range "
                      "are generated before repeating. Trigger reacts to rising edges; EvenTrig "
                      "is a legacy-named event input that reacts to every received update.";
    }

    void setup() override {
        addParameter(trigger.set("Trigger", {0}, {0}, {1}));
        // Keep the legacy parameter name so existing presets and connections load.
        // Unlike Trigger, this is an event input: any received value is valid.
        addParameter(evenTrig.set("EvenTrig", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(min.set("Min", 0, 0, 100));
        addParameter(max.set("Max", 10, 0, 100));
        addParameter(sequentialMode.set("Sequential Mode", false));
        addOutputParameter(output.set("Output", {0}, {0}, {100}));

        triggerListener = trigger.newListener([this](vector<float>& trigger){
            if(isTransportMode()) return; // Step drives the node in transport mode
            generateRandomWrapper(trigger, previousTriggerTrigger);
        });

        evenTrigListener = evenTrig.newListener([this](vector<float>& eventTrigger){
            if(isTransportMode()) return;
            generateRandomFromEvent(eventTrigger);
        });

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
        // ---- Sync To Transport ----
        // Output is a pure function of (Seed, Step, Min, Max, mode), so scrubbing the
        // timeline gives exactly what playback gives. Connect Step to a Phasor "Cycle".
        sessionSalt = ofxOceanodeDeterministicRandom::makeSessionSalt();
        addInspectorParameter(syncToTransport_Param.set("Sync To Transport", false));
        transportListeners.push(syncToTransport_Param.newListener([this](bool &b){
            setTransportInputsVisible(b);
            if(b) computeTransport();
        }));
        transportListeners.push(step_Param.newListener([this](vector<float> &){
            if(syncToTransport_Param) computeTransport();
        }));
        transportListeners.push(seed_Param.newListener([this](int &){
            if(syncToTransport_Param) computeTransport();
        }));
        transportListeners.push(min.newListener([this](int &){
            if(syncToTransport_Param) computeTransport();
        }));
        transportListeners.push(max.newListener([this](int &){
            if(syncToTransport_Param) computeTransport();
        }));
        transportListeners.push(sequentialMode.newListener([this](bool &){
            if(syncToTransport_Param) computeTransport();
        }));
#endif
    }

#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
    void loadBeforeConnections(ofJson &json) override {
        // Restore the mode before connections so saved "Step"/"Seed" connections find their inputs.
        deserializeParameter(json, syncToTransport_Param);
    }
#endif

    void generateRandomWrapper(vector<float>& triggerSource, vector<float>& previousTriggerSource) {
        std::lock_guard<std::mutex> lock(mutex);
        int newSize = triggerSource.size();
        outputVec.resize(newSize);
        sequences.resize(newSize);
        previousTriggerSource.resize(newSize, 0);
        
        for(int i = 0; i < newSize; i++) {
            if(triggerSource[i] == 1 && triggerSource[i] != previousTriggerSource[i]) {
                generateRandom(i);
            }
        }
        previousTriggerSource = triggerSource;
        output = outputVec;
    }

    void generateRandomFromEvent(const vector<float>& eventSource) {
        std::lock_guard<std::mutex> lock(mutex);
        const int newSize = static_cast<int>(eventSource.size());
        outputVec.resize(newSize);
        sequences.resize(newSize);

        // The parameter update is the event. Its numeric value and edge do not
        // matter, so every lane is regenerated for each received update.
        for(int i = 0; i < newSize; i++) {
            generateRandom(i);
        }

        output = outputVec;
    }

    void generateRandom(int index) {
            if(!sequentialMode) {
                int randNum = min + (rand() % static_cast<int>(max - min + 1));
                while(std::count(outputVec.begin(), outputVec.end(), randNum)) {
                    randNum = min + (rand() % static_cast<int>(max - min + 1));
                }
                outputVec[index] = randNum;
            }
            else {
                if(sequences[index].size() == 0) {
                    for(int i = min; i <= max; i++) {
                        sequences[index].push_back(i);
                    }
                    std::shuffle(sequences[index].begin(), sequences[index].end(), std::mt19937(std::random_device()()));
                }
                outputVec[index] = sequences[index].back();
                sequences[index].pop_back();
            }
        }

private:
    // ======================= Transport mode =======================
#if defined(OFX_OCEANODE_HAS_GLOBAL_TRANSPORT)
    bool isTransportMode() const { return syncToTransport_Param.get(); }
    ofParameter<bool> syncToTransport_Param;
    ofParameter<vector<float>> step_Param; // only present in Sync To Transport mode
    ofParameter<int> seed_Param;           // only present in Sync To Transport mode
    ofEventListeners transportListeners;
    uint64_t sessionSalt = 0;

    void setTransportInputsVisible(bool visible){
        const bool present = getParameterGroup().contains("Step");
        if(visible && !present){
            addParameter(step_Param.set("Step", {0}, {0}, {FLT_MAX}));
            addParameter(seed_Param.set("Seed", 0, INT_MIN, INT_MAX));
        }else if(!visible && present){
            getOceanodeParameter(step_Param).removeAllConnections();
            getOceanodeParameter(seed_Param).removeAllConnections();
            removeParameter("Step");
            removeParameter("Seed");
        }
    }

    uint64_t transportSeedKey(){
        return ofxOceanodeDeterministicRandom::seedKey(seed_Param.get(), sessionSalt);
    }

    static int64_t floorDiv(int64_t a, int64_t b){
        int64_t q = a / b;
        if((a % b != 0) && ((a < 0) != (b < 0))) q--;
        return q;
    }

    // --- Sequential mode: per-channel shuffle bags. Bag b holds steps [b*R, (b+1)*R);
    // its permutation is a pure function of (seed, channel, b). O(1) with the cache.
    struct BagCache {
        bool valid = false;
        uint64_t key = 0;
        int lo = 0, hi = 0;
        int64_t bag = 0;
        vector<int> perm;
    };
    vector<BagCache> bagCaches;

    int sequentialValue(int channel, int64_t step, uint64_t key, int lo, int hi){
        const int64_t R = static_cast<int64_t>(hi) - lo + 1;
        if(R <= 1) return lo;
        const int64_t bag = floorDiv(step, R);
        const int64_t pos = step - bag * R;
        if(bagCaches.size() <= static_cast<size_t>(channel)) bagCaches.resize(channel + 1);
        BagCache &cache = bagCaches[channel];
        if(!cache.valid || cache.key != key || cache.lo != lo || cache.hi != hi || cache.bag != bag){
            cache.perm.resize(R);
            for(int64_t j = 0; j < R; j++) cache.perm[j] = lo + static_cast<int>(j);
            const uint64_t stream = (static_cast<uint64_t>(channel) << 32) | 0x5EC0000ull;
            for(int64_t j = R - 1; j > 0; j--){ // Fisher-Yates driven by the hash
                const uint64_t h = ofxOceanodeDeterministicRandom::hash(key, bag, stream + static_cast<uint64_t>(j));
                std::swap(cache.perm[j], cache.perm[h % static_cast<uint64_t>(j + 1)]);
            }
            cache.valid = true; cache.key = key; cache.lo = lo; cache.hi = hi; cache.bag = bag;
        }
        return cache.perm[pos];
    }

    // --- Non-sequential mode: same rule as generateRandom() -- a new value must differ from
    // every value currently on the outputs (other channels and the channel's own previous
    // value). That makes it a chain, so it is replayed from step 0 with a hash-driven RNG;
    // the cache makes forward playback O(1) and a backward jump replays once.
    // Channels sharing the same step are resolved together (cross-channel uniqueness);
    // channels on different steps each run their own chain.
    struct ChainCache {
        bool valid = false;
        uint64_t key = 0;
        int lo = 0, hi = 0;
        vector<int> channels;
        int64_t step = 0;
        vector<int> values;
    };
    std::map<vector<int>, ChainCache> chainCaches;

    int drawUnrepeated(uint64_t key, int64_t step, int channel, int lo, int hi,
                       const vector<int> &current, size_t assigned){
        const int64_t R = static_cast<int64_t>(hi) - lo + 1;
        if(R <= 1) return lo;
        auto taken = [&](int v){ return std::find(current.begin(), current.end(), v) != current.end(); };
        const uint64_t stream = static_cast<uint64_t>(channel) << 32;
        for(uint64_t attempt = 0; attempt < 16; attempt++){ // fast path: rejection sampling
            const int v = lo + static_cast<int>(ofxOceanodeDeterministicRandom::hash(key, step, stream + attempt) % static_cast<uint64_t>(R));
            if(!taken(v)) return v;
        }
        // Exact fallback: pick among the free values. If the range is too small to avoid
        // every current value, only avoid the ones already assigned at this step, then none.
        vector<int> allowed;
        for(int v = lo; v <= hi; v++) if(!taken(v)) allowed.push_back(v);
        if(allowed.empty()){
            for(int v = lo; v <= hi; v++)
                if(std::find(current.begin(), current.begin() + assigned, v) == current.begin() + assigned) allowed.push_back(v);
        }
        if(allowed.empty()) for(int v = lo; v <= hi; v++) allowed.push_back(v);
        const uint64_t h = ofxOceanodeDeterministicRandom::hash(key, step, stream + 0xFA11BAC0ull);
        return allowed[h % allowed.size()];
    }

    vector<int> unrepeatedValues(const vector<int> &channels, int64_t step, uint64_t key, int lo, int hi){
        if(chainCaches.size() > 64 && chainCaches.find(channels) == chainCaches.end()) chainCaches.clear();
        ChainCache &cache = chainCaches[channels];
        const bool matches = cache.valid && cache.key == key && cache.lo == lo && cache.hi == hi;
        if(matches && cache.step == step) return cache.values;

        const int64_t anchor = step < 0 ? step : 0;
        vector<int> state;
        int64_t k;
        if(matches && cache.step < step && cache.step >= anchor){
            state = cache.values;
            k = cache.step;
        }else{
            state.assign(channels.size(), 0); // same initial outputs as the free-running node
            k = anchor - 1;
        }
        while(k < step){
            k++;
            for(size_t n = 0; n < channels.size(); n++){
                state[n] = drawUnrepeated(key, k, channels[n], lo, hi, state, n);
            }
        }
        cache.valid = true; cache.key = key; cache.lo = lo; cache.hi = hi;
        cache.channels = channels; cache.step = step; cache.values = state;
        return state;
    }

    void computeTransport(){
        const auto &steps = step_Param.get();
        if(steps.empty()) return;
        std::lock_guard<std::mutex> lock(mutex);
        const int lo = std::min(min.get(), max.get());
        const int hi = std::max(min.get(), max.get());
        const uint64_t key = transportSeedKey();
        const size_t n = steps.size();
        outputVec.assign(n, lo);

        vector<int64_t> channelSteps(n);
        for(size_t i = 0; i < n; i++) channelSteps[i] = ofxOceanodeDeterministicRandom::stepFromFloat(steps[i]);

        if(sequentialMode){
            for(size_t i = 0; i < n; i++) outputVec[i] = sequentialValue(static_cast<int>(i), channelSteps[i], key, lo, hi);
        }else{
            // Group channels by step so channels advancing together stay mutually unique.
            std::map<int64_t, vector<int>> groups;
            for(size_t i = 0; i < n; i++) groups[channelSteps[i]].push_back(static_cast<int>(i));
            for(auto &group : groups){
                const vector<int> values = unrepeatedValues(group.second, group.first, key, lo, hi);
                for(size_t g = 0; g < group.second.size(); g++) outputVec[group.second[g]] = values[g];
            }
        }
        output = outputVec;
    }

#else
    bool isTransportMode() const { return false; }
#endif

    // ======================= Free-running mode =======================
    vector<float> previousTriggerTrigger;
    vector<int> outputVec;
    vector<vector<int>> sequences;
    ofParameter<vector<float>> trigger;
    ofParameter<vector<float>> evenTrig;
    ofParameter<int> min;
    ofParameter<int> max;
    ofParameter<bool> sequentialMode;
    ofParameter<vector<int>> output;
    ofEventListener triggerListener;
    ofEventListener evenTrigListener;

    std::mutex mutex;
};

#endif /* UnrepeatedRandom_h */
