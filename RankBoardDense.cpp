#include "RankBoardDense.h"

// 生成随机层数
int SkipList::randomLevel() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, MAX_LVL);
    return dis(gen);
}
bool SkipList::checkThisNode(SkipListNode* node,std::string playerId){
    for (size_t i = 0; i < node->playerRankInfo.size(); i++){
        if(playerId == node->playerRankInfo[i].playerId ){
            return true;
        }
    }
    return false;
}
// 查找节点（根据 playerid 查找）,这里只知道playerid不知道分数，只能线性查找
SkipListNode* SkipList::find(const std::string& playerid) {
    SkipListNode* curr = head;
    while (curr->next[0] && !checkThisNode(curr->next[0],playerid)) {
        curr = curr->next[0];
    }
    return curr->next[0];
}
// 获得玩家排名 从1开始
int SkipList::getRank(const std::string& playerid) {
    SkipListNode* curr = head;
    int rank = 0;
    while (curr->next[0] &&  !checkThisNode(curr->next[0],playerid)) {
        curr = curr->next[0];
        rank++;
    }
    if (curr->next[0]){
        return rank;
    }
    return -1;
}
// 插入节点
void SkipList::insert(int64_t score, const std::string& playerid, time_t timestamp) {
    int level = randomLevel();
    //std::cout<< "level="<<level<<std::endl;
    SkipListNode* update[MAX_LVL];
    for (int i = 0; i < MAX_LVL; i++) {
        update[i] = nullptr;
    }
    // 找到每一层链表中的前置节点
    SkipListNode* curr = head;
    for (int i = MAX_LVL-1 ; i >= 0; i--) {
        while (curr->next[i] && (curr->next[i]->score >= score) ) {
            curr = curr->next[i];
        }
        update[i] = curr;
    }
    // 如果第0层已存在这个分数的节点
    if(update[0]&& (update[0]->score == score))
    {
        auto &tmp = update[0]->playerRankInfo.emplace_back();
        tmp.playerId = playerid;
        tmp.score = score;
        tmp.timestamp = timestamp;
        return ; 
    }
    // 新建节点
    SkipListNode* newNode = new SkipListNode(score, playerid, timestamp);
    for (int i = 0; i < level; i++) {
        newNode->next[i] = update[i]->next[i];
        update[i]->next[i] = newNode;
    }
}

// 删除节点
void SkipList::remove(const std::string& playerid) {
    SkipListNode* node =  find(playerid);
    if (!node){
        return;
    }
    
    if (node->playerRankInfo.size() > 1){
        //当这个节点存在多个数据，只删自己这一个数据
        for (auto itr = node->playerRankInfo.begin(); itr!= node->playerRankInfo.end();itr++ )
        {
            if(itr->playerId == playerid)
            {
                node->playerRankInfo.erase(itr);
                return ;
            }
        }  
    }else{
        // 这个节点只有一个数据，删除节点
        SkipListNode* update[MAX_LVL];
        for (int i = 0; i < MAX_LVL; i++) {
            update[i] = nullptr;
        }

        SkipListNode* curr = head;
        for (int i =0; i < MAX_LVL; i++) {
            while (curr->next[i] &&  !checkThisNode(curr->next[i],playerid)) {
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
}

// 打印最底层跳表，包含所有插入的元素
void SkipList::print() {
    SkipListNode* curr = head->next[0];
    int nodeIndex = 1;
    while (curr) {
        std::cout << "nodeIndex= "<< nodeIndex;  
        for (size_t i = 0; i < curr->playerRankInfo.size(); i++){
            std::cout << ",playerId= " <<  curr->playerRankInfo[i].playerId << ",score =  " << curr->playerRankInfo[i].score<<". " ;
        }
        std::cout <<std::endl;  
        curr = curr->next[0];
        nodeIndex++;
    }
}

SkipListNode* SkipList::getHeadNode(){
    return head;
}

void RankBoard::print(){
    skipList.print();
}

// 更新玩家积分，如果不存在则添加新玩家，加入时间戳参数并处理相同分数排序逻辑
void RankBoard::updateScore(const std::string& playerId, int64_t newScore,time_t timestamp) {
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
int RankBoard::getRank(const std::string& playerId) {
    return  skipList.getRank(playerId) + 1;
}

// 获取前N名玩家的分数和名次
std::vector<RankInfo> RankBoard::getTopNPlayers(int n) {
    std::vector<RankInfo> topNPlayers;
    topNPlayers.reserve(n);
    if(n < 1){
        return topNPlayers;
    }
    SkipListNode* cur =  skipList.getHeadNode();
    while ( cur->next[0] && n > 0){
        for (size_t i = 0; i < cur->next[0]->playerRankInfo.size(); i++)
        {
            RankInfo& tmp =  topNPlayers.emplace_back();
            tmp.playerId = cur->next[0]->playerRankInfo[i].playerId;
            tmp.score = cur->next[0]->playerRankInfo[i].score;
            tmp.timestamp = cur->next[0]->playerRankInfo[i].timestamp;
        }
        cur = cur->next[0];
        n--;
    }
    return topNPlayers;
}

// 查询自己名次前后共N名玩家的分数和名次
// 这里需要区别共N名玩家是否包含自己,这里的做法是包含自己. 如果n是偶数,前后不对称,这里的做法是向前多取一位
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
    while (cur->next[0] && skipList.checkThisNode(cur->next[0],playerId)){
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
        for (size_t i = 0; i < left_node->next[0]->playerRankInfo.size(); i++)
        {
            RankInfo& tmp =  nearbyPlayers.emplace_back();
            tmp.playerId = left_node->next[0]->playerRankInfo[i].playerId;
            tmp.score = left_node->next[0]->playerRankInfo[i].score;
            tmp.timestamp = left_node->next[0]->playerRankInfo[i].timestamp;
        }
        left_node = left_node->next[0];
        n--;
    }
    return nearbyPlayers;
}


int main() {
    //插入积分
    RankBoard rankBoard;

    rankBoard.updateScore("Player1", 100,100001);
    rankBoard.updateScore("Player2", 100,100002);
    rankBoard.updateScore("Player3", 95,100003);
    rankBoard.updateScore("Player4", 95,100004);
    rankBoard.updateScore("Player5", 90,100005);
 
    // 按顺序打印排名
    rankBoard.print();

    // 查询排名测试
    std::cout << "Player1 rank: " << rankBoard.getRank("Player1") << std::endl;
    std::cout << "Player2 rank: " << rankBoard.getRank("Player2") << std::endl;
    std::cout << "Player3 rank: " << rankBoard.getRank("Player3") << std::endl;
    std::cout << "Player4 rank: " << rankBoard.getRank("Player4") << std::endl;
    std::cout << "Player5 rank: " << rankBoard.getRank("Player5") << std::endl;

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
