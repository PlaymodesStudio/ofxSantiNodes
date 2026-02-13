#include "equalLoudness.h"

void EqualLoudness::setup() {
	description = "Generates amplitudes for given pitches with linear amplitude matching equal loudness.";

	addParameter(pitch.set("Pitch", {0}, {-FLT_MAX}, {FLT_MAX}));
	addParameter(strength.set("Strength", 1.0, 0.0, 1.0));
	addOutputParameter(outputAmplitudes.set("Output", {0}, {0}, {1}));

	listeners.push_back(std::make_unique<ofEventListener>(pitch.newListener([this](vector<float>& vf) {
		calculate();
	})));
	
	listeners.push_back(std::make_unique<ofEventListener>(strength.newListener([this](float& f) {
		calculate();
	})));

	calculate();
}

void EqualLoudness::calculate() {
	vector<float> pitches = pitch.get();
	vector<float> amplitudes(pitches.size());
	float strengthValue = strength.get();

	auto midiToFreq = [](float midi) {
		return 440.0 * pow(2.0, (midi - 69.0) / 12.0);
	};

	auto equalLoudnessAmplitude = [](float frequency) {
		// Fitted equal loudness approximation function
		double a = 1.95136520;
		double b = -0.376892168;
		double c = 0.0200933686;
		double d = 3.85649121e-6;
		double e = -3.19833134e-10;
		return a + b * log(frequency) + c * pow(log(frequency), 2) + d * frequency + e * pow(frequency, 2);
	};

	for (size_t i = 0; i < pitches.size(); ++i) {
		float freq = midiToFreq(pitches[i]);
		float equalLoudnessAmp = equalLoudnessAmplitude(freq);
		
		// Interpolate between 1.0 (strength=0) and equalLoudnessAmp (strength=1)
		amplitudes[i] = 1.0f + strengthValue * (equalLoudnessAmp - 1.0f);
	}

	outputAmplitudes = amplitudes;
}
