#ifndef TCP_UDP_CONN_ITEM_H_
#define TCP_UDP_CONN_ITEM_H_

#include<event2/util.h>
#include<event2/event.h>
#include<event2/bufferevent.h>
#include<thread>
#include<memory>
#include<map>
#include<mutex>
#include <fcntl.h>
#include<unistd.h>
#include<event2/buffer.h>
#include<queue>
//增加日志功能
#define ELPP_THREAD_SAFE
#include"easylogging++.h"
/* #include "ThreadPool.h" */
/* #include "ThreadPoolMethod.h" */
#include "nedmalloc.h"
#include "dataencap.h"
#include<unordered_map>

class WorkerThread;
enum ProtocolName{DXP,MXP,UNKNOWN};
enum TcpPacketType{TCPDEVICE,TCPSMS,TCPTERMINAL};

struct TcpConnItem{
    TcpConnItem(int sock,unsigned int session, WorkerThread *threadinfo){
        tcp_sock_=sock;
        data=NULL;
        totallength=0;
        remaininglength=0;
        sessionid=session;
        bev=NULL;
        packettype=TCPDEVICE;
        pthreadinfo=threadinfo;
        protocol=DXP;
    }

//TODO 将接口封装好
    void SetBuffer(bufferevent * b)
        {
            bev=b;
        }
    bool SetProtocol(ProtocolName p)
        {
            protocol=p;
        }
    void SetPacketType(TcpPacketType type)
        {
            packettype=type;
        }

    bool HasRemaining()
        {
            return remaininglength;
        }
    bool AllocateCopyData( struct evbuffer * in,unsigned short length=0)//allocate need length
        {
            bool ret=false;
            do{
                if(!totallength){
                    data=nedalloc::nedmalloc(length);
                    if(!data)
                        break;
                    totallength=length;
                    remaininglength=length;
                }

                unsigned short copied=evbuffer_remove(in,data+totallength-remaininglength,remaininglength);
                if(copied==-1)
                    break;
                remaininglength-=copied;
                ret=true;

            }while (0);
            return ret;
        }

    void FreeData()
        {
            if(data){
                nedalloc::nedfree(data);
                data=NULL;
            }
            totallength=0;
            remaininglength=0;
        }

    void ReleaseDataOwnership()
        {
            data=NULL;
            totallength=0;
            remaininglength=0;
        }
    int tcp_sock_;
    void * data;
    unsigned short totallength;
    unsigned short remaininglength;
    /* ProtocolName protocol; */
    unsigned int sessionid;
    bufferevent *bev;
    TcpPacketType packettype;
    WorkerThread *pthreadinfo;
    ProtocolName protocol;
};

struct TcpConnItemData{
    TcpConnItemData(std::shared_ptr<TcpConnItem> p,void * d,unsigned short l)
        {
            tcpconnitemptr=p;
            data=d;
            length=l;
        }
    std::weak_ptr<TcpConnItem> tcpconnitemptr;
    void * data;
    unsigned short length;
};

struct UdpConnItem{
    UdpConnItem(int sock){
        udp_sock_=sock;
        pudp_read_event_=NULL;
    }
    ~UdpConnItem(){
        if(pudp_read_event_)
            event_free(pudp_read_event_);
    }
    int udp_sock_;
    struct event * pudp_read_event_;
};
#endif
