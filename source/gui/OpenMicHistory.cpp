#include "OpenMicHistory.h"

namespace
{
using namespace AutomixTheme;

constexpr int laneLabelWidth = 34;
} // namespace

void OpenMicHistory::setChannelCount (int numChannels)
{
    numChannels = juce::jlimit (0, AutomixProcessor::kMaxChannels, numChannels);
    if (numChannels == numChannels_)
        return;

    numChannels_ = numChannels;
    repaint();
}

void OpenMicHistory::pushFrame (const std::vector<bool>& open,
                                int numChannels,
                                double frameSeconds)
{
    setChannelCount (numChannels);

    const int limit = juce::jmin (numChannels_, (int) open.size());

    // Latch activity into the bucket currently being written.
    for (int ch = 0; ch < limit; ++ch)
        if (open[(size_t) ch])
            lanes_[(size_t) ch][(size_t) writeBucket_] = 1;

    const double secondsPerBucket = (double) windowSeconds / (double) numBuckets;
    bucketAccum_ += frameSeconds;

    bool advanced = false;
    while (bucketAccum_ >= secondsPerBucket)
    {
        bucketAccum_ -= secondsPerBucket;
        writeBucket_ = (writeBucket_ + 1) % numBuckets;

        // Clear the bucket we are about to start writing, so the oldest data
        // falls off the end rather than blending into the newest.
        for (int ch = 0; ch < AutomixProcessor::kMaxChannels; ++ch)
            lanes_[(size_t) ch][(size_t) writeBucket_] = 0;

        advanced = true;
    }

    if (advanced || dirty_)
    {
        dirty_ = false;
        repaint();
    }
    else
    {
        dirty_ = true;
    }
}

void OpenMicHistory::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();

    drawPanel (g, area);
    area = area.reduced (12, 10);

    auto header = area.removeFromTop (12);
    drawLabel (g, "OPEN-MIC HISTORY", header.removeFromLeft (140), textDim, 10.0f, 0.16f);
    drawValue (g, "LAST " + juce::String (windowSeconds) + " SECONDS",
               header, textFaintest, 9.0f, AutomixFonts::Weight::medium);
    area.removeFromTop (6);

    if (numChannels_ <= 0)
        return;

    // Two columns, matching the design. Splitting the lanes halves the vertical
    // space each needs, which is what keeps 16 channels legible in 146px.
    const int leftCount = (numChannels_ + 1) / 2;
    const int rightCount = numChannels_ - leftCount;

    auto left = area.removeFromLeft (area.getWidth() / 2 - 10);
    area.removeFromLeft (20);
    auto right = area;

    paintColumn (g, left, 0, leftCount);
    if (rightCount > 0)
        paintColumn (g, right, leftCount, rightCount);
}

void OpenMicHistory::paintColumn (juce::Graphics& g,
                                  juce::Rectangle<int> area,
                                  int firstChannel,
                                  int count)
{
    if (count <= 0 || area.isEmpty())
        return;

    // Time ruler along the top of the column: oldest on the left, now on the
    // right, matching the direction the lanes scroll.
    auto ruler = area.removeFromTop (10);
    ruler.removeFromLeft (laneLabelWidth + 8);

    constexpr int ticks = 3;
    const int tickWidth = 30;
    for (int i = 0; i <= ticks; ++i)
    {
        const int seconds = windowSeconds - i * windowSeconds / ticks;
        const float t = (float) i / (float) ticks;
        const int x = ruler.getX()
                      + juce::roundToInt (t * (float) (ruler.getWidth() - tickWidth));

        drawValue (g,
                   seconds == 0 ? juce::String ("NOW") : "-" + juce::String (seconds) + "s",
                   juce::Rectangle<int> (x, ruler.getY(), tickWidth, ruler.getHeight()),
                   textGhost, 8.0f, AutomixFonts::Weight::medium,
                   i == ticks ? juce::Justification::centredRight
                              : juce::Justification::centredLeft);
    }
    area.removeFromTop (2);

    const float laneHeight = (float) area.getHeight() / (float) count;
    const int barHeight = juce::jlimit (3, 7, juce::roundToInt (laneHeight) - 2);

    for (int i = 0; i < count; ++i)
    {
        const int ch = firstChannel + i;
        auto lane = area.withY (area.getY() + juce::roundToInt ((float) i * laneHeight))
                        .withHeight (juce::roundToInt (laneHeight));

        const auto& row = lanes_[(size_t) ch];
        const bool openNow = row[(size_t) writeBucket_] != 0;

        // Below about nine pixels a lane cannot hold 8pt text without the rows
        // colliding, so past that the numbers are dropped and every fourth lane
        // is marked instead. A wall of overlapping labels is worse than none.
        const bool roomForLabel = laneHeight >= 9.0f;
        auto label = lane.removeFromLeft (laneLabelWidth);
        lane.removeFromLeft (8);

        if (roomForLabel)
        {
            drawValue (g, "CH " + juce::String (ch + 1).paddedLeft ('0', 2), label,
                       openNow ? accent : textGhost, 8.0f, AutomixFonts::Weight::medium,
                       juce::Justification::centredRight);
        }
        else if (ch % 4 == 0)
        {
            drawValue (g, juce::String (ch + 1), label,
                       openNow ? accent : textGhost, 7.0f, AutomixFonts::Weight::medium,
                       juce::Justification::centredRight);
        }

        auto track = lane.withSizeKeepingCentre (lane.getWidth(), barHeight);
        drawWell (g, track);

        // Draw runs of open buckets rather than per-bucket rectangles, so a
        // continuously open mic is one filled block, not 240 abutting ones.
        const float bucketWidth = (float) track.getWidth() / (float) numBuckets;
        int runStart = -1;

        for (int b = 0; b <= numBuckets; ++b)
        {
            // Oldest bucket first: the one after the write head.
            const int idx = (writeBucket_ + 1 + b) % numBuckets;
            const bool on = b < numBuckets && row[(size_t) idx] != 0;

            if (on && runStart < 0)
            {
                runStart = b;
            }
            else if (! on && runStart >= 0)
            {
                const float x = (float) track.getX() + (float) runStart * bucketWidth;
                const float w = juce::jmax (1.0f, (float) (b - runStart) * bucketWidth);
                g.setColour (colour (accent).withAlpha (0.85f));
                g.fillRect (juce::Rectangle<float> (x, (float) track.getY(), w,
                                                    (float) track.getHeight()));
                runStart = -1;
            }
        }
    }
}
