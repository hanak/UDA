// Create an IPv4 or IPv6 Socket Connection to the UDA server with a randomised time delay between connection attempts
//
//----------------------------------------------------------------
#ifdef _WIN32
#  include <cctype>
#  include <winsock2.h> // must be included before connection.h to avoid macro redefinition in rpc/types.h
#endif

#include "connection.hpp"

#include <cstdlib>
#include <cerrno>
#include <csignal>
#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>

#if defined(__GNUC__) && !defined(MINGW)
#  ifndef _WIN32
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <arpa/inet.h>
#    include <netdb.h>
#    include <netinet/tcp.h>
#  endif
#  include <unistd.h>
#  include <strings.h>
#else
#  include <process.h>
#  include <ws2tcpip.h>
#  define strcasecmp _stricmp
#  define sleep(DELAY) Sleep((DWORD)((DELAY)*1E3))
#  define close(SOCK) closesocket(SOCK)
#  ifndef MINGW
#    pragma comment(lib, "Ws2_32.lib")
#  endif

#  ifndef EAI_SYSTEM
#    define EAI_SYSTEM -11 /* System error returned in 'errno'. */
#  endif
#endif

#include <clientserver/errorLog.h>
#include <clientserver/manageSockets.h>
#include <logging/logging.h>

#include "host_list.hpp"
#include "error_codes.h"

#if defined(SSLAUTHENTICATION)
#  include <authentication/udaClientSSL.h>
#endif

#if defined(COMPILER_GCC) || defined(__clang__)
#  define ALLOW_UNUSED_TYPE __attribute__((__unused__))
#else
#  define ALLOW_UNUSED_TYPE
#endif

#define PORT_STRING    64

int uda::client::Connection::open()
{
    return client_socket != -1;
}

int uda::client::Connection::find_socket(int fh)
{
    int i = 0;
    for (const auto& socket : socket_list) {
        if (socket.fh == client_socket) {
            return i;
        }
        ++i;
    }
    return -1;
}

int uda::client::Connection::reconnect(XDR** client_input, XDR** client_output, time_t* tv_server_start,
              int* user_timeout)
{
    int err = 0;

    // Save current client and server timer settings, Socket and XDR handles

    time_t tv_server_start0 = *tv_server_start;
    int user_timeout0 = *user_timeout;
    int clientSocket0 = client_socket;
    XDR* clientInput0 = *client_input;
    XDR* clientOutput0 = *client_output;

    // Identify the current Socket connection in the Socket List

    int socketId = find_socket(client_socket);

    // Instance a new server if the Client has changed the host and/or port number

    if (environment.server_reconnect) {
        time(tv_server_start);         // Start a New Server AGE timer
        client_socket = -1;              // Flags no Socket is open
        environment.server_change_socket = 0;   // Client doesn't know the Socket ID so disable
    }

    // Client manages connections through the Socket id and specifies which running server to connect to

    if (environment.server_change_socket) {
        if ((socketId = find_socket(environment.server_socket)) < 0) {
            err = NO_SOCKET_CONNECTION;
            addIdamError(UDA_CODE_ERROR_TYPE, __func__, err, "The User Specified Socket Connection does not exist");
            return err;
        }

        // replace with previous timer settings and XDR handles

        *tv_server_start = socket_list[socketId].tv_server_start;
        *user_timeout = socket_list[socketId].user_timeout;
        client_socket = socket_list[socketId].fh;
        *client_input = socket_list[socketId].Input;
        *client_output = socket_list[socketId].Output;

        environment.server_change_socket = 0;
        environment.server_socket = socket_list[socketId].fh;
        environment.server_port = socket_list[socketId].port;
        strcpy(environment.server_host, socket_list[socketId].host);
    }

    // save Previous data if a previous socket existed

    if (socketId >= 0) {
        socket_list[socketId].tv_server_start = tv_server_start0;
        socket_list[socketId].user_timeout = user_timeout0;
        socket_list[socketId].fh = clientSocket0;
        socket_list[socketId].Input = clientInput0;
        socket_list[socketId].Output = clientOutput0;
    }

    return err;
}

void localhostInfo(int* ai_family)
{
    char addr_buf[64];
    struct addrinfo* info = nullptr, * result = nullptr;
    getaddrinfo("localhost", nullptr, nullptr, &info);
    result = info;    // Take the first linked list member
    if (result->ai_family == AF_INET) {
        *ai_family = AF_INET;
        inet_ntop(AF_INET, &((struct sockaddr_in*)result->ai_addr)->sin_addr, addr_buf, sizeof(addr_buf));
        UDA_LOG(UDA_LOG_DEBUG, "localhost Information: IPv4 - %s\n", addr_buf);
    } else {
        *ai_family = AF_INET6;
        inet_ntop(AF_INET6, &((struct sockaddr_in6*)result->ai_addr)->sin6_addr, addr_buf, sizeof(addr_buf));
        UDA_LOG(UDA_LOG_DEBUG, "localhost Information: IPv6 - %s\n", addr_buf);
    }
    if (info) freeaddrinfo(info);
}

void setHints(struct addrinfo* hints, const char* hostname)
{
    hints->ai_family = AF_UNSPEC;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_flags = 0; //AI_CANONNAME | AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG ;
    hints->ai_protocol = 0;
    hints->ai_canonname = nullptr;
    hints->ai_addr = nullptr;
    hints->ai_next = nullptr;

    // Localhost? Which IP family? (AF_UNSPEC gives an 'Unknown Error'!)

    if (!strcmp(hostname, "localhost")) localhostInfo(&hints->ai_family);

    // Is the address Numeric? Is it IPv6?

    if (strchr(hostname, ':')) {        // Appended port number should have been stripped off
        hints->ai_family = AF_INET6;
        hints->ai_flags = hints->ai_flags | AI_NUMERICHOST;
    } else {
        // make a list of address components
        std::vector<std::string> list;
        boost::split(list, hostname, boost::is_any_of("."), boost::token_compress_on);

        // Are all address components numeric?
        bool isNumeric = true;
        for (const auto& token : list) {
            size_t lstr = token.size();
            for (size_t j = 0; j < lstr; j++) {
                isNumeric &= (bool)std::isdigit(token[j]);
            }
        }

        if (isNumeric) {
            hints->ai_flags = hints->ai_flags | AI_NUMERICHOST;
        }
    }
}

int uda::client::Connection::create(XDR* client_input, XDR* client_output, const HostList& host_list)
{
    int window_size = DB_READ_BLOCK_SIZE;        // 128K
    int rc;

    static int max_socket_delay = -1;
    static int max_socket_attempts = -1;

    if (max_socket_delay < 0) {
        char* env = getenv("UDA_MAX_SOCKET_DELAY");
        if (env == nullptr) {
            max_socket_delay = 10;
        } else {
            max_socket_delay = (int)strtol(env, nullptr, 10);
        }
    }

    if (max_socket_attempts < 0) {
        char* env = getenv("UDA_MAX_SOCKET_ATTEMPTS");
        if (env == nullptr) {
            max_socket_attempts = 3;
        } else {
            max_socket_attempts = (int)strtol(env, nullptr, 10);
        }
    }

    if (client_socket >= 0) {
        // Check Already Opened?
        return 0;
    }

#if defined(SSLAUTHENTICATION) && !defined(FATCLIENT)
    putUdaClientSSLSocket(client_socket);
#endif

#ifdef _WIN32                            // Initialise WINSOCK Once only
    static unsigned int    initWinsock = 0;
    WORD sockVersion;
    WSADATA wsaData;
    if (!initWinsock) {
        sockVersion = MAKEWORD(2, 2);                // Select Winsock version 2.2
        WSAStartup(sockVersion, &wsaData);
        initWinsock = 1;
    }
#endif

    // Identify the UDA server host and is the socket IPv4 or IPv6?

    const char* hostname = environment.server_host;
    char serviceport[PORT_STRING];

    // Check if the host_name is an alias for an IP address or domain name in the client configuration - replace if found

    auto host = host_list.find_by_alias(hostname);
    if (host != nullptr) {
        if (host->host_name != environment.server_host) {
            strcpy(environment.server_host, host->host_name.c_str());    // Replace
        }
        int port = host->port;
        if (port > 0 && environment.server_port != port) {
            environment.server_port = port;
        }
    } else if ((host = host_list.find_by_name(hostname)) != nullptr) {
        // No alias found, maybe the domain name or ip address is listed
        int port = host->port;
        if (port > 0 && environment.server_port != port) {
            // Replace if found and different
            environment.server_port = port;
        }
    }
    sprintf(serviceport, "%d", environment.server_port);

    // Does the host name contain the SSL protocol prefix? If so strip this off

#if defined(SSLAUTHENTICATION) && !defined(FATCLIENT)
    if (!strncasecmp(hostname, "SSL://", 6)) {
        // Should be stripped already if via the HOST client configuration file
        strcpy(environment.server_host, &hostname[6]);  // Replace
        putUdaClientSSLProtocol(1);
    } else {
        if (host != nullptr && host->isSSL) {
            putUdaClientSSLProtocol(1);
        } else { 
            putUdaClientSSLProtocol(0);
        }
    }
#endif

    // Resolve the Host and the IP protocol to be used (Hints not used)

    struct addrinfo* result = nullptr;
    struct addrinfo hints = { 0 };
    setHints(&hints, hostname);

    errno = 0;
    if ((rc = getaddrinfo(hostname, serviceport, &hints, &result)) != 0 || (errno != 0 && errno != ESRCH)) {
        addIdamError(UDA_SYSTEM_ERROR_TYPE, __func__, rc, (char*)gai_strerror(rc));
        if (rc == EAI_SYSTEM || errno != 0) addIdamError(UDA_SYSTEM_ERROR_TYPE, __func__, errno, "");
        if (result) freeaddrinfo(result);
        return -1;
    }

    if (result->ai_family == AF_INET) {
        UDA_LOG(UDA_LOG_DEBUG, "Socket Connection is IPv4\n");
    } else {
        UDA_LOG(UDA_LOG_DEBUG, "Socket Connection is IPv6\n");
    }

    errno = 0;
    client_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

    if (client_socket < 0 || errno != 0) {
        if (errno != 0) {
            addIdamError(UDA_SYSTEM_ERROR_TYPE, __func__, errno, "");
        } else {
            addIdamError(UDA_CODE_ERROR_TYPE, __func__, -1, "Problem Opening Socket");
        }
        if (client_socket != -1) {
#ifndef _WIN32
            ::close(client_socket);
#else
            ::closesocket(clientSocket);
#endif
        }
        client_socket = -1;
        freeaddrinfo(result);
        return -1;
    }

    // Connect to server

    errno = 0;
    while ((rc = connect(client_socket, result->ai_addr, result->ai_addrlen)) && errno == EINTR) {}

    if (rc < 0 || (errno != 0 && errno != EINTR)) {

        // Try again for a maximum number of tries with a random time delay between attempts

        int ps;
        ps = getpid();
        srand((unsigned int)ps);                                                // Seed the random number generator with the process id
        unsigned int delay = max_socket_delay > 0 ? (unsigned int)(rand() % max_socket_delay) : 0; // random delay
        sleep(delay);
        errno = 0;                                                           // wait period
        for (int i = 0; i < max_socket_attempts; i++) {                             // try again
            while ((rc = connect(client_socket, result->ai_addr, result->ai_addrlen)) && errno == EINTR) {}

            if (rc == 0 && errno == 0) break;

            delay = max_socket_delay > 0 ? (unsigned int)(rand() % max_socket_delay) : 0;
            sleep(delay);                            // wait period
        }

        if (rc != 0 || errno != 0) {
            UDA_LOG(UDA_LOG_DEBUG, "Connect errno = %d\n", errno);
            UDA_LOG(UDA_LOG_DEBUG, "Connect rc = %d\n", rc);
            UDA_LOG(UDA_LOG_DEBUG, "Unable to connect to primary host: %s on port %s\n", hostname, serviceport);
        }

        // Abandon the principal Host - attempt to connect to the secondary host
        if (rc < 0 && strcmp(environment.server_host, environment.server_host2) != 0 &&
            strlen(environment.server_host2) > 0) {

            freeaddrinfo(result);
            result = nullptr;
#ifndef _WIN32
            ::close(client_socket);
#else
            ::closesocket(clientSocket);
#endif
            client_socket = -1;
            hostname = environment.server_host2;

            // Check if the host_name is an alias for an IP address or name in the client configuration - replace if found

            host = host_list.find_by_alias(hostname);
            if (host != nullptr) {
                if (host->host_name != environment.server_host2) {
                    strcpy(environment.server_host2, host->host_name.c_str());
                }
                int port = host->port;
                if (port > 0 && environment.server_port2 != port) {
                    environment.server_port2 = port;
                }
            } else if ((host = host_list.find_by_name(hostname)) != nullptr) {    // No alias found
                int port = host->port;
                if (port > 0 && environment.server_port2 != port) {
                    environment.server_port2 = port;
                }
            }
            sprintf(serviceport, "%d", environment.server_port2);

            // Does the host name contain the SSL protocol prefix? If so strip this off

#if defined(SSLAUTHENTICATION) && !defined(FATCLIENT)
            if (!strncasecmp(hostname, "SSL://", 6)){
                // Should be stripped already if via the HOST client configuration file
                strcpy(environment.server_host2, &hostname[6]);    // Replace
                putUdaClientSSLProtocol(1);
            } else {
                if (host != nullptr && host->isSSL) {
                    putUdaClientSSLProtocol(1);
                } else { 
                    putUdaClientSSLProtocol(0);
                }
            }
#endif

            // Resolve the Host and the IP protocol to be used (Hints not used)

            setHints(&hints, hostname);

            errno = 0;
            if ((rc = getaddrinfo(hostname, serviceport, &hints, &result)) != 0 || errno != 0) {
                addIdamError(UDA_SYSTEM_ERROR_TYPE, __func__, rc, (char*)gai_strerror(rc));
                if (rc == EAI_SYSTEM || errno != 0) addIdamError(UDA_SYSTEM_ERROR_TYPE, __func__, errno, "");
                if (result) freeaddrinfo(result);
                return -1;
            }
            errno = 0;
            client_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

            if (client_socket < 0 || errno != 0) {
                if (errno != 0) {
                    addIdamError(UDA_SYSTEM_ERROR_TYPE, __func__, errno, "");
                } else {
                    addIdamError(UDA_CODE_ERROR_TYPE, __func__, -1, "Problem Opening Socket");
                }
                if (client_socket != -1) {
#ifndef _WIN32
                    ::close(client_socket);
#else
                    ::closesocket(clientSocket);
#endif
                }
                client_socket = -1;
                freeaddrinfo(result);
                return -1;
            }

            for (int i = 0; i < max_socket_attempts; i++) {
                while ((rc = connect(client_socket, result->ai_addr, result->ai_addrlen)) && errno == EINTR) {}
                if (rc == 0) {
                    int port = environment.server_port2;                // Swap data so that accessor function reports correctly
                    environment.server_port2 = environment.server_port;
                    environment.server_port = port;
                    std::string name = environment.server_host2;
                    strcpy(environment.server_host2, environment.server_host);
                    strcpy(environment.server_host, name.c_str());
                    break;
                }
                delay = max_socket_delay > 0 ? (unsigned int)(rand() % max_socket_delay) : 0;
                sleep(delay);                            // wait period
            }
        }

        if (rc < 0) {
            if (errno != 0) {
                addIdamError(UDA_SYSTEM_ERROR_TYPE, __func__, errno, "");
            } else {
                addIdamError(UDA_CODE_ERROR_TYPE, __func__, -1, "Unable to Connect to Server Stream Socket");
            }
            if (client_socket != -1)
#ifndef _WIN32
            {
                ::close(client_socket);
            }
#else
            closesocket(clientSocket);
#endif
            client_socket = -1;
            if (result) freeaddrinfo(result);
            return -1;
        }
    }

    if (result) freeaddrinfo(result);

    // Set the receive and send buffer sizes

    setsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, (char*)&window_size, sizeof(window_size));

    setsockopt(client_socket, SOL_SOCKET, SO_RCVBUF, (char*)&window_size, sizeof(window_size));

    // Other Socket Options

    int on = 1;
    if (setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE, (char*)&on, sizeof(on)) < 0) {
        addIdamError(UDA_CODE_ERROR_TYPE, __func__, -1, "Error Setting KEEPALIVE on Socket");
        ::close(client_socket);
        client_socket = -1;
        return -1;
    }
    on = 1;
    if (setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on)) < 0) {
        addIdamError(UDA_CODE_ERROR_TYPE, __func__, -1, "Error Setting NODELAY on Socket");
        ::close(client_socket);
        client_socket = -1;
        return -1;
    }

    // Add New Socket to the Socket's List

    Sockets socket = {};

    socket.type = TYPE_UDA_SERVER;
    socket.status = 1;
    socket.fh = client_socket;
    socket.port = environment.server_port;
    strcpy(socket.host, environment.server_host);
    socket.tv_server_start = 0;
    socket.user_timeout = 0;
    socket.Input = client_input;
    socket.Output = client_output;

    socket_list.push_back(socket);

    environment.server_reconnect = 0;
    environment.server_change_socket = 0;
    environment.server_socket = client_socket;

    // Write the socket number to the SSL functions

#if defined(SSLAUTHENTICATION) && !defined(FATCLIENT)
    putUdaClientSSLSocket(client_socket);
#endif

    return 0;
}

void uda::client::Connection::close_socket(int fh)
{
    for (auto& socket : socket_list) {
        if (socket.fh == fh && socket.fh >= 0) {
            if (socket.type == TYPE_UDA_SERVER) {
#ifndef _WIN32
                ::close(fh);                    // Only Genuine Sockets!
#else
                ::closesocket(fh);
#endif
            }
            socket.status = 0;
            socket.fh = -1;
            break;
        }
    }
}

void uda::client::Connection::close(ClosedownType type)
{
    if (client_socket >= 0 && type != ClosedownType::CLOSE_ALL) {
        close_socket(client_socket);
    } else {
        for (auto& socket : socket_list) {
            close_socket(socket.fh);
        }
    }

    client_socket = -1;
}

void update_select_params(int fd, fd_set *rfds, struct timeval *tv) {
    FD_ZERO(rfds);
    FD_SET(fd, rfds);
    tv->tv_sec = 0;
    tv->tv_usec = 500;    // in microsecs => 0.5 ms wait
}

int uda::client::writeout(void* iohandle, char* buf, int count)
{
#ifndef _WIN32
    void (* OldSIGPIPEHandler)(int);
#endif
    int rc = 0;
    size_t BytesSent = 0;

    fd_set wfds;
    struct timeval tv = {};

    auto io_data = reinterpret_cast<IoData*>(iohandle);

    update_select_params(*io_data->client_socket, &wfds, &tv);

    errno = 0;

    while (select(*io_data->client_socket + 1, nullptr, &wfds, nullptr, &tv) <= 0) {

        if (errno == ECONNRESET || errno == ENETUNREACH || errno == ECONNREFUSED) {
            if (errno == ECONNRESET) {
                UDA_LOG(UDA_LOG_DEBUG, "ECONNRESET error!\n");
                addIdamError(UDA_CODE_ERROR_TYPE, __func__, -2,
                             "ECONNRESET: The server program has crashed or closed the socket unexpectedly");
                return -2;
            } else {
                if (errno == ENETUNREACH) {
                    UDA_LOG(UDA_LOG_DEBUG, "ENETUNREACH error!\n");
                    addIdamError(UDA_CODE_ERROR_TYPE, __func__, -3, "Server Unavailable: ENETUNREACH");
                    return -3;
                } else {
                    UDA_LOG(UDA_LOG_DEBUG, "ECONNREFUSED error!\n");
                    addIdamError(UDA_CODE_ERROR_TYPE, __func__, -4, "Server Unavailable: ECONNREFUSED");
                    return -4;
                }
            }
        }

        update_select_params(*io_data->client_socket, &wfds, &tv);
    }

    /* UNIX version

     Ignore the SIG_PIPE signal.  Standard behaviour terminates
     the application with an error code of 141.  When the signal
     is ignored, write calls (when there is no server process to
     communicate with) will return with errno set to EPIPE
    */

#ifndef _WIN32
    if ((OldSIGPIPEHandler = signal(SIGPIPE, SIG_IGN)) == SIG_ERR) {
        addIdamError(UDA_CODE_ERROR_TYPE, __func__, -1, "Error attempting to ignore SIG_PIPE");
        return -1;
    }
#endif

    // Write to socket, checking for EINTR, as happens if called from IDL

    while (BytesSent < (unsigned int)count) {
#ifndef _WIN32
        while (((rc = (int)write(*io_data->client_socket, buf, count)) == -1) && (errno == EINTR)) {}
#else
        while (((rc = send(*io_data->client_socket, buf , count, 0)) == SOCKET_ERROR) && (errno == EINTR)) {}
#endif
        BytesSent += rc;
        buf += rc;
    }

// Restore the original SIGPIPE handler set by the application

#ifndef _WIN32
    if (signal(SIGPIPE, OldSIGPIPEHandler) == SIG_ERR) {
        addIdamError(UDA_CODE_ERROR_TYPE, __func__, -1, "Error attempting to restore SIG_PIPE handler");
        return -1;
    }
#endif

    return rc;
}

int uda::client::readin(void* iohandle, char* buf, int count)
{
    int rc;
    fd_set rfds;
    struct timeval tv = {};

    int maxloop = 0;

    auto io_data = reinterpret_cast<IoData*>(iohandle);

    errno = 0;

    /* Wait till it is possible to read from socket */

    update_select_params(*io_data->client_socket, &rfds, &tv);

    while ((select(*io_data->client_socket + 1, &rfds, nullptr, nullptr, &tv) <= 0) && maxloop++ < MAXLOOP) {
        update_select_params(*io_data->client_socket, &rfds, &tv);        // Keep trying ...
    }

    // Read from it, checking for EINTR, as happens if called from IDL

#ifndef _WIN32
    while (((rc = (int)read(*io_data->client_socket, buf, count)) == -1) && (errno == EINTR)) {}
#else
    while (((rc=recv(*io_data->client_socket, buf, count, 0)) == SOCKET_ERROR) && (errno == EINTR)) {}
#endif

    // As we have waited to be told that there is data to be read, if nothing
    // arrives, then there must be an error

    if (!rc) {
        rc = -1;
        if (errno != 0 && errno != EINTR) {
            addIdamError(UDA_SYSTEM_ERROR_TYPE, __func__, rc, "");
        }
        addIdamError(UDA_CODE_ERROR_TYPE, __func__, rc, "No Data waiting at Socket when Data Expected!");
    }

    return rc;
}
