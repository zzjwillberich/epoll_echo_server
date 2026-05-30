#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <unordered_map>
#include "threadspool.h"

constexpr int PORT = 8888;
constexpr int BUF_SIZE = 1024;
constexpr int MAX_CLIENTS = 1024;


std::unordered_map<int,std::string> client_bufs;

//设置非阻塞fd
void set_unblock(int fd){
    int flags = fcntl(fd,F_GETFL,0);
    flags = flags | O_NONBLOCK;
    fcntl(fd,F_SETFL,flags);
}

void write_unblock(int epfd, int fd, std::string data);

//echo功能函数  改进成循环write  写不完放在缓冲区
void response(int epfd,int fd,std::string data){
    if(client_bufs.find(fd) == client_bufs.end() || client_bufs[fd].empty()){
        write_unblock(epfd,fd,data);
    }else{
        client_bufs[fd].append(data);
        std::string s = std::move(client_bufs[fd]);
        write_unblock(epfd,fd,s);
    }
}


void write_unblock(int epfd,int fd,std::string data){
    int n = data.size();
    int written = 0;
    while(written < n){
        int w = write(fd,data.data()+written,data.size()-written);
        if(w < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
                ev.data.fd = fd;
                epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev);
                client_bufs[fd] = data.substr(written);  
                break;     
            }else if(errno == EINTR){
                continue;
            }else{
                std::cerr << "write异常" << std::endl;
                epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr);
                close(fd);
                std::cout << "客户端" << fd << "断开连接";
                break;
            }
        }
        written += w;
    }
    if(written == n){
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev);
    }
}


int main(){
    //创建listen_fd
    int listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd < 0){
        std::cerr << "socket创建失败" << std::endl;
        return 1;
    }
    std::cout << "socket创建成功" << std::endl;

    //设置端口复用和非阻塞
    int opt = 1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    set_unblock(listen_fd);
    std::cout << "端口复用和非阻塞设置成功" << std::endl;

    //bind绑定listen_fd
    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int ret;
    ret = bind(listen_fd,(struct sockaddr*)&server_addr,sizeof(server_addr));
    if(ret < 0){
        std::cerr << "bind失败" << std::endl;
        close(listen_fd);
        return 1;
    }
    std::cout << "bind成功" << std::endl;

    //listen开始监听
    ret = listen(listen_fd,5);
    if(ret < 0){
        std::cerr << "listen失败" << std::endl;
        close(listen_fd);
        return 1;
    }
    std::cout << "listen成功 开始监听" << std::endl;

    //创建epoll实例
    int epfd = epoll_create(1024);
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd,EPOLL_CTL_ADD,listen_fd,&ev);

    //创建线程池
    ThreadsPool tp(10);

    struct epoll_event events[MAX_CLIENTS];
    //事件主循环
    while(true){
        int n_ready = epoll_wait(epfd,events,MAX_CLIENTS,-1);
        if(n_ready < 0){
            if(errno == EINTR){
                continue;
            }else{
                std::cerr << "epoll_wait异常" << std::endl;
                close(listen_fd);
                return 1;
            }
        }
        
        for(int i = 0;i < n_ready;++i){
            struct epoll_event event = events[i];
            
            //listen_fd准备好了
            if(event.data.fd == listen_fd){
                struct sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                
                //循环accept
                while(true){
                    int client_fd = accept(listen_fd,(struct sockaddr*)&client_addr,&client_len);
                    
                    //accept出现问题
                    if(client_fd < 0){
                        //没有新连接了
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            break;
                        }
                        //被信号打断了
                        else if(errno == EINTR){
                            continue;
                        }
                        else{
                            std::cerr << "accept出现异常" << std::endl;
                            close(listen_fd);
                            return 1;
                        }
                    }

                    set_unblock(client_fd);
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = client_fd;
                    epoll_ctl(epfd,EPOLL_CTL_ADD,client_fd,&ev);
                    std::cout << "客户端" << client_fd << "链接成功" << std::endl;
                }
            }
            //客户端准备好了
            else{
                if(event.events & EPOLLIN){
                    //循环读取数据
                    int client_fd = event.data.fd;
                    char buf[BUF_SIZE];
                    memset(buf,0,BUF_SIZE);
                    while(true){
                        int n = read(client_fd,buf,BUF_SIZE - 1);
                        if(n < 0){
                            if(errno == EAGAIN || errno == EWOULDBLOCK){
                                break;
                            }else if(errno == EINTR){
                                continue;
                            }else{
                                std::cerr << "read出现异常" << std::endl;
                                std::cout << "客户端" << client_fd << "断开连接" << std::endl;
                                epoll_ctl(epfd,EPOLL_CTL_DEL,client_fd,nullptr);
                                close(client_fd);
                                break;
                            }
                        }else if(n == 0){
                            std::cout << "客户端" << client_fd << "主动断开链接"  << std::endl;
                            epoll_ctl(epfd,EPOLL_CTL_DEL,client_fd,nullptr);
                            close(client_fd);
                            break;
                        }else{
                            buf[n] = '\0';
                            std::string data = buf;
                            tp.submit([epfd, client_fd, data](){
                                response(epfd, client_fd, data);
                            });
                        }
                    }
                }else if(event.events & EPOLLOUT){
                    int client_fd = event.data.fd;
                    // 缓冲区有未写完的数据，继续写
                    auto it = client_bufs.find(client_fd);
                    if(it != client_bufs.end() && !it->second.empty()){
                        std::string remaining = std::move(it->second);
                        client_bufs.erase(it);
                        write_unblock(epfd, client_fd, remaining);
                    }
                }
            }
        }
    }

    close(listen_fd);
    close(epfd);
    return 0;
}