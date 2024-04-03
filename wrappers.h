/* These are already defined in wrappers.c */
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);  //sigaction function wrapper

/* You'll need to supply wrappers for these. */
pid_t Fork(void);
void Exec(char *, char **, char **);
int Sigprocmask(int option, sigset_t *newmask, sigset_t *prevmask);
int Sigemptyset(sigset_t *mask);
int Sigaddset(sigset_t *mask, int option);
int Sigfillset(sigset_t *mask);
int Kill(pid_t pid, int signal);

