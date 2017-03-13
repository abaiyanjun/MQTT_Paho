/******************************************************************************/
/*This software is copyrighted by Bosch Connected Devices and Solutions GmbH, 2015.
  The use of this software is subject to the XDK SDK EULA
 */
/**
 *
 *  @file        CAT1_UartDev.c
 *
 * Demo application of printing BME280 Environmental data on serialport(USB virtual comport)
 * every one second, initiated by Environmental timer(freertos)
 * 
 * ****************************************************************************/
/*****************************************************************************/
/*    INCLUDE FILE DECLARATIONS                                   							 */
/*****************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "DBG_assert_ih.h"
#include "em_cmu.h"
#include "SER_serialDriver_ih.h"
#include "PTD_portDriver_ph.h"
#include "PTD_portDriver_ih.h"
#include "RB_ringBuffer_ih.h"
#include "cat1_UartDev.h"





/*****************************************************************************/  
/*    DEFINE DECLARATIONS                   */
/*****************************************************************************/  
#define UART_EMPTY 0
#define UART_FULL 1



/*****************************************************************************/
//   VARIABLE DECLARATIONS
/*****************************************************************************/


/*********** Serial receive buffer for ringbuffer **************/
static uint8_t Cat1_ser2RxBuf_ma[Cat1_RX_BUFFER_SIZE];
static uint8_t Cat1_ser2TxBuf_ma[Cat1_TX_BUFFER_SIZE];

/*test API timer*/
static xTimerHandle	 usrTestTime  = 0;
static xTimerHandle	 testApi  = 0;


SER_device_t Cat1_uartHandle;
/*****************************************************************************/
/*    DATA TYPE DECLARATIONS                                               
/*****************************************************************************/
static SER_init_t Cat1_uartInit =
    {
        .hwType = SER_HWTYPE_UART,
        .hwDevicePort = SER_UART1,
        .protocol = SER_UART_PROTOCOL,
        .baudRate = UINT32_C(115200),
        .hasHwFlowControl = false,
        .txBuf_p = Cat1_ser2TxBuf_ma,
        .txBufSize = Cat1_TX_BUFFER_SIZE,
        .rxBuf_p = Cat1_ser2RxBuf_ma,
        .rxBufSize = Cat1_RX_BUFFER_SIZE,
        .parity = SER_PARITY_NONE,
        .dataBits = SER_DATABITS_8,
        .stopBits = SER_STOPBITS_ONE,
        .routeLocation = SER_ROUTE_LOCATION2,
        .txCallback = NULL,
       	.rxCallback = NULL,
        .rtsPort = UINT8_C(0),
        .rtsPin = UINT8_C(0),
        .rtspolarity = SER_activeLow,
        .ctsPort = UINT8_C(0),
        .ctsPin = UINT8_C(0),
        .ctspolarity = SER_activeLow };




/*****************************************************************************/
/*    PRIVATE FUNCTION DECLARATIONS  */
/*****************************************************************************/

//GPIO config modified by lihao 20160609
static void Cat1PtdConfig(void)
{

    /* configure the cat1 RX pin as input */
    PTD_driveModeSet(PTD_PORT_EXTENSION_UART1_RX, PTD_MODE_EXTENSION_UART1_RX);

    /* configure the cat1 RX pin as input */
    PTD_pinModeSet(PTD_PORT_EXTENSION_UART1_RX, PTD_PIN_EXTENSION_UART1_RX,
        (GPIO_Mode_TypeDef) PTD_MODE_EXTENSION_UART1_RX, PTD_GPIO_INPUT_FILTER_OFF);

    /* configure the cat1 TX pin as output */
    PTD_driveModeSet(PTD_PORT_EXTENSION_UART1_TX, PTD_MODE_EXTENSION_UART1_TX);

    /* configure the cat1 TX pin as output */
    PTD_pinModeSet(PTD_PORT_EXTENSION_UART1_TX, PTD_PIN_EXTENSION_UART1_TX,
        (GPIO_Mode_TypeDef) PTD_MODE_EXTENSION_UART1_TX, PTD_DOUT_EXTENSION_UART1_TX);

}

/*
 * @brief enable the clock and the serial initialize
 *
 *
 * @return the State of value . > 0 is error
 */
 

Cat1_return_t Cat1_DriverInit(void)
{
	SER_errorCode_t ret;

	/* Enable the USART clock */
	CMU_ClockEnable(cmuClock_UART1, true);

	/* BLE pin configure initialize */
	Cat1PtdConfig();

	/* Create Live Data Stream Timer */
	ret = SER_serialInit((SER_device_t*) &Cat1_uartHandle,(SER_init_t*) &Cat1_uartInit);
	if ( ret == SER_SUCCESS) {
		return (Cat1_STATUS_SUCCESS);
	} else {
		return (Cat1_STATUS_FAILED);
	}
}


/**/
static Cat1_return_t serial_data_byte_pop(uint8_t *recvData)
{
    uint8_t receiveBuffer;
    SER_errorCode_t retVal = SER_SUCCESS;
    uint32_t reminingBytes;
    Cat1_return_t status;

    /* Reads the required number of bytes from internal buffer */
    retVal = SER_serialRead(&Cat1_uartHandle, &reminingBytes, &receiveBuffer, 1);

    if (retVal == SER_SUCCESS) {
		*recvData = receiveBuffer;
		//printf("---recvdata is %s\n",recvData);
        return (Cat1_STATUS_SUCCESS);
    } else {
        return (Cat1_STATUS_FAILED);
    }
}



/*
 * @brief send the data to serial
 *
 *@param[in] trasmiBuffer - send to the serial data
 *@param[in] transmitLength - send the data length
 * @return the State of value . > 0 is error
 */

static Cat1_return_t send_data_to_device(uint8_t* transmitBuffer,uint16_t transmitLength)
{
	Cat1_return_t ret;

    DBG_ASSERT(transmitBuffer != NULL,"NULL pointer had been passed to the function instead of valid dataToSend.");
    DBG_ASSERT(transmitLength != UINT8_C(0), "data send length Cannot be Zero.");

    /* The data has been delivered to the remote device*/
	ret = SER_serialWrite(&Cat1_uartHandle, NULL, transmitBuffer, transmitLength);
    if ( ret == SER_SUCCESS) {
        return (Cat1_STATUS_SUCCESS);
    } else {
        return (Cat1_STATUS_FAILED);
    }
}


static Cat1_return_t receive_data_from_device(uint8_t *recvBufferData)
{
	int count = 0;
	uint8_t recvData;
	uint8_t uartState = UART_EMPTY;

	switch (uartState) {
	case UART_EMPTY:
	{
		if (Cat1_STATUS_SUCCESS == serial_data_byte_pop (&recvData)) {
			recvBufferData[count++] = recvData;
			uartState = UART_FULL;
		} else {
			return Cat1_STATUS_FAILED;
		}
	}

	case UART_FULL:
	{
		while (1) {
			if (Cat1_STATUS_SUCCESS == serial_data_byte_pop (&recvData)) {
				recvBufferData[count++] = recvData;
			} else {
				vTaskDelay(2);
				if (Cat1_STATUS_SUCCESS == serial_data_byte_pop (&recvData)) {
					recvBufferData[count++] = recvData;

				} else {
					return Cat1_STATUS_SUCCESS;
				}
			}
		}
	}

	default:
		break;
	}
}


/*
 * @brief reveive the serial data to the recvBufferData
 *
 *@param[in] cmdSend - send the data to serial
 *@param[in] cmdRecv - receive the data from the serial
 *@param[in] timeoutMs - Within a period of time to receive the data from the serial
 *@return the State of value . > 0 is error
 */

Cat1_return_t send_AT_cmd(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs)
{
	Cat1_return_t ret;
	uint64_t lastTime;
	uint64_t curTime;
	
	send_data_to_device(cmdSend,strlen(cmdSend));
	lastTime = PowerMgt_GetSystemTimeMs();
	curTime = PowerMgt_GetSystemTimeMs();

	while (timeoutMs > (curTime - lastTime)) {
		curTime = PowerMgt_GetSystemTimeMs();
		if ((curTime - lastTime) < 0) {
			lastTime = curTime;
		}

		ret = receive_data_from_device(cmdRecv);

		if (ret == Cat1_STATUS_SUCCESS) {
			return ret;
		} else {
			continue;
		}
	}
	return Cat1_STATUS_TIMEOUT;
}




Cat1_return_t send_recv_cmd(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs)
{
	Cat1_return_t ret;
	char *str = NULL;
	uint64_t lastTime;
	uint64_t curTime;
	//vTaskDelay
	send_data_to_device(cmdSend,strlen(cmdSend));
	lastTime = PowerMgt_GetSystemTimeMs();
	curTime = PowerMgt_GetSystemTimeMs();

	while (timeoutMs > (curTime - lastTime)) {
		curTime = PowerMgt_GetSystemTimeMs();
		if ((curTime - lastTime) < 0) {
			lastTime = curTime;
		}

		ret = receive_data_from_device(cmdRecv);

		if (ret == Cat1_STATUS_SUCCESS) {
			str = strstr(cmdRecv, "AT^SISHR=0");
			if(NULL != str) 
				return ret;
			 else 
				continue;
			
		} else {
			continue;
		}
	}
	return Cat1_STATUS_TIMEOUT;
}



Cat1_return_t send_send_cmd(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs)
{
	Cat1_return_t ret;
	char *str = NULL;
	uint64_t lastTime;
	uint64_t curTime;
	//vTaskDelay
	send_data_to_device(cmdSend,strlen(cmdSend));
	lastTime = PowerMgt_GetSystemTimeMs();
	curTime = PowerMgt_GetSystemTimeMs();

	while (timeoutMs > (curTime - lastTime)) {
		curTime = PowerMgt_GetSystemTimeMs();
		if ((curTime - lastTime) < 0) {
			lastTime = curTime;
		}

		ret = receive_data_from_device(cmdRecv);
		//printf("-------cmdRecv3-------\n%s\n",cmdRecv);

		if (ret == Cat1_STATUS_SUCCESS) {
			str = strstr(cmdRecv, "AT^SISH=0");
			if(NULL != str)
				return ret;
			else
				continue;
		} else {
			continue;
		}
	}
	return Cat1_STATUS_TIMEOUT;
}







