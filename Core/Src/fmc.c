/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : FMC.c
 * Description        : This file provides code for the configuration
 *                      of the FMC peripheral for IS42S16400J-7TLI SDRAM
 *                      Operating at 140MHz
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "fmc.h"
#include <stdlib.h>
/* USER CODE BEGIN 0 */

/*============================================================================*/
/*                           DEFINES AND MACROS                               */
/*============================================================================*/

/* SDRAM Configuration */
#define SDRAM_TIMEOUT                   0x1000
#define SDRAM_REFRESH_ROWS               4096
#define SDRAM_REFRESH_PERIOD_MS            64  /* 64ms typical */

/* SDRAM Mode Register Definitions */
#define SDRAM_MODEREG_BURST_LENGTH_1     ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2     ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4     ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8     ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQ     ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_ILVD    ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2      ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3      ((uint16_t)0x0030)
#define SDRAM_MODEREG_OP_MODE_STD        ((uint16_t)0x0000)
#define SDRAM_MODEREG_WBURST_PROG        ((uint16_t)0x0000)
#define SDRAM_MODEREG_WBURST_SINGLE      ((uint16_t)0x0200)



/* SDRAM Timing Parameters (in clock cycles) */
/* Based on IS42S16400J-7TLI datasheet (143MHz max) */
#define SDRAM_TMRD                        2       /* Mode Register Set Command Cycle */
#define SDRAM_TXSR                        7       /* Exit Self Refresh Delay */
#define SDRAM_TRAS                        4       /* Active to Precharge Delay */
#define SDRAM_TRC                         6       /* Row Cycle Delay */
#define SDRAM_TWR                         2       /* Write Recovery Time */
#define SDRAM_TRP                         2       /* Row Precharge Delay */
#define SDRAM_TRCD                        2       /* RAS to CAS Delay */

/* Test Configuration */
#define SDRAM_TEST_START_ADDR            0xC0000000  /* SDRAM Bank1 base address */
#define SDRAM_TEST_SIZE_BYTES            (64 * 1024) /* 64KB test area */
#define SDRAM_TEST_PATTERN_1             0x55555555
#define SDRAM_TEST_PATTERN_2             0xAAAAAAAA
#define SDRAM_TEST_PATTERN_3             0xFFFFFFFF
#define SDRAM_TEST_PATTERN_4             0x00000000

/* Performance Test Iterations */
#define PERF_TEST_ITERATIONS             10000
#define PERF_TEST_BLOCK_SIZE             1024      /* 1KB blocks */

/*============================================================================*/
/*                           GLOBAL VARIABLES                                 */
/*============================================================================*/

SDRAM_HandleTypeDef hsdram1;

/* Performance metrics structure */
typedef struct
{
  uint32_t write_time_us;
  uint32_t read_time_us;
  uint32_t write_speed_mbps;
  uint32_t read_speed_mbps;
  uint32_t errors_found;
  uint32_t bytes_tested;
} SDRAM_Performance_t;

/*============================================================================*/
/*                           FUNCTION PROTOTYPES                              */
/*============================================================================*/

static uint32_t SDRAM_CalcRefreshRate(uint32_t sdclk_hz);
static uint32_t GetCurrentMicros(void);

/*============================================================================*/
/*                       SDRAM COMMAND FUNCTIONS                              */
/*============================================================================*/

/**
 * @brief  Send command to SDRAM
 * @param  CommandMode: Command to send
 * @param  Bank: Bank number (1 or 2)
 * @param  RefreshNum: Auto-refresh number (for auto-refresh command)
 * @param  RegVal: Mode register value (for load mode command)
 * @return 0 if OK, -1 if error
 */
int SDRAM_SendCommand(
  uint32_t CommandMode, uint32_t Bank, uint32_t RefreshNum, uint32_t RegVal)
{
  FMC_SDRAM_CommandTypeDef Command;

  /* Select target bank */
  if (Bank == 1)
  {
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  }
  else if (Bank == 2)
  {
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
  }
  else
  {
    Command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
  }

  Command.CommandMode = CommandMode;
  Command.AutoRefreshNumber = RefreshNum;
  Command.ModeRegisterDefinition = RegVal;

  if (HAL_SDRAM_SendCommand(&hsdram1, &Command, SDRAM_TIMEOUT) != HAL_OK)
  {
    return -1;
  }
  return 0;
}

/*============================================================================*/
/*                       CLOCK AND TIMING CALCULATIONS                        */
/*============================================================================*/

/**
 * @brief  Calculate refresh rate based on SDRAM clock frequency
 * @param  sdclk_hz: SDRAM clock frequency in Hz
 * @return Refresh timer count value
 */
static uint32_t SDRAM_CalcRefreshRate(uint32_t sdclk_hz)
{
  /* IS42S16400J requires 4096 refresh cycles every 64ms */
  /* Refresh interval = 64ms / 4096 = 15.625us */
  double refresh_interval_us = (double) SDRAM_REFRESH_PERIOD_MS
      * 1000.0/ SDRAM_REFRESH_ROWS;
  double clk_period_ns = 1000000000.0 / sdclk_hz;
  double count = (refresh_interval_us * 1000.0) / clk_period_ns;

  /* Subtract 20 cycles for safety margin as per datasheet */
  count -= 20.0;

  if (count < 0)
    count = 0;

  return (uint32_t) (count + 0.5); /* Round to nearest integer */
}

/**
 * @brief  Get current microsecond timestamp (DWT implementation)
 * @return Current microseconds
 */
static uint32_t GetCurrentMicros(void)
{
  /* Use DWT cycle counter for high precision timing */
  return (uint32_t) (DWT->CYCCNT / (SystemCoreClock / 1000000));
}

/**
 * @brief  Initialize DWT for cycle counting
 */
void DWT_Init(void)
{
  if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
  {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  }
}

/*============================================================================*/
/*                       SDRAM MEMORY TEST FUNCTIONS                          */
/*============================================================================*/

/**
 * @brief  Write pattern to SDRAM
 * @param  addr: Starting address
 * @param  size: Number of bytes to write
 * @param  pattern: 32-bit pattern to write
 * @return 0 if OK
 */
int SDRAM_WritePattern(uint32_t addr, uint32_t size, uint32_t pattern)
{
  uint32_t *ptr = (uint32_t*) addr;
  uint32_t words = size / 4;

  for ( uint32_t i = 0; i < words; i++ )
  {
    ptr[i] = pattern;
  }

  return 0;
}

/**
 * @brief  Verify pattern in SDRAM
 * @param  addr: Starting address
 * @param  size: Number of bytes to verify
 * @param  pattern: 32-bit pattern to verify
 * @return Number of errors found
 */
uint32_t SDRAM_VerifyPattern(uint32_t addr, uint32_t size, uint32_t pattern)
{
  uint32_t *ptr = (uint32_t*) addr;
  uint32_t words = size / 4;
  uint32_t errors = 0;

  for ( uint32_t i = 0; i < words; i++ )
  {
    if (ptr[i] != pattern)
    {
      errors++;
    }
  }

  return errors;
}

/**
 * @brief  Run marching test on SDRAM
 * @param  base_addr: Base address
 * @param  size: Size in bytes
 * @return Number of errors
 */
uint32_t SDRAM_MarchingTest(uint32_t base_addr, uint32_t size)
{
  uint32_t *ptr = (uint32_t*) base_addr;
  uint32_t words = size / 4;
  uint32_t errors = 0;
  uint32_t i;

  /* March element 1: Write background pattern */
  for ( i = 0; i < words; i++ )
  {
    ptr[i] = SDRAM_TEST_PATTERN_1;
  }

  /* March element 2: Read background, write complement */
  for ( i = 0; i < words; i++ )
  {
    if (ptr[i] != SDRAM_TEST_PATTERN_1)
      errors++;
    ptr[i] = SDRAM_TEST_PATTERN_2;
  }

  /* March element 3: Read complement, write original */
  for ( i = words; i > 0; i-- )
  {
    if (ptr[i - 1] != SDRAM_TEST_PATTERN_2)
      errors++;
    ptr[i - 1] = SDRAM_TEST_PATTERN_1;
  }

  /* Final verification */
  for ( i = 0; i < words; i++ )
  {
    if (ptr[i] != SDRAM_TEST_PATTERN_1)
      errors++;
  }

  return errors;
}

/**
 * @brief  Run address test (walking 1's)
 * @param  base_addr: Base address
 * @param  size: Size in bytes
 * @return Number of errors
 */
uint32_t SDRAM_AddressTest(uint32_t base_addr, uint32_t size)
{
  uint32_t *ptr = (uint32_t*) base_addr;
  uint32_t words = size / 4;
  uint32_t errors = 0;

  /* Write address to each location */
  for ( uint32_t i = 0; i < words; i++ )
  {
    ptr[i] = (uint32_t) &ptr[i];
  }

  /* Verify */
  for ( uint32_t i = 0; i < words; i++ )
  {
    if (ptr[i] != (uint32_t) &ptr[i])
    {
      errors++;
    }
  }
  return errors;
}

/**
 * @brief  Run random data test
 * @param  base_addr: Base address
 * @param  size: Size in bytes
 * @param  iterations: Number of test iterations
 * @return Number of errors
 */
uint32_t SDRAM_RandomTest(
  uint32_t base_addr, uint32_t size, uint32_t iterations)
{
  uint32_t *ptr = (uint32_t*) base_addr;
  uint32_t words = size / 4;
  uint32_t errors = 0;
  uint32_t *test_data = malloc(words * sizeof(uint32_t));

  if (test_data == NULL)
  {
    return 0xFFFFFFFF;
  }


  for ( uint32_t iter = 0; iter < iterations; iter++ )
  {
    /* Generate random data */
    for ( uint32_t i = 0; i < words; i++ )
    {
      test_data[i] = rand() | (rand() << 16);
    }

    /* Write to SDRAM */
    for ( uint32_t i = 0; i < words; i++ )
    {
      ptr[i] = test_data[i];
    }

    /* Verify */
    for ( uint32_t i = 0; i < words; i++ )
    {
      if (ptr[i] != test_data[i])
      {
        errors++;
      }
    }

    if ((iter + 1) % (iterations / 10) == 0)
    {
    }
  }

  free(test_data);
  return errors;
}

/*============================================================================*/
/*                       PERFORMANCE MEASUREMENT FUNCTIONS                    */
/*============================================================================*/

/**
 * @brief  Measure SDRAM write performance
 * @param  base_addr: Base address
 * @param  size_bytes: Size to test in bytes
 * @param  iterations: Number of iterations
 * @return Performance structure with results
 */
SDRAM_Performance_t SDRAM_MeasureWritePerformance(
  uint32_t base_addr, uint32_t size_bytes, uint32_t iterations)
{
  SDRAM_Performance_t perf = { 0 };
  uint32_t *ptr = (uint32_t*) base_addr;
  uint32_t words = size_bytes / 4;
  uint32_t start_time, end_time;
  uint32_t total_bytes = (uint64_t) size_bytes * iterations;

  /* Warm up */
  for ( uint32_t i = 0; i < words; i++ )
  {
    ptr[i] = SDRAM_TEST_PATTERN_1;
  }

  start_time = GetCurrentMicros();

  for ( uint32_t iter = 0; iter < iterations; iter++ )
  {
    for ( uint32_t i = 0; i < words; i++ )
    {
      ptr[i] = iter + i;
    }
  }

  end_time = GetCurrentMicros();

  perf.write_time_us = end_time - start_time;
  perf.write_speed_mbps = (uint32_t) ((total_bytes * 8) / perf.write_time_us);
  perf.bytes_tested = total_bytes;

  return perf;
}

/**
 * @brief  Measure SDRAM read performance
 * @param  base_addr: Base address
 * @param  size_bytes: Size to test in bytes
 * @param  iterations: Number of iterations
 * @return Performance structure with results
 */
SDRAM_Performance_t SDRAM_MeasureReadPerformance(
  uint32_t base_addr, uint32_t size_bytes, uint32_t iterations)
{
  SDRAM_Performance_t perf = { 0 };
  uint32_t *ptr = (uint32_t*) base_addr;
  uint32_t words = size_bytes / 4;
  uint32_t start_time, end_time;
  uint32_t total_bytes = (uint64_t) size_bytes * iterations;
  volatile uint32_t temp; /* Prevent optimization */

  /* Initialize data */
  for ( uint32_t i = 0; i < words; i++ )
  {
    ptr[i] = i;
  }

  start_time = GetCurrentMicros();

  for ( uint32_t iter = 0; iter < iterations; iter++ )
  {
    for ( uint32_t i = 0; i < words; i++ )
    {
      temp = ptr[i];
    }
  }

  end_time = GetCurrentMicros();

  perf.read_time_us = end_time - start_time;
  perf.read_speed_mbps = (uint32_t) ((total_bytes * 8) / perf.read_time_us);
  perf.bytes_tested = total_bytes;


  (void) temp; /* Suppress unused variable warning */
  return perf;
}

/**
 * @brief  Run comprehensive SDRAM test suite
 * @return 0 if all tests pass, non-zero if errors found
 */
int SDRAM_RunTestSuite(void)
{
  uint32_t total_errors = 0;
  SDRAM_Performance_t write_perf, read_perf;

  SDRAM_WritePattern(SDRAM_TEST_START_ADDR, SDRAM_TEST_SIZE_BYTES,
      SDRAM_TEST_PATTERN_1);
  total_errors += SDRAM_VerifyPattern(SDRAM_TEST_START_ADDR,
      SDRAM_TEST_SIZE_BYTES, SDRAM_TEST_PATTERN_1);

  SDRAM_WritePattern(SDRAM_TEST_START_ADDR, SDRAM_TEST_SIZE_BYTES,
      SDRAM_TEST_PATTERN_2);
  total_errors += SDRAM_VerifyPattern(SDRAM_TEST_START_ADDR,
      SDRAM_TEST_SIZE_BYTES, SDRAM_TEST_PATTERN_2);

  /* Test 2: Marching test */
  total_errors += SDRAM_MarchingTest(SDRAM_TEST_START_ADDR,
      SDRAM_TEST_SIZE_BYTES);

  /* Test 3: Address test */
  total_errors += SDRAM_AddressTest(SDRAM_TEST_START_ADDR,
      SDRAM_TEST_SIZE_BYTES);

  /* Test 4: Random test (limited iterations for time) */
  total_errors += SDRAM_RandomTest(SDRAM_TEST_START_ADDR, SDRAM_TEST_SIZE_BYTES,
      10);

  /* Performance measurements */
  write_perf = SDRAM_MeasureWritePerformance(SDRAM_TEST_START_ADDR,
  PERF_TEST_BLOCK_SIZE,
  PERF_TEST_ITERATIONS);
  read_perf = SDRAM_MeasureReadPerformance(SDRAM_TEST_START_ADDR,
  PERF_TEST_BLOCK_SIZE,
  PERF_TEST_ITERATIONS);

  (void) write_perf;
  (void) read_perf;

  return total_errors;
}

/* USER CODE END 0 */

SDRAM_HandleTypeDef hsdram1;

/* FMC initialization function */
void MX_FMC_Init(void)
{
  /* USER CODE BEGIN FMC_Init 0 */

  /* USER CODE END FMC_Init 0 */

  FMC_SDRAM_TimingTypeDef SdramTiming = {0};

  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SDRAM1 memory initialization sequence
  */
  hsdram1.Instance = FMC_SDRAM_DEVICE;
  /* hsdram1.Init */
  hsdram1.Init.SDBank = FMC_SDRAM_BANK1;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_1;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay = 2;
  SdramTiming.ExitSelfRefreshDelay = 7;
  SdramTiming.SelfRefreshTime = 4;
  SdramTiming.RowCycleDelay = 4;
  SdramTiming.WriteRecoveryTime = 2;
  SdramTiming.RPDelay = 4;
  SdramTiming.RCDDelay = 4;

  if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FMC_Init 2 */

  /* Initialize DWT for performance measurement */
  DWT_Init();

  /* SDRAM Initialization Sequence */
  /* Step 1: Clock Enable Command */
  if (SDRAM_SendCommand(FMC_SDRAM_CMD_CLK_ENABLE, 1, 1, 0) != 0)
  {
    Error_Handler();
  }

  for (volatile uint32_t i = 0; i < (SystemCoreClock / 1000) / 5; i++)
  {
    __NOP();
  }

  /* Step 2: Precharge All Command */
  if (SDRAM_SendCommand(FMC_SDRAM_CMD_PALL, 1, 1, 0) != 0)
  {
    Error_Handler();
  }

  /* Step 3: Auto-Refresh Commands (8 cycles as per datasheet) */
  if (SDRAM_SendCommand(FMC_SDRAM_CMD_AUTOREFRESH_MODE, 1, 8, 0) != 0)
  {
    Error_Handler();
  }

  /* Step 4: Load Mode Register */
  uint32_t mr = SDRAM_MODEREG_BURST_LENGTH_1 |
  SDRAM_MODEREG_BURST_TYPE_SEQ |
  SDRAM_MODEREG_CAS_LATENCY_3 |
  SDRAM_MODEREG_OP_MODE_STD |
  SDRAM_MODEREG_WBURST_SINGLE;

  if (SDRAM_SendCommand(FMC_SDRAM_CMD_LOAD_MODE, 1, 1, mr) != 0)
  {
    Error_Handler();
  }

  /* Step 5: Program Refresh Rate */
  /* Clock Configuration */

  uint32_t fmc_clock_freq_hz = SystemCoreClock / 2; /* FMC clock is HCLK/2 */
  uint32_t refresh_rate = SDRAM_CalcRefreshRate(fmc_clock_freq_hz);
  if (HAL_SDRAM_ProgramRefreshRate(&hsdram1, refresh_rate) != HAL_OK)
  {
    Error_Handler();
  }

  /* Optional: Run test suite (uncomment for testing) */
   //SDRAM_RunTestSuite();

  /* USER CODE END FMC_Init 2 */
}

static uint32_t FMC_Initialized = 0;

static void HAL_FMC_MspInit(void){
  /* USER CODE BEGIN FMC_MspInit 0 */

  /* USER CODE END FMC_MspInit 0 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if (FMC_Initialized) {
    return;
  }
  FMC_Initialized = 1;
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FMC;
    PeriphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_D1HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

  /* Peripheral clock enable */
  __HAL_RCC_FMC_CLK_ENABLE();

  /** FMC GPIO Configuration
  PF0   ------> FMC_A0
  PF1   ------> FMC_A1
  PF2   ------> FMC_A2
  PF3   ------> FMC_A3
  PF4   ------> FMC_A4
  PF5   ------> FMC_A5
  PC0   ------> FMC_SDNWE
  PC2_C   ------> FMC_SDNE0
  PC3_C   ------> FMC_SDCKE0
  PF11   ------> FMC_SDNRAS
  PF12   ------> FMC_A6
  PF13   ------> FMC_A7
  PF14   ------> FMC_A8
  PF15   ------> FMC_A9
  PG0   ------> FMC_A10
  PG1   ------> FMC_A11
  PE7   ------> FMC_D4
  PE8   ------> FMC_D5
  PE9   ------> FMC_D6
  PE10   ------> FMC_D7
  PE11   ------> FMC_D8
  PE12   ------> FMC_D9
  PE13   ------> FMC_D10
  PE14   ------> FMC_D11
  PE15   ------> FMC_D12
  PD8   ------> FMC_D13
  PD9   ------> FMC_D14
  PD10   ------> FMC_D15
  PD14   ------> FMC_D0
  PD15   ------> FMC_D1
  PG4   ------> FMC_BA0
  PG5   ------> FMC_BA1
  PG8   ------> FMC_SDCLK
  PD0   ------> FMC_D2
  PD1   ------> FMC_D3
  PG15   ------> FMC_SDNCAS
  PE0   ------> FMC_NBL0
  PE1   ------> FMC_NBL1
  */
  /* GPIO_InitStruct */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_11|GPIO_PIN_12
                          |GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /* GPIO_InitStruct */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* GPIO_InitStruct */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_8|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /* GPIO_InitStruct */
  GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* GPIO_InitStruct */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_FMC;

  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN FMC_MspInit 1 */

  /* USER CODE END FMC_MspInit 1 */
}

void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef* sdramHandle){
  /* USER CODE BEGIN SDRAM_MspInit 0 */

  /* USER CODE END SDRAM_MspInit 0 */
  HAL_FMC_MspInit();
  /* USER CODE BEGIN SDRAM_MspInit 1 */

  /* USER CODE END SDRAM_MspInit 1 */
}

static uint32_t FMC_DeInitialized = 0;

static void HAL_FMC_MspDeInit(void){
  /* USER CODE BEGIN FMC_MspDeInit 0 */

  /* USER CODE END FMC_MspDeInit 0 */
  if (FMC_DeInitialized) {
    return;
  }
  FMC_DeInitialized = 1;
  /* Peripheral clock enable */
  __HAL_RCC_FMC_CLK_DISABLE();

  /** FMC GPIO Configuration
  PF0   ------> FMC_A0
  PF1   ------> FMC_A1
  PF2   ------> FMC_A2
  PF3   ------> FMC_A3
  PF4   ------> FMC_A4
  PF5   ------> FMC_A5
  PC0   ------> FMC_SDNWE
  PC2_C   ------> FMC_SDNE0
  PC3_C   ------> FMC_SDCKE0
  PF11   ------> FMC_SDNRAS
  PF12   ------> FMC_A6
  PF13   ------> FMC_A7
  PF14   ------> FMC_A8
  PF15   ------> FMC_A9
  PG0   ------> FMC_A10
  PG1   ------> FMC_A11
  PE7   ------> FMC_D4
  PE8   ------> FMC_D5
  PE9   ------> FMC_D6
  PE10   ------> FMC_D7
  PE11   ------> FMC_D8
  PE12   ------> FMC_D9
  PE13   ------> FMC_D10
  PE14   ------> FMC_D11
  PE15   ------> FMC_D12
  PD8   ------> FMC_D13
  PD9   ------> FMC_D14
  PD10   ------> FMC_D15
  PD14   ------> FMC_D0
  PD15   ------> FMC_D1
  PG4   ------> FMC_BA0
  PG5   ------> FMC_BA1
  PG8   ------> FMC_SDCLK
  PD0   ------> FMC_D2
  PD1   ------> FMC_D3
  PG15   ------> FMC_SDNCAS
  PE0   ------> FMC_NBL0
  PE1   ------> FMC_NBL1
  */

  HAL_GPIO_DeInit(GPIOF, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_11|GPIO_PIN_12
                          |GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15);

  HAL_GPIO_DeInit(GPIOC, GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_3);

  HAL_GPIO_DeInit(GPIOG, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_8|GPIO_PIN_15);

  HAL_GPIO_DeInit(GPIOE, GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1);

  HAL_GPIO_DeInit(GPIOD, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1);

  /* USER CODE BEGIN FMC_MspDeInit 1 */

  /* USER CODE END FMC_MspDeInit 1 */
}

void HAL_SDRAM_MspDeInit(SDRAM_HandleTypeDef* sdramHandle){
  /* USER CODE BEGIN SDRAM_MspDeInit 0 */

  /* USER CODE END SDRAM_MspDeInit 0 */
  HAL_FMC_MspDeInit();
  /* USER CODE BEGIN SDRAM_MspDeInit 1 */

  /* USER CODE END SDRAM_MspDeInit 1 */
}
/**
  * @}
  */

/**
  * @}
  */
