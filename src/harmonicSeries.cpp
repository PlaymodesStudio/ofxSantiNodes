#include "harmonicSeries.h"
#include <algorithm> // For std::sort
#include <tuple>

void harmonicSeries::setup() {
    description = "Generates the harmonic series of given pitches. "
                  "Offers different shapes for amplitude distributions across the harmonic series, as well a simulation of LP and HP filtering.";

    addParameter(pitch.set("Pitch", {0}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(partialsNum.set("Partials", 1, 1, INT_MAX));
    addParameterDropdown(harmonicShape, "Shape", 0, {"None", "Square", "Saw", "Triangle"});
    addParameter(ampIn.set("Amp In", {1}, {0}, {1}));
    addParameter(lpCutoff.set("LP Cut", {1}, {0}, {1}));
    addParameter(hpCutoff.set("HP Cut", {0}, {0}, {1}));
    addParameter(detuneAmount.set("Detune", 0.0f, 0.0f, 1.0f));
    addParameter(oddHarmonicAmp.set("Odd", 1.0f, 0.0f, 1.0f));
    addParameter(evenHarmonicAmp.set("Even", 1.0f, 0.0f, 1.0f));
    addParameter(harmonicStretch.set("Stretch", 1.0f, 0.05f, 8.0f));

    addOutputParameter(output.set("Output Hz", {0}, {-FLT_MAX}, {FLT_MAX}));
    addOutputParameter(outputPitch.set("Output Pitch", {0}, {-FLT_MAX}, {FLT_MAX}));
    addOutputParameter(amplitudes.set("Amplitudes", {0}, {0}, {1}));
    addOutputParameter(sortedFreq.set("Sorted Frequencies", {0}, {-FLT_MAX}, {FLT_MAX}));
    addOutputParameter(sortedPitch.set("Sorted Pitches", {0}, {-FLT_MAX}, {FLT_MAX}));
    addOutputParameter(sortedAmp.set("Sorted Amplitudes", {0}, {0}, {1}));

    listeners.push_back(std::make_unique<ofEventListener>(harmonicShape.newListener([this](int &shapeIndex) {
        calculate();
    })));

    listeners.push_back(std::make_unique<ofEventListener>(pitch.newListener([this](vector<float>& vf) {
        calculate();
    })));

    listeners.push_back(std::make_unique<ofEventListener>(partialsNum.newListener([this](int& i) {
        calculateDetuneFactors();
        calculate();
    })));

    listeners.push_back(std::make_unique<ofEventListener>(ampIn.newListener([this](vector<float>& amps) {
        calculate();
    })));
    listeners.push_back(std::make_unique<ofEventListener>(lpCutoff.newListener([this](vector<float>& cutoffs) {
        calculate();
    })));

    listeners.push_back(std::make_unique<ofEventListener>(hpCutoff.newListener([this](vector<float>& cutoffs) {
        calculate();
    })));
    
    listeners.push_back(std::make_unique<ofEventListener>(detuneAmount.newListener([this](float &amount) {
            calculateDetuneFactors();
            calculate();
        })));
    listeners.push_back(std::make_unique<ofEventListener>(oddHarmonicAmp.newListener([this](float &amp) {
           calculate();
       })));
       
       listeners.push_back(std::make_unique<ofEventListener>(evenHarmonicAmp.newListener([this](float &amp) {
           calculate();
       })));
    listeners.push_back(std::make_unique<ofEventListener>(harmonicStretch.newListener([this](float& stretchValue) {
            calculate();
        })));
    
    calculateDetuneFactors();
    calculate();
}

// function to calculate the detune factors for each harmonic
void harmonicSeries::calculateDetuneFactors() {
    float maxDetuneInSemitones = detuneAmount.get();
    float maxDetuneFactor = pow(2.0, maxDetuneInSemitones / 12.0f);

    // Store the computed detune factors
    detuneFactors.clear();
    for (int i = 1; i <= partialsNum.get(); i++) {
        float detuneFactor = ofRandom(2.0 - maxDetuneFactor, maxDetuneFactor);
        detuneFactors.push_back(detuneFactor);
    }
}

void harmonicSeries::calculate() {
    vector<float> out;
    vector<float> outPitch;
    vector<float> outAmplitudes;
    int numPartials = partialsNum.get();
    int shapeIndex = harmonicShape.get();
    vector<float> inputAmplitudes = ampIn.get();
    float maxDetuneInSemitones = detuneAmount.get();
    float maxDetuneFactor = pow(2.0, maxDetuneInSemitones / 12.0f);
    float stretchFactor = harmonicStretch.get();

    for (int idx = 0; idx < pitch.get().size(); idx++) {
        const auto& p = pitch.get()[idx];
        float inputAmp = (idx < inputAmplitudes.size()) ? inputAmplitudes[idx] : 1.0f;

        float freq = 440.0f * pow(2.0f, (p - 69.0f) / 12.0f);
        float currentLpCutoff = (idx < lpCutoff.get().size()) ? 440.0f * pow(2.0f, (127 * lpCutoff.get()[idx] - 69.0f) / 12.0f) : FLT_MAX;
        float currentHpCutoff = (idx < hpCutoff.get().size()) ? 440.0f * pow(2.0f, (127 * hpCutoff.get()[idx] - 69.0f) / 12.0f) : 0.0f;
        float oddAmp = oddHarmonicAmp.get();
        float evenAmp = evenHarmonicAmp.get();

        for (int i = 1; i <= numPartials; i++) {
            float stretchedHarmonic = (i == 1) ? i : pow(i, stretchFactor);
            float detuneFactor = (i == 1) ? 1.0 : detuneFactors[i - 1];
            float partialFreq = freq * stretchedHarmonic * detuneFactor;

            out.push_back(partialFreq);
            outPitch.push_back(69 + 12 * log2(partialFreq / 440.0f));

            float amp;
            if (shapeIndex == 0) { // None
                amp = 1.0f;
            } else if (shapeIndex == 1) { // Square
                amp = (i % 2 == 0) ? 0 : 1.0f / i;
            } else if (shapeIndex == 2) { // Saw
                amp = 1.0f / i;
            } else { // Triangle
                amp = (i % 2 == 0) ? 0 : 1.0f / (i * i);
            }

            // Apply the HP and LP filters
            if (partialFreq > currentLpCutoff) {
                amp *= exp(-0.1 * (partialFreq - currentLpCutoff));
            }
            if (partialFreq < currentHpCutoff) {
                amp *= exp(-0.1 * (currentHpCutoff - partialFreq));
            }

            // Factor in the odd/even amplitude control
            float currentAmp = (i % 2 == 0) ? evenAmp : oddAmp;
            outAmplitudes.push_back(amp * currentAmp * inputAmp);
        }
    }

    // Combine into a single list of tuples for sorting
    vector<std::tuple<float, float, float>> combined;
    for (size_t i = 0; i < out.size(); ++i) {
        combined.push_back(std::make_tuple(out[i], outPitch[i], outAmplitudes[i]));
    }

    // Sort based on frequencies
    std::sort(combined.begin(), combined.end(), [](const auto &a, const auto &b) {
        return std::get<0>(a) < std::get<0>(b);
    });

    // Extract sorted lists
    vector<float> sortedFrequencies(out.size());
    vector<float> sortedPitches(out.size());
    vector<float> sortedAmplitudes(out.size());
    for (size_t i = 0; i < combined.size(); ++i) {
        sortedFrequencies[i] = std::get<0>(combined[i]);
        sortedPitches[i] = std::get<1>(combined[i]);
        sortedAmplitudes[i] = std::get<2>(combined[i]);
    }

    // Set the output parameters
    output = out;
    outputPitch = outPitch;
    amplitudes = outAmplitudes;
    sortedFreq = sortedFrequencies;
    sortedPitch = sortedPitches;
    sortedAmp = sortedAmplitudes;
}


