#!/bin/sh
shell_folder=$(cd "$(dirname "$0")";pwd)
cd qemu-7.0.0
if [ ! -d "$shell_folder/output/qemu" ];then
	./configure --prefix=$shell_folder/output/qemu --target-list=riscv64-softmmu --enable-gtk --enable-virtfs --disable-gio
fi
make -j16
make install
cd ..
