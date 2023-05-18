#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <map>
#include <sstream>

using namespace std;

//store a count for counting clients, and redirects to server with least clients
std::map<uint16_t, int> frontend_ports{{8080,0},{8081,0},{8082,0}};
std::map<uint16_t, bool> frontend_status;
std::mutex mtx;

vector<string> split(const string &str, char delimiter) {
    vector<string> tokens;
    istringstream ss(str);
    string token;

    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

uint16_t get_next_frontend_port() {
  std::unique_lock<std::mutex> lock(mtx);
  uint16_t min_port = 0;
  int min_count = 10000;

  for(const auto &entry : frontend_ports) {
    uint16_t port = entry.first;
    int count = entry.second;
    //check the port is alive, and client count is least
    if(frontend_status[port] && count < min_count) {
      min_count = count;
      min_port = port;
    }
  }

  frontend_ports[min_port]++;
  lock.unlock();
  return min_port;
}



bool is_frontend_alive(uint16_t port) {
 int sock = socket(AF_INET, SOCK_STREAM, 0);
 if (sock < 0) {
   return false;
 }

 sockaddr_in addr;
 std::memset(&addr, 0, sizeof(addr));
 addr.sin_family = AF_INET;
 addr.sin_port = htons(port);
 inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);


 int result = connect(sock, (sockaddr*)&addr, sizeof(addr));
 close(sock);
 return result == 0;
}


void *update_frontend_servers(void *) {
  while(true) {
    std::unique_lock<std::mutex> lock(mtx);
    for(const auto &entry : frontend_ports) {
      uint16_t port = entry.first;
      frontend_status[port] = is_frontend_alive(port);
    }
    lock.unlock();
    sleep(5);
  }
}

void *handle_client(void *client_socket_ptr) {
 int client_socket = *((int *)client_socket_ptr);
 try {
      uint16_t frontend_port = get_next_frontend_port();
      // parse url (TODO)
      // Read request from the client
	  char buffer[1024] = {0};
	  ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
	  if (bytes_read <= 0) {
		  perror("recv");
		  close(client_socket);
		  return nullptr;
	  }
	  string raw_request(buffer, bytes_read);

	  stringstream ss(raw_request);
	  string request_line = "";
	  string raw_headers = "";
	  getline(ss, request_line);
      // Parse the request line
	  vector<string> request_parts = split(request_line, ' ');
//	  string method = request_parts[0];
	  string url = request_parts[1];
//	  string version = request_parts[2];

	  // Find the position of the third occurrence of "/"
	  size_t pos = url.find("/", url.find("/", url.find("/") + 1) + 1);

	  // Get the substring after the third "/"
	  string path = url.substr(pos);

      // Prepare an HTTP redirect response
      std::string redirect_response = "HTTP/1.1 302 Found\r\n";
      redirect_response += "Location: http://127.0.0.1:" + std::to_string(frontend_port) + path + "\r\n";
      redirect_response += "Connection: close\r\n";
      redirect_response += "\r\n";

      // Send the redirect response to the client
      ssize_t bytes_sent = send(client_socket, redirect_response.c_str(), redirect_response.size(), 0);
      if (bytes_sent < 0) {
          std::cerr << "Failed to send redirect response to the client" << std::endl;
      }

      close(client_socket);
  } catch (std::exception& e) {
      std::cerr << "Exception in handle_client: " << e.what() << std::endl;
  }

}


int main(int argc, char* argv[]) {
 uint16_t port = static_cast<uint16_t>(8090);

 int server_socket = socket(AF_INET, SOCK_STREAM, 0);
 if (server_socket < 0) {
   std::cerr << "Failed to create server socket" << std::endl;
   return 1;
 }


 sockaddr_in server_address;
 std::memset(&server_address, 0, sizeof(server_address));
 server_address.sin_family = AF_INET;
 server_address.sin_port = htons(port);
 server_address.sin_addr.s_addr = htonl(INADDR_ANY);


 if (::bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
   std::cerr << "Failed to bind server socket" << std::endl;
   close(server_socket);
   return 1;
 }


 if (listen(server_socket, 10) < 0) {
   std::cerr << "Failed to listen on server socket" << std::endl;
   close(server_socket);
   return 1;
 }


  pthread_t update_frontend_servers_thread;
  pthread_create(&update_frontend_servers_thread, nullptr, (void *(*)(void *))update_frontend_servers, nullptr);
  pthread_detach(update_frontend_servers_thread);

  while (true) {
      int client_socket = accept(server_socket, nullptr, nullptr);
      if (client_socket < 0) {
          std::cerr << "Failed to accept client connection" << std::endl;
      } else {
          pthread_t client_thread;
          pthread_create(&client_thread, nullptr, (void *(*)(void *))handle_client, (void *)&client_socket);
          pthread_detach(client_thread);
      }
  }

  close(server_socket);
  return 0;
}

