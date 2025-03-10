all: lora_communication

LoRa.o: LoRa.c
	gcc -c LoRa.c -o LoRa.o -lpigpio -lrt -pthread -lm

lora_communication: LoRa.o lora_communication.o
	g++ -o old_lora_communication LoRa.c LoRa.h old_lora_communication.cpp -lpigpio -lrt -pthread -lm

lora_communication_server: 
	g++ -o lora_communication_server mongoose.h mongoose.c LoRa.c LoRa.h lora_communication_server.cpp -lpigpio -lrt -pthread -lm

server-test: server_test.cpp
	g++ -o server_test mongoose.h mongoose.c json.hpp server_test.cpp  -lrt -pthread -lm

server-test-start: server-test
	./server_test
clean:
	rm *.o
