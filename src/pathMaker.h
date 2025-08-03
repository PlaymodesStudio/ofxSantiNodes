#ifndef pathMaker_h
#define pathMaker_h

#include "ofxOceanodeNodeModel.h"

class pathMaker : public ofxOceanodeNodeModel {
public:
	pathMaker() : ofxOceanodeNodeModel("Path Maker") {
		description = "Creates a continuous path from input points. Input points define the ordered vertices of the path. The 'Close' parameter connects the last point back to the first point to create a closed figure. Output is compatible with trimPath and other path processing nodes.";
	}
	
	void setup() override {
		addParameter(x_in.set("X.In", {0.5}, {0}, {1}));
		addParameter(y_in.set("Y.In", {0.5}, {0}, {1}));
		addParameter(close.set("Close", false));
		addOutputParameter(x_out.set("X.Out", {0.5}, {0}, {1}));
		addOutputParameter(y_out.set("Y.Out", {0.5}, {0}, {1}));
		
		listeners.push(x_in.newListener([this](vector<float> &vf){
			calculatePath();
		}));
		
		listeners.push(y_in.newListener([this](vector<float> &vf){
			calculatePath();
		}));
		
		listeners.push(close.newListener([this](bool &b){
			calculatePath();
		}));
	}
	
private:
	void calculatePath() {
		// Check if input vectors have the same size and contain valid data
		if(x_in->size() != y_in->size() || x_in->size() == 0) {
			// If input is invalid, output empty vectors
			x_out = vector<float>();
			y_out = vector<float>();
			return;
		}
		
		vector<float> x_temp;
		vector<float> y_temp;
		
		// Reserve space for efficiency
		// We need space for all input points plus potentially one more if closing
		// plus separators for multiple shapes
		x_temp.reserve(x_in->size() * 2);
		y_temp.reserve(y_in->size() * 2);
		
		// Process input points to create continuous paths
		// Input can contain multiple shapes separated by -1 values
		vector<glm::vec2> currentShape;
		
		for(size_t i = 0; i < x_in->size(); i++) {
			if(x_in->at(i) == -1 || y_in->at(i) == -1) {
				// End of current shape - process it
				if(!currentShape.empty()) {
					addShapeToOutput(currentShape, x_temp, y_temp);
					currentShape.clear();
				}
			} else {
				// Add point to current shape
				currentShape.emplace_back(x_in->at(i), y_in->at(i));
			}
		}
		
		// Process the last shape if it doesn't end with -1
		if(!currentShape.empty()) {
			addShapeToOutput(currentShape, x_temp, y_temp);
		}
		
		// Set output
		x_out = x_temp;
		y_out = y_temp;
	}
	
	void addShapeToOutput(const vector<glm::vec2> &shape, vector<float> &x_temp, vector<float> &y_temp) {
		if(shape.empty()) return;
		
		// Add all points of the shape to create a continuous path
		for(const auto &point : shape) {
			x_temp.push_back(point.x);
			y_temp.push_back(point.y);
		}
		
		// If close is enabled and we have more than 2 points, add the first point again
		if(close && shape.size() > 2) {
			x_temp.push_back(shape[0].x);
			y_temp.push_back(shape[0].y);
		}
		
		// Add path separator (-1) to mark the end of this path
		x_temp.push_back(-1);
		y_temp.push_back(-1);
	}
	
	ofParameter<vector<float>> x_in, y_in;
	ofParameter<vector<float>> x_out, y_out;
	ofParameter<bool> close;
	
	ofEventListeners listeners;
};

#endif /* pathMaker_h */
