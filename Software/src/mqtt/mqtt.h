#ifndef MQTT_H
#define MQTT_H

extern uint16_t SOC;
extern uint16_t StateOfHealth;
extern uint16_t temperature_min;   //C+1,  Goes thru convert2unsignedint16 function (15.0C = 150, -15.0C =  65385)
extern uint16_t temperature_max;   //C+1,  Goes thru convert2unsignedint16 function (15.0C = 150, -15.0C =  65385)
extern uint16_t cell_max_voltage;  //mV,   0-4350
extern uint16_t cell_min_voltage;  //mV,   0-4350

void mqtt_setup(void);
void mqtt_loop(void);

#endif
