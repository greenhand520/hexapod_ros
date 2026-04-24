## Introduction

### Hardware

The hardware schematics and PCB designs were created using [LCEDA Pro](https://pro.lceda.cn/editor). Please click [here](https://pro.lceda.cn/greenhand520/hexapod) to get the latest designs.

4S 21700 Li-ion battery is used as the power source. 

A TPS54821 outputs 5V to power the Lubancat Zero W development board and the YDLIDAR X2 lidar. 

The lubancat Zero W development board is the main controller, communicating directly with 18 serial servo motors via serial port. The IMU uses an MPU6500 and communicates with the lubancat via SPI. 

A  YDLIDAR X2 lidar was used for navigation, related documentation can be found in the folder `docs/YDLIDAR X2`.

### Mechanism

The fuselage structure was completed using 3D printing, modeling parameters were adapted to PETG material and the Bambu Lab A1 printer. Other materials and printers have not been tested, and structural strength is not guaranteed. Please click here to get the latest specific printing files and BOM.

The `mechanism` folder contains the step files for the overall fuselage structure.
