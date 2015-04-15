#include        <sys/time.h>
#include        <sys/types.h>
#include        <sys/socket.h>
#include        <netinet/in.h>
#include        <errno.h>
#include        <netdb.h>
#include        <pthread.h>
#include        <signal.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include        <strings.h>
#include        <unistd.h>
#include	"bank.h"

#define CLIENT_PORT	54312
int TERM=0;
static pthread_attr_t kernel_attr;
void
set_iaddr_str( struct sockaddr_in * sockaddr, char * x, unsigned int port )
{
	struct hostent * hostptr;

	memset( sockaddr, 0, sizeof(sockaddr) );
	sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons( port );
	if ( (hostptr = gethostbyname( x )) == NULL )
	{
		printf( "Error getting addr information\n" );
	}
	else
	{
		bcopy( hostptr->h_addr, (char *)&sockaddr->sin_addr, hostptr->h_length );
	}
}

void printBuffer(char * ptr, int count)
{
char *c; 
int i;
printf("count --> %d\n", count);
for(i=0;i<count;i++)
{
        c = (ptr) + i;
	//printf("%d %d %c \n",i,c,*c);
        printf("%c", *c);
        fflush(stdout);
}
printf("PRINT DONE\n");
}






void *
client_read_thread(void* fd){
	pthread_detach(pthread_self());
	int	xd;
	xd=*(int*)fd;
	free(fd);
	char	buff[512];
	char	output[512];
	int bytes;
	float b;
	int term_count=0;		
	while(1){
		bank_response* bank_r;
		bank_r=(bank_response*)malloc(sizeof(bank_response));
		bytes=read(xd,bank_r,sizeof(bank_response));
		if(bytes==0)
			term_count++;
		if(term_count==20){
			printf("server connection failure\n");
			exit(0);
		}
		if(ntohs(bank_r->status)==SUCCESS){
			printf("command success: %s\n",bank_r->message);
		}else if(ntohs(bank_r->status) == FAILURE){
			printf("command failure: %s\n",bank_r->message);
		}else if(ntohs(bank_r->status)==TERMINATE){
			printf("exiting\n");
			TERM=1;
			pthread_exit(NULL);
		}else if(ntohs(bank_r->status)==QUERY_OP){
			b=(float)ntohl(bank_r->curr_balance);
			printf("query: your balance is %f\n",b);

		}
		
		//sprintf(output,"Result is >%s\n",buff);
		write(1, output, strlen(output));
	}
}	
int
main( int argc, char ** argv )
{
	int			sd;
	struct sockaddr_in	addr;
	char			string[512];
	char			buffer[512];
	char			prompt[] = "Enter a string>>";
	char			output[512];
	int			len;
	char *			func = "main";
	char *			token_p;
	bank_command *		bank_c=NULL;
	float			x;
		
	if ( argc < 2 )
	{
		printf( "\x1b[2;31mMust specify server name on command line\x1b[0m\n" );
		return 1;
	}
	set_iaddr_str( &addr, argv[1], CLIENT_PORT );
	printf( "Connecting to server %s ...\n", argv[1] );
	int c;
	do {
		errno = 0;
		if ( (sd = socket( AF_INET, SOCK_STREAM, 0 )) == -1 )
		{
			printf( "socket() failed in %s()\n", func );
			return -1;
		}
		else if ((c= connect( sd, (const struct sockaddr *)&addr, sizeof(addr))) < 0 )
		{
			printf("can't find server. trying again\n");
			fflush(stdout);
			close( sd );
			sleep(3);
		}
		
	} while ( errno == ECONNREFUSED );
			printf("c:%d\n",c);
	if ( errno != 0 )
	{
		printf( "Could not connect to server %s errno %d\n", argv[1], errno );
	}
	else
	{
		pthread_t tid;
		int * sdptr;
		sdptr = (int*)malloc(sizeof(int));
		*sdptr=sd;
		if(pthread_attr_init(&kernel_attr) !=0){
			printf("attr init failed at %s\n",func);
			return 0;
		}else if(pthread_attr_setscope(&kernel_attr, PTHREAD_SCOPE_SYSTEM) !=0 ){
			printf("barf\n");		
			return 0;
		}else if(pthread_create(&tid,&kernel_attr, client_read_thread, sdptr) != 0){
			printf("thread init barf\n");
			return 0;
		}
		bzero(string,sizeof(string));
		printf( "Connected to server %s\n", argv[1] );
		while ( write( 1, prompt, sizeof(prompt) ), (len = read( 0, string, sizeof(string) )) > 0 )
		{
			//printBuffer(string,sizeof(string));
			//printf("len:%d %d\n",len,sizeof(string));
			string[strlen(string)-1]='\0';
			//printBuffer(string,sizeof(string));
			//printf("string:%s:\n",string);	
			if(bank_c!=NULL)
				free(bank_c);
			bank_c=(bank_command*)malloc(sizeof(bank_command));
			bzero(bank_c,sizeof(bank_command));
			token_p=strtok(string," ");
			if(strcmp(token_p,"create")==0){

				bank_c->optcode=htons(CREATE_OP);
				token_p=strtok(NULL," ");
				if(token_p==NULL)
					continue;
				strcpy(bank_c->name,token_p);

			}else if(strcmp(token_p,"serve")==0){
				bank_c->optcode=htons(SERVE_OP);
				token_p=strtok(NULL," ");
				if(token_p==NULL)
					continue;
				strcpy(bank_c->name,token_p);

				
			}else if(strcmp(token_p,"deposit")==0){
				bank_c->optcode=htons(DEPOSIT_OP);
				token_p=strtok(NULL," ");
				if(token_p==NULL)
					continue;
				 x=atof(token_p);
				if(x<0){
					printf("must deposit positive value, enter again\n");
					continue;
				}	
				bank_c->amount = htonl((uint32_t)x);
				
			}else if(strcmp(token_p,"withdraw")==0){
				bank_c->optcode=htons(WITHDRAW_OP);
				token_p=strtok(NULL," ");
				if(token_p==NULL)
					continue;
				 x=atof(token_p);
				if(x<0){
					printf("must withdraw positive value, enter again\n");
					continue;
				}
				bank_c->amount = htonl((uint32_t)x);
				
	
			}else if(strcmp(token_p,"query")==0){
				bank_c->optcode=htons(QUERY_OP);
				
			}else if(strcmp(token_p,"end")==0){
				bank_c->optcode=htons(END_OP);

			}else if(strcmp(token_p,"quit")==0){
				bank_c->optcode=htons(QUIT_OP);

			}

			bzero(string,sizeof(string));
			write( sd,(char*)bank_c, sizeof(bank_command) );
			sleep(2);
			if(TERM)
				return 0;
		//	string=(char*)bank_c;	
		//	string[len]= '\0';
		//	write( sd, string, strlen( string ) + 1 );
		//	read( sd, buffer, sizeof(buffer) );
		//	sprintf( output, "Result is >%s<\n", buffer );
		//	write( 1, output, strlen(output) );
			
		}
	}

}
