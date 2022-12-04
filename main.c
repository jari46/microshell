//allowed functions: malloc, free, write, close, fork, waitpid, signal, kill, exit, chdir, execve, dup, dup2, pipe, strcmp, n

#define NULL 0

#define TRUE 1
#define FALSE 0

#define SYSFAIL -1 /*system call fail*/

#define CHILD 0

#define READ 0
#define WRITE 1

#define SAME 0

#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

int is_with(char **arr, char *str)
{
	int i = 0;
	while (arr[i])
	{
		if (strcmp(arr[i], str) == SAME)
			return (TRUE);
		i++;
	}
	return (FALSE);
}

void remove_pipe(char **command)
{
	int i = 0;
	while (command[i])
	{
		if (strcmp(command[i], "|") == SAME)
		{
			command[i] = NULL;
			break ;
		}
		i++;
	}
}

void display_error(const char *str)
{
	int i = 0;
	while (str[i])
	{
		write(STDERR_FILENO, &str[i], 1);//also can fail, but no error messages can be displayed...
		i++;
	}
}

void syscall_error()
{
	display_error("error: fatal\n");
	exit(1);
}

int count_until(char **job, int offset, char *delim)
{
	int i = 0;
	while (job[offset + i])
	{
		if (strcmp(job[offset + i], delim) == SAME)
		{
			i++;
			break ;
		}
		else
			i++;
	}
	return (i);
}

/*type 1*/
void change_directory(char **command)
{
	if (count_until(command, 0, "") != 2)
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

/*type 2*/
void execve_single_command(char **command, char **envp)
{
	pid_t pid;

	if ((pid = fork()) == SYSFAIL) syscall_error();
	if (pid == CHILD)
	{
		if (execve(command[0], command, envp) == SYSFAIL) syscall_error();
	}
	else
		waitpid(pid, NULL, 0);
}

char **get_next_command(char **job)
{
	static int offset = 0;
	char **command;
	int command_len;

	command_len = count_until(job, offset, "|");//offset부터 pipe까지
	if ((command = malloc(sizeof(char *) * (command_len + 1))) == NULL) syscall_error();

	int i = 0;
	while (i < command_len)
	{
		command[i] = job[offset + i];
		i++;
	}
	command[i] = NULL;
	offset += command_len;
	return (command);
}

char **get_next_job(char **argv)
{
	static int offset = 1;
	char **job;
	int job_len;

	job_len = count_until(argv, offset, ";");
	if (is_with(argv, ";") == TRUE)
		job_len--;

	if (job_len == 0)
		return (NULL);

	if ((job = malloc(sizeof(char *) * (job_len + 1))) == NULL) syscall_error();
	
	int i = 0;
	while (i < job_len)
	{
		job[i] = argv[offset + i];
		i++;
	}
	job[i] = NULL;
	if (is_with(argv, ";") == TRUE)
		job_len++;
	offset += job_len;
	return (job);
}
/*type 3*/
void execve_multi_commands(char **job, char **envp)
{
	int fd_read;
	int fd_pipe[2];
	pid_t pid;
	int child_num = 0;
	char **command = NULL;

	if ((fd_read = dup(STDIN_FILENO)) == SYSFAIL) syscall_error();
	while (TRUE)
	{
		if ((command = get_next_command(job)) == NULL) //파이프 있으면 파이프까지, 아니면 끝까지
			break;
		if (is_with(command, "|") == TRUE)
		{
			if (pipe(fd_pipe) == SYSFAIL) syscall_error();
		}
		if ((pid = fork()) == SYSFAIL) syscall_error();
		if (pid == CHILD)
		{
			dup2(fd_read, STDIN_FILENO);
			close(fd_read);
			if (is_with(command, "|") == TRUE)
			{
				if (dup2(fd_pipe[WRITE], STDOUT_FILENO) == SYSFAIL) syscall_error();
				if (close(fd_pipe[READ]) == SYSFAIL) syscall_error();
				if (close(fd_pipe[WRITE]) == SYSFAIL) syscall_error();
				remove_pipe(command);
			}
			if (execve(command[0], command, envp) == SYSFAIL) syscall_error();
		}
		else
		{
			if (is_with(command, "|") == TRUE)
			{
				if (dup2(fd_pipe[READ], fd_read) == SYSFAIL) syscall_error();
				if (close(fd_pipe[READ]) == SYSFAIL) syscall_error();
				if (close(fd_pipe[WRITE]) == SYSFAIL) syscall_error();
			}
			child_num++;
			free(command);
		}
	}
}

int main(int argc, char **argv, char **envp)
{
	if (argc > 1)
	{
		char **job = NULL;
		while (TRUE)
		{
			if ((job = get_next_job(argv)) == NULL)//allocate, 마지막에 NULL도 붙여주어야 cd에서 셀 수 있다. ; 없이 보내주고 offset은 그 다음으로 이동
				break ;
			if (is_with(job, "|") == FALSE)
			{
				if (strcmp(job[0], "cd") == SAME)
					change_directory(job);/*type 1*/
				else
					execve_single_command(job, envp);/*type 2*/
			}
			else
				execve_multi_commands(job, envp);/*type 3*/
			free(job);
		}
	}
	return (0);
}