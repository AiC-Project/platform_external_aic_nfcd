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

#include <sys/types.h>

#define NFC_UPDATE_PERIOD 1 /* period in sec between 2 nfc fix emission */

#define SIM_NFC_PORT 22800

// Protobuff
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <sensors_packet.pb.h>



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

#include <hal/include/nci_defs.h>

#include "userial.h"


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

void readBody(int csock,google::protobuf::uint32 siz)
{
  char c_status[255], c_latitude[255], c_longitude[255], c_altitude[255], c_bearing[255];

  int bytecount;
  sensors_packet payload;
  char buffer [siz+4];//size of the payload and hdr
  //Read the entire buffer including the hdr
  if((bytecount = recv(csock, (void *) buffer, 4+siz, MSG_WAITALL))== -1){
                fprintf(stderr, "Error receiving data %d\n", errno);
        }
  ALOGE(" readBody --  Second read byte count is %d",bytecount);
  //Assign ArrayInputStream with enough memory
  google::protobuf::io::ArrayInputStream ais( buffer,siz+4);
  CodedInputStream coded_input(&ais);
  //Read an unsigned integer with Varint encoding, truncating to 32 bits.
  coded_input.ReadVarint32(&siz);
  //After the message's length is read, PushLimit() is used to prevent the CodedInputStream
  //from reading beyond that length.Limits are used when parsing length-delimited
  //embedded messages
  google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(siz);
  //De-Serialize
  payload.ParseFromCodedStream(&coded_input);
  //Once the embedded message has been parsed, PopLimit() is called to undo the limit
  coded_input.PopLimit(msgLimit);

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


int main(int argc, char *argv[]) {
    int server = -1; int sim_server = -1;
    int client = -1; int sim_client = -1;

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

    tHAL_NFC_ENTRY *p_hal_entry_tbl;
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
//
    struct sockaddr_in local_nfc_server;
    local_nfc_server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    local_nfc_server.sin_family = AF_INET;
    local_nfc_server.sin_port = htons(UNFCD_PORT);

    if ((sim_server = start_server(22800)) == -1) {
        SLOGE(" NFC Unable to create socket\n");
        return 1;
    }

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
        return;
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
                return;
            }
        }

        if (flag){
            flag = 0;
            bytecount = send (sim_client,&cmd, sizeof(cmd), 0 ) ;
            ALOGE( "nfcd activate %x \n", cmd);
        }

        // Update NFC info every NFC_UPDATE_PERIOD seconds
        sleep(NFC_UPDATE_PERIOD);

        fd_local = socket(AF_INET, SOCK_STREAM, 0);
        for (i=0;i<30;i++) {
            sleep(1);
            if(!connect(fd_local, (struct sockaddr *)&local_nfc_server, sizeof(local_nfc_server) ) )
                break;
        }

         SLOGD("NFC - local nfc server %d" , fd_local);
         memset(inbuf, '\0', 256);

        //Peek into the socket and get the packet size
        //bytecount = send (sim_client,cmdbuf, 3, 0 ) ;
        bytecount = recv (sim_client,inbuf, 3, MSG_PEEK ) ;

        SLOGD("NFC - local nfc server recv inbuf %x%x%x" , inbuf[0], inbuf[1], inbuf[2]);
        int tt = strcmp (inbuf,"303") ;
        if (tt==0 ) {
            bytecount = recv (sim_client,inbuf, 5, MSG_WAITALL ) ;
            SLOGD("NFC - local nfc server recv inbuf - bytecount %d", bytecount);
            zSiz=(int)strtol( inbuf+3, NULL, 10 );
        }
        SLOGD("NFC - local nfc server recv inbuf tt=%d - zSiz=%d", tt, zSiz );

        SLOGD("NFC:: reveivinf data");
        bytecount = recv (sim_client,nci_data, zSiz, MSG_WAITALL ) ;

        SLOGD("NFC:: First read byte count is %d",bytecount);
        //readBody(sim_client,readHdr(inbuf) );
            {
                data_len = zSiz ;

                for( ii=0;ii<data_len; ii++)
                SLOGD("nfcd - nci_data %x",nci_data[ii]);

                int siz = write( fd_local , nci_data, zSiz );
                SLOGD("siz= %d ; go fool bufff !!!", siz);

                close(fd_local);
            }
        }
    return /*(server == -1 || client != -1)*/;
}
