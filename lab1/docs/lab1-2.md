# OSH 2019 Lab1
刘紫檀 PB17000232

## `/dev/sdc1`中用到了什么文件系统？
FAT32。不可以换成其它文件系统，因为固件里面的`Stage 1 bootloader`只认 FAT32。

## kernel 为什么会加载 `/dev/sdc2` 的 init
在 `/dev/sdc1` 中的 `cmdline.txt` 中有传递到 kernel 的选项，其中有`root=PARTUUID=7ee80803-02 rootfstype=ext4` 用来挂载 `/` 分区；`init=/init` 用来指定 init 是 `/` 分区下的 `init` 程序。

## 正常工作必须打开的选项
非常多..时间有限不能一一尝试。

- Processor Type 正确选择
- Block device & char device support
- 各种设备驱动
- GPIO
- 等等等等

我关掉的选项有：
- CONFIG_MD (RAID & LVM)
- Network
- iSCSI Drivers
- Radio Adapters
- Sound card support
- Miscellaneous Filesystems (eCrypt, Apple HFS, etc)
- KGDB
- SMP
- USB

## Linux Kernel 为什么会 panic
[https://unix.stackexchange.com/questions/195889/linux-kernel-action-upon-init-process-exiting](https://unix.stackexchange.com/questions/195889/linux-kernel-action-upon-init-process-exiting
)

里面说明了 init 的退出导致 Kernel Panic 的代码在`kernel/exit.c`中。

从功能性的角度来说，`init`负责启动其它用户态进程，而没有`init`的内核并不知道做什么，所以`panic`是最好的选择。