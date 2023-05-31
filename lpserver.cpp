#include <zmq.hpp>
#include <iostream>
#include <string>
#include <ctime>
#include <cstdlib>

int main(void)
{
        zmq::context_t context(1);
        zmq::socket_t server(context, ZMQ_REP);
        try
        {
                server.bind("tcp://*:5555");
        }
        catch (std::exception& e)
        {
                std::cout << "绑定失败" << std::endl;
        }
        srand(time(0));

        int cycle = 0;
        while(1)
        {
                zmq::message_t msg;
                try
                {
                        auto r_flag = server.recv(msg, zmq::recv_flags::none);
                }
                catch (std::exception &e)
                {
                        std::cerr << "Error: " << e.what() << std::endl;
                }
                cycle++;

                if(cycle > 3 && (rand() % 10) == 0)
                {
                        std::cout << "I: 模拟程序崩溃" << std::endl;
                        break;
                }
                else
                if(cycle > 3 && (rand() % 10) == 0)
                {
                        std::cout << "I: 模拟CPU过载" << std::endl;
                        sleep(2);
                }
                std::cout << "I: 正常请求" << msg.to_string() << std::endl;
                sleep(1);
                std::string tt = msg.to_string();
                zmq::message_t sg(tt.c_str(), strlen(tt.c_str()));
                server.send(sg, zmq::send_flags::none);
        }

        return 0;
}