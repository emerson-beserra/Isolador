/**
  ******************************************************************************
  * @file    ADC/ADC_RegularConversion_DMA/Src/main.c 
  * @author  MCD Application Team
  * @version V1.1.0
  * @date    26-June-2014
  * @brief   This example describes how to use the DMA to transfer continuously
  *          converted data.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"


/** @addtogroup STM32F4xx_HAL_Examples
  * @{
  */

/** @addtogroup ADC_RegularConversion_DMA
  * @{
  */ 

/* Private typedef -----------------------------------------------------------*/
    typedef enum State_Type
    {
      CONFIGURADO = 0,
      DADOS_CAPTURADOS,
      DADOS_SALVOS,
      USOM_PROCESSADO,
      RF_PROCESSADO,
      RNA_REPOSTA,
      REPOSTA_ARMAZENADA,
      INFO_TRANSMITIDA
    };

/* Private define ------------------------------------------------------------*/
#define SAMPLES_SIZE 4096
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
FATFS USBDISKFatFs;           /* File system object for USB disk logical drive */
FIL MyFile;                   /* File object */
char USBDISKPath[4];          /* USB Host logical drive path */
USBH_HandleTypeDef hUSB_Host; /* USB Host handle */

typedef enum {
  APPLICATION_IDLE = 0,  
  APPLICATION_START,    
  APPLICATION_RUNNING,
}MSC_ApplicationTypeDef;

MSC_ApplicationTypeDef Appli_state = APPLICATION_IDLE;

/* ADC handler declaration */
ADC_HandleTypeDef    AdcHandle;
State_Type estadoAtual;

/* Variable used to get converted value */
__IO uint32_t uhADCxConvertedValue[SAMPLES_SIZE];

volatile bool conversion_done = false;

/** 
 * Structure that contains information about the fft.
 */
static arm_rfft_fast_instance_f32 S;

/**
 * The FFT result. It's half the size needed because the function will ignore 
 * the seconde half of the computation.
 */
static float fft_out[SAMPLES_SIZE];

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);
void (*tabela_estados[8]) () = {Configurado, DadosCapturados, DadosSalvos, UsomProcessado, Rf_Processado, 
                  RnaResposta, RespostaArmazenada, InfoTransmitida};

static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);

static void write_register_in_file( const float raw_data[], const uint32_t size_raw_data );

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
int main(void)
{
  ADC_ChannelConfTypeDef sConfig;
  
  /* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */
  HAL_Init();
  
  /* Configure the system clock to 144 Mhz */
  SystemClock_Config();
  
  /* Configure LED4 and LED5 */
  BSP_LED_Init(LED4);
  BSP_LED_Init(LED5);
  
  /*##-1- Configure the ADC peripheral #######################################*/
  AdcHandle.Instance = ADCx;
  
  AdcHandle.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV8;
  AdcHandle.Init.Resolution = ADC_RESOLUTION12b;
  AdcHandle.Init.ScanConvMode = DISABLE;
  AdcHandle.Init.ContinuousConvMode = ENABLE;
  AdcHandle.Init.DiscontinuousConvMode = DISABLE;
  AdcHandle.Init.NbrOfDiscConversion = 0;
  AdcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  AdcHandle.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
  AdcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  AdcHandle.Init.NbrOfConversion = 1;
  AdcHandle.Init.DMAContinuousRequests = ENABLE;
  AdcHandle.Init.EOCSelection = DISABLE;
      
  if(HAL_ADC_Init(&AdcHandle) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler(); 
  }
  
  /*##-2- Configure ADC regular channel ######################################*/  
  sConfig.Channel = ADCx_CHANNEL;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES;
  sConfig.Offset = 0;
  
  if(HAL_ADC_ConfigChannel(&AdcHandle, &sConfig) != HAL_OK)
  {
    /* Channel Configuration Error */
    Error_Handler(); 
  }

  /*##-3- Start the conversion process and enable interrupt ##################*/  
  if(HAL_ADC_Start_DMA(&AdcHandle,(uint32_t*)&uhADCxConvertedValue, SAMPLES_SIZE) != HAL_OK)
  {
    /* Start Conversation Error */
    Error_Handler(); 
  }
  
  conversion_done = false;
  
  /* Infinite loop */
  while (1)
  {
      if( conversion_done == true )
      {
          conversion_done = false;
          arm_rfft_fast_init_f32( &S, SAMPLES_SIZE );
          arm_rfft_fast_f32( &S, ( float * ) uhADCxConvertedValue, fft_out, 0 );
          /* after this point the result of fft wil be in fft_out */
          
          if(FATFS_LinkDriver(&USBH_Driver, USBDISKPath) == 0)
          {
            /*##-2- Init Host Library ################################################*/
            USBH_Init(&hUSB_Host, USBH_UserProcess, 0);
            
            /*##-3- Add Supported Class ##############################################*/
            USBH_RegisterClass(&hUSB_Host, USBH_MSC_CLASS);
            
            /*##-4- Start Host Process ###############################################*/
            USBH_Start(&hUSB_Host);
            
            /*##-5- Run Application (Blocking mode) ##################################*/
            /* USB Host Background task */
            USBH_Process(&hUSB_Host);
  
            f_mount(&USBDISKFatFs, (TCHAR const*)USBDISKPath, 0);
            write_register_in_file( ( float const* ) uhADCxConvertedValue, SAMPLES_SIZE );
            FATFS_UnLinkDriver(USBDISKPath);
            
          }
      }
      else
      {
      }/* end if-else */
  }
}


static void write_register_in_file( const float raw_data[], const uint32_t size_raw_data )
{
  static uint32_t name_file = 0;
  uint8_t name_file_str[8];
  uint32_t idx_array = 0;
  uint8_t buffer[64];
  
  sprintf( ( char* ) name_file_str, "%d.csv", name_file );
  
  if( f_open( &MyFile, ( char const* ) name_file_str, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK ) 
  {
  }
  else
  {
    f_puts( "indice, valores\r\n", &MyFile );
      
    for( idx_array = 0; idx_array < size_raw_data; idx_array++ )
    {
      sprintf( ( char* ) buffer, "%d, %f \r\n", idx_array, raw_data[idx_array] ); 
      f_puts( ( char* ) buffer, &MyFile );
    }/* end if-else */
    
    f_close( &MyFile );
    ++name_file; /* file write succesfully, inc the name to the next file */
    if( name_file > 1000 )
    {
      name_file = 0;
    }
    else
    {
    }/* end if-else */
  }/* end if-else */
  
}/*end write_register_in_file()-----------------------------------------------*/

/**
  * @brief  User Process
  * @param  phost: Host handle
  * @param  id: Host Library user message ID
  * @retval None
  */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id)
{  
  switch(id)
  { 
  case HOST_USER_SELECT_CONFIGURATION:
    break;
    
  case HOST_USER_DISCONNECTION:
  // Appli_state = APPLICATION_IDLE;
    BSP_LED_Off(LED4); 
    BSP_LED_Off(LED5);  
    f_mount(NULL, (TCHAR const*)"", 0);          
    break;
    
  case HOST_USER_CLASS_ACTIVE:
    //Appli_state = APPLICATION_START;
    break;
    
  default:
    break; 
  }
}


/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 144000000
  *            HCLK(Hz)                       = 144000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 8000000
  *            PLL_M                          = 8
  *            PLL_N                          = 288
  *            PLL_P                          = 2
  *            PLL_Q                          = 6
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale2 mode
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __PWR_CLK_ENABLE();
  
  /* The voltage scaling allows optimizing the power consumption when the device is 
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 288;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 6;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;  
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
    /* Turn LED5 (RED) on */
    BSP_LED_On(LED5);
    while(1)
    {
    }
}

/**
  * @brief  Conversion complete callback in non blocking mode 
  * @param  AdcHandle : AdcHandle handle
  * @note   This example shows a simple way to report end of conversion, and 
  *         you can add your own implementation.    
  * @retval None
  */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* AdcHandle)
{
  /* Turn LED4 on: Transfer process is correct */
  HAL_ADC_Stop_DMA(AdcHandle);
  BSP_LED_On(LED4);
    conversion_done = true;
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}

#endif

void Configurado()
{}
void DadosCapturados()
{} 
void DadosSalvos()
{} 
void UsomProcessado()
{} 
void Rf_Processado()
{}
void RnaResposta()
{} 
void RespostaArmazenada()
{} 
void InfoTransmitida()
{}

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
