/* Copyright (c) 2015, IEEE@UCR 
 * You should have recieved a license bundled with this software.
 */


/* Mysql Configuration settings */
#define csi_mysql_server "127.0.0.1"
#define csi_mysql_user "bkluilkx_ieee"
#define csi_mysql_password "uavRoom1227"
#define csi_mysql_database "bkluilkx_ieee"
#define csi_mysql_port 0
#define csi_mysql_unix_socket NULL
#define csi_mysql_client_flag 0

/* Display Configuration Settings */
#define csi_row1_message "IEEE@UCR Card Login System\n"
#define csi_org "IEEE"
#define csi_org_a "an IEEE"

#define maxbuf 256
#define maxquerylen 1000

#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include "inc/sql.h"
#include "inc/member.h"

/* Card parsing is a bit weird.
 * Beginning from line one:
 * %B<card number>^<last name>/<first name>  <all the spaces><numbers>
 * ignore this line
 * ignore this line too
 */

/* Funciton Name: read_card
 * Description: Reads your card for informaiton.  This function interfaces
 * with the terminal directly and blocks out card information.  Since card
 * endings cannot be predicted by using newlines, the function uses a timeout
 * period using the select() function after the first line
 * Inputs: buffer
 * Outpts: size
 */
ssize_t read_card(char* buf, ssize_t max)
{
	struct termios org_term_settings, new_term_settings;
	tcgetattr(STDIN_FILENO, &org_term_settings);
	new_term_settings = org_term_settings;
	/* No echo, allow editing */
	new_term_settings.c_lflag &= ~ECHO;
	new_term_settings.c_lflag |= ICANON | ECHOE;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term_settings);

	int size;

	size = read(STDIN_FILENO, buf, max-1);

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

	/* Restore terminal to its previous state */
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
	int cnind = 0;
	while (1) {
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
	int lnind = 0;
	while (1) {
		/* Tries to find the next fmt character */
		if (fmt[fmtind] == buf[bufind]) {
			alpha_ln[lnind] = '\0';
			fmtind++;
			bufind++;
			break;
		}

		if (!isalpha(buf[bufind]) && buf[bufind] != ' ')
			return bufind;

		alpha_ln[lnind++] = buf[bufind++];
	}

	/* fmtind at four */
	/* Parse first name */
	int fnind = 0;
	while(1) {
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
	int sidind = 0;
	while (sid_count--) {
		if (!isdigit(buf[bufind]))
			return bufind;
		else
			alpha_sid[sidind++] = buf[bufind++];
	}
	/* Remember to null-terminate it! */
	alpha_sid[9] = '\0';

	/* Insert atoi-ish things here. */
	member->sid = atoi(alpha_sid);
	printf("%s", alpha_sid);
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
	printf(csi_row1_message);
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
 * Description: Queries the user (with the same header) his/her member num
 * Inputs: a buffer, the buffer's size
 * Outputs: NULL if something went wrong a character array if all is good
 */
char *query_imn(char* buf, ssize_t sz)
{
	/* Screen Messages */
	printf("\033[2J\033[H");
	printf(csi_row1_message);
	printf("Manual " csi_org " member number entry mode activated.\n");
	printf( csi_org " Member Number (press enter if not applicable): ");

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
	printf(csi_row1_message);
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
	printf(csi_row1_message);
	printf("Manual Last Name entry mode activated.\n");
	printf("Last Name: ");

	if(!fgets(buf, sz, stdin))
		exit(1);

	char *c = buf;

	while (*c++ != '\n');

	*(--c) = '\0';

	return buf;
}

/* Function Name: query_mtg_description
 * Description: Queries the admin for meeting information
 * Inputs: buffer, size
 * Outputs: no error conditions, strangely enough.
 */
void query_mtg_description(MYSQL *con, meeting_t *meeting, ssize_t sz)
{
	/* Screen Messages */
	printf("\033[2J\033[H");
	printf(csi_row1_message);
	printf("Meeting description entry mode activated.\n");
	printf("Meeting description: ");

	if (!fgets(meeting->description, sz, stdin))
		exit(1);

	char *c = meeting->description;

	while (*c++ != '\n');

	*(--c) = '\0';

	meeting_update(con, meeting);
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
	printf(csi_row1_message);
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

/* Function Name: getsinglechar
 * Description: gets a single character (without having to press enter)
 * Inputs: nothing, really...
 * Output: a character.
 */
char getsinglechar()
{
	fflush(stdout);

	struct termios org_term_settings, new_term_settings;
	tcgetattr(STDIN_FILENO, &org_term_settings);
	new_term_settings = org_term_settings;
	/* No echo, non-canonical mode with single character input */
	new_term_settings.c_lflag &= ~ECHO & ~ICANON;
	new_term_settings.c_cc[VMIN] = 1;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term_settings);

	char c;

	/* Read single character */
	read(STDIN_FILENO, &c, 1);

	/* Reset terminal to its previous state */
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &org_term_settings);

	return c;
}

/* Function Name: sid_correct
 * Description: asks the user if the SID is correct
 * Inputs: member_t
 * Outputs: 1 (yes) 0 (no)
 */
int sid_correct(member_t *member)
{
	printf("\033[2J\033[H");
	printf(csi_row1_message);
	while (1) {
		printf("Is your SID (%i) correct?. (\033[1mY\033[0m is default)"
				" \033[1mY\033[0m/n", member->sid);
		char c;
		c = getsinglechar();
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

	return 0;
}

/* Function Name: information_gather
 * Description: Gathers user information for database retrieval.
 * Inputs: member_t
 * Output: nothing
 */
void information_gather(member_t *member, unsigned long long new_card)
{
	int try = 0;
	do {
		char card_buf[maxbuf];
		int sz;
		printf("\033[2J\033[H");
		printf(csi_row1_message);
		if (try) {
			printf("Try again. %i\n", try);
		}
		printf("Please swipe your card or press ");
		printf("enter for manual entry.\n> ");
		fflush(stdout);

		sz = read_card(card_buf, maxbuf-1);

		if (sz <= 1) {
			char buf[maxbuf];
			query_sid(buf, maxbuf);

			member->sid = atoi(buf);
		} else
			parse_card(member, card_buf, new_card);

		try++;

		printf("%s", card_buf);
		printf("%i", member->sid);
	} while (sid_verify(member) || (!sid_correct(member)));
}

/* Function Name: information_gather2
 * Description: Gathers missing user information for database update
 * Inputs: member_t
 * Outputs: nothing
 */
void information_gather2(member_t *member)
{
	while(!*member->lname) {
		char buf[maxbuf];
		if (!query_lname(buf, maxbuf-1))
			*member->lname = '\0';
		else
			strcpy(member->lname, buf);
	}

	while(!*member->fname) {
		char buf[maxbuf];
		if (!query_fname(buf, maxbuf-1))
			*member->fname = '\0';
		else
			strcpy(member->fname, buf);
	}

	while(!*member->email) {
		char buf[maxbuf];
		if (!query_email(buf, maxbuf-1))
			*member->email = '\0';
		else
			strcpy(member->email, buf);
	}

	if (!member->mn) {
		char buf[maxbuf];
		if(!query_imn(buf, maxbuf-1))
			member->mn = 0;
		else
			member->mn = atoll(buf);
	}
}

/* Function Name: card_number_verification
 * Description: Checks the database-retrieved card number versus the current
 * Inputs: new_card_number, member_t
 * Output: int return condition
 */
void card_number_verification(unsigned long long nc, member_t *member)
{
	if (!nc)
		return;

	if (!member->crd) {
		member->crd = nc;
		return;
	}

	if (nc != member->crd) {
		printf("\033[2J\033[H");
		printf(csi_row1_message);
		printf("Scanned card does not match database.\n");
		while (1) {
			printf("Do you want to update your stored card? Y/n");
			char c;
			c = getsinglechar(stdin);
			if ((c & 0x5F) == 'Y') {
				member->crd = nc;
				return;
			}
			if ((c & 0x5F) == 'N') {
				return;
			}
			printf("\033[2H");
			printf("Invalid! ");
		}
	}
}

/* Function Name: query_mtg_type
 * Description: queries the meeting type of choice.  (This is a character)
 * Inputs: nothing much, no meeting struct yet.
 */

void query_mtg_type(MYSQL *con, meeting_t *meeting)
{
    printf("\033[2J\033[H");
    printf(csi_row1_message);
    printf("Meeting Type Query\n");
    printf("Meeting types are as follows:\n");
    printf("\033[1mG\033[0meneral Meetings\n");
    printf("Simply press return if you are here by mistake.\n");
    printf("Select the meeting type: ");
    char c = getsinglechar(stdin);
    if ((c & 0x5F) == 'G')
        meeting->type = 'G';
    else if ((c & 0x5F) == '\n')
        meeting->type = '\0';
    meeting_update(con, meeting);
}

/* Function Name: admin_mode_query
 * Description: queries the admin for his wise input
 * Inputs: meeting_t
 * Output: void
 */

void admin_mode_query(MYSQL *con, meeting_t *meeting)
{
	int breakout = 0;
	while (!breakout) {
        meeting_recall(con, meeting);
		printf("\033[2J\033[H");
		printf(csi_row1_message);
		printf("\033[1;31mUltra Secret Admin Mode\033[0m\n");
		printf("Choose wisely from the following options:\n");
		printf("\033[1mD\033[0mescription: ");
		printf("%s\n", meeting->description);
		printf("\033[1mT\033[0mype: ");
		printf("%s\n", meeting_type_string(meeting->type));
		printf("\033[1mR\033[0maffle generation\n");
		printf("generate mail bcc \033[1mL\033[0mists\n");
		printf("Enter leaves this top-secret mode.\n");
		while (1) {
			printf("\033[1m(D, T, R, L)\033[0m > ");
			char c = getsinglechar(stdin);
			if ((c & 0x5F) == 'T') {
				query_mtg_type(con, meeting);
				break;
			}

			if ((c & 0x5F) == 'D') {
				query_mtg_description(con, meeting, maxbuf-1);
				break;
			}

			if ((c & 0x5F) == 'R') {
				/*raffle_gen();*/
				break;
			}
			if ((c & 0x5F) == 'L') {
				/*list_gen_select();*/
				break;
			}

			if (c == '\n') {
				breakout = 1;
				break;
			}

			printf("\033[9HInvalid! ");
		}
	}
}

/* Function Name: print_member_information
 * Description: prints the member's information
 * Inputs: &member
 * Outputs: none
 */
void print_member_information(member_t *member) {
	printf("\033[2J\033[H");
	printf(csi_row1_message);
	printf("Confirm your information.\n");
	printf("\033[1mL\033[0mast Name: %s\n", member->lname);
	printf("\033[1mF\033[0mirst Name: %s\n", member->fname);
	printf("\033[1mE\033[0mmail: %s\n", member->email);
	printf("SID: %i\n", member->sid);
	if (member->mn)
		printf(csi_org " \033[1mM\033[0member: %lli\n", member->mn);
	else
		printf(csi_org " \033[1mM\033[0member: Not " csi_org_a " member yet\n");

}

/* Function Name: _confirm_change_information_helper_
 * Description: the main loop of confirm change information
 * Inputs: &mysql, &member, &meeting, &breakout
 * Output: void
 */
static void _confirm_change_information_helper_ (MYSQL *con,
        member_t* member, meeting_t *meeting, int* breakout)
{
	while (1) {
		printf("Press enter to continue or choose a field: ");
		char c = getsinglechar(stdin);
		if ((c & 0x5F) == 'L') {
			*member->lname = 0;
			while(!*member->lname) {
				char buf[maxbuf];
				if (!query_lname(buf, maxbuf-1))
					*member->lname = '\0';
				else
					strcpy(member->lname, buf);
			}
			break;
		}
		if ((c & 0x5F) == 'F') {
			*member->fname = 0;
			while(!*member->fname) {
				char buf[maxbuf];
				if (!query_fname(buf, maxbuf-1))
					*member->fname = '\0';
				else
					strcpy(member->fname, buf);
			}
			break;
		}
		if ((c & 0x5F) == 'E') {
			*member->email = 0;
			while(!*member->email) {
				char buf[maxbuf];
				if (!query_email(buf, maxbuf-1))
					*member->email = '\0';
				else
					strcpy(member->email, buf);
			}
			break;
		}
		if ((c & 0x5F) == 'M') {
			member->mn = 0;
			if (!member->mn) {
				char buf[maxbuf];
				if(!query_imn(buf, maxbuf-1))
					member->mn = 0;
				else
					member->mn = atoll(buf);
			}
			break;
		}
		if ((c & 0x5F) == 'A' && (member->admin & 0x5F) == 'Y') {
			admin_mode_query(con, meeting);

			break;
		}
		if (c == '\n') {
			*breakout = 1;
			break;
		}
		printf("\033[9H");
		printf("Invalid! ");
	}
}

/* Function Name: confirm_change_information
 * Description: Confirm or change your information!
 * Inputs: &MYSQL, &member, &meeting
 * Output: void
 */
void confirm_change_information(MYSQL *con, member_t *member,
        meeting_t *meeting)
{
	int breakout = 0;
	while(!breakout) {
		print_member_information(member);
		printf("\033[1m(L,F,E,M)\033[0m \n");

		_confirm_change_information_helper_(con, member, meeting, &breakout);

	}
}

int main()
{
	MYSQL *mysqlp = 0;

	meeting_t meeting;

	init_meeting(&meeting);
		
	if(!(mysqlp = mysql_init(mysqlp)))
		exit(1);


	if (!mysql_real_connect(mysqlp, csi_mysql_server, csi_mysql_user,
				csi_mysql_password, csi_mysql_database, csi_mysql_port,
				csi_mysql_unix_socket, csi_mysql_client_flag)) {
		finish_with_error(mysqlp);
	}

	meeting.exists = !meeting_recall(mysqlp, &meeting);

	mysql_close(mysqlp);

	while (1) {
		member_t member;
		init_member(&member);

		unsigned long long new_card = 0;

		information_gather(&member, new_card);

	    MYSQL *mysqlp = 0;

		if(!(mysqlp = mysql_init(mysqlp)))
			exit(1);

		if (!mysql_real_connect(mysqlp, csi_mysql_server, csi_mysql_user,
					csi_mysql_password, csi_mysql_database, csi_mysql_port,
					csi_mysql_unix_socket, csi_mysql_client_flag)) {
			finish_with_error(mysqlp);
		}

		database_check(mysqlp, &member);

		information_gather2(&member);

		card_number_verification(new_card, &member);

		confirm_change_information(mysqlp, &member, &meeting);

		database_entry(mysqlp, &member);

		mysql_close(mysqlp);

		free_member(&member);
	}

	free_meeting(&meeting);

	return 0;
}
