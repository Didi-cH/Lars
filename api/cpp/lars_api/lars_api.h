#pragma once

#include "lars_reactor.h"
#include <string>
#include <vector>

typedef std::pair<std::string, int> ip_port;
typedef std::vector<ip_port> route_set;

class lars_client
{
public:
    lars_client();
    ~lars_client();

    //注册一个模块(一个模块调用一次)
    int reg_init(int modid, int cmdid);

    //lars 系统获取host信息，得到可用的host ip和port api
    int get_host(int modid, int cmdid, std::string &ip, int &port);

    //lars 获取某modid/cmdid的全部的hosts
    int get_route(int modid, int cmdid, route_set &route);

    //lar 系统 上报host调用信息 api
    void report(int modid, int cmdid, std::string &ip, int port, int retcode);


private:
    int _sockfd[3]; //对应3个 lars系统udpserver
    uint32_t _seqid; //每个消息的序号，为了判断返回udp包是否合法
};
