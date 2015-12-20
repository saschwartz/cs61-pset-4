#include "sh61.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>


sig_atomic_t sig_received = 0;

pid_t foreground = 0;

// struct command
//    Data structure describing a command. Add your own stuff.

typedef struct command command;
typedef struct redirect redirect;

struct command {
    int argc;      // number of arguments
    char** argv;   // arguments, terminated by NULL
    pid_t pid;     // process ID running this command, -1 if none
    int bg;        // background job? 
    int status;     // store status of waitpid
    command* next;  // the next command
    command* prev;  // the previous command
    int condition_type;  // the type of the next condition
    redirect* redirection; // pointer to redirection linked list
};

/*
 * A struct to hold all the possible redirection commands
 */
struct redirect {
    char* token;    // the token
    char* file;     // the file to redirect to/from
    redirect* next; // the next redirect node
};


// command_alloc()
//    Allocate and return a new command structure.

static command* command_alloc(void) {
    command* c = (command*) malloc(sizeof(command));
    c->argc = 0;
    c->argv = NULL;
    c->pid = -1;
    c->next = NULL;
    c->prev = NULL;
    c->bg = 0;
    c->condition_type = -2;
    c->redirection = NULL;
    return c;
}


// allocate and return a redirect structure
static redirect* redirect_alloc(void) {
    redirect* red = (redirect*) malloc(sizeof(redirect));
    red->next = NULL;
    return red;
}

// signal handler for SIGINT
void handler(void) {
    sig_received = 1;
}

// command_free(c)
//    Free the linked list command structure `c`, including all its words, 
//    and redirection sublists

static void command_free(command* c) {
    
    command* trav = c;
    while (c != NULL) {
        for (int i = 0; i != c->argc; ++i)
            free(c->argv[i]);
        redirect* red;
        while(c->redirection != NULL) {
            red = c->redirection->next;
            free(c->redirection);
            c->redirection = red;
        }
        trav = c->next;
        free(c->argv);
        free(c);
        c = trav;
    }
}


// command_append_arg(c, word)
//    Add `word` as an argument to command `c`. This increments `c->argc`
//    and augments `c->argv`.

static void command_append_arg(command* c, char* word) {
    c->argv = (char**) realloc(c->argv, sizeof(char*) * (c->argc + 2));
    c->argv[c->argc] = word;
    c->argv[c->argc + 1] = NULL;
    ++c->argc;
}


// COMMAND EVALUATION

// start_command(c, pgid)
//    Start the single command indicated by `c`. Sets `c->pid` to the child
//    process running the command, and returns `c->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: The child process should be in the process group `pgid`, or
//       its own process group (if `pgid == 0`). To avoid race conditions,
//       this will require TWO calls to `setpgid`.

pid_t start_command(command* c, pid_t pgid) {
    (void) pgid;
    // Your code here!
    
/*    if (sig_received == 1)*/
/*        exit(0);*/
    
    // if command is a redirect
    if (strcmp(c->argv[0], "cd") == 0) {
        c->status = chdir(c->argv[1]);
        return 0;
    }
    
    // if not a background command set foreground to currnet pid
    if (c->bg == 0) {
        foreground = getpid();
    }
    
    // if we're in a pipe set foreground to current pgid
    if (c->prev != NULL && c->prev->condition_type != TOKEN_PIPE
        && c->condition_type == TOKEN_PIPE && c->bg == 0) {
        set_foreground(foreground);
        setpgid(foreground, foreground);
    }
    
    // if there is no command to run
    if (c->argv == NULL)
        return c->pid;
    
    // install the signal handler
    signal(SIGINT, &handler);
    
    // initialize status variable for waitpid call
    int status = 0;
    
    // create a redirect pointer to traverse redirect sublist with
    redirect* red = c->redirection;
    
    // initialize file descriptor incase we open any files for redirects
    int fd;
    
    // initialize the number of pipes we need to create to zero
    int pipes = 0;
    
    // create a command pointer to traverse command list
    command* trav = c; 
   
    // find out the number of pipes we need to create
    while(trav->condition_type == TOKEN_PIPE) {
        pipes++;
        trav = trav->next;
    }
    
    // create those pipe arrays
    int pipefd[pipes][2];
    
    // if we're at the begining of a pipeline
    if (c->condition_type == TOKEN_PIPE) {
       
        // create all the pipes we will need
        for (int i = 0; i < pipes; i++) {
            pipe(pipefd[i]);
            
            // fork the write end of the pipe
            if ((c->pid = fork()) == 0) {
                // if there are any redirections
                red = c->redirection;
                while(red != NULL) {
                    
                    // if redirect is > 
                    if (strcmp(red->token, ">") == 0) { 
                        if ((fd = open(red->file, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU)) == -1) {
                            printf("%s ", strerror(errno));
                            exit(1);
                        }
                        dup2(fd, STDOUT_FILENO);
                        close(fd);  
                    }   
                    
                    // if redirect is <
                    else if (strcmp(red->token, "<") == 0) {
                        if ((fd = open(red->file, O_RDONLY)) == -1) {
                            printf("%s ", strerror(errno));
                            exit(1);
                        }
                        dup2(fd, STDIN_FILENO);
                        close(fd);  
                    }
                    
                    // if redirect is 2>
                    else if (strcmp(red->token, "2>") == 0){
                        if ((fd = open(red->file, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU)) == -1) {
                            printf("%s ", strerror(errno));
                            exit(1);
                        }
                        dup2(fd, STDERR_FILENO);
                        close(fd);
                    }
                    red = red->next;
                }
                
                // create the write end of the pipe
                close(pipefd[i][0]);
                dup2(pipefd[i][1], STDOUT_FILENO);
                close(pipefd[i][1]);
                
                // run the command, checking for errors
                if (execvp(c->argv[0], c->argv) < 0) {
                    printf("%s: command not found. \n", c->argv[0]);
                    exit(0);
                }
            }
            
            // if there was an error with fork
            else if (c->pid == -1) {
                printf("error with fork: start_command");
            }
            
            else {
                
                // create the read end of the pipe
                close(pipefd[i][1]);
                dup2(pipefd[i][0], STDIN_FILENO);
                close(pipefd[i][0]);
                
                // go to the next command
                c=c->next;
            
            }
        }
    }
    
    // fork the last process in the pipe, or if not a pipe fork the process   
    c->pid = fork();
    
    // if child or parent
    switch(c->pid) {
        case 0: // child
            // handle any redirects
            red = c->redirection;
            while(red != NULL) {
                // if redirect is >
                if (strcmp(red->token, ">") == 0) {
                    // open the file, if error print error status and exit with exit(1)
                    if ((fd = open(red->file, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU)) == -1) {
                        printf("%s ",strerror(errno));
                        exit(1);
                    }
                    // copy fd into stdout
                    dup2(fd, STDOUT_FILENO);
                    
                    // close fd
                    close(fd);  
                }
                
                // if redirect is <
                else if (strcmp(red->token, "<") == 0) {
                    // open the file, if error print error status and exit with exit(1)
                    if ((fd = open(red->file, O_RDONLY)) == -1) {
                        printf("%s ", strerror(errno));
                        exit(1);
                    }
                    // copy fd into stdin
                    dup2(fd, STDIN_FILENO);
                    
                    // close fd
                    close(fd);  
                }
                
                // if redirect is 2>
                else if (strcmp(red->token, "2>") == 0){
                    // open the file, if error print error status and exit with exit(1)
                    if ((fd = open(red->file, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU)) == -1) {
                        printf("%s ", strerror(errno));
                        exit(1);
                    }
                    // copy fd into strerr
                    dup2(fd, STDERR_FILENO);
                    
                    // close fd
                    close(fd);
                }
                // go to the next redirect
                red = red->next;
            }
            
            // run the command, checking for errors            
            if (execvp(c->argv[0], c->argv) == -1) {
	            //printf(": command not found \n");
            }
            break;
        
        // if fork was unsuccesfull
        case -1:
            printf("fork failure: %d, %s\n", c->pid, *c->argv);
            break;
        
        // wait for child in parent process, checking for errors
        default:
            if(waitpid(c->pid, &status, 0) < 0)
                printf("waitfd: waitpid error: %s: process: %i", c->argv[0], c->pid);
    }
    
    // close all the pipes
    for (int i = 0; i < pipes; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }
    
    // set the status of c
    c->status = status;
    
    // return pid of process
    return c->pid;
}


// run_list(c) 
//    Run the command list starting at `c`.
//
//    PART 1: Start the single command `c` with `start_command`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in run_list (or in helper functions!).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Choose a process group for each pipeline.
//       - Call `set_foreground(pgid)` before waiting for the pipeline.
//       - Call `set_foreground(0)` once the pipeline is complete.
//       - Cancel the list when you detect interruption.

void run_list(command* c) { 
    
    // create pid for forking background processes
    pid_t pid; 
    
    // while there are commands still left to be run
    while(c != NULL) {
        // if we've received sig_int break out of loop
        if (sig_received == 1)
            break;
        
        // if we're running a background command
        else if (c->bg == 1) {
            
            // fork the process
            if ((pid = fork()) == 0) {
                // while there are background commands to be run
                while(c != NULL) {
                    // start the command
                    start_command(c,0);
                    
                    // if there is a pipeline, find last command in pipeline
                    // bc start commands runs an entire pipeline for us
                    while (c->condition_type == TOKEN_PIPE) {
                        c=c->next;
                    }   
                    
                    // if commands were &&'d together
                    if (c->condition_type == TOKEN_AND) {
                        
                        // if exit status is zero   
                        if (WEXITSTATUS(c->status) == 0)
                            // go to next command
                            c = c->next;
                        // else skip that command
                        else {
                            c->next->status = c->status;
                            c = c->next->next;
                        }
                    }  
                    
                    // if commands were ||'d together 
                    else if (c->condition_type == TOKEN_OR) {
                        // if exit status is not zero go to next command
                         if (WEXITSTATUS(c->status) != 0)
                            c = c->next;
                         // else skip next command 
                         else {
                             c->next->status = c->status;
                             c = c->next->next;
                         }
                     }
                    
                    // otherwise go to next command and return because 
                    // we've reached the end of the background command sequence
                    else {
                        c = c->next;
                        return;
                    }
                }
            }
            
            // if there was a fork error
            else if (pid == -1)
                printf("error in run_list: fork");
            // otherwise in parent find last command in background sequence
            // note: we don't wait so it will run in background
            else {
                while(c->condition_type != TOKEN_BACKGROUND) 
                    c=c->next;
                // then increment one past it
                c = c->next;
            }
        }
        
        // if not a background command - don't fork
        else {
            // start the command
            start_command(c,0);
            
            // if a pipline, find last command in pipeline
            while (c->condition_type == TOKEN_PIPE) {
                c=c->next;
            }
            
            // if commands were &&'d together
            if (c->condition_type == TOKEN_AND) {
                // if exit status is zero, go to next command
                if (WEXITSTATUS(c->status) == 0)
                    c = c->next;
                
                // else skip next command
                else {
                    c->next->status = c->status;
                    c = c->next->next;
                }
            }   
            
            // if commands were ||'d together
            else if (c->condition_type == TOKEN_OR) {
                // if exit status is not zero, go to next command
                if (WEXITSTATUS(c->status) != 0)
                    c = c->next;
                // else skip next command
                else {
                    c->next->status = c->status;
                    c = c->next->next;
                    }
            }
            
            // otherwise go to next command
            else {
                c = c->next;
            }
        }
    }
}            

    



// eval_line(c)
//    Parse the command list in `s` and run it via `run_list`.

void eval_line(const char* s) {
    int type;
    char* token;
    // Your code here!

    // build the command
    command* c = command_alloc();
    
    // create command pointers to traverse list
    command* start = c;
    command* previous = NULL;
    command* trav = NULL;
    
    // create a redirect pointer to traverse redirect list
    redirect* red;
    
    // boolean, if last token in command struct
    int last = 0;
    
    // while there are commands left to be parsed
    while ((s = parse_shell_token(s, &type, &token)) != NULL) {
    
        // if previous token was last in command
        if(last) {
            // alloc a new command struct
            c->next = command_alloc();
            
            // set prev to previous
            c->prev = previous;
            
            // incement previous
            previous = c;
            
            // increment c
            c = c->next;
            
            // append the token to incremented command struct
            command_append_arg(c, token);
            
            // no longer the last token in command
            last = 0;
        }
        
        // if token is of type TOKEN_REDIRECTION
        else if (type == TOKEN_REDIRECTION) {
            // allocate a redirect struct
            red = redirect_alloc();
            
            // set the token in that struct
            red->token = token;
            
            // get the file, incrementing s
            s = parse_shell_token(s, &type, &token);
            
            // set the file in the redirect struct
            red->file = token;
            
            // insert at thead of linked list
            red->next = c->redirection;
            c->redirection = red;
            
            // if last token, break out of loop
            if (s== NULL)
                break;
        }
        

        // if token is of some other specified type 
        else if (type == TOKEN_SEQUENCE || type == TOKEN_BACKGROUND || type == TOKEN_AND || type == TOKEN_OR ||
                 type == TOKEN_PIPE) {
            
            // this is the last token in this command
            last = 1;
            
            // set the condition_type field
            c->condition_type = type;
            
            // if type is of token background
            if (type == TOKEN_BACKGROUND) {
                // set bg to true
                c->bg = 1;
                
                // set all previous commands liked by || or && to background
                trav = previous;
                while(trav != NULL) {
                    if (trav->condition_type == TOKEN_SEQUENCE 
                        || trav->condition_type == TOKEN_BACKGROUND) {
                        break;
                    }
                    trav->bg = 1;
                    trav = trav->prev;
                }  
            }
        }
        
        // otherwise just append the token
        else
            command_append_arg(c, token);
    }
        
      // execute it
    if (start->argc)
        run_list(start);
    command_free(start);
}        


int main(int argc, char* argv[]) {
    
    FILE* command_file = stdin;
    int quiet = 0;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = 1;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    set_foreground(0);
    handle_signal(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    int needprompt = 1;

    while (!feof(command_file)) {
        sig_received = 0;
        foreground = 0;
        //set_foreground(0);
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = 0;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == NULL) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file))
                    perror("sh61");
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            eval_line(buf);
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
    }

    return 0;
}
