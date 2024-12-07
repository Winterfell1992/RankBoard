# RankBoard
排行榜
RankBoard文件为普通版的实现。RankBoardDense为密集版的实现。

整体思路：
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
