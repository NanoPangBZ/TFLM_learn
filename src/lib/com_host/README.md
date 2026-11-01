# COM_HOST 通信库使用文档

**作者**: NanoPang  
**邮箱**: codingbugstd@gmail.com

## 目录

1. [概述](#概述)
2. [实现原理](#实现原理)
3. [核心架构](#核心架构)
4. [快速开始](#快速开始)
5. [API详细说明](#api详细说明)
6. [使用示例](#使用示例)
7. [配置说明](#配置说明)
8. [常见问题](#常见问题)

---

## 概述

### 什么是 COM_HOST？

COM_HOST 是一个用于嵌入式设备间通信的轻量级C语言通信库。它提供了一套完整的消息路由、打包/解包、端口管理机制，支持在多设备组成的树形网络中进行可靠的消息传递。

### 主要特性

- **🌲 树形网络拓扑**：支持多主机、多端口的树形网络结构，不支持环状网络
- **📦 消息打包/解包**：内置YOUR_WAY_STUDIO协议打包器，自动处理消息的序列化和CRC校验
- **🚀 智能路由**：支持手动添加路由和自动路由学习
- **🔌 端口抽象**：统一的端口接口，支持UART、SPI、USB等多种物理层
- **💡 轻量级设计**：可配置的内存堆管理，适合资源受限的嵌入式系统
- **🔄 消息ACK机制**：支持消息应答，确保通信可靠性

### 版本信息

- **库版本**: 1.0.8
- **协议版本**: 1.0.27

---

## 实现原理

### 架构设计

COM_HOST 采用分层设计，从下到上分为以下几层：

```
┌─────────────────────────────────────────┐
│        应用层 (Application Layer)       │  ← 消息处理回调
├─────────────────────────────────────────┤
│      消息路由层 (Routing Layer)         │  ← 路由表、消息转发
├─────────────────────────────────────────┤
│      端口管理层 (Port Layer)            │  ← 端口抽象、多端口管理
├─────────────────────────────────────────┤
│     打包/解包层 (Packager Layer)        │  ← YOUR_WAY_STUDIO协议、CRC校验
├─────────────────────────────────────────┤
│    底层依赖层 (Low-Level Depend)        │  ← 时间、内存管理
├─────────────────────────────────────────┤
│       物理层 (Physical Layer)           │  ← UART/SPI/USB等
└─────────────────────────────────────────┘
```

### 核心概念

#### 1. 主机 (Host)

每个设备上运行一个或多个通信主机，每个主机有唯一的 **Host ID**：

- **主控设备**: `COM_HOST_ID_MAIN_CONTROLLER` (0x01) - 必须是树形网络的根节点
- **子控设备**: `COM_HOST_ID_SUB_CONTROLLER_1~3` (0x02~0x04)
- **配件设备**: `COM_HOST_ID_AUX_DEVICE_1~5` (0x11~0x15)
- **客户端**: `COM_HOST_ID_MOBOILE_APP` (0xE0), `COM_HOST_ID_PC_CLIENT` (0xE1) 等

#### 2. 端口 (Port)

端口是通信的物理通道抽象，每个端口包含：

- **名称**：用于标识端口（如 "uart1", "usb"）
- **属性**：可读/可写、打包协议类型
- **缓冲区**：打包/解包缓冲区
- **读写函数**：底层物理层的读写接口

#### 3. 消息 (Message)

消息是通信的基本单位，包含：

```c
typedef struct com_host_msg_t {
    com_host_msg_id_t msg_id;   // 消息ID（如0x0000为PING消息）
    uint8_t* data;              // 消息数据
    uint16_t data_len;          // 数据长度
    uint8_t need_ack:1;         // 是否需要ACK
    uint8_t is_ack:1;           // 是否为ACK消息
} com_host_msg_t;
```

#### 4. 消息包 (Message Package)

消息包是在网络中传输的完整数据单元：

```c
typedef struct msg_package_t {
    com_host_id_t sender_host_id;   // 发送者ID
    com_host_id_t target_host_id;   // 接收者ID
    com_host_msg_t* msg;            // 消息内容
    uint8_t seq;                    // 序列号
    uint16_t crc16;                 // CRC校验码
    uint8_t forword_cnt:4;          // 转发次数
} msg_package_t;
```

### 工作流程

#### 消息发送流程

```
应用层调用发送API
    ↓
查找路由表，确定目标主机的端口
    ↓
构建消息包（添加sender/target/seq等信息）
    ↓
打包器将消息包序列化为字节流（添加CRC）
    ↓
通过端口写函数发送到物理层
```

#### 消息接收流程

```
物理层接收到数据
    ↓
端口读函数将数据读入解包缓冲区
    ↓
打包器从字节流中解包出消息包
    ↓
检查目标ID是否为本机
    ↓
├─ 是本机：查找消息处理函数并调用
└─ 非本机：查找路由表，转发到目标主机
```

#### 路由机制

COM_HOST 支持两种路由方式：

1. **手动添加路由**：通过 `com_host_add_route()` 手动配置路由表
2. **智能路由学习**：启用 `COM_HOST_AUTO_ADD_ROUTE_ENABLE` 后，自动学习接收到消息的来源路由

路由转发规则：
- 最大转发次数由 `COM_HOST_MAX_ROUTE_FORWARD_CNT` 控制（默认8次）
- 每次转发会增加 `forword_cnt` 计数
- 超过最大转发次数的消息会被丢弃

---

## 核心架构

### 目录结构

```
com_host/
├── com_host.h                  # 公共 API 接口
├── com_host.c                  # 主机核心实现
├── com_host_cfg.h              # 配置文件
├── protocol_def.h              # 协议定义入口（含协议版本号）
├── protocol/
│   ├── com_host_id_def.h       # 主机 ID 定义
│   ├── com_msg_id_def.h        # 消息 ID 定义
│   ├── com_msg_data_def.h      # 消息数据结构定义
│   ├── com_msg_data_public_def.h
│   └── com_msg_music_staff_def.h  # 曲谱/演奏相关数据结构
├── private/
│   ├── com_host_private.h      # 内部数据结构
│   ├── com_host_port.c/h       # 端口管理
│   ├── com_host_packager.c/h   # 打包/解包器
│   ├── com_host_lib/           # 工具库（链表、环形缓冲区、CRC16）
│   ├── com_host_ll_depend/     # 底层依赖（内存堆、系统时间）
│   └── com_host_package/       # 打包协议实现（YOUR_WAY_STUDIO 协议）
├── ffi_bridge/                 # FFI 桥接（用于其他语言调用）
├── example/                    # 示例代码
├── test/                       # 测试代码
└── docs/                       # 文档（客户端同步、曲谱模式、效果器等）
```

### 核心数据结构

#### 通信主机结构体

```c
typedef struct com_host_t {
    com_host_id_t host_id;                    // 本主机ID
    void* default_cb_ctx;                     // 默认消息处理函数上下文
    com_host_msg_handler_t default_msg_handler; // 默认消息处理函数
    com_host_list_handle_t port_handle_list;  // 端口列表
    com_host_list_handle_t routable_host_list;// 可路由主机列表
    com_host_list_handle_t msg_handler_list;  // 消息处理函数列表
    uint8_t seq;                              // 消息序列号
} com_host_t;
```

#### 可路由主机结构体

```c
typedef struct routable_host_t {
    com_host_id_t host_id;                      // 主机ID
    com_host_list_handle_t routable_port_list;  // 可路由端口列表
    uint32_t last_recv_time;                    // 最后接收时间
} routable_host_t;
```

---

## 快速开始

### 1. 初始化底层依赖

```c
#include "com_host.h"
#include <stdlib.h>

// 系统时间获取函数（单位：毫秒）
uint32_t get_system_time_ms(void) {
    // 返回系统运行时间，如HAL_GetTick()
    return HAL_GetTick();
}

int main(void) {
    // 1. 准备栈缓冲区（用于内存分配）
    static uint8_t stack_buffer[4096];
    
    // 2. 配置底层依赖
    com_host_ll_depend_t ll_depend = {
        .get_time_ms = get_system_time_ms,
        .stack_buff = stack_buffer,
        .stack_buff_size = sizeof(stack_buffer),
        .stack_align = 4
    };
    
    // 3. 初始化通信库
    com_host_init(&ll_depend);
    
    // ... 后续步骤
}
```

### 2. 创建通信主机

```c
// 创建主控主机
com_host_desc_t host_desc = {
    .host_id = COM_HOST_ID_MAIN_CONTROLLER
};

com_host_handle_t host = com_host_create(&host_desc);
if (host == NULL) {
    // 创建失败处理
    return -1;
}
```

### 3. 添加通信端口

```c
// UART写函数（用户实现）
int16_t uart_write(void* opt_ctx, const uint8_t* data, uint16_t size) {
    // 调用HAL库发送数据，例如：
    // HAL_UART_Transmit(&huart1, data, size, 1000);
    return size; // 返回实际发送的字节数
}

// UART读函数（用户实现）
int16_t uart_read(void* opt_ctx, uint8_t* buffer, uint16_t size) {
    // 从UART读取数据，例如：
    // return HAL_UART_Receive(&huart1, buffer, size, 10);
    return 0; // 返回实际读取的字节数
}

// 配置端口
com_host_port_desc_t port_desc = {
    .name = "uart1",
    .attr = COM_HOST_PORT_ATTR_DEFAULT,
    .unpack_buf = NULL,        // NULL则自动分配
    .unpack_buf_size = 512,
    .pack_buf = NULL,
    .pack_buf_size = 512,
    .opt_ctx = NULL,           // 可传入UART句柄
    .write = uart_write,
    .read = uart_read
};

com_host_port_handle_t port = com_host_add_port(host, &port_desc);
if (port == NULL) {
    // 端口添加失败
    return -1;
}
```

### 4. 添加路由

```c
// 添加到客户端的路由（通过uart1端口）
com_host_add_route(host, COM_HOST_ID_MOBOILE_APP, port);

// 添加到子控的路由
com_host_add_route(host, COM_HOST_ID_SUB_CONTROLLER_1, port);
```

### 5. 注册消息处理函数

```c
// PING消息处理函数
void ping_handler(com_host_handle_t host,
                  com_host_port_handle_t port,
                  com_host_id_t sender_host_id,
                  com_host_msg_t* msg,
                  uint8_t seq,
                  void* cb_ctx) {
    
    printf("Received PING from host 0x%02X, seq=%d\n", sender_host_id, seq);
    
    // 如果需要ACK
    if (msg->need_ack) {
        uint8_t ack_data[] = {0x00}; // 返回成功
        com_host_send_ack(host, port, sender_host_id, msg,
                         ack_data, sizeof(ack_data), seq);
    }
}

// 注册处理函数
com_host_add_msg_handler(host, COM_HOST_MSG_ID_PING, ping_handler, NULL);
```

### 6. 主循环运行

```c
while (1) {
    // 定期调用，处理端口接收和解包
    com_host_run(host);
    
    // 其他任务...
    HAL_Delay(1);
}
```

### 7. 发送消息

```c
// 构建消息
com_host_msg_t msg = {
    .msg_id = COM_HOST_MSG_ID_PING,
    .data = NULL,
    .data_len = 0,
    .need_ack = 1,  // 需要ACK
    .is_ack = 0
};

uint8_t seq;
// 发送到手机APP
int ret = com_host_send_msg(host, COM_HOST_ID_MOBOILE_APP, &msg, &seq);
if (ret == 0) {
    printf("Message sent successfully, seq=%d\n", seq);
}
```

---

## API详细说明

### 初始化与创建

#### `com_host_init()`

初始化通信库（全局初始化，只需调用一次）

```c
void com_host_init(com_host_ll_depend_t* ll_depend);
```

**参数：**
- `ll_depend`: 底层依赖配置，包含时间函数、栈缓冲区等

**说明：**
- 必须在任何其他API之前调用
- 配置内存堆管理器和系统时间

---

#### `com_host_create()`

创建一个通信主机实例

```c
com_host_handle_t com_host_create(com_host_desc_t* desc);
```

**参数：**
- `desc`: 主机描述符，包含主机ID

**返回值：**
- 成功：主机句柄
- 失败：NULL

**示例：**
```c
com_host_desc_t desc = {.host_id = COM_HOST_ID_MAIN_CONTROLLER};
com_host_handle_t host = com_host_create(&desc);
```

---

#### `com_host_destroy()`

销毁通信主机，释放资源

```c
void com_host_destroy(com_host_handle_t host);
```

---

### 端口管理

#### `com_host_add_port()`

为主机添加一个通信端口

```c
com_host_port_handle_t com_host_add_port(
    com_host_handle_t host,
    com_host_port_desc_t* port_desc
);
```

**参数：**
- `host`: 主机句柄
- `port_desc`: 端口描述符

**端口属性：**
- `COM_HOST_PORT_ATTR_READABLE`: 端口可读
- `COM_HOST_PORT_ATTR_WRITEABLE`: 端口可写
- `COM_HOST_PORT_ATTR_PACK_PROTOCOL_YOUR_WAY_STUDIO`: YOUR_WAY_STUDIO打包协议
- `COM_HOST_PORT_ATTR_DEFAULT`: 默认属性（可读可写，YOUR_WAY_STUDIO协议）

**返回值：**
- 成功：端口句柄
- 失败：NULL

---

### 路由管理

#### `com_host_add_route()`

添加路由规则

```c
int16_t com_host_add_route(
    com_host_handle_t host,
    com_host_id_t target_host_id,
    com_host_port_handle_t routable_port
);
```

**参数：**
- `host`: 主机句柄
- `target_host_id`: 目标主机ID
- `routable_port`: 用于到达目标主机的端口

**返回值：**
- 0: 成功
- 非0: 失败

**说明：**
- 支持为同一目标主机添加多个端口（多路径）
- 发送消息时会通过所有路径发送（冗余）

---

### 消息处理

#### `com_host_add_msg_handler()`

注册消息处理函数

```c
int16_t com_host_add_msg_handler(
    com_host_handle_t host,
    com_host_msg_id_t msg_id,
    com_host_msg_handler_t handler,
    void* ctx
);
```

**参数：**
- `host`: 主机句柄
- `msg_id`: 要处理的消息ID
- `handler`: 消息处理回调函数
- `ctx`: 用户上下文指针，会传递给回调函数

**回调函数原型：**
```c
typedef void (*com_host_msg_handler_t)(
    com_host_handle_t host,
    com_host_port_handle_t port,
    com_host_id_t sender_host_id,
    com_host_msg_t* msg,
    uint8_t seq,
    void* cb_ctx
);
```

**返回值：**
- 0: 成功
- 非0: 失败（如重复注册）

---

#### `com_host_add_default_msg_handler()`

注册默认消息处理函数（用于处理未注册的消息ID）

```c
int16_t com_host_add_default_msg_handler(
    com_host_handle_t host,
    com_host_msg_handler_t handler,
    void* ctx
);
```

---

### 消息发送

#### `com_host_send_msg()`

发送消息到目标主机（通过路由表）

```c
int16_t com_host_send_msg(
    com_host_handle_t host,
    com_host_id_t target_host_id,
    com_host_msg_t* msg,
    uint8_t* seq
);
```

**参数：**
- `host`: 主机句柄
- `target_host_id`: 目标主机ID
- `msg`: 消息指针
- `seq`: 返回的消息序列号（可为NULL）

**返回值：**
- 0: 成功
- -1: 失败（如未找到路由）

---

#### `com_host_send_ack()`

发送ACK应答消息

```c
int16_t com_host_send_ack(
    com_host_handle_t host,
    com_host_port_handle_t port,
    com_host_id_t sender_host_id,
    com_host_msg_t* msg,
    uint8_t* ack_data,
    uint16_t ack_size,
    uint8_t seq
);
```

**参数：**
- `host`: 主机句柄
- `port`: 收到原消息的端口
- `sender_host_id`: 原消息发送者ID
- `msg`: 原消息指针
- `ack_data`: ACK数据
- `ack_size`: ACK数据长度
- `seq`: 原消息的序列号

**说明：**
- ACK消息会原路返回（通过接收端口）
- ACK消息的序列号与原消息相同
- ACK消息的 `is_ack` 标志为1

---

#### `com_host_send_msg_by_port()`

通过指定端口发送消息（不走路由，性能更高）

```c
int16_t com_host_send_msg_by_port(
    com_host_handle_t host,
    com_host_port_handle_t port,
    com_host_id_t target_host_id,
    com_host_msg_t* msg,
    uint8_t* seq
);
```

**使用场景：**
- 明确知道目标主机在哪个端口
- 需要更高的发送性能
- 避免路由查找开销

---

#### `com_host_send_msgs()`

批量发送多条消息到同一目标主机（通过路由表）

```c
int16_t com_host_send_msgs(
    com_host_handle_t host,
    com_host_id_t target_host_id,
    com_host_msg_t* msgs,
    uint8_t msg_cnt
);
```

**参数：**
- `host`: 主机句柄
- `target_host_id`: 目标主机 ID
- `msgs`: 消息数组指针
- `msg_cnt`: 消息数量（受 `MULTI_MSG_SEND_MAX_CNT` 限制，默认 128）

**返回值：**
- 0: 成功
- -1: 失败（如未找到路由或 `msg_cnt` 超限）

---

### 运行与维护

#### `com_host_run()`

主机运行一次（处理所有端口的接收和解包）

```c
void com_host_run(com_host_handle_t host);
```

**说明：**
- 必须在主循环中定期调用
- 频率建议：1~10ms
- 内部会遍历所有端口，调用解包函数

---

#### `com_host_last_recv_time()`

获取与目标主机的最后通信时间

```c
uint32_t com_host_last_recv_time(
    com_host_handle_t host,
    com_host_id_t target_host_id
);
```

**返回值：**
- 最后接收时间（毫秒）
- 0: 从未收到该主机消息

**用途：**
- 检测连接状态
- 超时检测

---

#### `com_host_get_mem_info()`

获取内存使用信息

```c
void com_host_get_mem_info(
    com_host_handle_t host,
    uint32_t* total_size,
    uint32_t* used_size
);
```

---

#### `com_host_get_version()`

获取库版本

```c
uint8_t com_host_get_version(
    uint8_t* major,
    uint8_t* minor,
    uint8_t* patch
);
```

**返回值：**
- 0: 已发布版本
- 1: 开发中版本

---

#### `com_host_get_protocol_version()`

获取协议版本（与对端兼容性判断时使用）

```c
uint8_t com_host_get_protocol_version(
    uint8_t* major,
    uint8_t* minor,
    uint8_t* patch
);
```

**说明：** 协议版本定义在 `protocol_def.h` 中。

---

## 使用示例

### 示例1：主控设备（树根节点）

```c
#include "com_host.h"
#include <stdio.h>
#include <string.h>

static uint8_t stack_buffer[8192];
static com_host_handle_t g_host = NULL;
static com_host_port_handle_t g_uart_port = NULL;

// UART读写函数（用户实现）
int16_t uart_write(void* ctx, const uint8_t* data, uint16_t size) {
    // HAL_UART_Transmit(...);
    return size;
}

int16_t uart_read(void* ctx, uint8_t* buffer, uint16_t size) {
    // return HAL_UART_Receive(...);
    return 0;
}

// 系统时间
uint32_t get_time_ms(void) {
    return HAL_GetTick();
}

// PING消息处理
void ping_handler(com_host_handle_t host, com_host_port_handle_t port,
                  com_host_id_t sender_id, com_host_msg_t* msg,
                  uint8_t seq, void* ctx) {
    printf("Received PING from 0x%02X\n", sender_id);
    
    if (msg->need_ack) {
        uint8_t ack = 0x00;
        com_host_send_ack(host, port, sender_id, msg, &ack, 1, seq);
    }
}

// 重启命令处理
void reboot_handler(com_host_handle_t host, com_host_port_handle_t port,
                    com_host_id_t sender_id, com_host_msg_t* msg,
                    uint8_t seq, void* ctx) {
    if (!msg->is_ack) {
        printf("Received REBOOT command\n");
        
        // 执行重启...
        
        if (msg->need_ack) {
            uint8_t ack = 0x00; // 成功
            com_host_send_ack(host, port, sender_id, msg, &ack, 1, seq);
        }
    }
}

void main_controller_init(void) {
    // 1. 初始化库
    com_host_ll_depend_t ll_depend = {
        .get_time_ms = get_time_ms,
        .stack_buff = stack_buffer,
        .stack_buff_size = sizeof(stack_buffer),
        .stack_align = 4
    };
    com_host_init(&ll_depend);
    
    // 2. 创建主机
    com_host_desc_t host_desc = {
        .host_id = COM_HOST_ID_MAIN_CONTROLLER
    };
    g_host = com_host_create(&host_desc);
    
    // 3. 添加UART端口
    com_host_port_desc_t port_desc = {
        .name = "uart1",
        .attr = COM_HOST_PORT_ATTR_DEFAULT,
        .unpack_buf = NULL,
        .unpack_buf_size = 1024,
        .pack_buf = NULL,
        .pack_buf_size = 1024,
        .opt_ctx = NULL,
        .write = uart_write,
        .read = uart_read
    };
    g_uart_port = com_host_add_port(g_host, &port_desc);
    
    // 4. 添加路由
    com_host_add_route(g_host, COM_HOST_ID_MOBOILE_APP, g_uart_port);
    com_host_add_route(g_host, COM_HOST_ID_SUB_CONTROLLER_1, g_uart_port);
    
    // 5. 注册消息处理函数
    com_host_add_msg_handler(g_host, COM_HOST_MSG_ID_PING, ping_handler, NULL);
    com_host_add_msg_handler(g_host, COM_HOST_MSG_ID_CMD_REBOOT, reboot_handler, NULL);
}

void main_loop(void) {
    while (1) {
        com_host_run(g_host);
        HAL_Delay(1);
    }
}
```

---

### 示例2：发送消息

```c
// 发送PING消息
void send_ping_to_mobile(void) {
    com_host_msg_t msg = {
        .msg_id = COM_HOST_MSG_ID_PING,
        .data = NULL,
        .data_len = 0,
        .need_ack = 1,
        .is_ack = 0
    };
    
    uint8_t seq;
    int ret = com_host_send_msg(g_host, COM_HOST_ID_MOBOILE_APP, &msg, &seq);
    if (ret == 0) {
        printf("PING sent, seq=%d\n", seq);
    }
}

// 发送带数据的命令
void send_set_effect_cmd(void) {
    // 假设效果参数
    uint8_t effect_data[16] = {0x01, 0x02, 0x03, ...};
    
    com_host_msg_t msg = {
        .msg_id = COM_HOST_MSG_ID_CMD_SET_VOICE_EFFECT,
        .data = effect_data,
        .data_len = sizeof(effect_data),
        .need_ack = 1,
        .is_ack = 0
    };
    
    com_host_send_msg(g_host, COM_HOST_ID_SUB_CONTROLLER_1, &msg, NULL);
}
```

---

### 示例3：处理带数据的消息

```c
void effect_cmd_handler(com_host_handle_t host, com_host_port_handle_t port,
                        com_host_id_t sender_id, com_host_msg_t* msg,
                        uint8_t seq, void* ctx) {
    
    // 解析消息数据
    if (msg->data_len >= sizeof(com_host_msg_cmd_set_voice_effect_data_t)) {
        com_host_msg_cmd_set_voice_effect_data_t* cmd = 
            (com_host_msg_cmd_set_voice_effect_data_t*)msg->data;
        
        printf("Effect type: %d\n", cmd->effect_type);
        printf("Effect enabled: %d\n", cmd->enabled);
        
        // 执行设置...
        
        // 发送ACK
        if (msg->need_ack) {
            com_host_msg_cmd_voice_effect_set_ack_data_t ack_data = {
                .result = COM_HOST_MSG_RET_OK
            };
            com_host_send_ack(host, port, sender_id, msg,
                             (uint8_t*)&ack_data, sizeof(ack_data), seq);
        }
    }
}
```

---

### 示例4：超时检测

```c
void check_connection_timeout(void) {
    uint32_t last_time = com_host_last_recv_time(g_host, COM_HOST_ID_MOBOILE_APP);
    uint32_t current_time = get_time_ms();
    
    if (last_time > 0 && (current_time - last_time) > 5000) {
        printf("Mobile app connection timeout!\n");
        // 执行超时处理...
    }
}
```

---

## 配置说明

所有配置项位于 `com_host_cfg.h` 文件中。

### 日志配置

```c
#define COM_HOST_DEBUG_LOG(...)  printf("[DEBUG] %d :", com_host_ll_get_sys_time_ms());printf(__VA_ARGS__);printf("\r\n")
#define COM_HOST_INFO_LOG(...)   printf("[INFO] %d :", com_host_ll_get_sys_time_ms());printf(__VA_ARGS__);printf("\r\n")
#define COM_HOST_WARN_LOG(...)   printf("[WARN] %d :", com_host_ll_get_sys_time_ms());printf(__VA_ARGS__);printf("\r\n")
#define COM_HOST_ERROR_LOG(...)  printf("[ERROR] %d :", com_host_ll_get_sys_time_ms());printf(__VA_ARGS__);printf("\r\n")
```

可根据需要重定向到其他日志系统（需包含 `com_host_ll_systime.h` 或通过 `com_host_cfg.h` 间接包含）。

---

### 端口配置

```c
// 端口接收超时时间（毫秒）
#define COM_HOST_PORT_RECV_TIMEOUT_MS  (200)
```

---

### 打包/解包配置

```c
// 解包超时时间（毫秒）
#define COM_HOST_PACKAGER_UNPACK_TIMEOUT_MS  (200)

// 单次解包数据量（字节）
#define UNPACK_ONCE_SIZE  (256)

// 单次解包最大轮询次数
#define UNPACK_MAX_POLLING_CNT  (10)

// 多消息发送时，单次最大发送消息数量
#define MULTI_MSG_SEND_MAX_CNT  (128)
```

**调整建议：**
- 资源受限设备：减小 `UNPACK_ONCE_SIZE` 以节省栈空间
- 高性能需求：增大 `UNPACK_ONCE_SIZE` 减少解包次数

---

### 路由配置

```c
// 启用路由转发功能
#define COM_HOST_FORWARD_ROUTE_ENABLE  (1)

// 启用最大转发次数限制
#define COM_HOST_ENABLE_MAX_ROUTE_FORWARD_ENABLE  (1)

// 最大路由转发次数（0~17）
#define COM_HOST_MAX_ROUTE_FORWARD_CNT  (8)

// 启用智能添加路由功能
#define COM_HOST_AUTO_ADD_ROUTE_ENABLE  (1)

// 启用包转发次数自增功能
#define COM_HOST_ENABLE_PKG_FORWARD_CNT_INC  (1)
```

**配置说明：**
- `COM_HOST_FORWARD_ROUTE_ENABLE`: 是否支持路由转发（非叶子节点需要）
- `COM_HOST_AUTO_ADD_ROUTE_ENABLE`: 是否自动学习路由（推荐开启）
- `COM_HOST_MAX_ROUTE_FORWARD_CNT`: 防止环路的最大跳数

### 消息处理配置

```c
// 单个 msg_id 是否支持多个处理函数（0：支持多 handler；1：仅允许单 handler）
#define COM_HOST_MULTI_MSG_HANDLER_ENABLE  (1)
```

---

## 常见问题

### Q1: 消息发送失败，返回 -1？

**可能原因：**
1. 未添加到目标主机的路由
2. 端口未正确初始化
3. 端口写函数返回错误

**解决方法：**
```c
// 检查路由
com_host_add_route(host, target_id, port);

// 检查日志输出，确认路由是否添加成功
```

---

### Q2: 收不到消息？

**排查步骤：**
1. 确认 `com_host_run()` 在主循环中被定期调用
2. 检查端口的读函数是否正确读取数据
3. 检查物理层是否正常接收数据
4. 启用DEBUG日志，查看解包过程

```c
// 在com_host_cfg.h中启用详细日志
#define COM_HOST_DEBUG_LOG(...)  printf(__VA_ARGS__)
```

---

### Q3: 如何判断消息是否被成功接收？

**方法1：使用ACK机制**
```c
msg.need_ack = 1; // 发送时要求ACK
com_host_send_msg(host, target_id, &msg, &seq);

// 在消息处理函数中检查 msg->is_ack
void handler(...) {
    if (msg->is_ack) {
        printf("Received ACK for seq %d\n", seq);
    }
}
```

**方法2：超时检测**
```c
uint32_t last_time = com_host_last_recv_time(host, target_id);
if (current_time - last_time > TIMEOUT) {
    // 超时处理
}
```

---

### Q4: 内存不足怎么办？

**优化方法：**
1. 减小栈缓冲区大小
2. 减小端口的打包/解包缓冲区
3. 减小 `UNPACK_ONCE_SIZE`
4. 使用 `com_host_get_mem_info()` 监控内存使用

```c
uint32_t total, used;
com_host_get_mem_info(host, &total, &used);
printf("Memory: %d/%d bytes\n", used, total);
```

---

### Q5: 如何支持多个物理接口？

为每个物理接口添加一个端口：

```c
// UART端口
com_host_port_desc_t uart_desc = {
    .name = "uart1",
    .write = uart_write,
    .read = uart_read,
    ...
};
com_host_port_handle_t uart_port = com_host_add_port(host, &uart_desc);

// USB端口
com_host_port_desc_t usb_desc = {
    .name = "usb",
    .write = usb_write,
    .read = usb_read,
    ...
};
com_host_port_handle_t usb_port = com_host_add_port(host, &usb_desc);

// 可为同一目标添加多个路径
com_host_add_route(host, COM_HOST_ID_MOBOILE_APP, uart_port);
com_host_add_route(host, COM_HOST_ID_MOBOILE_APP, usb_port);
```

---

### Q6: 如何处理未知消息ID？

注册默认消息处理函数：

```c
void default_handler(com_host_handle_t host, com_host_port_handle_t port,
                     com_host_id_t sender_id, com_host_msg_t* msg,
                     uint8_t seq, void* ctx) {
    printf("Unknown message ID: 0x%04X from 0x%02X\n", msg->msg_id, sender_id);
    
    // 可选：返回"不支持"的ACK
    if (msg->need_ack) {
        uint8_t ack = COM_HOST_MSG_RET_NOT_FOUND;
        com_host_send_ack(host, port, sender_id, msg, &ack, 1, seq);
    }
}

com_host_add_default_msg_handler(host, default_handler, NULL);
```

---

### Q7: 如何实现请求-响应模式？

**方法：使用序列号匹配**

```c
// 全局变量记录等待的ACK
static uint8_t waiting_seq = 0;
static uint8_t ack_received = 0;

// 发送请求
void send_request(void) {
    com_host_msg_t msg = {
        .msg_id = COM_HOST_MSG_ID_REQ_DEVICE_INFO,
        .need_ack = 1,
        .is_ack = 0
    };
    
    com_host_send_msg(host, target_id, &msg, &waiting_seq);
    ack_received = 0;
}

// 处理响应
void handler(com_host_handle_t host, com_host_port_handle_t port,
             com_host_id_t sender_id, com_host_msg_t* msg,
             uint8_t seq, void* ctx) {
    
    if (msg->is_ack && seq == waiting_seq) {
        ack_received = 1;
        // 处理响应数据...
    }
}

// 等待响应
void wait_response(void) {
    uint32_t start = get_time_ms();
    while (!ack_received && (get_time_ms() - start) < 1000) {
        com_host_run(host);
        HAL_Delay(1);
    }
    
    if (ack_received) {
        printf("Response received\n");
    } else {
        printf("Timeout\n");
    }
}
```

---

### Q8: 如何调试协议问题？

**1. 启用详细日志**
```c
#define COM_HOST_DEBUG_LOG(...)  printf(__VA_ARGS__)
```

**2. 使用串口监听工具**
- 使用逻辑分析仪或串口助手监听物理层数据
- 验证CRC校验码
- 检查帧格式

**3. 打印消息内容**
```c
void debug_print_msg(com_host_msg_t* msg) {
    printf("MSG_ID: 0x%04X, Len: %d\n", msg->msg_id, msg->data_len);
    printf("Data: ");
    for (int i = 0; i < msg->data_len; i++) {
        printf("%02X ", msg->data[i]);
    }
    printf("\n");
}
```

---

## 附录

### 主机ID定义

| ID范围 | 用途 | 数量 |
|--------|------|------|
| 0x01~0x0F | 主控/子控设备 | 15个 |
| 0x10~0x17 | 配件设备（动态分配） | 8个 |
| 0xE0~0xEF | 客户端 | 16个 |
| 0xF0~0xFE | 开发者/测试/工厂 | 15个 |

### 常用消息ID

| ID | 名称 | 说明 |
|----|------|------|
| 0x0000 | PING | 心跳/连接测试 |
| 0x0001 | REQ_PRODUCT_INFO | 请求产品信息 |
| 0x0002 | REQ_DEVICE_INFO | 请求设备信息 |
| 0x0003 | CMD_REBOOT | 重启命令 |
| 0x0010 | REQ_VOICE_EFFECTOR_BRIEF | 请求效果器简要信息 |
| 0x0013 | CMD_SET_VOICE_EFFECT | 设置效果参数 |

完整列表见 `protocol/com_msg_id_def.h`；消息数据结构见 `protocol/com_msg_data_def.h`、`protocol/com_msg_music_staff_def.h`。

---

### 返回码定义

| 值 | 名称 | 说明 |
|----|------|------|
| 0x00 | OK | 成功 |
| 0x01 | ERROR | 通用错误 |
| 0x02 | TIMEOUT | 超时 |
| 0x03 | NOT_FOUND | 未找到 |
| 0x04 | BUSY | 忙 |
| 0x05 | INVALID | 无效参数 |
| 0x06 | PERMISSION_DENIED | 权限不足 |
| 0x07 | STATE_ERROR | 状态错误 |
| 0x08 | MEMORY_NOT_ENOUGH | 内存不足 |

---

## 技术支持

如有问题，请参考：
- 源码注释
- `example/` 目录下的示例代码
- `docs/` 目录下的其他文档

---

**文档版本**: 1.1  
**更新日期**: 2026-03-09  
**库版本**: 1.0.8 | **协议版本**: 1.0.8
