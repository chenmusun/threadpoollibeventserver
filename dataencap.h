/*----------------------------------------------------------------------------/
/  Iot data encapsulation module                 (C)Dedao, 2016
/-----------------------------------------------------------------------------/
/ Iot data encapsulation module is a free software that opened under license policy of
/ following conditions.
/
/ Copyright (C) 2016, Dedao, all right reserved.
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/----------------------------------------------------------------------------*/

#ifndef DATA_ENCAP_H_
#define DATA_ENCAP_H_
#include<string.h>
#include <stdlib.h>
#include <stdbool.h>

#define DATAENCAP_STX 0X55

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

/* Error values */
enum dataencap_err{
        DATAENCAP_ERR_MEM= -1,
        DATAENCAP_ERR_SUCCESS = 0,
        DATAENCAP_ERR_VERSION=1,
        DATAENCAP_ERR_TRANSMODE=2,
        DATAENCAP_ERR_PRIORITY=3,
        DATAENCAP_ERR_LENGTH_OVER_MAX=4,
        DATAENCAP_ERR_IMEI=5,
        DATAENCAP_ERR_STX=6,
        DATAENCAP_ERR_SPARE=7,
        DATAENCAP_ERR_LENGTH_NOT_MATCH=8,
        DATAENCAP_ERR_CHK=9,
        DATAENCAP_ERR_SERVICEID=10,
        DATAENCAP_ERR_SERVICECMD=11
        /* DATAENCAP_ERR_TIMESTAMP=12, */
        /* DATAENCAP_ERR_MSGTTL=13 */
};

/*-----------------------------------------------------------------------*/
/* transform mode*/
/*-----------------------------------------------------------------------*/
enum dataencap_transmode{
        DATAENCAP_TRANSMODE_MQTT=0,
        DATAENCAP_TRANSMODE__SMS=1,
        DATAENCAP_TRANSMODE_HTTP=2,
        DATAENCAP_TRANSMODE_WEBSOCKET=3,
        DATAENCAP_TRANSMODE_XMPP=4,
        DATAENCAP_TRANSMODE_COAP=5
};

/*-----------------------------------------------------------------------*/
/* priority*/
/*-----------------------------------------------------------------------*/
enum dataencap_priority{
        DATAENCAP_PRIORITY_NORMAL=0,
        DATAENCAP_PRIORITY_HIGH=1,
        DATAENCAP_PRIORITY_HIGHEST=2
};

/*-----------------------------------------------------------------------*/
/* version*/
/*-----------------------------------------------------------------------*/
enum dataencap_version{
        DATAENCAP_VERSION1=1
};

/*-----------------------------------------------------------------------*/
/* struct related to specific  service*/
/*-----------------------------------------------------------------------*/
struct ServiceData
{
        struct ServiceData * next;
        uint16_t serviceid;
        uint8_t cmd;
        bool hasnext;
        uint16_t length;
        uint8_t data[1];
};

/*-----------------------------------------------------------------------*/
/* struct used to exchange with binary codes*/
/*-----------------------------------------------------------------------*/
struct IotPacket
{
        bool hasimei;
        bool hastimestamp;
        bool hasMSGTTL;
        bool haschk;
        bool isack;
        bool needack;
        uint8_t version;
        uint8_t transmode;
        uint8_t priority;
        uint8_t packetid;
        uint8_t IMEI[16];
        uint32_t TIMESTAMP;
        uint16_t MSGTTL;
        struct ServiceData * data;
};


/*-----------------------------------------------------------------------*/
/* struct used to exchange with binary codes*/
/*-----------------------------------------------------------------------*/
struct IotPacketWithStatus{
        struct IotPacket * packet;
        int statuscode;
};

/*-----------------------------------------------------------------------*/
/* struct that stores encoded binary codes*/
/*-----------------------------------------------------------------------*/
struct BinCodes{
        uint8_t * bincodes;
        uint16_t length;
        int statuscode;
};

/*-----------------------------------------------------------------------*/
/* create struct ServiceData from given parameters*/
/*-----------------------------------------------------------------------*/
struct ServiceData * CreateServiceData(uint16_t serviceid,uint8_t cmd,uint16_t length,uint8_t data[],bool hasnext,struct ServiceData * next);


/*-----------------------------------------------------------------------*/
/* transform struct IotPacket into binary codes*/
/*-----------------------------------------------------------------------*/
/* uint8_t * encode(struct IotPacket * packet,uint16_t * total_len,int *statuscode); */

/*-----------------------------------------------------------------------*/
/* transform binary codes into struct IotPacket*/
/*-----------------------------------------------------------------------*/
/* int decode(uint8_t * bytes,uint16_t  total_len,struct IotPacket * packet); */

/*-----------------------------------------------------------------------*/
/* transform struct IotPacket into struct BinCodes*/
/*-----------------------------------------------------------------------*/
struct BinCodes * TransformIotPacketToBinCodes(struct IotPacket * packet);

/*-----------------------------------------------------------------------*/
/* transform struct Bincodes to struct IotPakcet*/
/*-----------------------------------------------------------------------*/
struct IotPacketWithStatus * TransformBinCodesToIotPacket(struct BinCodes * bin);

/*-----------------------------------------------------------------------*/
/* free struct IotPacket that is allocated on heap*/
/*-----------------------------------------------------------------------*/
void FreeIotPacketOnHeap(struct IotPacketWithStatus *packet);

/*-----------------------------------------------------------------------*/
/* free struct BinCodes that is allocated on heap*/
/*-----------------------------------------------------------------------*/
void FreeBinCodesOnHeap(struct BinCodes * bin);


/*-----------------------------------------------------------------------*/
/* free ServiceData*/
/*-----------------------------------------------------------------------*/
void FreeServiceData(struct ServiceData * data);
#endif
