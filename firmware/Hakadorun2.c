#include "Hakadorun2.h"
#include <string.h>
#include <UART.h>
#include <USBHost.h>
#include <USBHostHID.h>
#include <USBSlave.h>
#include <FAT.h>
#include <BOMS.h>
#include <FirmwareUpdate.h>
#include "KeyTable.h"

typedef enum {
	GET_KEY_OK,
	GET_KEY_EXIT,
	GET_KEY_TIMEOUT
} GET_KEY_RESULTS;
#define GET_INPUT_TIMEOUT_MS          3000
#define GET_INPUT_TIMEOUT_ONE_DELAY   50
#define GET_INPUT_TIMEOUT_RETRY_COUNT (GET_INPUT_TIMEOUT_MS / GET_INPUT_TIMEOUT_ONE_DELAY)

Queue* g_main_thread_queue;
Queue* g_usb_host_queue;
Queue* g_usb_slave_queue;

static HAKADOROUN_TO_CLIENT_DATA m_to_client_data;
static vos_mutex_t m_to_client_data_mutex;
static CLIENT_TO_HAKADORUN_DATA m_from_client_data;
static vos_mutex_t m_from_client_data_mutex;

static uint8 handle_non_input_report_event(const MainThreadQueueEntry *entry);
static void run_macro(uint8 macro_no);

static uint8 is_mode_sw_on(void)
{
	uint8 v;
	vos_gpio_read_pin(GPIO_A_1, &v);
	return v;
}

static void init_uart(void)
{
	common_ioctl_cb_t c;
	VOS_HANDLE h;

	h = vos_dev_open(VOS_DEV_UART);

	c.ioctl_code = VOS_IOCTL_COMMON_ENABLE_DMA;
	c.set.param = DMA_ACQUIRE_AS_REQUIRED;
	vos_dev_ioctl(h, &c);

	c.ioctl_code = VOS_IOCTL_UART_SET_BAUD_RATE;
	c.set.uart_baud_rate = UART_BAUD_115200;
	vos_dev_ioctl(h, &c);

	c.ioctl_code = VOS_IOCTL_UART_SET_FLOW_CONTROL;
	c.set.param = UART_FLOW_NONE;
	vos_dev_ioctl(h, &c);

	c.ioctl_code = VOS_IOCTL_UART_SET_DATA_BITS;
	c.set.param = UART_DATA_BITS_8;
	vos_dev_ioctl(h, &c);

	c.ioctl_code = VOS_IOCTL_UART_SET_STOP_BITS;
	c.set.param = UART_STOP_BITS_1;
	vos_dev_ioctl(h, &c);

	c.ioctl_code = VOS_IOCTL_UART_SET_PARITY;
	c.set.param = UART_PARITY_NONE;
	vos_dev_ioctl(h, &c);

	stdioAttach(h);
}

vos_mutex_t m_debug_print_mutex;
void debug_print(const char* msg, const uint8 *data, uint8 data_size)
{
	uint8 i;
	vos_lock_mutex(&m_debug_print_mutex);
	printf("%s: ", msg);
	for (i = 0; i < data_size; i++) {
		printf("%x ", data[i]);
	}
	printf("\n");
	vos_unlock_mutex(&m_debug_print_mutex);
}

static void send_main_event_to_usb_slave(const MainThreadQueueEntry *main_entry)
{
	UsbSlaveQueueEntry slave_entry;
	slave_entry.event = main_entry->event;
	vos_memcpy(slave_entry.data.report, main_entry->data.report, INPUT_REPORT_SIZE);
	enqueue(g_usb_slave_queue, &slave_entry);
}

static void send_modified_report_to_usb_slave(const uint8* raw_report)
{
	UsbSlaveQueueEntry slave_entry;
	
	slave_entry.event = EVENT_INPUT_REPORT;
	modify_report(slave_entry.data.report, raw_report);
#ifndef RELEASE
	debug_print("B", raw_report, INPUT_REPORT_SIZE);
	debug_print("A", slave_entry.data.report, INPUT_REPORT_SIZE);
#endif
	enqueue(g_usb_slave_queue, &slave_entry);
}

static uint8 get_input_report(uint8 *report)
{
	uint16 retry_count;
	MainThreadQueueEntry entry;
	for (retry_count = 0; retry_count < GET_INPUT_TIMEOUT_RETRY_COUNT; /* retry_count++ */) {
		if (!try_dequeue(g_main_thread_queue, &entry)) {
			vos_delay_msecs(GET_INPUT_TIMEOUT_ONE_DELAY);
			retry_count++;
			continue;
		}
		if (handle_non_input_report_event(&entry)) {
			continue;
		}
		if (entry.event == EVENT_INPUT_REPORT) {
			vos_memcpy(report, entry.data.report, INPUT_REPORT_SIZE);
			return GET_KEY_OK;
		}
	}
	return GET_KEY_TIMEOUT;
}

static uint8 get_single_input_key(void)
{
	uint8 usage_id;
	uint8 report[INPUT_REPORT_SIZE];
	while (get_input_report(report) == GET_KEY_OK) {
		usage_id = extract_one_key_from_report(report);
		if (usage_id != 0) {
			return usage_id;
		}
	}
	return 0;
}

static void get_input_keys(uint8* report)
{
	uint8 current_report[INPUT_REPORT_SIZE];
	uint8 current_pressed_count, max_pressed_count;

	current_pressed_count = 0;
	max_pressed_count = 0;
	while (1) {
		if (get_input_report(current_report) != GET_KEY_OK) {
			continue;
		}
		current_pressed_count = get_pressed_key_count(current_report);

		if (current_pressed_count > max_pressed_count) {
			vos_memcpy(report, current_report, INPUT_REPORT_SIZE);
			max_pressed_count = current_pressed_count;
		}

		if ((current_pressed_count == 0) && (max_pressed_count != 0)) {
			break;
		}
	} 
}

static void init_client_if(void)
{
	vos_memset(&m_to_client_data, 0, sizeof(m_to_client_data));
	vos_init_mutex(&m_to_client_data_mutex, 0);
	vos_init_mutex(&m_from_client_data_mutex, 0);
}

void set_recv_client_data(const CLIENT_TO_HAKADORUN_DATA *data)
{
	vos_lock_mutex(&m_from_client_data_mutex);
	vos_memcpy(&m_from_client_data, data, sizeof(m_from_client_data));
	vos_unlock_mutex(&m_from_client_data_mutex);
}

void get_recv_client_data(CLIENT_TO_HAKADORUN_DATA *data)
{
	vos_lock_mutex(&m_from_client_data_mutex);
	vos_memcpy(data, &m_from_client_data, sizeof(m_from_client_data));
	vos_unlock_mutex(&m_from_client_data_mutex);
}

void set_send_client_data(const HAKADOROUN_TO_CLIENT_DATA *data)
{
	vos_lock_mutex(&m_to_client_data_mutex);
	vos_memcpy(&m_to_client_data, data, sizeof(m_to_client_data));
	vos_unlock_mutex(&m_to_client_data_mutex);
}

void get_send_client_data(HAKADOROUN_TO_CLIENT_DATA *data)
{
	vos_lock_mutex(&m_to_client_data_mutex);
	vos_memcpy(data, &m_to_client_data, sizeof(m_to_client_data));
	vos_unlock_mutex(&m_to_client_data_mutex);
}

static void update_client_message(const CLIENT_TO_HAKADORUN_DATA *recv_data, uint8 status, const char* message)
{
	HAKADOROUN_TO_CLIENT_DATA send_data;
	send_data.command = recv_data->command;
	send_data.seq_no = recv_data->seq_no;
	send_data.status = status;
	strcpy(send_data.message, message);
	set_send_client_data(&send_data);
}

static void handle_get_fw_version_command(const CLIENT_TO_HAKADORUN_DATA *recv_data)
{
	update_client_message(recv_data, COMMAND_COMPLETED, HAKADORUN_FW_VERSION);
}

static void handle_reset_keytable_command(const CLIENT_TO_HAKADORUN_DATA *recv_data)
{
	update_client_message(recv_data, COMMAND_RUNNING, "Resetting...");
	reset_key_table();
	update_client_message(recv_data, COMMAND_COMPLETED, "Reset Compelted");
}

static void handle_update_keytable_command(const CLIENT_TO_HAKADORUN_DATA *recv_data)
{
	uint8 src_key, dst_key;
	update_client_message(recv_data, COMMAND_RUNNING, "Enter the src key");
	
	src_key = get_single_input_key();
	if (src_key == 0) {
		goto abort_exit;
	}

	update_client_message(recv_data, COMMAND_RUNNING, "Enter the dst key");

	dst_key = get_single_input_key();
	if (dst_key == 0) {
		goto abort_exit;
	}

	update_client_message(recv_data, COMMAND_RUNNING, "Writing...");

	update_key_table(src_key, dst_key);

	update_client_message(recv_data, COMMAND_COMPLETED, "Writing completed");
	
	return;
abort_exit:
	update_client_message(recv_data, COMMAND_COMPLETED, "Aborted");
}

static void handle_set_lang_convert_mode(const CLIENT_TO_HAKADORUN_DATA *recv_data)
{
	update_client_message(recv_data, COMMAND_RUNNING, "Setting...");
	set_lang_convert_mode(recv_data->data.lang_convert.mode);
	update_client_message(recv_data, COMMAND_COMPLETED, "Setting Completed");
}

static uint8 record_key_sequence(uint8* key_data)
{
	uint8 count;
	uint8 prev_report_is_zero;
	uint8 get_report_result;
	uint8 report[INPUT_REPORT_SIZE];
	uint8* dst;

	count = 0;
	prev_report_is_zero = 1;
	dst = key_data;
	while (1) {
		get_report_result = get_input_report(report);
		if ((get_report_result == GET_KEY_TIMEOUT) && prev_report_is_zero) {
			break;
		}
		prev_report_is_zero = is_report_zero(report);

		dst[0] = report[0];
		vos_memcpy(dst + 1, report + 2, MACRO_ONE_KEY_SIZE - 1);
		dst += MACRO_ONE_KEY_SIZE;
		count++;
		if (count >= MACRO_MAX_KEY_COUNT) {
			break;
		}
	}

	return count;
}

static void handle_set_macro(const CLIENT_TO_HAKADORUN_DATA *recv_data)
{
	uint8 macro_no;
	MACRO_DATA *data;
	uint8 report_to_run_macro[INPUT_REPORT_SIZE];

	macro_no = recv_data->data.macro.no;
	data = get_macro_data(macro_no);
	if (data == NULL) {
		update_client_message(recv_data, COMMAND_COMPLETED, "internal error");
		return;
	}

	update_client_message(recv_data, COMMAND_RUNNING, "Enter keys to run macro...");
	get_input_keys(report_to_run_macro);
	
	update_client_message(recv_data, COMMAND_RUNNING, "Enter key sequence for macro");
	data->count = record_key_sequence(data->keys);

	update_client_message(recv_data, COMMAND_RUNNING, "Setting...");
	set_macro_run_key(macro_no, report_to_run_macro);
	update_macro_data(macro_no, data);
	update_client_message(recv_data, COMMAND_COMPLETED, "Setting Completed");
exit_func:
	vos_free(data);
}

static void command_handler(void)
{
	CLIENT_TO_HAKADORUN_DATA recv_data;
	get_recv_client_data(&recv_data);
	switch (recv_data.command) {
	case COMMAND_GET_FW_VERSION:
		handle_get_fw_version_command(&recv_data);
		break;
	case COMMAND_RESET_KEYTABLE:
		handle_reset_keytable_command(&recv_data);
		break;
	case COMMAND_UPDATE_KEYTABLE:
		handle_update_keytable_command(&recv_data);
		break;
	case COMMAND_SET_LANG_CONVERT_MODE:
		handle_set_lang_convert_mode(&recv_data);
		break;
	case COMMAND_SET_MACRO:
		handle_set_macro(&recv_data);
		break;
	default:
		break;
	}
}

static uint8 handle_non_input_report_event(const MainThreadQueueEntry *entry)
{
	switch (entry->event) {
	case EVENT_DEVICE_CONNECT:
	case EVENT_DEVICE_DISCONNECT:
		send_main_event_to_usb_slave(entry);
		return 1;
	case EVENT_RECV_COMMAND:
		command_handler();
		return 1;
	}
	return 0;
}

static void handle_input_report_event(const MainThreadQueueEntry *entry)
{
	uint8 macro_no;

	macro_no = get_macro_no_from_report(entry->data.report);
	if (macro_no != 0xFF) {
		run_macro(macro_no);
	} else {
		send_modified_report_to_usb_slave(entry->data.report);
	}				
}

static void wait_for_all_key_unpressed(void)
{
	MainThreadQueueEntry e;
	while (1) {
		dequeue(g_main_thread_queue, &e);
		if (handle_non_input_report_event(&e)) {
			continue;
		}
		if ((e.event == EVENT_INPUT_REPORT) && is_report_zero(e.data.report)) {
			return;
		}
	}
}

static void run_macro(uint8 macro_no)
{
	MACRO_DATA *d;
	uint8 report[INPUT_REPORT_SIZE];
	const uint8* key_data;
	uint8 remain_count;

	printf("run_macro %d\n", macro_no);

	d = get_macro_data(macro_no);
	if (d == NULL) {
		return;
	}
	if (! is_macro_data_valid(d)) {
		goto exit_run_macro;
	}

	wait_for_all_key_unpressed();

	key_data = d->keys;
	remain_count = d->count;
	while (remain_count) {

		report[0] = key_data[0];
		report[1] = 0;
		vos_memcpy(report + 2, key_data + 1, MACRO_ONE_KEY_SIZE - 1);
		send_modified_report_to_usb_slave(report);
		
		key_data += MACRO_ONE_KEY_SIZE;
		remain_count--;
	}
	if (! is_report_zero(report)) {
		vos_memset(report, 0, INPUT_REPORT_SIZE);
		send_modified_report_to_usb_slave(report);
	}

exit_run_macro:
	vos_free(d);
}

void main_thread(void)
{
	MainThreadQueueEntry main_entry;

	init_uart();
	printf("Version:");
	printf(HAKADORUN_FW_VERSION);

	vos_init_mutex(&m_debug_print_mutex, 0);
	
	if (is_mode_sw_on()) {
		fw_update_main();
		// Never retrun
	}

	init_client_if();

	load_key_config();
	load_key_table();

	g_main_thread_queue = create_queue(MAIN_THREAD_QUEUE_COUNT, sizeof(MainThreadQueueEntry));
	g_usb_host_queue = create_queue(USB_HOST_QUEUE_COUNT, sizeof(UsbHostQueueEntry));
	g_usb_slave_queue = create_queue(USB_SLAVE_QUEUE_COUNT, sizeof(UsbSlaveQueueEntry));

	vos_create_thread_ex(USB_HOST_THREAD_PRIORITY, USB_HOST_THREAD_STACK_SIZE, usb_host_thread, "Host", 0);
	vos_create_thread_ex(USB_SLAVE_THREAD_PRIORITY, USB_SLAVE_THREAD_STACK_SIZE, usb_slave_thread, "Slave", 0);

	while (1) {
		dequeue(g_main_thread_queue, &main_entry);
		if (handle_non_input_report_event(&main_entry)) {
			continue;
		}
		if (main_entry.event == EVENT_INPUT_REPORT) {
			handle_input_report_event(&main_entry);
		}
	}
}

static void setup_io_mux(void)
{
	// DEBUG In/Out 11pin
	vos_iomux_define_bidi(11, IOMUX_IN_DEBUGGER, IOMUX_OUT_DEBUGGER);

	// MODE_SW 12pin
	vos_iocell_set_config(12, VOS_IOCELL_DRIVE_CURRENT_4MA, VOS_IOCELL_TRIGGER_NORMAL, VOS_IOCELL_SLEW_RATE_FAST, VOS_IOCELL_PULL_UP_75K);
	vos_iomux_define_input(12, IOMUX_IN_GPIO_PORT_A_1);

	// UART TX (For Debug) 23pin
	vos_iomux_define_output(23, IOMUX_OUT_UART_TXD);
}

int b_memcmp(const void *a, const void *b, int size)
{
	int i;
	uint8 *p, *q;
	p = a;
	q = b;
	for (i = 0; i < size; i++) {
		if (*p++ != *q++) {
			return 1;
		}
	}
	return 0;
}

void main(void)
{
	vos_init(VOS_QUANTUM, VOS_TICK_INTERVAL, VOS_DEV_MAX);
	vos_set_clock_frequency(VOS_48MHZ_CLOCK_FREQUENCY);
	vos_set_idle_thread_tcb_size(512);
	setup_io_mux();

	uart_init(VOS_DEV_UART, NULL);

	usbhost_init(VOS_DEV_USB_HOST, -1, NULL);
	usbHostHID_init(VOS_DEV_USB_HOST_HID);

	usbslave_init(USBSLAVE_PORT_B, VOS_DEV_USB_SLAVE);

	fatdrv_init(VOS_DEV_FAT_FILE_SYSTEM);
	boms_init(VOS_DEV_BOMS);

	vos_create_thread_ex(MAIN_THREAD_PRIORITY, MAIN_THREAD_STACK_SIZE, main_thread, "Main", 0);

	vos_start_scheduler();
}
