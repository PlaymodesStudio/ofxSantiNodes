//Created by Santi Vilanova
//during a COVID quarantine
//between 24 and 29 of May, 2023

#ifndef segmentLength_h
#define segmentLength_h

#include "ofxOceanodeNodeModel.h"

class segmentLength : public ofxOceanodeNodeModel
{
public:
    segmentLength() : ofxOceanodeNodeModel("Segment Length"){}
    
    void setup() override
    {
        description = "calculates the length of segments";
        addParameter(pointsX.set("Points.X", {0.5}, {0}, {1}));
        addParameter(pointsY.set("Points.Y", {0.5}, {0}, {1}));
        addParameter(lengths.set("Lengths", {0.5}, {0}, {1}));
        addParameter(separator.set("Separator", true));
        addOutputParameter(output.set("Output", {0}, {0}, {1}));  // Output parameter for visualization

        listener = pointsX.newListener([this](vector<float> &vf){
            calculate();
        });
    }
    
    void calculate() {
        vector<float> aux;
        glm::vec2 point1, point2;
        
        int i = 0;
        while(i < pointsX.get().size()){
            // Check for the separator
            if(separator && (pointsX.get()[i] == -1 || pointsY.get()[i] == -1)){
                i++;
                continue;
            }

            // Get first point
            point1.x = pointsX.get()[i];
            point1.y = pointsY.get()[i];
            i++;
            
            // If there are no more points or if a separator is found, continue to the next loop
            if(i >= pointsX.get().size() || (separator && (pointsX.get()[i] == -1 || pointsY.get()[i] == -1))){
                continue;
            }
            
            // Get second point
            point2.x = pointsX.get()[i];
            point2.y = pointsY.get()[i];
            
            // Calculate distance and add it to the aux vector
            aux.push_back(glm::distance(point1, point2));
        }
        
        lengths = aux;
        output = lengths;  // Copy the lengths to the output parameter for visualization
    }
    
    void update(ofEventArgs &a) override
    {
        // Intentionally left empty
    }
        
    
private:
    ofParameter<vector<float>> pointsX;
    ofParameter<vector<float>> pointsY;
    ofParameter<vector<float>> lengths;
    ofParameter<bool> separator;

    ofParameter<vector<float>> output;  // Output parameter for visualization
    
    ofEventListener listener;
    

};


#endif /* segmentLength_h */
