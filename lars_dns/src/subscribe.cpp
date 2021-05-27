#include "subscribe.h"

extern TCP_SERVER *server;
//单例对象
SubscribeList *SubscribeList::_instance = NULL;
//用于保证创建单例的init方法只执行一次的锁
pthread_once_t SubscribeList::_once = PTHREAD_ONCE_INIT;

//主动推送任务
void push_change_task(event_loop *loop, void *args);


//==========================================================//

SubscribeList::SubscribeList()
{
    pthread_mutex_init(&_book_list_lock, NULL);
    pthread_mutex_init(&_push_list_lock, NULL);
}

//订阅
void SubscribeList::subscribe(uint64_t mod, int fd)
{
    //将mod->fd的关系加入到_book_list中
    pthread_mutex_lock(&_book_list_lock);
    _book_list[mod].insert(fd);
    pthread_mutex_unlock(&_book_list_lock);    
}
//取消订阅
void SubscribeList::unsubscribe(uint64_t mod, int fd)
{
    //将mod->fd关系从_book_list中删除
    pthread_mutex_lock(&_book_list_lock);
    if (_book_list.find(mod) != _book_list.end()) {
        _book_list[mod].erase(fd);
        if (_book_list[mod].empty() == true) {
            _book_list.erase(mod);
        }
    }
    pthread_mutex_unlock(&_book_list_lock); 
}
//发布
void SubscribeList::publish(std::vector<uint64_t> &change_mods)
{
    pthread_mutex_lock(&_book_list_lock);
    pthread_mutex_lock(&_push_list_lock);
    for (auto &mod:change_mods)
    {
        if(_book_list.find(mod)!=_book_list.end())
        {
            for(auto &fd:_book_list[mod])
            {
                _push_list[fd].insert(mod);
            }
        }
    }
    pthread_mutex_unlock(&_push_list_lock); 
    pthread_mutex_unlock(&_book_list_lock); 

    //通知server，让server给_push_list中的每个fd发送新的modid/cmdid对应的主机信息 回执消息
    server->get_thread_pool()->send_task(push_change_task, this);
}

//根据在线用户fd得到需要发布的列表
void SubscribeList::make_publish_map(std::unordered_set<int> &online_fds, publish_map &need_publish)
{
    pthread_mutex_lock(&_push_list_lock);
    for (auto it = _push_list.begin(); it != _push_list.end();it++)
    {
        if(online_fds.find(it->first)!=online_fds.end())
        {
            for (auto st = _push_list[it->first].begin(); st != _push_list[it->first].end();st++)
            {
                need_publish[it->first].insert(*st);
            }
            // need_publish[it->first] = _push_list[it->first];
            // _push_list.erase(it);
        }
    }
    pthread_mutex_unlock(&_push_list_lock); 
}


//====================================================//
void push_change_task(event_loop *loop, void *args)
{
    SubscribeList *subscribe = (SubscribeList*)args;

    //1 获取全部的在线客户端fd
    std::unordered_set<int> online_fds;
    loop->get_listen_fds(online_fds);

    //2 从subscribe的_push_list中 找到与online_fds集合匹配，放在一个新的publish_map里
    publish_map need_publish;
    need_publish.clear();
    subscribe->make_publish_map(online_fds, need_publish);

    //3 依次从need_publish取出数据 发送给对应客户端链接
    for(auto &list:need_publish)
    {
        int fd = list.first;
        for(auto &mod:list.second)
        {
            int modid = int(mod >> 32);
            int cmdid = int(mod);

            //组装pb消息，发送给客户
            lars::GetRouteResponse rsp; 
            rsp.set_modid(modid);
            rsp.set_cmdid(cmdid);

            //通过route查询对应的host ip/port信息 进行组装
            host_set hosts = Route::instance()->get_hosts(modid, cmdid) ;
            for (auto hit = hosts.begin(); hit != hosts.end(); hit++) {
                uint64_t ip_port_pair = *hit;
                lars::HostInfo host_info;
                host_info.set_ip((uint32_t)(ip_port_pair >> 32));
                host_info.set_port((int)ip_port_pair);

                //添加到rsp中
                rsp.add_host()->CopyFrom(host_info);
            }

            //给当前fd 发送一个更新消息
            std::string responseString;
            rsp.SerializeToString(&responseString);

            //通过fd取出链接信息
            net_connection *conn = TCP_SERVER::conns[fd];
            if (conn != nullptr) {
                conn->send_message(responseString.c_str(), responseString.size(), lars::ID_GetRouteResponse);
            }
            else 
            {
                printf("publish conn == NULL! error fd = %d\n", fd);
            }
            SubscribeList::instance()->get_push_list()->erase(fd);
        }
    }
}
