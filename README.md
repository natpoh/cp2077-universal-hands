# Cyberpunk Universal Hands VR

## Description
**Cyberpunk Universal Hands VR** is a powerful, **100% free** RED4ext and Cyber Engine Tweaks (CET) plugin that allows seamless external injection of VR hand tracking into Cyberpunk 2077's first-person animations. 

*Please note: This project is a completely free and open-source fork based on the original **CyberpunkVRPort** by dariulone ([GitHub](https://github.com/dariulone/cyberpunk-vr-port)).*

Originally built as a companion to **BodyWalkVR**, this mod uses a high-performance Shared Memory interface to read VR controller positions and rotations (from OpenXR, SteamVR, or custom tracking solutions) and applies them directly to the in-game player skeleton (VRIK) in real-time.

It features a full CET-based in-game UI to calibrate offsets, adjust arm reach, and persist settings between sessions.

## Key Features
* **Universal Tracking Support**: Reads any tracking data sent to the `Local\BodyWalkVR_UniversalTracking` shared memory map. Works with BodyWalkVR, custom scripts, and external VR overlays.
* **Full VRIK Arm Solver**: Bypasses the game's static arm animations and uses a custom 2-bone Inverse Kinematics (IK) solver to naturally position the shoulder, elbow, and wrist based on your real-life VR controllers.
* **In-Game Calibration (CET)**: Open the Cyber Engine Tweaks overlay to instantly adjust left and right hand offsets, reach scale, wrist rotation (pitch/yaw/roll), and elbow swing limits.
* **Persistent Settings**: All your calibration values are automatically saved to `config.json` and applied every time you launch the game.
* **Head-Relative Locking**: Prevents hands from swinging wildly when you physically turn your head or when the game forces the camera to look around.

## Requirements
* [Cyber Engine Tweaks (CET)](https://www.nexusmods.com/cyberpunk2077/mods/107)
* [RED4ext](https://www.nexusmods.com/cyberpunk2077/mods/2380)
* *Optional but recommended:* A VR Mod (like the LukeRoss VR mod) to view the hands in VR.
* *A Tracking Provider:* You need a program (like BodyWalkVR) to write your controller positions to the shared memory map, otherwise the hands will fall back to default animations.

## Installation & Download

1. Download the **BodyWalkVR Tracker App** from **[https://bodywalkvr.com/download](https://bodywalkvr.com/download)** (Required for tracking).
2. Download the Mod `.zip` file from the **Files** tab here on NexusMods.
3. Ensure you have **CET** and **RED4ext** installed and working.
4. Extract the contents of the mod `.zip` file into your main Cyberpunk 2077 game directory.
5. The folder structure should merge with your `bin/x64/plugins/` (for RED4ext) and `bin/x64/plugins/cyber_engine_tweaks/mods/` (for the CET UI).

## 🚀 Step-by-Step Startup Guide
*Because this mod connects external tracking to the game, please follow these exact steps to ensure it works correctly.*

### Step 1: Configure BodyWalkVR
1. Launch **BodyWalkVR**.
2. Go to the **Profiles** tab and load the **Cyberpunk** profile.
3. Go to the **Main** tab and make sure **Lock HMD position to center** is CHECKED.
4. Go to the **Mapping** tab and ensure your Tracking Input is set to **OpenXR** (or your preferred tracker).
5. Go to the **Startup** tab and ensure **Enable Universal Tracking Output** is CHECKED.

### Step 2: Launch Cyberpunk 2077
1. Launch the game and load a save where you are in First-Person view.
2. Open the **Cyber Engine Tweaks (CET)** overlay.
3. Find the **Cyberpunk Universal Hands** window and toggle **Enable Universal Hands** to ON.

### Step 3: Focus & Calibrate
1. **CRITICAL STEP:** BodyWalkVR acts as a virtual keyboard and gamepad emulator. You must click back into the Cyberpunk 2077 game window so it is completely **in focus**, otherwise the hand tracking will NOT register!
2. Use the sliders in the CET overlay to adjust your hand offsets so that the in-game hands align perfectly with your physical controllers.
3. *(The calibration settings are automatically saved and will load on your next session!)*

## For Developers (Open Source)
This mod exposes its Shared Memory layout and source code, allowing anyone to write custom drivers (e.g. for gloves, external trackers, or custom VR setups) and inject them directly into Cyberpunk's animation graph. 
Check out the GitHub repository for source code and documentation on the memory structure!
