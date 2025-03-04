/**
  ******************************************************************************
  * @file    sdram.c
  * @author  cyytx
  * @brief   SDRAM模块的源文件,实现SDRAM的初始化、读写等功能
  ******************************************************************************
  */

#include "sdram.h"

#if SDRAM_ENABLE

static SDRAM_HandleTypeDef hsdram1;
static FMC_SDRAM_TimingTypeDef SDRAM_Timing;

void SDRAM_Init(void)
{
    /** Perform the SDRAM1 memory initialization sequence */
    hsdram1.Instance = FMC_SDRAM_DEVICE;
    /* hsdram1.Init */
    hsdram1.Init.SDBank = FMC_SDRAM_BANK1;
    hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_10;
    hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_13;
    hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
    hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
    hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_1;
    hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
    hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_DISABLE;
    hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_DISABLE;
    hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;
    
    /* SDRAM timing */
    SDRAM_Timing.LoadToActiveDelay = 16;
    SDRAM_Timing.ExitSelfRefreshDelay = 16;
    SDRAM_Timing.SelfRefreshTime = 16;
    SDRAM_Timing.RowCycleDelay = 16;
    SDRAM_Timing.WriteRecoveryTime = 16;
    SDRAM_Timing.RPDelay = 16;
    SDRAM_Timing.RCDDelay = 16;

    if (HAL_SDRAM_Init(&hsdram1, &SDRAM_Timing) != HAL_OK)
    {
        Error_Handler();
    }
}

void SDRAM_WriteBuffer(uint32_t* buffer, uint32_t address, uint32_t size)
{
    // TODO: 实现SDRAM写入功能
}

void SDRAM_ReadBuffer(uint32_t* buffer, uint32_t address, uint32_t size)
{
    // TODO: 实现SDRAM读取功能
}

#endif /* SDRAM_ENABLE */ 