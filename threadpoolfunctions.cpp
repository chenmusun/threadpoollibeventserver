#include"threadpoolfunctions.h"
#include"libevent_server.h"
#include"dataencap.h"
// KeyProc threadpoolfunctions[]={
//         {"dxpupload",DxpUpload},
//         {"dxpdownload",DxpDownload},
// };
static void WritePacketToFile(IotPacket * packet,int fd)
{
    // struct IotPacket
    // {
    //     bool hasimei;
    //     bool hastimestamp;
    //     bool hasMSGTTL;
    //     bool haschk;
    //     bool isack;
    //     bool needack;
    //     uint8_t version;
    //     uint8_t transmode;
    //     uint8_t priority;
    //     uint8_t packetid;
    //     uint8_t IMEI[16];
    //     uint32_t TIMESTAMP;
    //     uint16_t MSGTTL;
    //     struct ServiceData * data;
    // };                          //
    char buf[1000]={0};
    int len=sprintf(buf,"hasimei :%d\nhastimestamp :%d\nhasMSGTTL ：%d\nhaschk :%d\nisack :%d\nneedack :%d\nversion :%d\ntransmode :%d\npriority :%d\npacketid :%d\nIMEI :%s\nTIMESTAMP :%u\nMSGTTL :%d\n",packet->hasimei,packet->hastimestamp,packet->hasMSGTTL,packet->haschk,packet->isack,packet->needack,packet->version,packet->transmode,packet->priority,packet->packetid,(char *)packet->IMEI,packet->TIMESTAMP,packet->MSGTTL);
        write(fd,buf,len);

    //service data
    struct ServiceData * servicedata=packet->data;
    while (servicedata) {
        char servicebuf[1000]={0};
        len=sprintf(servicebuf,"serviceid :%d\ncmd :%d\nhasnext :%d\nlength %d\nservicedata :\n",servicedata->serviceid,servicedata->cmd,servicedata->hasnext,servicedata->length);
        // struct ServiceData
        // {
        //     struct ServiceData * next;
        //     uint16_t serviceid;
        //     uint8_t cmd;
        //     bool hasnext;
        //     uint16_t length;
        //     uint8_t data[1];
        // };                      //
        write(fd,servicebuf,len);
        write(fd,servicedata->data,servicedata->length);
        write(fd,"\n",1);
        if(servicedata->hasnext)
            servicedata=servicedata->next;
        else {
            break;
        }
    }


}


void DxpUpload(TcpConnItemData data)
{
        BinCodes codes;
        codes.bincodes=(uint8_t *)data.data;
        codes.length=data.length;

        IotPacketWithStatus *packet=TransformBinCodesToIotPacket(&codes);
        //write to file
        if(packet)
        {
                if(!packet->statuscode)
                {
                        //验证合法性 合法继续后续操作，不合法断开连接

                        //保存IMEI号和tcp连接的映射关系
                        WorkerThread * pthreadinfo=data.tcpconnitemptr.lock()->pthreadinfo;
                        pthreadinfo->ls_->InsertImeiThreadIntoMap((const char *)packet->packet->IMEI,data.tcpconnitemptr);

                        int log_fd=open("./test.log",O_RDWR|O_CREAT|O_APPEND,S_IRWXU);
                        if(log_fd!=-1)
                                WritePacketToFile(packet->packet,log_fd);
                        //test 返回结果
                        // std::shared_ptr<DataToThread> ret(new DataToThread());
                        // DataToThread ret(data.data,data.sessionid,data.length);

                        // pthreadinfo->PushResultIntoQueue(std::shared_ptr result);
                        // pthreadinfo->NotifyWorkerThread('r');

                        //向REDIS注册，失败写log

                        //向kafka写消息
                }
                else {
                        LOG(WARNING)<<"Decode failed and error num is "<<packet->statuscode;
                }
                FreeIotPacketOnHeap(packet);
        }
}





void DxpDownload(TcpConnItemData data)
{

}
