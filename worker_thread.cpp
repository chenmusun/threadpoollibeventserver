/*
 * worker_thread.cpp
 *
 *  Created on: 2016年4月12日
 *      Author: chenms
 */
#include "libevent_server.h"
#include "worker_thread.h"

 thread_local WorkerThread *pthread_info=NULL;

// WorkerThread::WorkerThread(int tm,int tm_threshold,LibeventServer * ls)
WorkerThread::WorkerThread(LibeventServer * ls)
{
	pthread_event_base_=NULL;
	pnotify_event_=NULL;
	ptimeout_event_=NULL;
	notfiy_recv_fd_=-1;
	notfiy_send_fd_=-1;
    // overtime_threshold_=tm_threshold;
    // timespan_=tm;
    ls_=ls;
}

WorkerThread::~WorkerThread()
{
	if(notfiy_recv_fd_!=-1)
		close(notfiy_recv_fd_);
	if(notfiy_send_fd_!=-1)
		close(notfiy_send_fd_);
	if(pthread_event_base_!=NULL)
		event_base_free(pthread_event_base_);
	if(pnotify_event_!=NULL)
		event_free(pnotify_event_);
	if(ptimeout_event_)
		event_free(ptimeout_event_);

    // for(auto pos=map_tcp_conns_.begin();pos!=map_tcp_conns_.end();++pos)
    // 	delete pos->second;
    // for(auto pos=map_udp_conns_.begin();pos!=map_udp_conns_.end();++pos)
    // 	delete pos->second;
}

bool WorkerThread::Run()
{
	do{
		if(!CreateNotifyFds())
			break;
		if(!InitEventHandler())
			break;
		try{
			shared_ptr_thread_.reset(new std::thread([this]
													 {
                                                         pthread_info=this;//thread_local variable
														 event_base_loop(pthread_event_base_, 0);
													 }
										 ));
		}catch(...)
		{
			break;
		}
		return true;
	}while(0);
	return false;
}
bool WorkerThread::CreateNotifyFds()
{
	 int fds[2];
	 bool ret=false;
     if (!pipe2(fds,O_NONBLOCK))
	 {
		  notfiy_recv_fd_= fds[0];
		  notfiy_send_fd_ = fds[1];
		  ret=true;
	 }
	 return ret;
}

bool WorkerThread::InitEventHandler()
{
		do
		{
			pthread_event_base_=event_base_new();
			if(pthread_event_base_==NULL)
				break;
			pnotify_event_=event_new(pthread_event_base_,notfiy_recv_fd_,EV_READ | EV_PERSIST,HandleConn,(void *)this);
			if(pnotify_event_==NULL)
				break;
			if(event_add(pnotify_event_, 0))
				break;
            // ptimeout_event_=event_new(pthread_event_base_,-1,EV_TIMEOUT|EV_PERSIST,TimingProcessing,&overtime_threshold_);
            ptimeout_event_=event_new(pthread_event_base_,-1,EV_TIMEOUT|EV_PERSIST,TimingProcessing,&ls_->overtime_threshold_);
            if(!ptimeout_event_)
				break;
            timeval tv={ls_->timespan_,0};
			if(event_add(ptimeout_event_,&tv)==-1)
				break;
			return true;
		}while(0);
		return false;
}

void WorkerThread::HandleConn(evutil_socket_t fd, short what, void* arg)
{
    WorkerThread* pwt=static_cast<WorkerThread *>(arg);
    char  buf[1];
    if(read(fd, buf, 1)!=1)//从sockpair的另一端读数据
        LOG(ERROR)<<"workerthread accept connection failed\n";
    if(buf[0]=='t')
        HandleTcpConn(pwt);
    else if(buf[0]=='u')
        HandleUdpConn(pwt);
    else if(buf[0]=='r')
    {
        SendDataToClient(pwt);
    }
    else if(buf[0]=='k'){
            KillTcpConnection(pwt);
    }
    else{
        LOG(ERROR)<<"unkonwn protocol type";
    }
}


void WorkerThread::HandleTcpConn(WorkerThread* pwt)
{
    std::shared_ptr<TcpConnItem> ptci=pwt->PopTcpFromQueue();
    struct bufferevent * bev=bufferevent_socket_new(pwt->pthread_event_base_,ptci->tcp_sock_,BEV_OPT_CLOSE_ON_FREE);
	if(bev==NULL)
		return;
    //将链接信息加入缓存中
    ptci->SetBuffer(bev);
    pwt->AddTcpConnItem(ptci);
    bufferevent_setcb(bev, TcpConnReadCb, NULL/*ConnWriteCb*/, TcpConnEventCB,&ptci->sessionid/*arg*/);
    bufferevent_enable(bev, EV_READ /*| EV_WRITE*/ );
}


void WorkerThread::TcpConnReadCb(bufferevent * bev,void *ctx){
    unsigned int sessionid=*(static_cast<unsigned int *>(ctx));
    // std::shared_ptr<TcpConnItem> ptci=std::shared_ptr<TcpConnItem>(ctx);
    std::shared_ptr<TcpConnItem> ptci=pthread_info->FindTcpConnItem(sessionid);
        struct evbuffer * in=bufferevent_get_input(bev);
        struct evbuffer * out=bufferevent_get_output(bev);

        unsigned char ch[10]={0};
        unsigned short len=0;
        if(ptci->packettype!=TCPTERMINAL){
            if(ptci->HasRemaining())
                {
                        if(!ptci->AllocateCopyData(in)){
                                LOG(WARNING)<<"AllocateCopyData failed";
                                ptci->FreeData();
                        }

                        //has received an complete packet
                        if(!ptci->HasRemaining()){
                            TcpConnItemData data(ptci,ptci->data,ptci->totallength);
                            ptci->ReleaseDataOwnership();//释放所有权
                            std::shared_ptr<ThreadPoolMethod> method=pthread_info->ls_->map_thread_pool_method_["dxpupload"];
                            (*method)(data);
                        }
                }
                else{
                        if(evbuffer_copyout(in,ch,4)!=-1)
                        {
                                if(ch[0]==0x55){//DXP PROTOCOL
                                        uint8_t lsb=ch[2];
                                        uint8_t msb=0;
                                        if(lsb&0x01)
                                                msb=ch[3];

                                        lsb=lsb>>1;
                                        unsigned short len=lsb+msb*128;
                                        if(len>127)
                                                len+=4;
                                        else
                                                len+=3;

                                        len=lsb+msb*128;

                                        unsigned short occupied=0;

                                        if(len>127)
                                                len+=4;
                                        else
                                                len+=3;

                                        // ptci->SetProtocol(DXP);
                                }
                                else{//other protocols
                                        // ptci->SetProtocol(UNKNOWN);
                                }

                                if(!ptci->AllocateCopyData(in,len)){
                                        LOG(WARNING)<<"AllocateCopyData failed";
                                        ptci->FreeData();
                                }

                        }else{
                                LOG(WARNING)<<"Get the protocol name failed";
                                evbuffer_drain(in,65535);//clear the buffer
                        }

                }
        }
        else{//HANDLE CMD

        }


}

void WorkerThread::TcpConnEventCB(bufferevent *bev,short int  events,void * ctx){
    //TODO
	LOG(TRACE)<<"tcp conn takes an error";
    TcpConnItem* ptci=static_cast<TcpConnItem *>(ctx);
    //ThreadInfo * pti=static_cast<ThreadInfo *>(pthread_info);
    //when comes an error,delete the item in the cache
    pthread_info->DeleteTcpConnItem(ptci->sessionid);
    bufferevent_free(bev);
}

void WorkerThread::HandleUdpConn(WorkerThread* pwt){
    LOG(TRACE)<<"Worker got the udp conn";
    //Get the client address from the queue
	sockaddr_in addr;
	pwt->PopUdpFromQueue(addr);

    int udp_conn_socket=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    // sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(0);
    if(bind(udp_conn_socket,(const sockaddr*)&sin,sizeof(sin))==-1)
    {
        LOG(ERROR)<<"binding udp temp socket fails";
        return;
    }
    std::shared_ptr<UdpConnItem> puci(new UdpConnItem(udp_conn_socket));
    struct event *pudp_read_event=event_new(pwt->pthread_event_base_,udp_conn_socket,EV_READ | EV_PERSIST,UdpConnReadCb,(void *)puci.get());
    if(pudp_read_event==NULL)
        return;
    if(event_add(pudp_read_event, 0))
        return;
    puci->pudp_read_event_=pudp_read_event;
    //将udp“链接”信息加入threadinfo缓存中
    pwt->AddUdpConnItem(puci);
    //TODO send respond to the client by using the client_addr
    //   connect();
    //connect(udp_conn_socket, (struct sockaddr *)&addr, sizeof(sockaddr));
    if(sendto(udp_conn_socket,"hi",2,0,(const sockaddr *)&addr,sizeof(sockaddr))==-1){
        LOG(ERROR)<<"send to error";
    }
    // int sendto ( socket s , const void * msg, int len, unsigned int flags, const
    //              　　struct sockaddr * to , int tolen ) ;
}

void WorkerThread::UdpConnReadCb(evutil_socket_t fd, short what, void * arg){
    //TODO
    LOG(TRACE)<<"Read a udp data";
    socklen_t addr_len=sizeof(sockaddr_in);
    struct sockaddr_in addr;
    memset(&addr,0,addr_len);
    char buf[255]={0};
    if(recvfrom(fd,buf,255,0,(sockaddr *)&addr,&addr_len)==-1){
        return;
    }

    if(sendto(fd,"hi",2,0,(const sockaddr *)&addr,sizeof(sockaddr))==-1){
        LOG(ERROR)<<"send to error";
    }
    LOG(TRACE)<<"recvied data "<<buf;
}

void WorkerThread::UdpConnEventCB(evutil_socket_t fd, short what, void * arg){
    //TODO
    LOG(TRACE)<<"udp conn takes an error";
}

void WorkerThread::TimingProcessing(evutil_socket_t fd, short what, void * arg){
    //TODO
    // LOG(TRACE)<<"Timing Processing";
}


void WorkerThread::SendDataToClient(WorkerThread * pwt)
{
        if(pwt)
        {
                TcpConnItemData data=pwt->PopResultFromQueue();
                if(std::shared_ptr<TcpConnItem> ptr=data.tcpconnitemptr.lock()){//here seems don't need session id;
                        if(bufferevent_write(ptr->bev,data.data,data.length)==-1)
                        {

                        }
                }
                else{

                }
                nedalloc::nedfree(data.data);//释放资源，但data要由nedmalloc分配
        }
}

void WorkerThread::KillTcpConnection(WorkerThread * pwt)
{
        if(pwt)
        {
                unsigned int sessionid=pwt->PopKillFromQueue();
                pwt->CloseTcpConn(sessionid);
        }
}
