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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/file.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "src/kv/client.h"
#include "webmail_utils.h"

using namespace std;

const char *GREET_MSG = "220 localhost server ready (Author: Zhengjia Mao / zmao)\r\n";
const char *NEW_CON_MSG = "New connection\r\n";
const char *HELO_MSG = "250 localhost\r\n";
const char *MAIL_MSG = "OK FROM ";
const char *RCPT_MSG = "OK TO ";
const char *BYE_MSG = "221 Goodbye!\r\n";
const char *DATA_START_MSG = "354 Start data input\r\n";
const char *CLOSE_CON_MSG = "Connection closed\r\n";
const char *BAD_MSG = "503 Bad sequence of commands\r\n";
const char *ERR_MSG = "500 syntax error, command unrecognized\r\n";
const char *DOWN_MSG = "505 Server shutting down\r\n";
const char *NOT_FOUND_MSG = "550 Not found\r\n";
const char *OK_MSG = "250 OK\r\n";

const int MAX_CONNECTIONS = 100;
const int MAX_LENGTH = 1000;
const int DEFAULT_PORT = 2500;

// global variables, shared by threads
vector<pthread_t> THREADS;
vector<int> SOCKETS;
char *PARENT_DIR;
int FLAG_DEBUG = 0;

void signal_handler(int signo);
string compute_uid(string message);
void *worker(void *arg);

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);
    int c;
    int port = DEFAULT_PORT;

    // parse options
    while ((c = getopt(argc, argv, "p:av")) != -1) {
        switch (c) {
        case 'p':
            port = atoi(optarg);
            break;
        case 'a':
            cerr << "Zhengjia Mao / zmao" << endl;
            exit(EXIT_FAILURE);
        case 'v':
            FLAG_DEBUG = 1;
            break;
        case '?':
            cerr << "unknown command" << endl;
            exit(EXIT_FAILURE);
        default:
            cerr << "port number" << endl;
            abort();
        }
    }

    int server_fd;
    int opt = 1;
    struct sockaddr_in server_address;

    if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        cerr << "socket open fails" << endl;
        exit(EXIT_FAILURE);
    }

    SOCKETS.push_back(server_fd);

    // attach socket the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        cerr << "set socket fails" << endl;
        exit(EXIT_FAILURE);
    }

    // set up the server
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htons(INADDR_ANY);
    server_address.sin_port = htons(port);

    if (::bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        cerr << "bind server fails" << endl;
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        cerr << "listen fails" << endl;
        exit(EXIT_FAILURE);
    }

    while (1) {

        int *client_fd = (int *)malloc(sizeof(int));
        pthread_t thread;
        struct sockaddr_in client_address;
        socklen_t client_addrlen = sizeof(client_address);

        // set up the client
        if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_addrlen)) < 0) {
            cerr << "accept fails" << endl;
            exit(EXIT_FAILURE);
        }

        SOCKETS.push_back(*client_fd);
        pthread_create(&thread, NULL, &worker, client_fd);
        THREADS.push_back(thread);
    }

    for (int i = 0; i < (int)THREADS.size(); i++) {
        pthread_join(THREADS[i], NULL);
    }
    return 0;
}

void signal_handler(int signo) {
    cout << "\r\n" << DOWN_MSG;
    free(PARENT_DIR);

    for (int i = 0; i < (int)SOCKETS.size(); i++) {
        if (i != 0) {
            write(SOCKETS[i], DOWN_MSG, strlen(DOWN_MSG));
        }
        close(SOCKETS[i]);
        if (i != 0) {
            pthread_cancel(THREADS[i - 1]);
            pthread_join(THREADS[i - 1], NULL);
        }
    }

    close(SOCKETS[0]);
    exit(EXIT_SUCCESS);
}

void *worker(void *arg) {
    KVClient client("127.0.0.1:5000");

    /*
    0 = initial state
    1 = HELO
    2 = MAIL FROM
    3 = RCPT TO
    4 = DATA start
    5 = DATA finish
    6 = QUIT
    */
    int state = 0;
    int socket_fd = *(int *)arg;
    char buffer[MAX_LENGTH];
    string sender;
    string sender_host;
    string recipt;
    string recipt_host;
    string content;
    vector<string> local_recipts;
    vector<string> nonlocal_recipts;
    string command_buffer;
    int flag_QUIT = 0;

    write(socket_fd, GREET_MSG, strlen(GREET_MSG));

    if (FLAG_DEBUG == 1) {
        cout << "[" << socket_fd << "] " << NEW_CON_MSG;
        cout << "[" << socket_fd << "] " << GREET_MSG;
    }
    while (1) {
        int bytes_read = read(socket_fd, buffer, MAX_LENGTH);
        if (bytes_read <= 0)
            break;

        command_buffer.append(buffer, bytes_read);
        size_t end = command_buffer.find("\r\n");

        while (end != string::npos) {

            // parse the command
            string cmd = command_buffer.substr(0, end);
            string action = command_buffer.substr(0, end);
            string res;

            if (cmd.length() > MAX_LENGTH) {
                write(socket_fd, ERR_MSG, strlen(ERR_MSG));
                continue;
            }

            transform(action.begin(), action.end(), action.begin(), ::tolower);

            /* HELO Command */
            if (action.find("helo") == 0 && state != 4) {
                if (cmd.length() <= 4) {
                    res = ERR_MSG;
                } else if (state > 1) {
                    res = BAD_MSG;
                } else {
                    res = HELO_MSG;
                    state = 1;
                }

                /* MAIL Command */
            } else if (action.find("mail from") == 0 && state != 4) {
                if (cmd.length() <= 10 || cmd.find("@") == string::npos) {
                    res = ERR_MSG;
                } else if (state != 1) {
                    res = BAD_MSG;
                } else {
                    // parse the host and sender
                    sender_host = cmd.substr(cmd.find("@") + 1, cmd.find(">") - cmd.find("@") - 1);
                    sender = cmd.substr(cmd.find("<") + 1, cmd.find("@") - cmd.find("<") - 1);
                    // if (host != "localhost") {
                    //     res = NOT_FOUND_MSG;
                    // } else {
                    res = OK_MSG;
                    if (FLAG_DEBUG == 1)
                        cout << MAIL_MSG + string(sender) << endl;
                    state = 2;
                    // }
                }

                /* RCPT Command */
            } else if (action.find("rcpt to") == 0 && state != 4) {
                if (cmd.length() <= 8 || cmd.find("@") == string::npos) {
                    res = ERR_MSG;
                } else if (!(state == 2 or state == 3)) {
                    res = BAD_MSG;
                } else {
                    // parse the host and recipient
                    recipt_host = cmd.substr(cmd.find("@") + 1, cmd.find(">") - cmd.find("@") - 1);
                    recipt = cmd.substr(cmd.find("<") + 1, cmd.find("@") - cmd.find("<") - 1);
                    if (FLAG_DEBUG == 1)
                        cout << RCPT_MSG + recipt + "@" + recipt_host << endl;

                    // if nonlocal host, save the rcpts and send them out
                    if (recipt_host != "localhost") {
                        string recipt_address = recipt + "@" + recipt_host;
                        nonlocal_recipts.push_back(recipt_address);
                        res = OK_MSG;
                    }

                    // if local host, save the rcpts and store the message to the mbox
                    else {
                        local_recipts.push_back(recipt);
                        res = OK_MSG;
                    }
                    state = 3;
                }

                /* DATA Command */
            } else if (action.find("data") == 0 || state == 4) {
                if (state == 3) {
                    res = DATA_START_MSG;
                    state = 4;
                } else if (state == 4) {

                    // conclude and add the data into the mailbox
                    if (action == ".") {

                        // store the message to the local mailboxes
                        for (int i = 0; i < (int)local_recipts.size(); i++) {
                            chrono::system_clock::time_point now = chrono::system_clock::now();
                            time_t timestamp = chrono::system_clock::to_time_t(now);
                            string header_str = "Date: " + string(ctime(&timestamp));
                            string uid = compute_uid(header_str + content);

                            // put(recipient, uid) = email
                            try {
                                client.put(local_recipts[i], uid, header_str + content);
                                cout << "PUT recipient=" << local_recipts[i] << endl;
                            } catch (KVError &err) {
                                cout << "PUT RPC failed: " << err.what() << endl;
                            }
                            // put(recipient, inbox) = uid_list
                            while (1) {
                                try {
                                    string uid_list_old = client.getdefault(local_recipts[i], "inbox", "");
                                    string uid_list_new;
                                    if (uid_list_old == "") {
                                        uid_list_new = uid;
                                    } else {
                                        uid_list_new = uid_list_old + "," + uid;
                                    }
                                    client.cput(local_recipts[i], "inbox", uid_list_old, uid_list_new);
                                    cout << "CPUT recipient=" << local_recipts[i] << endl;
                                } catch (KVNoMatch &err) {
                                    cout << "CPUT RPC race conditions: " << err.what() << endl;
                                    continue;
                                } catch (KVError &err) {
                                    cout << "CPUT RPC failed: " << err.what() << endl;
                                }
                                break;
                            }

                            // put(sender, sent) = uid_list
                            while (1) {
                                try {
                                    string uid_list_old = client.getdefault(sender, "sent", "");
                                    string uid_list_new;
                                    if (uid_list_old == "") {
                                        uid_list_new = uid;
                                    } else {
                                        uid_list_new = uid_list_old + "," + uid;
                                    }
                                    client.cput(sender, "sent", uid_list_old, uid_list_new);
                                    cout << "CPUT recipient=" << local_recipts[i] << endl;
                                } catch (KVNoMatch &err) {
                                    cout << "CPUT RPC race conditions: " << err.what() << endl;
                                    continue;
                                } catch (KVError &err) {
                                    cout << "CPUT RPC failed: " << err.what() << endl;
                                }
                                break;
                            }
                            // put(sender, uid) = email
                            try {
                                client.put(sender, uid, header_str + content);
                                cout << "PUT recipient=" << sender << endl;
                            } catch (KVError &err) {
                                cout << "PUT RPC failed: " << err.what() << endl;
                            }
                        }

                        // send the message to the remote mailboxes
                        for (int i = 0; i < (int)nonlocal_recipts.size(); i++) {
                            chrono::system_clock::time_point now = chrono::system_clock::now();
                            time_t timestamp = chrono::system_clock::to_time_t(now);
                            string sender_address = sender + "@" + sender_host;
                            string header_str = "Date: " + string(ctime(&timestamp));
                            string message = header_str + content;
                            string uid = compute_uid(header_str + content);
                            send_to_remote(nonlocal_recipts[i], sender_address, message);
                            // put(user, sent) = uid_list
                            while (1) {
                                try {
                                    string uid_list_old = client.getdefault(sender, "sent", "");
                                    string uid_list_new;
                                    if (uid_list_old == "") {
                                        uid_list_new = uid;
                                    } else {
                                        uid_list_new = uid_list_old + "," + uid;
                                    }
                                    client.cput(sender, "sent", uid_list_old, uid_list_new);
                                    cout << "CPUT recipient=" << sender << endl;
                                } catch (KVNoMatch &err) {
                                    cout << "CPUT RPC race conditions: " << err.what() << endl;
                                    continue;
                                } catch (KVError &err) {
                                    cout << "CPUT RPC failed: " << err.what() << endl;
                                }
                                break;
                            }
                            
                            try {
                                client.put(sender, uid, header_str + content);
                                cout << "PUT recipient=" << sender << endl;
                            } catch (KVError &err) {
                                cout << "PUT RPC failed: " << err.what() << endl;
                            }
                        }

                        sender.clear();
                        local_recipts.clear();
                        nonlocal_recipts.clear();
                        content.clear();
                        res = OK_MSG;
                        state = 1;

                    } else { // keep writing
                        content += cmd + "\n";
                    }
                    // } else if (state == 5) {
                    //     res = OK_MSG;
                    //     state = 1;
                } else {
                    res = BAD_MSG;
                }
                /* QUIT Command */
            } else if (action.find("quit") == 0 && state != 4) {
                flag_QUIT = 1;
                res = BYE_MSG;

                /* RSET Command */
            } else if (action.find("rset") == 0 && state != 4) {
                if (state < 1)
                    res = BAD_MSG;
                else {
                    sender.clear();
                    local_recipts.clear();
                    nonlocal_recipts.clear();
                    res = OK_MSG;
                    state = 1;
                }

                /* NOOP Command */
            } else if (action.find("noop") == 0 && state != 4) {
                if (state < 1)
                    res = BAD_MSG;
                else
                    res = OK_MSG;

                /* All other commands */
            } else {
                res = ERR_MSG;
            }

            write(socket_fd, res.c_str(), res.length());

            // clear buffer and reset the line_end
            command_buffer.erase(0, end + 2);
            end = command_buffer.find("\r\n");
            if (res == "") {
                res = "\r\n";
            }
            if (FLAG_DEBUG == 1) {
                cout << "[" << socket_fd << "] C: " << cmd << "\r\n";
                if (flag_QUIT == 1)
                    break;
                cout << "[" << socket_fd << "] S: " << res;
            }
        }

        if (flag_QUIT == 1) {
            cout << "[" << socket_fd << "] S: " << BYE_MSG;
            break;
        }
    }

    if (FLAG_DEBUG == 1)
        cout << "[" << socket_fd << "] " << CLOSE_CON_MSG;

    close(socket_fd);
    pthread_exit(NULL);
}

string compute_uid(string message) {
    string uid;
    const char *test = message.c_str();
    int i;

    MD5_CTX md5;
    MD5_Init(&md5);
    MD5_Update(&md5, (const unsigned char *)test, message.length());
    unsigned char buffer_md5[16];
    MD5_Final(buffer_md5, &md5);
    char buf[32];
    for (i = 0; i < 16; i++) {
        sprintf(buf, "%02x", buffer_md5[i]);
        uid.append(buf);
    }

    return uid;
}
