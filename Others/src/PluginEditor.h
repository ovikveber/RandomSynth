#pragma once

#include "PluginProcessor.h"

#include <array>
#include <functional>

class DarkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

class RandomSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit RandomSynthAudioProcessorEditor (RandomSynthAudioProcessor&);
    ~RandomSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    class ParamSlider : public juce::Component
    {
    public:
        ParamSlider (const juce::String& labelText, double minValue, double maxValue,
                     double initial, std::function<void (double)> changeCallback);
        void resized() override;

        double getValue() const { return slider.getValue(); }
        void setValue (double v) { slider.setValue (v, juce::dontSendNotification); }

        juce::Label label;
        juce::Slider slider;

    private:
        std::function<void (double)> callback;
    };

    void timerCallback() override;
    void setPaused (bool shouldPause);
    void updateButtonColours();
    void setRatio (int index, double value);

    void drawWorld (juce::Graphics&);
    void drawPanel (juce::Graphics&);
    void drawRatioBar (juce::Graphics&);
    juce::Colour kindColour (PropKind kind, float value) const;
    float effectiveNote (const BallVis& ball) const;

    RandomSynthAudioProcessor& processor;
    DarkLookAndFeel lookAndFeel;
    Snapshot view;

    std::array<std::unique_ptr<ParamSlider>, 10> sliders;
    std::array<juce::TextButton, 4> shapeButtons;
    juce::TextButton resetButton;
    juce::TextButton addButton;
    juce::TextButton boosterButton;
    juce::TextButton clearButton;
    juce::TextButton pauseButton;

    double lastFpsTime = 0.0;
    int framesSinceFps = 0;
    float fps = 60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RandomSynthAudioProcessorEditor)
};
