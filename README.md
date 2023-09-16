This is (hopefuly eventually going to be) an emulator for the avr 8 bit architecture, based on teh spacific charachteristics of the ATtiny85.
The project is currently in its infancy.
for testing I reccomend using AVRA

# what needs doing:
 - tonnes of instructions need adding
 - add a user interface
 - create the correct startup environment
 - program the EEPROM logic
 - split program into multiple files
 - loads of testing + check each instruction is correctly implemented.

# stretch goals:
 - add a mechanism for tracking CPU cycles passed
 - allow for custom compilation or other forms of configuration to target different cores
 - simulate other hardware devices (EG watchdog timer)