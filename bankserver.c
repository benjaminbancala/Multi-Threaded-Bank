#include        <sys/time.h>
#include        <sys/types.h>
#include        <sys/socket.h>
#include        <netinet/in.h>
#include        <errno.h>
#include        <semaphore.h>
#include        <pthread.h>
#include        <signal.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
//#include        <synch.h>
#include        <unistd.h>
#include	"bank.h"
#define CLIENT_PORT	54312

static pthread_attr_t	user_attr;
static pthread_attr_t	kernel_attr;
static sem_t		actionCycleSemaphore;
static pthread_mutex_t	mutex;
static int		connection_count = 0;
static bank_account* account_root=NULL;
static pthread_mutex_t create_mutex;

static void
set_iaddr( struct sockaddr_in * sockaddr, long x, unsigned int port )
{
	memset( sockaddr, 0, sizeof(*sockaddr) );
	sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons( port );
	sockaddr->sin_addr.s_addr = htonl( x );
}

static char *
ps( unsigned int x, char * s, char * p )
{
	return x == 1 ? s : p;
}

void
periodic_action_handler( int signo, siginfo_t * ignore, void * ignore2 )
{
	if ( signo == SIGALRM )
	{
		sem_post( &actionCycleSemaphore );		// Perfectly safe to do ...
	}
}
void *
print_list(){
	int count=0;
	pthread_mutex_lock(&create_mutex);
	bank_account* ptr=account_root;
	while(ptr!=NULL){
		if(ptr->status==IN_SESSION)
		printf("account name: %s\n account balance: %f, account is in session\n ",ptr->name,ptr->balance);
		if(ptr->status!=IN_SESSION)
		
		printf("account name: %s\n account balance: %f, account is NOT in session\n ",ptr->name,ptr->balance);
		ptr=ptr->link;
		
	}
	pthread_mutex_unlock(&create_mutex);

}
void *
periodic_action_cycle_thread_old( void * ignore )
{
	struct sigaction	action;
	struct itimerval	interval;

	pthread_detach( pthread_self() );			// Don't wait for me, Argentina ...
	action.sa_flags = SA_SIGINFO | SA_RESTART;
	action.sa_sigaction = periodic_action_handler;
	sigemptyset( &action.sa_mask );
	sigaction( SIGALRM, &action, 0 );			// invoke periodic_action_handler() when timer expires
	interval.it_interval.tv_sec = 3;
	interval.it_interval.tv_usec = 0;
	interval.it_value.tv_sec = 3;
	interval.it_value.tv_usec = 0;
	setitimer( ITIMER_REAL, &interval, 0 );			// every 3 seconds
	for ( ;; )
	{
		sem_wait( &actionCycleSemaphore );		// Block until posted
		pthread_mutex_lock( &mutex );
		printf( "There %s %d active %s.\n", ps( connection_count, "is", "are" ),
			connection_count, ps( connection_count, "connection", "connections" ) );
		print_list();
		pthread_mutex_unlock( &mutex );
		sched_yield();					// necessary?
	}
	return 0;
}
void *
periodic_action_cycle_thread(void* ignore)
{
while(1){
	sleep(20);
	print_list();
}

}
void *
client_session_thread( void * arg )
{
	int			sd;
	char			request[2048];
	char			response[2048];
	char			temp;
	int			i;
	int			limit, size;
	float			ignore;
	long			senderIPaddr;
	char *			func = "client_session_thread";
	bank_account*		session=NULL;	
	int			thread_state=NOT_IN_SESSION;
	float			x;
	int			account_count=0;		
	sd = *(int *)arg;
	free( arg );					// keeping to memory management covenant
	pthread_detach( pthread_self() );		// Don't join on this thread
	pthread_mutex_lock( &mutex );
	++connection_count;				// multiple clients protected access
	pthread_mutex_unlock( &mutex );
	
	//while ( read( sd, request, sizeof(request) ) > 0 )
	while(1)
	{
		int bytes_read=0;
		int bytes_left=sizeof(bank_command);
		int c;
		bank_command* bank_c;
		bank_c=(bank_command*)malloc(sizeof(bank_command));
		bank_response* bank_r;
		bank_r=(bank_response*)malloc(sizeof(bank_response));
		char* ptr_r=(char*)bank_r;
		char* ptr=(char*)bank_c;
		while(bytes_left>0){
			ptr+=bytes_read;
			bytes_read=read(sd,ptr,bytes_left);
			bytes_left-=bytes_read;
				
		}
		bzero(bank_r,sizeof(bank_response));
		switch(ntohs(bank_c->optcode)){
			case CREATE_OP:
				if(account_count==20){
					bank_r->status=htons(FAILURE);
					strcpy(bank_r->message,"account creation failed, bank is at capacity");
					write(sd,ptr_r,sizeof(bank_response));	
					break;
				}
				printf("create %s\n",bank_c->name);
				c=SUCCESS;
				bank_r->status=htons(SUCCESS);
				pthread_mutex_lock(&create_mutex);
				bank_account* new_account=(bank_account*)malloc(sizeof(bank_account));
				
							
				
				strcpy(new_account->name,bank_c->name);
				new_account->balance=0;
				new_account->status=NOT_IN_SESSION;
				pthread_mutex_init(&(new_account->mutex),NULL);
				new_account->link=NULL;
				
				bank_account* next;
				bank_account** last;
				last=&account_root;
				next=*last;

				//get to the end of the linked list
				while(next!=NULL){
					if(strcmp(next->name,bank_c->name)==0)
						c=FAILURE;
					last=&next->link;
					next=next->link;			
					
				}
				if(c==SUCCESS){
					*last=new_account;
					bank_r->status=htons(SUCCESS);
					strcpy(bank_r->message,"account created");
					write(sd,ptr_r,sizeof(bank_response));
					account_count++;
				}else if (c==FAILURE){
					free(new_account);
					bank_r->status=htons(FAILURE);
					strcpy(bank_r->message,"account creation failed, account already exists");
					write(sd,ptr_r,sizeof(bank_response));	
				}						
				pthread_mutex_unlock(&create_mutex);
				break;
			case SERVE_OP:
				printf("serve %s\n",bank_c->name);
				c=FAILURE;
				bank_account* ptr=account_root;
				while(ptr!=NULL){
					if(strcmp(bank_c->name,ptr->name)==0){
						c=SUCCESS;
						break;
					}
					ptr=ptr->link;
				}
				if(c==SUCCESS){
					
					if(ptr->status==NOT_IN_SESSION && thread_state==NOT_IN_SESSION){
						ptr->status=IN_SESSION;	
						thread_state=IN_SESSION;
						bank_r->status=htons(SUCCESS);
						session=ptr;
						strcpy(bank_r->message,"you are now in session");
						write(sd,ptr_r,sizeof(bank_response));
					}
					else if(ptr->status==IN_SESSION && thread_state==NOT_IN_SESSION){
						bank_r->status=htons(FAILURE);
						strcpy(bank_r->message,"account is already in session");
						write(sd,ptr_r,sizeof(bank_response));

					}else if(thread_state==IN_SESSION){
						bank_r->status=htons(FAILURE);
						strcpy(bank_r->message,"you are already in session");
						write(sd,ptr_r,sizeof(bank_response));
					}
			
					
				}else if(c==FAILURE){	
					bank_r->status=htons(FAILURE);
					strcpy(bank_r->message,"no account of that name");
					write(sd,ptr_r,sizeof(bank_response));
				}
				break;
			case DEPOSIT_OP:
				pthread_mutex_lock(&(session->mutex));
				x=(float)ntohl(bank_c->amount);
				printf("deposit %f\n",x);
				if(thread_state==IN_SESSION && x>0){
					session->balance+=x;
					bank_r->status=htons(SUCCESS);
					strcpy(bank_r->message,"deposit successful");
					write(sd,ptr_r,sizeof(bank_response));	
				}else if(thread_state==IN_SESSION && x<=0){
					bank_r->status=htons(FAILURE);
					strcpy(bank_r->message,"must deposit a positive amount of money");
					write(sd,ptr_r,sizeof(bank_response));		
				}else if(thread_state==NOT_IN_SESSION){
					bank_r->status=htons(FAILURE);
					strcpy(bank_r->message,"you are not in session");
					write(sd,ptr_r,sizeof(bank_response));	
				}
				pthread_mutex_unlock(&(session->mutex));
				break;
			case WITHDRAW_OP:
				pthread_mutex_lock(&(session->mutex));
				x=(float)ntohl(bank_c->amount);
				printf("deposit %f\n",x);
				if(thread_state==IN_SESSION && x<=session->balance){
					session->balance-=x;
					bank_r->status=htons(SUCCESS);
					strcpy(bank_r->message,"withdraw successful");
					write(sd,ptr_r,sizeof(bank_response));	
				}else if(thread_state==IN_SESSION && x>session->balance){
					bank_r->status=htons(FAILURE);
					strcpy(bank_r->message,"you may not overdraw your account");
					write(sd,ptr_r,sizeof(bank_response));		
				}else if(thread_state==NOT_IN_SESSION){
					bank_r->status=htons(FAILURE);
					strcpy(bank_r->message,"you are not in session");
					write(sd,ptr_r,sizeof(bank_response));	
				}

				pthread_mutex_unlock(&(session->mutex));
				break;
			case QUERY_OP:
				printf("query\n");
				pthread_mutex_lock(&(session->mutex));
				if(thread_state==IN_SESSION){
					printf("TEST1\n");
					bank_r->status=htons(QUERY_OP);
					strcpy(bank_r->message,"query successful");
					uint32_t b = (uint32_t)session->balance;
					bank_r->curr_balance=htonl(b);
					write(sd,ptr_r,sizeof(bank_response));
				
				}else if(thread_state==NOT_IN_SESSION){
					printf("TEST2\n");
					bank_r->status=htons(FAILURE);
					strcpy(bank_r->message,"you are not in session");
					write(sd,ptr_r,sizeof(bank_response));	

				}
				pthread_mutex_unlock(&(session->mutex));
				break;
			case END_OP:
				printf("end\n");
				if(session!=NULL){
					session->status=NOT_IN_SESSION;
					thread_state=NOT_IN_SESSION;
					session=NULL;
					bank_r->status=htons(SUCCESS);
					strcpy(bank_r->message,"session has been ended");
					write(sd,ptr_r,sizeof(bank_response));			
				}else if(session==NULL){
					bank_r->status=htons(FAILURE);
					strcpy(bank_r->message,"no session to end");
					write(sd,ptr_r,sizeof(bank_response));	
						
				}
				break;
			case QUIT_OP:
				printf("quit\n");
				bank_r->status=htons(TERMINATE);
				strcpy(bank_r->message,"QUIT COMMAND ACK");
				write(sd,ptr_r,sizeof(bank_response));
				close( sd );
				pthread_mutex_lock( &mutex );
				--connection_count;				// multiple clients protected access
				pthread_mutex_unlock( &mutex );
				pthread_exit(NULL);
				return 0;
			
		}
	/*	printf( "server receives input:  %s\n", request );
		size = strlen( request );
		limit = strlen( request ) / 2;
		for ( i = 0 ; i < limit ; i++ )
		{
			temp = request[i];
			request[i] = request[size - i - 1];
			request[size - i - 1] = temp;
		}
		write( sd, request, strlen(request) + 1 );*/
		free(bank_c);
		free(bank_r);
	}
/*	close( sd );
	pthread_mutex_lock( &mutex );
	--connection_count;				// multiple clients protected access
	pthread_mutex_unlock( &mutex );
	return 0;*/
}

void *
session_acceptor_thread( void * ignore )
{
	int			sd;
	int			fd;
	int *			fdptr;
	struct sockaddr_in	addr;
	struct sockaddr_in	senderAddr;
	socklen_t		ic;
	pthread_t		tid;
	int			on = 1;
	char *			func = "session_acceptor_thread";

	pthread_detach( pthread_self() );
	if ( (sd = socket( AF_INET, SOCK_STREAM, 0 )) == -1 )
	{
		printf( "socket() failed in %s()\n", func );
		return 0;
	}
	else if ( setsockopt( sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) ) == -1 )
	{
		printf( "setsockopt() failed in %s()\n", func );
		return 0;
	}
	else if ( set_iaddr( &addr, INADDR_ANY, CLIENT_PORT ), errno = 0,
			bind( sd, (const struct sockaddr *)&addr, sizeof(addr) ) == -1 )
	{
		printf( "bind() failed in %s() line %d errno %d\n", func, __LINE__, errno );
		close( sd );
		return 0;
	}
	else if ( listen( sd, 100 ) == -1 )
	{
		printf( "listen() failed in %s() line %d errno %d\n", func, __LINE__, errno );
		close( sd );
		return 0;
	}
	else
	{
		ic = sizeof(senderAddr);
		while ( (fd = accept( sd, (struct sockaddr *)&senderAddr, &ic )) != -1 )
		{
			fdptr = (int *)malloc( sizeof(int) );
			*fdptr = fd;					// pointers are not the same size as ints any more.
			if ( pthread_create( &tid, &kernel_attr, client_session_thread, fdptr ) != 0 )
			{
				printf( "pthread_create() failed in %s()\n", func );
				return 0;
			}
			else
			{
				continue;
			}
		}
		close( sd );
		return 0;
	}
}

int
main( int argc, char ** argv )
{
	pthread_t		tid;
	char *			func = "server main";

	if ( pthread_attr_init( &user_attr ) != 0 )
	{
		printf( "pthread_attr_init() failed in %s()\n", func );
		return 0;
	}
	else if ( pthread_attr_setscope( &user_attr, PTHREAD_SCOPE_SYSTEM ) != 0 )
	{
		printf( "pthread_attr_setscope() failed in %s() line %d\n", func, __LINE__ );
		return 0;
	}
	else if ( pthread_attr_init( &kernel_attr ) != 0 )
	{
		printf( "pthread_attr_init() failed in %s()\n", func );
		return 0;
	}
	else if ( pthread_attr_setscope( &kernel_attr, PTHREAD_SCOPE_SYSTEM ) != 0 )
	{
		printf( "pthread_attr_setscope() failed in %s() line %d\n", func, __LINE__ );
		return 0;
	}
	else if ( sem_init( &actionCycleSemaphore, 0, 0 ) != 0 )
	{
		printf( "sem_init() failed in %s()\n", func );
		return 0;
	}
	else if ( pthread_mutex_init( &mutex, NULL ) != 0 )
	{
		printf( "pthread_mutex_init() failed in %s()\n", func );
		return 0;
	}else if( pthread_mutex_init( &create_mutex, NULL ) != 0 )
	{
		printf("pthread_mutex_init() for list failed in %s()\n",func);
		return 0;
	}
	else if ( pthread_create( &tid, &kernel_attr, session_acceptor_thread, 0 ) != 0 )
	{
		printf( "pthread_create() failed in %s()\n", func );
		return 0;
	}
	else if ( pthread_create( &tid, &kernel_attr, periodic_action_cycle_thread, 0 ) != 0 )
	{
		printf( "pthread_create() failed in %s()\n", func );
		return 0;
	}
	else
	{
		printf( "server is ready to receive client connections ...\n" );
		pthread_exit( 0 );
	}
}
