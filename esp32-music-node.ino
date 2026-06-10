#include <WiFi.h>
#include <WebServer.h>
#include <driver/i2s.h>
#include <math.h>

#if __has_include("config.h")
#include "config.h"
#endif

#ifndef WIFI_SSID
#error "Define WIFI_SSID in config.h or via build.extra_flags"
#endif

#ifndef WIFI_PASSWORD
#error "Define WIFI_PASSWORD in config.h or via build.extra_flags"
#endif

#define I2S_BCLK SCK
#define I2S_LRC  MOSI
#define I2S_DOUT 14

static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const int SAMPLE_RATE = 44100;
static const int BUFFER_SAMPLES = 256;
static const int MAX_DURATION_MS = 10000;
static const float TWO_PI_F = 6.28318530717958647692f;

WebServer server(80);

enum WaveShape {
  WAVE_SINE,
  WAVE_SQUARE,
  WAVE_SAW,
  WAVE_TRIANGLE
};

volatile float freq = 0.0f;
volatile float volume = 0.0f;
volatile WaveShape wave = WAVE_SINE;

static float phase = 0.0f;
static bool noteActive = false;
static uint32_t noteTotalSamples = 0;
static uint32_t noteSamplesElapsed = 0;

float clampFloat(float value, float minValue, float maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

String formatNumber(float value) {
  if (fabsf(value - roundf(value)) < 0.01f) {
    return String((long)roundf(value));
  }

  return String(value, 2);
}

bool parseFloatArg(String rawValue, float& parsedValue) {
  rawValue.trim();
  if (rawValue.length() == 0) {
    return false;
  }

  char* end = NULL;
  parsedValue = strtof(rawValue.c_str(), &end);

  return end != rawValue.c_str() && *end == '\0' && isfinite(parsedValue);
}

bool parseWaveArg(String rawValue, WaveShape& parsedWave) {
  rawValue.trim();
  rawValue.toLowerCase();

  if (rawValue == "sine") {
    parsedWave = WAVE_SINE;
  } else if (rawValue == "square") {
    parsedWave = WAVE_SQUARE;
  } else if (rawValue == "saw") {
    parsedWave = WAVE_SAW;
  } else if (rawValue == "triangle") {
    parsedWave = WAVE_TRIANGLE;
  } else {
    return false;
  }

  return true;
}

String waveName(WaveShape currentWave) {
  switch (currentWave) {
    case WAVE_SQUARE:
      return "square";
    case WAVE_SAW:
      return "saw";
    case WAVE_TRIANGLE:
      return "triangle";
    case WAVE_SINE:
    default:
      return "sine";
  }
}

float waveSample(WaveShape currentWave, float currentPhase) {
  float normalizedPhase = currentPhase / TWO_PI_F;

  switch (currentWave) {
    case WAVE_SQUARE:
      return currentPhase < PI ? 1.0f : -1.0f;
    case WAVE_SAW:
      return (2.0f * normalizedPhase) - 1.0f;
    case WAVE_TRIANGLE:
      return (2.0f * fabsf((2.0f * normalizedPhase) - 1.0f)) - 1.0f;
    case WAVE_SINE:
    default:
      return sinf(currentPhase);
  }
}

float envelopeGain(uint32_t elapsedSamples, uint32_t totalSamples) {
  if (totalSamples == 0) {
    return 0.0f;
  }

  uint32_t attackSamples = SAMPLE_RATE / 200;
  uint32_t releaseSamples = SAMPLE_RATE / 20;
  uint32_t maxRampSamples = totalSamples / 3;

  if (attackSamples > maxRampSamples) {
    attackSamples = maxRampSamples;
  }
  if (releaseSamples > maxRampSamples) {
    releaseSamples = maxRampSamples;
  }

  if (attackSamples > 0 && elapsedSamples < attackSamples) {
    return (float)elapsedSamples / attackSamples;
  }

  uint32_t remainingSamples = totalSamples - elapsedSamples;
  if (releaseSamples > 0 && remainingSamples < releaseSamples) {
    return (float)remainingSamples / releaseSamples;
  }

  return 1.0f;
}

String statusJson(bool includeIp) {
  float currentFreq = freq;
  float currentVolume = volume;
  WaveShape currentWave = wave;

  String json = "{";
  json += "\"ok\":true";
  json += ",\"freq\":" + formatNumber(currentFreq);
  json += ",\"volume\":" + formatNumber(currentVolume);
  json += ",\"wave\":\"" + waveName(currentWave) + "\"";
  if (includeIp) {
    json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  }
  json += "}";
  return json;
}

void sendJson(int statusCode, const String& body) {
  server.send(statusCode, "application/json", body);
}

void handleStatus() {
  sendJson(200, statusJson(true));
}

void handleSet() {
  float nextFreq = freq;
  float nextVolume = volume;
  WaveShape nextWave = wave;

  if (server.hasArg("freq")) {
    if (!parseFloatArg(server.arg("freq"), nextFreq)) {
      sendJson(400, "{\"ok\":false,\"error\":\"invalid_freq\"}");
      return;
    }

    if (nextFreq == 0.0f) {
      nextFreq = 0.0f;
    } else {
      nextFreq = clampFloat(nextFreq, 20.0f, 5000.0f);
    }
  }

  if (server.hasArg("volume")) {
    if (!parseFloatArg(server.arg("volume"), nextVolume)) {
      sendJson(400, "{\"ok\":false,\"error\":\"invalid_volume\"}");
      return;
    }

    nextVolume = clampFloat(nextVolume, 0.0f, 2000.0f);
  }

  if (server.hasArg("wave")) {
    if (!parseWaveArg(server.arg("wave"), nextWave)) {
      sendJson(400, "{\"ok\":false,\"error\":\"invalid_wave\"}");
      return;
    }
  }

  freq = nextFreq;
  volume = nextVolume;
  wave = nextWave;
  noteActive = false;

  sendJson(200, statusJson(false));
}

void handleNote() {
  float nextFreq = 0.0f;
  float nextVolume = 0.0f;
  float durationMs = 0.0f;
  WaveShape nextWave = wave;

  if (!server.hasArg("freq") || !parseFloatArg(server.arg("freq"), nextFreq)) {
    sendJson(400, "{\"ok\":false,\"error\":\"invalid_freq\"}");
    return;
  }

  if (!server.hasArg("volume") || !parseFloatArg(server.arg("volume"), nextVolume)) {
    sendJson(400, "{\"ok\":false,\"error\":\"invalid_volume\"}");
    return;
  }

  if (!server.hasArg("duration") || !parseFloatArg(server.arg("duration"), durationMs)) {
    sendJson(400, "{\"ok\":false,\"error\":\"invalid_duration\"}");
    return;
  }

  if (server.hasArg("wave")) {
    if (!parseWaveArg(server.arg("wave"), nextWave)) {
      sendJson(400, "{\"ok\":false,\"error\":\"invalid_wave\"}");
      return;
    }
  }

  if (nextFreq == 0.0f) {
    nextFreq = 0.0f;
  } else {
    nextFreq = clampFloat(nextFreq, 20.0f, 5000.0f);
  }

  nextVolume = clampFloat(nextVolume, 0.0f, 2000.0f);
  durationMs = clampFloat(durationMs, 1.0f, MAX_DURATION_MS);

  freq = nextFreq;
  volume = nextVolume;
  wave = nextWave;
  noteTotalSamples = (uint32_t)((durationMs / 1000.0f) * SAMPLE_RATE);
  noteSamplesElapsed = 0;
  noteActive = nextFreq > 0.0f && nextVolume > 0.0f;

  String json = statusJson(false);
  json.remove(json.length() - 1);
  json += ",\"duration\":" + formatNumber(durationMs);
  json += "}";
  sendJson(200, json);
}

void handleNotFound() {
  sendJson(404, "{\"ok\":false,\"error\":\"not_found\"}");
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupI2S() {
  i2s_config_t i2sConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_SAMPLES,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pinConfig = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_PORT, &i2sConfig, 0, NULL);
  i2s_set_pin(I2S_PORT, &pinConfig);
  i2s_zero_dma_buffer(I2S_PORT);
}

void setupServer() {
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/set", HTTP_GET, handleSet);
  server.on("/note", HTTP_GET, handleNote);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("HTTP server started");
}

void writeAudioBuffer() {
  int16_t samples[BUFFER_SAMPLES];
  float currentFreq = freq;
  float currentVolume = volume;
  WaveShape currentWave = wave;

  if (currentFreq <= 0.0f || currentVolume <= 0.0f) {
    for (int i = 0; i < BUFFER_SAMPLES; i++) {
      samples[i] = 0;
    }
  } else {
    float phaseStep = TWO_PI_F * currentFreq / SAMPLE_RATE;

    for (int i = 0; i < BUFFER_SAMPLES; i++) {
      float sampleVolume = currentVolume;

      if (noteActive) {
        if (noteSamplesElapsed >= noteTotalSamples) {
          noteActive = false;
          freq = 0.0f;
          volume = 0.0f;
          currentFreq = 0.0f;
          currentVolume = 0.0f;
          sampleVolume = 0.0f;
        } else {
          sampleVolume *= envelopeGain(noteSamplesElapsed, noteTotalSamples);
          noteSamplesElapsed++;
        }
      }

      samples[i] = (int16_t)(waveSample(currentWave, phase) * sampleVolume);
      phase += phaseStep;
      if (phase >= TWO_PI_F) {
        phase -= TWO_PI_F;
      }
    }
  }

  size_t bytesWritten = 0;
  i2s_write(I2S_PORT, samples, sizeof(samples), &bytesWritten, portMAX_DELAY);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  setupI2S();
  connectWifi();
  setupServer();
}

void loop() {
  server.handleClient();
  writeAudioBuffer();
}
