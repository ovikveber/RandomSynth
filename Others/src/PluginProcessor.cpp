#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <limits>

RandomSynthAudioProcessor::RandomSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    simThread = std::make_unique<SimThread> (*this);
    simThread->startThread();
}

RandomSynthAudioProcessor::~RandomSynthAudioProcessor()
{
    if (simThread != nullptr)
    {
        simThread->signalThreadShouldExit();
        simThread->stopThread (2000);
        simThread.reset();
    }
}

void RandomSynthAudioProcessor::SimThread::run()
{
    auto last = juce::Time::getMillisecondCounterHiRes() * 0.001;

    while (! threadShouldExit())
    {
        const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const auto dt = juce::jlimit (0.0, 0.05, now - last);
        last = now;

        processor.stepSimulation ((float) dt);
        wait (7);
    }
}

void RandomSynthAudioProcessor::applySharedParams()
{
    auto s = world.getSettings();
    s.ballScale = params.ballScale.load();
    s.ballSpeed = params.ballSpeed.load();
    s.speedShare = params.speedShare.load();
    s.speedStrength = params.speedStrength.load();
    s.waveSpeed = params.waveSpeed.load();
    s.waveRange = params.waveRange.load();
    s.ratio = { params.ratioPitch.load(), params.ratioAdsr.load(), params.ratioEffect.load() };

    if (std::abs (params.ballScale.load() - world.getSettings().ballScale) > 1.0e-4f)
        world.setBallScale (params.ballScale.load());

    world.setSettings (s);

    const auto shape = params.shape.load();
    if (shape != world.shape)
        world.setShape (shape);

    const auto scale = params.fieldScale.load();
    if (std::abs (scale - world.fieldScale) > 1.0e-4f)
        world.setFieldScale (scale);
}

void RandomSynthAudioProcessor::stepSimulation (float dt)
{
    applySharedParams();

    if (params.resetRequest.exchange (0) > 0)
        world.reset();

    if (params.spawnRequest.exchange (0) > 0)
        world.addBallAt ({ params.spawnX.load(), params.spawnY.load() }, params.spawnKind.load());

    if (auto count = params.addBallsRequest.exchange (0); count > 0)
        for (int i = 0; i < count; ++i)
            world.addBall();

    if (auto count = params.addBoosterRequest.exchange (0); count > 0)
        for (int i = 0; i < count; ++i)
            world.addBooster();

    if (params.clearRequest.exchange (0) > 0)
        world.clearEffects();

    world.paused = params.paused.load();
    world.update (dt);

    const auto& pending = world.getEvents();
    const auto total = juce::jmin ((int) pending.size(), eventCapacity);
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    eventFifo.prepareToWrite (total, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i)
        eventStorage[start1 + i] = pending[(size_t) i];
    for (int i = 0; i < size2; ++i)
        eventStorage[start2 + i] = pending[(size_t) (size1 + i)];

    eventFifo.finishedWrite (size1 + size2);

    publishSnapshot();
}

void RandomSynthAudioProcessor::publishSnapshot()
{
    const juce::ScopedLock sl (snapshotLock);
    world.fillSnapshot (snapshot);
}

Snapshot RandomSynthAudioProcessor::getSnapshot()
{
    const juce::ScopedLock sl (snapshotLock);
    return snapshot;
}

void RandomSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (auto& v : voices)
    {
        v = Voice();
        v.envelope.setSampleRate (sampleRate);
    }

    reverb.setSampleRate (sampleRate);
    juce::Reverb::Parameters p;
    p.roomSize = 0.78f;
    p.damping = 0.45f;
    p.wetLevel = 1.0f;
    p.dryLevel = 0.0f;
    p.width = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);

    reverbBus.setSize (2, juce::jmax (512, samplesPerBlock), false, false, true);
    reverbBus.clear();
}

void RandomSynthAudioProcessor::releaseResources()
{
    for (auto& v : voices)
        v.active = false;
}

bool RandomSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled();
}

void RandomSynthAudioProcessor::startVoice (const SoundEvent& ev)
{
    Voice* target = nullptr;
    for (auto& v : voices)
    {
        if (! v.active)
        {
            target = &v;
            break;
        }
    }

    if (target == nullptr)
    {
        juce::uint32 oldest = std::numeric_limits<juce::uint32>::max();
        for (auto& v : voices)
        {
            if (v.order < oldest)
            {
                oldest = v.order;
                target = &v;
            }
        }
    }

    if (target == nullptr)
        return;

    auto& v = *target;
    v.envelope.setSampleRate (currentSampleRate);
    v.envelope.setParameters ({ ev.attack, ev.decay, ev.sustain, ev.release });
    v.envelope.reset();
    v.envelope.noteOn();

    v.active = true;
    v.phase = 0.0;
    v.increment = juce::MathConstants<double>::twoPi
                * juce::MidiMessage::getMidiNoteInHertz (ev.note)
                / currentSampleRate;
    v.holdSamples = (int) (0.06 * currentSampleRate);

    const auto angle = (ev.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    v.panL = std::cos (angle);
    v.panR = std::sin (angle);
    v.gain = ev.velocity;
    v.send = ev.send;
    v.order = ++voiceOrder;
}

void RandomSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);
    buffer.clear();

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    const auto ready = eventFifo.getNumReady();
    if (ready > 0)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        eventFifo.prepareToRead (ready, start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i)
            startVoice (eventStorage[start1 + i]);
        for (int i = 0; i < size2; ++i)
            startVoice (eventStorage[start2 + i]);

        eventFifo.finishedRead (size1 + size2);
    }

    if (reverbBus.getNumSamples() < numSamples || reverbBus.getNumChannels() < 2)
        reverbBus.setSize (2, numSamples, false, false, true);
    reverbBus.clear();

    auto* outL = buffer.getWritePointer (0);
    auto* outR = numChannels > 1 ? buffer.getWritePointer (1) : outL;
    auto* revL = reverbBus.getWritePointer (0);
    auto* revR = reverbBus.getWritePointer (1);

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        for (int i = 0; i < numSamples; ++i)
        {
            if (v.holdSamples > 0)
            {
                --v.holdSamples;
                if (v.holdSamples == 0)
                    v.envelope.noteOff();
            }

            const auto env = v.envelope.getNextSample();
            const auto s = (float) std::sin (v.phase) * env * v.gain * 0.16f;

            outL[i] += s * v.panL;
            if (numChannels > 1)
                outR[i] += s * v.panR;

            revL[i] += s * v.send;
            revR[i] += s * v.send;

            v.phase += v.increment;
            if (v.phase >= juce::MathConstants<double>::twoPi)
                v.phase -= juce::MathConstants<double>::twoPi;
        }

        if (! v.envelope.isActive())
            v.active = false;
    }

    reverb.processStereo (revL, revR, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = std::tanh ((outL[i] + revL[i] * 0.85f) * 0.5f);
        if (numChannels > 1)
            outR[i] = std::tanh ((outR[i] + revR[i] * 0.85f) * 0.5f);
    }
}

juce::AudioProcessorEditor* RandomSynthAudioProcessor::createEditor()
{
    return new RandomSynthAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RandomSynthAudioProcessor();
}
