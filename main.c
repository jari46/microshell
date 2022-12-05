//allowed functions: malloc, free, write, close, fork, waitpid, signal, kill, exit, chdir, execve, dup, dup2, pipe, strcmp, strncmp

#define NULL 0
#define TRUE 1
#define FALSE 0
#define SYSFAIL -1 /* system call return */
#define CHILD 0
#define READ 0
#define WRITE 1
#define SAME 0

#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

/* 
 * ERROR CONTROL
 */
void display_error(const char *str)
{
	int i = 0;
	while (str[i])
	{
		write(STDERR_FILENO, &str[i], 1);
		i++;
	}
}

void syscall_error(void)
{
	display_error("error: fatal\n");
	exit(1);
}

/* 
 * UTILS FOR 'ARRAY OF POINTERS'
 */

int count_until(char *delim, char **job, int offset)
{
	int i = 0;
	while (job[offset + i] && (strcmp(job[offset + i], delim) != SAME))
		i++;
	return (i);
}

char **get_next_until(char *delim, char **job, int *offset)
{
	char **command;
	int command_len;

	command_len = count_until(delim, job, *offset);
	if (command_len == 0)
		return (NULL);
	if ((command = malloc(sizeof(char *) * (command_len + 1))) == NULL) syscall_error();
	int i = 0;
	while (i < command_len)
	{
		command[i] = job[*offset + i];
		i++;
	}
	command[i] = NULL;
	(*offset) += command_len;
	return (command);
}

/* 
 * MAIN FUNCTIONS
 */

void change_directory(char **command)
{
	if (count_until("", command, 0) != 2)
			display_error("error: cd: bad arguments\n");
	else
	{
		if (chdir(command[1]) == SYSFAIL)
		{
			display_error("error: cd: cannot change directory to ");
			display_error(command[1]);
			display_error("\n");
		}
	}
}

void execve_job(char **job, char **envp)
{
	int fd_read;
	int fd_pipe[2];
	pid_t pid;
	int child_num = 0;

	if ((fd_read = dup(STDIN_FILENO)) == SYSFAIL) syscall_error();
	int job_len = count_until("", job, 0);
	char **command = NULL;
	int offset = -1;
	while (offset < job_len)
	{
		offset++;/* go to the start-index */
		if ((command = get_next_until("|", job, &offset)) == NULL)/* after this: argv[offset] == "|" */
			break;
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
			if (execve(command[0], command, envp) == SYSFAIL) syscall_error();
		}
		else
		{
			if (offset < job_len)/* if it is not the last command */
			{
				if (dup2(fd_pipe[READ], fd_read) == SYSFAIL) syscall_error();
				if (close(fd_pipe[READ]) == SYSFAIL) syscall_error();
				if (close(fd_pipe[WRITE]) == SYSFAIL) syscall_error();
			}
			child_num++;
			free(command);
			command = NULL;
		}
	}
	if (close(fd_read) == SYSFAIL) syscall_error();
	int i = 0;
	while (i < child_num)
	{
		if (waitpid(0, NULL, 0) == SYSFAIL) syscall_error();
		i++;
	}
}

/* 
 * MAIN
 */

int main(int argc, char **argv, char **envp)
{
	if (argc > 1)
	{
		char **job = NULL;
		int offset = 0;
		while (offset < argc - 1)
		{
			offset++;/* go to the start-index */
			if ((job = get_next_until(";", argv, &offset)) == NULL)/* after this: argv[offset] == ";" */
				continue ;
			if (strcmp(job[0], "cd") == SAME)
				change_directory(job);
			else
				execve_job(job, envp);
			free(job);
			job = NULL;
		}
	}
	return (0);
}
