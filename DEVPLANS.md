# Dev Plans

This document captures requested product work for the current fork and turns it into implementation-sized feature requests.

Assumption for feature 1: "highlight of color for emojis" means emoji should remain visually distinct and preferably preserve or gain color emphasis instead of being flattened into the normal terminal foreground color.

## Summary

| Feature | Goal | Main Areas | Effort |
| --- | --- | --- | --- |
| Emoji color highlight | Make emoji visually readable and color-aware without breaking terminal cell alignment | `qmltermwidget/lib/TerminalDisplay.cpp`, `qmltermwidget/lib/TerminalCharacterDecoder.cpp`, `qmltermwidget/lib/konsole_wcwidth.cpp` | Medium to large |
| Mouse support improvement | Make mouse behavior reliable in terminal apps, especially `tmux` and `mc` with mouse enabled | `app/qml/PreprocessedTerminal.qml`, `qmltermwidget/lib/TerminalDisplay.cpp` | Medium |
| Bottom placed tabs | Allow tabs at the bottom of the terminal window without breaking focus or sizing | `app/qml/TerminalTabs.qml`, `app/qml/TerminalWindow.qml`, settings storage | Small to medium |
| Music player with synced visualizer | Add local audio playback with CRT-reactive visual effects and a path for MIDI/FM backends | `app/qml`, `app/shaders`, `app/app.pro` | Large |

## 1. Emoji Color Highlight

### Problem

Emoji currently flow through the normal terminal text paint path in `TerminalDisplay::drawCharacters()`. That path sets a single pen color from the terminal palette before calling `QPainter::drawText()`. This is good for normal terminal glyphs, but it is a weak fit for modern emoji because:

- emoji may render as monochrome or lose intended emphasis
- multi-codepoint emoji sequences can be width-sensitive
- CRT post-processing can make already-small emoji harder to read

### Goals

- Keep emoji aligned to terminal cells.
- Preserve readability for common emoji and emoji sequences.
- Avoid regressions for plain text, powerline glyphs, box drawing, and cursor rendering.
- Keep the retro look intact while allowing a controlled amount of color emphasis.

### Non-goals

- Full rich-text rendering inside the terminal.
- Per-app emoji theming.
- Replacing the existing terminal palette model.

### Proposed Implementation

1. Detect emoji presentation runs before painting text fragments.
2. Split mixed fragments into "normal text" and "emoji" runs when needed.
3. Introduce an emoji rendering mode with conservative defaults:
   - `terminal`: current behavior, emoji follow terminal foreground rules
   - `native-color`: allow color emoji rendering where supported by the platform/font stack
   - `accent-highlight`: keep the retro palette, but add a controlled highlight pass or fallback tint/background treatment for emoji cells
4. Validate grapheme cluster handling for ZWJ sequences, variation selectors, skin tones, and flags.
5. Review width handling in `TerminalCharacterDecoder.cpp` and `konsole_wcwidth.cpp` for emoji edge cases before shipping.

### Likely Code Touchpoints

- `qmltermwidget/lib/TerminalDisplay.cpp`
- `qmltermwidget/lib/TerminalCharacterDecoder.cpp`
- `qmltermwidget/lib/konsole_wcwidth.cpp`
- font and fallback behavior exposed through `app/qml/PreprocessedTerminal.qml`

### Risks

- Grapheme cluster width may disagree with terminal cell width rules.
- Platform font fallback may differ between Linux and macOS.
- Color emoji may clash with scanline, bloom, and curvature effects.
- Some emoji fonts may render outside expected ascent/descent bounds.

### Acceptance Criteria

- Single-codepoint emoji such as `🙂` and `🔥` stay aligned.
- Multi-codepoint emoji such as `❤️`, `👨‍💻`, and flags do not corrupt adjacent cells.
- Cursor, selection, and copy/paste behavior remain correct around emoji.
- Emoji remain readable across at least the default green and amber profiles.

## 2. Mouse Support Improvement, With tmux and mc Testing

### Problem

Mouse handling already exists, but it crosses multiple layers:

- `PreprocessedTerminal.qml` captures pointer input and corrects for CRT distortion.
- Synthetic mouse events are forwarded into `TerminalDisplay`.
- Terminal applications may enable mouse tracking modes that change whether clicks should select text or be passed through to the app.

This is exactly the kind of path that tends to work "well enough" in a shell and then fail in `tmux` or `mc`.

There is also a code smell worth verifying immediately: `TerminalDisplay::getUsesMouse()` currently returns `!usesMouse()`, which suggests the exposed QML property may be inverted.

### Goals

- Make click, drag, wheel, and release behavior predictable.
- Preserve normal text selection when the terminal app does not own the mouse.
- Pass mouse events through correctly when terminal apps do own the mouse.
- Explicitly validate behavior in `tmux` and `mc` with mouse support enabled.

### Proposed Implementation

1. Audit the meaning of `terminalUsesMouse` end-to-end:
   - QML property exposure
   - `TerminalDisplay::getUsesMouse()`
   - context menu gating
   - cursor shape switching
2. Recheck event forwarding from `PreprocessedTerminal.qml`:
   - press, release, move, double click, wheel
   - button and modifier mapping
   - focus acquisition on click
   - Shift override for text selection and context menu behavior
3. Verify distortion-corrected coordinates against terminal cell boundaries near edges and corners.
4. Add a repeatable manual test checklist to the repo so regressions are easy to re-run.

### Manual Test Matrix

| Scenario | Setup | Expected Result |
| --- | --- | --- |
| Plain shell selection | default shell, no mouse app active | drag selects text, right click opens context menu unless overridden |
| Wheel scrollback | shell output longer than viewport | wheel scrolls terminal history smoothly |
| `tmux` mouse mode | `tmux`, then `set -g mouse on` | clicks, pane changes, scroll, and selection behave as configured by tmux |
| `mc` mouse mode | launch `mc` inside terminal | panel selection, menu clicks, and wheel interactions work |
| Shift bypass | `tmux` or `mc` with mouse enabled | holding Shift forces terminal-side selection where supported |
| Edge coordinates | click near curved corners and margins | pointer mapping lands on the intended cell without off-by-one errors |

### Repo Deliverables

- Fixes in the mouse event path.
- A short testing note, likely under `docs/` or `scripts/`, with exact repro commands:
  - `tmux`
  - `set -g mouse on`
  - `mc`

### Acceptance Criteria

- `tmux` with mouse enabled supports pane selection, scrolling, and click targeting without obvious coordinate drift.
- `mc` with mouse enabled is usable for panel navigation and selection.
- Standard shell text selection still works when mouse mode is not active.
- Right click and Shift-modified behavior are consistent and documented.

## 3. Bottom Placed Tabs

### Problem

Tabs are currently hard-coded at the top of the window in `TerminalTabs.qml` via a `ColumnLayout` that renders the tab row first and the terminal stack second.

### Goals

- Support bottom-positioned tabs.
- Keep existing add-tab, close-tab, focus, and keyboard shortcut behavior.
- Avoid layout regressions in fullscreen, resizing, and single-tab mode.

### Proposed Implementation

1. Add a persisted setting for tab placement:
   - `top`
   - `bottom`
2. Refactor `TerminalTabs.qml` so the tab row can be rendered above or below the `StackLayout` without duplicating tab logic.
3. Keep the tab row hidden when only one tab exists, regardless of placement.
4. Verify interaction with:
   - terminal focus on tab change
   - add/close button placement
   - terminal size overlay
   - fullscreen and menu bar states

### Likely Code Touchpoints

- `app/qml/TerminalTabs.qml`
- `app/qml/TerminalWindow.qml`
- settings persistence in the existing QML storage/settings layer

### Acceptance Criteria

- Users can choose bottom tab placement and the choice persists.
- Switching tabs, creating tabs, and closing tabs behave the same as today.
- The terminal viewport does not overlap the tab bar.
- Keyboard shortcuts for tab creation, closing, and direct tab selection remain unchanged.

## 4. Music Player With Synced Visualizer

### Problem

The app already has a strong CRT shader pipeline, but it does not expose any media playback or music-reactive rendering. That leaves a clear opportunity: let the terminal act as an audiovisual surface, with playback driving bloom, flicker, scanline energy, glow, or dedicated overlay effects.

The full request includes:

- MP3 playback
- MIDI playback
- Sega or Yamaha FM style playback by reusing or integrating existing FM code
- synchronized on-screen effect visualization

### Goals

- Support local audio playback inside the app.
- Drive CRT effect intensity from a music-reactive signal instead of only static settings.
- Start with a low-risk backend for common files such as MP3.
- Leave a clean architecture for later MIDI and Yamaha FM playback integration.

### Non-goals For The First Slice

- Shipping a full Yamaha FM synthesizer in the initial patch.
- Perfect FFT-grade spectrum analysis on day one.
- A complete media library or playlist manager.

### Proposed Delivery Phases

1. Phase 1: MP3 player and shader-reactive visualizer scaffold.
   - Use `QtMultimedia` for local file playback.
   - Add a small player surface and file picker.
   - Introduce a visualizer state object that maps playback activity into shader controls.
2. Phase 2: Better synchronization.
   - Replace heuristic playback pulses with real audio level or spectrum analysis.
   - Add banded visualizer data for scanlines, bloom, and frame glow.
3. Phase 3: MIDI backend.
   - Add a MIDI playback path, likely through a dedicated synth backend rather than `MediaPlayer`.
   - Decide whether this is General MIDI, bundled soundfont playback, or FM-focused only.
4. Phase 4: Sega or Yamaha FM backend.
   - Reuse existing FM synthesis code if available and license-compatible.
   - Expose chip presets or profiles that fit the CRT aesthetic.

### Initial Implementation Shape

- `MusicPlayer.qml` or equivalent controller object for playback state
- `QtMultimedia`-based MP3 playback
- visualizer intensity properties surfaced into `ShaderTerminal.qml`
- optional overlay or compact controls for:
  - open file
  - play or pause
  - stop
  - current track label
- effect mapping such as:
  - louder signal increases `glowingLine`
  - mid signal increases `bloom`
  - transient spikes affect `flickering`, `jitter`, or a dedicated shader overlay

### Open Technical Questions

- Whether Qt's available multimedia APIs in the target releases expose enough audio analysis directly, or whether a small C++ helper is needed.
- Which external FM codebase is intended by "reuse code".
- Whether MIDI support should be software-synth only, or also support chip-emulation style playback.

### Acceptance Criteria For Phase 1

- A user can open and play a local MP3 file.
- Playback state is visible in the UI.
- The on-screen effects respond in a stable, obvious way while audio is playing.
- The base terminal experience remains usable when the player is idle or disabled.

## Suggested Delivery Order

1. Mouse support improvements and test plan.
2. Bottom tab placement.
3. Emoji color highlight, because it carries the highest rendering and cross-platform risk.
4. Music player and visualizer, starting with MP3 plus shader sync before MIDI or FM integration.
