#include "w25qxx.h"
#include "spi.h"

#define W25Q_CMD_WRITE_ENABLE 0x06U
#define W25Q_CMD_READ_STATUS1 0x05U
#define W25Q_CMD_PAGE_PROGRAM 0x02U
#define W25Q_CMD_READ_DATA    0x03U
#define W25Q_CMD_SECTOR_ERASE 0x20U
#define W25Q_CMD_JEDEC_ID     0x9FU
#define W25Q_STATUS_BUSY      0x01U
#define W25Q_SPI_TIMEOUT      1000U

static void W25Q_Select(void)
{
  HAL_GPIO_WritePin(W25Q_CS_GPIO_Port, W25Q_CS_Pin, GPIO_PIN_RESET);
}

static void W25Q_Deselect(void)
{
  HAL_GPIO_WritePin(W25Q_CS_GPIO_Port, W25Q_CS_Pin, GPIO_PIN_SET);
}

static HAL_StatusTypeDef W25Q_WriteEnable(void)
{
  uint8_t cmd = W25Q_CMD_WRITE_ENABLE;

  W25Q_Select();
  HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi1, &cmd, 1, W25Q_SPI_TIMEOUT);
  W25Q_Deselect();
  return st;
}

static HAL_StatusTypeDef W25Q_ReadStatus(uint8_t *status)
{
  uint8_t cmd = W25Q_CMD_READ_STATUS1;

  W25Q_Select();
  HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi1, &cmd, 1, W25Q_SPI_TIMEOUT);
  if (st == HAL_OK)
  {
    st = HAL_SPI_Receive(&hspi1, status, 1, W25Q_SPI_TIMEOUT);
  }
  W25Q_Deselect();
  return st;
}

static HAL_StatusTypeDef W25Q_WaitReady(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  uint8_t status = 0;

  do
  {
    if (W25Q_ReadStatus(&status) != HAL_OK)
    {
      return HAL_ERROR;
    }
    if ((status & W25Q_STATUS_BUSY) == 0U)
    {
      return HAL_OK;
    }
    HAL_Delay(1);
  } while ((HAL_GetTick() - start) < timeout_ms);

  return HAL_TIMEOUT;
}

uint32_t W25QXX_ReadJEDECID(void)
{
  uint8_t cmd = W25Q_CMD_JEDEC_ID;
  uint8_t id[3] = {0};

  W25Q_Select();
  if (HAL_SPI_Transmit(&hspi1, &cmd, 1, W25Q_SPI_TIMEOUT) != HAL_OK)
  {
    W25Q_Deselect();
    return 0;
  }
  if (HAL_SPI_Receive(&hspi1, id, sizeof(id), W25Q_SPI_TIMEOUT) != HAL_OK)
  {
    W25Q_Deselect();
    return 0;
  }
  W25Q_Deselect();

  return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
}

HAL_StatusTypeDef W25QXX_Init(void)
{
  uint32_t id = W25QXX_ReadJEDECID();

  if (id == 0U || id == 0xFFFFFFU)
  {
    return HAL_ERROR;
  }

  return W25Q_WaitReady(1000);
}

HAL_StatusTypeDef W25QXX_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
  uint8_t cmd[4];

  if ((addr + len) > W25Q128_TOTAL_SIZE)
  {
    return HAL_ERROR;
  }

  cmd[0] = W25Q_CMD_READ_DATA;
  cmd[1] = (uint8_t)(addr >> 16);
  cmd[2] = (uint8_t)(addr >> 8);
  cmd[3] = (uint8_t)addr;

  W25Q_Select();
  HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi1, cmd, sizeof(cmd), W25Q_SPI_TIMEOUT);
  if (st == HAL_OK)
  {
    st = HAL_SPI_Receive(&hspi1, buf, (uint16_t)len, W25Q_SPI_TIMEOUT);
  }
  W25Q_Deselect();

  return st;
}

HAL_StatusTypeDef W25QXX_EraseSector(uint32_t sector)
{
  uint32_t addr = sector * W25Q128_SECTOR_SIZE;
  uint8_t cmd[4];

  if (sector >= W25Q128_SECTOR_COUNT)
  {
    return HAL_ERROR;
  }
  if (W25Q_WriteEnable() != HAL_OK)
  {
    return HAL_ERROR;
  }

  cmd[0] = W25Q_CMD_SECTOR_ERASE;
  cmd[1] = (uint8_t)(addr >> 16);
  cmd[2] = (uint8_t)(addr >> 8);
  cmd[3] = (uint8_t)addr;

  W25Q_Select();
  HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi1, cmd, sizeof(cmd), W25Q_SPI_TIMEOUT);
  W25Q_Deselect();
  if (st != HAL_OK)
  {
    return st;
  }

  return W25Q_WaitReady(5000);
}

HAL_StatusTypeDef W25QXX_PageProgram(uint32_t addr, const uint8_t *buf, uint32_t len)
{
  uint8_t cmd[4];

  if (len == 0U || len > W25Q128_PAGE_SIZE || ((addr & (W25Q128_PAGE_SIZE - 1U)) + len) > W25Q128_PAGE_SIZE)
  {
    return HAL_ERROR;
  }
  if ((addr + len) > W25Q128_TOTAL_SIZE)
  {
    return HAL_ERROR;
  }
  if (W25Q_WriteEnable() != HAL_OK)
  {
    return HAL_ERROR;
  }

  cmd[0] = W25Q_CMD_PAGE_PROGRAM;
  cmd[1] = (uint8_t)(addr >> 16);
  cmd[2] = (uint8_t)(addr >> 8);
  cmd[3] = (uint8_t)addr;

  W25Q_Select();
  HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi1, cmd, sizeof(cmd), W25Q_SPI_TIMEOUT);
  if (st == HAL_OK)
  {
    st = HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, (uint16_t)len, W25Q_SPI_TIMEOUT);
  }
  W25Q_Deselect();
  if (st != HAL_OK)
  {
    return st;
  }

  return W25Q_WaitReady(1000);
}

HAL_StatusTypeDef W25QXX_WriteSector(uint32_t sector, const uint8_t *buf)
{
  uint32_t base;
  uint32_t offset;

  if (sector >= W25Q128_SECTOR_COUNT)
  {
    return HAL_ERROR;
  }
  if (W25QXX_EraseSector(sector) != HAL_OK)
  {
    return HAL_ERROR;
  }

  base = sector * W25Q128_SECTOR_SIZE;
  for (offset = 0; offset < W25Q128_SECTOR_SIZE; offset += W25Q128_PAGE_SIZE)
  {
    if (W25QXX_PageProgram(base + offset, &buf[offset], W25Q128_PAGE_SIZE) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }

  return HAL_OK;
}
