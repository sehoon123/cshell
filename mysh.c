#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <glob.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define NR_JOBS 20
#define PATH_BUFSIZE 1024
#define COMMAND_BUFSIZE 1024
#define TOKEN_BUFSIZE 64
#define TOKEN_DELIMITERS " \t\r\n\a"

#define BACKGROUND_EXECUTION 0
#define FOREGROUND_EXECUTION 1
#define PIPELINE_EXECUTION 2

#define COMMAND_EXTERNAL 0
#define COMMAND_EXIT 1
#define COMMAND_CD 2
#define COMMAND_JOBS 3
#define COMMAND_FG 4
#define COMMAND_BG 5
#define COMMAND_KILL 6
#define COMMAND_EXPORT 7
#define COMMAND_UNSET 8

#define STATUS_RUNNING 0
#define STATUS_DONE 1
#define STATUS_SUSPENDED 2
#define STATUS_CONTINUED 3
#define STATUS_TERMINATED 4

#define PROC_FILTER_ALL 0
#define PROC_FILTER_DONE 1
#define PROC_FILTER_REMAINING 2

#define COLOR_NONE "\033[m"
#define COLOR_RED "\033[1;37;41m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_CYAN "\033[0;36m"
#define COLOR_GREEN "\033[0;32;32m"
#define COLOR_GRAY "\033[1;30m"

const char* STATUS_STRING[] = {
    "running",
    "done",
    "suspended",
    "continued",
    "terminated"
};

struct process {
    char *command;
    int argc;
    char **argv;
    char *input_path;
    char *output_path;
    pid_t pid;
    int type;
    int status;
    struct process *next;
};

struct job {
    int id;
    struct process *root;
    char *command;
    pid_t pgid;
    int mode;
};

struct shell_info {
    char cur_user[TOKEN_BUFSIZE];
    char cur_dir[PATH_BUFSIZE];
    char pw_dir[PATH_BUFSIZE];
    struct job *jobs[NR_JOBS + 1];
};

struct shell_info *shell;

/*
The code first checks to see if the process with the given pid exists.
If it does, it loops through all of its children processes and looks for one that matches the given pid.
If found, it returns that child's id.
*/
int get_job_id_by_pid(int pid) {
    int i;
    struct process *proc;

    for (i = 1; i <= NR_JOBS; i++) {
        if (shell->jobs[i] != NULL) {
            for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) {
                if (proc->pid == pid) {
                    return i;
                }
            }
        }
    }

    return -1;
}

/*
The code first checks to see if the id value is greater than the number of jobs in the shell.
If it is, then the code returns NULL.
Otherwise, it uses the shell's built-in function to get a job by its id value.
The code will return the job with the given id.
If the id is greater than the number of jobs in the shell, then the code will return NULL.
*/
struct job* get_job_by_id(int id) {
    if (id > NR_JOBS) {
        return NULL;
    }

    return shell->jobs[id];
}

/*
The code first gets the job with the given id.
If the job isn't found, then an error is returned -1.
Next, the code looks at the pgid of the job.
This is a unique identifier for a particular job in a system.
Finally, the code returns this pgid to the caller.
The code will return the pgid of the job with the given id.
*/
int get_pgid_by_job_id(int id) {
    struct job *job = get_job_by_id(id);

    if (job == NULL) {
        return -1;
    }

    return job->pgid;
}

/*
The code first checks to see if the id parameter is greater than or equal to the number of jobs in the shell (NR_JOBS).
If not, it returns -1.
Next, the code loops through each process in the shell's jobs list.
For each process, it checks to see if its status is STATUS_DONE or STATUS_DONE but still running.
If so, then the count variable is incremented.
Otherwise, if filter specifies a PROC_FILTER value other than PROC_FILTER_ALL or PROC_FILTER_DONE, then only processes with that status are counted (i.e., those that have not yet completed their task).
Finally, when all processes have been checked, the count variable is returned.
The code will check if the given job id is greater than or equal to the number of jobs currently running on the shell, and if not it will return -1.
If the job id is within the number of running jobs, then it will loop through each process in that job and count how many times that process was filtered.
Finally, it returns the total count of processes filtered.
*/
int get_proc_count(int id, int filter) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    int count = 0;
    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (filter == PROC_FILTER_ALL ||
            (filter == PROC_FILTER_DONE && proc->status == STATUS_DONE) ||
            (filter == PROC_FILTER_REMAINING && proc->status != STATUS_DONE)) {
            count++;
        }
    }

    return count;
}

/*
 The code first checks to see if the shell has any jobs.
 If it doesn't, the code returns -1, indicating that there are no more jobs.
 If there are jobs, the code loops through each one and checks to see if it is NULL.
 If it is NULL, then the job's ID (i) is returned.
 Otherwise, i is incremented and the loop continues.
 The code will return the next job id in the shell's jobs array.
 If there is no job with that id, it will return -1.
 */
int get_next_job_id() {
    int i;

    for (i = 1; i <= NR_JOBS; i++) {
        if (shell->jobs[i] == NULL) {
            return i;
        }
    }

    return -1;
}

/*
 The code starts by checking to see if the id value is greater than or equal to the number of jobs in the shell (NR_JOBS).
 If it isn't, then the code returns -1, indicating an error.
 Next, the code prints out a list of all of the processes that are currently running on the system.
 For each process, it prints out its PID and then ends the loop.
 Finally, the code returns 0 to indicate success.
 The code prints the process IDs of all the jobs in the shell's job list.
 If a job is not found, then -1 is returned.
 */
int print_processes_of_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    printf("[%d]", id);

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        printf(" %d", proc->pid);
    }
    printf("\n");

    return 0;
}

/*
The code prints out the job status for each job in the shell.
The code starts by checking to see if the id of the job is greater than or equal to the number of jobs in the shell ( NR_JOBS ).
If it is not, then an error message is printed and execution ends.
Next, the code checks to see if any jobs in the shell have been deleted (i.e., they have a NULL value for their root process).
If so, then an error message is printed and execution ends.
The code loops through all of the jobs in the shell and prints out their pid , command , and status values.
It also prints out a string that contains information about each job's status ( STATUS_STRING ).
Finally, it prints a line indicating whether there are any more jobs in this Shell object or not.
The code prints out the status of all the jobs in the shell.
If there is a problem with finding or accessing a job, then the code will return an error code.
Otherwise, it prints out each job's information, including its pid, command, and status.
*/
int print_job_status(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    printf("[%d]", id);

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        printf("\t%d\t%s\t%s", proc->pid,
            STATUS_STRING[proc->status], proc->command);
        if (proc->next != NULL) {
            printf("|\n");
        } else {
            printf("\n");
        }
    }

    return 0;
}

/*
The code in this function releases a job by id.
If the id is greater than or equal to the number of jobs in the shell, or if the job with that id is not found, then an error occurs and return code is set to -1.
The first thing that happens in this function is that we check to see if the id given as an argument is greater than or equal to the number of jobs currently in the shell.
If it isn't, then we use a loop to look for a job with that id.
Once we find one, we use various functions to release it (freeing up its memory).
Finally, we free up all of those resources so they can be reused next time someone needs them.
The code will release the job with the given id.
If the id number is greater than or equal to the number of jobs currently stored in the shell's jobs array, then an error will be returned.
Otherwise, it will loop through each job in the jobs array and release it.
Finally, it frees up all associated resources for each job.
*/
int release_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    struct job *job = shell->jobs[id];
    struct process *proc, *tmp;
    for (proc = job->root; proc != NULL; ) {
        tmp = proc->next;
        free(proc->command);
        free(proc->argv);
        free(proc->input_path);
        free(proc->output_path);
        free(proc);
        proc = tmp;
    }

    free(job->command);
    free(job);

    return 0;
}

/*
The code first retrieves the next job ID, which is used to identify a specific job in the shell.
The code then assigns the ID of the retrieved job to the job variable.
Finally, the code sets the value of shell->jobs[id] to be equal to the newly assigned job variable.
The concept behind this code is that each time you run a command in a shell, it creates an entry in a list called "jobs."
This list contains information about all of the jobs currently running in your shell.
When you assign a new job to a variable, that new job becomes part of this list and will be executed when you run another command in your shell.
The code inserts a new job into the shell's jobs list.
The job's ID is set to the value of id, and it is assigned to the shell's Jobs property.
*/
int insert_job(struct job *job) {
    int id = get_next_job_id();

    if (id < 0) {
        return -1;
    }

    job->id = id;
    shell->jobs[id] = job;
    return id;
}

/*
The code in remove_job() first checks to see if the id passed in is greater than or equal to the number of jobs currently stored in shell->jobs[].
If it isn't, then the code returns -1, indicating an error.
Next, release_job() is called with the id passed in as a parameter.
This releases the job associated with that particular ID from memory and deletes it from shell->jobs[].
Finally, return 0 is returned to indicate success.
The code will remove the job with the given id from the shell's jobs list.
If the job with that id does not exist, then an error will be returned.
Finally, if the job has already been released, then its release is undone and the job is removed from the shell's jobs list.
*/
int remove_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    release_job(id);
    shell->jobs[id] = NULL;

    return 0;
}

/*
The code is trying to determine if the job with ID "id" has completed.
If it is not, then the function returns 0.
The code iterates through all of the processes in a list called "jobs".
It checks each process until it finds one that is done and returns 1.
The code is used to determine if the process with ID id has been completed.
If the process has not been completed, then it will return 0.
Otherwise, it will return 1.
*/
int is_job_completed(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return 0;
    }

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (proc->status != STATUS_DONE) {
            return 0;
        }
    }

    return 1;
}

/*
The code sets the process status of a given process to one of three values: running, stopped, or killed.
The code loops through all of the processes on the system and sets the status of each one to match its corresponding pid value.
If there is no matching pid value, then the loop terminates without doing anything else.
The first line in this code sets up an integer variable called i that will hold the number of processes on the system.
The next two lines declare a structure called shell that stores information about each process on the system.
This structure includes information such as the process's pid (a unique identifier), its root directory (the directory from which it was started), and its parent directory (if any).
Next, the code declares a variable called proc that will store a pointer to each individual process on the system.
The for loop begins by checking to see if shell->jobs[i] exists—this is simply a check to see if there is a Process object associated with job number i.
If there isn't, then nothing happens and execution continues at line 4; otherwise, execution proceeds to line 5 where it checks to see if proc->pid matches pid value stored in shell->jobs[i]->root.
If
The code sets the process status of a given process to the specified value.
The code first checks to see if the process with the given pid exists.
If it does, it then checks to see if the process's pid matches that of the current shell.
If it does, then the code sets proc->status to the specified value and returns 0.
Otherwise, it returns -1 and exits from the function.
*/
int set_process_status(int pid, int status) {
    int i;
    struct process *proc;

    for (i = 1; i <= NR_JOBS; i++) {
        if (shell->jobs[i] == NULL) {
            continue;
        }
        for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) {
            if (proc->pid == pid) {
                proc->status = status;
                return 0;
            }
        }
    }

    return -1;
}

/*
The code in this function sets the job status of an integer id to either done or failed.
If the id is greater than or equal to the number of jobs in the shell, then the code checks to see if a job with that id has already been completed.
If it has not been completed, then the code sets proc->status to done and returns 0.
Otherwise, if the id is less than or equal to the number of jobs in the shell, then the code loops through all of the jobs in that shell and sets proc->status for each one to either done or failed based on its current state.
The code sets the status of a job in the shell's jobs table to either completed or done.
If the job ID is greater than or equal to the number of jobs in the table (NR_JOBS), then the code checks to see if the job at that ID has already been completed.
If it has, then its status is set to done; otherwise, its status is set to completed.
Finally, for each job in the table, this code updates its status according to whether it is currently being executed or not.
*/
int set_job_status(int id, int status) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    int i;
    struct process *proc;

    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (proc->status != STATUS_DONE) {
            proc->status = status;
        }
    }

    return 0;
}

/*
The code starts by waiting for the process with the given pid to exit.
If it does, then the code sets the process status to "done".
Otherwise, if the process is terminated (signalled), or stopped (signd), then the code sets its status to "terminated" and returns that value.
If no process with that pid exists, then an error occurs and the program terminates with a status of -1.
The code will wait for the given process ID to exit.
If the process exits successfully, then the code sets its status to "done".
If the process fails, then the code sets its status to "terminated" and returns an error code.
Finally, if the process is stopped, then the code sets its status to "suspended".
*/
int wait_for_pid(int pid) {
    int status = 0;

    waitpid(pid, &status, WUNTRACED);
    if (WIFEXITED(status)) {
        set_process_status(pid, STATUS_DONE);
    } else if (WIFSIGNALED(status)) {
        set_process_status(pid, STATUS_TERMINATED);
    } else if (WSTOPSIG(status)) {
        status = -1;
        set_process_status(pid, STATUS_SUSPENDED);
    }

    return status;
}

/*
The code first checks to see if the job with the given id is already in progress.
If it is, then the code simply returns -1.
Otherwise, it gets information about the process that is currently running for that job and sets a variable called wait_pid to track its current status.
Next, the code loops through all of the processes that are waiting on this job and keeps track of how many times they have been signaled (WIFSIGNALED), terminated (WSTOPSIG), or exited (WEXITED).
Once it has counted up to proc_count, it sets a flag called status to indicate that this job has completed.
Finally, it prints out the status of the job's process so that users can know what happened.
The code will check if the job with the given id is already running, and if not it will start waiting for that job to finish.
If the job has already finished, then the code will set the process status to done.
Otherwise, if the job has been terminated, then the status will be set to terminated and an indicator printed to indicate this.
Finally, if a signal was received while waiting for the job, then the status will be set to suspended and another indicator printed.
If all goes well and there are no errors, then the code returns success.
*/
int wait_for_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    int proc_count = get_proc_count(id, PROC_FILTER_REMAINING);
    int wait_pid = -1, wait_count = 0;
    int status = 0;

    do {
        wait_pid = waitpid(-shell->jobs[id]->pgid, &status, WUNTRACED);
        wait_count++;

        if (WIFEXITED(status)) {
            set_process_status(wait_pid, STATUS_DONE);
        } else if (WIFSIGNALED(status)) {
            set_process_status(wait_pid, STATUS_TERMINATED);
        } else if (WSTOPSIG(status)) {
            status = -1;
            set_process_status(wait_pid, STATUS_SUSPENDED);
            if (wait_count == proc_count) {
                print_job_status(id);
            }
        }
    } while (wait_count < proc_count);

    return status;
}

/*
The code in this function checks to see if the command entered is one of the valid commands.
If it is, the code determines what type of command it is and returns the appropriate response.
The first line in this function checks to see if the command entered is equal to "exit".
If it is, then the code returns the value COMMAND_EXIT.
Next, the code checks to see if the command entered is equal to "cd".
If it is, then the code returns the value COMMAND_CD.
Next, the code checks to see if the command entered is equal to "jobs".
If it is, then the code returns the value COMMAND_JOBS.
Finally, if no valid command was found in that string or any other strings were provided as arguments, then an error message will be displayed and execution will terminate at this point.
The code will return the command type for the given command if it is found in the command string.
If not, then the code will return COMMAND_EXTERNAL.
 */
int get_command_type(char *command) {
    if (strcmp(command, "exit") == 0) {
        return COMMAND_EXIT;
    } else if (strcmp(command, "cd") == 0) {
        return COMMAND_CD;
    } else if (strcmp(command, "jobs") == 0) {
        return COMMAND_JOBS;
    } else if (strcmp(command, "fg") == 0) {
        return COMMAND_FG;
    } else if (strcmp(command, "bg") == 0) {
        return COMMAND_BG;
    } else if (strcmp(command, "kill") == 0) {
        return COMMAND_KILL;
    } else if (strcmp(command, "export") == 0) {
        return COMMAND_EXPORT;
    } else if (strcmp(command, "unset") == 0) {
        return COMMAND_UNSET;
    } else {
        return COMMAND_EXTERNAL;
    }
}

/*
The code fragment helper_strtrim() takes a single string, line, and removes any spaces from the beginning of the string and the end of the string.
The code starts by initializing head to point at line, which is just the first character in line.
Then it increments head and continues looping until head points past the end of line (line + strlen(line) - 1).
Next, while head still points to a space, that space is replaced with a '.'.
This effectively moves HEAD one position forward so that it now points at tail.
Finally, tail is set to point just after 'tail' (the last character in line), and '.'
is removed from tail.
The code will trim any whitespace from the beginning and end of the string line.
*/
char* helper_strtrim(char* line) {
    char *head = line, *tail = line + strlen(line);

    while (*head == ' ') {
        head++;
    }
    while (*tail == ' ') {
        tail--;
    }
    *(tail + 1) = '\0';

    return head;
}

/*
The code will get the current working directory of the shell, and store it in a variable called shell->cur_dir.
*/
void mysh_update_cwd_info() {
    getcwd(shell->cur_dir, sizeof(shell->cur_dir));
}

/*
The code first checks to see if there is only one argument - in this case, it is a string that specifies the directory to change to.
If there are no arguments, then the code calls the mysh_update_cwd_info() function, which updates the current working directory information.
If there are arguments, then the code first checks to see if chdir() was successful.
If not, then an error message is printed and the program ends.
Otherwise, cd() changes the current working directory to argv[1].
The code will first check to see if the user passed in a single argument - which is the directory they would like to cd into.
If not, it will then check to see if the user provided a second argument - which would be the name of the file or directory they wish to cd into.
If that file or directory does not exist, then an error will be returned and the code will terminate.
If everything goes according to plan, however, the code should successfully navigate to the desired location and print out a message indicating as much.
*/
int mysh_cd(int argc, char** argv) {
    if (argc == 1) {
        chdir(shell->pw_dir);
        mysh_update_cwd_info();
        return 0;
    }

    if (chdir(argv[1]) == 0) {
        mysh_update_cwd_info();
        return 0;
    } else {
        printf("mysh: cd %s: No such file or directory\n", argv[1]);
        return 0;
    }
}

/*
The code in mysh_jobs() starts by checking to see if the shell's jobs variable is NULL.
If it is, the code prints out information about the job currently being processed.
Next, the code loops through all of the jobs in the shell.
For each job, it checks to see if it exists and prints out information about that job.
The code will print the job status for every job in the shell's jobs array.
 */
int mysh_jobs(int argc, char **argv) {
    int i;

    for (i = 0; i < NR_JOBS; i++) {
        if (shell->jobs[i] != NULL) {
            print_job_status(i);
        }
    }

    return 0;
}

/*
The code starts by checking to see if the user provided any input.
If not, then the code prints out a usage message and returns an error code -1.
Next, the code checks to see if the user provided a pid.
If so, it gets that pid and sets up its process group membership.
If the job_id is supplied, then the code sets its status to "continued" and prints out its current status.
The code also checks to see if there are any jobs waiting for that particular pid.
If so, it waits for them using wait_for_job() .
Finally, it resets all of the process group membership variables back to their original values before returning 0 .
The code will check to see if the user provided a valid pid and, if so, it will use kill() to send the specified process to SIGKILL.
If the kill() call fails, then the code will print an error message and return -1.
Finally, it sets the current process group of tcsetpgrp() to 0 (the root user), sets the pid of getpid() to that of the process killed, signals SIGTTOU and SIG_DFL, and returns 0.
*/
int mysh_fg(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: fg <pid>\n");
        return -1;
    }

    int status;
    pid_t pid;
    int job_id = -1;

    if (argv[1][0] == '%') {
        job_id = atoi(argv[1] + 1);
        pid = get_pgid_by_job_id(job_id);
        if (pid < 0) {
            printf("mysh: fg %s: no such job\n", argv[1]);
            return -1;
        }
    } else {
        pid = atoi(argv[1]);
    }

    if (kill(-pid, SIGCONT) < 0) {
        printf("mysh: fg %d: job not found\n", pid);
        return -1;
    }

    tcsetpgrp(0, pid);

    if (job_id > 0) {
        set_job_status(job_id, STATUS_CONTINUED);
        print_job_status(job_id);
        if (wait_for_job(job_id) >= 0) {
            remove_job(job_id);
        }
    } else {
        wait_for_pid(pid);
    }

    signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(0, getpid());
    signal(SIGTTOU, SIG_DFL);

    return 0;
}

/*
The code starts by checking to see if the user provided any arguments.
If not, the code prints an error message and returns -1.
Next, the code checks to see if the user provided a pid.
If so, it gets that pid's process ID (PID).
If the job_id is greater than 0, then the code sets its status to CONTINUED.
It also prints out that status.
Finally, it calls kill() on PID to terminate the process.
The code will check to see if the user provided a valid pid, and if so, it will use get_pgid_by_job_id() to determine the process ID of that job.
If the process ID is not found, then an error message is printed and the function returns -1.
*/
int mysh_bg(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: bg <pid>\n");
        return -1;
    }

    pid_t pid;
    int job_id = -1;

    if (argv[1][0] == '%') {
        job_id = atoi(argv[1] + 1);
        pid = get_pgid_by_job_id(job_id);
        if (pid < 0) {
            printf("mysh: bg %s: no such job\n", argv[1]);
            return -1;
        }
    } else {
        pid = atoi(argv[1]);
    }

    if (kill(-pid, SIGCONT) < 0) {
        printf("mysh: bg %d: job not found\n", pid);
        return -1;
    }

    if (job_id > 0) {
        set_job_status(job_id, STATUS_CONTINUED);
        print_job_status(job_id);
    }

    return 0;
}

/*
The code starts by checking to see if the user provided any arguments.
If not, the code prints an error message and returns -1.
Next, the code checks to see if the user provided a pid value.
If so, it uses get_pgid_by_job_id() to get the pid of the job with that id.
If there is no such job, then kill() fails and return 0.
If there is a valid pid value, then the code sets the job status for that pid to STATUS_TERMINATED using set_job_status().
The code also prints out this information.
Finally, it waits for that job to finish (using wait_for_job()) and removes it from memory using remove_job().
The code will check to see if the user provided a valid pid parameter.
If not, it will print an error message and return -1.
If the user did provide a valid pid parameter, then it will check to see if that pid corresponds to a running job.
If so, it will terminate the job using SIGKILL.
Finally, it will set the job status to terminated and print out the job's status.
If the user wants to wait for a running job to finish, they can use the wait_for_job() function.
This function will wait until the specified job finishes or returns -1 (if the job has already finished).
*/
int mysh_kill(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: kill <pid>\n");
        return -1;
    }

    pid_t pid;
    int job_id = -1;

    if (argv[1][0] == '%') {
        job_id = atoi(argv[1] + 1);
        pid = get_pgid_by_job_id(job_id);
        if (pid < 0) {
            printf("mysh: kill %s: no such job\n", argv[1]);
            return -1;
        }
        pid = -pid;
    } else {
        pid = atoi(argv[1]);
    }

    if (kill(pid, SIGKILL) < 0) {
        printf("mysh: kill %d: job not found\n", pid);
        return 0;
    }

    if (job_id > 0) {
        set_job_status(job_id, STATUS_TERMINATED);
        print_job_status(job_id);
        if (wait_for_job(job_id) >= 0) {
            remove_job(job_id);
        }
    }

    return 1;
}

/*
The code in this function exports a variable.
The first argument is the name of the variable, and the second argument is the value to export.
The putenv() function sets the environment variable named KEY to the value given in argv[1].
The code will export the environment variable KEY to the value VALUE.
*/
int mysh_export(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: export KEY=VALUE\n");
        return -1;
    }

    return putenv(argv[1]);
}

/*
The code in this function takes two arguments: an integer, and a string of characters.
If the code is run without any arguments, it prints out the usage message.
The first line of the function declares two variables: argc, which holds the number of arguments passed to the function, and argv, which holds a pointer to the first argument (the string "usage").
Next, the function checks to see if there are any arguments passed in.
If not, it prints out a message telling users how to use this function.
If there are args present, then the next line of code sets up a few environment variables.
unsetenv() is a built-in C library function that removes all environment variables with matching names from memory.
The second argument to unsetenv() is the string "KEY".
This will remove all environment variables named KEY from memory.
Finally, unsetenv() returns -1 if there were any errors encountered while removing environment variables.
Otherwise it returns 0 .
The code will unset the environment variable KEY if it is set.
If argc is less than 2, an error message will be printed and the function will return -1.
*/
int mysh_unset(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: unset KEY\n");
        return -1;
    }

    return unsetenv(argv[1]);
}

/*
 The code prints a farewell message and then exits.
*/
int mysh_exit() {
    printf("Goodbye!\n");
    exit(0);
}

/*
The code is checking to see if the process is a zombie.
If it is, then it prints out the status of the job that was running and removes it from memory.
The code attempts to check if the process has been terminated or not.
If it has, then the status of the process is set as done.
If it hasn't, then the status of the process is set as suspended and a job id is checked to see if it's completed or not.
If it's completed, then a print_job_status() function will be called which prints out a list of all processes that have been completed on that system.
*/
void check_zombie() {
    int status, pid;
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED)) > 0) {
        if (WIFEXITED(status)) {
            set_process_status(pid, STATUS_DONE);
        } else if (WIFSTOPPED(status)) {
            set_process_status(pid, STATUS_SUSPENDED);
        } else if (WIFCONTINUED(status)) {
            set_process_status(pid, STATUS_CONTINUED);
        }

        int job_id = get_job_id_by_pid(pid);
        if (job_id > 0 && is_job_completed(job_id)) {
            print_job_status(job_id);
            remove_job(job_id);
        }
    }
}

void sigint_handler(int signal) {
    printf("\n");
}

/*
The code starts by declaring a variable called "status" which will be used to store the return value of the function.
The switch statement is then executed, and depending on what type of command it is, different commands are executed.
The first case in the switch statement executes COMMAND_EXIT, which causes mysh to exit.
The second case in the switch statement executes COMMAND_CD, which changes directories for mysh.
The third case in the switch statement executes COMMAND_JOBS, which prints out information about jobs that are running on mysh's system.
The fourth case in the switch statement executes COMMAND_FG or COMMAND_BG depending on whether you're using GNU screen or tmux respectively (the code doesn't specify).
This sets up your terminal session with either one of these programs so that you can use them without having to logout and login again every time you want to change sessions.
The code is a function that takes in a process pointer and returns the status of the command.
*/
int mysh_execute_builtin_command(struct process *proc) {
    int status = 1;

    switch (proc->type) {
        case COMMAND_EXIT:
            mysh_exit();
            break;
        case COMMAND_CD:
            mysh_cd(proc->argc, proc->argv);
            break;
        case COMMAND_JOBS:
            mysh_jobs(proc->argc, proc->argv);
            break;
        case COMMAND_FG:
            mysh_fg(proc->argc, proc->argv);
            break;
        case COMMAND_BG:
            mysh_bg(proc->argc, proc->argv);
            break;
        case COMMAND_KILL:
            mysh_kill(proc->argc, proc->argv);
            break;
        case COMMAND_EXPORT:
            mysh_export(proc->argc, proc->argv);
            break;
        case COMMAND_UNSET:
            mysh_unset(proc->argc, proc->argv);
            break;
        default:
            status = 0;
            break;
    }

    return status;
}


/*
The code starts by checking if the process is a command external to mysh.
If it is, then it executes the builtin command and returns 0.
Otherwise, it forks off a child process with fork().
The code checks for errors in the child process before returning back to main() with status of 0 or -1.
If there are no errors in the child process, then we check if any signals were sent to our program (SIGINT, SIGTSTP) and close all file descriptors that were opened up (dup2(), close()) before exiting with exit(0).
The code is a function that will be called by mysh_launch_process() when the process has been launched.
The function will fork off a child process and then wait for it to finish.
If the child process exits with an error, then we print out a message and exit with 0.
Otherwise, we set up the parent process to have its own PID and PGID (pgid) so that it can run in parallel with other processes on the system.
The code is a function that will be called by mysh_launch_process() when the process has been launched.
The function will fork off a child process and then wait for it to finish.
If the child process exits with an error, then we print out
*/
int mysh_launch_process(struct job *job, struct process *proc, int in_fd, int out_fd, int mode) {
    proc->status = STATUS_RUNNING;
    if (proc->type != COMMAND_EXTERNAL && mysh_execute_builtin_command(proc)) {
        return 0;
    }

    pid_t childpid;
    int status = 0;

    childpid = fork();

    if (childpid < 0) {
        return -1;
    } else if (childpid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        proc->pid = getpid();
        if (job->pgid > 0) {
            setpgid(0, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(0, job->pgid);
        }

        if (in_fd != 0) {
            dup2(in_fd, 0);
            close(in_fd);
        }

        if (out_fd != 1) {
            dup2(out_fd, 1);
            close(out_fd);
        }

        if (execvp(proc->argv[0], proc->argv) < 0) {
            printf("mysh: %s: command not found\n", proc->argv[0]);
            exit(0);
        }

        exit(0);
    } else {
        proc->pid = childpid;
        if (job->pgid > 0) {
            setpgid(childpid, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(childpid, job->pgid);
        }

        if (mode == FOREGROUND_EXECUTION) {
            tcsetpgrp(0, job->pgid);
            status = wait_for_job(job->id);
            signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(0, getpid());
            signal(SIGTTOU, SIG_DFL);
        }
    }

    return status;
}


/*
The code starts by checking if the job is a command-line job.
If so, it will insert the new job into the list of jobs and return -1 to indicate that it failed.
The code then goes through all of the processes in this particular job's root node (the process with no parent).
It checks if there is an input path for this process, which would be a file or directory on disk.
If there is one, it opens up that file or directory using O_RDONLY and returns 0 to indicate success.
Otherwise, it prints out "no such file or directory" and removes itself from the list of jobs associated with this particular command-line job.
If not, then mysh launches a new process using mysh_launch_process() .
The first parameter passed in is always going to be our current Job object; we'll use that later when we want to send signals back to our program running inside mysh .
The second parameter passed in is always going to be our current Process object; we'll use that later when we want to send signals back from within our program running inside mysh .
We pass in fd[0] as an argument for where I/O should happen (this
The code is a function that launches a job.
The code starts by checking if the job being launched is a command-external type of job.
If it is, then the code will insert the new job into the list of jobs and return an integer value representing the newly inserted position in which to place this new process.
The next section of code loops through all processes that are currently running on this machine and checks if they have input paths or output paths.
If they do, then it opens them up for execution and returns 0 as its status.
*/
int mysh_launch_job(struct job *job) {
    struct process *proc;
    int status = 0, in_fd = 0, fd[2], job_id = -1;

    check_zombie();
    if (job->root->type == COMMAND_EXTERNAL) {
        job_id = insert_job(job);
    }

    for (proc = job->root; proc != NULL; proc = proc->next) {
        if (proc == job->root && proc->input_path != NULL) {
            in_fd = open(proc->input_path, O_RDONLY);
            if (in_fd < 0) {
                printf("mysh: no such file or directory: %s\n", proc->input_path);
                remove_job(job_id);
                return -1;
            }
        }
        if (proc->next != NULL) {
            pipe(fd);
            status = mysh_launch_process(job, proc, in_fd, fd[1], PIPELINE_EXECUTION);
            close(fd[1]);
            in_fd = fd[0];
        } else {
            int out_fd = 1;
            if (proc->output_path != NULL) {
                out_fd = open(proc->output_path, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                if (out_fd < 0) {
                    out_fd = 1;
                }
            }
            status = mysh_launch_process(job, proc, in_fd, out_fd, job->mode);
        }
    }

    if (job->root->type == COMMAND_EXTERNAL) {
        if (status >= 0 && job->mode == FOREGROUND_EXECUTION) {
            remove_job(job_id);
        } else if (job->mode == BACKGROUND_EXECUTION) {
            print_processes_of_job(job_id);
        }
    }

    return status;
}

/*
The code starts by allocating a buffer of size TOKEN_BUFSIZE.
It then allocates memory for an array of pointers to char, which is the type used in mysh's command line parsing function.
The code then starts looping through the string segment and calling strtok() on it until it finds the delimiter token (TOKEN_DELIMITERS).
The while loop iterates over each token that was found in the string segment.
If there are no tokens left, this means that we reached the end of our input stream and so we exit out with EXIT_FAILURE.
Otherwise, we continue to parse each token using strtok().
Once a token has been parsed, it will be stored into one of two places: either into an existing pointer or into a newly allocated pointer depending on whether or not there is already another pointer pointing at that same location within our memory allocation for tokens.
The code allocates memory and sets it to the size of TOKEN_BUFSIZE.
The code then proceeds to iterate through each token in the string segment, which is a command line argument.
The code then stores this string into a local variable called command.
The next step is to use strtok() function on the token, which will return an iterator pointing to the next token in the string.
This is done for every iteration of looping through each token in order to parse out any arguments that may be present.
The last step is simply printing out what was parsed from within the tokens by using strtok().
The code starts by checking if the token is a glob.
If it is, then it will call the function glob().
The function takes three arguments: the string to be matched, an optional pointer to a buffer that will receive any matches found in this string, and an integer which tells how many times to go through this process.
The next step of the code checks if there are any characters after '*' or '?'
in the token.
If so, then they are used as wildcards for matching against files and directories on disk.
If not, then they are just treated as normal characters in their respective positions within the string being matched against files and directories on disk.
After all of these steps have been completed successfully (or unsuccessfully), we get to where we start looping over tokens again with position set at 0 because we're starting from scratch now since no more wildcards were found earlier in this iteration of our loop.
We also increase position by TOKEN_BUFSIZE each time around our loop so that when we reach end-of-file (EOF) later on during one iteration of our loop, it won't cause us problems due to having too few elements left in tokens[] array before reaching EOF
The code will allocate a buffer of size TOKEN_BUFSIZE and store the tokens in it.
The code will then iterate through the tokens, extracting each path from the glob buffer.
The code is used to extract paths from a glob buffer.
The code starts by declaring two pointers, input_path and output_path.
These are the paths to where the program will store its data.
The code then declares a variable called argc which is used to keep track of how many arguments were passed in from the command line.
Next, it loops through each argument that was passed in and stores them into tokens[i].
It also keeps track of how many characters there are in each token with strlen(tokens[i]).
After this loop finishes, it checks if any tokens have been set to < or > and breaks out of the loop if so.
If not, it continues on until i equals position - 1 (the last argument).
After all arguments have been processed, new_proc is created as a pointer to struct process which has command as its value and argv as its list of pointers containing all arguments that were passed in from the command line.
Then new-proc's input path is set equal to input_path while its output path is set equal to output_path; both these values point at their respective locations for storing data during execution.
Finally, new-proc's pid variable gets initialized with -1 because no process has yet started running yet when this function runs
The code is used to create a new process.
The code creates a new struct called process that has the following fields: command, argv, argc, input_path and output_path.
The code allocates memory for the struct using malloc(), which stores the pointer of the allocated memory in new_proc->argv.
The code allocates memory for the struct using malloc(), which stores the pointer of the allocated memory in new_proc->argv.
*/
struct process* mysh_parse_command_segment(char *segment) {
    int bufsize = TOKEN_BUFSIZE;
    int position = 0;
    char *command = strdup(segment);
    char *token;
    char **tokens = (char**) malloc(bufsize * sizeof(char*));

    if (!tokens) {
        fprintf(stderr, "mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(segment, TOKEN_DELIMITERS);
    while (token != NULL) {
        glob_t glob_buffer;
        int glob_count = 0;
        if (strchr(token, '*') != NULL || strchr(token, '?') != NULL) {
            glob(token, 0, NULL, &glob_buffer);
            glob_count = glob_buffer.gl_pathc;
        }

        if (position + glob_count >= bufsize) {
            bufsize += TOKEN_BUFSIZE;
            bufsize += glob_count;
            tokens = (char**) realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "mysh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        if (glob_count > 0) {
            int i;
            for (i = 0; i < glob_count; i++) {
                tokens[position++] = strdup(glob_buffer.gl_pathv[i]);
            }
            globfree(&glob_buffer);
        } else {
            tokens[position] = token;
            position++;
        }

        token = strtok(NULL, TOKEN_DELIMITERS);
    }

    int i = 0, argc = 0;
    char *input_path = NULL, *output_path = NULL;
    while (i < position) {
        if (tokens[i][0] == '<' || tokens[i][0] == '>') {
            break;
        }
        i++;
    }
    argc = i;

    for (; i < position; i++) {
        if (tokens[i][0] == '<') {
            if (strlen(tokens[i]) == 1) {
                input_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(input_path, tokens[i + 1]);
                i++;
            } else {
                input_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(input_path, tokens[i] + 1);
            }
        } else if (tokens[i][0] == '>') {
            if (strlen(tokens[i]) == 1) {
                output_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(output_path, tokens[i + 1]);
                i++;
            } else {
                output_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(output_path, tokens[i] + 1);
            }
        } else {
            break;
        }
    }

    for (i = argc; i <= position; i++) {
        tokens[i] = NULL;
    }

    struct process *new_proc = (struct process*) malloc(sizeof(struct process));
    new_proc->command = command;
    new_proc->argv = tokens;
    new_proc->argc = argc;
    new_proc->input_path = input_path;
    new_proc->output_path = output_path;
    new_proc->pid = -1;
    new_proc->type = get_command_type(tokens[0]);
    new_proc->next = NULL;
    return new_proc;
}

/*
The code starts by parsing the command line and storing it in a string.
The code then allocates memory for an array of pointers to struct jobs, which will be used later on.
The first while loop is where all the work happens.
It starts by checking if there are any more characters left in the input string (line).
If so, it continues to parse them until it finds a space or two spaces at the end of line.
Then, it checks if that character is '|'.
If so, then seg_len gets incremented and c gets set back to point at that same position in line_cursor again before continuing with this loop.
Otherwise, seg_len gets incremented and c points at whatever character was just parsed from line_cursor .
This means that when we get out of this loop we'll have either reached the end of line or found another '|' character after our last one.
In either case, we break out of this loop and continue onto parsing commands until we find a new '|'.
After finding a new '|', what's next?
Well now that there's no other characters left in our input string (line), what do we do?
We check if *c
The code is used to parse a command line and pass it on to the next process.
The first thing that happens in this code is that the line of text is trimmed down to remove any whitespace.
Then, the string "command" is allocated with strdup() and stored in memory.
The next step in this code is looping through all of the characters until it encounters a '|' character or reaches the end of the line.
In both cases, seg_len will be set to 0 and continue looping through all of the characters until it encounters another '|' character or reaches the end of the line again.
If there are no more characters on that particular line, then break from
*/
struct job* mysh_parse_command(char *line) {
    line = helper_strtrim(line);
    char *command = strdup(line);

    struct process *root_proc = NULL, *proc = NULL;
    char *line_cursor = line, *c = line, *seg;
    int seg_len = 0, mode = FOREGROUND_EXECUTION;

    if (line[strlen(line) - 1] == '&') {
        mode = BACKGROUND_EXECUTION;
        line[strlen(line) - 1] = '\0';
    }

    while (1) {
        if (*c == '\0' || *c == '|') {
            seg = (char*) malloc((seg_len + 1) * sizeof(char));
            strncpy(seg, line_cursor, seg_len);
            seg[seg_len] = '\0';

            struct process* new_proc = mysh_parse_command_segment(seg);
            if (!root_proc) {
                root_proc = new_proc;
                proc = root_proc;
            } else {
                proc->next = new_proc;
                proc = new_proc;
            }

            if (*c != '\0') {
                line_cursor = c;
                while (*(++line_cursor) == ' ');
                c = line_cursor;
                seg_len = 0;
                continue;
            } else {
                break;
            }
        } else {
            seg_len++;
            c++;
        }
    }

    struct job *new_job = (struct job*) malloc(sizeof(struct job));
    new_job->root = root_proc;
    new_job->command = command;
    new_job->pgid = -1;
    new_job->mode = mode;
    return new_job;
}


/*
The code starts by declaring the variables that will be used in the function.
The variable "bufsize" is declared as an integer and it is set to COMMAND_BUFSIZE, which is a constant defined in mysh.h.
The variable "position" is also declared as an integer and it's initial value is 0.
This means that when this function starts executing, position will start at 0 and increment each time through the loop until it reaches bufsize-1 (the last element of buffer).
The first line of code creates a new string called "buffer".
It allocates enough space for sizeof(char) * bufsize bytes (a total of 8192 characters).
If there was any error during allocation then fprintf() would print out on stderr with text similar to: mysh: allocation error
The code is the function that will be called to read a line of text from the user.
The function allocates memory for an array of characters and then loops until it has reached the end of the input string.
It then checks if there is an EOF (end-of-file) or a '\n' (newline character) in the input, and if so, it stores that character in each element of the buffer array.
If not, it stores whatever was just read into each element of the buffer array.
The code above is written in C language and uses functions such as malloc() and realloc().
*/
char* mysh_read_line() {
    int bufsize = COMMAND_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        c = getchar();

        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        if (position >= bufsize) {
            bufsize += COMMAND_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "mysh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void mysh_print_promt() {
    printf(COLOR_CYAN "%s" COLOR_NONE " in " COLOR_YELLOW "%s" COLOR_NONE "\n", shell->cur_user, shell->cur_dir);
    printf(COLOR_RED "mysh>" COLOR_NONE " ");
}

void mysh_print_welcome() {
    printf("Welcome to mysh by test!\n");
}

/*
The code starts by declaring a variable called mysh_loop.
This is the function that will be executed over and over again until it returns 0.
The code then declares a variable called line, which will hold the string of text that was read from the user's input.
After this, there are two loops: one for while (1) and another for while (1).
The first loop starts with an if statement to check whether or not there is any text in line.
If so, it checks to see if strlen(line) == 0; if so, it continues on to check_zombie().
If not, it prints out "mysh" followed by some other text before continuing onto checking job = mysh_parse_command(line); where job is declared as a struct job *job; meaning that this particular function can only be used with jobs created using mysh_parse_command().
Then status = 1; tells us what happened after we checked job = mysh_parse_command(line).
Finally, we launch our program into running status with mysh _launch _job();
The code is a loop that will keep running until the user enters "exit" as the command.
The first line of code creates an array called mysh_job which contains all jobs that are currently in progress.
The next line checks to see if there is any input from the user, and if not it goes back to the beginning of the loop.
If there is input, then it will read in one line at a time and check to see if it has been terminated with a newline character (i.e.).
If so, then it will parse out what was typed into that line and use that information to launch a job depending on what was typed in.
*/
void mysh_loop() {
    char *line;
    struct job *job;
    int status = 1;

    while (1) {
        mysh_print_promt();
        line = mysh_read_line();
        if (strlen(line) == 0) {
            check_zombie();
            continue;
        }
        job = mysh_parse_command(line);
        status = mysh_launch_job(job);
    }
}


/*
The code starts by declaring a struct sigaction object.
This is used to set up the signal handler for SIGINT, which will be called when the user presses Ctrl+C on their keyboard.
The sa_handler field of this structure is set to point to a function that takes no arguments and returns nothing, named sigint_handler.
Next, we call the function sigemptyset(), which sets all bits in the sa_mask field of our struct sigaction object to 0 (in other words, it clears them).
We then use the function signal() with an argument value of SIGINT and pass in NULL as its second argument so that it doesn't actually do anything when it's called.
Finally, we use another function called signal() with an argument value of SIGQUIT and pass in SIG_IGN as its second argument so that it won't send any signals out at all when it's called.
After setting up these two handlers for handling Ctrl+C keystrokes from users on their keyboards, we create some functions for handling signals sent by Linux: one for sending signals back into Linux (SIGTSTP), one for sending signals back into ourselves (SIGTTIN), and one for sending signals back
The code is a function that initializes the shell.
The code allocates memory for the shell, sets up the user and group IDs, and then starts to initialize the jobs array.
*/
void mysh_init() {
    struct sigaction sigint_action = {
        .sa_handler = &sigint_handler,
        .sa_flags = 0
    };
    sigemptyset(&sigint_action.sa_mask);
    sigaction(SIGINT, &sigint_action, NULL);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    pid_t pid = getpid();
    setpgid(pid, pid);
    tcsetpgrp(0, pid);

    shell = (struct shell_info*) malloc(sizeof(struct shell_info));
    getlogin_r(shell->cur_user, sizeof(shell->cur_user));

    struct passwd *pw = getpwuid(getuid());
    strcpy(shell->pw_dir, pw->pw_dir);

    int i;
    for (i = 0; i < NR_JOBS; i++) {
        shell->jobs[i] = NULL;
    }

    mysh_update_cwd_info();
}

int main(int argc, char **argv) {
    mysh_init();
    mysh_print_welcome();
    mysh_loop();

    return EXIT_SUCCESS;
}
