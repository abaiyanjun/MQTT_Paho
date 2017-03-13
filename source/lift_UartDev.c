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
#include "lift_UartDev.h"





/*****************************************************************************/  
/*    DEFINE DECLARATIONS                   */
/*****************************************************************************/  
#define UART_EMPTY 0
#define UART_FULL 1



/*****************************************************************************/
//   VARIABLE DECLARATIONS
/*****************************************************************************/


/*********** Serial receive buffer for ringbuffer **************/
static uint8_t Lift_ser2RxBuf_ma[Lift_RX_BUFFER_SIZE];
static uint8_t Lift_ser2TxBuf_ma[Lift_TX_BUFFER_SIZE];

/*test API timer*/
static xTimerHandle	 usrTestTime  = 0;
static xTimerHandle	 testApi  = 0;


SER_device_t Lift_uartHandle;
/*****************************************************************************/
/*    DATA TYPE DECLARATIONS                                               
/*****************************************************************************/
static SER_init_t Lift_uartInit =
    {
        .hwType = SER_HWTYPE_USART,
        .hwDevicePort = SER_USART2,
        .protocol = SER_UART_PROTOCOL,
        .baudRate = UINT32_C(9600),
        .hasHwFlowControl = false,
        .txBuf_p = Lift_ser2TxBuf_ma,
        .txBufSize = Lift_TX_BUFFER_SIZE,
        .rxBuf_p = Lift_ser2RxBuf_ma,
        .rxBufSize = Lift_RX_BUFFER_SIZE,
        .parity = SER_PARITY_NONE,
        .dataBits = SER_DATABITS_8,
        .stopBits = SER_STOPBITS_ONE,
        .routeLocation = SER_ROUTE_LOCATION1,
        .txCallback = NULL,
       	.rxCallback = NULL,
        .rtsPort = UINT8_C(0),
        .rtsPin = UINT8_C(0),
        .rtspolarity = SER_activeLow,
        .ctsPort = UINT8_C(0),
        .ctsPin = UINT8_C(0),
        .ctspolarity = SER_activeLow
};




/*****************************************************************************/
/*    PRIVATE FUNCTION DECLARATIONS  */
/*****************************************************************************/

/** UART TX pin configuration for Extension Bus */
#define PTD_PORT_EXTENSION_USART2_TX              gpioPortB
#define PTD_PIN_EXTENSION_USART2_TX               3
#define PTD_MODE_EXTENSION_USART2_TX              gpioModePushPull
#define PTD_DOUT_EXTENSION_USART2_TX              PTD_GPIO_HIGH

/** UART RX pin configuration for Extension Bus */
#define PTD_PORT_EXTENSION_USART2_RX              gpioPortB
#define PTD_PIN_EXTENSION_USART2_RX               4
#define PTD_MODE_EXTENSION_USART2_RX              gpioModeInput
#define PTD_DOUT_EXTENSION_USART2_RX              PTD_GPIO_INPUT_FILTER_OFF


//GPIO config modified by lihao 20160609
static void LiftPtdConfig(void)
{

    /* configure the cat1 RX pin as input */
    PTD_driveModeSet(PTD_PORT_EXTENSION_USART2_RX, PTD_MODE_EXTENSION_USART2_RX);

    /* configure the cat1 RX pin as input */
    PTD_pinModeSet(PTD_PORT_EXTENSION_USART2_RX, PTD_PIN_EXTENSION_USART2_RX,
        (GPIO_Mode_TypeDef) PTD_MODE_EXTENSION_USART2_RX, PTD_GPIO_INPUT_FILTER_OFF);

    /* configure the cat1 TX pin as output */
    PTD_driveModeSet(PTD_PORT_EXTENSION_USART2_TX, PTD_MODE_EXTENSION_USART2_TX);

    /* configure the cat1 TX pin as output */
    PTD_pinModeSet(PTD_PORT_EXTENSION_USART2_TX, PTD_PIN_EXTENSION_USART2_TX,
        (GPIO_Mode_TypeDef) PTD_MODE_EXTENSION_USART2_TX, PTD_DOUT_EXTENSION_USART2_TX);

}

/*
 * @brief enable the clock and the serial initialize
 *
 *
 * @return the State of value . > 0 is error
 */
 

Lift_return_t Lift_DriverInit(void)
{
	SER_errorCode_t ret;

	/* Enable the USART clock */
	CMU_ClockEnable(cmuClock_USART2, true);

	LiftPtdConfig();

	/* Create Live Data Stream Timer */
	ret = SER_serialInit((SER_device_t*) &Lift_uartHandle,(SER_init_t*) &Lift_uartInit);
	if ( ret == SER_SUCCESS) {
		return (Lift_STATUS_SUCCESS);
	} else {
		return (Lift_STATUS_FAILED);
	}
}


/**/
static Lift_return_t serial_data_byte_pop(uint8_t *recvData)
{
    uint8_t receiveBuffer;
    SER_errorCode_t retVal = SER_SUCCESS;
    uint32_t reminingBytes;
    Lift_return_t status;

    /* Reads the required number of bytes from internal buffer */
    retVal = SER_serialRead(&Lift_uartHandle, &reminingBytes, &receiveBuffer, 1);

    if (retVal == SER_SUCCESS) {
		*recvData = receiveBuffer;
		//DEBUG("LIFT---recvdata is %s\n",recvData);
        return (Lift_STATUS_SUCCESS);
    } else {
        return (Lift_STATUS_FAILED);
    }
}



/*
 * @brief send the data to serial
 *
 *@param[in] trasmiBuffer - send to the serial data
 *@param[in] transmitLength - send the data length
 * @return the State of value . > 0 is error
 */

static Lift_return_t send_data_to_device(uint8_t* transmitBuffer,uint16_t transmitLength)
{
	Lift_return_t ret;

    DBG_ASSERT(transmitBuffer != NULL,"NULL pointer had been passed to the function instead of valid dataToSend.");
    DBG_ASSERT(transmitLength != UINT8_C(0), "data send length Cannot be Zero.");

    /* The data has been delivered to the remote device*/
	ret = SER_serialWrite(&Lift_uartHandle, NULL, transmitBuffer, transmitLength);
    if ( ret == SER_SUCCESS) {
        return (Lift_STATUS_SUCCESS);
    } else {
        return (Lift_STATUS_FAILED);
    }
}


static Lift_return_t receive_data_from_device(uint8_t *recvBufferData)
{
	int count = 0;
	uint8_t recvData;
	uint8_t uartState = UART_EMPTY;

	switch (uartState) {
	case UART_EMPTY:
	{
		if (Lift_STATUS_SUCCESS == serial_data_byte_pop (&recvData)) {
			recvBufferData[count++] = recvData;
			uartState = UART_FULL;
		} else {
			return Lift_STATUS_FAILED;
		}
	}

	case UART_FULL:
	{
		while (1) {
			if (Lift_STATUS_SUCCESS == serial_data_byte_pop (&recvData)) {
				recvBufferData[count++] = recvData;
			} else {
				vTaskDelay(2);
				if (Lift_STATUS_SUCCESS == serial_data_byte_pop (&recvData)) {
					recvBufferData[count++] = recvData;

				} else {
					return Lift_STATUS_SUCCESS;
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

Lift_return_t send_AT_cmd_lift(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs)
{
	Lift_return_t ret;
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

		if (ret == Lift_STATUS_SUCCESS) {
			return ret;
		} else {
			continue;
		}
	}
	return Lift_STATUS_TIMEOUT;
}




Lift_return_t send_recv_cmd_lift(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs)
{
	Lift_return_t ret;
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
	}
	return Lift_STATUS_TIMEOUT;
}



Lift_return_t send_send_cmd_lift(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs)
{
	Lift_return_t ret;
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

		if (ret == Lift_STATUS_SUCCESS) {
			str = strstr(cmdRecv, "AT^SISH=0");
			if(NULL != str)
				return ret;
			else
				continue;
		} else {
			continue;
		}
	}
	return Lift_STATUS_TIMEOUT;
}


Lift_return_t send_recv_lift(uint8_t *cmdRecv)
{
	Lift_return_t ret;
	char *str = NULL;

	ret = receive_data_from_device(cmdRecv);
	return ret;
}





