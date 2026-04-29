# message /power/charger_state is from bq24725
Header header
bool    ac_present                      # Whether the AC adapter is connected
bool    charging                        # Whether it is charging
float32 charge_current_a                # Charging Current Setpoint (A)
float32 charge_voltage_v                # Charging Voltage Setpoint (V)
float32 input_current_a                 # Enter Current Limit Value (A)
uint8   charge_status                   # Charging Phase (Pre-Charge/Constant Current/Constant Voltage/Full)
float32 temperature_c                   # DCDC temperature (°C)
float32 temperature_warning_threshold   # Temperature Alarm Threshold (°C)