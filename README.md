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

1. Provide Wi-Fi credentials with shell environment variables or a local ignored `config.h`.
2. Upload `esp32-music-node.ino` with the Arduino ESP32 board support installed.
3. Open Serial Monitor at `115200` baud to see the assigned IP address.

`config.h` is ignored by Git so local Wi-Fi credentials are not committed. For a shell-based setup, add these to a private shell config such as `~/.zshrc`:

```sh
export ESP32_MUSIC_WIFI_SSID='YOUR_WIFI'
export ESP32_MUSIC_WIFI_PASSWORD='YOUR_PASSWORD'
```

## Build and Upload

Arduino CLI compile:

```sh
arduino-cli compile --fqbn esp32:esp32:adafruit_feather_esp32_v2 \
  --build-property "build.extra_flags=-DWIFI_SSID=\"${ESP32_MUSIC_WIFI_SSID}\" -DWIFI_PASSWORD=\"${ESP32_MUSIC_WIFI_PASSWORD}\"" \
  .
```

Arduino CLI upload, replacing the port if needed:

```sh
arduino-cli upload -p /dev/cu.usbserial-5B1E0644481 \
  --fqbn esp32:esp32:adafruit_feather_esp32_v2 \
  --upload-property upload.speed=115200 \
  --build-property "build.extra_flags=-DWIFI_SSID=\"${ESP32_MUSIC_WIFI_SSID}\" -DWIFI_PASSWORD=\"${ESP32_MUSIC_WIFI_PASSWORD}\"" \
  .
```

Serial monitor:

```sh
arduino-cli monitor -p /dev/cu.usbserial-5B1E0644481 -c baudrate=115200
```

## API

### `GET /status`

Returns current state:

```json
{
  "ok": true,
  "freq": 440,
  "volume": 12000,
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
- `0 <= volume <= 28000`

Example:

```txt
http://ESP32_IP/set?freq=880&volume=1000
```

For the current speaker setup, keep client-sent volume values at `2000` or lower.
