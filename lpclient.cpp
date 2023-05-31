#include <iostream>
#include <string>
#include <zmq.hpp>
#define REQUEST_TIMEOUT     2500    //  毫秒, (> 1000!)
#define REQUEST_RETRIES     3       //  尝试次数
#define SERVER_ENDPOINT     "tcp://localhost:5555"

int main(void)
{
        zmq::context_t context(1);
        zmq::socket_t client(context, ZMQ_REQ);
        client.connect(SERVER_ENDPOINT);

        int sequence = 0;
        int retries_left = REQUEST_RETRIES;
        int linger = 0;

        //设置成当client被关闭的时候，立刻归还资源
        client.set(zmq::sockopt::linger, linger);
        while(retries_left)
        {
                //发送请求
                char request[10];
                sprintf(request, "%d", ++sequence);
                zmq::message_t msg(request, strlen(request));
                client.send(msg, zmq::send_flags::none);

                int expect_reply = 1;
                while(expect_reply)
                {
                        //监视client是否有可读函数
                        zmq::pollitem_t items[] =
                        {
                                {client, 0, ZMQ_POLLIN, 0}
                        };
                        
                        //设置监视时长为REQUEST_TIMEOUT
                        int rc = zmq_poll(items, 1, REQUEST_TIMEOUT);
                        if(rc == -1)
                                break;

                        if (items[0].revents & ZMQ_POLLIN)
                        {
                                zmq::message_t reply;
                                auto r = client.recv(reply, zmq::recv_flags::none);
                                if(reply.size() == 0)
                                        break;
                                if(stoi(reply.to_string()) == sequence)
                                {
                                        std::cout << "I:服务器返回正常 " << reply << std::endl;
                                        retries_left = REQUEST_RETRIES;
                                        expect_reply = 0;
                                }
                                else
                                        std::cout << "E: 服务器返回异常: " << reply << std::endl;
                        }
                        else
                        {
                                if (--retries_left == 0)
                                {
                                        std::cout << "E: 服务器不可用，取消操作" << std::endl;
                                        break;
                                }
                                else
                                {
                                        std::cout << "W: 服务器没有响应，正在重试..." << std::endl;
                                        client.close();
                                        std::cout << "I: 服务器重连中..." << std::endl;
                                        zmq::socket_t tmp(context, ZMQ_REQ);
                                        client = std::move(tmp);
                                        client.connect(SERVER_ENDPOINT);
                                        client.set(zmq::sockopt::linger, linger);
                                }
                        }
                }
        }

}