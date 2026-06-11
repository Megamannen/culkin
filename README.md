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

Stops an audiobook near a target time, but at a natural chapter boundary so you don't lose your place mid-sentence. It detects chapter changes by watching `media_position` reset toward zero (so it's independent of playback speed and pauses). Within an early grace window before the target time it stops at the first boundary; at the target time it cuts mid-chapter only if no boundary is imminent within the late grace window; and it waits out an imminent boundary as a failsafe. Halting is `pause` then `stop` so your resume position is preserved. If duration/position aren't reported, it degrades to simply stopping at the target time.

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
| Early Grace Window | Minutes before the target a stop may land on a chapter boundary; 0 disables stopping early (default 10) |
| Late Grace Window | Minutes after the target a stop may drift to land on or wait out a chapter boundary; 0 always cuts at the target (default 10) |
| Override Stop Time | A time-only `input_datetime` helper. When set to anything other than `00:00` it replaces the Target Stop Time for that night, then resets to `00:00` after the stop (one-shot). `00:00` means off. |
