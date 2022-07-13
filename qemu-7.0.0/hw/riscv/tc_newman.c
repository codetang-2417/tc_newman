/*
 * QEMU RISC-V Board tc
 *
 * Copyright (c) 2022 ttc
 *
 * Provides a board compatible with the OpenTitan FPGA platform:
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

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/char/serial.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/tc_newman.h"
#include "hw/riscv/boot.h"
#include "hw/riscv/numa.h"
#include "chardev/char.h"
#include "sysemu/arch_init.h"
#include "sysemu/device_tree.h"
#include "sysemu/sysemu.h"


static const MemMapEntry virt_memmap[] = {
    [TC_NEWMAN_MROM]    = {        0x0,      0x8000 },
    [TC_NEWMAN_SRAM]    = {     0x8000,      0x8000 },
    [TC_NEWMAN_UART0]   = { 0x10000000,       0x100 },
    [TC_NEWMAN_DRAM]    = { 0x80000000,         0x0 },
};

static void tc_newman_setup_rom_reset_vec(MachineState *machine, RISCVHartArrayState *harts,
                               hwaddr start_addr,
                               hwaddr rom_base, hwaddr rom_size,
                               uint64_t kernel_entry,
                               uint32_t fdt_load_addr)
{
    int i;
    uint32_t start_addr_hi32 = 0x00000000;

    if (!riscv_is_32bit(harts)) {
        start_addr_hi32 = start_addr >> 32;
    }
    /* reset vector */
    uint32_t reset_vec[10] = {
        0x00000297,                  /* 1:  auipc  t0, %pcrel_hi(fw_dyn) */
        0x02828613,                  /*     addi   a2, t0, %pcrel_lo(1b) */
        0xf1402573,                  /*     csrr   a0, mhartid  */
        0,
        0,
        0x00028067,                  /*     jr     t0 */
        start_addr,                  /* start: .dword */
        start_addr_hi32,
        fdt_load_addr,               /* fdt_laddr: .dword */
        0x00000000,
                                     /* fw_dyn: */
    };
    if (riscv_is_32bit(harts)) {
        reset_vec[3] = 0x0202a583;   /*     lw     a1, 32(t0) */
        reset_vec[4] = 0x0182a283;   /*     lw     t0, 24(t0) */
    } else {
        reset_vec[3] = 0x0202b583;   /*     ld     a1, 32(t0) */
        reset_vec[4] = 0x0182b283;   /*     ld     t0, 24(t0) */
    }

    /* copy in the reset vector in little_endian byte order */
    for (i = 0; i < ARRAY_SIZE(reset_vec); i++) {
        reset_vec[i] = cpu_to_le32(reset_vec[i]);
    }

    rom_add_blob_fixed_as("mrom.reset", reset_vec, sizeof(reset_vec),
                          rom_base, &address_space_memory);
}

static void tc_newman_machine_init(MachineState *machine)
{
    const MemMapEntry *memmap = virt_memmap;
    TCNEWMANState *s = RISCV_VIRT_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *main_mem = g_new(MemoryRegion, 1);
    MemoryRegion *sram_mem = g_new(MemoryRegion, 1);
    MemoryRegion *mask_rom = g_new(MemoryRegion, 1);
    int i, base_hartid, hart_count;
    char *soc_name;

    if (TC_NEWMAN_SOCKETS_MAX < riscv_socket_count(machine)) {
        error_report("number of sockets/nodes should be less than %d",
            TC_NEWMAN_SOCKETS_MAX);
        exit(1);
    }

    for (i = 0; i < riscv_socket_count(machine); i++) {
        if (!riscv_socket_check_hartids(machine, i)) {
            error_report("discontinuous hartids in socket%d", i);
            exit(1);
        }

        base_hartid = riscv_socket_first_hartid(machine, i);
        if (base_hartid < 0) {
            error_report("can't find hartid base for socket%d", i);
            exit(1);
        }

        hart_count = riscv_socket_hart_count(machine, i);
        if (hart_count < 0) {
            error_report("can't find hart count for socket%d", i);
            exit(1);
        }

        soc_name = g_strdup_printf("soc%d", i);
        object_initialize_child(OBJECT(machine), soc_name, &s->soc[i],
                                TYPE_RISCV_HART_ARRAY);
        g_free(soc_name);
        object_property_set_str(OBJECT(&s->soc[i]), "cpu-type",
                                machine->cpu_type, &error_abort);
        object_property_set_int(OBJECT(&s->soc[i]), "hartid-base",
                                base_hartid, &error_abort);
        object_property_set_int(OBJECT(&s->soc[i]), "num-harts",
                                hart_count, &error_abort);
        sysbus_realize(SYS_BUS_DEVICE(&s->soc[i]), &error_abort);
    }

    memory_region_init_ram(main_mem, NULL, "riscv_tc_newman_board.dram",
                           machine->ram_size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[TC_NEWMAN_DRAM].base,
        main_mem);

    memory_region_init_ram(sram_mem, NULL, "riscv_tc_newman_board.sram",
                           memmap[TC_NEWMAN_SRAM].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[TC_NEWMAN_SRAM].base,
        sram_mem);

    memory_region_init_rom(mask_rom, NULL, "riscv_tc_newman_board.mrom",
                           memmap[TC_NEWMAN_MROM].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[TC_NEWMAN_MROM].base,
                                mask_rom);

    tc_newman_setup_rom_reset_vec(machine, &s->soc[0], memmap[TC_NEWMAN_MROM].base,
                              virt_memmap[TC_NEWMAN_MROM].base,
                              virt_memmap[TC_NEWMAN_MROM].size,
                              0x0, 0x0);
}

static void tc_newman_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);//QOM中使用宏对对象类型进行转换,MACHINE使用的是隐式转换函数OBJECT_DECLARE_TYPE，所以直接找这个定义是找不到的
    mc->desc = "RISC-V Tc newman board";//设备名称
    mc->init = tc_newman_machine_init;  //对象初始化？
    mc->max_cpus = TC_NEWMAN_CPUS_MAX;
    mc->default_cpu_type = TYPE_RISCV_CPU_BASE;//宏定义的CPU名称
    mc->pci_allow_0_address = true;
    mc->possible_cpu_arch_ids = riscv_numa_possible_cpu_arch_ids;//不明确具体的含义，但其他RISC-V厂商实现都依赖numa这类函数
    mc->cpu_index_to_instance_props = riscv_numa_cpu_index_to_props;//同上
    mc->get_default_cpu_node_id = riscv_numa_get_default_cpu_node_id;//同上
    mc->numa_mem_supported = true;//

}

static void tc_newman_machine_instance_init(Object *obj)
{

}
static const TypeInfo tc_newman_machine_typeinfo = {
    .name       = TYPE_RISCV_TC_NEWMAN_MACHINE,//类对象名称，一般定义在头文件中
    .parent     = TYPE_MACHINE,//父类对象名称
    .class_init = tc_newman_machine_class_init,//实现虚函数
    .instance_init = tc_newman_machine_instance_init,//没有实际动作，目前
    .instance_size = sizeof(TCNEWMANState),//实例大小
};

static void tc_newman_machine_init_register_types(void)
{
    type_register_static(&tc_newman_machine_typeinfo);//宏处理的类注册函数
}

type_init(tc_newman_machine_init_register_types)//宏处理，在执行main之前就预先将类加载好
