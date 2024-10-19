all: client tsamgroup23

scanner: client.cpp
	g++ -std=c++11 client.cpp -o $@

puzzlesolver: tsamgroup23.cpp
	g++ -std=c++11 tsamgroup23.cpp -o $@

clean:
	rm -f client tsamgroup23
