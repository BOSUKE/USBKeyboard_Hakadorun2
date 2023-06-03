#ifndef HAKADORUN2_H_
#define HAKADORUN2_H_
#include <vos.h>
#include <stdio.h>
#include "Queue.h"
#include "ClientIF.h"

#define HAKADORUN_FW_VERSION    "0.0.2"

enum VOS_DEV_ENUM {
    VOS_DEV_UART,
    VOS_DEV_USB_HOST,
    VOS_DEV_USB_HOST_HID,
    VOS_DEV_USB_SLAVE,
    VOS_DEV_FAT_FILE_SYSTEM,
    VOS_DEV_BOMS,
    VOS_DEV_MAX
};

enum HAKA_EVENT_ENUM {
    EVENT_DEVICE_CONNECT,
    EVENT_DEVICE_DISCONNECT,
    EVENT_INPUT_REPORT,
    EVENT_OUTPUT_REPORT,
    EVENT_RECV_COMMAND,
};


#define INPUT_REPORT_SIZE  8
#define OUTPUT_REPORT_SIZE 1
#define FEATURE_REPORT_SIZE 64

#define REPORT_DESCRIPTOR_MAX_SIZE 255

#define MAIN_THREAD_PRIORITY     20
#define USB_HOST_THREAD_PRIORITY 21
#define USB_SLAVE_THREAD_PRIORITY 22
#define USB_SLAVE_CONTROL_THREAD_PRIORITY 23

#define MAIN_THREAD_STACK_SIZE               1024
#define USB_HOST_THREAD_STACK_SIZE           2048
#define USB_SLAVE_THREAD_STACK_SIZE          1024
#define USB_SLAVE_CONTROL_THREAD_STACK_SIZE  2048

typedef struct MainThreadQueueEntry_ {
    uint8 event;
    union {
        uint8 report[INPUT_REPORT_SIZE];
    } data;
} MainThreadQueueEntry;
#define MAIN_THREAD_QUEUE_COUNT 8
extern Queue* g_main_thread_queue;

typedef struct UsbHostQueueEntry_ {
    uint8 event;
    union {
        uint8 report[OUTPUT_REPORT_SIZE];
    } data;
} UsbHostQueueEntry;
#define USB_HOST_QUEUE_COUNT 8
extern Queue* g_usb_host_queue;

typedef struct UsbSlaveQueueEntry_ {
    uint8 event;
    union {
        uint8 report[INPUT_REPORT_SIZE];
    } data;
} UsbSlaveQueueEntry;
#define USB_SLAVE_QUEUE_COUNT 8
extern Queue* g_usb_slave_queue;

extern void set_recv_client_data(const CLIENT_TO_HAKADORUN_DATA *data);
extern void get_recv_client_data(CLIENT_TO_HAKADORUN_DATA *data);
extern void set_send_client_data(const HAKADOROUN_TO_CLIENT_DATA *data);
extern void get_send_client_data(HAKADOROUN_TO_CLIENT_DATA *data);

extern void usb_host_thread(void);
extern void usb_slave_thread(void);

// @ FwUpdate.c
extern void fw_update_main(void);

// Util functions @ Hakadorun2.c
int b_memcmp(const void *a, const void *b, int size); // memcmp is not supported.
void debug_print(const char* msg, const uint8 *data, uint8 data_size);

#endif
