/*****************************************************************************
*
* © 2014 Written by Serikov Igor.
*
*****************************************************************************/
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"

#include "usblib/usblib.h"
#include "usblib/usb-ids.h"
#include "usblib/device/usbdevice.h"
#include "usblib/device/usbdbulk.h"
#include "usb_bulk_structs.h"

#include "../debug/debug.h"


volatile unsigned long g_ulTxCount = 0;
volatile unsigned long g_ulRxCount = 0;

// Global flag indicating that a USB configuration has been set.
static volatile tBoolean g_bUSBConfigured = false;

volatile unsigned long g_ulFlags = 0;
char *g_pcStatus;

// Flags used to pass commands from interrupt context to the main loop.
#define COMMAND_PACKET_RECEIVED 0x00000001
#define COMMAND_STATUS_UPDATE   0x00000002
//

//*****************************************************************************
//
// Receive new data and echo it back to the host.
//
// \param psDevice points to the instance data for the device whose data is to
// be processed.
// \param pcData points to the newly received data in the USB receive buffer.
// \param ulNumBytes is the number of bytes of data available to be processed.
//
// This function is called whenever we receive a notification that data is
// available from the host. We read the data, byte-by-byte and swap the case
// of any alphabetical characters found then write it back out to be
// transmitted back to the host.
//
// \return Returns the number of bytes of data processed.
//
//*****************************************************************************
static unsigned long
EchoNewDataToHost(tUSBDBulkDevice *psDevice, unsigned char *pcData,
                  unsigned long ulNumBytes)
{
    unsigned long ulLoop, ulSpace, ulCount;
    unsigned long ulReadIndex;
    unsigned long ulWriteIndex;
    tUSBRingBufObject sTxRing;

    //
    // Get the current buffer information to allow us to write directly to
    // the transmit buffer (we already have enough information from the
    // parameters to access the receive buffer directly).
    //
    USBBufferInfoGet(&g_sTxBuffer, &sTxRing);

    //
    // How much space is there in the transmit buffer?
    //
    ulSpace = USBBufferSpaceAvailable(&g_sTxBuffer);

    //
    // How many characters can we process this time round?
    //
    ulLoop = (ulSpace < ulNumBytes) ? ulSpace : ulNumBytes;
    ulCount = ulLoop;

    //
    // Update our receive counter.
    //
    g_ulRxCount += ulNumBytes;

    //
    // Dump a debug message.
    //
    dbg_printf("Received %d bytes\n", ulNumBytes);

    //
    // Set up to process the characters by directly accessing the USB buffers.
    //
    ulReadIndex = (unsigned long)(pcData - g_pucUSBRxBuffer);
    ulWriteIndex = sTxRing.ulWriteIndex;

    while(ulLoop)
    {
        //
        // Copy from the receive buffer to the transmit buffer converting
        // character case on the way.
        //

        //
        // Is this a lower case character?
        //
        if((g_pucUSBRxBuffer[ulReadIndex] >= 'a') &&
           (g_pucUSBRxBuffer[ulReadIndex] <= 'z'))
        {
            //
            // Convert to upper case and write to the transmit buffer.
            //
            g_pucUSBTxBuffer[ulWriteIndex] =
                (g_pucUSBRxBuffer[ulReadIndex] - 'a') + 'A';
        }
        else
        {
            //
            // Is this an upper case character?
            //
            if((g_pucUSBRxBuffer[ulReadIndex] >= 'A') &&
               (g_pucUSBRxBuffer[ulReadIndex] <= 'Z'))
            {
                //
                // Convert to lower case and write to the transmit buffer.
                //
                g_pucUSBTxBuffer[ulWriteIndex] =
                    (g_pucUSBRxBuffer[ulReadIndex] - 'Z') + 'z';
            }
            else
            {
                //
                // Copy the received character to the transmit buffer.
                //
                g_pucUSBTxBuffer[ulWriteIndex] = g_pucUSBRxBuffer[ulReadIndex];
            }
        }

        //
        // Move to the next character taking care to adjust the pointer for
        // the buffer wrap if necessary.
        //
        ulWriteIndex++;
        ulWriteIndex = (ulWriteIndex == BULK_BUFFER_SIZE) ? 0 : ulWriteIndex;

        ulReadIndex++;
        ulReadIndex = (ulReadIndex == BULK_BUFFER_SIZE) ? 0 : ulReadIndex;

        ulLoop--;
    }

    //
    // We've processed the data in place so now send the processed data
    // back to the host.
    //
    USBBufferDataWritten(&g_sTxBuffer, ulCount);

    dbg_printf("Wrote %d bytes\n", ulCount);

    //
    // We processed as much data as we can directly from the receive buffer so
    // we need to return the number of bytes to allow the lower layer to
    // update its read pointer appropriately.
    //
    return(ulCount);
}

//*****************************************************************************
//*****************************************************************************
// Handles bulk driver notifications related to the transmit channel (data to
// the USB host).
//
// \param pvCBData is the client-supplied callback pointer for this channel.
// \param ulEvent identifies the event we are being notified about.
// \param ulMsgValue is an event-specific value.
// \param pvMsgData is an event-specific pointer.
//
// This function is called by the bulk driver to notify us of any events
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
    // We are not required to do anything in response to any transmit event
    // in this example. All we do is update our transmit counter.
    //
    if(ulEvent == USB_EVENT_TX_COMPLETE)
    {
        g_ulTxCount += ulMsgValue;
    }

    //
    // Dump a debug message.
    //
    dbg_printf("TX complete %d\n", ulMsgValue);

    return(0);
}
//*****************************************************************************
//
// Handles bulk driver notifications related to the receive channel (data from
// the USB host).
//
// \param pvCBData is the client-supplied callback pointer for this channel.
// \param ulEvent identifies the event we are being notified about.
// \param ulMsgValue is an event-specific value.
// \param pvMsgData is an event-specific pointer.
//
// This function is called by the bulk driver to notify us of any events
// related to operation of the receive data channel (the OUT channel carrying
// data from the USB host).
//
// \return The return value is event-specific.
//
//*****************************************************************************
unsigned long
RxHandler(void *pvCBData, unsigned long ulEvent,
               unsigned long ulMsgValue, void *pvMsgData)
{
    //
    // Which event are we being sent?
    //
    switch(ulEvent)
    {
        //
        // We are connected to a host and communication is now possible.
        //
        case USB_EVENT_CONNECTED:
        {
            g_bUSBConfigured = true;
            g_pcStatus = "Host connected.";
            g_ulFlags |= COMMAND_STATUS_UPDATE;

            //
            // Flush our buffers.
            //
            USBBufferFlush(&g_sTxBuffer);
            USBBufferFlush(&g_sRxBuffer);

            break;
        }

        //
        // The host has disconnected.
        //
        case USB_EVENT_DISCONNECTED:
        {
            g_bUSBConfigured = false;
            g_pcStatus = "Host disconnected.";
            g_ulFlags |= COMMAND_STATUS_UPDATE;
            break;
        }

        //
        // A new packet has been received.
        //
        case USB_EVENT_RX_AVAILABLE:
        {
            tUSBDBulkDevice *psDevice;

            //
            // Get a pointer to our instance data from the callback data
            // parameter.
            //
            psDevice = (tUSBDBulkDevice *)pvCBData;

            //
            // Read the new packet and echo it back to the host.
            //
            return(EchoNewDataToHost(psDevice, pvMsgData, ulMsgValue));
        }

        //
        // Ignore SUSPEND and RESUME for now.
        //
        case USB_EVENT_SUSPEND:
        case USB_EVENT_RESUME:
            break;

        //
        // Ignore all other events and return 0.
        //
        default:
            break;
    }

    return(0);
}

//*****************************************************************************
//
void Init_USB()
{
     
   // Initialize the transmit and receive buffers.
    //
    USBBufferInit((tUSBBuffer *)&g_sTxBuffer);
    USBBufferInit((tUSBBuffer *)&g_sRxBuffer);

  // Set the USB stack mode to Device mode with VBUS monitoring.
    //
    USBStackModeSet(0, USB_MODE_DEVICE, 0);

    //
    // Pass our device information to the USB library and place the device
    // on the bus.
    //
    USBDBulkInit(0, (tUSBDBulkDevice *)&g_sBulkDevice);

    //

    //
    // Clear our local byte counters.
  
}