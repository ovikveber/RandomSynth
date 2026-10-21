#include "PluginEditor.h"

#include <cmath>

namespace
{
juce::String u8s (const char* text) { return juce::String::fromUTF8 (text); }

const juce::Colour bgDark (0xff0a0c10);
const juce::Colour panelColour (0xff10141b);
const juce::Colour panelLine (0xff212733);
const juce::Colour accent (0xffffa23a);
const juce::Colour textColour (0xffe8e9ec);
const juce::Colour dimText (0xff8992a1);
const juce::Colour wallColour (0xff343d4c);

constexpr int panelHeight = 230;

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
}

juce::Font DarkLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions()
                           .withName ("Microsoft YaHei")
                           .withHeight (juce::jmin (15.0f, (float) buttonHeight * 0.55f))
                           .withStyle ("Bold"));
}

RandomSynthAudioProcessorEditor::ParamSlider::ParamSlider (const juce::String& labelText,
                                                            double minValue, double maxValue,
                                                            double initial,
                                                            std::function<void (double)> changeCallback)
    : callback (std::move (changeCallback))
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, dimText);
    label.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei")
                                                    .withHeight (12.0f)
                                                    .withStyle ("Bold")));
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 18);
    slider.setRange (minValue, maxValue, 0.0);
    slider.setNumDecimalPlacesToDisplay (2);
    slider.setValue (initial, juce::dontSendNotification);
    slider.setDoubleClickReturnValue (true, initial);
    slider.setColour (juce::Slider::trackColourId, accent);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff252c38));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffdeb0));
    slider.setColour (juce::Slider::textBoxTextColourId, textColour);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1a2029));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.onValueChange = [this]
    {
        if (callback != nullptr)
            callback (slider.getValue());
    };
    addAndMakeVisible (slider);
}

void RandomSynthAudioProcessorEditor::ParamSlider::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (17));
    slider.setBounds (area);
}

RandomSynthAudioProcessorEditor::RandomSynthAudioProcessorEditor (RandomSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setLookAndFeel (&lookAndFeel);
    setSize (1580, 1000);
    setResizable (false, false);
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);

    lookAndFeel.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202734));
    lookAndFeel.setColour (juce::TextButton::buttonOnColourId, accent);
    lookAndFeel.setColour (juce::TextButton::textColourOffId, textColour);
    lookAndFeel.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff191207));
    lookAndFeel.setColour (juce::Slider::textBoxTextColourId, textColour);
    lookAndFeel.setColour (juce::Slider::thumbColourId, accent);

    auto addSlider = [this] (int index, const juce::String& text,
                             double minValue, double maxValue, double initial,
                             std::function<void (double)> onChange)
    {
        sliders[(size_t) index] = std::make_unique<ParamSlider> (text, minValue, maxValue, initial,
                                                                 std::move (onChange));
        addAndMakeVisible (*sliders[(size_t) index]);
    };

    addSlider (0, u8s ("场地大小"), 0.5, 1.0, processor.params.fieldScale.load(),
               [this] (double v) { processor.params.fieldScale.store ((float) v); });
    addSlider (1, u8s ("小球大小"), 0.3, 1.6, processor.params.ballScale.load(),
               [this] (double v) { processor.params.ballScale.store ((float) v); });
    addSlider (2, u8s ("小球速度"), 0.1, 3.0, processor.params.ballSpeed.load(),
               [this] (double v) { processor.params.ballSpeed.store ((float) v); });
    addSlider (3, u8s ("调速范围"), 0.4, 2.5, processor.params.speedShare.load(),
               [this] (double v) { processor.params.speedShare.store ((float) v); });
    addSlider (4, u8s ("调速强度"), 0.1, 3.0, processor.params.speedStrength.load(),
               [this] (double v) { processor.params.speedStrength.store ((float) v); });
    addSlider (5, u8s ("水波速度"), 0.2, 2.0, processor.params.waveSpeed.load(),
               [this] (double v) { processor.params.waveSpeed.store ((float) v); });
    addSlider (6, u8s ("水波范围"), 0.2, 1.5, processor.params.waveRange.load(),
               [this] (double v) { processor.params.waveRange.store ((float) v); });

    const std::array<float, 3> ratios { processor.params.ratioPitch.load(),
                                        processor.params.ratioAdsr.load(),
                                        processor.params.ratioEffect.load() };
    for (int i = 0; i < 3; ++i)
    {
        addSlider (7 + i, i == 0 ? u8s ("音高比例") : (i == 1 ? u8s ("ADSR 比例") : u8s ("效果比例")),
                   0.0, 100.0, ratios[(size_t) i] * 100.0,
                   [this, i] (double v) { setRatio (i, v); });
        sliders[(size_t) (7 + i)]->slider.setTextValueSuffix ("%");
        sliders[(size_t) (7 + i)]->slider.setNumDecimalPlacesToDisplay (0);
    }

    const juce::StringArray shapeLabels { u8s ("圆"), u8s ("矩"), u8s ("椭"), u8s ("六") };
    for (int i = 0; i < 4; ++i)
    {
        shapeButtons[(size_t) i].setButtonText (shapeLabels[i]);
        shapeButtons[(size_t) i].setWantsKeyboardFocus (false);
        shapeButtons[(size_t) i].onClick = [this, i]
        {
            processor.params.shape.store (i);
            updateButtonColours();
            repaint();
            grabKeyboardFocus();
        };
        addAndMakeVisible (shapeButtons[(size_t) i]);
    }

    auto setupAction = [this] (juce::TextButton& button, std::function<void()> action)
    {
        button.setWantsKeyboardFocus (false);
        button.onClick = [this, action = std::move (action)]
        {
            action();
            repaint();
            grabKeyboardFocus();
        };
        addAndMakeVisible (button);
    };

    resetButton.setButtonText (u8s ("重置"));
    addButton.setButtonText (u8s ("+球"));
    boosterButton.setButtonText (u8s ("加速"));
    clearButton.setButtonText (u8s ("清场"));
    pauseButton.setButtonText (u8s ("暂停"));

    setupAction (resetButton, [this] { processor.params.resetRequest.fetch_add (1); });
    setupAction (addButton, [this] { processor.params.addBallsRequest.fetch_add (5); });
    setupAction (boosterButton, [this] { processor.params.addBoosterRequest.fetch_add (1); });
    setupAction (clearButton, [this] { processor.params.clearRequest.fetch_add (1); });
    setupAction (pauseButton, [this] { setPaused (! processor.params.paused.load()); });

    updateButtonColours();
    resized();
    startTimerHz (60);
}

RandomSynthAudioProcessorEditor::~RandomSynthAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RandomSynthAudioProcessorEditor::resized()
{
    if (sliders[0] == nullptr)
        return;

    constexpr int x0 = 46;
    constexpr int row1Y = 810;
    constexpr int row1Gap = 12;
    constexpr int row1Width = (1580 - 92 - 6 * row1Gap) / 7;

    for (int i = 0; i < 7; ++i)
        sliders[(size_t) i]->setBounds (x0 + i * (row1Width + row1Gap), row1Y,
                                        row1Width, 44);

    for (int i = 0; i < 3; ++i)
        sliders[(size_t) (7 + i)]->setBounds (x0 + i * 310, 878, 290, 44);

    for (int i = 0; i < 4; ++i)
        shapeButtons[(size_t) i].setBounds (880 + i * 58, 38, 50, 30);

    resetButton.setBounds (1142, 38, 72, 30);
    addButton.setBounds (1222, 38, 72, 30);
    boosterButton.setBounds (1302, 38, 72, 30);
    clearButton.setBounds (1382, 38, 72, 30);
    pauseButton.setBounds (1462, 38, 72, 30);
}

void RandomSynthAudioProcessorEditor::setPaused (bool shouldPause)
{
    processor.params.paused.store (shouldPause);
    pauseButton.setButtonText (shouldPause ? u8s ("继续") : u8s ("暂停"));
    updateButtonColours();
    repaint();
}

void RandomSynthAudioProcessorEditor::updateButtonColours()
{
    const auto currentShape = processor.params.shape.load();
    for (int i = 0; i < 4; ++i)
    {
        const auto selected = i == currentShape;
        auto& b = shapeButtons[(size_t) i];
        b.setColour (juce::TextButton::buttonColourId,
                     selected ? accent : juce::Colour (0xff202734));
        b.setColour (juce::TextButton::textColourOffId,
                     selected ? juce::Colour (0xff191207) : textColour);
    }

    for (auto* b : { &resetButton, &addButton, &boosterButton, &clearButton })
    {
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202734));
        b->setColour (juce::TextButton::textColourOffId, textColour);
    }

    pauseButton.setColour (juce::TextButton::buttonColourId,
                           processor.params.paused.load() ? accent : juce::Colour (0xff202734));
    pauseButton.setColour (juce::TextButton::textColourOffId,
                           processor.params.paused.load() ? juce::Colour (0xff191207) : textColour);
}

void RandomSynthAudioProcessorEditor::setRatio (int index, double value)
{
    std::array<double, 3> ratios {
        sliders[7]->getValue(),
        sliders[8]->getValue(),
        sliders[9]->getValue()
    };

    ratios[(size_t) index] = juce::jlimit (0.0, 100.0, value);
    const auto remaining = 100.0 - ratios[(size_t) index];
    const auto i1 = (index + 1) % 3;
    const auto i2 = (index + 2) % 3;
    const auto otherSum = ratios[(size_t) i1] + ratios[(size_t) i2];

    if (otherSum <= 1.0e-9)
    {
        ratios[(size_t) i1] = remaining * 0.5;
        ratios[(size_t) i2] = remaining * 0.5;
    }
    else
    {
        ratios[(size_t) i1] *= remaining / otherSum;
        ratios[(size_t) i2] *= remaining / otherSum;
    }

    for (int i = 0; i < 3; ++i)
        sliders[(size_t) (7 + i)]->setValue (ratios[(size_t) i]);

    processor.params.ratioPitch.store ((float) (ratios[0] / 100.0));
    processor.params.ratioAdsr.store ((float) (ratios[1] / 100.0));
    processor.params.ratioEffect.store ((float) (ratios[2] / 100.0));
}

void RandomSynthAudioProcessorEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    ++framesSinceFps;
    if (lastFpsTime > 0.0 && now - lastFpsTime >= 500.0)
    {
        fps = (float) (framesSinceFps * 1000.0 / (now - lastFpsTime));
        framesSinceFps = 0;
        lastFpsTime = now;
    }
    else if (lastFpsTime <= 0.0)
    {
        lastFpsTime = now;
    }

    view = processor.getSnapshot();
    repaint();
}

void RandomSynthAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    if (event.y >= 1000 - panelHeight)
        return;

    processor.params.spawnX.store (event.position.x);
    processor.params.spawnY.store (event.position.y);
    processor.params.spawnKind.store (event.mods.isRightButtonDown() ? 2 : -1);
    processor.params.spawnRequest.store (1);
    grabKeyboardFocus();
}

bool RandomSynthAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    const auto ch = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if (code == juce::KeyPress::spaceKey)
    {
        setPaused (! processor.params.paused.load());
        return true;
    }

    if (ch == 'r')
    {
        processor.params.resetRequest.fetch_add (1);
        return true;
    }
    if (ch == 'n')
    {
        processor.params.addBallsRequest.fetch_add (5);
        return true;
    }
    if (ch == 'b')
    {
        processor.params.addBoosterRequest.fetch_add (1);
        return true;
    }
    if (ch == 'c')
    {
        processor.params.clearRequest.fetch_add (1);
        return true;
    }
    if (ch >= '1' && ch <= '4')
    {
        processor.params.shape.store ((int) (ch - '1'));
        updateButtonColours();
        return true;
    }

    return false;
}

juce::Colour RandomSynthAudioProcessorEditor::kindColour (PropKind kind, float value) const
{
    if (kind == PropKind::pitch) return pitchColour (value);
    if (kind == PropKind::adsr) return adsrColour (value);
    return effectColour (value);
}

float RandomSynthAudioProcessorEditor::effectiveNote (const BallVis& ball) const
{
    auto influence = 0.0f;
    for (const auto& f : view.fields)
    {
        if (f.kind != PropKind::pitch)
            continue;
        const auto d = ball.pos.getDistanceFrom (f.pos);
        if (d < f.radius)
            influence = juce::jmax (influence, f.strength
                                                * std::pow (1.0f - d / f.radius, 1.6f));
    }
    return juce::jlimit (12.0f, 108.0f, ball.note - influence * 18.0f);
}

void RandomSynthAudioProcessorEditor::drawWorld (juce::Graphics& g)
{
    juce::Path boundary;
    if (! view.segments.empty())
    {
        boundary.startNewSubPath (view.segments.front().a);
        for (const auto& s : view.segments)
            boundary.lineTo (s.b);
        boundary.closeSubPath();
    }

    g.saveState();
    if (! view.segments.empty())
        g.reduceClipRegion (boundary);

    for (const auto& f : view.fields)
    {
        const auto c = kindColour (f.kind, f.value);
        const auto alpha = juce::jlimit (0.0f, 1.0f, f.strength) * 0.30f;
        juce::ColourGradient grad (c.withAlpha (alpha), f.pos,
                                   c.withAlpha (0.0f), f.pos.translated (f.radius, 0.0f),
                                   true);
        g.setGradientFill (grad);
        g.fillEllipse (f.pos.x - f.radius, f.pos.y - f.radius,
                       f.radius * 2.0f, f.radius * 2.0f);
    }

    for (const auto& w : view.waves)
    {
        const auto c = kindColour (w.kind, 0.7f);
        const auto a = juce::jlimit (0.0f, 1.0f, w.amp);
        g.setColour (c.withAlpha (0.45f * a));
        g.drawEllipse (w.pos.x - w.radius, w.pos.y - w.radius,
                       w.radius * 2.0f, w.radius * 2.0f, 2.0f);

        if (w.kind == PropKind::effect)
        {
            g.setColour (c.withAlpha (0.22f * a));
            g.drawEllipse (w.pos.x - w.radius + 14.0f, w.pos.y - w.radius + 14.0f,
                           (w.radius - 14.0f) * 2.0f, (w.radius - 14.0f) * 2.0f, 1.5f);
            g.setColour (c.withAlpha (0.12f * a));
            g.drawEllipse (w.pos.x - w.radius + 28.0f, w.pos.y - w.radius + 28.0f,
                           (w.radius - 28.0f) * 2.0f, (w.radius - 28.0f) * 2.0f, 1.0f);
        }
    }

    for (const auto& l : view.links)
    {
        g.setColour (juce::Colour (0xff91d4ff).withAlpha (0.38f + 0.55f * l.amount));
        g.drawLine (l.a.x, l.a.y, l.b.x, l.b.y, 1.6f);
    }

    g.restoreState();

    for (const auto& s : view.segments)
    {
        auto col = wallColour;
        if (s.hasProp && s.strength > 0.02f)
        {
            const auto pc = kindColour (s.kind, s.value);
            col = wallColour.interpolatedWith (pc, 0.25f + 0.75f * juce::jlimit (0.0f, 1.0f, s.strength));
        }
        g.setColour (col);
        g.drawLine (s.a.x, s.a.y, s.b.x, s.b.y, s.strength > 0.25f ? 3.0f : 2.0f);
    }

    for (const auto& b : view.boosters)
    {
        const auto pulse = 0.5f + 0.5f * std::sin (view.elapsed * 4.0f + b.phase);
        const auto col = b.cooldown > 0.0f ? juce::Colour (0xff968e68) : juce::Colour (0xffffe278);
        g.setColour (col);
        g.drawEllipse (b.pos.x - b.radius - 1.0f - pulse * 2.0f,
                       b.pos.y - b.radius - 1.0f - pulse * 2.0f,
                       (b.radius + 1.0f + pulse * 2.0f) * 2.0f,
                       (b.radius + 1.0f + pulse * 2.0f) * 2.0f, 2.0f);

        for (int k = 0; k < 4; ++k)
        {
            const auto ang = view.elapsed * 0.8f + (float) k * juce::MathConstants<float>::halfPi;
            const auto p1 = b.pos + juce::Point<float> (std::cos (ang), std::sin (ang)) * (b.radius + 4.0f);
            const auto p2 = b.pos + juce::Point<float> (std::cos (ang), std::sin (ang)) * (b.radius + 10.0f + pulse * 3.0f);
            g.drawLine (p1.x, p1.y, p2.x, p2.y, 2.0f);
        }
        g.fillEllipse (b.pos.x - 3.0f, b.pos.y - 3.0f, 6.0f, 6.0f);
    }

    const auto mouse = getMouseXYRelative().toFloat();

    for (const auto& b : view.balls)
    {
        const auto c = kindColour (b.kind, b.value);
        const auto glowRadius = b.radius * 3.2f;
        juce::ColourGradient glow (c.withAlpha (0.22f), b.pos,
                                    c.withAlpha (0.0f), b.pos.translated (glowRadius, 0.0f),
                                    true);
        g.setGradientFill (glow);
        g.fillEllipse (b.pos.x - glowRadius, b.pos.y - glowRadius,
                       glowRadius * 2.0f, glowRadius * 2.0f);

        g.setColour (juce::Colour (0xff080a0e));
        g.fillEllipse (b.pos.x - b.radius - 2.0f, b.pos.y - b.radius - 2.0f,
                       (b.radius + 2.0f) * 2.0f, (b.radius + 2.0f) * 2.0f);

        auto core = b.flash > 0.0f ? c.brighter (b.flash * 3.0f) : c;
        g.setColour (core);
        g.fillEllipse (b.pos.x - b.radius, b.pos.y - b.radius,
                       b.radius * 2.0f, b.radius * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawEllipse (b.pos.x - b.radius, b.pos.y - b.radius,
                       b.radius * 2.0f, b.radius * 2.0f, 1.0f);

        if (b.kind == PropKind::effect)
        {
            const auto pulse = b.radius + 5.0f + 3.0f * (0.5f + 0.5f * std::sin (view.elapsed * 5.0f + b.phase));
            g.setColour (c.withAlpha (0.35f));
            g.drawEllipse (b.pos.x - pulse, b.pos.y - pulse, pulse * 2.0f, pulse * 2.0f, 1.0f);
        }

        const auto hover = b.pos.getDistanceFrom (mouse) < juce::jmax (18.0f, b.radius + 14.0f);
        if (b.radius < 5.2f && ! hover)
            continue;

        juce::String text;
        if (b.kind == PropKind::pitch)
            text = juce::MidiMessage::getMidiNoteName ((int) std::lround (effectiveNote (b)), true, true, 4);
        else if (b.kind == PropKind::adsr)
            text = "ADSR";
        else
            text = "REV";

        const auto font = juce::Font (juce::FontOptions().withName ("Microsoft YaHei")
                                                          .withHeight (11.0f)
                                                          .withStyle ("Bold"));
        g.setFont (font);
        const auto textWidth = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, text)) + 6;
        const juce::Rectangle<float> labelRect (b.pos.x - textWidth * 0.5f,
                                                b.pos.y - b.radius - 16.0f,
                                                (float) textWidth, 15.0f);
        g.setColour (c.withAlpha (0.92f));
        g.fillRoundedRectangle (labelRect, 3.5f);
        g.setColour (juce::Colour (0xff0a0c10));
        g.drawText (text, labelRect, juce::Justification::centred, false);
    }
}

void RandomSynthAudioProcessorEditor::drawRatioBar (juce::Graphics& g)
{
    const float x = 990.0f;
    const float y = 878.0f;
    const float width = 544.0f;
    const float barX = x + 38.0f;
    const float barW = width - 38.0f;

    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei")
                                                .withHeight (13.0f)
                                                .withStyle ("Bold")));
    g.setColour (textColour);
    g.drawText (u8s ("球类型比例"), (int) x, (int) y, 200, 16, juce::Justification::left, false);

    const auto total = juce::jmax (1, view.balls.empty() ? 0 : (int) view.balls.size());
    std::array<float, 3> current {
        (float) view.counts[0] / (float) total,
        (float) view.counts[1] / (float) total,
        (float) view.counts[2] / (float) total
    };

    std::array<float, 3> target {
        processor.params.ratioPitch.load(),
        processor.params.ratioAdsr.load(),
        processor.params.ratioEffect.load()
    };
    const auto sum = target[0] + target[1] + target[2];
    if (sum > 1.0e-9f)
        for (auto& v : target)
            v /= sum;
    else
        target = { 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f };

    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei").withHeight (11.0f)));
    const juce::String labels[2] { u8s ("当前"), u8s ("目标") };
    const std::array<std::array<float, 3>, 2> bars { current, target };

    for (int row = 0; row < 2; ++row)
    {
        const auto yy = y + 20.0f + row * 18.0f;
        g.setColour (dimText);
        g.drawText (labels[row], (int) x, (int) yy - 2, 34, 12, juce::Justification::left, false);

        auto xx = barX;
        for (int i = 0; i < 3; ++i)
        {
            const auto seg = barW * bars[(size_t) row][(size_t) i];
            g.setColour (kindColour ((PropKind) i, 0.8f));
            g.fillRect (xx, yy, seg, 8.0f);
            xx += seg;
        }
        g.setColour (juce::Colour (0xff2e3644));
        g.drawRect (barX, yy, barW, 8.0f, 1.0f);
    }

    const auto cur = juce::String (juce::roundToInt (current[0] * 100.0f)) + "% / "
                   + juce::String (juce::roundToInt (current[1] * 100.0f)) + "% / "
                   + juce::String (juce::roundToInt (current[2] * 100.0f)) + "%";
    const auto tgt = u8s ("目标 ") + juce::String (juce::roundToInt (target[0] * 100.0f)) + "% / "
                   + juce::String (juce::roundToInt (target[1] * 100.0f)) + "% / "
                   + juce::String (juce::roundToInt (target[2] * 100.0f)) + "%";
    g.setColour (dimText);
    g.drawText (cur + "    " + tgt, (int) barX, (int) (y + 51.0f), 500, 14,
                juce::Justification::left, false);
}

void RandomSynthAudioProcessorEditor::drawPanel (juce::Graphics& g)
{
    const auto panelY = 1000 - panelHeight;
    g.setColour (panelColour);
    g.fillRect (0, panelY, 1580, panelHeight);
    g.setColour (panelLine);
    g.drawLine (0.0f, (float) panelY, 1580.0f, (float) panelY, 1.0f);

    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei")
                                                .withHeight (30.0f)
                                                .withStyle ("Bold")));
    g.setColour (textColour);
    g.drawText (u8s ("随机合成器 · JUCE 桌面版"), 46, 22, 700, 40, juce::Justification::left, false);

    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei").withHeight (14.0f)));
    g.setColour (dimText);
    g.drawText (u8s ("碰撞 → 性质交换 → 辐射场 → 正弦发声 + 声像 + 混响"),
                46, 68, 800, 20, juce::Justification::left, false);

    const juce::String shapeNames[4] { u8s ("圆形"), u8s ("矩形"), u8s ("椭圆"), u8s ("六边形") };
    const auto info = shapeNames[juce::jlimit (0, 3, view.shape)]
                    + "   BALLS " + juce::String ((int) view.balls.size()).paddedLeft ('0', 2)
                    + "   BOOSTS " + juce::String ((int) view.boosters.size())
                    + "   FIELDS " + juce::String ((int) view.fields.size()).paddedLeft ('0', 2)
                    + "   WAVES " + juce::String ((int) view.waves.size()).paddedLeft ('0', 2)
                    + "   SPD " + juce::String (juce::roundToInt (view.averageSpeed)) + "/280"
                    + "   FPS " + juce::String (juce::roundToInt (fps));
    g.drawText (info, 800, 76, 734, 20, juce::Justification::right, false);

    const float legendY = (float) panelY + 12.0f;
    const juce::String legendNames[3] { u8s ("音高球"), u8s ("ADSR 球"), u8s ("效果球") };
    float lx = 46.0f;
    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei")
                                                .withHeight (14.0f)
                                                .withStyle ("Bold")));
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (kindColour ((PropKind) i, 0.8f));
        g.fillEllipse (lx, legendY + 2.0f, 12.0f, 12.0f);
        g.setColour (textColour);
        g.drawText (legendNames[i], (int) lx + 18, (int) legendY, 90, 16,
                    juce::Justification::left, false);
        lx += 130.0f;
    }

    drawRatioBar (g);

    g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei").withHeight (14.0f)));
    g.setColour (dimText);
    g.drawText (u8s ("SPACE 暂停    R 重置    N 加 5 球    B 加加速器    左键加球    右键加效果球"),
                46, panelY + 178, 820, 18, juce::Justification::left, false);
    g.drawText (u8s ("1-4 形状    C 清场    比例自动柔和配平    快球会把速度分给范围内慢球"),
                46, panelY + 202, 900, 18, juce::Justification::left, false);

    const juce::String tip1 = u8s ("撞墙交换性质");
    const juce::String tip2 = u8s ("正弦波 + 等功率声像 + 发送混响");
    g.drawText (tip1, 1150, panelY + 178, 384, 18, juce::Justification::right, false);
    g.drawText (tip2, 1150, panelY + 202, 384, 18, juce::Justification::right, false);
}

void RandomSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (juce::Colour (0xff1a1f28), 0.0f, 0.0f,
                                     juce::Colour (0xff0b0d11), 1580.0f, 1000.0f,
                                     false);
    g.setGradientFill (background);
    g.fillAll();

    drawWorld (g);
    drawPanel (g);

    if (processor.params.paused.load())
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (0, 0, 1580, 770);

        g.setColour (textColour);
        g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei")
                                                    .withHeight (46.0f)
                                                    .withStyle ("Bold")));
        g.drawText ("PAUSED", 0, 330, 1580, 60, juce::Justification::centred, false);
        g.setFont (juce::Font (juce::FontOptions().withName ("Microsoft YaHei").withHeight (15.0f)));
        g.setColour (dimText);
        g.drawText (u8s ("按 SPACE 继续"), 0, 392, 1580, 24, juce::Justification::centred, false);
    }
}
