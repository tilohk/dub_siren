# Changelog

All notable changes to dub_siren. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and the project uses [semantic versioning](https://semver.org/).

## [Unreleased]

### Fixed
- Restoring a saved state now sets every parameter to its saved value and tells the host. Before, a switch such
  as Echo Sync could keep a stale value in the host when its stored value had not changed.

### Build
- CI runs pluginval (strictness 10) on every plug-in format and `auval` on macOS.

## [1.0.0] - 2026-11-23

The first release.

### Sound
- Siren oscillator (square, saw, triangle, sine) with a base pitch from 30 Hz to 3 kHz. The
  **TRIGGER** button and MIDI note C3 play the base pitch; other notes transpose it.
- LFO that bends the pitch upwards: square, triangle, saw up, saw down, sine and S&H, with Rate,
  Depth (up to 36 semitones) and **Accel**, which speeds the LFO up or slows it down while the
  trigger is held. It restarts on every press.
- **Rotor** mode: an air-raid siren whose rotor spins up while held and coasts down after release,
  keeping its momentum between taps.
- Decay: Release time and a pitch Sweep (±48 semitones) after you let go.
- Tape echo: tempo-synced divisions or free time, Feedback up to 120 % (self-oscillation, kept
  bounded by saturation), Tape wear (slow time changes, wow and flutter), Tone and Level.
- Echo performance buttons: **THROW** (only the echo), **KILL** (stop feeding the echo) and
  **FREEZE** (loop what is in the echo).
- Output: Drive (level-matched saturation), Volume and a soft limiter.

### Interface
- Vintage hardware look: brushed-metal panels on a green case, black fluted knobs with aluminium
  caps, colour-coded section titles, a big black trigger and a red rotating siren lamp that lights
  while the siren plays.
- Icon buttons for the wave and LFO shapes.
- Five factory presets (Classic Siren, Rising Alarm, Laser Zap, Rotor Wind-up, Space Wobble),
  available from the selector in the header and as host programs.

### Platforms
- VST3 and standalone on macOS, Windows and Linux; AU on macOS (universal: Apple silicon and Intel);
  LV2 on Linux.
