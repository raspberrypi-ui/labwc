// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "common/spawn.h"
#include "common/fd_util.h"

void
spawn_async_no_shell(char const *command)
{
	GError *err = NULL;
	gchar **argv = NULL;

	assert(command);

	/* Use glib's shell-parse to mimic Openbox's behaviour */
	g_shell_parse_argv((gchar *)command, NULL, &argv, &err);
	if (err) {
		g_message("%s", err->message);
		g_error_free(err);
		return;
	}

	/*
	 * Avoid zombie processes by using a double-fork, whereby the
	 * grandchild becomes orphaned & the responsibility of the OS.
	 */
	pid_t child = 0, grandchild = 0;

	child = fork();
	switch (child) {
	case -1:
		wlr_log(WLR_ERROR, "unable to fork()");
		goto out;
	case 0:
		restore_nofile_limit();

		setsid();
		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, NULL);
		/* Restore ignored signals */
		signal(SIGPIPE, SIG_DFL);
		grandchild = fork();
		if (grandchild == 0) {
			execvp(argv[0], argv);
			_exit(0);
		} else if (grandchild < 0) {
			wlr_log(WLR_ERROR, "unable to fork()");
		}
		_exit(0);
	default:
		break;
	}
	waitpid(child, NULL, 0);
out:
	g_strfreev(argv);
}

