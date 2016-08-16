#ifndef THREAD_POOL_METHOD_H_
#define THREAD_POOL_METHOD_H_
/* #include "nedmalloc.h" */
/* #include "tcpudpconnitem.h" */
#include "threadpoolfunctions.h"
/* typedef void (*DataHandleProc)(TcpConnItemData); */

class ThreadPoolMethod{
public:
        ThreadPoolMethod(DataHandleProc proc){
                proc_=proc;
        }
        void operator()(TcpConnItemData arg)//此处只能用拷贝，不能用引用
        {
                proc_(arg);
                //回收内存
                nedalloc::nedfree(arg.data);
        }
private:
        DataHandleProc proc_;
};
#endif
