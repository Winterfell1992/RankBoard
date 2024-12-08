/* 整体思路：
    参照redis的zset跳表实现方案。
    构建一个跳表来存储RankInfo信息。以score和timestamp进行排名
    跳表第0层包含所有RankInfo的指针，可以用来顺序查找。
    插入前需要先顺序查找一下有没有老数据，插入操作时间复杂度退化为n。
    查找自己的排名由于key是playerId，也需要顺序查找，时间复杂度为n。
    前n名从头指针向后查找n个即可。
    自己前后n名，维护一个左指针，为n名中的第一个，顺序查找，找到自己后，从左指针向后取n个数据。

    数据量大且7*24小时运行：
    1.排行榜放在redis上集群 + 持久化 
    2.读写分离，实现一个rank_server来承载所有的读请求，rank_server定时向排行榜请求最新切片数据，所有客户端读到的都是rank_server的切片数据。
    3.单节点的redis大约可以承载十万级别的QPS,百万级别的数据单节点就可以承载。消息队列可以使用redis自己的pub/sub
 */

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>

 // 跳表最大层数
#define MAX_LVL 16

// 玩家结构体，包含名字、积分和时间戳，用于排行榜中作为值
struct RankInfo {
    std::string playerId;
    int64_t score;
    time_t timestamp;
};

// 跳表节点结构体
struct SkipListNode {
    int64_t score;
    std::string playerid;
    time_t timestamp;  // 添加时间戳字段
    SkipListNode* next[MAX_LVL]; // 跳表最大层数

    SkipListNode(int64_t s, const std::string& id, time_t t) : score(s), playerid(id), timestamp(t) {
        for (int i = 0; i < MAX_LVL; ++i) {
            next[i] = nullptr;
        }
    }
};

// 跳表类
class SkipList {
public:
    SkipList() {
        head = new SkipListNode(0, "", 0);  // 初始化时间戳为 0
    }
    // 查找节点（根据 playerid 查找）,这里只知道playerid不知道分数，只能线性查找
    SkipListNode* find(const std::string& playerid);
    // 获得玩家排名 从1开始
    int getRank(const std::string& playerid);
    // 插入节点
    void insert(int64_t score, const std::string& playerid, time_t timestamp) ;
    // 删除节点
    void remove(const std::string& playerid);
    // 打印最底层跳表，包含所有插入的元素
    void print();
    // 获得头节点
    SkipListNode* getHeadNode();
private:
    SkipListNode* head;  // 头节点
    // 生成随机层数
    int randomLevel();
};

class RankBoard {
public:
    // 更新玩家积分，如果不存在则添加新玩家，加入时间戳参数并处理相同分数排序逻辑
    void updateScore(const std::string& playerId, int64_t newScore,time_t timestamp);
    // 查询玩家当前排名
    int getRank(const std::string& playerId);
    // 获取前N名玩家的分数和名次
    std::vector<RankInfo> getTopNPlayers(int n);
    // 查询自己名次前后共N名玩家的分数和名次
    std::vector<RankInfo> getNearbyPlayers(const std::string& playerId, int n);
    // 打印排行榜
    void print();
private:
    SkipList skipList;
};