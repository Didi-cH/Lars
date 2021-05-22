# Lars
Lars-基于C++负载均衡远程服务器调度系统的实现，重新实现了一下开源项目
原始项目地址https://github.com/aceld/Lars

# Lars——reactor
lars_reactor_v0.1   基本socket模型
lars_reactor_v0.2   +内存管理（内存池、io缓冲）
lars_reactor_v0.3   +epoll
lars_reactor_v0.4   +消息封装、客户端接口
lars_reactor_v0.5   +连接属性   
lars_reactor_v0.6   +消息路由
lars_reactor_v0.7   +连接和销毁时的hook
lars_reactor_v0.8   +消息队列、线程池
lars_reactor_v0.9   +配置文件
lars_reactor_v0.10  +udp
lars_reactor_v0.11  +异步任务
lars_reactor_v0.12  +连接属性
qps_test            qps测试


QPS测试：
CPU个数：4个 ， 内存: 4GB , 系统：Ubuntu18.04虚拟机 
线程数/客户端 100	 QPS 18w/s
线程数/客户端 200	 QPS 16w/s
线程数/客户端 500	 QPS 14w/s
