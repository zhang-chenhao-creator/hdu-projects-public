#include "ff.h"
#include "diskio.h"
#include "w25qxx.h"

static volatile DSTATUS Stat = STA_NOINIT;

DSTATUS disk_initialize(BYTE pdrv)
{
  if (pdrv != 0U)
  {
    return STA_NOINIT;
  }

  if (W25QXX_Init() == HAL_OK)
  {
    Stat = 0;
  }
  else
  {
    Stat = STA_NOINIT;
  }

  return Stat;
}

DSTATUS disk_status(BYTE pdrv)
{
  if (pdrv != 0U)
  {
    return STA_NOINIT;
  }
  return Stat;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
  UINT i;

  if (pdrv != 0U || buff == 0 || count == 0U || (Stat & STA_NOINIT))
  {
    return RES_PARERR;
  }
  if ((sector + count) > W25Q128_SECTOR_COUNT)
  {
    return RES_PARERR;
  }

  for (i = 0; i < count; ++i)
  {
    uint32_t addr = ((uint32_t)sector + i) * W25Q128_SECTOR_SIZE;
    if (W25QXX_Read(addr, &buff[i * W25Q128_SECTOR_SIZE], W25Q128_SECTOR_SIZE) != HAL_OK)
    {
      return RES_ERROR;
    }
  }

  return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
  UINT i;

  if (pdrv != 0U || buff == 0 || count == 0U || (Stat & STA_NOINIT))
  {
    return RES_PARERR;
  }
  if ((sector + count) > W25Q128_SECTOR_COUNT)
  {
    return RES_PARERR;
  }

  for (i = 0; i < count; ++i)
  {
    if (W25QXX_WriteSector((uint32_t)sector + i, &buff[i * W25Q128_SECTOR_SIZE]) != HAL_OK)
    {
      return RES_ERROR;
    }
  }

  return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
  if (pdrv != 0U || buff == 0)
  {
    return RES_PARERR;
  }
  if (Stat & STA_NOINIT)
  {
    return RES_NOTRDY;
  }

  switch (cmd)
  {
    case CTRL_SYNC:
      return RES_OK;
    case GET_SECTOR_COUNT:
      *(LBA_t *)buff = W25Q128_SECTOR_COUNT;
      return RES_OK;
    case GET_SECTOR_SIZE:
      *(WORD *)buff = W25Q128_SECTOR_SIZE;
      return RES_OK;
    case GET_BLOCK_SIZE:
      *(DWORD *)buff = 1;
      return RES_OK;
    default:
      return RES_PARERR;
  }
}
