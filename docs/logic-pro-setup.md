# AutoMix — Logic Pro Setup Guide

AutoMix is a multi-channel plugin that processes up to 32 channels simultaneously. In Logic Pro, you need to route your microphone channels to a multi-channel bus where AutoMix is inserted.

> **Tip:** For high channel counts (8+), the **Standalone app** is often simpler — it connects directly to your audio interface with no bus routing needed.

## Setup Steps

### 1. Enable Surround in Project Settings

1. Open **File → Project Settings → Audio**
2. Under **Surround**, set the surround format to match your channel count (e.g., 7.1 for 8 channels, or a higher format)
3. Click **Apply**

### 2. Create a Multi-Channel Aux Bus

1. Open the **Mixer** (press **X**)
2. Click the **+** button to create a new **Aux** channel strip
3. Set the Aux channel's **Input** to an unused Bus (e.g., Bus 1)
4. Set the Aux channel's format to surround/multi-channel (click the format indicator and choose the appropriate channel configuration)

### 3. Route Microphone Tracks to the Bus

For each microphone track:

1. Click the track's **Output** slot
2. Select the same Bus you assigned to the Aux (e.g., Bus 1)
3. Use the **Pan** control to assign the track to a specific channel within the bus (e.g., channel 1, channel 2, etc.)

### 4. Insert AutoMix on the Aux

1. On the Aux channel strip, click an empty **Insert** slot
2. Navigate to **Audio Units → AutoMix → AutoMix**
3. AutoMix will now process all channels routed to this bus

### 5. Configure AutoMix

- Each routed microphone appears as a channel in AutoMix
- Use the channel strip controls (weight, mute, solo, bypass) to configure each mic
- Adjust global parameters (attack, release, hold) as needed

## Alternative: Summing Stack

For simpler setups:

1. Select all microphone tracks in the **Tracks** area
2. Go to **Track → Create Track Stack → Summing Stack**
3. Insert AutoMix on the resulting Stack channel strip

## Channel Count Limitations

Logic Pro's surround bus system supports specific channel configurations. For more than 12 channels, the Standalone app is recommended as it has no routing limitations.

## REAPER Users

REAPER has excellent multi-channel routing. Simply:

1. Create a track and insert AutoMix
2. Route other tracks to this track using REAPER's routing matrix
3. Each routed track maps to a channel in AutoMix
