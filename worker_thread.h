/*
 * worker_thread.h
 *
 *  Created on: 2016年4月12日
 *      Author: chenms
 */
#ifndef WORKER_THREAD_H_
#define WORKER_THREAD_H_

#include"tcpudpconnitem.h"
/* class WorkerThread; */
/* extern thread_local WorkerThread *pthread_info; */
class LibeventServer;
class WorkerThread
{
public:
	/* WorkerThread(int tm,int tm_threshold,LibeventServer * ls); */
	WorkerThread(LibeventServer * ls);
	~WorkerThread();
	bool Run();//运行工作线程
	//send data handle
	static void	SendDataToClient(WorkerThread * pwt);
	//kill tcp connection
	static void KillTcpConnection(WorkerThread * pwt);
	//TCP 处理
	static void HandleTcpConn(WorkerThread* pwt);
	static void TcpConnReadCb(bufferevent * bev,void *ctx);
	static void TcpConnEventCB(bufferevent *bev,short int  events,void * ctx);
	//UDP 处理
	static void HandleUdpConn(WorkerThread* pwt);
	static void UdpConnReadCb(evutil_socket_t fd, short what, void * arg);
	static void UdpConnEventCB(evutil_socket_t fd, short what, void * arg);
	//超时处理
	static void TimingProcessing(evutil_socket_t fd, short what, void * arg);
	//获取通知的处理
	static void HandleConn(evutil_socket_t fd, short what, void * arg);
	//线程缓存数据处理
	void AddTcpConnItem(std::shared_ptr<TcpConnItem> tci){
		map_tcp_conns_.insert(std::make_pair(tci->sessionid,tci));
	}

	std::shared_ptr<TcpConnItem> FindTcpConnItem(unsigned int sessionid)
	{
		auto pos=map_tcp_conns_.find(sessionid);
		if(pos!=map_tcp_conns_.end())
			return pos->second;
		else {
			return std::shared_ptr<TcpConnItem>();
		}
	}

	void DeleteTcpConnItem(unsigned int sessionid){
		map_tcp_conns_.erase(sessionid);
		/* delete tci; */
	}

	void AddUdpConnItem(std::shared_ptr<UdpConnItem> uci)
	{
		map_udp_conns_.insert(std::make_pair(uci->udp_sock_,uci));
	}

	void DeletUdpConnItem(int udp_sock){
		map_udp_conns_.erase(udp_sock);
		/* delete uci; */
	}

	void PushTcpIntoQueue(std::shared_ptr<TcpConnItem> tcpconn){
		std::lock_guard<std::mutex>  lock(mutex_tcp_queue_);
		queue_tcp_conns_.push(tcpconn);
	}

	std::shared_ptr<TcpConnItem> PopTcpFromQueue()
	{
		std::lock_guard<std::mutex>  lock(mutex_tcp_queue_);
		std::shared_ptr<TcpConnItem> ret=queue_tcp_conns_.front();
		queue_tcp_conns_.pop();
		return ret;
	}

	void PushUdpIntoQueue(sockaddr_in& addr){
		std::lock_guard<std::mutex> lock(mutex_udp_queue_);
		queue_udp_addrs_.push(addr);
	}

	void PopUdpFromQueue(sockaddr_in& addr){
		std::lock_guard<std::mutex> lock(mutex_udp_queue_);
		addr=queue_udp_addrs_.front();
		queue_udp_addrs_.pop();
	}

	bool NotifyWorkerThread(const char* pchar)
	{
		std::lock_guard<std::mutex> lock(mutex_notify_send_fd_);
		if(write(notfiy_send_fd_,pchar, 1)!=1)
			return false;
		return true;
	}

	/* void SetThreadPool(std::shared_ptr<ThreadPool> thread_pool){ */
	/* 	thread_pool_=thread_pool; */
	/* } */

	/* void SetThreadPoolMethod(std::shared_ptr<ThreadPoolMethod> method) */
	/* { */
	/* 	thread_pool_method_=method; */
	/* } */

	void CloseTcpConn(unsigned int sessionid)//actively close the connection
	{
		auto pos=map_tcp_conns_.find(sessionid);
		if(pos!=map_tcp_conns_.end())
		{
			/* pthread_info->DeleteTcpConnItem(ptci->tcp_sock_); */
			bufferevent_free(pos->second->bev);
			map_tcp_conns_.erase(pos);
		}
	}

	void PushResultIntoQueue(TcpConnItemData& result)
	{
		std::lock_guard<std::mutex> lock(mutex_queue_result_);
		queue_result_.push(result);
	}

	TcpConnItemData PopResultFromQueue()
	{
		std::lock_guard<std::mutex>  lock(mutex_queue_result_);
		TcpConnItemData ret=queue_result_.front();
		queue_result_.pop();
		return ret;
	}

	void PushKillIntoQueue(unsigned int sessionid)
	{
		std::lock_guard<std::mutex> lock(mutex_queue_kill_);
		queue_kill_.push(sessionid);
	}

	unsigned int PopKillFromQueue()
	{
		std::lock_guard<std::mutex>  lock(mutex_queue_kill_);
		unsigned int ret=queue_kill_.front();
		queue_kill_.pop();
		return ret;
	}
private:
	bool CreateNotifyFds();//创建主线程和工作线程通信管道
	bool InitEventHandler();//初始化事件处理器
public:
	evutil_socket_t  notfiy_recv_fd_;//工作线程接收端
	evutil_socket_t  notfiy_send_fd_;//监听线程发送端
	std::mutex mutex_notify_send_fd_;
	std::mutex mutex_queue_result_;
	std::queue<TcpConnItemData> queue_result_;
	std::mutex mutex_queue_kill_;
	std::queue<unsigned int> queue_kill_;
	std::shared_ptr<std::thread>   shared_ptr_thread_;
	std::unordered_map<unsigned int,std::shared_ptr<TcpConnItem> > map_tcp_conns_;
	std::map<int,std::shared_ptr<UdpConnItem> > map_udp_conns_;
	std::mutex mutex_tcp_queue_;
	std::queue<std::shared_ptr<TcpConnItem> > queue_tcp_conns_;
	std::mutex mutex_udp_queue_;
	std::queue<sockaddr_in> queue_udp_addrs_;
	/* std::shared_ptr<ThreadPool> thread_pool_; */
	/* std::shared_ptr<ThreadPoolMethod> thread_pool_method_; */
	LibeventServer * ls_;
private:
	struct event  * pnotify_event_; //主线程通知工作线程连接到来事件
	/* DataHandleProc handle_data_proc_;//数据处理回调函数 */
	struct event_base * pthread_event_base_;
	struct event * ptimeout_event_;
	/* int overtime_threshold_; */
	/* int timespan_; */

};
#endif



