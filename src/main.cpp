#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <Update.h>
#include "ssidpass.h"
#include <HTTPClient.h>
//#include <HTTPUpdate.h>

#define OTA_ENDPOINT "/kasma_update"

WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long ota_progress_millis = 0;

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected. IP address: " + WiFi.localIP().toString());
}

void onOTAStart() {
  Serial.println("OTA update started!");
}

void onOTAProgress(size_t current, size_t final) {
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress: %u/%u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  if (success) {
    Serial.println("OTA update finished successfully! Rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    Serial.println("Error during OTA update!");
  }
}

void handleInfo() {
  String info = "Device Info\n";
  info += "IP: " + WiFi.localIP().toString() + "\n";
  info += "Uptime: " + String(millis() / 1000) + "s\n";
  server.send(200, "text/plain", info);
}

void setup_webserver() {
  server.on("/", []() {
    server.send(200, "text/plain", "Hello! KASSU. I'm Ready?");
  });

  server.on("/info", handleInfo);

  ElegantOTA.begin(&server, OTA_ENDPOINT);
  ElegantOTA.setAuth("admin", "1234");
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();
  Serial.println("HTTP server started");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("]");

  if (String(topic) == mqtt_ota_topic) {
    payload[length] = '\0';
    String fwUrl = String((char*)payload);
    Serial.println("Firmware URL: " + fwUrl);

    if (fwUrl.startsWith("http")) {
      WiFiClient client;
      HTTPClient http;

      http.begin(client, fwUrl);
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        bool canBegin = Update.begin(contentLength);

        if (canBegin) {
          WiFiClient* stream = http.getStreamPtr();
          size_t written = Update.writeStream(*stream);
          if (written == contentLength) {
            Serial.println("Written : " + String(written) + " successfully");
          } else {
            Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
          }

          if (Update.end()) {
            Serial.println("OTA done!");
            if (Update.isFinished()) {
              Serial.println("Update successfully completed. Rebooting.");
              ESP.restart();
            } else {
              Serial.println("Update not finished? Something went wrong!");
            }
          } else {
            Serial.println("Error Occurred. Error #: " + String(Update.getError()));
          }
        } else {
          Serial.println("Not enough space to begin OTA");
        }
      } else {
        Serial.println("Server responded with code: " + String(httpCode));
      }
      http.end();
    } else {
      Serial.println("Invalid URL");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(mqtt_ota_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN,OUTPUT);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  setup_webserver();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  server.handleClient();
  /*digitalWrite(LED_BUILTIN,HIGH);
  delay(5000);
  digitalWrite(LED_BUILTIN,LOW);
  delay(5000);*/
  ElegantOTA.loop();
 
}
