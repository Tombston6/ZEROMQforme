#include "../zhelpers.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
        if (argc < 2)
        {
                std::cout << "syntax: peering1 me {your}..." << std::endl;
                exit(EXIT_FAILURE);
        }
        char *self = argv[1];
        std::cout << self << std::endl;

        zmq::context_t ctx(1);
        zmq::socket_t statebe(ctx, ZMQ_PUB);
        std::string beadr = std::string("ipc://") + std::string(self) + "-state.ipc";
        std::cout << beadr << std::endl;
        statebe.bind(beadr);

        zmq::socket_t statefe(ctx, ZMQ_SUB);
        int argn;
        for(argn = 2; argn < argc; argn++)
        {
                char* peer = argv[argn];
                printf ("I: 正在连接至同伴代理 '%s' 的状态流后端\n", peer);
                std::string pradr = std::string("ipc://") + std::string(peer) + "-state.ipc";
                statefe.connect(pradr);
        }

        //一定要设置过滤器，zmq默认屏蔽所有的信息，同时0这个参数也不可缺少
        statefe.setsockopt(ZMQ_SUBSCRIBE, "", 0);
        
        while(1)
        {
                zmq::pollitem_t items[] =
                {
                        {statefe, 0, ZMQ_POLLIN, 0}
                };
                int rc = zmq::poll(items, 1, 1000);
                if (rc == -1)
                        break;

                if(items[0].revents & ZMQ_POLLIN)
                {
                        zmq::message_t name;
                        zmq::message_t num;
                        statefe.recv(&name);
                        statefe.recv(&num);
                        std::cout << "同伴代理:" << name.to_string() << "有" << num.to_string() << "个work空间" << std::endl;
                }
                else
                {
                        zmq::message_t backadr(self, strlen(self));
                        statebe.send(backadr, ZMQ_SNDMORE);
                        statebe.send("3", 1);
                        std::cout << self << std::endl;
                 }

        }

        return 0;
}