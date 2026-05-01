# message from bq24725 charger ic

std_msgs/Header header
# Whether the AC adapter is connected
bool    ac_present
# Whether it is charging
bool    charging
# Charging Current Setpoint (A)
float32 charge_current_a
# Charging Voltage Setpoint (V)
float32 charge_voltage_v
# Enter Current Limit Value (A)
float32 input_current_a
# Charging Phase (Pre-Charge/Constant Current/Constant Voltage/Full)
uint8   charge_status
# DCDC temperature (°C)
float32 temperature