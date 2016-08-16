/*
 * libevent_server.h
 *
 *  Created on: 2016年4月8日
 *      Author: chenms
 */
#ifndef LIBEVENT_SERVER_H_
#define LIBEVENT_SERVER_H_
/* #include "worker_thread.h" */
#include<event2/listener.h>
#include<event2/bufferevent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unordered_map>
#include "ThreadPool.h"
#include "ThreadPoolMethod.h"
class LibeventServer{
public:
	/* LibeventServer(int tcp_port,int udp_port,int num_of_threads,int overtime,int timespan,int threadpool,ThreadPoolMethod * method); */
	LibeventServer(int tcp_port,int sms_port,int status_port,int udp_port,int num_of_threads,int overtime,int timespan,int threadpool);
	~LibeventServer();
	void WaitForListenThread();
	bool RunService();
	/* Tcp 处理 */
	static void AcceptTcpConn(evconnlistener *, int, sockaddr *, int, void *);
	static void AcceptTcpError(evconnlistener *, void *);
	/* Udp 处理 */
	static void AcceptUdpConn(evutil_socket_t fd, short what, void * arg);
	/* 定时处理 */
	static void TimingProcessing(evutil_socket_t fd, short what, void * arg);
	/* static void AcceptUdpError(evutil_socket_t fd, short what, void * arg); */
	void InsertImeiThreadIntoMap(const std::string& imei,std::weak_ptr<TcpConnItem> tcpconnptr)
	{
		std::lock_guard<std::mutex> lock(mutex_imei_thread_map_);
		imei_thread_map_.insert(std::make_pair(imei,tcpconnptr));
	}
	void EraseImeiThreadFromMap(const std::string& imei)
	{
		std::lock_guard<std::mutex> lock(mutex_imei_thread_map_);
		imei_thread_map_.erase(imei);
	}

	bool InitThreadPoolMethods();
private:
	bool StartTcpListen(int port,event_base ** base,evconnlistener ** listener,evconnlistener_cb TcpConnCb,evconnlistener_errorcb TcpErrCb,std::shared_ptr<std::thread> * thread);
	bool StartUdpListen();
	bool StartOvertimeCheck();//TODO暂时不用，超时检查用内部线程，不用公共线程，可以省去锁的开销
	/* bool CreateThreadPool(); */
	bool CreateWorkerThreads();
	bool StartMQMessageListen();
	/* void SetThreadPoolMethod(ThreadPoolMethod* method){ */
	/* 	thread_pool_method_.reset(method); */
	/* } */
private:
	std::vector<std::shared_ptr<WorkerThread> > worker_thread_vec_;
	event_base * tcp_listen_base_;
	evconnlistener *tcp_conn_listenner_;
	event_base * tcp_sms_listen_base_;
	evconnlistener *tcp_sms_conn_listenner_;
	event_base * tcp_status_query_listen_base_;
	evconnlistener *tcp_status_query_conn_listenner_;
	event_base * udp_listen_base_;
	struct event  * udp_conn_event_;
	int udp_listen_socket_;
	struct event  * overtime_event_;
	event_base * overtime_check_base_;
	std::shared_ptr<std::thread> overtime_check_thread_;
	std::shared_ptr<std::thread> tcp_listen_thread_;
	std::shared_ptr<std::thread> udp_listen_thread_;
	std::shared_ptr<std::thread> tcp_sms_listen_thread_;
	std::shared_ptr<std::thread> tcp_status_query_listen_thread_;
	std::shared_ptr<std::thread> mq_message_listen_thread_;
	int tcp_listen_port_;//TCP监听端口号
	int tcp_sms_listen_port_;//SMS监听端口号
	int tcp_status_query_listen_port_;//STATUS QUERY监听端口号
	int udp_listen_port_;//UDP监听端口号
	int num_of_workers_;//工作线程总数

	std::shared_ptr<ThreadPool> thread_pool_;//线程池
    int thread_pool_num_;
	std::mutex mutex_imei_thread_map_;
	std::unordered_map<std::string,std::weak_ptr<TcpConnItem> > imei_thread_map_;
public:
	unsigned int session_id_;//used to identify a connection
	int overtime_threshold_;//最大超时值,暂时不用
	int timespan_;
	int last_thread_index_;
	std::mutex mutex_thread_index_sessionid_;
	std::unordered_map<std::string,std::shared_ptr<ThreadPoolMethod> > map_thread_pool_method_;
};
#endif


