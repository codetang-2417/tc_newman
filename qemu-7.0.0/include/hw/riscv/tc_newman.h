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
#include "qom/object.h"

#define TC_NEWMAN_CPUS_MAX 8
#define TC_NEWMAN_SOCKETS_MAX 2

#define TYPE_RISCV_TC_NEWMAN_MACHINE MACHINE_TYPE_NAME("tc-newman")//利用宏生成一个字符串作为类对象的名字，看做是type名，代表将要注册的类对象。这是QOM的定义格式
typedef struct TCNEWMANState TCNEWMANState;//前置声明
DECLARE_INSTANCE_CHECKER(TCNEWMANState, RISCV_VIRT_MACHINE,
                         TYPE_RISCV_TC_NEWMAN_MACHINE)

struct TCNEWMANState {
    /*< private >*/
    MachineState parent;//父对象

    /*< public >*/
    RISCVHartArrayState soc[TC_NEWMAN_SOCKETS_MAX];//通过实例化对象，产生实际的对象数组，作为CPU
};

//枚举类型，便于代码阅读，定义内存空间时使用
enum {
    TC_NEWMAN_MROM,
    TC_NEWMAN_SRAM,
    TC_NEWMAN_UART0,
    TC_NEWMAN_DRAM,
};

//定义了中断号
enum {
    TC_NEWMAN_UART0_IRQ = 10,
};


#endif