#include "Hakadorun2.h"
#include <USB.h>
#include <USBSlave.h>
#include <USBHID.h>
#include <USBHost.h>
#include <USBHostHID.h>
#include <string.h>
#include "Queue.h"

#define SETUP_DATA_SIZE  9

#define USB_SLAVE_INTERRUPT_IN              1
#define USB_SLAVE_INTERRUPT_MAX_PACKET_SIZE INPUT_REPORT_SIZE
#define USB_SLAVE_INTERRUPT_INTERVAL_MS     1

#define TYPE_IN_BMREQUESTTYPE(request)       ((request) & 0x60)
#define RECIPIENT_IN_BMREQUESTTYPE(request)  ((request) & 0x1F)

#define CONTROL_BUFFER_SIZE     256
#define CONTROL_MAX_PACKET_SIZE 8

#define USB_CONFIGRATION_DESC_TOTAL_LENGTH (sizeof(usb_deviceConfigurationDescriptor_t) \
                                            + sizeof(usb_deviceInterfaceDescriptor_t)   \
                                            + sizeof(usb_hidDescriptor_t) \
                                            + sizeof(usb_deviceEndpointDescriptor_t)) // wTotalLength

#define ROM_COPY(dst_, src_, index_, count_) \
  for ((index_) = 0; (index_) < (count_); (index_)++) { \
    (dst_)[(index_)] = (src_)[(index_)]; \
  }

rom uint8 DEVICE_DESCRIPTOR[] = {
  sizeof(usb_deviceDescriptor_t), // bLength
  USB_DESCRIPTOR_TYPE_DEVICE,     // bDescriptorType
  0x10, 0x01,                     // bcdUSB
  0x00,                      // bDeviceClass
  0x00,                      // bDeviceSubClass
  0x00,                      // bDeviceProtocol
  CONTROL_MAX_PACKET_SIZE,   // bMaxPacketSize0
  0xC0, 0x16,                // idVendor 
  0xDB, 0x27,                // idProduct
  0x00, 0x00,                // bcdDevice
  0x01,                      // iManufacturer
  0x02,                      // iProduct
  0x03,                      // iSerialNumber
  0x01,                      // bNumConfigurations
};

rom uint8 CONFIG_DESCRIPTOR[] = {
  sizeof(usb_deviceConfigurationDescriptor_t),  // bLength
  USB_DESCRIPTOR_TYPE_CONFIGURATION,            // bDescriptorType,
  USB_CONFIGRATION_DESC_TOTAL_LENGTH, 0x00,   //wTotalLength,
  0x01,                                       // bNumInterface
  0x01,                                       // bConfigurationValue
  0x00,                                       // iConfiguration
  USB_CONFIG_BMATTRIBUTES_RESERVED_SET_TO_1,  // bmAttributes
  300 / 2                                     // MaxPower 
};

rom uint8 INTERFACE_DESCRIPTOR[] = {
  sizeof(usb_deviceInterfaceDescriptor_t), // bLength
  USB_DESCRIPTOR_TYPE_INTERFACE,    // bDescriptorType
  0x00,                         // bInterfaceNumber
  0x00,                         // bAlternateSetting
  0x01,                         // bNumEndPoints
  USB_CLASS_HID,                   // bInterfaceClass
  USB_SUBCLASS_HID_BOOT_INTERFACE, // bInterfaceSubClass
  USB_PROTOCOL_HID_KEYBOARD,       // bInterfaceProtocol
  0x00                             // iInterface
};

rom uint8 HID_REPORT_DESCRIPTOR[] = {
  0x05, 0x01,   // Usage Page (Generic Desktop)
  0x09, 0x06,   // Usage (Keyboard)
  0xA1, 0x01,   // Collection (Application)
  0x85, 0x01,   //     Report ID (1)
  0x05, 0x07,   //     Usage Page (Key Codes)
  0x19, 0xe0,   //     Usage Minimum (224)
  0x29, 0xe7,   //     Usage Maximum (231)
  0x15, 0x00,   //     Logical Minimum (0)
  0x25, 0x01,   //     Logical Maximum (1)
  0x75, 0x01,   //     Report Size (1)
  0x95, 0x08,   //     Report Count (8)
  0x81, 0x02,   //     Input (Data, Variable, Absolute)

  0x95, 0x01,   //     Report Count (1)
  0x75, 0x08,   //     Report Size (8)
  0x81, 0x01,   //     Input (Constant) reserved byte(1)

  0x95, 0x05,   //     Report Count (5)
  0x75, 0x01,   //     Report Size (1)
  0x05, 0x08,   //     Usage Page (Page# for LEDs)
  0x19, 0x01,   //     Usage Minimum (1)
  0x29, 0x05,   //     Usage Maximum (5)
  0x91, 0x02,   //     Output (Data, Variable, Absolute), Led report
  0x95, 0x01,   //     Report Count (1)
  0x75, 0x03,   //     Report Size (3)
  0x91, 0x01,   //     Output (Data, Variable, Absolute), Led report padding

  0x95, 0x06,   //     Report Count (6)
  0x75, 0x08,   //     Report Size (8)
  0x15, 0x00,   //     Logical Minimum (0)
  0x25, 0x65,   //     Logical Maximum (101)
  0x05, 0x07,   //     Usage Page (Key codes)
  0x19, 0x00,   //     Usage Minimum (0)
  0x29, 0x65,   //     Usage Maximum (101)
  0x81, 0x00,   //     Input (Data, Array) Key array(6 bytes)
  0xC0,         //  End Collection

  0x06, 0x00, 0xFF, // Usage Page (Vendor-defined)
  0x09, 0x01,       // Usage (Vendor-defined)
  0xA1, 0x01,       // Collection (Application)
  0x85, 0x02,       //     Report ID (2)
  0x15, 0x00,       //     Logical Minimum (0)
  0x26, 0xFF, 0x00, //     Logical Maximum (255)
  0x75, 0x08,       //     Report Size (8)
  0x95, 0x40,       //     Report Count(64)
  0x09, 0x00,       //     Usage (Undefined)
  0xb2, 0x02, 0x01, //     Feature (Data,Var,Abs,Buf)
  0xC0,             //  End Collection
};

rom uint8 HID_DESCRIPTOR[] = {
  sizeof(usb_hidDescriptor_t),   // bLength
  USB_DESCRIPTOR_TYPE_HID,       // bDescriptorType
  0x10, 0x01,                    // bcdHCD
  0x00,                          // ContryCode
  0x01,                          // bNumDescriptor
  USB_DESCRIPTOR_TYPE_REPORT,    // bDescriptorType2
  sizeof(HID_REPORT_DESCRIPTOR), 0x00  // wDescriptorLength2 
};

rom uint8 ENDPOINT_DESCRIPTOR[] = {
  sizeof(usb_deviceEndpointDescriptor_t),                         // bLength
  USB_DESCRIPTOR_TYPE_ENDPOINT,                                   // bDescriptorType
  USB_BMREQUESTTYPE_DEV_TO_HOST | USB_SLAVE_INTERRUPT_IN,        // bEndpointAddress
  USB_ENDPOINT_DESCRIPTOR_ATTR_INTERRUPT,                         // bmAttributes
  USB_SLAVE_INTERRUPT_MAX_PACKET_SIZE,  0x00,                     // wMaxPacketSize
  USB_SLAVE_INTERRUPT_INTERVAL_MS                                 // bInterval
};

rom uint8 HID_STRING_DESCRIPTOR_0[] = {
  sizeof(usb_deviceStringDescriptorZero_t), // bLength
  USB_DESCRIPTOR_TYPE_STRING,               // bDescriptorType
  0x09 , 0x04                               // wLANGID0
};

// Manufacturer
rom uint8 HID_STRING_DESCRIPTOR_1[] = {
  2 + (13 * 2),                   // bLenght
  USB_DESCRIPTOR_TYPE_STRING,    // bDescriptorType
  'd', 0x00,
  '-', 0x00,
  'r', 0x00,
  'i', 0x00,
  's', 0x00,
  's', 0x00,
  'o', 0x00,
  'k', 0x00,
  'u', 0x00,
  '.', 0x00,
  'n', 0x00,
  'e', 0x00,
  't', 0x00 
};

// Product
rom uint8 HID_STRING_DESCRIPTOR_2[] = {
  2 + (10 * 2),                   // bLenght
  USB_DESCRIPTOR_TYPE_STRING,    // bDescriptorType
  'H', 0x00,
  'a', 0x00,
  'k', 0x00,
  'a', 0x00,
  'd', 0x00,
  'o', 0x00,
  'r', 0x00,
  'u', 0x00,
  'n', 0x00,
  '2', 0x00
};

// SerialNumber
rom uint8 HID_STRING_DESCRIPTOR_3[] = {
  2 + (18 * 2),                   // bLenght
  USB_DESCRIPTOR_TYPE_STRING,    // bDescriptorType
  'd', 0x00,
  '-', 0x00,
  'r', 0x00,
  'i', 0x00,
  's', 0x00,
  's', 0x00,
  'o', 0x00,
  'k', 0x00,
  'u', 0x00,
  '.', 0x00,
  'n', 0x00,
  'e', 0x00,
  't', 0x00,
  ':', 0x00,
  '0', 0x00,
  '0', 0x00,
  '0', 0x00,
  '2', 0x00,
};


static VOS_HANDLE m_slave_usb;
static VOS_HANDLE m_control_ep_in;
static VOS_HANDLE m_control_ep_out;
static VOS_HANDLE m_interrupt_ep_in;
static vos_semaphore_t m_control_thread_end_semaphore;
static volatile uint8 m_interrupt_transfer_valid;
static uint8 m_current_output_report[OUTPUT_REPORT_SIZE];
static uint8 m_current_input_report[INPUT_REPORT_SIZE];
static uint8 m_configuration_no;
static uint8 m_protocol_no;
static uint8 m_duration;

static uint8 recv_setup_data(uint8 *data)
{
  usbslave_ioctl_cb_t c;
  c.ioctl_code = VOS_IOCTL_USBSLAVE_WAIT_SETUP_RCVD;
  c.request.setup_or_bulk_transfer.buffer = data;
  c.request.setup_or_bulk_transfer.size = SETUP_DATA_SIZE;
  return vos_dev_ioctl(m_slave_usb, &c);
}

static void stall_endpoint(uint8 ep_num)
{
  usbslave_ioctl_cb_t c;
  c.ioctl_code = VOS_IOCTL_USBSLAVE_ENDPOINT_STALL;
  c.ep = ep_num;
  vos_dev_ioctl(m_slave_usb, &c);
}

static uint8 is_endpoint_stall(uint8 ep_num)
{
  usbslave_ioctl_cb_t c;
  uint8 s;

  c.ioctl_code = VOS_IOCTL_USBSLAVE_ENDPOINT_STATE;
  c.ep = ep_num;
  c.get = &s;
  vos_dev_ioctl(m_slave_usb, &c);

  return s;
}

static void clear_endpoint(uint8 ep_num)
{
  usbslave_ioctl_cb_t c;

  c.ioctl_code = VOS_IOCTL_USBSLAVE_ENDPOINT_CLEAR;
  c.ep = ep_num;
  vos_dev_ioctl(m_slave_usb, &c);
}

static VOS_HANDLE get_control_endpoint(uint8 ep_num)
{
  usbslave_ioctl_cb_t c;
  VOS_HANDLE h;

  c.ioctl_code = VOS_IOCTL_USBSLAVE_GET_CONTROL_ENDPOINT_HANDLE;
  c.ep = ep_num;
  c.get = &h;
  vos_dev_ioctl(m_slave_usb, &c);

  return h;
}

static VOS_HANDLE get_interrupt_endpoint(uint8 ep_num)
{
  usbslave_ioctl_cb_t c;
  VOS_HANDLE h;

  c.ioctl_code = VOS_IOCTL_USBSLAVE_GET_INT_IN_ENDPOINT_HANDLE;
  c.ep = ep_num;
  c.get = &h;
  vos_dev_ioctl(m_slave_usb, &c);

  return h;
}

static void set_max_packet_size(VOS_HANDLE ep, uint32 max_packet_size)
{
  usbslave_ioctl_cb_t c;

  c.ioctl_code = VOS_IOCTL_USBSLAVE_SET_ENDPOINT_MAX_PACKET_SIZE;
  c.handle = ep;
  c.request.ep_max_packet_size = max_packet_size;
  vos_dev_ioctl(m_slave_usb, &c);
}

static void control_transfer(VOS_HANDLE ep, void *data, uint8 size)
{
  usbslave_ioctl_cb_t c;
  
  c.ioctl_code = VOS_IOCTL_USBSLAVE_SETUP_TRANSFER;
  c.handle = ep;
  c.request.setup_or_bulk_transfer.buffer = data;
  c.request.setup_or_bulk_transfer.size = size;
  vos_dev_ioctl(m_slave_usb, &c);
}

static void non_control_transfer(VOS_HANDLE ep, void *data, uint8 size)
{
  usbslave_ioctl_cb_t c;

  c.ioctl_code = VOS_IOCTL_USBSLAVE_TRANSFER;
  c.handle = ep;
  c.request.setup_or_bulk_transfer.buffer = data;
  c.request.setup_or_bulk_transfer.size = size;
  vos_dev_ioctl(m_slave_usb, &c);
}

static void set_address(uint8 addr)
{
  usbslave_ioctl_cb_t c;

  c.ioctl_code = VOS_IOCTL_USBSLAVE_SET_ADDRESS;
  c.set = (void*)addr;
  vos_dev_ioctl(m_slave_usb, &c);
}

static void set_configuration(uint8 no)
{
  usbslave_ioctl_cb_t c;

  c.ioctl_code = VOS_IOCTL_USBSLAVE_SET_CONFIGURATION;
  c.set = (void*)no;
  vos_dev_ioctl(m_slave_usb, &c);

  m_configuration_no = no;
}

static uint8 get_string_descriptor(uint8 index, uint8** pp_data, uint8* p_size)
{
  uint8 i;
  uint8 *data;

  data = *pp_data;
  switch (index) {
  case 0:
    ROM_COPY(data, HID_STRING_DESCRIPTOR_0, i, sizeof(HID_STRING_DESCRIPTOR_0)); 
    data += sizeof(HID_STRING_DESCRIPTOR_0); 
    *p_size += sizeof(HID_STRING_DESCRIPTOR_0);
    break;
  case 1:
    ROM_COPY(data, HID_STRING_DESCRIPTOR_1, i, sizeof(HID_STRING_DESCRIPTOR_1));
    data += sizeof(HID_STRING_DESCRIPTOR_1);
    *p_size += sizeof(HID_STRING_DESCRIPTOR_1);
    break;
  case 2:
    ROM_COPY(data, HID_STRING_DESCRIPTOR_2, i, sizeof(HID_STRING_DESCRIPTOR_2));
    data += sizeof(HID_STRING_DESCRIPTOR_2);
    *p_size += sizeof(HID_STRING_DESCRIPTOR_2);
    break;
  case 3:
    ROM_COPY(data, HID_STRING_DESCRIPTOR_3, i, sizeof(HID_STRING_DESCRIPTOR_3));
    data += sizeof(HID_STRING_DESCRIPTOR_3);
    *p_size += sizeof(HID_STRING_DESCRIPTOR_3);
    break;
  default:
    return 0;
  }
  *pp_data = data;

  return 1;
}

static uint8 get_descriptor(uint8 type, uint8 index, uint8** pp_data, uint8* p_size)
{
  uint8 i;
  uint8 *data;

  if (type == USB_DESCRIPTOR_TYPE_STRING) {
    return get_string_descriptor(index, pp_data, p_size);
  }

  if (index != 0) {
    return 0;
  }
  data = *pp_data;
  
  switch (type) {
  case USB_DESCRIPTOR_TYPE_DEVICE:
    ROM_COPY(data, DEVICE_DESCRIPTOR, i, sizeof(DEVICE_DESCRIPTOR));
    data += sizeof(DEVICE_DESCRIPTOR);
    *p_size += sizeof(DEVICE_DESCRIPTOR);
    break;
  case USB_DESCRIPTOR_TYPE_CONFIGURATION:
    ROM_COPY(data, CONFIG_DESCRIPTOR, i, sizeof(CONFIG_DESCRIPTOR));
    data += sizeof(CONFIG_DESCRIPTOR);
    *p_size += sizeof(CONFIG_DESCRIPTOR);
    get_descriptor(USB_DESCRIPTOR_TYPE_INTERFACE, 0, &data, p_size);
    get_descriptor(USB_DESCRIPTOR_TYPE_HID, 0, &data, p_size);
    get_descriptor(USB_DESCRIPTOR_TYPE_ENDPOINT, 0, &data, p_size);
    break;
  case USB_DESCRIPTOR_TYPE_INTERFACE:
    ROM_COPY(data, INTERFACE_DESCRIPTOR, i, sizeof(INTERFACE_DESCRIPTOR));
    data += sizeof(INTERFACE_DESCRIPTOR);
    *p_size += sizeof(INTERFACE_DESCRIPTOR);
    break;
  case USB_DESCRIPTOR_TYPE_ENDPOINT:
    ROM_COPY(data, ENDPOINT_DESCRIPTOR, i, sizeof(ENDPOINT_DESCRIPTOR));
    data += sizeof(ENDPOINT_DESCRIPTOR);
    *p_size += sizeof(ENDPOINT_DESCRIPTOR);
    break;
  case USB_DESCRIPTOR_TYPE_HID:
    ROM_COPY(data, HID_DESCRIPTOR, i, sizeof(HID_DESCRIPTOR));
    data += sizeof(HID_DESCRIPTOR);
    *p_size += sizeof(HID_DESCRIPTOR);
    break;
  case USB_DESCRIPTOR_TYPE_REPORT:
    ROM_COPY(data, HID_REPORT_DESCRIPTOR, i, sizeof(HID_REPORT_DESCRIPTOR));
    data += sizeof(HID_REPORT_DESCRIPTOR);
    *p_size += sizeof(HID_REPORT_DESCRIPTOR);
    break;
  default:
    return 0;
  }

  *pp_data = data;
  return 1;
}

static void send_output_report_to_host(const uint8* output_report)
{
  UsbHostQueueEntry e;
  e.event = EVENT_OUTPUT_REPORT;
  vos_memcpy(e.data.report, output_report, OUTPUT_REPORT_SIZE);
  enqueue(g_usb_host_queue, &e);
}

static uint8 device_req_handler(const usb_deviceRequest_t* dev_req, uint8 *send_data, uint8 *send_size)
{
  uint8 handled;
  
  handled = 0;
  switch (dev_req->bRequest) {
  case USB_REQUEST_CODE_GET_STATUS:
    send_data[0] = 0; // Bit0: Self-Powered Bit:1 RemoteWakeup
    *send_size = 1;
    handled = 1;
    break;
  case USB_REQUEST_CODE_SET_ADDRESS:
    set_address((uint8)dev_req->wValue);
    handled = 1;
    break;
  case USB_REQUEST_CODE_GET_DESCRIPTOR:
    handled = get_descriptor((uint8)(dev_req->wValue >> 8), // DescriptorType
                             (uint8)(dev_req->wValue),      // Index
                             &send_data, send_size);
    break;
  case USB_REQUEST_CODE_GET_CONFIGURATION:
    send_data[0] = m_configuration_no;
    *send_size = 1;
    handled = 1;
    break;
  case USB_REQUEST_CODE_SET_CONFIGURATION:
    set_configuration((uint8)dev_req->wValue);
    if (m_configuration_no == 1) {
      m_interrupt_transfer_valid = 1;
      handled = 1;
    } else {
      m_interrupt_transfer_valid = 0;
    }
    break;
  default:
    break;
  }
  return handled;

}

static uint8 interface_req_handler(const  usb_deviceRequest_t* dev_req, uint8 *send_data, uint8 *send_size)
{
  uint8 handled;

  handled = 0;
  switch (dev_req->bRequest) {
  case USB_REQUEST_CODE_GET_STATUS:
    send_data[0] = 0;
    send_data[1] = 0;
    *send_size = 2;
    handled = 1;
    break;
  case USB_REQUEST_CODE_GET_DESCRIPTOR:
    handled = get_descriptor((uint8)(dev_req->wValue >> 8), // DescriptorType
                            (uint8)(dev_req->wValue)     , // Index
                            &send_data, send_size);
    break;
  case USB_REQUEST_CODE_GET_INTERFACE:
    send_data[0] = 0;
    *send_size = 1;
    handled = 1;
    break;
  case USB_REQUEST_CODE_SET_INTERFACE:
    if (dev_req->wValue == 0) {
      handled = 1;
    }
    break;
  default:
    break;
  }

  return handled;
}

static uint8 endpoint_req_handler(const usb_deviceRequest_t* dev_req, uint8 *send_data, uint8 *send_size)
{
  uint8 ep_num;
  uint8 handled;

  ep_num = dev_req->wIndex & 0x0F;

  handled = 0;
  switch (dev_req->bRequest) {
  case USB_REQUEST_CODE_GET_STATUS:
    if (is_endpoint_stall(ep_num)) {
      send_data[0] = 1;
    } else {
      send_data[0] = 0;
    }
    send_data[1] = 0;
    *send_size = 2;
    handled = 1;
    break;
  case USB_REQUEST_CODE_CLEAR_FEATURE:
    if (dev_req->wValue == 0) {
      clear_endpoint(ep_num);
      handled = 1;
    }
    break;
  case USB_REQUEST_CODE_SET_FEATURE:
    if (dev_req->wValue == 0) {
      stall_endpoint(ep_num);
      handled = 1;
    }
    break;
  default:
    break;
  }

  return handled;
}

static void notify_recv_command_to_main(void)
{
  MainThreadQueueEntry e;
  e.event = EVENT_RECV_COMMAND;
  enqueue(g_main_thread_queue, &e);
}

static uint8 class_req_handler(const usb_deviceRequest_t* dev_req, uint8 *send_data, uint8 *send_size)
{
  uint8 handled;
  uint8 buf[128] = {0};

  handled = 0;
  switch (dev_req->bRequest) {
  case USB_HID_REQUEST_CODE_GET_REPORT:
    switch (dev_req->wValue >> 8) {
    case USB_HID_REPORT_TYPE_INPUT:
      send_data[0] = 0x01; // ReportID
      vos_memcpy(send_data + 1, m_current_input_report, INPUT_REPORT_SIZE);
      *send_size = 1 + INPUT_REPORT_SIZE;
      handled = 1;
      break;
    case USB_HID_REPORT_TYPE_OUTPUT:
      send_data[0] = 0x01; // RpoertID
      vos_memcpy(send_data + 1, m_current_output_report, OUTPUT_REPORT_SIZE);
      *send_size = 1 + OUTPUT_REPORT_SIZE;
      handled = 1;
      break;
    case USB_HID_REPORT_TYPE_FEATURE:
      send_data[0] = 0x02; // ReportID
      get_send_client_data((HAKADOROUN_TO_CLIENT_DATA*)(send_data + 1));
      *send_size = 1 + sizeof(HAKADOROUN_TO_CLIENT_DATA);
      handled = 1;
      break;
    default:
      break;
    }
    break;
  case USB_HID_REQUEST_CODE_GET_IDLE:
    send_data[0] = m_duration;
    *send_size = 1;
    handled = 1;
    break;
  case USB_HID_REQUEST_CODE_GET_PROTOCOL:
    send_data[0] = m_protocol_no;
    *send_size = 1;
    handled = 1;
    break;
  case USB_HID_REQUEST_CODE_SET_REPORT:
    if (dev_req->wLength > (uint16)sizeof(buf)) {
      return 0;
    }
    control_transfer(m_control_ep_out, buf, (uint8)dev_req->wLength);
    switch (dev_req->wValue >> 8) {
    case USB_HID_REPORT_TYPE_OUTPUT:
      if ((uint8)dev_req->wValue == 0x01) {
        vos_memcpy(m_current_output_report, &buf[1], OUTPUT_REPORT_SIZE);
        send_output_report_to_host(m_current_output_report);
        handled = 1;
      }
      break;
    case USB_HID_REPORT_TYPE_FEATURE:
      if ((uint8)dev_req->wValue == 0x02) {
        set_recv_client_data((CLIENT_TO_HAKADORUN_DATA*)&buf[1]);
        notify_recv_command_to_main();
        handled = 1;
      }
      break;
    default:
      break;
    }
    break;
  case USB_HID_REQUEST_CODE_SET_IDLE:
    m_duration = (uint8)(dev_req->wValue >> 8);
    handled = 1;
    break;    
  case USB_HID_REQUEST_CODE_SET_PROTOCOL:
    m_protocol_no = (uint8)(dev_req->wValue >> 8); 
    handled = 1;
    break;
  default:
    break;
  }
  return handled;
}

static void slave_control_thread(void)
{
  usb_deviceRequest_t *dev_req;
  uint8 setup_data[9];
  uint8 send_data[CONTROL_BUFFER_SIZE];
  uint8 send_size;
  uint8 handled;
  
  dev_req = (usb_deviceRequest_t*)setup_data;

  while (m_slave_usb) {
    if (recv_setup_data(setup_data) != 0) {
      break;
    }

    send_size = 0;
    handled = 0;
    switch (TYPE_IN_BMREQUESTTYPE(dev_req->bmRequestType)) {
    case USB_BMREQUESTTYPE_STANDARD:
      switch (RECIPIENT_IN_BMREQUESTTYPE(dev_req->bmRequestType)) {
      case USB_BMREQUESTTYPE_DEVICE:
        handled = device_req_handler(dev_req, send_data, &send_size);
        break;
      case USB_BMREQUESTTYPE_INTERFACE:
        handled = interface_req_handler(dev_req, send_data, &send_size);
        break;
      case USB_BMREQUESTTYPE_ENDPOINT:
        handled = endpoint_req_handler(dev_req, send_data, &send_size);
        break;
      default:
        break;
      }
      break;
    case USB_BMREQUESTTYPE_CLASS:
      handled = class_req_handler(dev_req, send_data, &send_size);
      break;
    default:
      break;
    }
    if (handled) {
      if ((uint16)send_size > dev_req->wLength) {
        send_size = (uint8)dev_req->wLength;
      }
      control_transfer(m_control_ep_in, send_data, send_size);
    } else {
      debug_print("NOT_HANDLED", setup_data, sizeof(setup_data));
      stall_endpoint(0);
    }
  }
  vos_signal_semaphore(&m_control_thread_end_semaphore);
}

static void connect(void)
{
  usbslave_ioctl_cb_t c;

  if (m_slave_usb) {
    return;
  }

  m_slave_usb = vos_dev_open(VOS_DEV_USB_SLAVE);
  
  c.ioctl_code = VOS_IOCTL_USBSLAVE_CONNECT;
  c.set = NULL;
  vos_dev_ioctl(m_slave_usb, &c);

  m_control_ep_in = get_control_endpoint(USBSLAVE_CONTROL_IN);
  set_max_packet_size(m_control_ep_in, USBSLAVE_MAX_PACKET_SIZE_8);
  
  m_control_ep_out = get_control_endpoint(USBSLAVE_CONTROL_OUT);
  set_max_packet_size(m_control_ep_out, USBSLAVE_MAX_PACKET_SIZE_8);

  m_interrupt_ep_in = get_interrupt_endpoint(USB_SLAVE_INTERRUPT_IN);
  set_max_packet_size(m_interrupt_ep_in, USBSLAVE_MAX_PACKET_SIZE_8);

  vos_init_semaphore(&m_control_thread_end_semaphore, 0);
  vos_create_thread_ex(USB_SLAVE_CONTROL_THREAD_PRIORITY, USB_SLAVE_CONTROL_THREAD_STACK_SIZE, slave_control_thread, "SCT", 0);
}

static void disconnect(void)
{
  VOS_HANDLE usb;
  usbslave_ioctl_cb_t c;

  if (! m_slave_usb) {
    return;
  }
  usb = m_slave_usb;
  m_slave_usb = NULL;
  m_interrupt_transfer_valid = 0;

  c.ioctl_code = VOS_IOCTL_USBSLAVE_DISCONNECT;
  c.set = NULL;
  vos_dev_ioctl(usb, &c);

  vos_wait_semaphore(&m_control_thread_end_semaphore);
  vos_dev_close(usb);
}

static void handle_input_report(const uint8 *report)
{
  uint8 buf[1 + INPUT_REPORT_SIZE]; // +1 ReportID
  vos_memcpy(m_current_input_report, report, INPUT_REPORT_SIZE);
  if (m_interrupt_transfer_valid) {
    buf[0] = 0x01;
    vos_memcpy(buf + 1, m_current_input_report, INPUT_REPORT_SIZE);
    non_control_transfer(m_interrupt_ep_in, buf, sizeof(buf));
  }
}

void usb_slave_thread(void)
{
  UsbSlaveQueueEntry entry;
  while (1) {
    dequeue(g_usb_slave_queue, &entry);
    switch (entry.event) {
    case EVENT_DEVICE_CONNECT:
      connect();
      break;
    case EVENT_DEVICE_DISCONNECT:
      disconnect();
      break;
    case EVENT_INPUT_REPORT:
      handle_input_report(entry.data.report);
      break;
    default:  
      break; // Do Nothing
    }
  }
}
