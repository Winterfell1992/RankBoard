#include "RankBoard.h"

int SkipList::randomLevel() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, MAX_LVL);
    return dis(gen);
}

SkipListNode* SkipList::find(const std::string& playerid) {
    SkipListNode* curr = head;
    while (curr->next[0] && curr->next[0]->playerid!= playerid) {
        curr = curr->next[0];
    }
    return curr->next[0];
}
int SkipList::getRank(const std::string& playerid) {
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
void SkipList::insert(int64_t score, const std::string& playerid, time_t timestamp) {
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

void SkipList::remove(const std::string& playerid) {
    SkipListNode* node =  find(playerid);
    if (!node){
        return;
    }
    SkipListNode* update[MAX_LVL];
    for (int i = 0; i < MAX_LVL; i++) {
        update[i] = nullptr;
    }

    for (int i = MAX_LVL-1; i >= 0; i--) {
        SkipListNode* curr = head;
        while (curr->next[i] && curr->next[i]->playerid != playerid) {
            curr = curr->next[i];
        }
        update[i] = curr;
    }
    for (int i = 0; i < MAX_LVL; i++) {
        if (update[i]->next[i]  && update[i]->next[i] == node) {
            update[i]->next[i] = node->next[i];
        }
    }
    delete node;
}

void SkipList::print() {
    SkipListNode* curr = head->next[0];
    while (curr) {
        std::cout << curr->score << " " << curr->playerid << " " << curr->timestamp << std::endl; 
        curr = curr->next[0];
    }
}

SkipListNode* SkipList::getHeadNode(){
    return head;
}

void RankBoard::print(){
    skipList.print();
}

void RankBoard::updateScore(const std::string& playerId, int64_t newScore,time_t timestamp) {
    SkipListNode* node =  skipList.find(playerId);
    if (node == nullptr) {
        // 不存在则添加新玩家，赋予当前时间戳
        skipList.insert(newScore,playerId,timestamp);
    } else {
        // 存在则删除,再创建新的node
        skipList.remove(playerId);
        skipList.insert(newScore,playerId,timestamp);
    }
}

int RankBoard::getRank(const std::string& playerId) {
    return  skipList.getRank(playerId) + 1;
}

std::vector<RankInfo> RankBoard::getTopNPlayers(int n) {
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
std::vector<RankInfo> RankBoard::getNearbyPlayers(const std::string& playerId, int n) {
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
