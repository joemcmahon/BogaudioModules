
#include <algorithm>

#include "dsp/dsp.hpp"
#include "BogaudioModules.hpp"

using namespace bogaudio::dsp;

struct ChannelAnalyzer : SpectrumAnalyzer {
	const int _averageN;
	const int _binsN;
	float* _bins;
	float* _frames;
	int _currentFrame;

	ChannelAnalyzer(
		SpectrumAnalyzer::Size size,
		SpectrumAnalyzer::Overlap overlap,
		SpectrumAnalyzer::WindowType windowType,
		float sampleRate,
		int averageN,
		int binSize
	)
	: SpectrumAnalyzer(size, overlap, windowType, sampleRate)
	, _averageN(averageN)
	, _binsN(size / binSize)
	, _bins(new float[size] {})
	, _frames(new float[_averageN * _binsN] {})
	, _currentFrame(0)
	{
	}
	virtual ~ChannelAnalyzer() {
		delete[] _bins;
		delete[] _frames;
	}

	virtual bool step(float sample) override;
	float getPeak();
};

bool ChannelAnalyzer::step(float sample) {
	if (SpectrumAnalyzer::step(sample)) {
		float* frame = _frames + _currentFrame*_binsN;
		getMagnitudes(frame, _binsN);

		for (int bin = 0; bin < _binsN; ++bin) {
			_bins[bin] = 0.0;
			for (int i = 0; i < _averageN; ++i) {
				_bins[bin] += _frames[i*_binsN + bin];
			}
			_bins[bin] /= (float)_averageN;
			// FIXME: still unclear if should or should not take the root here: _bins[bin] = sqrtf(_bins[bin]);
		}
		_currentFrame = (_currentFrame + 1) % _averageN;
		return true;
	}
	return false;
}

float ChannelAnalyzer::getPeak() {
	float max = 0.0;
	float sum = 0.0;
	int maxBin = 0;
	for (int bin = 0; bin < _binsN; ++bin) {
		if( _bins[bin] > max) {
			max = _bins[bin];
			maxBin = bin;
		}
		sum += _bins[bin];
	}
	const float fWidth = _sampleRate / (float)(_size / (_size / _binsN));
	return (maxBin + 1)*fWidth - fWidth/2.0;
}


struct Analyzer : Module {
	enum ParamsIds {
		RANGE_PARAM,
		SMOOTH_PARAM,
		QUALITY_PARAM,
		POWER_PARAM,
		NUM_PARAMS
	};

	enum InputsIds {
		SIGNALA_INPUT,
		SIGNALB_INPUT,
		SIGNALC_INPUT,
		SIGNALD_INPUT,
		NUM_INPUTS
	};

	enum OutputsIds {
		SIGNALA_OUTPUT,
		SIGNALB_OUTPUT,
		SIGNALC_OUTPUT,
		SIGNALD_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightsIds {
		QUALITY_HIGH_LIGHT,
		QUALITY_GOOD_LIGHT,
		POWER_ON_LIGHT,
		NUM_LIGHTS
	};

	enum Quality {
		QUALITY_HIGH,
		QUALITY_GOOD
	};

	int _averageN;
	ChannelAnalyzer* _channelA = NULL;
	ChannelAnalyzer* _channelB = NULL;
	ChannelAnalyzer* _channelC = NULL;
	ChannelAnalyzer* _channelD = NULL;
	float _range = 0.0;
	float _smooth = 0.0;
	Quality _quality = QUALITY_GOOD;
	const int _binAverageN = 2;

	Analyzer() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		reset();
	}
	virtual ~Analyzer() {
		reset();
	}

	virtual void reset() override;
	void resetChannels();
	SpectrumAnalyzer::Size size();
	virtual void step() override;
	void stepChannel(ChannelAnalyzer*& channelPointer, bool running, Input& input, Output& output);
};

void Analyzer::reset() {
	resetChannels();
}

void Analyzer::resetChannels() {
	if (_channelA) {
		delete _channelA;
		_channelA = NULL;
	}
	if (_channelB) {
		delete _channelB;
		_channelB = NULL;
	}
	if (_channelC) {
		delete _channelC;
		_channelC = NULL;
	}
	if (_channelD) {
		delete _channelD;
		_channelD = NULL;
	}
}

SpectrumAnalyzer::Size Analyzer::size() {
	switch (_quality) {
		case QUALITY_HIGH: {
			return SpectrumAnalyzer::SIZE_4096;
		}
		case QUALITY_GOOD: {
			return SpectrumAnalyzer::SIZE_1024;
		}
	}
}

void Analyzer::step() {
	_range = clampf(params[RANGE_PARAM].value, 0.1, 1.0);

	int smooth = (int)roundf(clampf(params[SMOOTH_PARAM].value, 1.0, 10.0));
	if (_averageN != smooth) {
		_averageN = smooth;
		resetChannels();
	}

	Quality quality = ((int)params[QUALITY_PARAM].value) == 2 ? QUALITY_HIGH : QUALITY_GOOD;
	if (_quality != quality) {
		_quality = quality;
		resetChannels();
	}

	bool running = params[POWER_PARAM].value == 1.0;
	stepChannel(_channelA, running, inputs[SIGNALA_INPUT], outputs[SIGNALA_OUTPUT]);
	stepChannel(_channelB, running, inputs[SIGNALB_INPUT], outputs[SIGNALB_OUTPUT]);
	stepChannel(_channelC, running, inputs[SIGNALC_INPUT], outputs[SIGNALC_OUTPUT]);
	stepChannel(_channelD, running, inputs[SIGNALD_INPUT], outputs[SIGNALD_OUTPUT]);

	lights[QUALITY_HIGH_LIGHT].value = running && quality == QUALITY_HIGH;
	lights[QUALITY_GOOD_LIGHT].value = running && quality == QUALITY_GOOD;
	lights[POWER_ON_LIGHT].value = running;
}

void Analyzer::stepChannel(ChannelAnalyzer*& channelPointer, bool running, Input& input, Output& output) {
	if (running && input.active) {
		if (!channelPointer) {
			channelPointer = new ChannelAnalyzer(
				size(),
				SpectrumAnalyzer::OVERLAP_2,
				SpectrumAnalyzer::WINDOW_HAMMING,
				engineGetSampleRate(),
				_averageN,
				_binAverageN
			);
		}

		channelPointer->step(input.value);
		output.value = input.value;
	}
	else {
		if (channelPointer) {
			delete channelPointer;
			channelPointer = NULL;
		}

		output.value = 0.0;
	}
}


struct AnalyzerDisplay : TransparentWidget {
	const int _insetAround = 2;
	const int _insetLeft = _insetAround + 12;
	const int _insetRight = _insetAround + 2;
	const int _insetTop = _insetAround + 13;
	const int _insetBottom = _insetAround + 9;

	const float _refDB0 = 20.0; // arbitrary; makes a 10.0-amplitude sine wave reach to about 0db on the output.
	const float _displayDB = 120.0;

	const float xAxisLogFactor = 1 / 3.321; // magic number.

	const NVGcolor _axisColor = nvgRGBA(0xff, 0xff, 0xff, 0x70);
	const NVGcolor _textColor = nvgRGBA(0xff, 0xff, 0xff, 0xc0);
	const NVGcolor _channelAColor = nvgRGBA(0x00, 0xff, 0x00, 0xd0);
	const NVGcolor _channelBColor = nvgRGBA(0xff, 0x00, 0x00, 0xd0);
	const NVGcolor _channelCColor = nvgRGBA(0x00, 0x00, 0xff, 0xd0);
	const NVGcolor _channelDColor = nvgRGBA(0xff, 0x00, 0xff, 0xd0);

	Analyzer* _module;
	const Vec _size;
	const Vec _graphSize;
	std::shared_ptr<Font> _font;

	AnalyzerDisplay(
		Analyzer* module,
		Vec size
	)
	: _module(module)
	, _size(size)
	, _graphSize(_size.x - _insetLeft - _insetRight, _size.y - _insetTop - _insetBottom)
	, _font(Font::load(assetPlugin(plugin, "res/fonts/inconsolata.ttf")))
	{
	}

	void draw(NVGcontext* vg) override;
	void drawBackground(NVGcontext* vg);
	void drawHeader(NVGcontext* vg);
	void drawYAxis(NVGcontext* vg);
	void drawXAxis(NVGcontext* vg);
	void drawXAxisLine(NVGcontext* vg, float hz, float maxHz);
	void drawGraph(NVGcontext* vg, float* bins, int binsN, NVGcolor color);
	void drawText(NVGcontext* vg, const char* s, float x, float y, float rotation = 0.0, const NVGcolor* color = NULL);
	int binValueToHeight(float value);
};

void AnalyzerDisplay::draw(NVGcontext* vg) {
	drawBackground(vg);

	if (_module->_channelA || _module->_channelB || _module->_channelC || _module->_channelD) {
		nvgSave(vg);
		nvgScissor(vg, _insetAround, _insetAround, _size.x - _insetAround, _size.y - _insetAround);
		{
			drawHeader(vg);
			drawYAxis(vg);
			drawXAxis(vg);

			if (_module->_channelA) {
				drawGraph(vg, _module->_channelA->_bins, _module->_channelA->_binsN, _channelAColor);
			}
			if (_module->_channelB) {
				drawGraph(vg, _module->_channelB->_bins, _module->_channelB->_binsN, _channelBColor);
			}
			if (_module->_channelC) {
				drawGraph(vg, _module->_channelC->_bins, _module->_channelC->_binsN, _channelCColor);
			}
			if (_module->_channelD) {
				drawGraph(vg, _module->_channelD->_bins, _module->_channelD->_binsN, _channelDColor);
			}
		}
		nvgRestore(vg);
	}
}

void AnalyzerDisplay::drawBackground(NVGcontext* vg) {
	nvgSave(vg);
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, _size.x, _size.y);
	nvgFillColor(vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBA(0xc0, 0xc0, 0xc0, 0xff));
	nvgStroke(vg);
	nvgRestore(vg);
}

void AnalyzerDisplay::drawHeader(NVGcontext* vg) {
	nvgSave(vg);

	const int textY = -4;
	const int charPx = 5;
	const int sLen = 100;
	char s[sLen];
	int x = _insetAround + 2;

	int n = snprintf(s, sLen, "Peaks (+/-%0.1f):", engineGetSampleRate() / (float)(_module->size() / (2 * _module->_binAverageN)));
	drawText(vg, s, x, _insetTop + textY);
	x += n * charPx - 2;

	if (_module->_channelA) {
		snprintf(s, sLen, "A:%7.1f", _module->_channelA->getPeak());
		drawText(vg, s, x, _insetTop + textY, 0.0, &_channelAColor);
	}
	x += 9 * charPx + 3;

	if (_module->_channelB) {
		snprintf(s, sLen, "B:%7.1f", _module->_channelB->getPeak());
		drawText(vg, s, x, _insetTop + textY, 0.0, &_channelBColor);
	}
	x += 9 * charPx + 3;

	if (_module->_channelC) {
		snprintf(s, sLen, "C:%7.1f", _module->_channelC->getPeak());
		drawText(vg, s, x, _insetTop + textY, 0.0, &_channelCColor);
	}
	x += 9 * charPx + 3;

	if (_module->_channelD) {
		snprintf(s, sLen, "D:%7.1f", _module->_channelD->getPeak());
		drawText(vg, s, x, _insetTop + textY, 0.0, &_channelDColor);
	}

	nvgRestore(vg);
}

void AnalyzerDisplay::drawYAxis(NVGcontext* vg) {
	nvgSave(vg);
	nvgStrokeColor(vg, _axisColor);
	const int lineX = _insetLeft - 2;
	const int textX = 9;
	const float textR = -M_PI/2.0;

	nvgBeginPath(vg);
	int lineY = _insetTop;
	nvgMoveTo(vg, lineX, lineY);
	nvgLineTo(vg, _size.x - _insetRight, lineY);
	nvgStroke(vg);

	nvgBeginPath(vg);
	lineY = _insetTop + (_graphSize.y - _graphSize.y*(_displayDB - 20.0)/_displayDB);
	nvgMoveTo(vg, lineX, lineY);
	nvgLineTo(vg, _size.x - _insetRight, lineY);
	nvgStroke(vg);

	drawText(vg, "0", textX, lineY + 2.3, textR);

	nvgBeginPath(vg);
	lineY = _insetTop + (_graphSize.y - _graphSize.y*(_displayDB - 70.0)/_displayDB);
	nvgMoveTo(vg, lineX, lineY);
	nvgLineTo(vg, _size.x - _insetRight, lineY);
	nvgStroke(vg);

	drawText(vg, "-50", textX, lineY + 10, textR);

	nvgBeginPath(vg);
	lineY = _insetTop + _graphSize.y + 1;
	nvgMoveTo(vg, lineX, lineY);
	nvgLineTo(vg, _size.x - _insetRight, lineY);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, lineX, _insetTop);
	nvgLineTo(vg, lineX, lineY);
	nvgStroke(vg);

	drawText(vg, "dB", textX, _size.y - _insetBottom, textR);

	nvgRestore(vg);
}

void AnalyzerDisplay::drawXAxis(NVGcontext* vg) {
	nvgSave(vg);
	nvgStrokeColor(vg, _axisColor);
	nvgStrokeWidth(vg, 0.8);

	const float maxHz = _module->_range * (engineGetSampleRate() / 2.0);
	float hz = 100.0;
	while (hz < maxHz && hz < 1001.0) {
		drawXAxisLine(vg, hz, maxHz);
		hz += 100.0;
	}
	hz = 2000.0;
	while (hz < maxHz && hz < 10001.0) {
		drawXAxisLine(vg, hz, maxHz);
		hz += 1000.0;
	}
	hz = 20000.0;
	while (hz < maxHz && hz < 100001.0) {
		drawXAxisLine(vg, hz, maxHz);
		hz += 10000.0;
	}

	drawText(vg, "Hz", _insetLeft, _size.y - 2);
	{
		float x = 100.0 / maxHz;
		x = powf(x, xAxisLogFactor);
		if (x < 1.0) {
			x *= _graphSize.x;
			drawText(vg, "100", _insetLeft + x - 8, _size.y - 2);
		}
	}
	{
		float x = 1000.0 / maxHz;
		x = powf(x, xAxisLogFactor);
		if (x < 1.0) {
			x *= _graphSize.x;
			drawText(vg, "1k", _insetLeft + x - 4, _size.y - 2);
		}
	}
	{
		float x = 10000.0 / maxHz;
		x = powf(x, xAxisLogFactor);
		if (x < 1.0) {
			x *= _graphSize.x;
			drawText(vg, "10k", _insetLeft + x - 7, _size.y - 2);
		}
	}

	nvgRestore(vg);
}

void AnalyzerDisplay::drawXAxisLine(NVGcontext* vg, float hz, float maxHz) {
	float x = hz / maxHz;
	x = powf(x, xAxisLogFactor);
	if (x < 1.0) {
		x *= _graphSize.x;
		nvgBeginPath(vg);
		nvgMoveTo(vg, _insetLeft + x, _insetTop);
		nvgLineTo(vg, _insetLeft + x, _insetTop + _graphSize.y);
		nvgStroke(vg);
	}
}

void AnalyzerDisplay::drawGraph(NVGcontext* vg, float* bins, int binsN, NVGcolor color) {
  const int pointsN = roundf(_module->_range*(_module->size()/2));
	nvgSave(vg);
	nvgScissor(vg, _insetLeft, _insetTop, _graphSize.x, _graphSize.y);
	nvgStrokeColor(vg, color);
	nvgBeginPath(vg);
	for (int i = 0; i < pointsN; ++i) {
		int height = binValueToHeight(bins[i]);
		if (i == 0) {
			nvgMoveTo(vg, _insetLeft, _insetTop + (_graphSize.y - height));
		}
		else {
			float x = _graphSize.x * powf(i / (float)pointsN, xAxisLogFactor);
			nvgLineTo(vg, _insetLeft + x, _insetTop + (_graphSize.y - height));
		}
	}
	nvgStroke(vg);
	nvgRestore(vg);
}

void AnalyzerDisplay::drawText(NVGcontext* vg, const char* s, float x, float y, float rotation, const NVGcolor* color) {
	nvgSave(vg);
	nvgTranslate(vg, x, y);
	nvgRotate(vg, rotation);
	nvgFontSize(vg, 10);
	nvgFontFaceId(vg, _font->handle);
	nvgFillColor(vg, color ? *color : _textColor);
	nvgText(vg, 0, 0, s, NULL);
	nvgRestore(vg);
}

int AnalyzerDisplay::binValueToHeight(float value) {
	value /= _refDB0;
	value = log10(value);
	value = std::max(-5.0f, value);
	value = std::min(1.0f, value);
	value *= 20.0;
	value = (value + _displayDB - 20.0) / _displayDB;
	return round(_graphSize.y * value);
}


struct OneTenKnob : Knob38 {
	OneTenKnob() : Knob38() {
		minAngle = -0.664*M_PI;
	}
};


struct IntegerOneTenKnob : OneTenKnob {
	IntegerOneTenKnob() : OneTenKnob() {
		snap = true;
	}
};


AnalyzerWidget::AnalyzerWidget() {
	auto module = new Analyzer();
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH * 20, RACK_GRID_HEIGHT);

	{
		auto panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/Analyzer.svg")));
		addChild(panel);
	}

	{
		auto inset = Vec(10, 25);
		auto size = Vec(box.size.x - 2*inset.x, 230);
		auto display = new AnalyzerDisplay(module, size);
		display->box.pos = inset;
		display->box.size = size;
		addChild(display);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 30, 365)));

	// generated by svg_widgets.rb
	auto rangeParamPosition = Vec(35.08, 271.08);
	auto smoothParamPosition = Vec(109.08, 271.08);
	auto qualityParamPosition = Vec(186.02, 298.02);
	auto powerParamPosition = Vec(259.02, 298.02);

	auto signalaInputPosition = Vec(13.5, 323.0);
	auto signalbInputPosition = Vec(86.5, 323.0);
	auto signalcInputPosition = Vec(160.5, 323.0);
	auto signaldInputPosition = Vec(233.5, 323.0);

	auto signalaOutputPosition = Vec(42.5, 323.0);
	auto signalbOutputPosition = Vec(115.5, 323.0);
	auto signalcOutputPosition = Vec(189.5, 323.0);
	auto signaldOutputPosition = Vec(262.5, 323.0);

	auto qualityHighLightPosition = Vec(179.0, 274.0);
	auto qualityGoodLightPosition = Vec(179.0, 289.0);
	auto powerOnLightPosition = Vec(252.0, 289.0);
	// end generated by svg_widgets.rb

	addParam(createParam<OneTenKnob>(rangeParamPosition, module, Analyzer::RANGE_PARAM, 0.1, 1.0, 0.5));
	addParam(createParam<IntegerOneTenKnob>(smoothParamPosition, module, Analyzer::SMOOTH_PARAM, 1.0, 10.0, 5.0));
	addParam(createParam<Button9Toggle2>(qualityParamPosition, module, Analyzer::QUALITY_PARAM, 1.0, 2.0, 1.0));
	addParam(createParam<Button9Toggle2>(powerParamPosition, module, Analyzer::POWER_PARAM, 0.0, 1.0, 1.0));

	addInput(createInput<PJ301MPort>(signalaInputPosition, module, Analyzer::SIGNALA_INPUT));
	addInput(createInput<PJ301MPort>(signalbInputPosition, module, Analyzer::SIGNALB_INPUT));
	addInput(createInput<PJ301MPort>(signalcInputPosition, module, Analyzer::SIGNALC_INPUT));
	addInput(createInput<PJ301MPort>(signaldInputPosition, module, Analyzer::SIGNALD_INPUT));

	addOutput(createOutput<PJ301MPort>(signalaOutputPosition, module, Analyzer::SIGNALA_OUTPUT));
	addOutput(createOutput<PJ301MPort>(signalbOutputPosition, module, Analyzer::SIGNALB_OUTPUT));
	addOutput(createOutput<PJ301MPort>(signalcOutputPosition, module, Analyzer::SIGNALC_OUTPUT));
	addOutput(createOutput<PJ301MPort>(signaldOutputPosition, module, Analyzer::SIGNALD_OUTPUT));

	addChild(createLight<TinyLight<GreenLight>>(qualityHighLightPosition, module, Analyzer::QUALITY_HIGH_LIGHT));
	addChild(createLight<TinyLight<GreenLight>>(qualityGoodLightPosition, module, Analyzer::QUALITY_GOOD_LIGHT));
	addChild(createLight<TinyLight<GreenLight>>(powerOnLightPosition, module, Analyzer::POWER_ON_LIGHT));
}