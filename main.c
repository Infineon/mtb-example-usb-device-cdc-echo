/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the USB Device CDC echo Example
*              for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* $ Copyright YEAR Cypress Semiconductor $
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "USB.h"
#include "USB_CDC.h"
#include <stdio.h>


/*******************************************************************************
* Macros
********************************************************************************/
#define USB_CONFIG_DELAY          (50U) /* In milliseconds */

/*******************************************************************************
* Function Prototypes
********************************************************************************/
void usb_add_cdc(void);

/*********************************************************************
*       Information that are used during enumeration
**********************************************************************/
static const USB_DEVICE_INFO usb_deviceInfo = {
    0x058B,                       /* VendorId    */
    0x027D,                       /* ProductId    */
    "Infineon Technologies",      /* VendorName   */
    "CDC Code Example",           /* ProductName  */
    "12345678"                    /* SerialNumber */
};


/*******************************************************************************
* Global Variables
********************************************************************************/
static USB_CDC_HANDLE usb_cdcHandle;
static char           read_buffer[USB_FS_BULK_MAX_PACKET_SIZE];
static char           write_buffer[USB_FS_BULK_MAX_PACKET_SIZE];


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM4 CPU.
*
*  1. It initializes the USB Device block
*  and enumerates as a CDC device.
*
*  2. It recevies data from host
*  and echos it back
*
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    int num_bytes_received;
    int num_bytes_to_write = 0;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;

    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* Initialize the User LED */
    cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("****************** "
           "emUSB Device: CDC echo application "
           "****************** \r\n\n");

    /* Initializes the USB stack */
    USBD_Init();

    /* Endpoint Initialization for CDC class */
    usb_add_cdc();

    /* Set device info used in enumeration */
    USBD_SetDeviceInfo(&usb_deviceInfo);

    /* Start the USB stack */
    USBD_Start();

    /* Turning the LED on to indicate device is active */
    cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);

    for (;;)
    {

        /* Wait for configuration */
        while ( (USBD_GetState() & USB_STAT_CONFIGURED) != USB_STAT_CONFIGURED)
        {
            cyhal_system_delay_ms(USB_CONFIG_DELAY);
        }

        num_bytes_received = USBD_CDC_Receive(usb_cdcHandle, read_buffer, sizeof(read_buffer), 0);

        memcpy(write_buffer + num_bytes_to_write, read_buffer, num_bytes_received);

        num_bytes_to_write +=  num_bytes_received;


        if ( ( num_bytes_to_write > 0 ) && ( read_buffer[num_bytes_received - 1] == '\n' ) )
        {

            /* Sending one packet to host */
            USBD_CDC_Write(usb_cdcHandle, write_buffer, num_bytes_to_write, 0);

            /* Waits for specified number of bytes to be written to host */
            USBD_CDC_WaitForTX(usb_cdcHandle, 0);

            /* If the last sent packet is exactly the maximum packet
            *  size, it is followed by a zero-length packet to assure
            *  that the end of the segment is properly identified by
            *  the terminal.
            */

            if(num_bytes_to_write == sizeof(write_buffer))
            {
                /* Sending zero-length packet to host */
                USBD_CDC_Write(usb_cdcHandle, NULL, 0, 0);

                /* Waits for specified number of bytes to be written to host */
                USBD_CDC_WaitForTX(usb_cdcHandle, 0);
            }

            num_bytes_to_write = 0;

        }

        cyhal_syspm_sleep();

    }

}

/*********************************************************************
* Function Name: USBD_CDC_Echo_Init
**********************************************************************
* Summary:
*  Add communication device class to USB stack
*
* Parameters:
*  void
*
* Return:
*  void
**********************************************************************/

void usb_add_cdc(void) {

    static U8             OutBuffer[USB_FS_BULK_MAX_PACKET_SIZE];
    USB_CDC_INIT_DATA     InitData;
    USB_ADD_EP_INFO       EPBulkIn;
    USB_ADD_EP_INFO       EPBulkOut;
    USB_ADD_EP_INFO       EPIntIn;

    memset(&InitData, 0, sizeof(InitData));
    EPBulkIn.Flags          = 0;                             /* Flags not used */
    EPBulkIn.InDir          = USB_DIR_IN;                    /* IN direction (Device to Host) */
    EPBulkIn.Interval       = 0;                             /* Interval not used for Bulk endpoints */
    EPBulkIn.MaxPacketSize  = USB_FS_BULK_MAX_PACKET_SIZE;   /* Maximum packet size (64B for Bulk in full-speed) */
    EPBulkIn.TransferType   = USB_TRANSFER_TYPE_BULK;        /* Endpoint type - Bulk */
    InitData.EPIn  = USBD_AddEPEx(&EPBulkIn, NULL, 0);

    EPBulkOut.Flags         = 0;                             /* Flags not used */
    EPBulkOut.InDir         = USB_DIR_OUT;                   /* OUT direction (Host to Device) */
    EPBulkOut.Interval      = 0;                             /* Interval not used for Bulk endpoints */
    EPBulkOut.MaxPacketSize = USB_FS_BULK_MAX_PACKET_SIZE;   /* Maximum packet size (64B for Bulk in full-speed) */
    EPBulkOut.TransferType  = USB_TRANSFER_TYPE_BULK;        /* Endpoint type - Bulk */
    InitData.EPOut = USBD_AddEPEx(&EPBulkOut, OutBuffer, sizeof(OutBuffer));

    EPIntIn.Flags           = 0;                             /* Flags not used */
    EPIntIn.InDir           = USB_DIR_IN;                    /* IN direction (Device to Host) */
    EPIntIn.Interval        = 64;                            /* Interval of 8 ms (64 * 125us) */
    EPIntIn.MaxPacketSize   = USB_FS_INT_MAX_PACKET_SIZE ;   /* Maximum packet size (64 for Interrupt) */
    EPIntIn.TransferType    = USB_TRANSFER_TYPE_INT;         /* Endpoint type - Interrupt */
    InitData.EPInt = USBD_AddEPEx(&EPIntIn, NULL, 0);

    usb_cdcHandle = USBD_CDC_Add(&InitData);
}

/* [] END OF FILE */
