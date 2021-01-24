#include "network.h"

#include <bsd/socket.h>
#include <debug.h>
#include <bsd/inet.h>
#include <sys/mem.h>
#include <ssl/ssl.h>
#if defined(__USE_STD__)
# include <errno.h>
#endif
#include <konexios/credentials.h>
static int _read(Network* n, unsigned char* buffer, int len, int timeout_ms) {
    if ( len <= 0 ) return -1;
    struct timeval interval = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if ((int)interval.tv_sec < 0 || (interval.tv_sec == 0 && interval.tv_usec <= 0)) {
        interval.tv_sec = 0;
        interval.tv_usec = 100;
    }

//    setsockopt(n->my_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&interval, sizeof(struct timeval));
    if(setsockopt(n->my_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&interval, sizeof(struct timeval)) != 0){
//           DBG("error: Set Socket Option Failure");
           return -1;
    }
    int bytes = 0;
    while (bytes < len) {
        int rc = 0;
//    	if (rc) DBG("mqtt recv ---%d", timeout_ms);
        if ( konexios_mqtt_host()->scheme == konexios_mqtt_scheme_tls ) {
    	rc = ssl_recv(n->my_socket, (char*)(buffer + bytes), (uint16_t)(len - bytes));
        } else {
    	rc = recv(n->my_socket, (char*)(buffer + bytes), (uint16_t)(len - bytes), 0);
        }
//      DBG("mqtt recv %d/%d", rc, len);
        if (rc < 0) {
#if defined(errno) && defined(__linux__) && defined(MQTT_DEBUG)
            DBG("error(%d): %s", rc, strerror(errno));
#endif
            bytes = -1;
            break;
        } else if (rc == 0) {
            break;
        }else {
            bytes += rc;
        }
    }
    return bytes;
}


static int _write(Network* n, unsigned char* buffer, int len, int timeout_ms) {
    struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
#if 0
    int error = 0;
    socklen_t errlen = sizeof (error);
    if ( getsockopt (n->my_socket, SOL_SOCKET, SO_ERROR, &error, &errlen) !=0 ) return -1;
    if ( error ) {
        soc_close(n->my_socket);
        n->my_socket = -1;
        return -1;
    }
#endif

    setsockopt(n->my_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    int rc = -1;
if ( konexios_mqtt_host()->scheme == konexios_mqtt_scheme_tls ) {
//    DBG("mqtt send %d", len);
     rc = ssl_send(n->my_socket, (char*)buffer, len);
} else {
     rc = send(n->my_socket, (char*)buffer, len, 0);
}
    return rc;
}


void NetworkInit(Network* n) {
    n->my_socket = -1;
    n->mqttread = _read;
    n->mqttwrite = _write;
}

void NetworkDisconnect(Network* n) {
# if defined(MQTT_CIPHER)
    ssl_close(n->my_socket);
#endif
    soc_close(n->my_socket);
}

// FIMXE connection timeout impl
int NetworkConnect(Network* n, char* addr, int port, int timeout) {
    struct sockaddr_in serv;
    struct hostent *serv_resolve;
    struct timeval interval = {timeout / 1000, (timeout % 1000) * 1000};

    if (!addr) {
    	DBG("addr NULL");
    	return -1;
    }

    serv_resolve = gethostbyname(addr);
    if (serv_resolve == NULL) {
        DBG("MQTT ERROR: no such host %s", addr);
        return -1;
    }
    memset(&serv, 0, sizeof(serv));

    if (serv_resolve->h_addrtype == AF_INET) {
        serv.sin_family = PF_INET;
        memcpy((char *)&serv.sin_addr.s_addr,
        		(char *)serv_resolve->h_addr,
                (size_t)serv_resolve->h_length);
        serv.sin_port = htons((uint16_t)port);
    } else
        return -1;

    n->my_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( n->my_socket < 0 ) {
        DBG("MQTT connetion fail %d", n->my_socket);
        return n->my_socket;
    }

    setsockopt(n->my_socket, SOL_SOCKET, SO_RCVTIMEO,
               (char *)&interval, sizeof(struct timeval));

    if ( connect(n->my_socket, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        soc_close(n->my_socket);
        return -2;
    }
if ( konexios_mqtt_host()->scheme == konexios_mqtt_scheme_tls ) {
    if ( ssl_connect(n->my_socket) < 0 ) {
    	soc_close(n->my_socket);
    	return -3;
    }
}
    if (socket_connect_done(n->my_socket) < 0 ) {
        NetworkDisconnect(n);
        return -4;
    }
    return n->my_socket;
}
