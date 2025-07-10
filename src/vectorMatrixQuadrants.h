#ifndef vectorMatrixQuadrants_h
#define vectorMatrixQuadrants_h

#include "ofxOceanodeNodeModel.h"

class vectorMatrixQuadrants : public ofxOceanodeNodeModel {
public:
	vectorMatrixQuadrants() : ofxOceanodeNodeModel("Vector Matrix Quadrants") {}

	void setup() override {
		description = "Creates quadrants on a matrix. QuadrantH and QuadrantW define the size of each quadrant. QuadSel selects which quadrant is highlighted (filled with 1s). The output is a 1D vector representing the unfolded matrix.";
		
		addParameter(matrixWidth.set("Matrix W", 4, 1, 32));
		addParameter(matrixHeight.set("Matrix H", 4, 1, 32));
		addParameter(quadrantWidth.set("Quadrant W", 2, 1, 16));
		addParameter(quadrantHeight.set("Quadrant H", 2, 1, 16));
		addParameter(quadrantSelect.set("Quad Sel", 0, 0, 15));
		addOutputParameter(output.set("Output", {0}, {0}, {1}));
		addOutputParameter(numQuads.set("Num Quads", 0, 0, INT_MAX));
		
		// Add listeners to trigger calculation when parameters change
		listeners.push(matrixWidth.newListener([this](int &){
			updateQuadrantCount();
			calculate();
		}));
		
		listeners.push(matrixHeight.newListener([this](int &){
			updateQuadrantCount();
			calculate();
		}));
		
		listeners.push(quadrantWidth.newListener([this](int &){
			updateQuadrantCount();
			calculate();
		}));
		
		listeners.push(quadrantHeight.newListener([this](int &){
			updateQuadrantCount();
			calculate();
		}));
		
		listeners.push(quadrantSelect.newListener([this](int &){
			calculate();
		}));
		
		// Initial calculation
		updateQuadrantCount();
		calculate();
	}

private:
	void updateQuadrantCount() {
		// Calculate how many quadrants fit in the matrix
		int quadrantsX = matrixWidth.get() / quadrantWidth.get();
		int quadrantsY = matrixHeight.get() / quadrantHeight.get();
		int maxQuadrants = quadrantsX * quadrantsY;
		
		// Update the max value for quadrantSelect
		quadrantSelect.setMax(std::max(0, maxQuadrants - 1));
		
		// Update the numQuads output
		numQuads.set(maxQuadrants);
		
		// Ensure current selection is within bounds
		if (quadrantSelect.get() >= maxQuadrants) {
			quadrantSelect.set(std::max(0, maxQuadrants - 1));
		}
	}
	
	void calculate() {
		int mWidth = matrixWidth.get();
		int mHeight = matrixHeight.get();
		int qWidth = quadrantWidth.get();
		int qHeight = quadrantHeight.get();
		int qSel = quadrantSelect.get();
		
		// Calculate how many quadrants fit in each dimension
		int quadrantsX = mWidth / qWidth;
		int quadrantsY = mHeight / qHeight;
		int totalQuadrants = quadrantsX * quadrantsY;
		
		// If selected quadrant is out of bounds, use 0
		if (qSel >= totalQuadrants) {
			qSel = 0;
		}
		
		// Calculate which quadrant is selected (row, col)
		int selectedQuadrantRow = qSel / quadrantsX;
		int selectedQuadrantCol = qSel % quadrantsX;
		
		// Create the matrix as a 1D vector
		vector<float> result(mWidth * mHeight, 0.0f);
		
		// Fill the selected quadrant with 1s
		for (int row = 0; row < mHeight; row++) {
			for (int col = 0; col < mWidth; col++) {
				// Determine which quadrant this cell belongs to
				int quadrantRow = row / qHeight;
				int quadrantCol = col / qWidth;
				
				// Check if this cell is in the selected quadrant
				if (quadrantRow == selectedQuadrantRow && quadrantCol == selectedQuadrantCol) {
					result[row * mWidth + col] = 1.0f;
				}
			}
		}
		
		output = result;
	}
	
	ofParameter<int> matrixWidth;
	ofParameter<int> matrixHeight;
	ofParameter<int> quadrantWidth;
	ofParameter<int> quadrantHeight;
	ofParameter<int> quadrantSelect;
	ofParameter<vector<float>> output;
	ofParameter<int> numQuads;
	
	ofEventListeners listeners;
};

#endif /* vectorMatrixQuadrants_h */
