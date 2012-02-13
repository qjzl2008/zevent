工程说明：
1，znet
   一个简便接口的异步通信网路库，用户下载服务器(loader)和客户端接口的通信。
   接口文件为znet.h.里面包含简历服务器和客户端的接口，客户端也需要使用该库与
   loader通信
2, miniloader 
   下载服务器,实现根据下载列表进行下载，同时实时处理客户端下载请求。downloader.h
   为miniloader编译为动态库时的接口文件，接口简单。支持限速。
   miniloader需要以下配置文件：

    minicfg.ini：基本配置
         "
           [misc]
           serverlist = miniserver.xml
           minilist = minilist.xml
         "
    miniserver.xml:服务器列表文件，具体配置项说明参加miniserver.xml
    minilist.xml:下载列表文件，具体配置项参加minilist.xml
    miniloader 服务启动后会加载列表同时等待客户端请求。miniloader会根据配置自动维持与服务器长连接。   
    服务器配置列表中会有多个服务域名，miniloader会随机选取一个可用服务，达到负载均衡的效果，如果
    随机服务不可用，则会顺序得到一个可用服务连接。