# OSH 2019 Lab1
刘紫檀 PB17000232

## Raspberry Pi 的启动过程

Ref: [Raspberry Pi Bootmodes Documentation](https://www.raspberrypi.org/documentation/hardware/raspberrypi/bootmodes/README.md)

大致流程概括如下：
- BCM2837 启动
- 从 OTP （仅一次可编程的「寄存器」） 读取引导模式选择
- 如果编程过`program_gpio_bootmode`选项，就读一些`GPIO`，根据读出的电平决定某些引导模式的开启/关闭
- 如果 SD1 引导被打开：检查主 SD 有没有`bootcode.bin`（GPIO 48-53）；如果成功则引导

下面略了，就 SD2 NAND SPI USB (Mass Storage Device 就找`bootcode.bin`，以太网芯片就等 PXE 网启)

所以`bootcode.bin`是重点，下面继续发掘。

### bootcode.bin

Ref: [Licenses to `bootcode.bin`, `start*.elf` and `fixup*.dat`](https://github.com/raspberrypi/firmware/blob/master/boot/LICENCE.broadcom)   ~~Broadcom 并不开源，告辞~~

Ref: [Understanding RaspberryPi Boot Process](https://wiki.beyondlogic.org/index.php?title=Understanding_RaspberryPi_Boot_Process) （注意这个文章是针对 BCM2835 的，所以有点过时，不过似乎问题不大）

第二阶段的 bootloader（`bootcode.bin`） 运行在 GPU 上，用来装载`start.elf`。 

第三阶段的 bootloader（`start.elf`）读取`config.txt`。`config.txt` 包含 GPU 设定和装载 Linux Kernel 需要的信息。

### config.txt

由于缺乏`start.elf`之源码，基本上作为用户只能通过修改`config.txt`来更改树莓派的行为。

`config.txt`中所列设置可以参照[这个](https://www.raspberrypi.org/documentation/configuration/config-txt/)。

### 文件系统

Ref: [Standalone partitioning explained](https://github.com/raspberrypi/noobs/wiki/Standalone-partitioning-explained)

GPU 固件（Stage 1）应该是直接烧录进去的，所以不会有文件系统。

由于 Stage 1 的固件只能认 FAT32，所以必须将 SD 卡中第一个分区格式化为 FAT32 分区，并放入`bootcode.bin`，`start*.elf` 等文件。

而传统上 Linux 需要一个分区作为根分区；有 FAT32 驱动的话 FAT32 应该也可胜任，但是 FAT32 不支持权限是硬伤。所以此处需要一个 ext4 分区。

综上，一般将 SD 卡分为两个分区。

## Raspberry Pi 和主流计算机启动有何不同

主流计算机的启动通常意味着复杂的初始化过程，这一复杂过程由 BIOS (Legacy / UEFI) 实现。

Ref: [UEFI PI 分析](https://zhuanlan.zhihu.com/p/25941340)

![UEFI PI](/home/libreliu/RDMA/Os/OSH-2019-Labs/docs/uefi_pi.jpg)

> 核心芯片初始化依赖于分阶段的方法。在初始系统复位时，仅有部分非常有限的硬件资源可用；不能访问设备，不能访问存储器等等限制。这才导致了随着硬件的初始化以及可用资源的增加，BIOS为软件的正常运行不断地改变操作环境。在PI中：
>
> - 在 SEC 阶段，系统从复位开始运行（由主机引导处理器取回的第一指令），通过初始化处理器高速缓存（Cache）来作为临时内存使用，我们有了堆栈，从而可以执行C程序，然后转到PEI阶段。
> - 在 PEI 最开始阶段，仅少量栈和堆可用，我们需要找到并使能足够我们使用的永久内存，通常是内存颗粒或内存条（DIMM），然后转入DXE。
>   DXE阶段有了永久存储空间，真正开始负责初始化核心芯片，然后转换到 BDS 阶段。
> - 核心芯片初始化完成后BDS阶段开始，并继续初始化引导操作系统（输入，输出和存储设备）所需的硬件。 纵观PI的整个阶段，BDS对应的是“执行UEFI驱动程序模型”来引导 OS 这一过程。
>
> 但是，x86 计算机和 Raspberry Pi 有一点显著不同：**在 Raspberry Pi 上，bootloader 的第一阶段是 GPU 执行的；而在 x86 上为 CPU 执行**。除此之外，**为了适配各种不同的设备，UEFI 设计更为复杂**。



