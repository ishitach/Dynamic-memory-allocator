/* 
 * tsh - A tiny shell program with job control
 * 
 * <Name: Ishita Chourasia ;          >
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{		struct job_t *job;
	char *argv[MAXARGS];  /*argv vector is filled in by parseline function given 
	below.It provides arguments needed to launch a process for execve() function*/
	pid_t child_pid; //contins process id
	int backgrnd;/*determines whether a job should run in bg or fg.returns 1 if job 
	runs in bg and 0 if it runs in fg*/
	backgrnd = parseline(cmdline, argv); /*Copies contents of cmdline into argv 
	and returns whether the job should run in background or foreground*/
	if (argv[0] == NULL) return;   // ignore empty lines 
	sigset_t mask;	
		/* Set up a mask so that it blocks signals while a job is being added
	 * A job should not be deleted before it is added.
	 

	 */

	sigemptyset(&mask);//Generate an empty signal set in mask
	sigaddset(&mask, SIGCHLD);//add sigchld to blocked signal sets
	sigaddset(&mask, SIGINT);//add sigint to blocked signal sets
	sigaddset(&mask, SIGTSTP);//add sigstp to blocked signal sets
	
	    if(!builtin_cmd(argv)){/*Checks whether command is built-in and 
	    executes it if yes, else enters if block*/
        sigprocmask(SIG_BLOCK, &mask, NULL); /*Blocks the signal set  in order 
        to avoid the race condition where the child is reaped by handleSIGCHLD,
        handlesigint,handlesigstp
        (and thus removed from the job list) before the parent gets to 
        add it to the job list*/
         	if ((child_pid = fork()) == 0) { /*forking a child if not a built in 
         	command
 	as child runs user job */
 	    sigprocmask(SIG_UNBLOCK, &mask, NULL);/*Unblock the signal sets 
 	    in child since child had earlier inherited blocked vectors from its parent*/
 	setpgid(0,0); /*setting the process group id identical to childs pid.This 
 	ensures that there will be only one process—your tsh shell—in the 
 	foreground process group. When you type ctrl-c, the shell should 
 	catch the SIGINT and then forward it to the appropriate 
 	foreground job (or more precisely, the process group that contains the 
 	foreground job). //New jobs should have new process ids else signal will kill shell also */         
 	if (execve(argv[0], argv, environ) < 0) { //returns -1 on error
 	printf("%s: Command not found.\n", argv[0]);
 	exit(0);
 	}
         	}
 	       if(backgrnd){              //If it is a background task
            addjob(jobs, child_pid, BG, cmdline);  //Add the process to jobs
            sigprocmask(SIG_UNBLOCK, &mask, NULL);    //Unblock the signal set afet adding the job
            job = getjobpid(jobs, child_pid);   //Get the specific job details
            printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline); /*Print the details of 
            specified job*/
        }
        /*parent waits for the job to terminate*/
                if(!backgrnd){         //If it is foreground task, 
            addjob(jobs, child_pid, FG, cmdline);  //Add the process to jobs
            sigprocmask(SIG_UNBLOCK, &mask, NULL);  //Unblock the signal set afet adding the job
            waitfg(child_pid);  //Parent waits for the foreground process to terminate as there cn be atmost only on e foreground proces
        }
         	}
    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
	//It will also check that argument is a built in command and will ignore if "&" is the first char;
	//this will exit if command is quit; will list jobs if command is jobs;will do_bgfg if command is bg or fg 
	//for all these argv returns 1 otherwise will return 0

	    if(!strcmp(argv[0], "quit"))  	//will exit immediately if command is quit
	      exit(0);

	    else if(!strcmp(argv[0], "&"))	//will ignore if the first char is "&"
	      return 1; 

	    else if(!strcmp(argv[0], "jobs"))	//will list all the ongoing jobs, if the command is jobs
	    {
		listjobs(jobs);
		return 1;
	    }

	    else if(!strcmp(argv[0], "fg"))	//the shell puts the current job into foreground process, if the command is fg
	    {
		do_bgfg(argv);
		return 1;
	    }
	    
	    else if(!strcmp(argv[0], "bg"))	//the shell puts the current job into background process, if the command is bg
	    {
		do_bgfg(argv);
		return 1;
	    }


    return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
	char* l;
	int jobid;

    if(argv[1] == NULL)			//if argument NULL is passed after fg or bg and pid/jid is missing 
    {
        printf("%s the command requires PID or %%jobid argument\n",argv[0]);
        return;
    }
    


 if (argv[1][0] == '%') {//if its jid
    int jid = atoi(&argv[1][1]);
    if (!(bgfg_job = getjobjid(jobs, jid))) {//if  no such jid  exists then throw error
      printf("%s: No such job with given job-id\n", argv[1]);
      return;
    }
	else
			{
				if(!strcmp(argv[0],"bg"))//making a transition from stopped state to BG state
				{
					bgfg_job->state = BG;//change job state to bg
					kill(-(bgfg_job->pid),SIGCONT);//send sigcont signal
					printf("[%d] (%d) %s",bgfg_job->jid,bgfg_job->pid,bgfg_job->cmdline);//print status
				}
				if(!strcmp(argv[0],"fg"))//making a transition from stopped or background stage to FG state*/
				{
					bgfg_job->state = FG;//change job sate to fg
					kill(-(bgfg_job->pid),SIGCONT);//send sigcont signal
					waitfg(bgfg_job->pid);//wait for that job to terminate cz we cn hv only 1 foreground job running at a particular time
				}
			}	
  }
  	 
     else if (isdigit(argv[1][0])) {//if its pid
    pid_t pid = atoi(argv[1]);
    if (!(bgfg_job = getjobpid(jobs, pid))) {// if no such pid exists then throw error
      printf("(%d): No such process with given pid\n", pid);
      return;
    }
	else
				{
					if(strcmp(argv[0],"bg")==0)//making a transition from stopped state to BG state
					{
						bgfg_job->state = BG;//change job state to bg
						kill(-(bgfg_job->pid),SIGCONT);//send sigcont signal
						printf("[%d] (%d) %s",bgfg_job->jid,bgfg_job->pid,bgfg_job->cmdline);//print status
					}
					if(strcmp(argv[0],"fg")==0)//making a transition from stopped or background stage to FG state
                                	{
                                        	bgfg_job->state = FG;//change job sate to fg
                                        	kill(-(bgfg_job->pid),SIGCONT);//send sigcont signal
                                        	waitfg(bgfg_job->pid);//wait for that job to terminate cz we cn hv only 1 foreground process
                                	}
				}
					
			}
			else
			{
				printf("%s: argument must be a PID or %%job-id\n",argv[0]);//enter either a valid pid or a valid jid
			}   
    
    
    /*
    else {			//parsing the jid/pid
	    	    
	      if((l = strstr(argv[1],"%"))!=NULL)	//for containing jobpid arguments
	      {
		       l++;
		       jobid=atoi(l);
		        if(getjobjid(jobs,jobid)!=NULL)     
			  jobid=getjobjid(jobs,jobid)->pid;
			  
		        else
			{
			  printf("%%[%d]: no such job with the provided jid-id\n",atoi(l));//if the arguments entered contains false jobpid
			  return; 
		        }
	      }
	      
	      else if(isdigit(argv[1][0]))
	      {
	       
		jobid=atoi(argv[1]);    //argv[1] is the pid.
		if(getjobpid(jobs,jobid)==NULL)
		  {
		    printf("(%d) no such process\n",jobid);	//no pid exists
		    return;
		  }
	      }
	      else
		{
		 printf("%s: argument must be a PID or %%jobid\n",argv[0]);//invalid pid
		 return;
		}
	      
	      kill(-jobid,SIGCONT);    //continuing the stopped execution

	     if(strcmp(argv[0],"fg")==0) //for fg argument
	     {
	       getjobpid(jobs,jobid)->state=FG; //set the state of foreground jobs
	       waitfg(jobid);//waiting for foreground jobs to terminate
	     }
	     else
	     {
	      getjobpid(jobs,jobid)->state =BG;//set the state of background jobs 
	      printf("[%d] (%d) %s",getjobpid(jobs,jobid)->jid,getjobpid(jobs,jobid)->pid,getjobpid(jobs,jobid)->cmdline);
	     }

	}*/
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
	//This will wait until program is out of FG and will sleep till state of given pid job is defined as FG
	
	 struct job_t *job_foreg;
	 job_foreg = getjobpid(jobs,pid);	//get all the foreground jobs

	    if(!job_foreg)			//checking the existance of job
      		return;  			//return without doing anything

	    while(job_foreg->pid == pid && job_foreg->state == FG)	//if job is FG
		{
		      sleep(1);			//sleep till the job is in foreground
		}	

    return;
		
}

/*****************
 * Signal handlers
 *****************/
/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{

	pid_t pid;
	int child_jid;
	int status = -1;
	
	  while((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0) //handling child processes as per their status
	  {
			//will check if any foreground job is terminated/stopped/completed
	    		// the delete job signal for sigchld
			if (WIFEXITED(status)) 
			{ 
				deletejob(jobs, pid); //deleting jobs if exited normally
			}
			
			else if( WIFSIGNALED(status) ){ 
				child_jid = pid2jid(pid);
				deletejob(jobs,pid);  // Deleting jobs which are terminated with signals
				printf("Job [%d] (%d) terminated by signal %d \n",pid2jid(pid),pid,getjobpid(jobs, pid)->pid);

//            printf("Job [%d] (%d) terminated by signal %d\n",child_jid,pid,WTERMSIG(status));
			
			}
			
			else if( WIFSTOPPED(status) ){
				printf("Job [%d] (%d) stopped by signal %d \n",pid2jid(pid),pid,getjobpid(jobs, pid)->pid);
			struct	job_t *changes = getjobpid(jobs, pid);
				changes -> state = ST;	//changing the state of stopped jobs
			}
			
		}
	return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) //SIGINT is sent to the shell when Ctrl-C is pressed by the user
{
	pid_t forep=fgpid(jobs);
	int jidp=pid2jid(forep);
	    if(forep==0||jidp==0)
		return;
	
		//send fg job or related process group signal	
		
	if(forep > 0)
	{
	
	    kill(-forep,SIGINT);   //send kill signal to process group and kill the foreground job and its children(having same group pid)
	    printf("Job [%d] (%d) terminated by signal %d\n",jidp,forep,SIGINT);
	    deletejob(jobs,forep); //delete the job
	    waitpid(forep,NULL,0); // reap the process
	}
	
	else
	{
		// do nothing since there is no ongoing foreground
	}

    return; 
    
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{  
    pid_t forep=fgpid(jobs);
    int jidp=pid2jid(forep);
    
    if(forep==0||jidp==0)
        return;

	//send fg job or related process group signal	
    if(forep > 0)
    {
    	
    	struct job_t *job = getjobpid(jobs,forep);
		job->state = ST;  	//change state of the child and stopped the signal
    		kill(-forep,SIGTSTP);	// stop the foreground job and its children (having same group pid) and sending 	
    					//SIGSTP signal to all stopped process
    		
    printf("Job [%d] (%d) stopped by signal %d\n",jidp,forep,SIGTSTP);
    
    }

    return; 
    
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}
