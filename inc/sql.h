#ifndef __IEEE_MYSQL__
#define __IEEE_MYSQL__

#include <unistd.h>
#include <mysql.h>
#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <string.h>
#include "member.h"
#include "meeting.h"

extern char* mtg_description;

/* Function Name: finish_with_error
 * Description: finishes the program with a mysql error
 * Inputs: mysql connection pointer
 * Outputs: nothing
 */
void finish_with_error(MYSQL *con)
{
	fprintf(stderr, "%s\n", mysql_error(con));
	mysql_close(con);
	exit(1);        
}

/* Function Name: _database_entry_helper_
 * Description: concatenates the user information into a DB enterable form --
 *     Does not input leading or trailing spaces.  (although they won't harm)
 * Inputs: buffer, mysql connection, member_t
 * Outputs: a character string
 */
static int _database_entry_helper_(char* buf, MYSQL *con, member_t *member)
{
	char* c = buf;

	c = (char*) strmov(c, "lname='");
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
	c = (char*) strmov(c, ", mn=");
	if (member->mn)
		c += sprintf(c, "%lli", member->mn);
	else
		c = (char*) strmov(c, "NULL");
	/* Just to make sure */
	*(c++) = '\0';

	return 0;
}

/* Function Name: database_entry
 * Description: Inputs the stuff into the database.
 * Inputs: mysql connection, member_t
 * Outputs: pretty much error conditions...
 */
int database_entry(MYSQL *con, member_t *member)
{
	char string[maxquerylen], *c, userinfo[maxquerylen];

	_database_entry_helper_(userinfo, con, member);

	c = (char*) strmov(string, "INSERT INTO member SET ");

	c = (char*) strmov(c, userinfo);

	c = (char*) strmov(c, " ON DUPLICATE KEY UPDATE ");

	c = (char*) strmov(c, userinfo);

	if (mysql_real_query(con, string, (unsigned int) (c - string))) {
		finish_with_error(con);
	}

	c = (char*) strmov(string, "UPDATE member SET"
			" meeting_count=meeting_count+1 WHERE (sid=");
	c += sprintf(c, "%i", member->sid);
	c = (char*) strmov(c, ") and ("
		"(SELECT type from meeting WHERE meeting=curdate()) is not "
		"null) and ((SELECT 1 FROM attendee WHERE meeting=curdate()"
		" and sid=");
	c += sprintf(c, "%i", member->sid);
	c = (char*) strmov(c, ") is null)");

	if (mysql_real_query(con, string, (unsigned int) (c - string))) {
		finish_with_error(con);
	}

	c = (char*) strmov(string, "INSERT IGNORE attendee SET "
			"meeting=curdate(),sid=");
	c += sprintf(c, "%i", member->sid);

	if (mysql_real_query(con, string, (unsigned int) (c - string))) {
		finish_with_error(con);
	}

	return 0;
}

/* Function Name: meeting_recall
 * Description: sees if there is a meeting already in place and warns if not
 * Inputs: mysql connection, meeting_t
 * Outputs: int: negative for error (probably not checked here), 0 for exist
 *  1 for does not exist
 */

int meeting_recall(MYSQL *con, meeting_t *meeting)
{
	char string[maxquerylen], *c;	

	c = (char*) strmov(string, "SELECT type,description from meeting "
		"WHERE meeting=curdate()");

	if (mysql_real_query(con, string, (unsigned int) (c - string))) {
		finish_with_error(con);
	}

	MYSQL_RES *result = mysql_store_result(con);

	if (result == NULL) {
		finish_with_error(con);
	}

	MYSQL_ROW row = mysql_fetch_row(result);

	if (!row)
		return 1;

	meeting->type = row[0] ? *row[0] : '\0';
	strcpy(meeting->description, row[1]);

	return 0;

}

/* Function Name: meeting_update
 * Description: Modifies the current meeting information
 * Inputs: mysql connection, meeting_t
 * Outputs: void
 */

void meeting_update(MYSQL *con, meeting_t *meeting)
{
	char string[maxquerylen], *c;

	c = (char*) strmov(string, "REPLACE INTO meeting SET meeting=curdate(),");
	c = (char*) strmov(c, "description=\"");
	c += mysql_real_escape_string(con, c, meeting->description,
			strlen(meeting->description));
    c = (char*) strmov(c, "\",type=");
    if (isalpha(meeting->type))
        c += sprintf(c, "'%c'", meeting->type);
    else
        c = (char*) strmov(c, "NULL");

	if (mysql_real_query(con, string, (unsigned int) (c - string))) {
		finish_with_error(con);
	}
}

/* Function Name: database_check
 * Description: Checks database for this specific member.
 * Inputs: member, mysql connection
 * Output: int return condition
 */
int database_check(MYSQL *con, member_t *member)
{
	char buf[maxbuf];
	sprintf(buf,
			"SELECT lname,fname,email,sid,cn,mn,admin FROM member WHERE sid = %i;",
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
	member->mn = row[mn_row] ? atoll(row[mn_row]) : 0;
	member->admin = row[admin_row] ? *row[admin_row] : 0;

	return 0;
}

#endif /* __IEEE_MYSQL__ */
