#define LOG_TAG "local_nfc"
#include <cutils/properties.h>
#include <cutils/log.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#define NO_PROTOBUF
#include "androidincloud.hpp"
//#include "aic.h"

#include <sys/types.h>

#define NFC_UPDATE_PERIOD 1 /* period in sec between 2 nfc fix emission */

#define SIM_NFC_PORT 22800

// Protobuff
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <nfc.pb.h>

/////NFC///////
#include <hardware/hardware.h>
#include <hardware/nfc.h>
#include <nfc/include/nfc_api.h>


#include <string.h>
#include <gki/common/gki.h>
#include <gki/ulinux/gki_int.h>
#include <include/nfc_target.h>
#include <include/bt_types.h>


#include <nfc/int/nfc_int.h>
#include <nfc/include/nci_hmsgs.h>
#include <nfa/include/nfa_rw_api.h>

#include <nfc/include/ndef_utils.h>

#include "userial.h"

#define NFCLOG(param, ...) {SLOGD("%s-"param, __FUNCTION__, ## __VA_ARGS__);}

#define KEY_PREFIX "aicd"
#define PROP_NFCD KEY_PREFIX".nfcd"

#define PROP_ACT PROP_NFCD".action" //"cmd:name@addr#devclass"

#define PROP_TYP PROP_NFCD".type"
#define PROP_LAN PROP_NFCD".lang"
#define PROP_TEX PROP_NFCD".text"
#define PROP_TIT PROP_NFCD".tittle"


void setBtdPropInit( const char *type, const char *lang, const char *text, const char *tittle){
    property_set(PROP_TYP, type);
    property_set(PROP_LAN, lang);
    property_set(PROP_TEX, text);
    property_set(PROP_TIT, tittle);
}

void setBtdProp(char *type, char *lang, char *text, char *tittle){

    if (strcmp(PROP_TYP, type) ){
        property_set(PROP_TYP, type);
    }

    if (strcmp(PROP_LAN, lang) ){
        property_set(PROP_LAN, lang);
    }

    if (strcmp(PROP_TEX, text) ){
        property_set(PROP_TEX, text);
    }

    if (strcmp(PROP_TIT, tittle) ){
        property_set(PROP_TIT, tittle);
    }
    property_get(PROP_TYP, type, "1");
    property_get(PROP_LAN, lang, "1");
    property_get(PROP_TEX, text, "google.fr");
    property_get(PROP_TIT, tittle, "default_tittle");

    NFCLOG("%s  -  %s - %s - %s", PROP_TYP ,PROP_LAN, PROP_TEX ,PROP_TIT);
}

void splitAction( char *action, char *type, char *lang, char *text, char *tittle){

    char strtemp[512] ;//= "0:0@google.fr#default_tittle";
    const char ok1[2] = ":";
    const char ok2[2] = "@";
    const char ok3[2] = "#";

    char *token;
    strcpy(strtemp, action);
    token = strtok(action, ok1);strcpy(type ,token);
    token = strtok(NULL, ok2  );strcpy(lang,token);
    token = strtok(NULL, ok3  );strcpy(text,token);
    token = strtok(NULL, ok3  );strcpy(tittle,token);

    strcpy(action, strtemp);
    NFCLOG("%s  -  %s - %s - %s", type, lang, text, tittle);
}

void createBufNdef_TypeURI(UINT8 *strIN, int sizLen, UINT8 *strOUT){

//  0  ,  1 , 2  ,  3 |,|  4 ,  5 ,  6 ,  7 ,  8 ,  9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 , 17 , 18 ,  19};
//{0xD1,0x01,0x10,0x55|,| 0x03,0x62,0x6c,0x6f,0x67,0x2e,0x7a,0x65,0x6e,0x69,0x6b,0x61,0x2e,0x63,0x6f,0x6d};
//     ,    ,    ,    |,|    ,  b ,  l ,  o ,  g ,  . ,  z ,  e ,  n ,  i ,  k ,  a ,  . ,  c ,  o ,  m };
// D1 : MB=1, ME=1, CR=0, SR=1, IL=0, TNF=001
// 01 : type sur 1 octet
// 10 : payload sur 16 octets
// 55 : type U (URI)
// 03 : préfixe http://
// 626C6F672E7A656E696B612E636F6D : blog.zenika.com

         int offset = 1 ; // for prefix type
         UINT8   rec_hdr = 0x00;

         rec_hdr |= NDEF_TNF_WKT;
         rec_hdr |=  NDEF_ME_MASK;
         rec_hdr |=  NDEF_MB_MASK;
         rec_hdr |=  NDEF_SR_MASK;

         strOUT[0] = rec_hdr ;
         strOUT[1] =  0x01;
         strOUT[2] =  sizLen + offset ;
         strOUT[3] =  0x55;

         strOUT[4] =  0x03;
         strncat((char*)strOUT, (const char *)strIN, sizLen);

         return;
}



void createBufNdef_TypeText(UINT8 *strIN, int sizLen, UINT8 *strOUT){

         int sizehdr = 4;

         int offset = 6 ;

         int count =0;
         UINT8 p_new_hdr[sizehdr];
         UINT8   rec_hdr ;

         rec_hdr = 0x00;

         rec_hdr |= NDEF_TNF_WKT;
         rec_hdr |=  NDEF_ME_MASK;
         rec_hdr |=  NDEF_MB_MASK;
         rec_hdr |=  NDEF_SR_MASK;

         strOUT[0] = rec_hdr ;
         strOUT[1] =  0x01;
         strOUT[2] =  sizLen + offset ;
         strOUT[3] =  0x54; // T

         strOUT[4] =  0x05; // x
         strOUT[5] =  0x65; // e
         strOUT[6] =  0x6E; // n
         strOUT[7] =  0x2D; // n
         strOUT[8] =  0x55; // U
         strOUT[9] =  0x53; // S

         strncat((char*)strOUT, (const char *)strIN, sizLen);

         return;
}

void createBufNdef_TypeSmartPoster(UINT8 *strIN, UINT8 *strIN2, int sizLen, UINT8 *strOUT){

         int sizehdr = 4;
         int offset = 6 ;

         int count =0;
         UINT8 p_new_hdr[sizehdr];
         UINT8   rec_hdr ;
         rec_hdr = 0x00;

         rec_hdr |= NDEF_TNF_WKT;
         rec_hdr |=  NDEF_ME_MASK;
         rec_hdr |=  NDEF_MB_MASK;
         rec_hdr |=  NDEF_SR_MASK;


         strOUT[0] = rec_hdr ;
         strOUT[1] =  0x02; // tnf 001
         strOUT[2] =  sizLen + offset + 10;
         strOUT[3] =  0x53; // S

         strOUT[4] =  0x70; // p
         strOUT[5] =  0x91; // x
         strOUT[6] =  0x01; // x
         strOUT[7] =  0x10; // x
         strOUT[8] =  0x55; // U
         strOUT[9] =  0x03; // x


         strncat((char*)strOUT, (const char *)strIN, sizLen);

        strOUT[sizehdr+offset+sizLen +0] =  0x51;
        strOUT[sizehdr+offset+sizLen +1] =  0x01;
        strOUT[sizehdr+offset+sizLen +2] =  0x07;
        strOUT[sizehdr+offset+sizLen +3] =  0x54;
        strOUT[sizehdr+offset+sizLen +4] =  0x02;
        strOUT[sizehdr+offset+sizLen +5] =  0x66;
        strOUT[sizehdr+offset+sizLen +6] =  0x72;
        strOUT[sizehdr+offset+sizLen +7] =  0x42;
        strOUT[sizehdr+offset+sizLen +8] =  0x6C;
        strOUT[sizehdr+offset+sizLen +9] =  0x6F;
        strOUT[sizehdr+offset+sizLen +10]=  0x67;

     return;
}


void vshort_actidata(UINT8 *strIN, int sizLen, UINT8 *strOUT)
{
    int ii;
    int offset =3;

    for(ii=0;ii<=sizLen+offset; ii++)
        strOUT[ii+offset]=strIN[ii];


    strOUT[0]=0x00;strOUT[1]=0x00;strOUT[2]=0x00;

    #define NCI_MSG_RF_INTF_ACTIVATED       5

     NCI_MSG_BLD_HDR0 (strOUT, NCI_MT_NTF, NCI_GID_RF_MANAGE);
     NCI_MSG_BLD_HDR1 (strOUT, NCI_MSG_RF_INTF_ACTIVATED);

}

void vshort_sendata(UINT8 *strIN, int sizLen, UINT8 *strOUT)
{
    int ii;
    int offset =3;

    for(ii=0;ii<=sizLen+offset; ii++)
        strOUT[ii+offset]=strIN[ii];

    strOUT[0]=0x00;strOUT[1]=0x00;strOUT[2]=0x00;

     NCI_DATA_BLD_HDR(strOUT, 0, sizLen) ;

}

int codeNFC( nfcPayload * nfcData , UINT8 *msg)
{
    google::protobuf::uint32 Type= nfcData->type();
    google::protobuf::uint32 Lang= nfcData->lang();

    int argLen  = strlen (nfcData->text().c_str());
    int offsetPrefix = 1;
    int sizehdr = 4 ; //header + Prefix
    int msg_len = 0;

    SLOGE("codeNFC - %d %d %s %s",
        nfcData->type(), nfcData->lang(), nfcData->text().c_str(), nfcData->tittle().c_str());

    int flag=1;
    switch (Type) {
        case 0 :
            sizehdr += offsetPrefix;
            msg_len = (argLen + sizehdr )  ;
            //msg = (UINT8*)calloc(msg_len, sizeof(UINT8));
             createBufNdef_TypeURI((unsigned char *)nfcData->text().c_str(),  argLen, msg);
            break;

        case 1 : // Type Text need calculate HeaderSize
            offsetPrefix = 6;
            if(Lang==0){
                //sizehdr += 6 ;
                sizehdr += offsetPrefix;
                msg_len = (argLen + sizehdr)   ;
                //msg = (UINT8*)calloc(msg_len, sizeof(UINT8));
                createBufNdef_TypeText((unsigned char *)nfcData->text().c_str(),  argLen, msg);
            }else if(Lang==1){
                //sizehdr += 3 ;
                sizehdr += offsetPrefix;
                msg_len = (argLen + sizehdr)  ;
                //msg = (UINT8*)calloc(msg_len, sizeof(UINT8));
                createBufNdef_TypeText((unsigned char *)nfcData->text().c_str(),  argLen, msg);
            }
            break;

        case 2 :
            offsetPrefix = 6;
             sizehdr += offsetPrefix;
             msg_len = (argLen + sizehdr) + 11 ;
             //msg = (UINT8*)calloc(msg_len, sizeof(UINT8));
             createBufNdef_TypeSmartPoster((unsigned char *)nfcData->text().c_str(),  (unsigned char *)nfcData->tittle().c_str(), argLen, msg);
             break;
        case 3 :
             sizehdr = 2;
             msg_len = (argLen + sizehdr )  ;
             msg = (UINT8*)calloc(msg_len, sizeof(UINT8));
             //vshort_sendata(argbuff,  argLen, msg);

        default :
            break;
    }
    return msg_len;
}


using namespace google::protobuf::io;

google::protobuf::uint32 readHdr(char *buf)
{
  google::protobuf::uint32 size;
  google::protobuf::io::ArrayInputStream ais(buf,4);
  CodedInputStream coded_input(&ais);
  coded_input.ReadVarint32(&size);//Decode the HDR and get the size
  ALOGE(" readHdr --   size of payload is %d", size);
  return size;
}

void readBody(int csock,google::protobuf::uint32 siz, int * msg_len , char* msg)
{

    int bytecount;
    nfcPayload  payload;
    char* buffer = (char*) calloc(siz+4, sizeof(char));//size of the payload and hdr
    //Read the entire buffer including the hdr
    if((bytecount = recv(csock, (void *)buffer, 4+siz, MSG_WAITALL))== -1)
    {
        ALOGE(" readBody: Error receiving data (%d)", errno);
        free((void*)buffer);
        return;
    }
    else if (bytecount != siz+4)
    {
        ALOGE(" readBody: Received the wrong payload size (expected %d, received %d)", siz+4, bytecount);
        free((void*)buffer);
        return;
    }
    ALOGE(" readBody --  Second read byte count is %d", bytecount);
    //Assign ArrayInputStream with enough memory
    google::protobuf::io::ArrayInputStream ais(buffer, siz+4);

        ALOGE(" readBody --  Assign ArrayInputStream with enough memory");


    CodedInputStream coded_input(&ais);
    //Read an unsigned integer with Varint encoding, truncating to 32 bits.
    ALOGE(" readBody --  Read an unsigned integer with Varint encoding, truncating to 32 bits.");
    coded_input.ReadVarint32(&siz);
    //After the message's length is read, PushLimit() is used to prevent the CodedInputStream
    //from reading beyond that length.Limits are used when parsing length-delimited
    //embedded messages
    ALOGE("After the message's length is read, PushLimit()");
    google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(siz);
    //De-Serialize
        ALOGE(" readBody --  De-Serialize");
    payload.ParseFromCodedStream(&coded_input);
    //Once the embedded message has been parsed, PopLimit() is called to undo the limit
    ALOGE(" readBody -- Once the embedded message has been parsed, PopLimit() ");
    coded_input.PopLimit(msgLimit);
    //Print the message

    // Code the message
    *msg_len =  codeNFC( &payload , msg);
    ALOGE(" readBody --  Print the message %d - %d ", payload.has_type(), *msg_len);

  return ;
}


static int start_server(uint16_t port) {
    int server = -1;
    struct sockaddr_in srv_addr;
    long haddr;

    bzero(&srv_addr, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(port);

    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        SLOGE(" NFC Unable to create socket\n");
        return -1;
    }

    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(server, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        SLOGE(" NFC Unable to bind socket, errno=%d\n", errno);
        return -1;
    }

    return server;
}

static int wait_for_client(int server) {
    int client = -1;

    if (listen(server, 1) < 0) {
        SLOGE("Unable to listen to socket, errno=%d\n", errno);
        return -1;
    }

    client = accept(server, NULL, 0);

    if (client < 0) {
        SLOGE("Unable to accept socket for main conection, errno=%d\n", errno);
        return -1;
    }

    return client;
}


int tcp_write_buff(int fd_local, unsigned char *data, int len)
{
    struct sockaddr_in local_nfc_server;
    local_nfc_server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    local_nfc_server.sin_family = AF_INET;
    local_nfc_server.sin_port = htons(UNFCD_PORT);

    fd_local = socket(AF_INET, SOCK_STREAM, 0);
    for (int i=0;i<30;i++) {
        sleep(1);
        if(!connect(fd_local, (struct sockaddr *)&local_nfc_server, sizeof(local_nfc_server) ) )
            break;
    }

    int err = send(fd_local, data, len, 0);

    close(fd_local);

    return (err);
}






void *hdl_player_conn(void * args){
    NFCLOG("");

    int *sim_server = (int*)args;
    int sim_client = -1;

    char inbuf[256];
    int16_t cmd;
    int bytecount=0;

    SLOGE(" NFC  A \n");

    int16_t offset=3; int zSiz;
    uint8_t nci_hdr[]={0x0,0x0,0x0};
    int16_t data_len=46;
    uint8_t *nci_data =  (UINT8*)calloc(data_len, sizeof(UINT8));
    uint8_t *nci_data_old =  (UINT8*)calloc(data_len, sizeof(UINT8));
    int ii;

    tHAL_NFC_ENTRY *p_hal_entry_tbl = NULL;
    NFC_Init (p_hal_entry_tbl);
    BT_HDR  *p_data;
    int i, fd_cmd, fd_local = 0;

    SLOGE(" NFC  B \n");

    SLOGE(" NFC  C \n");
    tNFC_STATUS test_status;

    struct sockaddr_in local_nfc_cmd;
    local_nfc_cmd.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    local_nfc_cmd.sin_family = AF_INET;
    local_nfc_cmd.sin_port = htons(UNFCD_PORT + 1 );

    int flag = 1 ;

    fd_cmd = socket(AF_INET, SOCK_STREAM, 0);

    for (;;) {
        sleep(1);
        if(!connect(fd_cmd, (struct sockaddr *)&local_nfc_cmd, sizeof(local_nfc_cmd) ) )
            break;
    }

    ALOGE( "nfcd waiting activation \n");
    recv(fd_cmd, &cmd , sizeof(cmd), MSG_WAITALL);
    ALOGE( "nfcd received activate %d \n", cmd);

    if (cmd!=1001 && cmd!=1002) {
        printf("Unknown cmd : %d, exit\n", cmd);
        return -1;
    }

    // Listen for main connection
    while ((sim_client = wait_for_client(sim_server)) != -1) {

        {
            int16_t cmd;
            recv(fd_cmd, &cmd, sizeof(cmd), MSG_DONTWAIT );
            printf ("\n received %d  \n", cmd);
            if (cmd==1002) {
                bytecount = send (sim_client,&cmd, sizeof(cmd), 0 ) ;
                ALOGE( "nfcd nfc poweroff %x \n", cmd);
                return -1;
            }
        }

        // Update NFC info every NFC_UPDATE_PERIOD seconds
        sleep(NFC_UPDATE_PERIOD);

        memset(inbuf, '\0', 4);

        //Peek into the socket and get the packet size
        if((bytecount = recv(sim_client, inbuf, 4, MSG_PEEK)) == -1)
        {
            SLOGE("nfcd::  Error receiving data ");
            return 0;
        }
        else if (bytecount == 0)
            return 0;

        SLOGD("nfcd:: First read byte count is %d",bytecount);

        UINT8 * msg = (UINT8*)calloc(1024, sizeof(UINT8));
        UINT8 *msg2 = (UINT8*)calloc(1024, sizeof(UINT8));
        int  msg_len ;
        nfcPayload  nfcData ;

        readBody(sim_client,readHdr(inbuf),&msg_len, msg);
        SLOGD("NFC:: reveivinf end data  %d" , msg_len);

        vshort_sendata(msg,  msg_len, msg2);
        int siz = tcp_write_buff( fd_local , msg2, msg_len +3 );
        SLOGD("siz= %d ; go fool bufff !!!", siz);

        vshort_actidata(msg,  msg_len, msg2);
        siz = tcp_write_buff( fd_local , msg2, msg_len + 3 );
        SLOGD("siz= %d ; go fool bufff !!!", siz);

    }

    return NULL;
}

void *hdl_new_getprop()
{
    char action[512] = {'\0'};
    char action_old[512] = {'\0'};

    int cmd = -1;
    int tmp=-1, i=-1;

    char bd_name[512] ;
    char bd_addr[512] ;
    char bd_class[512] ;
    uint8_t len = 0;
    uint8_t typ;
    int fd_local = 0;

    void *buf = (void *) malloc (512 * sizeof(uint8_t));
    unsigned char msg2[512] = {1};

    char *type,  *lang,  *text,  *tittle;

    type=(char*)malloc(512*sizeof(uint8_t));
    text=(char*)malloc(512*sizeof(uint8_t));
    lang=(char*)malloc(512*sizeof(uint8_t));
    tittle=(char*)malloc(512*sizeof(uint8_t));

    setBtdPropInit("1", "0F0F", "AIC", "640");
        strcpy(action_old, "1:1@aic#incloud");
    while(1){

        property_get(PROP_ACT, action, "1:1@aic#incloud");
        splitAction( action, type, lang, text, tittle);
        setBtdProp( type, lang, text, tittle);

        //cmd=atoi(s_cmd);

        sleep(1);
        NFCLOG(":: Waiting action=%s action_old=%s", action, action_old);
        NFCLOG(":: Waiting :: %s %s %s %s", type, lang, text, tittle);

        if ( strcmp(action, action_old) )
        {  // Detect prop changing
            strcpy(action_old, action);

            UINT8 * msg = (UINT8*)calloc(1024, sizeof(UINT8));
            UINT8 *msg2 = (UINT8*)calloc(1024, sizeof(UINT8));
            int  msg_len ;
            nfcPayload * nfcData = new nfcPayload;

            nfcData->set_type((google::protobuf::uint32 ) atoi(type) );
            nfcData->set_lang((google::protobuf::uint32 ) atoi(lang) );
            nfcData->set_text(text);
            nfcData->set_tittle(tittle);

            msg_len =  codeNFC( nfcData , msg);
            SLOGD("NFC:: reveivinf end data  %d" , msg_len);

            vshort_sendata(msg,  msg_len, msg2);
            int siz = tcp_write_buff( fd_local , msg2, msg_len +3 );
            SLOGD("siz= %d ; go fool bufff !!!", siz);

            vshort_actidata(msg,  msg_len, msg2);
            siz = tcp_write_buff( fd_local , msg2, msg_len + 3 );
            SLOGD("siz= %d ; go fool bufff !!!", siz);

        }// end if cmd
    }
    return NULL;
}//end hdl_new_getprop

int main(int argc, char *argv[]) {
    int sim_server = -1;

    NFCLOG("\n:::::::::::::::::::::::::::::::::::::::::::::::::::");
    NFCLOG(":: Btd app starting");

    if ((sim_server = start_server(22800)) == -1)
    {
        NFCLOG(" BT Unable to create socket\n");
        return -1;
    }
    pthread_t *thread0;
    pthread_t *thread1;
    thread0 = (pthread_t *)malloc(sizeof(pthread_t));
    thread1 = (pthread_t *)malloc(sizeof(pthread_t));

    NFCLOG(":: Btd app terminating");

    int s  = pthread_create(thread0, NULL, &hdl_player_conn, (void*)&sim_server);
    int s1 = pthread_create(thread1, NULL, &hdl_new_getprop, NULL);

    pthread_join(*thread0, NULL);

    NFCLOG(":: Btd app terminating");

    return 0;
}

