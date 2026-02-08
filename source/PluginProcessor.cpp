#include "PluginProcessor.h"
#include "PluginEditor.h"

AutomixProcessor::AutomixProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::discreteChannels (kMaxChannels), true)
          .withOutput ("Output", juce::AudioChannelSet::discreteChannels (kMaxChannels), true))
{
}

AutomixProcessor::~AutomixProcessor()
{
    if (engine_ != nullptr)
    {
        automix_destroy (engine_);
        engine_ = nullptr;
    }
}

void AutomixProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    if (engine_ != nullptr)
    {
        automix_destroy (engine_);
    }

    engine_ = automix_create (
        static_cast<uint32_t> (getTotalNumInputChannels()),
        static_cast<float> (sampleRate),
        static_cast<uint32_t> (samplesPerBlock));
}

void AutomixProcessor::releaseResources()
{
    if (engine_ != nullptr)
    {
        automix_destroy (engine_);
        engine_ = nullptr;
    }
}

void AutomixProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (engine_ == nullptr)
        return;

    automix_process (
        engine_,
        buffer.getArrayOfWritePointers(),
        static_cast<uint32_t> (buffer.getNumChannels()),
        static_cast<uint32_t> (buffer.getNumSamples()));
}

bool AutomixProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != mainOutput)
        return false;

    int numChannels = mainInput.size();
    return numChannels >= 1 && numChannels <= kMaxChannels;
}

juce::AudioProcessorEditor* AutomixProcessor::createEditor()
{
    return new AutomixEditor (*this);
}

void AutomixProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void AutomixProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutomixProcessor();
}
