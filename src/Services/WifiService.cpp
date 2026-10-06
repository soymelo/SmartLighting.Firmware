#include "WifiService.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "../../NetworkConfig.h"

namespace {
unsigned long lastAttempt = 0;
bool wasConnected = false;
}

void WifiService::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (NetworkConfig::Ssid[0] != '\0') {
    WiFi.begin(NetworkConfig::Ssid, NetworkConfig::Password);
    lastAttempt = millis();
  } else {
    Serial.println("Configure NetworkConfig.h antes de conectar ao Wi-Fi.");
  }
}

void WifiService::update() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected) {
    Serial.print("Wi-Fi conectado: ");
    Serial.println(WiFi.localIP());
    configTime(0, 0, "pool.ntp.org", "time.google.com");
  }
  if (!connected && wasConnected) Serial.println("Wi-Fi desconectado.");
  wasConnected = connected;

  if (!connected && NetworkConfig::Ssid[0] != '\0' &&
      millis() - lastAttempt >= 10000UL) {
    WiFi.reconnect();
    lastAttempt = millis();
  }
}
