/*
    Renders the plugin editor offscreen to a PNG.

    Reviewing the interface by launching the standalone app and screenshotting it
    is unreliable: the window can end up behind other applications, and the
    standalone build asks for microphone access before it will draw. This
    renders the same editor headlessly, at any size and channel count, with
    meters driven by synthetic audio, so a design change can be checked in one
    command and in CI.

    Usage: AutoMixSnapshot <out.png> [width] [height] [channels]
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
/// Push audio through the processor so the meters show something. Channels are
/// given different levels and one is left silent, which is what the interface
/// has to distinguish.
void runBlocks (AutomixProcessor& processor, int numChannels, int blocks, juce::Random& rng)
{
    const int blockSize = 128;
    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    juce::MidiBuffer midi;

    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            // A spread of levels, one dead channel, one hot.
            float amplitude = 0.0f;
            switch (ch % 5)
            {
                case 0: amplitude = 0.42f; break;
                case 1: amplitude = 0.16f; break;
                case 2: amplitude = 0.0f;  break;   // silent: should stay closed
                case 3: amplitude = 0.28f; break;
                case 4: amplitude = 0.06f; break;
            }

            auto* w = buffer.getWritePointer (ch);
            for (int i = 0; i < blockSize; ++i)
                w[i] = amplitude * (rng.nextFloat() * 2.0f - 1.0f);
        }

        processor.processBlock (buffer, midi);
    }
}
} // namespace

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        juce::Logger::outputDebugString ("usage: AutoMixSnapshot <out.png> [w] [h] [channels]");
        return 1;
    }

    const juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File out (juce::File::getCurrentWorkingDirectory()
                              .getChildFile (juce::String (argv[1])));
    const int width    = argc > 2 ? juce::String (argv[2]).getIntValue() : 1200;
    const int height   = argc > 3 ? juce::String (argv[3]).getIntValue() : 700;
    const int channels = argc > 4 ? juce::String (argv[4]).getIntValue() : 16;

    AutomixProcessor processor;
    processor.setPlayConfigDetails (channels, channels, 48000.0, 128);
    processor.prepareToPlay (48000.0, 128);

    juce::Random rng (0x9a71c3);

    // Long enough for the noise floor to settle and the gain smoothers to
    // converge, so the snapshot shows a steady state rather than a transient.
    runBlocks (processor, channels, 600, rng);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
    {
        juce::Logger::outputDebugString ("editor could not be created");
        return 2;
    }

    editor->setSize (width, height);

    // Step the interface directly rather than running a message loop, and keep
    // feeding the processor between frames. The editor watches the processor's
    // block counter to tell "silent" from "the host stopped calling us", so a
    // burst of audio followed by 900 idle frames would snapshot as NO AUDIO.
    // 900 frames is the 30 seconds the history lane holds, so it fills.
    const int blocksPerFrame = juce::roundToInt (48000.0 / 30.0 / 128.0);

    if (auto* automixEditor = dynamic_cast<AutomixEditor*> (editor.get()))
    {
        for (int i = 0; i < 900; ++i)
        {
            runBlocks (processor, channels, blocksPerFrame, rng);
            automixEditor->refresh();
        }
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

    editor.reset();
    processor.releaseResources();

    out.deleteFile();
    if (auto stream = out.createOutputStream())
    {
        juce::PNGImageFormat png;
        if (! png.writeImageToStream (image, *stream))
        {
            juce::Logger::outputDebugString ("failed to encode PNG");
            return 3;
        }
    }
    else
    {
        juce::Logger::outputDebugString ("could not open " + out.getFullPathName());
        return 4;
    }

    return 0;
}
