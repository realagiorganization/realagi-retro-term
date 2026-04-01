# realagi-retro-term

|> Default Amber|C:\ IBM DOS|$ Default Green|
|---|---|---|
|![Default Amber Realagi Retro Term](https://user-images.githubusercontent.com/121322/32070717-16708784-ba42-11e7-8572-a8fcc10d7f7d.gif)|![IBM DOS](https://user-images.githubusercontent.com/121322/32070716-16567e5c-ba42-11e7-9e64-ba96dfe9b64d.gif)|![Default Green Realagi Retro Term](https://user-images.githubusercontent.com/121322/32070715-163a1c94-ba42-11e7-80bb-41fbf10fc634.gif)|

## Description
realagi-retro-term is a renamed fork of cool-retro-term, a terminal emulator which mimics the look and feel of old cathode tube screens.
It is designed to stay eye-candy, customizable, and reasonably lightweight while publishing the fork under the Realagi organization.

It uses the QML port of qtermwidget (Konsole): https://github.com/Swordfish90/qmltermwidget.

This terminal emulator works under Linux and macOS and requires Qt6.

Settings such as colors, fonts, and effects can be accessed via context menu.

## Screenshots
![Image](<https://i.imgur.com/TNumkDn.png>)
![Image](<https://i.imgur.com/hfjWOM4.png>)
![Image](<https://i.imgur.com/GYRDPzJ.jpg>)

## Install

If you want to get a hold of the latest version, go to the Releases page in this repository and grab the latest AppImage (Linux) or dmg (macOS).

Some distributions such as Ubuntu, Fedora, or Arch already package the upstream cool-retro-term project in their official repositories, but this renamed fork should be installed from this repository's release artifacts or built from source.

## Building

Build instructions still track the upstream wiki for now:

- Linux: <https://github.com/Swordfish90/cool-retro-term/wiki/Build-Instructions-(Linux)>
- macOS: <https://github.com/Swordfish90/cool-retro-term/wiki/Build-Instructions-(macOS)>

### Local Linux quickstart

For a Debian or Kali host with Qt 6 packages available:

```bash
sudo apt-get install -y build-essential pkg-config qmake6 qt6-base-dev qt6-declarative-dev qt6-5compat-dev qt6-shadertools-dev qt6-svg-dev
git submodule update --init --recursive
./scripts/build-local.sh
./build/local/realagi-retro-term
```

The local build helper prefers `qmake6` and falls back to `qmake`, so it also works on environments where Qt 6 does not install a `qmake` alias.
