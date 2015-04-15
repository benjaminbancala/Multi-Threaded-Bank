#define CREATE_OP 1
#define SERVE_OP 2
#define DEPOSIT_OP 3
#define WITHDRAW_OP 4
#define QUERY_OP 5
#define END_OP 6
#define QUIT_OP 7
#define SUCCESS 0
#define FAILURE 1
#define TERMINATE 10
#define IN_SESSION 9
#define NOT_IN_SESSION 8
typedef struct bank_command{
	 uint16_t optcode;
	 char name[100];
	uint32_t amount;
	
}bank_command;

typedef struct bank_response{
	uint16_t status;
	char message[100];
	uint32_t curr_balance;

}bank_response;

typedef struct bank_account_tag{
	struct bank_account_tag* link;
	char name[100];
	float balance;
	int status;
	pthread_mutex_t mutex;
	


}bank_account;
