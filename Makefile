all:
	g++ -std=c++11 ServerA.cpp -o A
	g++ -std=c++11 ServerB.cpp -o B
	g++ -std=c++11 Mainserver.cpp -o Mainserver
	g++ client.cpp -o client

serverA:
	./A
serverB:
	./B
mainserver:
	./Mainserver


