//*****************************************************************************
//
// usb_dev_serial.c - Main routines for the USB CDC serial example.
//
//*****************************************************************************

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_uart.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include "driverlib/usb.h"
#include "driverlib/rom.h"
#include "usblib/usblib.h"
#include "usblib/usbcdc.h"
#include "usblib/usb-ids.h"
#include "usblib/device/usbdevice.h"
#include "usblib/device/usbdcdc.h"
//#include "utils/ustdlib.h"
#include "usb_serial_structs.h"

// Variables tracking transmit and receive counts.
//
//*****************************************************************************
volatile unsigned long g_ulUARTTxCount = 0;
volatile unsigned long g_ulUARTRxCount = 0;
#ifdef DEBUG
unsigned long g_ulUARTRxErrors = 0;
#endif

static void* pDeviceUSB;


// Global system tick counter
//
//*****************************************************************************
volatile unsigned long g_ulSysTickCount = 0;

// Flags used to pass commands from interrupt context to the main loop.
//
//*****************************************************************************
#define COMMAND_PACKET_RECEIVED 0x00000001
#define COMMAND_STATUS_UPDATE   0x00000002

volatile unsigned long g_ulFlags = 0;
char *g_pcStatus;

//*****************************************************************************
//
// Global flag indicating that a USB configuration has been set.
//
//*****************************************************************************
static volatile tBoolean g_bUSBConfigured = false;


//*****************************************************************************
//
// Set the state of the RS232 RTS and DTR signals.
//
//*****************************************************************************
//
// Handles CDC driver notifications related to control and setup of the device.
//
// \param pvCBData is the client-supplied callback pointer for this channel.
// \param ulEvent identifies the event we are being notified about.
// \param ulMsgValue is an event-specific value.
// \param pvMsgData is an event-specific pointer.
//
// This function is called by the CDC driver to perform control-related
// operations on behalf of the USB host.  These functions include setting
// and querying the serial communication parameters, setting handshake line
// states and sending break conditions.
//
// \return The return value is event-specific.
//
//*****************************************************************************
unsigned long
ControlHandler(void *pvCBData, unsigned long ulEvent,
               unsigned long ulMsgValue, void *pvMsgData)
{
    unsigned long ulIntsOff;

    //
    // Which event are we being asked to process?
    //
    switch(ulEvent)
    {
        //
        // We are connected to a host and communication is now possible.
        //
        case USB_EVENT_CONNECTED:
            g_bUSBConfigured = true;

            //
            // Flush our buffers.
            //
            USBBufferFlush(&g_sTxBuffer);
            USBBufferFlush(&g_sRxBuffer);

            //
            // Tell the main loop to update the display.
            //
            ulIntsOff = ROM_IntMasterDisable();
            g_pcStatus = "Host connected.";
            g_ulFlags |= COMMAND_STATUS_UPDATE;
            if(!ulIntsOff)
            {
                ROM_IntMasterEnable();
            }
            break;

        //
        // The host has disconnected.
        //
        case USB_EVENT_DISCONNECTED:
            g_bUSBConfigured = false;
            ulIntsOff = ROM_IntMasterDisable();
            g_pcStatus = "Host disconnected.";
            g_ulFlags |= COMMAND_STATUS_UPDATE;
            if(!ulIntsOff)
            {
                ROM_IntMasterEnable();
            }
            break;

        //
        // Return the current serial communication parameters.
        //
        case USBD_CDC_EVENT_GET_LINE_CODING:
            //GetLineCoding(pvMsgData);
            break;

        //
        // Set the current serial communication parameters.
        //
        case USBD_CDC_EVENT_SET_LINE_CODING:
            //SetLineCoding(pvMsgData);
            break;

        //
        // Set the current serial communication parameters.
        //
        case USBD_CDC_EVENT_SET_CONTROL_LINE_STATE:
            //SetControlLineState((unsigned short)ulMsgValue);
            break;

        //
        // Send a break condition on the serial line.
        //
        case USBD_CDC_EVENT_SEND_BREAK:
            //SendBreak(true);
            break;

        //
        // Clear the break condition on the serial line.
        //
        case USBD_CDC_EVENT_CLEAR_BREAK:
            //SendBreak(false);
            break;

        //
        // Ignore SUSPEND and RESUME for now.
        //
        case USB_EVENT_SUSPEND:
        case USB_EVENT_RESUME:
            break;

        //
        // We don't expect to receive any other events.  Ignore any that show
        // up in a release build or hang in a debug build.
        //
        default:
            break;


    }

    return(0);
}

//*****************************************************************************
//
// Handles CDC driver notifications related to the transmit channel (data to
// the USB host).
//
// \param ulCBData is the client-supplied callback pointer for this channel.
// \param ulEvent identifies the event we are being notified about.
// \param ulMsgValue is an event-specific value.
// \param pvMsgData is an event-specific pointer.
//
// This function is called by the CDC driver to notify us of any events
// related to operation of the transmit data channel (the IN channel carrying
// data to the USB host).
//
// \return The return value is event-specific.
//
//*****************************************************************************
unsigned long
TxHandler(void *pvCBData, unsigned long ulEvent, unsigned long ulMsgValue,
          void *pvMsgData)
{
    //
    // Which event have we been sent?
    //
    switch(ulEvent)
    {
        case USB_EVENT_TX_COMPLETE:
            //
            // Since we are using the USBBuffer, we don't need to do anything
            // here.
            //
            break;

        //
        // We don't expect to receive any other events.  Ignore any that show
        // up in a release build or hang in a debug build.
        //
        default:

            break;
    }
    return(0);
}

//*****************************************************************************
//
// Handles CDC driver notifications related to the receive channel (data from
// the USB host).
//
// \param ulCBData is the client-supplied callback data value for this channel.
// \param ulEvent identifies the event we are being notified about.
// \param ulMsgValue is an event-specific value.
// \param pvMsgData is an event-specific pointer.
//
// This function is called by the CDC driver to notify us of any events
// related to operation of the receive data channel (the OUT channel carrying
// data from the USB host).
//
// \return The return value is event-specific.
//
//*****************************************************************************
unsigned long
RxHandler(void *pvCBData, unsigned long ulEvent, unsigned long ulMsgValue,
          void *pvMsgData)
{
    unsigned long ulCount;
    unsigned char ucChar;
    int ulRead;
    //
    // Which event are we being sent?
    //
    switch(ulEvent)
    {
        //
        // A new packet has been received.
        //
        case USB_EVENT_RX_AVAILABLE:
        {
            //
            // Feed some characters into the UART TX FIFO and enable the
            // interrupt so we are told when there is more space.
            //
           ulRead = USBBufferRead((tUSBBuffer *)&g_sRxBuffer, &ucChar, 1);
           //
           if (ulRead)
           {
              //we get the charecter
           }
            break;
        }

        //
        // We are being asked how much unprocessed data we have still to
        // process. We return 0 if the UART is currently idle or 1 if it is
        // in the process of transmitting something. The actual number of
        // bytes in the UART FIFO is not important here, merely whether or
        // not everything previously sent to us has been transmitted.
        //
        case USB_EVENT_DATA_REMAINING:
        {
            //
            // Get the number of bytes in the buffer and add 1 if some data
            // still has to clear the transmitter.
            //
            //ulCount = UARTBusy(USB_UART_BASE) ? 1 : 0;
          ulCount=0;
            return(ulCount);
        }

        //
        // We are being asked to provide a buffer into which the next packet
        // can be read. We do not support this mode of receiving data so let
        // the driver know by returning 0. The CDC driver should not be sending
        // this message but this is included just for illustration and
        // completeness.
        //
        case USB_EVENT_REQUEST_BUFFER:
        {
            return(0);
        }

        //
        // We don't expect to receive any other events.  Ignore any that show
        // up in a release build or hang in a debug build.
        //
        default:

            break;
    }

    return(0);
}

//*****************************************************************************
//
// This is the main application entry function.
//
//*****************************************************************************
int
Init_USB(void)
{
   
    //
    g_bUSBConfigured = false;

    //
    // Initialize the transmit and receive buffers.
    //
    USBBufferInit((tUSBBuffer *)&g_sTxBuffer);
    USBBufferInit((tUSBBuffer *)&g_sRxBuffer);

    //???
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
     SysCtlPeripheralEnable(SYSCTL_PERIPH_USB0);
    SysCtlUSBPLLEnable();
    //
    //SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);            // ID+VBUS
    //GPIOPinTypeUSBAnalog(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1); // ID+VBUS
    // Set the USB stack mode to Device mode with VBUS monitoring.
     // Configure USB pins, according to 
    // http://forum.stellarisiti.com/topic/353-work-around-stellaris-launchpad-usb-serial-example-not-enumerating-correctly/?p=1544
    // Enable the GPIO port so we can configure it
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);   
    // Setup the port to be in USB analog mode. Routes incoming signals to 
    // on-chip USB PHY
    GPIOPinTypeUSBAnalog(GPIO_PORTD_BASE, GPIO_PIN_4 | GPIO_PIN_5); 

     IntPrioritySet(INT_USB0, 0);
     USBIntEnable(USB0_BASE,USB_INT_ALL); 
      IntEnable(INT_USB0);
    //
    USBStackModeSet(0, USB_MODE_DEVICE, 0);

    //
    // Pass our device information to the USB library and place the device
    // on the bus.
    //
    pDeviceUSB = USBDCDCInit(0, (tUSBDCDCDevice *)&g_sCDCDevice);
    ////
    return (0);
    
}
