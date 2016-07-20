#include "eeprom.h"
#include <xc.h>

uint16_t __attribute__((space(eedata))) eedata[__EEDATA_LENGTH >> 1];

void EEPROM_erase(uint16_t address){
    NVMCON = 0x4058;
    
    /* Set up a pointer to the EEPROM location to be written */
    TBLPAG = __builtin_tblpage(eedata);
    uint16_t offset = __builtin_tbloffset(eedata) + (address << 1);
    __builtin_tblwtl(offset, offset);
    
    asm volatile ("disi #5");
    __builtin_write_NVM();
    while(NVMCONbits.WR == 1);  // wait for write sequence to complete
}

void EEPROM_write(uint16_t address, uint16_t value){
    EEPROM_erase(address);
    
    NVMCON = 0x4004;
    
    /* Set up a pointer to the EEPROM location to be written */
    TBLPAG = __builtin_tblpage(eedata);
    uint16_t offset = __builtin_tbloffset(eedata) + (address << 1);
    __builtin_tblwtl(offset, value);
    
    asm volatile ("disi #5");
    __builtin_write_NVM();
    while(NVMCONbits.WR == 1);  // wait for write sequence to complete
}

uint16_t EEPROM_read(uint16_t address){
    uint16_t value;
    
    /* Set up a pointer to the EEPROM location to be read */
    TBLPAG = __builtin_tblpage(eedata);
    uint16_t offset = __builtin_tbloffset(eedata) + (address << 1);
    
    value = __builtin_tblrdl(offset);
    
    return value;
}