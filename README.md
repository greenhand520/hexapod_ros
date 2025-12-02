## Introduction

### Hardware

The hardware schematics and PCB designs were created using [LCEDA Pro](https://pro.lceda.cn/editor). Please click [here](https://pro.lceda.cn/greenhand520/hexapod) to get the latest designs.

4S 21700 Li-ion battery is used as the power source. The servo motor power supply PCB uses an SC8802 DC-DC converter to charge the battery and power 18 servos. For details on this PCB, please refer to [my another open-source project](https://github.com/greenhand520/SC8802_bidirectional_power). A TPS54821 outputs 5V to power the Lubancat Zero W development board and the YDLIDAR X2 lidar. 

The lubancat Zero W development board is the main controller, communicating directly with 18 serial servo motors via serial port. The IMU uses an MPU6500 and communicates with the lubancat via SPI. Considering cost and that ADC detection is not the focus of this project, a PCF8591 chip salvaged from an ADC module was used to detect voltage and temperature.

### Mechanism

The fuselage structure was completed using 3D printing, modeling parameters were adapted to PETG material and the Bambu Lab A1 printer. Other materials and printers have not been tested, and structural strength is not guaranteed. Please click here to get the latest specific printing files and BOM.

The mechanism folder contains the step files for the overall fuselage structure.
