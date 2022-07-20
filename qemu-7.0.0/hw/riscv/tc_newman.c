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
#include "hw/intc/riscv_aclint.h"
#include "hw/intc/sifive_plic.h"
#include "chardev/char.h"
#include "sysemu/arch_init.h"
#include "sysemu/device_tree.h"
#include "sysemu/sysemu.h"

//定义内存空间,CLINT:Core Local Interruptor
static const MemMapEntry virt_memmap[] = {
    [TC_NEWMAN_MROM]    = {        0x0,      0x8000 },
    [TC_NEWMAN_SRAM]    = {     0x8000,      0x8000 },
    [TC_NEWMAN_CLINT]   = {  0X2000000,      0X10000},    
    [TC_NEWMAN_PLIC]    = {  0XC000000,      TC_NEWMAN_PLIC_SIZE(TC_NEWMAN_CPUS_MAX * 2)},//所有的core共用外设控制器
    [TC_NEWMAN_UART0]   = { 0x10000000,       0x100 },
    [TC_NEWMAN_UART1]   = { 0x10001000,       0x100 },
    [TC_NEWMAN_UART2]   = { 0x10002000,       0x100 },
    [TC_NEWMAN_FLASH]   = { 0x20000000,   0x2000000 },
    [TC_NEWMAN_DRAM]    = { 0x80000000,         0x0 },
};

#define TC_NEWMAN_FLASH_SECTOR_SIZE (256 * KiB)
//创建pflash的子函数
static PFlashCFI01 *tc_newman_flash_create(TCNEWMANState *s,
                                       const char *name,
                                       const char *alias_prop_name)//MACHINE object,pflash的名字,pflash别名
{
    DeviceState *dev = qdev_new(TYPE_PFLASH_CFI01);

    qdev_prop_set_uint64(dev, "sector-length", TC_NEWMAN_FLASH_SECTOR_SIZE);
    qdev_prop_set_uint8(dev, "width", 4);
    qdev_prop_set_uint8(dev, "device-width", 2);
    qdev_prop_set_bit(dev, "big-endian", false);
    qdev_prop_set_uint16(dev, "id0", 0x89);
    qdev_prop_set_uint16(dev, "id1", 0x18);
    qdev_prop_set_uint16(dev, "id2", 0x00);
    qdev_prop_set_uint16(dev, "id3", 0x00);
    qdev_prop_set_string(dev, "name", name);

    object_property_add_child(OBJECT(s), name, OBJECT(dev));
    object_property_add_alias(OBJECT(s), alias_prop_name,
                              OBJECT(dev), "drive");

    return PFLASH_CFI01(dev);
}

//将flash所开辟的空间加入到system_memory
static void tc_newman_flash_map(PFlashCFI01 *flash,
                            hwaddr base, hwaddr size,
                            MemoryRegion *sysmem)
{
    DeviceState *dev = DEVICE(flash);

    //assert是一个宏，称为 断言。如果它的条件返回错误，则终止程序执行。
    assert(QEMU_IS_ALIGNED(size, TC_NEWMAN_FLASH_SECTOR_SIZE));//QEMU_IS_ALIGNED(n,m);Check if n is a multiple of m; flash要求能被整数划分
    assert(size / TC_NEWMAN_FLASH_SECTOR_SIZE <= UINT32_MAX);
    qdev_prop_set_uint32(dev, "num-blocks", size / TC_NEWMAN_FLASH_SECTOR_SIZE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);//与qdev_realize()类似，区别在于此函数会将引用去除，仅用于私有引用

    memory_region_add_subregion(sysmem, base,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(dev),
                                                       0));
}

//risc-v启动代码,根据文件 hw/riscv/boot.c中的代码而来。去掉了fdt，FDT：Flattened Device Tree即扁平设备树。hart指的是(hardware thread 硬件线程)
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
    TCNEWMANState *s = TC_NEWMAN_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *main_mem = g_new(MemoryRegion, 1);
    MemoryRegion *sram_mem = g_new(MemoryRegion, 1);
    MemoryRegion *mask_rom = g_new(MemoryRegion, 1);//开辟空间
    int i, j, base_hartid, hart_count;
    char *plic_hart_config, *soc_name;//外设中断控制器的配置字符串，soc的名字
    size_t plic_hart_config_len;//config的字符串的长度
    DeviceState *mmio_plic=NULL;//用于初始化串口的设备对象

    //检查CPU插槽是否超过定义的最大数
    if (TC_NEWMAN_SOCKETS_MAX < riscv_socket_count(machine)) {
        error_report("number of sockets/nodes should be less than %d",
            TC_NEWMAN_SOCKETS_MAX);
        exit(1);
    }

    //根据CPU的线程数建立中断控制器和cpu的初始化
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

        hart_count = riscv_socket_hart_count(machine, i);//TODO:调试出是否为默认的cpu数，也即hart数
        if (hart_count < 0) {
            error_report("can't find hart count for socket%d", i);
            exit(1);
        }

        soc_name = g_strdup_printf("soc%d", i);
        object_initialize_child(OBJECT(machine), soc_name, &s->soc[i],
                                TYPE_RISCV_HART_ARRAY);
        g_free(soc_name);
        //将str类型的变量写入对象的属性中
        object_property_set_str(OBJECT(&s->soc[i]), "cpu-type",
                                machine->cpu_type, &error_abort);
        object_property_set_int(OBJECT(&s->soc[i]), "hartid-base",
                                base_hartid, &error_abort);
        object_property_set_int(OBJECT(&s->soc[i]), "num-harts",
                                hart_count, &error_abort);
        sysbus_realize(SYS_BUS_DEVICE(&s->soc[i]), &error_abort);//实现设备，设备开始运行

        //创建Core Local Interruptor (CLINT) 局部中断控制器
        riscv_aclint_mtimer_create(
            memmap[TC_NEWMAN_CLINT].base + i * memmap[TC_NEWMAN_CLINT].size,
                memmap[TC_NEWMAN_CLINT].size, base_hartid, hart_count,
                RISCV_ACLINT_DEFAULT_MTIMECMP, RISCV_ACLINT_DEFAULT_MTIME,
                RISCV_ACLINT_DEFAULT_TIMEBASE_FREQ, true);
        //Platform-Level Interrupt Controller (PLIC)配置字符串初始化
        //* Per-socket PLIC hart topology configuration string */
        plic_hart_config_len =
        (strlen(TC_NEWMAN_PLIC_HART_CONFIG) + 1) * hart_count;
        plic_hart_config = g_malloc0(plic_hart_config_len);//开辟空间，给每一个线程都开辟了一个

        for (j = 0; j < hart_count; j++) {
            if (j != 0) {
                strncat(plic_hart_config, ",", plic_hart_config_len);
            }
            strncat(plic_hart_config, TC_NEWMAN_PLIC_HART_CONFIG,
                plic_hart_config_len);
            plic_hart_config_len -= (strlen(TC_NEWMAN_PLIC_HART_CONFIG) + 1);
        }

        //按照参数要求传参，建立platform中断控制器
        s->plic[i] = sifive_plic_create(
            memmap[TC_NEWMAN_PLIC].base + i * memmap[TC_NEWMAN_PLIC].size,
            plic_hart_config, base_hartid,
            base_hartid,
            TC_NEWMAN_PLIC_NUM_SOURCES,
            TC_NEWMAN_PLIC_NUM_PRIORITIES,
            TC_NEWMAN_PLIC_PRIORITY_BASE,
            TC_NEWMAN_PLIC_PENDING_BASE,
            TC_NEWMAN_PLIC_ENABLE_BASE,
            TC_NEWMAN_PLIC_ENABLE_STRIDE,
            TC_NEWMAN_PLIC_CONTEXT_BASE,
            TC_NEWMAN_PLIC_CONTEXT_STRIDE,
            memmap[TC_NEWMAN_PLIC].size);
        g_free(plic_hart_config);

        if (i == 0) {
            mmio_plic = s->plic[i];
        }
    }

    //加载maskrom的固件到mrom区域，初始化各类mem
    /*register system main memory (actual RAM)*/
    memory_region_init_ram(main_mem, NULL, "riscv_tc_newman_board.dram",
                           machine->ram_size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[TC_NEWMAN_DRAM].base,
        main_mem);//用于注册到系统memory中

    
    memory_region_init_ram(sram_mem, NULL, "riscv_tc_newman_board.sram",
                           memmap[TC_NEWMAN_SRAM].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[TC_NEWMAN_SRAM].base,
        sram_mem);

    /* boot rom */
    memory_region_init_rom(mask_rom, NULL, "riscv_tc_newman_board.mrom",
                           memmap[TC_NEWMAN_MROM].size, &error_fatal);
    memory_region_add_subregion(system_memory, memmap[TC_NEWMAN_MROM].base,
                                mask_rom);

    tc_newman_setup_rom_reset_vec(machine, &s->soc[0], memmap[TC_NEWMAN_FLASH].base,//将向量引导到FLASH中，可以在flash写入一些代码，用于测试
                              memmap[TC_NEWMAN_MROM].base,
                              memmap[TC_NEWMAN_MROM].size,
                              0x0, 0x0);
    
    //创建串口示例，仿真ns16550a的定义，后续的opensbi，u-boot，kernel中都带有这个串口的驱动
    serial_mm_init(system_memory, memmap[TC_NEWMAN_UART0].base,
        0, qdev_get_gpio_in(DEVICE(mmio_plic), TC_NEWMAN_UART0_IRQ), 399193,
        serial_hd(0), DEVICE_LITTLE_ENDIAN);
    serial_mm_init(system_memory, memmap[TC_NEWMAN_UART1].base,
        0, qdev_get_gpio_in(DEVICE(mmio_plic), TC_NEWMAN_UART1_IRQ), 399193,
        serial_hd(1), DEVICE_LITTLE_ENDIAN);
    serial_mm_init(system_memory, memmap[TC_NEWMAN_UART2].base,
        0, qdev_get_gpio_in(DEVICE(mmio_plic), TC_NEWMAN_UART2_IRQ), 399193,
        serial_hd(2), DEVICE_LITTLE_ENDIAN);

    //创建pflash
    s->flash = tc_newman_flash_create(s, "tc-newman.flash0", "pflash0");
    pflash_cfi01_legacy_drive(s->flash, drive_get(IF_PFLASH, 0, 0));
    tc_newman_flash_map(s->flash, memmap[TC_NEWMAN_FLASH].base,
                         memmap[TC_NEWMAN_FLASH].size, system_memory);
}

static void tc_newman_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);//QOM中使用宏对对象类型进行转换,MACHINE使用的是隐式转换函数OBJECT_DECLARE_TYPE，所以直接找这个定义是找不到的
    mc->desc = "RISC-V Tc newman board";//设备描述
    mc->init = tc_newman_machine_init;  //虚函数的实现
    mc->max_cpus = TC_NEWMAN_CPUS_MAX;//定义了逻辑CPU的最大数目
    mc->default_cpu_type = TYPE_RISCV_CPU_BASE;//宏定义的CPU名称
    mc->pci_allow_0_address = true;
    mc->possible_cpu_arch_ids = riscv_numa_possible_cpu_arch_ids;//NUMA nodes：一个Sockets可以划分为多个NUMA node。Numa使用node来管理CPU和内存。，但其他RISC-V厂商实现都依赖numa这类函数
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

type_init(tc_newman_machine_init_register_types)//宏处理，在执行main之前就预先将类加载好，可以视作初始化
