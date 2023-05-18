How to Test Basic Frontend Server:

1. Open terminal and compile by executing: 
g++ frontend_server.cc request_handler.cc -o frontend_server -lpthread

2. Run the server:
./frontend_server

3.1 In another terminal and execute(this checks 404):
curl http://localhost:8080

3.2 In another terminal and execute(this checks hello):
curl http://localhost:8080/hello

Feel free to edit it when you implement other requests.

3.3 In another terminal and execute(this checks "post /login"):
curl -X POST -d "username=123&password=456" "http://localhost:8080/login"
test admin:
curl -X POST -d "serverType=frontend&action=shutdown&serverIP=127.0.0.1:8080" "http://localhost:8888/admin"