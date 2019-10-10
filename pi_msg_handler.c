#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define MAX 80
#define PORT 2288
#define NUM_THREADS 3
#define SA struct sockaddr
#define TRUE 1
#define FALSE 0
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define YELLOW_BOLD "\033[1;33m"
#define RESET_COLOR "\033[0m"

pthread_mutex_t lock;

//! Functions
void *server(void *threadid);
void *client(void *threadid);
void  send_list(int sockfd, int start_point);
void *message_generator(void *threadid);
void  check_msg_index(void);
void  alarmHandler(int sig);

//! Global Variables
char msg_list_[2000][256]; //data buffer
int  msg_index_ = 0;
uint32_t aem_ = 8856;
int  num_of_receivers_ = 4;
uint32_t receivers_[4] = {8794, 8808, 8884, 8828};
int start_point_[4] = {0, 0, 0, 0};
int duration_ = 7200;
time_t first_timestamp_;

/*
*************************************************************
*   Three threads. Server, client and a message generator   *
*************************************************************
*/

int main (int argc, char *argv[])
{
    first_timestamp_ = time(NULL);

    signal(SIGALRM, alarmHandler);
    alarm(duration_);

    printf(RESET_COLOR "***** STARTING PROCESS *****\n");

    pthread_t threads[NUM_THREADS];
    int rc_1, rc_2, rc_3;
    long t=0;

    if(pthread_mutex_init(&lock, NULL) !=0)
    {
        printf("\nMutex init failed.\n");
        return 1;
    }

    printf("In main: creating thread #%ld\n", t);
    rc_1 = pthread_create(&threads[t], NULL, server, (void *)t);

    if (rc_1)
    {
        printf("ERROR; return code from pthread_create() is %d\n", rc_1);
        exit(-1);
    }

    t=t+1;
    printf("In main: creating thread #%ld\n", t);
    rc_2 = pthread_create(&threads[t], NULL, client, (void *)t);

    if (rc_2)
    {
        printf("ERROR; return code from pthread_create() is %d\n", rc_2);
        exit(-1);
    }

    t=t+1;
    printf("In main: creating thread #%ld\n", t);
    rc_3 = pthread_create(&threads[t], NULL, message_generator, (void *)t);

    if (rc_3)
    {
        printf("ERROR; return code from pthread_create() is %d\n", rc_3);
        exit(-1);
    }

    //! Last thing that main() should do
    pthread_mutex_destroy(&lock);
    pthread_exit(NULL);
}

/*
************************************************************************
*   Other clients send their list to our server, and then our server   *
*   adds the messages to our msg_list_ (if they are not already in)    *
************************************************************************
*/

void *server(void *threadid)
{
	int opt = TRUE;
	int my_socket, addrlen, new_socket, sd;
	struct sockaddr_in address;
	char input[100];
	char welcome_msg[2] = "OK";
    char file1[100] = "my_messages.txt";
    char file2[100] = "list_messages.txt";
    FILE *fp1, *fp2;

	//! Set of socket descriptors
	fd_set readfds;

	//! Create a master socket
	if( (my_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
		perror("socket failed");

	//! Set master socket to allow multiple connections
	if( setsockopt(my_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
		perror("setsockopt");

	//! Type of socket created
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );

	//! Bind the socket to localhost port 2288
	if (bind(my_socket, (struct sockaddr *)&address, sizeof(address))<0)
		perror("bind failed");

	printf("Listener on port %d \n", PORT);

	//! Try to specify maximum of 3 pending connections for the master socket
	if (listen(my_socket, 3) < 0)
		perror("listen");

	//! Accept the incoming connection
	addrlen = sizeof(address);
	puts("Waiting for connections ... \n");

	while(TRUE)
	{
		//! Clear the socket set
		FD_ZERO(&readfds);

		//! Add master socket to set
		FD_SET(my_socket, &readfds);

		//! If something happened on the master socket,
		//! then it's an incoming connection
		if (FD_ISSET(my_socket, &readfds))
		{
			if ((new_socket = accept(my_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
				perror("accept");

			//! Inform user of socket number - used in send and receive commands
			printf(GREEN "\nNew connection detected:" RESET_COLOR " socket fd is %d , ip is : %s , port : %d\n", new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

			//! Send new connection the welcome message
			if( send(new_socket, welcome_msg, sizeof(welcome_msg), 0) < 0 )
				perror("send");

            pthread_mutex_lock(&lock);

            //! Store back the messages that came in one by one
            while( recv( new_socket , input, sizeof(input), 0) > 0 )
            {
				int flag = 0;
                for(int i=0; i<msg_index_; i++)
                    if(!strcmp(msg_list_[i], input))
                        flag = 1;

                if(!flag)
                {
                    char temp[5];
                	for(int i=5;i<9;i++)
                		temp[i-5] = input[i];
                	temp[4] = '\0';

                    char aem[5];
                    sprintf(aem, "%d", aem_);
                    if( strcmp(temp, aem) )
					{
                        check_msg_index();
                        strcpy(msg_list_[msg_index_++], input);

                        //! Open the file for writing the message
                        fp2 = fopen(file2, "a");
                        fprintf(fp2, "%s\n", input);

                        //! Close the file
                        fclose(fp2);
					}else
                    {
                        //! Open the file for writing the message
                        fp1 = fopen(file1, "a");
                        fprintf(fp1, "%s\n", input);

                        //! Close the file
                        fclose(fp1);
                    }
                }
            }

            for(int i=0; i<msg_index_; i++)
                printf("%s\n", msg_list_[i]);

            printf(YELLOW_BOLD "[Index: %d]\n" RESET_COLOR, msg_index_);

            pthread_mutex_unlock(&lock);

            //! Client disconnected, get his details and print
            getpeername(sd , (struct sockaddr*)&address ,  (socklen_t*)&addrlen);
            printf(RED "Host disconnected:" RESET_COLOR " ip %s , port %d \n\n",inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
        }
    }

}

/*
**************************************************************************
*   Our client continuously checks if the given IP addresses are into    *
*   range. When one is detected, elements of msg_list_ are sent to it,   *
*   one by one.                                                          *
**************************************************************************
*/

void *client(void *threadid)
{
	char addresses[4][50] = {"10.0.88.84", "10.0.87.94", "10.0.88.8", "10.0.88.28"};
    int addresses_index = 0;
    int sockfd;
    char welcome_msg[2];
    struct sockaddr_in servaddr;
    char file[100] = "connection_timestamps.txt";
    FILE *fp;

    //! Main client process
    while(TRUE)
    {
        //! Socket create and varification
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        	printf(RED "Error:" RESET_COLOR "socket creation failed...\n");

        bzero(&servaddr, sizeof(servaddr));

        //! Assign IP, PORT
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(addresses[addresses_index]);
        servaddr.sin_port = htons(PORT);

        //! Connect the client socket to server socket
        if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) < 0)
        	printf(RED "Error: " RESET_COLOR "connection with the server %s failed...\n", addresses[addresses_index]);

        pthread_mutex_lock(&lock);
        //! Function for chat
        if(recv(sockfd, welcome_msg, sizeof(welcome_msg), 0) < 0)
            perror(RED "Error:" RESET_COLOR);
        else if(!strcmp(welcome_msg, "OK"))
        {
            //! Open the file for writing the timestamp of each connection
            time_t timestamp = time(NULL);
            fp = fopen (file, "a");
            fprintf(fp, "%ld\n", timestamp - first_timestamp_);

            //! Close the file
            fclose(fp);

            send_list(sockfd, start_point_[addresses_index]);
            start_point_[addresses_index] = msg_index_;
        }

        pthread_mutex_unlock(&lock);

        if(addresses_index == num_of_receivers_ - 1)
            addresses_index = 0;
        else
            addresses_index++;

        usleep(0.5*1000000);

        //! Close the socket
        close(sockfd);
    }

}

/*
*******************************************************************************
*   The client sends the elements of msg_list_ to other servers, one by one   *
*******************************************************************************
*/

void send_list(int sockfd, int start_point)
{
	for (int i=start_point; i<msg_index_; i++)
	{
        if( send(sockfd, msg_list_[i], strlen(msg_list_[i]) + 1, 0) != (strlen(msg_list_[i]) + 1) )
            perror("send");
        usleep(0.2 * 1000000);
	}
}

/*
*****************************************************************************
*   A message with the format "Sender_Receiver_Timestamp_Text Message" is   *
*   generated, and added to msg_list_ if it isn't already there, every      *
*   1-5 minutes.                                                            *
*****************************************************************************
*/

void *message_generator(void *threadid)
{
    char file1[100] = "time_between_generated.txt";
    char file2[100] = "list_messages.txt";
    FILE *fp1, *fp2;

    while(TRUE)
    {
        char receiver[5];
        char msg[MAX];
        char phrases[5][50] = {"_Hello", "_Whats up", "_Tuna", "_Beer", "_Bacon"};
        uint64_t timestamp = time(NULL);
        uint64_t previous_timestamp;
    	char time[12];

        sprintf(receiver, "%d", receivers_[rand() % num_of_receivers_]);
        sprintf(time, "%ld", timestamp);

        sprintf(msg, "%d" , aem_);
        strcat(msg, "_");
        strcat(msg, receiver);
        strcat(msg, "_");
        strcat(msg, time);
        strcat(msg, phrases[rand()%5]);
        strcat(msg, "\0");

        pthread_mutex_lock(&lock);

        int flag = 0;
        for(int i=0; i<msg_index_; i++)
            if(!strcmp(msg_list_[i], msg))
                flag = 1;

        if(!flag)
        {
            check_msg_index();
            strcpy(msg_list_[msg_index_++], msg);

            //! Open the file for writing the message
            fp2 = fopen(file2, "a");
            fprintf(fp2, "%s\n", msg);

            //! Close the file
            fclose(fp2);

            if(msg_index_>1)
            {
                //! Open the file for writing the duration between message generations
                fp1 = fopen (file1, "a");
                fprintf(fp1, "%ld\n", timestamp - previous_timestamp);

                //! Close the file
                fclose(fp1);
            }

            previous_timestamp = timestamp;
        }

        pthread_mutex_unlock(&lock);

        sleep(rand()%241+60);
    }
}

/*
********************************************************************************
*   If the index of msg_list_ has reached 2000, then the oldest element gets   *
*   removed, and the new element enters the list through the last position.    *
********************************************************************************
*/

void check_msg_index(void)
{
    if(msg_index_ == 2000)
    {
        for(int i=0; i<3; i++)
            if(start_point_[i] != 0)
                start_point_[i]--;

    	for(int i=1; i<2000; i++)
    		strcpy(msg_list_[i-1],msg_list_[i]);

    	msg_index_--;
    }
}

/*
********************************
*   Set an alarm for 2 hours   *
********************************
*/

void alarmHandler(int sig)
{
	exit(0);
}
