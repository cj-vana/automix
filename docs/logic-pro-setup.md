# AutoMix — DAW Setup Guide

AutoMix is a multi-channel plugin that processes up to 32 microphone channels simultaneously through a single plugin instance. This requires all channels to be routed to one plugin — which works differently depending on your DAW.

## Standalone App (Recommended)

The standalone app connects directly to your audio interface and sees all available input channels with no routing setup required. This is the simplest and most reliable way to use AutoMix, especially with high channel counts.

1. Launch **AutoMix.app**
2. Open **Settings** and select your audio interface
3. All input channels from the interface appear as AutoMix channels

This is the recommended approach for Logic Pro users, since Logic's routing model does not easily support multi-channel effect plugins (see below).

## REAPER

REAPER has native support for multi-channel track routing, making it the best DAW option for AutoMix.

1. Create a new track and insert **AutoMix** as an effect
2. Set the track's channel count to match your mic count (right-click track → set to N channels)
3. For each microphone track, open the **Routing** dialog and add a send to the AutoMix track
4. Set each send to target a different channel pair/offset on the destination track
5. Each routed microphone maps to a discrete channel in AutoMix

## Logic Pro — Limitations

Logic Pro's audio routing is designed around stereo (or standard surround format) channel strips. It does not natively support routing arbitrary numbers of mono tracks into a single multi-channel effect plugin.

**Why summing stacks don't work:** A summing stack mixes all subtracks down to stereo *before* the main track's plugins process the audio. AutoMix on a summing stack would only see a 2-channel stereo mix — not the individual microphone channels it needs.

**Why surround buses are limited:** Logic Pro's surround formats top out at 7.1.4 (12 channels), and AutoMix's `discreteChannels(32)` format is not a standard surround layout that Logic recognizes. The plugin may only appear in stereo mode.

**Recommendation for Logic Pro users:** Use the **Standalone app** with your audio interface. This avoids Logic's routing limitations entirely and gives you full access to all channels.

## Other DAWs

- **Cubase / Nuendo** — Supports multi-channel buses and flexible routing. Create a group channel with enough channels and route tracks to it.
- **Pro Tools** — Supports multi-channel aux buses for routing. Create an aux track, route mic tracks to it via buses, and insert AutoMix.
- **Ableton Live** — Limited multi-channel support. Use the Standalone app instead.
