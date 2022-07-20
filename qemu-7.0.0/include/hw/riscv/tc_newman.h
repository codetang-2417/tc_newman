/*
 * QEMU RISC-V tc Board
 *
 * Copyright (c) 2022 ttc
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_RISCV_TC_NEWMAN__H
#define HW_RISCV_TC_NEWMAN__H

#include "hw/riscv/riscv_hart.h"
#include "hw/sysbus.h"
#include "hw/block/flash.h"
#include "qom/object.h"

#define TC_NEWMAN_CPUS_MAX 8
#define TC_NEWMAN_SOCKETS_MAX 2

#define TYPE_RISCV_TC_NEWMAN_MACHINE MACHINE_TYPE_NAME("tc-newman")//利用宏生成一个字符串作为类对象的名字，看做是type名，代表将要注册的类对象。这是QOM的定义格式
typedef struct TCNEWMANState TCNEWMANState;//前置声明
DECLARE_INSTANCE_CHECKER(TCNEWMANState, TC_NEWMAN_MACHINE,
                         TYPE_RISCV_TC_NEWMAN_MACHINE)

struct TCNEWMANState {
    /*< private >*/
    MachineState parent;//父对象

    /*< public >*/
    RISCVHartArrayState soc[TC_NEWMAN_SOCKETS_MAX];//通过实例化对象，产生实际的对象数组，作为CPU
    DeviceState *plic[TC_NEWMAN_SOCKETS_MAX];//外设中断控制器
    PFlashCFI01 *flash;
};

//枚举类型，便于代码阅读，定义内存空间时使用
enum {
    TC_NEWMAN_MROM,
    TC_NEWMAN_SRAM,
    TC_NEWMAN_CLINT,
    TC_NEWMAN_PLIC,
    TC_NEWMAN_UART0,
    TC_NEWMAN_UART1,
    TC_NEWMAN_UART2,
    TC_NEWMAN_FLASH,
    TC_NEWMAN_DRAM,
};

//定义了中断号
enum {
    TC_NEWMAN_UART0_IRQ = 10,
    TC_NEWMAN_UART1_IRQ = 11,
    TC_NEWMAN_UART2_IRQ = 12,
};

#define TC_NEWMAN_PLIC_HART_CONFIG    "MS"
#define TC_NEWMAN_PLIC_NUM_SOURCES    127   ///* Arbitrary maximum number of interrupts */
#define TC_NEWMAN_PLIC_NUM_PRIORITIES 7
#define TC_NEWMAN_PLIC_PRIORITY_BASE  0x04
#define TC_NEWMAN_PLIC_PENDING_BASE   0x1000
#define TC_NEWMAN_PLIC_ENABLE_BASE    0x2000
#define TC_NEWMAN_PLIC_ENABLE_STRIDE  0x80
#define TC_NEWMAN_PLIC_CONTEXT_BASE   0x200000
#define TC_NEWMAN_PLIC_CONTEXT_STRIDE 0x1000
#define TC_NEWMAN_PLIC_SIZE(__num_context) \
    (TC_NEWMAN_PLIC_CONTEXT_BASE + (__num_context) * TC_NEWMAN_PLIC_CONTEXT_STRIDE)

#endif