# Lite-WebServer
C++实现的Web服务器

[项目原作者](https://github.com/markparticle/WebServer)

## 修改
- 固定任务队列最大任务数，在任务队列满时增加工作线程
- 修改原项目的错误
    - logger在Sql连接池初始化之前初始化，否则不能记录Sql连接池相关的日志信息。


## 编译执行
1. 在build/目录下执行`cmake ../`
2. 在build/目录下执行`make`
3. 可执行文件在`bin/`下