#pragma once

#include "MidiPluginProcessor.h"

#include <array>
#include <memory>
#include <vector>

class MidiLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MidiLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

class RandomSynthMidiAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit RandomSynthMidiAudioProcessorEditor (RandomSynthMidiAudioProcessor&);
    ~RandomSynthMidiAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Knob : public juce::Component
    {
    public:
        explicit Knob (const juce::String& labelText, int textBoxWidth = 70);

        void resized() override;

        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    enum GroupIndex { worldGroup = 0, tuningGroup, voiceGroup, gateGroup, groupCount };

    void timerCallback() override;
    void addKnob (int group, const juce::String& label, const juce::String& parameterID,
                  int textBoxWidth = 70);
    void updateTransportColours();
    void updateRangeSnap();

    void drawWorld (juce::Graphics&);
    void drawStatusBar (juce::Graphics&);
    void drawPanels (juce::Graphics&);
    juce::Colour kindColour (PropKind kind, float value) const;

    RandomSynthMidiAudioProcessor& processor;
    MidiLookAndFeel lookAndFeel;
    Snapshot view;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::array<std::vector<Knob*>, groupCount> groupKnobs;
    Knob* rangeKnob = nullptr;

    juce::ComboBox midiChannelBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiChannelAttachment;

    juce::ToggleButton fineButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> fineAttachment;

    juce::TextButton runButton;
    juce::TextButton stopButton;
    juce::TextButton resetButton;

    juce::Rectangle<int> worldRect;
    juce::Rectangle<int> statusRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RandomSynthMidiAudioProcessorEditor)
};
