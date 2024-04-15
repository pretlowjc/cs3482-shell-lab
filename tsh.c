/*
 * tsh - A tiny shell program with job control
 *
 * <Put your name(s) and login ID(s) here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "jobs.h"     //prototypes for functions that manage the jobs list
#include "wrappers.h" //prototypes for functions in wrappers.c
//#include <string>


/* Misc manifest constants */
#define MAXLINE 1024 /* max line size */
#define MAXARGS 128  /* max args on a command line */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output (-v option) */
char sbuf[MAXLINE];      /* for composing sprintf messages */

/* Here are the prototypes for the functions that you will
 * implement in this file.
 */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);
void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Routines in this file that are already written */
int parseline(const char *cmdline, char **argv);
void sigquit_handler(int sig);
void usage(void);

/*
 * main - The shell's main routine
 *        The main is complete. You have nothing to do here.
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
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h': /* print help message */
            usage();
            break;
        case 'v': /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':            /* don't print a prompt */
            emit_prompt = 0; /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT, sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1)
    {

        /* Read command line */
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin))
        { /* End of file (ctrl-d) */
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

/* TODO:
 * Your work involves writing the functions below.  Plus, you'll need
 * to implement wrappers for system calls (fork, exec, etc.) Those wrappers
 * will go in wrappers.c.  The prototypes for those are in wrappers.h.
 *
 * Do NOT just start implementing the functions below.  You will do trace file
 * by trace file development by following the steps in the lab.
 * Seriously, do not do this any other way. You will be sorry.
 *
 * For many of these functions, you'll need to call functions in the jobs.c
 * file.  Be sure to take a look at those.
 */

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
{
    char *argv[MAXARGS];
    char buf[MAXLINE];

    // int status, i; // compiler say this is unused.
    // sigset_t prev_all; // compiler says this is unused.
    sigset_t mask_all, mask_one, prev_one;
    Sigfillset(&mask_all);

    int bg;
    pid_t pid;

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    if (argv[0] == NULL)
    {
        return;
    }

    if (!builtin_cmd(argv))
    {
        // Parent process blocks SIGCHLD
        Sigemptyset(&mask_one);
        Sigaddset(&mask_one, SIGCHLD);
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);

        // Spawn a child process
        // This is also the child process logic.
        if ((pid = Fork()) == 0)
        {
            // Child process restores all signals (for itself).
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            setpgid(0, 0);
            Exec(argv[0], argv, environ);
        }

        // Parent progress
        // Block all signals, add child to job list, restore all signals (including SIGCHLD).

        if (!bg)
        {
            // int status; // compiler says that it is unused.
            Sigprocmask(SIG_BLOCK, &mask_all, NULL);
            addjob(jobs, pid, FG, cmdline);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            waitfg(pid);
        }
        else
        {
            Sigprocmask(SIG_BLOCK, &mask_all, NULL);
            addjob(jobs, pid, BG, cmdline);
            int jid = pid2jid(pid);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            printf("[%d] (%d) %s", jid, pid, cmdline);
            // do background process
        }
    }
    return;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
int builtin_cmd(char **argv)
{
    if (!strncmp(argv[0], "quit", 4))
    {
        exit(0);
    }

    if (!strncmp(argv[0], "jobs", 4))
    {
        listjobs(jobs);
        return (1);
    }
    if (!strncmp(argv[0], "bg", 2))
    {
        do_bgfg(argv);
        return (1);
    }
    if (!strncmp(argv[0], "fg", 2))
    {
        do_bgfg(argv);
        return (1);
    }
    /*
     * runs builtins, and return 1
     */
    return 0; /* not a builtin command */
}

/*
    do_bgfg - Execute the builtin bg and fg commands

    Add code to grab either the job id or the pid depending upon whether
    the parameter starts with a % or not.

    If the parameter is a job id, then uses the getjobjid function to get the pid.

    If argv[0] is "bg" then use the kill command to send the SIGCONT signal to every
    process in the job. Also, set the state of the job to BG.

    (Note: sending a SIGCONT signal to a child process will cause a SIGCHLD signal
    to be sent to the parent process (the shell) just like SIGINT and SIGTSTP, but
    we're not going to do anything with those particular SIGCHLD signals in order
    to keep the code simpler.)

    Finally, print the the job number in [ ], the pid number in (), and the original
    command line.  (See on ASULearn page.)

    The original command line is available in the jobs array.  Before you modify the
    job state, block all signals and restore them afterward, for reasons mentioned earlier.
*/
void do_bgfg(char **argv)
{
    pid_t pid;
    int jid;
    sigset_t mask_all, prev_all;
    Sigfillset(&mask_all);
    job_t * job;
    char * p;

    // If no parameter...
    if (argv[1] == NULL) {
        printf("%s command requires PID or %%jobid argument\n", argv[0]); 
        return;
    }
    
    

    // Get whether the second parameter is a jid or a pid;
    // Retrieve and store that value.
    if (!strncmp(argv[1], "%", 1))
    {
        //use the getjobjid function to get the pid.
        jid = strtol((argv[1] + 1), &p, 10);
        job = getjobjid(jobs, jid);
        // Make sure numerical conversion worked fine.
        if (p == argv[1]) {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            return;
        }

        // Make sure the job exists.
        if (job == NULL) {
            printf("%s: No such job\n", argv[1]);
            return;
        }
        pid = job->pid;
    } 
    else
    {
        // use the kill command to send the SIGCONT signal to every process in the job.
        pid = (pid_t) strtol(argv[1], &p, 10);
        jid = pid2jid(pid);
        job = getjobpid(jobs, pid);
        // Make sure numerical conversion worked fine,
        if (p == argv[1]) {
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            return;
        }

        // Make sure the process exists.
        if (job == NULL) {
            printf("(%s): No such process\n", argv[1]);
            return;
        }
    }
    


    // Check whether we ran bg or fg
    if (!strncmp(argv[0], "bg", 2)) {
        
        // If the second argument (parameters to bg) begin with '%'...
        
        Kill(-pid, SIGCONT);
        
        /*
        // before you modify the job state, block all signals and restore them afterward.
        deletejob(jobs, pid);
        */

        // set the state of the job to BG.
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        job->state = BG;
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        
        // print the job number in [], the pid number in (), and the original command line.
        // the original command line is available in the jobs array.
        // unsure about the cmdline part for the print statement!
        
        printf("[%d] (%d)  %s", jid, pid, job->cmdline);
    }
    
    // If fg...
    if (!strncmp(argv[0], "fg", 2)) {
        
        // If job is stopped, continue and set to FG.
        // If job state is BG, set to FG.
        if (job->state == ST) {
            Kill(-pid, SIGCONT);
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            job->state = FG;
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        if (job->state == BG) {
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            job->state = FG;
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }

        waitfg(pid);
    }

   return;
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    while (pid == fgpid(jobs))
    {
        sleep(1);
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
    int status;
    // int i; // compiler says that this is unused.
    pid_t pid;
    sigset_t mask_all, prev_all;
    Sigfillset(&mask_all);

    while ((pid = waitpid(-1, &status, (WNOHANG | WUNTRACED))) > 0)
    {
        if (WIFEXITED(status))
        {
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(jobs, pid);
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        if (WIFSIGNALED(status))
        {
            int jid = pid2jid(pid);
            printf("Job [%d] (%d) terminated by signal %d\n", jid, pid, WTERMSIG(status));
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(jobs, pid);
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        if (WIFSTOPPED(status))
        {
            int jid = pid2jid(pid);
            // if so, print out a message to indicate the child was stopped.
            printf("Job [%d] (%d) stopped by signal %d\n", jid, pid, WSTOPSIG(status));
            /*
            Change the state of the job in the jobs array to stopped(ST).
            You can use the WSTOPSIG macro to retrieve the number of the signal that
            was sent to the child that caused it to stop.

            You can use either the getjobpid or the getjobjid to get a pointer to the job
            in order to change the state.
            */
            if (WSTOPSIG(status))
            {
                job_t *changedState = getjobjid(jobs, jid);
                changedState->state = ST;
            }
            else
            {
                Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
                deletejob(jobs, pid);
                Sigprocmask(SIG_SETMASK, &prev_all, NULL);
            }
        }
    }
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigint_handler(int sig)
{
    pid_t pid = fgpid(jobs);
    if (pid == 0)
    {
        return;
    }
    Kill(-pid, SIGINT);
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig)
{
    pid_t pid = fgpid(jobs);
    if (pid == 0)
    {
        return;
    }
    // use the negative because it won't kill the children.
    Kill(-pid, SIGTSTP);
    return;
}

/*************************************
 *  The rest of these are complete.
 *************************************/

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
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
    buf[strlen(buf) - 1] = ' '; /* replace trailing '\n' with space */
    while (*buf && (*buf == ' '))
        buf++; /* ignore leading spaces */

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }
    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' '))
            buf++; /* ignore spaces */

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0)
        return 1; /* ignore blank line */

    /* should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
    {
        argv[--argc] = NULL;
    }
    return bg;
}

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    //-v enables verbose
    printf("   -v   print additional diagnostic information\n");
    // the tester uses the -p option
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}
