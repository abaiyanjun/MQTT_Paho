/******************************************************************************/
/**
 * This software is copyrighted by Bosch Connected Devices and Solutions GmbH, 2015.
 * The use of this software is subject to the XDK SDK EULA
 */
/**
 *
 *  @file        CAT1_UARTDEV.H
 *
 * Demo application of printing BME280 Environmental data on serialport(USB virtual comport)
 * every one second, initiated by Environmental timer(freertos)
 *
 * ****************************************************************************/

/* header definition ******************************************************** */
#ifndef _CAT1_UARTDEV_H_
#define _CAT1_UARTDEV_H_


#define PED_THREESECONDDELAY  UINT32_C(3000)	 	/** one second is represented by this macro */
#define PED_ONESECONDDELAY  UINT32_C(1000)
#define PED_TIMERBLOCKTIME  UINT32_C(0xffff) 	/** Macro used to define blocktime of a timer*/
#define PED_USEI2CPROTOCOL  UINT8_C(0)	 		/** I2c protocol is represented by this macro*/
#define PED_VALUE_ZERO 		UINT8_C(0)	 		/** default value*/
#define PED_I2C_ADDRESS     UINT8_C(0x18) 	    /** i2c address for BME280 */
#define UART_ENABLE_FLAG                     UINT8_C(1)                       /**< macro to represent the "enable" */
#define UART_DISABLE_FLAG                    UINT8_C(0)                       /**< macro to represent the "Disable" */
#define UART_NUMBER_ZERO                     UINT8_C(0)
#define UNUSED_PARAMETER(P)                                     (P = P)

#define CAT1_TIMEOUT					 UINT32_C(10000)
#define CAT1_DELAY_1000MS				UINT32_C(1000)
#define Cat1_TX_BUFFER_SIZE           	(UINT32_C(1492))
#define Cat1_RX_BUFFER_SIZE           	(UINT32_C(1492))


 typedef enum Cat1_return_e {
	Cat1_STATUS_SUCCESS =  UINT8_C(0),
	Cat1_STATUS_FAILED =  UINT8_C(1),
	Cat1_STATUS_INVALID_PARMS =  UINT8_C(2),
	Cat1_STATUS_TIMEOUT =  UINT8_C(3)

} Cat1_return_t;
/* local function prototype declarations */

extern Cat1_return_t Cat1_DriverInit(void);
extern Cat1_return_t send_AT_cmd(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs);

extern Cat1_return_t send_send_cmd(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs);
extern Cat1_return_t send_recv_cmd(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs);





/**@}*/
//#endif /* XDK110_PED_PRINTENVDATA_IH_H_ */  uart debug by lihao 20160609

/** ************************************************************************* */
#endif

/* public global variable declarations */

/* inline function definitions */

/**@}*/
//#endif /* XDK110_PED_PRINTENVDATA_IH_H_ */  uart debug by lihao 20160609

/** ************************************************************************* */

