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
static const float TWO_PI_F = 6.28318530717958647692f;

WebServer server(80);

volatile float freq = 440.0f;
volatile float volume = 1000.0f;

static float phase = 0.0f;

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

String statusJson(bool includeIp) {
  float currentFreq = freq;
  float currentVolume = volume;

  String json = "{";
  json += "\"ok\":true";
  json += ",\"freq\":" + formatNumber(currentFreq);
  json += ",\"volume\":" + formatNumber(currentVolume);
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

  freq = nextFreq;
  volume = nextVolume;

  sendJson(200, statusJson(false));
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
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("HTTP server started");
}

void writeAudioBuffer() {
  int16_t samples[BUFFER_SAMPLES];
  float currentFreq = freq;
  float currentVolume = volume;

  if (currentFreq <= 0.0f || currentVolume <= 0.0f) {
    for (int i = 0; i < BUFFER_SAMPLES; i++) {
      samples[i] = 0;
    }
  } else {
    float phaseStep = TWO_PI_F * currentFreq / SAMPLE_RATE;

    for (int i = 0; i < BUFFER_SAMPLES; i++) {
      samples[i] = (int16_t)(sinf(phase) * currentVolume);
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
