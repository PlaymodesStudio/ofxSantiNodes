#ifndef imageManager_h
#define imageManager_h

#include "ofxOceanodeNodeModel.h"
#include "ofxThreadedImageLoader.h"

class imageManager : public ofxOceanodeNodeModel {
public:
	imageManager() : ofxOceanodeNodeModel("Image Manager") {}
	
	void setup() override {
		description = "Loads images from any file path, displays them as texture output, and allows saving copies to disk.";
		
		loaded = false;
		needsTextureUpdate = false;
		
		// Parameters
		addParameter(imagePath.set("Path", ""));
		addParameter(openButton.set("Open"));
		addParameter(saveButton.set("Save"));
		addParameter(imageWidth.set("Width", 0, 0, INT_MAX));
		addParameter(imageHeight.set("Height", 0, 0, INT_MAX));
		addParameter(loadingStatus.set("Status", "Ready"));
		addOutputParameter(texture.set("Output", nullptr));
		
		// Listeners
		pathListener = imagePath.newListener([this](string &path) {
			loadImageFromPath(path);
		});
		
		openListener = openButton.newListener([this]() {
			openFileDialog();
		});
		
		saveListener = saveButton.newListener([this]() {
			saveImageDialog();
		});
	}
	
	void draw(ofEventArgs &a) override {
		// Check if we have new pixel data ready (but no texture yet)
		if (loadingImage.isAllocated() && loadingImage.getWidth() > 0 && loadingImage.getHeight() > 0) {
			// Only update if this is a new image (different from current)
			if (!loaded || currentImagePath != pendingImagePath) {
				// Copy just the pixel data, not the texture
				displayPixels = loadingImage.getPixels();
				needsTextureUpdate = true;
				currentImagePath = pendingImagePath;
				
				//ofLogNotice("imageManager") << "Pixels ready: " << currentImagePath;
				//ofLogNotice("imageManager") << "Dimensions: " << displayPixels.getWidth() << "x" << displayPixels.getHeight();
				
				// Clear the loading image to free memory
				loadingImage.clear();
			}
		}
		
		// Create texture from pixels in draw() when we're definitely on main thread
		if (needsTextureUpdate && displayPixels.isAllocated()) {
			try {
				displayImage.clear(); // Clear any existing texture first
				displayImage.setFromPixels(displayPixels);
				
				if (displayImage.isAllocated()) {
					loaded = true;
					needsTextureUpdate = false;
					//ofLogNotice("imageManager") << "Texture created successfully: " << currentImagePath;
				} else {
					ofLogError("imageManager") << "Failed to create texture for: " << currentImagePath;
				}
			} catch (const std::exception& e) {
				ofLogError("imageManager") << "Exception creating texture: " << e.what();
				needsTextureUpdate = false;
			}
		}
		
		// Always use the stable displayImage for output
		if (loaded && displayImage.isAllocated() && displayImage.getWidth() > 0 && displayImage.getHeight() > 0) {
			texture = &displayImage.getTexture();
			imageWidth = displayImage.getWidth();
			imageHeight = displayImage.getHeight();
			loadingStatus = "Loaded";
		} else {
			texture = nullptr;
			imageWidth = 0;
			imageHeight = 0;
			if (needsTextureUpdate || (!pendingImagePath.empty() && !displayPixels.isAllocated())) {
				loadingStatus = "Loading...";
			} else {
				loadingStatus = "Ready";
			}
		}
	}
	
private:
	// Parameters
	ofParameter<string> imagePath;
	ofParameter<void> openButton;
	ofParameter<void> saveButton;
	ofParameter<int> imageWidth;
	ofParameter<int> imageHeight;
	ofParameter<string> loadingStatus;
	ofParameter<ofTexture*> texture;
	
	// Event listeners
	ofEventListener pathListener;
	ofEventListener openListener;
	ofEventListener saveListener;
	
	// Internal variables
	ofImage displayImage;     // Stable image for display (main thread only)
	ofImage loadingImage;     // Image being loaded by threaded loader
	ofPixels displayPixels;   // Pixel data ready for texture creation
	ofxThreadedImageLoader imageLoader;
	bool loaded;
	bool needsTextureUpdate;  // Flag to trigger texture creation in draw()
	string currentImagePath;  // Path of currently displayed image
	string pendingImagePath;  // Path of image being loaded
	
	void loadImageFromPath(const string& path) {
		if (path.empty()) {
			loaded = false;
			needsTextureUpdate = false;
			currentImagePath = "";
			pendingImagePath = "";
			displayImage.clear();
			displayPixels.clear();
			loadingImage.clear();
			return;
		}
		
		// Check if file exists
		ofFile file(path);
		if (!file.exists()) {
			ofLogError("imageManager") << "File does not exist: " << path;
			loaded = false;
			needsTextureUpdate = false;
			currentImagePath = "";
			pendingImagePath = "";
			return;
		}
		
		// Set pending path
		pendingImagePath = path;
		
		// Clear loading image and start threaded loading
		loadingImage.clear();
		imageLoader.loadFromDisk(loadingImage, path);
		
		//ofLogNotice("imageManager") << "Started loading: " << path;
	}
	
	void openFileDialog() {
		// Open file dialog for image selection
		ofFileDialogResult result = ofSystemLoadDialog("Select an image file", false, ofToDataPath("", true));
		
		if (result.bSuccess) {
			string selectedPath = result.getPath();
			imagePath = selectedPath;
		}
	}
	
	void saveImageDialog() {
		if (!loaded || currentImagePath.empty()) {
			ofLogWarning("imageManager") << "No image loaded to save";
			return;
		}
		
		// Get the original filename for default save name
		ofFile originalFile(currentImagePath);
		string originalName = originalFile.getFileName();
		
		// Open save dialog
		ofFileDialogResult result = ofSystemSaveDialog(originalName, "Save image copy");
		
		if (result.bSuccess) {
			string savePath = result.getPath();
			
			// Copy the original file to the new location
			ofFile sourceFile(currentImagePath);
			if (sourceFile.exists()) {
				bool copySuccess = sourceFile.copyTo(savePath, false, true);
				if (copySuccess) {
					//ofLogNotice("imageManager") << "Successfully saved image copy to: " << savePath;
				} else {
					ofLogError("imageManager") << "Failed to save image copy to: " << savePath;
				}
			} else {
				ofLogError("imageManager") << "Original file no longer exists: " << currentImagePath;
			}
		}
	}
};

#endif /* imageManager_h */
