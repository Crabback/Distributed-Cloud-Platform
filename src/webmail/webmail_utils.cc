#include <algorithm>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <resolv.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/file.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "webmail_utils.h"

using namespace std;

void send_to_webmail(string recipient_address, string sender_address, string message) {
    string input = message;
    string dns;
    string src_cmd;
    string des_cmd;
    string host;
    char ip[INET_ADDRSTRLEN];

    struct connection conn1;

    host = recipient_address.substr(recipient_address.find("@") + 1, recipient_address.find(">") - recipient_address.find("@") - 1);
    src_cmd = "MAIL FROM: <" + sender_address + ">\r\n";
    des_cmd = "RCPT TO: <" + recipient_address + ">\r\n";
    message = "From: <" + sender_address + ">\r\n" + "To: <" + recipient_address + ">\r\n" + message;

    printf("source : %s\n", sender_address.c_str());
    printf("destination : %s\n", recipient_address.c_str());
    printf("Domain : %s\n", host.c_str());

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        cerr << "Error creating socket." << endl;
        return;
    }

    // Set up the server address and port
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(2500);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Error connecting to the server." << endl;
        return;
    }

    // Send a command to the server
    const char *command = "HELO Tester\r\n";
    ssize_t bytes_sent = send(sockfd, command, strlen(command), 0);
    if (bytes_sent < 0) {
        cerr << "Error sending command to the server." << endl;
        return;
    }
    send(sockfd, src_cmd.c_str(), strlen(src_cmd.c_str()), 0);
    send(sockfd, des_cmd.c_str(), strlen(des_cmd.c_str()), 0);
    command = "DATA\r\n";
    send(sockfd, command, strlen(command), 0);

    size_t pos = 0;
    while ((pos = message.find("\\n", pos)) != string::npos) {
        message.replace(pos, 2, "\r\n");
        pos += 2;
    }
    message += "\r\n";
    send(sockfd, message.c_str(), message.length(), 0);

    command = ".\r\n";
    send(sockfd, command, strlen(command), 0);

    // Close the socket
    close(sockfd);
    return;
}
void send_to_remote(string recipient_address, string sender_address, string message) {
    string input = message;
    string dns;
    string src_cmd;
    string des_cmd;
    string host;
    char ip[INET_ADDRSTRLEN];

    struct connection conn1;

    cout << "debug: " << message << endl;

    host = recipient_address.substr(recipient_address.find("@") + 1, recipient_address.find(">") - recipient_address.find("@") - 1);
    src_cmd = "MAIL FROM: <" + sender_address + ">\r\n";
    des_cmd = "RCPT TO: <" + recipient_address + ">\r\n";
    message = "From: <" + sender_address + ">\r\n" + "To: <" + recipient_address + ">\r\n" + message;

    printf("source : %s\n", sender_address.c_str());
    printf("destination : %s\n", recipient_address.c_str());
    printf("Domain : %s\n", host.c_str());

    ns_msg msg;
    ns_rr rr;
    int l;
    int buf_size = 4096;
    u_char nsbuf[buf_size];
    char dispbuf[buf_size];

    // Get the MX records
    l = res_query(host.c_str(), ns_c_in, ns_t_mx, nsbuf, sizeof(nsbuf));
    if (l < 0) {
        cerr << "res_query fails." << endl;
        return;
    }
    ns_initparse(nsbuf, l, &msg);
    l = ns_msg_count(msg, ns_s_an);

    // get the first entry from MX records
    ns_parserr(&msg, ns_s_an, 0, &rr);
    ns_sprintrr(&msg, &rr, NULL, NULL, dispbuf, sizeof(dispbuf));
    printf("\t%s \n", dispbuf);

    // trim spaces to get the DNS
    char *copy = strdup(dispbuf);
    char *split = strtok(copy, " ");
    int j = 0;
    while (split != NULL) {
        j++;
        if (j == 4) {
            dns = string(split);
            break;
        }
        split = strtok(NULL, " ");
    }
    free(copy);
    cout << "DNS: " << dns << endl;
    // Get the IP address
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result;
    int error = getaddrinfo(dns.c_str(), NULL, &hints, &result);
    if (error) {
        cerr << "Error getting IP address for " << host << ": " << gai_strerror(error) << endl;
        return;
    }

    inet_ntop(AF_INET, &((struct sockaddr_in *)result->ai_addr)->sin_addr, ip, sizeof(ip));
    cout << host << ": " << ip << endl;

    initializeBuffers(&conn1, 5000);

    // Open a connection with the IP (global variable);
    connectToPort(&conn1, 25, ip);
    expectToRead(&conn1, "220 *");

    // Communicate with the remote SMTP server
    writeString(&conn1, "HELO tester\r\n");
    expectToRead(&conn1, "250 *");

    writeString(&conn1, src_cmd.c_str());
    expectToRead(&conn1, "250 OK");

    writeString(&conn1, des_cmd.c_str());
    expectToRead(&conn1, "250 OK");

    writeString(&conn1, "DATA\r\n");
    expectToRead(&conn1, "354 *");

    string line;
    size_t pos = 0;
    while ((pos = input.find('\n')) != string::npos) {
        line = input.substr(0, pos);
        writeString(&conn1, (line + "\r\n").c_str());
        input.erase(0, pos + 1);
    }
    writeString(&conn1, ".\r\n");
    expectToRead(&conn1, "250 OK");
}

// All the functions below are from common.cc provided by the class (credits to Prof. Phan)
void log(const char *prefix, const char *data, int len, const char *suffix) {
    printf("%s", prefix);
    for (int i = 0; i < len; i++) {
        if (data[i] == '\n')
            printf("<LF>");
        else if (data[i] == '\r')
            printf("<CR>");
        else if (isprint(data[i]))
            printf("%c", data[i]);
        else
            printf("<0x%02X>", (unsigned int)(unsigned char)data[i]);
    }
    printf("%s", suffix);
}

// This function writes a string to a connection. If a LF is required,
// it must be part of the 'data' argument. (The idea is that we might
// sometimes want to send 'partial' lines to see how the server handles these.)
void writeString(struct connection *conn, const char *data) {
    int len = strlen(data);
    log("C: ", data, len, "\n");
    int wptr = 0;
    while (wptr < len) {
        int w = write(conn->fd, &data[wptr], len - wptr);
        if (w < 0)
            panic("Cannot write to conncetion (%s)", strerror(errno));
        if (w == 0)
            panic("Connection closed unexpectedly");
        wptr += w;
    }
}

// This function verifies that the server has sent us more data at this point.
// It does this by temporarily putting the socket into nonblocking mode and then
// attempting a read, which (if there is no data) should return EAGAIN.
// Note that some of the server's data might still be 'in flight', so it is best
// to call this only after a certain delay.

void expectNoMoreData(struct connection *conn) {
    int flags = fcntl(conn->fd, F_GETFL, 0);
    fcntl(conn->fd, F_SETFL, flags | O_NONBLOCK);
    int r = read(conn->fd, &conn->buf[conn->bytesInBuffer], conn->bufferSizeBytes - conn->bytesInBuffer);
    if ((r < 0) && (errno != EAGAIN))
        panic("Read from connection failed (%s)", strerror(errno));
    if (r > 0)
        conn->bytesInBuffer += r;
    if (conn->bytesInBuffer > 0) {
        log("S: ", conn->buf, conn->bytesInBuffer, " [unexpected; server should not have sent anything!]\n");
        conn->bytesInBuffer = 0;
    }
    fcntl(conn->fd, F_SETFL, flags);
}

// Attempts to connect to a port
void connectToPort(struct connection *conn, int portno, const char *ip) {
    conn->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (conn->fd < 0)
        panic("Cannot open socket (%s)", strerror(errno));
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(portno);
    inet_pton(AF_INET, ip, &(servaddr.sin_addr));
    if (connect(conn->fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        panic("Cannot connect to IP: (%s)", strerror(errno));
    conn->bytesInBuffer = 0;
}

// Reads a line of text from the server (until it sees a LF) and then compares
// the line to the argument. The argument should not end with a LF; the function
// strips off any LF or CRLF from the incoming data before doing the comparison.
// (This is to avoid assumptions about whether the server terminates its lines
// with a CRLF or with a LF.)
void expectToRead(struct connection *conn, const char *data) {
    // Keep reading until we see a LF
    int lfpos = -1;
    while (true) {
        for (int i = 0; i < conn->bytesInBuffer; i++) {
            if (conn->buf[i] == '\n') {
                lfpos = i;
                break;
            }
        }
        if (lfpos >= 0)
            break;
        if (conn->bytesInBuffer >= conn->bufferSizeBytes)
            panic("Read %d bytes, but no CRLF found", conn->bufferSizeBytes);
        int bytesRead = read(conn->fd, &conn->buf[conn->bytesInBuffer], conn->bufferSizeBytes - conn->bytesInBuffer);
        if (bytesRead < 0)
            panic("Read failed (%s)", strerror(errno));
        if (bytesRead == 0)
            panic("Connection closed unexpectedly");
        conn->bytesInBuffer += bytesRead;
    }
    log("S: ", conn->buf, lfpos + 1, "");
    // Get rid of the LF (or, if it is preceded by a CR, of both the CR and the LF)
    bool crMissing = false;
    if ((lfpos == 0) || (conn->buf[lfpos - 1] != '\r')) {
        crMissing = true;
        conn->buf[lfpos] = 0;
    } else {
        conn->buf[lfpos - 1] = 0;
    }
    // Check whether the server's actual response matches the expected response
    // Note: The expected response might end in a wildcard (*) in which case
    // the rest of the server's line is ignored.
    int argptr = 0, bufptr = 0;
    bool match = true;
    while (match && data[argptr]) {
        if (data[argptr] == '*')
            break;
        if (data[argptr++] != conn->buf[bufptr++])
            match = false;
    }
    if (!data[argptr] && conn->buf[bufptr])
        match = false;
    // Annotate the output to indicate whether the response matched the expectation.
    if (match) {
        if (crMissing)
            printf(" [Terminated by LF, not by CRLF]\n");
        else
            printf(" [OK]\n");
    } else {
        log(" [Expected: '", data, strlen(data), "']\n");
    }
    // 'Eat' the line we just parsed. However, keep in mind that there might still be
    // more bytes in the buffer (e.g., another line, or a part of one), so we have to
    // copy the rest of the buffer up.
    for (int i = lfpos + 1; i < conn->bytesInBuffer; i++)
        conn->buf[i - (lfpos + 1)] = conn->buf[i];
    conn->bytesInBuffer -= (lfpos + 1);
}

// This function verifies that the remote end has closed the connection.

void expectRemoteClose(struct connection *conn) {
    int r = read(conn->fd, &conn->buf[conn->bytesInBuffer], conn->bufferSizeBytes - conn->bytesInBuffer);
    if (r < 0)
        panic("Read failed (%s)", strerror(errno));
    if (r > 0) {
        log("S: ", conn->buf, r + conn->bytesInBuffer, " [unexpected; server should have closed the connection]\n");
        conn->bytesInBuffer = 0;
    }
}

// This function initializes the read buffer

void initializeBuffers(struct connection *conn, int bufferSizeBytes) {
    conn->fd = -1;
    conn->bufferSizeBytes = bufferSizeBytes;
    conn->bytesInBuffer = 0;
    conn->buf = (char *)malloc(bufferSizeBytes);
    if (!conn->buf)
        panic("Cannot allocate %d bytes for buffer", bufferSizeBytes);
}

// This function closes our local end of a connection

void closeConnection(struct connection *conn) { close(conn->fd); }

// This function frees the allocated read buffer
void freeBuffers(struct connection *conn) {
    free(conn->buf);
    conn->buf = NULL;
}