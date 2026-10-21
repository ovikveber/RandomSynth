#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <memory>

#include "World.h"

struct SharedParams
{
    std::atomic<float> fieldScale { 1.0f };
    std::atomic<float> ballScale { 0.55f };
    std::atomic<float> ballSpeed { 1.0f };
    std::atomic<float> speedShare { 1.0f };
    std::atomic<float> speedStrength { 1.0f };
    std::atomic<float> waveSpeed { 0.65f };
    std::atomic<float> waveRange { 0.50f };
    std::atomic<float> ratioPitch { 0.50f };
    std::atomic<float> ratioAdsr { 0.25f };
    std::atomic<float> ratioEffect { 0.25f };

    std::atomic<int> shape { 0 };
    std::atomic<float> spawnX { -1.0f };
    std::atomic<float> spawnY { -1.0f };
    std::atomic<int> spawnKind { -1 };
    std::atomic<int> spawnRequest { 0 };
    std::atomic<int> resetRequest { 0 };
    std::atomic<int> addBallsRequest { 0 };
    std::atomic<int> addBoosterRequest { 0 };
    std::atomic<int> clearRequest { 0 };
    std::atomic<bool> paused { false };
};

class RandomSynthAudioProcessor : public juce::AudioProcessor
{
public:
    RandomSynthAudioProcessor();
    ~RandomSynthAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RandomSynth"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    Snapshot getSnapshot();
    SharedParams params;

private:
    class SimThread : public juce::Thread
    {
    public:
        explicit SimThread (RandomSynthAudioProcessor& p)
            : juce::Thread ("RandomSynthSim"), processor (p) {}

        void run() override;

    private:
        RandomSynthAudioProcessor& processor;
    };

    struct Voice
    {
        bool active = false;
        double phase = 0.0;
        double increment = 0.0;
        juce::ADSR envelope;
        int holdSamples = 0;
        float gain = 0.0f;
        float panL = 0.707f;
        float panR = 0.707f;
        float send = 0.0f;
        juce::uint32 order = 0;
    };

    void stepSimulation (float dt);
    void publishSnapshot();
    void startVoice (const SoundEvent& ev);
    void applySharedParams();

    World world;
    juce::CriticalSection snapshotLock;
    Snapshot snapshot;

    std::unique_ptr<SimThread> simThread;
    juce::uint32 voiceOrder = 0;

    std::array<Voice, 48> voices;
    juce::Reverb reverb;
    juce::AudioBuffer<float> reverbBus;

    static constexpr int eventCapacity = 2048;
    juce::AbstractFifo eventFifo { eventCapacity };
    std::array<SoundEvent, eventCapacity> eventStorage;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RandomSynthAudioProcessor)
};
