//SERIAL-LINK-TRANSMITTER-INVERTER.cpp

#include "SERIAL-LINK-TRANSMITTER-INVERTER.h"
#include "../devboard/utils/events.h"

/*
*         SerialDataLink
*           txid=1, rxid=0 gives this the startup transmit priority_queue
*           Will transmit max 16 int variable - receive none
*/

#define BATTERY_SEND_NUM_VARIABLES 17
#define BATTERY_RECV_NUM_VARIABLES 1

#ifdef BATTERY_RECV_NUM_VARIABLES
const uint8_t receivingNumVariables = BATTERY_RECV_NUM_VARIABLES;
#else
const uint8_t receivingNumVariables = 0;
#endif

//                                     txid,rxid,num_tx,num_rx
SerialDataLink dataLinkTransmit(Serial2, 0x01, 0, BATTERY_SEND_NUM_VARIABLES, receivingNumVariables);

void printSendingValues();

void _getData() {
  inverterAllowsContactorClosing = dataLinkTransmit.getReceivedData(0);
  //var2 = dataLinkTransmit.getReceivedData(1); // For future expansion,
  //var3 = dataLinkTransmit.getReceivedData(2); // if inverter needs to send data to battery
}

void manageSerialLinkTransmitter() {
  static bool initLink = false;
  static unsigned long updateTime = 0;
  static bool lasterror = false;
  //static unsigned long lastNoError = 0;
  static unsigned long transmitGoodSince = 0;
  static unsigned long lastGood = 0;

  unsigned long currentTime = millis();

  dataLinkTransmit.run();

#ifdef BATTERY_RECV_NUM_VARIABLES
  bool readError = dataLinkTransmit.checkReadError(true);  // check for error & clear error flag
  if (dataLinkTransmit.checkNewData(true))                 // true = clear Flag
  {
    _getData();
  }
#endif

  if (currentTime - updateTime > 100) {
    updateTime = currentTime;
    if (!initLink) {
      initLink = true;
      transmitGoodSince = currentTime;
      // sends variables every 5000mS even if no change
      dataLinkTransmit.setUpdateInterval(10000);
    }
    bool sendError = dataLinkTransmit.checkTransmissionError(true);
    if (sendError) {
      Serial.print(currentTime);
      Serial.println(" - ERROR: Serial Data Link - SEND Error");
      lasterror = true;
      transmitGoodSince = currentTime;
    }

    /* new feature */
    /* @getLastAcknowledge(bool resetFlag)
     *    - returns: 
     *      -2 NACK received from receiver
     *      -1 no ACK received 
     *      0  no activity 
     *      1  ACK received 
     *      resetFlag = true will clear to 0
     */

    int ackReceived = dataLinkTransmit.getLastAcknowledge(true);
    if (ackReceived > 0)
      lastGood = currentTime;

    if (lasterror && (ackReceived > 0)) {
      lasterror = false;
      Serial.print(currentTime);
      Serial.println(" - RECOVERY: Serial Data Link - Send GOOD");
    }

    //--- reporting every 60 seconds that transmission is good
    if (currentTime - transmitGoodSince > 60000) {
      transmitGoodSince = currentTime;
      Serial.print(currentTime);
      Serial.println(" - Transmit Good");
//  printUsefullData();
#ifdef DEBUG_VIA_USB
      void printSendingValues();
#endif
    }

    //--- report that Errors been ocurring for > 60 seconds
    if (currentTime - lastGood > 60000)  // 60 seconds
    {
      lastGood = currentTime;
      Serial.print(currentTime);
      Serial.println(" - Transmit Failed : 60 seconds");
      //  print the max_ data
      Serial.println("SerialDataLink : bms_status=4");
      Serial.println("SerialDataLink : LEDcolor = RED");
      Serial.println("SerialDataLink : max_target_discharge_power = 0");
      Serial.println("SerialDataLink : max_target_charge_power = 0");

      max_target_discharge_power = 0;
      max_target_charge_power = 0;
      set_event(EVENT_SERIAL_TX_FAILURE, 0);
      // throw error
    }
    /*
		//  lastMessageReceived from CAN bus (Battery)
		if (currentTime - lastMessageReceived > (5 * 60000) )  // 5 minutes
		{
			Serial.print(millis());
			Serial.println(" - Data Stale : 5 minutes");
			// throw error
			
			// stop transmitting until fresh
		}
		*/

    static unsigned long updateDataTime = 0;

    if (currentTime - updateDataTime > 999) {
      updateDataTime = currentTime;
      dataLinkTransmit.updateData(0, SOC);
      dataLinkTransmit.updateData(1, StateOfHealth);
      dataLinkTransmit.updateData(2, battery_voltage);
      dataLinkTransmit.updateData(3, battery_current);
      dataLinkTransmit.updateData(4, capacity_Wh);
      dataLinkTransmit.updateData(5, remaining_capacity_Wh);
      dataLinkTransmit.updateData(6, max_target_discharge_power);
      dataLinkTransmit.updateData(7, max_target_charge_power);
      dataLinkTransmit.updateData(8, bms_status);
      dataLinkTransmit.updateData(9, bms_char_dis_status);
      dataLinkTransmit.updateData(10, stat_batt_power);
      dataLinkTransmit.updateData(11, temperature_min);
      dataLinkTransmit.updateData(12, temperature_max);
      dataLinkTransmit.updateData(13, cell_max_voltage);
      dataLinkTransmit.updateData(14, cell_min_voltage);
      dataLinkTransmit.updateData(15, (int16_t)LFP_Chemistry);
      dataLinkTransmit.updateData(16, batteryAllowsContactorClosing);
    }
  }
}

void printSendingValues() {
  Serial.println("Values from battery: ");
  Serial.print("SOC: ");
  Serial.print(SOC);
  Serial.print(" SOH: ");
  Serial.print(StateOfHealth);
  Serial.print(" Voltage: ");
  Serial.print(battery_voltage);
  Serial.print(" Current: ");
  Serial.print(battery_current);
  Serial.print(" Capacity: ");
  Serial.print(capacity_Wh);
  Serial.print(" Remain cap: ");
  Serial.print(remaining_capacity_Wh);
  Serial.print(" Max discharge W: ");
  Serial.print(max_target_discharge_power);
  Serial.print(" Max charge W: ");
  Serial.print(max_target_charge_power);
  Serial.print(" BMS status: ");
  Serial.print(bms_status);
  Serial.print(" BMS status dis/cha: ");
  Serial.print(bms_char_dis_status);
  Serial.print(" Power: ");
  Serial.print(stat_batt_power);
  Serial.print(" Temp min: ");
  Serial.print(temperature_min);
  Serial.print(" Temp max: ");
  Serial.print(temperature_max);
  Serial.print(" Cell max: ");
  Serial.print(cell_max_voltage);
  Serial.print(" Cell min: ");
  Serial.print(cell_min_voltage);
  Serial.print(" LFP : ");
  Serial.print(LFP_Chemistry);
  Serial.print(" batteryAllowsContactorClosing: ");
  Serial.print(batteryAllowsContactorClosing);
  Serial.print(" inverterAllowsContactorClosing: ");
  Serial.print(inverterAllowsContactorClosing);

  Serial.println("");
}
