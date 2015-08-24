#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql.h>

/* Card parsing is a bit weird.
 * Beginning from line one:
 * %B<card number>^<last name>/<first name>  <all the spaces><numbers>
 * ignore this line
 * ignore this line too
 */

#define maxbuf 255
#define maxcharlen 256
#define def_num_fields 6
#define lname_row 0
#define fname_row 1
#define email_row 2
#define sid_row 3
#define crd_row 4
#define imn_row 5


void finish_with_error(MYSQL *con)
{
	fprintf(stderr, "%s\n", mysql_error(con));
	mysql_close(con);
	exit(1);        
}

/* Struct Name: member */
typedef struct
{
	char *lname;
	char *fname;
	char *email;
	unsigned int sid;
	unsigned long long crd;
	unsigned long long imn;
} member_t;

/* Function Name: init_member
 * Description: Initializes the "member" struct
 * Input: member_t pointer
 * Output: return status
 */
int init_member(member_t *member)
{
	/* Allocate */
	member->lname = (char*) malloc(maxcharlen);
	member->fname = (char*) malloc(maxcharlen);
	member->email = (char*) malloc(maxcharlen);

	/* Zero */
	*member->lname = '\0';
	*member->fname = '\0';
	*member->email = '\0';
	member->sid = 0;
	member->crd = 0;
	member->imn = 0;

	/* Check it */
	if (!(member->lname))
		return 1;

	if (!(member->fname))
		return 1;

	if (!(member->email))
		return 1; 

	return 0;
}

int free_member(member_t *member)
{
	/* Deallocate */
	free(member->lname);
	free(member->fname);
	free(member->email);
}


/* Funciton Name: read_card
 * Description: Reads your card for informaiton.  This function interfaces
 * with the terminal directly and blocks out card information.  Since card
 * exits cannot be predicted by using newlines, the function uses a timeout
 * period using the select() function.
 * Inputs: buffer
 * Outpts: size
 * TODO: change this function to do the things
 */
ssize_t read_card(char* buf)
{
	struct termios org_term_settings, new_term_settings;
	tcgetattr(STDIN_FILENO, &org_term_settings);
	new_term_settings = org_term_settings;
	/* No echo, allow editing */
	new_term_settings.c_lflag &= ~ECHO;
	new_term_settings.c_lflag |= ICANON | ECHOE;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term_settings);

	int size;

	size = read(STDIN_FILENO, buf, 255);

	/* select return variable */
	int reading;

	do {
		/* Variables for select() */
		fd_set input_set;
		struct timeval timeout;
		/* FD set initialization */
		FD_ZERO(&input_set);
		FD_SET(STDIN_FILENO, &input_set);
		/* timeout period initialization */
		timeout.tv_sec = 0;
		timeout.tv_usec = 250000;

		reading = select(1, &input_set, NULL, NULL, &timeout);

		if (reading == -1)
			return -1;

		if (reading) {
			char c;
			while ((c = getchar()) != '\n' && c != EOF);
		}

	} while (reading);

	/* Reset terminal to its previous state */
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &org_term_settings);

	return size;
}

/* Function Name: Parse Card
 * Description: Parses the card string for SID and card ID and fname and lname
 * Inputs: member_t struct, card buffer
 * Outpts: parsing success or failure
 */
int parse_card(member_t *member, char* buf, unsigned long long new_card)
{

	char alpha_cn[17];
	char *alpha_fn = member->fname;
	char *alpha_ln = member->lname;
	char alpha_sid[10];
	const char fmt[] = "%B^/ ^491212000000";
	int fmtind = 0;
	int bufind = 0;

	/* First two characters are always constant. */

	while (fmtind < 2) {
		if (fmt[fmtind++] != buf[bufind])
			return (!bufind) ? -1 : bufind;
		bufind++;
	}

	/* Card ID ends with a '^' and should always be sixteen characters*/
	while (1) {
		static int cnind = 0;
		/* Tries to find the next fmt character */
		if (fmt[fmtind] == buf[bufind] && cnind == 16) {
			alpha_cn[cnind] = '\0';
			/* Null terminal for the string */
			fmtind++;
			bufind++;
			break;
		} else if (fmt[fmtind] == buf[bufind] || cnind == 16)
			return bufind;

		if (!isdigit(buf[bufind]))
			return bufind;

		alpha_cn[cnind++] = buf[bufind++];
	}

	/* fmtind at three */
	/* Parse last name */
	while (1) {
		static int lnind = 0;
		/* Tries to find the next fmt character */
		if (fmt[fmtind] == buf[bufind]) {
			alpha_ln[lnind] = '\0';
			fmtind++;
			bufind++;
			break;
		}

		if (!isalpha(buf[bufind]))
			return bufind;

		alpha_ln[lnind++] = buf[bufind++];
	}

	/* fmtind at four */
	/* Parse first name */
	while(1) {
		static int fnind = 0;
		/* Try to find the next fmt character */
		if (fmt[fmtind] == buf[bufind]) {
			alpha_fn[fnind] = '\0';
			fmtind++;
			bufind++;
			break;
		}

		if (!isalpha(buf[bufind]))
			return bufind;

		alpha_fn[fnind++] = buf[bufind++];
	}

	/* fmtind at five */
	/* Find carat*/
	while (buf[bufind])
		if (fmt[fmtind] == buf[bufind++])
			break;

	fmtind++;

	if (!buf[bufind])
		return bufind;

	while (fmt[fmtind])
		if (!buf[bufind] || fmt[fmtind++] != buf[bufind++])
			return bufind;

	/* fmtind is at the end of fmt */
	/* Get the SID! */
	int sid_count = 9;
	while (sid_count--) {
		static int sidind = 0;
		if (!isdigit(buf[bufind]))
			return bufind;
		else
			alpha_sid[sidind++] = buf[bufind++];
	}
	/* Remember to null-terminate it! */
	alpha_sid[9] = '\0';

	/* Insert atoi-ish things here. */
	member->sid = atoi(alpha_sid);
	new_card = atoll(alpha_cn);

	/* And we can ignore the rest of the card string. */

	return 0;
}

/* Function Name: query_sid
 * Description: Queries the user (with the same header) his/her SID.
 * Inputs: a buffer, the buffer's size
 * Outputs: NULL if something went wrong a character array if all is good
 */
char *query_sid(char* buf, ssize_t sz)
{
	/* Screen Messages */
	printf("\033[2J\033[H");
	printf("IEEE@UCR Card Login System\n");
	printf("Manual SID entry mode activated.\n");
	printf("UCR SID: ");

	if(!fgets(buf, sz, stdin))
		exit(1);

	char *c = buf;
	while (*c != '\n')
		if(!isdigit(*c++))
			return NULL;

	return buf;
}

/* Function Name: query_imn
 * Description: Queries the user (with the same header) his/her IEEE member num
 * Inputs: a buffer, the buffer's size
 * Outputs: NULL if something went wrong a character array if all is good
 */
char *query_imn(char* buf, ssize_t sz)
{
	/* Screen Messages */
	printf("\033[2J\033[H");
	printf("IEEE@UCR Card Login System\n");
	printf("Manual IEEE member number entry mode activated.\n");
	printf("IEEE Member Number (press enter if not applicable): ");

	if(!fgets(buf, sz, stdin))
		exit(1);

	char *c = buf;

	if (*c == '\n')
		return NULL;

	while (*c != '\n')
		if(!isdigit(*c++))
			return NULL;

	return buf;
}

/* Function Name: query_email
 * Description: Queries the user for their email.
 * Inputs: a buffer, buffer size
 * Outputs: NULL if something went wrong
 */
char *query_email(char* buf, ssize_t sz)
{
	/* Screen Messages */
	printf("\033[2J\033[H");
	printf("IEEE@UCR Card Login System\n");
	printf("Manual email entry mode activated.\n");
	printf("UCR email: ");

	if(!fgets(buf, sz, stdin))
		exit(1);

	int return_ok = -1;

	char *c = buf;

	/* Email verification */
	while(*c != '\n') {
		if (*c++ == '@' && return_ok == 0) {
			return_ok = 1;
			break;
		}
		return_ok = 0;
	}

	int indx = 0;

	while(*c != '\n') {
		if (*c == '.' && indx == -1) {
			indx++;
		} else if (*c == 'u' && indx == 0) {
			indx++;
		} else if (*c == 'c' && indx == 1) {
			indx++;
		} else if (*c == 'r' && indx == 2) {
			indx++;
		} else if (*c == '.' && indx == 3) {
			indx++;
		} else if (*c == 'e' && indx == 4) {
			indx++;
		} else if (*c == 'd' && indx == 5) {
			indx++;
		} else if (*c == 'u' && indx == 6) {
			indx++;
		} else if (indx == 7) {
			indx = 0;
			break;
		} else if (isalpha(*c)) {
			indx = -1;
		} else {
			indx = 0;
			break;
		}
		c++;
	}

	if (indx == 7)
		return_ok++;

	*c = '\0';

	return return_ok == 2 ? buf : NULL;
}

/* Function Name: query_lname
 * Description: Queries the user for their last name
 * Inputs: buffer, size
 * Outputs: NULL if something went wrong, buffer if all is good
 */
char *query_lname(char* buf, ssize_t sz)
{
	/* Screen Messages */
	printf("\033[2J\033[H");
	printf("IEEE@UCR Card Login System\n");
	printf("Manual Last Name entry mode activated.\n");
	printf("Last Name: ");

	if(!fgets(buf, sz, stdin))
		exit(1);

	char *c = buf;

	while (*c++ != '\n');

	*(--c) = '\0';

	return buf;
}

/* Function Name: query_fname
 * Description: Queries the user for their last name
 * Inputs: buffer, size
 * Outputs: NULL if something went wrong, buffer if all is good
 */
char *query_fname(char* buf, ssize_t sz)
{
	/* Screen Messages */
	printf("\033[2J\033[H");
	printf("IEEE@UCR Card Login System\n");
	printf("Manual First Name entry mode activated.\n");
	printf("First Name: ");

	if(!fgets(buf, sz, stdin))
		exit(1);

	char *c = buf;

	while (*c++ != '\n');

	*(--c) = '\0';

	return buf;
}

/* Function Name: sid_verify
 * Description: verifies a student's SID
 * Inputs: member_t pointer
 * Outputs: Returns 0 if valid SID
 */
int sid_verify(member_t *member)
{
	if (!member)
		return 1;

	if (member->sid / 1000000 > 861 || member->sid / 1000000 < 860)
		return 1;

	return 0;
}

/* Function Name: sid_correct
 * Description: asks the user if the SID is correct
 * Inputs: member_t
 * Outputs: 1 (yes) 0 (no)
 */
int sid_correct(member_t *member)
{
	printf("\033[2J\033[H");
	printf("IEEE@UCR Card Login System\n");
	while (1) {
		printf("Is your SID (%i) correct?. Y/n", member->sid);
		char c;
		c = getc(stdin);
		if ((c & 0x5F) == 'Y') {
			return 1;
		}
		if ((c & 0x5F) == 'N') {
			return 0;
		}
		if (c == '\n') {
			return 1;
		}
		printf("\033[2H");
		printf("Invalid! ");
	}
}

/* Function Name: information_gather
 * Description: Gathers user information for database retrieval.
 * Inputs: member_t
 * Output: apparently an interger in case it encounters an error...
 */
int information_gather(member_t *member, unsigned long long new_card)
{
	int try = 0;
	do {
		char card_buf[maxcharlen];
		int sz;
		printf("\033[2J\033[H");
		printf("IEEE@UCR Card Login System\n");
		if (try) {
			printf("Try again. %i\n", try);
		}
		printf("Please swipe your card or press ");
		printf("enter for manual entry.\n> ");
		fflush(stdout);

		sz = read_card(card_buf);

		if (sz <= 1) {
			char buf[maxcharlen];
			query_sid(buf, maxcharlen);

			member->sid = atoi(buf);
		} else
			parse_card(member, card_buf, new_card);

		try++;
	} while (sid_verify(member) || (!sid_correct(member)));
}

/* Function Name: information_gather2
 * Description: Gathers missing user information for database update
 * Inputs: member_t
 * Outputs: return condition in case of error
 */
int information_gather2(member_t *member)
{
	while(!*member->lname) {
		char buf[maxcharlen];
		if (!query_lname(buf, maxbuf))
			*member->lname = '\0';
		else
			strcpy(member->lname, buf);
	}

	while(!*member->fname) {
		char buf[maxcharlen];
		if (!query_fname(buf, maxbuf))
			*member->fname = '\0';
		else
			strcpy(member->fname, buf);
	}

	while(!*member->email) {
		char buf[maxcharlen];
		if (!query_email(buf, maxbuf))
			*member->email = '\0';
		else
			strcpy(member->email, buf);
	}

	if (!member->imn) {
		char buf[maxcharlen];
		if(!query_imn(buf, maxbuf))
			member->imn = 0;
		else
			member->imn = atoll(buf);
	}
}

/* Function Name: card_number_verification
 * Description: Checks the database-retrieved card number versus the current
 * Inputs: new_card_number, member_t
 * Output: int return condition
 */
int card_number_verification(unsigned long long nc, member_t *member)
{
	if (!nc)
		return 0;

	if (!member->crd) {
		member->crd = nc;
		return 0;
	}

	if (nc != member->crd) {
		printf("\033[2J\033[H");
		printf("IEEE@UCR Card Login System\n");
		printf("Scanned card does not match database.\n");
		while (1) {
			printf("Do you want to update your stored card? Y/n");
			char c;
			c = getc(stdin);
			if ((c & 0x5F) == 'Y') {
				member->crd = nc;
				return 0;
			}
			if ((c & 0x5F) == 'N') {
				return 0;
			}
			printf("\033[2H");
			printf("Invalid! ");
		}
	}
}

/* Function Name: database_check
 * Description: Checks database for this specific member.
 * Inputs: member, mysql connection
 * Output: int return condition
 */
int database_check(MYSQL *con, member_t *member)
{
	char buf[256];
	sprintf(buf,
			"SELECT lname,fname,email,sid,cn,ieeem FROM member WHERE sid = %i;",
			member->sid);
	if (mysql_query(con, buf)) {
		finish_with_error(con);
	}

	MYSQL_RES *result = mysql_store_result(con);

	if (result == NULL) 
	{
		finish_with_error(con);
	}

	int num_fields = mysql_num_fields(result);

	if (num_fields != def_num_fields) {
		fprintf(stderr, "Not right field number!\n");
		exit (1);
	}

	MYSQL_ROW row = mysql_fetch_row(result);

	if (!row)
		return 0;

	strcpy(member->lname, row[lname_row]);
	strcpy(member->fname, row[fname_row]);
	strcpy(member->email, row[email_row]);
	member->sid = atoi(row[sid_row]);
	member->crd = row[crd_row] ? atoll(row[crd_row]) : 0;
	member->imn = row[imn_row] ? atoll(row[imn_row]) : 0;

	return 0;
}

/* Function Name: print_member_information
 * Description: prints the member's information
 * Inputs: &member
 * Outputs: none
 */
void print_member_information(member_t *member) {
	printf("\033[2J\033[H");
	printf("IEEE@UCR Card Login System\n");
	printf("Confirm your information.\n");
	printf("\033[1mL\033[0mast Name: %s\n", member->lname);
	printf("\033[1mF\033[0mirst Name: %s\n", member->fname);
	printf("\033[1mE\033[0mmail: %s\n", member->email);
	printf("SID: %i\n", member->sid);
	if (member->imn)
		printf("\033[1mI\033[0mEEE Member: %lli\n", member->imn);
	else
		printf("\033[1mI\033[0mEEE Member: Not an IEEE member yet\n");

}

/* Function Name: confirm_change_information
 * Description: Confirm or change your information!
 * Inputs: &member
 * Output: int return condition
 */
int confirm_change_information(member_t *member)
{
	int breakout = 0;
	while(!breakout) {
		print_member_information(member);
		printf("Choose the field you would like to change.\n");
		printf("\033[1m(L,F,E,I)\033[0m \n");
		while (1) {
			printf("Press enter to continue or choose a field: ");
			char c = getc(stdin);
			if (c != '\n')
				while (getc(stdin) != '\n');
			if ((c & 0x5F) == 'L') {
				*member->lname = 0;
				while(!*member->lname) {
					char buf[maxcharlen];
					if (!query_lname(buf, maxbuf))
						*member->lname = '\0';
					else
						strcpy(member->lname, buf);
				}
				break;
			}
			if ((c & 0x5F) == 'F') {
				*member->fname = 0;
				while(!*member->fname) {
					char buf[maxcharlen];
					if (!query_fname(buf, maxbuf))
						*member->fname = '\0';
					else
						strcpy(member->fname, buf);
				}
				break;
			}
			if ((c & 0x5F) == 'E') {
				*member->email = 0;
				while(!*member->email) {
					char buf[maxcharlen];
					if (!query_email(buf, maxbuf))
						*member->email = '\0';
					else
						strcpy(member->email, buf);
				}
				break;
			}
			if ((c & 0x5F) == 'I') {
				member->imn = 0;
				if (!member->imn) {
					char buf[maxcharlen];
					if(!query_imn(buf, maxbuf))
						member->imn = 0;
					else
						member->imn = atoll(buf);
				}
				break;
			}
			if (c == '\n') {
				breakout = 1;
				break;
			}
			printf("\033[10H");
			printf("Invalid! ");
			while (getc(stdin) != '\n');
		}

	}
}

/* Function Name: database_entry
 * Description: Inputs the stuff into the database.
 * Inputs: mysql connection, member_t
 * Outputs: pretty much error conditions...
 */
int database_entry(MYSQL *con, member_t *member)
{
	char string[1000], *c;

	c = (char*) strmov(string, "INSERT INTO member SET");
	c = (char*) strmov(c, " lname='");
	c += mysql_real_escape_string(con, c, member->lname,
			strlen(member->lname));
	c = (char*) strmov(c, "', fname='");
	c += mysql_real_escape_string(con, c, member->fname,
			strlen(member->fname));
	c = (char*) strmov(c, "', email='");
	c += mysql_real_escape_string(con, c, member->email,
			strlen(member->email));
	c = (char*) strmov(c, "', sid=");
	c += sprintf(c, "%i", member->sid);
	c = (char*) strmov(c, ", cn=");
	if (member->crd)
		c += sprintf(c, "%lli", member->crd);
	else
		c = (char*) strmov(c, "NULL");
	c = (char*) strmov(c, ", ieeem=");
	if (member->imn)
		c += sprintf(c, "%lli", member->imn);
	else
		c = (char*) strmov(c, "NULL");
	c = (char*) strmov(c, " ON DUPLICATE KEY UPDATE ");
	c = (char*) strmov(c, " lname='");
	c += mysql_real_escape_string(con, c, member->lname,
			strlen(member->lname));
	c = (char*) strmov(c, "', fname='");
	c += mysql_real_escape_string(con, c, member->fname,
			strlen(member->fname));
	c = (char*) strmov(c, "', email='");
	c += mysql_real_escape_string(con, c, member->email,
			strlen(member->email));
	c = (char*) strmov(c, "', sid=");
	c += sprintf(c, "%i", member->sid);
	c = (char*) strmov(c, ", cn=");
	if (member->crd)
		c += sprintf(c, "%lli", member->crd);
	else
		c = (char*) strmov(c, "NULL");
	c = (char*) strmov(c, ", ieeem=");
	if (member->imn)
		c += sprintf(c, "%lli", member->imn);
	else
		c = (char*) strmov(c, "NULL");

	if (mysql_real_query(con, string, (unsigned int) (c - string))) {
		finish_with_error(con);
	}
	
	return 0;
}

int main()
{
	MYSQL *mysqlp;

	if(!(mysqlp = mysql_init(mysqlp)))
		exit(1);

	if (!mysql_real_connect(mysqlp, "127.0.0.1", "bkluilkx_ieee",
				"uavRoom1227", "bkluilkx_ieee", 0, NULL, 0)) {
		finish_with_error(mysqlp);
	}

	while (1) {
		member_t member;
		init_member(&member);

		unsigned long long new_card = 0;

		information_gather(&member, new_card);

		database_check(mysqlp, &member);

		information_gather2(&member);

		card_number_verification(new_card, &member);

		confirm_change_information(&member);

		database_entry(mysqlp, &member);

		free_member(&member);
	}

	mysql_close(mysqlp);

	return 0;
}