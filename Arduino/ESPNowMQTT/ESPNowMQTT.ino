#ifdef ESP32
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#error Platform not supported
#endif
#include <ArduinoJson.h>
#include <MQTT.h>
#include "../config.h"

#ifdef ESP32
extern "C" int rom_phy_get_vdd33();
#include <esp_wifi.h>
#include <esp_now.h>
#elif defined(ESP8266)
extern "C" {
  #include <espnow.h>
  #include "user_interface.h"
}
#endif

WiFiClient net;
MQTTClient client(256);

volatile boolean haveReading = false;
char mqtt_msg[256];

#ifdef ESP32
double getMyVcc()
{
  int internalBatReading = rom_phy_get_vdd33();
  double vcc = (double)(((uint32_t)internalBatReading * 2960) / 2798) / 1000;
  /*
   * Ever since espressif released there latest arduino core the rom_phy_get_vdd33() 
   * returns a weird number. The best I can tell is it's double the actual reading 
   * so if the result is over 4 volts I half it before returning. 
   * This should allow the code to work both with the new core and the original one.
   */
  if (vcc > 4)
    vcc = (vcc / 2);
  return vcc;
}
#endif

void initEspNow() {
  if (esp_now_init()!=0) {
    Serial.println("*** ESP_Now init failed");
    ESP.restart();
  }

#ifdef ESP32
  esp_now_register_recv_cb([](const uint8_t *mac, const uint8_t *data, int len) {
#elif defined(ESP8266)
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  // esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb([](uint8_t *mac, uint8_t *data, uint8_t len) {
#endif
    Serial.print("$$"); // $$ just an indicator that this line is a received ESP-Now message

    String deviceMac = "";
    deviceMac += String(mac[0], HEX);
    deviceMac += String(mac[1], HEX);
    deviceMac += String(mac[2], HEX);
    deviceMac += String(mac[3], HEX);
    deviceMac += String(mac[4], HEX);
    deviceMac += String(mac[5], HEX);
    const size_t temp_size = sizeof(sensorData.temperature) / sizeof(sensorData.temperature[0]);
    const size_t pres_size = sizeof(sensorData.pressure) / sizeof(sensorData.pressure[0]);
    const size_t hum_size = sizeof(sensorData.humidity) / sizeof(sensorData.humidity[0]);
    const size_t capacity = JSON_ARRAY_SIZE(temp_size) + JSON_ARRAY_SIZE(pres_size) + JSON_ARRAY_SIZE(hum_size) + JSON_OBJECT_SIZE(8) + 200;
    DynamicJsonDocument doc(capacity);
    doc["mac"] = deviceMac;
    memcpy(&sensorData, data, sizeof(sensorData));
    doc["motion"] = sensorData.motion;
    doc["friendly_name"] = sensorData.friendlyName;
    doc["name"] = sensorData.deviceName;
    doc["battery_percent"] = sensorData.battery_percent;
    JsonArray temperature = doc.createNestedArray("temperature");
    for(int i = 0; i < temp_size; i++){
      temperature.add(sensorData.temperature[i]);
    }
    JsonArray pressure = doc.createNestedArray("pressure");
    for(int i = 0; i < pres_size; i++){
      pressure.add(sensorData.pressure[i]);
    }
    JsonArray humidity = doc.createNestedArray("humidity");
    for(int i = 0; i < hum_size; i++){
      humidity.add(sensorData.humidity[i]);
    }
    serializeJson(doc, Serial);
    serializeJson(doc, mqtt_msg);
    Serial.println();

    haveReading = true;
  });
}

void configDeviceAP() {
  bool result = WiFi.softAP(ESPSSID, ESPPASS, WIFI_CHANNEL, 1); //hidden network
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(ESPSSID));
  }
}

void connectWiFi(){
  WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
  uint8_t count = 0;
  Serial.printf("WiFi Connecting to %s ", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    count++;
    Serial.print(".");
    delay(500);
    if(count > 120){
      ESP.restart();
    }
  }
  Serial.println(" connected!");
}

void connectMQTT(){
  if(WiFi.status() != WL_CONNECTED){
    connectWiFi();
  }
  Serial.print("\nMQTT .");
  while (!client.connect(FRIENDLY_NAME, MQTT_USER, MQTT_PASS)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" connected!");
}

void resetWiFi() {
  WiFi.persistent(false);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_AP_STA);
}

void setup() {
  Serial.begin(115200);
  while(!Serial)
    ;
  Serial.println();
  WiFi.mode(WIFI_AP_STA);
  #ifdef ESP32
  esp_wifi_set_mac(WIFI_IF_AP, &mac[0]);
  #elif defined(ESP8266)
  wifi_set_macaddr(SOFTAP_IF, &mac[0]);
  #endif
  Serial.print("This node AP mac: "); Serial.println(WiFi.softAPmacAddress());
  Serial.print("This node STA mac: "); Serial.println(WiFi.macAddress());

  configDeviceAP();
  connectWiFi();
  client.begin(MQTT_HOST, MQTT_PORT, net);
  connectMQTT();
  initEspNow();
}

unsigned long heartBeat;

void loop() {

  if (!client.connected()) {
    connectMQTT();
  } else {
    client.loop();
  }
  if (millis()-heartBeat > 30000) {
    Serial.println("Waiting for ESP-NOW messages...");
    #ifdef ESP32
    Serial.printf("VCC: %3.2f\n", getMyVcc());
    #endif
    heartBeat = millis();
  }

  if (haveReading) {
    haveReading = false;
    client.publish("home/espnow/" + String(sensorData.friendlyName), mqtt_msg);
  }
}