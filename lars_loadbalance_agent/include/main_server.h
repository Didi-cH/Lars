#pragma once
#include <pthread.h>
#include "lars_reactor.h"
#include "lars.pb.h"
#include "route_lb.h"


//与report_client通信的thread_queue消息队列
extern thread_queue<lars::ReportStatusRequest>* report_queue;
//与dns_client通信的thread_queue消息队列
extern thread_queue<lars::GetRouteRequest>* dns_queue;
extern route_lb *r_lb[3];

struct load_balance_config
{
    //经过若干次请求host节点后， 试探1次overload的过载节点
    int probe_num;

    //初始化一个host_info主机访问的成功次数，防止刚启动少量失败就认为过载
    int init_succ_cnt;

    int init_err_cnt;

    //当idle节点失败率高于此值，节点变为overload状态
    float err_rate;

    //当overload节点成功率高于此值，节点变为idle状态
    float succ_rate;

    //当idle节点连续失败次数超过此值，节点变为overload状态
    int contin_err_limit;

    //当overload节点成功次数超过此值，节点变为idle状态
    int contin_succ_limit;

    //对于某个modid/cmdid下的idle状态的主机，需要清理一次负载窗口的时间
    int idle_timeout;

    //对于某个modid/cmdid下的overload状态主机， 在过载队列等待的最大时间
    int overload_timeout;

    //对于每个NEW状态的modid/cmdid 多久从远程dns更新一次到本地路由
    long update_timeout;
};

extern struct load_balance_config lb_config;

// 启动udp server服务,用来接收业务层(调用者/使用者)的消息
void start_UDP_servers(void);

// 启动lars_reporter client 线程
void start_report_client(void);

// 启动lars_dns client 线程
void start_dns_client(void);