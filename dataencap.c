#include "dataencap.h" /* Declarations of Iot data encapsulation module API */

/*-----------------------------------------------------------------------*/
/* get length from binary data packet*/
/*-----------------------------------------------------------------------*/
static int getlen(uint8_t * bytes){
        uint8_t lsb=bytes[0];
        uint8_t msb=0;
        if(lsb&0x01)
                msb=bytes[1];

        lsb=lsb>>1;
        return(lsb+msb*128);
}
/*-----------------------------------------------------------------------*/
/* set length into binary data packet*/
/*-----------------------------------------------------------------------*/
static int setlen(uint8_t * bytes,uint16_t len)
{
        uint8_t lsb=0,msb=0;
        if(len<0||len>32767)
                return DATAENCAP_ERR_LENGTH_OVER_MAX;
        msb=len/128;
        lsb=len%128;
        bytes[0]=lsb<<1;
        if(msb)
        {
                bytes[0]|=0x01;
                bytes[1]=msb;
        }
        return 0;
}
/*-----------------------------------------------------------------------*/
/* generate CHK to check the integrity of binary data packet*/
/*-----------------------------------------------------------------------*/
static uint8_t getchk(uint8_t * bytes,int len)
{
        uint8_t crc=0;
        while(len)
        {
                crc ^= bytes[len-1];
                len--;
        }
        return crc;
}

/*-----------------------------------------------------------------------*/
/* calculate the length of the binary codes except the STX ,CTRL and LEN fields*/
/*-----------------------------------------------------------------------*/
static uint16_t CalcPacketRemainLength(struct IotPacket * packet)
{
        uint16_t ret=2;
        struct ServiceData * servicedata=NULL;
        if(packet->hasimei)
                ret+=8;
        if(packet->hastimestamp)
                ret+=4;
        if(packet->hasMSGTTL)
                ret+=2;

        servicedata=packet->data;
        do{
                ret+=2+servicedata->length;
                ret+=servicedata->length>=128?2:1;

                if(servicedata->hasnext){
                        servicedata=servicedata->next;
                }
                else
                        break;

        }
        while(1);

        if(packet->haschk)
                ret+=1;
        return ret;
}
/*-----------------------------------------------------------------------*/
/* convert hex char to number*/
/*-----------------------------------------------------------------------*/
static int ConvertHexCharToNum(char c )/* hex char */
{
        int ret=-1;
        if(c<='9'&&c>='0')
                ret=c-'0';
        else if(c<='F'&&c>='A')
                ret=c-'A'+10;
        else if(c<='f'&&c>='a')
                ret=c-'a'+10;
        else{

        }

        return ret;
}
/*-----------------------------------------------------------------------*/
/* convert number to hex char*/
/*-----------------------------------------------------------------------*/
static char ConvertNumToHexChar(uint8_t num)
{
        char ret=-1;
        if(num>=0&&num<=9)
                ret=num+'0';
        else if(num>=10&&num<=15){
                ret=num+'A'-10;
        }

        return ret;
}
/*-----------------------------------------------------------------------*/
/* create struct ServiceData from given parameters*/
/*-----------------------------------------------------------------------*/
struct ServiceData * CreateServiceData(uint16_t serviceid,uint8_t cmd,uint16_t length,uint8_t data[],bool hasnext,struct ServiceData * next)
{
        struct ServiceData * ret=(struct ServiceData *)calloc(sizeof(struct ServiceData)+length-1,1);
        if(ret){
                ret->serviceid=serviceid;
                ret->cmd=cmd;
                ret->hasnext=hasnext;
                ret->next=next;
                ret->length=length;
                memcpy(ret->data,data,length);
        }

        return ret;

}
/*-----------------------------------------------------------------------*/
/* transform struct IotPacket into binary codes*/
/*-----------------------------------------------------------------------*/
static uint8_t * encode(
	struct IotPacket * packet,//pointer to the IotPacket object used to generate binary codes
    uint16_t * total_len,//pointer to the variable that will receive the total length of generated binary codes
    int *statuscode //status code that reflects the status
	)
{
        int remain_len=0;
        int pos=0;
        uint8_t * ret=NULL;
        bool result=false;
        struct ServiceData * servicedata=NULL,*nextservicedata=NULL;
        uint8_t CHK;
        *statuscode=0;
        do
        {
                remain_len=CalcPacketRemainLength(packet);//calculate remain len
                *total_len=remain_len>=128?remain_len+4:remain_len+3;
                ret=(uint8_t *)calloc(*total_len,1);
                if(!ret){
                        //memory error
                        *statuscode=DATAENCAP_ERR_MEM;
                        break;
                }

                ret[pos]=DATAENCAP_STX;//0x55
                pos++;
                //SET CTRL
                if(packet->version<1||packet->version>7)
                {
                        *statuscode=DATAENCAP_ERR_VERSION;
                        break;
                }

                ret[pos]=packet->version<<2;

                if(packet->hasimei)
                        ret[pos]|=0x80;
                if(packet->hastimestamp)
                        ret[pos]|=0x40;
                if(packet->hasMSGTTL)
                        ret[pos]|=0x20;
                if(packet->haschk)
                        ret[pos]|=0x02;
                pos++;
                //SET LEN
                if(*statuscode=setlen(&ret[pos],remain_len))
                        break;
                if(remain_len>=128)
                        pos+=2;
                else
                        pos+=1;

                //PACKET_TYPE and ACK
                if(packet->isack)
                        ret[pos]|=0x80;
                else{
                        if(packet->needack)
                                ret[pos]|=0x40;
                }

                //TRANS-MODE and PRIORITY
                if(packet->transmode<0||packet->transmode>7){
                        *statuscode=DATAENCAP_ERR_TRANSMODE;
                        break;
                }


                if(packet->priority<0||packet->priority>7){
                        *statuscode=DATAENCAP_ERR_PRIORITY;
                        break;
                }


                ret[pos]|=packet->transmode<<3;
                ret[pos]|=packet->priority;
                pos++;
                //PACKET-ID
                ret[pos]=packet->packetid;
                pos++;
                //ctrl segment
                if(packet->hasimei){
                        bool isimeivalid=true;
                        int i=0;
                        for(;i<15;++i)
                        {
                                int c=ConvertHexCharToNum(packet->IMEI[i]);

                                if(c<0)
                                {
                                        *statuscode=DATAENCAP_ERR_IMEI;
                                        isimeivalid=false;
                                        break;
                                }

                                if(i%2==0)
                                {
                                        ret[pos]=c<<4;
                                }
                                else{
                                        ret[pos]+=c;
                                        pos++;
                                }
                        }

                        if(!isimeivalid)
                                break;
                        pos++;
                }

                if(packet->hastimestamp){
                        /* if(packet->TIMESTAMP<0||packet->TIMESTAMP>4294967296) */
                        /* { */
                        /*         *statuscode=DATAENCAP_ERR_TIMESTAMP; */
                        /*         break; */
                        /* } */
                        uint32_t dividend=packet->TIMESTAMP;
                        uint32_t divisor=256*256*256;
                        int i=0;
                        for(;i<4;++i)
                        {
                                uint8_t c=dividend/divisor;
                                dividend%=divisor;
                                divisor/=256;
                                ret[pos+i]=c;
                        }
                        pos+=4;
                }

                if(packet->hasMSGTTL){
                        /* if(packet->MSGTTL<0||packet->MSGTTL>65536) */
                        /* { */
                        /*         *statuscode=DATAENCAP_ERR_MSGTTL; */
                        /*         break; */
                        /* } */
                        ret[pos]=packet->MSGTTL/256;
                        ret[pos+1]=packet->MSGTTL%256;
                        pos+=2;
                }

                servicedata=packet->data;
                do{
                        if(servicedata->serviceid<0||servicedata->serviceid>1024){
                                *statuscode=DATAENCAP_ERR_SERVICEID;
                                break;
                        }
                        ret[pos]=servicedata->serviceid>>2;
                        pos++;
                        ret[pos]|=(servicedata->serviceid%4)<<6;
                        if(servicedata->cmd<0||servicedata->cmd>32)
                        {
                                *statuscode=DATAENCAP_ERR_SERVICECMD;
                                break;
                        }

                        ret[pos]|=servicedata->cmd<<1;
                        if(servicedata->hasnext){
                                ret[pos]|=0x01;
                                nextservicedata=servicedata->next;
                        }
                        else {
                                nextservicedata=NULL;
                        }


                        pos++;
                        if(*statuscode=setlen(&ret[pos],servicedata->length))
                                break;
                        if(servicedata->length>=128)
                                pos+=2;
                        else
                                pos+=1;

                        if(servicedata->length)
                        {
                                memcpy(&ret[pos],servicedata->data,servicedata->length);
                                pos+=servicedata->length;
                        }
                        servicedata=nextservicedata;
                }
                while(servicedata);

                if(*statuscode)//error has occured
                        break;
                //set CHK
                if(packet->haschk){
                        CHK=getchk(&ret[0],*total_len-1);
                        ret[*total_len-1]=CHK;
                }
                result=true;

        }while(0);
        if(!result){
                free(ret);
                ret=NULL;
        }

        return ret;
}
/*-----------------------------------------------------------------------*/
/* transform binary codes into struct IotPacket*/
/*-----------------------------------------------------------------------*/
static struct IotPacket *  decode(
	uint8_t * bytes,//pointer to the start of binary codes that will be handled
	uint16_t  total_len,//the length of the binary codes
    int * statuscode//pointer to the object receiving statuscode
)
{
        /* int ret=0; */
        struct IotPacket * packet=NULL;
        int pos=0;
        int remain_len=0;
        int total_len_tmp=0;
        uint8_t CHK=0;
        int servicelen=0;
        *statuscode=0;

        struct ServiceData * servicedata=NULL,*lastservicedata=NULL;
        do{
                if(total_len<9){
                        *statuscode=DATAENCAP_ERR_LENGTH_NOT_MATCH;
                        break;
                }


                /* memset(packet,0,sizeof(struct IotPacket)); */
                packet=(struct IotPacket *)calloc(sizeof(struct IotPacket),1);
                if(!packet){
                        *statuscode=DATAENCAP_ERR_MEM;
                        break;
                }

                if(bytes[pos]==0x55){
                        pos++;
                        if(bytes[pos]&0x01){
                                *statuscode=DATAENCAP_ERR_SPARE;
                                break;
                        }


                        remain_len=getlen(&bytes[pos+1]);
                        if(remain_len>=128)
                                total_len_tmp=remain_len+4;
                        else {
                                total_len_tmp=remain_len+3;
                        }

                        if(total_len_tmp!=total_len)
                        {
                                *statuscode=DATAENCAP_ERR_LENGTH_NOT_MATCH;
                                break;
                        }


                        if((bytes[pos]>>1)&0x01){
                                CHK=getchk(bytes,total_len-1);
                                if(CHK!=bytes[total_len-1]){
                                        *statuscode=DATAENCAP_ERR_CHK;
                                        break;
                                }

                                packet->haschk=true;

                        }

                        if(remain_len>=128)
                                pos+=3;
                        else
                                pos+=2;

                        //version
                        packet->version=(bytes[1]>>2)&0x07;

                        //PACKET_TYPE
                        packet->isack=bytes[pos]&0x80;
                        //ACK
                        packet->needack=bytes[pos]&0x40;
                        //TRANS-MODE
                        packet->transmode=(bytes[pos]>>3)&0x07;
                        //PRIORITY
                        packet->priority=bytes[pos]&0x07;

                        pos++;
                        packet->packetid=bytes[pos];

                        pos++;
                        //IMEI
                        if(bytes[1]&0x80)
                        {
                                packet->hasimei=true;
                                int i=0;
                                for(;i<8;++i)
                                {
                                        packet->IMEI[2*i]=ConvertNumToHexChar(bytes[pos]>>4);
                                        packet->IMEI[2*i+1]=ConvertNumToHexChar(bytes[pos]&0x0f);
                                        ++pos;
                                }

                                packet->IMEI[15]='\0';
                        }
                        //TIMESTAMP
                        if(bytes[1]&0x40)
                        {
                                packet->hastimestamp=true;
                                packet->TIMESTAMP=bytes[pos]*256*256*256+bytes[pos+1]*256*256+bytes[pos+2]*256+bytes[pos+3];
                                pos+=4;
                        }
                        //MSGTTL
                        if(bytes[1]&0x20)
                        {
                                packet->hasMSGTTL=true;
                                packet->MSGTTL=bytes[pos]*256+bytes[pos+1];
                                pos+=2;
                        }

                        //SERVICE DATA
                        packet->data=NULL;
                        do{
                                servicelen=getlen(&bytes[pos+2]);
                                servicedata=(struct ServiceData *)calloc(sizeof(struct ServiceData)+servicelen-1,1);
                                if(!servicedata)
                                {
                                        *statuscode=DATAENCAP_ERR_MEM;
                                        break;
                                }
                                servicedata->serviceid=(bytes[pos]<<2)+((bytes[pos+1]&0xc0)>>6);
                                servicedata->cmd=(bytes[pos+1]&0x3e)>>1;
                                servicedata->hasnext=bytes[pos+1]&0x01;
                                servicedata->length=servicelen;
                                if(servicelen>=128)
                                        pos+=4;
                                else {
                                        pos+=3;
                                }
                                memcpy(servicedata->data,&bytes[pos],servicelen);
                                pos+=servicelen;

                                if(!packet->data)
                                {
                                        packet->data=servicedata;
                                        lastservicedata=packet->data;
                                }
                                 else{
                                        lastservicedata->next=servicedata;
                                        lastservicedata=servicedata;
                                }

                        }while (lastservicedata->hasnext);

                }
                else {
                        *statuscode=DATAENCAP_ERR_STX;
                        break;
                }
        }while (0);

        if(*statuscode)//error has occured
        {
                if(packet)
                        FreeServiceData(packet->data);
                free(packet);
                packet=NULL;
        }
        return packet;
}

/*-----------------------------------------------------------------------*/
/* transform struct IotPacket to struct BinCodes*/
/*-----------------------------------------------------------------------*/
struct BinCodes * TransformIotPacketToBinCodes(
        struct IotPacket * packet  //pointer to the struct Bincodes object
        )
{
        struct BinCodes * bin=(struct BinCodes *)calloc(sizeof(struct BinCodes),1);
        if(bin){
                bin->bincodes=encode(packet,&bin->length,&bin->statuscode);
        }

        return bin;
}

/*-----------------------------------------------------------------------*/
/* transform struct Bincodes to struct IotPacketWithStatus*/
/*-----------------------------------------------------------------------*/
struct IotPacketWithStatus * TransformBinCodesToIotPacket(
        struct BinCodes * bin  //pointer to the struct BinCodes object
        )
{
        struct IotPacketWithStatus * packet=(struct IotPacketWithStatus *)calloc(sizeof(struct IotPacketWithStatus),1);
        if(packet){
                packet->packet=decode(bin->bincodes,bin->length,&packet->statuscode);
        }
        return packet;
}

/*-----------------------------------------------------------------------*/
/* free struct IotPacketWithStatus that is allocated on heap*/
/*-----------------------------------------------------------------------*/
void FreeIotPacketOnHeap(
        struct IotPacketWithStatus *packet  //pointer to the struct IotPacketWithStatus object
        )
{
        if(packet){
                if(packet->packet)
                        FreeServiceData(packet->packet->data);
                free(packet->packet);
                free(packet);
                packet=NULL;
        }
}

/*-----------------------------------------------------------------------*/
/* free struct BinCodes that is allocated on heap*/
/*-----------------------------------------------------------------------*/
void FreeBinCodesOnHeap(
        struct BinCodes * bin  //pointer to the struct BinCodes object
        )
{
        if(bin){
                free(bin->bincodes);
                free(bin);
                bin=NULL;
        }
}


/*-----------------------------------------------------------------------*/
/* free ServiceData List*/
/*-----------------------------------------------------------------------*/
void FreeServiceData(
        struct ServiceData * data // pointer to the start of ServiceData List
        )
{
        struct ServiceData * pos=NULL;
        while (data) {
                pos=data;
                if(data->hasnext)
                        data=data->next;
                else
                        data=NULL;

                free(pos);
        }
}
