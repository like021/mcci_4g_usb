/*
 *  Copyright (C) 2021 Texas Instruments Incorporated
 *  Copyright (C) 2021 MCCI Corporation
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

#include "FreeRTOS.h"
#include "queue.h"

#include "usbpump_freertos_am64x_api.h"
#include "usbpump_am64x_host_api.h"
#include "usbpump_am64x_host_scd_api.h"

#include "uplatformapi.h"
#include "usbpump_allocation.h"
#include "usbpumpdebug.h"
#include "mcci_scd_freertos.h"



MCCI_USB_SCD_CONTEXT gUsbScdContext;


void mcci_scd_freertos_init(
	void
	)
	{
	MCCI_USB_SCD_CONTEXT * const pUsbScdContext = &gUsbScdContext;

	/* initialize class drivers */
	UsbPumpAm64x_HostClassCompositeInit();
	mcciAm64x_HostClassScdAcmInit();

	pUsbScdContext->hQueue =
		xQueueCreate(10, sizeof(MCCI_USB_SCD_MESSAGE));
	if (pUsbScdContext->hQueue == NULL)
		{
		DebugP_log("Can't create event queue\n");
		return;
		}

	if (xTaskCreate(
		mcci_scd_freertos,
		"UsbScdTask",
		1024,	/* stack size */
		pUsbScdContext,	/* parameter */
		configMAX_PRIORITIES-16,
		&pUsbScdContext->hTask
		) != pdPASS)
		{
		DebugP_log("USB scd task creation failed!\n");
		}
	}

void mcci_scd_freertos(
	void *	args
	)
	{
	MCCI_USB_SCD_CONTEXT * const pUsbScdContext = args;

	if (UsbPumpAm64xHostScd_Init(
		gk_UsbPumpHostScdClassName_CdcAcm,
		2,	/* NumDevices */
		USBPUMP_UDMASK_ANY |
		USBPUMP_UDMASK_ERRORS |
		0,	/* DebugFlags */
		McciUsb_ScdAcmEvent,
		pUsbScdContext
		) != USBPUMP_AM64X_HOST_SCD_STAT_OK)
		{
		DebugP_log("UsbPumpAm64xHostScd_Init() failed\n");
		}

	while (pUsbScdContext->hQueue != NULL)
		{
		McciUsb_ScdCheckEvent(pUsbScdContext);
		}
	}

/* This function called from DataPump task */
#define FUNCTION "McciUsb_ScdAcmEvent"

static
void
McciUsb_ScdAcmEvent(
	void *					pContext,
	USBPUMP_AM64X_HOST_SCD_EVENT		Event,
	USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE	hDevice
	)
	{
	MCCI_USB_SCD_CONTEXT * const	pUsbScdContext = pContext;

	switch (Event)
		{
	case USBPUMP_AM64X_HOST_SCD_EVENT_DEVICE_ATTACHED:
		if (hDevice == USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE_INVALID)
			return;
		break;

	case USBPUMP_AM64X_HOST_SCD_EVENT_DEVICE_DETACHED:
		if (hDevice == USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE_INVALID)
			return;
		break;

	default:
		return;
		}

	if (pUsbScdContext->hQueue != NULL)
		{
		MCCI_USB_SCD_MESSAGE	Message;

		Message.Event = Event;
		Message.hDevice = hDevice;

		xQueueSend(pUsbScdContext->hQueue, &Message, 0);
		}
	}
#undef  FUNCTION

static
void
McciUsb_ScdCheckEvent(
	MCCI_USB_SCD_CONTEXT *	pUsbScdContext
	)
	{
	MCCI_USB_SCD_MESSAGE	Message;

	if (xQueueReceive(
		pUsbScdContext->hQueue,
		&Message,
		portMAX_DELAY
		) != pdPASS)
		{
		return;
		}

	if (Message.Event == USBPUMP_AM64X_HOST_SCD_EVENT_DEVICE_ATTACHED)
		{
		McciUsb_ScdEventAttached(pUsbScdContext, Message.hDevice);
		return;
		}
	if (Message.Event == USBPUMP_AM64X_HOST_SCD_EVENT_DEVICE_DETACHED)
		{
		McciUsb_ScdEventDetached(pUsbScdContext, Message.hDevice);
		return;
		}
	}

int
McciUsb_ScdSendAtCommand(
//	MCCI_USB_SCD_CONTEXT *			pUsbScdContext,
	USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE	hDevice,
	const char *				pAtCommand,
	const char *				pResult
	)
	{
	USBPUMP_AM64X_HOST_SCD_STAT	Status;
	unsigned int			i;
	unsigned int			ReadBytes;
	unsigned int			Result;
	char *				pReadBuffer;

	for (i = 0; pAtCommand[i] != '\0'; ++i)
		{
	    gUsbScdContext.WriteBuffer[i] = pAtCommand[i];
		}

	Status = UsbPumpAm64xHostScdDevice_Write(
			0,
			gUsbScdContext.WriteBuffer,
			i,
			2000
			);
	if (Status != USBPUMP_AM64X_HOST_SCD_STAT_OK)
		{
		DebugP_log("?Write() %u\n", Status);
		return 0;
		}

	Status = UsbPumpAm64xHostScdDevice_Read(
			0,
			gUsbScdContext.ReadBuffer,
			gUsbScdContext.DeviceInfo.wMaxPacketSizeIn,
			2000,
			&ReadBytes
			);
	if (Status != USBPUMP_AM64X_HOST_SCD_STAT_OK)
		{
		DebugP_log("?Read() %u\n", Status);
		return 0;
		}

	pReadBuffer = (char *) gUsbScdContext.ReadBuffer;
	DebugP_log("%s", pAtCommand);
	DebugP_log("%s\n", pReadBuffer);
	Result = 1;
//	for (i = 0; i < ReadBytes && pReadBuffer[i] != '\0'; ++i)
//		{
//		if (pResult[i] == '\0')
//			break;
//		if (pReadBuffer[i] != pResult[i])
//			Result = 0;
//		}
    if(NULL == strstr((const char *)pReadBuffer, (const char *)pResult))
    {
        Result = 0;
    }
	return Result;
	}

static
void
McciUsb_ScdEventAttached(
	MCCI_USB_SCD_CONTEXT *			pUsbScdContext,
	USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE	hDevice
	)
	{
	USBPUMP_AM64X_HOST_SCD_STAT	Status;

	DebugP_log(" McciUsb_ScdEventAttached(): hDevice=%u\n", hDevice);

	Status = UsbPumpAm64xHostScdDevice_GetDeviceInfo(
			hDevice,
			&pUsbScdContext->DeviceInfo
			);
	if (Status != USBPUMP_AM64X_HOST_SCD_STAT_OK)
		{
		DebugP_log("?GetDeviceInfo() %u\n", Status);
		return;
		}

	DebugP_log(" VID=%04x PID=%04x Interface=%u Class=%02x\n",
		pUsbScdContext->DeviceInfo.idVendor,
		pUsbScdContext->DeviceInfo.idProduct,
		pUsbScdContext->DeviceInfo.bFunction,
		pUsbScdContext->DeviceInfo.bFunctionClass
		);

	if (McciUsb_ScdSendAtCommand(
//		pUsbScdContext,
		hDevice,
		"AT\r\n",
		"OK"
		) == 0)
		{
		DebugP_log("McciUsb_ScdSendAtCommand() failed\n");
		return;
		}
	if (McciUsb_ScdSendAtCommand(
//		pUsbScdContext,
		hDevice,
		"ATE0\r\n",
		"OK"
		) == 0)
		{
		DebugP_log("McciUsb_ScdSendAtCommand() failed\n");
		return;
		}
	if (McciUsb_ScdSendAtCommand(
//		pUsbScdContext,
		hDevice,
		"AT+QCCID\r\n",
		"QCCID"
		) == 0)
		{
		DebugP_log("McciUsb_ScdSendAtCommand() failed\n");
		return;
		}
	if (McciUsb_ScdSendAtCommand(
//		pUsbScdContext,
		hDevice,
		"AT+QICSGP=1,1,\"\",\"\",\"\",0\r\n",
		"OK"
		) == 0)
		{
		DebugP_log("McciUsb_ScdSendAtCommand() failed\n");
		return;
		}
	if (McciUsb_ScdSendAtCommand(
//		pUsbScdContext,
		hDevice,
		"AT+QIDEACT=1\r\n",
		"OK"
		) == 0)
		{
		DebugP_log("McciUsb_ScdSendAtCommand() failed\n");
		return;
		}
	if (McciUsb_ScdSendAtCommand(
//		pUsbScdContext,
		hDevice,
		"AT+QIACT=1\r\n",
		"OK"
		) == 0)
		{
		DebugP_log("McciUsb_ScdSendAtCommand() failed\n");
		return;
		}
	if (McciUsb_ScdSendAtCommand(
//		pUsbScdContext,
		hDevice,
		"AT+CSQ\r\n",
		"OK"
		) == 0)
		{
		DebugP_log("McciUsb_ScdSendAtCommand() failed\n");
		return;
		}
	if (McciUsb_ScdSendAtCommand(
//		pUsbScdContext,
		hDevice,
		"AT+QPING=1,\"cloud.gomake.cn\",1,1\r\n",
		"PING: 0,\""
		) == 0)
		{
		DebugP_log("McciUsb_ScdSendAtCommand() failed\n");
		return;
		}
	}

static
void
McciUsb_ScdEventDetached(
	MCCI_USB_SCD_CONTEXT *			pUsbScdContext,
	USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE	hDevice
	)
	{
	DebugP_log(" McciUsb_ScdEventDetached(): hDevice=%u\n", hDevice);
	}
