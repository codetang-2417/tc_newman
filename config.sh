#!/bin/sh
SHELL_FOLDER=$(cd "$(dirname "$0")";pwd)
cd qemu-7.0.0
if [ ! -d "$SHELL_FOLDER/output/qemu" ];then
	./configure --prefix=$SHELL_FOLDER/output/qemu --target-list=riscv64-softmmu --enable-gtk --enable-virtfs --disable-gio
fi
make -j16
make install
cd ..

CROSS_PREFIX=/opt/riscv64-lp64d--glibc--stable-2021.11-1/bin/riscv64-linux
if [ ! -d "$SHELL_FOLDER/output/lowlevelboot" ]; then  
mkdir $SHELL_FOLDER/output/lowlevelboot
fi
cd lowlevelboot
# 编译汇编文件startup.s到obj文件
$CROSS_PREFIX-gcc -x assembler-with-cpp -c startup.s -o $SHELL_FOLDER/output/lowlevelboot/startup.o
# 使用链接脚本链接obj文件生成elf可执行文件
$CROSS_PREFIX-gcc -nostartfiles -T./boot.lds -Wl,-Map=$SHELL_FOLDER/output/lowlevelboot/lowlevel_fw.map -Wl,--gc-sections $SHELL_FOLDER/output/lowlevelboot/startup.o -o $SHELL_FOLDER/output/lowlevelboot/lowlevel_fw.elf
# 使用gnu工具生成原始的程序bin文件
$CROSS_PREFIX-objcopy -O binary -S $SHELL_FOLDER/output/lowlevelboot/lowlevel_fw.elf $SHELL_FOLDER/output/lowlevelboot/lowlevel_fw.bin
# 使用gnu工具生成反汇编文件，方便调试分析（当然我们这个代码太简单，不是很需要）
$CROSS_PREFIX-objdump --source --demangle --disassemble --reloc --wide $SHELL_FOLDER/output/lowlevelboot/lowlevel_fw.elf > $SHELL_FOLDER/output/lowlevelboot/lowlevel_fw.lst
#制作固件详细见文档
cd $SHELL_FOLDER/output/lowlevelboot
rm -rf fw.bin
dd of=fw.bin bs=1k count=32k if=/dev/zero
dd of=fw.bin bs=1k conv=notrunc seek=0 if=lowlevel_fw.bin
cd $SHELL_FOLDER