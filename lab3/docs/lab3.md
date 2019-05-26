# Lab3 实验报告

刘紫檀 PB17000232

## 实验设计
在本实验中我实现了一个基于 epoll 调用的简易事件驱动高性能 HTTP 服务器。epoll 用来监听网络请求时效率很高。

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
  - `SEND_RESPONSE_CONTENT`：进行「读文件」=>「写入套接字」=>「读文件」=>「写入套接字」...的循环，直到写完或 `read` error（且非 EAGAIN）或达到最大循环次数（`RWCYCLE_MAX_TRIES`)，关闭文件和套接字描述符

## 编译和运行方法
```sh
cmake .                 # I've set -O3 flag in CMakeLists.txt, no worry!
make
ulimit -Sn 10000        # (*optional*) Make sure this <= ~10000, or it'll SIGSEGV due to mem shortage
						# if oom_reaper starts, toggle this value lower
						# at 10000 it'll consume ~600MB of mem
./server > /dev/null # Suppess output to improve performance (A great deal!)
```

## 测试
在 RPi 3B+ 上架设该服务器，通过（千兆）直连到另一台计算机进行测试。

因为 RPi 3B+ 的千兆以太网卡挂在 USB2.0 控制器上，所以性能略小于预期。

用 `/dev/urandom` 产生各个大小的测试文件。

使用 `siege -c 200 -r 20` 进行测试。

测试结果如下表：

> 格式说明：Availability / Trans. Rate / Throughput / Concurrency / Elapsed Time

|      | RapidHTTP                                                    | Nginx                                                        |
| ---- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| 1K   | `100.00%` / `3305.78 trans/sec` / `3.23 MB/sec` / `191.19` / `1.21 secs` | `100.00%` / `2614.38 trans/sec` / `2.55 MB/sec` / `159.30` / `1.53 secs` |
| 16K  | `100.00%` / `1007.56 trans/sec` / `15.74 MB/sec` / `193.29` / `3.97 secs` | `100.00%` / `980.39 trans/sec` / `15.32 MB/sec` / `197.51` / `4.08 secs` |
| 256K | `100.00%` / `88.77 trans/sec` / `22.19 MB/sec` / `199.60` / `45.06 secs` | `100.00%` / `91.87 trans/sec` / `22.97 MB/sec` / `199.69` / `43.54 secs` |
| 4M   | `100.00%` / `6.07 trans/sec` / `24.29 MB/sec` / `199.81` / `658.84 secs` | `100.00%` / `6.04 trans/sec` / `24.15 MB/sec` / `199.80` / `662.44 secs` |

可以看到，在 200 并发下，RapidHTTP 与 Nginx 性能相当。

## 异常处理

在 HTTP 请求中可能遇到各种各样的异常：

1. 「已解决」对面提前关闭了连接
   - 忽略 `SIGPIPE` 信号，并且检测 `write` 返回值来判断， 防止服务器接收信号后自动关闭
2. 「已解决」对面发送垃圾信息（比如含 `\0` 的信息，以及不支持的 HTTP 方法）
   - 不能假设没有 `\0`（所以不能直接用诸如 `strtok` 函数）；接收缓冲区满，却还不能成功解析请求则关闭连接
3. 「未解决」对面发送速度过慢 / 不发送请求
   - 跟踪每个连接的建立时间，对超过 `HTTP_TIMEOUT` 的连接直接关闭「**尚未实现**，犯懒了...」
4. 「已解决」对面发送错误的路径（如 path escaping）
   - 利用 getcwd 和 realpath 函数，比较两个正则路径前面的部分是否匹配
   - 对于 realpath 无法跟踪的路径（比如不存在 / 权限问题），会 500
   - （可以用 `printf "GET /../../../../../../../../proc/cpuinfo HTTP/1.0\r\n\r\n" | nc 127.0.0.1 8000` 测试）

## 遇到的问题和解决方案

1. `epoll` 无法跟踪普通文件
    - 因为 POSIX 中明确规定普通文件的读写总是“阻塞”的，设置 `O_NONBLOCK` 等没有意义。
    - 这样，如果不想因为文件操作阻塞，就只能开一个线程专门用于文件读写，或采用 POSIX aio / AIO syscall；本实现中为了简洁暂时忽略。

## BUGS
1. 在文件系统/磁盘响应较慢（比如 docroot 放在 U 盘上...）的情形下，服务可能响应缓慢
    - 因为普通文件的读写总是“阻塞”的，而本实现没有采用多线程
    - 降低一次处理的数据量（`RWCYCLE_MAX_TRIES`）可以减轻这个问题