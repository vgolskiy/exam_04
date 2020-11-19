#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define PIPE_IN		1
#define PIPE_OUT	0

#define TYPE_END	0
#define TYPE_PIPE	1
#define TYPE_BREAK	2

#ifdef TEST_SH
# define TEST		1
#else
# define TEST		0
#endif

typedef struct	s_list
{
	char			**args;
	int				len;
	int				type;
	int				pipe[2];
	struct s_list	*prev;
	struct s_list	*next;
}				t_list;

int		ft_strlen(const char *s)
{
	int	i;

	i = 0;
	if (s)
		while (s[i])
			i++;
	return (i);
}

int		puterr(const char *s)
{
	if (s)
		write(STDERR_FILENO, s, ft_strlen(s));
	return (EXIT_FAILURE);
}

int		error(void)
{
	puterr("error: fatal\n");
	exit(EXIT_FAILURE);
	return (EXIT_FAILURE);
}

void	*error_ptr(void)
{
	error();
	return (00);
}

void	*ft_calloc(int len, int size)
{
	void	*res;
	char	*tmp;
	int		i;

	if (!(res = malloc(len * size)))
		return (error_ptr());
	tmp = (char *)res;
	i = -1;
	while (++i < (len * size))
		tmp[i] = '\0';
	return (res);
}

char	*ft_strdup(const char *s)
{
	char	*res;
	int		i;
	int		len;

	len = ft_strlen(s);
	if (!(res = (char *)ft_calloc((len + 1), sizeof(char))))
		return (00);
	i = -1;
	while (++i < len)
		res[i] = s[i];
	return (res);
}

t_list	*list_new(void)
{
	t_list	*new;

	if (!(new = (t_list *)malloc(sizeof(t_list))))
		return (error_ptr());
	new->args = 00;
	new->len = 0;
	new->type = TYPE_END;
	new->next = 00;
	new->prev = 00;
	return (new);
}

int		add_arg(t_list *list, char *arg)
{
	char	**res;
	int		i;

	if (!(res = (char **)ft_calloc(list->len + 2, sizeof(char *))))
		return (EXIT_FAILURE);
	i = -1;
	while (++i < list->len)
		res[i] = list->args[i];
	if (!(res[i] = ft_strdup(arg)))
		return (EXIT_FAILURE);
	if (list->len)
		free(list->args);
	list->args = res;
	list->len++;
	return (EXIT_SUCCESS);
}

int		list_push(t_list **list, char *arg)
{
	t_list	*new;

	new = list_new();
	if (*list)
	{
		(*list)->next = new;
		new->prev = *list;
	}
	*list = new;
	return (add_arg(new, arg));
}

void	list_first(t_list **list)
{
	while (*list && (*list)->prev)
		*list = (*list)->prev;
}

void	list_clean(t_list **list)
{
	t_list	*tmp;
	int		i;

	list_first(list);
	while (*list)
	{
		tmp = (*list)->next;
		i = -1;
		while (++i < (*list)->len)
			free((*list)->args[i]);
		free((*list)->args);
		free(*list);
		*list = tmp;
	}
	*list = 00;
}

int		parser(t_list **cmd, char *arg)
{
	int	stop;

	stop = (!(strcmp(";", arg)));
	if (stop && !*cmd)
		return (EXIT_SUCCESS);
	else if (!stop && (!*cmd || (*cmd)->type > TYPE_END))
		return (list_push(cmd, arg));
	else if (!(strcmp("|", arg)))
		(*cmd)->type = TYPE_PIPE;
	else if (stop)
		(*cmd)->type = TYPE_BREAK;
	else
		return (add_arg(*cmd, arg));
	return (EXIT_SUCCESS);
}

int		exec_cmd(t_list *cmd, char **env)
{
	pid_t	pid;
	int		status;
	int		res;
	int		pipe_open;

	pipe_open = 0;
	res = EXIT_FAILURE;
	if (cmd->type == TYPE_PIPE || (cmd->prev && cmd->prev->type == TYPE_PIPE))
	{
		pipe_open = 1;
		if (pipe(cmd->pipe))
			return (error());
	}
	pid = fork();
	if (pid < 0)
		return (error());
	if (!pid)
	{
		if (cmd->type == TYPE_PIPE
		&& dup2(cmd->pipe[PIPE_IN], STDOUT_FILENO) < 0)
			return (error());
		if (cmd->prev && cmd->prev->type == TYPE_PIPE
		&& dup2(cmd->prev->pipe[PIPE_OUT], STDIN_FILENO) < 0)
			return (error());
		if ((res = execve(cmd->args[0], cmd->args, env)) < 0)
		{
			puterr("error: cannot execute ");
			puterr(cmd->args[0]);
			puterr("\n");
		}
		exit(res);
	}
	else
	{
		waitpid(pid, &status, 0);
		if (pipe_open)
		{
			close(cmd->pipe[PIPE_IN]);
			if (!cmd->next || cmd->type == TYPE_BREAK)
				close(cmd->pipe[PIPE_OUT]);
		}
		if (cmd->prev && cmd->prev->type == TYPE_PIPE)
			close(cmd->prev->pipe[PIPE_OUT]);
		if (WIFEXITED(status))
			res = WEXITSTATUS(status);
	}
	return (res);
}

int		exec_cmds(t_list **cmds, char **env)
{
//	t_list	*curr;
	int		res;

	res = EXIT_SUCCESS;
	list_first(cmds);
	while (*cmds)
	{
	//	curr = *cmds;
		res = EXIT_SUCCESS;
		if (!strcmp("cd", (*cmds)->args[0]))
		{
			if ((*cmds)->len < 2)
				res = puterr("error: cd: bad arguments");
			else if (chdir((*cmds)->args[1]))
			{
				res = puterr("error: cd: cannot change directory to ");
				puterr((*cmds)->args[1]);
				puterr("\n");
			}
		}
		else
			res = exec_cmd(*cmds, env);
		if (!(*cmds)->next)
			break ;
		*cmds = (*cmds)->next;
	}
	return (res);
}

int		main(int argc, char **argv, char **env)
{
	int		i;
	int		res;
	t_list	*cmds;

	res = EXIT_SUCCESS;
	cmds = 00;
	i = 0;
	while (++i < argc)
		if (parser(&cmds, argv[i]))
			return (EXIT_FAILURE);
	if (cmds)
		res = exec_cmds(&cmds, env);
	list_clean(&cmds);
	if (TEST)
		while (1);
	return (res);
}
	