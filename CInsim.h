/*
 * Copyright (c) 2013, Cristóbal Marco
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * CInsim v0.7
 * ===========
 *
 * CInsim is a LFS InSim library written in basic C/C++. It provides basic
 * functionality to interact with InSim in Windows and *NIX. It uses WinSock2 for
 * the socket communication under Windows, and pthreads-w32 for thread safe sending
 * method under Windows.
 *
 * *NIX style sockets and POSIX compliant threads are used for *NIX compatibility.
 * *NIX compatibility and additional code and corrections provided by MadCatX.
 */

#ifndef _CINSIM_H
#define _CINSIM_H


typedef unsigned char byte;
typedef unsigned short word;

// Custom InSim specific database types
typedef struct NodeLap NodeLap;
typedef struct CompCar CompCar;
typedef struct CarContact CarContact;
typedef struct CarContOBJ CarContOBJ;
typedef struct ObjectInfo ObjectInfo;

typedef struct
{
    int x;
    int y;
    int z;
} Vec;

typedef struct
{
    float x;
    float y;
    float z;
} Vector;

#include "insim.h"

/* Defines whether the Windows or Linux part of the source will be compiled.
 * Options are CIS_WINDOWS or CIS_LINUX
 */
#ifdef __linux__
#define CIS_LINUX
#elif _WIN32
#define CIS_WINDOWS
#endif

#include <stdio.h>
#include <string.h>
#include <string>
#include <cstdarg>
#include <stdexcept>

// Includes for Windows (uses winsock2)
#ifdef CIS_WINDOWS
#include <winsock2.h>
#include <mutex>

// Includes for *NIX (no winsock2, these headers are needed instead)
#elif defined CIS_LINUX
#include <mutex>
#include <limits.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fstream>
#include <unistd.h>
#endif

#define PACKET_BUFFER_SIZE 1020
#define PACKET_MAX_SIZE 1020
#define IS_TIMEOUT 5

#define IS_USE_STATIC

#define LIGHT_COLOR_RED 1
#define LIGHT_COLOR_YELLOW 2
#define LIGHT_COLOR_NONE 4
#define LIGHT_COLOR_GREEN 8

// Definition for our buffer datatype
struct packBuffer
{
	char buffer[PACKET_BUFFER_SIZE];    // Packet buffer - 512 should be more than enough
	unsigned int bytes;                 // Number of bytes currently in buffer
};

/**
* CInsim class to manage the Insim connection and processing of the packets
*/
class CInsim
{
  private:
    std::string hostname;
    word tcpPort;
    word udpPort;
    std::string product;
    std::string password;
    byte prefix;
    word flags;
    word interval;
    byte version;

    bool sendPackVer = true;
    std::string hostVersion;
    std::string hostProduct;
    byte   hostInSimVersion;



    #ifdef IS_USE_STATIC
    CInsim();
    CInsim(const std::string hostname, const word port, const std::string product, const std::string admin, byte prefix = 0, word flags = 0, word interval = 0, word udpport = 0, byte version = 8);
    ~CInsim();
    static CInsim* self;
    #endif // IS_USE_STATIC

    #ifdef CIS_WINDOWS
    SOCKET sock;                            // TCP Socket (most packets)
    SOCKET sockudp;                         // UDP Socket (if requested, for NLP and MCI)
    #elif defined CIS_LINUX
    int sock;                               // TCP Socket (most packets)
    int sockudp;                            // UDP Socket (if requested, for NLP and MCI)
    #endif
    byte using_udp;                         // 1 if we are using UDP for NLP or MCI packets
    struct packBuffer gbuf;                     // Our global buffer
    struct packBuffer lbuf;                     // Our local buffer
    char packet[PACKET_MAX_SIZE];           // A buffer where the current packet is stored
    fd_set readfd, exceptfd;                // File descriptor watches
    #ifdef CIS_WINDOWS
    struct timeval select_timeout;          // timeval struct for the select() call
    #elif defined CIS_LINUX

    struct timespec select_timeout;        // timeval struct for the pselect() call
    #endif
    struct packBuffer udp_lbuf;                 // (for NLP and MCI packets via UDP) Our local buffer (no global buffer needed for UDP)
    char udp_packet[PACKET_MAX_SIZE];       // (for NLP and MCI packets via UDP) A buffer where the current packet is stored
    fd_set udp_readfd, udp_exceptfd;        // (for NLP and MCI packets via UDP) File descriptor watches
    std::mutex *ismutex;                // Mutex var used for send_packet() method

  public:
    #ifdef IS_USE_STATIC
    static CInsim* getInstance();
    static CInsim* getInstance(const std::string hostname, const word port, const std::string product, const std::string admin, byte prefix = 0, word flags = 0, word interval = 0, word udpport = 0, byte version = 9);
    static void removeInstance();
    #else
    CInsim();
    CInsim(const std::string hostname, const word port, const std::string product, const std::string admin, byte prefix = 0, word flags = 0, word interval = 0, word udpport = 0, byte version = 8);
    ~CInsim();
    #endif // IS_USE_STATIC

    CInsim* setHost(const std::string hostname);
    CInsim* setTCPPort(const word port);
    CInsim* setUDPPort(const word port);
    CInsim* setProduct(const std::string name);
    CInsim* setPassword(const std::string password);
    CInsim* setPrefix(const byte prefix);
    CInsim* setFlags(const word flags);
    CInsim* setInterval(const word interval);
    CInsim* setVersion(const byte version);

    byte    getHostVersion();

    //  "int connect(...)" Establishes connection with the socket and insim.
    //+ The last argument ch_ver is a pointer to a IS_VER struct. If it's used an IS_VER packet
    //+ will be returned. If ch_ver is not used in the call no IS_VER will be requested/returned.
    int init();
    int disconnect();                   // Closes connection from insim and from the socket
    int next_packet();                  // Gets next packet ready into "char packet[]"
    char peek_packet();                 // Returns the type of the current packet
    void* get_packet();                 // Returns a pointer to the current packet. Must be casted
    int send_packet(void* packet);      // Sends a packet to the host
    int udp_next_packet();              // (UDP) Gets next packet ready into "char udp_packet[]"
    char udp_peek_packet();             // (UDP) Returns the type of the current packet
    void* udp_get_packet();             // (UDP) Returns a pointer to the current packet. Must be casted

    void SendMST(std::string Text);
    void SendMSX(std::string Text);
    void SendMTC(byte UCID, std::string Text, byte Sound = SND_SILENT);
    void SendBFN(byte UCID, byte ClickID);
    void SendBFN(byte UCID, byte ClickIdFrom, byte ClickIdTo);
    void SendBFNAll(byte UCID);
    void SendPLC (byte UCID, unsigned PLC);
    void SendButton(byte ReqI,byte UCID, byte ClickID,byte Left, byte Top, byte Width, byte Height,byte BStyle, std::string Text);
    void SendButton(byte ReqI,byte UCID, byte ClickID,byte Left, byte Top, byte Width, byte Height,byte BStyle, std::string Text, byte TypeIn);
    void SendTiny(byte SubT);
    void SendTiny(byte SubT, byte ReqI);
    void SendSmall(byte SubT, unsigned UVal);
    void SendSmall(byte SubT, unsigned UVal, byte ReqI);

    void LightSet(byte Id,byte Color);
    void LightReset(byte Id);
    void LightResetAll();

    void SendJRR(byte JRRAction = 0, byte UCID = 0);
    void SendJRR(byte JRRAction, byte PLID, ObjectInfo obj);

    /** @brief Reset player by PLID to current respawn point
     *
     * @param byte PLayer ID
     * @param int Raw MCI X position
     * @param int Raw MCI Y position
     * @param int Raw MCI Z position
     * @param word Raw MCI Heading
     * @param bool Reset or reset and repair
     * @return void
     *
     */
    void ResetCar(byte PLID, int X, int Y, int Z, word Heading, bool repair = true);

    std::string GetLanguageCode(byte LID);
};


/**
* Other functions!!!
*/

char* ms2str (long milisecs, char *str, int thousands=0); // Converts miliseconds to a C string (including negative miliseconds).
bool is_ascii_char(char c);  // checks if character c is 0-9 / A-Z / a-z
bool is_valid_id(unsigned id); // return 1 if the supplied id is valid as a mod's skin ID
bool is_official_prefix(const char* prefix);
char* expand_prefix(const char* prefix); // fill a static buffer "char expanded_prefix[8]" and return a pointer to it

#endif
