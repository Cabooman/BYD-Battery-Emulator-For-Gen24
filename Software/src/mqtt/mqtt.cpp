#include <Arduino.h>
#include <WiFi.h>
#include "mqtt.h"
#include "USER_SETTINGS.h"

#include "../lib/knolleary-pubsubclient/PubSubClient.h"

const char* sub_strings[] = MQTT_SUBSCRIPTIONS;

WiFiClient espClient;
PubSubClient client(espClient);
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
static unsigned long previousMillisUpdateVal;

#define SECONDS 5

#ifdef ENABLE_MQTT
void publish_values(void) {
    static uint16_t SOC_Previous = SOC;
    static uint16_t StateOfHealth_Previous = StateOfHealth;
    static uint16_t temperature_min_Previous = temperature_min;   //C+1,  Goes thru convert2unsignedint16 function (15.0C = 150, -15.0C =  65385)
    static uint16_t temperature_max_Previous = temperature_max;   //C+1,  Goes thru convert2unsignedint16 function (15.0C = 150, -15.0C =  65385)
    static uint16_t cell_max_voltage_Previous = cell_max_voltage;  //mV,   0-4350
    static uint16_t cell_min_voltage_Previous = cell_min_voltage;  //mV,   0-4350
    static uint8_t cnt = 0u; // Counter to "force update" with some interval regardless of change in the underlying variable

    /** This part below should be re-written to pack a JSON string instead, but I'm keeping this since it works as is. Some MQTT stuff in HA:
     *  configuration.yaml:
     *  mqtt: !include mqtt.yaml
     * 
     *  mqtt.yaml:
     *  sensor:
     *    - name: "Cell max"
     *      state_topic: "battery/info/cell_max_voltage"
     *      value_template: "{{ value.split()[1] | int }}"
     *    - name: "Temperature max"
     *      state_topic: "battery/info/temperature_max"
     *      value_template: "{{ value.split()[1] | round(1) }}"
    */

    if((SOC != SOC_Previous) || (cnt % (10 * SECONDS) == 0u)) {
        snprintf(msg, MSG_BUFFER_SIZE, "SOC: %.1f", ((float)SOC) / 100.0);
        client.publish("battery/info/SOC", msg);
        Serial.println(msg);
        SOC_Previous = SOC;
    }
    if((StateOfHealth != StateOfHealth_Previous) || (cnt % (10 * SECONDS) == 0u)) {
        snprintf(msg, MSG_BUFFER_SIZE, "StateOfHealth: %d", ((float)StateOfHealth) / 100);
        client.publish("battery/info/StateOfHealth", msg);
        Serial.println(msg);
        StateOfHealth_Previous = StateOfHealth;
    }
    if((temperature_min != temperature_min_Previous) || (cnt % (10 * SECONDS) == 0u)) {
        snprintf(msg, MSG_BUFFER_SIZE, "temperature_min: %.1f", ((float)((int16_t)temperature_min)) / 10.0);
        client.publish("battery/info/temperature_min", msg);
        Serial.println(msg);
        temperature_min_Previous = temperature_min;
    }
    if((temperature_max != temperature_max_Previous) || (cnt % (10 * SECONDS) == 0u)) {
        snprintf(msg, MSG_BUFFER_SIZE, "temperature_max: %.1f", ((float)((int16_t)temperature_max)) / 10.0);
        client.publish("battery/info/temperature_max", msg);
        Serial.println(msg);
        temperature_max_Previous = temperature_max;
    }
    if((cell_max_voltage != cell_max_voltage_Previous) || (cnt % (10 * SECONDS) == 0u)) {
        snprintf(msg, MSG_BUFFER_SIZE, "cell_max_voltage: %d mV", cell_max_voltage);
        client.publish("battery/info/cell_max_voltage", msg);
        Serial.println(msg);
        cell_max_voltage_Previous = cell_max_voltage;
    }
    if((cell_min_voltage != cell_min_voltage_Previous) || (cnt % (10 * SECONDS) == 0u)) {
        snprintf(msg, MSG_BUFFER_SIZE, "cell_min_voltage: %d mV", cell_min_voltage);
        client.publish("battery/info/cell_min_voltage", msg);
        Serial.println(msg);
        cell_min_voltage_Previous = cell_min_voltage;
    }
    
    cnt++;
}

void setup_wifi(void) {
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
    reconnect();
    client.loop();
    if (millis() - previousMillisUpdateVal >= 200)  // Every 0.2s
    {
        previousMillisUpdateVal = millis();
        publish_values();  // Update values heading towards inverter. Prepare for sending on CAN, or write directly to Modbus.
    }
#endif
}
