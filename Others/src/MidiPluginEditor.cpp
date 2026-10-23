#include "MidiPluginEditor.h"

#include <cmath>

namespace
{
juce::String u8s (const char* text) { return juce::String::fromUTF8 (text); }

const juce::Colour backgroundTop (0xff151a22);
const juce::Colour backgroundBottom (0xff0a0c10);
const juce::Colour panelColour (0xff11161e);
const juce::Colour panelLine (0xff2a3340);
const juce::Colour accent (0xffffa23a);
const juce::Colour textColour (0xffe8e9ec);
const juce::Colour dimText (0xff8b95a5);
const juce::Colour wallColour (0xff343d4c);

constexpr int windowWidth = 1180;
constexpr int windowHeight = 800;

juce::Colour pitchColour (float value)
{
    const auto t = juce::jlimit (0.0f, 1.0f, (value - 36.0f) / 48.0f);
    return juce::Colour (0xffff7e36).interpolatedWith (juce::Colour (0xffffe58c), t);
}

juce::Colour adsrColour (float value)
{
    const auto t = juce::jlimit (0.0f, 1.0f, value);
    return juce::Colour (0xff2c96a0).interpolatedWith (juce::Colour (0xff6ef0d6), t);
}

juce::Colour effectColour (float value)
{
    const auto t = juce::jlimit (0.0f, 1.0f, value);
    return juce::Colour (0xff8c42be).interpolatedWith (juce::Colour (0xffee96ff), t);
}

struct GroupInfo
{
    juce::Rectangle<int> rect;
    const char* title;
    int columns;
};
}

MidiLookAndFeel::MidiLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, textColour);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1a2029));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202734));
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, textColour);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff191207));

    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1a2029));
    setColour (juce::ComboBox::textColourId, textColour);
    setColour (juce::ComboBox::outlineColourId, panelLine);
    setColour (juce::ComboBox::arrowColourId, accent);

    setColour (juce::ToggleButton::textColourId, textColour);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff2c3542));
}

void MidiLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPosProportional, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (9.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle
                     + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const auto stroke = juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff2c3542));
    g.strokePath (track, stroke);

    if (sliderPosProportional > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                             rotaryStartAngle, angle, true);
        g.setColour (accent);
        g.strokePath (value, stroke);
    }

    const auto thumbAngle = angle - juce::MathConstants<float>::halfPi;
    const auto thumb = centre + juce::Point<float> (std::cos (thumbAngle),
                                                    std::sin (thumbAngle)) * radius;
    g.setColour (juce::Colour (0xffffd9a0));
    g.drawLine (centre.x, centre.y, thumb.x, thumb.y, 1.3f);
    g.fillEllipse (thumb.x - 2.0f, thumb.y - 2.0f, 4.0f, 4.0f);
}

juce::Font MidiLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions().withName ("Microsoft YaHei").withHeight (12.5f));
}

juce::Font MidiLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions().withName ("Microsoft YaHei").withHeight (12.5f));
}

RandomSynthMidiAudioProcessorEditor::Knob::Knob (const juce::String& labelText, int textBoxWidth)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, textColour);
    label.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei")
                                                    .withHeight (12.0f)));
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxWidth, 18);
    slider.setNumDecimalPlacesToDisplay (2);
    addAndMakeVisible (slider);
}

void RandomSynthMidiAudioProcessorEditor::Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (18));
    slider.setBounds (area);
}

RandomSynthMidiAudioProcessorEditor::RandomSynthMidiAudioProcessorEditor (RandomSynthMidiAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setLookAndFeel (&lookAndFeel);
    setSize (windowWidth, windowHeight);
    setResizable (false, false);

    for (int channel = 1; channel <= 16; ++channel)
        midiChannelBox.addItem (juce::String (channel), channel);

    midiChannelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, RandomSynthMidiParamIDs::midiChannel, midiChannelBox);
    addAndMakeVisible (midiChannelBox);

    fineButton.setButtonText ("FINE");
    fineAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, RandomSynthMidiParamIDs::fine, fineButton);
    fineButton.onStateChange = [this] { updateRangeSnap(); };
    addAndMakeVisible (fineButton);

    runButton.setButtonText ("RUN");
    stopButton.setButtonText ("STOP");
    resetButton.setButtonText ("RESET");

    runButton.onClick = [this]
    {
        processor.setRunning (true);
        updateTransportColours();
        repaint();
    };
    stopButton.onClick = [this]
    {
        processor.setRunning (false);
        updateTransportColours();
        repaint();
    };
    resetButton.onClick = [this] { processor.resetWorld(); };

    addAndMakeVisible (runButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (resetButton);

    addKnob (worldGroup,  u8s ("小球数量"), RandomSynthMidiParamIDs::ballCount, 58);
    addKnob (worldGroup,  u8s ("加速器"), RandomSynthMidiParamIDs::boosterCount, 58);
    addKnob (worldGroup,  u8s ("场地大小"), RandomSynthMidiParamIDs::field, 66);
    addKnob (worldGroup,  u8s ("小球大小"), RandomSynthMidiParamIDs::ball, 66);
    addKnob (worldGroup,  u8s ("小球速度"), RandomSynthMidiParamIDs::speed, 66);
    addKnob (worldGroup,  u8s ("调速范围"), RandomSynthMidiParamIDs::speedShare, 66);
    addKnob (worldGroup,  u8s ("调速强度"), RandomSynthMidiParamIDs::strength, 66);
    addKnob (worldGroup,  u8s ("水波速度"), RandomSynthMidiParamIDs::waveSpeed, 66);
    addKnob (worldGroup,  u8s ("水波范围"), RandomSynthMidiParamIDs::waveRange, 66);
    addKnob (worldGroup,  u8s ("场地形状"), RandomSynthMidiParamIDs::shape, 76);

    addKnob (tuningGroup, u8s ("音阶"), RandomSynthMidiParamIDs::scale, 84);
    addKnob (tuningGroup, u8s ("音域"), RandomSynthMidiParamIDs::range, 78);
    rangeKnob = groupKnobs[(size_t) tuningGroup].back();

    addKnob (voiceGroup,  u8s ("音高比例"), RandomSynthMidiParamIDs::ratioPitch, 62);
    addKnob (voiceGroup,  u8s ("ADSR 比例"), RandomSynthMidiParamIDs::ratioAdsr, 62);
    addKnob (voiceGroup,  u8s ("效果比例"), RandomSynthMidiParamIDs::ratioEffect, 62);

    addKnob (gateGroup,   u8s ("GATE"), RandomSynthMidiParamIDs::gate, 66);
    addKnob (gateGroup,   u8s ("GATE MIN"), RandomSynthMidiParamIDs::gateMin, 66);
    addKnob (gateGroup,   u8s ("GATE MAX"), RandomSynthMidiParamIDs::gateMax, 66);

    updateTransportColours();
    updateRangeSnap();
    resized();
    startTimerHz (60);
}

void RandomSynthMidiAudioProcessorEditor::updateRangeSnap()
{
    if (rangeKnob == nullptr)
        return;

    const auto fine = fineButton.getToggleState();

    auto range = rangeKnob->slider.getNormalisableRange();
    range.interval = fine ? 0.0 : 1.0;
    rangeKnob->slider.setNormalisableRange (range);

    if (! fine)
    {
        if (auto* parameter = processor.apvts.getParameter (RandomSynthMidiParamIDs::range))
        {
            const auto halfSteps = parameter->convertFrom0to1 (parameter->getValue());
            const auto snapped = std::round (halfSteps);

            if (std::abs (halfSteps - snapped) > 1.0e-4f)
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (snapped));
        }
    }
}

RandomSynthMidiAudioProcessorEditor::~RandomSynthMidiAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RandomSynthMidiAudioProcessorEditor::addKnob (int group, const juce::String& label,
                                                    const juce::String& parameterID, int textBoxWidth)
{
    auto knob = std::make_unique<Knob> (label, textBoxWidth);
    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, parameterID, knob->slider);

    groupKnobs[(size_t) group].push_back (knob.get());
    addAndMakeVisible (*knob);
    knobs.push_back (std::move (knob));
}

void RandomSynthMidiAudioProcessorEditor::timerCallback()
{
    view = processor.getSnapshot();
    updateTransportColours();
    repaint();
}

void RandomSynthMidiAudioProcessorEditor::updateTransportColours()
{
    auto setButton = [] (juce::TextButton& button, bool active)
    {
        button.setColour (juce::TextButton::buttonColourId,
                          active ? accent : juce::Colour (0xff202734));
        button.setColour (juce::TextButton::textColourOffId,
                          active ? juce::Colour (0xff191207) : textColour);
    };

    setButton (runButton, processor.isRunning());
    setButton (stopButton, ! processor.isRunning());

    resetButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202734));
    resetButton.setColour (juce::TextButton::textColourOffId, textColour);
}

juce::Colour RandomSynthMidiAudioProcessorEditor::kindColour (PropKind kind, float value) const
{
    if (kind == PropKind::pitch) return pitchColour (value);
    if (kind == PropKind::adsr) return adsrColour (value);
    return effectColour (value);
}

void RandomSynthMidiAudioProcessorEditor::drawWorld (juce::Graphics& g)
{
    const auto area = worldRect.toFloat();
    g.setColour (juce::Colour (0xff0a0e14));
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (panelLine);
    g.drawRoundedRectangle (area, 8.0f, 1.0f);

    auto inner = area.reduced (8.0f);
    const auto scale = juce::jmin (inner.getWidth() / World::fieldW,
                                   inner.getHeight() / World::fieldH);
    const auto ox = inner.getX() + (inner.getWidth() - World::fieldW * scale) * 0.5f
                  - World::fieldX * scale;
    const auto oy = inner.getY() + (inner.getHeight() - World::fieldH * scale) * 0.5f
                  - World::fieldY * scale;
    const auto transform = juce::AffineTransform::scale (scale).translated (ox, oy);

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());
    g.addTransform (transform);

    if (! view.segments.empty())
    {
        juce::Path boundary;
        boundary.startNewSubPath (view.segments.front().a);
        for (const auto& segment : view.segments)
            boundary.lineTo (segment.b);
        boundary.closeSubPath();

        g.setColour (juce::Colour (0xff0c1118));
        g.fillPath (boundary);

        const auto pixel = 1.0f / scale;
        for (const auto& segment : view.segments)
        {
            auto colour = wallColour;
            if (segment.hasProp && segment.strength > 0.02f)
            {
                const auto propColour = kindColour (segment.kind, segment.value);
                colour = wallColour.interpolatedWith (propColour,
                                                      juce::jlimit (0.0f, 1.0f, segment.strength));
            }

            g.setColour (colour);
            g.drawLine (segment.a.x, segment.a.y, segment.b.x, segment.b.y,
                        (segment.strength > 0.25f ? 2.0f : 1.2f) * pixel);
        }
    }

    const auto pixel = 1.0f / scale;

    for (const auto& field : view.fields)
    {
        const auto colour = kindColour (field.kind, field.value);
        g.setColour (colour.withAlpha (juce::jlimit (0.0f, 1.0f, field.strength) * 0.12f));
        g.fillEllipse (field.pos.x - field.radius, field.pos.y - field.radius,
                       field.radius * 2.0f, field.radius * 2.0f);
    }

    for (const auto& wave : view.waves)
    {
        const auto colour = kindColour (wave.kind, 0.7f);
        const auto alpha = juce::jlimit (0.0f, 1.0f, wave.amp) * 0.65f;
        g.setColour (colour.withAlpha (alpha));
        g.drawEllipse (wave.pos.x - wave.radius, wave.pos.y - wave.radius,
                       wave.radius * 2.0f, wave.radius * 2.0f, 1.4f * pixel);
    }

    for (const auto& link : view.links)
    {
        g.setColour (juce::Colour (0xff91d4ff).withAlpha (0.22f + 0.50f * link.amount));
        g.drawLine (link.a.x, link.a.y, link.b.x, link.b.y, 1.2f * pixel);
    }

    for (const auto& booster : view.boosters)
    {
        const auto pulse = 0.5f + 0.5f * std::sin (view.elapsed * 4.0f + booster.phase);
        const auto colour = booster.cooldown > 0.0f ? juce::Colour (0xff8c8562)
                                                     : juce::Colour (0xffffdb59);
        const auto ringRadius = booster.radius + 1.0f + pulse * 2.0f;
        g.setColour (colour);
        g.drawEllipse (booster.pos.x - ringRadius, booster.pos.y - ringRadius,
                       ringRadius * 2.0f, ringRadius * 2.0f, 1.4f * pixel);

        for (int k = 0; k < 4; ++k)
        {
            const auto angle = view.elapsed * 0.8f + (float) k
                             * juce::MathConstants<float>::halfPi;
            const auto p1 = booster.pos + juce::Point<float> (std::cos (angle), std::sin (angle))
                          * (booster.radius + 4.0f);
            const auto p2 = booster.pos + juce::Point<float> (std::cos (angle), std::sin (angle))
                          * (booster.radius + 10.0f + pulse * 3.0f);
            g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.2f * pixel);
        }
    }

    for (const auto& ball : view.balls)
    {
        const auto colour = kindColour (ball.kind, ball.value);
        const auto radius = juce::jmax (2.0f, ball.radius);
        const auto glowRadius = radius * 2.6f;

        g.setColour (colour.withAlpha (0.12f));
        g.fillEllipse (ball.pos.x - glowRadius, ball.pos.y - glowRadius,
                       glowRadius * 2.0f, glowRadius * 2.0f);

        auto fill = colour;
        if (ball.flash > 0.0f)
            fill = juce::Colour::fromFloatRGBA (juce::jmin (1.0f, colour.getFloatRed() + 0.5f),
                                                juce::jmin (1.0f, colour.getFloatGreen() + 0.5f),
                                                juce::jmin (1.0f, colour.getFloatBlue() + 0.5f),
                                                1.0f);

        g.setColour (fill);
        g.fillEllipse (ball.pos.x - radius, ball.pos.y - radius,
                       radius * 2.0f, radius * 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawEllipse (ball.pos.x - radius, ball.pos.y - radius,
                       radius * 2.0f, radius * 2.0f, 0.8f * pixel);
    }

    g.restoreState();

    if (! processor.isRunning())
    {
        g.setColour (juce::Colours::black.withAlpha (0.42f));
        g.fillRoundedRectangle (area, 8.0f);
        g.setColour (textColour);
        g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei")
                                                    .withHeight (28.0f)));
        g.drawText ("STOPPED", area, juce::Justification::centred, false);
    }
}

void RandomSynthMidiAudioProcessorEditor::drawStatusBar (juce::Graphics& g)
{
    const auto area = statusRect.toFloat();
    g.setColour (juce::Colour (0xff11161e));
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (panelLine);
    g.drawRoundedRectangle (area, 6.0f, 1.0f);

    const auto scaleText = juce::String::fromUTF8 (scaleName (processor.getScaleIndex()));
    const auto noteName = juce::MidiMessage::getMidiNoteName (processor.getLastNote(), true, true, 4);
    const auto gate = processor.isGateActive();
    const auto trig = processor.isTrigActive();

    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei").withHeight (11.5f)));
    g.setColour (dimText);

    auto effectiveRange = processor.getRangeValue();
    if (! processor.isFineEnabled())
        effectiveRange = std::round (effectiveRange * 2.0f) * 0.5f;

    const auto left = juce::String ("CH ") + juce::String (processor.getMidiChannel())
                    + "    BALLS " + juce::String ((int) view.balls.size())
                    + "    BOOST " + juce::String ((int) view.boosters.size())
                    + "    SPD " + juce::String (juce::roundToInt (view.averageSpeed))
                    + "    RNG " + juce::String (effectiveRange, 1) + " oct"
                    + "    " + scaleText;
    g.drawText (left, statusRect.reduced (12, 0), juce::Justification::left, false);

    const auto right = juce::String ("OUT ") + noteName
                     + "    GATE " + (gate ? "ON" : "OFF")
                     + "    TRIG " + (trig ? "ON" : "OFF")
                     + "    FX " + juce::String (processor.getLastSend(), 2)
                     + "    PAN " + juce::String (processor.getLastPan(), 2);
    g.setColour (gate || trig ? textColour : dimText);
    g.drawText (right, statusRect.reduced (12, 0), juce::Justification::right, false);
}

void RandomSynthMidiAudioProcessorEditor::drawPanels (juce::Graphics& g)
{
    const std::array<GroupInfo, groupCount> groups {{
        { { 20, 536, 520, 206 }, "WORLD", 5 },
        { { 550, 536, 220, 206 }, "TUNING", 2 },
        { { 780, 536, 180, 206 }, "VOICE", 2 },
        { { 970, 536, 180, 206 }, "GATE", 2 }
    }};

    for (const auto& group : groups)
    {
        g.setColour (panelColour);
        g.fillRoundedRectangle (group.rect.toFloat(), 8.0f);
        g.setColour (panelLine);
        g.drawRoundedRectangle (group.rect.toFloat(), 8.0f, 1.0f);

        g.setColour (accent.withAlpha (0.9f));
        g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei")
                                                    .withHeight (12.0f)));
        g.drawText (group.title, group.rect.getX() + 14, group.rect.getY() + 9,
                    group.rect.getWidth() - 28, 18, juce::Justification::left, false);
    }
}

void RandomSynthMidiAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (backgroundTop, 0.0f, 0.0f,
                                     backgroundBottom, 0.0f, (float) windowHeight, false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (accent);
    g.fillRoundedRectangle (24.0f, 20.0f, 4.0f, 30.0f, 2.0f);

    g.setColour (textColour);
    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei")
                                                .withHeight (22.0f)));
    g.drawText ("RandomSynth MIDI", 40, 14, 360, 28, juce::Justification::left, false);

    g.setColour (dimText);
    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei").withHeight (12.0f)));
    g.drawText (u8s ("碰撞生成式 MIDI 音序器 · 单一 VST3 · 通道在插件内选择"),
                42, 46, 560, 20, juce::Justification::left, false);

    g.setColour (dimText);
    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei").withHeight (12.0f)));
    g.drawText ("MIDI CH", 690, 24, 64, 20, juce::Justification::right, false);

    drawWorld (g);
    drawStatusBar (g);
    drawPanels (g);

    g.setColour (dimText);
    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei").withHeight (11.5f)));
    g.drawText (u8s ("RUN/STOP 控制粒子引擎；RESET 重置碰撞场。ADSR 球会改写其他球的 Gate。"),
                24, 756, 820, 18, juce::Justification::left, false);
    g.drawText (u8s ("波形/混响 CV 与 ADSR 只保留为 SoundEvent 代码接口，不再占用面板旋钮。"),
                24, 774, 900, 18, juce::Justification::left, false);
}

void RandomSynthMidiAudioProcessorEditor::resized()
{
    midiChannelBox.setBounds (760, 20, 80, 28);
    runButton.setBounds (860, 20, 64, 28);
    stopButton.setBounds (932, 20, 64, 28);
    resetButton.setBounds (1004, 20, 76, 28);

    worldRect = { 20, 92, 1140, 396 };
    statusRect = { 20, 496, 1140, 30 };

    const std::array<GroupInfo, groupCount> groups {{
        { { 20, 536, 520, 206 }, "WORLD", 5 },
        { { 550, 536, 220, 206 }, "TUNING", 2 },
        { { 780, 536, 180, 206 }, "VOICE", 2 },
        { { 970, 536, 180, 206 }, "GATE", 2 }
    }};

    for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex)
    {
        auto area = groups[(size_t) groupIndex].rect.reduced (12);
        area.removeFromTop (24);

        if (groupIndex == tuningGroup)
            area.removeFromBottom (42);

        auto& list = groupKnobs[(size_t) groupIndex];
        const auto columns = groups[(size_t) groupIndex].columns;
        const auto rows = juce::jmax (1, (int) std::ceil ((float) list.size() / (float) columns));
        const auto cellWidth = area.getWidth() / columns;
        const auto cellHeight = juce::jmin (82, area.getHeight() / rows);
        const auto contentY = area.getY() + (area.getHeight() - cellHeight * rows) / 2;

        for (int i = 0; i < (int) list.size(); ++i)
        {
            const auto column = i % columns;
            const auto row = i / columns;
            list[(size_t) i]->setBounds (area.getX() + column * cellWidth,
                                         contentY + row * cellHeight,
                                         cellWidth, cellHeight);
        }
    }

    const auto tuningRect = groups[(size_t) tuningGroup].rect;
    fineButton.setBounds (tuningRect.getX() + 16, tuningRect.getBottom() - 48,
                          tuningRect.getWidth() - 32, 28);
}
