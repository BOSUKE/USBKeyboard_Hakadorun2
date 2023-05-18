#include "Hakadorun2.h"
#include <USB.h>
#include <USBHost.h>
#include <FAT.h>
#include <BOMS.h>
#include <MSI.h>
#include <FirmwareUpdate.h>

extern rom uint8 userDataArea[16];

#define REFLASHER_ADRESS  0x1F740

static void wait_usb_connected(VOS_HANDLE usb)
{
    usbhost_ioctl_cb_t c;
    uint8 state = PORT_STATE_DISCONNECTED;
    do {
        vos_delay_msecs(1);
        c.ioctl_code = VOS_IOCTL_USBHOST_GET_CONNECT_STATE;
        c.get = &state;
        vos_dev_ioctl(usb, &c);
    } while (state != PORT_STATE_ENUMERATED);
}

static VOS_HANDLE attach_boms(VOS_HANDLE usb)
{
    usbhost_ioctl_cb_class_t ctrl_class;
    usbhost_ioctl_cb_t ctrl;
    usbhost_device_handle_ex if_disk;

    boms_ioctl_cb_attach_t boms_att;
    msi_ioctl_cb_t boms_ctrl;
    VOS_HANDLE boms;

    ctrl_class.dev_class = USB_CLASS_MASS_STORAGE;
    ctrl_class.dev_subclass = USB_SUBCLASS_MASS_STORAGE_SCSI;
    ctrl_class.dev_protocol = USB_PROTOCOL_MASS_STORAGE_BOMS;
    ctrl.ioctl_code = VOS_IOCTL_USBHOST_DEVICE_FIND_HANDLE_BY_CLASS;
    ctrl.handle.dif = NULL;
    ctrl.set = &ctrl_class;
    ctrl.get = &if_disk;
    if (vos_dev_ioctl(usb, &ctrl) != USBHOST_OK) {
        return NULL;
    }

    boms = vos_dev_open(VOS_DEV_BOMS);

    boms_att.hc_handle = usb;
    boms_att.ifDev = if_disk;
    boms_ctrl.ioctl_code = MSI_IOCTL_BOMS_ATTACH;
    boms_ctrl.set = &boms_att;
    boms_ctrl.get = NULL;
    if (vos_dev_ioctl(boms, &boms_ctrl) != MSI_OK) {
        vos_dev_close(boms);
        return NULL;
    }
    return boms;
}

static void detach_boms(VOS_HANDLE boms)
{
    msi_ioctl_cb_t c;
    c.ioctl_code = MSI_IOCTL_BOMS_DETACH;
    vos_dev_ioctl(boms, &c);
    vos_dev_close(boms);
}

static VOS_HANDLE attach_fat(VOS_HANDLE boms)
{
    VOS_HANDLE fat;
    fatdrv_ioctl_cb_attach_t attr;
    fat_ioctl_cb_t c;
    unsigned char attach_result;

    fat = vos_dev_open(VOS_DEV_FAT_FILE_SYSTEM);

    attr.msi_handle = boms;
    attr.partition = 0;
    c.ioctl_code = FAT_IOCTL_FS_ATTACH;
    c.set = &attr;
    attach_result = vos_dev_ioctl(fat, &c);
    if (attach_result != FAT_OK) {
        vos_dev_close(fat);
        return NULL;
    }
    return fat;
}

static void detach_fat(VOS_HANDLE fat)
{
    fat_ioctl_cb_t c;
    c.ioctl_code = FAT_IOCTL_FS_DETACH;
    vos_dev_ioctl(fat, &c);
    vos_dev_close(fat);
}

static int8 compare_rom_and_file_version(FILE *fp)
{
    int fresult;
    unsigned int ver_file, ver_rom;
    
    fresult = fseek(fp, 0x30, SEEK_SET);
    if (fresult != 0) {
        return -1;
    }
    fresult = fread(&ver_file, 4, 1, fp);
    fseek(fp, 0, SEEK_SET);
    if (fresult != 4) {
        return -1;
    }

    ver_rom = userDataArea[1];
    ver_rom |= (unsigned int)userDataArea[0] << 8;
    ver_rom |= (unsigned int)userDataArea[3] << 16;
    ver_rom |= (unsigned int)userDataArea[2] << 24;

    printf("ver_file %x\n", ver_file);
    printf("rom_file %x\n", ver_rom);

    if (ver_file == ver_rom) {
        return 1;
    }
    return 0;
}

 void fw_update_main(void)
 {
    VOS_HANDLE usb, boms, fat;
    FILE *fp;
    unsigned char update_result;

    usb = vos_dev_open(VOS_DEV_USB_HOST);
    while (1) {
        printf("wait_usb_connected\n");
        wait_usb_connected(usb);

        boms = attach_boms(usb);
        if (boms == NULL) {
            printf("attach boms failed\n");
            vos_delay_msecs(3000);
            continue;
        }

        fat = attach_fat(boms);
        if (fat == NULL) {
            printf("attach fat failed\n");
            detach_boms(boms);
            vos_delay_msecs(3000);
            continue;
        }

        fsAttach(fat);
        fp = fopen("fw.rom", "r");
        if (fp != NULL) {

            if (compare_rom_and_file_version(fp) == 0) {
                printf("Start firmware update\n");
                update_result = FirmwareUpdateFATFileFeedback(fp, REFLASHER_ADRESS, FIRMWARE_UPDATE_FEEDBACK_UART);
                printf("result %d\n", update_result);
                fclose(fp);
            } else {
                printf("Version same\n");
            }
        }

        detach_fat(fat);
        detach_boms(boms);

        vos_delay_msecs(3000);
    }
 }