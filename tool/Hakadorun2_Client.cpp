#include <iostream>
#include <functional>
#include <vector>
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <format>
#include <cwchar>
#include <chrono>
#include <thread>
#include "../firmware/ClientIF.h"

#pragma comment(lib, "Hid.lib")
#pragma comment(lib, "SetupAPI.lib")

constexpr USHORT HAKADORUN_VID = 0x16C0;
constexpr USHORT HAKADORUN_PID = 0x27DB;
constexpr WCHAR HAKADORUN_SERIAL_NUM[] = L"d-rissoku.net:0002";

HANDLE enumurate_hid(std::function<bool(HANDLE)> callback)
{
    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);
    HDEVINFO dev_info_list = SetupDiGetClassDevs(&hid_guid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
    if (dev_info_list == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    SP_DEVICE_INTERFACE_DATA dev_interface_data = { sizeof(SP_DEVICE_INTERFACE_DATA) };
    HANDLE return_handle = INVALID_HANDLE_VALUE;
    for (DWORD interfaceIndex = 0; ; interfaceIndex++) {

        if (!SetupDiEnumDeviceInterfaces(dev_info_list, NULL, &hid_guid, interfaceIndex, &dev_interface_data)) {
            break;
        }

        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetail(dev_info_list, &dev_interface_data, NULL, 0, &required_size, NULL);

        std::vector<uint8_t> device_detail_buffer(required_size);
        auto device_detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(device_detail_buffer.data());
        device_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        SetupDiGetDeviceInterfaceDetail(dev_info_list, &dev_interface_data, device_detail, required_size, &required_size, NULL);

        HANDLE handle = CreateFile(device_detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (handle == INVALID_HANDLE_VALUE) {
            continue;
        }
        if (callback(handle)) {
          return_handle = handle;
          break;
        }
        CloseHandle(handle);
    }
    SetupDiDestroyDeviceInfoList(dev_info_list);
    return return_handle;
}

HANDLE find_hakadorun(void)
{
  return enumurate_hid([](HANDLE h) {
    
    HIDD_ATTRIBUTES attr = { sizeof(HIDD_ATTRIBUTES) };
    HidD_GetAttributes(h, &attr);
    if ((attr.VendorID != HAKADORUN_VID) || (attr.ProductID != HAKADORUN_PID)) {
      return false;
    }

    WCHAR serial_number[256] = { 0 };
    HidD_GetSerialNumberString(h, serial_number, sizeof(serial_number));
    
    return wcscmp(serial_number, HAKADORUN_SERIAL_NUM) == 0;

   });
}

bool send_to_hakadorun(HANDLE hid_handle, const CLIENT_TO_HAKADORUN_DATA* data)
{
  uint8 buf[sizeof(CLIENT_TO_HAKADORUN_DATA) + 1] = {}; 
  buf[0] = 0x02; // ReportID
  memcpy(&buf[1], data, sizeof(*data));
  
  return HidD_SetFeature(hid_handle, buf, sizeof(buf));
}

bool recv_from_hakadorun(HANDLE hid_handle, HAKADOROUN_TO_CLIENT_DATA* data)
{
  uint8 buf[sizeof(HAKADOROUN_TO_CLIENT_DATA) + 1] = {};
  buf[0] = 0x02; // ReportID
  BOOL result = HidD_GetFeature(hid_handle, buf, sizeof(buf));
  if (result) {
    memcpy(data, &buf[1], sizeof(*data));
  }
  return result;
}

void do_command(HANDLE hid_handle, CLIENT_TO_HAKADORUN_DATA &command)
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

void do_command(HANDLE hid_handle, const std::string command)
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
      std::cout << std::format("Usage: {} command", argv[0]) << std::endl;
      std::cout << "command: version | update | reset | lang_ja_en | lang_en_ja | lang_none | macroX" << std::endl;
      return 0;
    }
    const std::string command = argv[1];

    HANDLE hid_handle = find_hakadorun();
    if (hid_handle == INVALID_HANDLE_VALUE) {
      std::cout << "Hakadorun Not Found" << std::endl;
      return -1;
    }
    
    do_command(hid_handle, command);

    CloseHandle(hid_handle);
    return 0;
}
