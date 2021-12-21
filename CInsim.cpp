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

#include <CInsim.h>

#ifdef CIS_LINUX
#define INVALID_SOCKET -1
#endif

#define IS_BTN_HDRSIZE 12
#define IS_BTN_MAXTLEN 239
#define IS_MTC_HDRSIZE 8
#define IS_MTC_MAXTLEN 127

#ifdef IS_USE_STATIC
CInsim*
CInsim::self = nullptr;

CInsim*
CInsim::getInstance()
{
    if(!self)
        self = new CInsim();

    return self;
}

CInsim*
CInsim::getInstance(const std::string hostname, const word port, const std::string name, const std::string password, byte prefix, word flags, word interval, word udpport, byte version)
{
    if(!self)
        self = new CInsim(hostname, port, name, password, prefix, flags, interval, udpport, version);

    return self;
}

void
CInsim::removeInstance()
{
    if(self)
        delete self;
}
#endif // IS_USE_STATIC

/**
* Constructor: Initialize the buffers
*/
CInsim::CInsim ()
{
    // Initialize the mutex var
    ismutex = new std::mutex();

    // Initialize global buffers
    memset(gbuf.buffer, 0, PACKET_BUFFER_SIZE);
    gbuf.bytes = 0;

    // Initialize local buffers
    memset(lbuf.buffer, 0, PACKET_BUFFER_SIZE);
    memset(udp_lbuf.buffer, 0, PACKET_BUFFER_SIZE);
    lbuf.bytes = 0;
    udp_lbuf.bytes = 0;

    // Initialize packet buffers
    memset(packet, 0, PACKET_MAX_SIZE);
    memset(udp_packet, 0, PACKET_MAX_SIZE);

    // By default we're not using UDP
    using_udp = 0;
}

CInsim::CInsim(const std::string hostname, const word port, const std::string name, const std::string password, byte prefix, word flags, word interval, word udpport, byte version)
{
    // Initialize the mutex var
    ismutex = new std::mutex();

    // Initialize global buffers
    memset(gbuf.buffer, 0, PACKET_BUFFER_SIZE);
    gbuf.bytes = 0;

    // Initialize local buffers
    memset(lbuf.buffer, 0, PACKET_BUFFER_SIZE);
    memset(udp_lbuf.buffer, 0, PACKET_BUFFER_SIZE);
    lbuf.bytes = 0;
    udp_lbuf.bytes = 0;

    // Initialize packet buffers
    memset(packet, 0, PACKET_MAX_SIZE);
    memset(udp_packet, 0, PACKET_MAX_SIZE);

    // By default we're not using UDP
    using_udp = 0;

     this->hostname = hostname;
     this->tcpPort = port;
     this->udpPort = udpport;

     if(name.length() > 16) {
        throw new std::logic_error("InSim name must be less or equal than 16 chars");
    }
    this->product = name;

    if(password.length() > 16) {
        throw new std::logic_error("InSim password must be less or equal than 16 chars");
    }
    this->password = password;
    this->prefix = prefix;
    this->flags = flags;
    this->interval = interval;
    this->version = version;
}


/**
* Destructor: Initialize the buffers
*/
CInsim::~CInsim ()
{
    // Destroy the mutex var
    delete ismutex;
}

CInsim* CInsim::setHost(const std::string hostname)
{
    this->hostname = hostname;
    return this;
}

CInsim* CInsim::setTCPPort(const word port)
{
    this->tcpPort = port;
    return this;
}

CInsim* CInsim::setUDPPort(const word port)
{
    this->udpPort = port;
    return this;
}

CInsim* CInsim::setProduct(const std::string name)
{
    if(name.length() > 16) {
        throw new std::logic_error("InSim name must be less or equal than 16 chars");
    }
    this->product = name;
    return this;
}

CInsim* CInsim::setPassword(const std::string password)
{
    if(password.length() > 16) {
        throw new std::logic_error("InSim password must be less or equal than 16 chars");
    }
    this->password = password;
    return this;
}

CInsim* CInsim::setPrefix(const byte prefix)
{
    this->prefix = prefix;
    return this;
}

CInsim* CInsim::setFlags(const word flags)
{
    this->flags = flags;
    return this;
}

CInsim* CInsim::setInterval(const word interval)
{
    this->interval = interval;
    return this;
}

CInsim* CInsim::setVersion(const byte version)
{
    this->version = version;
    return this;
}


byte CInsim::getHostVersion()
{
    return this->hostInSimVersion;
}


/**
* Initialize the socket and the Insim connection
* If "struct IS_VER *pack_ver" is set it will contain an IS_VER packet after returning. It's an optional argument
*/
int CInsim::init()
{
    // Initialise WinSock
    // Only required on Windows
    #ifdef CIS_WINDOWS
    WSADATA wsadata;
    if (WSAStartup(0x202, &wsadata) == SOCKET_ERROR) {
      WSACleanup();
      return -1;
    }
    #endif

    // Create the TCP socket - this defines the type of socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    // Could we get the socket handle? If not the OS might be too busy or has run out of available socket descriptors
    if (sock == INVALID_SOCKET) {
      #ifdef CIS_WINDOWS
      closesocket(sock);
      WSACleanup();
      #elif defined CIS_LINUX
      close(sock);
      #endif

      return -1;
    }

    // Resolve the IP address
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));

    saddr.sin_family = AF_INET;

    struct hostent *hp;
    hp = gethostbyname(this->hostname.c_str());

    if (hp != NULL)
      saddr.sin_addr.s_addr = *((unsigned long*)hp->h_addr);
    else
      saddr.sin_addr.s_addr = inet_addr(this->hostname.c_str());

    // Set the port number in the socket structure - we convert it from host unsigned char order, to network
    saddr.sin_port = htons(this->tcpPort);

    // Now the socket address structure is full, lets try to connect
    if (connect(sock, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
      #ifdef CIS_WINDOWS
      closesocket(sock);
      WSACleanup();
      #elif defined CIS_LINUX
      close(sock);
      #endif

      return -1;
    }

	// If the user asked for NLP or MCI packets and defined an udpport
	if (this->udpPort > 0) {
        // Create the UDP socket - this defines the type of socket
        sockudp = socket(AF_INET, SOCK_DGRAM, 0);

        // Could we get the socket handle? If not the OS might be too busy or have run out of available socket descriptors
        if (sockudp == INVALID_SOCKET) {
            #ifdef CIS_WINDOWS
            closesocket(sock);
            closesocket(sockudp);
            WSACleanup();
            #elif defined CIS_LINUX
            close(sock);
            close(sockudp);
            #endif
            return -1;
        }

        // Resolve the IP address
        struct sockaddr_in udp_saddr, my_addr;
        memset(&udp_saddr, 0, sizeof(udp_saddr));
        memset(&my_addr, 0, sizeof(my_addr));

        // Bind the UDP socket to my specified udpport and address
        my_addr.sin_family = AF_INET;         // host unsigned char order
        my_addr.sin_port = htons(this->udpPort);     // short, network unsigned char order
        my_addr.sin_addr.s_addr = INADDR_ANY;
        memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

        // don't forget your error checking for bind():
        bind(sockudp, (struct sockaddr *)&my_addr, sizeof my_addr);

        // Set the server address and the connect to it
        udp_saddr.sin_family = AF_INET;


        if (hp != NULL)
            udp_saddr.sin_addr.s_addr = *((unsigned long*)hp->h_addr);
        else
            udp_saddr.sin_addr.s_addr = inet_addr(this->hostname.c_str());

        // Set the UDP port number in the UDP socket structure - we convert it from host unsigned char order, to network
        udp_saddr.sin_port = htons(this->udpPort);

        // Connect the UDP using the same address as in the TCP socket
        if (connect(sockudp, (struct sockaddr *) &udp_saddr, sizeof(udp_saddr)) < 0) {
            #ifdef CIS_WINDOWS
            closesocket(sock);
            closesocket(sockudp);
            WSACleanup();
            #elif defined CIS_LINUX
            close(sock);
            close(sockudp);
            #endif
            return -1;
        }

	    // We are using UDP!
	    using_udp = 1;
	}

    // Ok, so we're connected. First we need to let LFS know we're here by sending the IS_ISI packet
	struct IS_ISI isi_p;
	memset(&isi_p, 0, sizeof(struct IS_ISI));
	isi_p.Size = sizeof(struct IS_ISI);
	isi_p.Type = ISP_ISI;
	if (this->sendPackVer) {
        isi_p.ReqI = 1;
    }
	isi_p.Prefix = this->prefix;
	isi_p.UDPPort = this->udpPort;
	isi_p.Flags = this->flags;
	isi_p.InSimVer = this->version;
	isi_p.Interval = this->interval;
	memcpy(isi_p.IName, this->product.c_str(), sizeof(isi_p.IName)-1);
	memcpy(isi_p.Admin, this->password.c_str(), 16);

    // Send the initialization packet
    if(send_packet(&isi_p) < 0) {
        if (using_udp) {
            #ifdef CIS_WINDOWS
            closesocket(sockudp);
            #elif defined CIS_LINUX
            close(sockudp);
            #endif
	}

        #ifdef CIS_WINDOWS
        closesocket(sock);
        WSACleanup();
        #elif defined CIS_LINUX
        close(sock);
        #endif
        return -1;
    }

    // Set the timeout period
    select_timeout.tv_sec = IS_TIMEOUT;
    #ifdef CIS_WINDOWS
    select_timeout.tv_usec = 0;
    #elif defined CIS_LINUX
    select_timeout.tv_nsec = 0;
    #endif

    // If an IS_VER packet was requested
    if (this->sendPackVer)
    {
        if (next_packet() < 0) {             // Get next packet, supposed to be an IS_VER
            if (disconnect() < 0) {
                if (using_udp) {
                    #ifdef CIS_WINDOWS
                    closesocket(sockudp);
                    #elif defined CIS_LINUX
                    close(sockudp);
                    #endif
                }

                #ifdef CIS_WINDOWS
                closesocket(sock);
                WSACleanup();
                #elif defined CIS_LINUX
                close(sock);
                #endif
                return -1;
            }
            return -1;
        }

        switch (peek_packet())              // Check if the packet returned was an IS_VER
        {
            case ISP_VER:                    // It was, get it!
                IS_VER packVer;
                memcpy(&packVer, (struct IS_VER*)get_packet(), sizeof(struct IS_VER));
                this->hostProduct = packVer.Product;
                this->hostVersion = packVer.Version;
                this->hostInSimVersion = packVer.InSimVer;
                break;
            default:                          // It wasn't, something went wrong. Quit
                if (disconnect() < 0) {
                    if (using_udp) {
                        #ifdef CIS_WINDOWS
                        closesocket(sockudp);
                        #elif defined CIS_LINUX
                        close(sockudp);
                        #endif
                    }

                    #ifdef CIS_WINDOWS
                    closesocket(sock);
                    WSACleanup();
                    #elif defined CIS_LINUX
                    close(sock);
                    #endif
                }
                return -1;
        }
    }
	return 0;
}

/**
* Close connection to InSim
*/
int CInsim::disconnect()
{
    struct IS_TINY cl_packet;
    cl_packet.Size = 4;
    cl_packet.Type = ISP_TINY;
    cl_packet.ReqI = 0;
    cl_packet.SubT = TINY_CLOSE;

    if (send_packet(&cl_packet) < 0)
        return -1;

    if (using_udp) {
        #ifdef CIS_WINDOWS
        closesocket(sockudp);
        #elif defined CIS_LINUX
        close(sockudp);
        #endif
    }

    #ifdef CIS_WINDOWS
    closesocket(sock);
    WSACleanup();
    #elif defined CIS_LINUX
    close(sock);
    #endif
    return 0;
}

/**
* Get next packet ready
* This function also keeps the connection alive as long as you keep calling it
*/
int CInsim::next_packet()
{
    unsigned short oldp_size, p_size;
    bool alive = true;

    while (alive)                                               // Keep the connection alive!
    {
        alive = false;
        oldp_size = (unsigned char)*lbuf.buffer;

        if (this->version > 8) {
            oldp_size *= 4;
        }

        if ((lbuf.bytes > 0) && (lbuf.bytes >= oldp_size)) {        // There's an old packet in the local buffer, skip it
            // Copy the leftovers from local buffer to global buffer
            memcpy(gbuf.buffer, lbuf.buffer+oldp_size, lbuf.bytes-oldp_size);
            gbuf.bytes = lbuf.bytes - oldp_size;

            // Copy from global buffer back to the beginning of local buffer
            memset(lbuf.buffer, 0, PACKET_BUFFER_SIZE);
            memcpy(lbuf.buffer, gbuf.buffer, gbuf.bytes);
            lbuf.bytes = gbuf.bytes;
        }

        p_size = (unsigned char)*lbuf.buffer;

        if (this->version > 8) {
            p_size *= 4;
        }

        while ((lbuf.bytes < p_size) || (lbuf.bytes < 1))       // Read until we have a full packet
        {
            // Clear them
            FD_ZERO(&readfd);
            FD_ZERO(&exceptfd);

            // Set them to watch our socket for data to read and exceptions that maybe thrown
            FD_SET(sock, &readfd);
            FD_SET(sock, &exceptfd);

            #ifdef CIS_WINDOWS
            int rc = select(0, &readfd, NULL, &exceptfd, &select_timeout);
            #elif defined CIS_LINUX
            int rc = pselect(sock + 1, &readfd, NULL, &exceptfd, &select_timeout, NULL);
            #endif

            if (rc == 0)                    // Timeout
                continue;

            if (rc < 0)                     // An error occured
                return -1;

            if (FD_ISSET(sock, &exceptfd))    // An exception occured - we want to quit
                return -1;
            else
            {  // We got data!
                // Recieve any waiting bytes
                int retval = recv(sock, lbuf.buffer + lbuf.bytes, PACKET_BUFFER_SIZE - lbuf.bytes, 0);

                // Deal with the results
                if (retval == 0)                // Connection has been closed at the other end
                    return -2;

                if (retval < 0)                 // An error ocurred
                    return -1;

                p_size = *lbuf.buffer;
                if (this->version > 8) {
                    p_size *= 4;
                }
                lbuf.bytes += retval;
            }
        }

        memcpy(packet, lbuf.buffer, p_size);

        if ((peek_packet() == ISP_TINY) && (*(packet+3) == TINY_NONE)) {
            alive = true;

            struct IS_TINY keepalive;
            keepalive.Size = sizeof(struct IS_TINY);
            keepalive.Type = ISP_TINY;
            keepalive.ReqI = 0;
            keepalive.SubT = TINY_NONE;

            // Send it back
            if (send_packet(&keepalive) < 0)
                return -1;
        }
    }

    return 0;
}

/**
* Return the type of the next packet
*/
char CInsim::peek_packet()
{
    return *(packet+1);
}

/**
* Return the contents of the next packet
*/
void* CInsim::get_packet()
{
    if (peek_packet() == ISP_NONE)
        return NULL;

    return packet;
}


/**
* Get next UDP packet ready
*/
int CInsim::udp_next_packet()
{
    // Clear the local buffer
    memset(udp_lbuf.buffer, 0, PACKET_BUFFER_SIZE);
    udp_lbuf.bytes = 0;

    // Read until we have a full packet
    while (udp_lbuf.bytes < 1)
    {
        // Clear them
        FD_ZERO(&udp_readfd);
        FD_ZERO(&udp_exceptfd);

        // Set them to watch our socket for data to read and exceptions that maybe thrown
        FD_SET(sockudp, &udp_readfd);
        FD_SET(sockudp, &udp_exceptfd);

        #ifdef CIS_WINDOWS
        int rc = select(0, &udp_readfd, NULL, &udp_exceptfd, &select_timeout);
        #elif defined CIS_LINUX
        int rc = pselect(sockudp + 1, &udp_readfd, NULL, &udp_exceptfd, &select_timeout, NULL);
        #endif

        if (rc == 0)                    // Timeout
            continue;

        if (rc < 0)                     // An error occured
            return -1;

        if (FD_ISSET(sockudp, &udp_exceptfd))    // An exception occured - we want to quit
            return -1;
        else  // We got data!
        {
            // Recieve any waiting bytes
            int retval = recv(sockudp, udp_lbuf.buffer + udp_lbuf.bytes, PACKET_BUFFER_SIZE - udp_lbuf.bytes, 0);

            // Deal with the results
            if (retval < 0)                 // An error ocurred
                return -1;

            udp_lbuf.bytes += retval;
        }

    }

    memcpy(udp_packet, udp_lbuf.buffer, (unsigned char)*udp_lbuf.buffer);

    return 0;
}


/**
* Return the type of the next UDP packet
*/
char CInsim::udp_peek_packet()
{
    return *(udp_packet+1);
}


/**
* Return the contents of the next UDP packet
*/
void* CInsim::udp_get_packet()
{
    return udp_packet;
}


/**
* Send a packet
*/
int CInsim::send_packet(void* s_packet)
{
    ismutex->lock();

    //Detect packet type
    switch(*((unsigned char*)s_packet+1))
    {
        case ISP_BTN:
        {
            struct IS_BTN* pack = (struct IS_BTN*)s_packet;
            unsigned char text_len = strlen(pack->Text);

            /*TODO: Should we truncate the string if it's too long
            * or should we just discard the packet?
            * If we discard the packet, perhaps another
            * return code should be used. */
            if(text_len > IS_BTN_MAXTLEN)
            {
                ismutex->unlock();
                return -1;
            }

            unsigned char texttosend;
            unsigned char remdr = text_len % 4;

            if(remdr == 0)
                texttosend = text_len;
            else
                texttosend = text_len + 4 - remdr;

            pack->Size = IS_BTN_HDRSIZE + texttosend;
        }
        break;

        case ISP_MTC:
        {
            struct IS_MTC* pack = (struct IS_MTC*)s_packet;
            unsigned char text_len = strlen(pack->Text);

            //Same as above
            if(text_len > IS_MTC_MAXTLEN)
            {
                ismutex->unlock();
                return -1;
            }

            unsigned char texttosend = text_len + 4 - text_len % 4;

            pack->Size = IS_MTC_HDRSIZE + texttosend;
        }
        break;

        default:
            break;
    }

    size_t psize = *((unsigned char*)s_packet);

    if(this->version > 8) {
        *((unsigned char*)s_packet) = psize / 4;
    }

    if (send(sock, (const char *)s_packet, psize, 0) < 0)
    {
        ismutex->unlock();
        return -1;
    }
    ismutex->unlock();
    return 0;
}

void
CInsim::SendMTC (byte UCID, std::string Msg, byte Sound)
{
    IS_MTC *pack = new IS_MTC;
    memset( pack, 0, sizeof( IS_MTC ) );
    pack->Size = sizeof( IS_MTC );
    pack->Type = ISP_MTC;
    pack->UCID = UCID;
    pack->Sound = Sound;
    strcpy( pack->Text, Msg.substr(0,127).c_str());
    send_packet( pack );
    delete pack;
}

void
CInsim::SendMST (std::string Text)
{
    IS_MST *pack = new IS_MST;
    memset( pack, 0, sizeof( IS_MST));
    pack->Size = sizeof( IS_MST);
    pack->Type = ISP_MST;
    sprintf( pack->Msg, "%.63s", Text.c_str() );
    send_packet( pack );
    delete pack;
}

void
CInsim::SendMSX(std::string Text)
{
    IS_MST *pack = new IS_MST;
    memset( pack, 0, sizeof( IS_MST ));
    pack->Size = sizeof( IS_MST );
    pack->Type = ISP_MST;
    sprintf( pack->Msg, "%.95s", Text.c_str() );
    send_packet( pack );
    delete pack;
}

void
CInsim::SendBFN (byte UCID, byte ClickID)
{
    IS_BFN *pack = new IS_BFN;
    memset( pack, 0, sizeof( IS_BFN ) );
    pack->Size = sizeof( IS_BFN );
    pack->Type = ISP_BFN;
    pack->UCID = UCID;
    pack->ClickID = ClickID;
    send_packet( pack );
    delete pack;
}

void
CInsim::SendBFN(byte UCID, byte ClickIdFrom, byte ClickIdTo)
{
    if(ClickIdFrom == ClickIdTo)
    {
        return SendBFN(UCID,ClickIdFrom);
    }

	IS_BFN *pack = new IS_BFN;
    memset( pack, 0, sizeof( IS_BFN ) );

    if( ClickIdFrom > ClickIdTo)
	{
	    pack->ClickID = ClickIdTo;
        pack->ClickMax = ClickIdFrom;
	}
	else
	{
		pack->ClickID = ClickIdFrom;
        pack->ClickMax = ClickIdTo;
	}

    pack->Size = sizeof( IS_BFN );
    pack->Type = ISP_BFN;
    pack->UCID = UCID;

    send_packet( pack );
    delete pack;
}

void
CInsim::SendBFNAll ( byte UCID )
{
    IS_BFN *pack = new IS_BFN;
    memset( pack, 0, sizeof( IS_BFN ) );
    pack->Size = sizeof( IS_BFN );
    pack->Type = ISP_BFN;
    pack->UCID = UCID;
    pack->SubT = BFN_CLEAR;
    send_packet( pack );
    delete pack;
}

void
CInsim::SendPLC (byte UCID, unsigned PLC)
{
    IS_PLC *pack = new IS_PLC;
    memset( pack, 0, sizeof( IS_PLC ) );
    pack->Size = sizeof( IS_PLC );
    pack->Type = ISP_PLC;
    pack->UCID = UCID;
    pack->Cars = PLC;
    send_packet( pack );
    delete pack;
}

void
CInsim::SendButton(byte ReqI, byte UCID, byte ClickID, byte Left, byte Top, byte Width, byte Height, byte BStyle, std::string Text)
{
    SendButton(ReqI, UCID, ClickID, Left, Top, Width, Height, BStyle, Text, 0);
}

void
CInsim::SendButton(byte ReqI, byte UCID, byte ClickID, byte Left, byte Top, byte Width, byte Height, byte BStyle, std::string Text, byte TypeIn)
{
    IS_BTN *pack = new IS_BTN;
    memset( pack, 0, sizeof( IS_BTN ) );
    pack->Size = sizeof( IS_BTN );
    pack->Type = ISP_BTN;
    pack->ReqI = ReqI;
    pack->UCID = UCID;
    pack->Inst = 0;
    pack->BStyle = BStyle;
    pack->TypeIn = TypeIn;
    pack->ClickID = ClickID;
    pack->L = Left;
    pack->T = Top;
    pack->W = Width;
    pack->H = Height;
    strcpy(pack->Text, Text.c_str());
    send_packet( pack );
    delete pack;
}

void
CInsim::SendTiny(byte SubT)
{
    SendTiny(SubT,0);
}

void
CInsim::SendTiny(byte SubT, byte ReqI)
{
    IS_TINY *packet = new IS_TINY;
    memset(packet, 0, sizeof(IS_TINY));
    packet->Size = sizeof(IS_TINY);
    packet->Type = ISP_TINY;
    packet->ReqI = ReqI;
    packet->SubT = SubT;
    send_packet(packet);
    delete packet;
}

void
CInsim::SendSmall(byte SubT, unsigned UVal)
{
    SendSmall(SubT,UVal,0);
}

void
CInsim::SendSmall(byte SubT, unsigned UVal, byte ReqI)
{
    IS_SMALL *packet = new IS_SMALL;
    memset(packet, 0, sizeof(IS_SMALL));
    packet->Size = sizeof(IS_SMALL);
    packet->Type = ISP_SMALL;
    packet->ReqI = ReqI;
    packet->SubT = SubT;
    packet->UVal = UVal;
    send_packet(packet);
    delete packet;
}

std::string
CInsim::GetLanguageCode(byte LID)
{
    switch(LID)
    {
        case LFS_ENGLISH:
            return "en";
            break;
        case LFS_DEUTSCH:
            return "de";
            break;
        case LFS_PORTUGUESE:
            return "pt";
            break;
        case LFS_FRENCH:
            return "fr";
            break;
        case LFS_SUOMI:
            return "fi";
            break;
        case LFS_NORSK:
            return "no";
            break;
        case LFS_NEDERLANDS:
            return "nl";
            break;
        case LFS_CATALAN:
            return "ca";
            break;
        case LFS_TURKISH:
            return "tr";
            break;
        case LFS_CASTELLANO:
            return "ca";
            break;
        case LFS_ITALIANO:
            return "it";
            break;
        case LFS_DANSK:
            return "da";
            break;
        case LFS_CZECH:
            return "cz";
            break;
        case LFS_RUSSIAN:
            return "ru";
            break;
        case LFS_ESTONIAN:
            return "et";
            break;
        case LFS_SERBIAN:
            return "sr";
            break;
        case LFS_GREEK:
            return "el";
            break;
        case LFS_POLSKI:
            return "pl";
            break;
        case LFS_CROATIAN:
            return "hr";
            break;
        case LFS_HUNGARIAN:
            return "hu";
            break;
        case LFS_BRAZILIAN:
            return "br";
            break;
        case LFS_SWEDISH:
            return "sv";
            break;
        case LFS_SLOVAK:
            return "sl";
            break;
        case LFS_GALEGO:
            return "ga";
            break;
        case LFS_SLOVENSKI:
            return "sl";
            break;
        case LFS_BELARUSSIAN:
            return "be";
            break;
        case LFS_LATVIAN:
            return "lv";
            break;
        case LFS_LITHUANIAN:
            return "lt";
            break;
        case LFS_TRADITIONAL_CHINESE:
            return "zh-Hant";
            break;
        case LFS_SIMPLIFIED_CHINESE:
            return "zh-Hans";
            break;
        case LFS_JAPANESE:
            return "jp";
            break;
        case LFS_KOREAN:
            return "ko";
            break;
        case LFS_BULGARIAN:
            return "bg";
            break;
        case LFS_LATINO:
            return "la";
            break;
        case LFS_UKRAINIAN:
            return "ua";
            break;
        case LFS_INDONESIAN:
            return "in";
            break;
        case LFS_ROMANIAN:
            return "ro";
            break;
        default:
            return "";
            break;
    }
}

void
CInsim::LightSet(byte Id,byte Color)
{
    IS_OCO* packet = new IS_OCO;
    memset(packet,0,sizeof(IS_OCO));
    packet->Size = sizeof(IS_OCO);
    packet->Type = ISP_OCO;
    packet->OCOAction = OCO_LIGHTS_SET;
    packet->Index = 149;
    packet->Identifier = Id;
    packet->Data = Color;
    send_packet(packet);
    delete packet;
}

void
CInsim::LightReset(byte Id)
{
    IS_OCO* packet = new IS_OCO;
    memset(packet,0,sizeof(IS_OCO));
    packet->Size = sizeof(IS_OCO);
    packet->Type = ISP_OCO;
    packet->OCOAction = OCO_LIGHTS_UNSET;
    packet->Index = 149;
    packet->Identifier = Id;
    send_packet(packet);
    delete packet;
}

void
CInsim::LightResetAll()
{
    IS_OCO* packet = new IS_OCO;
    memset(packet,0,sizeof(IS_OCO));
    packet->Size = sizeof(IS_OCO);
    packet->Type = ISP_OCO;
    packet->OCOAction = OCO_LIGHTS_RESET;
    packet->Index = 149;
    send_packet(packet);
    delete packet;
}

void
CInsim::SendJRR(byte JRRAction, byte UCID)
{
    if(JRRAction != JRR_REJECT && JRRAction != JRR_SPAWN)
    {
        throw new std::logic_error("SendJRR: JRRAction must be JRR_SPAWN or JRR_REJECT");
    }

    IS_JRR* packet = new IS_JRR;
    memset(packet,0,sizeof(IS_JRR));
    packet->Size = sizeof(IS_JRR);
    packet->Type = ISP_JRR;

    packet->JRRAction = JRRAction;
    packet->UCID = UCID;

    send_packet(packet);
    delete packet;
}

void
CInsim::SendJRR(byte JRRAction, byte PLID, ObjectInfo obj)
{
    if(JRRAction == JRR_REJECT || JRRAction == JRR_SPAWN)
    {
        throw new std::logic_error("SendJRR: JRRAction must be not JRR_SPAWN and JRR_REJECT");
    }

    IS_JRR* packet = new IS_JRR;
    memset(packet,0,sizeof(IS_JRR));
    packet->Size = sizeof(IS_JRR);
    packet->Type = ISP_JRR;

    packet->JRRAction = JRRAction;
    packet->PLID = PLID;

    packet->StartPos = obj;

    send_packet(packet);
    delete packet;
}

void
CInsim::ResetCar(byte PLID, int X, int Y, int Z, word Heading, bool repair)
{
    ObjectInfo o;
    memset(&o, 0, sizeof(ObjectInfo));

    o.X = X/4096;
    o.Y = Y/4096;
    o.Zbyte = Z/16384;
    o.Heading = Heading > 0 ? ((Heading / 182 + 180) % 360 * 256 / 360) : 0;
    o.Flags = (X != 0 || Y != 0 || Z != 0 || Heading != 0) ? 0x80 : 0;

    byte jrrAction = repair ? JRR_RESET : JRR_RESET_NO_REPAIR;

    this->SendJRR(jrrAction, PLID, o);
}

/**
* Other functions!!!
*/


/**
* Converts miliseconds to a C string
* 14 characters needed in str to not run into buffer overflow ("-hh:mm:ss.xxx\0")
* @param    milisecs    Miliseconds to convert
* @param    str         String to be filled with the result in format "-hh:mm:ss.xxx"
* @param    thousands   Result shows: 0 = result hundreths of second; other = thousandths of second
*/
char* ms2str (long milisecs, char *str, int thousands)
{
    unsigned hours = 0;
    unsigned minutes = 0;
    unsigned seconds = 0;
    unsigned hundthou = 0;

    char shours[3], sminutes[3], sseconds[3], shundthou[4];

    memset(str, 0, 14);

    if (milisecs < 0)
    {
        strcpy(str,"-");
        milisecs *= -1;
    }

    if (milisecs >= 360000000)
        return 0;

    if (milisecs >= 3600000)
    {
        hours = milisecs / 3600000;
        milisecs %= 3600000;
    }
    if (milisecs >= 60000)
    {
        minutes = milisecs / 60000;
        milisecs %= 60000;
    }
    if (milisecs >= 1000)
    {
        seconds = milisecs / 1000;
        milisecs %= 1000;
    }
    if (thousands)
        hundthou = milisecs;
    else
        hundthou = milisecs / 10;

    if (hundthou == 0)
    {
        if (thousands)
            strcpy(shundthou, "000");
        else
            strcpy(shundthou, "00");
    }

    if (hours > 0)
    {
        sprintf(shours,"%d",hours);
        strcat(strcat(str,shours),":");

        if (minutes > 9)
            sprintf(sminutes,"%d",minutes);
        else{
            strcpy(sminutes,"0");
            sprintf(sminutes+1,"%d",minutes);
        }
        strcat(strcat(str,sminutes),":");

        if (seconds > 9)
            sprintf(sseconds,"%d",seconds);
        else{
            strcpy(sseconds,"0");
            sprintf(sseconds+1,"%d",seconds);
        }
        strcat(strcat(str,sseconds),".");

        if (thousands)
        {
            if (hundthou > 99)
                sprintf(shundthou,"%d",hundthou);
            else if (hundthou > 9){
                strcpy(shundthou,"0");
                sprintf(shundthou+1,"%d",hundthou);
            }
            else{
                strcpy(shundthou,"00");
                sprintf(shundthou+2,"%d",hundthou);
            }
        }
        else
        {
            if (hundthou > 9)
                sprintf(shundthou,"%d",hundthou);
            else{
                strcpy(shundthou,"0");
                sprintf(shundthou+1,"%d",hundthou);
            }
        }
        strcat(str,shundthou);

    }
    else if (minutes > 0)
    {
        sprintf(sminutes,"%d",minutes);
        strcat(strcat(str,sminutes),":");

        if (seconds > 9)
            sprintf(sseconds,"%d",seconds);
        else{
            strcpy(sseconds,"0");
            sprintf(sseconds+1,"%d",seconds);
        }
        strcat(strcat(str,sseconds),".");

        if (thousands)
        {
            if (hundthou > 99)
                sprintf(shundthou,"%d",hundthou);
            else if (hundthou > 9){
                strcpy(shundthou,"0");
                sprintf(shundthou+1,"%d",hundthou);
            }
            else{
                strcpy(shundthou,"00");
                sprintf(shundthou+2,"%d",hundthou);
            }
        }
        else
        {
            if (hundthou > 9)
                sprintf(shundthou,"%d",hundthou);
            else{
                strcpy(shundthou,"0");
                sprintf(shundthou+1,"%d",hundthou);
            }
        }
        strcat(str,shundthou);
    }
    else if (seconds > 0)
    {
        sprintf(sseconds,"%d",seconds);
        strcat(strcat(str,sseconds),".");

        if (thousands)
        {
            if (hundthou > 99)
                sprintf(shundthou,"%d",hundthou);
            else if (hundthou > 9){
                strcpy(shundthou,"0");
                sprintf(shundthou+1,"%d",hundthou);
            }
            else{
                strcpy(shundthou,"00");
                sprintf(shundthou+2,"%d",hundthou);
            }
        }
        else
        {
            if (hundthou > 9)
                sprintf(shundthou,"%d",hundthou);
            else{
                strcpy(shundthou,"0");
                sprintf(shundthou+1,"%d",hundthou);
            }
        }
        strcat(str,shundthou);
    }
    else
    {
        strcat(str,"0.");

        if (thousands)
        {
            if (hundthou > 99)
                sprintf(shundthou,"%d",hundthou);
            else if (hundthou > 9){
                strcpy(shundthou,"0");
                sprintf(shundthou+1,"%d",hundthou);
            }
            else{
                strcpy(shundthou,"00");
                sprintf(shundthou+2,"%d",hundthou);
            }
        }
        else
        {
            if (hundthou > 9)
                sprintf(shundthou,"%d",hundthou);
            else{
                strcpy(shundthou,"0");
                sprintf(shundthou+1,"%d",hundthou);
            }
        }
        strcat(str,shundthou);
    }

    return str;
}

bool is_ascii_char(char c)
{
    if (c >= '0' && c <= '9') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= 'a' && c <= 'z') return true;
    return 0;
}

bool is_valid_id(unsigned id)
{
    if ((id & 0xff)==0        ||    // 1st char is zero
        (id & 0xff00)==0    ||    // 2nd char is zero
        (id & 0xff0000)==0    ||    // 3rd char is zero
         id & 0xff000000)        // 4th char is non-zero
    {
        return false; // invalid
    }

    if (is_ascii_char(id & 0xff) &&
        is_ascii_char((id & 0xff00) >> 8) &&
        is_ascii_char((id & 0xff0000) >> 16))
    {
        return false; // all 3 characters are 0-9 / A-Z / a-z -> reserve this id for official cars!
    }

    return true; // ok
}

bool is_official_prefix(const char* prefix)
{
    return is_ascii_char(prefix[0]) && is_ascii_char(prefix[1]) && is_ascii_char(prefix[2]) && prefix[3]==0;
}

char* expand_prefix(const char* prefix)
{
    char expanded_prefix[8];

    memset(expanded_prefix, 0, 8);

    if (prefix && prefix[3]==0) // valid prefix supplied
    {
        if (is_official_prefix(prefix))
        {
            *(unsigned *)expanded_prefix = *(unsigned *)prefix; // official car - direct copy
        }
        else // mod
        {
            sprintf(expanded_prefix, "%06X", *(unsigned *)prefix); // regard the prefix as an unsigned integer and expand it as a 6-digit hexadecimal string
        }
    }

    return expanded_prefix;
}


