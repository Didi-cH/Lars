#pragma once

#include <pthread.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "lars_reactor.h"
#include "lars.pb.h"
#include "dns_route.h"

//定义订阅列表数据关系类型，key->modid/cmdid， value->fds(订阅的客户端文件描述符)
typedef std::unordered_map<uint64_t, std::unordered_set<int>> subscribe_map;
//定义发布列表的数据关系类型, key->fd(订阅客户端的文件描述符), value->modids
typedef std::unordered_map<int, std::unordered_set<uint64_t>> publish_map;

class SubscribeList
{
public:
    static void init()
    {
        _instance = new SubscribeList();
    }
    static SubscribeList* instance()
    {
        pthread_once(&_once, init);
        return _instance;
    }

    //订阅
    void subscribe(uint64_t mod, int fd);

    //取消订阅
    void unsubscribe(uint64_t mod, int fd);

    //发布
    void publish(std::vector<uint64_t> &change_mods);

    //根据在线用户fd得到需要发布的列表
    void make_publish_map(std::unordered_set<int> &online_fds, publish_map &need_publish);

    publish_map *get_push_list()
    {
        return &_push_list;
    }

private:
    SubscribeList();
    SubscribeList(const SubscribeList &);
    const SubscribeList &operator=(const SubscribeList &);
    static SubscribeList *_instance;
    static pthread_once_t _once;

    //订阅清单
    subscribe_map _book_list; 
    pthread_mutex_t _book_list_lock;

    //发布清单
    publish_map _push_list; 
    pthread_mutex_t _push_list_lock;
};
