#ifndef vectorBlur_h
#define vectorBlur_h

#include "ofxOceanodeNodeModel.h"

class vectorBlur : public ofxOceanodeNodeModel {
public:
    vectorBlur() : ofxOceanodeNodeModel("Vector Blur") {
    }

    void setup() override {
        description="Vector Blur applies a localized averaging effect to a given input vector. The 'Influence' parameter determines the intensity of the blur, while the 'Area' limits how many surrounding indices can affect each point. An Influence of 0 retains the original vector, whereas an Influence of 1 averages the entire vector.";
        addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
        addParameter(influence.set("Influence", {0.5f}, {0.0f}, {1.0f}));
        addParameter(area.set("Area", 1, 0, 1000));

        addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));

        inputListener = input.newListener([this](vector<float> &values) {
            calculate();
        });

        influenceListener = influence.newListener([this](vector<float> &values) {
            calculate();
        });

        
    }

    void calculate() {
        const auto& in = input.get();
        const auto& influences = influence.get();
        bool isVectorInfluence = influences.size() > 1;

        area.setMax(in.size());
        std::vector<float> out(in.size(), 0.0f);

        for(size_t i = 0; i < in.size(); i++) {
            float currentInfluence = isVectorInfluence ?
                                     (i < influences.size() ? influences[i] : influences.back())
                                     : influences.front();

            if(currentInfluence == 0.0f){
                out[i] = in[i];
                continue;
            }
            else if(currentInfluence == 1.0f){
                out[i] = std::accumulate(in.begin(), in.end(), 0.0f) / in.size();
                continue;
            }

            float weightSum = 0.0f;
            int minInfluenceIndex = max(static_cast<int>(i) - area.get(), 0); // Left boundary of the area
            int maxInfluenceIndex = min(static_cast<int>(i) + area.get(), static_cast<int>(in.size() - 1)); // Right boundary of the area

            for(size_t j = minInfluenceIndex; j <= maxInfluenceIndex; j++) {
                float dist = abs((int)i - (int)j);
                float influenceWeight = pow(currentInfluence, dist);
                out[i] += in[j] * influenceWeight;
                weightSum += influenceWeight;
            }
            out[i] /= weightSum;
        }

        output = out;
    }



private:
    ofParameter<vector<float>> input;
    ofParameter<vector<float>> influence;
    ofParameter<int> area;

    ofParameter<vector<float>> output;

    ofEventListener inputListener;
    ofEventListener influenceListener;

};

#endif /* vectorBlur_h */
