#ifndef CLIENT_IF_H_
#define CLIENT_IF_H_
#ifdef _MSC_VER
#include <stdint.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
#pragma pack(push, 1)
#endif

typedef enum LANG_CONVERT_MODE_ {
    LANG_CONVERT_NONE,
    LANG_CONVERT_JA_TO_EN,
    LANG_CONVERT_EN_TO_JA
} LANG_CONVERT_MODE;

typedef struct CLIENT_TO_HAKADORUN_DATA_ {
    uint8 command;
    uint8 seq_no;
    union {
        struct {
            uint8 mode;
        } lang_convert;
        struct {
            uint8 no;
        } macro;
        uint8 raw[62];
    } data;
} CLIENT_TO_HAKADORUN_DATA;

typedef struct HAKADOROUN_TO_CLIENT_DATA_ {
    uint8 command;
    uint8 seq_no;
    #define COMMAND_NOT_COMPLETE 0x00
    #define COMMAND_RUNNING      0x01
    #define COMMAND_COMPLETED    0x02
    uint8 status;
    uint8 reserved;
    uint8 message[60];
} HAKADOROUN_TO_CLIENT_DATA;

typedef enum HAKADORUN_COMMAND_ {
    COMMAND_NOP,
    COMMAND_GET_FW_VERSION,
    COMMAND_RESET_KEYTABLE,
    COMMAND_UPDATE_KEYTABLE,
    COMMAND_SET_LANG_CONVERT_MODE,
    COMMAND_SET_MACRO
} HAKADORUN_COMMAND;

#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
