/* 
    密集版本相较于原始版本，每个节点记录多个同分的RankInfo
    删除时检查是不是本节点唯一一个数据，如果是唯一数据删除节点，否则删除RankInfo
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
    std::vector<RankInfo> playerRankInfo;
    SkipListNode* next[MAX_LVL]; // 跳表最大层数
    SkipListNode(int64_t s, const std::string& pid, time_t t) {
        for (int i = 0; i < MAX_LVL; i++) {
            next[i] = nullptr;
        }
       auto& tmp =  playerRankInfo.emplace_back();
       tmp.playerId = pid;
       tmp.score = s;
       tmp.timestamp = t;
       score =s;
    }
};

// 跳表类
class SkipList {
public:
    //查找这个玩家是否在这个节点中
    bool checkThisNode(SkipListNode* node,std::string playerId);
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
    void print() ;
    SkipListNode* getHeadNode();

private:
    SkipListNode* head;  // 头节点
    // 生成随机层数
    int randomLevel();
};

class RankBoard {
    SkipList skipList;
public:
    void print();
    // 更新玩家积分，如果不存在则添加新玩家，加入时间戳参数并处理相同分数排序逻辑
    void updateScore(const std::string& playerId, int64_t newScore,time_t timestamp);
    // 查询玩家当前排名
    int getRank(const std::string& playerId);
    // 获取前N名玩家的分数和名次
    std::vector<RankInfo> getTopNPlayers(int n) ;
    // 查询自己名次前后共N名玩家的分数和名次
    // 这里需要区别共N名玩家是否包含自己,这里的做法是包含自己. 如果n是偶数,前后不对称,这里的做法是向前多取一位
    std::vector<RankInfo> getNearbyPlayers(const std::string& playerId, int n);
};
