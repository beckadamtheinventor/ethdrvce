#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usbdrvce.h>
#include <ti/getcsc.h>
#include <ti/screen.h>
#include "ethdrvce.h"
    
eth_device_t eth;
bool eth_init = false;

void tx_callback(eth_error_t error, eth_transfer_t *xfer) {
    if(error) printf("tx error: %u\n", error);
    else printf("tx len: %u\n", xfer->len);
}

void rx_callback(eth_error_t error, eth_transfer_t *xfer) {
    if(error) printf("rx error: %u\n", error);
    else printf("rx len: %u\n", xfer->len);
}
    
usb_error_t eth_event_callback(usb_event_t event, void *event_data, void *callback_data){
    usb_device_t usb_device = event_data;
    /* Enable newly connected devices */
    switch (event)
    {
        case USB_DEVICE_CONNECTED_EVENT:
            if (!(usb_GetRole() & USB_ROLE_DEVICE))
                usb_ResetDevice(usb_device);
            break;
        case USB_DEVICE_ENABLED_EVENT:
            if(eth_USBHandleHubs(usb_device))
                break;
            if(!eth_init)
                eth_init = (bool)(eth_Open(&eth, usb_device, rx_callback)==ETH_OK);
            break;
        case USB_DEVICE_DISCONNECTED_EVENT:
        case USB_DEVICE_DISABLED_EVENT:
            eth_Close(&eth);
            break;
        case USB_HOST_PORT_CONNECT_STATUS_CHANGE_INTERRUPT:
            return USB_ERROR_NO_DEVICE;
            break;
        default:
            break;
    }
    return USB_SUCCESS;
}
    
    
    
/* Main function, called first */
int main(void)
{
    /* Clear the homescreen */
    os_ClrHome();
    char *sendeth = "This is a test";
    if(usb_Init(eth_event_callback, NULL, NULL, USB_DEFAULT_INIT_FLAGS))
        printf("usb init error");
    
    sk_key_t key = 0;
    
    do {
        key = os_GetCSC();
        if(key == sk_2nd) {
            eth_transfer_t xfer;
            xfer.buffer = sendeth;
            xfer.len = strlen(sendeth);
            xfer.callback = tx_callback;
            eth_Write(&eth, &xfer);
        }
        usb_HandleEvents();
    } while(key != sk_Clear);
        
    /* Return 0 for success */
    eth_Close(&eth);
    usb_Cleanup();
    return 0;
    
}
