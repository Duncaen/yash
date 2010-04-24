/* Yash: yet another shell */
/* parser.h: syntax parser */
/* (C) 2007-2009 magicant */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


/* Limitation: Don't include "parser.h" and <termios.h> at a time
 *    because identifiers prefixed "c_" conflict. */


#ifndef YASH_PARSER_H
#define YASH_PARSER_H

#include <stddef.h>
#include "input.h"


/********** Parse Tree Structures **********/

/* Basically, parse tree structure elements constitute linked lists.
 * For each element, the `next' member points to the next element. */

/* and/or list */
typedef struct and_or_T {
    struct and_or_T   *next;
    struct pipeline_T *ao_pipelines;  /* pipelines in this and/or list */
    _Bool              ao_async;      /* an asynchronous list? */
} and_or_T;

/* pipeline */
typedef struct pipeline_T {
    struct pipeline_T *next;
    struct command_T  *pl_commands;  /* commands in this pipeline */
    _Bool              pl_neg, pl_cond;
} pipeline_T;
/* pl_neg:  indicates this pipeline is prefix by "!", in which case the exit
 *          status of the pipeline is inverted.
 * pl_cond: true for "&&", false for "||". Ignored for the first pipeline in an
 *          and/or list. */

/* type of command_T */
typedef enum {
    CT_SIMPLE,     /* simple command */
    CT_GROUP,      /* command group enclosed by { } */
    CT_SUBSHELL,   /* subshell command group enclosed by ( ) */
    CT_IF,         /* if command */
    CT_FOR,        /* for command */
    CT_WHILE,      /* while/until command */
    CT_CASE,       /* case command */
    CT_FUNCDEF,    /* function definition */
} commandtype_T;

/* command in a pipeline */
typedef struct command_T {
    struct command_T *next;
    unsigned          refcount;   /* reference count */
    commandtype_T     c_type;
    unsigned long     c_lineno;   /* line number */
    struct redir_T   *c_redirs;   /* redirections */
    union {
	struct {
	    struct assign_T *assigns;  /* assignments */
	    void           **words;    /* command name and arguments */
	} simplecontent;
	struct and_or_T     *subcmds;  /* contents of command group */
	struct ifcommand_T  *ifcmds;   /* contents of if command */
	struct {
	    wchar_t         *forname;  /* loop variable of for loop */
	    void           **forwords; /* words assigned to loop variable */
	    struct and_or_T *forcmds;  /* commands executed in for loop */
	} forcontent;
	struct {
	    _Bool            whltype;  /* 1 for while loop, 0 for until */
	    struct and_or_T *whlcond;  /* loop condition of while/until loop */
	    struct and_or_T *whlcmds;  /* commands executed in loop */
	} whilecontent;
	struct {
	    struct wordunit_T *casword;   /* word compared to case patterns */
	    struct caseitem_T *casitems;  /* pairs of patterns and commands */
	} casecontent;
	struct {
	    wchar_t          *funcname;  /* name of function */
	    struct command_T *funcbody;  /* body of function */
	} funcdef;
    } c_content;
} command_T;
#define c_assigns  c_content.simplecontent.assigns
#define c_words    c_content.simplecontent.words
#define c_subcmds  c_content.subcmds
#define c_ifcmds   c_content.ifcmds
#define c_forname  c_content.forcontent.forname
#define c_forwords c_content.forcontent.forwords
#define c_forcmds  c_content.forcontent.forcmds
#define c_whltype  c_content.whilecontent.whltype
#define c_whlcond  c_content.whilecontent.whlcond
#define c_whlcmds  c_content.whilecontent.whlcmds
#define c_casword  c_content.casecontent.casword
#define c_casitems c_content.casecontent.casitems
#define c_funcname c_content.funcdef.funcname
#define c_funcbody c_content.funcdef.funcbody
/* `c_words' and `c_forwords' are NULL-terminated arrays of pointers to
 * `wordunit_T', cast to `void *'.
 * If `c_forwords' is NULL, the for loop doesn't have the "in" clause.
 * If `c_forwords[0]' is NULL, the "in" clause is empty. */

/* condition and commands of an if command */
typedef struct ifcommand_T {
    struct ifcommand_T *next;
    struct and_or_T    *ic_condition;  /* condition */
    struct and_or_T    *ic_commands;   /* commands */
} ifcommand_T;
/* For a "else" clause, `next' and `ic_condition' are NULL. */

/* patterns and commands of a case command */
typedef struct caseitem_T {
    struct caseitem_T *next;
    void             **ci_patterns;  /* patterns to do matching */
    struct and_or_T   *ci_commands;  /* commands executed if match succeeds */
} caseitem_T;
/* `ci_patterns' is a NULL-terminated array of pointers to `wordunit_T',
 * cast to `void *'. */

/* embedded command */
typedef struct embedcmd_T {
    _Bool is_preparsed;
    union {
	wchar_t  *unparsed;
	and_or_T *preparsed;
    } value;
} embedcmd_T;

/* type of wordunit_T */
typedef enum {
    WT_STRING,  /* string (including quotes) */
    WT_PARAM,   /* parameter expansion */
    WT_CMDSUB,  /* command substitution */
    WT_ARITH,   /* arithmetic expansion */
} wordunittype_T;

/* element of a word subject to expansion */
typedef struct wordunit_T {
    struct wordunit_T *next;
    wordunittype_T     wu_type;
    union {
	wchar_t           *string;  /* string (including quotes) */
	struct paramexp_T *param;   /* parameter expansion */
	embedcmd_T         cmdsub;  /* command substitution */
	struct wordunit_T *arith;   /* expression for arithmetic expansion */
    } wu_value;
} wordunit_T;
#define wu_string wu_value.string
#define wu_param  wu_value.param
#define wu_cmdsub wu_value.cmdsub
#define wu_arith  wu_value.arith
/* In arithmetic expansion, the expression is subject to parameter expansion
 * before it is parsed. So `wu_arith' is of type `wordunit_T *'. */

/* type of paramexp_T */
typedef enum {
    PT_NONE,                   /* normal */
    PT_MINUS,                  /* ${name-subst} */
    PT_PLUS,                   /* ${name+subst} */
    PT_ASSIGN,                 /* ${name=subst} */
    PT_ERROR,                  /* ${name?subst} */
    PT_MATCH,                  /* ${name#match}, ${name%match} */
    PT_SUBST,                  /* ${name/match/subst} */
    PT_MASK         = (1 << 3) - 1,
    PT_NUMBER       = 1 << 3,  /* ${#name}  (only valid for PT_NONE) */
    PT_COLON        = 1 << 4,  /* ${name:-subst}, ${name:+subst}, etc. */
    PT_MATCHHEAD    = 1 << 5,  /* only matches at the head */
    PT_MATCHTAIL    = 1 << 6,  /* only matches at the tail */
    PT_MATCHLONGEST = 1 << 7,  /* match as long as possible */
    PT_SUBSTALL     = 1 << 8,  /* substitute all the match */
    PT_NEST         = 1 << 9,  /* have nested expn. like ${${VAR#foo}%bar} */
} paramexptype_T;
/*            type   COLON  MATCHH MATCHT MATCHL SUBSTA
 * ${n-s}     MINUS   no
 * ${n+s}     PLUS    no
 * ${n=s}     ASSIGN  no
 * ${n?s}     ERROR   no
 * ${n:-s}    MINUS   yes
 * ${n:+s}    PLUS    yes
 * ${n:=s}    ASSIGN  yes
 * ${n:?s}    ERROR   yes
 * ${n#m}     MATCH   no     yes    no    no     no
 * ${n##m}    MATCH   no     yes    no    yes    no
 * ${n%m}     MATCH   no     no     yes   no     no
 * ${n%%m}    MATCH   no     no     yes   yes    no
 * ${n/m/s}   SUBST   no     no     no    yes    no
 * ${n/#m/s}  SUBST   no     yes    no    yes    no
 * ${n/%m/s}  SUBST   no     no     yes   yes    no
 * ${n//m/s}  SUBST   no     no     no    yes    yes
 * ${n:/m/s}  SUBST   yes    yes    yes
 *
 * PT_SUBST and PT_NEST is beyond POSIX. */

/* parameter expansion */
typedef struct paramexp_T {
    paramexptype_T pe_type;
    union {
	wchar_t           *name;
	struct wordunit_T *nest;
    } pe_value;
    struct wordunit_T *pe_start, *pe_end;
    struct wordunit_T *pe_match, *pe_subst;
} paramexp_T;
#define pe_name pe_value.name
#define pe_nest pe_value.nest
/* pe_name:  name of parameter
 * pe_nest:  nested parameter expansion
 * pe_start: index of the first element in the range
 * pe_end:   index of the last element in the range
 * pe_match: word to be matched with the value of the parameter
 * pe_subst: word to to substitute the matched string with
 * `pe_start' and `pe_end' is NULL if the indices are not specified.
 * `pe_match' and `pe_subst' may be NULL to denote an empty string. */

/* type of assignment */
typedef enum {
    A_SCALAR, A_ARRAY,
} assigntype_T;

/* assignment */
typedef struct assign_T {
    struct assign_T *next;
    assigntype_T a_type;
    wchar_t *a_name;
    union {
	struct wordunit_T *scalar;
	void **array;          
    } a_value;  /* value to assign */
} assign_T;
#define a_scalar a_value.scalar
#define a_array  a_value.array
/* `a_scalar' may be NULL to denote an empty string.
 * `a_array' is an array of pointers to `wordunit_T'. */

/* type of redirection */
typedef enum {
    RT_INPUT,    /* <file */
    RT_OUTPUT,   /* >file */
    RT_CLOBBER,  /* >|file */
    RT_APPEND,   /* >>file */
    RT_INOUT,    /* <>file */
    RT_DUPIN,    /* <&fd */
    RT_DUPOUT,   /* >&fd */
    RT_PIPE,     /* >>|fd */
    RT_HERE,     /* <<END */
    RT_HERERT,   /* <<-END */
    RT_HERESTR,  /* <<<str */
    RT_PROCIN,   /* <(command) */
    RT_PROCOUT,  /* >(command) */
} redirtype_T;

/* redirection */
typedef struct redir_T {
    struct redir_T *next;
    redirtype_T rd_type;
    int rd_fd;  /* file descriptor to redirect */
    union {
	struct wordunit_T *filename;
	struct {
	    wchar_t *hereend;  /* token indicating end of here-document */
	    struct wordunit_T *herecontent;  /* contents of here-document */
	} heredoc;
	embedcmd_T command;
    } rd_value;
} redir_T;
#define rd_filename    rd_value.filename
#define rd_hereend     rd_value.heredoc.hereend
#define rd_herecontent rd_value.heredoc.herecontent
#define rd_command     rd_value.command
/* For example, for "2>&1", `rd_type' = RT_DUPOUT, `rd_fd' = 2 and
 * `rd_filename' = "1".
 * For RT_HERERT, all the lines in `rd_herecontent' have the leading tabs
 * already removed. If `rd_hereend' is quoted, `rd_herecontent' is a single
 * word unit of type WT_STRING, since no parameter expansions are performed.
 * Anyway `rd_herecontent' is expanded calling `expand_string' with `esc'
 * argument being true. */


/********** Interface to Parsing Routines **********/

struct parsestate_T;

/* object containing the info for parsing */
typedef struct parseinfo_T {
    _Bool print_errmsg;   /* print error messages? */
    _Bool enable_verbose; /* echo input if `shopt_verbose' is true? */
#if YASH_ENABLE_ALIAS
    _Bool enable_alias;   /* perform alias substitution? */
#endif
    const char *filename; /* the input filename, which may be NULL */
    unsigned long lineno; /* line number, which should be initialized to 1 */
    inputfunc_T *input;   /* input function */
    void *inputinfo;      /* pointer passed to the input function */
    _Bool intrinput;      /* input is interactive? */
    inputresult_T lastinputresult;  /* last return value of input function */
} parseinfo_T;
/* If `intrinput' is true, `input' is `input_readline' and `inputinfo' is a
 * pointer to a `struct input_readline_info' object.
 * Note that input may not be from a terminal even if `intrinput' is true. */

typedef enum parseresult_T {
    PR_OK, PR_EOF, PR_SYNTAX_ERROR, PR_INPUT_ERROR,
} parseresult_T;

extern struct parsestate_T *save_parse_state(void)
    __attribute__((malloc,warn_unused_result));
extern void restore_parse_state(struct parsestate_T *state)
    __attribute__((nonnull));


extern parseresult_T read_and_parse(
	parseinfo_T *restrict info, and_or_T **restrict result)
    __attribute__((nonnull));

extern _Bool parse_string(
	parseinfo_T *restrict info, wordunit_T **restrict result)
    __attribute__((nonnull));


/********** Auxiliary Functions **********/

extern _Bool is_name_char(wchar_t c)
    __attribute__((pure));
extern _Bool is_name(const wchar_t *s)
    __attribute__((pure));
extern _Bool is_keyword(const wchar_t *s)
    __attribute__((nonnull,pure));
extern _Bool is_token_delimiter_char(wchar_t c)
    __attribute__((pure));


/********** Functions That Convert Parse Trees into Strings **********/

extern wchar_t *pipelines_to_wcs(const pipeline_T *pipelines)
    __attribute__((malloc,warn_unused_result));
extern wchar_t *command_to_wcs(const command_T *command, _Bool multiline)
    __attribute__((malloc,warn_unused_result));


/********** Functions That Free/Duplicate Parse Trees **********/

extern void andorsfree(and_or_T *a);
extern void wordfree(wordunit_T *w);
static inline command_T *comsdup(command_T *c);
extern void comsfree(command_T *c);


/* Duplicates the specified command (virtually). */
command_T *comsdup(command_T *c)
{
    c->refcount++;
    return c;
}


#endif /* YASH_PARSER_H */


/* vim: set ts=8 sts=4 sw=4 noet tw=80: */
