#ifndef KEYTABLE_H_
#define KEYTABLE_H_

typedef enum {
  LEFT_CTRL_KEY,
  LEFT_SHIFT_KEY,
  LEFT_ALT_KEY,
  LEFT_WIN_KEY,
  RIGHT_CTRL_KEY,
  RIGHT_SHIFT_KEY,
  RIGHT_ALT_KEY,
  RIGHT_WIN_KEY,
  MODIFIER_KEY_MAX
} MODIFIER_KEY_ENUM;
#define MODIFIRE_KEY_MIN LEFT_CTRL_KEY
#define LEFT_SHIFT_KEY_BIT  (1 << LEFT_SHIFT_KEY)
#define RIGHT_SHIFT_KEY_BIT (1 << RIGHT_SHIFT_KEY)
#define SHIFT_KEY_BITMAP (LEFT_SHIFT_KEY_BIT | RIGHT_SHIFT_KEY_BIT)

// UsageId 0 : 押されてない
// UsageId 1 : Error Roll Over
// UsageId 2 : POST Fail
// UsageId 3 : Error Undefined
// UsageId 4～E7h : 各キー
// UaageId E8h～ : Reserved (ここにModifierKeyを割り当てる)
#define VALID_USAGE_ID_MIN           0x04
#define VALID_USAGE_ID_MAX           0xE7
#define IS_THIS_USAGE_ID_MODIFIER(usage_id)          ((usage_id) > VALID_USAGE_ID_MAX)
#define PSEUDO_USAGE_ID_TO_MODIFIRE_KEY(usage_id)    ((usage_id) - VALID_USAGE_ID_MAX - 1)
#define MODIFIRE_KEY_TO_PSEUDO_USAGE_ID(key)         ((key) + VALID_USAGE_ID_MAX + 1)

// KeyTable[] の構造
//  [00..03]                   Signature
//  [04..E7]                   通常のキー
//  [E8..E8+MODIFIER_KEY_MAX]  ModifierKey
//  ～                         Padding (FlashROMの R/W は 128Byte 単位)
#define KEY_CONVERT_TABLE_ENTRY_COUNT               (VALID_USAGE_ID_MAX + MODIFIER_KEY_MAX)


#define MACRO_SIGNATURE  0xC8C7C6C5
#define MACRO_ONE_KEY_SIZE   7
#define MACRO_MAX_KEY_COUNT  54
#define MACRO_TOTAL_KEY_SIZE (MACRO_ONE_KEY_SIZE * MACRO_MAX_KEY_COUNT)
typedef struct MACRO_DATA_ {
  uint32 signature;
  uint8 count;
  uint8 keys[MACRO_TOTAL_KEY_SIZE];
  uint8 padding[1];
} MACRO_DATA;
// sizeof(MACRO_DATA) = 384
// FLASH_PAGESIZE_BYTES(128) * MACRO_DATA_SIZE_IN_PAGE(3) = 384
#define MACRO_DATA_SIZE_IN_PAGE  3
#define MACRO_DATA_COUNT         4

void load_key_config(void);
void load_key_table(void);
void reset_key_table(void);
void set_lang_convert_mode(uint8 mode);
void modify_report(uint8 *dst_report, const uint8 *src_report);
void update_key_table(uint8 src_id, uint8 dst_id);
uint8 extract_one_key_from_report(const uint8* report);
uint8 get_pressed_key_count(const uint8* report);

void set_macro_run_key(uint8 macro_no, const uint8 *report);
uint8 get_macro_no_from_report(const uint8 *report);
MACRO_DATA* get_macro_data(uint8 macro_no);
uint8 is_macro_data_valid(const MACRO_DATA *data);
void update_macro_data(uint8 macro_no, MACRO_DATA *data);

uint8 is_report_zero(const uint8* report);



#endif
