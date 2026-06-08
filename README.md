# culkin — Home Assistant Blueprints

A collection of my Home Assistant blueprints.

## Blueprints

### Relaxing Sleep Sounds

Plays a looping track on selected media players at a set evening time. Restarts automatically if playback stops during the night window, and stops at the configured morning time — only if the blueprint's own track is still playing (so it won't interrupt anything you switched to manually).

**Requirements:** Native Home Assistant only. No third-party integrations needed.

**Import:**

[![Open your Home Assistant instance and show the blueprint import dialog](https://my.home-assistant.io/badges/blueprint_import.svg)](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fraw.githubusercontent.com%2FYOUR_USERNAME%2Fculkin%2Fmain%2Fblueprints%2Fautomation%2Frelaxing-sleep-sounds.yaml)

Or manually paste this URL in **Settings → Automations → Blueprints → Import Blueprint**:

```
https://raw.githubusercontent.com/YOUR_USERNAME/culkin/main/blueprints/automation/relaxing-sleep-sounds.yaml
```

**Inputs:**

| Input | Description |
|---|---|
| Media Players | One or more `media_player` entities |
| Media Content ID | URL, URI, or path of the track to play |
| Media Content Type | `music`, `playlist`, `url`, etc. |
| Start Time | When to start playing each evening |
| Stop Time | When to stop in the morning |
| Volume | Playback volume (0.0 – 1.0) |
| Thumbnail URL | Optional image shown on the media player display (PNG/JPG URL, e.g. from `raw.githubusercontent.com`) |

### Audiobook Stopper

Stops an audiobook near a target time, but at a natural chapter boundary so you don't lose your place mid-sentence. It detects chapter changes by watching `media_position` reset toward zero (so it's independent of playback speed and pauses). Within a grace window around the target time it stops at the first boundary; at the target time it cuts mid-chapter only if no boundary is imminent; and it waits out an imminent boundary up to the end of the window as a failsafe. Halting is `pause` then `stop` so your resume position is preserved. If duration/position aren't reported, it degrades to simply stopping at the target time.

**Requirements:** Native Home Assistant only. No third-party integrations needed.

**Import:**

[![Open your Home Assistant instance and show the blueprint import dialog](https://my.home-assistant.io/badges/blueprint_import.svg)](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fraw.githubusercontent.com%2FYOUR_USERNAME%2Fculkin%2Fmain%2Fblueprints%2Fautomation%2Faudiobook-stopper.yaml)

Or manually paste this URL in **Settings → Automations → Blueprints → Import Blueprint**:

```
https://raw.githubusercontent.com/YOUR_USERNAME/culkin/main/blueprints/automation/audiobook-stopper.yaml
```

**Inputs:**

| Input | Description |
|---|---|
| Media Player | A single `media_player` entity to track and stop |
| Target Stop Time | The time you want the audiobook to stop around |
| Grace Window | Minutes before/after the target a stop may drift to land on a chapter boundary (default 15) |
