#pragma once

#include "bogaudio.hpp"

extern Model* modelSwitch;

namespace bogaudio {

struct Switch : Module {
	enum ParamsIds {
		GATE_PARAM,
		LATCH_PARAM,
		NUM_PARAMS
	};

	enum InputsIds {
		GATE_INPUT,
		HIGH1_INPUT,
		LOW1_INPUT,
		HIGH2_INPUT,
		LOW2_INPUT,
		NUM_INPUTS
	};

	enum OutputsIds {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightsIds {
		LATCH_LIGHT,
		NUM_LIGHTS
	};

	Trigger _trigger;
	bool _latchedHigh = false;

	Switch() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	void onReset() override;
	void step() override;
};

} // namespace bogaudio
