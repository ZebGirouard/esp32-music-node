# esp32-music-node

Wi-Fi controllable audio node for an Adafruit ESP32 Feather V2, MAX98357A I2S amplifier, and speaker.

## Hardware

Known working I2S wiring:

```cpp
#define I2S_BCLK SCK
#define I2S_LRC  MOSI
#define I2S_DOUT 14
```

## Setup

1. Copy `.env.example` to `.env`.
2. Set your Wi-Fi credentials in `.env`.
3. Upload `esp32-music-node.ino` with the Arduino ESP32 board support installed.
4. Open Serial Monitor at `115200` baud to see the assigned IP address.

`.env` and `config.h` are ignored by Git so local Wi-Fi credentials are not committed.

`.env`:

```sh
ESP32_MUSIC_WIFI_SSID=YOUR_WIFI
ESP32_MUSIC_WIFI_PASSWORD=YOUR_PASSWORD
ESP32_HOST=192.168.x.x
```

## Build and Upload

Compile:

```sh
make compile
```

Upload, replacing the port if needed:

```sh
make upload PORT=/dev/cu.usbserial-5B1E0644481
```

Serial monitor:

```sh
make monitor PORT=/dev/cu.usbserial-5B1E0644481
```

HTTP helpers:

```sh
make status ESP32_HOST=192.168.x.x
make mute ESP32_HOST=192.168.x.x
make set ESP32_HOST=192.168.x.x FREQ=440 VOLUME=700 WAVE=saw
make note ESP32_HOST=192.168.x.x FREQ=440 VOLUME=700 DURATION=250 WAVE=triangle
```

If `ESP32_HOST` is set in `.env`, the helper commands can omit it.

## API

### `GET /status`

Returns current state:

```json
{
  "ok": true,
  "freq": 0,
  "volume": 0,
  "wave": "sine",
  "ip": "192.168.x.x"
}
```

### `GET /set?freq=440&volume=1000&wave=sine`

Updates continuous tone parameters immediately:

```json
{
  "ok": true,
  "freq": 440,
  "volume": 1000,
  "wave": "sine"
}
```

### `GET /note?freq=440&volume=1000&duration=250&wave=triangle`

Plays a one-shot note with a short built-in attack/release envelope, then returns to silence:

```json
{
  "ok": true,
  "freq": 440,
  "volume": 1000,
  "wave": "triangle",
  "duration": 250
}
```

Constraints:

- `freq=0` mutes output
- `20 <= freq <= 5000`
- `0 <= volume <= 2000`
- `1 <= duration <= 10000` for `/note`
- `wave` can be `sine`, `square`, `saw`, or `triangle`
- Invalid numeric values return `400` without changing the current tone.

Example:

```txt
http://ESP32_IP/note?freq=880&volume=1000&duration=250&wave=saw
```

The firmware clamps volume to `2000` or lower for the current speaker setup.
On boot, the node starts silent with `freq=0` and `volume=0` until a `/set` request starts playback.
