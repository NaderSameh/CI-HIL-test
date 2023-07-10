#ifndef FLASH_SPI_H
#define FLASH_SPI_H

#define FLASH_SECTOR_SIZE 4096
#define FLASH_CYTAG_ASSIGN 0x1FA000

int FLASH_INIT();
int writeFifoToFlash();
int importFifoFromFlash();
int removeFifoFromFlash();

#endif