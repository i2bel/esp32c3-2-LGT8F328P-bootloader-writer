esp32c3-2-LGT8F328P-bootloader-writer

Connect lgt8f328p chip to esp32c3. Use ArduinoIDE for upload bootloader. Set chip - LGT8f328p. Set port - usb port on which esp32c3 connected.
Connect swd interface     
    SWC_PIN=4 esp32c3 to SWC LGT8f328p
    SWD_PIN=5 esp32c3 to SWD LGT8f328p
    RSTN_PIN=6 esp32c3 to RSTN LGT8f328p
    Dont forget +3.3 and GND.

    Set parameters as on picture pic1.jpg

Wait 10 sec and enjoy.

Project is a port: https://github.com/brother-yan/LGTISP
