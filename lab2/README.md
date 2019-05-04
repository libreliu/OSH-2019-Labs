# Lab2 实验报告

## 为什么 cd 要搞成内建命令
因为子进程的 `cwd` 没法影响父进程，所以无法改变目录。

## Feature
```
cd /
pwd
ls
ls | wc
ls | cat | wc
env
export MY_OWN_VAR=1
env | grep MY_OWN_VAR
```
Works good.

除此之外：
- 支持「>>」「>」「<」和其增加 IO Number 字段的组合（如「10>」）
- 支持内建命令的重定向
- 支持「ls >wtf -l」等语法（除「>wtf」（单独）未实现）

（解析了 Heredoc，限于时间没有处理；「>&」等同理）
## 大致路线
- 采用 yacc 和 lex 进行词法和语法分析，再生成语法树
- 根据语法树进行执行（见`Shell`类）