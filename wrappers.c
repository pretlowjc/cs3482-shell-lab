#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "wrappers.h"

//Implement the missing wrappers (for fork, exec, sigprocmask, etc.)
//and call them in your tsh code.
//
//Note that exec does not return a value (void function). If an error
//occurs, it will simply output <command>: Command not found, 
//where <command> is the value of argv[0] and then call exit. (See code in figure 8.24.)
//The other wrappers will be in the style of the Fork wrapper in section 8.3. 
//
//The headers for all of the wrappers (including the missing ones) are
//in wrappers.h
//
//Note you will not need a wrapper for waitpid since you have to check
//the return value of that in your code anyway.

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

/*  Add the missing wrappers for: fork, exec, sigprocmask, sigemptyset,
 *  sigfillset, and kill
 */
pid_t Fork(void) {
    pid_t pid;

    if ((pid = fork()) < 0) {
        unix_error("Fork error");
    }
    
    return pid;
}

//Execute a program
//Doesn't return unless there is an error
void Exec(char *file, char **argv, char **environ) {
    if (execve(file, argv, environ) < 0) {
        printf("%s: Command not found\n", file);
        exit(0);
    }
}

int Sigprocmask(int option, sigset_t *newmask, sigset_t *prevmask) {
    if (sigprocmask(option, newmask, prevmask) < 0) {
        unix_error("sigprocmask error");
    }
    return 0;
}

int Sigemptyset(sigset_t *mask) {
    if (sigemptyset(mask) < 0) {
        unix_error("sigemptyset error");
    }
    return 0;
}

int Sigaddset(sigset_t *mask, int option) {
    if (sigaddset(mask, option) < 0) {
        unix_error("sigaddset error");
    }
    return 0;
}

int Sigfillset(sigset_t *mask) {
    if (sigfillset(mask) < 0) {
        unix_error("sigfillset error");
    }
    return 0;
}

int Kill(pid_t pid, int signal) {
    if (kill(pid, signal) < 0) {
        unix_error("kill error");
    }
    return 0;
}
