#include <vector>
#include <thread>
#include <memory>
#include <functional>

#include <zmq.hpp>
#include "../zhelpers.hpp"

class client_task
{
public:
        //构造函数，使用的是DEALER模式
        client_task()
                : ctx_(1),
                  client_socket_(ctx_, ZMQ_DEALER)
        {}

        void start()
        {
                char identity[10] = {};
                sprintf(identity, "%04X-%04X", within(0x10000), within(0x10000));
                printf("%s\n", identity);

                //设置id
                client_socket_.setsockopt(ZMQ_IDENTITY, identity, strlen(identity));
                client_socket_.connect("tcp://localhost:5570");

                //正在轮询的套接字
                zmq::pollitem_t items[] =
                {
                        {client_socket_, 0, ZMQ_POLLIN, 0}
                };

                int request_nbr = 0;
                try
                {
                        while (true)
                        {
                                //先接受100次信息再发送
                                for(int i = 0; i < 100; ++i)
                                {
                                        //多路复用输出/输入事件的方法,10毫秒一次
                                        zmq::poll(items, 1, 10);
                                        if(items[0].revents & ZMQ_POLLIN)
                                        {
                                                printf("\n%s ", identity);

                                                //将接收到的信息的详情打印出来
                                                s_dump(client_socket_);
                                        }
                                }
                                char request_string[16] = {};
                                sprintf(request_string, "request #%d", ++request_nbr);
                                client_socket_.send(request_string, strlen(request_string));
                        }
                }
                catch (std::exception &e) {}
        }
private:
        zmq::context_t ctx_;
        zmq::socket_t client_socket_;
};


class server_worker
{
        //将接收到的信息转发出去

public:
        server_worker(zmq::context_t &ctx, int sock_type)
                : ctx_(ctx),
                  worker_(ctx_, sock_type)
                  {}
        void work()
        {
                worker_.connect("inproc://backend");

                try
                {
                        while(true)
                        {
                                zmq::message_t identity;
                                zmq::message_t msg;
                                zmq::message_t copied_id;
                                zmq::message_t copied_msg;

                                //接收身份
                                worker_.recv(&identity);

                                //信封内容
                                worker_.recv(&msg);

                                int replies = within(5);
                                for(int reply = 0; reply < replies; ++reply)
                                {
                                        s_sleep(within(1000) + 1);
                                        copied_id.copy(&identity);
                                        copied_msg.copy(&msg);

                                        //ZMQ_SNDMORE表面，这帧数据先暂时别发送，直到下一帧消息也发送
                                        //多半用在数据大小超过帧的最大
                                        worker_.send(copied_id, ZMQ_SNDMORE);
                                        worker_.send(copied_msg);
                                }
                        }
                }
                catch(std::exception &e) {}
                }
private:
        zmq::context_t &ctx_;
        zmq::socket_t worker_;
};

class server_task
{
public:
        //构造函数，生成ROUTER和DEALER套接字
        server_task()
        : ctx_(1),
          frontend_(ctx_, ZMQ_ROUTER),
          backend_(ctx_, ZMQ_DEALER)
        {}

        enum {kMaxThread = 5};

        void run()
        {
                frontend_.bind("tcp://*:5570");
                backend_.bind("inproc://backend");

                //对于自定义的server_worker的容器
                std::vector<server_worker *> worker;

                //创建线程池
                std::vector<std::thread *> worker_thread;

                for(int i = 0; i < kMaxThread; ++i)
                {
                        //先创建一个新的server_worker
                        worker.push_back(new server_worker(ctx_, ZMQ_DEALER));

                        //将server_worker.work的线程放入线程池
                        worker_thread.push_back(new std::thread(std::bind(&server_worker::work, worker[i])));

                        //当线程结束的时候回收所有的资源
                        worker_thread[i]->detach();
                }

                try
                {
                        //解耦
                        zmq::proxy(static_cast<void*>(frontend_), static_cast<void*>(backend_), nullptr);
                }
                catch (std::exception &e) {}

                for(int i = 0; i < kMaxThread; ++i)
                {
                        //对一个new的销毁
                        delete worker[i];
                        delete worker_thread[i];
                         }
        }
private:
        zmq::context_t ctx_;
        zmq::socket_t frontend_;
        zmq::socket_t backend_;
};

int main(void)
{
        client_task ct1;
        client_task ct2;
        client_task ct3;
        server_task st;

        std::thread t1(std::bind(&client_task::start, &ct1));
        std::thread t2(std::bind(&client_task::start, &ct2));
        std::thread t3(std::bind(&client_task::start, &ct3));
        std::thread t4(std::bind(&server_task::run, &st));

        //当线程退出的时候所有的资源都将释放
        t1.detach();
        t2.detach();
        t3.detach();
        t4.detach();

        getchar();
        return 0;
}