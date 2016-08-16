#ifndef THREAD_POOL_FUNCTIONS_H_
#define THREAD_POOL_FUNCTIONS_H_
/* #include"tcpudpconnitem.h" */
#include"worker_thread.h"

typedef void (*DataHandleProc)(TcpConnItemData);
struct KeyProc{
        const char * key;
        DataHandleProc proc;
};

extern void DxpUpload(TcpConnItemData );
extern void DxpDownload(TcpConnItemData );

const KeyProc threadpoolfunctions[]={
        {"dxpupload",DxpUpload},
        {"dxpdownload",DxpDownload},
};

/* extern KeyProc threadpoolfunctions[]={ */

/* } */
#endif
