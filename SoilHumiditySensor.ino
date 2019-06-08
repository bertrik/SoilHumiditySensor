#include <stdio.h>

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

#define PIN_GND     D5
#define PIN_VCC     D0
#define PIN_AOUT    A0

#define MQTT_HOST   "mqtt.space.hackalot.nl"
#define MQTT_PORT   1883
// MQTT scheme: prefix/type/id/property
// property 'status' can indicate 'online' or 'offline'
#define MQTT_TOPIC  "hackalot/plant/%s/%s"

#define LOG_PERIOD 10000

static char esp_id[16];
static WiFiManager wifiManager;
static WiFiClient espClient;
static PubSubClient mqttClient(espClient);
static char statustopic[64];
static char valuetopic[64];

static bool mqtt_send(const char *statustopic, const char *topic, const char *value, bool retained)
{
    bool result = false;
    if (!mqttClient.connected()) {
        Serial.print("Connecting MQTT...");
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        result = mqttClient.connect(esp_id, statustopic, 0, retained, "offline");
        if (result) {
            result = mqttClient.publish(statustopic, "online", retained);
        }
        Serial.println(result ? "OK" : "FAIL");
    }
    if (mqttClient.connected()) {
        Serial.print("Publishing ");
        Serial.print(value);
        Serial.print(" to ");
        Serial.print(topic);
        Serial.print("...");
        result = mqttClient.publish(topic, value, retained);
        Serial.println(result ? "OK" : "FAIL");
    }
    return result;
}

void setup(void)
{
    Serial.begin(115200);

    pinMode(PIN_GND, OUTPUT);
    digitalWrite(PIN_GND, 0);
    pinMode(PIN_VCC, OUTPUT);
    digitalWrite(PIN_VCC, 1);
    pinMode(PIN_AOUT, INPUT);

    // get ESP id
    sprintf(esp_id, "%06X", ESP.getChipId());
    Serial.print("ESP ID: ");
    Serial.println(esp_id);

    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);

    wifiManager.setConfigPortalTimeout(120);
    wifiManager.autoConnect("ESP-PLANT");

    snprintf(statustopic, sizeof(statustopic), MQTT_TOPIC, esp_id, "status");
    snprintf(valuetopic, sizeof(valuetopic), MQTT_TOPIC, esp_id, "humidity");
}


void loop(void)
{
    static unsigned long last_sent = 0;

    unsigned long ms = millis();
    if ((ms - last_sent) > LOG_PERIOD) {
        last_sent = ms;

        // https://www.domoticz.com/forum/viewtopic.php?t=24116
        float humidity = (840 - analogRead(PIN_AOUT)) / 425.0 * 100.0;
        char value[16];
        snprintf(value, sizeof(value), "%.1f", humidity);
        if (!mqtt_send(statustopic, valuetopic, value, true)) {
            Serial.println("Restarting ESP...");
            ESP.restart();
        }
    }

    mqttClient.loop();
}

