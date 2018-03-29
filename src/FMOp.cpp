
#include "FMOp.hpp"
#include "dsp/pitch.hpp"

void FMOp::onReset() {
	_steps = modulationSteps;
	_envelope.reset();
	_gateTrigger.reset();
}

void FMOp::onSampleRateChange() {
	_steps = modulationSteps;
	_envelope.setSampleRate(engineGetSampleRate());
	_phasor.setSampleRate(engineGetSampleRate());
	_sineTable.setSampleRate(engineGetSampleRate());
}

void FMOp::step() {
	if (!outputs[AUDIO_OUTPUT].active) {
		lights[ENV_TO_LEVEL_LIGHT].value = params[ENV_TO_LEVEL_PARAM].value > 0.5f;
		lights[ENV_TO_FEEDBACK_LIGHT].value = params[ENV_TO_FEEDBACK_PARAM].value > 0.5f;
		return;
	}
	lights[ENV_TO_LEVEL_LIGHT].value = _levelEnvelopeOn;
	lights[ENV_TO_FEEDBACK_LIGHT].value = _feedbackEnvelopeOn;

	float pitchIn = 0.0f;
	if (inputs[PITCH_INPUT].active) {
		pitchIn = inputs[PITCH_INPUT].value;
	}
	float gateIn = 0.0f;
	if (inputs[GATE_INPUT].active) {
		gateIn = inputs[GATE_INPUT].value;
	}

	++_steps;
	if (_steps >= modulationSteps) {
		_steps = 0;

		float ratio = params[RATIO_PARAM].value;
		if (ratio < 0.0f) {
			ratio = std::max(1.0f + ratio, 0.01f);
		}
		else {
			ratio *= 9.0f;
			ratio += 1.0f;
		}
		_baseHZ = pitchIn;
		_baseHZ += params[FINE_PARAM].value;
		_baseHZ = cvToFrequency(_baseHZ);
		_baseHZ *= ratio;

		bool levelEnvelopeOn = params[ENV_TO_LEVEL_PARAM].value > 0.5f;
		bool feedbackEnvelopeOn = params[ENV_TO_FEEDBACK_PARAM].value > 0.5f;
		if (_levelEnvelopeOn != levelEnvelopeOn || _feedbackEnvelopeOn != feedbackEnvelopeOn) {
			_levelEnvelopeOn = levelEnvelopeOn;
			_feedbackEnvelopeOn = feedbackEnvelopeOn;
			bool envelopeOn = _levelEnvelopeOn || _feedbackEnvelopeOn;
			if (envelopeOn && !_envelopeOn) {
				_envelope.reset();
			}
			_envelopeOn = envelopeOn;
		}
		if (_envelopeOn) {
			float sustain = params[SUSTAIN_PARAM].value;
			if (inputs[SUSTAIN_INPUT].active) {
				sustain *= clamp(inputs[SUSTAIN_INPUT].value / 10.0f, 0.0f, 1.0f);
			}
			_envelope.setAttack(params[ATTACK_PARAM].value);
			_envelope.setDecay(params[DECAY_PARAM].value);
			_envelope.setSustain(sustain);
			_envelope.setRelease(params[RELEASE_PARAM].value);
		}

		_feedback = params[FEEDBACK_PARAM].value;
		if (inputs[FEEDBACK_INPUT].active) {
			_feedback *= clamp(inputs[FEEDBACK_INPUT].value / 10.0f, 0.0f, 1.0f);
		}

		_depth = params[DEPTH_PARAM].value;
		if (inputs[DEPTH_INPUT].active) {
			_depth *= clamp(inputs[DEPTH_INPUT].value / 10.0f, 0.0f, 1.0f);
		}

		_level = params[LEVEL_PARAM].value;
		if (inputs[LEVEL_INPUT].active) {
			_level *= clamp(inputs[LEVEL_INPUT].value / 10.0f, 0.0f, 1.0f);
		}
	}

	float envelope = 0.0f;
	if (_envelopeOn) {
		_gateTrigger.process(gateIn);
		_envelope.setGate(_gateTrigger.isHigh());
		envelope = _envelope.next();
	}

	float feedback = _feedback;
	if (_feedbackEnvelopeOn) {
		feedback *= envelope;
	}

	_phasor.setFrequency(_baseHZ);
	float offset = feedback * _phasor.next();
	if (inputs[FM_INPUT].active) {
		offset += inputs[FM_INPUT].value * _depth * 2.0f;
	}
	float out = _sineTable.nextFromPhasor(_phasor, Phasor::radiansToPhase(offset));
	out *= _level;
	if (_levelEnvelopeOn) {
		out *= envelope;
	}
	outputs[AUDIO_OUTPUT].value = amplitude * out;
}

struct FMOpWidget : ModuleWidget {
	FMOpWidget(FMOp* module) : ModuleWidget(module) {
		box.size = Vec(RACK_GRID_WIDTH * 10, RACK_GRID_HEIGHT);

		{
			SVGPanel *panel = new SVGPanel();
			panel->box.size = box.size;
			panel->setBackground(SVG::load(assetPlugin(plugin, "res/FMOp.svg")));
			addChild(panel);
		}

		addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 365)));

		// generated by svg_widgets.rb
		auto ratioParamPosition = Vec(30.0, 45.0);
		auto fineParamPosition = Vec(112.0, 57.0);
		auto attackParamPosition = Vec(107.0, 90.0);
		auto decayParamPosition = Vec(107.0, 133.0);
		auto sustainParamPosition = Vec(107.0, 176.0);
		auto releaseParamPosition = Vec(107.0, 219.0);
		auto depthParamPosition = Vec(36.0, 113.0);
		auto feedbackParamPosition = Vec(36.0, 162.0);
		auto levelParamPosition = Vec(36.0, 211.0);
		auto envToLevelParamPosition = Vec(78.0, 255.3);
		auto envToFeedbackParamPosition = Vec(128.0, 255.3);

		auto depthInputPosition = Vec(15.0, 274.0);
		auto feedbackInputPosition = Vec(47.0, 274.0);
		auto levelInputPosition = Vec(79.0, 274.0);
		auto sustainInputPosition = Vec(111.0, 274.0);
		auto pitchInputPosition = Vec(15.0, 318.0);
		auto gateInputPosition = Vec(47.0, 318.0);
		auto fmInputPosition = Vec(79.0, 318.0);

		auto audioOutputPosition = Vec(111.0, 318.0);

		auto envToLevelLightPosition = Vec(40.0, 256.5);
		auto envToFeedbackLightPosition = Vec(94.0, 256.5);
		// end generated by svg_widgets.rb

		addParam(ParamWidget::create<Knob38>(ratioParamPosition, module, FMOp::RATIO_PARAM, -1.0, 1.0, 0.0));
		addParam(ParamWidget::create<Knob16>(fineParamPosition, module, FMOp::FINE_PARAM, -1.0, 1.0, 0.0));
		addParam(ParamWidget::create<Knob26>(attackParamPosition, module, FMOp::ATTACK_PARAM, 0.0, 1.0, 0.1));
		addParam(ParamWidget::create<Knob26>(decayParamPosition, module, FMOp::DECAY_PARAM, 0.0, 1.0, 0.1));
		addParam(ParamWidget::create<Knob26>(sustainParamPosition, module, FMOp::SUSTAIN_PARAM, 0.0, 1.0, 1.0));
		addParam(ParamWidget::create<Knob26>(releaseParamPosition, module, FMOp::RELEASE_PARAM, 0.0, 1.0, 0.3));
		addParam(ParamWidget::create<Knob26>(depthParamPosition, module, FMOp::DEPTH_PARAM, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<Knob26>(feedbackParamPosition, module, FMOp::FEEDBACK_PARAM, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<Knob26>(levelParamPosition, module, FMOp::LEVEL_PARAM, 0.0, 1.0, 1.0));
		addParam(ParamWidget::create<StatefulButton9>(envToLevelParamPosition, module, FMOp::ENV_TO_LEVEL_PARAM, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<StatefulButton9>(envToFeedbackParamPosition, module, FMOp::ENV_TO_FEEDBACK_PARAM, 0.0, 1.0, 0.0));

		addInput(Port::create<Port24>(sustainInputPosition, Port::INPUT, module, FMOp::SUSTAIN_INPUT));
		addInput(Port::create<Port24>(depthInputPosition, Port::INPUT, module, FMOp::DEPTH_INPUT));
		addInput(Port::create<Port24>(feedbackInputPosition, Port::INPUT, module, FMOp::FEEDBACK_INPUT));
		addInput(Port::create<Port24>(levelInputPosition, Port::INPUT, module, FMOp::LEVEL_INPUT));
		addInput(Port::create<Port24>(pitchInputPosition, Port::INPUT, module, FMOp::PITCH_INPUT));
		addInput(Port::create<Port24>(gateInputPosition, Port::INPUT, module, FMOp::GATE_INPUT));
		addInput(Port::create<Port24>(fmInputPosition, Port::INPUT, module, FMOp::FM_INPUT));

		addOutput(Port::create<Port24>(audioOutputPosition, Port::OUTPUT, module, FMOp::AUDIO_OUTPUT));

		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(envToLevelLightPosition, module, FMOp::ENV_TO_LEVEL_LIGHT));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(envToFeedbackLightPosition, module, FMOp::ENV_TO_FEEDBACK_LIGHT));
	}
};

Model* modelFMOp = Model::create<FMOp, FMOpWidget>("Bogaudio", "Bogaudio-FMOp", "FM-OP", OSCILLATOR_TAG);
