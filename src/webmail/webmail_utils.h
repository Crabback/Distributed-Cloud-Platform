#pragma once

#define panic(a...) do { fprintf(stderr, a); fprintf(stderr, "\n"); exit(1); } while (0) 

using namespace std;

// For each connection we keep a) its file descriptor, and b) a buffer that contains
// any data we have read from the connection but not yet processed. This is necessary
// because sometimes the server might send more bytes than we immediately expect.

struct connection {
  int fd;
  char *buf;
  int bytesInBuffer;
  int bufferSizeBytes;
};

void send_to_webmail(string recipient_address, string sender_address, string message);
void send_to_remote(string recipient, string sender, string message);
void log(const char *prefix, const char *data, int len, const char *suffix);
void writeString(struct connection *conn, const char *data);
void expectNoMoreData(struct connection *conn);
void connectToPort(struct connection *conn, int portno, const char *ip);
void expectToRead(struct connection *conn, const char *data);
void expectRemoteClose(struct connection *conn);
void initializeBuffers(struct connection *conn, int bufferSizeBytes);
void closeConnection(struct connection *conn);
void freeBuffers(struct connection *conn);