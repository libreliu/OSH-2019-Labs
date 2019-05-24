# Lab3 实验报告

刘紫檀 PB17000232

## 实验设计
在本实验中我实现了一个基于 epoll 调用的简易事件驱动高性能 HTTP 服务器。

- 对于每个文件描述符，在服务器启动时分配一个数据结构 `conn_info`，用来保存所有连接/文件相关信息
- 服务器启动时注册 SIGPIPE 和 SIGINT 的信号处理，防止 bug （见源码）
- 对于每个 `accept` 到的客户端套接字，初始化连接对应的数据结构，同时加入 epoll 兴趣列表，监听 EPOLLIN 事件；触发方式 Edge Triggered
- 在 EPOLLIN 发生时，检查是哪个描述符，同时检查该连接当前的状态（`NOT_ALLOCATED`, `WAIT_FOR_REQUEST_METHOD`, `WAIT_FOR_REQUEST_CONTENT`, `WAIT_FOR_REQUEST_END`, `PREPARE_RESPONSE_HEADER`, `SEND_RESPONSE_HEADER`, `SEND_RESPONSE_CONTENT`）其中之一
- 对每个状态，进行对应的处理。
  - `WAIT_FOR_REQUEST_METHOD`：读入（`read_buf`，下同），等待直到客户端发送的字节数达到 4，然后判断是否为 `"GET "`。
    - 是，则进入`WAIT_FOR_REQUEST_CONTENT`
    - 不是，则直接 `do_reject`（关闭套接字）
  - `WAIT_FOR_REQUEST_CONTENT`：等待直到收到一个空格，然后把这部分字符串分离出来，然后进入 `WAIT_FOR_REQUEST_END`。
    - 如果这串字符中有 `\0`，则判定为恶意请求，`do_reject`
  - `WAIT_FOR_REQUEST_END`：等待直到缓冲区连续收到两个`\r\n`，然后进入`PREPARE_RESPONSE_HEADER`
  - `PREPARE_RESPONSE_HEADER`：检查请求的资源是否存在（`realpath` 和 `getcwd` 来检查是否在当前目录及其子目录内），用`stat`返回大小，同时将构造的响应头保存下来（`write_buf`），将 EPOLLOUT （*LT，非ET*）加入兴趣列表，然后进入`SEND_RESPONSE_HEADER`
    - `stat` 返回 -1 就 404（比较粗糙）
    - 正常打开返回 200
    - 其它情况返回 500
  - `SEND_RESPONSE_HEADER`：发送响应头
  - `SEND_RESPONSE_CONTENT`：进行「读文件」=>「写入套接字」=>「读文件」=>「写入套接字」...的循环，直到写完或 `read` error（且非 EAGAIN），关闭文件和套接字描述符

## 测试
分别在 SSD 和 tmpfs 中进行了测试。
- CPU: Intel(R) Core(TM) i5-3210M CPU @ 2.50GHz
- Mem: Hynix DDR3-1600 4GiB * 2
- System: ArchLinux Kernel 5.0.13-arch1-1-ARCH
  - gcc 8.3.0
  - nginx/1.16.0

### SSD 测试
- SSD: SAMSUNG 860-EVO 256GiB (@ SATA 2)
  - 读取速度： 271.89 MB/sec (`sudo hdparm -t /dev/sdb1`)
  - 测试分区： ext4

用 `/dev/urandom` 产生 `testzero.txt`，`test1k.txt`，`test2k.txt`，`test4k.txt`，`test1M.txt` 和 `test512M.txt`。

```
[libreliu@thinkpad-ssd build]$ siege -c 1000 -r 100 http://127.0.0.1:8000/testzero.txt >/dev/null
** SIEGE 4.0.4
** Preparing 255 concurrent users for battle.
The server is now under siege...

Transactions:		       25500 hits
Availability:		      100.00 %
Elapsed time:		        8.60 secs
Data transferred:	        0.00 MB
Response time:		        0.06 secs
Transaction rate:	     2965.12 trans/sec
Throughput:		        0.00 MB/sec
Concurrency:		      168.34
Successful transactions:       25500
Failed transactions:	           0
Longest transaction:	        7.18
Shortest transaction:	        0.00
```

## 遇到的问题和解决方案
1. `epoll` 无法跟踪普通文件
  - 因为 POSIX 中明确规定普通文件的读写总是“阻塞”的，设置 `O_NONBLOCK` 等没有意义。
  - 这样，如果不想因为文件操作阻塞，就只能开一个线程专门用于文件读写；本实现中为了实现的简洁暂时忽略。

## BUGS
1. 在文件系统/磁盘响应较慢的情形下，服务可能响应缓慢
  - 因为普通文件的读写总是“阻塞”的，而本实现没有采用多线程
  - 降低一次处理的数据量（`RWCYCLE_MAX_TRIES`）可以减轻这个问题