/**
 * @file src/comunicationsmanagerfilesockets.cpp
 * @brief MEGAcmd: Communications manager using Network Sockets
 *
 * (c) 2013 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGAcmd.
 *
 * MEGAcmd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#ifndef WIN32

#include "comunicationsmanagerfilesockets.h"
#include "megacmdutils.h"
#include <sys/ioctl.h>

#ifdef __MACH__
#define MSG_NOSIGNAL 0
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

using namespace mega;

namespace megacmd {
int ComunicationsManagerFileSockets::get_next_comm_id()
{
    std::lock_guard<std::mutex> g(informerMutex);
    return ++count;
}

int ComunicationsManagerFileSockets::create_new_socket(int *sockId)
{
    int thesock;
    int attempts = 10;
    bool socketsucceded = false;
    while (--attempts && !socketsucceded)
    {
       thesock = socket(AF_UNIX, SOCK_STREAM, 0);

        if (thesock < 0)
        {
            if (errno == EMFILE)
            {
                LOG_verbose << " Trying to reduce number of used files by sending ACK to listeners to discard disconnected ones.";
                string sack="ack";
                informStateListeners(sack);
            }
            if (attempts !=10)
            {
                LOG_fatal << "ERROR opening socket ID=" << sockId << " errno: " << errno << ". Attempts: " << attempts;
            }
            sleepMilliSeconds(500);
        }
        else
        {
            socketsucceded = true;
        }
        if (fcntl(thesock, F_SETFD, FD_CLOEXEC) == -1)
        {
            LOG_err << "ERROR setting CLOEXEC to socket: " << errno;
        }
    }
    if (thesock < 0)
    {
        return -1;
    }

    char socket_path[60];
    *sockId = get_next_comm_id();
    bzero(socket_path, sizeof( socket_path ) * sizeof( *socket_path ));
    sprintf(socket_path, "/tmp/megaCMD_%d/srv_%d", getuid(), *sockId);

    struct sockaddr_un addr;
    socklen_t saddrlen = sizeof( addr );

    memset(&addr, 0, sizeof( addr ));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof( addr.sun_path ) - 1);

    unlink(socket_path);

    bool bindsucceeded = false;

    attempts = 10;
    while (--attempts && !bindsucceeded)
    {
        if (::bind(thesock, (struct sockaddr*)&addr, saddrlen))
        {
            if (errno == EADDRINUSE)
            {
                LOG_warn << "ERROR on binding socket: Already in use. Attempts: " << attempts;
            }
            else
            {
                LOG_fatal << "ERROR on binding socket " << socket_path << " errno: " << errno << ". Attempts: " << attempts;
            }
            sleepMilliSeconds(500);
        }
        else
        {
            bindsucceeded = true;
        }
    }


    if (bindsucceeded)
    {
        if (thesock)
        {
            int returned = listen(thesock, 150);
            if (returned)
            {
                LOG_fatal << "ERROR on listen socket: " << errno;
            }
        }
        return thesock;
    }

    return 0;
}


ComunicationsManagerFileSockets::ComunicationsManagerFileSockets()
{
    count = 0;
    initialize();
}

int ComunicationsManagerFileSockets::initialize()
{
    MegaFileSystemAccess *fsAccess = new MegaFileSystemAccess();
    char csocketsFolder[34]; // enough to hold all numbers up to 64-bits
    sprintf(csocketsFolder, "/tmp/megaCMD_%d", getuid());
    LocalPath socketsFolder = LocalPath::fromLocalname(csocketsFolder);

    fsAccess->setdefaultfolderpermissions(0700);
    fsAccess->rmdirlocal(socketsFolder);
    LOG_debug << "CREATING sockets folder: " << socketsFolder.toPath(*fsAccess) << "!!!";
    if (!fsAccess->mkdirlocal(socketsFolder, false))
    {
        LOG_fatal << "ERROR CREATING sockets folder: " << socketsFolder.toPath(*fsAccess) << ": " << errno;
    }
    delete fsAccess;

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        LOG_fatal << "ERROR opening socket";
    }
    if (fcntl(sockfd, F_SETFD, FD_CLOEXEC) == -1)
    {
        LOG_err << "ERROR setting CLOEXEC to socket: " << errno;
    }

    struct sockaddr_un addr;
    socklen_t saddrlen = sizeof( addr );
    memset(&addr, 0, sizeof( addr ));
    addr.sun_family = AF_UNIX;

    char socketPath[60];
    bzero(socketPath, sizeof( socketPath ) * sizeof( *socketPath ));
    sprintf(socketPath, "/tmp/megaCMD_%d/srv", getuid());

    strncpy(addr.sun_path, socketPath, sizeof( addr.sun_path ) - 1);

    unlink(socketPath);

    if (::bind(sockfd, (struct sockaddr*)&addr, saddrlen))
    {
        if (errno == EADDRINUSE)
        {
            LOG_warn << "ERROR on binding socket: " << socketPath << ": Already in use.";
        }
        else
        {
            LOG_fatal << "ERROR on binding socket: " << socketPath << ": " << errno;
            sockfd = -1;
        }
    }
    else
    {
        int returned = listen(sockfd, 150);
        if (returned)
        {
            LOG_fatal << "ERROR on listen socket initializing communications manager: " << socketPath << ": " << errno;
            return errno;
        }
    }
    return 0;
}

bool ComunicationsManagerFileSockets::receivedPetition()
{
    return FD_ISSET(sockfd, &fds);
}

int ComunicationsManagerFileSockets::waitForPetition()
{
    FD_ZERO(&fds);
    if (sockfd)
    {
        FD_SET(sockfd, &fds);
    }
    int rc = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
    if (rc < 0)
    {
        if (errno != EINTR)  //syscall
        {
            LOG_fatal << "Error at select: " << errno;
            return errno;
        }
    }
    return 0;
}

void ComunicationsManagerFileSockets::stopWaiting()
{
#ifdef _WIN32
    shutdown(sockfd,SD_BOTH);
#else
    LOG_verbose << "Shutting down main socket ";

    if (shutdown(sockfd,SHUT_RDWR) == -1)
    { //shutdown failed. we need to send something to the blocked thread so as to wake up from select

        int clientsocket = socket(AF_UNIX, SOCK_STREAM, 0);
        char socket_path[60];
        if (clientsocket < 0 )
        {
            LOG_err << "ERROR opening client socket to exit select: " << errno;
            close (sockfd);
        }
        else
        {
            if (fcntl(clientsocket, F_SETFD, FD_CLOEXEC) == -1)
            {
                LOG_err << "ERROR setting CLOEXEC to socket: " << errno;
            }
            bzero(socket_path, sizeof( socket_path ) * sizeof( *socket_path ));
            {
                sprintf(socket_path, "/tmp/megaCMD_%d/srv", getuid() );
            }

            struct sockaddr_un addr;

            memset(&addr, 0, sizeof( addr ));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, socket_path, sizeof( addr.sun_path ) - 1);

            if (::connect(clientsocket, (struct sockaddr*)&addr, sizeof( addr )) != -1)
            {
                if (send(clientsocket,"no matter",1,MSG_NOSIGNAL) == -1)
                {
                    LOG_err << "ERROR sending via client socket to exit select: " << errno;
                    close (sockfd);
                }

                close(clientsocket);
            }
            else
            {
                LOG_err << "ERROR connecting client socket to exit select: " << errno;
                close (sockfd);
            }
        }
    }
    LOG_verbose << "Main socket shut down";
#endif
}


void ComunicationsManagerFileSockets::registerStateListener(CmdPetition *inf)
{
    LOG_debug << "Registering state listener petition with socket: " << ((CmdPetitionPosixSockets *) inf)->outSocket;
    ComunicationsManager::registerStateListener(inf);
}

/**
 * @brief returnAndClosePetition
 * I will clean struct and close the socket within
 */
void ComunicationsManagerFileSockets::returnAndClosePetition(CmdPetition *inf, OUTSTRINGSTREAM *s, int outCode)
{
    LOG_verbose << "Output to write in socket " << ((CmdPetitionPosixSockets *)inf)->outSocket;
    sockaddr_in cliAddr;
    socklen_t cliLength = sizeof( cliAddr );
    int connectedsocket = ((CmdPetitionPosixSockets *)inf)->acceptedOutSocket;
    if (connectedsocket == -1)
    {
        connectedsocket = accept(((CmdPetitionPosixSockets *)inf)->outSocket, (struct sockaddr*)&cliAddr, &cliLength);
        if (fcntl(connectedsocket, F_SETFD, FD_CLOEXEC) == -1)
        {
            LOG_err << "ERROR setting CLOEXEC to socket: " << errno;
        }
        ((CmdPetitionPosixSockets *)inf)->acceptedOutSocket = connectedsocket; //So that it gets closed in destructor
    }
    if (connectedsocket == -1)
    {
        LOG_fatal << "Return and close: Unable to accept on outsocket " << ((CmdPetitionPosixSockets *)inf)->outSocket << " error: " << errno;
        delete inf;
        return;
    }

    string sout = s->str();

    int n = send(connectedsocket, (void*)&outCode, sizeof( outCode ), MSG_NOSIGNAL);
    if (n < 0)
    {
        LOG_err << "ERROR writing output Code to socket: " << errno;
    }
    n = send(connectedsocket, sout.data(), max(1,(int)sout.size()), MSG_NOSIGNAL); // for some reason without the max recv never quits in the client for empty responses
    if (n < 0)
    {
        LOG_err << "ERROR writing to socket: " << errno;
    }

    delete inf;
}

void ComunicationsManagerFileSockets::sendPartialOutput(CmdPetition *inf, OUTSTRING *s)
{
    if (inf->clientDisconnected)
    {
        return;
    }

    sockaddr_in cliAddr;
    socklen_t cliLength = sizeof( cliAddr );
    int connectedsocket = ((CmdPetitionPosixSockets *)inf)->acceptedOutSocket;
    if (connectedsocket == -1)
    {
        connectedsocket = accept(((CmdPetitionPosixSockets *)inf)->outSocket, (struct sockaddr*)&cliAddr, &cliLength);
        if (fcntl(connectedsocket, F_SETFD, FD_CLOEXEC) == -1)
        {
            std::cerr << "ERROR setting CLOEXEC to socket: " << errno << endl; // we do not log this stuff, because that would cause some loop sending more partial output
        }
        ((CmdPetitionPosixSockets *)inf)->acceptedOutSocket = connectedsocket; //So that it gets closed in destructor
    }
    if (connectedsocket == -1)
    {
        std::cerr << "Return and close: Unable to accept on outsocket " << ((CmdPetitionPosixSockets *)inf)->outSocket << " error: " << errno << endl;
        return;
    }

    if (s->size())
    {
        size_t size = s->size();

        int outCode = MCMD_PARTIALOUT;
        int n = send(connectedsocket, (void*)&outCode, sizeof( outCode ), MSG_NOSIGNAL);
        if (n < 0)
        {
            std::cerr << "ERROR writing MCMD_PARTIALOUT to socket: " << errno << endl;
            if (errno == EPIPE)
            {
                std::cerr << "WARNING: Client disconnected, the rest of the output will be discarded" << endl;
                inf->clientDisconnected = true;
            }
            return;
        }
        n = send(connectedsocket, (void*)&size, sizeof( size ), MSG_NOSIGNAL);
        if (n < 0)
        {
            std::cerr << "ERROR writing size of partial output to socket: " << errno << endl;
            return;
        }


        n = send(connectedsocket, s->data(), size, MSG_NOSIGNAL); // for some reason without the max recv never quits in the client for empty responses

        if (n < 0)
        {
            std::cerr << "ERROR writing to socket partial output: " << errno << endl;
            return;
        }
    }
}

int ComunicationsManagerFileSockets::informStateListener(CmdPetition *inf, string &s)
{
    std::lock_guard<std::mutex> g(informerMutex);
    LOG_verbose << "Inform State Listener: Output to write in socket " << ((CmdPetitionPosixSockets *)inf)->outSocket << ": <<" << s << ">>";

    sockaddr_in cliAddr;
    socklen_t cliLength = sizeof( cliAddr );

    static map<int,int> connectedsockets;

    int connectedsocket = -1;
    if (connectedsockets.find(((CmdPetitionPosixSockets *)inf)->outSocket) == connectedsockets.end())
    {
        //select with timeout and accept non-blocking, so that things don't get stuck
        fd_set set;
        FD_ZERO(&set);
        FD_SET(((CmdPetitionPosixSockets *)inf)->outSocket, &set);

        struct timeval timeout;
        timeout.tv_sec = 4;
        timeout.tv_usec = 0;
        int rv = select(((CmdPetitionPosixSockets *)inf)->outSocket+1, &set, NULL, NULL, &timeout);
        if(rv == -1)
        {
            LOG_err << "Informing state listener: Unable to select on outsocket " << ((CmdPetitionPosixSockets *)inf)->outSocket << " error: " << errno;
            return -1;
        }
        else if(rv == 0)
        {
            LOG_warn << "Informing state listener: timeout in select on outsocket " << ((CmdPetitionPosixSockets *)inf)->outSocket;
        }
        else
        {
            int oldfl = fcntl(sockfd, F_GETFL);
            fcntl(((CmdPetitionPosixSockets *)inf)->outSocket, F_SETFL, oldfl | O_NONBLOCK);
            connectedsocket = accept(((CmdPetitionPosixSockets *)inf)->outSocket, (struct sockaddr*)&cliAddr, &cliLength);
            if (fcntl(connectedsocket, F_SETFD, FD_CLOEXEC) == -1)
            {
                LOG_err << "ERROR setting CLOEXEC to socket: " << errno;
            }
            fcntl(((CmdPetitionPosixSockets *)inf)->outSocket, F_SETFL, oldfl);
        }
        connectedsockets[((CmdPetitionPosixSockets *)inf)->outSocket] = connectedsocket;
    }
    else
    {
        connectedsocket = connectedsockets[((CmdPetitionPosixSockets *)inf)->outSocket];
    }

    if (connectedsocket == -1)
    {
        if (errno == 32) //socket closed
        {
            LOG_debug << "Unregistering no longer listening client. Original petition: " << inf->line;
            connectedsockets.erase(((CmdPetitionPosixSockets *)inf)->outSocket);
            return -1;
        }
        else
        {
            LOG_err << "Informing state listener: Unable to accept on outsocket " << ((CmdPetitionPosixSockets *)inf)->outSocket << " error: " << errno;
        }
        return 0;
    }

#ifdef __MACH__
#define MSG_NOSIGNAL 0
#endif

    int n = send(connectedsocket, s.data(), s.size(), MSG_NOSIGNAL);
    if (n < 0)
    {
        if (errno == 32) //socket closed
        {
            LOG_debug << "Unregistering no longer listening client. Original petition: " << inf->line;
            close(connectedsocket);
            connectedsockets.erase(((CmdPetitionPosixSockets *)inf)->outSocket);
            return -1;
        }
        else
        {
            LOG_err << "ERROR writing to socket: " << errno;
        }
    }

    return 0;
}

/**
 * @brief getPetition
 * @return pointer to new CmdPetitionPosix. Petition returned must be properly deleted (this can be calling returnAndClosePetition)
 */
CmdPetition * ComunicationsManagerFileSockets::getPetition()
{
    CmdPetitionPosixSockets *inf = new CmdPetitionPosixSockets();

    clilen = sizeof( cli_addr );

    newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
    if (fcntl(newsockfd, F_SETFD, FD_CLOEXEC) == -1)
    {
        LOG_err << "ERROR setting CLOEXEC to socket: " << errno;
    }
    if (newsockfd < 0)
    {
        if (errno == EMFILE)
        {
            LOG_fatal << "ERROR on accept at getPetition: TOO many open files.";
            //send state listeners an ACK command to see if they are responsive and close them otherwise
            string sack = "ack";
            informStateListeners(sack);
        }
        else
        {
            LOG_fatal << "ERROR on accept at getPetition: " << errno;
        }

        sleep(1);
        inf->line = strdup("ERROR");
        return inf;
    }

    string wholepetition;

    int n = read(newsockfd, buffer, 1023);
    while(n == 1023)
    {
        unsigned long int total_available_bytes;
        if (-1 == ioctl(newsockfd, FIONREAD, &total_available_bytes))
        {
            LOG_err << "Failed to PeekNamedPipe. errno: " << errno;
            break;
        }
        if (total_available_bytes == 0)
        {
            break;
        }
        buffer[n]=0;
        wholepetition.append(buffer);
        n = read(newsockfd, buffer, 1023);
    }
    if (n >=0 )
    {
        buffer[n]=0;
        wholepetition.append(buffer);
    }
    if (n < 0)
    {
        LOG_fatal << "ERROR reading from socket at getPetition: " << errno;
        inf->line = strdup("ERROR");
        return inf;
    }

    int socket_id = 0;
    inf->outSocket = create_new_socket(&socket_id);
    if (!inf->outSocket || !socket_id)
    {
        LOG_fatal << "ERROR creating output socket at getPetition: " << errno;
        inf->line = strdup("ERROR");
        return inf;
    }

    n = write(newsockfd, &socket_id, sizeof( socket_id ));
    if (n < 0)
    {
        LOG_fatal << "ERROR writing to socket at getPetition: " << errno;
        inf->line = strdup("ERROR");
        return inf;
    }
    close(newsockfd);


    inf->line = strdup(wholepetition.c_str());

    return inf;
}

int ComunicationsManagerFileSockets::getConfirmation(CmdPetition *inf, string message)
{
    sockaddr_in cliAddr;
    socklen_t cliLength = sizeof( cliAddr );
    int connectedsocket = ((CmdPetitionPosixSockets *)inf)->acceptedOutSocket;
    if (connectedsocket == -1)
    {
        connectedsocket = accept(((CmdPetitionPosixSockets *)inf)->outSocket, (struct sockaddr*)&cliAddr, &cliLength);
        if (fcntl(connectedsocket, F_SETFD, FD_CLOEXEC) == -1)
        {
            LOG_err << "ERROR setting CLOEXEC to socket: " << errno;
        }
    }
    ((CmdPetitionPosixSockets *)inf)->acceptedOutSocket = connectedsocket;
    if (connectedsocket == -1)
    {
        LOG_fatal << "Getting Confirmation: Unable to accept on outsocket " << ((CmdPetitionPosixSockets *)inf)->outSocket << " error: " << errno;
        delete inf;
        return false;
    }

    int outCode = MCMD_REQCONFIRM;
    int n = send(connectedsocket, (void*)&outCode, sizeof( outCode ), MSG_NOSIGNAL);
    if (n < 0)
    {
        LOG_err << "ERROR writing output Code to socket: " << errno;
    }
    n = send(connectedsocket, message.data(), max(1,(int)message.size()), MSG_NOSIGNAL); // for some reason without the max recv never quits in the client for empty responses
    if (n < 0)
    {
        LOG_err << "ERROR writing to socket: " << errno;
    }

    int response;
    n = recv(connectedsocket,&response, sizeof(response), MSG_NOSIGNAL);

    return response;
}

string ComunicationsManagerFileSockets::getUserResponse(CmdPetition *inf, string message)
{
    sockaddr_in cliAddr;
    socklen_t cliLength = sizeof( cliAddr );
    int connectedsocket = ((CmdPetitionPosixSockets *)inf)->acceptedOutSocket;
    if (connectedsocket == -1)
    {
        connectedsocket = accept(((CmdPetitionPosixSockets *)inf)->outSocket, (struct sockaddr*)&cliAddr, &cliLength);
        if (fcntl(connectedsocket, F_SETFD, FD_CLOEXEC) == -1)
        {
            LOG_err << "ERROR setting CLOEXEC to socket: " << errno;
        }
    }
    ((CmdPetitionPosixSockets *)inf)->acceptedOutSocket = connectedsocket;
    if (connectedsocket == -1)
    {
        LOG_fatal << "Getting Confirmation: Unable to accept on outsocket " << ((CmdPetitionPosixSockets *)inf)->outSocket << " error: " << errno;
        delete inf;
        return "FAILED";
    }

    int outCode = MCMD_REQSTRING;
    int n = send(connectedsocket, (void*)&outCode, sizeof( outCode ), MSG_NOSIGNAL);
    if (n < 0)
    {
        LOG_err << "ERROR writing output Code to socket: " << errno;
    }
    n = send(connectedsocket, message.data(), max(1,(int)message.size()), MSG_NOSIGNAL); // for some reason without the max recv never quits in the client for empty responses
    if (n < 0)
    {
        LOG_err << "ERROR writing to socket: " << errno;
    }

    string response;
    int BUFFERSIZE = 1024;
    char buffer[1025];
    do{
        n = recv(connectedsocket, buffer, BUFFERSIZE, MSG_NOSIGNAL);
        if (n)
        {
            buffer[n]='\0';
            response += buffer;
        }
    } while(n == BUFFERSIZE && n != SOCKET_ERROR);

    return response;
}

string ComunicationsManagerFileSockets::get_petition_details(CmdPetition *inf)
{
    ostringstream os;
    os << "socket output: " << ((CmdPetitionPosixSockets *)inf)->outSocket;
    return os.str();
}


ComunicationsManagerFileSockets::~ComunicationsManagerFileSockets()
{
}
}//end namespace

#endif