/*
 *
 *
 *  Created on: 2025Äê3ÔÂ12ÈÕ
 *      Author: DELL
 */

#ifndef MCCI_SCD_FREERTOS_H_
#define MCCI_SCD_FREERTOS_H_

/****************************************************************************\
|
|   Manifest constants & typedefs.
|
\****************************************************************************/

typedef struct MCCI_USB_SCD_CONTEXT_s
    {
    TaskHandle_t                hTask;
    QueueHandle_t               hQueue;
    USBPUMP_AM64X_HOST_SCD_DEVICE_INFO  DeviceInfo;
    unsigned char               ReadBuffer[512];
    unsigned char               WriteBuffer[256];
    } MCCI_USB_SCD_CONTEXT;

typedef struct MCCI_USB_SCD_MESSAGE_s
    {
    USBPUMP_AM64X_HOST_SCD_EVENT        Event;
    USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE    hDevice;
    } MCCI_USB_SCD_MESSAGE;

extern MCCI_USB_SCD_CONTEXT gUsbScdContext;
void mcci_scd_freertos(void *args);
extern void mcciAm64x_HostClassScdAcmInit(void);

static
void
McciUsb_ScdAcmEvent(
    void *                  pContext,
    USBPUMP_AM64X_HOST_SCD_EVENT        Event,
    USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE    hDevice
    );

static
void
McciUsb_ScdCheckEvent(
    MCCI_USB_SCD_CONTEXT *  pUsbScdContext
    );

static
void
McciUsb_ScdEventAttached(
    MCCI_USB_SCD_CONTEXT *          pUsbScdContext,
    USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE    hDevice
    );

static
void
McciUsb_ScdEventDetached(
    MCCI_USB_SCD_CONTEXT *          pUsbScdContext,
    USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE    hDevice
    );

void mcci_scd_freertos_init(
    void
    );

int
McciUsb_ScdSendAtCommand(
//    MCCI_USB_SCD_CONTEXT *          pUsbScdContext,
    USBPUMP_AM64X_HOST_SCD_DEVICE_HANDLE    hDevice,
    const char *                pAtCommand,
    const char *                pResult
    );

#endif /* MCCI_SCD_FREERTOS_H_ */
