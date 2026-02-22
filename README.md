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
