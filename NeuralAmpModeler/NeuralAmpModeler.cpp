#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
#include <filesystem>
#include <iostream>
#include <utility>

#include "Colors.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
// clang-format on
#include "architecture.hpp"

#include "NeuralAmpModelerControls.h"

using namespace iplug;
using namespace igraphics;

const double kDCBlockerFrequency = 5.0;

// Styles
const IVColorSpec colorSpec{
  DEFAULT_BGCOLOR, // Background
  PluginColors::NAM_THEMECOLOR, // Foreground
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.3f), // Pressed
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.4f), // Frame
  PluginColors::MOUSEOVER, // Highlight
  DEFAULT_SHCOLOR, // Shadow
  PluginColors::NAM_THEMECOLOR, // Extra 1
  COLOR_RED, // Extra 2 --> color for clipping in meters
  PluginColors::NAM_THEMECOLOR.WithContrast(0.1f), // Extra 3
};

const IVStyle style =
  IVStyle{true, // Show label
          true, // Show value
          colorSpec,
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Middle, PluginColors::NAM_THEMEFONTCOLOR}, // Knob label text5
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, PluginColors::NAM_THEMEFONTCOLOR}, // Knob value text
          DEFAULT_HIDE_CURSOR,
          DEFAULT_DRAW_FRAME,
          false,
          DEFAULT_EMBOSS,
          0.2f,
          2.f,
          DEFAULT_SHADOW_OFFSET,
          DEFAULT_WIDGET_FRAC,
          DEFAULT_WIDGET_ANGLE};
const IVStyle titleStyle =
  DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular")).WithDrawFrame(false).WithShadowOffset(2.f);
const IVStyle radioButtonStyle =
  style
    .WithColor(EVColor::kON, PluginColors::NAM_THEMECOLOR) // Pressed buttons and their labels
    .WithColor(EVColor::kOFF, PluginColors::NAM_THEMECOLOR.WithOpacity(0.1f)) // Unpressed buttons
    .WithColor(EVColor::kX1, PluginColors::NAM_THEMECOLOR.WithOpacity(0.6f)); // Unpressed buttons' labels

EMsgBoxResult _ShowMessageBox(iplug::igraphics::IGraphics* pGraphics, const char* str, const char* caption,
                              EMsgBoxType type)
{
#ifdef OS_MAC
  // macOS is backwards?
  return pGraphics->ShowMessageBox(caption, str, type);
#else
  return pGraphics->ShowMessageBox(str, caption, type);
#endif
}

const std::string kCalibrateInputParamName = "CalibrateInput";
const bool kDefaultCalibrateInput = false;
const std::string kInputCalibrationLevelParamName = "InputCalibrationLevel";
const double kDefaultInputCalibrationLevel = 12.0;


NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  _InitToneStack();
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);
  GetParam(kOutputMode)->InitEnum("OutputMode", 1, {"Raw", "Normalized", "Calibrated"}); // TODO DRY w/ control
  GetParam(kIRToggle)->InitBool("IRToggle", true);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");
  GetParam(kSlim)->InitDouble("Slim", 1.0, 0.0, 1.0, 0.01);
  GetParam(kPreModelEnabled)->InitBool("PreModelEnabled", false);
  GetParam(kPreModelInputLevel)->InitGain("PreModelInput", 0.0, -20.0, 20.0, 0.1);
  GetParam(kPreModelOutputLevel)->InitGain("PreModelOutput", 0.0, -20.0, 20.0, 0.1);
  GetParam(kPreModelSlim)->InitDouble("PreModelSlim", 1.0, 0.0, 1.0, 0.01);

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);

  mMakeGraphicsFunc = [&]() {

#ifdef OS_IOS
    auto scaleFactor = GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT) * 0.85f;
#else
    auto scaleFactor = 1.0f;
#endif

    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scaleFactor);
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachTextEntryControl();
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Michroma-Regular", MICHROMA_FN);

    const auto gearSVG = pGraphics->LoadSVG(GEAR_FN);
    const auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    const auto globeSVG = pGraphics->LoadSVG(GLOBE_ICON_FN);
    const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    const auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    const auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const auto modelIconSVG = pGraphics->LoadSVG(MODEL_ICON_FN);
    const auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);
    const auto slimIconSVG = pGraphics->LoadSVG(SLIMMABLE_ICON_FN);
    const auto logoSVG = pGraphics->LoadSVG(MY_LOGO_FN);

    const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);
    const auto linesBitmap = pGraphics->LoadBitmap(LINES_FN);
    const auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    const auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);
    const auto meterBackgroundBitmap = pGraphics->LoadBitmap(METERBACKGROUND_FN);

    const auto b = pGraphics->GetBounds();
    const auto mainArea = b.GetPadded(-20);
    const auto contentArea = mainArea.GetPadded(-10);
    const auto titleHeight = 50.0f;
    const auto titleArea = contentArea.GetFromTop(titleHeight);

    // Areas for knobs
    const auto knobsPad = 20.0f;
    const auto knobsExtraSpaceBelowTitle = 25.0f;
    const auto singleKnobPad = -2.0f;
    const auto knobsArea = contentArea.GetFromTop(NAM_KNOB_HEIGHT)
                             .GetReducedFromLeft(knobsPad)
                             .GetReducedFromRight(knobsPad)
                             .GetVShifted(titleHeight + knobsExtraSpaceBelowTitle);
    const auto inputKnobArea = knobsArea.GetGridCell(0, kInputLevel, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto noiseGateArea = knobsArea.GetGridCell(0, kNoiseGateThreshold, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto bassKnobArea = knobsArea.GetGridCell(0, kToneBass, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto midKnobArea = knobsArea.GetGridCell(0, kToneMid, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto trebleKnobArea = knobsArea.GetGridCell(0, kToneTreble, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto outputKnobArea = knobsArea.GetGridCell(0, kOutputLevel, 1, numKnobs).GetPadded(-singleKnobPad);

    const auto ngToggleArea =
      noiseGateArea.GetVShifted(noiseGateArea.H() - 10.0f).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const auto eqToggleArea =
      midKnobArea.GetVShifted(midKnobArea.H() - 10.0f).SubRectVertical(2, 0).GetReducedFromTop(10.0f);

    // Areas for model and IR
    const auto fileWidth = 200.0f;
    const auto fileHeight = 30.0f;
    const auto irYOffset = 38.0f;
    const auto modelArea =
      contentArea.GetFromBottom((2.0f * fileHeight)).GetFromTop(fileHeight).GetMidHPadded(fileWidth).GetVShifted(-1);
    const auto slimIconArea =
      IRECT(modelArea.R + 6.f, modelArea.MH() - 14.f, modelArea.R + 6.f + 2.f * 28.f, modelArea.MH() + 14.f);
    const auto modelIconArea = modelArea.GetFromLeft(30).GetTranslated(-40, 10);
    const auto preModelArea = modelArea.GetVShifted(-30.0f);
    const auto addPreModelArea = preModelArea.GetCentredInside(72.0f, fileHeight);
    const auto preModelEnabledArea =
      IRECT(preModelArea.L - 66.0f, preModelArea.T, preModelArea.L - 6.0f, preModelArea.B);
    const auto preModelInputArea =
      IRECT(preModelArea.L - 116.0f, preModelArea.MH() - 15.0f, preModelArea.L - 72.0f, preModelArea.MH() + 15.0f);
    const auto preModelOutputArea =
      IRECT(preModelArea.R + 6.0f, preModelArea.MH() - 15.0f, preModelArea.R + 50.0f, preModelArea.MH() + 15.0f);
    const auto preModelQualityArea =
      IRECT(preModelArea.R + 56.0f, preModelArea.MH() - 15.0f, preModelArea.R + 108.0f, preModelArea.MH() + 15.0f);
    const auto removePreModelArea =
      IRECT(preModelArea.R + 114.0f, preModelArea.T, preModelArea.R + 142.0f, preModelArea.B);
    const auto irArea = modelArea.GetVShifted(irYOffset);
    const auto irSwitchArea = irArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);

    // Areas for meters
    const auto inputMeterArea = contentArea.GetFromLeft(30).GetHShifted(-20).GetMidVPadded(100).GetVShifted(-25);
    const auto outputMeterArea = contentArea.GetFromRight(30).GetHShifted(20).GetMidVPadded(100).GetVShifted(-25);

    // Misc Areas
    const auto settingsButtonArea = CornerButtonArea(b);

    // Model loader button
    auto loadModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        // Sets mAmpModelSlot.mPath and mStagedNAM
        const std::string msg = _StageModel(EModelSlot::Amp, fileName);
        // TODO error messages like the IR loader.
        if (msg.size())
        {
          std::stringstream ss;
          ss << "Failed to load NAM model. Message:\n\n" << msg;
          _ShowMessageBox(GetUI(), ss.str().c_str(), "Failed to load model!", kMB_OK);
        }
        std::cout << "Loaded: " << fileName.Get() << std::endl;
      }
    };

    auto loadPreModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        const std::string msg = _StageModel(EModelSlot::Pre, fileName);
        if (msg.empty())
          SetParameterValue(kPreModelEnabled, 1.0);
        else
        {
          std::stringstream ss;
          ss << "Failed to load pre model. Message:\n\n" << msg;
          _ShowMessageBox(GetUI(), ss.str().c_str(), "Failed to load pre model!", kMB_OK);
        }
      }
    };

    // IR loader button
    auto loadIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        mIRPath = fileName;
        const dsp::wav::LoadReturnCode retCode = _StageIR(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

    pGraphics->AttachBackground(BACKGROUND_FN);
    pGraphics->AttachControl(new IBitmapControl(b, linesBitmap));
    const auto logoArea = titleArea.GetCentredInside(44.0f, 44.0f);
pGraphics->AttachControl(new ISVGControl(logoArea, logoSVG));
    pGraphics->AttachControl(new ISVGControl(modelIconArea, modelIconSVG));

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultIRString = "Select IR directory...";
#else
    const std::string defaultNamFileString = "Select model...";
    const std::string defaultIRString = "Select IR...";
#endif
    // Getting started page listing additional resources
    const char* const getUrl = "https://www.neuralampmodeler.com/users#comp-marb84o5";
    pGraphics->AttachControl(
      new NAMFileBrowserControl(modelArea, kMsgTagClearModel, defaultNamFileString.c_str(), "nam",
                                loadModelCompletionHandler, style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG,
                                fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl, "NAM"),
      kCtrlTagModelFileBrowser);

    const auto compactStyle = style.WithShowValue(false)
                                .WithDrawFrame(false)
                                .WithLabelText(IText(9.0f, PluginColors::NAM_THEMEFONTCOLOR));
    auto* addPreModelButton = pGraphics->AttachControl(
      new IVButtonControl(addPreModelArea, SplashClickActionFunc, "+", style.WithShowValue(false)),
      kCtrlTagAddPreModel);
    addPreModelButton->SetTooltip("Add a model before the amp");
    addPreModelButton->SetAnimationEndActionFunction([](IControl* pCaller) {
      auto* ui = pCaller->GetUI();
      ui->ForControlInGroup("PRE_MODEL_CONTROLS", [](IControl* control) { control->Hide(false); });
      if (auto* quality = ui->GetControlWithTag(kCtrlTagPreModelSlim))
        quality->Hide(true);
      if (auto* input = ui->GetControlWithTag(kCtrlTagPreModelInputLevel))
        input->SetDisabled(true);
      if (auto* output = ui->GetControlWithTag(kCtrlTagPreModelOutputLevel))
        output->SetDisabled(true);
      pCaller->Hide(true);
      ui->SetAllControlsDirty();
    });
    pGraphics->AttachControl(
      new NAMFileBrowserControl(preModelArea, kMsgTagClearPreModel, "Select pre model...", "nam",
                                loadPreModelCompletionHandler, style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG,
                                fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl, "NAM"),
      kCtrlTagPreModelFileBrowser, "PRE_MODEL_CONTROLS");
    pGraphics->AttachControl(
      new IVToggleControl(preModelEnabledArea, kPreModelEnabled, "", compactStyle.WithShowValue(true), "BYPASS", "ON"),
      kCtrlTagPreModelEnabled, "PRE_MODEL_CONTROLS");
    pGraphics->AttachControl(
      new IVKnobControl(preModelInputArea, kPreModelInputLevel, "IN", compactStyle, true),
      kCtrlTagPreModelInputLevel, "PRE_MODEL_CONTROLS");
    pGraphics->AttachControl(
      new IVKnobControl(preModelOutputArea, kPreModelOutputLevel, "OUT", compactStyle, true),
      kCtrlTagPreModelOutputLevel, "PRE_MODEL_CONTROLS");
    pGraphics->AttachControl(
      new IVKnobControl(preModelQualityArea, kPreModelSlim, "QUALITY", compactStyle, true),
      kCtrlTagPreModelSlim, "PRE_MODEL_CONTROLS");
    pGraphics
      ->AttachControl(
        new IVButtonControl(removePreModelArea, SplashClickActionFunc, "-", compactStyle.WithShowLabel(true)), kNoTag,
        "PRE_MODEL_CONTROLS")
      ->SetAnimationEndActionFunction([](IControl* pCaller) {
        pCaller->GetDelegate()->SendArbitraryMsgFromUI(kMsgTagClearPreModel);
      });
    pGraphics->ForControlInGroup("PRE_MODEL_CONTROLS", [](IControl* control) { control->Hide(true); });

    auto hideSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(true);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(true);
      ui->SetAllControlsDirty();
    };
    auto showSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(false);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(false);
      ui->SetAllControlsDirty();
    };

    pGraphics
      ->AttachControl(
        new NAMSquareButtonControl(slimIconArea, DefaultClickActionFunc, slimIconSVG), kCtrlTagSlimmableIcon)
      ->SetAnimationEndActionFunction(showSlimOverlay)
      ->Hide(true);

    pGraphics->AttachControl(new ISVGSwitchControl(irSwitchArea, {irIconOffSVG, irIconOnSVG}, kIRToggle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(irArea, kMsgTagClearIR, defaultIRString.c_str(), "wav", loadIRCompletionHandler, style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "Get IRs", getUrl, "IR"),
      kCtrlTagIRFileBrowser);
    pGraphics->AttachControl(
      new NAMSwitchControl(ngToggleArea, kNoiseGateActive, "Noise Gate", style, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(eqToggleArea, kEQActive, "EQ", style, switchHandleBitmap));

    // The knobs
    pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, kInputLevel, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(noiseGateArea, kNoiseGateThreshold, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(
      new NAMKnobControl(bassKnobArea, kToneBass, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(midKnobArea, kToneMid, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(trebleKnobArea, kToneTreble, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(outputKnobArea, kOutputLevel, "", style, knobBackgroundBitmap));

    // The meters
    pGraphics->AttachControl(new NAMMeterControl(inputMeterArea, meterBackgroundBitmap, style), kCtrlTagInputMeter);
    pGraphics->AttachControl(new NAMMeterControl(outputMeterArea, meterBackgroundBitmap, style), kCtrlTagOutputMeter);

    // Settings/help/about box
    pGraphics->AttachControl(new NAMCircleButtonControl(
      settingsButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      gearSVG));

    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, backgroundBitmap, inputLevelBackgroundBitmap, switchHandleBitmap,
                                                 crossSVG, style, radioButtonStyle),
                      kCtrlTagSettingsBox)
      ->Hide(true);

    const auto slimKnobArea = b.GetCentredInside(100.f, NAM_KNOB_HEIGHT + 24.f);
    pGraphics->AttachControl(new NAMSlimOverlayBackdropControl(b, hideSlimOverlay), kCtrlTagSlimOverlayBackdrop)
      ->Hide(true);
    pGraphics
      ->AttachControl(new NAMKnobControl(slimKnobArea, kSlim, "CPU / Quality", style, knobBackgroundBitmap), kCtrlTagSlimKnob)
      ->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });

    // pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetMouseEventsWhenDisabled(false);
    // pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetMouseEventsWhenDisabled(false);
  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{
  _DeallocateIOPointers();
}

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const size_t numChannelsExternalIn = (size_t)NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
  const size_t numChannelsInternal = kNumChannelsInternal;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  _PrepareBuffers(numChannelsInternal, numFrames);
  // Input is collapsed to mono in preparation for the NAM.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  _ApplyDSPStaging();
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive = GetParam(kEQActive)->Value();

  // Noise gate trigger
  sample** triggerOutput = mInputPointers;
  if (noiseGateActive)
  {
    const double time = 0.01;
    const double threshold = GetParam(kNoiseGateThreshold)->Value(); // GetParam...
    const double ratio = 0.1; // Quadratic...
    const double openTime = 0.005;
    const double holdTime = 0.01;
    const double closeTime = 0.05;
    const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
    mNoiseGateTrigger.SetParams(triggerParams);
    mNoiseGateTrigger.SetSampleRate(sampleRate);
    triggerOutput = mNoiseGateTrigger.Process(mInputPointers, numChannelsInternal, numFrames);
  }

  sample** ampInput = triggerOutput;
  const bool preModelTargetActive = GetParam(kPreModelEnabled)->Bool() && mPreModelSlot.mModel != nullptr;
  const bool processPreModel = mPreModelSlot.mModel != nullptr && (preModelTargetActive || mPreModelMix > 0.0);
  if (processPreModel)
  {
    const double inputGain = DBToAmp(GetParam(kPreModelInputLevel)->Value());
    const double outputGain = DBToAmp(GetParam(kPreModelOutputLevel)->Value());
    for (size_t c = 0; c < numChannelsInternal; c++)
      for (size_t s = 0; s < numFrames; s++)
        mPreModelInputPointers[c][s] = inputGain * triggerOutput[c][s];

    mPreModelSlot.mModel->process(mPreModelInputPointers, mPreModelOutputPointers, nFrames);

    const double targetMix = preModelTargetActive ? 1.0 : 0.0;
    const double fadeSamples = std::max(1.0, 0.01 * sampleRate);
    const double mixStep = 1.0 / fadeSamples;
    for (size_t s = 0; s < numFrames; s++)
    {
      if (mPreModelMix < targetMix)
        mPreModelMix = std::min(targetMix, mPreModelMix + mixStep);
      else if (mPreModelMix > targetMix)
        mPreModelMix = std::max(targetMix, mPreModelMix - mixStep);

      for (size_t c = 0; c < numChannelsInternal; c++)
      {
        const sample dry = triggerOutput[c][s];
        const sample wet = outputGain * mPreModelOutputPointers[c][s];
        mPreModelOutputPointers[c][s] = dry + mPreModelMix * (wet - dry);
      }
    }
    ampInput = mPreModelOutputPointers;
  }
  else
  {
    mPreModelMix = 0.0;
  }

  if (mAmpModelSlot.mModel != nullptr)
    mAmpModelSlot.mModel->process(ampInput, mOutputPointers, nFrames);
  else
    _FallbackDSP(ampInput, mOutputPointers, numChannelsInternal, numFrames);
  // Apply the noise gate after the NAM
  sample** gateGainOutput =
    noiseGateActive ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, numFrames) : mOutputPointers;

  sample** toneStackOutPointers = (toneStackActive && mToneStack != nullptr)
                                    ? mToneStack->Process(gateGainOutput, numChannelsInternal, nFrames)
                                    : gateGainOutput;

  sample** irPointers = toneStackOutPointers;
  if (mIR != nullptr && GetParam(kIRToggle)->Value())
    irPointers = mIR->Process(toneStackOutPointers, numChannelsInternal, numFrames);

  // And the HPF for DC offset (Issue 271)
  const double highPassCutoffFreq = kDCBlockerFrequency;
  // const double lowPassCutoffFreq = 20000.0;
  const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
  // const recursive_linear_filter::LowPassParams lowPassParams(sampleRate, lowPassCutoffFreq);
  mHighPass.SetParams(highPassParams);
  // mLowPass.SetParams(lowPassParams);
  sample** hpfPointers = mHighPass.Process(irPointers, numChannelsInternal, numFrames);
  // sample** lpfPointers = mLowPass.Process(hpfPointers, numChannelsInternal, numFrames);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // _ProcessOutput(lpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  _UpdateMeters(mInputPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

void NeuralAmpModeler::OnReset()
{
  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = GetBlockSize();

  // Tail is because the HPF DC blocker has a decay.
  // 10 cycles should be enough to pass the VST3 tests checking tail behavior.
  // I'm ignoring the model & IR, but it's not the end of the world.
  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, GetBlockSize());
  mToneStack->Reset(sampleRate, maxBlockSize);
  _UpdateLatency();
}

void NeuralAmpModeler::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);

  if (mAmpModelSlot.mNewModelLoadedInDSP)
  {
    if (GetUI() != nullptr)
    {
      _UpdateControlsFromModel();
      mAmpModelSlot.mNewModelLoadedInDSP = false;
    }
  }

  if (mPreModelSlot.mNewModelLoadedInDSP)
  {
    if (auto* pGraphics = GetUI())
    {
      _SetPreModelUIVisible(true);
      if (auto* quality = pGraphics->GetControlWithTag(kCtrlTagPreModelSlim))
        quality->Hide(mPreModelSlot.mModel == nullptr || mPreModelSlot.mModel->GetSlimmableModel() == nullptr);
      _UpdateControlsFromModel();
      pGraphics->SetAllControlsDirty();
      mPreModelSlot.mNewModelLoadedInDSP = false;
    }
  }

  if (mAmpModelSlot.mModelCleared)
  {
    if (auto* pGraphics = GetUI())
    {
      static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->ClearModelInfo();
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimKnob))
        p->Hide(true);
      pGraphics->SetAllControlsDirty();
      mAmpModelSlot.mModelCleared = false;
    }
  }

  if (mPreModelSlot.mModelCleared)
  {
    if (GetUI() != nullptr)
    {
      _SetPreModelUIVisible(false);
      _UpdateControlsFromModel();
      mPreModelSlot.mModelCleared = false;
    }
  }
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  // If this isn't here when unserializing, then we know we're dealing with something before v0.8.0.
  WDL_String header("###NeuralAmpModeler###"); // Don't change this!
  chunk.PutStr(header.Get());
  // Plugin version, so we can load legacy serialized states in the future!
  WDL_String version(PLUG_VERSION_STR);
  chunk.PutStr(version.Get());
  // Model directory (don't serialize the model itself; we'll just load it again
  // when we unserialize)
  chunk.PutStr(mAmpModelSlot.mPath.Get());
  chunk.PutStr(mIRPath.Get());
  chunk.PutStr(mPreModelSlot.mPath.Get());
  return SerializeParams(chunk);
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  // Look for the expected header. If it's there, then we'll know what to do.
  WDL_String header;
  int pos = startPos;
  pos = chunk.GetStr(header, pos);

  const char* kExpectedHeader = "###NeuralAmpModeler###";
  if (strcmp(header.Get(), kExpectedHeader) == 0)
  {
    return _UnserializeStateWithKnownVersion(chunk, pos);
  }
  else
  {
    return _UnserializeStateWithUnknownVersion(chunk, startPos);
  }
}

void NeuralAmpModeler::OnUIOpen()
{
  Plugin::OnUIOpen();

  auto restoreModelBrowser = [&](ModelSlot& slot) {
    if (slot.mPath.GetLength())
    {
      SendControlMsgFromDelegate(slot.mBrowserCtrlTag, kMsgTagLoadedModel, slot.mPath.GetLength(), slot.mPath.Get());
      if (slot.mModel == nullptr && slot.mStagedModel == nullptr)
        SendControlMsgFromDelegate(slot.mBrowserCtrlTag, kMsgTagLoadFailed);
    }
  };
  restoreModelBrowser(mAmpModelSlot);
  restoreModelBrowser(mPreModelSlot);
  _SetPreModelUIVisible(mPreModelSlot.mPath.GetLength() > 0);
  if (auto* pGraphics = GetUI())
    if (auto* quality = pGraphics->GetControlWithTag(kCtrlTagPreModelSlim))
      quality->Hide(mPreModelSlot.mModel == nullptr || mPreModelSlot.mModel->GetSlimmableModel() == nullptr);

  if (mIRPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
    if (mIR == nullptr && mStagedIR == nullptr)
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  if (mAmpModelSlot.mModel != nullptr)
    _UpdateControlsFromModel();
}

void NeuralAmpModeler::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
    // Changes to the input gain
    case kCalibrateInput:
    case kInputCalibrationLevel:
    case kInputLevel: _SetInputGain(); break;
    // Changes to the output gain
    case kOutputLevel:
    case kOutputMode: _SetOutputGain(); break;
    // Tone stack:
    case kToneBass: mToneStack->SetParam("bass", GetParam(paramIdx)->Value()); break;
    case kToneMid: mToneStack->SetParam("middle", GetParam(paramIdx)->Value()); break;
    case kToneTreble: mToneStack->SetParam("treble", GetParam(paramIdx)->Value()); break;
    case kSlim: _ApplySlimParamToModelSlot(EModelSlot::Amp); break;
    case kPreModelSlim: _ApplySlimParamToModelSlot(EModelSlot::Pre); break;
    case kPreModelEnabled:
      _SetInputGain();
      _UpdateLatency();
      break;
    default: break;
  }
}

void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (auto pGraphics = GetUI())
  {
    bool active = GetParam(paramIdx)->Bool();

    switch (paramIdx)
    {
      case kNoiseGateActive: pGraphics->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active); break;
      case kEQActive:
        pGraphics->ForControlInGroup("EQ_KNOBS", [active](IControl* pControl) { pControl->SetDisabled(!active); });
        break;
      case kIRToggle: pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->SetDisabled(!active); break;
      case kPreModelEnabled:
        if (auto* control = pGraphics->GetControlWithTag(kCtrlTagPreModelInputLevel))
          control->SetDisabled(!active);
        if (auto* control = pGraphics->GetControlWithTag(kCtrlTagPreModelOutputLevel))
          control->SetDisabled(!active);
        if (auto* control = pGraphics->GetControlWithTag(kCtrlTagPreModelSlim))
          control->SetDisabled(!active);
        _UpdateControlsFromModel();
        break;
      default: break;
    }
  }
}

bool NeuralAmpModeler::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModel: mAmpModelSlot.mShouldRemove = true; return true;
    case kMsgTagClearPreModel:
      mPreModelSlot.mShouldRemove = true;
      SetParameterValue(kPreModelEnabled, 0.0);
      _SetPreModelUIVisible(false);
      return true;
    case kMsgTagClearIR: mShouldRemoveIR = true; return true;
    case kMsgTagHighlightColor:
    {
      mHighLightColor.Set((const char*)pData);

      if (GetUI())
      {
        GetUI()->ForStandardControlsFunc([&](IControl* pControl) {
          if (auto* pVectorBase = pControl->As<IVectorBase>())
          {
            IColor color = IColor::FromColorCodeStr(mHighLightColor.Get());

            pVectorBase->SetColor(kX1, color);
            pVectorBase->SetColor(kPR, color.WithOpacity(0.3f));
            pVectorBase->SetColor(kFR, color.WithOpacity(0.4f));
            pVectorBase->SetColor(kX3, color.WithContrast(0.1f));
          }
          pControl->GetUI()->SetAllControlsDirty();
        });
      }

      return true;
    }
    default: return false;
  }
}

// Private methods ============================================================

void NeuralAmpModeler::_AllocateIOPointers(const size_t nChans)
{
  if (mInputPointers != nullptr || mPreModelInputPointers != nullptr || mPreModelOutputPointers != nullptr ||
      mOutputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate audio pointers without freeing them");

  mInputPointers = new sample*[nChans];
  mPreModelInputPointers = new sample*[nChans];
  mPreModelOutputPointers = new sample*[nChans];
  mOutputPointers = new sample*[nChans];
}

void NeuralAmpModeler::_ApplyDSPStaging()
{
  ModelSlot* slots[] = {&mPreModelSlot, &mAmpModelSlot};
  bool modelConfigurationChanged = false;
  for (ModelSlot* slot : slots)
  {
    if (slot->mShouldRemove)
    {
      slot->mModel = nullptr;
      slot->mStagedModel = nullptr;
      slot->mPath.Set("");
      slot->mShouldRemove = false;
      slot->mModelCleared = true;
      modelConfigurationChanged = true;
      if (slot == &mPreModelSlot)
        mPreModelMix = 0.0;
    }

    if (slot->mStagedModel != nullptr)
    {
      slot->mModel = std::move(slot->mStagedModel);
      slot->mNewModelLoadedInDSP = true;
      modelConfigurationChanged = true;
    }
  }

  if (modelConfigurationChanged)
  {
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
  }

  if (mShouldRemoveIR)
  {
    mIR = nullptr;
    mIRPath.Set("");
    mShouldRemoveIR = false;
  }
  if (mStagedIR != nullptr)
    mIR = std::move(mStagedIR);
}

void NeuralAmpModeler::_DeallocateIOPointers()
{
  delete[] mInputPointers;
  delete[] mPreModelInputPointers;
  delete[] mPreModelOutputPointers;
  delete[] mOutputPointers;
  mInputPointers = nullptr;
  mPreModelInputPointers = nullptr;
  mPreModelOutputPointers = nullptr;
  mOutputPointers = nullptr;
}

void NeuralAmpModeler::_FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels,
                                    const size_t numFrames)
{
  for (size_t c = 0; c < numChannels; c++)
    for (size_t s = 0; s < numFrames; s++)
      outputs[c][s] = inputs[c][s];
}

void NeuralAmpModeler::_ResetModelAndIR(const double sampleRate, const int maxBlockSize)
{
  ModelSlot* slots[] = {&mPreModelSlot, &mAmpModelSlot};
  for (ModelSlot* slot : slots)
  {
    if (slot->mStagedModel != nullptr)
      slot->mStagedModel->Reset(sampleRate, maxBlockSize);
    else if (slot->mModel != nullptr)
      slot->mModel->Reset(sampleRate, maxBlockSize);
  }

  if (mStagedIR != nullptr)
  {
    const double irSampleRate = mStagedIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mStagedIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
  else if (mIR != nullptr)
  {
    const double irSampleRate = mIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
}

void NeuralAmpModeler::_SetInputGain()
{
  iplug::sample inputGainDB = GetParam(kInputLevel)->Value();
  ResamplingNAM* calibrationModel = nullptr;
  if (GetParam(kPreModelEnabled)->Bool() && mPreModelSlot.mModel != nullptr &&
      mPreModelSlot.mModel->HasInputLevel())
    calibrationModel = mPreModelSlot.mModel.get();
  else if (mAmpModelSlot.mModel != nullptr && mAmpModelSlot.mModel->HasInputLevel())
    calibrationModel = mAmpModelSlot.mModel.get();

  if (calibrationModel != nullptr && GetParam(kCalibrateInput)->Bool())
    inputGainDB += GetParam(kInputCalibrationLevel)->Value() - calibrationModel->GetInputLevel();

  mInputGain = DBToAmp(inputGainDB);
}

void NeuralAmpModeler::_SetOutputGain()
{
  double gainDB = GetParam(kOutputLevel)->Value();
  if (mAmpModelSlot.mModel != nullptr)
  {
    const int outputMode = GetParam(kOutputMode)->Int();
    switch (outputMode)
    {
      case 1: // Normalized
        if (mAmpModelSlot.mModel->HasLoudness())
        {
          const double loudness = mAmpModelSlot.mModel->GetLoudness();
          const double targetLoudness = -18.0;
          gainDB += (targetLoudness - loudness);
        }
        break;
      case 2: // Calibrated
        if (mAmpModelSlot.mModel->HasOutputLevel())
        {
          const double inputLevel = GetParam(kInputCalibrationLevel)->Value();
          const double outputLevel = mAmpModelSlot.mModel->GetOutputLevel();
          gainDB += (outputLevel - inputLevel);
        }
        break;
      case 0: // Raw
      default: break;
    }
  }
  mOutputGain = DBToAmp(gainDB);
}

ModelSlot& NeuralAmpModeler::_GetModelSlot(EModelSlot slot)
{
  return slot == EModelSlot::Pre ? mPreModelSlot : mAmpModelSlot;
}

const ModelSlot& NeuralAmpModeler::_GetModelSlot(EModelSlot slot) const
{
  return slot == EModelSlot::Pre ? mPreModelSlot : mAmpModelSlot;
}

void NeuralAmpModeler::_ApplySlimParamToModelSlot(EModelSlot slotId)
{
  ModelSlot& slot = _GetModelSlot(slotId);
  const double value = GetParam(slot.mSlimParamIdx)->Value();
  auto apply = [value](ResamplingNAM* model) {
    if (model != nullptr)
      if (nam::SlimmableModel* slimmable = model->GetSlimmableModel())
        slimmable->SetSlimmableSize(value);
  };
  apply(slot.mModel.get());
  apply(slot.mStagedModel.get());
}

std::string NeuralAmpModeler::_StageModel(EModelSlot slotId, const WDL_String& modelPath)
{
  ModelSlot& slot = _GetModelSlot(slotId);
  WDL_String previousPath = slot.mPath;
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    std::unique_ptr<nam::DSP> model = nam::get_dsp(dspPath);

    if (model->NumInputChannels() != 1)
      throw std::runtime_error("Model must have 1 input channel, but has " + std::to_string(model->NumInputChannels()));
    if (model->NumOutputChannels() != 1)
      throw std::runtime_error("Model must have 1 output channel, but has " +
                               std::to_string(model->NumOutputChannels()));

    std::unique_ptr<ResamplingNAM> temp = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate());
    temp->Reset(GetSampleRate(), GetBlockSize());
    if (nam::SlimmableModel* slimmable = temp->GetSlimmableModel())
      slimmable->SetSlimmableSize(GetParam(slot.mSlimParamIdx)->Value());

    slot.mStagedModel = std::move(temp);
    slot.mShouldRemove = false;
    slot.mPath = modelPath;
    SendControlMsgFromDelegate(slot.mBrowserCtrlTag, kMsgTagLoadedModel, slot.mPath.GetLength(), slot.mPath.Get());
  }
  catch (std::runtime_error& e)
  {
    SendControlMsgFromDelegate(slot.mBrowserCtrlTag, kMsgTagLoadFailed);
    slot.mStagedModel = nullptr;
    slot.mPath = previousPath;
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIR(const WDL_String& irPath)
{
  // FIXME it'd be better for the path to be "staged" as well. Just in case the
  // path and the model got caught on opposite sides of the fence...
  WDL_String previousIRPath = mIRPath;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    mStagedIR = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = mStagedIR->GetWavState();
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    mShouldRemoveIR = false;
    mIRPath = irPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
  }
  else
  {
    if (mStagedIR != nullptr)
    {
      mStagedIR = nullptr;
    }
    mIRPath = previousIRPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  return wavState;
}

size_t NeuralAmpModeler::_GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
  return mInputArray.size();
}

size_t NeuralAmpModeler::_GetBufferNumFrames() const
{
  if (_GetBufferNumChannels() == 0)
    return 0;
  return mInputArray[0].size();
}

void NeuralAmpModeler::_InitToneStack()
{
  // If you want to customize the tone stack, then put it here!
  mToneStack = std::make_unique<dsp::tone_stack::BasicNamToneStack>();
}
void NeuralAmpModeler::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != _GetBufferNumChannels();
  const bool updateFrames = updateChannels || (_GetBufferNumFrames() != numFrames);

  if (updateChannels)
  {
    _PrepareIOPointers(numChannels);
    mInputArray.resize(numChannels);
    mPreModelInputArray.resize(numChannels);
    mPreModelOutputArray.resize(numChannels);
    mOutputArray.resize(numChannels);
  }
  if (updateFrames)
  {
    auto resizeAndClear = [numFrames](std::vector<std::vector<sample>>& buffer) {
      for (auto& channel : buffer)
      {
        channel.resize(numFrames);
        std::fill(channel.begin(), channel.end(), 0.0);
      }
    };
    resizeAndClear(mInputArray);
    resizeAndClear(mPreModelInputArray);
    resizeAndClear(mPreModelOutputArray);
    resizeAndClear(mOutputArray);
  }

  for (size_t c = 0; c < numChannels; c++)
  {
    mInputPointers[c] = mInputArray[c].data();
    mPreModelInputPointers[c] = mPreModelInputArray[c].data();
    mPreModelOutputPointers[c] = mPreModelOutputArray[c].data();
    mOutputPointers[c] = mOutputArray[c].data();
  }
}

void NeuralAmpModeler::_PrepareIOPointers(const size_t numChannels)
{
  _DeallocateIOPointers();
  _AllocateIOPointers(numChannels);
}

void NeuralAmpModeler::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut)
{
  // We'll assume that the main processing is mono for now. We'll handle dual amps later.
  if (nChansOut != 1)
  {
    std::stringstream ss;
    ss << "Expected mono output, but " << nChansOut << " output channels are requested!";
    throw std::runtime_error(ss.str());
  }

  // On the standalone, we can probably assume that the user has plugged into only one input and they expect it to be
  // carried straight through. Don't apply any division over nChansIn because we're just "catching anything out there."
  // However, in a DAW, it's probably something providing stereo, and we want to take the average in order to avoid
  // doubling the loudness. (This would change w/ double mono processing)
  double gain = mInputGain;
#ifndef APP_API
  gain /= (float)nChansIn;
#endif
  // Assume _PrepareBuffers() was already called
  for (size_t c = 0; c < nChansIn; c++)
    for (size_t s = 0; s < nFrames; s++)
      if (c == 0)
        mInputArray[0][s] = gain * inputs[c][s];
      else
        mInputArray[0][s] += gain * inputs[c][s];
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = mOutputGain;
  // Assume _PrepareBuffers() was already called
  if (nChansIn != 1)
    throw std::runtime_error("Plugin is supposed to process in mono.");
  // Broadcast the internal mono stream to all output channels.
  const size_t cin = 0;
  for (auto cout = 0; cout < nChansOut; cout++)
    for (auto s = 0; s < nFrames; s++)
#ifdef APP_API // Ensure valid output to interface
      outputs[cout][s] = std::clamp(gain * inputs[cin][s], -1.0, 1.0);
#else // In a DAW, other things may come next and should be able to handle large
      // values.
      outputs[cout][s] = gain * inputs[cin][s];
#endif
}

void NeuralAmpModeler::_SetPreModelUIVisible(bool visible)
{
  if (auto* pGraphics = GetUI())
  {
    pGraphics->ForControlInGroup("PRE_MODEL_CONTROLS", [visible](IControl* control) { control->Hide(!visible); });
    if (auto* addButton = pGraphics->GetControlWithTag(kCtrlTagAddPreModel))
      addButton->Hide(visible);
    pGraphics->SetAllControlsDirty();
  }
}

void NeuralAmpModeler::_UpdateControlsFromModel()
{
  if (mAmpModelSlot.mModel == nullptr)
  {
    return;
  }
  if (auto* pGraphics = GetUI())
  {
    ModelInfo modelInfo;
    modelInfo.sampleRate.known = true;
    modelInfo.sampleRate.value = mAmpModelSlot.mModel->GetEncapsulatedSampleRate();
    modelInfo.inputCalibrationLevel.known = mAmpModelSlot.mModel->HasInputLevel();
    modelInfo.inputCalibrationLevel.value =
      mAmpModelSlot.mModel->HasInputLevel() ? mAmpModelSlot.mModel->GetInputLevel() : 0.0;
    modelInfo.outputCalibrationLevel.known = mAmpModelSlot.mModel->HasOutputLevel();
    modelInfo.outputCalibrationLevel.value =
      mAmpModelSlot.mModel->HasOutputLevel() ? mAmpModelSlot.mModel->GetOutputLevel() : 0.0;

    static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->SetModelInfo(modelInfo);

    const ResamplingNAM* inputCalibrationModel = nullptr;
    if (GetParam(kPreModelEnabled)->Bool() && mPreModelSlot.mModel != nullptr &&
        mPreModelSlot.mModel->HasInputLevel())
      inputCalibrationModel = mPreModelSlot.mModel.get();
    else if (mAmpModelSlot.mModel->HasInputLevel())
      inputCalibrationModel = mAmpModelSlot.mModel.get();
    const bool disableInputCalibrationControls = inputCalibrationModel == nullptr;
    pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetDisabled(disableInputCalibrationControls);
    pGraphics->GetControlWithTag(kCtrlTagInputCalibrationLevel)->SetDisabled(disableInputCalibrationControls);
    {
      auto* c = static_cast<OutputModeControl*>(pGraphics->GetControlWithTag(kCtrlTagOutputMode));
      c->SetNormalizedDisable(!mAmpModelSlot.mModel->HasLoudness());
      c->SetCalibratedDisable(!mAmpModelSlot.mModel->HasOutputLevel());
    }

    if (auto* pSlimIcon = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
    {
      const bool show = mAmpModelSlot.mModel->GetSlimmableModel() != nullptr;
      pSlimIcon->Hide(!show);
    }
  }
}

void NeuralAmpModeler::_UpdateLatency()
{
  int latency = 0;
  if (GetParam(kPreModelEnabled)->Bool() && mPreModelSlot.mModel != nullptr)
    latency += mPreModelSlot.mModel->GetLatency();
  if (mAmpModelSlot.mModel != nullptr)
    latency += mAmpModelSlot.mModel->GetLatency();

  if (GetLatency() != latency)
    SetLatency(latency);
}

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  // Right now, we didn't specify MAXNC when we initialized these, so it's 1.
  const int nChansHack = 1;
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
}

// HACK
#include "Unserialization.cpp"
