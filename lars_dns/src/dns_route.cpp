#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include "lars_reactor.h"
#include "dns_route.h"
#include "string.h"
#include "config_file.h"

//单例对象
Route * Route::_instance = nullptr;

//用于保证创建单例的init方法只执行一次的锁
pthread_once_t Route::_once = PTHREAD_ONCE_INIT;

Route::Route():_version(0)
{
    printf("Route init\n");
    memset(_sql, 0, 1000);
    //1 初始化锁
    pthread_rwlock_init(&_map_lock, NULL);

    //2 初始化map
    _data_pointer = new route_map();//RouterDataMap_A
    _temp_pointer = new route_map();//RouterDataMap_B

    //3 链接数据库
    this->connect_db();

    //将数据库中的RouteData的数据加载到_data_pointer中
    this->build_map();
}

void Route::connect_db()
{
    //加载mysql的配置参数
    std::string db_host = config_file::instance()->GetString("mysql", "db_host", "127.0.0.1");
    short db_port = config_file::instance()->GetNumber("mysql", "db_port", 3306);
    std::string db_user = config_file::instance()->GetString("mysql", "db_user", "root");
    std::string db_passwd = config_file::instance()->GetString("mysql", "db_passwd", "910910");
    std::string db_name = config_file::instance()->GetString("mysql", "db_name", "lars_dns");

    //初始化mysql连接
    mysql_init(&_db_conn);

    //设置一个超时定期重连
    mysql_options(&_db_conn, MYSQL_OPT_CONNECT_TIMEOUT,"30");
    my_bool reconnect = 1;
    mysql_options(&_db_conn, MYSQL_OPT_RECONNECT, & reconnect);

    if (!mysql_real_connect(&_db_conn, db_host.c_str(), db_user.c_str(), db_passwd.c_str(), db_name.c_str(), db_port, nullptr, 0)) 
    {
        fprintf(stderr, "Failed to connect mysql\n");
        exit(1);
    }

    printf("connect db succ!\n");
}

void Route::build_map()
{
    int ret = 0;
    //查询RouteData数据库
    memset(_sql, 0, 1000);
    snprintf(_sql, 1000, "SELECT * FROM RouteData;");
    ret = mysql_real_query(&_db_conn, _sql, strlen(_sql));
    if ( ret != 0) 
    {
        fprintf(stderr, "failed to find any data, error %s\n", mysql_error(&_db_conn));
        exit(1);
    }

    //获取一个结果集合
    MYSQL_RES *result = mysql_store_result(&_db_conn);

    //遍历分析集合中的元素，加入_data_pointer(mapA)
    long line_num = mysql_num_rows(result);

    MYSQL_ROW row;
    for (long i = 0; i < line_num; i++) {
        row = mysql_fetch_row(result);
        int modID = atoi(row[1]);
        int cmdID = atoi(row[2]);
        unsigned ip = atoi(row[3]);
        int port = atoi(row[4]);

        printf("modid = %d, cmdid = %d, ip = %u, port = %d\n", modID, cmdID, ip, port);

        //组装map的key，有modID/cmdID组合
        uint64_t key = ((uint64_t)modID << 32) + cmdID;
        uint64_t value = ((uint64_t)ip << 32) + port;


        //插入到RouterDataMap_A中
        (*_data_pointer)[key].insert(value);
    }

    mysql_free_result(result);

}

//获取modid/cmdid对应的host信息
host_set Route::get_hosts(int modid, int cmdid)
{
    host_set hosts;     

    //组装key
    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    pthread_rwlock_rdlock(&_map_lock);
    auto it = _data_pointer->find(key);
    if (it != _data_pointer->end()) {
        //找到对应的ip + port对
        hosts = it->second;
    }
    pthread_rwlock_unlock(&_map_lock);

    return hosts;
}

    // return 0, 表示 加载成功，version没有改变
    //         1, 表示 加载成功，version有改变
    //        -1 表示 加载失败
int Route::load_version()
{
    memset(_sql, 0, 1000);
    snprintf(_sql, 1000, "SELECT version from RouteVersion WHERE id = 1;");

    int ret = mysql_real_query(&_db_conn, _sql, strlen(_sql));
    if (ret != 0) {
        fprintf(stderr, "select RouteVersion error %s\n", mysql_error(&_db_conn));
        exit(1);
    }

    MYSQL_RES *result = mysql_store_result(&_db_conn);

    long line_num = mysql_num_rows(result);
    if (line_num == 0)
    {
        fprintf(stderr, "No version in table RouteVersion: %s\n", mysql_error(&_db_conn));
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    long new_version = atol(row[0]);

    if (new_version == this->_version)
    {
        //加载成功但是没有修改
        return 0;
    }
    this->_version = new_version;
    printf("now route version is %ld\n", this->_version);

    mysql_free_result(result);

    return 1;
}

//加载RouteChange 得到修改的modid/cmdid,放在vector中
void Route::load_changes(std::vector<uint64_t> &change_list)
{
    memset(_sql, 0, 1000);
    //读取当前版本之前的全部修改 
    snprintf(_sql, 1000, "SELECT modid,cmdid FROM RouteChange WHERE version <= %ld;", _version);
    int ret = mysql_real_query(&_db_conn, _sql, strlen(_sql));
    if (ret != 0) {
        fprintf(stderr, "select RouteChange error %s\n", mysql_error(&_db_conn));
        exit(1);
    }

    MYSQL_RES *result = mysql_store_result(&_db_conn);

    long lineNum = mysql_num_rows(result);
    if (lineNum == 0)
    {
        fprintf(stderr,  "No version in table ChangeLog: %s\n", mysql_error(&_db_conn));
        return ;
    }
    MYSQL_ROW row;
    for (long i = 0;i < lineNum; ++i)
    {
        row = mysql_fetch_row(result);
        int modid = atoi(row[0]);
        int cmdid = atoi(row[1]);
        uint64_t key = (((uint64_t)modid) << 32) + cmdid;
        change_list.push_back(key);
    }
    mysql_free_result(result);  
}

//加载RouteData到_temp_pointer
int Route::load_route_data() 
{
    _temp_pointer->clear();

    memset(_sql, 0, 1000);
    snprintf(_sql, 1000, "SELECT * FROM RouteData;");

    int ret = mysql_real_query(&_db_conn, _sql, strlen(_sql));
    if (ret != 0) {
        fprintf(stderr, "select RouteData error %s\n", mysql_error(&_db_conn));
        exit(1);
    }

    MYSQL_RES *result = mysql_store_result(&_db_conn);

    long line_num = mysql_num_rows(result);
    MYSQL_ROW row;
    for (long i = 0;i < line_num; ++i)
    {
        row = mysql_fetch_row(result);
        int modid = atoi(row[1]);
        int cmdid = atoi(row[2]);
        unsigned ip = atoi(row[3]);
        int port = atoi(row[4]);

        uint64_t key = ((uint64_t)modid << 32) + cmdid;
        uint64_t value = ((uint64_t)ip << 32) + port;

        (*_temp_pointer)[key].insert(value);
    }
    printf("load data to tmep succ! size is %lu\n", _temp_pointer->size());

    mysql_free_result(result);

    return 0;
}


//将temp_pointer的数据更新到data_pointer
void Route::swap()
{
    pthread_rwlock_wrlock(&_map_lock);
    route_map *temp = _data_pointer;
    _data_pointer = _temp_pointer;
    _temp_pointer = temp;
    pthread_rwlock_unlock(&_map_lock);
}

//将RouteChange
//删除RouteChange的全部修改记录数据,remove_all为全部删除
//否则默认删除当前版本之前的全部修改
void Route::remove_changes(bool remove_all)
{
    if (remove_all == false)
    {
        snprintf(_sql, 1000, "DELETE FROM RouteChange WHERE version <= %ld;", _version);
    }
    else
    {
        snprintf(_sql, 1000, "DELETE FROM RouteChange;");
    }
    int ret = mysql_real_query(&_db_conn, _sql, strlen(_sql));
    if (ret != 0)
    {
        fprintf(stderr, "delete RouteChange: %s\n", mysql_error(&_db_conn));
        return ;
    } 

    return;
}

//---------------------------------------------------//
//周期性后端检查db的route信息的更新变化业务
void *check_route_changes(void *args)
{
    int wait_time = 10; //10s自动修改一次，也可以从配置文件读取
    long last_load_time = time(NULL);


    while (true)
    {
        sleep(1);
        long current_time = time(NULL);

        //1、判断版本是否已经被修改
        int ret = Route::instance()->load_version();
        if(ret==1)
        {
            //version已经被更改。有modid/cmdid被修改

            //1 将最新的RouteData的数据加载到_temp_pointer中
            Route::instance()->load_route_data();

            //2 将_temp_pointer的数据更新到_data_pointer中
            Route::instance()->swap();
            last_load_time = current_time;

            //3 获取当前已经被修改的modid/cmdid集合vector
            std::vector<uint64_t> changes;
            Route::instance()->load_changes(changes);

            //4 给订阅修改的mod客户端agent 推送消息
            SubscribeList::instance()->publish(changes);

            //5 删除当前版本之前的修改记录
            Route::instance()->remove_changes(true);
        }
        else
        {
            //version没有被修改
            if(current_time-last_load_time>=wait_time)
            {
                //定期检查超时，强制加载_temp_pointer-->_data_pointer中
                Route::instance()->load_route_data();
                Route::instance()->swap();
                last_load_time = current_time;

            }
        }
    }
}


// void *publish_change_mod_test(void *args)
// {
//     while(true)
//     {
//         sleep(1);
//         // modid=1,cmdid=1
//         int modid1 = 1;
//         int cmdid1 = 1;
//         uint64_t mod1 = (((uint64_t)modid1) << 32) + cmdid1;
//         // modid=1,cmdid=2
//         int modid2 = 1;
//         int cmdid2 = 2;
//         uint64_t mod2 = (((uint64_t)modid2) << 32) + cmdid2;

//         std::vector<uint64_t> changes;
//         changes.push_back(mod1);
//         changes.push_back(mod2);

//         SubscribeList::instance()->publish(changes);
//     }
// }
