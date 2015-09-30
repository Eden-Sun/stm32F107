
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
#include "lwip/pbuf.h"
#include "lwip/raw.h"


struct netif gnetif;
USBD_HandleTypeDef USBD_Device;
FATFS SDFatFs;  /* File system object for SD card logical drive */
FIL MyFile;     /* File object */
char SDPath[4]; /* SD card logical drive path */
FRESULT res;
uint32_t byteswritten, bytesread;                     /* File write/read counts */
uint8_t product_version[] = "N-BOX v0.0.0"; /* File write buffer */

uint8_t sw_status[3];
uint8_t sw_status_temp;                                   /* File read buffer */
uint16_t getHandShake=0;
uint8_t rtext[100];
uint8_t buf[60]={
  0x01,0x80,0xc2,0x00,0x00,0x0e,
  0x02,0x00,0x00,0x00,0x00,0x00,
  0x67,0x27
};
uint16_t firmName_length;
uint16_t firmName_counter=0;
uint8_t firmName[60];
/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void IO_Init(void);
static void Netif_Config(uint8_t,uint8_t,uint8_t,uint8_t);
static void Netif_ChageIp(uint8_t ip3,uint8_t ip2,uint8_t ip1,uint8_t ip0);
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
  

  // passed sd check
  BSP_LED_Off(LED_FINISH0);
  BSP_LED_Off(LED_STATE0);
  BSP_LED_On(LED_FINISH1);
  
  if( BSP_PB_GetState(SW1) == 0){    // switch on sw1 to be USB mode
    FATFS_UnLinkDriver(SDPath);  // only unlink if going to USB MODE ******   VERY IMPORTANT
		USBD_Init(&USBD_Device, &MSC_Desc, 0);
    USBD_RegisterClass(&USBD_Device, USBD_MSC_CLASS);
    USBD_MSC_RegisterStorage(&USBD_Device, &USBD_DISK_fops);
    USBD_Start(&USBD_Device);
    while(1);
  }else{
    // check file to import
    DIR dj;         /* Directory search object */
    FILINFO fno;    /* File information */
    char lfn[_MAX_LFN + 1];
    fno.lfname = lfn;
    fno.lfsize = _MAX_LFN + 1;
    res = f_findfirst(&dj, &fno, "", "*.upg");
    while (res == FR_OK && fno.fname[0] && firmName_counter<2) {
			if((*fno.lfname)!='.'){
        firmName_length = strlen(fno.lfname);
        strcpy(firmName,fno.lfname);
        firmName_counter++;
      }
      res = f_findnext(&dj, &fno);              /* Search for next item */
		}
    f_closedir(&dj);
		/* Initilaize the LwIP stack */
		lwip_init();
		/* Configure the Network interface */
		Netif_Config(192,168,0,12);  
		/* tcp echo server Init */
		//tcp_echoserver_init();
		//udp_echoclient_connect();
	}


  /* Infinite loop */
  sw_status[2] = sw_status_temp;
  struct pbuf *leyer2 ;
	leyer2 = pbuf_alloc(PBUF_TRANSPORT,60,PBUF_POOL);
  struct ip_addr ;
  getHandShake = 1 ;
  buf[14]=0x0f; // set get-IP packet
  pbuf_take(leyer2,buf,40);
  sw_status_temp = BSP_PB_GetState(SW2);
    // if( sw_status[2] != sw_status_temp){
      // sw_status[2] = sw_status_temp;
      // if(sw_status_temp)BSP_LED_Off(LED_STATE1);
      // else BSP_LED_On(LED_STATE1);
    // }
    // int ss = BSP_PB_GetState(BUTTON_START);
  while (getHandShake)
  {  
    if(getHandShake%512==1){
      gnetif.linkoutput(&gnetif,leyer2);
      BSP_LED_Toggle(LED_STATE0);
    }
    HAL_Delay(1);
    if(getHandShake==65535)getHandShake=1;
    else getHandShake++;
    ethernetif_input(&gnetif,getHandShake,rtext);
    if(rtext[12]==0x67&&rtext[13]==0x27&&rtext[14]==0x8f){
      getHandShake = 0;
      BSP_LED_Off(LED_STATE0);
      BSP_LED_On(LED_STATE1);
      Netif_ChageIp(rtext[15],rtext[16],rtext[17],rtext[18]);  
      tftpd_init();
    }
    sys_check_timeouts();
  }
  while(1){
    if(BSP_PB_GetState(BUTTON_START)==0){   // press start btn
      HAL_Delay(50);
      while(BSP_PB_GetState(BUTTON_START)==0);
      BSP_LED_Toggle(LED_STATE1);
      uint8_t switchMode = BSP_PB_GetState(SW1)<<2 | BSP_PB_GetState(SW2)<<1 | BSP_PB_GetState(SW3);
      switch(switchMode){
        case 7:    //jumper 000 export config 
          buf[14]=0x00;
        break;
        case 6:    //jumper 001 import config 
          buf[14]=0x01;
        break;
        case 4:    //jumper 01X upload firmware 
        case 5:        
          buf[14]=0x02;
          buf[15]=firmName_length;
          strcpy(buf+16,firmName);
        break;
        default:
        break;
      }
      pbuf_take(leyer2,buf,40);
      gnetif.linkoutput(&gnetif,leyer2);
    }
    ethernetif_input(&gnetif,0,rtext);
    /* Handle timeouts */
    sys_check_timeouts();
  }
}

static void IO_Init(void)
{
  /* Initialize STM3210C-EVAL's LEDs & BUTTONs */
  BSP_LED_Init(LED_FINISH0);
	BSP_LED_Init(LED_FINISH1);
	BSP_LED_Init(LED_STATE0);
	BSP_LED_Init(LED_STATE1);
  BSP_PB_Enable_All_Clock();
  BSP_PB_Init(SW1,BUTTON_MODE_GPIO);
  BSP_PB_Init(SW2,BUTTON_MODE_GPIO);
  BSP_PB_Init(SW3,BUTTON_MODE_GPIO);
  BSP_PB_Init(BUTTON_START,BUTTON_MODE_GPIO);
 
}

static void Netif_ChageIp(uint8_t ip3,uint8_t ip2,uint8_t ip1,uint8_t ip0)
{
  struct ip_addr ipaddr;
  struct ip_addr netmask;
  struct ip_addr gw;
  
  IP4_ADDR(&ipaddr, ip3, ip2, ip1, ip0);
  IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
  IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);

  netif_set_addr(&gnetif,&ipaddr,&netmask,&gw);
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
  //netif_set_link_callback(&gnetif, ethernetif_update_config);
	
	// not work link changes,and still works without it
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
