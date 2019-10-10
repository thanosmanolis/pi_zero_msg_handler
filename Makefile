CC = gcc

all:
	arm-linux-gnueabihf-gcc -pthread pi_msg_handler.c -o pi_msg_handler
