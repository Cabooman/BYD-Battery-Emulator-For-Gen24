#include "TESLA-MODEL-3-BATTERY.h"
#include "../devboard/utils/events.h"
#include "../lib/miwagner-ESP32-Arduino-CAN/CAN_config.h"
#include "../lib/miwagner-ESP32-Arduino-CAN/ESP32CAN.h"

/* Do not change code below unless you are sure what you are doing */
/* Credits: Most of the code comes from Per Carlen's bms_comms_tesla_model3.py (https://gitlab.com/pelle8/batt2gen24/) */

static unsigned long previousMillis30 = 0;  // will store last time a 30ms CAN Message was send
static const int interval30 = 30;           // interval (ms) at which send CAN Messages
static uint8_t stillAliveCAN = 6;           //counter for checking if CAN is still alive

CAN_frame_t TESLA_221_1 = {
    .FIR = {.B =
                {
                    .DLC = 8,
                    .FF = CAN_frame_std,
                }},
    .MsgID = 0x221,
    .data = {0x41, 0x11, 0x01, 0x00, 0x00, 0x00, 0x20, 0x96}};  //Contactor frame 221 - close contactors
CAN_frame_t TESLA_221_2 = {
    .FIR = {.B =
                {
                    .DLC = 8,
                    .FF = CAN_frame_std,
                }},
    .MsgID = 0x221,
    .data = {0x61, 0x15, 0x01, 0x00, 0x00, 0x00, 0x20, 0xBA}};  //Contactor Frame 221 - hv_up_for_drive

static uint32_t total_discharge = 0;
static uint32_t total_charge = 0;
static uint16_t volts = 0;     // V
static int16_t amps = 0;       // A
static int16_t power = 0;      // W
static uint16_t raw_amps = 0;  // A
static int16_t max_temp = 0;   // C*
static int16_t min_temp = 0;   // C*
static uint16_t energy_buffer = 0;
static uint16_t energy_to_charge_complete = 0;
static uint16_t expected_energy_remaining = 0;
static uint8_t full_charge_complete = 0;
static uint16_t ideal_energy_remaining = 0;
static uint16_t nominal_energy_remaining = 0;
static uint16_t nominal_full_pack_energy = 600;
static uint16_t bat_beginning_of_life = 600;
static uint16_t battery_charge_time_remaining = 0;  // Minutes
static uint16_t regenerative_limit = 0;
static uint16_t discharge_limit = 0;
static uint16_t max_heat_park = 0;
static uint16_t hvac_max_power = 0;
static uint16_t min_voltage = 0;
static uint16_t max_discharge_current = 0;
static uint16_t max_charge_current = 0;
static uint16_t max_voltage = 0;
static uint16_t high_voltage = 0;
static uint16_t low_voltage = 0;
static uint16_t output_current = 0;
static uint16_t soc_min = 0;
static uint16_t soc_max = 0;
static uint16_t soc_vi = 0;
static uint16_t soc_calculated = 0;
static uint16_t soc_ave = 0;
static uint16_t cell_max_v = 3700;
static uint16_t cell_min_v = 3700;
static uint16_t cell_deviation_mV = 0;  //contains the deviation between highest and lowest cell in mV
static uint8_t max_vno = 0;
static uint8_t min_vno = 0;
static uint8_t contactor = 0;  //State of contactor
static uint8_t hvil_status = 0;
static uint8_t packContNegativeState = 0;
static uint8_t packContPositiveState = 0;
static uint8_t packContactorSetState = 0;
static uint8_t packCtrsClosingAllowed = 0;
static uint8_t pyroTestInProgress = 0;
static uint8_t send221still = 10;
//Fault codes
static uint8_t WatchdogReset = 0;           //Warns if the processor has experienced a reset due to watchdog reset.
static uint8_t PowerLossReset = 0;          //Warns if the processor has experienced a reset due to power loss.
static uint8_t SwAssertion = 0;             //An internal software assertion has failed.
static uint8_t CrashEvent = 0;              //Warns if the crash signal is detected by HVP
static uint8_t OverDchgCurrentFault = 0;    //Warns if the pack discharge is above max discharge current limit
static uint8_t OverChargeCurrentFault = 0;  //Warns if the pack discharge current is above max charge current limit
static uint8_t OverCurrentFault = 0;      //Warns if the pack current (discharge or charge) is above max current limit.
static uint8_t OverTemperatureFault = 0;  //A pack module temperature is above maximum temperature limit
static uint8_t OverVoltageFault = 0;      //A brick voltage is above maximum voltage limit
static uint8_t UnderVoltageFault = 0;     //A brick voltage is below minimum voltage limit
static uint8_t PrimaryBmbMiaFault = 0;    //Warns if the voltage and temperature readings from primary BMB chain are mia
static uint8_t SecondaryBmbMiaFault =
    0;  //Warns if the voltage and temperature readings from secondary BMB chain are mia
static uint8_t BmbMismatchFault =
    0;                               //Warns if the primary and secondary BMB chain readings don't match with each other
static uint8_t BmsHviMiaFault = 0;   //Warns if the BMS node is mia on HVS or HVI CAN
static uint8_t CpMiaFault = 0;       //Warns if the CP node is mia on HVS CAN
static uint8_t PcsMiaFault = 0;      //The PCS node is mia on HVS CAN
static uint8_t BmsFault = 0;         //Warns if the BMS ECU has faulted
static uint8_t PcsFault = 0;         //Warns if the PCS ECU has faulted
static uint8_t CpFault = 0;          //Warns if the CP ECU has faulted
static uint8_t ShuntHwMiaFault = 0;  //Warns if the shunt current reading is not available
static uint8_t PyroMiaFault = 0;     //Warns if the pyro squib is not connected
static uint8_t hvsMiaFault = 0;      //Warns if the pack contactor hw fault
static uint8_t hviMiaFault = 0;      //Warns if the FC contactor hw fault
static uint8_t Supply12vFault = 0;   //Warns if the low voltage (12V) battery is below minimum voltage threshold
static uint8_t VerSupplyFault = 0;   //Warns if the Energy reserve voltage supply is below minimum voltage threshold
static uint8_t HvilFault = 0;        //Warn if a High Voltage Inter Lock fault is detected
static uint8_t BmsHvsMiaFault = 0;   //Warns if the BMS node is mia on HVS or HVI CAN
static uint8_t PackVoltMismatchFault =
    0;                           //Warns if the pack voltage doesn't match approximately with sum of brick voltages
static uint8_t EnsMiaFault = 0;  //Warns if the ENS line is not connected to HVC
static uint8_t PackPosCtrArcFault = 0;  //Warns if the HVP detectes series arc at pack contactor
static uint8_t packNegCtrArcFault = 0;  //Warns if the HVP detectes series arc at FC contactor
static uint8_t ShuntHwAndBmsMiaFault = 0;
static uint8_t fcContHwFault = 0;
static uint8_t robinOverVoltageFault = 0;
static uint8_t packContHwFault = 0;
static uint8_t pyroFuseBlown = 0;
static uint8_t pyroFuseFailedToBlow = 0;
static uint8_t CpilFault = 0;
static uint8_t PackContactorFellOpen = 0;
static uint8_t FcContactorFellOpen = 0;
static uint8_t packCtrCloseBlocked = 0;
static uint8_t fcCtrCloseBlocked = 0;
static uint8_t packContactorForceOpen = 0;
static uint8_t fcContactorForceOpen = 0;
static uint8_t dcLinkOverVoltage = 0;
static uint8_t shuntOverTemperature = 0;
static uint8_t passivePyroDeploy = 0;
static uint8_t logUploadRequest = 0;
static uint8_t packCtrCloseFailed = 0;
static uint8_t fcCtrCloseFailed = 0;
static uint8_t shuntThermistorMia = 0;
static const char* contactorText[] = {"UNKNOWN(0)",  "OPEN",        "CLOSING",    "BLOCKED", "OPENING",
                                      "CLOSED",      "UNKNOWN(6)",  "WELDED",     "POS_CL",  "NEG_CL",
                                      "UNKNOWN(10)", "UNKNOWN(11)", "UNKNOWN(12)"};
static const char* contactorState[] = {"SNA",        "OPEN",       "PRECHARGE",   "BLOCKED",
                                       "PULLED_IN",  "OPENING",    "ECONOMIZED",  "WELDED",
                                       "UNKNOWN(8)", "UNKNOWN(9)", "UNKNOWN(10)", "UNKNOWN(11)"};
static const char* hvilStatusState[] = {"NOT OK",
                                        "STATUS_OK",
                                        "CURRENT_SOURCE_FAULT",
                                        "INTERNAL_OPEN_FAULT",
                                        "VEHICLE_OPEN_FAULT",
                                        "PENTHOUSE_LID_OPEN_FAULT",
                                        "UNKNOWN_LOCATION_OPEN_FAULT",
                                        "VEHICLE_NODE_FAULT",
                                        "NO_12V_SUPPLY",
                                        "VEHICLE_OR_PENTHOUSE_LID_OPENFAULT",
                                        "UNKNOWN(10)",
                                        "UNKNOWN(11)",
                                        "UNKNOWN(12)",
                                        "UNKNOWN(13)",
                                        "UNKNOWN(14)",
                                        "UNKNOWN(15)"};

#define MAX_SOC 1000  //BMS never goes over this value. We use this info to rescale SOC% sent to inverter
#define MIN_SOC 0     //BMS never goes below this value. We use this info to rescale SOC% sent to inverter
#define MAX_CELL_VOLTAGE_NCA_NCM 4250   //Battery is put into emergency stop if one cell goes over this value
#define MIN_CELL_VOLTAGE_NCA_NCM 2950   //Battery is put into emergency stop if one cell goes below this value
#define MAX_CELL_DEVIATION_NCA_NCM 500  //LED turns yellow on the board if mv delta exceeds this value

#define MAX_CELL_VOLTAGE_LFP 3500   //Battery is put into emergency stop if one cell goes over this value
#define MIN_CELL_VOLTAGE_LFP 2800   //Battery is put into emergency stop if one cell goes below this value
#define MAX_CELL_DEVIATION_LFP 150  //LED turns yellow on the board if mv delta exceeds this value

#define REASONABLE_ENERGYAMOUNT 20  //When the BMS stops making sense on some values, they are always <20

void update_values_tesla_model_3_battery() {  //This function maps all the values fetched via CAN to the correct parameters used for modbus
  //After values are mapped, we perform some safety checks, and do some serial printouts
  //Calculate the SOH% to send to inverter
  if (bat_beginning_of_life != 0) {  //div/0 safeguard
    StateOfHealth =
        static_cast<uint16_t>((static_cast<double>(nominal_full_pack_energy) / bat_beginning_of_life) * 10000.0);
  }
  //If the value is unavailable, set SOH to 99%
  if (nominal_full_pack_energy < REASONABLE_ENERGYAMOUNT) {
    StateOfHealth = 9900;
  }

  //Calculate the SOC% value to send to inverter
  soc_calculated = soc_vi;
  soc_calculated = MIN_SOC + (MAX_SOC - MIN_SOC) * (soc_calculated - MINPERCENTAGE) / (MAXPERCENTAGE - MINPERCENTAGE);
  if (soc_calculated < 0) {  //We are in the real SOC% range of 0-20%, always set SOC sent to Inverter as 0%
    soc_calculated = 0;
  }
  if (soc_calculated > 1000) {  //We are in the real SOC% range of 80-100%, always set SOC sent to Inverter as 100%
    soc_calculated = 1000;
  }
  SOC = (soc_calculated * 10);  //increase SOC range from 0-100.0 -> 100.00

  battery_voltage = (volts * 10);  //One more decimal needed (370 -> 3700)

  battery_current = convert2unsignedInt16(amps);  //13.0A

  capacity_Wh = BATTERY_WH_MAX;  //Use the configured value to avoid overflows

  //Calculate the remaining Wh amount from SOC% and max Wh value.
  remaining_capacity_Wh = static_cast<uint16_t>((static_cast<double>(SOC) / 10000) * BATTERY_WH_MAX);

  // Define the allowed discharge power
  max_target_discharge_power = (max_discharge_current * volts);
  // Cap the allowed discharge power if battery is empty, or discharge power is higher than the maximum discharge power allowed
  if (SOC == 0) {
    max_target_discharge_power = 0;
  } else if (max_target_discharge_power > MAXDISCHARGEPOWERALLOWED) {
    max_target_discharge_power = MAXDISCHARGEPOWERALLOWED;
  }

  //The allowed charge power behaves strangely. We instead estimate this value
  if (SOC == 10000) {  // When scaled SOC is 100%, set allowed charge power to 0
    max_target_charge_power = 0;
  } else if (soc_vi > 950) {  // When real SOC is between 95-99.99%, ramp the value between Max<->0
    max_target_charge_power = MAXCHARGEPOWERALLOWED * (1 - (soc_vi - 950) / 50.0);
  } else {  // No limits, max charging power allowed
    max_target_charge_power = MAXCHARGEPOWERALLOWED;
  }

  power = ((volts / 10) * amps);
  stat_batt_power = convert2unsignedInt16(power);

  temperature_min = convert2unsignedInt16(min_temp);

  temperature_max = convert2unsignedInt16(max_temp);

  cell_max_voltage = cell_max_v;

  cell_min_voltage = cell_min_v;

  /* Value mapping is completed. Start to check all safeties */

  /* Check if the BMS is still sending CAN messages. If we go 60s without messages we raise an error*/
  if (!stillAliveCAN) {
    set_event(EVENT_CAN_RX_FAILURE, 0);
  } else {
    stillAliveCAN--;
    clear_event(EVENT_CAN_RX_FAILURE);
  }

  if (hvil_status == 3) {  //INTERNAL_OPEN_FAULT - Someone disconnected a high voltage cable while battery was in use
    set_event(EVENT_INTERNAL_OPEN_FAULT, 0);
  } else {
    clear_event(EVENT_INTERNAL_OPEN_FAULT);
  }

  cell_deviation_mV = (cell_max_v - cell_min_v);

  //Determine which chemistry battery pack is using (crude method, TODO: replace with real CAN data later)
  if (soc_vi > 900) {  //When SOC% is over 90.0%, we can use max cell voltage to estimate what chemistry is used
    if (cell_max_v < 3450) {
      LFP_Chemistry = true;
    }
    if (cell_max_v > 3700) {
      LFP_Chemistry = false;
    }
  }

  //Check if SOC% is plausible
  if (battery_voltage >
      (ABSOLUTE_MAX_VOLTAGE - 100)) {  // When pack voltage is close to max, and SOC% is still low, raise FAULT
    if (SOC < 6500) {                  //When SOC is less than 65.00% when approaching max voltage
      set_event(EVENT_SOC_PLAUSIBILITY_ERROR, SOC / 100);
    }
  }

  //Check if BMS is in need of recalibration
  if (nominal_full_pack_energy > 1 && nominal_full_pack_energy < REASONABLE_ENERGYAMOUNT) {
    Serial.println("Warning: kWh remaining " + String(nominal_full_pack_energy) +
                   " reported by battery not plausible. Battery needs cycling.");
    set_event(EVENT_KWH_PLAUSIBILITY_ERROR, nominal_full_pack_energy);
  } else if (nominal_full_pack_energy <= 1) {
    Serial.println("Info: kWh remaining battery is not reporting kWh remaining.");
    set_event(EVENT_KWH_PLAUSIBILITY_ERROR, nominal_full_pack_energy);
  }

  if (LFP_Chemistry) {  //LFP limits used for voltage safeties
    if (cell_max_v >= MAX_CELL_VOLTAGE_LFP) {
      set_event(EVENT_CELL_OVER_VOLTAGE, (cell_max_v - MAX_CELL_VOLTAGE_LFP));
    }
    if (cell_min_v <= MIN_CELL_VOLTAGE_LFP) {
      set_event(EVENT_CELL_UNDER_VOLTAGE, (MIN_CELL_VOLTAGE_LFP - cell_min_v));
    }
    if (cell_deviation_mV > MAX_CELL_DEVIATION_LFP) {
      set_event(EVENT_CELL_DEVIATION_HIGH, cell_deviation_mV);
    } else {
      clear_event(EVENT_CELL_DEVIATION_HIGH);
    }
  } else {  //NCA/NCM limits used
    if (cell_max_v >= MAX_CELL_VOLTAGE_NCA_NCM) {
      set_event(EVENT_CELL_OVER_VOLTAGE, (cell_max_v - MAX_CELL_VOLTAGE_NCA_NCM));
    }
    if (cell_min_v <= MIN_CELL_VOLTAGE_NCA_NCM) {
      set_event(EVENT_CELL_UNDER_VOLTAGE, (MIN_CELL_VOLTAGE_NCA_NCM - cell_min_v));
    }
    if (cell_deviation_mV > MAX_CELL_DEVIATION_NCA_NCM) {
      set_event(EVENT_CELL_DEVIATION_HIGH, cell_deviation_mV);
    } else {
      clear_event(EVENT_CELL_DEVIATION_HIGH);
    }
  }

  if (bms_status == FAULT) {  //Incase we enter a critical fault state, zero out the allowed limits
    max_target_charge_power = 0;
    max_target_discharge_power = 0;
  }

  /* Safeties verified. Perform USB serial printout if configured to do so */

#ifdef DEBUG_VIA_USB

  printFaultCodesIfActive();

  Serial.print("STATUS: Contactor: ");
  Serial.print(contactorText[contactor]);  //Display what state the contactor is in
  Serial.print(", HVIL: ");
  Serial.print(hvilStatusState[hvil_status]);
  Serial.print(", NegativeState: ");
  Serial.print(contactorState[packContNegativeState]);
  Serial.print(", PositiveState: ");
  Serial.print(contactorState[packContPositiveState]);
  Serial.print(", setState: ");
  Serial.print(contactorState[packContactorSetState]);
  Serial.print(", close allowed: ");
  Serial.print(packCtrsClosingAllowed);
  Serial.print(", Pyrotest: ");
  Serial.println(pyroTestInProgress);

  Serial.print("Battery values: ");
  Serial.print("Real SOC: ");
  Serial.print(soc_vi / 10.0, 1);
  print_int_with_units(", Battery voltage: ", volts, "V");
  print_int_with_units(", Battery HV current: ", (amps * 0.1), "A");
  Serial.print(", Fully charged?: ");
  if (full_charge_complete)
    Serial.print("YES, ");
  else
    Serial.print("NO, ");
  if (LFP_Chemistry) {
    Serial.print("LFP chemistry detected!");
  }
  Serial.println("");
  Serial.print("Cellstats, Max: ");
  Serial.print(cell_max_v);
  Serial.print("mV (cell ");
  Serial.print(max_vno);
  Serial.print("), Min: ");
  Serial.print(cell_min_v);
  Serial.print("mV (cell ");
  Serial.print(min_vno);
  Serial.print("), Imbalance: ");
  Serial.print(cell_deviation_mV);
  Serial.println("mV.");

  print_int_with_units("High Voltage Output Pins: ", high_voltage, "V");
  Serial.print(", ");
  print_int_with_units("Low Voltage: ", low_voltage, "V");
  Serial.println("");
  print_int_with_units("DC/DC 12V current: ", output_current, "A");
  Serial.println("");

  Serial.println("Values passed to the inverter: ");
  print_SOC(" SOC: ", SOC);
  print_int_with_units(" Max discharge power: ", max_target_discharge_power, "W");
  Serial.print(", ");
  print_int_with_units(" Max charge power: ", max_target_charge_power, "W");
  Serial.println("");
  print_int_with_units(" Max temperature: ", ((int16_t)temperature_max * 0.1), "°C");
  Serial.print(", ");
  print_int_with_units(" Min temperature: ", ((int16_t)temperature_min * 0.1), "°C");
  Serial.println("");
#endif
}

void receive_can_tesla_model_3_battery(CAN_frame_t rx_frame) {
  static int mux = 0;
  static int temp = 0;

  switch (rx_frame.MsgID) {
    case 0x352:
      //SOC
      nominal_full_pack_energy = (((rx_frame.data.u8[1] & 0x0F) << 8) | (rx_frame.data.u8[0]));  //Example 752 (75.2kWh)
      nominal_energy_remaining = (((rx_frame.data.u8[2] & 0x3F) << 5) | ((rx_frame.data.u8[1] & 0xF8) >> 3)) *
                                 0.1;  //Example 1247 * 0.1 = 124.7kWh
      expected_energy_remaining = (((rx_frame.data.u8[4] & 0x01) << 10) | (rx_frame.data.u8[3] << 2) |
                                   ((rx_frame.data.u8[2] & 0xC0) >> 6));  //Example 622 (62.2kWh)
      ideal_energy_remaining = (((rx_frame.data.u8[5] & 0x0F) << 7) | ((rx_frame.data.u8[4] & 0xFE) >> 1)) *
                               0.1;  //Example 311 * 0.1 = 31.1kWh
      energy_to_charge_complete = (((rx_frame.data.u8[6] & 0x7F) << 4) | ((rx_frame.data.u8[5] & 0xF0) >> 4)) *
                                  0.1;  //Example 147 * 0.1 = 14.7kWh
      energy_buffer =
          (((rx_frame.data.u8[7] & 0x7F) << 1) | ((rx_frame.data.u8[6] & 0x80) >> 7)) * 0.1;  //Example 1 * 0.1 = 0
      full_charge_complete = ((rx_frame.data.u8[7] & 0x80) >> 7);
      break;
    case 0x20A:
      //Contactor state
      packContNegativeState = (rx_frame.data.u8[0] & 0x07);
      packContPositiveState = (rx_frame.data.u8[0] & 0x38) >> 3;
      contactor = (rx_frame.data.u8[1] & 0x0F);
      packContactorSetState = (rx_frame.data.u8[1] & 0x0F);
      packCtrsClosingAllowed = (rx_frame.data.u8[4] & 0x08) >> 3;
      pyroTestInProgress = (rx_frame.data.u8[4] & 0x20) >> 5;
      hvil_status = (rx_frame.data.u8[5] & 0x0F);
      break;
    case 0x252:
      //Limits
      regenerative_limit = ((rx_frame.data.u8[1] << 8) | rx_frame.data.u8[0]) * 0.01;  //Example 4715 * 0.01 = 47.15kW
      discharge_limit = ((rx_frame.data.u8[3] << 8) | rx_frame.data.u8[2]) * 0.013;  //Example 2009 * 0.013 = 26.117???
      max_heat_park = (((rx_frame.data.u8[5] & 0x03) << 8) | rx_frame.data.u8[4]) * 0.01;  //Example 500 * 0.01 = 5kW
      hvac_max_power =
          (((rx_frame.data.u8[7] << 6) | ((rx_frame.data.u8[6] & 0xFC) >> 2))) * 0.02;  //Example 1000 * 0.02 = 20kW?
      break;
    case 0x132:
      //battery amps/volts
      volts = ((rx_frame.data.u8[1] << 8) | rx_frame.data.u8[0]) * 0.01;      //Example 37030mv * 0.01 = 370V
      amps = ((rx_frame.data.u8[3] << 8) | rx_frame.data.u8[2]);              //Example 65492 (-4.3A) OR 225 (22.5A)
      raw_amps = ((rx_frame.data.u8[5] << 8) | rx_frame.data.u8[4]) * -0.05;  //Example 10425 * -0.05 = ?
      battery_charge_time_remaining =
          (((rx_frame.data.u8[7] & 0x0F) << 8) | rx_frame.data.u8[6]) * 0.1;  //Example 228 * 0.1 = 22.8min
      if (battery_charge_time_remaining == 4095) {
        battery_charge_time_remaining = 0;
      }

      break;
    case 0x3D2:
      // total charge/discharge kwh
      total_discharge = ((rx_frame.data.u8[3] << 24) | (rx_frame.data.u8[2] << 16) | (rx_frame.data.u8[1] << 8) |
                         rx_frame.data.u8[0]) *
                        0.001;
      total_charge = ((rx_frame.data.u8[7] << 24) | (rx_frame.data.u8[6] << 16) | (rx_frame.data.u8[5] << 8) |
                      rx_frame.data.u8[4]) *
                     0.001;
      break;
    case 0x332:
      //min/max hist values
      mux = (rx_frame.data.u8[0] & 0x03);

      if (mux == 1)  //Cell voltages
      {
        temp = ((rx_frame.data.u8[1] << 6) | (rx_frame.data.u8[0] >> 2));
        temp = (temp & 0xFFF);
        cell_max_v = temp * 2;
        temp = ((rx_frame.data.u8[3] << 8) | rx_frame.data.u8[2]);
        temp = (temp & 0xFFF);
        cell_min_v = temp * 2;
        max_vno = 1 + (rx_frame.data.u8[4] & 0x7F);  //This cell has highest voltage
        min_vno = 1 + (rx_frame.data.u8[5] & 0x7F);  //This cell has lowest voltage
      }
      if (mux == 0)  //Temperature sensors
      {
        max_temp = (rx_frame.data.u8[2] * 5) - 400;  //Temperature values have 40.0*C offset, 0.5*C /bit
        min_temp = (rx_frame.data.u8[3] * 5) - 400;  //Multiply by 5 and remove offset to get C+1 (0x61*5=485-400=8.5*C)
      }
      break;
    case 0x401:  // Cell stats
      mux = (rx_frame.data.u8[0]);

      static uint16_t volts;
      static uint8_t mux_zero_counter = 0u;
      static uint8_t mux_max = 0u;

      if (rx_frame.data.u8[1] == 0x2A)  // status byte must be 0x2A to read cellvoltages
      {
        // Example, frame3=0x89,frame2=0x1D = 35101 / 10 = 3510mV
        volts = ((rx_frame.data.u8[3] << 8) | rx_frame.data.u8[2]) / 10;
        cellvoltages[mux * 3] = volts;
        volts = ((rx_frame.data.u8[5] << 8) | rx_frame.data.u8[4]) / 10;
        cellvoltages[1 + mux * 3] = volts;
        volts = ((rx_frame.data.u8[7] << 8) | rx_frame.data.u8[6]) / 10;
        cellvoltages[2 + mux * 3] = volts;

        // Track the max value of mux. If we've seen two 0 values for mux, we've probably gathered all
        // cell voltages. Then, 2 + mux_max * 3 + 1 is the number of cell voltages.
        mux_max = (mux > mux_max) ? mux : mux_max;
        if (mux_zero_counter < 2 && mux == 0u) {
          mux_zero_counter++;
          if (mux_zero_counter == 2u) {
            // The max index will be 2 + mux_max * 3 (see above), so "+ 1" for the number of cells
            nof_cellvoltages = 2 + 3 * mux_max + 1;
            // Increase the counter arbitrarily another time to make the initial if-statement evaluate to false
            mux_zero_counter++;
          }
        }
      }
      break;
    case 0x2d2:
      //Min / max limits
      min_voltage = ((rx_frame.data.u8[1] << 8) | rx_frame.data.u8[0]) * 0.01 * 2;  //Example 24148mv * 0.01 = 241.48 V
      max_voltage = ((rx_frame.data.u8[3] << 8) | rx_frame.data.u8[2]) * 0.01 * 2;  //Example 40282mv * 0.01 = 402.82 V
      max_charge_current =
          (((rx_frame.data.u8[5] & 0x3F) << 8) | rx_frame.data.u8[4]) * 0.1;  //Example 1301? * 0.1 = 130.1?
      max_discharge_current =
          (((rx_frame.data.u8[7] & 0x3F) << 8) | rx_frame.data.u8[6]) * 0.128;  //Example 430? * 0.128 = 55.4?
      break;
    case 0x2b4:
      low_voltage = (((rx_frame.data.u8[1] & 0x03) << 8) | rx_frame.data.u8[0]) * 0.0390625;
      high_voltage = ((((rx_frame.data.u8[2] & 0x3F) << 6) | ((rx_frame.data.u8[1] & 0xFC) >> 2))) * 0.146484;
      output_current = (((rx_frame.data.u8[4] & 0x0F) << 8) | rx_frame.data.u8[3]) / 100;
      break;
    case 0x292:
      stillAliveCAN = 12;  //We are getting CAN messages from the BMS, set the CAN detect counter
      bat_beginning_of_life = (((rx_frame.data.u8[6] & 0x03) << 8) | rx_frame.data.u8[5]);
      soc_min = (((rx_frame.data.u8[1] & 0x03) << 8) | rx_frame.data.u8[0]);
      soc_vi = (((rx_frame.data.u8[2] & 0x0F) << 6) | ((rx_frame.data.u8[1] & 0xFC) >> 2));
      soc_max = (((rx_frame.data.u8[3] & 0x3F) << 4) | ((rx_frame.data.u8[2] & 0xF0) >> 4));
      soc_ave = ((rx_frame.data.u8[4] << 2) | ((rx_frame.data.u8[3] & 0xC0) >> 6));
      break;
    case 0x3aa:  //HVP_alertMatrix1
      WatchdogReset = (rx_frame.data.u8[0] & 0x01);
      PowerLossReset = ((rx_frame.data.u8[0] & 0x02) >> 1);
      SwAssertion = ((rx_frame.data.u8[0] & 0x04) >> 2);
      CrashEvent = ((rx_frame.data.u8[0] & 0x08) >> 3);
      OverDchgCurrentFault = ((rx_frame.data.u8[0] & 0x10) >> 4);
      OverChargeCurrentFault = ((rx_frame.data.u8[0] & 0x20) >> 5);
      OverCurrentFault = ((rx_frame.data.u8[0] & 0x40) >> 6);
      OverTemperatureFault = ((rx_frame.data.u8[1] & 0x80) >> 7);
      OverVoltageFault = (rx_frame.data.u8[1] & 0x01);
      UnderVoltageFault = ((rx_frame.data.u8[1] & 0x02) >> 1);
      PrimaryBmbMiaFault = ((rx_frame.data.u8[1] & 0x04) >> 2);
      SecondaryBmbMiaFault = ((rx_frame.data.u8[1] & 0x08) >> 3);
      BmbMismatchFault = ((rx_frame.data.u8[1] & 0x10) >> 4);
      BmsHviMiaFault = ((rx_frame.data.u8[1] & 0x20) >> 5);
      CpMiaFault = ((rx_frame.data.u8[1] & 0x40) >> 6);
      PcsMiaFault = ((rx_frame.data.u8[1] & 0x80) >> 7);
      BmsFault = (rx_frame.data.u8[2] & 0x01);
      PcsFault = ((rx_frame.data.u8[2] & 0x02) >> 1);
      CpFault = ((rx_frame.data.u8[2] & 0x04) >> 2);
      ShuntHwMiaFault = ((rx_frame.data.u8[2] & 0x08) >> 3);
      PyroMiaFault = ((rx_frame.data.u8[2] & 0x10) >> 4);
      hvsMiaFault = ((rx_frame.data.u8[2] & 0x20) >> 5);
      hviMiaFault = ((rx_frame.data.u8[2] & 0x40) >> 6);
      Supply12vFault = ((rx_frame.data.u8[2] & 0x80) >> 7);
      VerSupplyFault = (rx_frame.data.u8[3] & 0x01);
      HvilFault = ((rx_frame.data.u8[3] & 0x02) >> 1);
      BmsHvsMiaFault = ((rx_frame.data.u8[3] & 0x04) >> 2);
      PackVoltMismatchFault = ((rx_frame.data.u8[3] & 0x08) >> 3);
      EnsMiaFault = ((rx_frame.data.u8[3] & 0x10) >> 4);
      PackPosCtrArcFault = ((rx_frame.data.u8[3] & 0x20) >> 5);
      packNegCtrArcFault = ((rx_frame.data.u8[3] & 0x40) >> 6);
      ShuntHwAndBmsMiaFault = ((rx_frame.data.u8[3] & 0x80) >> 7);
      fcContHwFault = (rx_frame.data.u8[4] & 0x01);
      robinOverVoltageFault = ((rx_frame.data.u8[4] & 0x02) >> 1);
      packContHwFault = ((rx_frame.data.u8[4] & 0x04) >> 2);
      pyroFuseBlown = ((rx_frame.data.u8[4] & 0x08) >> 3);
      pyroFuseFailedToBlow = ((rx_frame.data.u8[4] & 0x10) >> 4);
      CpilFault = ((rx_frame.data.u8[4] & 0x20) >> 5);
      PackContactorFellOpen = ((rx_frame.data.u8[4] & 0x40) >> 6);
      FcContactorFellOpen = ((rx_frame.data.u8[4] & 0x80) >> 7);
      packCtrCloseBlocked = (rx_frame.data.u8[5] & 0x01);
      fcCtrCloseBlocked = ((rx_frame.data.u8[5] & 0x02) >> 1);
      packContactorForceOpen = ((rx_frame.data.u8[5] & 0x04) >> 2);
      fcContactorForceOpen = ((rx_frame.data.u8[5] & 0x08) >> 3);
      dcLinkOverVoltage = ((rx_frame.data.u8[5] & 0x10) >> 4);
      shuntOverTemperature = ((rx_frame.data.u8[5] & 0x20) >> 5);
      passivePyroDeploy = ((rx_frame.data.u8[5] & 0x40) >> 6);
      logUploadRequest = ((rx_frame.data.u8[5] & 0x80) >> 7);
      packCtrCloseFailed = (rx_frame.data.u8[6] & 0x01);
      fcCtrCloseFailed = ((rx_frame.data.u8[6] & 0x02) >> 1);
      shuntThermistorMia = ((rx_frame.data.u8[6] & 0x04) >> 2);
      break;
    default:
      break;
  }
}
void send_can_tesla_model_3_battery() {
  /*From bielec: My fist 221 message, to close the contactors is 0x41, 0x11, 0x01, 0x00, 0x00, 0x00, 0x20, 0x96 and then, 
to cause "hv_up_for_drive" I send an additional 221 message 0x61, 0x15, 0x01, 0x00, 0x00, 0x00, 0x20, 0xBA  so 
two 221 messages are being continuously transmitted.   When I want to shut down, I stop the second message and only send 
the first, for a few cycles, then stop all  messages which causes the contactor to open. */

  unsigned long currentMillis = millis();
  //Send 30ms message
  if (currentMillis - previousMillis30 >= interval30) {
    previousMillis30 = currentMillis;

    if (inverterAllowsContactorClosing == 1) {
      if (bms_status == ACTIVE) {
        send221still = 50;
        batteryAllowsContactorClosing = true;
        ESP32Can.CANWriteFrame(&TESLA_221_1);
        ESP32Can.CANWriteFrame(&TESLA_221_2);
      } else {  //bms_status == FAULT or inverter requested opening contactors
        if (send221still > 0) {
          batteryAllowsContactorClosing = false;
          ESP32Can.CANWriteFrame(&TESLA_221_1);
          send221still--;
        }
      }
    }
  }
}
uint16_t convert2unsignedInt16(int16_t signed_value) {
  if (signed_value < 0) {
    return (65535 + signed_value);
  } else {
    return (uint16_t)signed_value;
  }
}

void print_int_with_units(char* header, int value, char* units) {
  Serial.print(header);
  Serial.print(value);
  Serial.print(units);
}

void print_SOC(char* header, int SOC) {
  Serial.print(header);
  Serial.print(SOC / 100);
  Serial.print(".");
  int hundredth = SOC % 100;
  if (hundredth < 10)
    Serial.print(0);
  Serial.print(hundredth);
  Serial.println("%");
}

void printFaultCodesIfActive() {
  if (packCtrsClosingAllowed == 0) {
    Serial.println(
        "ERROR: Check high voltage connectors and interlock circuit! Closing contactor not allowed! Values: ");
  }
  if (pyroTestInProgress == 1) {
    Serial.println("ERROR: Please wait for Pyro Connection check to finish, HV cables successfully seated!");
  }
  if (inverterAllowsContactorClosing == 0) {
    Serial.println(
        "ERROR: Solar inverter does not allow for contactor closing. Check inverterAllowsContactorClosing parameter");
  }
  // Check each symbol and print debug information if its value is 1
  printDebugIfActive(WatchdogReset, "ERROR: The processor has experienced a reset due to watchdog reset");
  printDebugIfActive(PowerLossReset, "ERROR: The processor has experienced a reset due to power loss");
  printDebugIfActive(SwAssertion, "ERROR: An internal software assertion has failed");
  printDebugIfActive(CrashEvent, "ERROR: crash signal is detected by HVP");
  printDebugIfActive(OverDchgCurrentFault,
                     "ERROR: Pack discharge current is above the safe max discharge current limit!");
  printDebugIfActive(OverChargeCurrentFault, "ERROR: Pack charge current is above the safe max charge current limit!");
  printDebugIfActive(OverCurrentFault, "ERROR: Pack current (discharge or charge) is above max current limit!");
  printDebugIfActive(OverTemperatureFault, "ERROR: A pack module temperature is above the max temperature limit!");
  printDebugIfActive(OverVoltageFault, "ERROR: A brick voltage is above maximum voltage limit");
  printDebugIfActive(UnderVoltageFault, "ERROR: A brick voltage is below minimum voltage limit");
  printDebugIfActive(PrimaryBmbMiaFault, "ERROR: voltage and temperature readings from primary BMB chain are mia");
  printDebugIfActive(SecondaryBmbMiaFault, "ERROR: voltage and temperature readings from secondary BMB chain are mia");
  printDebugIfActive(BmbMismatchFault, "ERROR: primary and secondary BMB chain readings don't match with each other");
  printDebugIfActive(BmsHviMiaFault, "ERROR: BMS node is mia on HVS or HVI CAN");
  //printDebugIfActive(CpMiaFault, "ERROR: CP node is mia on HVS CAN"); //Uncommented due to not affecting usage
  printDebugIfActive(PcsMiaFault, "ERROR: PCS node is mia on HVS CAN");
  //printDebugIfActive(BmsFault, "ERROR: BmsFault is active"); //Uncommented due to not affecting usage
  printDebugIfActive(PcsFault, "ERROR: PcsFault is active");
  //printDebugIfActive(CpFault, "ERROR: CpFault is active"); //Uncommented due to not affecting usage
  printDebugIfActive(ShuntHwMiaFault, "ERROR: shunt current reading is not available");
  printDebugIfActive(PyroMiaFault, "ERROR: pyro squib is not connected");
  printDebugIfActive(hvsMiaFault, "ERROR: pack contactor hw fault");
  printDebugIfActive(hviMiaFault, "ERROR: FC contactor hw fault");
  printDebugIfActive(Supply12vFault, "ERROR: Low voltage (12V) battery is below minimum voltage threshold");
  printDebugIfActive(VerSupplyFault, "ERROR: Energy reserve voltage supply is below minimum voltage threshold");
  printDebugIfActive(HvilFault, "ERROR: High Voltage Inter Lock fault is detected");
  printDebugIfActive(BmsHvsMiaFault, "ERROR: BMS node is mia on HVS or HVI CAN");
  printDebugIfActive(PackVoltMismatchFault,
                     "ERROR: Pack voltage doesn't match approximately with sum of brick voltages");
  //printDebugIfActive(EnsMiaFault, "ERROR: ENS line is not connected to HVC"); //Uncommented due to not affecting usage
  printDebugIfActive(PackPosCtrArcFault, "ERROR: HVP detectes series arc at pack contactor");
  printDebugIfActive(packNegCtrArcFault, "ERROR: HVP detectes series arc at FC contactor");
  printDebugIfActive(ShuntHwAndBmsMiaFault, "ERROR: ShuntHwAndBmsMiaFault is active");
  printDebugIfActive(fcContHwFault, "ERROR: fcContHwFault is active");
  printDebugIfActive(robinOverVoltageFault, "ERROR: robinOverVoltageFault is active");
  printDebugIfActive(packContHwFault, "ERROR: packContHwFault is active");
  printDebugIfActive(pyroFuseBlown, "ERROR: pyroFuseBlown is active");
  printDebugIfActive(pyroFuseFailedToBlow, "ERROR: pyroFuseFailedToBlow is active");
  //printDebugIfActive(CpilFault, "ERROR: CpilFault is active"); //Uncommented due to not affecting usage
  printDebugIfActive(PackContactorFellOpen, "ERROR: PackContactorFellOpen is active");
  printDebugIfActive(FcContactorFellOpen, "ERROR: FcContactorFellOpen is active");
  printDebugIfActive(packCtrCloseBlocked, "ERROR: packCtrCloseBlocked is active");
  printDebugIfActive(fcCtrCloseBlocked, "ERROR: fcCtrCloseBlocked is active");
  printDebugIfActive(packContactorForceOpen, "ERROR: packContactorForceOpen is active");
  printDebugIfActive(fcContactorForceOpen, "ERROR: fcContactorForceOpen is active");
  printDebugIfActive(dcLinkOverVoltage, "ERROR: dcLinkOverVoltage is active");
  printDebugIfActive(shuntOverTemperature, "ERROR: shuntOverTemperature is active");
  printDebugIfActive(passivePyroDeploy, "ERROR: passivePyroDeploy is active");
  printDebugIfActive(logUploadRequest, "ERROR: logUploadRequest is active");
  printDebugIfActive(packCtrCloseFailed, "ERROR: packCtrCloseFailed is active");
  printDebugIfActive(fcCtrCloseFailed, "ERROR: fcCtrCloseFailed is active");
  printDebugIfActive(shuntThermistorMia, "ERROR: shuntThermistorMia is active");
}

void printDebugIfActive(uint8_t symbol, const char* message) {
  if (symbol == 1) {
    Serial.println(message);
  }
}
