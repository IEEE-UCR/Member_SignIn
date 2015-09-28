#ifndef __IEEE_MEETING__
#define __IEEE_MEETING__

#define general_meeting 'G'

/* If the meeting has no type, then it is clearly a test meeting. */

typedef struct
{
	int exists;
	char type;
	char *description;
} meeting_t;

/* Function Name: init_meeting
 * Description: initializes the "meeting" struct
 * Input: meeting_t pointer
 * Output: nothing
 */

void init_meeting(meeting_t *meeting)
{
	meeting->description = (char*) malloc(maxbuf);

	if (meeting->description == 0) {
		exit(1);
	}

	meeting->exists = 0;
	meeting->type = '\0';
	*meeting->description = '\0';
}

void free_meeting(meeting_t *meeting)
{
	free(meeting->description);
}

char *meeting_type_string(char type)
{
	switch(type) {
		case 'G':
			return "General Meeting";
			break;
		default:
			return "Testing";
			break; 
	}
}

#endif //ifndef __IEEE_MEETING__
