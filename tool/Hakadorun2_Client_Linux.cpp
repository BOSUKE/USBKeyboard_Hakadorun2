#include <iostream>
#include <functional>
#include <vector>

#include <fcntl.h>
#include <unistd.h>
#include <linux/hidraw.h>
#include <hidapi/hidapi.h>

//#include <format>
#include <cwchar>
#include <cstring>
#include <chrono>
#include <thread>
#include "../firmware/ClientIF.h"

constexpr USHORT HAKADORUN_VID = 0x16C0;
constexpr USHORT HAKADORUN_PID = 0x27DB;
constexpr WCHAR HAKADORUN_SERIAL_NUM[] = L"d-rissoku.net:0002";

HID_API_EXPORT hid_device * HID_API_CALL enumurate_hid(std::function<bool(hid_device_info*)> callback)
{

  hid_device_info* dev_info_list = hid_enumerate(0x0, 0x0);
  if (dev_info_list == NULL) {
      return NULL;
  }

  hid_device_info* current_dev = dev_info_list;
		hid_device *return_handle = nullptr;

  while (current_dev) {
    printf("Vendor ID: 0x%04x %ls\n", current_dev->vendor_id, current_dev->serial_number);
				if (callback(current_dev)) {
      printf("Open Vendor ID: 0x%04x %s\n", current_dev->vendor_id, current_dev->path);
					return_handle = hid_open_path(current_dev->path);
      break;
    }

    current_dev = current_dev->next;
  }

  hid_free_enumeration(dev_info_list);
		return return_handle;
}

HID_API_EXPORT hid_device * HID_API_CALL find_hakadorun(void)
{
  return enumurate_hid([](hid_device_info* h) {
    
    if ((h->vendor_id != HAKADORUN_VID) || (h->product_id != HAKADORUN_PID)) {
      return false;
    }

    if (h->serial_number != nullptr)
      return wcscmp(h->serial_number, HAKADORUN_SERIAL_NUM) == 0;

    return false;

   });
}

bool send_to_hakadorun(hid_device *hid_handle, const CLIENT_TO_HAKADORUN_DATA* data)
{
  uint8 buf[sizeof(CLIENT_TO_HAKADORUN_DATA) + 1] = {}; 
  buf[0] = 0x02; // ReportID
  memcpy(&buf[1], data, sizeof(*data));
  
  int result = hid_send_feature_report(hid_handle, buf, sizeof(buf));
  return result >= 0;
}

bool recv_from_hakadorun(hid_device *hid_handle, HAKADOROUN_TO_CLIENT_DATA* data)
{
  uint8 buf[sizeof(HAKADOROUN_TO_CLIENT_DATA) + 1] = {};
  buf[0] = 0x02; // ReportID
  int result = hid_get_feature_report(hid_handle, buf, sizeof(buf));
  if (result >= 0) {
    memcpy(data, &buf[1], sizeof(*data));
  }
  return result >= 0;
}

void do_command(hid_device *hid_handle, CLIENT_TO_HAKADORUN_DATA &command)
{
  HAKADOROUN_TO_CLIENT_DATA recv_data;
  if (!recv_from_hakadorun(hid_handle, &recv_data)) {
    std::cerr << "recv_from_hakadorun failed" << std::endl;
    return;
  }

  CLIENT_TO_HAKADORUN_DATA send_data = command;
  send_data.seq_no = recv_data.seq_no + 1;
  if (!send_to_hakadorun(hid_handle, &send_data)) {
    std::cerr << "send_to_hakadorun failed" << std::endl;
    return;
  }

  std::string prev_message;
  while (1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (!recv_from_hakadorun(hid_handle, &recv_data)) {
      std::cerr << "recv_from_hakadorun failed" << std::endl;
      return;
    }
    if ((recv_data.command != send_data.command) || (recv_data.seq_no != send_data.seq_no) || (recv_data.status == COMMAND_NOT_COMPLETE)) {
      continue;
    }
    std::string message = reinterpret_cast<char*>(recv_data.message);
    if (message != prev_message) {
      std::cout << message << std::endl;
       prev_message = message;
    }
    if (recv_data.status == COMMAND_COMPLETED) {
      break;
    }
  }
}

void do_command(hid_device *hid_handle, const std::string command)
{
  CLIENT_TO_HAKADORUN_DATA cmd;
  cmd.command = COMMAND_NOP;
  if (command == "version") {
    cmd.command = COMMAND_GET_FW_VERSION;
  } else if (command == "update") {
    cmd.command = COMMAND_UPDATE_KEYTABLE;
  } else if (command == "reset") {
    cmd.command = COMMAND_RESET_KEYTABLE;
  } else if (command == "lang_ja_en") {
    cmd.command = COMMAND_SET_LANG_CONVERT_MODE;
    cmd.data.lang_convert.mode = LANG_CONVERT_JA_TO_EN;
  } else if (command == "lang_en_ja") {
    cmd.command = COMMAND_SET_LANG_CONVERT_MODE;
    cmd.data.lang_convert.mode = LANG_CONVERT_EN_TO_JA;
  } else if (command == "lang_none") {
    cmd.command = COMMAND_SET_LANG_CONVERT_MODE;
    cmd.data.lang_convert.mode = LANG_CONVERT_NONE;
  } else if (command.substr(0, 5) == "macro" && command.size() == 6) {
    int no = command[5] - '0';
    if (no < 4) {
      cmd.command = COMMAND_SET_MACRO;
      cmd.data.macro.no = (uint8)no;
    }
  }

  if (cmd.command != COMMAND_NOP) {
    do_command(hid_handle, cmd);
  } else {
    std::cout << "invalid command" << std::endl;
  }
}


int main(int argc, char* argv[])
{
    if (argc != 2) {
      //std::cout << std::format("Usage: {} command", argv[0]) << std::endl;
      std::cout << "Usage: " << argv[0] << " command" << std::endl; // FIXME: C++20
      std::cout << "command: version | update | reset | lang_ja_en | lang_en_ja | lang_none | macroX" << std::endl;
      return 0;
    }
    const std::string command = argv[1];

    hid_device *hid_handle = find_hakadorun();

    if (hid_handle == nullptr) {
      std::cerr << "Hakadorun Not Found" << std::endl;
      return -1;
    }

    do_command(hid_handle, command);

    if (hid_handle != nullptr) {
        hid_close(hid_handle);
    }
    return 0;
}
