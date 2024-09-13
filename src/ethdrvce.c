#include <stdint.h>
#include <usbdrvce.h>

void* (*usr_malloc)(size_t);
void (*usr_free)(void*);

enum eth_device_type {
    ETH_TYPE_ECM,
    ETH_TYPE_NCM
};


bool eth_SetAllocator(void* (*in_malloc)(size_t), void (in_free)(void*)){
    if(in_malloc==NULL || in_free==NULL) return false;
    usr_malloc = in_malloc;
    usr_free = in_free;
}

#define ETHERNET_MTU    1518
#define ETHERNET_IN_MAX 2048
#define ETH_INTERRUPT_BUFFER_SIZE   64
uint8_t eth_rx_buf[ETHERNET_IN_MAX];
uint8_t eth_tx_buf[ETHERNET_MTU];
uint8_t eth_int_buf[ETH_INTERRUPT_BUFFER_SIZE]

typedef enum {
    ETH_OK,
    ETH_ERROR_INVALID_PARAM,
    ETH_ERROR_USB_FAILED,
    ETH_ERROR_DEVICE_INCOMPATIBLE,
    ETH_ERROR_TRANSFER_ERROR,
    ETH_ERROR_NO_MEMORY,
    ETH_ERROR_DEVICE_DISCONNECTED
} eth_error_t;

struct eth_packet {
    size_t length;
    struct eth_packet *next;
    uint8_t data[];
};

typedef struct {
    usb_device_t dev; /**< USB device */
    /**< USB device subtype (ECM/NCM) */
    uint8_t type;
    /**< An IN endpoint. */
    struct {
        usb_endpoint_t endpoint;
        bool (*linkoutput)(eth_device_t, void*, size_t)
    } tx;
    struct {
        usb_endpoint_t endpoint;
        bool (*callback)(void*, size_t)
    } rx;
    struct {
        usb_endpoint_t endpoint;
        bool (*callback)(void*, size_t)
    } interrupt;
    /**< An INT endpoint. */
    usb_endpoint_t it;
    struct {
        struct eth_packet *read;
        struct eth_packet *write;
    } rx_packet_queue;
    eth_error_t err;
    bool network_connection:1;
} eth_device_t;

usb_error_t
interrupt_receive_callback(__attribute__((unused)) usb_endpoint_t endpoint,
                           usb_transfer_status_t status,
                           size_t transferred,
                           usb_transfer_data_t *data) {
    eth_device_t* eth_device = usb_GetEndpointData(endpoint);
    eth_device->err = ETH_OK;
    if (status) eth_device->err = ETH_ERROR_TRANSFER_ERROR;
    else if ((status == USB_TRANSFER_COMPLETED) && transferred) {
        usb_control_setup_t *notify;
        size_t bytes_parsed = 0;
        do {
            notify = (usb_control_setup_t *)&ibuf[bytes_parsed];
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
    usb_ScheduleInterruptTransfer(eth_device->interrupt.endpoint,
                                  eth_int_buf, sizeof eth_int_buf,
                                  eth_device->interrupt.callback, NULL);
    return USB_SUCCESS;
}

usb_error_t bulk_transmit_callback(usb_endpoint_t endpoint,
                                   usb_transfer_status_t status,
                                   __attribute__((unused)) size_t transferred,
                                   usb_transfer_data_t *data) {
    eth_device_t* eth_device = usb_GetEndpointData(endpoint);
    eth_device->err = ETH_OK;
    if (status) eth_device->err = ETH_ERROR_TRANSFER_ERROR;
    usr_free(data);
    return USB_SUCCESS;
}
    

///------------------------------------------------------------------------
/// @brief linkinput callback function for @b Ethernet_Control_Model (ECM)
usb_error_t ecm_receive_callback(__attribute__((unused)) usb_endpoint_t endpoint,
                                 usb_transfer_status_t status,
                                 size_t transferred,
                                 __attribute__((unused)) usb_transfer_data_t *data){
    eth_device_t* eth_device = usb_GetEndpointData(endpoint);
    eth_device->err = ETH_OK;
    if (status) eth_device->err = ETH_ERROR_TRANSFER_ERROR;
    else if ((status == USB_TRANSFER_COMPLETED) && transferred) {
        
        struct eth_packet *input = usr_malloc(transferred + sizeof(struct eth_packet));
        if(input){
            
            input->length = transferred;
            input->next = NULL;
            memcpy(input->data, eth_rx_buf, transferred);
            
            if(eth_device->rx_packet_queue.write){
                eth_device->rx_packet_queue.write.next = input;
                eth_device->rx_packet_queue.write = input;
            }
            else
                eth_device->rx_packet_queue.write = input;
        }
        else eth_device->err = ETH_ERROR_NO_MEMORY;
    }
    usb_ScheduleBulkTransfer(eth_device->rx.endpoint, eth_rx_buf, ETHERNET_MTU, eth_device->rx.callback, NULL);
    return USB_SUCCESS;
}
    

///----------------------------------------------------------------
/// @brief linkoutput function for @b Ethernet_Control_Model (ECM)
bool ecm_bulk_transmit(eth_device_t* device, void* buf, size_t len)
{
    struct eth_packet *output = usr_malloc(len + sizeof(struct eth_packet));
    if(output){
        output->length = len;
        memcpy(output->data, buf, len);
        usb_ScheduleBulkTransfer(device->tx.endpoint, output->data, output->len, device->tx.linkoutput, output);
    } else eth_device->err = ETH_ERROR_NO_MEMORY;
    return USB_SUCCESS;
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
#define ncm_device_supports(dev, bm) (((dev)->class.ncm.bm_capabilities >> (bm)) & 1)

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
    if (eth->type != USB_NCM_SUBCLASS)
        return USB_SUCCESS;
    size_t transferred;
    usb_error_t error = 0;
    usb_control_setup_t get_ntb_params = {0b10100001, REQUEST_GET_NTB_PARAMETERS, 0, 0, 0x1c};
    usb_control_setup_t ntb_config_request = {0b00100001, REQUEST_SET_NTB_INPUT_SIZE, 0, 0, ncm_device_supports(eth, CAPABLE_NTB_INPUT_SIZE_8BYTE) ? 8 : 4};
    // usb_control_setup_t multicast_filter_request = {0b00100001, REQUEST_SET_ETHERNET_MULTICAST_FILTERS, 0, 0, 0};
    usb_control_setup_t packet_filter_request = {0b00100001, REQUEST_SET_ETHERNET_PACKET_FILTER, 0x01, 0, 0};
    struct _ntb_config_data ntb_config_data = {NCM_RX_NTB_MAX_SIZE, NCM_RX_MAX_DATAGRAMS, 0};
    
    /* Query NTB Parameters for device (NCM devices) */
    error |= usb_DefaultControlTransfer(eth->device, &get_ntb_params, &eth->class.ncm.ntb_params, USB_CDC_MAX_RETRIES, &transferred);
    
    /* Set NTB Max Input Size to 2048 (recd minimum NCM spec v 1.2) */
    error |= usb_DefaultControlTransfer(eth->device, &ntb_config_request, &ntb_config_data, USB_CDC_MAX_RETRIES, &transferred);
    
    /* Reset packet filters */
    if (ncm_device_supports(eth, CAPABLE_ETHERNET_PACKET_FILTER))
        error |= usb_DefaultControlTransfer(eth->device, &packet_filter_request, NULL, USB_CDC_MAX_RETRIES, &transferred);
    
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
    if (status) eth_device->err = ETH_ERROR_TRANSFER_ERROR;
    else if((status == USB_TRANSFER_COMPLETED) && transferred)
    {
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
                // attempt to allocate pbuf
                struct eth_packet *input = usr_malloc(idx[dg_num].wDatagramLen + sizeof(struct eth_packet));
                if (input == NULL)
                { // if allocation failed, break loops
                    parse_ntb = false;
                    break;
                }
                
                input->length = idx[dg_num].wDatagramLen;
                input->next = NULL;
                memcpy(input->data, eth_rx_buf, transferred);
                
                if(eth_device->rx_packet_queue.write)
                    while(eth_device->rx_packet_queue.write.next)
                        eth_device->rx_packet_queue.write = eth_device->rx_packet_queue.write.next;
                eth_device->rx_packet_queue.write = input;
                
                
                if (pbuf_take(p, &ntb[idx[dg_num].wDatagramIndex], idx[dg_num].wDatagramLen))
                { // if pbuf take fails, break loops
                    parse_ntb = false;
                    break;
                }
                // enqueue packet, we will finish processing first
                rx_queue[enqueued++] = p;
                dg_num++;
            } while ((idx[dg_num].wDatagramIndex) && (idx[dg_num].wDatagramLen));
            // if next NDP is 0, NTB is done and so is my sanity
            if (ndp->wNextNdpIndex == 0)
                break;
            // set next NDP
            ndp = (struct ncm_ndp *)&ntb[ndp->wNextNdpIndex];
        } while (parse_ntb);
        
        LINK_STATS_INC(link.recv);
        MIB2_STATS_NETIF_ADD(&dev->iface, ifinoctets, transferred);
        
        // queue up next transfer first
        usb_ScheduleBulkTransfer(dev->rx.endpoint, dev->rx.buf, NCM_RX_NTB_MAX_SIZE, dev->rx.callback, data);
        
        // hand packet queue to lwIP
        for (int i = 0; i < enqueued; i++)
            if (rx_queue[i])
                if (dev->iface.input(rx_queue[i], &dev->iface) != ERR_OK)
                    pbuf_free(rx_queue[i]);
    }
    return USB_SUCCESS;
}
