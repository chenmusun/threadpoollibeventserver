/*
 * libevent_server.cpp
 *
 *  Created on: 2016年4月8日
 *      Author: chenms
 */
#include "libevent_server.h"

thread_local int thread_local_port=-1;

// LibeventServer::LibeventServer(int tcp_port,int udp_port,int num_of_threads,int overtime,int timespan,int threadpool,ThreadPoolMethod * method)
LibeventServer::LibeventServer(int tcp_port,int sms_port,int status_port,int udp_port,int num_of_threads,int overtime,int timespan,int threadpool)
{
    tcp_listen_base_=NULL;
    tcp_conn_listenner_=NULL;
    udp_listen_base_=NULL;
    udp_conn_event_=NULL;
    overtime_event_=NULL;
    overtime_check_base_=NULL;
    tcp_sms_listen_base_=NULL;
    tcp_sms_conn_listenner_=NULL;
    tcp_status_query_listen_base_=NULL;
    tcp_status_query_conn_listenner_=NULL;
    tcp_listen_port_=tcp_port;
    tcp_sms_listen_port_=sms_port;
    tcp_status_query_listen_port_=status_port;
    udp_listen_port_=udp_port;
    num_of_workers_=num_of_threads;
    overtime_threshold_=overtime;
    udp_listen_socket_=-1;
    timespan_=timespan;
    last_thread_index_=-1;
    thread_pool_num_=threadpool;
    session_id_=0;
}
LibeventServer::~LibeventServer()
{
    if(tcp_listen_base_!=NULL)
        event_base_free(tcp_listen_base_);
    if(udp_listen_base_!=NULL)
        event_base_free(udp_listen_base_);
    if(overtime_check_base_!=NULL)
        event_base_free(overtime_check_base_);
    if(tcp_conn_listenner_!=NULL)
        evconnlistener_free(tcp_conn_listenner_);
    if(udp_conn_event_!=NULL)
        event_free(overtime_event_);
    if(overtime_event_!=NULL)
        event_free(overtime_event_);
    if(udp_listen_socket_!=-1)
        close(udp_listen_socket_);
    if(tcp_sms_listen_base_)
        event_base_free(tcp_sms_listen_base_);
    if(tcp_sms_conn_listenner_)
        evconnlistener_free(tcp_sms_conn_listenner_);
    if(tcp_status_query_listen_base_)
        event_base_free(tcp_status_query_listen_base_);
    if(tcp_status_query_conn_listenner_)
        evconnlistener_free(tcp_status_query_conn_listenner_);
}
//int port,event_base ** base,evconnlistener ** listener,evconnlistener_cb TcpConnCb,evconnlistener_cb TcpErrCb,std::shared_ptr<std::thread> * thread
bool LibeventServer::RunService()
{
	do
    {
        if(!InitThreadPoolMethods())
            break;
        if(!CreateWorkerThreads())
            break;
        if(!StartTcpListen(tcp_listen_port_,&tcp_listen_base_,&tcp_conn_listenner_,AcceptTcpConn,AcceptTcpError,&tcp_listen_thread_))
            break;
        if(!StartTcpListen(tcp_sms_listen_port_,&tcp_sms_listen_base_,&tcp_sms_conn_listenner_,AcceptTcpConn,AcceptTcpError,&tcp_sms_listen_thread_))
            break;
        if(!StartTcpListen(tcp_status_query_listen_port_,&tcp_status_query_listen_base_,&tcp_status_query_conn_listenner_,AcceptTcpConn,AcceptTcpError,&tcp_status_query_listen_thread_))
            break;
        if(!StartUdpListen())
            break;
        return true;
	}while(0);
	return false;
}

bool LibeventServer::StartMQMessageListen()
{
    return true;
}

void LibeventServer::WaitForListenThread()
{
        tcp_listen_thread_->join();
        tcp_sms_listen_thread_->join();
        tcp_status_query_listen_thread_->join();
        udp_listen_thread_->join();
        // overtime_check_thread_->join();
}


void LibeventServer::AcceptTcpError(evconnlistener *listener, void *ptr)
{
	//TODO
    LOG(ERROR)<<"TCP Listen Thread  Accept Error";
}

void LibeventServer::AcceptTcpConn(evconnlistener * listener, int sock, sockaddr * addr, int len, void *ptr)
{
  LibeventServer * pls=static_cast<LibeventServer *>(ptr);
  int cur_thread_index=0;
  unsigned int sessionid=0;
  {
      std::lock_guard<std::mutex>  lock(pls->mutex_thread_index_sessionid_);
      cur_thread_index = (pls->last_thread_index_ + 1) %pls->num_of_workers_; // 轮循选择工作线程
      pls->last_thread_index_ = cur_thread_index;
      sessionid=pls->session_id_++;
  }
  std::shared_ptr<TcpConnItem> ptci(new TcpConnItem(sock,sessionid,pls->worker_thread_vec_[cur_thread_index].get()));
  if(!ptci)
      return;

  if(pls->tcp_listen_port_==thread_local_port)
  {
      ptci->SetPacketType(TCPDEVICE);
  }
  else if(pls->tcp_sms_listen_port_==thread_local_port)
  {
      ptci->SetPacketType(TCPSMS);
  }
  else if(pls->tcp_status_query_listen_port_==thread_local_port){
      ptci->SetPacketType(TCPTERMINAL);
 }
  else{

  }

  pls->worker_thread_vec_[cur_thread_index]->PushTcpIntoQueue(ptci);//准备好数据然后通知

  if(!pls->worker_thread_vec_[cur_thread_index]->NotifyWorkerThread("t")){
      LOG(WARNING)<<"tcp notify worker failed";
      pls->worker_thread_vec_[cur_thread_index]->PopTcpFromQueue();
  }


}

void LibeventServer::AcceptUdpConn(evutil_socket_t fd, short what, void * arg){
    LOG(TRACE)<<"Accept udp Conn,Send it to worker";
    LibeventServer * pls=static_cast<LibeventServer *>(arg);
    socklen_t addr_len=sizeof(sockaddr_in);
    struct sockaddr_in addr;
    memset(&addr,0,addr_len);
    char buf[255]={0};
    //TODO
    if(recvfrom(fd,buf,255,0,(sockaddr *)&addr,&addr_len)==-1){
        return;
    }

    int cur_thread_index = (pls->last_thread_index_ + 1) %pls->num_of_workers_; // 轮循选择工作线程
    pls->last_thread_index_ = cur_thread_index;
    pls->worker_thread_vec_[cur_thread_index]->PushUdpIntoQueue(addr);//准备好数据然后通知
    if(!pls->worker_thread_vec_[cur_thread_index]->NotifyWorkerThread("u")){
        LOG(ERROR)<<"udp notify worker failed";
        pls->worker_thread_vec_[cur_thread_index]->PopUdpFromQueue(addr);
    }
}

bool LibeventServer::CreateWorkerThreads()
{
    bool ret=true;
    try {
        thread_pool_.reset(new ThreadPool(thread_pool_num_));
        for(int i=0;i<num_of_workers_;++i){
            // std::shared_ptr<WorkerThread> pti(new WorkerThread(timespan_,overtime_threshold_));
            std::shared_ptr<WorkerThread> pti(new WorkerThread(this));
            if(!pti->Run())
            {
                ret=false;
                break;
            }
            // pti->SetThreadPool(thread_pool_);//设置线程池
            // pti->SetThreadPoolMethod(thread_pool_method_);//设置线程池方法
            worker_thread_vec_.push_back(pti);
        }
    } catch (...) {
        ret=false;
    }

    return ret;
}

bool LibeventServer::StartTcpListen(int port,event_base ** base,evconnlistener ** listener,evconnlistener_cb TcpConnCb,evconnlistener_errorcb TcpErrCb,std::shared_ptr<std::thread> * thread)
{
	do{
        struct sockaddr_in sin;
        *base = event_base_new();
        if (!*base)
            break;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = htonl(0);
        sin.sin_port = htons(port);

        *listener = evconnlistener_new_bind(*base, TcpConnCb, this,
                                                      LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
                                                      (struct sockaddr*)&sin, sizeof(sin));
        if (!*listener)
            break;
        evconnlistener_set_error_cb(*listener, TcpErrCb);

        try{
            (*thread).reset(new std::thread([this,port,base]
                                                     {
                                                         thread_local_port=port;
                                                         event_base_dispatch(*base);
                                                     }
                                         ));
        }catch(...){
            break;
        }
        return true;
	}while(0);
    LOG(ERROR)<<"Tcp Listen on the port "<<tcp_listen_port_<<" failed"<<std::endl;
	return false;
}

bool LibeventServer::StartUdpListen(){
    do{
        udp_listen_socket_=socket(AF_INET,SOCK_DGRAM,0);
        if(udp_listen_socket_==-1)
        {
            LOG(ERROR)<<"creating udp listen socket fails";
            break;
        }
        struct sockaddr_in sin;
        udp_listen_base_ = event_base_new();
        if (!udp_listen_base_)
            break;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = htonl(0);
        sin.sin_port = htons(udp_listen_port_);
        if(bind(udp_listen_socket_,(const sockaddr *)&sin,sizeof(sin))==-1)
        {
            LOG(ERROR)<<"binding udp listen socket fails";
            break;
        }

        udp_conn_event_=event_new(udp_listen_base_,udp_listen_socket_,EV_READ | EV_PERSIST,AcceptUdpConn,(void *)this);
        if(udp_conn_event_==NULL)
            break;
        if(event_add(udp_conn_event_, 0))
            break;

        try{
            udp_listen_thread_.reset(new std::thread([this]
                                                     {
                                                         event_base_dispatch(udp_listen_base_);
                                                     }
                                         ));
        }catch(...){
            break;
        }
        return true;
    }while(0);
    LOG(ERROR)<<"Udp Listen on the port "<<udp_listen_port_<<" failed"<<std::endl;
    return false;
}
bool LibeventServer::StartOvertimeCheck()
{
    do{
        try{
            overtime_check_base_=event_base_new();
            if(!overtime_check_base_)
                break;
            //ADD OVERTIME
            overtime_event_=event_new(overtime_check_base_,-1,EV_TIMEOUT|EV_PERSIST,TimingProcessing,&overtime_threshold_);
            if(!overtime_event_)
                break;
            timeval tv={timespan_,0};
            if(event_add(overtime_event_,&tv)==-1)
                break;
            overtime_check_thread_.reset(new std::thread([this]
                                                         {
                                                             event_base_dispatch(overtime_check_base_);
                                                         }
                                             ));
        }
        catch(...){
            break;
        }
        return true;
    }while (0);
    LOG(ERROR)<<"failed start the overtime check thread";
    return false;
}
void LibeventServer::TimingProcessing(evutil_socket_t fd, short what, void * arg)
{
}

bool LibeventServer::InitThreadPoolMethods()
{
    try
    {
        for(int i=0;i<sizeof(threadpoolfunctions)/sizeof(KeyProc);++i)
        {
            auto keyproc=threadpoolfunctions[i];
            std::shared_ptr<ThreadPoolMethod> method(new ThreadPoolMethod(keyproc.proc));
            map_thread_pool_method_.insert(std::make_pair(keyproc.key,method));

        }
        return true;
    }
    catch(...)
    {
        return false;
    }
}
