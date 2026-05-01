# stm32 board info and state, message: /sensor/board_state

std_msgs/Header header
# servo DCDC Temperature (°C)
float32 servo_power_temperature
# servo DCDC output voltage
float32 servo_power_voltage_v
# stm32 MCU temperature
float32 mcu_temperature
# charge_state
ChargerState charger_state