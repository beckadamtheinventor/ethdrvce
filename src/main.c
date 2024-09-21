#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usbdrvce.h>
#include <ti/getcsc.h>
#include <ti/screen.h>
#include "ethdrvce.h"
    
eth_device_t eth;
bool eth_init = false;
    
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
            if(eth_UsbCheckConfigureHubs(usb_device))
                break;
            if(!eth_init)
                eth_init = (bool)(eth_Open(&eth, usb_device)==ETH_OK);
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
    eth_SetAllocator(malloc, free);
    struct eth_packet *packet;
    char sendeth = "This is a test";
    if(usb_Init(eth_event_callback, NULL, NULL, USB_DEFAULT_INIT_FLAGS))
        printf("usb init error");
    
    sk_key_t key = 0;
    
    do {
        key = os_GetCSC();
        if(key == sk_2nd) eth_Write(&eth, sendeth, strlen(sendeth));
        eth_Read(&eth, &packet);
        if(packet) {
            os_ClrHome();
            printf("packet len %u\n", packet->length);
            free(packet);
        }
        usb_HandleEvents();
    } while(key != sk_Clear);
        
    /* Return 0 for success */
    eth_Close(&eth);
    usb_Cleanup();
    return 0;
    
}
