# Mr.Settings — MrRobotOS System Settings

> **A fully custom GTK4 system settings application for MrRobotOS**, built from scratch on a suckless/DWM stack with a Mr. Robot/fsociety aesthetic.

![License](https://img.shields.io/badge/license-GPL--3.0--or--later-green)
![Platform](https://img.shields.io/badge/platform-Arch%20Linux-blue)
![GTK](https://img.shields.io/badge/GTK-4.0-orange)
![Language](https://img.shields.io/badge/language-C-lightgrey)

---

## Overview

Mr.Settings is the central configuration hub of MrRobotOS. It provides a unified interface for managing every aspect of the system — from network connectivity and display arrangement to DWM color schemes and user account settings — all without leaving the desktop environment.

The application uses lazy page loading so only the page you navigate to is built, keeping startup instant. It supports CLI flags to jump directly to any settings page, making it scriptable from dwmblocks, keybindings, or shell scripts.

---

### Account
<img width="900" alt="Account" src="https://github.com/user-attachments/assets/ff67687b-383b-4c09-aac1-a72f1406bf8f" />

The account page shows your profile avatar with circular crop, username, hostname, shell, and home directory. Personal info fields (full name, email, phone, organisation, address, city, country, website) are editable and saved to `~/.config/mrrobotos/mrsettings/account/info`. The avatar is saved to `~/.config/mrrobotos/mrsettings/account/avatar`.

---

### Displays
<img width="900" alt="Displays" src="https://github.com/user-attachments/assets/6d74d7d5-ccf3-4f92-b120-5fea5711db6b" />

Interactive multi-monitor arrangement canvas. Click monitors to select them, configure resolution, refresh rate, position relative to other monitors, and set the primary display. Changes are applied live via xrandr.

---

### Sound
<img width="900" alt="Sound" src="https://github.com/user-attachments/assets/3202c73d-e648-4983-975a-5d6666178095" />

Output and input device management via PipeWire/PulseAudio. Per-device volume sliders, mute toggles, port selection, and default device switching. Auto-refreshes every 10 seconds.

---

### Battery
<img width="900" alt="Battery" src="https://github.com/user-attachments/assets/90820d53-6543-4241-a25f-f0c2af0d1684" />

Live battery gauge with health, power draw, time remaining, cycle count, voltage, and temperature cards. CPU power mode selector (Performance / Balanced / Power Saver) that writes directly to the kernel cpufreq governor.

---

### Keybindings
<img width="900" alt="Keybindings" src="https://github.com/user-attachments/assets/df3379c5-7e7b-44f1-9bd9-1a8b9b3e13cb" />

Full reference of all DWM keybindings rendered as keyboard key badges — launchers, window management, focus, layouts, tags, gaps, and session controls. Each binding includes a description and detail line.

---

### Wallpaper
<img width="900" alt="Wallpaper" src="https://github.com/user-attachments/assets/7b15efdc-f65c-4b23-8940-c630d732c06c" />

Current wallpaper preview with a horizontally scrollable strip of wallpapers from `/usr/share/mrrobotos/mrsettings/Wallpapers/`. Click any thumbnail to set it instantly via feh. Browse button opens a full file picker for any directory.

---

### Sharing
<img width="900" alt="Sharing" src="https://github.com/user-attachments/assets/f6e1ee31-ba5e-4ab0-bac5-114a5b99710e" />

Planned cloud and storage integration — VM management, storage replication, distributed storage, and backup/snapshot management. Currently shows a Coming Soon roadmap.

---

## Features

### Connectivity
- **Wi-Fi** — Network list with signal strength bars, connect/disconnect, password dialog for secured networks, WPA/WPA2 support via nmcli, auto-refresh every 5 seconds
- **Bluetooth** — Device list with paired/connected status, pair/trust/connect/disconnect/remove, background scan with agent, auto-refresh every 10 seconds
- **VPN** — Lists all nmcli VPN connections (OpenVPN, WireGuard, L2TP, PPTP), connect/disconnect per connection, auto-refresh every 15 seconds

### Hardware
- **Displays** — xrandr-based multi-monitor management, interactive canvas, resolution/refresh dropdown, position placement, primary monitor selection, enable/disable per monitor
- **Sound** — PipeWire/PulseAudio output and input device management, volume, mute, port selection, default device
- **Keyboard** — XKB layout and variant selection from the full evdev list, searchable dropdown, apply via setxkbmap
- **Battery** — Live gauge, battery health, power draw, time remaining, cycle count, voltage, temperature, CPU governor control

### Navigation
- **Keybindings** — Full DWM keyboard shortcut reference with key badges
- **Clicks & Buttons** — Full DWM mouse binding reference
- **Shortcuts** — Full st terminal keyboard shortcut reference including color scheme switching

### Personalisation
- **Appearance** — Toggle xfce4-panel and DWM status bar, xfce4-panel background color picker, DWM color scheme editor (9 schemes × 3 colors each), recompile and restart DWM in place via SIGUSR2
- **Wallpaper** — Wallpaper folder browser, full file picker, click-to-set with feh
- **Brightness** — Per-monitor brightness control via xrandr `--brightness`
- **Notifications** — dunst daemon status, Do Not Disturb toggle, config file editor, live preview of key config values

### System
- **Users & Groups** — Lists all system users (UID 1000+) with avatar, UID, shell, sudo status, and all groups with member lists
- **Date & Time** — Live clock, NTP toggle via timedatectl, searchable timezone dropdown
- **Region & Language** — Language locale, format locale (LC_TIME, LC_NUMERIC, LC_MONETARY, LC_PAPER), searchable timezone, keyboard layout shortcut to Keyboard page
- **Software & Updates** — pacman update checker via `checkupdates`, per-package and update-all via xterm, auto-checks every 30 minutes

### Privacy & Security
- **Sharing** — Cloud & storage integration roadmap (Coming Soon)

### Applications
- **Applications** — Full installed application list via GAppInfo, click to launch

### About
- **About** — MrRobotOS logo, kernel version, hostname, CPU, memory, GPU, disk usage, architecture

---

## CLI Flags

Mr.Settings can be opened directly to any page from the command line, a keybinding, or a dwmblocks script:

```sh
mrsettings --account
mrsettings --wifi
mrsettings --bluetooth
mrsettings --vpn
mrsettings --displays
mrsettings --sound
mrsettings --keyboard
mrsettings --battery
mrsettings --keybindings
mrsettings --clicks
mrsettings --shortcuts
mrsettings --appearance
mrsettings --wallpaper
mrsettings --brightness
mrsettings --notifications
mrsettings --users
mrsettings --datetime
mrsettings --region
mrsettings --updates
mrsettings --sharing
mrsettings --applications
mrsettings --about
mrsettings --help
```

---

## Installation

### Dependencies

```sh
sudo pacman -S \
    gtk4 \
    gdk-pixbuf2 \
    cairo \
    pango \
    glib2 \
    libx11 \
    xdg-desktop-portal \
    xdg-desktop-portal-gtk \
    networkmanager \
    bluez \
    bluez-utils \
    xorg-xrandr \
    xorg-setxkbmap \
    feh \
    dunst \
    pipewire \
    pipewire-pulse \
    wireplumber \
    pacman-contrib \
    xterm \
    xfce4-panel \
    xfce4-settings \
    pciutils
```

### Build & Install

```sh
git clone https://github.com/borapocan/mrsettings
cd mrsettings
make
sudo make install
```

This installs:
- Binary → `/usr/bin/mrsettings`
- Desktop entry → `/usr/share/applications/mrsettings.desktop`

### Source Location

Source lives at `/usr/local/src/mrrobotos/mrsettings/` on a MrRobotOS installation so users can modify and recompile.

---

## Configuration

### Avatar
Stored at `~/.config/mrrobotos/mrsettings/account/avatar` (JPEG, PNG, no extension). Pick via the camera button on the Account page.

### Personal Info
Stored at `~/.config/mrrobotos/mrsettings/account/info` (GKeyFile format). Edited on the Account page and saved with the Save Info button.

### Wallpapers Folder
Bundled wallpapers are read from `/usr/share/mrrobotos/mrsettings/Wallpapers/`. The Browse button opens any directory on the filesystem.

### DWM Color Schemes
Edited on the Appearance page and written to `/usr/local/src/mrrobotos/mrdwm/colors.h`. Apply & Restart DWM recompiles mrdwm and sends SIGUSR2 to restart it in place.

---

## License

Copyright (C) 2026 Merih Bora Poçan (MrRobotOS)

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

`SPDX-License-Identifier: GPL-3.0-or-later`
