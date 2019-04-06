# OSH 2019 Lab1
刘紫檀 PB17000232

## Comments
实现的时候不是很清楚「间隔2s」，所以我的行为是「点亮1s，熄灭1s」的循环。

## `/dev/sdc1` 文件分析
- `overlays/*`：Linux 内核中的 Device Tree Overlays，用于板级信息描述。有些只有二进制，没有源码。自己写裸机汇编的话，去掉没什么影响

- `*.dtb`：也是 Device Tree Overlays

- `bootcode.bin`：Stage 2 Bootloader，必须

- `start*.elf`：对应于不同的启动选项，默认是`start.elf`。`start_x.elf`在启动时会进行 Camera 有关的工作（我也不知道是什么）；`start_db.elf`用于 Debug，而`start_cd.elf`文档没有详细给出。如果不用对应的选项，应该不需要对应文件

- `fixup*.dat`：处理 VideoCore 显存的 Memory Split 的文件。如果不用对应的选项，应该不需要对应文件

- `config.txt`：用来放选项的配置文件，必须

- `cmdline.txt`：用来放内核启动选项的配置文件，原则上因为我们的 bare-metal 不需要选项，可以置空/删除（起码可以置空）

- `kernel7.img`：用于加载到内存的一段代码（Eg. Kernel），除非有`kernel8.img`，否则必须保留。如果`kernel8.img`存在，会优先加载`kernel8.img`并且进入 64 位模式

- 其它文件（License 等）：没卵用

## `/dev/sdc2`是否被用到？
显然没有。连 ext4 驱动都没写，还想读写`/dev/sdc2`，想得美（滑稽

## Usages
### as, ld 的作用
as 用来汇编（生成`object file`）。

ld 用来链接（生成 ELF），加上`--verbose`可以看到内部链接脚本，链接了很多 section。

### objcopy 的作用
objcopy 用来把 ELF 头去掉，生成`raw binary`。

>       objcopy can be used to generate a raw binary file by using an output
>       target of binary (e.g., use -O binary).  When objcopy generates a raw
>       binary file, it will essentially produce a memory dump of the contents
>       of the input object file.  All symbols and relocation information will
>       be discarded.  The memory dump will start at the load address of the
>       lowest section copied into the output file.
