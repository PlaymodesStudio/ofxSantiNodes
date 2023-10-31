//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#include "vectorPointer.h"

void vectorPointer::setup() {
    description = "The vectorPointer node takes a source vector and uses the pointer vector to find "
                     "the closest values in the source. If ampTrig is true, the resulting output "
                     "vector will be multiplied by the amps vector. Otherwise, it will be set to 1 for "
                     "every closest value found. The output represents the 'activity' at each position "
                     "of the source vector.";

    addParameter(source.set("Source", {0}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(pointer.set("Pointer", {0}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(amps.set("Amps", {1}, {-FLT_MAX}, {FLT_MAX}));
    addParameter(ampTrig.set("Amp Trig", true));
    addOutputParameter(output.set("Output", {0}, {0}, {1}));

    listeners.push_back(std::make_unique<ofEventListener>(source.newListener([this](vector<float>& vf) {
        calculate();
    })));

    listeners.push_back(std::make_unique<ofEventListener>(pointer.newListener([this](vector<float>& vf) {
        calculate();
    })));

    listeners.push_back(std::make_unique<ofEventListener>(amps.newListener([this](vector<float>& vf) {
        if (ampTrig.get()) {
            calculate();
        }
    })));
}

void vectorPointer::calculate() {
    vector<float> out(source.get().size(), 0.0);
    bool useDefaultAmp = amps.get().empty() || amps.get().size() != pointer.get().size();

    for (size_t i = 0; i < pointer.get().size(); i++) {
        auto& p = pointer.get()[i];

        if(p == 0) {
            continue;
        }

        auto closest_iter = std::min_element(source.get().begin(), source.get().end(),
            [&](const float& a, const float& b) {return std::abs(a - p) < std::abs(b - p);});

        if (closest_iter != source.get().end()) {
            int closest_idx = std::distance(source.get().begin(), closest_iter);
            out[closest_idx] = useDefaultAmp ? 1.0 : amps.get()[i];
        }
    }

    output = out;
}
