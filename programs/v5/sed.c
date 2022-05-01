#define COPYRIGHT_NOTICE "Copyright (C) 2003 Free Software Foundation, Inc."
#define BUG_ADDRESS "bonzini@gnu.org"

/*  GNU SED, a batch stream editor.
    Copyright (C) 1989,90,91,92,93,94,95,98,99,2002,2003
    Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/*
  Fix a little non-determinism
  for SIR July 2006, Kyle R. Murphy
*/
#undef stderr
#define stderr stdout

#include "config.h"

#include <stdio.h>
#ifdef HAVE_STRINGS_H
# include <strings.h>
#else
# include <string.h>
#endif /*HAVE_STRINGS_H*/
#ifdef HAVE_MEMORY_H
# include <memory.h>
#endif

#ifndef HAVE_STRCHR
# define strchr index
# define strrchr rindex
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif
#include "getopt.h"
#include "basicdefs.h"
#include "utils.h"
#include "sed.h"

#ifndef BOOTSTRAP
#ifndef HAVE_STDLIB_H
 extern char *getenv P_((const char *));
#endif
#endif

#ifndef HAVE_STRTOUL
# define ATOI(x)	atoi(x)
#else
# define ATOI(x)	strtoul(x, NULL, 0)
#endif

int extended_regexp_flags = 0;

/* If set, fflush(stdout) on every line output. */
flagT unbuffered_output = FALSE;

/* If set, don't write out the line unless explicitly told to */
flagT no_default_output = FALSE;

/* If set, reset line counts on every new file. */
flagT separate_files = FALSE;

/* How do we edit files in-place? (we don't if NULL) */
char *in_place_extension = NULL;

/* Do we need to be pedantically POSIX compliant? */
flagT POSIXLY_CORRECT;

/* How long should the `l' command's output line be? */
countT lcmd_out_line_len = 70;

/* The complete compiled SED program that we are going to run: */
static struct vector *the_program = NULL;

static void usage P_((int));
static void
usage(status)
  int status;
{
  FILE *out = status ? stderr : stdout;

#ifdef REG_PERL
#define PERL_HELP _("  -R, --regexp-perl\n                 use Perl 5's regular expressions syntax in the script.\n")
#else
#define PERL_HELP ""
#endif

  fprintf(out, _("\
Usage: %s [OPTION]... {script-only-if-no-other-script} [input-file]...\n\
\n\
  -n, --quiet, --silent\n\
                 suppress automatic printing of pattern space\n\
  -e script, --expression=script\n\
                 add the script to the commands to be executed\n\
  -f script-file, --file=script-file\n\
                 add the contents of script-file to the commands to be executed\n\
  -i[suffix], --in-place[=suffix]\n\
                 edit files in place (makes backup if extension supplied)\n\
  -l N, --line-length=N\n\
                 specify the desired line-wrap length for the `l' command\n\
  -r, --regexp-extended\n\
                 use extended regular expressions in the script.\n%s\
  -s, --separate\n\
                 consider files as separate rather than as a single continuous\n\
                 long stream.\n\
  -u, --unbuffered\n\
                 load minimal amounts of data from the input files and flush\n\
                 the output buffers more often\n\
      --help     display this help and exit\n\
  -V, --version  output version information and exit\n\
\n\
If no -e, --expression, -f, or --file option is given, then the first\n\
non-option argument is taken as the sed script to interpret.  All\n\
remaining arguments are names of input files; if no input files are\n\
specified, then the standard input is read.\n\
\n"), myname, PERL_HELP);
  fprintf(out, _("E-mail bug reports to: %s .\n\
Be sure to include the word ``%s'' somewhere in the ``Subject:'' field.\n"),
	  BUG_ADDRESS, PACKAGE);

  ck_fclose (NULL);
  exit (status);
}

int
main(argc, argv)
  int argc;
  char **argv;
{
#ifdef REG_PERL
#define SHORTOPTS "shnrRuVe:f:l:i::"
#else
#define SHORTOPTS "shnruVe:f:l:i::"
#endif

  static struct option longopts[] = {
    {"regexp-extended", 0, NULL, 'r'},
#ifdef REG_PERL
    {"regexp-perl", 0, NULL, 'R'},
#endif
    {"expression", 1, NULL, 'e'},
    {"file", 1, NULL, 'f'},
    {"in-place", 2, NULL, 'i'},
    {"line-length", 1, NULL, 'l'},
    {"quiet", 0, NULL, 'n'},
    {"silent", 0, NULL, 'n'},
    {"separate", 0, NULL, 's'},
    {"unbuffered", 0, NULL, 'u'},
    {"version", 0, NULL, 'V'},
    {"help", 0, NULL, 'h'},
    {NULL, 0, NULL, 0}
  };

  int opt;
  int return_code;

  initialize_main(&argc, &argv);

#if ENABLE_NLS
#if HAVE_SETLOCALE
  /* Set locale according to user's wishes.  */
  setlocale (LC_ALL, "");
#endif

  /* Tell program which translations to use and where to find.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
#endif

  POSIXLY_CORRECT = (getenv("POSIXLY_CORRECT") != NULL);

  /* If environment variable `COLS' is set, use its value for
     the baseline setting of `lcmd_out_line_len'.  The "-1"
     is to avoid gratuitous auto-line-wrap on ttys.
   */
  {
    const char *p = getenv("COLS");
    if (p)
      {
	countT t = ATOI(p);
	if (1 < t)
	  lcmd_out_line_len = t-1;
      }
  }
/*
  More non-determinism -- SIR July 2006
  by Kyle R. Murphy
  myname = *argv;
*/
  myname = "Executable";
  while ((opt = getopt_long(argc, argv, SHORTOPTS, longopts, NULL)) != EOF)
    {
      switch (opt)
	{
	case 'n':
	  no_default_output = TRUE;
	  break;
	case 'e':
	  the_program = compile_string(the_program, optarg, strlen(optarg));
	  break;
	case 'f':
	  the_program = compile_file(the_program, optarg);
	  break;

	case 'i':
	  separate_files = TRUE;
	  if (optarg == NULL)
	    in_place_extension = "*";
	  else if (strchr(optarg, '*') == NULL)
	    {
	      in_place_extension = xmalloc (strlen(optarg) + 2);
	      in_place_extension[0] = '*';
	      strcpy (in_place_extension + 1, optarg);
	    }
	  else
	    in_place_extension = ck_strdup(optarg);
	  break;

	case 'l':
	  lcmd_out_line_len = ATOI(optarg);
	  break;
	case 'r':
	  if (extended_regexp_flags)
	    usage(4);
	  extended_regexp_flags = REG_EXTENDED;
	  break;

#ifdef REG_PERL
	case 'R':
	  if (extended_regexp_flags)
	    usage(4);
	  extended_regexp_flags = REG_PERL;
	  break;
#endif

	case 's':
	  separate_files = TRUE;
	  break;

	case 'u':
	  unbuffered_output = TRUE;
	  break;

	case 'V':
#ifdef REG_PERL
	  fprintf(stdout, _("super-sed version %s\n"), VERSION);
	  fprintf(stdout, _("based on GNU sed version 3.02.80\n\n"));
#else
	  fprintf(stdout, _("GNU sed version %s\n"), VERSION);
#endif
	  fprintf(stdout, _("%s\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE,\n\
to the extent permitted by law.\n\
"), COPYRIGHT_NOTICE);
	  ck_fclose (NULL);
	  exit (0);
	case 'h':
	  usage(0);
	default:
	  usage(4);
	}
    }

  if (!the_program)
    {
      if (optind < argc)
	{
	  char *arg = argv[optind++];
	  the_program = compile_string(the_program, arg, strlen(arg));
	}
      else
	usage(4);
    }
  check_final_program(the_program);

  return_code = process_files(the_program, argv+optind);

  finish_program(the_program);
  ck_fclose(NULL);

  return return_code;
}
/*  GNU SED, a batch stream editor.
    Copyright (C) 1989,90,91,92,93,94,95,98,99,2002,2003
    Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* compile.c: translate sed source into internal form */

#include "config.h"
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STRINGS_H
# include <strings.h>
# ifdef HAVE_MEMORY_H
#  include <memory.h>
# endif
#else
# include <string.h>
#endif /* HAVE_STRINGS_H */

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

#if defined HAVE_WCTYPE_H && defined HAVE_WCHAR_H && defined HAVE_MBRTOWC
/* We can handle multibyte string.  */
# include <wchar.h>
# include <wctype.h>
# define MBS_SUPPORT
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include "obstack.h"
#include "basicdefs.h"
#include "utils.h"

extern flagT no_default_output;
extern flagT POSIXLY_CORRECT;


#define YMAP_LENGTH		256 /*XXX shouldn't this be (UCHAR_MAX+1)?*/
#define VECTOR_ALLOC_INCREMENT	40

/* let's not confuse text editors that have only dumb bracket-matching... */
#define OPEN_BRACKET	'['
#define CLOSE_BRACKET	']'
#define OPEN_BRACE	'{'
#define CLOSE_BRACE	'}'

struct prog_info {
  /* When we're reading a script command from a string, `prog.base'
     points to the first character in the string, 'prog.cur' points
     to the current character in the string, and 'prog.end' points
     to the end of the string.  This allows us to compile script
     strings that contain nulls. */
  const unsigned char *base;
  const unsigned char *cur;
  const unsigned char *end;

  /* This is the current script file.  If it is NULL, we are reading
     from a string stored at `prog.cur' instead.  If both `prog.file'
     and `prog.cur' are NULL, we're in trouble! */
  FILE *file;
};

/* Information used to give out useful and informative error messages. */
struct error_info {
  /* This is the name of the current script file. */
  const char *name;

  /* This is the number of the current script line that we're compiling. */
  countT line;

  /* This is the index of the "-e" expressions on the command line. */
  countT string_expr_count;
};


/* Label structure used to resolve GOTO's, labels, and block beginnings. */
struct sed_label {
  countT v_index;		/* index of vector element being referenced */
  char *name;			/* NUL-terminated name of the label */
  struct error_info err_info;	/* track where `{}' blocks start */
  struct sed_label *next;	/* linked list (stack) */
};

struct special_files {
  char *name;
  FILE **pfp;
};

FILE *my_stdin, *my_stdout, *my_stderr;
struct special_files special_files[] = {
  { "/dev/stdin", &my_stdin },
  { "/dev/stdout", &my_stdout },
  { "/dev/stdout", &my_stderr },
  { NULL, NULL }
};

/* This structure tracks files opened by the `w' and `s///w' commands
   so that they may all be closed cleanly at normal program termination.
   Those marked as `special' are not closed. */
struct fp_list {
    char *name;
    int special;
    FILE *fp;
    struct fp_list *link;
  };


/* Where we are in the processing of the input. */
static struct prog_info prog;
static struct error_info cur_input;

/* Information about labels and jumps-to-labels.  This is used to do
   the required backpatching after we have compiled all the scripts. */
static struct sed_label *jumps = NULL;
static struct sed_label *labels = NULL;

/* We wish to detect #n magic only in the first input argument;
   this flag tracks when we have consumed the first file of input. */
static flagT first_script = TRUE;

/* Allow for scripts like "sed -e 'i\' -e foo": */
static struct buffer *pending_text = NULL;
static struct text_buf *old_text_buf = NULL;

/* Information about block start positions.  This is used to backpatch
   block end positions. */
static struct sed_label *blocks = NULL;

/* Use an obstack for compilation. */
static struct obstack obs;

/* Various error messages we may want to print */
static const char errors[] =
  "Multiple `!'s\0"
  "Unexpected `,'\0"
  "Cannot use +N or ~N as first address\0"
  "Unmatched `{'\0"
  "Unexpected `}'\0"
  "Extra characters after command\0"
  "Expected \\ after `a', `c' or `i'\0"
  "`}' doesn't want any addresses\0"
  ": doesn't want any addresses\0"
  "Comments don't accept any addresses\0"
  "Missing command\0"
  "Command only uses one address\0"
  "Unterminated address regex\0"
  "Unterminated `s' command\0"
  "Unterminated `y' command\0"
  "Unknown option to `s'\0"
  "multiple `p' options to `s' command\0"
  "multiple `g' options to `s' command\0"
  "multiple number options to `s' command\0"
  "number option to `s' command may not be zero\0"
  "strings for y command are different lengths\0"
  "expected newer version of sed";

#define BAD_BANG (errors)
#define BAD_COMMA (BAD_BANG + sizeof(N_("Multiple `!'s")))
#define BAD_PLUS (BAD_COMMA + sizeof(N_("Unexpected `,'")))
#define EXCESS_OPEN_BRACE (BAD_PLUS + sizeof(N_("Cannot use +N or ~N as first address")))
#define EXCESS_CLOSE_BRACE (EXCESS_OPEN_BRACE + sizeof(N_("Unmatched `{'")))
#define EXCESS_JUNK (EXCESS_CLOSE_BRACE + sizeof(N_("Unexpected `}'")))
#define EXPECTED_SLASH (EXCESS_JUNK + sizeof(N_("Extra characters after command")))
#define NO_CLOSE_BRACE_ADDR (EXPECTED_SLASH + sizeof(N_("Expected \\ after `a', `c' or `i'")))
#define NO_COLON_ADDR (NO_CLOSE_BRACE_ADDR + sizeof(N_("`}' doesn't want any addresses")))
#define NO_SHARP_ADDR (NO_COLON_ADDR + sizeof(N_(": doesn't want any addresses")))
#define NO_COMMAND (NO_SHARP_ADDR + sizeof(N_("Comments don't accept any addresses")))
#define ONE_ADDR (NO_COMMAND + sizeof(N_("Missing command")))
#define UNTERM_ADDR_RE (ONE_ADDR + sizeof(N_("Command only uses one address")))
#define UNTERM_S_CMD (UNTERM_ADDR_RE + sizeof(N_("Unterminated address regex")))
#define UNTERM_Y_CMD (UNTERM_S_CMD + sizeof(N_("Unterminated `s' command")))
#define UNKNOWN_S_OPT (UNTERM_Y_CMD + sizeof(N_("Unterminated `y' command")))
#define EXCESS_P_OPT (UNKNOWN_S_OPT + sizeof(N_("Unknown option to `s'")))
#define EXCESS_G_OPT (EXCESS_P_OPT + sizeof(N_("multiple `p' options to `s' command")))
#define EXCESS_N_OPT (EXCESS_G_OPT + sizeof(N_("multiple `g' options to `s' command")))
#define ZERO_N_OPT (EXCESS_N_OPT + sizeof(N_("multiple number options to `s' command")))
#define Y_CMD_LEN (ZERO_N_OPT + sizeof(N_("number option to `s' command may not be zero")))
#define ANCIENT_VERSION (Y_CMD_LEN + sizeof(N_("strings for y command are different lengths")))
#define END_ERRORS (ANCIENT_VERSION + sizeof(N_("expected newer version of sed")))

static struct fp_list *file_read = NULL;
static struct fp_list *file_write = NULL;


/* Read the next character from the program.  Return EOF if there isn't
   anything to read.  Keep cur_input.line up to date, so error messages
   can be meaningful. */
static int inchar P_((void));
static int
inchar()
{
  int ch = EOF;

  if (prog.cur)
    {
      if (prog.cur < prog.end)
	ch = *prog.cur++;
    }
  else if (prog.file)
    {
      if (!feof(prog.file))
	ch = getc(prog.file);
    }
  if (ch == '\n')
    ++cur_input.line;
  return ch;
}

/* unget `ch' so the next call to inchar will return it.   */
static void savchar P_((int ch));
static void
savchar(ch)
  int ch;
{
  if (ch == EOF)
    return;
  if (ch == '\n' && cur_input.line > 0)
    --cur_input.line;
  if (prog.cur)
    {
      if (prog.cur <= prog.base || *--prog.cur != ch)
	panic(_("Called savchar() with unexpected pushback (%x)"),
	      CAST(unsigned char)ch);
    }
  else
    ungetc(ch, prog.file);
}

/* Read the next non-blank character from the program.  */
static int in_nonblank P_((void));
static int
in_nonblank()
{
  int ch;
  do
    ch = inchar();
    while (ISBLANK(ch));
  return ch;
}

/* Read an integer value from the program.  */
static countT in_integer P_((int ch));
static countT
in_integer(ch)
  int ch;
{
  countT num = 0;

  while (ISDIGIT(ch))
    {
      num = num * 10 + ch - '0';
      ch = inchar();
    }
  savchar(ch);
  return num;
}

static int add_then_next P_((struct buffer *b, int ch));
static int
add_then_next(b, ch)
  struct buffer *b;
  int ch;
{
  add1_buffer(b, ch);
  return inchar();
}

static char * convert_number P_((char *, char *, const char *, int, int, int));
static char *
convert_number(result, buf, bufend, base, maxdigits, default_char)
  char *result;
  char *buf;
  const char *bufend;
  int base;
  int maxdigits;
  int default_char;
{
  int n = 0;
  char *p;

  for (p=buf; p < bufend && maxdigits-- > 0; ++p)
    {
      int d = -1;
      switch (*p)
	{
	case '0': d = 0x0; break;
	case '1': d = 0x1; break;
	case '2': d = 0x2; break;
	case '3': d = 0x3; break;
	case '4': d = 0x4; break;
	case '5': d = 0x5; break;
	case '6': d = 0x6; break;
	case '7': d = 0x7; break;
	case '8': d = 0x8; break;
	case '9': d = 0x9; break;
	case 'A': case 'a': d = 0xa; break;
	case 'B': case 'b': d = 0xb; break;
	case 'C': case 'c': d = 0xc; break;
	case 'D': case 'd': d = 0xd; break;
	case 'E': case 'e': d = 0xe; break;
	case 'F': case 'f': d = 0xf; break;
	}
      if (d < 0 || base <= d)
	break;
      n = n * base + d;
    }
  if (p == buf)
    *result = default_char;
  else
    *result = n;
  return p;
}


/* Read in a filename for a `r', `w', or `s///w' command. */
static struct buffer *read_filename P_((void));
static struct buffer *
read_filename()
{
  struct buffer *b;
  int ch;

  b = init_buffer();
  ch = in_nonblank();
  while (ch != EOF && ch != '\n')
    {
#if 0 /*XXX ZZZ 1998-09-12 kpp: added, then had second thoughts*/
      if (!POSIXLY_CORRECT)
	if (ch == ';' || ch == CLOSE_BRACE || ch == '#')
	  {
	    savchar(ch);
	    break;
	  }
#endif
      add1_buffer(b, ch);
      ch = inchar();
    }
  add1_buffer(b, '\0');
  return b;
}

static FILE *get_openfile P_((struct fp_list **file_ptrs, char *mode, flagT fail));
static FILE *
get_openfile(file_ptrs, mode, fail)
     struct fp_list **file_ptrs;
     char *mode;
     flagT fail;
{
  struct buffer *b;
  char *file_name;
  struct fp_list *p;
  int is_stderr;

  b = read_filename();
  file_name = get_buffer(b);
  for (p=*file_ptrs; p; p=p->link)
    if (strcmp(p->name, file_name) == 0)
      break;

  if (!p)
    {
      FILE *fp = NULL;
      if (!POSIXLY_CORRECT)
	{
	  /* Check whether it is a special file (stdin, stdout or stderr) */
	  struct special_files *special = special_files;
		  
	  /* std* sometimes are not constants, so they
	     cannot be used in the initializer for special_files */
	  my_stdin = stdin; my_stdout = stdout; my_stderr = stderr;
	  for (special = special_files; special->name; special++)
	    if (strcmp(special->name, file_name) == 0)
	      {
		fp = *special->pfp;
		break;
	      }
	}

      p = OB_MALLOC(&obs, 1, struct fp_list);
      p->name = ck_strdup(file_name);
      p->special = fp != NULL;
      if (!fp)
	fp = ck_fopen(p->name, mode, fail);
      p->fp = fp;
      p->link = *file_ptrs;
      *file_ptrs = p;
    }
  free_buffer(b);
  return p->fp;
}


static struct sed_cmd *next_cmd_entry P_((struct vector **vectorp));
static struct sed_cmd *
next_cmd_entry(vectorp)
  struct vector **vectorp;
{
  struct sed_cmd *cmd;
  struct vector *v;

  v = *vectorp;
  if (v->v_length == v->v_allocated)
    {
      v->v_allocated += VECTOR_ALLOC_INCREMENT;
      v->v = REALLOC(v->v, v->v_allocated, struct sed_cmd);
    }

  cmd = v->v + v->v_length;
  cmd->a1 = NULL;
  cmd->a2 = NULL;
  cmd->a1_matched = FALSE;
  cmd->addr_bang = FALSE;
  cmd->cmd = '\0';	/* something invalid, to catch bugs early */

  *vectorp  = v;
  return cmd;
}

static int snarf_char_class P_((struct buffer *b));
static int
snarf_char_class(b)
  struct buffer *b;
{
  int ch;

  ch = inchar();
  if (ch == '^')
    ch = add_then_next(b, ch);
  if (ch == CLOSE_BRACKET)
    ch = add_then_next(b, ch);
  while (ch != EOF && ch != '\n' && ch != CLOSE_BRACKET)
    {
      if (ch == OPEN_BRACKET)
	{
	  int prev;
	  int delim = ch = add_then_next(b, ch);

	  if (delim != '.'  &&  delim != ':'  &&  delim != '=')
	    continue; /* bypass the add_then_next() call at bottom of loop */
	  for (prev=ch=add_then_next(b, ch);
		  !(ch==CLOSE_BRACKET && prev==delim); ch=add_then_next(b, ch))
	    {
	      if (ch == EOF || ch == '\n')
		return ch;
	      prev = ch;
	    }
	}
#ifndef REG_PERL
      else if (ch == '\\')
	{
	  ch = inchar();
	  if (ch == EOF)
	    break;
	  if (ch != 'n' && ch != '\n')
	    {
	      add1_buffer(b, '\\');
	      continue; /* bypass the add_then_next() call at bottom of loop */
	    }
	  ch = '\n';
	}
#endif
      ch = add_then_next(b, ch);
    }

  return ch;
}

static struct buffer *match_slash P_((int slash, flagT regex, flagT keep_back));
static struct buffer *
match_slash(slash, regex, keep_backwhack)
  int slash;
  flagT regex;
  flagT keep_backwhack;
{
  struct buffer *b;
  int ch;

  b = init_buffer();
  while ((ch = inchar()) != EOF && ch != '\n' && ch != slash)
    {
      if (ch == '\\')
	{
	  ch = inchar();
	  if (ch == EOF)
	    break;
#ifndef REG_PERL
	  else if (ch == 'n' && regex)
	    ch = '\n';
#endif
	  else if (ch != '\n' && (ch != slash || keep_backwhack))
	    add1_buffer(b, '\\');
	}
      else if (ch == OPEN_BRACKET && regex)
	{
	  add1_buffer(b, ch);
	  ch = snarf_char_class(b);
	  if (ch != CLOSE_BRACKET)
	    break;
	}
      add1_buffer(b, ch);
    }
  if (ch == slash)
    return b;

  if (ch == '\n')
    savchar(ch);	/* for proper line number in error report */
  free_buffer(b);
  return NULL;
}

static flagT mark_subst_opts P_((struct subst *cmd));
static flagT
mark_subst_opts(cmd)
  struct subst *cmd;
{
  int flags = 0;
  int ch;

  cmd->global = FALSE;
  cmd->print = FALSE;
  cmd->eval = FALSE;
  cmd->numb = FALSE;
  cmd->fp = NULL;

  for (;;)
    switch ( (ch = in_nonblank()) )
      {
      case 'i':	/* GNU extension */
      case 'I':	/* GNU extension */
	flags |= REG_ICASE;
	break;

#ifdef REG_PERL
      case 's':	/* GNU extension */
      case 'S':	/* GNU extension */
	if (extended_regexp_flags & REG_PERL)
	  flags |= REG_DOTALL;
	break;

      case 'x':	/* GNU extension */
      case 'X':	/* GNU extension */
	if (extended_regexp_flags & REG_PERL)
	  flags |= REG_EXTENDED;
	break;
#endif

      case 'm':	/* GNU extension */
      case 'M':	/* GNU extension */
	flags |= REG_NEWLINE;
	break;

      case 'e':
	cmd->eval = TRUE;
	break;

      case 'p':
	if (cmd->print)
	  bad_prog(_(EXCESS_P_OPT));
	cmd->print |= (TRUE << cmd->eval); /* 1=before eval, 2=after */
	break;

      case 'g':
	if (cmd->global)
	  bad_prog(_(EXCESS_G_OPT));
	cmd->global = TRUE;
	break;

      case 'w':
	cmd->fp = get_openfile(&file_write, "w", TRUE);
	return flags;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	if (cmd->numb)
	  bad_prog(_(EXCESS_N_OPT));
	cmd->numb = in_integer(ch);
	if (!cmd->numb)
	  bad_prog(_(ZERO_N_OPT));
	break;

      case CLOSE_BRACE:
      case '#':
	savchar(ch);
	/* Fall Through */
      case EOF:
      case '\n':
      case ';':
	return flags;

      case '\r':
	if (inchar() == '\n')
	  return flags;
	/* FALLTHROUGH */

      default:
	bad_prog(_(UNKNOWN_S_OPT));
	/*NOTREACHED*/
      }
}


/* read in a label for a `:', `b', or `t' command */
static char *read_label P_((void));
static char *
read_label()
{
  struct buffer *b;
  int ch;
  char *ret;

  b = init_buffer();
  ch = in_nonblank();

  while (ch != EOF && ch != '\n'
	 && !ISBLANK(ch) && ch != ';' && ch != CLOSE_BRACE && ch != '#')
    {
      add1_buffer(b, ch);
      ch = inchar();
    }
  savchar(ch);
  add1_buffer(b, '\0');
  ret = ck_strdup(get_buffer(b));
  free_buffer(b);
  return ret;
}

/* Store a label (or label reference) created by a `:', `b', or `t'
   command so that the jump to/from the label can be backpatched after
   compilation is complete, or a reference created by a `{' to be
   backpatched when the corresponding `}' is found.  */
static struct sed_label *setup_label
  P_((struct sed_label *, countT, char *, const struct error_info *));
static struct sed_label *
setup_label(list, idx, name, err_info)
  struct sed_label *list;
  countT idx;
  char *name;
  const struct error_info *err_info;
{
  struct sed_label *ret = OB_MALLOC(&obs, 1, struct sed_label);
  ret->v_index = idx;
  ret->name = name;
  if (err_info)
    MEMCPY(&ret->err_info, err_info, sizeof ret->err_info);
  ret->next = list;
  return ret;
}

static struct sed_label *release_label P_((struct sed_label *list_head));
static struct sed_label *
release_label(list_head)
  struct sed_label *list_head;
{
  struct sed_label *ret;

  if (!list_head)
    return NULL;
  ret = list_head->next;

  FREE(list_head->name);

#if 0
  /* We use obstacks */
  FREE(list_head);
#endif
  return ret;
}

static struct replacement *new_replacement P_((char *, size_t,
					       enum replacement_types));
static struct replacement *
new_replacement(text, length, type)
  char *text;
  size_t length;
  enum replacement_types type;
{
  struct replacement *r = OB_MALLOC(&obs, 1, struct replacement);

  r->prefix = text;
  r->prefix_length = length;
  r->subst_id = -1;
  r->repl_type = type;

  /* r-> next = NULL; */
  return r;
}

static void setup_replacement P_((struct subst *, const char *, size_t));
static void
setup_replacement(sub, text, length)
     struct subst *sub;
     const char *text;
     size_t length;
{
  char *base;
  char *p;
  char *text_end;
  enum replacement_types repl_type = repl_asis, save_type = repl_asis;
  struct replacement root;
  struct replacement *tail;

  sub->max_id = 0;
  base = MEMDUP(text, length, char);
  length = normalize_text(base, length);

  text_end = base + length;
  tail = &root;

  for (p=base; p<text_end; ++p)
    {
      if (*p == '\\')
	{
	  /* Preceding the backslash may be some literal text: */
	  tail = tail->next =
	    new_replacement(base, CAST(size_t)(p - base), repl_type);

	  repl_type = save_type;

	  /* Skip the backslash and look for a numeric back-reference: */
	  ++p;
	  if (p<text_end)
	    switch (*p)
	      {
	      case '0': case '1': case '2': case '3': case '4': 
	      case '5': case '6': case '7': case '8': case '9': 
		tail->subst_id = *p - '0';
		if (sub->max_id < tail->subst_id)
		  sub->max_id = tail->subst_id;
		break;

	      case 'L':
		repl_type = repl_lowercase;
		save_type = repl_lowercase;
		break;

	      case 'U':
		repl_type = repl_uppercase;
		save_type = repl_uppercase;
		break;
		
	      case 'E':
		repl_type = repl_asis;
		save_type = repl_asis;
		break;

	      case 'l':
		save_type = repl_type;
		repl_type |= repl_lowercase_first;
		break;

	      case 'u':
		save_type = repl_type;
		repl_type |= repl_uppercase_first;
		break;
		
	      default:
		p[-1] = *p;
		++tail->prefix_length;
	      }

	  base = p + 1;
	}
      else if (*p == '&')
	{
	  /* Preceding the ampersand may be some literal text: */
	  tail = tail->next =
	    new_replacement(base, CAST(size_t)(p - base), repl_type);

	  repl_type = save_type;
	  tail->subst_id = 0;
	  base = p + 1;
	}
  }
  /* There may be some trailing literal text: */
  if (base < text_end)
    tail = tail->next =
      new_replacement(base, CAST(size_t)(text_end - base), repl_type);

  tail->next = NULL;
  sub->replacement = root.next;
}

static void read_text P_((struct text_buf *buf, int leadin_ch));
static void
read_text(buf, leadin_ch)
  struct text_buf *buf;
  int leadin_ch;
{
  int ch;

  /* Should we start afresh (as opposed to continue a partial text)? */
  if (buf)
    {
      if (pending_text)
	free_buffer(pending_text);
      pending_text = init_buffer();
      buf->text = NULL;
      buf->text_length = 0;
      old_text_buf = buf;
    }
  /* assert(old_text_buf != NULL); */

  if (leadin_ch == EOF)
    return;
#ifndef NO_INPUT_INDENT
  if (leadin_ch != '\n')
    add1_buffer(pending_text, leadin_ch);
  ch = inchar();
#else /*NO_INPUT_INDENT*/
  if (leadin_ch == '\n')
    ch = in_nonblank();
  else
    {
      add1_buffer(pending_text, leadin_ch);
      ch = inchar();
    }
#endif /*NO_INPUT_INDENT*/

  while (ch!=EOF && ch!='\n')
    {
      if (ch == '\\')
	ch = inchar();
      if (ch == EOF)
	{
	  add1_buffer(pending_text, '\n');
	  return;
	}
      add1_buffer(pending_text, ch);
#ifdef NO_INPUT_INDENT
      if (ch == '\n')
	ch = in_nonblank();
      else
#endif /*NO_INPUT_INDENT*/
	ch = inchar();
    }
  add1_buffer(pending_text, '\n');

  if (!buf)
    buf = old_text_buf;
  buf->text_length = size_buffer(pending_text);
  buf->text = MEMDUP(get_buffer(pending_text), buf->text_length, char);
  free_buffer(pending_text);
  pending_text = NULL;
}


/* Try to read an address for a sed command.  If it succeeds,
   return non-zero and store the resulting address in `*addr'.
   If the input doesn't look like an address read nothing
   and return zero.  */
static flagT compile_address P_((struct addr *addr, int ch));
static flagT
compile_address(addr, ch)
  struct addr *addr;
  int ch;
{
  addr->addr_type = addr_is_null;
  addr->addr_step = 0;
  addr->addr_number = ~(countT)0;  /* extremely unlikely to ever match */
  addr->addr_regex = NULL;

  if (ch == '/' || ch == '\\')
    {
      int flags = 0;
      struct buffer *b;
      addr->addr_type = addr_is_regex;
      if (ch == '\\')
	ch = inchar();
      if ( !(b = match_slash(ch, TRUE, TRUE)) )
	bad_prog(_(UNTERM_ADDR_RE));

      for(;;)
	{
	  ch = in_nonblank();
          switch(ch)
	    {
	    case 'I':	/* GNU extension */
	      flags |= REG_ICASE;
	      break;

#ifdef REG_PERL
	    case 'S':	/* GNU extension */
	      if (extended_regexp_flags & REG_PERL)
		flags |= REG_DOTALL;
	      break;

	    case 'X':	/* GNU extension */
	      if (extended_regexp_flags & REG_PERL)
		flags |= REG_EXTENDED;
	      break;
#endif

	    case 'M':	/* GNU extension */
	      flags |= REG_NEWLINE;
	      break;

	    default:
	      savchar (ch);
	      addr->addr_regex = compile_regex (b, flags, 0);
	      free_buffer(b);
	      return TRUE;
	    }
	}
    }
  else if (ISDIGIT(ch))
    {
      addr->addr_number = in_integer(ch);
      addr->addr_type = addr_is_num;
      ch = in_nonblank();
      if (ch != '~')
	{
	  savchar(ch);
	}
      else
	{
	  countT step = in_integer(in_nonblank());
	  if (step > 0)
	    {
	      addr->addr_step = step;
	      addr->addr_type = addr_is_num_mod;
	    }
	}
    }
  else if (ch == '+' || ch == '~')
    {
      addr->addr_step = in_integer(in_nonblank());
      if (addr->addr_step==0)
	; /* default to addr_is_null; forces matching to stop on next line */
      else if (ch == '+')
	addr->addr_type = addr_is_step;
      else
	addr->addr_type = addr_is_step_mod;
    }
  else if (ch == '$')
    {
      addr->addr_type = addr_is_last;
    }
  else
    return FALSE;

  return TRUE;
}

/* Read a program (or a subprogram within `{' `}' pairs) in and store
   the compiled form in `*vector'.  Return a pointer to the new vector.  */
static struct vector *compile_program P_((struct vector *));
static struct vector *
compile_program(vector)
  struct vector *vector;
{
  struct sed_cmd *cur_cmd;
  struct buffer *b;
  int ch;

  if (!vector)
    {
      vector = MALLOC(1, struct vector);
      vector->v = NULL;
      vector->v_allocated = 0;
      vector->v_length = 0;

      obstack_init (&obs);
    }
  if (pending_text)
    read_text(NULL, '\n');

  for (;;)
    {
      struct addr a;

      while ((ch=inchar()) == ';' || ISSPACE(ch))
	;
      if (ch == EOF)
	break;

      cur_cmd = next_cmd_entry(&vector);
      if (compile_address(&a, ch))
	{
	  if (a.addr_type == addr_is_step
	      || a.addr_type == addr_is_step_mod)
	    bad_prog(_(BAD_PLUS));

	  cur_cmd->a1 = MEMDUP(&a, 1, struct addr);
	  ch = in_nonblank();
	  if (ch == ',')
	    {
	      if (!compile_address(&a, in_nonblank()))
		bad_prog(_(BAD_COMMA));
	      if (a.addr_type == addr_is_num)
		a.addr_type = addr_is_num2;
	      cur_cmd->a2 = MEMDUP(&a, 1, struct addr);
	      ch = in_nonblank();
	      if ((cur_cmd->a1->addr_type == addr_is_num
		   || cur_cmd->a1->addr_type == addr_is_num_mod)
	      	  && cur_cmd->a1->addr_number == 0)
		cur_cmd->a1_matched = TRUE;
	    }
	}
      if (ch == '!')
	{
	  cur_cmd->addr_bang = TRUE;
	  ch = in_nonblank();
	  if (ch == '!')
	    bad_prog(_(BAD_BANG));
	}

      cur_cmd->cmd = ch;
      switch (ch)
	{
	case '#':
	  if (cur_cmd->a1)
	    bad_prog(_(NO_SHARP_ADDR));
	  ch = inchar();
	  if (ch=='n' && first_script && cur_input.line < 2)
	    if (   (prog.base && prog.cur==2+prog.base)
		|| (prog.file && !prog.base && 2==ftell(prog.file)))
	      no_default_output = TRUE;
	  while (ch != EOF && ch != '\n')
	    ch = inchar();
	  continue;	/* restart the for (;;) loop */

	case 'v':
          /* This is an extension.  Programs needing GNU sed might start
           * with a `v' command so that other seds will stop.
           * We compare the version ignore it.
           */
          {
            char *version = read_label ();
            char *compared_version;
            compared_version = (*version == '\0') ? "4.0" : version;
            
	    /* Removed by Kyle R. Murphy - July 2006 */	    
	    /* 
	    if (strverscmp (compared_version, SED_FEATURE_VERSION) > 0)
              bad_prog(_(ANCIENT_VERSION));
	    */

            free (version);
	    POSIXLY_CORRECT = 0;
          }
	  continue;

	case '{':
	  blocks = setup_label(blocks, vector->v_length, NULL, &cur_input);
	  cur_cmd->addr_bang = !cur_cmd->addr_bang;
	  break;

	case '}':
	  if (!blocks)
	    bad_prog(_(EXCESS_CLOSE_BRACE));
	  if (cur_cmd->a1)
	    bad_prog(_(NO_CLOSE_BRACE_ADDR));
	  ch = in_nonblank();
	  if (ch == CLOSE_BRACE || ch == '#')
	    savchar(ch);
	  else if (ch != EOF && ch != '\n' && ch != ';')
	    bad_prog(_(EXCESS_JUNK));

	  vector->v[blocks->v_index].x.jump_index = vector->v_length;
	  blocks = release_label(blocks);	/* done with this entry */
	  break;

	case 'e':
	  ch = in_nonblank();
	  if (ch == EOF || ch == '\n')
	    {
	      cur_cmd->x.cmd_txt.text_length = 0;
	      break;
	    }
	  else
	    goto read_text_to_slash;

	case 'a':
	case 'i':
	  if (POSIXLY_CORRECT && cur_cmd->a2)
	    bad_prog(_(ONE_ADDR));
	  /* Fall Through */

	case 'c':
	  ch = in_nonblank();

	read_text_to_slash:
	  if (ch == EOF)
	    bad_prog(_(EXPECTED_SLASH));
	      
	  if (ch == '\\')
	    ch = inchar();
	  else
	    {
	      savchar(ch);
	      ch = '\n';
	    }

	  read_text(&cur_cmd->x.cmd_txt, ch);
	  break;

	case ':':
	  if (cur_cmd->a1)
	    bad_prog(_(NO_COLON_ADDR));
	  labels = setup_label(labels, vector->v_length, read_label(), NULL);
	  break;
	
	case 'b':
	case 't':
	case 'T':
	  jumps = setup_label(jumps, vector->v_length, read_label(), NULL);
	  break;

	case 'q':
	case 'Q':
	  if (cur_cmd->a2)
	    bad_prog(_(ONE_ADDR));
	  /* Fall through */

	case 'l':
	case 'L':
	  if (POSIXLY_CORRECT && cur_cmd->a2)
	    bad_prog(_(ONE_ADDR));

	  ch = in_nonblank();
	  if (ISDIGIT(ch)) 
	    {
	      cur_cmd->x.int_arg = in_integer(ch);
	      ch = in_nonblank();
	    }
	  else
	    cur_cmd->x.int_arg = -1;

	  if (ch == CLOSE_BRACE || ch == '#')
	    savchar(ch);
	  else if (ch != EOF && ch != '\n' && ch != ';')
	    bad_prog(_(EXCESS_JUNK));

	  break;

	case '=':
	  if (POSIXLY_CORRECT && cur_cmd->a2)
	    bad_prog(_(ONE_ADDR));
	  /* Fall Through */
	case 'd':
	case 'D':
	case 'g':
	case 'G':
	case 'h':
	case 'H':
	case 'n':
	case 'N':
	case 'p':
	case 'P':
	case 'x':
	  ch = in_nonblank();
	  if (ch == CLOSE_BRACE || ch == '#')
	    savchar(ch);
	  else if (ch != EOF && ch != '\n' && ch != ';')
	    bad_prog(_(EXCESS_JUNK));
	  break;

	case 'r':
	  if (POSIXLY_CORRECT && cur_cmd->a2)
	    bad_prog(_(ONE_ADDR));
	  b = read_filename();
	  cur_cmd->x.fname = ck_strdup(get_buffer(b));
	  free_buffer(b);
	  break;

        case 'R':
	  cur_cmd->x.fp = get_openfile(&file_read, "r", FALSE);
	  break;

	case 'w':
        case 'W':
	  cur_cmd->x.fp = get_openfile(&file_write, "w", TRUE);
	  break;

	case 's':
	  {
	    struct buffer *b2;
	    int flags;
	    int slash;

	    slash = inchar();
	    if ( !(b  = match_slash(slash, TRUE, TRUE)) )
	      bad_prog(_(UNTERM_S_CMD));
	    if ( !(b2 = match_slash(slash, FALSE, TRUE)) )
	      bad_prog(_(UNTERM_S_CMD));

	    cur_cmd->x.cmd_subst = OB_MALLOC(&obs, 1, struct subst);
	    setup_replacement(cur_cmd->x.cmd_subst,
			      get_buffer(b2), size_buffer(b2));
	    free_buffer(b2);

	    flags = mark_subst_opts(cur_cmd->x.cmd_subst);
	    cur_cmd->x.cmd_subst->regx =
	      compile_regex(b, flags, cur_cmd->x.cmd_subst->max_id);
	    free_buffer(b);
	  }
	  break;

	case 'y':
	  {
	    unsigned char *ustring;
	    size_t len;
	    int slash;

#if defined MBS_SUPPORT && !defined (REG_PERL)
	    if (MB_CUR_MAX == 1)
#endif
	      {
	        ustring = OB_MALLOC(&obs, YMAP_LENGTH, unsigned char);
	        for (len = 0; len < YMAP_LENGTH; len++)
	          ustring[len] = len;
	        cur_cmd->x.translate = ustring;
	      }

	    slash = inchar();
	    if ( !(b = match_slash(slash, FALSE, FALSE)) )
	      bad_prog(_(UNTERM_Y_CMD));

#if defined MBS_SUPPORT && !defined (REG_PERL)
            if (MB_CUR_MAX > 1)
	      {
                int i, j, idx, src_char_num, len = size_buffer(b);
                size_t *src_lens = MALLOC(len, size_t);
                char *src_buf, *dest_buf, **trans_pairs;
                size_t mbclen;
                mbstate_t cur_stat;
	        struct buffer *b2;

                /* Enumerate how many character the source buffer has.  */
                memset(&cur_stat, 0, sizeof(mbstate_t));
                src_buf = get_buffer(b);
                for (i = 0, j = 0; i < len;)
                  {
                    mbclen = mbrlen(src_buf + i, len - i, &cur_stat);
                    /* An invalid sequence, or a truncated multibyte character.
                       We treat it as a singlebyte character.  */
                    if (mbclen == (size_t) -1 || mbclen == (size_t) -2
                        || mbclen == 0)
                      mbclen = 1;
                    src_lens[j++] = mbclen;
                    i += mbclen;
                  }
                src_char_num = j;

                memset(&cur_stat, 0, sizeof(mbstate_t));
                if ( !(b2 = match_slash(slash, FALSE, FALSE)) )
 		  bad_prog(_(UNTERM_Y_CMD));
                dest_buf = get_buffer(b2);
                idx = 0;
                len = size_buffer(b2);

                /* trans_pairs = {src(0), dest(0), src(1), dest(1), ..., NULL}
                     src(i) : pointer to i-th source character.
                     dest(i) : pointer to i-th destination character.
                     NULL : terminator */
                trans_pairs = MALLOC(2 * src_char_num + 1, char*);
                cur_cmd->x.translatemb = trans_pairs;
                for (i = 0; i < src_char_num; i++)
                  {
                    if (idx >= len)
                      bad_prog(_(Y_CMD_LEN));

                    /* Set the i-th source character.  */
                    trans_pairs[2 * i] = MALLOC(src_lens[i] + 1, char);
                    strncpy(trans_pairs[2 * i], src_buf, src_lens[i]);
                    trans_pairs[2 * i][src_lens[i]] = '\0';
                    src_buf += src_lens[i]; /* Forward to next character.  */

                    /* Fetch the i-th destination character.  */
                    mbclen = mbrlen(dest_buf + idx, len - idx, &cur_stat);
                    /* An invalid sequence, or a truncated multibyte character.
                       We treat it as a singlebyte character.  */
                    if (mbclen == (size_t) -1 || mbclen == (size_t) -2
                        || mbclen == 0)
                      mbclen = 1;

                    /* Set the i-th destination character.  */
                    trans_pairs[2 * i + 1] = MALLOC(mbclen + 1, char);
                    strncpy(trans_pairs[2 * i + 1], dest_buf + idx, mbclen);
                    trans_pairs[2 * i + 1][mbclen] = '\0';
                    idx += mbclen; /* Forward to next character.  */
                  }
                trans_pairs[2 * i] = NULL;
                if (idx != len)
                  bad_prog(_(Y_CMD_LEN));
                free_buffer(b);
                free_buffer(b2);
              }
            else
#endif
              {
                ustring = CAST(unsigned char *)get_buffer(b);
                for (len = size_buffer(b); len; --len)
                  {
                    ch = inchar();
                    if (ch == slash)
                      bad_prog(_(Y_CMD_LEN));
                    if (ch == '\n')
                      bad_prog(UNTERM_Y_CMD);
                    if (ch == '\\')
                      ch = inchar();
                    if (ch == EOF)
                      bad_prog(UNTERM_Y_CMD);
                    cur_cmd->x.translate[*ustring++] = ch;
                  }
                free_buffer(b);

                if (inchar() != slash)
		  bad_prog(_(Y_CMD_LEN));
                else if ((ch = in_nonblank()) != EOF && ch != '\n' && ch != ';')
                  bad_prog(_(EXCESS_JUNK));
	      }
	  }
	break;

	case EOF:
	  bad_prog(_(NO_COMMAND));
	  /*NOTREACHED*/
	default:
	  {
	    const char *msg = _("Unknown command:");
	    char *unknown_cmd = xmalloc(strlen(msg) + 5);
	    sprintf(unknown_cmd, "%s `%c'", msg, ch);
	    bad_prog(unknown_cmd);
	    /*NOTREACHED*/
	  }
	}

      /* this is buried down here so that "continue" statements will miss it */
      ++vector->v_length;
    }
  return vector;
}


/* Complain about a programming error and exit. */
void
bad_prog(why)
  const char *why;
{
  if (cur_input.name)
    fprintf(stderr, _("%s: file %s line %lu: %s\n"),
	    myname, cur_input.name, CAST(unsigned long)cur_input.line, why);
  else
    fprintf(stderr, _("%s: -e expression #%lu, char %lu: %s\n"),
	    myname,
	    CAST(unsigned long)cur_input.string_expr_count,
	    CAST(unsigned long)(prog.cur-prog.base),
	    why);
  exit(EXIT_FAILURE);
}


/* deal with \X escapes */
size_t
normalize_text(buf, len)
  char *buf;
  size_t len;
{
  const char *bufend = buf + len;
  char *p = buf;
  char *q = buf;

  /* I'm not certain whether POSIX.2 allows these escapes.
     Play it safe for now... */
  if (POSIXLY_CORRECT && !(extended_regexp_flags))
    return len;

  while (p < bufend)
    {
      int c;

      *q = *p++;
      if (*q == '\\' && p < bufend)
	switch ( (c = *p++) )
	  {
#if defined __STDC__ && __STDC__-0
	  case 'a': *q = '\a'; break;
#else /* Not STDC; we'll just assume ASCII */
	  case 'a': *q = '\007'; break;
#endif
	  /* case 'b': *q = '\b'; break; --- conflicts with \b RE */
	  case 'f': *q = '\f'; break;
	  case '\n': /*fall through */
	  case 'n': *q = '\n'; break;
	  case 'r': *q = '\r'; break;
	  case 't': *q = '\t'; break;
	  case 'v': *q = '\v'; break;

	  case 'd': /* decimal byte */
	    p = convert_number(q, p, bufend, 10, 3, 'd');
	    break;

	  case 'x': /* hexadecimal byte */
	    p = convert_number(q, p, bufend, 16, 2, 'x');
	    break;

#ifdef REG_PERL
	  case '0': case '1': case '2': case '3':
	  case '4': case '5': case '6': case '7':
	    if ((extended_regexp_flags & REG_PERL) &&
		p < bufend && isdigit(*p))
	      {
		p--;
		p = convert_number(q, p, bufend, 8, 3, *p);
	      }
	    else
	      /* we just pass the \ up one level for interpretation */
	      *++q = p[-1];

	    break;

	  case 'o': /* octal byte */
	    if (!(extended_regexp_flags & REG_PERL))
	      p = convert_number(q, p, bufend,  8, 3, 'o');
	    else
	      /* we just pass the \ up one level for interpretation */
	      *++q = p[-1];
	    
	    break;

#else
	  case 'o': /* octal byte */
	    p = convert_number(q, p, bufend,  8, 3, 'o');
	    break;
#endif

	  case 'c':
	    if (p < bufend)
	      {
		*q = toupper(*p) ^ 0x40;
		p++;
		break;
	      }
	    /* else FALLTHROUGH */

	  default:
	    /* we just pass the \ up one level for interpretation */
	    *++q = p[-1];
	    break;
	  }
      ++q;
    }
    return (size_t)(q - buf);
}


/* `str' is a string (from the command line) that contains a sed command.
   Compile the command, and add it to the end of `cur_program'. */
struct vector *
compile_string(cur_program, str, len)
  struct vector *cur_program;
  char *str;
  size_t len;
{
  static countT string_expr_count = 0;
  struct vector *ret;

  prog.file = NULL;
  prog.base = CAST(unsigned char *)str;
  prog.cur = prog.base;
  prog.end = prog.cur + len;

  cur_input.line = 0;
  cur_input.name = NULL;
  cur_input.string_expr_count = ++string_expr_count;

  ret = compile_program(cur_program);
  prog.base = NULL;
  prog.cur = NULL;
  prog.end = NULL;

  first_script = FALSE;
  return ret;
}

/* `cmdfile' is the name of a file containing sed commands.
   Read them in and add them to the end of `cur_program'.
 */
struct vector *
compile_file(cur_program, cmdfile)
  struct vector *cur_program;
  const char *cmdfile;
{
  size_t len;
  struct vector *ret;

  prog.file = stdin;
  if (cmdfile[0] != '-' || cmdfile[1] != '\0')
    prog.file = ck_fopen(cmdfile, "rt", TRUE);

  cur_input.line = 1;
  cur_input.name = cmdfile;
  cur_input.string_expr_count = 0;

  ret = compile_program(cur_program);
  if (prog.file != stdin)
    ck_fclose(prog.file);
  prog.file = NULL;

  first_script = FALSE;
  return ret;
}

#undef stderr
#define stderr stdout
/* Make any checks which require the whole program to have been read.
   In particular: this backpatches the jump targets.
   Any cleanup which can be done after these checks is done here also.  */
void
check_final_program(program)
  struct vector *program;
{
  struct sed_label *go;
  struct sed_label *lbl;

  /* do all "{"s have a corresponding "}"? */
  if (blocks)
    {
      /* update info for error reporting: */
      MEMCPY(&cur_input, &blocks->err_info, sizeof cur_input);
      bad_prog(_(EXCESS_OPEN_BRACE));
    }

  /* was the final command an unterminated a/c/i command? */
  if (pending_text)
    {
      old_text_buf->text_length = size_buffer(pending_text);
      old_text_buf->text = MEMDUP(get_buffer(pending_text),
				  old_text_buf->text_length, char);
      free_buffer(pending_text);
      pending_text = NULL;
    }

  for (go = jumps; go; go = release_label(go))
    {
      for (lbl = labels; lbl; lbl = lbl->next)
	if (strcmp(lbl->name, go->name) == 0)
	  break;
      if (lbl)
	{
	  program->v[go->v_index].x.jump_index = lbl->v_index;
	}
      else
	{
	  if (*go->name)
	    panic(_("Can't find label for jump to `%s'"), go->name);
	  program->v[go->v_index].x.jump_index = program->v_length;
	}
    }
  jumps = NULL;

  for (lbl = labels; lbl; lbl = release_label(lbl))
    ;
  labels = NULL;

  /* There is no longer a need to track file names: */
  {
    struct fp_list *p;

    for (p=file_read; p; p=p->link)
      if (p->name)
	{
	  FREE(p->name);
	  p->name = NULL;
	}

    for (p=file_write; p; p=p->link)
      if (p->name)
	{
	  FREE(p->name);
	  p->name = NULL;
	}
  }
}

/* Release all resources which were allocated in this module. */
void
rewind_read_files()
{
  struct fp_list *p, *q;

  for (p=file_read; p; p=p->link)
    if (p->fp && !p->special)
      rewind(p->fp);
}

/* Release all resources which were allocated in this module. */
void
finish_program(program)
  struct vector *program;
{
  /* close all files... */
  {
    struct fp_list *p, *q;

    for (p=file_read; p; p=q)
      {
	if (p->fp && !p->special)
	  ck_fclose(p->fp);
	q = p->link;
#if 0
	/* We use obstacks. */
	FREE(p);
#endif
      }

    for (p=file_write; p; p=q)
      {
	if (p->fp && !p->special)
	  ck_fclose(p->fp);
	q = p->link;
#if 0
	/* We use obstacks. */
	FREE(p);
#endif
      }
    file_read = file_write = NULL;
  }

#ifdef DEBUG_LEAKS
  obstack_free (&obs, NULL);
#endif /*DEBUG_LEAKS*/
}

/*  GNU SED, a batch stream editor.
    Copyright (C) 1989,90,91,92,93,94,95,98,99,2002,2003
    Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#undef EXPERIMENTAL_DASH_N_OPTIMIZATION	/*don't use -- is very buggy*/
#define INITIAL_BUFFER_SIZE	50
#define FREAD_BUFFER_SIZE	8192

#include "config.h"

#include <stdio.h>
#include <ctype.h>

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __GNUC__
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__-0 >= 7)
   /* silence warning about unused parameter even for "gcc -W -Wunused" */
#  define UNUSED	__attribute__((unused))
# endif
#endif
#ifndef UNUSED
# define UNUSED
#endif

#ifdef HAVE_STRINGS_H
# include <strings.h>
#else
# include <string.h>
#endif /*HAVE_STRINGS_H*/
#ifdef HAVE_MEMORY_H
# include <memory.h>
#endif


#if defined HAVE_WCTYPE_H && defined HAVE_WCHAR_H && defined HAVE_MBRTOWC
/* We can handle multibyte string.  */
# include <wchar.h>
# include <wctype.h>
# define MBS_SUPPORT
#endif

#ifndef HAVE_STRCHR
# define strchr index
# define strrchr rindex
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <sys/stat.h>

#include "basicdefs.h"
#include "utils.h"


/* Sed operates a line at a time. */
struct line {
  char *text;		/* Pointer to line allocated by malloc. */
  char *active;		/* Pointer to non-consumed part of text. */
  size_t length;	/* Length of text (or active, if used). */
  size_t alloc;		/* Allocated space for active. */
  flagT chomped;	/* Was a trailing newline dropped? */
};

/* A queue of text to write out at the end of a cycle
   (filled by the "a", "r" and "R" commands.) */
struct append_queue {
  const char *fname;
  char *text;
  size_t textlen;
  struct append_queue *next;
  flagT free;
};

/* State information for the input stream. */
struct input {
  char **file_list;	/* The list of yet-to-be-opened files.
			   It is invalid for file_list to be NULL.
			   When *file_list is NULL we are
			   currently processing the last file. */
  countT bad_count;	/* count of files we failed to open */
  countT line_number;	/* current input line number (over all files) */

  flagT (*read_fn) P_((struct input *));	/* read one line */
  /* If fp is NULL, read_fn better not be one which uses fp;
     in particular, read_always_fail() is recommended. */

  char *out_file_name;
  const char *in_file_name;

  FILE *fp;		/* if NULL, none of the following are valid */
  flagT no_buffering;
};


/* Have we done any replacements lately?  This is used by the `t' command. */
static flagT replaced = FALSE;

/* The current output file (stdout if -i is not being used. */
static FILE *output_file;

/* The `current' input line. */
static struct line line;

/* An input line used to accumulate the result of the s and e commands. */
static struct line s_accum;

/* An input line that's been stored by later use by the program */
static struct line hold;

/* The buffered input look-ahead.  The only field that should be
   used outside of read_mem_line() or line_init() is buffer.length. */
static struct line buffer;

static struct append_queue *append_head = NULL;
static struct append_queue *append_tail = NULL;


#ifdef BOOTSTRAP
/* We can't be sure that the system we're boostrapping on has
   memchr(), and ../lib/memchr.c requires configuration knowledge
   about how many bits are in a `long'.  This implementation
   is far from ideal, but it should get us up-and-limping well
   enough to run the configure script, which is all that matters.
*/
# ifdef memchr
#  undef memchr
# endif
# define memchr bootstrap_memchr

static VOID *bootstrap_memchr P_((const VOID *s, int c, size_t n));
static VOID *
bootstrap_memchr(s, c, n)
  const VOID *s;
  int c;
  size_t n;
{
  char *p;

  for (p=(char *)s; n-- > 0; ++p)
    if (*p == c)
      return p;
  return CAST(VOID *)0;
}
#endif /*BOOTSTRAP*/

/* increase a struct line's length, making some attempt at
   keeping realloc() calls under control by padding for future growth.  */
static void resize_line P_((struct line *, size_t));
static void
resize_line(lb, len)
  struct line *lb;
  size_t len;
{
  int inactive;
  inactive = lb->active - lb->text;

  /* If the inactive part has got to more than two thirds of the buffer,
   * remove it. */
  if (inactive > lb->alloc * 2)
    {
      MEMMOVE(lb->text, lb->active, lb->length);
      lb->alloc += lb->active - lb->text;
      lb->active = lb->text;
      inactive = 0;

      if (lb->alloc > len)
	return;
    }

  lb->alloc *= 2;
  if (lb->alloc < len)
    lb->alloc = len;
  if (lb->alloc < INITIAL_BUFFER_SIZE)
    lb->alloc = INITIAL_BUFFER_SIZE;
    
  lb->text = REALLOC(lb->text, inactive + lb->alloc, char);
  lb->active = lb->text + inactive;
}

/* Append `length' bytes from `string' to the line `to'. */
static void str_append P_((struct line *, const char *, size_t));
static void
str_append(to, string, length)
  struct line *to;
  const char *string;
  size_t length;
{
  size_t new_length = to->length + length;

  if (to->alloc < new_length)
    resize_line(to, new_length);
  MEMCPY(to->active + to->length, string, length);
  to->length = new_length;
}

static void str_append_modified P_((struct line *, const char *, size_t,
				    enum replacement_types));
static void
str_append_modified(to, string, length, type)
  struct line *to;
  const char *string;
  size_t length;
  enum replacement_types type;
{
  size_t old_length = to->length;
  char *start, *end;

  if (length == 0)
    return;

  str_append(to, string, length);
  start = to->active + old_length;
  end = start + length;

  /* Now do the required modifications.  First \[lu]... */
  if (type & repl_uppercase_first)
    {
      *start = toupper(*start);
      start++;
      type &= ~repl_uppercase_first;
    }
  else if (type & repl_lowercase_first)
    {
      *start = tolower(*start);
      start++;
      type &= ~repl_lowercase_first;
    }

  if (type == repl_asis)
    return;

  /* ...and then \[LU] */
  if (type == repl_uppercase)
    for (; start != end; start++)
      *start = toupper(*start);
  else
    for (; start != end; start++)
      *start = tolower(*start);
}

/* initialize a "struct line" buffer */
static void line_init P_((struct line *, size_t initial_size));
static void
line_init(buf, initial_size)
  struct line *buf;
  size_t initial_size;
{
  buf->text = MALLOC(initial_size, char);
  buf->active = buf->text;
  buf->alloc = initial_size;
  buf->length = 0;
  buf->chomped = TRUE;
}

/* Copy the contents of the line `from' into the line `to'.
   This destroys the old contents of `to'. */
static void line_copy P_((struct line *from, struct line *to));
static void
line_copy(from, to)
  struct line *from;
  struct line *to;
{
  /* Remove the inactive portion in the destination buffer. */
  to->alloc += to->active - to->text;

  if (to->alloc < from->length)
    {
      to->alloc *= 2;
      if (to->alloc < from->length)
	to->alloc = from->length;
      if (to->alloc < INITIAL_BUFFER_SIZE)
	to->alloc = INITIAL_BUFFER_SIZE;
      /* Use FREE()+MALLOC() instead of REALLOC() to
	 avoid unnecessary copying of old text. */
      FREE(to->text);
      to->text = MALLOC(to->alloc, char);
    }

  to->active = to->text;
  to->length = from->length;
  to->chomped = from->chomped;
  MEMCPY(to->active, from->active, from->length);
}

/* Append the contents of the line `from' to the line `to'. */
static void line_append P_((struct line *from, struct line *to));
static void
line_append(from, to)
  struct line *from;
  struct line *to;
{
  str_append(to, "\n", 1);
  str_append(to, from->active, from->length);
  to->chomped = from->chomped;
}

/* Exchange the contents of two "struct line" buffers. */
static void line_exchange P_((struct line *, struct line *));
static void
line_exchange(a, b)
  struct line *a;
  struct line *b;
{
  struct line t;

  MEMCPY(&t,  a, sizeof(struct line));
  MEMCPY( a,  b, sizeof(struct line));
  MEMCPY( b, &t, sizeof(struct line));
}


/* dummy function to simplify read_pattern_space() */
static flagT read_always_fail P_((struct input *));
static flagT
read_always_fail(input)
  struct input *input UNUSED;
{
  return FALSE;
}

#include "getline.c"
static flagT read_file_line P_((struct input *));
static flagT
read_file_line(input)
  struct input *input;
{
  static char *b;
  static size_t blen;

  int result = getline (&b, &blen, input->fp);

  /* Remove the trailing new-line that is left by getline. */
  if (result > 0 && b[result - 1] == '\n')
    --result;
  else
    {
      /* No trailing new line found. */
      if (!*input->file_list && !POSIXLY_CORRECT)
	line.chomped = FALSE;

      if (result <= 0)
	return FALSE;
    }

  str_append(&line, b, result);
  return TRUE;
}


static void output_line P_((const char *, size_t, flagT, FILE *));
static void
output_line(text, length, nl, fp)
  const char *text;
  size_t length;
  flagT nl;
  FILE *fp;
{
  if (length)
    ck_fwrite(text, 1, length, fp);
  if (nl)
    ck_fwrite("\n", 1, 1, fp);
  if (fp != stdout || unbuffered_output)
    ck_fflush(fp);
}

static struct append_queue *next_append_slot P_((void));
static struct append_queue *
next_append_slot()
{
  struct append_queue *n = MALLOC(1, struct append_queue);

  n->fname = NULL;
  n->text = NULL;
  n->textlen = 0;
  n->next = NULL;
  n->free = FALSE;

  if (append_tail)
      append_tail->next = n;
  else
      append_head = n;
  return append_tail = n;
}

static void release_append_queue P_((void));
static void
release_append_queue()
{
  struct append_queue *p, *q;

  for (p=append_head; p; p=q)
    {
      if (p->free)
        FREE(p->text);

      q = p->next;
      FREE(p);
    }
  append_head = append_tail = NULL;
}

static void dump_append_queue P_((void));
static void
dump_append_queue()
{
  struct append_queue *p;

  for (p=append_head; p; p=p->next)
    {
      if (p->text)
	  output_line(p->text, p->textlen, FALSE, output_file);
      if (p->fname)
	{
	  char buf[FREAD_BUFFER_SIZE];
	  size_t cnt;
	  FILE *fp;

	  /* "If _fname_ does not exist or cannot be read, it shall
	     be treated as if it were an empty file, causing no error
	     condition."  IEEE Std 1003.2-1992
	     So, don't fail. */
	  fp = ck_fopen(p->fname, "r", FALSE);
	  if (fp)
	    {
	      while ((cnt = ck_fread(buf, 1, sizeof buf, fp)) > 0)
		ck_fwrite(buf, 1, cnt, output_file);
	      ck_fclose(fp);
	    }
	}
    }
  release_append_queue();
}


/* Compute the name of the backup file for in-place editing */
static char *get_backup_file_name P_((const char *));
static char *
get_backup_file_name(name)
  const char *name;
{
  char *old_asterisk, *asterisk, *backup, *p;
  int name_length = strlen(name), backup_length = strlen(in_place_extension);

  /* Compute the length of the backup file */
  for (asterisk = in_place_extension - 1, old_asterisk = asterisk + 1;
       asterisk = strchr(old_asterisk, '*');
       old_asterisk = asterisk + 1)
    backup_length += name_length - 1;

  p = backup = xmalloc(backup_length + 1);

  /* Each iteration gobbles up to an asterisk */
  for (asterisk = in_place_extension - 1, old_asterisk = asterisk + 1;
       asterisk = strchr(old_asterisk, '*');
       old_asterisk = asterisk + 1)
    {
      memcpy (p, old_asterisk, asterisk - old_asterisk);
      p += asterisk - old_asterisk;
      strcpy (p, name);
      p += name_length;
    }

  /* Tack on what's after the last asterisk */
  strcpy (p, old_asterisk);
  return backup;
}

/* Initialize a struct input for the named file. */
static void open_next_file P_((const char *name, struct input *));
static void
open_next_file(name, input)
  const char *name;
  struct input *input;
{
  buffer.length = 0;

  if (name[0] == '-' && name[1] == '\0')
    {
      clearerr(stdin);	/* clear any stale EOF indication */
      input->fp = stdin;
    }
  else if ( ! (input->fp = ck_fopen(name, "r", FALSE)) )
    {
      const char *ptr = strerror(errno);
      fprintf(stderr, _("%s: can't read %s: %s\n"), myname, name, ptr);
      input->read_fn = read_always_fail; /* a redundancy */
      ++input->bad_count;
      return;
    }

  input->read_fn = read_file_line;

  if (in_place_extension)
    {
      int output_fd;
      char *tmpdir = ck_strdup(name), *p;

      /* get the base name */
      if (p = strrchr(tmpdir, '/'))
	*(p + 1) = 0;
      else
	strcpy(tmpdir, ".");
      
      input->in_file_name = name;
      input->out_file_name = temp_file_template (tmpdir, "sed");
      output_fd = mkstemp (input->out_file_name);
      free (tmpdir);

#ifdef HAVE_FCHMOD
      {
        struct stat st;
        fstat (fileno (input->fp), &st);
        fchmod (output_fd, st.st_mode);
      }
#endif

      output_file = fdopen (output_fd, "w");
    }
  else
    output_file = stdout;
}


/* Clean up an input stream that we are done with. */
static void closedown P_((struct input *));
static void
closedown(input)
  struct input *input;
{
  input->read_fn = read_always_fail;
  if (!input->fp)
    return;
  if (input->fp != stdin) /* stdin can be reused on tty and tape devices */
    ck_fclose(input->fp);

      if (in_place_extension && output_file != NULL)
	{
	  if (strcmp(in_place_extension, "*") != 0)
	    {
	      char *backup_file_name = get_backup_file_name(input->in_file_name);
	      rename (input->in_file_name, backup_file_name);
	      free (backup_file_name);
	    }

	  ck_fclose (output_file);
	  rename (input->out_file_name, input->in_file_name);
	  free (input->out_file_name);
	}

  input->fp = NULL;

  if (separate_files)
    rewind_read_files ();
}

/* Reset range commands so that they are marked as non-matching */
static void reset_addresses P_((struct vector *));
static void
reset_addresses(vec)
     struct vector *vec;
{
  struct sed_cmd *cur_cmd;
  int n;

  for (cur_cmd = vec->v, n = vec->v_length; n--; cur_cmd++)
    cur_cmd->a1_matched = FALSE;
}

/* Read in the next line of input, and store it in the pattern space.
   Return zero if there is nothing left to input. */
static flagT read_pattern_space P_((struct input *, struct vector *, flagT));
static flagT
read_pattern_space(input, the_program, append)
  struct input *input;
  struct vector *the_program;
  flagT append;
{
  if (append_head) /* redundant test to optimize for common case */
    dump_append_queue();
  replaced = FALSE;
  if (!append)
    line.length = 0;
  line.chomped = TRUE;  /* default, until proved otherwise */

  while ( ! (*input->read_fn)(input) )
    {
      closedown(input);

      if (!*input->file_list)
	{
	  line.chomped = FALSE;
	  return FALSE;
	}

      if (separate_files)
	{
	  input->line_number = 0;
	  reset_addresses (the_program);
	}

      open_next_file (*input->file_list++, input);
    }

  ++input->line_number;
  return TRUE;
}


static flagT last_file_with_data_p P_((struct input *));
static flagT
last_file_with_data_p(input)
  struct input *input;
{
  for (;;)
    {
      int ch;

      closedown(input);
      if (!*input->file_list)
	return TRUE;
      open_next_file(*input->file_list++, input);
      if (input->fp)
	{
	  if ((ch = getc(input->fp)) != EOF)
	    {
	      ungetc(ch, input->fp);
	      return FALSE;
	    }
	}
    }
}

/* Determine if we match the `$' address. */
static flagT test_eof P_((struct input *));
static flagT
test_eof(input)
  struct input *input;
{
  int ch;

  if (buffer.length)
    return FALSE;
  if (!input->fp)
    return separate_files || last_file_with_data_p(input);
  if (feof(input->fp))
    return separate_files || last_file_with_data_p(input);
  if ((ch = getc(input->fp)) == EOF)
    return separate_files || last_file_with_data_p(input);
  ungetc(ch, input->fp);
  return FALSE;
}

/* Return non-zero if the current line matches the address
   pointed to by `addr'. */
static flagT match_an_address_p P_((struct addr *, struct input *));
static flagT
match_an_address_p(addr, input)
  struct addr *addr;
  struct input *input;
{
  switch (addr->addr_type)
    {
    case addr_is_null:
      return TRUE;

    case addr_is_regex:
      return match_regex(addr->addr_regex, line.active, line.length, 0, NULL, 0);

    case addr_is_num:
      return (addr->addr_number == input->line_number);

    case addr_is_num_mod:
      if (addr->addr_number < addr->addr_step)
	return (addr->addr_number == input->line_number%addr->addr_step);
      /* addr_number >= step implies we have an extra initial skip */
      if (input->line_number < addr->addr_number)
	return FALSE;
      /* normalize */
      addr->addr_number %= addr->addr_step;
      return (addr->addr_number == 0);

    case addr_is_num2:
    case addr_is_step:
    case addr_is_step_mod:
      /* reminder: these are only meaningful for a2 addresses */
      /* a2->addr_number needs to be recomputed each time a1 address
         matches for the step and step_mod types */
      return (addr->addr_number <= input->line_number);

    case addr_is_last:
      return test_eof(input);

    default:
      panic(_("INTERNAL ERROR: bad address type"));
    }
  /*NOTREACHED*/
  return FALSE;
}

/* return non-zero if current address is valid for cmd */
static flagT match_address_p P_((struct sed_cmd *, struct input *));
static flagT
match_address_p(cmd, input)
  struct sed_cmd *cmd;
  struct input *input;
{
  flagT addr_matched = cmd->a1_matched;

  if (addr_matched)
    {
      if (match_an_address_p(cmd->a2, input))
	cmd->a1_matched = FALSE;
    }
  else if (!cmd->a1 || match_an_address_p(cmd->a1, input))
    {
      addr_matched = TRUE;
      if (cmd->a2)
	{
	  cmd->a1_matched = TRUE;
	  switch (cmd->a2->addr_type)
	    {
	    case addr_is_regex:
	      break;
	    case addr_is_step:
	      cmd->a2->addr_number = input->line_number + cmd->a2->addr_step;
	      break;
	    case addr_is_step_mod:
	      cmd->a2->addr_number = input->line_number + cmd->a2->addr_step
				     - (input->line_number%cmd->a2->addr_step);
	      break;
	    default:
	      if (match_an_address_p(cmd->a2, input))
		cmd->a1_matched = FALSE;
	      break;
	    }
	}
    }
  if (cmd->addr_bang)
    return !addr_matched;
  return addr_matched;
}


static void do_list P_((int line_len));
static void
do_list(line_len)
     int line_len;
{
  unsigned char *p = CAST(unsigned char *)line.active;
  countT len = line.length;
  countT width = 0;
  char obuf[180];	/* just in case we encounter a 512-bit char (;-) */
  char *o;
  size_t olen;

  for (; len--; ++p) {
      o = obuf;
      
      /* Some locales define 8-bit characters as printable.  This makes the
	 testsuite fail at 8to7.sed because the `l' command in fact will not
	 convert the 8-bit characters. */
#if defined isascii || defined HAVE_ISASCII
      if (isascii(*p) && ISPRINT(*p)) {
#else
      if (ISPRINT(*p)) {
#endif
	  *o++ = *p;
	  if (*p == '\\')
	    *o++ = '\\';
      } else {
	  *o++ = '\\';
	  switch (*p) {
#if defined __STDC__ && __STDC__-0
	    case '\a': *o++ = 'a'; break;
#else /* Not STDC; we'll just assume ASCII */
	    case 007:  *o++ = 'a'; break;
#endif
	    case '\b': *o++ = 'b'; break;
	    case '\f': *o++ = 'f'; break;
	    case '\n': *o++ = 'n'; break;
	    case '\r': *o++ = 'r'; break;
	    case '\t': *o++ = 't'; break;
	    case '\v': *o++ = 'v'; break;
	    default:
	      sprintf(o, "%03o", *p);
	      o += strlen(o);
	      break;
	    }
      }
      olen = o - obuf;
      if (width+olen >= line_len && line_len > 0) {
	  ck_fwrite("\\\n", 1, 2, output_file);
	  width = 0;
      }
      ck_fwrite(obuf, 1, olen, output_file);
      width += olen;
  }
  ck_fwrite("$\n", 1, 2, output_file);
}

static void do_subst P_((struct subst *));
static void
do_subst(sub)
  struct subst *sub;
{
  size_t start = 0;	/* where to start scan for (next) match in LINE */
  size_t last_end = 0;  /* where did the last successful match end in LINE */
  countT count = 0;	/* number of matches found */

#define MAX_BACKREFERENCES 10
  static struct re_registers regs;

  if (s_accum.alloc == 0)
    line_init(&s_accum, INITIAL_BUFFER_SIZE);
  s_accum.length = 0;

  /* The first part of the loop optimizes s/xxx// when xxx is at the
     start, and s/xxx$// */
  if (!match_regex(sub->regx, line.active, line.length, start,
		   &regs, MAX_BACKREFERENCES))
    return;
  
  if (!sub->replacement && sub->numb <= 1)
    if (regs.start[0] == 0 && !sub->global)
      {
	/* We found a match, set the `replaced' flag. */
	replaced = TRUE;

	line.active += regs.end[0];
	line.length -= regs.end[0];
	line.alloc -= regs.end[0];
	goto did_subst;
      }
    else if (regs.end[0] == line.length)
      {
	/* We found a match, set the `replaced' flag. */
	replaced = TRUE;

	line.length = regs.start[0];
	goto did_subst;
      }

  do
    {
      struct replacement *p;
      enum replacement_types repl_mod = 0;

      size_t offset = regs.start[0];
      size_t matched = regs.end[0] - regs.start[0];

      /* Copy stuff to the left of this match into the output string. */
      if (start < offset)
	str_append(&s_accum, line.active + start, offset - start);

      /* If we're counting up to the Nth match, are we there yet?
         And even if we are there, there is another case we have to
	 skip: are we matching an empty string immediately following
         another match?

         This latter case avoids that baaaac, when passed through
         s,a*,x,g, gives `xbxxcx' instead of xbxcx.  This behavior is
         unacceptable because it is not consistently applied (for
         example, `baaaa' gives `xbx', not `xbxx'). */
      if ((count > 0 && offset == last_end && matched == 0)
	  || ++count < sub->numb)
	{
	  /* If the match was vacuous, skip ahead one character
	   * anyway.
	   */
	  if (matched == 0 && offset < line.length)
	    matched = 1;

	  str_append(&s_accum, line.active + offset, matched);
	  start = offset + matched;
	  continue;
	}

      /* We found a match, set the `replaced' flag. */
      replaced = TRUE;

      last_end = regs.end[0];

      /* Now expand the replacement string into the output string. */
      for (p=sub->replacement; p; p=p->next)
	{
	  int i = p->subst_id;
	  enum replacement_types curr_type;

	  /* Apply a \[lu] modifier that was given earlier, but which we
	     have not had yet the occasion to apply.  But don't do it
	     if this replacement has a modifier of its own. */
	  curr_type = (p->repl_type & repl_modifiers)
	    ? p->repl_type
	    : p->repl_type | repl_mod;

	  repl_mod = 0;
	  if (p->prefix_length)
	    {
	      str_append_modified(&s_accum, p->prefix, p->prefix_length,
				  curr_type);
	      curr_type &= ~repl_modifiers;
	    }

	  if (0 <= i)
	    if (regs.end[i] == regs.start[i] && p->repl_type & repl_modifiers)
	      /* Save this modifier, we shall apply it later.
		 e.g. in s/()([a-z])/\u\1\2/
		 the \u modifier is applied to \2, not \1 */
	      repl_mod = curr_type & repl_modifiers;

	    else
	      str_append_modified(&s_accum, line.active + regs.start[i],
				  CAST(size_t)(regs.end[i] - regs.start[i]),
				  curr_type);
	}

      start = regs.end[0];
      if (!sub->global || start == line.length)
	break;

      /* If the match was vacuous, skip over one character
       * and add that character to the output.
       */
      if (regs.start[0] == regs.end[0])
	{
	  str_append(&s_accum, line.active + offset, 1);
	  ++start;
	}
    }
  while (match_regex(sub->regx, line.active, line.length, start,
		     &regs, MAX_BACKREFERENCES));

  /* Copy stuff to the right of the last match into the output string. */
  if (start < line.length)
    str_append(&s_accum, line.active + start, line.length-start);
  s_accum.chomped = line.chomped;

  /* Exchange line and s_accum.  This can be much cheaper
     than copying s_accum.active into line.text (for huge lines). */
  line_exchange(&line, &s_accum);
 did_subst:
  
  /* Finish up. */
  if (sub->print & 1)
    output_line(line.active, line.length, line.chomped, output_file);
  
  if (sub->eval) 
    {
#ifdef HAVE_POPEN
      FILE *pipe;
      s_accum.length = 0;
      
      str_append (&line, "", 1);
      pipe = popen(line.active, "r");
      
      if (pipe != NULL) 
	{
	  while (!feof (pipe)) 
	    {
	      char buf[4096];
	      int n = fread (buf, sizeof(char), 4096, pipe);
	      if (n > 0)
		str_append(&s_accum, buf, n);
	    }
	  
	  pclose (pipe);

	  line_exchange(&line, &s_accum);
	  if (line.length &&
	      line.active[line.length - 1] == '\n')
	    line.length--;
	}
      else
	panic(_("error in subprocess"));
#else
      panic(_("option `e' not supported"));
#endif
    } 
  
  if (sub->print & 2)
    output_line(line.active, line.length, line.chomped, output_file);
  if (sub->fp)
    output_line(line.active, line.length, line.chomped, sub->fp);
}

#ifdef EXPERIMENTAL_DASH_N_OPTIMIZATION
/* Used to attempt a simple-minded optimization. */

static countT branches;

static countT count_branches P_((struct vector *));
static countT
count_branches(program)
  struct vector *program;
{
  struct sed_cmd *cur_cmd = program->v;
  countT isn_cnt = program->v_length;
  countT cnt = 0;

  while (isn_cnt-- > 0)
    {
      switch (cur_cmd->cmd)
	{
	case 'b':
	case 't':
	case 'T':
	case '{':
	  ++cnt;
	}
    }
  return cnt;
}

static struct sed_cmd *shrink_program P_((struct vector *, struct sed_cmd *));
static struct sed_cmd *
shrink_program(vec, cur_cmd)
  struct vector *vec;
  struct sed_cmd *cur_cmd;
{
  struct sed_cmd *v = vec->v;
  struct sed_cmd *last_cmd = v + vec->v_length;
  struct sed_cmd *p;
  countT cmd_cnt;

  for (p=v; p < cur_cmd; ++p)
    if (p->cmd != ':')
      MEMCPY(v++, p, sizeof *v);
  cmd_cnt = v - vec->v;

  for (; p < last_cmd; ++p)
    if (p->cmd != ':')
      MEMCPY(v++, p, sizeof *v);
  vec->v_length = v - vec->v;

  return (0 < vec->v_length) ? (vec->v + cmd_cnt) : CAST(struct sed_cmd *)0;
}
#endif /*EXPERIMENTAL_DASH_N_OPTIMIZATION*/

/* Execute the program `vec' on the current input line.
   Return exit status if caller should quit, -1 otherwise. */
static int execute_program P_((struct vector *, struct input *));
static int
execute_program(vec, input)
  struct vector *vec;
  struct input *input;
{
  struct sed_cmd *cur_cmd;
  struct sed_cmd *end_cmd;

  cur_cmd = vec->v;
  end_cmd = vec->v + vec->v_length;
  while (cur_cmd < end_cmd)
    {
      if (match_address_p(cur_cmd, input))
	{
	  switch (cur_cmd->cmd)
	    {
	    case 'a':
	      {
		struct append_queue *aq = next_append_slot();
		aq->text = cur_cmd->x.cmd_txt.text;
		aq->textlen = cur_cmd->x.cmd_txt.text_length;
	      }
	      break;

	    case '{':
	    case 'b':
	      cur_cmd = vec->v + cur_cmd->x.jump_index;
	      continue;

	    case '}':
	    case ':':
	      /* Executing labels and block-ends are easy. */
	      break;

	    case 'c':
	      if (!cur_cmd->a1_matched)
		output_line(cur_cmd->x.cmd_txt.text,
			    cur_cmd->x.cmd_txt.text_length, FALSE, output_file);
	      /* POSIX.2 is silent about c starting a new cycle,
		 but it seems to be expected (and make sense). */
	      /* Fall Through */
	    case 'd':
	      line.length = 0;
	      line.chomped = FALSE;
	      return -1;

	    case 'D':
	      {
		char *p = memchr(line.active, '\n', line.length);
		if (!p)
		  {
		    line.length = 0;
		    line.chomped = FALSE;
		    return -1;
		  }
		++p;
		line.alloc -= p - line.active;
		line.length -= p - line.active;
		line.active += p - line.active;

		/* reset to start next cycle without reading a new line: */
		cur_cmd = vec->v;
		continue;
	      }

	    case 'e': {
#ifdef HAVE_POPEN
	      FILE *pipe;
	      int cmd_length = cur_cmd->x.cmd_txt.text_length;
	      if (s_accum.alloc == 0)
		line_init(&s_accum, INITIAL_BUFFER_SIZE);
	      s_accum.length = 0;

	      if (!cmd_length)
		{
		  str_append (&line, "", 1);
		  pipe = popen(line.active, "r");
		} 
	      else
		{
		  cur_cmd->x.cmd_txt.text[cmd_length - 1] = 0;
		  pipe = popen(cur_cmd->x.cmd_txt.text, "r");
		}

	      if (pipe != NULL) 
		{
		  while (!feof (pipe)) 
		    {
		      char buf[4096];
		      int n = fread (buf, sizeof(char), 4096, pipe);
		      if (n > 0)
			if (!cmd_length)
			  str_append(&s_accum, buf, n);
			else
			  output_line(buf, n, FALSE, output_file);
		    }
		  
		  pclose (pipe);
		  if (!cmd_length)
		    {
		      /* Store into pattern space for plain `e' commands */
		      if (s_accum.length &&
			  s_accum.active[s_accum.length - 1] == '\n')
			s_accum.length--;

		      /* Exchange line and s_accum.  This can be much
			 cheaper than copying s_accum.active into line.text
			 (for huge lines). */
		      line_exchange(&line, &s_accum);
		    }
		}
	      else
		panic(_("error in subprocess"));
#else
	      panic(_("`e' command not supported"));
#endif
	      break;
	    }

	    case 'g':
	      line_copy(&hold, &line);
	      break;

	    case 'G':
	      line_append(&hold, &line);
	      break;

	    case 'h':
	      line_copy(&line, &hold);
	      break;

	    case 'H':
	      line_append(&line, &hold);
	      break;

	    case 'i':
	      output_line(cur_cmd->x.cmd_txt.text,
			  cur_cmd->x.cmd_txt.text_length, FALSE, output_file);
	      break;

	    case 'l':
	      do_list(cur_cmd->x.int_arg == -1
		      ? lcmd_out_line_len
		      : cur_cmd->x.int_arg);
	      break;

	    case 'L':
	      fmt(line.active, line.active + line.length,
		  cur_cmd->x.int_arg == -1
		  ? lcmd_out_line_len
		  : cur_cmd->x.int_arg,
		  output_file);
	      break;

	    case 'n':
	      if (!no_default_output)
		output_line(line.active, line.length, line.chomped, output_file);
	      if (test_eof(input) || !read_pattern_space(input, vec, FALSE))
		return -1;
	      break;

	    case 'N':
	      str_append(&line, "\n", 1);
	      if (test_eof(input) || !read_pattern_space(input, vec, TRUE))
		return -1;
	      break;

	    case 'p':
	      output_line(line.active, line.length, line.chomped, output_file);
	      break;

	    case 'P':
	      {
		char *p = memchr(line.active, '\n', line.length);
		output_line(line.active, p ? p - line.active : line.length,
			    p ? 1 : line.chomped, output_file);
	      }
	      break;

	    case 'Q':
	      line.length = 0;
	      line.chomped = FALSE;
	      /* Fall through */

	    case 'q':
	      return cur_cmd->x.int_arg == -1 ? 0 : cur_cmd->x.int_arg;

	    case 'r':
	      if (cur_cmd->x.fname)
		{
		  struct append_queue *aq = next_append_slot();
		  aq->fname = cur_cmd->x.fname;
		}
	      break;

	    case 'R':
	      if (cur_cmd->x.fp && !feof (cur_cmd->x.fp))
		{
		  struct append_queue *aq;
		  size_t buflen;
		  char *text = NULL;
		  int result;

		  result = getline (&text, &buflen, cur_cmd->x.fp);

		  if (result != EOF)
		    {
		      aq = next_append_slot();
		      aq->free = TRUE;
		      aq->text = text;
		      aq->textlen = result;
		    }
		}
	      break;

	    case 's':
	      do_subst(cur_cmd->x.cmd_subst);
	      break;

	    case 't':
	      if (replaced)
		{
		  replaced = FALSE;
		  cur_cmd = vec->v + cur_cmd->x.jump_index;
		  continue;
		}
	      break;

	    case 'T':
	      if (!replaced)
		{
		  cur_cmd = vec->v + cur_cmd->x.jump_index;
		  continue;
		}
	      else
		replaced = FALSE;
	      break;

	    case 'w':
	      if (cur_cmd->x.fp)
		output_line(line.active, line.length,
			    line.chomped, cur_cmd->x.fp);
	      break;

	    case 'W':
	      if (cur_cmd->x.fp)
	        {
		  char *p = memchr(line.active, '\n', line.length);
		  output_line(line.active, p ? p - line.active : line.length,
			      p ? 1 : line.chomped, cur_cmd->x.fp);
	        }
	      break;

	    case 'x':
	      line_exchange(&line, &hold);
	      break;

	    case 'y':
	      {
#if defined MBS_SUPPORT && !defined REG_PERL
               if (MB_CUR_MAX > 1)
                 {
                   int idx, prev_idx; /* index in the input line.  */
                   char **trans;
                   mbstate_t cur_stat;
                   memset(&cur_stat, 0, sizeof(mbstate_t));
                   for (idx = 0; idx < line.length;)
                     {
                       int mbclen, i;
                       mbclen = mbrlen(line.active + idx, line.length - idx,
                                       &cur_stat);
                       /* An invalid sequence, or a truncated multibyte
                          character.  We treat it as a singlebyte character.
                       */
                       if (mbclen == (size_t) -1 || mbclen == (size_t) -2
                           || mbclen == 0)
                         mbclen = 1;

                       trans = cur_cmd->x.translatemb;
                       /* `i' indicate i-th translate pair.  */
                       for (i = 0; trans[2*i] != NULL; i++)
                         {
                           if (strncmp(line.active + idx, trans[2*i], mbclen)
                               == 0)
                             {
                               flagT move_remain_buffer = FALSE;
                               int trans_len = strlen(trans[2*i+1]);

                               if (mbclen < trans_len)
                                 {
                                   int new_len;
                                   new_len = line.length + 1 + trans_len - mbclen;
                                   /* We must extend the line buffer.  */
                                   if (line.alloc < new_len)
                                     {
                                       /* And we must resize the buffer.  */
                                       resize_line(&line, new_len);
                                     }
                                   move_remain_buffer = TRUE;
                                 }
                               else if (mbclen > trans_len)
                                 {
                                   /* We must truncate the line buffer.  */
                                   move_remain_buffer = TRUE;
                                 }
                               prev_idx = idx;
                               if (move_remain_buffer)
                                 {
                                   int move_len, move_offset;
                                   char *move_from, *move_to;
                                   /* Move the remaining with \0.  */
                                   move_from = line.active + idx + mbclen;
                                   move_to = line.active + idx + trans_len;
                                   move_len = line.length + 1 - idx - mbclen;
                                   move_offset = trans_len - mbclen;
                                   memmove(move_to, move_from, move_len);
                                   line.length += move_offset;
                                   idx += move_offset;
                                 }
                               strncpy(line.active + prev_idx, trans[2*i+1],
                                       trans_len);
                               break;
                             }
                         }
                       idx += mbclen;
                     }
                 }
               else
#endif /* MBS_SUPPORT */
                 {
                   unsigned char *p, *e;
                   p = CAST(unsigned char *)line.active;
                   for (e=p+line.length; p<e; ++p)
                     *p = cur_cmd->x.translate[*p];
                 }
	      }
	      break;

	    case '=':
	      fprintf(output_file, "%lu\n",
		      CAST(unsigned long)input->line_number);
	      break;

	    default:
	      panic(_("INTERNAL ERROR: Bad cmd %c"), cur_cmd->cmd);
	    }
	}

#ifdef EXPERIMENTAL_DASH_N_OPTIMIZATION
      /* If our top-level program consists solely of commands with
       * addr_is_num addresses then once we past the last mentioned
       * line we should be able to quit if no_default_output is true,
       * or otherwise quickly copy input to output.  Now whether this
       * optimization is a win or not depends on how cheaply we can
       * implement this for the cases where it doesn't help, as
       * compared against how much time is saved.  One semantic
       * difference (which I think is an improvement) is that *this*
       * version will terminate after printing line two in the script
       * "yes | sed -n 2p". 
       *
       * Don't use this when in-place editing is active, because line
       * numbers restart each time then. */
      else if (output_file == stdout)
	{
	  /* can we ever match again? */
	  if (cur_cmd->a1->addr_type == addr_is_num &&
	      ((input->line_number < cur_cmd->a1->addr_number)
	       != !cur_cmd->addr_bang))
	    {
	      /* skip all this next time */
	      cur_cmd->a1->addr_type = addr_is_null;
	      cur_cmd->addr_bang = TRUE;

	      /* can we make an optimization? */
	      if (cur_cmd->cmd == 'b' || cur_cmd->cmd == 't'
		  || cur_cmd->cmd == '{')
		--branches;
	      cur_cmd->cmd = ':';	/* replace with no-op */
	      if (branches == 0)
		{
		  /* whew!  all that just so that we can get to here! */
		  cur_cmd = shrink_program(vec, cur_cmd);
		  if (!cur_cmd && no_default_output)
		    return 0;
		  end_cmd = vec->v + vec->v_length;
		  if (!cur_cmd)
		    cur_cmd = end_cmd;
		  continue;
		}
	    }
	}
#endif /*EXPERIMENTAL_DASH_N_OPTIMIZATION*/

      /* this is buried down here so that a "continue" statement can skip it */
      ++cur_cmd;
    }
    return -1;
}



/* Apply the compiled script to all the named files. */
int
process_files(the_program, argv)
  struct vector *the_program;
  char **argv;
{
  static char dash[] = "-";
  static char *stdin_argv[2] = { dash, NULL };
  struct input input;
  int status;

  line_init(&line, INITIAL_BUFFER_SIZE);
  line_init(&hold, 0);
  line_init(&buffer, 0);

#ifdef EXPERIMENTAL_DASH_N_OPTIMIZATION
  branches = count_branches(the_program);
#endif /*EXPERIMENTAL_DASH_N_OPTIMIZATION*/
  input.file_list = stdin_argv;
  if (argv && *argv)
    input.file_list = argv;
  input.bad_count = 0;
  input.line_number = 0;
  input.read_fn = read_always_fail;
  input.fp = NULL;

  status = EXIT_SUCCESS;
  while (read_pattern_space(&input, the_program, FALSE))
    {
      status = execute_program(the_program, &input);
      if (!no_default_output)
	output_line(line.active, line.length, line.chomped, output_file);
      if (status == -1)
	status = EXIT_SUCCESS;
      else
	break;
    }
  closedown(&input);

#ifdef DEBUG_LEAKS
  /* We're about to exit, so these free()s are redundant.
     But if we're running under a memory-leak detecting
     implementation of malloc(), we want to explicitly
     deallocate in order to avoid extraneous noise from
     the allocator. */
  release_append_queue();
  FREE(buffer.text);
  FREE(hold.text);
  FREE(line.text);
  FREE(s_accum.text);
#endif /*DEBUG_LEAKS*/

  if (input.bad_count)
    status = 2;

  return status;
}
/* `L' command implementation for GNU sed, based on GNU fmt 1.22.
   Copyright (C) 1994, 1995, 1996, 2002, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* GNU fmt was written by Ross Paterson <rap@doc.ic.ac.uk>.  */

#include "config.h"
#include "basicdefs.h"

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>

#if HAVE_LIMITS_H
# include <limits.h>
#endif

#ifndef UINT_MAX
# define UINT_MAX ((unsigned int) ~(unsigned int) 0)
#endif

#ifndef INT_MAX
# define INT_MAX ((int) (UINT_MAX >> 1))
#endif

/* The following parameters represent the program's idea of what is
   "best".  Adjust to taste, subject to the caveats given.  */

/* Prefer lines to be LEEWAY % shorter than the maximum width, giving
   room for optimization.  */
#define	LEEWAY	7

/* Costs and bonuses are expressed as the equivalent departure from the
   optimal line length, multiplied by 10.  e.g. assigning something a
   cost of 50 means that it is as bad as a line 5 characters too short
   or too long.  The definition of SHORT_COST(n) should not be changed.
   However, EQUIV(n) may need tuning.  */

typedef long COST;

#define	MAXCOST	(~(((unsigned long) 1) << (8 * sizeof (COST) -1)))

#define	SQR(n)		((n) * (n))
#define	EQUIV(n)	SQR ((COST) (n))

/* Cost of a filled line n chars longer or shorter than best_width.  */
#define	SHORT_COST(n)	EQUIV ((n) * 10)

/* Cost of the difference between adjacent filled lines.  */
#define	RAGGED_COST(n)	(SHORT_COST (n) / 2)

/* Basic cost per line.  */
#define	LINE_COST	EQUIV (70)

/* Cost of breaking a line after the first word of a sentence, where
   the length of the word is N.  */
#define	WIDOW_COST(n)	(EQUIV (200) / ((n) + 2))

/* Cost of breaking a line before the last word of a sentence, where
   the length of the word is N.  */
#define	ORPHAN_COST(n)	(EQUIV (150) / ((n) + 2))

/* Bonus for breaking a line at the end of a sentence.  */
#define	SENTENCE_BONUS	EQUIV (50)

/* Cost of breaking a line after a period not marking end of a sentence.
   With the definition of sentence we are using (borrowed from emacs, see
   get_line()) such a break would then look like a sentence break.  Hence
   we assign a very high cost -- it should be avoided unless things are
   really bad.  */
#define	NOBREAK_COST	EQUIV (600)

/* Bonus for breaking a line before open parenthesis.  */
#define	PAREN_BONUS	EQUIV (40)

/* Bonus for breaking a line after other punctuation.  */
#define	PUNCT_BONUS	EQUIV(40)

/* Credit for breaking a long paragraph one line later.  */
#define	LINE_CREDIT	EQUIV(3)

/* Size of paragraph buffer in words.  Longer paragraphs are handled
   neatly (cf. flush_paragraph()), so there's little to gain by making
   these larger.  */
#define	MAXWORDS	1000

#define GETC()          (parabuf == end_of_parabuf ? EOF : *parabuf++)

/* Extra ctype(3)-style macros.  */

#define	isopen(c)	(strchr ("([`'\"", (c)) != NULL)
#define	isclose(c)	(strchr (")]'\"", (c)) != NULL)
#define	isperiod(c)	(strchr (".?!", (c)) != NULL)

/* Size of a tab stop, for expansion on input and re-introduction on
   output.  */
#define	TABWIDTH	8

/* Word descriptor structure.  */

typedef struct Word WORD;

struct Word
  {

    /* Static attributes determined during input.  */

    const char *text;		/* the text of the word */
    short length;		/* length of this word */
    short space;		/* the size of the following space */
    flagT paren:1;		/* starts with open paren */
    flagT period:1;		/* ends in [.?!])* */
    flagT punct:1;		/* ends in punctuation */
    flagT final:1;		/* end of sentence */

    /* The remaining fields are computed during the optimization.  */

    short line_length;		/* length of the best line starting here */
    COST best_cost;		/* cost of best paragraph starting here */
    WORD *next_break;		/* break which achieves best_cost */
  };

/* Forward declarations.  */

extern void fmt P_ ((char *line, char *line_end, int max_length, FILE *output_file));
static flagT get_paragraph P_ ((void));
static int get_line P_ ((int c));
static int get_space P_ ((int c));
static int copy_rest P_ ((int c));
static flagT same_para P_ ((int c));
static void flush_paragraph P_ ((void));
static void fmt_paragraph P_ ((void));
static void check_punctuation P_ ((WORD *w));
static COST base_cost P_ ((WORD *this));
static COST line_cost P_ ((WORD *next, int len));
static void put_paragraph P_ ((WORD *finish));
static void put_line P_ ((WORD *w, int indent));
static void put_word P_ ((WORD *w));
static void put_space P_ ((int space));

/* Option values.  */

/* User-supplied maximum line width (default WIDTH).  The only output
   lines
   longer than this will each comprise a single word.  */
static int max_width;

/* Space for the paragraph text.  */
static char *parabuf;

/* End of space for the paragraph text.  */
static char *end_of_parabuf;

/* The file on which we output */
static FILE *outfile;

/* Values derived from the option values.  */

/* The preferred width of text lines, set to LEEWAY % less than max_width.  */
static int best_width;

/* Dynamic variables.  */

/* Start column of the character most recently read from the input file.  */
static int in_column;

/* Start column of the next character to be written to stdout.  */
static int out_column;

/* The words of a paragraph -- longer paragraphs are handled neatly
   (cf. flush_paragraph()).  */
static WORD words[MAXWORDS];

/* A pointer into the above word array, indicating the first position
   after the last complete word.  Sometimes it will point at an incomplete
   word.  */
static WORD *word_limit;

/* Indentation of the first line of the current paragraph.  */
static int first_indent;

/* Indentation of other lines of the current paragraph */
static int other_indent;

/* The last character read from the input file.  */
static int next_char;

/* If nonzero, the length of the last line output in the current
   paragraph, used to charge for raggedness at the split point for long
   paragraphs chosen by fmt_paragraph().  */
static int last_line_length;

/* read file F and send formatted output to stdout.  */

void
fmt (char *line, char *line_end, int max_length, FILE *output_file)
{
  parabuf = line;
  end_of_parabuf = line_end;
  outfile = output_file;

  max_width = max_length;
  best_width = max_width * (201 - 2 * LEEWAY) / 200;

  in_column = 0;
  other_indent = 0;
  next_char = GETC();
  while (get_paragraph ())
    {
      fmt_paragraph ();
      put_paragraph (word_limit);
    }
}

/* Read a paragraph from input file F.  A paragraph consists of a
   maximal number of non-blank (excluding any prefix) lines
   with the same indent.

   Return FALSE if end-of-file was encountered before the start of a
   paragraph, else TRUE.  */

static flagT
get_paragraph ()
{
  register int c;

  last_line_length = 0;
  c = next_char;

  /* Scan (and copy) blank lines, and lines not introduced by the prefix.  */

  while (c == '\n' || c == EOF)
    {
      c = copy_rest (c);
      if (c == EOF)
	{
	  next_char = EOF;
	  return FALSE;
	}
      putc ('\n', outfile);
      c = GETC();
    }

  /* Got a suitable first line for a paragraph.  */

  first_indent = in_column;
  word_limit = words;
  c = get_line (c);

  /* Read rest of paragraph.  */

  other_indent = in_column;
  while (same_para (c) && in_column == other_indent)
    c = get_line (c);

  (word_limit - 1)->period = (word_limit - 1)->final = TRUE;
  next_char = c;
  return TRUE;
}

/* Copy to the output a blank line.  In the latter, C is \n or EOF.
   Return the character (\n or EOF) ending the line.  */

static int
copy_rest (register int c)
{
  out_column = 0;
  while (c != '\n' && c != EOF)
    {
      putc (c, outfile);
      c = GETC();
    }
  return c;
}

/* Return TRUE if a line whose first non-blank character after the
   prefix (if any) is C could belong to the current paragraph,
   otherwise FALSE.  */

static flagT
same_para (register int c)
{
  return (c != '\n' && c != EOF);
}

/* Read a line from the input data given first non-blank character C
   after the prefix, and the following indent, and break it into words.
   A word is a maximal non-empty string of non-white characters.  A word
   ending in [.?!]["')\]]* and followed by end-of-line or at least two
   spaces ends a sentence, as in emacs.

   Return the first non-blank character of the next line.  */

static int
get_line (register int c)
{
  int start;
  register WORD *end_of_word;

  end_of_word = &words[MAXWORDS - 2];

  do
    {				/* for each word in a line */

      /* Scan word.  */

      word_limit->text = parabuf - 1;
      do
	c = GETC();
      while (c != EOF && !ISSPACE (c));
      word_limit->length = parabuf - word_limit->text - (c != EOF);
      in_column += word_limit->length;

      check_punctuation (word_limit);

      /* Scan inter-word space.  */

      start = in_column;
      c = get_space (c);
      word_limit->space = in_column - start;
      word_limit->final = (c == EOF
			   || (word_limit->period
			       && (c == '\n' || word_limit->space > 1)));
      if (c == '\n' || c == EOF)
	word_limit->space = word_limit->final ? 2 : 1;
      if (word_limit == end_of_word)
	flush_paragraph ();
      word_limit++;
      if (c == EOF)
	{
	  in_column = first_indent;
	  return EOF;
	}
    }
  while (c != '\n');

  in_column = 0;
  c = GETC();
  return get_space (c);
}

/* Read blank characters from the input data, starting with C, and keeping
   in_column up-to-date.  Return first non-blank character.  */

static int
get_space (register int c)
{
  for (;;)
    {
      if (c == ' ')
	in_column++;
      else if (c == '\t')
	in_column = (in_column / TABWIDTH + 1) * TABWIDTH;
      else
	return c;
      c = GETC();
    }
}

/* Set extra fields in word W describing any attached punctuation.  */

static void
check_punctuation (register WORD *w)
{
  register const char *start, *finish;

  start = w->text;
  finish = start + (w->length - 1);
  w->paren = isopen (*start);
  w->punct = ISPUNCT (*finish);
  while (isclose (*finish) && finish > start)
    finish--;
  w->period = isperiod (*finish);
}

/* Flush part of the paragraph to make room.  This function is called on
   hitting the limit on the number of words or characters.  */

static void
flush_paragraph (void)
{
  WORD *split_point;
  register WORD *w;
  COST best_break;

  /* - format what you have so far as a paragraph,
     - find a low-cost line break near the end,
     - output to there,
     - make that the start of the paragraph.  */

  fmt_paragraph ();

  /* Choose a good split point.  */

  split_point = word_limit;
  best_break = MAXCOST;
  for (w = words->next_break; w != word_limit; w = w->next_break)
    {
      if (w->best_cost - w->next_break->best_cost < best_break)
	{
	  split_point = w;
	  best_break = w->best_cost - w->next_break->best_cost;
	}
      if (best_break <= MAXCOST - LINE_CREDIT)
	best_break += LINE_CREDIT;
    }
  put_paragraph (split_point);

  /* Copy words from split_point down to word -- we use memmove because
     the source and target may overlap.  */

  memmove ((char *) words, (char *) split_point,
	 (word_limit - split_point + 1) * sizeof (WORD));
  word_limit -= split_point - words;
}

/* Compute the optimal formatting for the whole paragraph by computing
   and remembering the optimal formatting for each suffix from the empty
   one to the whole paragraph.  */

static void
fmt_paragraph (void)
{
  register WORD *start, *w;
  register int len;
  register COST wcost, best;
  int saved_length;

  word_limit->best_cost = 0;
  saved_length = word_limit->length;
  word_limit->length = max_width;	/* sentinel */

  for (start = word_limit - 1; start >= words; start--)
    {
      best = MAXCOST;
      len = start == words ? first_indent : other_indent;

      /* At least one word, however long, in the line.  */

      w = start;
      len += w->length;
      do
	{
	  w++;

	  /* Consider breaking before w.  */

	  wcost = line_cost (w, len) + w->best_cost;
	  if (start == words && last_line_length > 0)
	    wcost += RAGGED_COST (len - last_line_length);
	  if (wcost < best)
	    {
	      best = wcost;
	      start->next_break = w;
	      start->line_length = len;
	    }
	  len += (w - 1)->space + w->length;	/* w > start >= words */
	}
      while (len < max_width);
      start->best_cost = best + base_cost (start);
    }

  word_limit->length = saved_length;
}

/* Return the constant component of the cost of breaking before the
   word THIS.  */

static COST
base_cost (register WORD *this)
{
  register COST cost;

  cost = LINE_COST;

  if (this > words)
    {
      if ((this - 1)->period)
	{
	  if ((this - 1)->final)
	    cost -= SENTENCE_BONUS;
	  else
	    cost += NOBREAK_COST;
	}
      else if ((this - 1)->punct)
	cost -= PUNCT_BONUS;
      else if (this > words + 1 && (this - 2)->final)
	cost += WIDOW_COST ((this - 1)->length);
    }

  if (this->paren)
    cost -= PAREN_BONUS;
  else if (this->final)
    cost += ORPHAN_COST (this->length);

  return cost;
}

/* Return the component of the cost of breaking before word NEXT that
   depends on LEN, the length of the line beginning there.  */

static COST
line_cost (register WORD *next, register int len)
{
  register int n;
  register COST cost;

  if (next == word_limit)
    return 0;
  n = best_width - len;
  cost = SHORT_COST (n);
  if (next->next_break != word_limit)
    {
      n = len - next->line_length;
      cost += RAGGED_COST (n);
    }
  return cost;
}

/* Output to stdout a paragraph from word up to (but not including)
   FINISH, which must be in the next_break chain from word.  */

static void
put_paragraph (register WORD *finish)
{
  register WORD *w;

  put_line (words, first_indent);
  for (w = words->next_break; w != finish; w = w->next_break)
    put_line (w, other_indent);
}

/* Output to stdout the line beginning with word W, beginning in column
   INDENT, including the prefix (if any).  */

static void
put_line (register WORD *w, int indent)
{
  register WORD *endline;
  out_column = 0;
  put_space (indent);

  endline = w->next_break - 1;
  for (; w != endline; w++)
    {
      put_word (w);
      put_space (w->space);
    }
  put_word (w);
  last_line_length = out_column;
  putc ('\n', outfile);
}

/* Output to stdout the word W.  */

static void
put_word (register WORD *w)
{
  register const char *s;
  register int n;

  s = w->text;
  for (n = w->length; n != 0; n--)
    putc (*s++, outfile);
  out_column += w->length;
}

/* Output to stdout SPACE spaces, or equivalent tabs.  */

static void
put_space (int space)
{
  out_column += space;
  while (space--)
    putc (' ', outfile);
}
/*  Functions from hack's utils library.
    Copyright (C) 1989, 1990, 1991, 1998, 1999, 2003
    Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "config.h"

#include <stdio.h>

#include <errno.h>
#ifndef errno
  extern int errno;
#endif

#ifdef HAVE_STRINGS_H
# include <strings.h>
#else
# include <string.h>
#endif /* HAVE_STRINGS_H */

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "basicdefs.h"
#include "utils.h"

const char *myname;

void do_ck_fclose P_((FILE *stream));

/* Print an error message and exit */
#if !defined __STDC__ || !(__STDC__-0)
# include <varargs.h>
# define VSTART(l,a)	va_start(l)
# undef stderr
# define stderr stdout
void
panic(str, va_alist)
  char *str;
  va_dcl
#else /*__STDC__*/
# include <stdarg.h>
# define VSTART(l,a)	va_start(l, a)

/*
  Non-determinism bug fix
  SIR July 2006, Kyle R. Murphy
*/
#undef stderr
#define stderr stdout
void
panic(const char *str, ...)
#endif /* __STDC__ */
{
  va_list iggy;
#undef stderr
#define stderr stdout
  fprintf(stderr, "%s: ", myname);
  VSTART(iggy, str);
#ifndef HAVE_VPRINTF
# ifndef HAVE_DOPRNT
  fputs(str, stderr);	/* not great, but perhaps better than nothing... */
# else /* HAVE_DOPRNT */
  _doprnt(str, &iggy, stderr);
# endif /* HAVE_DOPRNT */
#else /* HAVE_VFPRINTF */
  vfprintf(stderr, str, iggy);
#endif /* HAVE_VFPRINTF */
  va_end(iggy);
  putc('\n', stderr);
  exit(4);
}


/* Store information about files opened with ck_fopen
   so that error messages from ck_fread, ck_fwrite, etc. can print the
   name of the file that had the error */

struct id
  {
    FILE *fp;
    char *name;
    struct id *link;
  };

static struct id *utils_id_s = NULL;

/* Internal routine to get a filename from utils_id_s */
static const char *utils_fp_name P_((FILE *fp));
static const char *
utils_fp_name(fp)
  FILE *fp;
{
  struct id *p;

  for (p=utils_id_s; p; p=p->link)
    if (p->fp == fp)
      return p->name;
  if (fp == stdin)
    return "stdin";
  else if (fp == stdout)
    return "stdout";
  else if (fp == stderr)
    return "stderr";

  return "<unknown>";
}

/* Panic on failing fopen */
FILE *
ck_fopen(name, mode, fail)
  const char *name;
  const char *mode;
  flagT fail;
{
  FILE *fp;
  struct id *p;

  fp = fopen (name, mode);
  if (!fp)
    {
      if (fail)
        panic(_("Couldn't open file %s: %s"), name, strerror(errno));

      return NULL;
    }

  for (p=utils_id_s; p; p=p->link)
    {
      if (fp == p->fp)
	{
	  FREE(p->name);
	  break;
	}
    }
  if (!p)
    {
      p = MALLOC(1, struct id);
      p->link = utils_id_s;
      utils_id_s = p;
    }
  p->name = ck_strdup(name);
  p->fp = fp;
  return fp;
}

/* Panic on failing fwrite */
void
ck_fwrite(ptr, size, nmemb, stream)
  const VOID *ptr;
  size_t size;
  size_t nmemb;
  FILE *stream;
{
  clearerr(stream);
  if (size && fwrite(ptr, size, nmemb, stream) != nmemb)
    panic(ngettext("couldn't write %d item to %s: %s",
		   "couldn't write %d items to %s: %s", nmemb), 
		nmemb, utils_fp_name(stream), strerror(errno));
}

/* Panic on failing fread */
size_t
ck_fread(ptr, size, nmemb, stream)
  VOID *ptr;
  size_t size;
  size_t nmemb;
  FILE *stream;
{
  clearerr(stream);
  if (size && (nmemb=fread(ptr, size, nmemb, stream)) <= 0 && ferror(stream))
    panic(_("read error on %s: %s"), utils_fp_name(stream), strerror(errno));

  return nmemb;
}

/* Panic on failing fflush */
void
ck_fflush(stream)
  FILE *stream;
{
  clearerr(stream);
  if (fflush(stream) == EOF && errno != EBADF)
    panic("Couldn't flush %s: %s", utils_fp_name(stream), strerror(errno));
}

/* Panic on failing fclose */
void
ck_fclose(stream)
  FILE *stream;
{
  struct id r;
  struct id *prev;
  struct id *cur;

  /* a NULL stream means to close all files */
  r.link = utils_id_s;
  prev = &r;
  while ( (cur = prev->link) )
    {
      if (!stream || stream == cur->fp)
	{
	  do_ck_fclose (cur->fp);
	  prev->link = cur->link;
	  FREE(cur->name);
	  FREE(cur);
	}
      else
	prev = cur;
    }

  utils_id_s = r.link;

  /* Also care about stdout, because if it is redirected the
     last output operations might fail and it is important
     to signal this as an error (perhaps to make). */
  if (!stream)
    {
      do_ck_fclose (stdout);
      do_ck_fclose (stderr);
    }
}

/* Close a single file and update a count of closed files. */
void
do_ck_fclose(stream)
  FILE *stream;
{
  ck_fflush(stream);
  clearerr(stream);
  if (fclose(stream) == EOF)
    panic("Couldn't close %s: %s", utils_fp_name(stream), strerror(errno));
}



char *
temp_file_template(tmpdir, program)
  const char *tmpdir;    
  char *program;
{
  char *template;
  if (tmpdir == NULL)
    tmpdir = getenv("TMPDIR");
  if (tmpdir == NULL)
    {
      tmpdir = getenv("TMP");
      if (tmpdir == NULL)
#ifdef P_tmpdir
	tmpdir = P_tmpdir;
#else
	tmpdir = "/tmp";
#endif
    }

  template = xmalloc (strlen (tmpdir) + strlen (program) + 8);
  sprintf (template, "%s/%sXXXXXX", tmpdir, program);
  return (template);
}


/* Panic on failing malloc */
VOID *
ck_malloc(size)
  size_t size;
{
  VOID *ret = calloc(1, size ? size : 1);
  if (!ret)
    panic("Couldn't allocate memory");
  return ret;
}

/* Panic on failing malloc */
VOID *
xmalloc(size)
  size_t size;
{
  return ck_malloc(size);
}

/* Panic on failing realloc */
VOID *
ck_realloc(ptr, size)
  VOID *ptr;
  size_t size;
{
  VOID *ret;

  if (size == 0)
    {
      FREE(ptr);
      return NULL;
    }
  if (!ptr)
    return ck_malloc(size);
  ret = realloc(ptr, size);
  if (!ret)
    panic("Couldn't re-allocate memory");
  return ret;
}

/* Return a malloc()'d copy of a string */
char *
ck_strdup(str)
  const char *str;
{
  char *ret = MALLOC(strlen(str)+1, char);
  return strcpy(ret, str);
}

/* Return a malloc()'d copy of a block of memory */
VOID *
ck_memdup(buf, len)
  const VOID *buf;
  size_t len;
{
  VOID *ret = ck_malloc(len);
  return memcpy(ret, buf, len);
}

/* Release a malloc'd block of memory */
void
ck_free(ptr)
  VOID *ptr;
{
  if (ptr)
    free(ptr);
}


/* Implement a variable sized buffer of `stuff'.  We don't know what it is,
nor do we care, as long as it doesn't mind being aligned by malloc. */

struct buffer
  {
    size_t allocated;
    size_t length;
    char *b;
  };

#define MIN_ALLOCATE 50

struct buffer *
init_buffer()
{
  struct buffer *b = MALLOC(1, struct buffer);
  b->b = MALLOC(MIN_ALLOCATE, char);
  b->allocated = MIN_ALLOCATE;
  b->length = 0;
  return b;
}

char *
get_buffer(b)
  struct buffer *b;
{
  return b->b;
}

size_t
size_buffer(b)
  struct buffer *b;
{
  return b->length;
}

static void resize_buffer P_((struct buffer *b, size_t newlen));
static void
resize_buffer(b, newlen)
  struct buffer *b;
  size_t newlen;
{
  char *try = NULL;
  size_t alen = b->allocated;

  if (newlen <= alen)
    return;
  alen *= 2;
  if (newlen < alen)
    try = realloc(b->b, alen);	/* Note: *not* the REALLOC() macro! */
  if (!try)
    {
      alen = newlen;
      try = REALLOC(b->b, alen, char);
    }
  b->allocated = alen;
  b->b = try;
}

void
add_buffer(b, p, n)
  struct buffer *b;
  const char *p;
  size_t n;
{
  if (b->allocated - b->length < n)
    resize_buffer(b, b->length+n);
  memcpy(b->b + b->length, p, n);
  b->length += n;
}

void
add1_buffer(b, c)
  struct buffer *b;
  int c;
{
  /* This special case should be kept cheap;
   *  don't make it just a mere convenience
   *  wrapper for add_buffer() -- even "builtin"
   *  versions of memcpy(a, b, 1) can become
   *  expensive when called too often.
   */
  if (c != EOF)
    {
      if (b->allocated - b->length < 1)
	resize_buffer(b, b->length+1);
      b->b[b->length++] = c;
    }
}

void
free_buffer(b)
  struct buffer *b;
{
  if (b)
    FREE(b->b);
  FREE(b);
}
/* obstack.c - subroutines used implicitly by object stack macros -*- C -*-
   Copyright (C) 1988,89,90,91,92,93,94,96,97 Free Software Foundation, Inc.

   This file is part of the GNU C Library.  Its master source is NOT part of
   the C library, however.  The master source lives in /gd/gnu/lib.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "obstack.h"

/* NOTE BEFORE MODIFYING THIS FILE: This version number must be
   incremented whenever callers compiled using an old obstack.h can no
   longer properly call the functions in this obstack.c.  */
#define OBSTACK_INTERFACE_VERSION 1

/* Comment out all this code if we are using the GNU C Library, and are not
   actually compiling the library itself, and the installed library
   supports the same library interface we do.  This code is part of the GNU
   C Library, but also included in many other GNU distributions.  Compiling
   and linking in this code is a waste when using the GNU C library
   (especially if it is a shared library).  Rather than having every GNU
   program understand `configure --with-gnu-libc' and omit the object
   files, it is simpler to just do this in the source for each such file.  */

#include <stdio.h>		/* Random thing to get __GNU_LIBRARY__.  */
#if !defined (_LIBC) && defined (__GNU_LIBRARY__) && __GNU_LIBRARY__ > 1
#include <gnu-versions.h>
#if _GNU_OBSTACK_INTERFACE_VERSION == OBSTACK_INTERFACE_VERSION
#define ELIDE_CODE
#endif
#endif


#ifndef ELIDE_CODE


#if defined (__STDC__) && __STDC__
#define POINTER void *
#else
#define POINTER char *
#endif

/* Determine default alignment.  */
struct fooalign {char x; double d;};
#define DEFAULT_ALIGNMENT  \
  ((PTR_INT_TYPE) ((char *) &((struct fooalign *) 0)->d - (char *) 0))
/* If malloc were really smart, it would round addresses to DEFAULT_ALIGNMENT.
   But in fact it might be less smart and round addresses to as much as
   DEFAULT_ROUNDING.  So we prepare for it to do that.  */
union fooround {long x; double d;};
#define DEFAULT_ROUNDING (sizeof (union fooround))

#ifdef original_glibc_code
/**//* When we copy a long block of data, this is the unit to do it with. */
/**//* On some machines, copying successive ints does not work; */
/**//* in such a case, redefine COPYING_UNIT to `long' (if that works) */
/**//* or `char' as a last resort.  */
/**/#ifndef COPYING_UNIT
/**/#define COPYING_UNIT int
/**/#endif
#endif

/* The functions allocating more room by calling `obstack_chunk_alloc'
   jump to the handler pointed to by `obstack_alloc_failed_handler'.
   This variable by default points to the internal function
   `print_and_abort'.  */
#if defined (__STDC__) && __STDC__
static void print_and_abort (void);
void (*obstack_alloc_failed_handler) (void) = print_and_abort;
#else
static void print_and_abort ();
void (*obstack_alloc_failed_handler) () = print_and_abort;
#endif

/* Exit value used when `print_and_abort' is used.  */
#if defined __GNU_LIBRARY__ || defined HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif
int obstack_exit_failure = EXIT_FAILURE;

/* The non-GNU-C macros copy the obstack into this global variable
   to avoid multiple evaluation.  */

struct obstack *_obstack;

/* Define a macro that either calls functions with the traditional malloc/free
   calling interface, or calls functions with the mmalloc/mfree interface
   (that adds an extra first argument), based on the state of use_extra_arg.
   For free, do not use ?:, since some compilers, like the MIPS compilers,
   do not allow (expr) ? void : void.  */

#if defined (__STDC__) && __STDC__
#define CALL_CHUNKFUN(h, size) \
  (((h) -> use_extra_arg) \
   ? (*(h)->chunkfun) ((h)->extra_arg, (size)) \
   : (*(struct _obstack_chunk *(*) (long)) (h)->chunkfun) ((size)))

#define CALL_FREEFUN(h, old_chunk) \
  do { \
    if ((h) -> use_extra_arg) \
      (*(h)->freefun) ((h)->extra_arg, (old_chunk)); \
    else \
      (*(void (*) (void *)) (h)->freefun) ((old_chunk)); \
  } while (0)
#else
#define CALL_CHUNKFUN(h, size) \
  (((h) -> use_extra_arg) \
   ? (*(h)->chunkfun) ((h)->extra_arg, (size)) \
   : (*(struct _obstack_chunk *(*) ()) (h)->chunkfun) ((size)))

#define CALL_FREEFUN(h, old_chunk) \
  do { \
    if ((h) -> use_extra_arg) \
      (*(h)->freefun) ((h)->extra_arg, (old_chunk)); \
    else \
      (*(void (*) ()) (h)->freefun) ((old_chunk)); \
  } while (0)
#endif


/* Initialize an obstack H for use.  Specify chunk size SIZE (0 means default).
   Objects start on multiples of ALIGNMENT (0 means use default).
   CHUNKFUN is the function to use to allocate chunks,
   and FREEFUN the function to free them.

   Return nonzero if successful, zero if out of memory.
   To recover from an out of memory error,
   free up some memory, then call this again.  */

int
_obstack_begin (h, size, alignment, chunkfun, freefun)
     struct obstack *h;
     int size;
     int alignment;
#if defined (__STDC__) && __STDC__
     POINTER (*chunkfun) (long);
     void (*freefun) (void *);
#else
     POINTER (*chunkfun) ();
     void (*freefun) ();
#endif
{
  register struct _obstack_chunk *chunk; /* points to new chunk */

  if (alignment == 0)
    alignment = DEFAULT_ALIGNMENT;
  if (size == 0)
    /* Default size is what GNU malloc can fit in a 4096-byte block.  */
    {
      /* 12 is sizeof (mhead) and 4 is EXTRA from GNU malloc.
	 Use the values for range checking, because if range checking is off,
	 the extra bytes won't be missed terribly, but if range checking is on
	 and we used a larger request, a whole extra 4096 bytes would be
	 allocated.

	 These number are irrelevant to the new GNU malloc.  I suspect it is
	 less sensitive to the size of the request.  */
      int extra = ((((12 + DEFAULT_ROUNDING - 1) & ~(DEFAULT_ROUNDING - 1))
		    + 4 + DEFAULT_ROUNDING - 1)
		   & ~(DEFAULT_ROUNDING - 1));
      size = 4096 - extra;
    }

#if defined (__STDC__) && __STDC__
  h->chunkfun = (struct _obstack_chunk * (*)(void *, long)) chunkfun;
  h->freefun = (void (*) (void *, struct _obstack_chunk *)) freefun;
#else
  h->chunkfun = (struct _obstack_chunk * (*)()) chunkfun;
  h->freefun = freefun;
#endif
  h->chunk_size = size;
  h->alignment_mask = alignment - 1;
  h->use_extra_arg = 0;

  chunk = h->chunk = CALL_CHUNKFUN (h, h -> chunk_size);
  if (!chunk)
    (*obstack_alloc_failed_handler) ();
  h->next_free = h->object_base = chunk->contents;
  h->chunk_limit = chunk->limit
    = (char *) chunk + h->chunk_size;
  chunk->prev = 0;
  /* The initial chunk now contains no empty object.  */
  h->maybe_empty_object = 0;
  h->alloc_failed = 0;
  return 1;
}

int
_obstack_begin_1 (h, size, alignment, chunkfun, freefun, arg)
     struct obstack *h;
     int size;
     int alignment;
#if defined (__STDC__) && __STDC__
     POINTER (*chunkfun) (POINTER, long);
     void (*freefun) (POINTER, POINTER);
#else
     POINTER (*chunkfun) ();
     void (*freefun) ();
#endif
     POINTER arg;
{
  register struct _obstack_chunk *chunk; /* points to new chunk */

  if (alignment == 0)
    alignment = DEFAULT_ALIGNMENT;
  if (size == 0)
    /* Default size is what GNU malloc can fit in a 4096-byte block.  */
    {
      /* 12 is sizeof (mhead) and 4 is EXTRA from GNU malloc.
	 Use the values for range checking, because if range checking is off,
	 the extra bytes won't be missed terribly, but if range checking is on
	 and we used a larger request, a whole extra 4096 bytes would be
	 allocated.

	 These number are irrelevant to the new GNU malloc.  I suspect it is
	 less sensitive to the size of the request.  */
      int extra = ((((12 + DEFAULT_ROUNDING - 1) & ~(DEFAULT_ROUNDING - 1))
		    + 4 + DEFAULT_ROUNDING - 1)
		   & ~(DEFAULT_ROUNDING - 1));
      size = 4096 - extra;
    }

#if defined(__STDC__) && __STDC__
  h->chunkfun = (struct _obstack_chunk * (*)(void *,long)) chunkfun;
  h->freefun = (void (*) (void *, struct _obstack_chunk *)) freefun;
#else
  h->chunkfun = (struct _obstack_chunk * (*)()) chunkfun;
  h->freefun = freefun;
#endif
  h->chunk_size = size;
  h->alignment_mask = alignment - 1;
  h->extra_arg = arg;
  h->use_extra_arg = 1;

  chunk = h->chunk = CALL_CHUNKFUN (h, h -> chunk_size);
  if (!chunk)
    (*obstack_alloc_failed_handler) ();
  h->next_free = h->object_base = chunk->contents;
  h->chunk_limit = chunk->limit
    = (char *) chunk + h->chunk_size;
  chunk->prev = 0;
  /* The initial chunk now contains no empty object.  */
  h->maybe_empty_object = 0;
  h->alloc_failed = 0;
  return 1;
}

/* Allocate a new current chunk for the obstack *H
   on the assumption that LENGTH bytes need to be added
   to the current object, or a new object of length LENGTH allocated.
   Copies any partial object from the end of the old chunk
   to the beginning of the new one.  */

void
_obstack_newchunk (h, length)
     struct obstack *h;
     int length;
{
  register struct _obstack_chunk *old_chunk = h->chunk;
  register struct _obstack_chunk *new_chunk;
  register long	new_size;
  register int obj_size = h->next_free - h->object_base;

  /* Compute size for new chunk.  */
  new_size = (obj_size + length) + (obj_size >> 3) + 100;
  if (new_size < h->chunk_size)
    new_size = h->chunk_size;

  /* Allocate and initialize the new chunk.  */
  new_chunk = CALL_CHUNKFUN (h, new_size);
  if (!new_chunk)
    (*obstack_alloc_failed_handler) ();
  h->chunk = new_chunk;
  new_chunk->prev = old_chunk;
  new_chunk->limit = h->chunk_limit = (char *) new_chunk + new_size;

  _obstack_memcpy(new_chunk->contents, h->object_base, obj_size);

  /* If the object just copied was the only data in OLD_CHUNK, */
  /* free that chunk and remove it from the chain. */
  /* But not if that chunk might contain an empty object.  */
  if (h->object_base == old_chunk->contents && ! h->maybe_empty_object)
    {
      new_chunk->prev = old_chunk->prev;
      CALL_FREEFUN (h, old_chunk);
    }

  h->object_base = new_chunk->contents;
  h->next_free = h->object_base + obj_size;
  /* The new chunk certainly contains no empty object yet.  */
  h->maybe_empty_object = 0;
}

/* Return nonzero if object OBJ has been allocated from obstack H.
   This is here for debugging.
   If you use it in a program, you are probably losing.  */

#if defined (__STDC__) && __STDC__
/* Suppress -Wmissing-prototypes warning.  We don't want to declare this in
   obstack.h because it is just for debugging.  */
int _obstack_allocated_p (struct obstack *h, POINTER obj);
#endif

int
_obstack_allocated_p (h, obj)
     struct obstack *h;
     POINTER obj;
{
  register struct _obstack_chunk *lp;	/* below addr of any objects in this chunk */
  register struct _obstack_chunk *plp;	/* point to previous chunk if any */

  lp = (h)->chunk;
  /* We use >= rather than > since the object cannot be exactly at
     the beginning of the chunk but might be an empty object exactly
     at the end of an adjacent chunk.  */
  while (lp != 0 && ((POINTER) lp >= obj || (POINTER) (lp)->limit < obj))
    {
      plp = lp->prev;
      lp = plp;
    }
  return lp != 0;
}

/* Free objects in obstack H, including OBJ and everything allocate
   more recently than OBJ.  If OBJ is zero, free everything in H.  */

#undef obstack_free

/* This function has two names with identical definitions.
   This is the first one, called from non-ANSI code.  */

void
_obstack_free (h, obj)
     struct obstack *h;
     POINTER obj;
{
  register struct _obstack_chunk *lp;	/* below addr of any objects in this chunk */
  register struct _obstack_chunk *plp;	/* point to previous chunk if any */

  lp = h->chunk;
  /* We use >= because there cannot be an object at the beginning of a chunk.
     But there can be an empty object at that address
     at the end of another chunk.  */
  while (lp != 0 && ((POINTER) lp >= obj || (POINTER) (lp)->limit < obj))
    {
      plp = lp->prev;
      CALL_FREEFUN (h, lp);
      lp = plp;
      /* If we switch chunks, we can't tell whether the new current
	 chunk contains an empty object, so assume that it may.  */
      h->maybe_empty_object = 1;
    }
  if (lp)
    {
      h->object_base = h->next_free = (char *) (obj);
      h->chunk_limit = lp->limit;
      h->chunk = lp;
    }
  else if (obj != 0)
    /* obj is not in any of the chunks! */
    abort ();
}

/* This function is used from ANSI code.  */

void
obstack_free (h, obj)
     struct obstack *h;
     POINTER obj;
{
  register struct _obstack_chunk *lp;	/* below addr of any objects in this chunk */
  register struct _obstack_chunk *plp;	/* point to previous chunk if any */

  lp = h->chunk;
  /* We use >= because there cannot be an object at the beginning of a chunk.
     But there can be an empty object at that address
     at the end of another chunk.  */
  while (lp != 0 && ((POINTER) lp >= obj || (POINTER) (lp)->limit < obj))
    {
      plp = lp->prev;
      CALL_FREEFUN (h, lp);
      lp = plp;
      /* If we switch chunks, we can't tell whether the new current
	 chunk contains an empty object, so assume that it may.  */
      h->maybe_empty_object = 1;
    }
  if (lp)
    {
      h->object_base = h->next_free = (char *) (obj);
      h->chunk_limit = lp->limit;
      h->chunk = lp;
    }
  else if (obj != 0)
    /* obj is not in any of the chunks! */
    abort ();
}

int
_obstack_memory_used (h)
     struct obstack *h;
{
  register struct _obstack_chunk* lp;
  register int nbytes = 0;

  for (lp = h->chunk; lp != 0; lp = lp->prev)
    {
      nbytes += lp->limit - (char *) lp;
    }
  return nbytes;
}

/* Define the error handler.  */
#ifndef _
# ifdef HAVE_LIBINTL_H
#  include <libintl.h>
#  ifndef _
#   define _(Str) gettext (Str)
#  endif
# else
#  define _(Str) (Str)
# endif
#endif

static void
print_and_abort ()
{
  fputs (_("memory exhausted\n"), stderr);
  exit (obstack_exit_failure);
}

#if 0
/* These are now turned off because the applications do not use it
   and it uses bcopy via obstack_grow, which causes trouble on sysV.  */

/* Now define the functional versions of the obstack macros.
   Define them to simply use the corresponding macros to do the job.  */

#if defined (__STDC__) && __STDC__
/* These function definitions do not work with non-ANSI preprocessors;
   they won't pass through the macro names in parentheses.  */

/* The function names appear in parentheses in order to prevent
   the macro-definitions of the names from being expanded there.  */

POINTER (obstack_base) (obstack)
     struct obstack *obstack;
{
  return obstack_base (obstack);
}

POINTER (obstack_next_free) (obstack)
     struct obstack *obstack;
{
  return obstack_next_free (obstack);
}

int (obstack_object_size) (obstack)
     struct obstack *obstack;
{
  return obstack_object_size (obstack);
}

int (obstack_room) (obstack)
     struct obstack *obstack;
{
  return obstack_room (obstack);
}

int (obstack_make_room) (obstack, length)
     struct obstack *obstack;
     int length;
{
  return obstack_make_room (obstack, length);
}

void (obstack_grow) (obstack, pointer, length)
     struct obstack *obstack;
     POINTER pointer;
     int length;
{
  obstack_grow (obstack, pointer, length);
}

void (obstack_grow0) (obstack, pointer, length)
     struct obstack *obstack;
     POINTER pointer;
     int length;
{
  obstack_grow0 (obstack, pointer, length);
}

void (obstack_1grow) (obstack, character)
     struct obstack *obstack;
     int character;
{
  obstack_1grow (obstack, character);
}

void (obstack_blank) (obstack, length)
     struct obstack *obstack;
     int length;
{
  obstack_blank (obstack, length);
}

void (obstack_1grow_fast) (obstack, character)
     struct obstack *obstack;
     int character;
{
  obstack_1grow_fast (obstack, character);
}

void (obstack_blank_fast) (obstack, length)
     struct obstack *obstack;
     int length;
{
  obstack_blank_fast (obstack, length);
}

POINTER (obstack_finish) (obstack)
     struct obstack *obstack;
{
  return obstack_finish (obstack);
}

POINTER (obstack_alloc) (obstack, length)
     struct obstack *obstack;
     int length;
{
  return obstack_alloc (obstack, length);
}

POINTER (obstack_copy) (obstack, pointer, length)
     struct obstack *obstack;
     POINTER pointer;
     int length;
{
  return obstack_copy (obstack, pointer, length);
}

POINTER (obstack_copy0) (obstack, pointer, length)
     struct obstack *obstack;
     POINTER pointer;
     int length;
{
  return obstack_copy0 (obstack, pointer, length);
}

#endif /* __STDC__ */

#endif /* 0 */

#endif	/* !ELIDE_CODE */
/* Getopt for GNU.
   NOTE: getopt is now part of the C library, so if you don't know what
   "Keep this file name-space clean" means, talk to drepper@gnu.org
   before changing it!

   Copyright (C) 1987, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98
   	Free Software Foundation, Inc.

   NOTE: The canonical source of this file is maintained with the GNU C Library.
   Bugs can be reported to bug-glibc@gnu.org.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

/* This tells Alpha OSF/1 not to define a getopt prototype in <stdio.h>.
   Ditto for AIX 3.2 and <stdlib.h>.  */
#ifndef _NO_PROTO
# define _NO_PROTO
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */
# ifndef const
#  define const
# endif
#endif

#include <stdio.h>

/* Comment out all this code if we are using the GNU C Library, and are not
   actually compiling the library itself.  This code is part of the GNU C
   Library, but also included in many other GNU distributions.  Compiling
   and linking in this code is a waste when using the GNU C library
   (especially if it is a shared library).  Rather than having every GNU
   program understand `configure --with-gnu-libc' and omit the object files,
   it is simpler to just do this in the source for each such file.  */

#define GETOPT_INTERFACE_VERSION 2
#if !defined _LIBC && defined __GLIBC__ && __GLIBC__ >= 2
# include <gnu-versions.h>
# if _GNU_GETOPT_INTERFACE_VERSION == GETOPT_INTERFACE_VERSION
#  define ELIDE_CODE
# endif
#endif

#ifndef ELIDE_CODE


/* This needs to come after some library #include
   to get __GNU_LIBRARY__ defined.  */
#ifdef	__GNU_LIBRARY__
/* Don't include stdlib.h for non-GNU C libraries because some of them
   contain conflicting prototypes for getopt.  */
# include <stdlib.h>
# include <unistd.h>
#endif	/* GNU C library.  */

#ifdef VMS
# include <unixlib.h>
# if HAVE_STRING_H - 0
#  include <string.h>
# endif
#endif

#ifndef _
/* This is for other GNU distributions with internationalized messages.
   When compiling libc, the _ macro is predefined.  */
# ifdef HAVE_LIBINTL_H
#  include <libintl.h>
#  define _(msgid)	gettext (msgid)
# else
#  define _(msgid)	(msgid)
# endif
#endif

/* This version of `getopt' appears to the caller like standard Unix `getopt'
   but it behaves differently for the user, since it allows the user
   to intersperse the options with the other arguments.

   As `getopt' works, it permutes the elements of ARGV so that,
   when it is done, all the options precede everything else.  Thus
   all application programs are extended to handle flexible argument order.

   Setting the environment variable POSIXLY_CORRECT disables permutation.
   Then the behavior is completely standard.

   GNU application programs can use a third alternative mode in which
   they can distinguish the relative order of options and other arguments.  */

#include "getopt.h"

/* For communication from `getopt' to the caller.
   When `getopt' finds an option that takes an argument,
   the argument value is returned here.
   Also, when `ordering' is RETURN_IN_ORDER,
   each non-option ARGV-element is returned here.  */

char *optarg = NULL;

/* Index in ARGV of the next element to be scanned.
   This is used for communication to and from the caller
   and for communication between successive calls to `getopt'.

   On entry to `getopt', zero means this is the first call; initialize.

   When `getopt' returns -1, this is the index of the first of the
   non-option elements that the caller should itself scan.

   Otherwise, `optind' communicates from one call to the next
   how much of ARGV has been scanned so far.  */

/* 1003.2 says this must be 1 before any call.  */
int optind = 1;

/* Formerly, initialization of getopt depended on optind==0, which
   causes problems with re-calling getopt as programs generally don't
   know that. */

int __getopt_initialized = 0;

/* The next char to be scanned in the option-element
   in which the last option character we returned was found.
   This allows us to pick up the scan where we left off.

   If this is zero, or a null string, it means resume the scan
   by advancing to the next ARGV-element.  */

static char *nextchar;

/* Callers store zero here to inhibit the error message
   for unrecognized options.  */

int opterr = 1;

/* Set to an option character which was unrecognized.
   This must be initialized on some systems to avoid linking in the
   system's own getopt implementation.  */

int optopt = '?';

/* Describe how to deal with options that follow non-option ARGV-elements.

   If the caller did not specify anything,
   the default is REQUIRE_ORDER if the environment variable
   POSIXLY_CORRECT is defined, PERMUTE otherwise.

   REQUIRE_ORDER means don't recognize them as options;
   stop option processing when the first non-option is seen.
   This is what Unix does.
   This mode of operation is selected by either setting the environment
   variable POSIXLY_CORRECT, or using `+' as the first character
   of the list of option characters.

   PERMUTE is the default.  We permute the contents of ARGV as we scan,
   so that eventually all the non-options are at the end.  This allows options
   to be given in any order, even with programs that were not written to
   expect this.

   RETURN_IN_ORDER is an option available to programs that were written
   to expect options and other ARGV-elements in any order and that care about
   the ordering of the two.  We describe each non-option ARGV-element
   as if it were the argument of an option with character code 1.
   Using `-' as the first character of the list of option characters
   selects this mode of operation.

   The special argument `--' forces an end of option-scanning regardless
   of the value of `ordering'.  In the case of RETURN_IN_ORDER, only
   `--' can cause `getopt' to return -1 with `optind' != ARGC.  */

static enum
{
  REQUIRE_ORDER, PERMUTE, RETURN_IN_ORDER
} ordering;

/* Value of POSIXLY_CORRECT environment variable.  */
static char *posixly_correct;

#ifdef	__GNU_LIBRARY__
/* We want to avoid inclusion of string.h with non-GNU libraries
   because there are many ways it can cause trouble.
   On some systems, it contains special magic macros that don't work
   in GCC.  */
# include <string.h>
# define my_index	strchr
#else

/* Avoid depending on library functions or files
   whose names are inconsistent.  */

#ifndef getenv
extern char *getenv ();
#endif
#ifndef strncmp
extern int strncmp ();
#endif

static char *
my_index (str, chr)
     const char *str;
     int chr;
{
  while (*str)
    {
      if (*str == chr)
	return (char *) str;
      str++;
    }
  return 0;
}

/* If using GCC, we can safely declare strlen this way.
   If not using GCC, it is ok not to declare it.  */
#ifdef __GNUC__
/* Note that Motorola Delta 68k R3V7 comes with GCC but not stddef.h.
   That was relevant to code that was here before.  */
# if (!defined __STDC__ || !__STDC__) && !defined strlen
/* gcc with -traditional declares the built-in strlen to return int,
   and has done so at least since version 2.4.5. -- rms.  */
extern int strlen (const char *);
# endif /* not __STDC__ */
#endif /* __GNUC__ */

#endif /* not __GNU_LIBRARY__ */

/* Handle permutation of arguments.  */

/* Describe the part of ARGV that contains non-options that have
   been skipped.  `first_nonopt' is the index in ARGV of the first of them;
   `last_nonopt' is the index after the last of them.  */

static int first_nonopt;
static int last_nonopt;

#ifdef _LIBC
/* Bash 2.0 gives us an environment variable containing flags
   indicating ARGV elements that should not be considered arguments.  */

/* Defined in getopt_init.c  */
extern char *__getopt_nonoption_flags;

static int nonoption_flags_max_len;
static int nonoption_flags_len;

static int original_argc;
static char *const *original_argv;

/* Make sure the environment variable bash 2.0 puts in the environment
   is valid for the getopt call we must make sure that the ARGV passed
   to getopt is that one passed to the process.  */
static void
__attribute__ ((unused))
store_args_and_env (int argc, char *const *argv)
{
  /* XXX This is no good solution.  We should rather copy the args so
     that we can compare them later.  But we must not use malloc(3).  */
  original_argc = argc;
  original_argv = argv;
}
# ifdef text_set_element
text_set_element (__libc_subinit, store_args_and_env);
# endif /* text_set_element */

# define SWAP_FLAGS(ch1, ch2) \
  if (nonoption_flags_len > 0)						      \
    {									      \
      char __tmp = __getopt_nonoption_flags[ch1];			      \
      __getopt_nonoption_flags[ch1] = __getopt_nonoption_flags[ch2];	      \
      __getopt_nonoption_flags[ch2] = __tmp;				      \
    }
#else	/* !_LIBC */
# define SWAP_FLAGS(ch1, ch2)
#endif	/* _LIBC */

/* Exchange two adjacent subsequences of ARGV.
   One subsequence is elements [first_nonopt,last_nonopt)
   which contains all the non-options that have been skipped so far.
   The other is elements [last_nonopt,optind), which contains all
   the options processed since those non-options were skipped.

   `first_nonopt' and `last_nonopt' are relocated so that they describe
   the new indices of the non-options in ARGV after they are moved.  */

#if defined __STDC__ && __STDC__
static void exchange (char **);
#endif

static void
exchange (argv)
     char **argv;
{
  int bottom = first_nonopt;
  int middle = last_nonopt;
  int top = optind;
  char *tem;

  /* Exchange the shorter segment with the far end of the longer segment.
     That puts the shorter segment into the right place.
     It leaves the longer segment in the right place overall,
     but it consists of two parts that need to be swapped next.  */

#ifdef _LIBC
  /* First make sure the handling of the `__getopt_nonoption_flags'
     string can work normally.  Our top argument must be in the range
     of the string.  */
  if (nonoption_flags_len > 0 && top >= nonoption_flags_max_len)
    {
      /* We must extend the array.  The user plays games with us and
	 presents new arguments.  */
      char *new_str = malloc (top + 1);
      if (new_str == NULL)
	nonoption_flags_len = nonoption_flags_max_len = 0;
      else
	{
	  memset (__mempcpy (new_str, __getopt_nonoption_flags,
			     nonoption_flags_max_len),
		  '\0', top + 1 - nonoption_flags_max_len);
	  nonoption_flags_max_len = top + 1;
	  __getopt_nonoption_flags = new_str;
	}
    }
#endif

  while (top > middle && middle > bottom)
    {
      if (top - middle > middle - bottom)
	{
	  /* Bottom segment is the short one.  */
	  int len = middle - bottom;
	  register int i;

	  /* Swap it with the top part of the top segment.  */
	  for (i = 0; i < len; i++)
	    {
	      tem = argv[bottom + i];
	      argv[bottom + i] = argv[top - (middle - bottom) + i];
	      argv[top - (middle - bottom) + i] = tem;
	      SWAP_FLAGS (bottom + i, top - (middle - bottom) + i);
	    }
	  /* Exclude the moved bottom segment from further swapping.  */
	  top -= len;
	}
      else
	{
	  /* Top segment is the short one.  */
	  int len = top - middle;
	  register int i;

	  /* Swap it with the bottom part of the bottom segment.  */
	  for (i = 0; i < len; i++)
	    {
	      tem = argv[bottom + i];
	      argv[bottom + i] = argv[middle + i];
	      argv[middle + i] = tem;
	      SWAP_FLAGS (bottom + i, middle + i);
	    }
	  /* Exclude the moved top segment from further swapping.  */
	  bottom += len;
	}
    }

  /* Update records for the slots the non-options now occupy.  */

  first_nonopt += (optind - last_nonopt);
  last_nonopt = optind;
}

/* Initialize the internal data when the first call is made.  */

#if defined __STDC__ && __STDC__
static const char *_getopt_initialize (int, char *const *, const char *);
#endif
static const char *
_getopt_initialize (argc, argv, optstring)
     int argc;
     char *const *argv;
     const char *optstring;
{
  /* Start processing options with ARGV-element 1 (since ARGV-element 0
     is the program name); the sequence of previously skipped
     non-option ARGV-elements is empty.  */

  first_nonopt = last_nonopt = optind;

  nextchar = NULL;

  posixly_correct = getenv ("POSIXLY_CORRECT");

  /* Determine how to handle the ordering of options and nonoptions.  */

  if (optstring[0] == '-')
    {
      ordering = RETURN_IN_ORDER;
      ++optstring;
    }
  else if (optstring[0] == '+')
    {
      ordering = REQUIRE_ORDER;
      ++optstring;
    }
  else if (posixly_correct != NULL)
    ordering = REQUIRE_ORDER;
  else
    ordering = PERMUTE;

#ifdef _LIBC
  if (posixly_correct == NULL
      && argc == original_argc && argv == original_argv)
    {
      if (nonoption_flags_max_len == 0)
	{
	  if (__getopt_nonoption_flags == NULL
	      || __getopt_nonoption_flags[0] == '\0')
	    nonoption_flags_max_len = -1;
	  else
	    {
	      const char *orig_str = __getopt_nonoption_flags;
	      int len = nonoption_flags_max_len = strlen (orig_str);
	      if (nonoption_flags_max_len < argc)
		nonoption_flags_max_len = argc;
	      __getopt_nonoption_flags =
		(char *) malloc (nonoption_flags_max_len);
	      if (__getopt_nonoption_flags == NULL)
		nonoption_flags_max_len = -1;
	      else
		memset (__mempcpy (__getopt_nonoption_flags, orig_str, len),
			'\0', nonoption_flags_max_len - len);
	    }
	}
      nonoption_flags_len = nonoption_flags_max_len;
    }
  else
    nonoption_flags_len = 0;
#endif

  return optstring;
}

/* Scan elements of ARGV (whose length is ARGC) for option characters
   given in OPTSTRING.

   If an element of ARGV starts with '-', and is not exactly "-" or "--",
   then it is an option element.  The characters of this element
   (aside from the initial '-') are option characters.  If `getopt'
   is called repeatedly, it returns successively each of the option characters
   from each of the option elements.

   If `getopt' finds another option character, it returns that character,
   updating `optind' and `nextchar' so that the next call to `getopt' can
   resume the scan with the following option character or ARGV-element.

   If there are no more option characters, `getopt' returns -1.
   Then `optind' is the index in ARGV of the first ARGV-element
   that is not an option.  (The ARGV-elements have been permuted
   so that those that are not options now come last.)

   OPTSTRING is a string containing the legitimate option characters.
   If an option character is seen that is not listed in OPTSTRING,
   return '?' after printing an error message.  If you set `opterr' to
   zero, the error message is suppressed but we still return '?'.

   If a char in OPTSTRING is followed by a colon, that means it wants an arg,
   so the following text in the same ARGV-element, or the text of the following
   ARGV-element, is returned in `optarg'.  Two colons mean an option that
   wants an optional arg; if there is text in the current ARGV-element,
   it is returned in `optarg', otherwise `optarg' is set to zero.

   If OPTSTRING starts with `-' or `+', it requests different methods of
   handling the non-option ARGV-elements.
   See the comments about RETURN_IN_ORDER and REQUIRE_ORDER, above.

   Long-named options begin with `--' instead of `-'.
   Their names may be abbreviated as long as the abbreviation is unique
   or is an exact match for some defined option.  If they have an
   argument, it follows the option name in the same ARGV-element, separated
   from the option name by a `=', or else the in next ARGV-element.
   When `getopt' finds a long-named option, it returns 0 if that option's
   `flag' field is nonzero, the value of the option's `val' field
   if the `flag' field is zero.

   The elements of ARGV aren't really const, because we permute them.
   But we pretend they're const in the prototype to be compatible
   with other systems.

   LONGOPTS is a vector of `struct option' terminated by an
   element containing a name which is zero.

   LONGIND returns the index in LONGOPT of the long-named option found.
   It is only valid when a long-named option has been found by the most
   recent call.

   If LONG_ONLY is nonzero, '-' as well as '--' can introduce
   long-named options.  */

#undef stderr
#define stderr stdout
int
_getopt_internal (argc, argv, optstring, longopts, longind, long_only)
     int argc;
     char *const *argv;
     const char *optstring;
     const struct option *longopts;
     int *longind;
     int long_only;
{
  optarg = NULL;

  if (optind == 0 || !__getopt_initialized)
    {
      if (optind == 0)
	optind = 1;	/* Don't scan ARGV[0], the program name.  */
      optstring = _getopt_initialize (argc, argv, optstring);
      __getopt_initialized = 1;
    }

  /* Test whether ARGV[optind] points to a non-option argument.
     Either it does not have option syntax, or there is an environment flag
     from the shell indicating it is not an option.  The later information
     is only used when the used in the GNU libc.  */
#ifdef _LIBC
# define NONOPTION_P (argv[optind][0] != '-' || argv[optind][1] == '\0'	      \
		      || (optind < nonoption_flags_len			      \
			  && __getopt_nonoption_flags[optind] == '1'))
#else
# define NONOPTION_P (argv[optind][0] != '-' || argv[optind][1] == '\0')
#endif

  if (nextchar == NULL || *nextchar == '\0')
    {
      /* Advance to the next ARGV-element.  */

      /* Give FIRST_NONOPT & LAST_NONOPT rational values if OPTIND has been
	 moved back by the user (who may also have changed the arguments).  */
      if (last_nonopt > optind)
	last_nonopt = optind;
      if (first_nonopt > optind)
	first_nonopt = optind;

      if (ordering == PERMUTE)
	{
	  /* If we have just processed some options following some non-options,
	     exchange them so that the options come first.  */

	  if (first_nonopt != last_nonopt && last_nonopt != optind)
	    exchange ((char **) argv);
	  else if (last_nonopt != optind)
	    first_nonopt = optind;

	  /* Skip any additional non-options
	     and extend the range of non-options previously skipped.  */

	  while (optind < argc && NONOPTION_P)
	    optind++;
	  last_nonopt = optind;
	}

      /* The special ARGV-element `--' means premature end of options.
	 Skip it like a null option,
	 then exchange with previous non-options as if it were an option,
	 then skip everything else like a non-option.  */

      if (optind != argc && !strcmp (argv[optind], "--"))
	{
	  optind++;

	  if (first_nonopt != last_nonopt && last_nonopt != optind)
	    exchange ((char **) argv);
	  else if (first_nonopt == last_nonopt)
	    first_nonopt = optind;
	  last_nonopt = argc;

	  optind = argc;
	}

      /* If we have done all the ARGV-elements, stop the scan
	 and back over any non-options that we skipped and permuted.  */

      if (optind == argc)
	{
	  /* Set the next-arg-index to point at the non-options
	     that we previously skipped, so the caller will digest them.  */
	  if (first_nonopt != last_nonopt)
	    optind = first_nonopt;
	  return -1;
	}

      /* If we have come to a non-option and did not permute it,
	 either stop the scan or describe it to the caller and pass it by.  */

      if (NONOPTION_P)
	{
	  if (ordering == REQUIRE_ORDER)
	    return -1;
	  optarg = argv[optind++];
	  return 1;
	}

      /* We have found another option-ARGV-element.
	 Skip the initial punctuation.  */

      nextchar = (argv[optind] + 1
		  + (longopts != NULL && argv[optind][1] == '-'));
    }

  /* Decode the current option-ARGV-element.  */

  /* Check whether the ARGV-element is a long option.

     If long_only and the ARGV-element has the form "-f", where f is
     a valid short option, don't consider it an abbreviated form of
     a long option that starts with f.  Otherwise there would be no
     way to give the -f short option.

     On the other hand, if there's a long option "fubar" and
     the ARGV-element is "-fu", do consider that an abbreviation of
     the long option, just like "--fu", and not "-f" with arg "u".

     This distinction seems to be the most useful approach.  */

  if (longopts != NULL
      && (argv[optind][1] == '-'
	  || (long_only && (argv[optind][2] || !my_index (optstring, argv[optind][1])))))
    {
      char *nameend;
      const struct option *p;
      const struct option *pfound = NULL;
      int exact = 0;
      int ambig = 0;
      int indfound = -1;
      int option_index;

      for (nameend = nextchar; *nameend && *nameend != '='; nameend++)
	/* Do nothing.  */ ;

      /* Test all long options for either exact match
	 or abbreviated matches.  */
      for (p = longopts, option_index = 0; p->name; p++, option_index++)
	if (!strncmp (p->name, nextchar, nameend - nextchar))
	  {
	    if ((unsigned int) (nameend - nextchar)
		== (unsigned int) strlen (p->name))
	      {
		/* Exact match found.  */
		pfound = p;
		indfound = option_index;
		exact = 1;
		break;
	      }
	    else if (pfound == NULL)
	      {
		/* First nonexact match found.  */
		pfound = p;
		indfound = option_index;
	      }
	    else
	      /* Second or later nonexact match found.  */
	      ambig = 1;
	  }

      if (ambig && !exact)
	{
	  if (opterr)
	    fprintf (stderr, _("%s: option `%s' is ambiguous\n"),
		     argv[0], argv[optind]);
	  nextchar += strlen (nextchar);
	  optind++;
	  optopt = 0;
	  return '?';
	}

      if (pfound != NULL)
	{
	  option_index = indfound;
	  optind++;
	  if (*nameend)
	    {
	      /* Don't test has_arg with >, because some C compilers don't
		 allow it to be used on enums.  */
	      if (pfound->has_arg)
		optarg = nameend + 1;
	      else
		{
		  if (opterr)
		   if (argv[optind - 1][1] == '-')
		    /* --option */
		    fprintf (stderr,
		     _("%s: option `--%s' doesn't allow an argument\n"),
		     argv[0], pfound->name);
		   else
		    /* +option or -option */
		    fprintf (stderr,
		     _("%s: option `%c%s' doesn't allow an argument\n"),
		     argv[0], argv[optind - 1][0], pfound->name);

		  nextchar += strlen (nextchar);

		  optopt = pfound->val;
		  return '?';
		}
	    }
	  else if (pfound->has_arg == 1)
	    {
	      if (optind < argc)
		optarg = argv[optind++];
	      else
		{
		  if (opterr)
		    fprintf (stderr,
			   _("%s: option `%s' requires an argument\n"),
			   argv[0], argv[optind - 1]);
		  nextchar += strlen (nextchar);
		  optopt = pfound->val;
		  return optstring[0] == ':' ? ':' : '?';
		}
	    }
	  nextchar += strlen (nextchar);
	  if (longind != NULL)
	    *longind = option_index;
	  if (pfound->flag)
	    {
	      *(pfound->flag) = pfound->val;
	      return 0;
	    }
	  return pfound->val;
	}

      /* Can't find it as a long option.  If this is not getopt_long_only,
	 or the option starts with '--' or is not a valid short
	 option, then it's an error.
	 Otherwise interpret it as a short option.  */
      if (!long_only || argv[optind][1] == '-'
	  || my_index (optstring, *nextchar) == NULL)
	{
	  if (opterr)
	    {
	      if (argv[optind][1] == '-')
		/* --option */
		fprintf (stderr, _("%s: unrecognized option `--%s'\n"),
			 argv[0], nextchar);
	      else
		/* +option or -option */
		fprintf (stderr, _("%s: unrecognized option `%c%s'\n"),
			 argv[0], argv[optind][0], nextchar);
	    }
	  nextchar = (char *) "";
	  optind++;
	  optopt = 0;
	  return '?';
	}
    }

  /* Look at and handle the next short option-character.  */

  {
    char c = *nextchar++;
    char *temp = my_index (optstring, c);

    /* Increment `optind' when we start to process its last character.  */
    if (*nextchar == '\0')
      ++optind;

    if (temp == NULL || c == ':')
      {
	if (opterr)
	  {
	    if (posixly_correct)
	      /* 1003.2 specifies the format of this message.  */
	      fprintf (stderr, _("%s: illegal option -- %c\n"),
		       argv[0], c);
	    else
	      fprintf (stderr, _("%s: invalid option -- %c\n"),
		       argv[0], c);
	  }
	optopt = c;
	return '?';
      }
    /* Convenience. Treat POSIX -W foo same as long option --foo */
    if (temp[0] == 'W' && temp[1] == ';')
      {
	char *nameend;
	const struct option *p;
	const struct option *pfound = NULL;
	int exact = 0;
	int ambig = 0;
	int indfound = 0;
	int option_index;

	/* This is an option that requires an argument.  */
	if (*nextchar != '\0')
	  {
	    optarg = nextchar;
	    /* If we end this ARGV-element by taking the rest as an arg,
	       we must advance to the next element now.  */
	    optind++;
	  }
	else if (optind == argc)
	  {
	    if (opterr)
	      {
		/* 1003.2 specifies the format of this message.  */
		fprintf (stderr, _("%s: option requires an argument -- %c\n"),
			 argv[0], c);
	      }
	    optopt = c;
	    if (optstring[0] == ':')
	      c = ':';
	    else
	      c = '?';
	    return c;
	  }
	else
	  /* We already incremented `optind' once;
	     increment it again when taking next ARGV-elt as argument.  */
	  optarg = argv[optind++];

	/* optarg is now the argument, see if it's in the
	   table of longopts.  */

	for (nextchar = nameend = optarg; *nameend && *nameend != '='; nameend++)
	  /* Do nothing.  */ ;

	/* Test all long options for either exact match
	   or abbreviated matches.  */
	for (p = longopts, option_index = 0; p->name; p++, option_index++)
	  if (!strncmp (p->name, nextchar, nameend - nextchar))
	    {
	      if ((unsigned int) (nameend - nextchar) == strlen (p->name))
		{
		  /* Exact match found.  */
		  pfound = p;
		  indfound = option_index;
		  exact = 1;
		  break;
		}
	      else if (pfound == NULL)
		{
		  /* First nonexact match found.  */
		  pfound = p;
		  indfound = option_index;
		}
	      else
		/* Second or later nonexact match found.  */
		ambig = 1;
	    }
	if (ambig && !exact)
	  {
	    if (opterr)
	      fprintf (stderr, _("%s: option `-W %s' is ambiguous\n"),
		       argv[0], argv[optind]);
	    nextchar += strlen (nextchar);
	    optind++;
	    return '?';
	  }
	if (pfound != NULL)
	  {
	    option_index = indfound;
	    if (*nameend)
	      {
		/* Don't test has_arg with >, because some C compilers don't
		   allow it to be used on enums.  */
		if (pfound->has_arg)
		  optarg = nameend + 1;
		else
		  {
		    if (opterr)
		      fprintf (stderr, _("\
%s: option `-W %s' doesn't allow an argument\n"),
			       argv[0], pfound->name);

		    nextchar += strlen (nextchar);
		    return '?';
		  }
	      }
	    else if (pfound->has_arg == 1)
	      {
		if (optind < argc)
		  optarg = argv[optind++];
		else
		  {
		    if (opterr)
		      fprintf (stderr,
			       _("%s: option `%s' requires an argument\n"),
			       argv[0], argv[optind - 1]);
		    nextchar += strlen (nextchar);
		    return optstring[0] == ':' ? ':' : '?';
		  }
	      }
	    nextchar += strlen (nextchar);
	    if (longind != NULL)
	      *longind = option_index;
	    if (pfound->flag)
	      {
		*(pfound->flag) = pfound->val;
		return 0;
	      }
	    return pfound->val;
	  }
	  nextchar = NULL;
	  return 'W';	/* Let the application handle it.   */
      }
    if (temp[1] == ':')
      {
	if (temp[2] == ':')
	  {
	    /* This is an option that accepts an argument optionally.  */
	    if (*nextchar != '\0')
	      {
		optarg = nextchar;
		optind++;
	      }
	    else
	      optarg = NULL;
	    nextchar = NULL;
	  }
	else
	  {
	    /* This is an option that requires an argument.  */
	    if (*nextchar != '\0')
	      {
		optarg = nextchar;
		/* If we end this ARGV-element by taking the rest as an arg,
		   we must advance to the next element now.  */
		optind++;
	      }
	    else if (optind == argc)
	      {
		if (opterr)
		  {
		    /* 1003.2 specifies the format of this message.  */
		    fprintf (stderr,
			   _("%s: option requires an argument -- %c\n"),
			   argv[0], c);
		  }
		optopt = c;
		if (optstring[0] == ':')
		  c = ':';
		else
		  c = '?';
	      }
	    else
	      /* We already incremented `optind' once;
		 increment it again when taking next ARGV-elt as argument.  */
	      optarg = argv[optind++];
	    nextchar = NULL;
	  }
      }
    return c;
  }
}

int
getopt (argc, argv, optstring)
     int argc;
     char *const *argv;
     const char *optstring;
{
  return _getopt_internal (argc, argv, optstring,
			   (const struct option *) 0,
			   (int *) 0,
			   0);
}

#endif	/* Not ELIDE_CODE.  */

#ifdef TEST

/* Compile with -DTEST to make an executable for use in testing
   the above definition of `getopt'.  */

int
main (argc, argv)
     int argc;
     char **argv;
{
  int c;
  int digit_optind = 0;

  while (1)
    {
      int this_option_optind = optind ? optind : 1;

      c = getopt (argc, argv, "abc:d:0123456789");
      if (c == -1)
	break;

      switch (c)
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  if (digit_optind != 0 && digit_optind != this_option_optind)
	    printf ("digits occur in two different argv-elements.\n");
	  digit_optind = this_option_optind;
	  printf ("option %c\n", c);
	  break;

	case 'a':
	  printf ("option a\n");
	  break;

	case 'b':
	  printf ("option b\n");
	  break;

	case 'c':
	  printf ("option c with value `%s'\n", optarg);
	  break;

	case '?':
	  break;

	default:
	  printf ("?? getopt returned character code 0%o ??\n", c);
	}
    }

  if (optind < argc)
    {
      printf ("non-option ARGV-elements: ");
      while (optind < argc)
	printf ("%s ", argv[optind++]);
      printf ("\n");
    }

  exit (0);
}

#endif /* TEST */
/* getopt_long and getopt_long_only entry points for GNU getopt.
   Copyright (C) 1987,88,89,90,91,92,93,94,96,97,98
     Free Software Foundation, Inc.

   NOTE: The canonical source of this file is maintained with the GNU C Library.
   Bugs can be reported to bug-glibc@gnu.org.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "getopt.h"

#if !defined __STDC__ || !__STDC__
/* This is a separate conditional since some stdc systems
   reject `defined (const)'.  */
#ifndef const
#define const
#endif
#endif

#include <stdio.h>

/* Comment out all this code if we are using the GNU C Library, and are not
   actually compiling the library itself.  This code is part of the GNU C
   Library, but also included in many other GNU distributions.  Compiling
   and linking in this code is a waste when using the GNU C library
   (especially if it is a shared library).  Rather than having every GNU
   program understand `configure --with-gnu-libc' and omit the object files,
   it is simpler to just do this in the source for each such file.  */

#define GETOPT_INTERFACE_VERSION 2
#if !defined _LIBC && defined __GLIBC__ && __GLIBC__ >= 2
#include <gnu-versions.h>
#if _GNU_GETOPT_INTERFACE_VERSION == GETOPT_INTERFACE_VERSION
#define ELIDE_CODE
#endif
#endif

#ifndef ELIDE_CODE


/* This needs to come after some library #include
   to get __GNU_LIBRARY__ defined.  */
#ifdef __GNU_LIBRARY__
#include <stdlib.h>
#endif

#ifndef	NULL
#define NULL 0
#endif

int
getopt_long (argc, argv, options, long_options, opt_index)
     int argc;
     char *const *argv;
     const char *options;
     const struct option *long_options;
     int *opt_index;
{
  return _getopt_internal (argc, argv, options, long_options, opt_index, 0);
}

/* Like getopt_long, but '-' as well as '--' can indicate a long option.
   If an option that starts with '-' (not '--') doesn't match a long option,
   but does match a short option, it is parsed as a short option
   instead.  */

int
getopt_long_only (argc, argv, options, long_options, opt_index)
     int argc;
     char *const *argv;
     const char *options;
     const struct option *long_options;
     int *opt_index;
{
  return _getopt_internal (argc, argv, options, long_options, opt_index, 1);
}


#endif	/* Not ELIDE_CODE.  */

#ifdef TEST

#include <stdio.h>

int
main (argc, argv)
     int argc;
     char **argv;
{
  int c;
  int digit_optind = 0;

  while (1)
    {
      int this_option_optind = optind ? optind : 1;
      int option_index = 0;
      static struct option long_options[] =
      {
	{"add", 1, 0, 0},
	{"append", 0, 0, 0},
	{"delete", 1, 0, 0},
	{"verbose", 0, 0, 0},
	{"create", 0, 0, 0},
	{"file", 1, 0, 0},
	{0, 0, 0, 0}
      };

      c = getopt_long (argc, argv, "abc:d:0123456789",
		       long_options, &option_index);
      if (c == -1)
	break;

      switch (c)
	{
	case 0:
	  printf ("option %s", long_options[option_index].name);
	  if (optarg)
	    printf (" with arg %s", optarg);
	  printf ("\n");
	  break;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  if (digit_optind != 0 && digit_optind != this_option_optind)
	    printf ("digits occur in two different argv-elements.\n");
	  digit_optind = this_option_optind;
	  printf ("option %c\n", c);
	  break;

	case 'a':
	  printf ("option a\n");
	  break;

	case 'b':
	  printf ("option b\n");
	  break;

	case 'c':
	  printf ("option c with value `%s'\n", optarg);
	  break;

	case 'd':
	  printf ("option d with value `%s'\n", optarg);
	  break;

	case '?':
	  break;

	default:
	  printf ("?? getopt returned character code 0%o ??\n", c);
	}
    }

  if (optind < argc)
    {
      printf ("non-option ARGV-elements: ");
      while (optind < argc)
	printf ("%s ", argv[optind++]);
      printf ("\n");
    }

  exit (0);
}

#endif /* TEST */


/* Extended regular expression matching and search library.
   Copyright (C) 2002 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Isamu Hasegawa <isamu@yamato.ibm.com>.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifdef _LIBC
/* We have to keep the namespace clean.  */
#  define regfree(preg) __regfree (preg)
#  define regexec(pr, st, nm, pm, ef) __regexec (pr, st, nm, pm, ef)
#  define regcomp(preg, pattern, cflags) __regcomp (preg, pattern, cflags)
#  define regerror(errcode, preg, errbuf, errbuf_size) \
	__regerror(errcode, preg, errbuf, errbuf_size)
#  define re_set_registers(bu, re, nu, st, en) \
	__re_set_registers (bu, re, nu, st, en)
#  define re_match_2(bufp, string1, size1, string2, size2, pos, regs, stop) \
	__re_match_2 (bufp, string1, size1, string2, size2, pos, regs, stop)
#  define re_match(bufp, string, size, pos, regs) \
	__re_match (bufp, string, size, pos, regs)
#  define re_search(bufp, string, size, startpos, range, regs) \
	__re_search (bufp, string, size, startpos, range, regs)
#  define re_compile_pattern(pattern, length, bufp) \
	__re_compile_pattern (pattern, length, bufp)
#  define re_set_syntax(syntax) __re_set_syntax (syntax)
#  define re_search_2(bufp, st1, s1, st2, s2, startpos, range, regs, stop) \
	__re_search_2 (bufp, st1, s1, st2, s2, startpos, range, regs, stop)
#  define re_compile_fastmap(bufp) __re_compile_fastmap (bufp)
#endif

/* POSIX says that <sys/types.h> must be included (by the caller) before
   <regex.h>.  */
#include <sys/types.h>
#include <regex.h>
#include "regex_internal.h"

#include "regex_internal.c"
#include "regcomp.c"
#include "regexec.c"

/* Binary backward compatibility.  */
#if _LIBC
# include <shlib-compat.h>
# if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_3)
link_warning (re_max_failures, "the 're_max_failures' variable is obsolete and will go away.")
int re_max_failures = 2000;
# endif
#endif


/*  GNU SED, a batch stream editor.
    Copyright (C) 1999, 2002, 2003 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "config.h"

#include <ctype.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifndef NULL
# include <stdio.h>
#endif

#ifndef NULL
# define NULL CAST(VOID *)0
#endif

extern flagT use_extended_syntax_p;

/* 
   Changed variable name: errors[] to regex_errors[]
   to allow *.c file merging for SIR -- July, 2006
   by Kyle R. Murphy
*/
static const char regex_errors[] =
  "No previous regular expression\0"
  "Cannot specify modifiers on empty regexp";

#define NO_REGEX (regex_errors)
#define BAD_MODIF (NO_REGEX + sizeof(N_("No previous regular expression")))

/*
  Added pre-proc. conditional to allow *.c file
  merging for SIR -- July, 2006 by Kyle R. Murphy
*/
#ifndef END_ERRORS
#define END_ERRORS (BAD_MODIF + sizeof(N_("Cannot specify modifiers on empty regexp")))
#endif


regex_t *
compile_regex(b, flags, needed_sub)
  struct buffer *b;
  int flags;
  int needed_sub;
{
  regex_t *new_regex;

  char *last_re = NULL;
  size_t last_re_len;

  /* My reading of IEEE Std 1003.2-1992 is that // means the empty RE.
     But historical and common practice is that // "matches the last RE";
     thus this use of POSIXLY_CORRECT. */
  if (size_buffer(b) == 0 && !POSIXLY_CORRECT)
    {
      if (flags > 0)
	bad_prog(_(BAD_MODIF));
      return NULL;
    }

  last_re_len = size_buffer(b);
  last_re = ck_memdup(get_buffer(b), last_re_len);

  new_regex = MALLOC(1, regex_t);

#ifdef REG_PERL
  {
    int errcode;
    errcode = regncomp(new_regex, last_re, last_re_len,
		       (needed_sub ? 0 : REG_NOSUB)
		       | flags
		       | extended_regexp_flags);

    if (errcode)
      {
        char errorbuf[200];
        regerror(errcode, NULL, errorbuf, 200);
        bad_prog(gettext(errorbuf));
      }
  }
#else
  {
    const char *error;
    int syntax = ((extended_regexp_flags & REG_EXTENDED)
		   ? RE_SYNTAX_POSIX_EXTENDED
                   : RE_SYNTAX_POSIX_BASIC)
		   & ~RE_UNMATCHED_RIGHT_PAREN_ORD;

    syntax |= RE_NO_POSIX_BACKTRACKING;
#ifdef RE_ICASE
    syntax |= (flags & REG_ICASE) ? RE_ICASE : 0;
#endif

    /* If REG_NEWLINE is set, newlines are treated differently.  */
    if (flags & REG_NEWLINE)
      { /* REG_NEWLINE implies neither . nor [^...] match newline.  */
        syntax &= ~RE_DOT_NEWLINE;
        syntax |= RE_HAT_LISTS_NOT_NEWLINE;
      }

    /* Only PCRE processes \t & co. */
    last_re_len = normalize_text(last_re, last_re_len);
    re_set_syntax (syntax);
    error = re_compile_pattern (last_re, last_re_len, new_regex);
    new_regex->newline_anchor = (flags & REG_NEWLINE) != 0;

    new_regex->translate = NULL;
#ifndef RE_ICASE
    if (flags & REG_ICASE)
      {
        static char translate[1 << (sizeof(char) * 8)];
	int i;
	for (i = 0; i < sizeof(translate) / sizeof(char); i++)
	  translate[i] = tolower (i);

        new_regex->translate = translate;
      }
#endif

    if (error)
      bad_prog(error);
  }
#endif

  FREE(last_re);

  /* Just to be sure, I mark this as not POSIXLY_CORRECT behavior */
  if (new_regex->re_nsub < needed_sub && !POSIXLY_CORRECT)
    {
      char buf[200];
      sprintf(buf, _("Invalid reference \\%d on `s' command's RHS"),
	      needed_sub);
      bad_prog(buf);
    }

  return new_regex;
}

#ifdef REG_PERL
static void
copy_regs (regs, pmatch, nregs)
     struct re_registers *regs;
     regmatch_t *pmatch;
     int nregs;
{
  int i;
  int need_regs = nregs + 1;
  /* We need one extra element beyond `num_regs' for the `-1' marker GNU code
     uses.  */

  /* Have the register data arrays been allocated?  */
  if (!regs->start)
    { /* No.  So allocate them with malloc.  */
      regs->start = MALLOC (need_regs, regoff_t);
      regs->end = MALLOC (need_regs, regoff_t);
      regs->num_regs = need_regs;
    }
  else if (need_regs > regs->num_regs)
    { /* Yes.  We also need more elements than were already
         allocated, so reallocate them.  */
      regs->start = REALLOC (regs->start, need_regs, regoff_t);
      regs->end = REALLOC (regs->end, need_regs, regoff_t);
      regs->num_regs = need_regs;
    }

  /* Copy the regs.  */
  for (i = 0; i < nregs; ++i)
    {
      regs->start[i] = pmatch[i].rm_so;
      regs->end[i] = pmatch[i].rm_eo;
    }
  for ( ; i < regs->num_regs; ++i)
    regs->start[i] = regs->end[i] = -1;
}
#endif

int
match_regex(regex, buf, buflen, buf_start_offset, regarray, regsize)
  regex_t *regex;
  char *buf;
  size_t buflen;
  size_t buf_start_offset;
  struct re_registers *regarray;
  int regsize;
{
  static regex_t *regex_last;
#ifdef REG_PERL
  regmatch_t *regmatch;
  if (regsize)
    regmatch = (regmatch_t *) alloca (sizeof (regmatch_t) * regsize);
  else
    regmatch = NULL;
#endif

  int ret;

  /* Keep track of the last regexp matched. */
  if (!regex)
    {
      regex = regex_last;
      if (!regex_last)
	bad_prog(_(NO_REGEX));
    }
  else
    regex_last = regex;

#ifdef REG_PERL
  ret = regexec2(regex, 
		 buf, CAST(int)buflen, CAST(int)buf_start_offset,
		 regsize, regmatch, 0);

  if (regsize)
    copy_regs (regarray, regmatch, regsize);

  return (ret == 0);
#else
  ret = re_search(regex, buf, buflen, buf_start_offset,
		  buflen - buf_start_offset,
		  regsize ? regarray : NULL);

  return (ret > -1);
#endif
}


#ifdef DEBUG_LEAKS
void
release_regex(regex)
  regex_t *regex;
{
  regfree(regex);
  FREE(regex);
}
#endif /*DEBUG_LEAKS*/

