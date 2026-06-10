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

## API

### `GET /status`

Returns current state:

```json
{
  "ok": true,
  "freq": 0,
  "volume": 0,
  "ip": "192.168.x.x"
}
```

### `GET /set?freq=440&volume=1000`

Updates tone parameters immediately:

```json
{
  "ok": true,
  "freq": 440,
  "volume": 1000
}
```

Constraints:

- `freq=0` mutes output
- `20 <= freq <= 5000`
- `0 <= volume <= 2000`
- Invalid numeric values return `400` without changing the current tone.

Example:

```txt
http://ESP32_IP/set?freq=880&volume=1000
```

The firmware clamps volume to `2000` or lower for the current speaker setup.
On boot, the node starts silent with `freq=0` and `volume=0` until a `/set` request starts playback.
