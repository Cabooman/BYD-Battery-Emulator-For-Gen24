#include "SMA-CAN.h"
#include "../lib/miwagner-ESP32-Arduino-CAN/CAN_config.h"
#include "../lib/miwagner-ESP32-Arduino-CAN/ESP32CAN.h"

/* Do not change code below unless you are sure what you are doing */
static unsigned long previousMillis100ms = 0;  // will store last time a 100ms CAN Message was send
static const int interval100ms = 100;          // interval (ms) at which send CAN Messages

//Actual content messages
static const CAN_frame_t SMA_558 = {
    .FIR = {.B =
                {
                    .DLC = 8,
                    .FF = CAN_frame_std,
                }},
    .MsgID = 0x558,
    .data = {0x03, 0x12, 0x00, 0x04, 0x00, 0x59, 0x07, 0x07}};  //7x BYD modules, Vendor ID 7 BYD
static const CAN_frame_t SMA_598 = {
    .FIR = {.B =
                {
                    .DLC = 8,
                    .FF = CAN_frame_std,
                }},
    .MsgID = 0x598,
    .data = {0x00, 0x00, 0x12, 0x34, 0x5A, 0xDE, 0x07, 0x4F}};  //B0-4 Serial, rest unknown
static const CAN_frame_t SMA_5D8 = {.FIR = {.B =
                                                {
                                                    .DLC = 8,
                                                    .FF = CAN_frame_std,
                                                }},
                                    .MsgID = 0x5D8,
                                    .data = {0x00, 0x42, 0x59, 0x44, 0x00, 0x00, 0x00, 0x00}};  //B Y D
static const CAN_frame_t SMA_618_1 = {.FIR = {.B =
                                                  {
                                                      .DLC = 8,
                                                      .FF = CAN_frame_std,
                                                  }},
                                      .MsgID = 0x618,
                                      .data = {0x00, 0x42, 0x61, 0x74, 0x74, 0x65, 0x72, 0x79}};  //0 B A T T E R Y
static const CAN_frame_t SMA_618_2 = {.FIR = {.B =
                                                  {
                                                      .DLC = 8,
                                                      .FF = CAN_frame_std,
                                                  }},
                                      .MsgID = 0x618,
                                      .data = {0x01, 0x2D, 0x42, 0x6F, 0x78, 0x20, 0x48, 0x39}};  //1 - B O X   H
static const CAN_frame_t SMA_618_3 = {.FIR = {.B =
                                                  {
                                                      .DLC = 8,
                                                      .FF = CAN_frame_std,
                                                  }},
                                      .MsgID = 0x618,
                                      .data = {0x02, 0x2E, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00}};  //2 - 0
CAN_frame_t SMA_358 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x358,
                       .data = {0x0F, 0x6C, 0x06, 0x20, 0x00, 0x00, 0x00, 0x00}};
CAN_frame_t SMA_3D8 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x3D8,
                       .data = {0x04, 0x10, 0x27, 0x10, 0x00, 0x18, 0xF9, 0x00}};
CAN_frame_t SMA_458 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x458,
                       .data = {0x00, 0x00, 0x06, 0x75, 0x00, 0x00, 0x05, 0xD6}};
CAN_frame_t SMA_518 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x518,
                       .data = {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF}};
CAN_frame_t SMA_4D8 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x4D8,
                       .data = {0x09, 0xFD, 0x00, 0x00, 0x00, 0xA8, 0x02, 0x08}};
CAN_frame_t SMA_158 = {.FIR = {.B =
                                   {
                                       .DLC = 8,
                                       .FF = CAN_frame_std,
                                   }},
                       .MsgID = 0x158,
                       .data = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0x6A, 0xAA, 0xAA}};

static int discharge_current = 0;
static int charge_current = 0;
static int temperature_average = 0;
static int ampere_hours_remaining = 0;

void update_values_can_sma() {  //This function maps all the values fetched from battery CAN to the correct CAN messages
  //Calculate values
  charge_current =
      ((max_target_charge_power * 10) / max_voltage);  //Charge power in W , max volt in V+1decimal (P=UI, solve for I)
  //The above calculation results in (30 000*10)/3700=81A
  charge_current = (charge_current * 10);  //Value needs a decimal before getting sent to inverter (81.0A)

  discharge_current = ((max_target_discharge_power * 10) /
                       max_voltage);  //Charge power in W , max volt in V+1decimal (P=UI, solve for I)
  //The above calculation results in (30 000*10)/3700=81A
  discharge_current = (discharge_current * 10);  //Value needs a decimal before getting sent to inverter (81.0A)

  temperature_average = ((temperature_max + temperature_min) / 2);

  ampere_hours_remaining =
      ((remaining_capacity_Wh / battery_voltage) * 100);  //(WH[10000] * V+1[3600])*100 = 270 (27.0Ah)

  //Map values to CAN messages
  //Maxvoltage (eg 400.0V = 4000 , 16bits long)
  SMA_358.data.u8[0] = (max_voltage >> 8);
  SMA_358.data.u8[1] = (max_voltage & 0x00FF);
  //Minvoltage (eg 300.0V = 3000 , 16bits long)
  SMA_358.data.u8[2] = (min_voltage >> 8);  //Minvoltage behaves strange on SMA, cuts out at 56% of the set value?
  SMA_358.data.u8[3] = (min_voltage & 0x00FF);
  //Discharge limited current, 500 = 50A, (0.1, A)
  SMA_358.data.u8[4] = (discharge_current >> 8);
  SMA_358.data.u8[5] = (discharge_current & 0x00FF);
  //Charge limited current, 125 =12.5A (0.1, A)
  SMA_358.data.u8[6] = (charge_current >> 8);
  SMA_358.data.u8[7] = (charge_current & 0x00FF);

  //SOC (100.00%)
  SMA_3D8.data.u8[0] = (SOC >> 8);
  SMA_3D8.data.u8[1] = (SOC & 0x00FF);
  //StateOfHealth (100.00%)
  SMA_3D8.data.u8[2] = (StateOfHealth >> 8);
  SMA_3D8.data.u8[3] = (StateOfHealth & 0x00FF);
  //State of charge (AH, 0.1)
  SMA_3D8.data.u8[4] = (ampere_hours_remaining >> 8);
  SMA_3D8.data.u8[5] = (ampere_hours_remaining & 0x00FF);

  //Voltage (370.0)
  SMA_4D8.data.u8[0] = (battery_voltage >> 8);
  SMA_4D8.data.u8[1] = (battery_voltage & 0x00FF);
  //Current (TODO: signed OK?)
  SMA_4D8.data.u8[2] = (battery_current >> 8);
  SMA_4D8.data.u8[3] = (battery_current & 0x00FF);
  //Temperature average
  SMA_4D8.data.u8[4] = (temperature_average >> 8);
  SMA_4D8.data.u8[5] = (temperature_average & 0x00FF);

  //Error bits
  //SMA_158.data.u8[0] = //bit12 Fault high temperature, bit34Battery cellundervoltage, bit56 Battery cell overvoltage, bit78 batterysystemdefect
  //TODO: add all error bits. Sending message with all 0xAA until that.
}

void receive_can_sma(CAN_frame_t rx_frame) {
  switch (rx_frame.MsgID) {
    case 0x360:  //Message originating from SMA inverter - Voltage and current
      //Frame0-1 Voltage
      //Frame2-3 Current
      break;
    case 0x420:  //Message originating from SMA inverter - Timestamp
      //Frame0-3 Timestamp
      break;
    case 0x660:  //Message originating from SMA inverter
      break;
    case 0x5E0:  //Message originating from SMA inverter
      break;
    case 0x560:  //Message originating from SMA inverter
      break;
    default:
      break;
  }
}

void send_can_sma() {
  unsigned long currentMillis = millis();

  // Send CAN Message every 100ms
  if (currentMillis - previousMillis100ms >= interval100ms) {
    previousMillis100ms = currentMillis;

    ESP32Can.CANWriteFrame(&SMA_558);
    ESP32Can.CANWriteFrame(&SMA_598);
    ESP32Can.CANWriteFrame(&SMA_5D8);
    ESP32Can.CANWriteFrame(&SMA_618_1);
    ESP32Can.CANWriteFrame(&SMA_618_2);
    ESP32Can.CANWriteFrame(&SMA_618_3);
    ESP32Can.CANWriteFrame(&SMA_358);
    ESP32Can.CANWriteFrame(&SMA_3D8);
    ESP32Can.CANWriteFrame(&SMA_458);
    ESP32Can.CANWriteFrame(&SMA_518);
    ESP32Can.CANWriteFrame(&SMA_4D8);
    ESP32Can.CANWriteFrame(&SMA_158);
  }
}
