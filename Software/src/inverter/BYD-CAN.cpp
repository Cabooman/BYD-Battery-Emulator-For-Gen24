#include "BYD-CAN.h"
#include "../lib/miwagner-ESP32-Arduino-CAN/CAN_config.h"
#include "../lib/miwagner-ESP32-Arduino-CAN/ESP32CAN.h"

/* Do not change code below unless you are sure what you are doing */
static unsigned long previousMillis2s = 0;   // will store last time a 2s CAN Message was send
static unsigned long previousMillis10s = 0;  // will store last time a 10s CAN Message was send
static unsigned long previousMillis60s = 0;  // will store last time a 60s CAN Message was send
static const int interval2s = 2000;          // interval (ms) at which send CAN Messages
static const int interval10s = 10000;        // interval (ms) at which send CAN Messages
static const int interval60s = 60000;        // interval (ms) at which send CAN Messages
static uint8_t char1_151 = 0;
static uint8_t char2_151 = 0;
static uint8_t char3_151 = 0;
static uint8_t char4_151 = 0;
static uint8_t char5_151 = 0;
static uint8_t char6_151 = 0;
static uint8_t char7_151 = 0;

//Startup messages
const CAN_frame_t BYD_250 = {
    .FIR = {.B =
                {
                    .DLC = 8,
                    .FF = CAN_frame_std,
                }},
    .MsgID = 0x250,
    .data = {0x03, 0x16, 0x00, 0x66, (uint8_t)((BATTERY_WH_MAX / 100) >> 8), (uint8_t)(BATTERY_WH_MAX / 100), 0x02,
             0x09}};  //3.16 FW , Capacity kWh byte4&5 (example 24kWh = 240)
const CAN_frame_t BYD_290 = {.FIR = {.B =
                                         {
                                             .DLC = 8,
                                             .FF = CAN_frame_std,
                                         }},
                             .MsgID = 0x290,
                             .data = {0x06, 0x37, 0x10, 0xD9, 0x00, 0x00, 0x00, 0x00}};
const CAN_frame_t BYD_2D0 = {.FIR = {.B =
                                         {
                                             .DLC = 8,
                                             .FF = CAN_frame_std,
                                         }},
                             .MsgID = 0x2D0,
                             .data = {0x00, 0x42, 0x59, 0x44, 0x00, 0x00, 0x00, 0x00}};  //BYD
const CAN_frame_t BYD_3D0_0 = {.FIR = {.B =
                                           {
                                               .DLC = 8,
                                               .FF = CAN_frame_std,
                                           }},
                               .MsgID = 0x3D0,
                               .data = {0x00, 0x42, 0x61, 0x74, 0x74, 0x65, 0x72, 0x79}};  //Battery
const CAN_frame_t BYD_3D0_1 = {.FIR = {.B =
                                           {
                                               .DLC = 8,
                                               .FF = CAN_frame_std,
                                           }},
                               .MsgID = 0x3D0,
                               .data = {0x01, 0x2D, 0x42, 0x6F, 0x78, 0x20, 0x50, 0x72}};  //-Box Pr
const CAN_frame_t BYD_3D0_2 = {.FIR = {.B =
                                           {
                                               .DLC = 8,
                                               .FF = CAN_frame_std,
                                           }},
                               .MsgID = 0x3D0,
                               .data = {0x02, 0x65, 0x6D, 0x69, 0x75, 0x6D, 0x20, 0x48}};  //emium H
const CAN_frame_t BYD_3D0_3 = {.FIR = {.B =
                                           {
                                               .DLC = 8,
                                               .FF = CAN_frame_std,
                                           }},
                               .MsgID = 0x3D0,
                               .data = {0x03, 0x56, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00}};  //VS
//Actual content messages
CAN_frame_t BYD_110 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x110,
                       .data = {0x01, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
CAN_frame_t BYD_150 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x150,
                       .data = {0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00}};
CAN_frame_t BYD_190 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x190,
                       .data = {0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}};
CAN_frame_t BYD_1D0 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x1D0,
                       .data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x08}};
CAN_frame_t BYD_210 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x210,
                       .data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

static int discharge_current = 0;
static int charge_current = 0;
static int initialDataSent = 0;
static int16_t temperature_average = 0;
static int16_t temp_min = 0;
static int16_t temp_max = 0;
static int inverter_voltage = 0;
static int inverter_SOC = 0;
static long inverter_timestamp = 0;

void update_values_can_byd() {  //This function maps all the values fetched from battery CAN to the correct CAN messages
  //Calculate values
  charge_current =
      ((max_target_charge_power * 10) / max_voltage);  //Charge power in W , max volt in V+1decimal (P=UI, solve for I)
  //The above calculation results in (30 000*10)/3700=81A
  charge_current = (charge_current * 10);  //Value needs a decimal before getting sent to inverter (81.0A)
  if (charge_current > MAXCHARGEAMP) {
    charge_current = MAXCHARGEAMP;  //Cap the value to the max allowed Amp. Some inverters cannot handle large values.
  }

  discharge_current = ((max_target_discharge_power * 10) /
                       max_voltage);  //Charge power in W , max volt in V+1decimal (P=UI, solve for I)
  //The above calculation results in (30 000*10)/3700=81A
  discharge_current = (discharge_current * 10);  //Value needs a decimal before getting sent to inverter (81.0A)
  if (discharge_current > MAXDISCHARGEAMP) {
    discharge_current =
        MAXDISCHARGEAMP;  //Cap the value to the max allowed Amp. Some inverters cannot handle large values.
  }

  temp_min = temperature_min;  //Convert from unsigned to signed
  temp_max = temperature_max;
  temperature_average = ((temp_max + temp_min) / 2);

  //Map values to CAN messages
  //Maxvoltage (eg 400.0V = 4000 , 16bits long)
  BYD_110.data.u8[0] = (max_voltage >> 8);
  BYD_110.data.u8[1] = (max_voltage & 0x00FF);
  //Minvoltage (eg 300.0V = 3000 , 16bits long)
  BYD_110.data.u8[2] = (min_voltage >> 8);
  BYD_110.data.u8[3] = (min_voltage & 0x00FF);
  //Maximum discharge power allowed (Unit: A+1)
  BYD_110.data.u8[4] = (discharge_current >> 8);
  BYD_110.data.u8[5] = (discharge_current & 0x00FF);
  //Maximum charge power allowed (Unit: A+1)
  BYD_110.data.u8[6] = (charge_current >> 8);
  BYD_110.data.u8[7] = (charge_current & 0x00FF);

  //SOC (100.00%)
  BYD_150.data.u8[0] = (SOC >> 8);
  BYD_150.data.u8[1] = (SOC & 0x00FF);
  //StateOfHealth (100.00%)
  BYD_150.data.u8[2] = (StateOfHealth >> 8);
  BYD_150.data.u8[3] = (StateOfHealth & 0x00FF);
  //Maximum charge power allowed (Unit: A+1)
  BYD_150.data.u8[4] = (charge_current >> 8);
  BYD_150.data.u8[5] = (charge_current & 0x00FF);
  //Maximum discharge power allowed (Unit: A+1)
  BYD_150.data.u8[6] = (discharge_current >> 8);
  BYD_150.data.u8[7] = (discharge_current & 0x00FF);

  //Voltage (ex 370.0)
  BYD_1D0.data.u8[0] = (battery_voltage >> 8);
  BYD_1D0.data.u8[1] = (battery_voltage & 0x00FF);
  //Current (ex 81.0A)
  BYD_1D0.data.u8[2] = (battery_current >> 8);
  BYD_1D0.data.u8[3] = (battery_current & 0x00FF);
  //Temperature average
  BYD_1D0.data.u8[4] = (temperature_average >> 8);
  BYD_1D0.data.u8[5] = (temperature_average & 0x00FF);

  //Temperature max
  BYD_210.data.u8[0] = (temperature_max >> 8);
  BYD_210.data.u8[1] = (temperature_max & 0x00FF);
  //Temperature min
  BYD_210.data.u8[2] = (temperature_min >> 8);
  BYD_210.data.u8[3] = (temperature_min & 0x00FF);

#ifdef DEBUG_VIA_USB
  if (char1_151 != 0) {
    Serial.print("Detected inverter: ");
    Serial.print((char)char1_151);
    Serial.print((char)char2_151);
    Serial.print((char)char3_151);
    Serial.print((char)char4_151);
    Serial.print((char)char5_151);
    Serial.print((char)char6_151);
    Serial.println((char)char7_151);
  }
#endif
}

void receive_can_byd(CAN_frame_t rx_frame) {
  switch (rx_frame.MsgID) {
    case 0x151:  //Message originating from BYD HVS compatible inverter. Reply with CAN identifier!
      if (rx_frame.data.u8[0] & 0x01) {  //Battery requests identification
        send_intial_data();
      } else {  // We can identify what inverter type we are connected to
        char1_151 = rx_frame.data.u8[1];
        char2_151 = rx_frame.data.u8[2];
        char3_151 = rx_frame.data.u8[3];
        char4_151 = rx_frame.data.u8[4];
        char5_151 = rx_frame.data.u8[5];
        char6_151 = rx_frame.data.u8[6];
        char7_151 = rx_frame.data.u8[7];
      }
      break;
    case 0x091:
      inverter_voltage = ((rx_frame.data.u8[1] << 8) | rx_frame.data.u8[0]) * 0.1;
      break;
    case 0x0D1:
      inverter_SOC = ((rx_frame.data.u8[1] << 8) | rx_frame.data.u8[0]) * 0.1;
      break;
    case 0x111:
      inverter_timestamp = ((rx_frame.data.u8[3] << 24) | (rx_frame.data.u8[2] << 16) | (rx_frame.data.u8[1] << 8) |
                            rx_frame.data.u8[0]);
      break;
    default:
      break;
  }
}

void send_can_byd() {
  unsigned long currentMillis = millis();
  // Send initial CAN data once on bootup
  if (!initialDataSent) {
    send_intial_data();
    initialDataSent = 1;
  }

  // Send 2s CAN Message
  if (currentMillis - previousMillis2s >= interval2s) {
    previousMillis2s = currentMillis;

    ESP32Can.CANWriteFrame(&BYD_110);
  }
  // Send 10s CAN Message
  if (currentMillis - previousMillis10s >= interval10s) {
    previousMillis10s = currentMillis;

    ESP32Can.CANWriteFrame(&BYD_150);
    ESP32Can.CANWriteFrame(&BYD_1D0);
    ESP32Can.CANWriteFrame(&BYD_210);
  }
  //Send 60s message
  if (currentMillis - previousMillis60s >= interval60s) {
    previousMillis60s = currentMillis;

    ESP32Can.CANWriteFrame(&BYD_190);
  }
}

void send_intial_data() {
  ESP32Can.CANWriteFrame(&BYD_250);
  ESP32Can.CANWriteFrame(&BYD_290);
  ESP32Can.CANWriteFrame(&BYD_2D0);
  ESP32Can.CANWriteFrame(&BYD_3D0_0);
  ESP32Can.CANWriteFrame(&BYD_3D0_1);
  ESP32Can.CANWriteFrame(&BYD_3D0_2);
  ESP32Can.CANWriteFrame(&BYD_3D0_3);
}
