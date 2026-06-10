FQBN := esp32:esp32:adafruit_feather_esp32_v2
PORT ?= /dev/cu.usbserial-5B1E0644481
UPLOAD_SPEED ?= 115200
BUILD_PATH ?= /tmp/esp32-music-node-arduino-build
BUILD_FLAGS := -DWIFI_SSID=\"$${ESP32_MUSIC_WIFI_SSID}\" -DWIFI_PASSWORD=\"$${ESP32_MUSIC_WIFI_PASSWORD}\"
ESP32_HOST ?=
FREQ ?= 440
VOLUME ?= 700
DURATION ?= 250
WAVE ?= sine

ifneq (,$(wildcard .env))
include .env
export ESP32_MUSIC_WIFI_SSID
export ESP32_MUSIC_WIFI_PASSWORD
endif

.PHONY: check-env check-host compile upload monitor status mute set note

check-env:
	@test -n "$$ESP32_MUSIC_WIFI_SSID" || { echo "Missing ESP32_MUSIC_WIFI_SSID"; exit 1; }
	@test -n "$$ESP32_MUSIC_WIFI_PASSWORD" || { echo "Missing ESP32_MUSIC_WIFI_PASSWORD"; exit 1; }

check-host:
	@test -n "$(ESP32_HOST)" || { echo "Missing ESP32_HOST. Set it in .env or pass ESP32_HOST=192.168.x.x"; exit 1; }

compile: check-env
	arduino-cli compile --fqbn "$(FQBN)" --build-path "$(BUILD_PATH)" --build-property "build.extra_flags=$(BUILD_FLAGS)" .

upload: compile
	arduino-cli upload -p "$(PORT)" --fqbn "$(FQBN)" --upload-property upload.speed=$(UPLOAD_SPEED) --build-path "$(BUILD_PATH)" .

monitor:
	arduino-cli monitor -p "$(PORT)" -c baudrate=115200

status: check-host
	curl --max-time 5 "http://$(ESP32_HOST)/status"

mute: check-host
	curl --max-time 5 "http://$(ESP32_HOST)/set?freq=0&volume=0"

set: check-host
	curl --max-time 5 "http://$(ESP32_HOST)/set?freq=$(FREQ)&volume=$(VOLUME)&wave=$(WAVE)"

note: check-host
	curl --max-time 5 "http://$(ESP32_HOST)/note?freq=$(FREQ)&volume=$(VOLUME)&duration=$(DURATION)&wave=$(WAVE)"
