#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>

void EEPROM_erase(uint16_t address);
void EEPROM_write(uint16_t address, uint16_t value);
uint16_t EEPROM_read(uint16_t address);

#endif
