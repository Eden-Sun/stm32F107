
#include "main.h"
#include "lwip/opt.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/lwip_timers.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "app_ethernet.h"
#include "tcp_echoserver.h"
#include "lwip/sockets.h"

struct netif gnetif;
USBD_HandleTypeDef USBD_Device;
FATFS SDFatFs;  /* File system object for SD card logical drive */
FIL MyFile;     /* File object */
char SDPath[4]; /* SD card logical drive path */
FRESULT res;
uint32_t byteswritten, bytesread;                     /* File write/read counts */
uint8_t product_version[] = "N-BOX v0.0.0"; /* File write buffer */
uint8_t rtext[100];                                   /* File read buffer */
/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void IO_Init(void);
static void Netif_Config(uint8_t,uint8_t,uint8_t,uint8_t);
static void Error_Handler(void)
{
  while(1)
  {
    /* Toggle LED_RED fast */
    BSP_LED_Toggle(LED_STATE0);
    HAL_Delay(40);
  }
}

int main(void)
{
	HAL_Init();  
  SystemClock_Config(); // Configure the system clock to 72 Mhz 
	IO_Init();
  // first do a sd check
  FATFS_LinkDriver(&SD_Driver,SDPath);
  f_mount(&SDFatFs,(TCHAR const*)SDPath,0);
  f_open(&MyFile , "version.txt" , FA_CREATE_ALWAYS | FA_READ | FA_WRITE);
  res = f_write(&MyFile,product_version,sizeof(product_version),(void *)&byteswritten);
  if(res != FR_OK)  // means if no plug in a sd card then blink reds and reboot
  {  
    HAL_Delay(50);  // reboot take about 100ms
		BSP_LED_Toggle(LED_FINISH0);
    BSP_LED_Toggle(LED_STATE0);
    HAL_Delay(150);
		NVIC_SystemReset();
  }
  f_close(&MyFile);
  FATFS_UnLinkDriver(SDPath);

  // passed sd check
  BSP_LED_Off(LED_FINISH0);
  BSP_LED_Off(LED_STATE0);
  BSP_LED_On(LED_FINISH1);
  BSP_LED_On(LED_STATE1);
  if( BSP_PB_GetState(SW1) == 0){    // switch on sw1 to be USB mode
    USBD_Init(&USBD_Device, &MSC_Desc, 0);
    USBD_RegisterClass(&USBD_Device, USBD_MSC_CLASS);
    USBD_MSC_RegisterStorage(&USBD_Device, &USBD_DISK_fops);
    USBD_Start(&USBD_Device);
    while(1);
  }else{
		/* Initilaize the LwIP stack */
		lwip_init();
		/* Configure the Network interface */
		Netif_Config(192,168,0,10);  
		tftpd_init();
		/* tcp echo server Init */
		tcp_echoserver_init();
		udp_echoclient_connect();
	}

  /* Infinite loop */
  while (1)
  {  
    // int s1 = 
    // int s2 = BSP_PB_GetState(SW2);
    // int s3 = BSP_PB_GetState(SW3);
    // int ss = BSP_PB_GetState(BUTTON_START);
		ethernetif_input(&gnetif);
			/* Handle timeouts */
		sys_check_timeouts();
  }
}

static void IO_Init(void)
{
  /* Initialize STM3210C-EVAL's LEDs */
  BSP_LED_Init(LED_FINISH0);
	BSP_LED_Init(LED_FINISH1);
	BSP_LED_Init(LED_STATE0);
	BSP_LED_Init(LED_STATE1);
  BSP_PB_Enable_All_Clock();
  BSP_PB_Init(SW1,BUTTON_MODE_GPIO);
  BSP_PB_Init(SW2,BUTTON_MODE_GPIO);
  BSP_PB_Init(SW3,BUTTON_MODE_GPIO);
  BSP_PB_Init(BUTTON_START,BUTTON_MODE_GPIO);
  //BSP_LED_Init(LED3);
}


static void Netif_Config(uint8_t ip3,uint8_t ip2,uint8_t ip1,uint8_t ip0)
{
  struct ip_addr ipaddr;
  struct ip_addr netmask;
  struct ip_addr gw;
  
  IP4_ADDR(&ipaddr, ip3, ip2, ip1, ip0);
  IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
  IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
  
  /* Add the network interface 
    the ethernetif_init pointer can initial the eth hw setting 
  */ 
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);
  
  /* Registers the default network interface */
  netif_set_default(&gnetif);
  
  if (netif_is_link_up(&gnetif))
  {
    /* When the netif is fully configured this function must be called */
    netif_set_up(&gnetif);
  }
  else
  {
    /* When the netif link is down this function must be called */
    netif_set_down(&gnetif);
  }
  
  /* Set the link callback function, this function is called on change of link status*/
  netif_set_link_callback(&gnetif, ethernetif_update_config);
}


void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef clkinitstruct = {0};
  RCC_OscInitTypeDef oscinitstruct = {0};
  RCC_PeriphCLKInitTypeDef rccperiphclkinit = {0};
  /* Configure PLLs ------------------------------------------------------*/
  /* PLL2 configuration: PLL2CLK = (HSE / HSEPrediv2Value) * PLL2MUL = (25 / 5) * 8 = 40 MHz */
  /* PREDIV1 configuration: PREDIV1CLK = PLL2CLK / HSEPredivValue = 40 / 5 = 8 MHz */
  /* PLL configuration: PLLCLK = PREDIV1CLK * PLLMUL = 8 * 9 = 72 MHz */
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  oscinitstruct.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
  oscinitstruct.HSEState        = RCC_HSE_ON;
  oscinitstruct.HSEPredivValue  = RCC_HSE_PREDIV_DIV5;
  oscinitstruct.PLL.PLLMUL      = RCC_PLL_MUL9; 
  oscinitstruct.Prediv1Source   = RCC_CFGR2_PREDIV1SRC_PLL2;
  oscinitstruct.PLL.PLLState    = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
  oscinitstruct.PLL2.PLL2State  = RCC_PLL2_ON;  
  oscinitstruct.PLL2.HSEPrediv2Value = RCC_HSE_PREDIV2_DIV5;
  oscinitstruct.PLL2.PLL2MUL    = RCC_PLL2_MUL8;
  HAL_RCC_OscConfig(&oscinitstruct);
  
	/* USB clock selection */
  rccperiphclkinit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  rccperiphclkinit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV3;
  HAL_RCCEx_PeriphCLKConfig(&rccperiphclkinit);
	
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
  clocks dividers */
  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;  
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2);

}


#ifdef  USE_FULL_ASSERT


void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {}
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
