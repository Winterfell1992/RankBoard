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
private:
    SkipListNode* head;  // 头节点

    // 生成随机层数
    int randomLevel() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(1, MAX_LVL);
        return dis(gen);
    }
public:
    SkipList() {
        head = new SkipListNode(0, "", 0);  // 初始化时间戳为 0
    }
    // 查找节点（根据 playerid 查找）,这里只知道playerid不知道分数，只能线性查找
    SkipListNode* find(const std::string& playerid) {
        SkipListNode* curr = head;
        while (curr->next[0] && curr->next[0]->playerid!= playerid) {
            curr = curr->next[0];
        }
        return curr->next[0];
    }
    // 获得玩家排名 从1开始
    int getRank(const std::string& playerid) {
        SkipListNode* curr = head;
        int rank = 0;
        while (curr->next[0] && curr->next[0]->playerid!= playerid) {
            curr = curr->next[0];
            rank++;
        }
        if (curr->next[0]){
            return rank;
        }
        return -1;
    }
    // 插入节点
    void insert(int64_t score, const std::string& playerid, time_t timestamp) {
        SkipListNode* newNode = new SkipListNode(score, playerid, timestamp);

        int level = randomLevel();
        //std::cout<< "level="<<level<<std::endl;
        SkipListNode* update[MAX_LVL];
        for (int i = 0; i < MAX_LVL; i++) {
            update[i] = nullptr;
        }
        //找到每一层链表中的前置节点
        SkipListNode* curr = head;
        for (int i = MAX_LVL-1 ; i >= 0; i--) {
            while (curr->next[i] && (curr->next[i]->score > score ||(curr->next[i]->score == score && curr->next[i]->timestamp < timestamp ) ) ) {
                curr = curr->next[i];
            }
            update[i] = curr;
        }
        // 在前level层链表中插入新节点
        for (int i = 0; i < level; i++) {
            newNode->next[i] = update[i]->next[i];
            update[i]->next[i] = newNode;
        }
    }

    // 删除节点
    void remove(const std::string& playerid) {
        SkipListNode* node =  find(playerid);
        if (!node){
            return;
        }

        SkipListNode* update[MAX_LVL];
        for (int i = 0; i < MAX_LVL; i++) {
            update[i] = nullptr;
        }

        SkipListNode* curr = head;
        for (int i = MAX_LVL-1; i >= 0; i++) {
            while (curr->next[i] && curr->next[i]->playerid < playerid) {
                curr = curr->next[i];
            }
            update[i] = curr;
        }

        for (int i = 0; i < MAX_LVL; i++) {
            if (update[i] && update[i]->next[i] == node) {
                update[i]->next[i] = node->next[i];
            }
        }
        delete node;
    }

    // 打印最底层跳表，包含所有插入的元素
    void print() {
        SkipListNode* curr = head->next[0];
        while (curr) {
            std::cout << curr->score << " " << curr->playerid << " " << curr->timestamp << std::endl; 
            curr = curr->next[0];
        }
    }

    SkipListNode* getHeadNode(){
        return head;
    }
};



// 玩家结构体，包含名字、积分和时间戳，用于排行榜中作为值
struct RankInfo {
    std::string playerId;
    int64_t score;
    time_t timestamp;
};

// 比较函数，用于按照分数从高到低排序，如果分数相同，按时间戳从小到大排序（时间戳小的在前）
bool comparePlayer(const RankInfo& p1, const RankInfo& p2) {
    if (p1.score == p2.score) {
        return p1.timestamp < p2.timestamp;
    }
    return p1.score > p2.score;
}

class RankBoard {
    SkipList skipList;
public:
    void print(){
        skipList.print();
    }
    // 更新玩家积分，如果不存在则添加新玩家，加入时间戳参数并处理相同分数排序逻辑
    void updateScore(const std::string& playerId, int64_t newScore,time_t timestamp) {
        SkipListNode* node =  skipList.find(playerId);
        if (node==nullptr) {
            // 不存在则添加新玩家，赋予当前时间戳
            skipList.insert(newScore,playerId,timestamp);
        } else {
            // 存在则删除,再创建新的node
            skipList.remove(playerId);
            skipList.insert(newScore,playerId,timestamp);
        }
    }

    // 查询玩家当前排名
    int getRank(const std::string& playerId) {
        return  skipList.getRank(playerId) + 1;
    }

    // 获取前N名玩家的分数和名次
    std::vector<RankInfo> getTopNPlayers(int n) {
        std::vector<RankInfo> topNPlayers;
        topNPlayers.reserve(n);
        if(n < 1){
            return topNPlayers;
        }
        SkipListNode* cur =  skipList.getHeadNode();
        while ( cur->next[0] && n > 0){
            RankInfo& tmp =  topNPlayers.emplace_back();
            tmp.playerId = cur->next[0]->playerid;
            tmp.score = cur->next[0]->score;
            tmp.timestamp = cur->next[0]->timestamp;
            cur = cur->next[0];
            n--;
        }
        return topNPlayers;
    }

    // 查询自己名次前后共N名玩家的分数和名次
    // 这里需要区别共N名玩家是否包含自己,这里的做法是包含自己. 如果n是偶数,前后不对称,这里的做法是向前多取一位
    std::vector<RankInfo> getNearbyPlayers(const std::string& playerId, int n) {
        std::vector<RankInfo> nearbyPlayers;
        nearbyPlayers.reserve(n);
        if(n < 1){
            return nearbyPlayers;
        }
        // 要找的n名玩家的起始node指针
        SkipListNode* left_node = skipList.getHeadNode();
        SkipListNode* cur = left_node;
        int diff = 0;
        //std::cout<<"playerId="<<playerId<< std::endl;;
        while (cur->next[0] && cur->next[0]->playerid != playerId){
            //std::cout<<"cur->next[0]->playerid="<<cur->next[0]->playerid<< std::endl;
            cur = cur->next[0];
           if(diff == n/2 ){
                left_node = left_node->next[0];
           }else{
                diff++;
           }
        }

        if (!cur->next[0]){
            // 没找到该玩家
            return nearbyPlayers;
        }
        // 填充n个玩家
        while (n > 0 && left_node->next[0])
        {
            RankInfo& tmp =  nearbyPlayers.emplace_back();
            tmp.playerId = left_node->next[0]->playerid;
            tmp.score = left_node->next[0]->score;
            tmp.timestamp = left_node->next[0]->timestamp;
            left_node = left_node->next[0];
            n--;
        }
        return nearbyPlayers;
    }
};

int main() {
    //插入积分
    RankBoard rankBoard;
    rankBoard.updateScore("Player5", 62,100005);
    rankBoard.updateScore("Player6", 60,100006);
    // 更新积分测试，注意此时相同分数新加入的玩家应排在后面
    rankBoard.updateScore("Player7", 60,100007);
    rankBoard.updateScore("Player8", 40,100008);
    rankBoard.updateScore("Player9", 30,100009);

    rankBoard.updateScore("Player1", 10,100001);
    rankBoard.updateScore("Player2", 80,100002);
    rankBoard.updateScore("Player3", 70,100003);
    // Player4积分更新2次
    rankBoard.updateScore("Player4", 65,100004);
    rankBoard.updateScore("Player4", 60,100005);
 
    // 按顺序打印排名
    rankBoard.print();

    // 查询排名测试
    std::cout << "Player1 rank: " << rankBoard.getRank("Player1") << std::endl;
    std::cout << "Player2 rank: " << rankBoard.getRank("Player2") << std::endl;
    std::cout << "Player3 rank: " << rankBoard.getRank("Player3") << std::endl;
    std::cout << "Player4 rank: " << rankBoard.getRank("Player4") << std::endl;
    std::cout << "Player5 rank: " << rankBoard.getRank("Player5") << std::endl;
    std::cout << "Player6 rank: " << rankBoard.getRank("Player6") << std::endl;
    std::cout << "Player7 rank: " << rankBoard.getRank("Player7") << std::endl;
    std::cout << "Player8 rank: " << rankBoard.getRank("Player8") << std::endl;
    std::cout << "Player9 rank: " << rankBoard.getRank("Player9") << std::endl;

    {
        // 获取前N名玩家测试 top5
        std::vector<RankInfo> topPlayers = rankBoard.getTopNPlayers(5);
        std::cout << "Top 5 players: " << std::endl;
        for (const auto& player : topPlayers) {
            std::cout << player.playerId << " - Score: " << player.score << " - Timestamp: " << player.timestamp << std::endl;
        }
    }

    { 
        // 获取前N名玩家测试 top20
        std::vector<RankInfo> topPlayers = rankBoard.getTopNPlayers(20);
        std::cout << "Top 20 players: " << std::endl;
        for (const auto& player : topPlayers) {
            std::cout << player.playerId << " - Score: " << player.score << " - Timestamp: " << player.timestamp << std::endl;
        }
    }
    {
        // 获取自己名次前后N名玩家测试 
        std::cout << "Nearby players of Player5:  Nearby :5" << std::endl;
        std::vector<RankInfo> nearbyPlayers = rankBoard.getNearbyPlayers("Player5", 5);
        for (const auto& player : nearbyPlayers) {
            std::cout << player.playerId << " - Score: " << player.score << " - Timestamp: " << player.timestamp << std::endl;
        }
    }
    {
        // 获取自己名次前后N名玩家测试 
        std::cout << "Nearby players of Player5: Nearby :4" << std::endl;
        std::vector<RankInfo> nearbyPlayers = rankBoard.getNearbyPlayers("Player5", 4);
        for (const auto& player : nearbyPlayers) {
            std::cout << player.playerId << " - Score: " << player.score << " - Timestamp: " << player.timestamp << std::endl;
        }
    }
    {
        // 获取自己名次前后N名玩家测试 
        std::cout << "Nearby players of Player5: Nearby :2 " << std::endl;
        std::vector<RankInfo> nearbyPlayers = rankBoard.getNearbyPlayers("Player5", 2);
        for (const auto& player : nearbyPlayers) {
            std::cout << player.playerId << " - Score: " << player.score << " - Timestamp: " << player.timestamp << std::endl;
        }
    }
    {
        // 获取自己名次前后N名玩家测试 
        std::cout << "Nearby players of Player5: Nearby :200" << std::endl;
        std::vector<RankInfo> nearbyPlayers = rankBoard.getNearbyPlayers("Player5", 200);
        for (const auto& player : nearbyPlayers) {
            std::cout << player.playerId << " - Score: " << player.score << " - Timestamp: " << player.timestamp << std::endl;
        }
    }
    return 0;
}
