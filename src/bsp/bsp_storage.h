#pragma once

#include <stdint.h>

/******************************************************类型定义*************************************************************************/

/* 存储器句柄 */
typedef struct bsp_storage_t* bsp_storage_handle_t;

/* 存储器介质枚举 */
typedef enum bsp_storage_medium_e{
    BSP_STORAGE_MEDIUM_UNKNOWN = 0,     //未知
    BSP_STORAGE_MEDIUM_SD = 1,          //SD卡
    BSP_STORAGE_MEDIUM_EMMC = 2,        //eMMC卡
    BSP_STORAGE_MEDIUM_FLASH = 3,       //FLASH
    BSP_STORAGE_MEDIUM_RAM = 4,         //RAM
    BSP_STORAGE_MEDIUM_BIN_FILE = 5,    //二进制文件
}bsp_storage_medium_e;
typedef uint8_t bsp_storage_medium_t;

/* 存储器信息 */
typedef struct bsp_storage_info_t{
    bsp_storage_medium_t storage_medium;    //存储器介质
    uint32_t total_storage;                 //总存储空间
    uint32_t erase_unit;                    //擦除单元大小 - 为0表示不支持擦除，1表示任意擦除
    uint32_t program_unit;                  //编程单元大小 - 为0表示不支持编程，1表示任意编程
    uint32_t erase_value;                   //擦除值 - 0xFF表示擦除值为0xFF，0x00表示擦除值为0x00
    uint32_t write_direct:1;                //是否支持直接写入 - 1:不需要擦除，可以直接写入 0:需要擦除，才能写入
    uint32_t reserved:31;                   //保留
}bsp_storage_info_t;

/* 存储器类型枚举 */
typedef enum bsp_storage_type_e{
    BSP_STORAGE_TYPE_UNKNOWN = 0,
    BSP_STORAGE_TYPE_USER,      //用户存储
    BSP_STORAGE_TYPE_SYSTEM,    //系统存储
}bsp_storage_type_e;
typedef uint8_t bsp_storage_type_t;

/* 存储器属性枚举 */
typedef enum bsp_storage_attr_e{
    BSP_STORAGE_ATTR_EXCLUSIVE_OPEN = 0x01,            // 独占打开

    BSP_STORAGE_ATTR_DEFAULT = BSP_STORAGE_ATTR_EXCLUSIVE_OPEN, // 默认属性
}bsp_storage_attr_e;
typedef uint32_t bsp_storage_attr_t;

/* 存储器描述 */
typedef struct bsp_storage_desc_t{
    bsp_storage_type_t type;    //类型
    uint8_t index;              //索引
    bsp_storage_attr_t attr;    //属性
}bsp_storage_desc_t;

/* 存储器映射表项 - 由 bsp_cfg.h 的 BSP_STORAGE_MAP_TABLE 提供
 * SDL 后端把每个表项映射成一个固定大小的 bin 文件，模拟真实 flash：
 *   erase_unit   擦除粒度（如 4KB sector）
 *   program_unit 编程粒度（如 256B page）
 *   erase_value  擦除后的填充值（NOR flash 为 0xFF）
 *   write_direct 1: 可直接覆盖写；0: 必须先擦除（写入按 old & data 生效） */
typedef struct bsp_storage_map_t{
    bsp_storage_type_t type;
    uint8_t index;
    bsp_storage_medium_t medium;
    const char* filename;       // 文件名（不含 .bin），实际文件为 <base>/<filename>.bin
    uint32_t total_size;        // 总容量（字节），必须与 bin 文件大小一致
    uint32_t erase_unit;        // 擦除单元（字节），0=不支持擦除，1=任意擦除
    uint32_t program_unit;      // 编程单元（字节），0=不支持编程，1=任意编程
    uint32_t erase_value;       // 擦除值，flash 为 0xFF
    uint8_t write_direct;       // 1=可直接写，0=需要先擦除
}bsp_storage_map_t;


#ifdef __cplusplus
extern "C" {
#endif

/******************************************************打开/关闭 接口*****************************************************/

/**
 * @brief 打开存储器
 * @param desc 存储器描述结构体指针
 * @return 存储器句柄
*/
bsp_storage_handle_t bsp_storage_open(bsp_storage_desc_t* desc);

/**
 * @brief 关闭存储器
 * @param handle 存储器句柄
*/
void bsp_storage_close(bsp_storage_handle_t handle);

/******************************************************查询/控制 接口*****************************************************/

/**
 * @brief 获取存储器信息
 * @param handle 存储器句柄
 * @return 存储器信息
*/
const bsp_storage_info_t* bsp_storage_get_info(bsp_storage_handle_t handle);

/**
 * @brief 写入数据
 * @param handle 存储器句柄
 * @param address 写入地址(存储器内部地址)
 * @param data 数据缓冲区
 * @param length 写入长度
 * @return 成功写入的长度
 * @note 允许任意地址/长度（不要求对齐）。对 write_direct=0 的 flash，
 *       未擦除区域按 old & data 写入（只能 1->0），跨 program_unit 页会自动处理。
*/
uint32_t bsp_storage_write(bsp_storage_handle_t handle, uint32_t address , const uint8_t* data , uint32_t length);

/**
 * @brief 擦除数据
 * @param handle 存储器句柄
 * @param address 擦除地址(存储器内部地址)
 * @param length 擦除长度
 * @return 成功擦除的长度
 * @note 要求 address 与 length 都按 erase_unit 对齐；不对齐返回 0。
*/
uint32_t bsp_storage_erase(bsp_storage_handle_t handle, uint32_t address , uint32_t length);

/**
 * @brief 读取数据
 * @param handle 存储器句柄
 * @param address 读取地址(存储器内部地址)
 * @param data 数据缓冲区
 * @param length 读取长度
 * @return 成功读取的长度
 * @note 读取不要求对齐。
*/
uint32_t bsp_storage_read(bsp_storage_handle_t handle, uint32_t address , uint8_t* data , uint32_t length);
/**
 * @brief 刷新数据
 * @param handle 存储器句柄
*/
void bsp_storage_flush(bsp_storage_handle_t handle);

#ifdef __cplusplus
}
#endif

