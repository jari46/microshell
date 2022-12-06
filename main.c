/* MICROSHELL: The program that will behave like executing a shell command
 *
 * Implementation:
 * 1) The command line to execute will be the arguments of this program
 * 2) implement "|" and ";" like in bash
 * 3) implement the built-in command cd only with a path as argument
 * 3) if something failed, print messages in STDERR as instructed
 * 
 * Example:
 * $>./microshell /bin/ls "|" /usr/bin/grep microshell ";" /bin/echo i love my microshell
 * 
 * Allowed functions: 
 * malloc, free, write, close, fork, waitpid, signal, kill, exit, chdir, execve, dup, dup2, pipe, strcmp, strncmp
 * 
 */

#define NULL 0

#define SAME 0 /* strcmp() return value */

#define CHILD 0 /* fork() return value */
#define READ 0 /* pipe() return value */
#define WRITE 1 /* pipe() return value */

#define SYSFAIL -1 /* systemcall return value */

#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

/* 
 * ERROR CONTROL
 */
void error(const char *str)
{
	int i = 0;
	while (str[i])
	{
		write(STDERR_FILENO, str + i, 1);
		i++;
	}
}

void syscall_error(void)
{
	error("error: fatal\n");
	exit(1);
}

/* 
 * UTILS FOR 'ARRAY OF POINTERS'
 *
 * ( working data:
 *   start: arr[*offset]
 *   end: before arr[i] == limit_str )
 */
int count_until(char *limit_str, char **arr, int offset)
/* 1) get length of sub-array
 * 2) to get whole length, put empty string("") as limit_str
 */
{
	int i = 0;
	while (arr[offset + i] && (strcmp(arr[offset + i], limit_str) != SAME))
		i++;
	return (i);
}

char **get_next_until(char *limit_str, char **arr, int *offset)
/* 1) get sub-array from arr
 * 2) be used to get JOB(until ";") and COMMAND(until "|")
 */
{
	char **result = NULL;
	int result_len = count_until(limit_str, arr, *offset);

	if (result_len == 0)
		return (NULL);

	if ((result = malloc(sizeof(char *) * (result_len + 1))) == NULL) syscall_error();
	int i = 0;
	while (i < result_len)
	{
		result[i] = arr[*offset + i];
		i++;
	}
	result[i] = NULL;
	(*offset) += result_len;
	return (result);
}

/* 
 * MAIN FUNCTIONS
 */
void change_directory(char **command)
{
	if (count_until("", command, 0) != 2) error("error: cd: bad arguments\n");
	else
	{
		if (chdir(command[1]) == SYSFAIL)
			{ error("error: cd: cannot change directory to "); error(command[1]); error("\n"); }
	}
}

void execute_job(char **job, char **envp)
{
	int fd_read;
	int fd_pipe[2];
	pid_t pid;

	char **command = NULL;
	int job_len = count_until("", job, 0);
	int offset = -1;

	if ((fd_read = dup(STDIN_FILENO)) == SYSFAIL) syscall_error();
	int child_num = 0;
	while (offset < job_len)
	{
		offset++;/* go to the start-index */
		command = get_next_until("|", job, &offset); /* after this: job[offset] == "|" */
		
		if (offset < job_len)/* if it is not the last command */
			if (pipe(fd_pipe) == SYSFAIL) syscall_error();
		if ((pid = fork()) == SYSFAIL) syscall_error();
		
		if (pid == CHILD)
		{
			if (dup2(fd_read, STDIN_FILENO) == SYSFAIL) syscall_error();
			if (close(fd_read) == SYSFAIL) syscall_error();
			if (offset < job_len)/* if it is not the last command */
			{
				if (dup2(fd_pipe[WRITE], STDOUT_FILENO) == SYSFAIL) syscall_error();
				if (close(fd_pipe[READ]) == SYSFAIL) syscall_error();
				if (close(fd_pipe[WRITE]) == SYSFAIL) syscall_error();
			}
			if (execve(command[0], command, envp) == SYSFAIL)
				{ error("error: cannot execute "); error(command[0]); error("\n"); }
		}
		else
		{
			if (offset < job_len)/* if it is not the last command */
			{
				if (dup2(fd_pipe[READ], fd_read) == SYSFAIL) syscall_error();
				if (close(fd_pipe[READ]) == SYSFAIL) syscall_error();
				if (close(fd_pipe[WRITE]) == SYSFAIL) syscall_error();
			}
			else/* if it is the last command */
				if (close(fd_read) == SYSFAIL) syscall_error();
			child_num++;
			free(command);
			command = NULL;
		}
	}
	int i = 0;
	while (i < child_num)
	{
		if (waitpid(0, NULL, 0) == SYSFAIL) syscall_error();
		i++;
	}
}

/* 
 * MAIN
 * 
 * Notions:
 * [command] | [command] | [command] ; [command]
 * \________________________________/  \_______/
 *               [job]                   [job]
 */
int main(int argc, char **argv, char **envp)
{
	if (argc > 1)
	{
		char **job = NULL;/* store pointers of argv */
		int offset = 0;
		while (offset < argc - 1)
		{
			offset++;/* go to the start-index */
 			if ((job = get_next_until(";", argv, &offset)) != NULL)/* after this: argv[offset] == ";" */
			{
				if (strcmp(job[0], "cd") == SAME)
					change_directory(job);
				else
					execute_job(job, envp);
				free(job);
				job = NULL;
			}
			else/* if there is no contents between ";" and ";" */
				continue ;
		}
	}
	return (0);
}
