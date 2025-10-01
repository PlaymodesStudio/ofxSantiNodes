class generativeGrid : public ofxOceanodeNodeModel{
public:
	generativeGrid() : ofxOceanodeNodeModel("Generative Grid"){
		description = "Generates vector graphics in a grid layout with various shape types per cell. CellShape values: 0=empty, 1=left line, 2=left+right lines, 3=top line, 4=top+bottom lines, 5=diagonal TL-BR, 6=diagonal TR-BL, 7=diagonal cross, 8-11=triangles, 12=rectangle, 13=ellipse. ShapeSizeX/Y control independent horizontal/vertical scaling.";
	};
	~generativeGrid(){};
	
	void setup(){
		addParameter(cellsX.set("CellsX", 4, 1, 100));
		addParameter(cellsY.set("CellsY", 4, 1, 100));
		addParameter(cellShape.set("CellShape", {0}, {0}, {13}));
		addParameter(shapeSizeX.set("ShapeSizeX", {1}, {1}, {10}));
		addParameter(shapeSizeY.set("ShapeSizeY", {1}, {1}, {10}));
		addParameter(offsetX.set("OffsetX", {0}, {-1}, {1}));
		addParameter(offsetY.set("OffsetY", {0}, {-1}, {1}));
		addParameter(xOut.set("X.Out", {0.5}, {0}, {1}));
		addParameter(yOut.set("Y.Out", {0.5}, {0}, {1}));
		
		// Listen to parameter changes
		listeners.push(cellsX.newListener([this](int &i){ calculate(); }));
		listeners.push(cellsY.newListener([this](int &i){ calculate(); }));
		listeners.push(cellShape.newListener([this](vector<int> &v){ calculate(); }));
		listeners.push(shapeSizeX.newListener([this](vector<int> &v){ calculate(); }));
		listeners.push(shapeSizeY.newListener([this](vector<int> &v){ calculate(); }));
		listeners.push(offsetX.newListener([this](vector<float> &v){ calculate(); }));
		listeners.push(offsetY.newListener([this](vector<float> &v){ calculate(); }));
	}
	
	void calculate(){
		vector<float> x_temp;
		vector<float> y_temp;
		
		int totalCells = cellsX * cellsY;
		float cellWidth = 1.0f / cellsX;
		float cellHeight = 1.0f / cellsY;
		
		// Pre-calculate all fitting sizes and positions before generating any shapes
		vector<tuple<int, float, float, float, float>> shapeData; // shape, left, top, right, bottom
		shapeData.reserve(totalCells);
		
		for(int row = 0; row < cellsY; row++){
			for(int col = 0; col < cellsX; col++){
				int cellIndex = row * cellsX + col;
				
				// Get parameters for this cell
				int shape = getParameterValueForCell(cellShape, cellIndex, totalCells);
				int requestedSizeX = getParameterValueForCell(shapeSizeX, cellIndex, totalCells);
				int requestedSizeY = getParameterValueForCell(shapeSizeY, cellIndex, totalCells);
				float offX = getParameterValueForCell(offsetX, col, cellsX);
				float offY = getParameterValueForCell(offsetY, row, cellsY);
				
				// Calculate base cell bounds with offset
				float baseCellLeft = col * cellWidth + offX * cellWidth;
				float baseCellTop = row * cellHeight + offY * cellHeight;
				
				// Find the maximum size that fits in the grid and determine anchor
				auto [finalSizeX, finalSizeY, anchorX, anchorY] = calculateFittingSize(
					col, row, requestedSizeX, requestedSizeY, cellWidth, cellHeight);
				
				// Only store shape data if it actually fits (size > 0)
				if(finalSizeX > 0 && finalSizeY > 0) {
					// Calculate final cell bounds based on anchor point
					float cellLeft, cellTop, cellRight, cellBottom;
					calculateCellBounds(baseCellLeft, baseCellTop, cellWidth, cellHeight,
									  finalSizeX, finalSizeY, anchorX, anchorY,
									  cellLeft, cellTop, cellRight, cellBottom);
					
					shapeData.emplace_back(shape, cellLeft, cellTop, cellRight, cellBottom);
				}
			}
		}
		
		// Now generate all shapes at once after all calculations are complete
		for(const auto& [shape, cellLeft, cellTop, cellRight, cellBottom] : shapeData) {
			generateShape(shape, cellLeft, cellTop, cellRight, cellBottom, x_temp, y_temp);
		}
		
		// Only update output after all shapes are generated
		xOut = x_temp;
		yOut = y_temp;
	}
	
private:
	ofParameter<int> cellsX, cellsY;
	ofParameter<vector<int>> cellShape, shapeSizeX, shapeSizeY;
	ofParameter<vector<float>> offsetX, offsetY;
	ofParameter<vector<float>> xOut, yOut;
	ofEventListeners listeners;
	
	// Calculate the largest size that fits and determine best anchor point
	std::tuple<int, int, int, int> calculateFittingSize(int col, int row, int requestedSizeX, int requestedSizeY,
														 float cellWidth, float cellHeight) {
		// Try different anchor points: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right
		for(int anchor = 0; anchor < 4; anchor++) {
			int testSizeX = requestedSizeX;
			int testSizeY = requestedSizeY;
			
			// Recursively reduce size until it fits
			while(testSizeX > 0 && testSizeY > 0) {
				if(doesSizeFitWithAnchor(col, row, testSizeX, testSizeY, anchor)) {
					return std::make_tuple(testSizeX, testSizeY, anchor % 2, anchor / 2); // anchorX, anchorY
				}
				
				// Reduce both dimensions equally
				testSizeX--;
				testSizeY--;
			}
		}
		
		// If no size fits with any anchor, return 0,0 (no shape)
		return std::make_tuple(0, 0, 0, 0);
	}
	
	// Check if a size fits with a given anchor point
	bool doesSizeFitWithAnchor(int col, int row, int sizeX, int sizeY, int anchor) {
		int anchorX = anchor % 2;  // 0=left, 1=right
		int anchorY = anchor / 2;  // 0=top, 1=bottom
		
		int startCol, endCol, startRow, endRow;
		
		if(anchorX == 0) { // Left anchor
			startCol = col;
			endCol = col + sizeX - 1;
		} else { // Right anchor
			endCol = col;
			startCol = col - sizeX + 1;
		}
		
		if(anchorY == 0) { // Top anchor
			startRow = row;
			endRow = row + sizeY - 1;
		} else { // Bottom anchor
			endRow = row;
			startRow = row - sizeY + 1;
		}
		
		// Check if all cells are within grid bounds
		return startCol >= 0 && endCol < cellsX && startRow >= 0 && endRow < cellsY;
	}
	
	// Calculate final cell bounds based on anchor point
	void calculateCellBounds(float baseCellLeft, float baseCellTop, float cellWidth, float cellHeight,
						   int sizeX, int sizeY, int anchorX, int anchorY,
						   float& cellLeft, float& cellTop, float& cellRight, float& cellBottom) {
		float scaledWidth = cellWidth * sizeX;
		float scaledHeight = cellHeight * sizeY;
		
		if(anchorX == 0) { // Left anchor
			cellLeft = baseCellLeft;
			cellRight = baseCellLeft + scaledWidth;
		} else { // Right anchor
			cellRight = baseCellLeft + cellWidth;
			cellLeft = cellRight - scaledWidth;
		}
		
		if(anchorY == 0) { // Top anchor
			cellTop = baseCellTop;
			cellBottom = baseCellTop + scaledHeight;
		} else { // Bottom anchor
			cellBottom = baseCellTop + cellHeight;
			cellTop = cellBottom - scaledHeight;
		}
	}
	
	void generateShape(int shapeType, float left, float top, float right, float bottom, vector<float>& x_temp, vector<float>& y_temp){
		float centerX = (left + right) * 0.5f;
		float centerY = (top + bottom) * 0.5f;
		
		// Skip empty shapes entirely
		if(shapeType == 0) return;
		
		// Add separator before each shape (except first)
		if(!x_temp.empty()){
			x_temp.push_back(-1);
			y_temp.push_back(-1);
		}
		
		switch(shapeType){
			case 0: // empty (should never reach here)
				break;
				
			case 1: // horizontal line aligned to left
				x_temp.push_back(left);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(top);
				break;
				
			case 2: // 2 horizontal lines at left and right
				x_temp.push_back(left);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(top);
				x_temp.push_back(-1);
				y_temp.push_back(-1);
				x_temp.push_back(left);
				y_temp.push_back(bottom);
				x_temp.push_back(right);
				y_temp.push_back(bottom);
				break;
				
			case 3: // vertical line aligned to top
				x_temp.push_back(left);
				y_temp.push_back(top);
				x_temp.push_back(left);
				y_temp.push_back(bottom);
				break;
				
			case 4: // 2 vertical lines at top and bottom
				x_temp.push_back(left);
				y_temp.push_back(top);
				x_temp.push_back(left);
				y_temp.push_back(bottom);
				x_temp.push_back(-1);
				y_temp.push_back(-1);
				x_temp.push_back(right);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(bottom);
				break;
				
			case 5: // diagonal from top left to bottom right
				x_temp.push_back(left);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(bottom);
				break;
				
			case 6: // diagonal from top right to bottom left
				x_temp.push_back(right);
				y_temp.push_back(top);
				x_temp.push_back(left);
				y_temp.push_back(bottom);
				break;
				
			case 7: // diagonal cross
				x_temp.push_back(left);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(bottom);
				x_temp.push_back(-1);
				y_temp.push_back(-1);
				x_temp.push_back(right);
				y_temp.push_back(top);
				x_temp.push_back(left);
				y_temp.push_back(bottom);
				break;
				
			case 8: // triangle: top left, bottom right, bottom left
				x_temp.push_back(left);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(bottom);
				x_temp.push_back(left);
				y_temp.push_back(bottom);
				x_temp.push_back(left);
				y_temp.push_back(top);
				break;
				
			case 9: // triangle: top left, top right, bottom left
				x_temp.push_back(left);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(top);
				x_temp.push_back(left);
				y_temp.push_back(bottom);
				x_temp.push_back(left);
				y_temp.push_back(top);
				break;
				
			case 10: // triangle: top left, top right, bottom right
				x_temp.push_back(left);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(bottom);
				x_temp.push_back(left);
				y_temp.push_back(top);
				break;
				
			case 11: // triangle: top right, bottom right, bottom left
				x_temp.push_back(right);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(bottom);
				x_temp.push_back(left);
				y_temp.push_back(bottom);
				x_temp.push_back(right);
				y_temp.push_back(top);
				break;
				
			case 12: // rectangle
				x_temp.push_back(left);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(top);
				x_temp.push_back(right);
				y_temp.push_back(bottom);
				x_temp.push_back(left);
				y_temp.push_back(bottom);
				x_temp.push_back(left);
				y_temp.push_back(top);
				break;
				
			case 13: // ellipse (approximated with 16 points)
				{
					int segments = 16;
					float radiusX = (right - left) * 0.5f;
					float radiusY = (bottom - top) * 0.5f;
					
					for(int i = 0; i <= segments; i++){
						float angle = (i * 2.0f * M_PI) / segments;
						float x = centerX + radiusX * cos(angle);
						float y = centerY + radiusY * sin(angle);
						x_temp.push_back(x);
						y_temp.push_back(y);
					}
				}
				break;
		}
	}
	
	template<typename T>
	T getParameterValueForCell(const ofParameter<vector<T>>& param, int index, int maxSize){
		const auto& vec = param.get();
		if(vec.empty()) return T{};
		if(vec.size() == 1) return vec[0];
		if(index < vec.size()) return vec[index];
		return vec[0]; // fallback
	}
};
