/*- -*- mode: c; c-basic-offset: 4; -*-
 *
 * The public interface to the students' memory allocator.
 */

int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);

/* 
 * Students work in teams of one or two.  Teams enter their team name, personal
 * names and login IDs in a struct of this type in their mm.c file.
 */
typedef struct {
    char *teamname; 
    char *name1;    
    char *id1;      
    char *name2;    
    char *id2;      
} team_t;

extern team_t team;
