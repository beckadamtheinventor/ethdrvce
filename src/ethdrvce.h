
/****************************************************************************
 * Header File for Communications Data Class (CDC)
 * for USB-Ethernet devices
 * Includes definitions for CDC-ECM and CDC-NCM
 */

#ifndef eth_h
#define eth_h

#include <stdbool.h>
#include <stdint.h>
#include <usbdrvce.h>

/* Ethernet MTU - ECM & NCM */
#define ETHERNET_MTU 1518

/* Interrupt buffer size */
#define ETH_INTERRUPT_BUFFER_SIZE 64

/* Reserved buffer sizes */
#define ETH_RX_BUFFER_SIZE  2048
#define ETH_TX_BUFFER_SIZE  2048

/* USB CDC Ethernet Device Classes */
enum eth_cdc_ethernet_device_types {
    ETH_TYPE_ECM = 0x06,
    ETH_TYPE_NCM = 0x0D
};

enum eth_cdc_class_specific_descriptors {
    ETH_UNION_FUNCTIONAL_DESCRIPTOR = 0x06,
    ETH_ETHERNET_FUNCTIONAL_DESCRIPTOR = 0x0F,
    ETH_NCM_FUNCTIONAL_DESCRIPTOR = 0x1A,
    ETH_CLASS_SPECIFIC_INTERFACE_DESCRIPTOR = 0x24
};

/* Class-Specific Interface Descriptor*/
typedef struct
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t data[];
} eth_class_specific_interface_descriptor_t;

/* Union Functional Descriptor*/
typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bControlInterface;
    uint8_t bInterface;
} eth_union_functional_descriptor_t;

/* Ethernet Functional Descriptor */
typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t iMacAddress;
    uint8_t bmEthernetStatistics[4];
    uint8_t wMaxSegmentSize[2];
    uint8_t wNumberMCFilters[2];
    uint8_t bNumberPowerFilters;
} eth_ethernet_functional_descriptor_t;

/* NCM Functional Descriptor*/
typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint16_t bcdNcmVersion;
    uint8_t bmNetworkCapabilities;
} eth_ncm_functional_descriptor_t;

/* CDC Control Request Codes */
enum eth_cdc_request_codes
{
    REQUEST_SEND_ENCAPSULATED_COMMAND = 0,
    REQUEST_GET_ENCAPSULATED_RESPONSE,
    REQUEST_SET_ETHERNET_MULTICAST_FILTERS = 0x40,
    REQUEST_SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER,
    REQUEST_GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER,
    REQUEST_SET_ETHERNET_PACKET_FILTER,
    REQUEST_GET_ETHERNET_STATISTIC,
    REQUEST_GET_NTB_PARAMETERS = 0x80,
    REQUEST_GET_NET_ADDRESS,
    REQUEST_SET_NET_ADDRESS,
    REQUEST_GET_NTB_FORMAT,
    REQUEST_SET_NTB_FORMAT,
    REQUEST_GET_NTB_INPUT_SIZE,
    REQUEST_SET_NTB_INPUT_SIZE,
    REQUEST_GET_MAX_DATAGRAM_SIZE,
    REQUEST_SET_MAX_DATAGRAM_SIZE,
    REQUEST_GET_CRC_MODE,
    REQUEST_SET_CRC_MODE,
};

/* CDC Notification Codes */
enum _cdc_notification_codes
{
    NOTIFY_NETWORK_CONNECTION,
    NOTIFY_RESPONSE_AVAILABLE,
    NOTIFY_CONNECTION_SPEED_CHANGE = 0x2A
};

/* eth driver error codes */
typedef enum {
    ETH_OK,
    ETH_ERROR_INVALID_PARAM,
    ETH_ERROR_DEVICE_CONFIG_ERROR,
    ETH_ERROR_USB_FAILED,
    ETH_ERROR_DEVICE_INCOMPATIBLE,
    ETH_ERROR_TRANSFER_ERROR,
    ETH_ERROR_ALLOCATOR_UNSET,
    ETH_ERROR_NO_MEMORY,
    ETH_ERROR_MTU_OVERFLOW,
    ETH_ERROR_DEVICE_DISCONNECTED,
    ETH_ERROR_NETWORK_FAILURE
} eth_error_t;

/* Defines NTB Parameter Structure for CDC-NCM */
struct eth_ntb_parameters {
    uint16_t wLength;
    uint16_t bmNtbFormatsSupported;
    uint32_t dwNtbInMaxSize;
    uint16_t wNdpInDivisor;
    uint16_t wNdpInPayloadRemainder;
    uint16_t wNdpInAlignment;
    uint16_t reserved;
    uint32_t dwNtbOutMaxSize;
    uint16_t wNdpOutDivisor;
    uint16_t wNdpOutPayloadRemainder;
    uint16_t wNdpOutAlignment;
    uint16_t wNtbOutMaxDatagrams;
};


struct _eth_device;
struct _eth_transfer;
typedef struct _eth_device eth_device_t;
typedef struct _eth_transfer eth_transfer_t;
typedef void (*eth_transfer_callback_t)(eth_error_t error, eth_transfer_t *xfer);

/* Defines a structure for holding transfer data. */
typedef struct _eth_transfer {
    void *buffer;
    size_t len;
    eth_transfer_callback_t callback;
    void *callback_data;
    uint8_t priv[64];
} eth_transfer_t;

struct _eth_device {
    usb_device_t dev;           /**< USB device */
    uint8_t type;               /**< USB device subtype (ECM/NCM) */
    uint8_t hwaddr[6];
    struct {
        /** Endpoint refs for device. */
        usb_endpoint_t tx;
        usb_endpoint_t rx;
        usb_endpoint_t interrupt;
    } endpoint;
    struct {
        /** Defines link-level callbacks (ECM/NCM usb bulk transfer) */
        usb_transfer_callback_t rx;
        eth_transfer_callback_t recvd;
        bool (*tx)(eth_device_t *device, eth_transfer_t *xfer);
        usb_transfer_callback_t interrupt;
    } link_fn;
    struct eth_ntb_parameters ntb_params;       /**< NCM parameters, unused for ECM. */
    uint16_t sequence;                          /**< per NCM device - transfer sequence counter */
    uint8_t bm_capabilities;                    /**< device capabilities */
    bool network_connection:1;                  /**< Connection state of interface. */
};

bool eth_USBHandleHubs(usb_device_t device);


eth_error_t eth_Open(eth_device_t *eth_device,
                     usb_device_t usb_device,
                     eth_transfer_callback_t recvd_fn);

bool eth_SetRecvdCallback(eth_device_t *eth_device, eth_transfer_callback_t recvd_fn);
eth_error_t eth_Write(eth_device_t *eth_device, eth_transfer_t *xfer);


eth_error_t eth_Close(eth_device_t *eth_device);

#endif
