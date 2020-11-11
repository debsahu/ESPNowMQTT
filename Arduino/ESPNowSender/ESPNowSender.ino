#ifdef ESP32
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <espnow.h>
#else
#error Platform not supported
#endif

#include "../config.h";

#define SEND_TIMEOUT 245 // 245 millis seconds timeout
#define SERIAL_DEBUG false // change this to true if you want serial data

#ifdef ESP32
gpio_num_t pirPin = GPIO_NUM_33; // GPIO>32, in this case GPIO33
gpio_num_t ledPin = GPIO_NUM_4;  // GPIO4 HIGH = ON, LOW = OFF

// If you want ESP32 to read more than one pin to wake it up
#define BUTTON_PIN_BITMASK 0x300000000
#elif defined(ESP8266)
uint8_t holdPin = 0; // defines GPIO 0 as the hold pin (will hold CH_PD high untill we power down).
uint8_t pirPin = 12; // defines GPIO 12 as the PIR read pin (reads the state of the PIR output).
uint8_t ledPin = 4;  // GPIO4 HIGH = ON, LOW = OFF
#endif
bool pir = true;     // sets the PIR record (pir) to 1 (it must have been we woke up).
unsigned long prevMillis = 0;

volatile boolean callbackCalled;

uint8_t batPercentage(float voltage, bool asin = true)
{
  if (voltage >= 4.2) return 100;
  if (voltage <= 3.5) return 0;
  uint8_t result = (asin) ? ( 101 - (101 / pow(1 + pow(1.33 * (voltage - 3.5)/0.7, 4.5), 3))): (uint8_t) ((voltage - 3.5) / 0.007);
  return result >= 100 ? 100 : result;
}

void initPins()
{
#ifdef ESP32
    pinMode(pirPin, INPUT);
    pinMode(ledPin, OUTPUT);    // Key LED ON
    digitalWrite(ledPin, HIGH); // LED ON
#elif defined(ESP8266)
    pinMode(holdPin, OUTPUT);    // sets GPIO 0 to output
    digitalWrite(holdPin, HIGH); // sets GPIO 0 to high (this holds CH_PD high even if the PIR output goes low)
    pinMode(pirPin, INPUT);      // sets GPIO 12 to an input so we can read the PIR output state
    // pinMode(LED_BUILTIN, OUTPUT); // BuiltIn LED
    pinMode(ledPin, OUTPUT);    // Key LED ON
    digitalWrite(ledPin, HIGH); // LED ON
#endif
}

void gotoSleep()
{
    if(SERIAL_DEBUG)
    {
        Serial.println();
        Serial.println(F("OFF"));
        Serial.flush();
    }
#ifdef ESP32
    //Configure pirPin/GPIO33 as ext0 wake up source for HIGH logic level
    esp_sleep_enable_ext0_wakeup(pirPin, 1);
    
    //Set timer to TIME_TO_SLEEP seconds // uncomment below to wake up every few seconds
    //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    if(SERIAL_DEBUG)
    {
    //Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
    }

    //Configure GPIO32 & GPIO33 as ext1 wake up source for HIGH logic level
    // esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);

    //Go to sleep now
    esp_deep_sleep_start();
#elif defined(ESP8266)
    //digitalWrite(LED_BUILTIN, HIGH); // BuiltIn LED OFF
    digitalWrite(ledPin, LOW);  // LED OFF
    digitalWrite(holdPin, LOW); // set GPIO 0 low this takes CH_PD & powers down the ESP
#endif
}

void sendData(bool motion)
{
    callbackCalled = false;
    sensorData.motion = motion;
    uint8_t bs[sizeof(sensorData)];
    memcpy(bs, &sensorData, sizeof(sensorData));
    esp_now_send(NULL, bs, sizeof(sensorData)); // NULL means send to all peers
    //esp_now_send(mac, bs, sizeof(sensorData)); // mac of peer
}

void setup()
{
    initPins();
    if(SERIAL_DEBUG)
    {
        Serial.begin(115200);
        Serial.println();
    }

    WiFi.mode(WIFI_STA); // Station mode for esp-now sensor node
    WiFi.disconnect();

    if(SERIAL_DEBUG)
    {
        Serial.printf("This mac: %s, ", WiFi.macAddress().c_str());
        Serial.printf("target mac: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        Serial.printf(", channel: %i\n", WIFI_CHANNEL);
    }

    if (esp_now_init() != 0)
    {
        if(SERIAL_DEBUG)
            Serial.println("*** ESP_Now init failed");
        gotoSleep();
    }

#ifdef ESP32
    WiFi.setTxPower(WIFI_POWER_19_5dBm); //max power
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        if(SERIAL_DEBUG) Serial.println("Failed to add peer");
        gotoSleep();
    }

    esp_now_register_send_cb([](const uint8_t *mac, esp_now_send_status_t sendStatus) {
        if(SERIAL_DEBUG)
        {
            Serial.print("\r\nLast Packet Send Status:\t");
            Serial.println(sendStatus == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
        }
        callbackCalled = true;
    });

#elif defined(ESP8266)
    WiFi.setOutputPower(20.5); //max power
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(mac, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0);

    esp_now_register_send_cb([](uint8_t *mac, uint8_t sendStatus) {
        if(SERIAL_DEBUG) Serial.printf("send_cb, send done, status = %i\n", sendStatus);
        callbackCalled = true;
    });
#endif

    memcpy(sensorData.friendlyName, FRIENDLY_NAME, strlen(FRIENDLY_NAME) + 1);
    memcpy(sensorData.deviceName, DEVICE_NAME_FULL, strlen(DEVICE_NAME_FULL) + 1);

    sendData(true);
    prevMillis = millis();
}

void loop()
{
    unsigned long currentMillis = millis();

    if (callbackCalled || (currentMillis - prevMillis > SEND_TIMEOUT))
    {
        if (!pir)
        { // if (pir) == 0, which its not first time through as we set it to "1" above
            gotoSleep();
        }
        else
        { // if (pir) == 0 is not true
            //digitalWrite(LED_BUILTIN, LOW);  // BuiltIn LED ON
            digitalWrite(ledPin, HIGH); // LED ON
            if(SERIAL_DEBUG) Serial.println(F("ON"));
            while (digitalRead(pirPin) == 1)
            { // read GPIO 12, while GPIO 12 = 1 is true, wait (delay below) & read again, when GPIO 2 = 1 is false skip delay & move on out of "while loop"
                delay(50);
                if(SERIAL_DEBUG) Serial.print(".");
            }
            pir = false;   // set the value of (pir) to 0
            sensorData.motion = false;
            sendData(false);
            delay(20); // wait 20 msec
        }
        prevMillis = millis();
    }
}