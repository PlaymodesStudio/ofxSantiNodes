#ifndef vectorMatrixSymmetry_h
#define vectorMatrixSymmetry_h

#include "ofxOceanodeNodeModel.h"

class vectorMatrixSymmetry : public ofxOceanodeNodeModel {
public:
	vectorMatrixSymmetry() : ofxOceanodeNodeModel("Vector Matrix Symmetry") {}

	void setup() override {
		description = "Reflects input vector values in X or Y based on matrix configuration. Two modes: Spatial (moves values) or Value Inversion (applies 1-x). Input is mapped row-by-row to a Columns x Rows matrix.";
		
		addParameter(input.set("Input", {0}, {-FLT_MAX}, {FLT_MAX}));
		addParameter(columns.set("Cols", 3, 1, 100));
		addParameter(rows.set("Rows", 3, 1, 100));
		addParameter(xReflections.set("X Stages", 0, 0, 64));
		addParameter(yReflections.set("Y Stages", 0, 0, 64));
		addParameter(xOffset.set("X Offset", 0, 0, 1));
		addParameter(yOffset.set("Y Offset", 0, 0, 1));
		addParameter(useValueInversion.set("Value Mode", true));
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
		
		listeners.push(xReflections.newListener([this](int &){
			process();
		}));
		
		listeners.push(yReflections.newListener([this](int &){
			process();
		}));
		
		listeners.push(xOffset.newListener([this](float &){
			process();
		}));
		
		listeners.push(yOffset.newListener([this](float &){
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
		int matrixSize = cols * numRows;
		
		// Create matrix from input vector (row-by-row)
		vector<vector<float>> matrix(numRows, vector<float>(cols));
		vector<vector<int>> inversionMatrix(numRows, vector<int>(cols, 0)); // Track inversions
		fillMatrixFromVector(inputVec, matrix, cols, numRows);
		
		// Apply reflections
		applyReflections(matrix, inversionMatrix, cols, numRows);
		
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
	
	void applyReflections(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		// Apply X reflections (creates horizontal zones)
		if (xReflections.get() > 0) {
			if (useInversions.get()) {
				if (useValueInversion.get()) {
					applyXValueInversionZones(matrix, inversionMatrix, cols, numRows);
				} else {
					applyXSpatialReflectionZones(matrix, inversionMatrix, cols, numRows);
				}
			} else {
				// Just apply offsets without any inversions
				applyXOffsetOnlyZones(matrix, inversionMatrix, cols, numRows);
			}
		}
		
		// Apply Y reflections (creates vertical zones)
		if (yReflections.get() > 0) {
			if (useInversions.get()) {
				if (useValueInversion.get()) {
					applyYValueInversionZones(matrix, inversionMatrix, cols, numRows);
				} else {
					applyYSpatialReflectionZones(matrix, inversionMatrix, cols, numRows);
				}
			} else {
				// Just apply offsets without any inversions
				applyYOffsetOnlyZones(matrix, inversionMatrix, cols, numRows);
			}
		}
	}
	
	void applyXSpatialReflectionZones(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		int numZones = xReflections.get(); // Direct mapping: 3 reflections = 3 zones
		if (numZones == 0) return;
		
		float zoneWidth = (float)cols / numZones;
		
		for (int zone = 0; zone < numZones; zone++) {
			// Only affect odd-numbered zones (1, 3, 5...)
			if (zone % 2 == 1) {
				int zoneStart = (int)(zone * zoneWidth);
				int zoneEnd = (int)((zone + 1) * zoneWidth);
				
				// Find the corresponding unaltered zone to copy from (zone 0)
				int sourceZoneStart = 0;
				int sourceZoneEnd = (int)zoneWidth;
				
				// Calculate angular offset for this zone (incremental)
				int affectedZoneIndex = (zone + 1) / 2; // 1st affected zone = 1, 2nd = 2, etc.
				float angleOffset = xOffset.get() * affectedZoneIndex;
				
				for (int row = 0; row < numRows; row++) {
					for (int col = zoneStart; col < zoneEnd && col < cols; col++) {
						// Map position within affected zone to source zone
						int relativePos = col - zoneStart;
						int sourceCol = sourceZoneStart + relativePos;
						if (sourceCol < sourceZoneEnd && sourceCol < cols) {
							// Copy value and apply angular offset with wrapping
							float originalValue = matrix[row][sourceCol];
							matrix[row][col] = fmod(originalValue + angleOffset, 1.0f);
							if (matrix[row][col] < 0) matrix[row][col] += 1.0f; // Handle negative wrap
							inversionMatrix[row][col] = 1; // Mark as affected
						}
					}
				}
			}
		}
	}
	
	void applyXValueInversionZones(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		int numZones = xReflections.get(); // Direct mapping: 3 reflections = 3 zones
		if (numZones == 0) return;
		
		float zoneWidth = (float)cols / numZones;
		
		for (int zone = 0; zone < numZones; zone++) {
			// Only affect odd-numbered zones (1, 3, 5...)
			if (zone % 2 == 1) {
				int zoneStart = (int)(zone * zoneWidth);
				int zoneEnd = (int)((zone + 1) * zoneWidth);
				
				// Calculate angular offset for this zone (incremental)
				int affectedZoneIndex = (zone + 1) / 2; // 1st affected zone = 1, 2nd = 2, etc.
				float angleOffset = xOffset.get() * affectedZoneIndex;
				
				for (int row = 0; row < numRows; row++) {
					for (int col = zoneStart; col < zoneEnd && col < cols; col++) {
						// Apply value inversion (1-x) then add angular offset with wrapping
						float invertedValue = 1.0f - matrix[row][col];
						matrix[row][col] = fmod(invertedValue + angleOffset, 1.0f);
						if (matrix[row][col] < 0) matrix[row][col] += 1.0f; // Handle negative wrap
						inversionMatrix[row][col] = 1; // Mark as affected
					}
				}
			}
		}
	}
	
	void applyXOffsetOnlyZones(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		int numZones = xReflections.get(); // Direct mapping: 3 reflections = 3 zones
		if (numZones == 0) return;
		
		float zoneWidth = (float)cols / numZones;
		
		for (int zone = 0; zone < numZones; zone++) {
			int zoneStart = (int)(zone * zoneWidth);
			int zoneEnd = (int)((zone + 1) * zoneWidth);
			
			// Calculate angular offset for this zone (incremental)
			float angleOffset = xOffset.get() * (zone + 1); // Each zone gets incremental offset
			
			if (angleOffset != 0.0f) { // Only process if there's an offset
				for (int row = 0; row < numRows; row++) {
					for (int col = zoneStart; col < zoneEnd && col < cols; col++) {
						// Apply angular offset with wrapping
						float originalValue = matrix[row][col];
						matrix[row][col] = fmod(originalValue + angleOffset, 1.0f);
						if (matrix[row][col] < 0) matrix[row][col] += 1.0f; // Handle negative wrap
						inversionMatrix[row][col] = 1; // Mark as affected
					}
				}
			}
		}
	}
	
	void applyYSpatialReflectionZones(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		int numZones = yReflections.get(); // Direct mapping: 3 reflections = 3 zones
		if (numZones == 0) return;
		
		float zoneHeight = (float)numRows / numZones;
		
		for (int zone = 0; zone < numZones; zone++) {
			// Only affect odd-numbered zones (1, 3, 5...)
			if (zone % 2 == 1) {
				int zoneStart = (int)(zone * zoneHeight);
				int zoneEnd = (int)((zone + 1) * zoneHeight);
				
				// Find the corresponding unaltered zone to copy from (zone 0)
				int sourceZoneStart = 0;
				int sourceZoneEnd = (int)zoneHeight;
				
				// Calculate angular offset for this zone (incremental)
				int affectedZoneIndex = (zone + 1) / 2; // 1st affected zone = 1, 2nd = 2, etc.
				float angleOffset = yOffset.get() * affectedZoneIndex;
				
				for (int row = zoneStart; row < zoneEnd && row < numRows; row++) {
					// Map position within affected zone to source zone
					int relativePos = row - zoneStart;
					int sourceRow = sourceZoneStart + relativePos;
					if (sourceRow < sourceZoneEnd && sourceRow < numRows) {
						for (int col = 0; col < cols; col++) {
							// Copy value and apply angular offset with wrapping
							float originalValue = matrix[sourceRow][col];
							matrix[row][col] = fmod(originalValue + angleOffset, 1.0f);
							if (matrix[row][col] < 0) matrix[row][col] += 1.0f; // Handle negative wrap
							inversionMatrix[row][col] = 1; // Mark as affected
						}
					}
				}
			}
		}
	}
	
	void applyYValueInversionZones(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		int numZones = yReflections.get(); // Direct mapping: 3 reflections = 3 zones
		if (numZones == 0) return;
		
		float zoneHeight = (float)numRows / numZones;
		
		for (int zone = 0; zone < numZones; zone++) {
			// Only affect odd-numbered zones (1, 3, 5...)
			if (zone % 2 == 1) {
				int zoneStart = (int)(zone * zoneHeight);
				int zoneEnd = (int)((zone + 1) * zoneHeight);
				
				// Calculate angular offset for this zone (incremental)
				int affectedZoneIndex = (zone + 1) / 2; // 1st affected zone = 1, 2nd = 2, etc.
				float angleOffset = yOffset.get() * affectedZoneIndex;
				
				for (int row = zoneStart; row < zoneEnd && row < numRows; row++) {
					for (int col = 0; col < cols; col++) {
						// Apply value inversion (1-x) then add angular offset with wrapping
						float invertedValue = 1.0f - matrix[row][col];
						matrix[row][col] = fmod(invertedValue + angleOffset, 1.0f);
						if (matrix[row][col] < 0) matrix[row][col] += 1.0f; // Handle negative wrap
						inversionMatrix[row][col] = 1; // Mark as affected
					}
				}
			}
		}
	}
	
	void applyYOffsetOnlyZones(vector<vector<float>>& matrix, vector<vector<int>>& inversionMatrix, int cols, int numRows) {
		int numZones = yReflections.get(); // Direct mapping: 3 reflections = 3 zones
		if (numZones == 0) return;
		
		float zoneHeight = (float)numRows / numZones;
		
		for (int zone = 0; zone < numZones; zone++) {
			int zoneStart = (int)(zone * zoneHeight);
			int zoneEnd = (int)((zone + 1) * zoneHeight);
			
			// Calculate angular offset for this zone (incremental)
			float angleOffset = yOffset.get() * (zone + 1); // Each zone gets incremental offset
			
			if (angleOffset != 0.0f) { // Only process if there's an offset
				for (int row = zoneStart; row < zoneEnd && row < numRows; row++) {
					for (int col = 0; col < cols; col++) {
						// Apply angular offset with wrapping
						float originalValue = matrix[row][col];
						matrix[row][col] = fmod(originalValue + angleOffset, 1.0f);
						if (matrix[row][col] < 0) matrix[row][col] += 1.0f; // Handle negative wrap
						inversionMatrix[row][col] = 1; // Mark as affected
					}
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
	ofParameter<int> xReflections;
	ofParameter<int> yReflections;
	ofParameter<float> xOffset;
	ofParameter<float> yOffset;
	ofParameter<bool> useValueInversion;
	ofParameter<bool> useInversions;
	ofParameter<vector<float>> output;
	ofParameter<vector<int>> inversions;
	
	ofEventListeners listeners;
};

#endif /* vectorMatrixSymmetry_h */
