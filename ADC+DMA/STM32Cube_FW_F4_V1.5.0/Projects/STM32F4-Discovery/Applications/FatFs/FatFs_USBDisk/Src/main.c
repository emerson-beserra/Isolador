/**
  ******************************************************************************
  * @file    FatFs/FatFs_USBDisk/Src/main.c 
  * @author  MCD Application Team
  * @version V1.2.1
  * @date    13-March-2015
  * @brief   Main program body
  *          This sample code shows how to use FatFs with USB disk drive.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"    

/* Private typedef -----------------------------------------------------------*/
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

/* Variable used to get converted value */
__IO uint32_t uhADCxConvertedValue[SAMPLES_SIZE];

volatile bool conversion_done = false;
bool fft_conversion = false;

/* ADC handler declaration */
ADC_HandleTypeDef    AdcHandle;

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
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);
static void MSC_Application(void);
static bool write_register_in_file( uint32_t * raw_data, float * fft_data, const uint32_t size_raw_data );
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
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
  
  /* Configure LED4 and LED5 */
  BSP_LED_Init(LED4);
  BSP_LED_Init(LED5);  
  
  /* Configure the system clock to 168 MHz */
  SystemClock_Config();
  
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
  
  /*##-1- Link the USB Host disk I/O driver ##################################*/
  if(FATFS_LinkDriver(&USBH_Driver, USBDISKPath) == 0)
  {
    /*##-2- Init Host Library ################################################*/
    USBH_Init(&hUSB_Host, USBH_UserProcess, 0);
    
    /*##-3- Add Supported Class ##############################################*/
    USBH_RegisterClass(&hUSB_Host, USBH_MSC_CLASS);
    
    /*##-4- Start Host Process ###############################################*/
    USBH_Start(&hUSB_Host);
    
//    
//      conversion_done = true;
//  
//  uhADCxConvertedValue[0] = 0;
//  uhADCxConvertedValue[1] = 1;
//  uhADCxConvertedValue[2] = 2;
//  uhADCxConvertedValue[3] = 3;
//  uhADCxConvertedValue[4] = 4;
//  uhADCxConvertedValue[5] = 5;
//  uhADCxConvertedValue[6] = 6;
//  uhADCxConvertedValue[7] = 7;
//  uhADCxConvertedValue[8] = 8;
//  uhADCxConvertedValue[9] = 9;
//  uhADCxConvertedValue[10] =10;
//  uhADCxConvertedValue[11] =11;
//  uhADCxConvertedValue[12] =12;
//  uhADCxConvertedValue[13] =13;
//  uhADCxConvertedValue[14] =14;
//  uhADCxConvertedValue[15] =15;
  
    /*##-5- Run Application (Blocking mode) ##################################*/
    while (1)
    {
      /* USB Host Background task */
      USBH_Process(&hUSB_Host);
      
      if( conversion_done == true )
      {
        if( fft_conversion == false )
        {
          arm_rfft_fast_init_f32( &S, SAMPLES_SIZE );
          arm_rfft_fast_f32( &S, ( float * ) uhADCxConvertedValue, fft_out, 0 );
          /* after this point the result of fft wil be in fft_out */
          fft_conversion = true;
        }
        
        if(f_mount(&USBDISKFatFs, (TCHAR const*)USBDISKPath, 0) == FR_OK)
        {
          if( write_register_in_file( ( uint32_t * ) uhADCxConvertedValue, fft_out, SAMPLES_SIZE ) == true )
          {
            FATFS_UnLinkDriver(USBDISKPath);
            conversion_done = false;
            fft_conversion = false;
          }
          else
          {
          }/* end if-else */
        }
        else
        {
        }/* end if-else */
      }
      else
      {
      }/* end if-else */
    }/* end while */
  }
  else
  {
  }/* end if-else */
  
  /* TrueStudio compilation error correction */
  while (1)
  {
    Error_Handler();
  }
}/*end main()-----------------------------------------------------------------*/

/**
 * Method that will create the file with the data acquired by DMA.
 *
 * @param  None
 *
 * @return None
 *
 * @author Emerson Beserra
 * @since 21/05/15
 */
static bool write_register_in_file( uint32_t * raw_data, float * fft_data, const uint32_t size_raw_data )
{
  static uint32_t name_file = 0;
  uint8_t name_file_str[8];
  uint32_t idx_array = 0;
  uint8_t buffer[64];
  bool val_ret = false;
  
  sprintf( ( char* ) name_file_str, "%d.csv", name_file );
  
  if( f_open( &MyFile, ( char const* ) name_file_str, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK ) 
  {
  }
  else
  {
    f_puts( "indice; aquisicao; ftt \r", &MyFile );
      
    for( idx_array = 0; idx_array < size_raw_data; idx_array++ )
    {
      sprintf( ( char* ) buffer, "%d; %d; %g \r", idx_array, raw_data[idx_array], fft_data[idx_array] ); 
      f_puts( ( char* ) buffer, &MyFile );
    }/* end if-else */
    
    f_close( &MyFile );
    val_ret = true;
    ++name_file; /* file write succesfully, inc the name to the next file */
    if( name_file > 1000 )
    {
      name_file = 0;
    }
    else
    {
    }/* end if-else */
  }/* end if-else */
  
  return val_ret;
}/*end write_register_in_file()-----------------------------------------------*/

/**
  * @brief  Main routine for Mass Storage Class
  * @param  None
  * @retval None
  */
static void MSC_Application(void)
{
  FRESULT res;                                          /* FatFs function common result code */
  uint32_t byteswritten, bytesread;                     /* File write/read counts */
  uint8_t wtext[] = "This is STM32 working with FatFs"; /* File write buffer */
  uint8_t rtext[100];                                   /* File read buffer */
  
  /* Register the file system object to the FatFs module */
  if(f_mount(&USBDISKFatFs, (TCHAR const*)USBDISKPath, 0) != FR_OK)
  {
    /* FatFs Initialization Error */
    Error_Handler();
  }
  else
  {
      /* Create and Open a new text file object with write access */
      if(f_open(&MyFile, "STM32.TXT", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) 
      {
        /* 'STM32.TXT' file Open for write Error */
        Error_Handler();
      }
      else
      {
        /* Write data to the text file */
        res = f_write(&MyFile, wtext, sizeof(wtext), (void *)&byteswritten);
        
        if((byteswritten == 0) || (res != FR_OK))
        {
          /* 'STM32.TXT' file Write or EOF Error */
          Error_Handler();
        }
        else
        {
          /* Close the open text file */
          f_close(&MyFile);
          
        /* Open the text file object with read access */
        if(f_open(&MyFile, "STM32.TXT", FA_READ) != FR_OK)
        {
          /* 'STM32.TXT' file Open for read Error */
          Error_Handler();
        }
        else
        {
          /* Read data from the text file */
          res = f_read(&MyFile, rtext, sizeof(rtext), (void *)&bytesread);
          
          if((bytesread == 0) || (res != FR_OK))
          {
            /* 'STM32.TXT' file Read or EOF Error */
            Error_Handler();
          }
          else
          {
            /* Close the open text file */
            f_close(&MyFile);
            
            /* Compare read data with the expected data */
            if((bytesread != byteswritten))
            {                
              /* Read data is different from the expected data */
              Error_Handler();
            }
            else
            {
          /* Success of the demo: no error occurrence */
              BSP_LED_On(LED4);
            }
          }
        }
      }
    }
  }
  
  /* Unlink the USB disk I/O driver */
  FATFS_UnLinkDriver(USBDISKPath);
}

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
    Appli_state = APPLICATION_IDLE;
    BSP_LED_Off(LED4); 
    BSP_LED_Off(LED5);  
    f_mount(NULL, (TCHAR const*)"", 0);          
    break;
    
  case HOST_USER_CLASS_ACTIVE:
    Appli_state = APPLICATION_START;
    break;
    
  default:
    break; 
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


#if 0
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 168000000
  *            HCLK(Hz)                       = 168000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 8000000
  *            PLL_M                          = 8
  *            PLL_N                          = 336
  *            PLL_P                          = 2
  *            PLL_Q                          = 7
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 5
  * @param  None
  * @retval None
  */
static void SystemClock_Config  (void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();
  
  /* The voltage scaling allows optimizing the power consumption when the device is 
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig (&RCC_OscInitStruct);
  
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;  
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  /* STM32F405x/407x/415x/417x Revision Z devices: prefetch is supported  */
  if (HAL_GetREVID() == 0x1001)
  {
    /* Enable the Flash prefetch */
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
  }
}

#else
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
#endif

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
  /* Turn LED5 on */
  BSP_LED_On(LED5);
  while(1)
  {
  }
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
