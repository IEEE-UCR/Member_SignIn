#ifndef __IEEE_MEMBER__
#define __IEEE_MEMBER__

#define def_num_fields 7
#define lname_row 0
#define fname_row 1
#define email_row 2
#define sid_row 3
#define crd_row 4
#define mn_row 5
#define admin_row 6

/* Struct Name: member */
typedef struct
{
	char *lname;
	char *fname;
	char *email;
	unsigned int sid;
	unsigned long long crd;
	unsigned long long mn;
	char admin;
} member_t;

/* Function Name: init_member
 * Description: Initializes the "member" struct
 * Input: member_t pointer
 * Output: return status
 */
int init_member(member_t *member)
{
	/* Allocate */
	member->lname = (char*) malloc(maxbuf);
	member->fname = (char*) malloc(maxbuf);
	member->email = (char*) malloc(maxbuf);

	/* Zero */
	*member->lname = '\0';
	*member->fname = '\0';
	*member->email = '\0';
	member->sid = 0;
	member->crd = 0;
	member->mn = 0;
	member->admin = 0;

	/* Check it */
	if (!(member->lname))
		return 1;

	if (!(member->fname))
		return 1;

	if (!(member->email))
		return 1; 

	return 0;
}

void free_member(member_t *member)
{
	/* Deallocate */
	free(member->lname);
	free(member->fname);
	free(member->email);
}

#endif /* __IEEE_MEMBER__ */
