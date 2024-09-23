#include <sys/util.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <usbdrvce.h>
#include "ethdrvce.h"

uint8_t eth_rx_buf[ETH_RX_BUFFER_SIZE];
uint8_t eth_int_buf[ETH_INTERRUPT_BUFFER_SIZE];

/// UTF-16 -> hex conversion
uint8_t
nibble(uint16_t c)
{
    c -= '0';
    if (c < 10)
        return c;
    c -= 'A' - '0';
    if (c < 6)
        return c + 10;
    c -= 'a' - 'A';
    if (c < 6)
        return c + 10;
    return 0xff;
}


usb_error_t
interrupt_receive_callback(usb_endpoint_t endpoint,
                           usb_transfer_status_t status,
                           size_t transferred,
                           __attribute__((unused)) usb_transfer_data_t *data) {
    eth_device_t* eth_device = usb_GetEndpointData(endpoint);

    if ((status == USB_TRANSFER_COMPLETED) && transferred) {
        usb_control_setup_t *notify;
        size_t bytes_parsed = 0;
        do {
            notify = (usb_control_setup_t *)&eth_int_buf[bytes_parsed];
            if (notify->bmRequestType == 0b10100001)
            {
                switch (notify->bRequest)
                {
                    case NOTIFY_NETWORK_CONNECTION:
                        eth_device->network_connection = (bool)notify->wValue;
                        break;
                }
            }
            bytes_parsed += sizeof(usb_control_setup_t) + notify->wLength;
        } while (bytes_parsed < transferred);
    }
    usb_ScheduleInterruptTransfer(eth_device->endpoint.interrupt,
                                  eth_int_buf, sizeof eth_int_buf,
                                  eth_device->link_fn.interrupt, NULL);
    return USB_SUCCESS;
}

usb_error_t eth_tx_callback(__attribute__((unused)) usb_endpoint_t endpoint,
                            usb_transfer_status_t status,
                            __attribute__((unused)) size_t transferred,
                            usb_transfer_data_t *data) {
    eth_transfer_t* xfer = (eth_transfer_t*)data;
    eth_error_t err = ETH_OK;
    if (status)     // this indicates a transfer issue
        err = ETH_ERROR_TRANSFER_ERROR;
    
    if(xfer->callback)     // if link-layer sent callback
        xfer->callback(err, xfer);   // call it
    
    return USB_SUCCESS;
}
    

///------------------------------------------------------------------------
/// @brief linkinput callback function for @b Ethernet_Control_Model (ECM)
usb_error_t ecm_receive_callback(usb_endpoint_t endpoint,
                                 usb_transfer_status_t status,
                                 size_t transferred,
                                 __attribute__((unused)) usb_transfer_data_t *data){
    eth_device_t* eth_device = usb_GetEndpointData(endpoint);
    eth_transfer_t xfer;
    eth_error_t err = ETH_OK;
    memset(&xfer, 0, sizeof xfer);
    
    if ((status == USB_TRANSFER_COMPLETED) && transferred) {     // no error and data xfered
        xfer.buffer = eth_rx_buf;
        xfer.len = transferred;
        eth_device->link_fn.recvd(err, &xfer);
    }
    
    usb_ScheduleBulkTransfer(eth_device->endpoint.rx, eth_rx_buf, ETHERNET_MTU, eth_device->link_fn.rx, NULL);
    return USB_SUCCESS;
}
    

///----------------------------------------------------------------
/// @brief linkoutput function for @b Ethernet_Control_Model (ECM)
bool ecm_bulk_transmit(eth_device_t* device, eth_transfer_t *xfer)
{
    if(xfer->len <= ETHERNET_MTU) {
        usb_ScheduleBulkTransfer(device->endpoint.tx, xfer->buffer, xfer->len, eth_tx_callback, xfer);
        return true;
    }
    return false;
}


/****************************************************************************
 * Code for Network Control Model (NCM)
 * for USB-Ethernet devices
 */

/* Flag Values for NCM Network Capabilities. */
enum _cdc_ncm_bm_networkcapabilities
{
    CAPABLE_ETHERNET_PACKET_FILTER,
    CAPABLE_NET_ADDRESS,
    CAPABLE_ENCAPSULATED_RESPONSE,
    CAPABLE_MAX_DATAGRAM,
    CAPABLE_NTB_INPUT_SIZE_8BYTE
};
/* Helper Macro for Returning State of Network Capabilities Flag. */
#define ncm_device_supports(dev, bm) (((dev)->bm_capabilities >> (bm)) & 1)

/* Data Structure for NTB Config control request. */
struct _ntb_config_data
{
    uint32_t dwNtbInMaxSize;
    uint16_t wNtbInMaxDatagrams;
    uint16_t reserved;
};

/* NCM Transfer Header (NTH) Defintion */
#define NCM_NTH_SIG 0x484D434E
struct ncm_nth
{
    uint32_t dwSignature;   // "NCMH"
    uint16_t wHeaderLength; // size of this header structure (should be 12 for NTB-16)
    uint16_t wSequence;     // counter for NTB's sent
    uint16_t wBlockLength;  // size of the NTB
    uint16_t wNdpIndex;     // offset to first NDP
};

/* NCM Datagram Pointers (NDP) Definition */
struct ncm_ndp_idx
{
    uint16_t wDatagramIndex; // offset of datagram, if 0, then is end of datagram list
    uint16_t wDatagramLen;   // length of datagram, if 0, then is end of datagram list
};
#define NCM_NDP_SIG0 0x304D434E
#define NCM_NDP_SIG1 0x314D434E
struct ncm_ndp
{
    uint32_t dwSignature;               // "NCM0"
    uint16_t wLength;                   // size of NDP16
    uint16_t wNextNdpIndex;             // offset to next NDP16
    struct ncm_ndp_idx wDatagramIdx[1]; // pointer to end of NDP
};

#define NCM_NTH_LEN sizeof(struct ncm_nth)
#define NCM_NDP_LEN sizeof(struct ncm_ndp)
#define NCM_RX_MAX_DATAGRAMS 4
#define NCM_RX_DATAGRAMS_OVERFLOW_MUL 16 // this is here in the event that max datagrams is unsupported
#define NCM_RX_QUEUE_LEN (NCM_RX_MAX_DATAGRAMS * NCM_RX_DATAGRAMS_OVERFLOW_MUL)

///------------------------------------------------------------
/// @brief control setup for @b Network_Control_Model (NCM)
usb_error_t ncm_control_setup(eth_device_t *eth)
{
    size_t transferred;
    usb_error_t error = 0;
    usb_control_setup_t get_ntb_params = {0b10100001, REQUEST_GET_NTB_PARAMETERS, 0, 0, 0x1c};
    usb_control_setup_t ntb_config_request = {0b00100001, REQUEST_SET_NTB_INPUT_SIZE, 0, 0, ncm_device_supports(eth, CAPABLE_NTB_INPUT_SIZE_8BYTE) ? 8 : 4};
    // usb_control_setup_t multicast_filter_request = {0b00100001, REQUEST_SET_ETHERNET_MULTICAST_FILTERS, 0, 0, 0};
    usb_control_setup_t packet_filter_request = {0b00100001, REQUEST_SET_ETHERNET_PACKET_FILTER, 0x01, 0, 0};
    struct _ntb_config_data ntb_config_data = {ETH_RX_BUFFER_SIZE, NCM_RX_MAX_DATAGRAMS, 0};
    
    /* Query NTB Parameters for device (NCM devices) */
    error |= usb_DefaultControlTransfer(eth->dev, &get_ntb_params, &eth->ntb_params, 3, &transferred);
    
    /* Set NTB Max Input Size to 2048 (recd minimum NCM spec v 1.2) */
    error |= usb_DefaultControlTransfer(eth->dev, &ntb_config_request, &ntb_config_data, 3, &transferred);
    
    /* Reset packet filters */
    if (ncm_device_supports(eth, CAPABLE_ETHERNET_PACKET_FILTER))
        error |= usb_DefaultControlTransfer(eth->dev, &packet_filter_request, NULL, 3, &transferred);
    
    return error;
}

///------------------------------------------------------------
/// @brief linkinput function for @b Network_Control_Model (NCM)
usb_error_t ncm_receive_callback(usb_endpoint_t endpoint,
                                 usb_transfer_status_t status,
                                 size_t transferred,
                                 __attribute__((unused)) usb_transfer_data_t *data)
{
    eth_device_t* eth_device = usb_GetEndpointData(endpoint);
    eth_transfer_t xfer;
    memset(&xfer, 0, sizeof xfer);

    if((status == USB_TRANSFER_COMPLETED) && transferred) {
        bool parse_ntb = true;
        
        // get header and first NDP pointers
        uint8_t *ntb = eth_rx_buf;
        struct ncm_nth *nth = (struct ncm_nth *)ntb;
        if (nth->dwSignature != NCM_NTH_SIG)
            return USB_SUCCESS; // validate NTH signature field. If invalid, fail out
        
        // start proc'ing first NDP
        struct ncm_ndp *ndp = (struct ncm_ndp *)&ntb[nth->wNdpIndex];
        
        // repeat while ndp->wNextNdpIndex is non-zero
        do
        {
            if (ndp->dwSignature != NCM_NDP_SIG0)
                return USB_SUCCESS; // validate NDP signature field, if invalid, fail out
            
            // set datagram number to 0 and set datagram index pointer
            uint16_t dg_num = 0;
            struct ncm_ndp_idx *idx = (struct ncm_ndp_idx *)&ndp->wDatagramIdx;
            
            // a null datagram index structure indicates end of NDP
            do
            {
        
                xfer.buffer = &ntb[idx[dg_num].wDatagramIndex];
                xfer.len = idx[dg_num].wDatagramLen;
                                   
                eth_device->link_fn.recvd(ETH_OK, &xfer);   // net-layer callback
                
                dg_num++;
            } while ((idx[dg_num].wDatagramIndex) && (idx[dg_num].wDatagramLen));
            // if next NDP is 0, NTB is done and so is my sanity
            if (ndp->wNextNdpIndex == 0)
                break;
            // set next NDP
            ndp = (struct ncm_ndp *)&ntb[ndp->wNextNdpIndex];
        } while (parse_ntb);
                                        
                                        
        // queue up next transfer first
        usb_ScheduleBulkTransfer(eth_device->endpoint.rx, eth_rx_buf, ETH_RX_BUFFER_SIZE, eth_device->link_fn.rx, NULL);
    }
    return USB_SUCCESS;
}

/* This code packs a single TX Ethernet frame into an NCM transfer. */
#define NCM_HBUF_SIZE 64
#define get_next_offset(offset, divisor, remainder) \
(offset) + (divisor) - ((offset) % (divisor)) + (remainder)
                                         
///---------------------------------------------------------------
/// @brief linkoutput function for @b Network_Control_Model (NCM)
bool ncm_bulk_transmit(eth_device_t* device, eth_transfer_t *xfer)
{
    if( device==NULL || xfer==NULL )
        return false;
    
    memset(xfer->priv, 0, 64);
    uint16_t offset_ndp = get_next_offset(NCM_NTH_LEN, device->ntb_params.wNdpInAlignment, 0);
    
    // set up NTH
    struct ncm_nth *nth = (struct ncm_nth *)xfer->priv;
    nth->dwSignature = NCM_NTH_SIG;
    nth->wHeaderLength = NCM_NTH_LEN;
    nth->wSequence = device->sequence++;
    nth->wBlockLength = NCM_HBUF_SIZE + xfer->len;
    nth->wNdpIndex = offset_ndp;
        
    // set up NDP
    struct ncm_ndp *ndp = (struct ncm_ndp *)&xfer->priv[offset_ndp];
    ndp->dwSignature = NCM_NDP_SIG0;
    ndp->wLength = NCM_NDP_LEN + 4;
    ndp->wNextNdpIndex = 0;
    ndp->wDatagramIdx[0].wDatagramIndex = NCM_HBUF_SIZE;
    ndp->wDatagramIdx[0].wDatagramLen = xfer->len;
    ndp->wDatagramIdx[1].wDatagramIndex = 0;
    ndp->wDatagramIdx[1].wDatagramLen = 0;
        
    // send headers
    usb_ScheduleBulkTransfer(device->endpoint.tx, xfer->priv, 64, NULL, NULL);
    // send datagram
    usb_ScheduleBulkTransfer(device->endpoint.tx, xfer->buffer, xfer->len, eth_tx_callback, xfer);
    // if multiple of packet size, send 0-len transfer
    if(xfer->len % 64 == 0)
        usb_ScheduleBulkTransfer(device->endpoint.tx, xfer->buffer, 0, NULL, NULL);
    
    return USB_SUCCESS;
}


/** DESCRIPTOR PARSER AWAIT STATES */
enum _descriptor_parser_await_states
{
    PARSE_HAS_CONTROL_IF = 1,
    PARSE_HAS_MAC_ADDR = (1 << 1),
    PARSE_HAS_BULK_IF_NUM = (1 << 2),
    PARSE_HAS_BULK_IF = (1 << 3),
    PARSE_HAS_ENDPOINT_INT = (1 << 4),
    PARSE_HAS_ENDPOINT_IN = (1 << 5),
    PARSE_HAS_ENDPOINT_OUT = (1 << 6)
};

/*****************************************************************************************
 * @brief Parses descriptors for a USB device and checks for a valid CDC Ethernet device.
 * @return \b True if success (with NETIF initialized), \b False if not CDC-ECM/NCM or error.
 */
#define DESCRIPTOR_MAX_LEN 256

bool eth_USBHandleHubs(usb_device_t device){
    if(usb_GetDeviceFlags(device) & USB_IS_HUB){
        // add handling for hubs
        union
        {
            uint8_t bytes[DESCRIPTOR_MAX_LEN];   // allocate 256 bytes for descriptors
            usb_configuration_descriptor_t conf; // .. config descriptor alias
        } descriptor;
        size_t desc_len = usb_GetConfigurationDescriptorTotalLength(device, 0);
        size_t xferd;
        usb_GetConfigurationDescriptor(device, 0, &descriptor.conf, desc_len, &xferd);
        if(desc_len != xferd) return false;
        usb_SetConfiguration(device, &descriptor.conf, desc_len);
        return true;
    }
    return false;
}


eth_error_t eth_Open(eth_device_t *eth_device,
                     usb_device_t usb_device,
                     eth_transfer_callback_t recvd_fn){
    memset(eth_device, 0, sizeof (eth_device_t));
    if((eth_device == NULL) ||
       (usb_device == NULL) ||
       (recvd_fn == NULL))
        return ETH_ERROR_INVALID_PARAM;
       
    
    usb_RefDevice(usb_device);
    size_t xferd, parsed_len, desc_len;
    usb_error_t err;
    
    struct
    {
        usb_configuration_descriptor_t *addr;
        size_t len;
    } configdata;
    struct
    {
        usb_interface_descriptor_t *addr;
        size_t len;
    } if_bulk;
    struct
    {
        uint8_t in, out, interrupt;
    } endpoint_addr;
    union
    {
        uint8_t bytes[DESCRIPTOR_MAX_LEN];   // allocate 256 bytes for descriptors
        usb_descriptor_t desc;               // descriptor type aliases
        usb_device_descriptor_t dev;         // .. device descriptor alias
        usb_configuration_descriptor_t conf; // .. config descriptor alias
    } descriptor;
    
    err = usb_GetDeviceDescriptor(usb_device, &descriptor.dev, sizeof(usb_device_descriptor_t), &xferd);
    if (err || (xferd != sizeof(usb_device_descriptor_t)))
        return ETH_ERROR_DEVICE_CONFIG_ERROR;
     
    // check for main DeviceClass being type 0x00 - composite/if-specific
    if (descriptor.dev.bDeviceClass != USB_INTERFACE_SPECIFIC_CLASS)
        return ETH_ERROR_DEVICE_INCOMPATIBLE;
   
    // parse both configs for the correct one
    uint8_t num_configs = descriptor.dev.bNumConfigurations;
    for (uint8_t config = 0; config < num_configs; config++)
    {
        uint8_t ifnum = 0;
        uint8_t parse_state = 0;
        desc_len = usb_GetConfigurationDescriptorTotalLength(usb_device, config);
        parsed_len = 0;
        if (desc_len > 256)
            // if we overflow buffer, skip descriptor
            continue;

        // fetch config descriptor
        err = usb_GetConfigurationDescriptor(usb_device, config, &descriptor.conf, desc_len, &xferd);
        if (err || (xferd != desc_len))
            // if error or not full descriptor, skip
            continue;
        
        // set pointer to current working descriptor
        usb_descriptor_t *desc = &descriptor.desc;
        while (parsed_len < desc_len)
        {
            switch (desc->bDescriptorType)
            {
                case USB_ENDPOINT_DESCRIPTOR:
                {
                    // we should only look for this IF we have found the ECM control interface,
                    // and have retrieved the bulk data interface number from the CS_INTERFACE descriptor
                    // see case USB_CS_INTERFACE_DESCRIPTOR and case USB_INTERFACE_DESCRIPTOR
                    usb_endpoint_descriptor_t *endpoint = (usb_endpoint_descriptor_t *)desc;
                    if (parse_state & PARSE_HAS_BULK_IF)
                    {
                        if (endpoint->bEndpointAddress & (USB_DEVICE_TO_HOST))
                        {
                            endpoint_addr.in = endpoint->bEndpointAddress; // set out endpoint address
                            parse_state |= PARSE_HAS_ENDPOINT_IN;
                        }
                        else
                        {
                            endpoint_addr.out = endpoint->bEndpointAddress; // set in endpoint address
                            parse_state |= PARSE_HAS_ENDPOINT_OUT;
                        }
                        if ((parse_state & PARSE_HAS_ENDPOINT_IN) &&
                            (parse_state & PARSE_HAS_ENDPOINT_OUT) &&
                            (parse_state & PARSE_HAS_ENDPOINT_INT) &&
                            (parse_state & PARSE_HAS_MAC_ADDR))
                            goto init_success; // if we have both, we are done -- hard exit
                    }
                    else if (parse_state & PARSE_HAS_CONTROL_IF)
                    {
                        endpoint_addr.interrupt = endpoint->bEndpointAddress;
                        parse_state |= PARSE_HAS_ENDPOINT_INT;
                    }
                }
                    break;
                case USB_INTERFACE_DESCRIPTOR:
                    // we should look for this to either:
                    // (1) find the CDC Control Class/ECM interface, or
                    // (2) find the Interface number that matches the Interface indicated by the USB_CS_INTERFACE descriptor
                {
                    // cast to struct of type interface descriptor
                    usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)desc;
                    // if we have a control interface and ifnum is non-zero (we have an interface num to look for)
                    if (parse_state & PARSE_HAS_BULK_IF_NUM)
                    {
                        if ((iface->bInterfaceNumber == ifnum) &&
                            (iface->bNumEndpoints == 2) &&
                            (iface->bInterfaceClass == USB_CDC_DATA_CLASS))
                        {
                            if_bulk.addr = iface;
                            if_bulk.len = desc_len - parsed_len;
                            parse_state |= PARSE_HAS_BULK_IF;
                        }
                    }
                    else
                    {
                        // if we encounter another interface type after a control interface that isn't the CS_INTERFACE
                        // then we don't have the correct interface. This could be a malformed descriptor or something else
                        parse_state = 0; // reset parser state
                        
                        // If the interface is class CDC control and subtype ECM, this might be the correct interface union
                        // the next thing we should encounter is see case USB_CS_INTERFACE_DESCRIPTOR
                        if ((iface->bInterfaceClass == USB_COMM_CLASS) &&
                            ((iface->bInterfaceSubClass == ETH_TYPE_ECM) ||
                             (iface->bInterfaceSubClass == ETH_TYPE_NCM)))
                        {
                            // use this to set configuration
                            configdata.addr = &descriptor.conf;
                            configdata.len = desc_len;
                            eth_device->type = iface->bInterfaceSubClass;
                            if (usb_SetConfiguration(usb_device, configdata.addr, configdata.len))
                                return ETH_ERROR_DEVICE_CONFIG_ERROR;
                           
                            parse_state |= PARSE_HAS_CONTROL_IF;
                        }
                    }
                }
                    break;
                case ETH_CLASS_SPECIFIC_INTERFACE_DESCRIPTOR:
                    // this is a class-specific descriptor that specifies the interfaces used by the control interface
                {
                    eth_class_specific_interface_descriptor_t *cs = (eth_class_specific_interface_descriptor_t *)desc;
                    switch (cs->bDescriptorSubType)
                    {
                        case ETH_ETHERNET_FUNCTIONAL_DESCRIPTOR:
                        {
                            eth_ethernet_functional_descriptor_t *ethdesc = (eth_ethernet_functional_descriptor_t *)cs;
                            usb_control_setup_t get_mac_addr = {0b10100001, REQUEST_GET_NET_ADDRESS, 0, 0, 6};
                            size_t xferd_tmp;
                            uint8_t string_descriptor_buf[DESCRIPTOR_MAX_LEN];
                            usb_string_descriptor_t *macaddr = (usb_string_descriptor_t *)string_descriptor_buf;
                            // Get MAC address and save for lwIP use (ETHARP header)
                            // if index for iMacAddress valid and GetStringDescriptor success, save MAC address
                            // else attempt control transfer to get mac address and save it
                            // else generate random compliant local MAC address and send control request to set the hwaddr, then save it
                            if (ethdesc->iMacAddress &&
                                (!usb_GetStringDescriptor(usb_device, ethdesc->iMacAddress, 0, macaddr, DESCRIPTOR_MAX_LEN, &xferd_tmp)))
                            {
                                for (uint24_t i = 0; i < 6; i++)
                                    eth_device->hwaddr[i] = (nibble(macaddr->bString[2 * i]) << 4) | nibble(macaddr->bString[2 * i + 1]);
                                
                                parse_state |= PARSE_HAS_MAC_ADDR;
                            }
                            else if (!usb_DefaultControlTransfer(usb_device, &get_mac_addr, eth_device->hwaddr, 3, &xferd_tmp))
                            {
                                parse_state |= PARSE_HAS_MAC_ADDR;
                            }
                            else
                            {
#define RMAC_RANDOM_MAX 0xFFFFFF
                                usb_control_setup_t set_mac_addr = {0b00100001, REQUEST_SET_NET_ADDRESS, 0, 0, 6};
                                uint24_t rmac[2];
                                rmac[0] = (uint24_t)(random() & RMAC_RANDOM_MAX);
                                rmac[1] = (uint24_t)(random() & RMAC_RANDOM_MAX);
                                memcpy(eth_device->hwaddr, rmac, 6);
                                eth_device->hwaddr[0] &= 0xFE;
                                eth_device->hwaddr[0] |= 0x02;
                                if (!usb_DefaultControlTransfer(usb_device, &set_mac_addr, eth_device->hwaddr, 3, &xferd_tmp))
                                {
                                    parse_state |= PARSE_HAS_MAC_ADDR;
                                }
                            }
                        }
                            break;
                        case ETH_UNION_FUNCTIONAL_DESCRIPTOR:
                        {
                            // if union functional type, this contains interface number for bulk transfer
                            eth_union_functional_descriptor_t *func = (eth_union_functional_descriptor_t *)cs;
                            ifnum = func->bInterface;
                            parse_state |= PARSE_HAS_BULK_IF_NUM;
                        }
                            break;
                        case ETH_NCM_FUNCTIONAL_DESCRIPTOR:
                        {
                            eth_ncm_functional_descriptor_t *ncm = (eth_ncm_functional_descriptor_t *)cs;
                            eth_device->bm_capabilities = ncm->bmNetworkCapabilities;
                        }
                            break;
                    }
                }
            }
            parsed_len += desc->bLength;
            desc = (usb_descriptor_t *)(((uint8_t *)desc) + desc->bLength);
        }
    }
    return ETH_ERROR_DEVICE_CONFIG_ERROR;
init_success:
    eth_device->dev = usb_device;
    if(eth_device->type == ETH_TYPE_NCM)
        if (ncm_control_setup(eth_device))
            return ETH_ERROR_DEVICE_CONFIG_ERROR;
    
    /// Set link-layer functions
    eth_device->link_fn.rx = (eth_device->type == ETH_TYPE_NCM)   ? ncm_receive_callback
    : (eth_device->type == ETH_TYPE_ECM) ? ecm_receive_callback
    : NULL;
    
    eth_device->link_fn.tx = (eth_device->type == ETH_TYPE_NCM)   ? ncm_bulk_transmit
    : (eth_device->type == ETH_TYPE_ECM) ? ecm_bulk_transmit
    : NULL;
    
    eth_device->link_fn.interrupt = interrupt_receive_callback;
    eth_device->link_fn.recvd = recvd_fn;
    
    if ((eth_device->link_fn.tx == NULL) ||
        (eth_device->link_fn.rx == NULL) ||
        (eth_device->link_fn.recvd == NULL))
        return ETH_ERROR_DEVICE_CONFIG_ERROR;
    
    // switch to alternate interface
    if (usb_SetInterface(usb_device, if_bulk.addr, if_bulk.len))
        return ETH_ERROR_DEVICE_CONFIG_ERROR;
        
    // set endpoints
    eth_device->endpoint.rx = usb_GetDeviceEndpoint(usb_device, endpoint_addr.in);
    eth_device->endpoint.tx = usb_GetDeviceEndpoint(usb_device, endpoint_addr.out);
    eth_device->endpoint.interrupt = usb_GetDeviceEndpoint(usb_device, endpoint_addr.interrupt);
    
    if(eth_device->type == ETH_TYPE_NCM)
        // NCM we need to do manual terminate mode
        usb_SetEndpointFlags(eth_device->endpoint.tx, USB_MANUAL_TERMINATE);
    
    // set eth_device_t as data to endpoint and device
    usb_SetEndpointData(eth_device->endpoint.rx, eth_device);
    usb_SetEndpointData(eth_device->endpoint.tx, eth_device);
    usb_SetEndpointData(eth_device->endpoint.interrupt, eth_device);
    usb_SetDeviceData(usb_device, eth_device);
    
    // enqueue callbacks for receiving interrupt and RX transfers from this device.
    usb_ScheduleInterruptTransfer(eth_device->endpoint.interrupt,
                                  eth_int_buf, ETH_INTERRUPT_BUFFER_SIZE,
                                  eth_device->link_fn.interrupt, NULL);
    usb_ScheduleBulkTransfer(eth_device->endpoint.rx, eth_rx_buf,
                             (eth_device->type == ETH_TYPE_NCM) ? ETH_RX_BUFFER_SIZE : ETHERNET_MTU, eth_device->link_fn.rx, NULL);
    return ETH_OK;
}

bool eth_SetRecvdCallback(eth_device_t *eth_device, eth_transfer_callback_t recvd_fn){
    if(recvd_fn==NULL) return false;
    eth_device->link_fn.recvd = recvd_fn;
    return true;
}


eth_error_t eth_Write(eth_device_t *eth_device, eth_transfer_t *xfer){
    eth_error_t err;
    if(eth_device == NULL || xfer == NULL)
        return ETH_ERROR_INVALID_PARAM;
    
    if(eth_device->link_fn.tx){
        if(eth_device->link_fn.tx(eth_device, xfer))
            return ETH_OK;
        else err = ETH_ERROR_TRANSFER_ERROR;
    } else err = ETH_ERROR_DEVICE_CONFIG_ERROR;
    return err;
}


eth_error_t eth_Close(eth_device_t *eth_device){
    usb_ResetDevice(eth_device->dev);
    usb_UnrefDevice(eth_device->dev);
    memset(eth_device, 0, sizeof (eth_device_t));
    return ETH_OK;
}

