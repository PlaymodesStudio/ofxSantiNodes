#ifndef segmentLength_h
#define segmentLength_h

#include "ofxOceanodeNodeModel.h"

class segmentLength : public ofxOceanodeNodeModel {
public:
    segmentLength() : ofxOceanodeNodeModel("Segment Length"){}
    
    void setup() override {
        description = "calculates the length of segments and their midpoints";
        addParameter(pointsX.set("Points.X", {0.5}, {0}, {1}));
        addParameter(pointsY.set("Points.Y", {0.5}, {0}, {1}));
        addParameter(lengths.set("Lengths", {0.5}, {0}, {1}));
        addParameter(separator.set("Separator", true));
        addOutputParameter(output.set("Output", {0}, {0}, {1}));  // Output parameter for visualization
        addOutputParameter(midpointX.set("Midpoint.X", {0}, {0}, {1}));  // New midpoint X output
        addOutputParameter(midpointY.set("Midpoint.Y", {0}, {0}, {1}));  // New midpoint Y output

        listener = pointsX.newListener([this](vector<float> &vf){
            calculate();
        });
    }
    
    void calculate() {
		if(pointsX.get().size()!=0 && pointsY.get().size()!=0 && pointsX.get().size()==pointsY.get().size())
		{
			vector<float> aux;
			vector<float> midX;
			vector<float> midY;
			glm::vec2 point1, point2;
			
			int i = 0;
			while(i < pointsX.get().size()) {
				// Check for the separator
				if(separator && (pointsX.get()[i] == -1 || pointsY.get()[i] == -1)) {
					i++;
					continue;
				}

				// Get first point
				point1.x = pointsX.get()[i];
				point1.y = pointsY.get()[i];
				i++;
				
				// If there are no more points or if a separator is found, continue to the next loop
				if(i >= pointsX.get().size() || (separator && (pointsX.get()[i] == -1 || pointsY.get()[i] == -1))) {
					continue;
				}
				
				// Get second point
				point2.x = pointsX.get()[i];
				point2.y = pointsY.get()[i];
				
				// Calculate distance and add it to the aux vector
				aux.push_back(glm::distance(point1, point2));
				
				// Calculate and store midpoints
				midX.push_back((point1.x + point2.x) / 2.0f);
				midY.push_back((point1.y + point2.y) / 2.0f);
			}
			
			lengths = aux;
			output = lengths;  // Copy the lengths to the output parameter for visualization
			midpointX = midX;  // Set midpoint X output
			midpointY = midY;  // Set midpoint Y output

		}
    }
    
    void update(ofEventArgs &a) override {
        // Intentionally left empty
    }
        
private:
    ofParameter<vector<float>> pointsX;
    ofParameter<vector<float>> pointsY;
    ofParameter<vector<float>> lengths;
    ofParameter<bool> separator;
    ofParameter<vector<float>> output;
    ofParameter<vector<float>> midpointX;
    ofParameter<vector<float>> midpointY;
    
    ofEventListener listener;
};

#endif /* segmentLength_h */
