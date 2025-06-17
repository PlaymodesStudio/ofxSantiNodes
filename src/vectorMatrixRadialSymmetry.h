#ifndef vectorMatrixRadialSymmetry_h
#define vectorMatrixRadialSymmetry_h

#include "ofxOceanodeNodeModel.h"

class vectorMatrixRadialSymmetry : public ofxOceanodeNodeModel {
public:
	vectorMatrixRadialSymmetry() : ofxOceanodeNodeModel("Vector Matrix Radial Symmetry") {}

	void setup() override {
		description = "Applies radial symmetry from matrix center using concentric rings. Two modes: Spatial (moves values) or Value Inversion (applies 1-x). Input is mapped row-by-row to a Columns x Rows matrix.";
		
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(columns.set("Cols", 3, 1, 100));
		addParameter(rows.set("Rows", 3, 1, 100));
		addParameter(radialStages.set("Radial Stages", 0, 0, 64));
		addParameter(radialOffset.set("Radial Offset", 0, 0, 1));
		addParameter(useValueInversion.set("Value Inv", false));
		addParameter(useInversions.set("Use Inv", true));
		addOutputParameter(output.set("Output", {0}, {-FLT_MAX}, {FLT_MAX}));
		addOutputParameter(inversions.set("Inversions", {0}, {0}, {1}));
		
		// Add listeners to trigger processing when parameters change
		listeners.push(input.newListener([this](vector<float> &){
			process();
		}));
		
		listeners.push(columns.newListener([this](int &){
			process();
		}));
		
		listeners.push(rows.newListener([this](int &){
			process();
		}));
		
		listeners.push(radialStages.newListener([this](int &){
			process();
		}));
		
		listeners.push(radialOffset.newListener([this](float &){
			process();
		}));
		
		listeners.push(useValueInversion.newListener([this](bool &){
			process();
		}));
		
		listeners.push(useInversions.newListener([this](bool &){
			process();
		}));
	}

private:
	void process() {
		const vector<float>& inputVec = input.get();
		if (inputVec.empty()) {
			output.set(vector<float>());
			return;
		}
		
		int cols = std::max(1, columns.get());
		int numRows = std::max(1, rows.get());
		
		// Create matrix from input vector (row-by-row)
		vector<vector<float>> matrix(numRows, vector<float>(cols));
		vector<vector<int>> inversionMatrix(numRows, vector<int>(cols, 0)); // Track inversions
		fillMatrixFromVector(inputVec, matrix, cols, numRows);
		
		// Apply radial reflections
		applyRadialReflections(matrix, inversionMatrix, cols, numRows);
		
		// Convert back to vector (row-by-row)
		vector<float> result;
		vector<int> inversionResult;
		matrixToVector(matrix, result, cols, numRows);
		inversionMatrixToVector(inversionMatrix, inversionResult, cols, numRows);
		
		// Ensure output is same size as input
		result.resize(inputVec.size());
		inversionResult.resize(inputVec.size(), 0);
		
		output.set(result);
		inversions.set(inversionResult);
	}
	
	void fillMatrixFromVector(const vector<float>& inputVec, vector<vector<float>>& matrix, int cols, int numRows) {
		int inputSize = inputVec.size();
		
		for (int row = 0; row < numRows; row++) {
			for (int col = 0; col < cols; col++) {
				int index = row * cols + col;
				// Repeat pattern if input is smaller than matrix
				matrix[row][col] = inputVec[index % inputSize];
			}
		}
	}
	
	void applyRadialReflections(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		if (radialStages.get() <= 0) return;
		
		if (useInversions.get()) {
			if (useValueInversion.get()) {
				applyRadialValueInversionZones(matrix, inversionMatrix, cols, numRows);
			} else {
				applyRadialSpatialReflectionZones(matrix, inversionMatrix, cols, numRows);
			}
		} else {
			// Just apply offsets without any inversions
			applyRadialOffsetOnlyZones(matrix, inversionMatrix, cols, numRows);
		}
	}
	
	float getDistanceFromCenter(int col, int row, int cols, int numRows) {
		float centerX = (cols - 1) / 2.0f;
		float centerY = (numRows - 1) / 2.0f;
		float dx = col - centerX;
		float dy = row - centerY;
		return sqrt(dx * dx + dy * dy);
	}
	
	float getMaxDistance(int cols, int numRows) {
		float centerX = (cols - 1) / 2.0f;
		float centerY = (numRows - 1) / 2.0f;
		// Distance to farthest corner
		float maxDist = 0;
		float corners[4][2] = {{0.0f, 0.0f}, {(float)(cols-1), 0.0f}, {0.0f, (float)(numRows-1)}, {(float)(cols-1), (float)(numRows-1)}};
		for (int i = 0; i < 4; i++) {
			float dx = corners[i][0] - centerX;
			float dy = corners[i][1] - centerY;
			float dist = sqrt(dx * dx + dy * dy);
			maxDist = std::max(maxDist, dist);
		}
		return maxDist;
	}
	
	int getRadialZone(float distance, float maxDistance, int numStages) {
		if (maxDistance == 0) return 0;
		float normalizedDistance = distance / maxDistance;
		int zone = (int)(normalizedDistance * numStages);
		return std::min(zone, numStages - 1);
	}
	
	void applyRadialSpatialReflectionZones(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		int numStages = radialStages.get();
		float maxDist = getMaxDistance(cols, numRows);
		
		// Store original matrix for reference
		vector<vector<float>> originalMatrix = matrix;
		
		for (int row = 0; row < numRows; row++) {
			for (int col = 0; col < cols; col++) {
				float distance = getDistanceFromCenter(col, row, cols, numRows);
				int zone = getRadialZone(distance, maxDist, numStages);
				
				// Only affect odd-numbered zones (1, 3, 5...)
				if (zone % 2 == 1) {
					// Find corresponding position in zone 0 (center)
					float centerX = (cols - 1) / 2.0f;
					float centerY = (numRows - 1) / 2.0f;
					
					// Map to innermost ring (zone 0)
					float zoneWidth = maxDist / numStages;
					float targetDistance = zoneWidth * 0.5f; // Middle of first zone
					
					if (distance > 0) {
						float ratio = targetDistance / distance;
						int sourceCol = (int)(centerX + (col - centerX) * ratio + 0.5f);
						int sourceRow = (int)(centerY + (row - centerY) * ratio + 0.5f);
						
						// Clamp to valid bounds
						sourceCol = std::max(0, std::min(cols - 1, sourceCol));
						sourceRow = std::max(0, std::min(numRows - 1, sourceRow));
						
						// Calculate angular offset for this zone
						int affectedZoneIndex = (zone + 1) / 2;
						float angleOffset = radialOffset.get() * affectedZoneIndex;
						
						// Copy value and apply angular offset
						float originalValue = originalMatrix[sourceRow][sourceCol];
						matrix[row][col] = fmod(originalValue + angleOffset, 1.0f);
						if (matrix[row][col] < 0) matrix[row][col] += 1.0f;
						inversionMatrix[row][col] = 1;
					}
				}
			}
		}
	}
	
	void applyRadialValueInversionZones(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		int numStages = radialStages.get();
		float maxDist = getMaxDistance(cols, numRows);
		
		for (int row = 0; row < numRows; row++) {
			for (int col = 0; col < cols; col++) {
				float distance = getDistanceFromCenter(col, row, cols, numRows);
				int zone = getRadialZone(distance, maxDist, numStages);
				
				// Only affect odd-numbered zones (1, 3, 5...)
				if (zone % 2 == 1) {
					// Calculate angular offset for this zone
					int affectedZoneIndex = (zone + 1) / 2;
					float angleOffset = radialOffset.get() * affectedZoneIndex;
					
					// Apply value inversion then angular offset
					float invertedValue = 1.0f - matrix[row][col];
					matrix[row][col] = fmod(invertedValue + angleOffset, 1.0f);
					if (matrix[row][col] < 0) matrix[row][col] += 1.0f;
					inversionMatrix[row][col] = 1;
				}
			}
		}
	}
	
	void applyRadialOffsetOnlyZones(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		int numStages = radialStages.get();
		float maxDist = getMaxDistance(cols, numRows);
		
		for (int row = 0; row < numRows; row++) {
			for (int col = 0; col < cols; col++) {
				float distance = getDistanceFromCenter(col, row, cols, numRows);
				int zone = getRadialZone(distance, maxDist, numStages);
				
				// Calculate angular offset for this zone (incremental)
				float angleOffset = radialOffset.get() * (zone + 1);
				
				if (angleOffset != 0.0f) {
					// Apply angular offset
					float originalValue = matrix[row][col];
					matrix[row][col] = fmod(originalValue + angleOffset, 1.0f);
					if (matrix[row][col] < 0) matrix[row][col] += 1.0f;
					inversionMatrix[row][col] = 1;
				}
			}
		}
	}
	
	void matrixToVector(const vector<vector<float>>& matrix, vector<float>& result, int cols, int numRows) {
		result.clear();
		result.reserve(cols * numRows);
		
		for (int row = 0; row < numRows; row++) {
			for (int col = 0; col < cols; col++) {
				result.push_back(matrix[row][col]);
			}
		}
	}
	
	void inversionMatrixToVector(const vector<vector<int>>& inversionMatrix, vector<int>& result, int cols, int numRows) {
		result.clear();
		result.reserve(cols * numRows);
		
		for (int row = 0; row < numRows; row++) {
			for (int col = 0; col < cols; col++) {
				result.push_back(inversionMatrix[row][col]);
			}
		}
	}
	
	ofParameter<vector<float>> input;
	ofParameter<int> columns;
	ofParameter<int> rows;
	ofParameter<int> radialStages;
	ofParameter<float> radialOffset;
	ofParameter<bool> useValueInversion;
	ofParameter<bool> useInversions;
	ofParameter<vector<float>> output;
	ofParameter<vector<int>> inversions;
	
	ofEventListeners listeners;
};

#endif /* vectorMatrixRadialSymmetry_h */
