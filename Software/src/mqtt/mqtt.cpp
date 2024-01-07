#include <Arduino.h>
#include <WiFi.h>
#include "mqtt.h"
#include "../../USER_SETTINGS.h"

#include "../lib/knolleary-pubsubclient/PubSubClient.h"

const char* sub_strings[] = MQTT_SUBSCRIPTIONS;

WiFiClient espClient;
PubSubClient client(espClient);
#define MSG_BUFFER_SIZE (256)
char msg[MSG_BUFFER_SIZE];
int value = 0;
static unsigned long previousMillisUpdateVal;

#define SECONDS 5

#ifdef ENABLE_MQTT
/** Publish global values and call callbacks for specific modules */
void publish_values(void) {

    snprintf(msg, sizeof(msg),
           "{\n"
           "  \"SOC\": %.3f,\n"
           "  \"StateOfHealth\": %.3f,\n"
           "  \"temperature_min\": %.3f,\n"
           "  \"temperature_max\": %.3f,\n"
           "  \"cell_max_voltage\": %d,\n"
           "  \"cell_min_voltage\": %d\n"
           "}\n",
           ((float) SOC) / 100.0,
           ((float) StateOfHealth) / 100.0,
           ((float)((int16_t) temperature_min)) / 10.0,
           ((float)((int16_t) temperature_max)) / 10.0,
           cell_max_voltage,
           cell_min_voltage);
    client.publish("battery/info", msg);
    Serial.println(msg);
}

void setup_wifi(void) {
    uint8_t wifi_timeout = 0u;
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PSW);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        if(++wifi_timeout > 20u) {
            /* Timeout after 10 s */
            Serial.println("Could not connect to wifi, check settings in USER_SETTINGS.h");
            return;
        }
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

/* This is called whenever a subscribed topic changes (hopefully) */
void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

/* If we lose the connection, get it back and re-sub */
void reconnect() {
    // Loop until we're reconnected
    while (!client.connected())
    {
        if(WiFi.status() != WL_CONNECTED) {
            bool result = WiFi.reconnect();
            if (result == false) {
                break;
            }
        }
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "LilyGoClient-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PSW))
        {
            Serial.println("connected");
            
            for (int i = 0; i < sizeof(sub_strings) / sizeof(sub_strings[0]); i++) {
                client.subscribe(sub_strings[i]);
                Serial.println(sub_strings[i]);
            }
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}
#endif

void mqtt_setup(void) {
#ifdef ENABLE_MQTT
    setup_wifi();
    
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);

    previousMillisUpdateVal = millis();
#endif
}

void mqtt_loop(void) {
#ifdef ENABLE_MQTT
    // while(true) {
        reconnect();
        if(client.connected() && (WiFi.status() == WL_CONNECTED)) {
            client.loop();
            if (millis() - previousMillisUpdateVal >= 5000)  // Every 5s
            {
                previousMillisUpdateVal = millis();
                publish_values();  // Update values heading towards inverter. Prepare for sending on CAN, or write directly to Modbus.
            }
        }
        delay(1);
    // }
#endif
}
