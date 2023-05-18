#include "Hakadorun2.h"
#include <USB.h>
#include <USBHost.h>
#include <USBHID.h>
#include <USBHostHID.h>
#include <string.h>
#include "Queue.h"

static void wait_for_device_connection(VOS_HANDLE hostUsb)
{
    uint8 state;
    usbhost_ioctl_cb_t c;
    c.ioctl_code = VOS_IOCTL_USBHOST_GET_CONNECT_STATE;
    c.get = &state;
    while (1) {
        vos_dev_ioctl(hostUsb, &c);
        if (state == PORT_STATE_ENUMERATED) {
            break;
        }
        vos_delay_msecs(100);
    }
}

static VOS_HANDLE attach_hid_keyboard(VOS_HANDLE hostUsb)
{
    usbhost_ioctl_cb_t host_ctrl;
    usbhost_ioctl_cb_class_t host_class;
    usbhost_device_handle_ex interface_handle;
    VOS_HANDLE hid;
    usbHostHID_ioctl_t hid_ctrl;
    usbHostHID_ioctl_cb_attach_t attach_param;

    host_ctrl.ioctl_code = VOS_IOCTL_USBHOST_DEVICE_FIND_HANDLE_BY_CLASS;
    host_ctrl.handle.dif = 0;
    host_ctrl.set = &host_class;
        host_class.dev_class = USB_CLASS_HID;
        host_class.dev_subclass = USB_SUBCLASS_HID_BOOT_INTERFACE;
        host_class.dev_protocol = USB_PROTOCOL_HID_KEYBOARD;
    host_ctrl.get = &interface_handle;
    if (vos_dev_ioctl(hostUsb, &host_ctrl) != USBHOST_OK) {
        return NULL;
    }

    hid = vos_dev_open(VOS_DEV_USB_HOST_HID);
    
    hid_ctrl.ioctl_code = VOS_IOCTL_USBHOSTHID_ATTACH;
    hid_ctrl.set.att = &attach_param;
        attach_param.hc_handle = hostUsb;
        attach_param.ifDev = interface_handle;
    hid_ctrl.get.data = NULL;
    if (vos_dev_ioctl(hid, &hid_ctrl) != USBHOSTHID_OK) {
        vos_dev_close(hid);
        return NULL;
    }

    return hid;
}

static void detach_hid(VOS_HANDLE hid)
{
    usbHostHID_ioctl_t c;
    c.ioctl_code = VOS_IOCTL_USBHOSTHID_DETACH;
    vos_dev_ioctl(hid, &c);
}

static void set_idle(VOS_HANDLE hid, uint8 duration)
{
    usbHostHID_ioctl_t c;
    c.ioctl_code = VOS_IOCTL_USBHOSTHID_SET_IDLE;
    c.reportID = USB_HID_REPORT_ID_ZERO;
    c.idleDuration = duration;
    vos_dev_ioctl(hid, &c);
}

static void output_report(VOS_HANDLE hid, uint8 report)
{
    usbHostHID_ioctl_t c;
    c.reportType = USB_HID_REPORT_TYPE_OUTPUT;
    c.reportID = USB_HID_REPORT_ID_ZERO;
    c.Length = 1;
    c.set.data = &report;
    c.ioctl_code = VOS_IOCTL_USBHOSTHID_SET_REPORT;
    vos_dev_ioctl(hid, &c);
}

static void send_input_report_to_main(const uint8* report)
{
    MainThreadQueueEntry entry;
    entry.event = EVENT_INPUT_REPORT;
    memcpy(entry.data.report, report, INPUT_REPORT_SIZE);
    enqueue(g_main_thread_queue, &entry);
}

static void handle_host_queue(VOS_HANDLE hid)
{
    UsbHostQueueEntry queue_entry;
    if (! try_dequeue(g_usb_host_queue, &queue_entry)) {
        return;
    }
    switch (queue_entry.event) {
    case EVENT_OUTPUT_REPORT:
        output_report(hid, queue_entry.data.report[0]);
        break;
    default:
        break; // Do Nothing
    }
}

static void notify_connect(void)
{
    MainThreadQueueEntry e;
    e.event = EVENT_DEVICE_CONNECT;
    enqueue(g_main_thread_queue, &e);
}

static void notify_disconnect(void)
{
    MainThreadQueueEntry e;
    e.event = EVENT_DEVICE_DISCONNECT;
    enqueue(g_main_thread_queue, &e);
}

void usb_host_main(VOS_HANDLE hid)
{   
    int first, report_index;
    uint8 input_report[2][INPUT_REPORT_SIZE];

    set_idle(hid, 50);
    
    first = 1;
    report_index = 0;
    while (1) {
        if (vos_dev_read(hid, &input_report[report_index][0], INPUT_REPORT_SIZE, NULL) != USBHOSTHID_OK) {
            return;
        }
        if (first || (b_memcmp(&input_report[0][0], &input_report[1][0], INPUT_REPORT_SIZE) != 0)) {
            send_input_report_to_main(&input_report[report_index][0]);
            report_index ^= 0x01;
        }
        first = 0;
        handle_host_queue(hid);
    }
}

void usb_host_thread(void)
{
    VOS_HANDLE hostUsb, hid;

   
    while (1) {
        hostUsb = vos_dev_open(VOS_DEV_USB_HOST);
        
        wait_for_device_connection(hostUsb);

        hid = attach_hid_keyboard(hostUsb);
        if (hid != NULL) {
            notify_connect();

            usb_host_main(hid);
            
            notify_disconnect();
            detach_hid(hid);
            vos_dev_close(hid);
        }
        vos_dev_close(hostUsb);
    }
}
