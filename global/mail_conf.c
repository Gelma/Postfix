/*++
/* NAME
/*	mail_conf 3
/* SUMMARY
/*	global configuration parameter management
/* SYNOPSIS
/*	#include <mail_conf.h>
/*
/*	void	mail_conf_read()
/*
/*	void	mail_conf_update(name, value)
/*	const char *name;
/*	const char *value;
/*
/*	const char *mail_conf_lookup(name)
/*	const char *name;
/*
/*	const char *mail_conf_eval(string)
/*	const char *string;
/*
/*	const char *mail_conf_lookup_eval(name)
/*	const char *name;
/* DESCRIPTION
/*	mail_conf_read() reads the global Postfix configuration file, and
/*	stores its values into a global configuration dictionary.
/*
/*	The following routines are wrappers around the generic dictionary
/*	access routines.
/*
/*	mail_conf_update() updates the named global parameter. This has
/*	no effect on parameters whose value has already been looked up.
/*	The update succeeds or the program terminates with fatal error.
/*
/*	mail_conf_lookup() looks up the value of the named parameter.
/*	A null pointer result means the parameter was not found.
/*	The result is volatile and should be copied if it is to be
/*	used for any appreciable amount of time.
/*
/*	mail_conf_eval() recursively expands any $parameters in the
/*	string argument. The result is volatile and should be copied
/*	if it is to be used for any appreciable amount of time.
/*
/*	mail_conf_lookup_eval() looks up the named parameter, and expands any
/*	$parameters in the result. The result is volatile and should be
/*	copied if it is to be used for any appreciable amount of time.
/* DIAGNOSTICS
/*	Fatal errors: malformed numerical value.
/* FILES
/*	/etc/postfix: default Postfix configuration directory.
/* ENVIRONMENT
/*	MAIL_CONFIG, non-default configuration database
/*	MAIL_VERBOSE, enable verbose mode
/* SEE ALSO
/*	dict(3) generic dictionary manager
/*	mail_conf_int(3) integer-valued parameters
/*	mail_conf_str(3) string-valued parameters
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <stdlib.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <vstream.h>
#include <vstring.h>
#include <dict.h>
#include <safe.h>
#include <stringops.h>

/* Global library. */

#include "mail_params.h"
#include "mail_conf.h"

/* mail_conf_read - read global configuration file */

void    mail_conf_read(void)
{
    char   *config_dir;
    char   *path;

    /*
     * Permit references to unknown configuration variable names. We rely on
     * a separate configuration checking tool to spot misspelled names and
     * other kinds of trouble. Enter the configuration directory into the
     * default dictionary.
     */
    dict_unknown_allowed = 1;
    if (var_config_dir)
	myfree(var_config_dir);
    var_config_dir = mystrdup((config_dir = safe_getenv(CONF_ENV_PATH)) != 0 ?
			      config_dir : DEF_CONFIG_DIR);	/* XXX */
    set_mail_conf_str(VAR_CONFIG_DIR, var_config_dir);
    path = concatenate(var_config_dir, "/", "main.cf", (char *) 0);
    dict_load_file(CONFIG_DICT, path);
    myfree(path);
    mail_params_init();
}

/* mail_conf_eval - expand macros in string */

const char *mail_conf_eval(const char *string)
{
#define RECURSIVE	1

    return (dict_eval(CONFIG_DICT, string, RECURSIVE));
}

/* mail_conf_lookup - lookup named variable */

const char *mail_conf_lookup(const char *name)
{
    return (dict_lookup(CONFIG_DICT, name));
}

/* mail_conf_lookup_eval - expand named variable */

const char *mail_conf_lookup_eval(const char *name)
{
    const char *value;

#define RECURSIVE	1

    if ((value = dict_lookup(CONFIG_DICT, name)) != 0)
	value = dict_eval(CONFIG_DICT, value, RECURSIVE);
    return (value);
}

/* mail_conf_update - update parameter */

void    mail_conf_update(const char *key, const char *value)
{
    dict_update(CONFIG_DICT, key, value);
}
