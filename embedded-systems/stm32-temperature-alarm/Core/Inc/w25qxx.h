#ifndef __W25QXX_H__
#define __W25QXX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define W25Q128_SECTOR_SIZE 4096U
#define W25Q128_SECTOR_COUNT 4096U
#define W25Q128_PAGE_SIZE 256U
#define W25Q128_TOTAL_SIZE (W25Q128_SECTOR_SIZE * W25Q128_SECTOR_COUNT)

HAL_StatusTypeDef W25QXX_Init(void);
uint32_t W25QXX_ReadJEDECID(void);
HAL_StatusTypeDef W25QXX_Read(uint32_t addr, uint8_t *buf, uint32_t len);
HAL_StatusTypeDef W25QXX_EraseSector(uint32_t sector);
HAL_StatusTypeDef W25QXX_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len);
HAL_StatusTypeDef W25QXX_WriteSector(uint32_t sector, const uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif
