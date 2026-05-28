# message from bq24725 charger ic

std_msgs/Header header
# whether the AC adapter is connected
bool ac_present

# whether it is charging
bool charging

# charging current setpoint (A)
float32 charge_current_a

# charging voltage setpoint (V)
float32 charge_voltage_v

# AC adapter enter current limit setpoint (A)
float32 input_current_a

# real-time charging current on the battery side (A)
float32 realtime_current_a

# real-time AC adapter input voltage (V)
float32 realtime_input_voltage_v

# charging phase (0:Unknow/1:Not-Charged/2:Pre-Charge/3:CC/4:CV/5:Full/6:HUSB238A-Fault/7:Charger-Unsupported)
uint8 charge_status

# DC-DC temperature (°C)
float32 temperature