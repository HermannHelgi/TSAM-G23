all: client tsamgroup23 serversim

client: client.cpp
	g++ -std=c++11 client.cpp -o $@

tsamgroup23: tsamgroup23.cpp server.cpp
	g++ -std=c++11 tsamgroup23.cpp server.cpp -o $@

server: server.cpp
	g++ -std=c++11 server.cpp -o $@

serversim: serversim.cpp
	g++ -std=c++11 serversim.cpp -o $@

clean:
	rm -f client tsamgroup23 server serversim