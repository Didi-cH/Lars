#include "main_server.h"

//与report_client通信的thread_queue消息队列
thread_queue<lars::ReportStatusRequest>* report_queue = nullptr;
//与dns_client通信的thread_queue消息队列
thread_queue<lars::GetRouteRequest>* dns_queue = nullptr;

//每个agent UDP server 会对应一个Route_lb;
route_lb *r_lb[3];

struct load_balance_config lb_config;

static void create_route_lb()
{
    for (int i = 0; i < 3; i++) {
        int id = i + 1; //route_lb的id从1 开始计数
        r_lb[i]  = new route_lb(id);
        if (r_lb[i] == nullptr) {
            fprintf(stderr, "no more space to new route_lb\n");
            exit(1);
        }
    }
}

static void init_lb_agent()
{
    //1. 加载配置文件
    //设置配置文件路径(只能执行一次)
    config_file::setPath("./conf/lb_agent.ini");

    lb_config.probe_num = config_file::instance()->GetNumber("loadbalance", "probe_num", 10);
    lb_config.init_succ_cnt = config_file::instance()->GetNumber("loadbalance", "init_succ_cnt", 180);
    lb_config.init_err_cnt = config_file::instance()->GetNumber("loadbalance", "init_err_cnt", 5);
    lb_config.err_rate = config_file::instance()->GetFloat("loadbalance", "err_rate", 0.1);
    lb_config.succ_rate = config_file::instance()->GetFloat("loadbalance", "succ_rate", 0.95);
    lb_config.contin_succ_limit = config_file::instance()->GetNumber("loadbalance", "contin_succ_limit", 10);
    lb_config.contin_err_limit = config_file::instance()->GetNumber("loadbalance", "contin_err_limit", 10);
    lb_config.idle_timeout = config_file::instance()->GetNumber("loadbalance", "idle_timeout", 15);
    lb_config.overload_timeout = config_file::instance()->GetNumber("loadbalance", "overload_timeout", 15);
    lb_config.update_timeout = config_file::instance()->GetNumber("loadbalance", "update_timeout", 15);

    //2. 初始化3个route_lb模块
    create_route_lb();
}


int main(int argc,char **argv)
{
    //1 初始化
    init_lb_agent();

    //2 启动udp server服务（3个udp server）,用来接收业务层(调用者/使用者)的消息
    start_UDP_servers();

    //3 启动lars_reporter client 线程
    report_queue = new thread_queue<lars::ReportStatusRequest>();
    if (report_queue == nullptr) {
        fprintf(stderr, "create report queue error!\n");
        exit(1);
    }
    start_report_client();

    //4 启动lars_dns client 线程
    dns_queue = new thread_queue<lars::GetRouteRequest>();
    if (dns_queue == nullptr) {
        fprintf(stderr, "create dns queue error!\n");
        exit(1);
    }
    start_dns_client();

    //5 主线程应该是阻塞状态
    while(1)
    {
        sleep(10);
    }
    return 0;
}