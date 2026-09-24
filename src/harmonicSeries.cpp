#include "harmonicSeries.h"
#include <algorithm> // For std::sort
#include <cmath>
#include <tuple>

void harmonicSeries::setup() {
    description = "Generates a selectable range of harmonic or subharmonic series for given pitches. "
                  "Pow redistributes the selected partials toward the low or high end of the range. "
                  "Offers different shapes for amplitude distributions across the harmonic series, as well a simulation of LP and HP filtering.";
    
    previousDetuneAmounts.clear();
    detuneFactors.clear();

    addSeparator("SOURCE", ofColor(200));
    addParameter(pitch.set("Pitch", {0}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(ampIn.set("Amp In", {1}, {0}, {1}));
    addParameter(subharmonic.set("Subharmonic", false));

    addSeparator("PARTIAL RANGE", ofColor(200));
    addParameter(partialsNum.set("Partials", 1, 1, INT_MAX));
    addParameter(partialStart.set("Partial Start", 1, 1, INT_MAX));
    addParameter(partialJump.set("Partial Jump", 1.0f, 0.0f, FLT_MAX));
    addParameter(partialPow.set("Pow", 1.0f, 0.05f, 8.0f));
    addParameter(harmonicStretch.set("Stretch", 1.0f, 0.05f, 8.0f));
    addParameter(detuneAmount.set("Detune", {0}, {0}, {1}));

    addSeparator("AMPLITUDE", ofColor(200));
    addParameterDropdown(harmonicShape, "Shape", 0, {"None", "Square", "Saw", "Triangle"});
    addParameter(oddHarmonicAmp.set("Odd", 1.0f, 0.0f, 1.0f));
    addParameter(evenHarmonicAmp.set("Even", 1.0f, 0.0f, 1.0f));
    addParameter(lpCutoff.set("LP Cut", {1}, {0}, {1}));
    addParameter(hpCutoff.set("HP Cut", {0}, {0}, {1}));

    addSeparator("OUTPUTS", ofColor(200));
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

    listeners.push_back(std::make_unique<ofEventListener>(partialStart.newListener([this](int& i) {
        calculate();
    })));

    listeners.push_back(std::make_unique<ofEventListener>(partialJump.newListener([this](float& value) {
        calculate();
    })));

    listeners.push_back(std::make_unique<ofEventListener>(partialPow.newListener([this](float& value) {
        calculate();
    })));

    listeners.push_back(std::make_unique<ofEventListener>(subharmonic.newListener([this](bool& enabled) {
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
    
    listeners.push_back(std::make_unique<ofEventListener>(detuneAmount.newListener([this](vector<float> &amounts) {
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
    vector<float> currentDetuneAmounts = detuneAmount.get();
    int numPartials = partialsNum.get();
    
    // Resize vectors if needed
    if (detuneFactors.size() != numPartials) {
        int oldSize = detuneFactors.size();
        detuneFactors.resize(numPartials);
        previousDetuneAmounts.resize(numPartials);
        
        // Initialize new elements
        for (int i = oldSize; i < numPartials; i++) {
            float currentDetuneAmount;
            if (i < currentDetuneAmounts.size()) {
                currentDetuneAmount = currentDetuneAmounts[i];
            } else if (!currentDetuneAmounts.empty()) {
                currentDetuneAmount = currentDetuneAmounts.back();
            } else {
                currentDetuneAmount = 0.0f;
            }
            
            float maxDetuneFactor = pow(2.0, currentDetuneAmount / 12.0f);
            detuneFactors[i] = ofRandom(2.0 - maxDetuneFactor, maxDetuneFactor);
            previousDetuneAmounts[i] = currentDetuneAmount;
        }
    }

    // For each partial, only regenerate if detune amount changed
    for (int i = 0; i < numPartials; i++) {
        // Get the appropriate detune amount for this partial
        float currentDetuneAmount;
        if (i < currentDetuneAmounts.size()) {
            currentDetuneAmount = currentDetuneAmounts[i];
        } else if (!currentDetuneAmounts.empty()) {
            currentDetuneAmount = currentDetuneAmounts.back();
        } else {
            currentDetuneAmount = 0.0f;
        }

        // Only generate new random value if detune amount changed
        if (currentDetuneAmount != previousDetuneAmounts[i]) {
            float maxDetuneFactor = pow(2.0, currentDetuneAmount / 12.0f);
            detuneFactors[i] = ofRandom(2.0 - maxDetuneFactor, maxDetuneFactor);
            previousDetuneAmounts[i] = currentDetuneAmount;
        }
    }
}

void harmonicSeries::calculate() {
    vector<float> out;
    vector<float> outPitch;
    vector<float> outAmplitudes;
    int numPartials = partialsNum.get();
    int firstPartial = partialStart.get();
    float partialStep = partialJump.get();
    bool useSubharmonics = subharmonic.get();
    int shapeIndex = harmonicShape.get();
    vector<float> inputAmplitudes = ampIn.get();
    vector<float> detuneAmounts = detuneAmount.get();
    float partialPower = partialPow.get();
    float stretchFactor = harmonicStretch.get();
    float stretchedStart = (firstPartial == 1)
                           ? 1.0f
                           : pow(static_cast<float>(firstPartial), stretchFactor);

    for (int idx = 0; idx < pitch.get().size(); idx++) {
        const auto& p = pitch.get()[idx];
        float inputAmp = (idx < inputAmplitudes.size()) ? inputAmplitudes[idx] : 1.0f;

        float freq = 440.0f * pow(2.0f, (p - 69.0f) / 12.0f);
        float currentLpCutoff = (idx < lpCutoff.get().size()) ? 440.0f * pow(2.0f, (127 * lpCutoff.get()[idx] - 69.0f) / 12.0f) : FLT_MAX;
        float currentHpCutoff = (idx < hpCutoff.get().size()) ? 440.0f * pow(2.0f, (127 * hpCutoff.get()[idx] - 69.0f) / 12.0f) : 0.0f;
        float oddAmp = oddHarmonicAmp.get();
        float evenAmp = evenHarmonicAmp.get();

        for (int partialOffset = 0; partialOffset < numPartials; partialOffset++) {
            int curvedPartialOffset = partialOffset;
            if (numPartials > 1 && partialPower != 1.0f) {
                double normalizedOffset = static_cast<double>(partialOffset) /
                                          static_cast<double>(numPartials - 1);
                curvedPartialOffset = static_cast<int>(std::round(
                    std::pow(normalizedOffset, static_cast<double>(partialPower)) *
                    static_cast<double>(numPartials - 1)));
            }

            float partialValue = static_cast<float>(firstPartial) + curvedPartialOffset * partialStep;
            bool isEvenPartial = std::abs(std::fmod(partialValue, 2.0f)) < 0.0001f;
            float stretchedPartial = (partialValue == 1.0f)
                                     ? 1.0f
                                     : pow(partialValue, stretchFactor);
            float partialRatio = stretchedPartial / stretchedStart;
            float detuneFactor = (partialOffset == 0) ? 1.0f : detuneFactors[partialOffset];
            float partialFreq = (useSubharmonics
                                 ? freq / partialRatio
                                 : freq * partialRatio) * detuneFactor;
                       

            out.push_back(partialFreq);
            outPitch.push_back(69 + 12 * log2(partialFreq / 440.0f));

            float amp;
            if (shapeIndex == 0) { // None
                amp = 1.0f;
            } else if (shapeIndex == 1) { // Square
                amp = isEvenPartial ? 0 : 1.0f / partialValue;
            } else if (shapeIndex == 2) { // Saw
                amp = 1.0f / partialValue;
            } else { // Triangle
                amp = isEvenPartial ? 0 : 1.0f / (partialValue * partialValue);
            }

            // Apply the HP and LP filters
            if (partialFreq > currentLpCutoff) {
                amp *= exp(-0.1 * (partialFreq - currentLpCutoff));
            }
            if (partialFreq < currentHpCutoff) {
                amp *= exp(-0.1 * (currentHpCutoff - partialFreq));
            }

            // Factor in the odd/even amplitude control
            float currentAmp = isEvenPartial ? evenAmp : oddAmp;
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
