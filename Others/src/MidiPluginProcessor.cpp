#include "MidiPluginProcessor.h"
#include "MidiPluginEditor.h"

#include <cmath>
#include <limits>

namespace
{
juce::String u8s (const char* text) { return juce::String::fromUTF8 (text); }

juce::NormalisableRange<float> steppedRange (float minValue, float maxValue, float step)
{
    return { minValue, maxValue, step };
}
}

RandomSynthMidiAudioProcessor::RandomSynthMidiAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    params.field       = apvts.getRawParameterValue (RandomSynthMidiParamIDs::field);
    params.ball        = apvts.getRawParameterValue (RandomSynthMidiParamIDs::ball);
    params.speed       = apvts.getRawParameterValue (RandomSynthMidiParamIDs::speed);
    params.speedShare  = apvts.getRawParameterValue (RandomSynthMidiParamIDs::speedShare);
    params.strength    = apvts.getRawParameterValue (RandomSynthMidiParamIDs::strength);
    params.waveSpeed   = apvts.getRawParameterValue (RandomSynthMidiParamIDs::waveSpeed);
    params.waveRange   = apvts.getRawParameterValue (RandomSynthMidiParamIDs::waveRange);
    params.shape       = apvts.getRawParameterValue (RandomSynthMidiParamIDs::shape);
    params.ballCount   = apvts.getRawParameterValue (RandomSynthMidiParamIDs::ballCount);
    params.boosterCount = apvts.getRawParameterValue (RandomSynthMidiParamIDs::boosterCount);

    params.scale       = apvts.getRawParameterValue (RandomSynthMidiParamIDs::scale);
    params.range       = apvts.getRawParameterValue (RandomSynthMidiParamIDs::range);
    params.fine        = apvts.getRawParameterValue (RandomSynthMidiParamIDs::fine);

    params.ratioPitch  = apvts.getRawParameterValue (RandomSynthMidiParamIDs::ratioPitch);
    params.ratioAdsr   = apvts.getRawParameterValue (RandomSynthMidiParamIDs::ratioAdsr);
    params.ratioEffect = apvts.getRawParameterValue (RandomSynthMidiParamIDs::ratioEffect);

    params.gate        = apvts.getRawParameterValue (RandomSynthMidiParamIDs::gate);
    params.gateMin     = apvts.getRawParameterValue (RandomSynthMidiParamIDs::gateMin);
    params.gateMax     = apvts.getRawParameterValue (RandomSynthMidiParamIDs::gateMax);

    params.midiChannel = apvts.getRawParameterValue (RandomSynthMidiParamIDs::midiChannel);

    apvts.addParameterListener (RandomSynthMidiParamIDs::range, this);
    apvts.addParameterListener (RandomSynthMidiParamIDs::fine, this);
}

RandomSynthMidiAudioProcessor::~RandomSynthMidiAudioProcessor()
{
    apvts.removeParameterListener (RandomSynthMidiParamIDs::range, this);
    apvts.removeParameterListener (RandomSynthMidiParamIDs::fine, this);
}

juce::String RandomSynthMidiAudioProcessor::scaleLabel (int index)
{
    return juce::String::fromUTF8 (scaleName (index));
}

int RandomSynthMidiAudioProcessor::getScaleIndex() const
{
    if (auto* parameter = apvts.getParameter (RandomSynthMidiParamIDs::scale))
        return juce::jlimit (0, (int) ScaleMode::Count - 1,
                             (int) std::lround (parameter->convertFrom0to1 (parameter->getValue())));

    return 0;
}

int RandomSynthMidiAudioProcessor::getShapeIndex() const
{
    if (auto* parameter = apvts.getParameter (RandomSynthMidiParamIDs::shape))
        return juce::jlimit (0, 3,
                             (int) std::lround (parameter->convertFrom0to1 (parameter->getValue())));

    return 0;
}

int RandomSynthMidiAudioProcessor::getMidiChannel() const
{
    if (auto* parameter = apvts.getParameter (RandomSynthMidiParamIDs::midiChannel))
        return juce::jlimit (1, 16,
                             (int) std::lround (parameter->convertFrom0to1 (parameter->getValue())) + 1);

    return 1;
}

bool RandomSynthMidiAudioProcessor::isFineEnabled() const
{
    if (auto* parameter = apvts.getParameter (RandomSynthMidiParamIDs::fine))
        return parameter->getValue() >= 0.5f;

    return false;
}

float RandomSynthMidiAudioProcessor::getRangeValue() const
{
    if (auto* parameter = apvts.getParameter (RandomSynthMidiParamIDs::range))
        return parameter->convertFrom0to1 (parameter->getValue()) * 0.5f;

    return 4.0f;
}

void RandomSynthMidiAudioProcessor::parameterChanged (const juce::String& parameterID,
                                                       float)
{
    if (snappingRange)
        return;

    if ((parameterID == RandomSynthMidiParamIDs::range
         || parameterID == RandomSynthMidiParamIDs::fine)
        && ! isFineEnabled())
    {
        if (auto* parameter = apvts.getParameter (RandomSynthMidiParamIDs::range))
        {
            const auto halfSteps = parameter->convertFrom0to1 (parameter->getValue());
            const auto snapped = std::round (halfSteps);

            if (std::abs (halfSteps - snapped) > 1.0e-4f)
            {
                const juce::ScopedValueSetter<bool> guard (snappingRange, true);
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (snapped));
            }
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
RandomSynthMidiAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const juce::String& name,
                               float minValue, float maxValue, float step,
                               float defaultValue, const juce::String& suffix = {},
                               float displayMultiplier = 1.0f)
    {
        auto formatValue = [suffix, displayMultiplier] (float value, int maxLength)
        {
            auto text = juce::String (value * displayMultiplier, 3);

            if (text.containsChar ('.'))
            {
                while (text.endsWithChar ('0'))
                    text = text.dropLastCharacters (1);
                if (text.endsWithChar ('.'))
                    text = text.dropLastCharacters (1);
            }

            if (suffix.isNotEmpty())
                text += suffix.startsWithChar ('%') ? suffix : " " + suffix.trim();

            return maxLength > 0 ? text.substring (0, maxLength) : text;
        };

        auto attributes = juce::AudioParameterFloatAttributes()
                              .withLabel (suffix)
                              .withStringFromValueFunction (formatValue);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            steppedRange (minValue, maxValue, step),
            defaultValue, attributes));
    };

    addFloat (RandomSynthMidiParamIDs::field,       u8s ("场地大小"), 0.5f, 1.0f, 0.001f, 1.0f);
    addFloat (RandomSynthMidiParamIDs::ball,        u8s ("小球大小"), 0.3f, 1.6f, 0.001f, 0.55f);
    addFloat (RandomSynthMidiParamIDs::speed,       u8s ("小球速度"), 0.1f, 3.0f, 0.001f, 1.0f);
    addFloat (RandomSynthMidiParamIDs::speedShare,  u8s ("调速范围"), 0.4f, 2.5f, 0.001f, 1.0f);
    addFloat (RandomSynthMidiParamIDs::strength,    u8s ("调速强度"), 0.1f, 3.0f, 0.001f, 1.0f);
    addFloat (RandomSynthMidiParamIDs::waveSpeed,   u8s ("水波速度"), 0.2f, 2.0f, 0.001f, 0.65f);
    addFloat (RandomSynthMidiParamIDs::waveRange,   u8s ("水波范围"), 0.2f, 1.5f, 0.001f, 0.5f);
    addFloat (RandomSynthMidiParamIDs::ballCount,   u8s ("小球数量"), 1.0f, 32.0f, 1.0f, 12.0f);
    addFloat (RandomSynthMidiParamIDs::boosterCount, u8s ("加速器数量"), 0.0f, 5.0f, 1.0f, 2.0f);

    juce::StringArray shapeChoices { u8s ("圆形"), u8s ("矩形"), u8s ("椭圆"), u8s ("六边形") };
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { RandomSynthMidiParamIDs::shape, 1 }, u8s ("场地形状"),
        shapeChoices, 0));

    juce::StringArray scaleChoices;
    for (int i = 0; i < (int) ScaleMode::Count; ++i)
        scaleChoices.add (scaleLabel (i));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { RandomSynthMidiParamIDs::scale, 1 }, u8s ("音阶"),
        scaleChoices, 0));

    addFloat (RandomSynthMidiParamIDs::range, u8s ("音域"), 1.0f, 16.0f, 0.0f, 8.0f,
              u8s (" 八度"), 0.5f);
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { RandomSynthMidiParamIDs::fine, 1 }, u8s ("微调音域"), false));

    addFloat (RandomSynthMidiParamIDs::ratioPitch,  u8s ("音高比例"), 0.0f, 100.0f, 0.1f, 50.0f, "%");
    addFloat (RandomSynthMidiParamIDs::ratioAdsr,   u8s ("ADSR 比例"), 0.0f, 100.0f, 0.1f, 25.0f, "%");
    addFloat (RandomSynthMidiParamIDs::ratioEffect, u8s ("效果比例"), 0.0f, 100.0f, 0.1f, 25.0f, "%");

    addFloat (RandomSynthMidiParamIDs::gate,    u8s ("GATE"), 0.01f, 2.0f, 0.001f, 0.08f, u8s (" s"));
    addFloat (RandomSynthMidiParamIDs::gateMin, u8s ("GATE MIN"), 0.01f, 2.0f, 0.001f, 0.03f, u8s (" s"));
    addFloat (RandomSynthMidiParamIDs::gateMax, u8s ("GATE MAX"), 0.01f, 2.0f, 0.001f, 0.50f, u8s (" s"));

    juce::StringArray midiChannels;
    for (int channel = 1; channel <= 16; ++channel)
        midiChannels.add (juce::String (channel));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { RandomSynthMidiParamIDs::midiChannel, 1 }, u8s ("MIDI 通道"),
        midiChannels, 0));

    return layout;
}

void RandomSynthMidiAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    activeNotes.clear();
    publishSnapshot();
}

void RandomSynthMidiAudioProcessor::releaseResources()
{
    activeNotes.clear();
}

void RandomSynthMidiAudioProcessor::reset()
{
    resetRequested.store (true);
}

void RandomSynthMidiAudioProcessor::setRunning (bool shouldRun)
{
    if (shouldRun)
    {
        running.store (true);
    }
    else if (running.exchange (false))
    {
        stopRequested.store (true);
    }
}

Snapshot RandomSynthMidiAudioProcessor::getSnapshot()
{
    const juce::ScopedLock sl (snapshotLock);
    return snapshot;
}

void RandomSynthMidiAudioProcessor::publishSnapshot()
{
    const juce::ScopedLock sl (snapshotLock);
    world.fillSnapshot (snapshot);
}

bool RandomSynthMidiAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::stereo()
            || output == juce::AudioChannelSet::mono())
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled();
}

void RandomSynthMidiAudioProcessor::applyParameters()
{
    const auto rangeParam = getRangeValue();
    const auto fine = isFineEnabled();
    const auto snappedRange = fine ? rangeParam : std::round (rangeParam * 2.0f) * 0.5f;

    world.setFieldScale (params.field->load());
    world.setBallScale (params.ball->load());

    auto settings = world.getSettings();
    settings.ballSpeed = params.speed->load();
    settings.speedShare = params.speedShare->load();
    settings.speedStrength = params.strength->load();
    settings.waveSpeed = params.waveSpeed->load();
    settings.waveRange = params.waveRange->load();
    settings.ratio = {
        params.ratioPitch->load() / 100.0f,
        params.ratioAdsr->load() / 100.0f,
        params.ratioEffect->load() / 100.0f
    };
    world.setSettings (settings);

    world.setPitchRange (snappedRange);
    world.setScale (getScaleIndex());
    world.setShape (getShapeIndex());

    world.setDefaultGate (params.gate->load());
    world.setGateRange (params.gateMin->load(), params.gateMax->load());
    world.setBallCount ((int) std::lround (params.ballCount->load()));
    world.setBoosterCount ((int) std::lround (params.boosterCount->load()));

    currentMidiChannel = getMidiChannel();
}

void RandomSynthMidiAudioProcessor::advanceActiveNotes (juce::MidiBuffer& midi,
                                                        int blockOffset, int numSamples)
{
    for (int i = (int) activeNotes.size(); --i >= 0;)
    {
        auto& note = activeNotes[(size_t) i];
        if (note.samplesRemaining <= numSamples)
        {
            const auto offset = blockOffset + juce::jmax (0, note.samplesRemaining);
            midi.addEvent (juce::MidiMessage::noteOff (note.channel, note.note), offset);
            activeNotes.erase (activeNotes.begin() + i);
        }
        else
        {
            note.samplesRemaining -= numSamples;
        }
    }
}

void RandomSynthMidiAudioProcessor::triggerNote (const SoundEvent& event, juce::MidiBuffer& midi,
                                                 int blockOffset, int chunkSamples)
{
    const auto note = juce::jlimit (0, 127, (int) std::lround (event.note));
    const auto velocity = juce::jlimit (1, 127, (int) std::lround (event.velocity * 127.0f));
    const auto gateSamples = juce::jlimit (1, (int) (4.0 * currentSampleRate),
                                          (int) std::lround (event.gateSeconds * currentSampleRate));

    uiLastNote.store (note);
    uiLastPan.store (event.pan);
    uiLastSend.store (event.send);
    uiGateSamples.store (juce::jmax (uiGateSamples.load(), gateSamples));
    uiTrigSamples.store (juce::jmax (uiTrigSamples.load(),
                                     (int) std::lround (0.05 * currentSampleRate)));

    // Retriggering the same pitch must stop the previous note first, otherwise
    // the old note-off would cut the new note short.
    for (int i = (int) activeNotes.size(); --i >= 0;)
    {
        const auto& active = activeNotes[(size_t) i];
        if (active.note == note && active.channel == currentMidiChannel)
        {
            midi.addEvent (juce::MidiMessage::noteOff (active.channel, note), blockOffset);
            activeNotes.erase (activeNotes.begin() + i);
        }
    }

    midi.addEvent (juce::MidiMessage::noteOn (currentMidiChannel, note, (juce::uint8) velocity),
                   blockOffset);

    if (outputMacroCc)
    {
        const auto ccValue = [] (float value)
        {
            return juce::jlimit (0, 127, (int) std::lround (value * 127.0f));
        };

        midi.addEvent (juce::MidiMessage::controllerEvent (currentMidiChannel, macroPanCc,
                                                        ccValue ((event.pan + 1.0f) * 0.5f)),
                       blockOffset);
        midi.addEvent (juce::MidiMessage::controllerEvent (currentMidiChannel, macroWaveformCc,
                                                        ccValue (event.waveform)),
                       blockOffset);
        midi.addEvent (juce::MidiMessage::controllerEvent (currentMidiChannel, macroReverbCc,
                                                        ccValue (event.send)),
                       blockOffset);
    }

    if (gateSamples <= chunkSamples)
        midi.addEvent (juce::MidiMessage::noteOff (currentMidiChannel, note),
                       blockOffset + gateSamples);
    else
        activeNotes.push_back ({ note, currentMidiChannel, gateSamples - chunkSamples });
}

void RandomSynthMidiAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();
    buffer.clear();

    const auto allNotesOff = [&midiMessages]
    {
        // Active notes may have been started on different channels before the
        // MIDI CH selection was changed, so clear every channel.
        for (int channel = 1; channel <= 16; ++channel)
            midiMessages.addEvent (juce::MidiMessage::allNotesOff (channel), 0);
    };

    if (resetRequested.exchange (false))
    {
        world.reset();
        activeNotes.clear();
        uiGateSamples.store (0);
        uiTrigSamples.store (0);
        allNotesOff();
    }

    if (stopRequested.exchange (false))
    {
        activeNotes.clear();
        uiGateSamples.store (0);
        uiTrigSamples.store (0);
        allNotesOff();
    }

    if (getSampleRate() > 0.0)
        currentSampleRate = getSampleRate();

    applyParameters();

    const auto numSamples = buffer.getNumSamples();

    if (running.load())
    {
        constexpr int maxChunk = 64;

        for (int position = 0; position < numSamples;)
        {
            const auto chunk = juce::jmin (maxChunk, numSamples - position);

            advanceActiveNotes (midiMessages, position, chunk);

            world.update ((float) (chunk / currentSampleRate));

            for (const auto& event : world.getEvents())
                triggerNote (event, midiMessages, position, chunk);

            position += chunk;
        }
    }

    uiGateSamples.store (juce::jmax (0, uiGateSamples.load() - numSamples));
    uiTrigSamples.store (juce::jmax (0, uiTrigSamples.load() - numSamples));
    uiGateActive.store (uiGateSamples.load() > 0 || ! activeNotes.empty());
    uiTrigActive.store (uiTrigSamples.load() > 0);

    publishSnapshot();
}

juce::AudioProcessorEditor* RandomSynthMidiAudioProcessor::createEditor()
{
    return new RandomSynthMidiAudioProcessorEditor (*this);
}

void RandomSynthMidiAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }
}

void RandomSynthMidiAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RandomSynthMidiAudioProcessor();
}
