#include "Hakadorun2.h"
#include "KeyTable.h"
#include <Flash.h>

// Flash ROM MAP
//  Page  Address  Offset
//  000h  0_0000h  0_0000h
//   :       :         :     Program
//  7CEh  1_F380h  3_E70h  ---------------------------------------
//   :                       Config              1Page
//   :                       KeyTable            2Page(KEY_TABLE_SIZE_IN_PAGE)
//   :                       MacroData[0..3]     12Page(3*4)
//  7DDh  1_F740h -----------------------------
//   :       :        :      ReflashFATFile.rom  35Page確保 (> 4314Byte/128Byte)
//  800h  2_0000h  4_0000h
//        (x 0x40)   (x 0x80)
#define KEY_CONFIG_START_PAGE          0x7CE
#define KEY_CONFIG_SIZE_IN_PAGE        1
#define KEY_TABLE_START_PAGE           (KEY_CONFIG_START_PAGE + KEY_CONFIG_SIZE_IN_PAGE)
#define KEY_TABLE_SIZE_IN_PAGE         2
#define MACRO_DATA_START_PAGE          (KEY_TABLE_START_PAGE + KEY_TABLE_SIZE_IN_PAGE)
#define MACRO_DATA_TOTAL_SIZE_IN_PAGE  (MACRO_DATA_SIZE_IN_PAGE * MACRO_DATA_COUNT)
#define REFLASH_START_PAGE             (MACRO_DATA_START_PAGE + MACRO_DATA_TOTAL_SIZE_IN_PAGE)

#define CONFIG_SIGNATURE_0      0xAB
#define CONFIG_SIGNATURE_1      0xCD
#define CONFIG_SIGNATURE_2      0xEF
#define CONFIG_SIGNATURE_3      0x38

typedef enum LANG_CONVERT_MODE_ {
    LANG_CONVERT_NONE,
    LANG_CONVERT_JA_TO_EN,
    LANG_CONVERT_EN_TO_JA
} LANG_CONVERT_MODE;

typedef struct KEY_CONFIG_ {
    uint8 signature[4];
    uint8 lang_convert_mode;
    uint8 macro_run_key[MACRO_ONE_KEY_SIZE * MACRO_DATA_COUNT];
} KEY_CONFIG;

#define KEY_TABLE_SIZE_IN_BYTE     (KEY_TABLE_SIZE_IN_PAGE * FLASH_PAGESIZE_BYTES)


static KEY_CONFIG m_key_config;
static uint8 m_key_table[KEY_TABLE_SIZE_IN_BYTE];

rom uint8 ASCII_TO_JAPANESE_USAGEID[] = {
  0x2c,  0x00,// (space)
  0x1e,  0x02,// !
  0x1f,  0x02,// "
  0x20,  0x02,// #
  0x21,  0x02,// $
  0x22,  0x02,// %
  0x23,  0x02,// &
  0x24,  0x02,// '
  0x25,  0x02,// (
  0x26,  0x02,// )
  0x34,  0x02,// *
  0x33,  0x02,// +
  0x36,  0x00,// ,
  0x2d,  0x00,// -
  0x37,  0x00,// .
  0x38,  0x00,// /
  0x27,  0x00,// 0
  0x1e,  0x00,// 1
  0x1f,  0x00,// 2
  0x20,  0x00,// 3
  0x21,  0x00,// 4
  0x22,  0x00,// 5
  0x23,  0x00,// 6
  0x24,  0x00,// 7
  0x25,  0x00,// 8
  0x26,  0x00,// 9
  0x34,  0x00,// :
  0x33,  0x00,// ;
  0x36,  0x02,// <
  0x2d,  0x02,// =
  0x37,  0x02,// >
  0x38,  0x02,// ?
  0x2f,  0x00,// @
  0x04,  0x02,// A
  0x05,  0x02,// B
  0x06,  0x02,// C
  0x07,  0x02,// D
  0x08,  0x02,// E
  0x09,  0x02,// F
  0x0a,  0x02,// G
  0x0b,  0x02,// H
  0x0c,  0x02,// I
  0x0d,  0x02,// J
  0x0e,  0x02,// K
  0x0f,  0x02,// L
  0x10,  0x02,// M
  0x11,  0x02,// N
  0x12,  0x02,// O
  0x13,  0x02,// P
  0x14,  0x02,// Q
  0x15,  0x02,// R
  0x16,  0x02,// S
  0x17,  0x02,// T
  0x18,  0x02,// U
  0x19,  0x02,// V
  0x1a,  0x02,// W
  0x1b,  0x02,// X
  0x1c,  0x02,// Y
  0x1d,  0x02,// Z
  0x30,  0x00,// [
  0x87,  0x00,// BackSlash
  0x32,  0x00,// ]
  0x2e,  0x00,// ^
  0x87,  0x02,// _
  0x2f,  0x02,// `
  0x04,  0x00,// a
  0x05,  0x00,// b
  0x06,  0x00,// c
  0x07,  0x00,// d
  0x08,  0x00,// e
  0x09,  0x00,// f
  0x0a,  0x00,// g
  0x0b,  0x00,// h
  0x0c,  0x00,// i
  0x0d,  0x00,// j
  0x0e,  0x00,// k
  0x0f,  0x00,// l
  0x10,  0x00,// m
  0x11,  0x00,// n
  0x12,  0x00,// o
  0x13,  0x00,// p
  0x14,  0x00,// q
  0x15,  0x00,// r
  0x16,  0x00,// s
  0x17,  0x00,// t
  0x18,  0x00,// u
  0x19,  0x00,// v
  0x1a,  0x00,// w
  0x1b,  0x00,// x
  0x1c,  0x00,// y
  0x1d,  0x00,// z
  0x30,  0x02,// {
  0x89,  0x02,// |
  0x32,  0x02,// }
  0x2e,  0x02,// ~
};

rom uint8 ASCII_TO_ENGLISH_USAGEID[] = {
  0x2c,  0x00,// (space)
  0x1e,  0x02,// !
  0x34,  0x02,// "
  0x20,  0x02,// #
  0x21,  0x02,// $
  0x22,  0x02,// %
  0x24,  0x02,// &
  0x34,  0x00,// '
  0x26,  0x02,// (
  0x27,  0x02,// )
  0x25,  0x02,// *
  0x2e,  0x02,// +
  0x36,  0x00,// ,
  0x2d,  0x00,// -
  0x37,  0x00,// .
  0x38,  0x00,// /
  0x27,  0x00,// 0
  0x1e,  0x00,// 1
  0x1f,  0x00,// 2
  0x20,  0x00,// 3
  0x21,  0x00,// 4
  0x22,  0x00,// 5
  0x23,  0x00,// 6
  0x24,  0x00,// 7
  0x25,  0x00,// 8
  0x26,  0x00,// 9
  0x33,  0x02,// :
  0x33,  0x00,// ;
  0x36,  0x02,// <
  0x2e,  0x00,// =
  0x37,  0x02,// >
  0x38,  0x02,// ?
  0x1f,  0x02,// @
  0x04,  0x02,// A
  0x05,  0x02,// B
  0x06,  0x02,// C
  0x07,  0x02,// D
  0x08,  0x02,// E
  0x09,  0x02,// F
  0x0A,  0x02,// G
  0x0B,  0x02,// H
  0x0C,  0x02,// I
  0x0D,  0x02,// J
  0x0E,  0x02,// K
  0x0F,  0x02,// L
  0x10,  0x02,// M
  0x11,  0x02,// N
  0x12,  0x02,// O
  0x13,  0x02,// P
  0x14,  0x02,// Q
  0x15,  0x02,// R
  0x16,  0x02,// S
  0x17,  0x02,// T
  0x18,  0x02,// U
  0x19,  0x02,// V
  0x1A,  0x02,// W
  0x1B,  0x02,// X
  0x1C,  0x02,// Y
  0x1D,  0x02,// Z
  0x2f,  0x00,// [
  0x31,  0x00,// BackSlash
  0x30,  0x00,// ]
  0x23,  0x02,// ^
  0x2d,  0x02,// _
  0x35,  0x00,// `
  0x04,  0x00,// a
  0x05,  0x00,// b
  0x06,  0x00,// c
  0x07,  0x00,// d
  0x08,  0x00,// e
  0x09,  0x00,// f
  0x0a,  0x00,// g
  0x0b,  0x00,// h
  0x0c,  0x00,// i
  0x0d,  0x00,// j
  0x0e,  0x00,// k
  0x0f,  0x00,// l
  0x10,  0x00,// m
  0x11,  0x00,// n
  0x12,  0x00,// o
  0x13,  0x00,// p
  0x14,  0x00,// q
  0x15,  0x00,// r
  0x16,  0x00,// s
  0x17,  0x00,// t
  0x18,  0x00,// u
  0x19,  0x00,// v
  0x1a,  0x00,// w
  0x1b,  0x00,// x
  0x1c,  0x00,// y
  0x1d,  0x00,// z
  0x2f,  0x02,// {
  0x31,  0x02,// |
  0x30,  0x02,// }
  0x35,  0x02,// ~
};

static uint8 ja_usageid_to_ascii_tableindex(uint8 usage_id, uint8 modifier)
{
    uint8 modifier_to_search;
    uint8 index;
    uint8 usage_id_in_table, modifier_in_table;

    if (modifier & SHIFT_KEY_BITMAP) {
        modifier_to_search = 0x02; 
    } else {
        modifier_to_search = 0;
    }

    //  '￥' を \ に
    if ((usage_id == 0x89) && (modifier_to_search == 0)) {
        return (('\\' - ' ') << 1);
    }

    for (index = 0; index < sizeof(ASCII_TO_JAPANESE_USAGEID); index += 2) {
        usage_id_in_table = ASCII_TO_JAPANESE_USAGEID[index];
        if (usage_id_in_table != usage_id) {
            continue;
        }

        modifier_in_table = ASCII_TO_JAPANESE_USAGEID[index+1];
        if (modifier_in_table != modifier_to_search) {
            continue;
        }
        return index;
    }
    return 0xFF;
}

static uint8 en_usageid_to_ascii_tableindex(uint8 usage_id, uint8 modifier)
{
    uint8 modifier_to_search;
    uint8 index;
    uint8 usage_id_in_table, modifier_in_table;

    if (modifier & SHIFT_KEY_BITMAP) {
        modifier_to_search = 0x02; 
    } else {
        modifier_to_search = 0;
    }

    for (index = 0; index < sizeof(ASCII_TO_ENGLISH_USAGEID); index += 2) {
        usage_id_in_table = ASCII_TO_ENGLISH_USAGEID[index];
        if (usage_id_in_table != usage_id) {
            continue;
        }

        modifier_in_table = ASCII_TO_ENGLISH_USAGEID[index+1];
        if (modifier_in_table != modifier_to_search) {
            continue;
        }
        return index;
    }
    return 0xFF;
}

static uint8 ja_to_en_usage_id(uint8 ja_usage_id, uint8* p_modifier)
{ 
    uint8 modifier;
    uint8 index;
    uint8 en_usage_id, en_modifier;
    

    modifier = *p_modifier;
    index = ja_usageid_to_ascii_tableindex(ja_usage_id, modifier);
    if (index == 0xFF) {
        return ja_usage_id;
    }

    en_usage_id = ASCII_TO_ENGLISH_USAGEID[index];
    en_modifier = ASCII_TO_ENGLISH_USAGEID[index + 1];
    if (en_modifier & SHIFT_KEY_BITMAP) {
        if (! (modifier & SHIFT_KEY_BITMAP)) {
            *p_modifier = modifier | LEFT_SHIFT_KEY_BIT;
        }
    } else {
        if (modifier & SHIFT_KEY_BITMAP) {
            *p_modifier = modifier & (~SHIFT_KEY_BITMAP);
        }
    }

    return en_usage_id;
}

static uint8 en_to_ja_usage_id(uint8 en_usage_id, uint8* p_modifier)
{
    uint8 modifier;
    uint8 index;
    uint8 ja_usage_id, ja_modifier;
    

    modifier = *p_modifier;
    index = en_usageid_to_ascii_tableindex(en_usage_id, modifier);
    if (index == 0xFF) {
        return en_usage_id;
    }

    ja_usage_id = ASCII_TO_JAPANESE_USAGEID[index];
    ja_modifier = ASCII_TO_JAPANESE_USAGEID[index + 1];
    if (ja_modifier & SHIFT_KEY_BITMAP) {
        if (! (modifier & SHIFT_KEY_BITMAP)) {
            *p_modifier = modifier | LEFT_SHIFT_KEY_BIT;
        }
    } else {
        if (modifier & SHIFT_KEY_BITMAP) {
            *p_modifier = modifier & (~SHIFT_KEY_BITMAP);
        }
    }

    return ja_usage_id;   
}

static void convert_ja_report_to_en(uint8* report)
{
    uint8 i;
    for (i = 2; i < INPUT_REPORT_SIZE; i++) {
        report[i] = ja_to_en_usage_id(*(report + i), report);
    }
}

static void convert_en_report_to_ja(uint8* report)
{
    uint8 i;    
    for (i = 2; i < INPUT_REPORT_SIZE; i++) {
        report[i] = en_to_ja_usage_id(*(report + i), report);       
    }
}

static void set_default_to_key_config(void)
{
    uint8 i;
    vos_memset(&m_key_config, 0, sizeof(m_key_config));
    m_key_config.signature[0] = CONFIG_SIGNATURE_0;
    m_key_config.signature[1] = CONFIG_SIGNATURE_1;
    m_key_config.signature[2] = CONFIG_SIGNATURE_2;
    m_key_config.signature[3] = CONFIG_SIGNATURE_3;
    for (i = 0; i < MACRO_DATA_COUNT; i++) {
        m_key_config.macro_run_key[i * MACRO_ONE_KEY_SIZE] = 0xFF; // Invalid value
    }
}

static void set_default_to_key_table(void)
{
    uint8 i;

    vos_memset(m_key_table, 0, sizeof(m_key_table));

    m_key_table[0] = CONFIG_SIGNATURE_0;
    m_key_table[1] = CONFIG_SIGNATURE_1;
    m_key_table[2] = CONFIG_SIGNATURE_2;
    m_key_table[3] = CONFIG_SIGNATURE_3;

    for (i = VALID_USAGE_ID_MIN; i < KEY_CONVERT_TABLE_ENTRY_COUNT; i++) {
        m_key_table[i] = i;
    }
}

static void read_flash(uint16 start_page, uint16 page_count, uint8 *buffer)
{
    uint16 page, end_page;
    end_page = start_page + page_count;
    for (page = start_page; page < end_page; page++) {
        flash_readPage(page, buffer);
        buffer += FLASH_PAGESIZE_BYTES;
    }
}

static void write_flash(uint16 start_page, uint16 page_count, const uint8* buffer)
{
    uint16 page, end_page;
    end_page = start_page + page_count;
    for (page = start_page; page < end_page; page++) {
        flash_writePage(page, buffer);
        buffer += FLASH_PAGESIZE_BYTES;
    }
}

static void read_key_config_from_flash(void)
{
    uint8 buffer[KEY_TABLE_SIZE_IN_BYTE];
    read_flash(KEY_CONFIG_START_PAGE, KEY_CONFIG_SIZE_IN_PAGE, buffer);
    vos_memcpy(&m_key_config, buffer, sizeof(m_key_config));
}

static void write_key_config_to_flash(void)
{
    uint8 buffer[KEY_TABLE_SIZE_IN_BYTE];
    vos_memset(buffer, 0, KEY_TABLE_SIZE_IN_BYTE);
    vos_memcpy(buffer, &m_key_config, sizeof(m_key_config));
    write_flash(KEY_CONFIG_START_PAGE, KEY_CONFIG_SIZE_IN_PAGE, buffer);
}

static void read_key_table_from_flash(void)
{
    read_flash(KEY_TABLE_START_PAGE, KEY_TABLE_SIZE_IN_PAGE, m_key_table);
}  

static void write_key_table_to_flash(void)
{
    write_flash(KEY_TABLE_START_PAGE, KEY_TABLE_SIZE_IN_PAGE, m_key_table);
}

static uint8 is_usage_id_valid(uint8 usage_id)
{
    if ((VALID_USAGE_ID_MIN <= usage_id) && (usage_id <= VALID_USAGE_ID_MAX)) {
        return 1;
    }
    return 0;
}

static uint8 compare_report_and_run_key(const uint8* report, const uint8* run_key)
{
    if (report[0] != run_key[0]) {
        return 1;
    }
    return b_memcmp(report + 2, run_key + 1, MACRO_ONE_KEY_SIZE - 1);
}

static void modify_report_by_key_table(uint8 *dst_report, const uint8 *src_report)
{
    uint8 src_modifier_bitmap;
    uint8 key, usage_id, dst_index, src_index;

    vos_memset(dst_report, 0, INPUT_REPORT_SIZE);

    dst_index = 2;
    src_modifier_bitmap = src_report[0];
    for (key = MODIFIRE_KEY_MIN; key < MODIFIER_KEY_MAX; key++) {
        if (! (src_modifier_bitmap & (1 << key))) {
            continue;
        }
        usage_id = m_key_table[MODIFIRE_KEY_TO_PSEUDO_USAGE_ID(key)];
        if (IS_THIS_USAGE_ID_MODIFIER(usage_id)) {
            dst_report[0] |= (1 << PSEUDO_USAGE_ID_TO_MODIFIRE_KEY(usage_id));
        } else {
            dst_report[dst_index] = usage_id;
            dst_index++;
            if (dst_index >= INPUT_REPORT_SIZE) {
                return;
            }
        }
    }

    for (src_index = 2; src_index < INPUT_REPORT_SIZE; src_index++) {
        usage_id = src_report[src_index];
        if (! is_usage_id_valid(usage_id)) {
            continue;
        }
        usage_id = m_key_table[usage_id];
        if (IS_THIS_USAGE_ID_MODIFIER(usage_id)) {
            dst_report[0] |= (1 << PSEUDO_USAGE_ID_TO_MODIFIRE_KEY(usage_id));
        } else {
            dst_report[dst_index] = usage_id;
            dst_index++;
            if (dst_index >= INPUT_REPORT_SIZE) {
                return;
            }
        }        
    }
}


void load_key_config(void)
{
    read_key_config_from_flash();
    if ((m_key_config.signature[0] != CONFIG_SIGNATURE_0)
        || (m_key_config.signature[1] != CONFIG_SIGNATURE_1)
        || (m_key_config.signature[2] != CONFIG_SIGNATURE_2)
        || (m_key_config.signature[3] != CONFIG_SIGNATURE_3)) {
        set_default_to_key_config();
        write_key_config_to_flash();
    }
}

void load_key_table(void)
{
    read_key_table_from_flash();
    if ((m_key_table[0] != CONFIG_SIGNATURE_0)
        || (m_key_table[1] != CONFIG_SIGNATURE_1)
        || (m_key_table[2] != CONFIG_SIGNATURE_2)
        || (m_key_table[3] != CONFIG_SIGNATURE_3)) {
        reset_key_table();
    }
}

void reset_key_table(void)
{
    set_default_to_key_table();
    write_key_table_to_flash();
}

void set_lang_convert_mode(uint8 mode)
{
    m_key_config.lang_convert_mode = mode;
    write_key_config_to_flash();
}

void modify_report(uint8 *dst_report, const uint8 *src_report)
{
    modify_report_by_key_table(dst_report, src_report);
    switch (m_key_config.lang_convert_mode) {
    case LANG_CONVERT_JA_TO_EN:
        convert_ja_report_to_en(dst_report);
        break;
    case LANG_CONVERT_EN_TO_JA:
        convert_en_report_to_ja(dst_report);
        break;
    }
}

void update_key_table(uint8 src_id, uint8 dst_id)
{
    m_key_table[src_id] = dst_id;
    write_key_table_to_flash();
}

uint8 extract_one_key_from_report(const uint8 *report)
{
    uint8 index, usage_id;
    
    for (index = (INPUT_REPORT_SIZE - 1); index >= 2; index--) {
        usage_id = report[index];
        if (is_usage_id_valid(usage_id)) {
            return usage_id;
        }
    }
    
    for (index = MODIFIRE_KEY_MIN; index < MODIFIER_KEY_MAX; index++) {
        if (report[0] & (1 << index)) {
            return MODIFIRE_KEY_TO_PSEUDO_USAGE_ID(index);
        }
    }

    return 0;
}

uint8 get_pressed_key_count(const uint8* report)
{
    uint8 pressed_count;
    uint8 i;
    uint8 modifier;

    pressed_count = 0;
    modifier = report[0];
    for (i = MODIFIRE_KEY_MIN; i < MODIFIER_KEY_MAX; i++) {
        if (modifier & (1 << i)) {
            pressed_count++;
        }
    }
    for (i = 2; i < INPUT_REPORT_SIZE; i++) {
        if (is_usage_id_valid(*(report + i))) {
            pressed_count++;
        }
    }

    return pressed_count;
}

void set_macro_run_key(uint8 macro_no, const uint8 *report)
{
    uint8 *dst;

    dst = &m_key_config.macro_run_key[macro_no * MACRO_ONE_KEY_SIZE];

    *dst = report[0];
    vos_memcpy(dst + 1, report + 2, MACRO_ONE_KEY_SIZE - 1);

    write_key_config_to_flash();
}

uint8 get_macro_no_from_report(const uint8 *report)
{
    uint8 macro_no;
    uint8 offset;

    offset = 0;
    for (macro_no = 0; macro_no < MACRO_DATA_COUNT; macro_no++) {
        if (compare_report_and_run_key(report, &m_key_config.macro_run_key[offset]) == 0) {
            return macro_no;
        }
        offset += MACRO_ONE_KEY_SIZE;
    }
    return 0xFF;
}

MACRO_DATA* get_macro_data(uint8 macro_no)
{
    MACRO_DATA *d;

    d = (MACRO_DATA*)vos_malloc(sizeof(MACRO_DATA));
    if (d == NULL) {
        return NULL;
    }

    read_flash(MACRO_DATA_START_PAGE + (MACRO_DATA_SIZE_IN_PAGE * macro_no), MACRO_DATA_SIZE_IN_PAGE, (uint8*)d);

    return d;
}

uint8 is_macro_data_valid(const MACRO_DATA *data)
{
    if (data->signature != MACRO_SIGNATURE) {
        return 0;
    }
    if ((data->count == 0) || (data->count > MACRO_MAX_KEY_COUNT)) {
        return 0;
    }
    return 1;
}

void update_macro_data(uint8 macro_no, MACRO_DATA *data)
{
    data->signature = MACRO_SIGNATURE;
    write_flash(MACRO_DATA_START_PAGE + (MACRO_DATA_SIZE_IN_PAGE * macro_no), MACRO_DATA_SIZE_IN_PAGE, (uint8*)data);
}

uint8 is_report_zero(const uint8* report)
{
    uint8 i;
    for (i = 0; i < INPUT_REPORT_SIZE; i++) {
        if (report[i]) {
            return 0;
        }
    }
    return 1;
}
