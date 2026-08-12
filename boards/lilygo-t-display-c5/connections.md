## USING CUSTOM BOARD, with SPI
| Device  | SCK   | MISO  | MOSI  | CS    | GDO0/CE |
| ---     | :---: | :---: | :---: | :---: | :---:   |
| SDCard  | 7     | 6     | 9     | 27    | -       |
| CC1101  | 7     | 6     | 9     | 4     | 5       |
| NRF24   | 7     | 6     | 9     | 4     | 5       |

| Device  | RX    | TX    | GPIO  |
| ---     | :---: | :---: | :---: |
| GPS     | 24    | 25    | ---   |
| IR RX   |  ---  | ---   | 1     |
| IR TX   |  ---  | ---   | 1     |

FM Radio, PN532 on I2C, other I2C devices
I2C SDA: 2
I2C SCL: 3

GPS Connections
SERIAL_TX: 25
SERIAL_RX: 24
