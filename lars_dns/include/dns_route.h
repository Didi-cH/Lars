#pragma once
#include <pthread.h>
#include <unordered_map>
#include <unordered_set>
#include "mysql.h"
#include "subscribe.h"

// void *publish_change_mod_test(void *args);
void *check_route_changes(void *args);

/* 
* 表示modid/cmdid ---> host:ip,host:port的对应关系 
* */
//定义用来保存host的IP/host的port的的集合 数据类型
typedef std::unordered_set<uint64_t> host_set;
//定义用来保存modID/cmdID与host的IP/host的port的对应的关系 数据类型  
typedef std::unordered_map<uint64_t, host_set> route_map;

class Route
{
public:
    //创建一个单例的方法
    static void init()
    {
        _instance = new Route();
    }

    //获取当前的单例
    static Route*instance()
    {
        pthread_once(&_once, init);
        return _instance;
    }

    //-------------------------------//
    //数据库操作
    void connect_db();

    //构建map route数据的方法
    //将RouteData表中的数据加载到map中
    void build_map();

    //通过modid/cmdid获取全部当前模块所挂载的host集合
    host_set get_hosts(int modid, int cmdid);

    //加载版本信息
    // return 0, 表示 加载成功，version没有改变
    //         1, 表示 加载成功，version有改变
    //        -1 表示 加载失败
    int load_version();

    //加载RouteChange 得到修改的modid/cmdid,放在vector中
    void load_changes(std::vector<uint64_t> &change_list);

    //加载RouteData到_temp_pointer
    int load_route_data();

    //将temp_pointer的数据更新到data_pointer
    void swap();

    void remove_changes(bool remove_all);

private:
    //全部构造函数私有化
    Route();
    Route(const Route&);
    const Route &operator=(const Route &);

    //指向当前单例对象的指针
    static Route *_instance;

    //单例锁
    static pthread_once_t _once;


    //---------属性--------------//
    //数据库
    MYSQL _db_conn;  //mysql链接  
    char _sql[1000]; //sql语句

    //modid/cmdid---ip/port 对应的route关系map
    route_map *_data_pointer; //指向RouterDataMap_A 当前的关系map
    route_map *_temp_pointer; //指向RouterDataMap_B 临时的关系map
    pthread_rwlock_t _map_lock;

    //当前版本号
    long _version;
};