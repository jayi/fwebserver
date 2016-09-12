# 项目简介

本项目的工作是根据HTTP协议，设计一个linux下的简单的服务器，能够响应部分HTTP请求。

# 需求&目标

## 需求

执行方式：通过如下命令行执行。

    fwebserver -h 127.0.0.1 -p 80
  
-h指定监听IP，默认为0.0.0.0，表示监听所有IP。

-p指定监听端口，默认为80端口。

对收到的请求进行识别并发送对应的响应。

其中请求的格式为：

    方法 URI HTTP/版本
    请求头1：值1
    请求头2：值2

响应的格式为：

    Server: FakeWebServer
    Connection: 连接类型
    Content-Length: 实际长度
    Date: 当前时间

## 技术目标

* 支持高并发请求的处理。
* 请求的响应时间尽量短。
* 程序后台运行。
* 至少支持16k大小的请求头。