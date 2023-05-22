#include "../zhelpers.hpp"
#include <pthread.h>
#include <queue>

//LRU模式的高级请求-应答模式
//首先work端先发送一个“READY”给ROUTER，ROUTER将获得的workaddress入队
//接着client端发送的“hello”就能发送给另一端的ROUTER，由于一端ROUTER到另一端的ROUTER会有两层信封，所以，client端的请求消息为"workaddr"+"" + "clientaddr" + "" + "request"
//这是因为要让ROUTER知道目标地址是谁，同时REQ会自动去点信封和第一个""

static void* work_task(void *args)
{
    zmq::context_t context(1);

    //REQ模式
    zmq::socket_t work(context, ZMQ_REQ);
    s_set_id(work);

    //进程间的通信
    work.connect("ipc://backend.ipc");

    //给ROUTER发送已准备好的消息
    s_send(work, std::string("READY"));

    while(1)
    {
        //最上面一层是地址信息
        std::string address = s_recv(work);
        {
            //中间的一层是空字符串，如果不是就会报错
            std::string empty = s_recv(work);
            assert(empty.size() == 0);
        }

        //最下面一层是信的内容
        std::string request = s_recv(work);
        std::cout << request << std::endl;

        //向ROUTER发送回去
        s_sendmore(work, address);
        s_sendmore(work, std::string(""));
        s_send(work, std::string("OK"));
    }

    return NULL;
}

static void* client_task(void* args)
{
    zmq::context_t context(1);

    //REQ模式
    zmq::socket_t client(context, ZMQ_REQ);
    
    //设置编号
    s_set_id(client);
    client.connect("ipc://frontend.ipc");

    //给ROUTER发送准备好的消息
    s_send(client, std::string("HELLO"));
    std::string reply = s_recv(client);
    std::cout << reply << std::endl;

    return NULL;
}

int main(void)
{
    zmq::context_t context(1);

    //两个ROUTER模式
    zmq::socket_t frontend(context, ZMQ_ROUTER);
    zmq::socket_t backend(context, ZMQ_ROUTER);

    //绑定通讯
    frontend.bind("ipc://frontend.ipc");
    backend.bind("ipc://backend.ipc");

    //client线程
    int client_nbr;
    for(client_nbr = 0; client_nbr < 10; client_nbr++)
    {
        pthread_t client;
        pthread_create(&client, NULL, client_task, (void*)(intptr_t)clinet_nbr);
    }

    //创建work线程
    int work_nbr;
    for(work_nbr = 0; work_nbr < 3; work_nbr++)
    {
        pthread_t work;
        pthread_create(&work, NULL, work_task, (void*)(intptr_t)work_nbr);
    }

    //消息队列
    std::queue<std::string> worker_queue;

    while(1)
    {

        //创建了一个数组，数组内容是正在轮询事件的套接字
        zmq::pollitem_t items[] = 
        {
            {backend, 0, ZMQ_POLLIN, 0},
            {frontend, 0, ZMQ_POLLIN, 0},
        };

        //如果队列不为空，则处理前后端的事件，否则只用处理后端的事件
        if(worker_queue.size())
            //处理数组中的轮询事件，2为数组中的数量，-1则是选项为阻塞
            zmq::poll(&items[0], 2, -1);
        else
            zmq::poll(&items[0], 1, -1);
        
        //是否可以从后端读取数据
        if(items[0].revents & ZMQ_POLLIN)
        {
            //将work的地址入队列,ROUTE接收到REQ的消息时，会自己加上消息发出者的信封
            worker_queue.push(s_recv(backend));
            {
                //跳过空帧，REQ会加空帧
                std::string empty = s_recv(backend);
                assert(empty.size());
            }

            //第三帧是“READY”或者是一个client的地址
            std::string client_addr = s_recv(backend);

            //如果是client地址，就将内容转发
            if(client_addr.compare("READY") != 0)
            {
                {
                    std::string empty = s_recv(backend);
                    assert(empty.size() == 0);
                }
                std::string reply = s_recv(backend);
                s_sendmore(frontend, client_addr);
                s_sendmore(frontend, std::string(""));
                s_send(frontend, reply);

                if(--client_nbr == 0)
                    break;
            }
        }

        //是否可以从前端读取数据
        if(items[1].revents & ZMQ_POLLIN)
        {
            //获取下一个client的请求，交给空闲的worker处理
            //client请求的格式是：[client地址][空帧][请求内容]
            worker_queue.push(s_recv(frontend));
            {
                std::string empty = s_recv(frontend);
                assert(empty.size() == 0);
            }
            std::string request = s_recv(frontend);

            std::string worker_addr = worker_queue.front();
            
            //将该work地址出队
            worker_queue.pop();

            s_sendmore(backend, worker_addr);
            s_sendmore(backend, std::string(""));
            s_sendmore(backend, client_addr);
            s_sendmore(backend, std::string(""));
            s_send(backend, request);
        }
    }

    return 0;
}