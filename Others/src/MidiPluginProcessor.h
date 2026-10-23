#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

#include "World.h"

namespace RandomSynthMidiParamIDs
{
inline constexpr const char* field       = "field";
inline constexpr const char* ball        = "ball";
inline constexpr const char* speed       = "speed";
inline constexpr const char* speedShare  = "speedShare";
inline constexpr const char* strength    = "strength";
inline constexpr const char* waveSpeed   = "waveSpeed";
inline constexpr const char* waveRange   = "waveRange";
inline constexpr const char* shape       = "shape";
inline constexpr const char* ballCount   = "ballCount";
inline constexpr const char* boosterCount = "boosterCount";

inline constexpr const char* scale       = "scale";
inline constexpr const char* range       = "range";
inline constexpr const char* fine        = "fine";

inline constexpr const char* ratioPitch  = "ratioPitch";
inline constexpr const char* ratioAdsr   = "ratioAdsr";
inline constexpr const char* ratioEffect = "ratioEffect";

inline constexpr const char* gate        = "gate";
inline constexpr const char* gateMin     = "gateMin";
inline constexpr const char* gateMax     = "gateMax";

inline constexpr const char* midiChannel = "midiChannel";
}

// Pure MIDI generator. It runs the same particle World as the audio version,
// but converts collision events into MIDI note-on/note-off messages instead of
// rendering audio.
//
// Integration hook:
//   World::getEvents() returns SoundEvent objects containing note, velocity,
//   gateSeconds, pan, send (FX/reverb CV), waveform CV and ADSR values.
//   A downstream synth can consume those fields directly, or map them to macros.
//   The VST3 emits notes on the channel selected by the MIDI CH control;
//   optional macro CCs can be enabled with outputMacroCc below.
class RandomSynthMidiAudioProcessor : public juce::AudioProcessor,
                                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    RandomSynthMidiAudioProcessor();
    ~RandomSynthMidiAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RandomSynth MIDI"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void setRunning (bool shouldRun);
    bool isRunning() const { return running.load(); }
    void resetWorld() { resetRequested.store (true); }

    int getScaleIndex() const;
    int getShapeIndex() const;
    int getMidiChannel() const;
    bool isFineEnabled() const;
    float getRangeValue() const;

    int getLastNote() const { return uiLastNote.load(); }
    bool isGateActive() const { return uiGateActive.load(); }
    bool isTrigActive() const { return uiTrigActive.load(); }
    float getLastPan() const { return uiLastPan.load(); }
    float getLastSend() const { return uiLastSend.load(); }

    Snapshot getSnapshot();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Set this to true in your integration if you want the macro values sent
    // as MIDI CC in addition to being available in SoundEvent.
    static constexpr bool outputMacroCc = false;
    static constexpr int macroWaveformCc = 74;
    static constexpr int macroReverbCc = 91;
    static constexpr int macroPanCc = 10;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct ActiveNote
    {
        int note = 60;
        int channel = 1;
        int samplesRemaining = 0;
    };

    struct ParamPointers
    {
        std::atomic<float>* field = nullptr;
        std::atomic<float>* ball = nullptr;
        std::atomic<float>* speed = nullptr;
        std::atomic<float>* speedShare = nullptr;
        std::atomic<float>* strength = nullptr;
        std::atomic<float>* waveSpeed = nullptr;
        std::atomic<float>* waveRange = nullptr;
        std::atomic<float>* shape = nullptr;
        std::atomic<float>* ballCount = nullptr;
        std::atomic<float>* boosterCount = nullptr;

        std::atomic<float>* scale = nullptr;
        std::atomic<float>* range = nullptr;
        std::atomic<float>* fine = nullptr;

        std::atomic<float>* ratioPitch = nullptr;
        std::atomic<float>* ratioAdsr = nullptr;
        std::atomic<float>* ratioEffect = nullptr;

        std::atomic<float>* gate = nullptr;
        std::atomic<float>* gateMin = nullptr;
        std::atomic<float>* gateMax = nullptr;

        std::atomic<float>* midiChannel = nullptr;
    };

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void applyParameters();
    void advanceActiveNotes (juce::MidiBuffer& midi, int blockOffset, int numSamples);
    void triggerNote (const SoundEvent& event, juce::MidiBuffer& midi,
                      int blockOffset, int chunkSamples);
    void publishSnapshot();

    static juce::String scaleLabel (int index);

    World world;
    ParamPointers params;
    std::vector<ActiveNote> activeNotes;
    std::atomic<bool> resetRequested { false };
    std::atomic<bool> stopRequested { false };
    std::atomic<bool> running { true };
    int currentMidiChannel = 1;
    double currentSampleRate = 44100.0;

    std::atomic<int> uiLastNote { 60 };
    std::atomic<bool> uiGateActive { false };
    std::atomic<bool> uiTrigActive { false };
    std::atomic<float> uiLastPan { 0.0f };
    std::atomic<float> uiLastSend { 0.0f };
    std::atomic<int> uiGateSamples { 0 };
    std::atomic<int> uiTrigSamples { 0 };
    bool snappingRange = false;

    juce::CriticalSection snapshotLock;
    Snapshot snapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RandomSynthMidiAudioProcessor)
};
