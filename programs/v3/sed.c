#define COPYRIGHT_NOTICE "Copyright (C) 1998 Free Software Foundation, Inc."
#define BUG_ADDRESS "bug-gnu-utils@gnu.org"

/*  GNU SED, a batch stream editor.
    Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1998 \
    Free Software Foundation, Inc."

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

#undef stderr
#define stderr stdout

#ifndef HAVE_STRING_H
# include <strings.h>
#else
# include <string.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_MMAP
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
# include <sys/types.h>
# include <sys/mman.h>
# ifndef MAP_FAILED
#  define MAP_FAILED	((char *)-1)	/* what a stupid concept... */
# endif
# include <sys/stat.h>
# ifndef S_ISREG
#  define S_ISREG(m)	(((m)&S_IFMT) == S_IFMT)
# endif
#endif /* HAVE_MMAP */

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include "regex-sed.h"
#include "getopt.h"
#include "basicdefs.h"
#include "utils.h"
#include "sed.h"

#ifndef HAVE_STDLIB_H
 extern char *getenv P_((const char *));
#endif

#ifdef STUB_FROM_RX_LIBRARY_USAGE
extern void print_rexp P_((void));
extern int rx_basic_unfaniverse_delay;
#endif /*STUB_FROM_RX_LIBRARY_USAGE*/

static void usage P_((int));

flagT use_extended_syntax_p = 0;

flagT rx_testing = 0;

/* If set, don't write out the line unless explicitly told to */
flagT no_default_output = 0;

/* Do we need to be pedantically POSIX compliant? */
flagT POSIXLY_CORRECT;

static void
usage(status)
  int status;
{
  FILE *out = status ? stderr : stdout;

  fprintf(out, "\
Usage: %s [OPTION]... {script-only-if-no-other-script} [input-file]...\n\
\n\
  -n, --quiet, --silent\n\
                 suppress automatic printing of pattern space\n\
  -e script, --expression=script\n\
                 add the script to the commands to be executed\n\
  -f script-file, --file=script-file\n\
                 add the contents of script-file to the commands to be executed\n\
      --help     display this help and exit\n\
  -V, --version  output version information and exit\n\
\n\
If no -e, --expression, -f, or --file option is given, then the first\n\
non-option argument is taken as the sed script to interpret.  All\n\
remaining arguments are names of input files; if no input files are\n\
specified, then the standard input is read.\n\
\n", myname);
  fprintf(out, "E-mail bug reports to: %s .\n\
Be sure to include the word ``%s'' somewhere in the ``Subject:'' field.\n",
	  BUG_ADDRESS, PACKAGE);
  exit(status);
}

int
main(argc, argv)
  int argc;
  char **argv;
{
  static struct option longopts[] = {
    {"rxtest", 0, NULL, 'r'},
    {"expression", 1, NULL, 'e'},
    {"file", 1, NULL, 'f'},
    {"quiet", 0, NULL, 'n'},
    {"silent", 0, NULL, 'n'},
    {"version", 0, NULL, 'V'},
    {"help", 0, NULL, 'h'},
    {NULL, 0, NULL, 0}
  };

  /* The complete compiled SED program that we are going to run: */
  struct vector *the_program = NULL;
  int opt;
  flagT bad_input;	/* If this variable is non-zero at exit, one or
			   more of the input files couldn't be opened. */

  POSIXLY_CORRECT = (getenv("POSIXLY_CORRECT") != NULL);
#ifdef STUB_FROM_RX_LIBRARY_USAGE
  if (!rx_default_cache)
    print_rexp();
  /* Allow roughly a megabyte of cache space. */
  rx_default_cache->bytes_allowed = 1<<20;
  rx_basic_unfaniverse_delay = 512 * 4;
#endif /*STUB_FROM_RX_LIBRARY_USAGE*/
 /* Commented by Amit Goel on 30th August 2001 to remove Nondeterminism */
 /* myname = *argv; */
 /* Added By Amit Goel on 30th August 2001 */
    myname = "Executable";
  while ((opt = getopt_long(argc, argv, "hnrVe:f:", longopts, NULL)) != EOF)
    {
      switch (opt)
	{
	case 'n':
	  no_default_output = 1;
	  break;
	case 'e':
	  the_program = compile_string(the_program, optarg);
	  break;
	case 'f':
	  the_program = compile_file(the_program, optarg);
	  break;

	case 'r':
	  use_extended_syntax_p = 1;
	  rx_testing = 1;
	  break;
	case 'V':
	  fprintf(stdout, "GNU %s version %s\n\n", PACKAGE, VERSION);
	  fprintf(stdout, "%s\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE,\n\
to the extent permitted by law.\n\
", COPYRIGHT_NOTICE);
	  exit(0);
	case 'h':
	  usage(0);
	default:
	  usage(4);
	}
    }

  if (!the_program)
    {
      if (optind < argc)
	the_program = compile_string(the_program, argv[optind++]);
      else
	usage(4);
    }
  check_final_program(the_program);

  bad_input = process_files(the_program, argv+optind);

  close_all_files();
  ck_fclose(stdout);
  if (bad_input)
    exit(2);
  return 0;
}


/* Attempt to mmap() a file.  On failure, just return a zero,
   otherwise set *base and *len and return non-zero.  */
flagT
map_file(fp, base, len)
  FILE *fp;
  VOID **base;
  size_t *len;
{
#ifdef HAVE_MMAP
  struct stat s;
  VOID *nbase;

  if (fstat(fileno(fp), &s) == 0
      && S_ISREG(s.st_mode)
      && s.st_size == CAST(size_t)s.st_size)
    {
      /* "As if" the whole file was read into memory at this moment... */
      nbase = VCAST(VOID *)mmap(NULL, CAST(size_t)s.st_size, PROT_READ,
				MAP_PRIVATE, fileno(fp), CAST(off_t)0);
      if (nbase != MAP_FAILED)
	{
	  *base = nbase;
	  *len =  s.st_size;
	  return 1;
	}
    }
#endif /* HAVE_MMAP */

  return 0;
}

/* Attempt to munmap() a memory region. */
void
unmap_file(base, len)
  VOID *base;
  size_t len;
{
#ifdef HAVE_MMAP
  if (base)
    munmap(VCAST(caddr_t)base, len);
#endif
}
/*  GNU SED, a batch stream editor.
    Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1998
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

#ifndef __STRICT_ANSI__
# define _GNU_SOURCE
#endif
#include <ctype.h>

#ifndef HAVE_STRING_H
# include <strings.h>
# ifdef HAVE_MEMORY_H
#  include <memory.h>
# endif
#else
# include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include "regex-sed.h"
#include "basicdefs.h"
#include "utils.h"
#include "sed.h"


extern flagT rx_testing;
extern flagT no_default_output;
extern flagT use_extended_syntax_p;


#define YMAP_LENGTH		256 /*XXX shouldn't this be (UCHAR_MAX+1)?*/
#define VECTOR_ALLOC_INCREMENT	40

struct prog_info {
  /* When we're reading a script command from a string, 'prog.base'
     points to the first character in the string, 'prog.cur' points
     to the current character in the string, and 'prog.end' points
     to the end of the string.  This allows us to compile script
     strings that contain nulls, which might happen if we're mmap'ing
     a file. */
  VOID *base;
  const unsigned char *cur;
  const unsigned char *end;

  /* This is the current script file.  If it is NULL, we are reading
     from a string stored at 'prog.cur' instead.  If both 'prog.file'
     and 'prog.cur' are NULL, we're in trouble! */
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


static struct vector *compile_program P_((struct vector *));
static struct vector *new_vector
	P_((struct error_info *errinfo, struct vector *old_vector));
static void read_text P_((struct text_buf *buf, int leadin_ch));
static struct sed_cmd *next_cmd_entry P_((struct vector **vectorp));
static int add_then_next P_((struct buffer *b, int ch));
static int snarf_char_class P_((struct buffer *b));
static struct buffer *match_slash P_((int slash, flagT regex, flagT keep_back));
static regex_t *compile_regex P_((struct buffer *b, flagT icase, flagT nosub));
static flagT compile_address P_((struct addr *addr, int ch));
static void compile_filename P_((flagT readit, const char **name, FILE **fp));
static struct sed_label *setup_jump
	P_((struct sed_label *list, struct sed_cmd *cmd, struct vector *vec));
static flagT mark_subst_opts P_((struct subst *cmd));
static int inchar P_((void));
static int in_nonblank P_((void));
static countT in_integer P_((int ch));
static void savchar P_((int ch));
static void bad_prog P_((const char *why));


/* Where we are in the processing of the input. */
static struct prog_info prog;
static struct error_info cur_input;

/* Information about labels and jumps-to-labels.  This is used to do
   the required backpatching after we have compiled all the scripts. */
static struct sed_label *jumps = NULL;
static struct sed_label *labels = NULL;

/* We wish to detect #n magic only in the first input argument;
   this flag tracks when we have consumed the first file of input. */
static flagT first_script = 1;

/* Allow for scripts like "sed -e 'i\' -e foo": */
static struct buffer *pending_text = NULL;
static struct text_buf *old_text_buf = NULL;

/* Various error messages we may want to print */
static const char ONE_ADDR[] = "Command only uses one address";
static const char NO_ADDR[] = "Command doesn't take any addresses";
static const char LINE_JUNK[] = "Extra characters after command";
static const char BAD_EOF[] = "Unexpected End-of-file";
static const char BAD_REGEX_FMT[] = "bad regexp: %s\n";
static const char EXCESS_OPEN_BRACKET[] = "Unmatched `{'";
static const char EXCESS_CLOSE_BRACKET[] = "Unexpected `}'";
static const char NO_REGEX[] = "No previous regular expression";
static const char NO_COMMAND[] = "Missing command";
static const char UNTERM_S_CMD[] = "Unterminated `s' command";
static const char UNTERM_Y_CMD[] = "Unterminated `y' command";


/* This structure tracks files opened by the 'r', 'w', and 's///w' commands
   so that they may all be closed cleanly at normal program termination.  */
struct fp_list {
    char *name;
    FILE *fp;
    flagT readit_p;
    struct fp_list *link;
  };
static struct fp_list *file_ptrs = NULL;

void
close_all_files()
{
  struct fp_list *p, *q;

  for (p=file_ptrs; p; p=q)
    {
      if (p->fp)
	ck_fclose(p->fp);
      FREE(p->name);
      q = p->link;
      FREE(p);
    }
  file_ptrs = NULL;
}

/* 'str' is a string (from the command line) that contains a sed command.
   Compile the command, and add it to the end of 'cur_program'. */
struct vector *
compile_string(cur_program, str)
  struct vector *cur_program;
  char *str;
{
  static countT string_expr_count = 0;
  struct vector *ret;

  prog.file = NULL;
  prog.base = VCAST(VOID *)str;
  prog.cur = CAST(unsigned char *)str;
  prog.end = prog.cur + strlen(str);

  cur_input.line = 0;
  cur_input.name = NULL;
  cur_input.string_expr_count = ++string_expr_count;

  ret = compile_program(cur_program);
  prog.base = NULL;
  prog.cur = NULL;
  prog.end = NULL;

  first_script = 0;
  return ret;
}

/* 'cmdfile' is the name of a file containing sed commands.
   Read them in and add them to the end of 'cur_program'.
 */
struct vector *
compile_file(cur_program, cmdfile)
  struct vector *cur_program;
  const char *cmdfile;
{
  size_t len;
  struct vector *ret;

  prog.base = NULL;
  prog.cur = NULL;
  prog.end = NULL;
  prog.file = stdin;
  if (cmdfile[0] != '-' || cmdfile[1] != '\0')
    prog.file = ck_fopen(cmdfile, "r");
  if (map_file(prog.file, &prog.base, &len))
    {
      prog.cur = VCAST(const unsigned char *)prog.base;
      prog.end = prog.cur + len;
    }

  cur_input.line = 1;
  cur_input.name = cmdfile;
  cur_input.string_expr_count = 0;
   
  ret = compile_program(cur_program);
  if (prog.base)
    {
      unmap_file(prog.base, len);
      prog.base = NULL;
      prog.cur = NULL;
      prog.end = NULL;
    }
  if (prog.file != stdin)
    ck_fclose(prog.file);
  prog.file = NULL;

  first_script = 0;
  return ret;
}

/* Make any checks which require the whole program to have been read.
   In particular: this backpatches the jump targets.
   Any cleanup which can be done after these checks is done here also.  */
void
check_final_program(program)
  struct vector *program;
{
  struct sed_label *go;
  struct sed_label *lbl;
  struct sed_label *nxt;

  /* do all "{"s have a corresponding "}"? */
  if (program->return_v)
    {
      cur_input = *program->return_v->err_info; /* for error reporting */
      bad_prog(EXCESS_OPEN_BRACKET);
    }
  FREE(program->err_info);	/* no longer need this */
  program->err_info = NULL;

  /* was the final command an unterminated a/c/i command? */
  if (pending_text)
    {
      old_text_buf->text_len = size_buffer(pending_text);
      old_text_buf->text = MEMDUP(get_buffer(pending_text),
				  old_text_buf->text_len, char);
      free_buffer(pending_text);
      pending_text = NULL;
    }

  for (go = jumps; go; go = nxt)
    {
      for (lbl = labels; lbl; lbl = lbl->next)
	if (strcmp(lbl->name, go->name) == 0)
	  break;
      if (*go->name && !lbl)
	panic("Can't find label for jump to '%s'", go->name);
      go->v->v[go->v_index].x.jump = lbl;
      nxt = go->next;
      FREE(go->name);
      FREE(go);
    }
  jumps = NULL;

  for (lbl = labels; lbl; lbl = lbl->next)
    {
      FREE(lbl->name);
      lbl->name = NULL;
    }
}

static struct vector *
new_vector(errinfo, old_vector)
  struct error_info *errinfo;
  struct vector *old_vector;
{
  struct vector *vector = MALLOC(1, struct vector);
  vector->v = NULL;
  vector->v_allocated = 0;
  vector->v_length = 0;
  vector->err_info = MEMDUP(errinfo, 1, struct error_info);
  vector->return_v = old_vector;
  vector->return_i = old_vector ? old_vector->v_length : 0;
  return vector;
}

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
      buf->text_len = 0;
      old_text_buf = buf;
    }
  /* assert(old_text_buf != NULL); */

  if (leadin_ch == EOF)
    return;
  while ((ch = inchar()) != EOF && ch != '\n')
    {
      if (ch == '\\')
	{
	  if ((ch = inchar()) == EOF)
	    {
	      add1_buffer(pending_text, '\n');
	      return;
	    }
	}
      add1_buffer(pending_text, ch);
    }
  add1_buffer(pending_text, '\n');

  if (!buf)
    buf = old_text_buf;
  buf->text_len = size_buffer(pending_text);
  buf->text = MEMDUP(get_buffer(pending_text), buf->text_len, char);
  free_buffer(pending_text);
  pending_text = NULL;
}


/* Read a program (or a subprogram within '{' '}' pairs) in and store
   the compiled form in *'vector'.  Return a pointer to the new vector.  */
static struct vector *
compile_program(vector)
  struct vector *vector;
{
  struct sed_cmd *cur_cmd;
  struct buffer *b;
  struct buffer *b2;
  unsigned char *ustring;
  size_t len;
  int slash;
  int ch;

  if (!vector)
    vector = new_vector(&cur_input, NULL);
  if (pending_text)
    read_text(NULL, '\n');

  for (;;)
    {
      while ((ch=inchar()) == ';' || ISSPACE(ch))
	;
      if (ch == EOF)
	break;

      cur_cmd = next_cmd_entry(&vector);
      if (compile_address(&cur_cmd->a1, ch))
	{
	  ch = in_nonblank();
	  if (ch == ',')
	    {
	      if (!compile_address(&cur_cmd->a2, in_nonblank()))
		bad_prog("Unexpected ','");
	      ch = in_nonblank();
	    }
	}
      if (cur_cmd->a2.addr_type == addr_is_num
	  && cur_cmd->a1.addr_type == addr_is_num
	  && cur_cmd->a2.a.addr_number < cur_cmd->a1.a.addr_number)
	cur_cmd->a2.a.addr_number = cur_cmd->a1.a.addr_number;
      if (ch == '!')
	{
	  cur_cmd->addr_bang = 1;
	  ch = in_nonblank();
	  if (ch == '!')
	    bad_prog("Multiple '!'s");
	}

      cur_cmd->cmd = ch;
      switch (ch)
	{
	case '#':
	  if (cur_cmd->a1.addr_type != addr_is_null)
	    bad_prog(NO_ADDR);
	  ch = inchar();
	  if (ch=='n' && first_script && cur_input.line < 2)
	    if (   (prog.base && prog.cur==2+CAST(unsigned char *)prog.base)
	        || (prog.file && !prog.base && 2==ftell(prog.file)))
	      no_default_output = 1;
	  while (ch != EOF && ch != '\n')
	    ch = inchar();
	  continue;	/* restart the for (;;) loop */

	case '{':
	  ++vector->v_length;	/* be sure to count this insn */
	  vector = cur_cmd->x.sub = new_vector(&cur_input, vector);
	  continue;	/* restart the for (;;) loop */

	case '}':
	  /* a return insn for subprograms -t */
	  if (!vector->return_v)
	    bad_prog(EXCESS_CLOSE_BRACKET);
	  if (cur_cmd->a1.addr_type != addr_is_null)
	    bad_prog(/*{*/ "} doesn't want any addresses");
	  ch = in_nonblank();
	  if (ch != EOF && ch != '\n' && ch != ';')
	    bad_prog(LINE_JUNK);
	  ++vector->v_length;		/* we haven't counted this insn yet */
	  FREE(vector->err_info);	/* no longer need this */
	  vector->err_info = NULL;
	  vector = vector->return_v;
	  continue;	/* restart the for (;;) loop */

	case 'a':
	case 'i':
	  if (cur_cmd->a2.addr_type != addr_is_null)
	    bad_prog(ONE_ADDR);
	  /* Fall Through */
	case 'c':
	  ch = in_nonblank();
	  if (ch != '\\')
	    bad_prog(LINE_JUNK);
	  ch = inchar();
	  if (ch != '\n' && ch != EOF)
	    bad_prog(LINE_JUNK);
	  read_text(&cur_cmd->x.cmd_txt, ch);
	  break;

	case ':':
	  if (cur_cmd->a1.addr_type != addr_is_null)
	    bad_prog(": doesn't want any addresses");
	  labels = setup_jump(labels, cur_cmd, vector);
	  break;
	case 'b':
	case 't':
	  jumps = setup_jump(jumps, cur_cmd, vector);
	  break;

	case 'q':
	case '=':
	  if (cur_cmd->a2.addr_type != addr_is_null)
	    bad_prog(ONE_ADDR);
	  /* Fall Through */
	case 'd':
	case 'D':
	case 'g':
	case 'G':
	case 'h':
	case 'H':
	case 'l':
	case 'n':
	case 'N':
	case 'p':
	case 'P':
	case 'x':
	  ch = in_nonblank();
	  if (ch != EOF && ch != '\n' && ch != ';')
	    bad_prog(LINE_JUNK);
	  break;

	case 'r':
	  if (cur_cmd->a2.addr_type != addr_is_null)
	    bad_prog(ONE_ADDR);
	  compile_filename(1, &cur_cmd->x.rfile, NULL);
	  break;
	case 'w':
	  compile_filename(0, NULL, &cur_cmd->x.wfile);
	  break;

	case 's':
	  slash = inchar();
	  if ( !(b  = match_slash(slash, 1, 1)) )
	    bad_prog(UNTERM_S_CMD);
	  if ( !(b2 = match_slash(slash, 0, 1)) )
	    bad_prog(UNTERM_S_CMD);

	  cur_cmd->x.cmd_regex.replace_length = size_buffer(b2);
	  cur_cmd->x.cmd_regex.replacement = MEMDUP(get_buffer(b2),
				    cur_cmd->x.cmd_regex.replace_length, char);
	  free_buffer(b2);

	  {
	    flagT caseless = mark_subst_opts(&cur_cmd->x.cmd_regex);
	    cur_cmd->x.cmd_regex.regx = compile_regex(b, caseless, 0);
	  }
	  free_buffer(b);
	  break;

	case 'y':
	  ustring = MALLOC(YMAP_LENGTH, unsigned char);
	  for (len = 0; len < YMAP_LENGTH; len++)
	    ustring[len] = len;
	  cur_cmd->x.translate = ustring;

	  slash = inchar();
	  if ( !(b = match_slash(slash, 0, 0)) )
	    bad_prog(UNTERM_Y_CMD);
	  ustring = CAST(unsigned char *)get_buffer(b);
	  for (len = size_buffer(b); len; --len)
	    {
	      ch = inchar();
	      if (ch == slash)
		bad_prog("strings for y command are different lengths");
	      if (ch == '\n')
		bad_prog(UNTERM_Y_CMD);
	      if (ch == '\\')
		ch = inchar();
	      if (ch == EOF)
		bad_prog(BAD_EOF);
	      cur_cmd->x.translate[*ustring++] = ch;
	    }
	  free_buffer(b);

	  if (inchar() != slash ||
	      ((ch = in_nonblank()) != EOF && ch != '\n' && ch != ';'))
	    bad_prog(LINE_JUNK);
	  break;

	case EOF:
	  bad_prog(NO_COMMAND);
	  /*NOTREACHED*/
	default:
	  {
	    static char unknown_cmd[] = "Unknown command: ``_''";
	    unknown_cmd[sizeof(unknown_cmd)-4] = ch;
	    bad_prog(unknown_cmd);
	  }
	  /*NOTREACHED*/
	}

      /* this is buried down here so that "continue" statements will miss it */
      ++vector->v_length;
    }
  return vector;
}

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
  cmd->a1.addr_type = addr_is_null;
  cmd->a2.addr_type = addr_is_null;
  cmd->a1_matched = 0;
  cmd->addr_bang = 0;

  *vectorp  = v;
  return cmd;
}


/* let's not confuse text editors that have only dumb bracket-matching... */
#define OPEN_BRACKET	'['
#define CLOSE_BRACKET	']'

static int
add_then_next(b, ch)
  struct buffer *b;
  int ch;
{
  add1_buffer(b, ch);
  return inchar();
}

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
	    if (ch == EOF || ch == '\n')
	      return EOF;
	}
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
      ch = add_then_next(b, ch);
    }

  /* ensure that a newline here is an error, not a "slash" match: */
  return ch == '\n' ? EOF : ch;
}

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
	  else if (ch == 'n' && regex)
	    ch = '\n';
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
  if (regex && rx_testing)
    {
      /* This is slightly bogus.  Sed is noticing an ill-formed regexp,
       * but rx is getting the credit.  Fortunately, this only affects
       * a few spencer tests.
       */
      size_t sz = size_buffer(b);
      char *re_txt;
      add1_buffer(b, '\0');
      re_txt = get_buffer(b);
      if (sz > 0 && re_txt[sz] == '\n')
	re_txt[sz] = '\0';
      printf(BAD_REGEX_FMT, re_txt);
      exit(1);
    }
  free_buffer(b);
  return NULL;
}

static regex_t *
compile_regex(b, ignore_case, nosub)
  struct buffer *b;
  flagT ignore_case;
  flagT nosub;
{
  /* ``An empty regular expression is equivalent to the last regular
     expression read'' so we have to keep track of the last regexp read.
     We keep track the _source_ form of the regular expression because
     it is possible for different modifiers to be specified. */
  static char *last_re = NULL;
  regex_t *new_regex = MALLOC(1, regex_t);
  size_t sz;
  int cflags;
  int error;

  sz = size_buffer(b);
  if (sz > 0)
    {
      add1_buffer(b, '\0');
      FREE(last_re);
      last_re = ck_strdup(get_buffer(b));
    }
  else if (!last_re)
    bad_prog(NO_REGEX);

  cflags = 0;
  if (ignore_case)
    cflags |= REG_ICASE;
  if (nosub)
    cflags |= REG_NOSUB;
  if (use_extended_syntax_p)
    cflags |= REG_EXTENDED;
  if ( (error = regcomp(new_regex, last_re, cflags)) )
    {
      char msg[1000];
      if (rx_testing)
	{
	  printf(BAD_REGEX_FMT, last_re);
	  exit(1);
	}
      regerror(error, NULL, msg, sizeof msg);
      bad_prog(msg);
  }

  return new_regex;
}

/* Try to read an address for a sed command.  If it succeeds,
   return non-zero and store the resulting address in *'addr'.
   If the input doesn't look like an address read nothing
   and return zero.  */
static flagT
compile_address(addr, ch)
  struct addr *addr;
  int ch;
{
  if (ISDIGIT(ch))
    {
      countT num = in_integer(ch);
      ch = in_nonblank();
#if 1
      if (ch != '~')
	{
	  savchar(ch);
	}
      else
	{
	  countT num2 = in_integer(in_nonblank());
	  if (num2 > 0)
	    {
	      addr->addr_type = addr_is_mod;
	      addr->a.m.offset = num;
	      addr->a.m.modulo = num2;
	      return 1;
	    }
	}
      addr->addr_type = addr_is_num;
      addr->a.addr_number = num;
#else
      if (ch == '~')
	{
	  countT num2 = in_integer(in_nonblank());
	  if (num2 == 0)
	    bad_prog("Zero step size for ~ addressing is not valid");
	  addr->addr_type = addr_is_mod;
	  addr->a.m.offset = num;
	  addr->a.m.modulo = num2;
	}
      else
	{
	  savchar(ch);
	  addr->addr_type = addr_is_num;
	  addr->a.addr_number = num;
	}
#endif
      return 1;
    }
  else if (ch == '/' || ch == '\\')
    {
      flagT caseless = 0;
      struct buffer *b;
      addr->addr_type = addr_is_regex;
      if (ch == '\\')
	ch = inchar();
      if ( !(b = match_slash(ch, 1, 1)) )
	bad_prog("Unterminated address regex");
      ch = in_nonblank();
      if (ch == 'I')	/* lower case i would break existing programs */
	caseless = 1;
      else
	savchar(ch);
      addr->a.addr_regex = compile_regex(b, caseless, 1);
      free_buffer(b);
      return 1;
    }
  else if (ch == '$')
    {
      addr->addr_type = addr_is_last;
      return 1;
    }
  return 0;
}

/* read in a filename for a 'r', 'w', or 's///w' command, and
   update the internal structure about files.  The file is
   opened if it isn't already open in the given mode.  */
static void
compile_filename(readit, name, fp)
  flagT readit;
  const char **name;
  FILE **fp;
{
  struct buffer *b;
  int ch;
  char *file_name;
  struct fp_list *p;

  b = init_buffer();
  ch = in_nonblank();
  while (ch != EOF && ch != '\n')
    {
      add1_buffer(b, ch);
      ch = inchar();
    }
  add1_buffer(b, '\0');
  file_name = get_buffer(b);

  for (p=file_ptrs; p; p=p->link)
    if (p->readit_p == readit && strcmp(p->name, file_name) == 0)
      break;

  if (!p)
    {
      p = MALLOC(1, struct fp_list);
      p->name = ck_strdup(file_name);
      p->readit_p = readit;
      p->fp = NULL;
      if (!readit)
	p->fp = ck_fopen(p->name, "w");
      p->link = file_ptrs;
      file_ptrs = p;
    }

  free_buffer(b);
  if (name)
    *name = p->name;
  if (fp)
    *fp = p->fp;
}

/* Store a label (or label reference) created by a ':', 'b', or 't'
   command so that the jump to/from the label can be backpatched after
   compilation is complete.  */
static struct sed_label *
setup_jump(list, cmd, vec)
  struct sed_label *list;
  struct sed_cmd *cmd;
  struct vector *vec;
{
  struct sed_label *ret;
  struct buffer *b;
  int ch;

  b = init_buffer();
  ch = in_nonblank();

  while (ch != EOF && ch != '\n'
	 && !ISBLANK(ch) && ch != ';' && ch != /*{*/ '}')
    {
      add1_buffer(b, ch);
      ch = inchar();
    }
  savchar(ch);
  add1_buffer(b, '\0');
  ret = MALLOC(1, struct sed_label);
  ret->name = ck_strdup(get_buffer(b));
  ret->v = vec;
  ret->v_index = cmd - vec->v;
  ret->next = list;
  free_buffer(b);
  return ret;
}

static flagT
mark_subst_opts(cmd)
  struct subst *cmd;
{
  flagT caseless = 0;
  int ch;

  cmd->global = 0;
  cmd->print = 0;
  cmd->numb = 0;
  cmd->wfile = NULL;

  for (;;)
    switch ( (ch = in_nonblank()) )
      {
      case 'i':	/* GNU extension */
      case 'I':	/* GNU extension */
	caseless = 1;
	break;

      case 'p':
	cmd->print = 1;
	break;

      case 'g':
	cmd->global = 1;
	break;

      case 'w':
	compile_filename(0, NULL, &cmd->wfile);
	return caseless;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	if (cmd->numb)
	  bad_prog("multiple number options to 's' command");
	cmd->numb = in_integer(ch);
	if (!cmd->numb)
	  bad_prog("number option to 's' command may not be zero");
	break;

      case EOF:
      case '\n':
      case ';':
	return caseless;

      default:
	bad_prog("Unknown option to 's'");
	/*NOTREACHED*/
      }
}


/* Read the next character from the program.  Return EOF if there isn't
   anything to read.  Keep cur_input.line up to date, so error messages
   can be meaningful. */
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

/* Read the next non-blank character from the program.  */
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

/* unget 'ch' so the next call to inchar will return it.   */
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
      if (prog.cur <= CAST(const unsigned char *)prog.base
	  || *--prog.cur != ch)
	panic("Called savchar() with unexpected pushback (%x)",
	      CAST(unsigned char)ch);
    }
  else
    ungetc(ch, prog.file);
}

/* Complain about a programming error and exit. */
static void
bad_prog(why)
  const char *why;
{
  if (cur_input.name)
    fprintf(stderr, "%s: file %s line %lu: %s\n",
	    myname, cur_input.name, CAST(unsigned long)cur_input.line, why);
  else
    fprintf(stderr, "%s: -e expression #%lu, char %lu: %s\n",
	    myname,
	    CAST(unsigned long)cur_input.string_expr_count,
	    CAST(unsigned long)(prog.cur-CAST(unsigned char *)prog.base),
	    why);
  exit(1);
}
/*  GNU SED, a batch stream editor.
    Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1998
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

#undef EXPERIMENTAL_DASH_N_OPTIMIZATION	/*don't use -- is buggy*/
#define INITIAL_BUFFER_SIZE	50
#define LCMD_OUT_LINE_LEN	70
#define FREAD_BUFFER_SIZE	8192

#include "config.h"
#include <stdio.h>
#include <ctype.h>

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#ifdef HAVE_ISATTY
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
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

#ifndef HAVE_STRING_H
# include <strings.h>
# ifdef HAVE_MEMORY_H
#  include <memory.h>
# endif
#else
# include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include "regex-sed.h"
#include "basicdefs.h"
#include "utils.h"
#include "sed.h"

#ifdef BOOTSTRAP
# ifdef memchr
#  undef memchr
# endif
# define memchr bootstrap_memchr
static VOID *bootstrap_memchr P_((const VOID *s, int c, size_t n));
#endif /*BOOTSTRAP*/

#ifndef HAVE_REGNEXEC
# define regnexec(x,t,l,r,n,f)	regexec(x,t,r,n,f)
#endif

/* If set, don't write out the line unless explicitly told to */
extern flagT no_default_output;

/* Do we need to be pedantically POSIX compliant? */
extern flagT POSIXLY_CORRECT;


/* Sed operates a line at a time. */
struct line {
  char *text;		/* Pointer to line allocated by malloc. */
  char *active;		/* Pointer to non-consumed part of text. */
  size_t length;	/* Length of text (or active, if used). */
  size_t alloc;		/* Allocated space for text. */
  flagT chomped;	/* Was a trailing newline dropped? */
};

/* A queue of text to write out at the end of a cycle
   (filled by the "a" and "r" commands.) */
struct append_queue {
  const char *rfile;
  const char *text;
  size_t textlen;
  struct append_queue *next;
};

/* State information for the input stream. */
struct input {
  char **file_list;	/* The list of yet-to-be-opened files.
			   It is invalid for file_list to be NULL.
			   When *file_list is NULL we are
			   currently processing the last file. */
  countT bad_count;	/* count of files we failed to open */
  countT line_number;	/* Current input line number (over all files) */

  flagT (*read_fn) P_((struct input *));	/* read one line */
  /* If fp is NULL, read_fn better not be one which uses fp;
     in particular, read_always_fail() is recommended. */

  FILE *fp;		/* if NULL, none of the following are valid */
  VOID *base;		/* if non-NULL, we are using mmap()ed input */
  const char *cur;	/* only valid if base is non-NULL */
  size_t length;	/* only valid if base is non-NULL */
  size_t left;		/* only valid if base is non-NULL */
#ifdef HAVE_ISATTY
  flagT is_tty;		/* only valid if base is NULL */
#endif
};

static void resize_line P_((struct line *lb, size_t len));
static void open_next_file P_((const char *name, struct input *input));
static void closedown P_((struct input *input));
static flagT read_always_fail P_((struct input *input));
static flagT read_mem_line P_((struct input *input));
static size_t slow_getline P_((char *buf, size_t buflen, FILE *in));
static flagT read_file_line P_((struct input *input));
static flagT read_pattern_space P_((struct input *input, flagT append));
static flagT last_file_with_data_p P_((struct input *input));
static flagT test_dollar_EOF P_((struct input *input));
static flagT match_an_address_p P_((struct addr *, struct input *, flagT));
static flagT match_address_p P_((struct sed_cmd *cmd, struct input *input));
static void do_list P_((void));
static void do_subst P_((struct subst *subst));
static flagT execute_program P_((struct vector *, struct input *input));
static void output_line
		P_((const char *text, size_t length, flagT nl, FILE *fp));
static void line_init P_((struct line *buf, size_t initial_size));
static void line_copy P_((struct line *from, struct line *to));
static void str_append P_((struct line *to, const char *string, size_t length));
static void nul_append P_((struct line *dest));
static void line_append P_((struct line *from, struct line *to));
static void dump_append_queue P_((void));
static struct append_queue *next_append_slot P_((void));

#ifdef EXPERIMENTAL_DASH_N_OPTIMIZATION
static countT count_branches P_((struct vector *));
static struct sed_cmd *shrink_program
		P_((struct vector *vec, struct sed_cmd *cur_cmd, countT*n));

/* Used to attempt a simple-minded optimization. */
static countT branches_in_top_level;
#endif /*EXPERIMENTAL_DASH_N_OPTIMIZATION*/


/* Have we done any replacements lately?  This is used by the 't' command. */
static flagT replaced = 0;

/* The 'current' input line. */
static struct line line;

/* An input line that's been stored by later use by the program */
static struct line hold;

/* The buffered input look-ahead. */
static struct line buffer;

static struct append_queue *append_head = NULL;
static struct append_queue *append_tail = NULL;


/* Apply the compiled script to all the named files. */
countT
process_files(the_program, argv)
  struct vector *the_program;
  char **argv;
{
  static char dash[] = "-";
  static char *stdin_argv[2] = { dash, NULL };
  struct input input;

#ifdef EXPERIMENTAL_DASH_N_OPTIMIZATION
  branches_in_top_level = count_branches(the_program);
#endif /*EXPERIMENTAL_DASH_N_OPTIMIZATION*/
  input.file_list = stdin_argv;
  if (argv && *argv)
    input.file_list = argv;
  input.bad_count = 0;
  input.line_number = 0;
  input.read_fn = read_always_fail;
  input.fp = NULL;

  line_init(&line, INITIAL_BUFFER_SIZE);
  line_init(&hold, INITIAL_BUFFER_SIZE);
  line_init(&buffer, FREAD_BUFFER_SIZE);

  while (read_pattern_space(&input, 0))
    {
      flagT quit = execute_program(the_program, &input);
      if (!no_default_output)
	output_line(line.text, line.length, line.chomped, stdout);
      if (quit)
	break;
    }
  closedown(&input);
  return input.bad_count;
}

/* increase a struct line's length, making some attempt at
   keeping realloc() calls under control by padding for future growth.  */
static void
resize_line(lb, len)
  struct line *lb;
  size_t len;
{
  lb->alloc *= 2;
  if (lb->alloc - lb->length < len)
    lb->alloc = lb->length + len;
  lb->text = REALLOC(lb->text, lb->alloc, char);
}

/* Initialize a struct input for the named file. */
static void
open_next_file(name, input)
  const char *name;
  struct input *input;
{
  input->base = NULL;
  buffer.length = 0;

  if (name[0] == '-' && name[1] == '\0')
    {
      clearerr(stdin);	/* clear any stale EOF indication */
      input->fp = stdin;
    }
  else if ( ! (input->fp = fopen(name, "r")) )
    {
      const char *ptr = strerror(errno);
      fprintf(stderr, "%s: can't read %s: %s\n", myname, name, ptr);
      input->read_fn = read_always_fail; /* a redundancy */
      ++input->bad_count;
      return;
    }

  input->read_fn = read_file_line;
  if (map_file(input->fp, &input->base, &input->length))
    {
      input->cur = VCAST(char *)input->base;
      input->left = input->length;
      input->read_fn = read_mem_line;
    }
#ifdef HAVE_ISATTY
  else
    input->is_tty = isatty(fileno(input->fp));
#endif
}

/* Clean up an input stream that we are done with. */
static void
closedown(input)
  struct input *input;
{
  input->read_fn = read_always_fail;
  if (!input->fp)
    return;
  if (input->base)
    unmap_file(input->base, input->length);
  if (input->fp != stdin) /* stdin can be reused on tty and tape devices */
    ck_fclose(input->fp);
  input->fp = NULL;
}

/* dummy function to simplify read_pattern_space() */
static flagT
read_always_fail(input)
  struct input *input UNUSED;
{
  return 0;
}

/* The quick-and-easy mmap()'d case... */
static flagT
read_mem_line(input)
  struct input *input;
{
  const char *e;
  size_t l;

  if ( (e = memchr(input->cur, '\n', input->left)) )
    {
      /* common case */
      l = e++ - input->cur;
      line.chomped = 1;
    }
  else
    {
      /* This test is placed _after_ the memchr() fails for performance
	 reasons (because this branch is the uncommon case and the
	 memchr() will safely fail if input->left == 0).  Okay, so the
	 savings is trivial.  I'm doing it anyway. */
      if (input->left == 0)
	return 0;
      e = input->cur + input->left;
      l = input->left;
      if (*input->file_list || POSIXLY_CORRECT)
	line.chomped = 1;
    }

  if (line.alloc - line.length < l)
    resize_line(&line, l);
  memcpy(line.text + line.length, input->cur, l);
  line.length += l;
  input->left -= e - input->cur;
  input->cur = e;
  return 1;
}

#ifdef HAVE_ISATTY
/* fgets() doesn't let us handle NULs and fread() buffers too much
 * for interactive use.
 */
static size_t
slow_getline(buf, buflen, in)
  char *buf;
  size_t buflen;
  FILE *in;
{
  size_t resultlen = 0;
  int c;

  while (resultlen<buflen && (c=getc(in))!=EOF)
    {
      ++resultlen;
      *buf++ = c;
      if (c == '\n')
	break;
    }
  if (ferror(in))
    panic("input read error: %s", strerror(errno));
  return resultlen;
}
#endif /*HAVE_ISATTY*/

static flagT
read_file_line(input)
  struct input *input;
{
  char *b;
  size_t blen;
  size_t initial_length = line.length;

  if (!buffer.active)
    buffer.active = buffer.text;
  while (!(b = memchr(buffer.active, '\n', buffer.length)))
    {
      if (line.alloc-line.length < buffer.length)
	resize_line(&line, buffer.length);
      memcpy(line.text+line.length, buffer.active, buffer.length);
      line.length += buffer.length;
      buffer.length = 0;
      if (!feof(input->fp))
	{
#ifdef HAVE_ISATTY
	  if (input->is_tty)
	    buffer.length = slow_getline(buffer.text, buffer.alloc, input->fp);
	  else
#endif
	    buffer.length = ck_fread(buffer.text, 1, buffer.alloc, input->fp);
	}

      if (buffer.length == 0)
	{
	  if (*input->file_list || POSIXLY_CORRECT)
	    line.chomped = 1;
	  /* Did we hit EOF without reading anything?  If so, try
	     the next file; otherwise just go with what we did get. */
	  return (line.length > initial_length);
	}
      buffer.active = buffer.text;
    }

  blen = b - buffer.active;
  if (line.alloc-line.length < blen)
    resize_line(&line, blen);
  memcpy(line.text+line.length, buffer.active, blen);
  line.length += blen;
  ++blen;
  line.chomped = 1;
  buffer.active += blen;
  buffer.length -= blen;
  return 1;
}

/* Read in the next line of input, and store it in the pattern space.
   Return zero if there is nothing left to input. */
static flagT
read_pattern_space(input, append)
  struct input *input;
  flagT append;
{
  dump_append_queue();
  replaced = 0;
  line.chomped = 0;
  if (!append)
    line.length = 0;

  while ( ! (*input->read_fn)(input) )
    {
      closedown(input);
      if (!*input->file_list)
	return 0;
      open_next_file(*input->file_list++, input);
    }

  ++input->line_number;
  return 1;
}

static flagT
last_file_with_data_p(input)
  struct input *input;
{
  /*XXX reference implementation is equivalent to:
      return !*input->file_list;
   */
  for (;;)
    {
      int ch;

      closedown(input);
      if (!*input->file_list)
	return 1;
      open_next_file(*input->file_list++, input);
      if (input->fp)
	{
	  if (input->base)
	    {
	      if (0 < input->left)
		return 0;
	    }
	  else if ((ch = getc(input->fp)) != EOF)
	    {
	      ungetc(ch, input->fp);
	      return 0;
	    }
	}
    }
}

/* Determine if we match the '$' address.  */
static flagT
test_dollar_EOF(input)
  struct input *input;
{
  int ch;

  if (buffer.length)
    return 0;
  if (!input->fp)
    return last_file_with_data_p(input);
  if (input->base)
    return (input->left==0 && last_file_with_data_p(input));
  if (feof(input->fp))
    return last_file_with_data_p(input);
  if ((ch = getc(input->fp)) == EOF)
    return last_file_with_data_p(input);
  ungetc(ch, input->fp);
  return 0;
}

/* Append a NUL char after the end of DEST, without resizing DEST.
   This is to permit the use of regexp routines which expect C strings
   instead of counted strings.
 */
static void
nul_append(dest)
  struct line *dest;
{
#ifndef HAVE_REGNEXEC
  if (dest->alloc - dest->length < 1)
    resize_line(dest, 1);
  dest->text[dest->length] = '\0';
#endif
}

/* Return non-zero if the current line matches the address
   pointed to by 'addr'.  */
static flagT
match_an_address_p(addr, input, is_addr2_p)
  struct addr *addr;
  struct input *input;
  flagT is_addr2_p;
{
  switch (addr->addr_type)
    {
    case addr_is_null:
      return 1;
    case addr_is_num:
      if (is_addr2_p)
	return (input->line_number >= addr->a.addr_number);
      return (input->line_number == addr->a.addr_number);
    case addr_is_mod:
      if (addr->a.m.offset < addr->a.m.modulo)
	return (input->line_number%addr->a.m.modulo == addr->a.m.offset);
      /* offset >= modulo implies we have an extra initial skip */
      if (input->line_number < addr->a.m.offset)
	return 0;
      /* normalize */
      addr->a.m.offset %= addr->a.m.modulo;
      return 1;
    case addr_is_last:
      return test_dollar_EOF(input);
    case addr_is_regex:
      nul_append(&line);
      return !regnexec(addr->a.addr_regex,
			line.text, line.length, 0, NULL, 0);
    default:
      panic("INTERNAL ERROR: bad address type");
    }
  /*NOTREACHED*/
  return 0;
}

/* return non-zero if current address is valid for cmd */
static flagT
match_address_p(cmd, input)
  struct sed_cmd *cmd;
  struct input *input;
{
  flagT addr_matched = cmd->a1_matched;

  if (addr_matched)
    {
      if (match_an_address_p(&cmd->a2, input, 1))
	cmd->a1_matched = 0;
    }
  else if (match_an_address_p(&cmd->a1, input, 0))
    {
      addr_matched = 1;
      if (cmd->a2.addr_type != addr_is_null)
	if (   (cmd->a2.addr_type == addr_is_regex)
	    || !match_an_address_p(&cmd->a2, input, 1))
	  cmd->a1_matched = 1;
    }
  if (cmd->addr_bang)
    addr_matched = !addr_matched;
  return addr_matched;
}

static void
do_list()
{
  unsigned char *p = CAST(unsigned char *)line.text;
  countT len = line.length;
  countT width = 0;
  char obuf[180];	/* just in case we encounter a 512-bit char ;-) */
  char *o;
  size_t olen;

  for (; len--; ++p) {
      o = obuf;
      if (ISPRINT(*p)) {
	  *o++ = *p;
	  if (*p == '\\')
	    *o++ = '\\';
      } else {
	  *o++ = '\\';
	  switch (*p) {
#if defined __STDC__ && __STDC__
	    case '\a': *o++ = 'a'; break;
#else
	    /* If not STDC we'll just assume ASCII */
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
      if (width+olen >= LCMD_OUT_LINE_LEN) {
	  ck_fwrite("\\\n", 1, 2, stdout);
	  width = 0;
      }
      ck_fwrite(obuf, 1, olen, stdout);
      width += olen;
  }
  ck_fwrite("$\n", 1, 2, stdout);
}

static void
do_subst(sub)
  struct subst *sub;
{
  /* s_accum will accumulate the result of the s command. */
  /* static so as to keep malloc() calls down */
  static struct line s_accum;

  size_t start;		/* where to start scan for match in LINE */
  size_t offset;	/* where in LINE a match was found */
  size_t remain;	/* length after START, sans trailing newline */
  countT count;		/* number of matches found */
  char *rep;		/* the replacement string */
  char *rep_end;	/* end of the replacement string */
  flagT not_bol_p;
  flagT did_subst;
  regmatch_t regs[10];

  if (s_accum.alloc == 0)
    line_init(&s_accum, INITIAL_BUFFER_SIZE);
  s_accum.length = 0;

  count = 0;
  did_subst = 0;
  not_bol_p = 0;
  start = 0;
  remain = line.length - start;
  rep = sub->replacement;
  rep_end = rep + sub->replace_length;

  nul_append(&line);
  while ( ! regnexec(sub->regx,
		     line.text + start, remain,
		     sizeof(regs) / sizeof(regs[0]), regs, not_bol_p) )
    {
      ++count;
      offset = regs[0].rm_so + start;

      /*
       *  +- line.text   +-[offset]
       *  V              V
       * "blah blah blah xyzzy blah blah blah blah"
       *       ^             ^                    ^
       *       +-[start]     +-[end]              +-[start + remain]
       *
       *
       * regs[0].rm_so == offset - start
       * regs[0].rm_eo == end - start
       */

      /* Copy stuff to the left of the next match into the output
       * string.
       */
      if (start < offset)
	str_append(&s_accum, line.text + start, offset - start);

      /* If we're counting up to the Nth match, are we there yet? */
      if (count < sub->numb)
	{
	  /* Not there yet...so skip this match. */
	  size_t matched = regs[0].rm_eo - regs[0].rm_so;

	  /* If the match was vacuous, skip ahead one character
	   * anyway.   It isn't at all obvious to me that this is
	   * the right behavior for this case.    -t	XXX
	   */
	  if (matched == 0 && offset < line.length)
	    matched = 1;
	  str_append(&s_accum, line.text + offset, matched);
	  start = offset + matched;
	  remain = line.length - start;
	  not_bol_p = 1;
	  continue;
	}

      /* Expand the replacement string into the output string. */
      {
	char *rep_cur;	/* next burst being processed in replacement */
	char *rep_next;	/* end of burst in replacement */

	/* Replacement strings can be viewed as a sequence of variable
	 * length commands.  A command can be a literal string or a
	 * backreference (either numeric or "&").
	 *
	 * This loop measures off the next command between
	 * REP_CUR and REP_NEXT, handles that command, and loops.
	 */
	for (rep_next = rep_cur = rep;
	     rep_next < rep_end;
	     rep_next++)
	  {
	    if (*rep_next == '\\')
	      {
		/* Preceding the backslash may be some literal
		 * text:
		 */
		if (rep_cur < rep_next)
		  str_append(&s_accum, rep_cur,
			     CAST(size_t)(rep_next - rep_cur));

		/* Skip the backslash itself and look for
		 * a numeric back-reference:
		 */
		rep_next++;
		if (rep_next < rep_end
		    && '0' <= *rep_next && *rep_next <= '9')
		  {
		    int i = *rep_next - '0';
		    str_append(&s_accum, line.text + start + regs[i].rm_so,
			       CAST(size_t)(regs[i].rm_eo-regs[i].rm_so));
		  }
		else
		  str_append(&s_accum, rep_next, 1);
		rep_cur = rep_next + 1;
	      }
	    else if (*rep_next == '&')
	      {
		/* Preceding the ampersand may be some literal
		 * text:
		 */
		if (rep_cur < rep_next)
		  str_append(&s_accum, rep_cur,
			     CAST(size_t)(rep_next - rep_cur));
		str_append(&s_accum, line.text + start + regs[0].rm_so,
			   CAST(size_t)(regs[0].rm_eo - regs[0].rm_so));
		rep_cur = rep_next + 1;
	      }
	  }

	/* There may be a trailing literal in the replacement string. */
	if (rep_cur < rep_next)
	  str_append(&s_accum, rep_cur, CAST(size_t)(rep_next - rep_cur));
      }

      did_subst = 1;
      not_bol_p = 1;
      start += regs[0].rm_eo;
      remain = line.length - start;
      if (!sub->global || remain == 0)
	break;

      /* If the match was vacuous, skip over one character
       * and add that character to the output.
       */
      if (regs[0].rm_so == regs[0].rm_eo)
	{
	  str_append(&s_accum, line.text + offset, 1);
	  ++start;
	  --remain;
	}
    }

  if (did_subst)
    {
      struct line tmp;
      if (start < line.length)
	str_append(&s_accum, line.text + start, remain);
      memcpy(VCAST(VOID *)&tmp, VCAST(VOID *)&line, sizeof line);
      memcpy(VCAST(VOID *)&line, VCAST(VOID *)&s_accum, sizeof line);
      line.chomped = tmp.chomped;
      memcpy(VCAST(VOID *)&s_accum, VCAST(VOID *)&tmp, sizeof line);
      if (sub->wfile)
	output_line(line.text, line.length, line.chomped, sub->wfile);
      if (sub->print)
	output_line(line.text, line.length, line.chomped, stdout);
      replaced = 1;
    }
}

/* Execute the program 'vec' on the current input line.
   Return non-zero if caller should quit, 0 otherwise.  */
static flagT
execute_program(vec, input)
  struct vector *vec;
  struct input *input;
{
  struct vector *cur_vec;
  struct sed_cmd *cur_cmd;
  size_t n;

  cur_vec = vec;
  cur_cmd = cur_vec->v;
  n = cur_vec->v_length;
  while (n)
    {
      if (match_address_p(cur_cmd, input))
	{
	  switch (cur_cmd->cmd)
	    {
	    case '{':		/* Execute sub-program */
	      if (cur_cmd->x.sub->v_length)
		{
		  cur_vec = cur_cmd->x.sub;
		  cur_cmd = cur_vec->v;
		  n = cur_vec->v_length;
		  continue;
		}
	      break;

	    case '}':
	      {
		countT i = cur_vec->return_i;
		cur_vec = cur_vec->return_v;
		cur_cmd = cur_vec->v + i;
		n = cur_vec->v_length - i;
	      }
	      continue;

	    case 'a':
	      {
		struct append_queue *aq = next_append_slot();
		aq->text = cur_cmd->x.cmd_txt.text;
		aq->textlen = cur_cmd->x.cmd_txt.text_len;
	      }
	      break;

	    case 'b':
	      if (cur_cmd->x.jump)
		{
		  struct sed_label *j = cur_cmd->x.jump;
		  countT i = j->v_index;
		  cur_vec = j->v;
		  cur_cmd = cur_vec->v + i;
		  n = cur_vec->v_length - i;
		  continue;
		}
	      return 0;

	    case 'c':
	      line.length = 0;
	      line.chomped = 0;
	      if (!cur_cmd->a1_matched)
		output_line(cur_cmd->x.cmd_txt.text,
			    cur_cmd->x.cmd_txt.text_len, 0, stdout);
	      /* POSIX.2 is silent about c starting a new cycle,
		 but it seems to be expected (and make sense). */
	      return 0;

	    case 'd':
	      line.length = 0;
	      line.chomped = 0;
	      return 0;

	    case 'D':
	      {
		char *p = memchr(line.text, '\n', line.length);
		if (!p)
		  {
		    line.length = 0;
		    line.chomped = 0;
		    return 0;
		  }
		++p;
		memmove(line.text, p, line.length);
		line.length -= p - line.text;

		/* reset to start next cycle without reading a new line: */
		cur_vec = vec;
		cur_cmd = cur_vec->v;
		n = cur_vec->v_length;
		continue;
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
			  cur_cmd->x.cmd_txt.text_len, 0, stdout);
	      break;

	    case 'l':
	      do_list();
	      break;

	    case 'n':
	      if (!no_default_output)
		output_line(line.text, line.length, line.chomped, stdout);
	      if (!read_pattern_space(input, 0))
		return 1;
	      break;

	    case 'N':
	      str_append(&line, "\n", 1);
	      if (!read_pattern_space(input, 1))
		return 1;
	      break;

	    case 'p':
	      output_line(line.text, line.length, line.chomped, stdout);
	      break;

	    case 'P':
	      {
		char *p = memchr(line.text, '\n', line.length);
		output_line(line.text, p ? p - line.text : line.length,
			    p ? 1 : line.chomped, stdout);
	      }
	      break;

	    case 'q':
	      return 1;

	    case 'r':
	      if (cur_cmd->x.rfile)
		{
		  struct append_queue *aq = next_append_slot();
		  aq->rfile = cur_cmd->x.rfile;
		}
	      break;

	    case 's':
	      do_subst(&cur_cmd->x.cmd_regex);
	      break;

	    case 't':
	      if (replaced)
		{
		  replaced = 0;
		  if (cur_cmd->x.jump)
		    {
		      struct sed_label *j = cur_cmd->x.jump;
		      countT i = j->v_index;
		      cur_vec = j->v;
		      cur_cmd = cur_vec->v + i;
		      n = cur_vec->v_length - i;
		      continue;
		    }
		  return 0;
		}
	      break;

	    case 'w':
	      if (cur_cmd->x.wfile)
		output_line(line.text, line.length,
			    line.chomped, cur_cmd->x.wfile);
	      break;

	    case 'x':
	      {
		struct line temp;
		memcpy(VCAST(VOID *)&temp, VCAST(VOID *)&line, sizeof line);
		memcpy(VCAST(VOID *)&line, VCAST(VOID *)&hold, sizeof line);
		memcpy(VCAST(VOID *)&hold, VCAST(VOID *)&temp, sizeof line);
	      }
	      break;

	    case 'y':
	      {
		unsigned char *p, *e;
		p = CAST(unsigned char *)line.text;
		for (e=p+line.length; p<e; ++p)
		  *p = cur_cmd->x.translate[*p];
	      }
	      break;

	    case ':':
	      /* Executing labels is easy. */
	      break;

	    case '=':
	      printf("%lu\n", CAST(unsigned long)input->line_number);
	      break;

	    default:
	      panic("INTERNAL ERROR: Bad cmd %c", cur_cmd->cmd);
	    }
	}
#ifdef EXPERIMENTAL_DASH_N_OPTIMIZATION
/* If our top-level program consists solely of commands with addr_is_num
 * address then once we past the last mentioned line we should be able
 * to quit if no_default_output is true, or otherwise quickly copy input
 * to output.  Now whether this optimization is a win or not depends
 * on how cheaply we can implement this for the cases where it doesn't
 * help, as compared against how much time is saved.
 * One semantic difference (which I think is an improvement) is
 * that *this* version will terminate after printing line two
 * in the script "yes | sed -n 2p".
 */
      else
	{
	  /* can we ever match again? */
	  if (cur_cmd->a1.addr_type == addr_is_num &&
	      ((input->line_number < cur_cmd->a1.a.addr_number)
		!= !cur_cmd->addr_bang))
	    {
	      /* skip all this next time */
	      cur_cmd->a1.addr_type = addr_is_null;
	      cur_cmd->addr_bang = 1;
	      /* can we make an optimization? */
	      if (cur_cmd->cmd == '{' || cur_cmd->cmd == '}'
		    || cur_cmd->cmd == 'b' || cur_cmd->cmd == 't')
		{
		  if (cur_vec == vec)
		    --branches_in_top_level;
		  cur_cmd->cmd = ':';	/* replace with no-op */
		}
	      if (cur_vec == vec && branches_in_top_level == 0)
		{
		  /* whew!  all that just so that we can get to here! */
		  countT new_n = n;
		  cur_cmd = shrink_program(cur_vec, cur_cmd, &new_n);
		  n = new_n;
		  if (!cur_cmd && no_default_output)
		    return 1;
		  continue;
		}
	    }
	}
#endif /*EXPERIMENTAL_DASH_N_OPTIMIZATION*/

      /* these are buried down here so that a "continue" statement
	 can skip them */
      ++cur_cmd;
      --n;
    }
    return 0;
}

static void
output_line(text, length, nl, fp)
  const char *text;
  size_t length;
  flagT nl;
  FILE *fp;
{
  ck_fwrite(text, 1, length, fp);
  if (nl)
    ck_fwrite("\n", 1, 1, fp);
  if (fp != stdout)
    ck_fflush(fp);
}


/* initialize a "struct line" buffer */
static void
line_init(buf, initial_size)
  struct line *buf;
  size_t initial_size;
{
  buf->text = MALLOC(initial_size, char);
  buf->active = NULL;
  buf->alloc = initial_size;
  buf->length = 0;
  buf->chomped = 1;
}

/* Copy the contents of the line 'from' into the line 'to'.
   This destroys the old contents of 'to'.  It will still work
   if the line 'from' contains NULs. */
static void
line_copy(from, to)
  struct line *from;
  struct line *to;
{
  if (to->alloc < from->length)
    {
      to->alloc = from->length;
      to->text = REALLOC(to->text, to->alloc, char);
    }
  memcpy(to->text, from->text, from->length);
  to->length = from->length;
  to->chomped = from->chomped;
}

/* Append 'length' bytes from 'string' to the line 'to'
   This routine *will* append NUL bytes without failing.  */
static void
str_append(to, string, length)
  struct line *to;
  const char *string;
  size_t length;
{
  if (to->alloc - to->length < length)
    resize_line(to, length);
  memcpy(to->text + to->length, string, length);
  to->length += length;
}

/* Append the contents of the line 'from' to the line 'to'.
   This routine will work even if the line 'from' contains nulls */
static void
line_append(from, to)
  struct line *from;
  struct line *to;
{
  str_append(to, "\n", 1);
  str_append(to, from->text, from->length);
  to->chomped = from->chomped;
}



static struct append_queue *
next_append_slot()
{
  struct append_queue *n = MALLOC(1, struct append_queue);

  n->rfile = NULL;
  n->text = NULL;
  n->textlen = 0;
  n->next = NULL;

  if (append_tail)
      append_tail->next = n;
  else
      append_head = n;
  return append_tail = n;
}

static void
dump_append_queue()
{
  struct append_queue *p, *q;

  for (p=append_head; p; p=q)
    {
      if (p->text)
	  output_line(p->text, p->textlen, 0, stdout);
      if (p->rfile)
	{
	  char buf[FREAD_BUFFER_SIZE];
	  size_t cnt;
	  FILE *fp;

	  fp = fopen(p->rfile, "r");
	  /* Not ck_fopen() because: "If _rfile_ does not exist or cannot be
	     read, it shall be treated as if it were an empty file, causing
	     no error condition."  IEEE Std 1003.2-1992 */
	  if (fp)
	    {
	      while ((cnt = ck_fread(buf, 1, sizeof buf, fp)) > 0)
		ck_fwrite(buf, 1, cnt, stdout);
	      fclose(fp);
	    }
	}
      q = p->next;
      FREE(p);
    }
  append_head = append_tail = NULL;
}

#ifdef EXPERIMENTAL_DASH_N_OPTIMIZATION
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
	case '{':
	case '}':
	case 'b':
	case 't':
	  ++cnt;
	}
    }
  return cnt;
}

static struct sed_cmd *
shrink_program(vec, cur_cmd, n)
  struct vector *vec;
  struct sed_cmd *cur_cmd;
  countT *n;
{
  struct sed_cmd *v = vec->v;
  struct sed_cmd *last_cmd = v + vec->v_length;
  struct sed_cmd *p;
  countT cmd_cnt;

  for (p=v; p < cur_cmd; ++p)
    if (p->cmd != ':')
      *v++ = *p;
  cmd_cnt = v - vec->v;

  for (++p; p < last_cmd; ++p)
    if (p->cmd != ':')
      *v++ = *p;
  vec->v_length = v - vec->v;

  *n = vec->v_length - cmd_cnt;
  return 0 < vec->v_length ? cur_cmd-cmd_cnt : CAST(struct sed_cmd *)0;
}
#endif /*EXPERIMENTAL_DASH_N_OPTIMIZATION*/

#ifdef BOOTSTRAP
/* We can't be sure that the system we're boostrapping on has
   memchr(), and ../lib/memchr.c requires configuration knowledge
   about how many bits are in a `long'.  This implementation
   is far from ideal, but it should get us up-and-limping well
   enough to run the configure script, which is all that matters.
*/
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
/*  Functions from hack's utils library.
    Copyright (C) 1989, 1990, 1991, 1998
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

/* These routines were written as part of a library (by hack), but since most
   people don't have the library, here they are.  */
/* Tweaked by ken */

#include "config.h"

#include <stdio.h>

#include <errno.h>
#ifndef errno
  extern int errno;
#endif

#ifndef HAVE_STRING_H
# include <strings.h>
#else
# include <string.h>
#endif /* HAVE_STRING_H */

#ifndef HAVE_STDLIB_H
# ifdef RX_MEMDBUG
#  include <sys/types.h>
#  include <malloc.h>
# endif /* RX_MEMDBUG */
#else /* HAVE_STDLIB_H */
# include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "basicdefs.h"
#include "utils.h"

#ifndef HAVE_STDLIB_H
# ifndef RX_MEMDBUG
   VOID *malloc();
   VOID *realloc();
# endif /* RX_MEMDBUG */
#endif /* HAVE_STDLIB_H */

const char *myname;


/* Print an error message and exit */
#if !defined __STDC__ || !__STDC__
# include <varargs.h>
# define VSTART(l,a)	va_start(l)
void
panic(str, va_alist)
  char *str;
  va_dcl
#else /*__STDC__*/
# include <stdarg.h>
# define VSTART(l,a)	va_start(l, a)
void
panic(const char *str, ...)
#endif /* __STDC__ */
{
  va_list iggy;

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
static const char *utils_fp_name P_((FILE *fp));

/* Internal routine to get a filename from utils_id_s */
static const char *
utils_fp_name(fp)
  FILE *fp;
{
  struct id *p;

  for (p=utils_id_s; p; p=p->link)
    if (p->fp == fp)
      return p->name;
  if (fp == stdin)
    return "{standard input}";
  else if (fp == stdout)
    return "{standard output}";
  else if (fp == stderr)
    return "{standard error}";
  return "{Unknown file pointer}";
}

/* Panic on failing fopen */
FILE *
ck_fopen(name, mode)
  const char *name;
  const char *mode;
{
  FILE *fp;
  struct id *p;

  if ( ! (fp = fopen(name, mode)) )
    panic("Couldn't open file %s", name);
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
  if (size && fwrite(ptr, size, nmemb, stream) != nmemb)
    panic("couldn't write %d item%s to %s: %s",
	  nmemb, nmemb==1 ? "" : "s", utils_fp_name(stream), strerror(errno));
}

/* Panic on failing fread */
size_t
ck_fread(ptr, size, nmemb, stream)
  VOID *ptr;
  size_t size;
  size_t nmemb;
  FILE *stream;
{
  if (size && (nmemb=fread(ptr, size, nmemb, stream)) <= 0 && ferror(stream))
    panic("read error on %s: %s", utils_fp_name(stream), strerror(errno));
  return nmemb;
}

/* Panic on failing fflush */
void
ck_fflush(stream)
  FILE *stream;
{
  if (fflush(stream) == EOF)
    panic("Couldn't flush %s", utils_fp_name(stream));
}

/* Panic on failing fclose */
void
ck_fclose(stream)
  FILE *stream;
{
  struct id r;
  struct id *prev;
  struct id *cur;

  if (!stream)
    return;
  if (fclose(stream) == EOF)
    panic("Couldn't close %s", utils_fp_name(stream));
  r.link = utils_id_s;
  prev = &r;
  while ( (cur = prev->link) )
    {
      if (stream == cur->fp)
	{
	  prev->link = cur->link;
	  FREE(cur->name);
	  FREE(cur);
#ifdef TRUST_THAT_ID_CHAIN_IS_CLEAN
	  break;
#endif
	}
      else
	{
	  prev = cur;
	}
    }
  utils_id_s = r.link;
}


/* Panic on failing malloc */
VOID *
ck_malloc(size)
  size_t size;
{
  VOID *ret = malloc(size ? size : 1);
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


/* Implement a variable sized buffer of 'stuff'.  We don't know what it is,
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
		     myname, argv[optind]);
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
		     myname, pfound->name);
		   else
		    /* +option or -option */
		    fprintf (stderr,
		     _("%s: option `%c%s' doesn't allow an argument\n"),
		     myname, argv[optind - 1][0], pfound->name);

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
			   myname, argv[optind - 1]);
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
			 myname, nextchar);
	      else
		/* +option or -option */
		fprintf (stderr, _("%s: unrecognized option `%c%s'\n"),
			 myname, argv[optind][0], nextchar);
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
		       myname, c);
	    else
	      fprintf (stderr, _("%s: invalid option -- %c\n"),
		       myname, c);
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
			 myname, c);
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
		       myname, argv[optind]);
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
			       myname, pfound->name);

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
			       myname, argv[optind - 1]);
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
			   myname, c);
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
/* Extended regular expression matching and search library,
   version 0.12.
   (Implements POSIX draft P1003.2/D11.2, except for some of the
   internationalization features.)
   Copyright (C) 1993, 94, 95, 96, 97, 98 Free Software Foundation, Inc.

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

/* AIX requires this to be the first thing in the file. */
#if defined _AIX && !defined REGEX_MALLOC
  #pragma alloca
#endif

#undef	_GNU_SOURCE
#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef PARAMS
# if defined __GNUC__ || (defined __STDC__ && __STDC__)
#  define PARAMS(args) args
# else
#  define PARAMS(args) ()
# endif  /* GCC.  */
#endif  /* Not PARAMS.  */

#if defined STDC_HEADERS && !defined emacs
# include <stddef.h>
#else
/* We need this for `regex-gnu.h', and perhaps for the Emacs include files.  */
# include <sys/types.h>
#endif

/* For platform which support the ISO C amendement 1 functionality we
   support user defined character classes.  */
#if defined _LIBC || (defined HAVE_WCTYPE_H && defined HAVE_WCHAR_H)
# include <wctype.h>
# include <wchar.h>

/* We have to keep the namespace clean.  */
# define regfree(preg) __regfree (preg)
# define regexec(pr, st, nm, pm, ef) __regexec (pr, st, nm, pm, ef)
# define regnexec(pr, st, ln, nm, pm, ef) __regnexec (pr, st, ln, nm, pm, ef)
# define regcomp(preg, pattern, cflags) __regcomp (preg, pattern, cflags)
# define regerror(errcode, preg, errbuf, errbuf_size) \
	__regerror(errcode, preg, errbuf, errbuf_size)
# define re_set_registers(bu, re, nu, st, en) \
	__re_set_registers (bu, re, nu, st, en)
# define re_match_2(bufp, string1, size1, string2, size2, pos, regs, stop) \
	__re_match_2 (bufp, string1, size1, string2, size2, pos, regs, stop)
# define re_match(bufp, string, size, pos, regs) \
	__re_match (bufp, string, size, pos, regs)
# define re_search(bufp, string, size, startpos, range, regs) \
	__re_search (bufp, string, size, startpos, range, regs)
# define re_compile_pattern(pattern, length, bufp) \
	__re_compile_pattern (pattern, length, bufp)
# define re_set_syntax(syntax) __re_set_syntax (syntax)
# define re_search_2(bufp, st1, s1, st2, s2, startpos, range, regs, stop) \
	__re_search_2 (bufp, st1, s1, st2, s2, startpos, range, regs, stop)
# define re_compile_fastmap(bufp) __re_compile_fastmap (bufp)

#define btowc __btowc
#endif

/* This is for other GNU distributions with internationalized messages.  */
#if HAVE_LIBINTL_H || defined _LIBC
# include <libintl.h>
#else
# define gettext(msgid) (msgid)
#endif

#ifndef gettext_noop
/* This define is so xgettext can find the internationalizable
   strings.  */
# define gettext_noop(String) String
#endif

/* The `emacs' switch turns on certain matching commands
   that make sense only in Emacs. */
#ifdef emacs

# include "lisp.h"
# include "buffer.h"
# include "syntax.h"

#else  /* not emacs */

/* If we are not linking with Emacs proper,
   we can't use the relocating allocator
   even if config.h says that we can.  */
# undef REL_ALLOC

# if defined STDC_HEADERS || defined _LIBC
#  include <stdlib.h>
# else
char *malloc ();
char *realloc ();
# endif

/* When used in Emacs's lib-src, we need to get bzero and bcopy somehow.
   If nothing else has been done, use the method below.  */
# ifdef INHIBIT_STRING_HEADER
#  if !(defined HAVE_BZERO && defined HAVE_BCOPY)
#   if !defined bzero && !defined bcopy
#    undef INHIBIT_STRING_HEADER
#   endif
#  endif
# endif

/* This is the normal way of making sure we have a bcopy and a bzero.
   This is used in most programs--a few other programs avoid this
   by defining INHIBIT_STRING_HEADER.  */
# ifndef INHIBIT_STRING_HEADER
#  if defined HAVE_STRING_H || defined STDC_HEADERS || defined _LIBC
#   include <string.h>
#   ifndef bzero
#    ifndef _LIBC
#     define bzero(s, n)	(memset (s, '\0', n), (s))
#    else
#     define bzero(s, n)	__bzero (s, n)
#    endif
#   endif
#  else
#   include <strings.h>
#   ifndef memcmp
#    define memcmp(s1, s2, n)	bcmp (s1, s2, n)
#   endif
#   ifndef memcpy
#    define memcpy(d, s, n)	(bcopy (s, d, n), (d))
#   endif
#  endif
# endif

/* Define the syntax stuff for \<, \>, etc.  */

/* This must be nonzero for the wordchar and notwordchar pattern
   commands in re_match_2.  */
# ifndef Sword
#  define Sword 1
# endif

# ifdef SWITCH_ENUM_BUG
#  define SWITCH_ENUM_CAST(x) ((int)(x))
# else
#  define SWITCH_ENUM_CAST(x) (x)
# endif

/* How many characters in the character set.  */
# define CHAR_SET_SIZE 256

# ifdef SYNTAX_TABLE

extern char *re_syntax_table;

# else /* not SYNTAX_TABLE */

static char re_syntax_table[CHAR_SET_SIZE];

static void
init_syntax_once ()
{
   register int c;
   static int done = 0;

   if (done)
     return;

   bzero (re_syntax_table, sizeof re_syntax_table);

   for (c = 'a'; c <= 'z'; c++)
     re_syntax_table[c] = Sword;

   for (c = 'A'; c <= 'Z'; c++)
     re_syntax_table[c] = Sword;

   for (c = '0'; c <= '9'; c++)
     re_syntax_table[c] = Sword;

   re_syntax_table['_'] = Sword;

   done = 1;
}

# endif /* not SYNTAX_TABLE */

# define SYNTAX(c) re_syntax_table[c]

#endif /* not emacs */

/* Get the interface, including the syntax bits.  */
#include "regex-gnu.h"

/* isalpha etc. are used for the character classes.  */
#include <ctype.h>

/* Jim Meyering writes:

   "... Some ctype macros are valid only for character codes that
   isascii says are ASCII (SGI's IRIX-4.0.5 is one such system --when
   using /bin/cc or gcc but without giving an ansi option).  So, all
   ctype uses should be through macros like ISPRINT...  If
   STDC_HEADERS is defined, then autoconf has verified that the ctype
   macros don't need to be guarded with references to isascii. ...
   Defining isascii to 1 should let any compiler worth its salt
   eliminate the && through constant folding."
   Solaris defines some of these symbols so we must undefine them first.  */

#undef ISASCII
#if defined STDC_HEADERS || (!defined isascii && !defined HAVE_ISASCII)
# define ISASCII(c) 1
#else
# define ISASCII(c) isascii(c)
#endif

#ifdef isblank
# define ISBLANK(c) (ISASCII (c) && isblank (c))
#else
# define ISBLANK(c) ((c) == ' ' || (c) == '\t')
#endif
#ifdef isgraph
# define ISGRAPH(c) (ISASCII (c) && isgraph (c))
#else
# define ISGRAPH(c) (ISASCII (c) && isprint (c) && !isspace (c))
#endif

#undef ISPRINT
#define ISPRINT(c) (ISASCII (c) && isprint (c))
#define ISDIGIT(c) (ISASCII (c) && isdigit (c))
#define ISALNUM(c) (ISASCII (c) && isalnum (c))
#define ISALPHA(c) (ISASCII (c) && isalpha (c))
#define ISCNTRL(c) (ISASCII (c) && iscntrl (c))
#define ISLOWER(c) (ISASCII (c) && islower (c))
#define ISPUNCT(c) (ISASCII (c) && ispunct (c))
#define ISSPACE(c) (ISASCII (c) && isspace (c))
#define ISUPPER(c) (ISASCII (c) && isupper (c))
#define ISXDIGIT(c) (ISASCII (c) && isxdigit (c))

#ifndef NULL
# define NULL (void *)0
#endif

/* We remove any previous definition of `SIGN_EXTEND_CHAR',
   since ours (we hope) works properly with all combinations of
   machines, compilers, `char' and `unsigned char' argument types.
   (Per Bothner suggested the basic approach.)  */
#undef SIGN_EXTEND_CHAR
#if __STDC__
# define SIGN_EXTEND_CHAR(c) ((signed char) (c))
#else  /* not __STDC__ */
/* As in Harbison and Steele.  */
# define SIGN_EXTEND_CHAR(c) ((((unsigned char) (c)) ^ 128) - 128)
#endif

/* Should we use malloc or alloca?  If REGEX_MALLOC is not defined, we
   use `alloca' instead of `malloc'.  This is because using malloc in
   re_search* or re_match* could cause memory leaks when C-g is used in
   Emacs; also, malloc is slower and causes storage fragmentation.  On
   the other hand, malloc is more portable, and easier to debug.

   Because we sometimes use alloca, some routines have to be macros,
   not functions -- `alloca'-allocated space disappears at the end of the
   function it is called in.  */

#ifdef REGEX_MALLOC

# define REGEX_ALLOCATE malloc
# define REGEX_REALLOCATE(source, osize, nsize) realloc (source, nsize)
# define REGEX_FREE free

#else /* not REGEX_MALLOC  */

/* Emacs already defines alloca, sometimes.  */
# ifndef alloca

/* Make alloca work the best possible way.  */
#  ifdef __GNUC__
#   define alloca __builtin_alloca
#  else /* not __GNUC__ */
#   if HAVE_ALLOCA_H
#    include <alloca.h>
#   endif /* HAVE_ALLOCA_H */
#  endif /* not __GNUC__ */

# endif /* not alloca */

# define REGEX_ALLOCATE alloca

/* Assumes a `char *destination' variable.  */
# define REGEX_REALLOCATE(source, osize, nsize)				\
  (destination = (char *) alloca (nsize),				\
   memcpy (destination, source, osize))

/* No need to do anything to free, after alloca.  */
# define REGEX_FREE(arg) ((void)0) /* Do nothing!  But inhibit gcc warning.  */

#endif /* not REGEX_MALLOC */

/* Define how to allocate the failure stack.  */

#if defined REL_ALLOC && defined REGEX_MALLOC

# define REGEX_ALLOCATE_STACK(size)				\
  r_alloc (&failure_stack_ptr, (size))
# define REGEX_REALLOCATE_STACK(source, osize, nsize)		\
  r_re_alloc (&failure_stack_ptr, (nsize))
# define REGEX_FREE_STACK(ptr)					\
  r_alloc_free (&failure_stack_ptr)

#else /* not using relocating allocator */

# ifdef REGEX_MALLOC

#  define REGEX_ALLOCATE_STACK malloc
#  define REGEX_REALLOCATE_STACK(source, osize, nsize) realloc (source, nsize)
#  define REGEX_FREE_STACK free

# else /* not REGEX_MALLOC */

#  define REGEX_ALLOCATE_STACK alloca

#  define REGEX_REALLOCATE_STACK(source, osize, nsize)			\
   REGEX_REALLOCATE (source, osize, nsize)
/* No need to explicitly free anything.  */
#  define REGEX_FREE_STACK(arg)

# endif /* not REGEX_MALLOC */
#endif /* not using relocating allocator */


/* True if `size1' is non-NULL and PTR is pointing anywhere inside
   `string1' or just past its end.  This works if PTR is NULL, which is
   a good thing.  */
#define FIRST_STRING_P(ptr) 					\
  (size1 && string1 <= (ptr) && (ptr) <= string1 + size1)

/* (Re)Allocate N items of type T using malloc, or fail.  */
#define TALLOC(n, t) ((t *) malloc ((n) * sizeof (t)))
#define RETALLOC(addr, n, t) ((addr) = (t *) realloc (addr, (n) * sizeof (t)))
#define RETALLOC_IF(addr, n, t) \
  if (addr) RETALLOC((addr), (n), t); else (addr) = TALLOC ((n), t)
#define REGEX_TALLOC(n, t) ((t *) REGEX_ALLOCATE ((n) * sizeof (t)))

#define BYTEWIDTH 8 /* In bits.  */

#define STREQ(s1, s2) ((strcmp (s1, s2) == 0))

#undef MAX
#undef MIN
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef char boolean;
#define false 0
#define true 1

static int re_match_2_internal PARAMS ((struct re_pattern_buffer *bufp,
					const char *string1, int size1,
					const char *string2, int size2,
					int pos,
					struct re_registers *regs,
					int stop));

/* These are the command codes that appear in compiled regular
   expressions.  Some opcodes are followed by argument bytes.  A
   command code can specify any interpretation whatsoever for its
   arguments.  Zero bytes may appear in the compiled regular expression.  */

typedef enum
{
  no_op = 0,

  /* Succeed right away--no more backtracking.  */
  succeed,

        /* Followed by one byte giving n, then by n literal bytes.  */
  exactn,

        /* Matches any (more or less) character.  */
  anychar,

        /* Matches any one char belonging to specified set.  First
           following byte is number of bitmap bytes.  Then come bytes
           for a bitmap saying which chars are in.  Bits in each byte
           are ordered low-bit-first.  A character is in the set if its
           bit is 1.  A character too large to have a bit in the map is
           automatically not in the set.  */
  charset,

        /* Same parameters as charset, but match any character that is
           not one of those specified.  */
  charset_not,

        /* Start remembering the text that is matched, for storing in a
           register.  Followed by one byte with the register number, in
           the range 0 to one less than the pattern buffer's re_nsub
           field.  Then followed by one byte with the number of groups
           inner to this one.  (This last has to be part of the
           start_memory only because we need it in the on_failure_jump
           of re_match_2.)  */
  start_memory,

        /* Stop remembering the text that is matched and store it in a
           memory register.  Followed by one byte with the register
           number, in the range 0 to one less than `re_nsub' in the
           pattern buffer, and one byte with the number of inner groups,
           just like `start_memory'.  (We need the number of inner
           groups here because we don't have any easy way of finding the
           corresponding start_memory when we're at a stop_memory.)  */
  stop_memory,

        /* Match a duplicate of something remembered. Followed by one
           byte containing the register number.  */
  duplicate,

        /* Fail unless at beginning of line.  */
  begline,

        /* Fail unless at end of line.  */
  endline,

        /* Succeeds if at beginning of buffer (if emacs) or at beginning
           of string to be matched (if not).  */
  begbuf,

        /* Analogously, for end of buffer/string.  */
  endbuf,

        /* Followed by two byte relative address to which to jump.  */
  jump,

	/* Same as jump, but marks the end of an alternative.  */
  jump_past_alt,

        /* Followed by two-byte relative address of place to resume at
           in case of failure.  */
  on_failure_jump,

        /* Like on_failure_jump, but pushes a placeholder instead of the
           current string position when executed.  */
  on_failure_keep_string_jump,

        /* Throw away latest failure point and then jump to following
           two-byte relative address.  */
  pop_failure_jump,

        /* Change to pop_failure_jump if know won't have to backtrack to
           match; otherwise change to jump.  This is used to jump
           back to the beginning of a repeat.  If what follows this jump
           clearly won't match what the repeat does, such that we can be
           sure that there is no use backtracking out of repetitions
           already matched, then we change it to a pop_failure_jump.
           Followed by two-byte address.  */
  maybe_pop_jump,

        /* Jump to following two-byte address, and push a dummy failure
           point. This failure point will be thrown away if an attempt
           is made to use it for a failure.  A `+' construct makes this
           before the first repeat.  Also used as an intermediary kind
           of jump when compiling an alternative.  */
  dummy_failure_jump,

	/* Push a dummy failure point and continue.  Used at the end of
	   alternatives.  */
  push_dummy_failure,

        /* Followed by two-byte relative address and two-byte number n.
           After matching N times, jump to the address upon failure.  */
  succeed_n,

        /* Followed by two-byte relative address, and two-byte number n.
           Jump to the address N times, then fail.  */
  jump_n,

        /* Set the following two-byte relative address to the
           subsequent two-byte number.  The address *includes* the two
           bytes of number.  */
  set_number_at,

  wordchar,	/* Matches any word-constituent character.  */
  notwordchar,	/* Matches any char that is not a word-constituent.  */

  wordbeg,	/* Succeeds if at word beginning.  */
  wordend,	/* Succeeds if at word end.  */

  wordbound,	/* Succeeds if at a word boundary.  */
  notwordbound	/* Succeeds if not at a word boundary.  */

#ifdef emacs
  ,before_dot,	/* Succeeds if before point.  */
  at_dot,	/* Succeeds if at point.  */
  after_dot,	/* Succeeds if after point.  */

	/* Matches any character whose syntax is specified.  Followed by
           a byte which contains a syntax code, e.g., Sword.  */
  syntaxspec,

	/* Matches any character whose syntax is not that specified.  */
  notsyntaxspec
#endif /* emacs */
} re_opcode_t;

/* Common operations on the compiled pattern.  */

/* Store NUMBER in two contiguous bytes starting at DESTINATION.  */

#define STORE_NUMBER(destination, number)				\
  do {									\
    (destination)[0] = (number) & 0377;					\
    (destination)[1] = (number) >> 8;					\
  } while (0)

/* Same as STORE_NUMBER, except increment DESTINATION to
   the byte after where the number is stored.  Therefore, DESTINATION
   must be an lvalue.  */

#define STORE_NUMBER_AND_INCR(destination, number)			\
  do {									\
    STORE_NUMBER (destination, number);					\
    (destination) += 2;							\
  } while (0)

/* Put into DESTINATION a number stored in two contiguous bytes starting
   at SOURCE.  */

#define EXTRACT_NUMBER(destination, source)				\
  do {									\
    (destination) = *(source) & 0377;					\
    (destination) += SIGN_EXTEND_CHAR (*((source) + 1)) << 8;		\
  } while (0)

#ifdef DEBUG
static void extract_number _RE_ARGS ((int *dest, unsigned char *source));
static void
extract_number (dest, source)
    int *dest;
    unsigned char *source;
{
  int temp = SIGN_EXTEND_CHAR (*(source + 1));
  *dest = *source & 0377;
  *dest += temp << 8;
}

# ifndef EXTRACT_MACROS /* To debug the macros.  */
#  undef EXTRACT_NUMBER
#  define EXTRACT_NUMBER(dest, src) extract_number (&dest, src)
# endif /* not EXTRACT_MACROS */

#endif /* DEBUG */

/* Same as EXTRACT_NUMBER, except increment SOURCE to after the number.
   SOURCE must be an lvalue.  */

#define EXTRACT_NUMBER_AND_INCR(destination, source)			\
  do {									\
    EXTRACT_NUMBER (destination, source);				\
    (source) += 2; 							\
  } while (0)

#ifdef DEBUG
static void extract_number_and_incr _RE_ARGS ((int *destination,
					       unsigned char **source));
static void
extract_number_and_incr (destination, source)
    int *destination;
    unsigned char **source;
{
  extract_number (destination, *source);
  *source += 2;
}

# ifndef EXTRACT_MACROS
#  undef EXTRACT_NUMBER_AND_INCR
#  define EXTRACT_NUMBER_AND_INCR(dest, src) \
  extract_number_and_incr (&dest, &src)
# endif /* not EXTRACT_MACROS */

#endif /* DEBUG */

/* If DEBUG is defined, Regex prints many voluminous messages about what
   it is doing (if the variable `debug' is nonzero).  If linked with the
   main program in `iregex.c', you can enter patterns and strings
   interactively.  And if linked with the main program in `main.c' and
   the other test files, you can run the already-written tests.  */

#ifdef DEBUG

/* We use standard I/O for debugging.  */
# include <stdio.h>

/* It is useful to test things that ``must'' be true when debugging.  */
# include <assert.h>

static int debug = 0;

# define DEBUG_STATEMENT(e) e
# define DEBUG_PRINT1(x) if (debug) printf (x)
# define DEBUG_PRINT2(x1, x2) if (debug) printf (x1, x2)
# define DEBUG_PRINT3(x1, x2, x3) if (debug) printf (x1, x2, x3)
# define DEBUG_PRINT4(x1, x2, x3, x4) if (debug) printf (x1, x2, x3, x4)
# define DEBUG_PRINT_COMPILED_PATTERN(p, s, e) 				\
  if (debug) print_partial_compiled_pattern (s, e)
# define DEBUG_PRINT_DOUBLE_STRING(w, s1, sz1, s2, sz2)			\
  if (debug) print_double_string (w, s1, sz1, s2, sz2)


/* Print the fastmap in human-readable form.  */

void
print_fastmap (fastmap)
    char *fastmap;
{
  unsigned was_a_range = 0;
  unsigned i = 0;

  while (i < (1 << BYTEWIDTH))
    {
      if (fastmap[i++])
	{
	  was_a_range = 0;
          putchar (i - 1);
          while (i < (1 << BYTEWIDTH)  &&  fastmap[i])
            {
              was_a_range = 1;
              i++;
            }
	  if (was_a_range)
            {
              printf ("-");
              putchar (i - 1);
            }
        }
    }
  putchar ('\n');
}


/* Print a compiled pattern string in human-readable form, starting at
   the START pointer into it and ending just before the pointer END.  */

void
print_partial_compiled_pattern (start, end)
    unsigned char *start;
    unsigned char *end;
{
  int mcnt, mcnt2;
  unsigned char *p1;
  unsigned char *p = start;
  unsigned char *pend = end;

  if (start == NULL)
    {
      printf ("(null)\n");
      return;
    }

  /* Loop over pattern commands.  */
  while (p < pend)
    {
      printf ("%d:\t", p - start);

      switch ((re_opcode_t) *p++)
	{
        case no_op:
          printf ("/no_op");
          break;

	case exactn:
	  mcnt = *p++;
          printf ("/exactn/%d", mcnt);
          do
	    {
              putchar ('/');
	      putchar (*p++);
            }
          while (--mcnt);
          break;

	case start_memory:
          mcnt = *p++;
          printf ("/start_memory/%d/%d", mcnt, *p++);
          break;

	case stop_memory:
          mcnt = *p++;
	  printf ("/stop_memory/%d/%d", mcnt, *p++);
          break;

	case duplicate:
	  printf ("/duplicate/%d", *p++);
	  break;

	case anychar:
	  printf ("/anychar");
	  break;

	case charset:
        case charset_not:
          {
            register int c, last = -100;
	    register int in_range = 0;

	    printf ("/charset [%s",
	            (re_opcode_t) *(p - 1) == charset_not ? "^" : "");

            assert (p + *p < pend);

            for (c = 0; c < 256; c++)
	      if (c / 8 < *p
		  && (p[1 + (c/8)] & (1 << (c % 8))))
		{
		  /* Are we starting a range?  */
		  if (last + 1 == c && ! in_range)
		    {
		      putchar ('-');
		      in_range = 1;
		    }
		  /* Have we broken a range?  */
		  else if (last + 1 != c && in_range)
              {
		      putchar (last);
		      in_range = 0;
		    }

		  if (! in_range)
		    putchar (c);

		  last = c;
              }

	    if (in_range)
	      putchar (last);

	    putchar (']');

	    p += 1 + *p;
	  }
	  break;

	case begline:
	  printf ("/begline");
          break;

	case endline:
          printf ("/endline");
          break;

	case on_failure_jump:
          extract_number_and_incr (&mcnt, &p);
  	  printf ("/on_failure_jump to %d", p + mcnt - start);
          break;

	case on_failure_keep_string_jump:
          extract_number_and_incr (&mcnt, &p);
  	  printf ("/on_failure_keep_string_jump to %d", p + mcnt - start);
          break;

	case dummy_failure_jump:
          extract_number_and_incr (&mcnt, &p);
  	  printf ("/dummy_failure_jump to %d", p + mcnt - start);
          break;

	case push_dummy_failure:
          printf ("/push_dummy_failure");
          break;

        case maybe_pop_jump:
          extract_number_and_incr (&mcnt, &p);
  	  printf ("/maybe_pop_jump to %d", p + mcnt - start);
	  break;

        case pop_failure_jump:
	  extract_number_and_incr (&mcnt, &p);
  	  printf ("/pop_failure_jump to %d", p + mcnt - start);
	  break;

        case jump_past_alt:
	  extract_number_and_incr (&mcnt, &p);
  	  printf ("/jump_past_alt to %d", p + mcnt - start);
	  break;

        case jump:
	  extract_number_and_incr (&mcnt, &p);
  	  printf ("/jump to %d", p + mcnt - start);
	  break;

        case succeed_n:
          extract_number_and_incr (&mcnt, &p);
	  p1 = p + mcnt;
          extract_number_and_incr (&mcnt2, &p);
	  printf ("/succeed_n to %d, %d times", p1 - start, mcnt2);
          break;

        case jump_n:
          extract_number_and_incr (&mcnt, &p);
	  p1 = p + mcnt;
          extract_number_and_incr (&mcnt2, &p);
	  printf ("/jump_n to %d, %d times", p1 - start, mcnt2);
          break;

        case set_number_at:
          extract_number_and_incr (&mcnt, &p);
	  p1 = p + mcnt;
          extract_number_and_incr (&mcnt2, &p);
	  printf ("/set_number_at location %d to %d", p1 - start, mcnt2);
          break;

        case wordbound:
	  printf ("/wordbound");
	  break;

	case notwordbound:
	  printf ("/notwordbound");
          break;

	case wordbeg:
	  printf ("/wordbeg");
	  break;

	case wordend:
	  printf ("/wordend");

# ifdef emacs
	case before_dot:
	  printf ("/before_dot");
          break;

	case at_dot:
	  printf ("/at_dot");
          break;

	case after_dot:
	  printf ("/after_dot");
          break;

	case syntaxspec:
          printf ("/syntaxspec");
	  mcnt = *p++;
	  printf ("/%d", mcnt);
          break;

	case notsyntaxspec:
          printf ("/notsyntaxspec");
	  mcnt = *p++;
	  printf ("/%d", mcnt);
	  break;
# endif /* emacs */

	case wordchar:
	  printf ("/wordchar");
          break;

	case notwordchar:
	  printf ("/notwordchar");
          break;

	case begbuf:
	  printf ("/begbuf");
          break;

	case endbuf:
	  printf ("/endbuf");
          break;

        default:
          printf ("?%d", *(p-1));
	}

      putchar ('\n');
    }

  printf ("%d:\tend of pattern.\n", p - start);
}


void
print_compiled_pattern (bufp)
    struct re_pattern_buffer *bufp;
{
  unsigned char *buffer = bufp->buffer;

  print_partial_compiled_pattern (buffer, buffer + bufp->used);
  printf ("%ld bytes used/%ld bytes allocated.\n",
	  bufp->used, bufp->allocated);

  if (bufp->fastmap_accurate && bufp->fastmap)
    {
      printf ("fastmap: ");
      print_fastmap (bufp->fastmap);
    }

  printf ("re_nsub: %d\t", bufp->re_nsub);
  printf ("regs_alloc: %d\t", bufp->regs_allocated);
  printf ("can_be_null: %d\t", bufp->can_be_null);
  printf ("newline_anchor: %d\n", bufp->newline_anchor);
  printf ("no_sub: %d\t", bufp->no_sub);
  printf ("not_bol: %d\t", bufp->not_bol);
  printf ("not_eol: %d\t", bufp->not_eol);
  printf ("syntax: %lx\n", bufp->syntax);
  /* Perhaps we should print the translate table?  */
}


void
print_double_string (where, string1, size1, string2, size2)
    const char *where;
    const char *string1;
    const char *string2;
    int size1;
    int size2;
{
  int this_char;

  if (where == NULL)
    printf ("(null)");
  else
    {
      if (FIRST_STRING_P (where))
        {
          for (this_char = where - string1; this_char < size1; this_char++)
            putchar (string1[this_char]);

          where = string2;
        }

      for (this_char = where - string2; this_char < size2; this_char++)
        putchar (string2[this_char]);
    }
}

void
printchar (c)
     int c;
{
  putc (c, stderr);
}

#else /* not DEBUG */

# undef assert
# define assert(e)

# define DEBUG_STATEMENT(e)
# define DEBUG_PRINT1(x)
# define DEBUG_PRINT2(x1, x2)
# define DEBUG_PRINT3(x1, x2, x3)
# define DEBUG_PRINT4(x1, x2, x3, x4)
# define DEBUG_PRINT_COMPILED_PATTERN(p, s, e)
# define DEBUG_PRINT_DOUBLE_STRING(w, s1, sz1, s2, sz2)

#endif /* not DEBUG */

/* Set by `re_set_syntax' to the current regexp syntax to recognize.  Can
   also be assigned to arbitrarily: each pattern buffer stores its own
   syntax, so it can be changed between regex compilations.  */
/* This has no initializer because initialized variables in Emacs
   become read-only after dumping.  */
reg_syntax_t re_syntax_options;


/* Specify the precise syntax of regexps for compilation.  This provides
   for compatibility for various utilities which historically have
   different, incompatible syntaxes.

   The argument SYNTAX is a bit mask comprised of the various bits
   defined in regex-gnu.h.  We return the old syntax.  */

reg_syntax_t
re_set_syntax (syntax)
    reg_syntax_t syntax;
{
  reg_syntax_t ret = re_syntax_options;

  re_syntax_options = syntax;
#ifdef DEBUG
  if (syntax & RE_DEBUG)
    debug = 1;
  else if (debug) /* was on but now is not */
    debug = 0;
#endif /* DEBUG */
  return ret;
}
#ifdef _LIBC
weak_alias (__re_set_syntax, re_set_syntax)
#endif

/* This table gives an error message for each of the error codes listed
   in regex-gnu.h.  Obviously the order here has to be same as there.
   POSIX doesn't require that we do anything for REG_NOERROR,
   but why not be nice?  */

static const char *re_error_msgid[] =
  {
    gettext_noop ("Success"),	/* REG_NOERROR */
    gettext_noop ("No match"),	/* REG_NOMATCH */
    gettext_noop ("Invalid regular expression"), /* REG_BADPAT */
    gettext_noop ("Invalid collation character"), /* REG_ECOLLATE */
    gettext_noop ("Invalid character class name"), /* REG_ECTYPE */
    gettext_noop ("Trailing backslash"), /* REG_EESCAPE */
    gettext_noop ("Invalid back reference"), /* REG_ESUBREG */
    gettext_noop ("Unmatched [ or [^"),	/* REG_EBRACK */
    gettext_noop ("Unmatched ( or \\("), /* REG_EPAREN */
    gettext_noop ("Unmatched \\{"), /* REG_EBRACE */
    gettext_noop ("Invalid content of \\{\\}"), /* REG_BADBR */
    gettext_noop ("Invalid range end"),	/* REG_ERANGE */
    gettext_noop ("Memory exhausted"), /* REG_ESPACE */
    gettext_noop ("Invalid preceding regular expression"), /* REG_BADRPT */
    gettext_noop ("Premature end of regular expression"), /* REG_EEND */
    gettext_noop ("Regular expression too big"), /* REG_ESIZE */
    gettext_noop ("Unmatched ) or \\)"), /* REG_ERPAREN */
  };

/* Avoiding alloca during matching, to placate r_alloc.  */

/* Define MATCH_MAY_ALLOCATE unless we need to make sure that the
   searching and matching functions should not call alloca.  On some
   systems, alloca is implemented in terms of malloc, and if we're
   using the relocating allocator routines, then malloc could cause a
   relocation, which might (if the strings being searched are in the
   ralloc heap) shift the data out from underneath the regexp
   routines.

   Here's another reason to avoid allocation: Emacs
   processes input from X in a signal handler; processing X input may
   call malloc; if input arrives while a matching routine is calling
   malloc, then we're scrod.  But Emacs can't just block input while
   calling matching routines; then we don't notice interrupts when
   they come in.  So, Emacs blocks input around all regexp calls
   except the matching calls, which it leaves unprotected, in the
   faith that they will not malloc.  */

/* Normally, this is fine.  */
#define MATCH_MAY_ALLOCATE

/* When using GNU C, we are not REALLY using the C alloca, no matter
   what config.h may say.  So don't take precautions for it.  */
#ifdef __GNUC__
# undef C_ALLOCA
#endif

/* The match routines may not allocate if (1) they would do it with malloc
   and (2) it's not safe for them to use malloc.
   Note that if REL_ALLOC is defined, matching would not use malloc for the
   failure stack, but we would still use it for the register vectors;
   so REL_ALLOC should not affect this.  */
#if (defined C_ALLOCA || defined REGEX_MALLOC) && defined emacs
# undef MATCH_MAY_ALLOCATE
#endif


/* Failure stack declarations and macros; both re_compile_fastmap and
   re_match_2 use a failure stack.  These have to be macros because of
   REGEX_ALLOCATE_STACK.  */


/* Number of failure points for which to initially allocate space
   when matching.  If this number is exceeded, we allocate more
   space, so it is not a hard limit.  */
#ifndef INIT_FAILURE_ALLOC
# define INIT_FAILURE_ALLOC 5
#endif

/* Roughly the maximum number of failure points on the stack.  Would be
   exactly that if always used MAX_FAILURE_ITEMS items each time we failed.
   This is a variable only so users of regex can assign to it; we never
   change it ourselves.  */

#ifdef INT_IS_16BIT

# if defined MATCH_MAY_ALLOCATE
/* 4400 was enough to cause a crash on Alpha OSF/1,
   whose default stack limit is 2mb.  */
long int re_max_failures = 4000;
# else
long int re_max_failures = 2000;
# endif

union fail_stack_elt
{
  unsigned char *pointer;
  long int integer;
};

typedef union fail_stack_elt fail_stack_elt_t;

typedef struct
{
  fail_stack_elt_t *stack;
  unsigned long int size;
  unsigned long int avail;		/* Offset of next open position.  */
} fail_stack_type;

#else /* not INT_IS_16BIT */

# if defined MATCH_MAY_ALLOCATE
/* 4400 was enough to cause a crash on Alpha OSF/1,
   whose default stack limit is 2mb.  */
int re_max_failures = 20000;
# else
int re_max_failures = 2000;
# endif

union fail_stack_elt
{
  unsigned char *pointer;
  int integer;
};

typedef union fail_stack_elt fail_stack_elt_t;

typedef struct
{
  fail_stack_elt_t *stack;
  unsigned size;
  unsigned avail;			/* Offset of next open position.  */
} fail_stack_type;

#endif /* INT_IS_16BIT */

#define FAIL_STACK_EMPTY()     (fail_stack.avail == 0)
#define FAIL_STACK_PTR_EMPTY() (fail_stack_ptr->avail == 0)
#define FAIL_STACK_FULL()      (fail_stack.avail == fail_stack.size)


/* Define macros to initialize and free the failure stack.
   Do `return -2' if the alloc fails.  */

#ifdef MATCH_MAY_ALLOCATE
# define INIT_FAIL_STACK()						\
  do {									\
    fail_stack.stack = (fail_stack_elt_t *)				\
      REGEX_ALLOCATE_STACK (INIT_FAILURE_ALLOC * sizeof (fail_stack_elt_t)); \
									\
    if (fail_stack.stack == NULL)					\
      return -2;							\
									\
    fail_stack.size = INIT_FAILURE_ALLOC;				\
    fail_stack.avail = 0;						\
  } while (0)

# define RESET_FAIL_STACK()  REGEX_FREE_STACK (fail_stack.stack)
#else
# define INIT_FAIL_STACK()						\
  do {									\
    fail_stack.avail = 0;						\
  } while (0)

# define RESET_FAIL_STACK()
#endif


/* Double the size of FAIL_STACK, up to approximately `re_max_failures' items.

   Return 1 if succeeds, and 0 if either ran out of memory
   allocating space for it or it was already too large.

   REGEX_REALLOCATE_STACK requires `destination' be declared.   */

#define DOUBLE_FAIL_STACK(fail_stack)					\
  ((fail_stack).size > (unsigned) (re_max_failures * MAX_FAILURE_ITEMS)	\
   ? 0									\
   : ((fail_stack).stack = (fail_stack_elt_t *)				\
        REGEX_REALLOCATE_STACK ((fail_stack).stack, 			\
          (fail_stack).size * sizeof (fail_stack_elt_t),		\
          ((fail_stack).size << 1) * sizeof (fail_stack_elt_t)),	\
									\
      (fail_stack).stack == NULL					\
      ? 0								\
      : ((fail_stack).size <<= 1, 					\
         1)))


/* Push pointer POINTER on FAIL_STACK.
   Return 1 if was able to do so and 0 if ran out of memory allocating
   space to do so.  */
#define PUSH_PATTERN_OP(POINTER, FAIL_STACK)				\
  ((FAIL_STACK_FULL ()							\
    && !DOUBLE_FAIL_STACK (FAIL_STACK))					\
   ? 0									\
   : ((FAIL_STACK).stack[(FAIL_STACK).avail++].pointer = POINTER,	\
      1))

/* Push a pointer value onto the failure stack.
   Assumes the variable `fail_stack'.  Probably should only
   be called from within `PUSH_FAILURE_POINT'.  */
#define PUSH_FAILURE_POINTER(item)					\
  fail_stack.stack[fail_stack.avail++].pointer = (unsigned char *) (item)

/* This pushes an integer-valued item onto the failure stack.
   Assumes the variable `fail_stack'.  Probably should only
   be called from within `PUSH_FAILURE_POINT'.  */
#define PUSH_FAILURE_INT(item)					\
  fail_stack.stack[fail_stack.avail++].integer = (item)

/* Push a fail_stack_elt_t value onto the failure stack.
   Assumes the variable `fail_stack'.  Probably should only
   be called from within `PUSH_FAILURE_POINT'.  */
#define PUSH_FAILURE_ELT(item)					\
  fail_stack.stack[fail_stack.avail++] =  (item)

/* These three POP... operations complement the three PUSH... operations.
   All assume that `fail_stack' is nonempty.  */
#define POP_FAILURE_POINTER() fail_stack.stack[--fail_stack.avail].pointer
#define POP_FAILURE_INT() fail_stack.stack[--fail_stack.avail].integer
#define POP_FAILURE_ELT() fail_stack.stack[--fail_stack.avail]

/* Used to omit pushing failure point id's when we're not debugging.  */
#ifdef DEBUG
# define DEBUG_PUSH PUSH_FAILURE_INT
# define DEBUG_POP(item_addr) *(item_addr) = POP_FAILURE_INT ()
#else
# define DEBUG_PUSH(item)
# define DEBUG_POP(item_addr)
#endif


/* Push the information about the state we will need
   if we ever fail back to it.

   Requires variables fail_stack, regstart, regend, reg_info, and
   num_regs_pushed be declared.  DOUBLE_FAIL_STACK requires `destination'
   be declared.

   Does `return FAILURE_CODE' if runs out of memory.  */

#define PUSH_FAILURE_POINT(pattern_place, string_place, failure_code)	\
  do {									\
    char *destination;							\
    /* Must be int, so when we don't save any registers, the arithmetic	\
       of 0 + -1 isn't done as unsigned.  */				\
    /* Can't be int, since there is not a shred of a guarantee that int	\
       is wide enough to hold a value of something to which pointer can	\
       be assigned */							\
    active_reg_t this_reg;						\
    									\
    DEBUG_STATEMENT (failure_id++);					\
    DEBUG_STATEMENT (nfailure_points_pushed++);				\
    DEBUG_PRINT2 ("\nPUSH_FAILURE_POINT #%u:\n", failure_id);		\
    DEBUG_PRINT2 ("  Before push, next avail: %d\n", (fail_stack).avail);\
    DEBUG_PRINT2 ("                     size: %d\n", (fail_stack).size);\
									\
    DEBUG_PRINT2 ("  slots needed: %ld\n", NUM_FAILURE_ITEMS);		\
    DEBUG_PRINT2 ("     available: %d\n", REMAINING_AVAIL_SLOTS);	\
									\
    /* Ensure we have enough space allocated for what we will push.  */	\
    while (REMAINING_AVAIL_SLOTS < NUM_FAILURE_ITEMS)			\
      {									\
        if (!DOUBLE_FAIL_STACK (fail_stack))				\
          return failure_code;						\
									\
        DEBUG_PRINT2 ("\n  Doubled stack; size now: %d\n",		\
		       (fail_stack).size);				\
        DEBUG_PRINT2 ("  slots available: %d\n", REMAINING_AVAIL_SLOTS);\
      }									\
									\
    /* Push the info, starting with the registers.  */			\
    DEBUG_PRINT1 ("\n");						\
									\
    if (1)								\
      for (this_reg = lowest_active_reg; this_reg <= highest_active_reg; \
	   this_reg++)							\
	{								\
	  DEBUG_PRINT2 ("  Pushing reg: %lu\n", this_reg);		\
	  DEBUG_STATEMENT (num_regs_pushed++);				\
									\
	  DEBUG_PRINT2 ("    start: %p\n", regstart[this_reg]);		\
	  PUSH_FAILURE_POINTER (regstart[this_reg]);			\
									\
	  DEBUG_PRINT2 ("    end: %p\n", regend[this_reg]);		\
	  PUSH_FAILURE_POINTER (regend[this_reg]);			\
									\
	  DEBUG_PRINT2 ("    info: %p\n      ",				\
			reg_info[this_reg].word.pointer);		\
	  DEBUG_PRINT2 (" match_null=%d",				\
			REG_MATCH_NULL_STRING_P (reg_info[this_reg]));	\
	  DEBUG_PRINT2 (" active=%d", IS_ACTIVE (reg_info[this_reg]));	\
	  DEBUG_PRINT2 (" matched_something=%d",			\
			MATCHED_SOMETHING (reg_info[this_reg]));	\
	  DEBUG_PRINT2 (" ever_matched=%d",				\
			EVER_MATCHED_SOMETHING (reg_info[this_reg]));	\
	  DEBUG_PRINT1 ("\n");						\
	  PUSH_FAILURE_ELT (reg_info[this_reg].word);			\
	}								\
									\
    DEBUG_PRINT2 ("  Pushing  low active reg: %ld\n", lowest_active_reg);\
    PUSH_FAILURE_INT (lowest_active_reg);				\
									\
    DEBUG_PRINT2 ("  Pushing high active reg: %ld\n", highest_active_reg);\
    PUSH_FAILURE_INT (highest_active_reg);				\
									\
    DEBUG_PRINT2 ("  Pushing pattern %p:\n", pattern_place);		\
    DEBUG_PRINT_COMPILED_PATTERN (bufp, pattern_place, pend);		\
    PUSH_FAILURE_POINTER (pattern_place);				\
									\
    DEBUG_PRINT2 ("  Pushing string %p: `", string_place);		\
    DEBUG_PRINT_DOUBLE_STRING (string_place, string1, size1, string2,   \
				 size2);				\
    DEBUG_PRINT1 ("'\n");						\
    PUSH_FAILURE_POINTER (string_place);				\
									\
    DEBUG_PRINT2 ("  Pushing failure id: %u\n", failure_id);		\
    DEBUG_PUSH (failure_id);						\
  } while (0)

/* This is the number of items that are pushed and popped on the stack
   for each register.  */
#define NUM_REG_ITEMS  3

/* Individual items aside from the registers.  */
#ifdef DEBUG
# define NUM_NONREG_ITEMS 5 /* Includes failure point id.  */
#else
# define NUM_NONREG_ITEMS 4
#endif

/* We push at most this many items on the stack.  */
/* We used to use (num_regs - 1), which is the number of registers
   this regexp will save; but that was changed to 5
   to avoid stack overflow for a regexp with lots of parens.  */
#define MAX_FAILURE_ITEMS (5 * NUM_REG_ITEMS + NUM_NONREG_ITEMS)

/* We actually push this many items.  */
#define NUM_FAILURE_ITEMS				\
  (((0							\
     ? 0 : highest_active_reg - lowest_active_reg + 1)	\
    * NUM_REG_ITEMS)					\
   + NUM_NONREG_ITEMS)

/* How many items can still be added to the stack without overflowing it.  */
#define REMAINING_AVAIL_SLOTS ((fail_stack).size - (fail_stack).avail)


/* Pops what PUSH_FAIL_STACK pushes.

   We restore into the parameters, all of which should be lvalues:
     STR -- the saved data position.
     PAT -- the saved pattern position.
     LOW_REG, HIGH_REG -- the highest and lowest active registers.
     REGSTART, REGEND -- arrays of string positions.
     REG_INFO -- array of information about each subexpression.

   Also assumes the variables `fail_stack' and (if debugging), `bufp',
   `pend', `string1', `size1', `string2', and `size2'.  */

#define POP_FAILURE_POINT(str, pat, low_reg, high_reg, regstart, regend, reg_info)\
{									\
  DEBUG_STATEMENT (unsigned failure_id;)				\
  active_reg_t this_reg;						\
  const unsigned char *string_temp;					\
									\
  assert (!FAIL_STACK_EMPTY ());					\
									\
  /* Remove failure points and point to how many regs pushed.  */	\
  DEBUG_PRINT1 ("POP_FAILURE_POINT:\n");				\
  DEBUG_PRINT2 ("  Before pop, next avail: %d\n", fail_stack.avail);	\
  DEBUG_PRINT2 ("                    size: %d\n", fail_stack.size);	\
									\
  assert (fail_stack.avail >= NUM_NONREG_ITEMS);			\
									\
  DEBUG_POP (&failure_id);						\
  DEBUG_PRINT2 ("  Popping failure id: %u\n", failure_id);		\
									\
  /* If the saved string location is NULL, it came from an		\
     on_failure_keep_string_jump opcode, and we want to throw away the	\
     saved NULL, thus retaining our current position in the string.  */	\
  string_temp = POP_FAILURE_POINTER ();					\
  if (string_temp != NULL)						\
    str = (const char *) string_temp;					\
									\
  DEBUG_PRINT2 ("  Popping string %p: `", str);				\
  DEBUG_PRINT_DOUBLE_STRING (str, string1, size1, string2, size2);	\
  DEBUG_PRINT1 ("'\n");							\
									\
  pat = (unsigned char *) POP_FAILURE_POINTER ();			\
  DEBUG_PRINT2 ("  Popping pattern %p:\n", pat);			\
  DEBUG_PRINT_COMPILED_PATTERN (bufp, pat, pend);			\
									\
  /* Restore register info.  */						\
  high_reg = (active_reg_t) POP_FAILURE_INT ();				\
  DEBUG_PRINT2 ("  Popping high active reg: %ld\n", high_reg);		\
									\
  low_reg = (active_reg_t) POP_FAILURE_INT ();				\
  DEBUG_PRINT2 ("  Popping  low active reg: %ld\n", low_reg);		\
									\
  if (1)								\
    for (this_reg = high_reg; this_reg >= low_reg; this_reg--)		\
      {									\
	DEBUG_PRINT2 ("    Popping reg: %ld\n", this_reg);		\
									\
	reg_info[this_reg].word = POP_FAILURE_ELT ();			\
	DEBUG_PRINT2 ("      info: %p\n",				\
		      reg_info[this_reg].word.pointer);			\
									\
	regend[this_reg] = (const char *) POP_FAILURE_POINTER ();	\
	DEBUG_PRINT2 ("      end: %p\n", regend[this_reg]);		\
									\
	regstart[this_reg] = (const char *) POP_FAILURE_POINTER ();	\
	DEBUG_PRINT2 ("      start: %p\n", regstart[this_reg]);		\
      }									\
  else									\
    {									\
      for (this_reg = highest_active_reg; this_reg > high_reg; this_reg--) \
	{								\
	  reg_info[this_reg].word.integer = 0;				\
	  regend[this_reg] = 0;						\
	  regstart[this_reg] = 0;					\
	}								\
      highest_active_reg = high_reg;					\
    }									\
									\
  set_regs_matched_done = 0;						\
  DEBUG_STATEMENT (nfailure_points_popped++);				\
} /* POP_FAILURE_POINT */



/* Structure for per-register (a.k.a. per-group) information.
   Other register information, such as the
   starting and ending positions (which are addresses), and the list of
   inner groups (which is a bits list) are maintained in separate
   variables.

   We are making a (strictly speaking) nonportable assumption here: that
   the compiler will pack our bit fields into something that fits into
   the type of `word', i.e., is something that fits into one item on the
   failure stack.  */


/* Declarations and macros for re_match_2.  */

typedef union
{
  fail_stack_elt_t word;
  struct
  {
      /* This field is one if this group can match the empty string,
         zero if not.  If not yet determined,  `MATCH_NULL_UNSET_VALUE'.  */
#define MATCH_NULL_UNSET_VALUE 3
    unsigned match_null_string_p : 2;
    unsigned is_active : 1;
    unsigned matched_something : 1;
    unsigned ever_matched_something : 1;
  } bits;
} register_info_type;

#define REG_MATCH_NULL_STRING_P(R)  ((R).bits.match_null_string_p)
#define IS_ACTIVE(R)  ((R).bits.is_active)
#define MATCHED_SOMETHING(R)  ((R).bits.matched_something)
#define EVER_MATCHED_SOMETHING(R)  ((R).bits.ever_matched_something)


/* Call this when have matched a real character; it sets `matched' flags
   for the subexpressions which we are currently inside.  Also records
   that those subexprs have matched.  */
#define SET_REGS_MATCHED()						\
  do									\
    {									\
      if (!set_regs_matched_done)					\
	{								\
	  active_reg_t r;						\
	  set_regs_matched_done = 1;					\
	  for (r = lowest_active_reg; r <= highest_active_reg; r++)	\
	    {								\
	      MATCHED_SOMETHING (reg_info[r])				\
		= EVER_MATCHED_SOMETHING (reg_info[r])			\
		= 1;							\
	    }								\
	}								\
    }									\
  while (0)

/* Registers are set to a sentinel when they haven't yet matched.  */
static char reg_unset_dummy;
#define REG_UNSET_VALUE (&reg_unset_dummy)
#define REG_UNSET(e) ((e) == REG_UNSET_VALUE)

/* Subroutine declarations and macros for regex_compile.  */

static reg_errcode_t regex_compile _RE_ARGS ((const char *pattern, size_t size,
					      reg_syntax_t syntax,
					      struct re_pattern_buffer *bufp));
static void store_op1 _RE_ARGS ((re_opcode_t op, unsigned char *loc, int arg));
static void store_op2 _RE_ARGS ((re_opcode_t op, unsigned char *loc,
				 int arg1, int arg2));
static void insert_op1 _RE_ARGS ((re_opcode_t op, unsigned char *loc,
				  int arg, unsigned char *end));
static void insert_op2 _RE_ARGS ((re_opcode_t op, unsigned char *loc,
				  int arg1, int arg2, unsigned char *end));
static boolean at_begline_loc_p _RE_ARGS ((const char *pattern, const char *p,
					   reg_syntax_t syntax));
static boolean at_endline_loc_p _RE_ARGS ((const char *p, const char *pend,
					   reg_syntax_t syntax));
static reg_errcode_t compile_range _RE_ARGS ((const char **p_ptr,
					      const char *pend,
					      char *translate,
					      reg_syntax_t syntax,
					      unsigned char *b));

/* Fetch the next character in the uncompiled pattern---translating it
   if necessary.  Also cast from a signed character in the constant
   string passed to us by the user to an unsigned char that we can use
   as an array index (in, e.g., `translate').  */
#ifndef PATFETCH
# define PATFETCH(c)							\
  do {if (p == pend) return REG_EEND;					\
    c = (unsigned char) *p++;						\
    if (translate) c = (unsigned char) translate[c];			\
  } while (0)
#endif

/* Fetch the next character in the uncompiled pattern, with no
   translation.  */
#define PATFETCH_RAW(c)							\
  do {if (p == pend) return REG_EEND;					\
    c = (unsigned char) *p++; 						\
  } while (0)

/* Go backwards one character in the pattern.  */
#define PATUNFETCH p--


/* If `translate' is non-null, return translate[D], else just D.  We
   cast the subscript to translate because some data is declared as
   `char *', to avoid warnings when a string constant is passed.  But
   when we use a character as a subscript we must make it unsigned.  */
#ifndef TRANSLATE
# define TRANSLATE(d) \
  (translate ? (char) translate[(unsigned char) (d)] : (d))
#endif


/* Macros for outputting the compiled pattern into `buffer'.  */

/* If the buffer isn't allocated when it comes in, use this.  */
#define INIT_BUF_SIZE  32

/* Make sure we have at least N more bytes of space in buffer.  */
#define GET_BUFFER_SPACE(n)						\
    while ((unsigned long) (b - bufp->buffer + (n)) > bufp->allocated)	\
      EXTEND_BUFFER ()

/* Make sure we have one more byte of buffer space and then add C to it.  */
#define BUF_PUSH(c)							\
  do {									\
    GET_BUFFER_SPACE (1);						\
    *b++ = (unsigned char) (c);						\
  } while (0)


/* Ensure we have two more bytes of buffer space and then append C1 and C2.  */
#define BUF_PUSH_2(c1, c2)						\
  do {									\
    GET_BUFFER_SPACE (2);						\
    *b++ = (unsigned char) (c1);					\
    *b++ = (unsigned char) (c2);					\
  } while (0)


/* As with BUF_PUSH_2, except for three bytes.  */
#define BUF_PUSH_3(c1, c2, c3)						\
  do {									\
    GET_BUFFER_SPACE (3);						\
    *b++ = (unsigned char) (c1);					\
    *b++ = (unsigned char) (c2);					\
    *b++ = (unsigned char) (c3);					\
  } while (0)


/* Store a jump with opcode OP at LOC to location TO.  We store a
   relative address offset by the three bytes the jump itself occupies.  */
#define STORE_JUMP(op, loc, to) \
  store_op1 (op, loc, (int) ((to) - (loc) - 3))

/* Likewise, for a two-argument jump.  */
#define STORE_JUMP2(op, loc, to, arg) \
  store_op2 (op, loc, (int) ((to) - (loc) - 3), arg)

/* Like `STORE_JUMP', but for inserting.  Assume `b' is the buffer end.  */
#define INSERT_JUMP(op, loc, to) \
  insert_op1 (op, loc, (int) ((to) - (loc) - 3), b)

/* Like `STORE_JUMP2', but for inserting.  Assume `b' is the buffer end.  */
#define INSERT_JUMP2(op, loc, to, arg) \
  insert_op2 (op, loc, (int) ((to) - (loc) - 3), arg, b)


/* This is not an arbitrary limit: the arguments which represent offsets
   into the pattern are two bytes long.  So if 2^16 bytes turns out to
   be too small, many things would have to change.  */
/* Any other compiler which, like MSC, has allocation limit below 2^16
   bytes will have to use approach similar to what was done below for
   MSC and drop MAX_BUF_SIZE a bit.  Otherwise you may end up
   reallocating to 0 bytes.  Such thing is not going to work too well.
   You have been warned!!  */
#if defined _MSC_VER  && !defined WIN32
/* Microsoft C 16-bit versions limit malloc to approx 65512 bytes.
   The REALLOC define eliminates a flurry of conversion warnings,
   but is not required. */
# define MAX_BUF_SIZE  65500L
# define EXTEND_BUFFER_REALLOC(p,s) realloc ((p), (size_t) (s))
#else
# define MAX_BUF_SIZE (1L << 16)
# define EXTEND_BUFFER_REALLOC(p,s) realloc ((p), (s))
#endif

/* Extend the buffer by twice its current size via realloc and
   reset the pointers that pointed into the old block to point to the
   correct places in the new one.  If extending the buffer results in it
   being larger than MAX_BUF_SIZE, then flag memory exhausted.  */
#define EXTEND_BUFFER()							\
  do { 									\
    unsigned char *old_buffer = bufp->buffer;				\
    if (bufp->allocated == MAX_BUF_SIZE) 				\
      return REG_ESIZE;							\
    bufp->allocated <<= 1;						\
    if (bufp->allocated > MAX_BUF_SIZE)					\
      bufp->allocated = MAX_BUF_SIZE; 					\
    bufp->buffer = (unsigned char *) EXTEND_BUFFER_REALLOC (bufp->buffer, bufp->allocated);\
    if (bufp->buffer == NULL)						\
      return REG_ESPACE;						\
    /* If the buffer moved, move all the pointers into it.  */		\
    if (old_buffer != bufp->buffer)					\
      {									\
        b = (b - old_buffer) + bufp->buffer;				\
        begalt = (begalt - old_buffer) + bufp->buffer;			\
        if (fixup_alt_jump)						\
          fixup_alt_jump = (fixup_alt_jump - old_buffer) + bufp->buffer;\
        if (laststart)							\
          laststart = (laststart - old_buffer) + bufp->buffer;		\
        if (pending_exact)						\
          pending_exact = (pending_exact - old_buffer) + bufp->buffer;	\
      }									\
  } while (0)


/* Since we have one byte reserved for the register number argument to
   {start,stop}_memory, the maximum number of groups we can report
   things about is what fits in that byte.  */
#define MAX_REGNUM 255

/* But patterns can have more than `MAX_REGNUM' registers.  We just
   ignore the excess.  */
typedef unsigned regnum_t;


/* Macros for the compile stack.  */

/* Since offsets can go either forwards or backwards, this type needs to
   be able to hold values from -(MAX_BUF_SIZE - 1) to MAX_BUF_SIZE - 1.  */
/* int may be not enough when sizeof(int) == 2.  */
typedef long pattern_offset_t;

typedef struct
{
  pattern_offset_t begalt_offset;
  pattern_offset_t fixup_alt_jump;
  pattern_offset_t inner_group_offset;
  pattern_offset_t laststart_offset;
  regnum_t regnum;
} compile_stack_elt_t;


typedef struct
{
  compile_stack_elt_t *stack;
  unsigned size;
  unsigned avail;			/* Offset of next open position.  */
} compile_stack_type;


#define INIT_COMPILE_STACK_SIZE 32

#define COMPILE_STACK_EMPTY  (compile_stack.avail == 0)
#define COMPILE_STACK_FULL  (compile_stack.avail == compile_stack.size)

/* The next available element.  */
#define COMPILE_STACK_TOP (compile_stack.stack[compile_stack.avail])


/* Set the bit for character C in a list.  */
#define SET_LIST_BIT(c)                               \
  (b[((unsigned char) (c)) / BYTEWIDTH]               \
   |= 1 << (((unsigned char) c) % BYTEWIDTH))


/* Get the next unsigned number in the uncompiled pattern.  */
#define GET_UNSIGNED_NUMBER(num) 					\
  { if (p != pend)							\
     {									\
       PATFETCH (c); 							\
       while (ISDIGIT (c)) 						\
         { 								\
           if (num < 0)							\
              num = 0;							\
           num = num * 10 + c - '0'; 					\
           if (p == pend) 						\
              break; 							\
           PATFETCH (c);						\
         } 								\
       } 								\
    }

#if defined _LIBC || (defined HAVE_WCTYPE_H && defined HAVE_WCHAR_H)
/* The GNU C library provides support for user-defined character classes
   and the functions from ISO C amendement 1.  */
# ifdef CHARCLASS_NAME_MAX
#  define CHAR_CLASS_MAX_LENGTH CHARCLASS_NAME_MAX
# else
/* This shouldn't happen but some implementation might still have this
   problem.  Use a reasonable default value.  */
#  define CHAR_CLASS_MAX_LENGTH 256
# endif

# ifdef _LIBC
#  define IS_CHAR_CLASS(string) __wctype (string)
# else
#  define IS_CHAR_CLASS(string) wctype (string)
# endif
#else
# define CHAR_CLASS_MAX_LENGTH  6 /* Namely, `xdigit'.  */

# define IS_CHAR_CLASS(string)						\
   (STREQ (string, "alpha") || STREQ (string, "upper")			\
    || STREQ (string, "lower") || STREQ (string, "digit")		\
    || STREQ (string, "alnum") || STREQ (string, "xdigit")		\
    || STREQ (string, "space") || STREQ (string, "print")		\
    || STREQ (string, "punct") || STREQ (string, "graph")		\
    || STREQ (string, "cntrl") || STREQ (string, "blank"))
#endif

#ifndef MATCH_MAY_ALLOCATE

/* If we cannot allocate large objects within re_match_2_internal,
   we make the fail stack and register vectors global.
   The fail stack, we grow to the maximum size when a regexp
   is compiled.
   The register vectors, we adjust in size each time we
   compile a regexp, according to the number of registers it needs.  */

static fail_stack_type fail_stack;

/* Size with which the following vectors are currently allocated.
   That is so we can make them bigger as needed,
   but never make them smaller.  */
static int regs_allocated_size;

static const char **     regstart, **     regend;
static const char ** old_regstart, ** old_regend;
static const char **best_regstart, **best_regend;
static register_info_type *reg_info;
static const char **reg_dummy;
static register_info_type *reg_info_dummy;

/* Make the register vectors big enough for NUM_REGS registers,
   but don't make them smaller.  */

static
regex_grow_registers (num_regs)
     int num_regs;
{
  if (num_regs > regs_allocated_size)
    {
      RETALLOC_IF (regstart,	 num_regs, const char *);
      RETALLOC_IF (regend,	 num_regs, const char *);
      RETALLOC_IF (old_regstart, num_regs, const char *);
      RETALLOC_IF (old_regend,	 num_regs, const char *);
      RETALLOC_IF (best_regstart, num_regs, const char *);
      RETALLOC_IF (best_regend,	 num_regs, const char *);
      RETALLOC_IF (reg_info,	 num_regs, register_info_type);
      RETALLOC_IF (reg_dummy,	 num_regs, const char *);
      RETALLOC_IF (reg_info_dummy, num_regs, register_info_type);

      regs_allocated_size = num_regs;
    }
}

#endif /* not MATCH_MAY_ALLOCATE */

static boolean group_in_compile_stack _RE_ARGS ((compile_stack_type
						 compile_stack,
						 regnum_t regnum));

/* `regex_compile' compiles PATTERN (of length SIZE) according to SYNTAX.
   Returns one of error codes defined in `regex-gnu.h', or zero for success.

   Assumes the `allocated' (and perhaps `buffer') and `translate'
   fields are set in BUFP on entry.

   If it succeeds, results are put in BUFP (if it returns an error, the
   contents of BUFP are undefined):
     `buffer' is the compiled pattern;
     `syntax' is set to SYNTAX;
     `used' is set to the length of the compiled pattern;
     `fastmap_accurate' is zero;
     `re_nsub' is the number of subexpressions in PATTERN;
     `not_bol' and `not_eol' are zero;

   The `fastmap' and `newline_anchor' fields are neither
   examined nor set.  */

/* Return, freeing storage we allocated.  */
#define FREE_STACK_RETURN(value)		\
  return (free (compile_stack.stack), value)

static reg_errcode_t
regex_compile (pattern, size, syntax, bufp)
     const char *pattern;
     size_t size;
     reg_syntax_t syntax;
     struct re_pattern_buffer *bufp;
{
  /* We fetch characters from PATTERN here.  Even though PATTERN is
     `char *' (i.e., signed), we declare these variables as unsigned, so
     they can be reliably used as array indices.  */
  register unsigned char c, c1;

  /* A random temporary spot in PATTERN.  */
  const char *p1;

  /* Points to the end of the buffer, where we should append.  */
  register unsigned char *b;

  /* Keeps track of unclosed groups.  */
  compile_stack_type compile_stack;

  /* Points to the current (ending) position in the pattern.  */
  const char *p = pattern;
  const char *pend = pattern + size;

  /* How to translate the characters in the pattern.  */
  RE_TRANSLATE_TYPE translate = bufp->translate;

  /* Address of the count-byte of the most recently inserted `exactn'
     command.  This makes it possible to tell if a new exact-match
     character can be added to that command or if the character requires
     a new `exactn' command.  */
  unsigned char *pending_exact = 0;

  /* Address of start of the most recently finished expression.
     This tells, e.g., postfix * where to find the start of its
     operand.  Reset at the beginning of groups and alternatives.  */
  unsigned char *laststart = 0;

  /* Address of beginning of regexp, or inside of last group.  */
  unsigned char *begalt;

  /* Place in the uncompiled pattern (i.e., the {) to
     which to go back if the interval is invalid.  */
  const char *beg_interval;

  /* Address of the place where a forward jump should go to the end of
     the containing expression.  Each alternative of an `or' -- except the
     last -- ends with a forward jump of this sort.  */
  unsigned char *fixup_alt_jump = 0;

  /* Counts open-groups as they are encountered.  Remembered for the
     matching close-group on the compile stack, so the same register
     number is put in the stop_memory as the start_memory.  */
  regnum_t regnum = 0;

#ifdef DEBUG
  DEBUG_PRINT1 ("\nCompiling pattern: ");
  if (debug)
    {
      unsigned debug_count;

      for (debug_count = 0; debug_count < size; debug_count++)
        putchar (pattern[debug_count]);
      putchar ('\n');
    }
#endif /* DEBUG */

  /* Initialize the compile stack.  */
  compile_stack.stack = TALLOC (INIT_COMPILE_STACK_SIZE, compile_stack_elt_t);
  if (compile_stack.stack == NULL)
    return REG_ESPACE;

  compile_stack.size = INIT_COMPILE_STACK_SIZE;
  compile_stack.avail = 0;

  /* Initialize the pattern buffer.  */
  bufp->syntax = syntax;
  bufp->fastmap_accurate = 0;
  bufp->not_bol = bufp->not_eol = 0;

  /* Set `used' to zero, so that if we return an error, the pattern
     printer (for debugging) will think there's no pattern.  We reset it
     at the end.  */
  bufp->used = 0;

  /* Always count groups, whether or not bufp->no_sub is set.  */
  bufp->re_nsub = 0;

#if !defined emacs && !defined SYNTAX_TABLE
  /* Initialize the syntax table.  */
   init_syntax_once ();
#endif

  if (bufp->allocated == 0)
    {
      if (bufp->buffer)
	{ /* If zero allocated, but buffer is non-null, try to realloc
             enough space.  This loses if buffer's address is bogus, but
             that is the user's responsibility.  */
          RETALLOC (bufp->buffer, INIT_BUF_SIZE, unsigned char);
        }
      else
        { /* Caller did not allocate a buffer.  Do it for them.  */
          bufp->buffer = TALLOC (INIT_BUF_SIZE, unsigned char);
        }
      if (!bufp->buffer) FREE_STACK_RETURN (REG_ESPACE);

      bufp->allocated = INIT_BUF_SIZE;
    }

  begalt = b = bufp->buffer;

  /* Loop through the uncompiled pattern until we're at the end.  */
  while (p != pend)
    {
      PATFETCH (c);

      switch (c)
        {
        case '^':
          {
            if (   /* If at start of pattern, it's an operator.  */
                   p == pattern + 1
                   /* If context independent, it's an operator.  */
                || syntax & RE_CONTEXT_INDEP_ANCHORS
                   /* Otherwise, depends on what's come before.  */
                || at_begline_loc_p (pattern, p, syntax))
              BUF_PUSH (begline);
            else
              goto normal_char;
          }
          break;


        case '$':
          {
            if (   /* If at end of pattern, it's an operator.  */
                   p == pend
                   /* If context independent, it's an operator.  */
                || syntax & RE_CONTEXT_INDEP_ANCHORS
                   /* Otherwise, depends on what's next.  */
                || at_endline_loc_p (p, pend, syntax))
               BUF_PUSH (endline);
             else
               goto normal_char;
           }
           break;


	case '+':
        case '?':
          if ((syntax & RE_BK_PLUS_QM)
              || (syntax & RE_LIMITED_OPS))
            goto normal_char;
        handle_plus:
        case '*':
          /* If there is no previous pattern... */
          if (!laststart)
            {
              if (syntax & RE_CONTEXT_INVALID_OPS)
                FREE_STACK_RETURN (REG_BADRPT);
              else if (!(syntax & RE_CONTEXT_INDEP_OPS))
                goto normal_char;
            }

          {
            /* Are we optimizing this jump?  */
            boolean keep_string_p = false;

            /* 1 means zero (many) matches is allowed.  */
            char zero_times_ok = 0, many_times_ok = 0;

            /* If there is a sequence of repetition chars, collapse it
               down to just one (the right one).  We can't combine
               interval operators with these because of, e.g., `a{2}*',
               which should only match an even number of `a's.  */

            for (;;)
              {
                zero_times_ok |= c != '+';
                many_times_ok |= c != '?';

                if (p == pend)
                  break;

                PATFETCH (c);
                if (c == '*'
                    || (!(syntax & RE_BK_PLUS_QM) && (c == '+' || c == '?')))
                  ;

                else if (syntax & RE_BK_PLUS_QM  &&  c == '\\')
                  {
                    if (p == pend) FREE_STACK_RETURN (REG_EESCAPE);

                    PATFETCH (c1);
                    if (!(c1 == '+' || c1 == '?'))
                      {
                        PATUNFETCH;
                        PATUNFETCH;
                        break;
                      }

                    c = c1;
                  }
                else
                  {
                    PATUNFETCH;
                    break;
                  }

                /* If we get here, we found another repeat character.  */
               }

            /* Star, etc. applied to an empty pattern is equivalent
               to an empty pattern.  */
            if (!laststart)
              break;

            /* Now we know whether or not zero matches is allowed
               and also whether or not two or more matches is allowed.  */
            if (many_times_ok)
              { /* More than one repetition is allowed, so put in at the
                   end a backward relative jump from `b' to before the next
                   jump we're going to put in below (which jumps from
                   laststart to after this jump).

                   But if we are at the `*' in the exact sequence `.*\n',
                   insert an unconditional jump backwards to the .,
                   instead of the beginning of the loop.  This way we only
                   push a failure point once, instead of every time
                   through the loop.  */
                assert (p - 1 > pattern);

                /* Allocate the space for the jump.  */
                GET_BUFFER_SPACE (3);

                /* We know we are not at the first character of the pattern,
                   because laststart was nonzero.  And we've already
                   incremented `p', by the way, to be the character after
                   the `*'.  Do we have to do something analogous here
                   for null bytes, because of RE_DOT_NOT_NULL?  */
                if (TRANSLATE (*(p - 2)) == TRANSLATE ('.')
		    && zero_times_ok
                    && p < pend && TRANSLATE (*p) == TRANSLATE ('\n')
                    && !(syntax & RE_DOT_NEWLINE))
                  { /* We have .*\n.  */
                    STORE_JUMP (jump, b, laststart);
                    keep_string_p = true;
                  }
                else
                  /* Anything else.  */
                  STORE_JUMP (maybe_pop_jump, b, laststart - 3);

                /* We've added more stuff to the buffer.  */
                b += 3;
              }

            /* On failure, jump from laststart to b + 3, which will be the
               end of the buffer after this jump is inserted.  */
            GET_BUFFER_SPACE (3);
            INSERT_JUMP (keep_string_p ? on_failure_keep_string_jump
                                       : on_failure_jump,
                         laststart, b + 3);
            pending_exact = 0;
            b += 3;

            if (!zero_times_ok)
              {
                /* At least one repetition is required, so insert a
                   `dummy_failure_jump' before the initial
                   `on_failure_jump' instruction of the loop. This
                   effects a skip over that instruction the first time
                   we hit that loop.  */
                GET_BUFFER_SPACE (3);
                INSERT_JUMP (dummy_failure_jump, laststart, laststart + 6);
                b += 3;
              }
            }
	  break;


	case '.':
          laststart = b;
          BUF_PUSH (anychar);
          break;


        case '[':
          {
            boolean had_char_class = false;

            if (p == pend) FREE_STACK_RETURN (REG_EBRACK);

            /* Ensure that we have enough space to push a charset: the
               opcode, the length count, and the bitset; 34 bytes in all.  */
	    GET_BUFFER_SPACE (34);

            laststart = b;

            /* We test `*p == '^' twice, instead of using an if
               statement, so we only need one BUF_PUSH.  */
            BUF_PUSH (*p == '^' ? charset_not : charset);
            if (*p == '^')
              p++;

            /* Remember the first position in the bracket expression.  */
            p1 = p;

            /* Push the number of bytes in the bitmap.  */
            BUF_PUSH ((1 << BYTEWIDTH) / BYTEWIDTH);

            /* Clear the whole map.  */
            bzero (b, (1 << BYTEWIDTH) / BYTEWIDTH);

            /* charset_not matches newline according to a syntax bit.  */
            if ((re_opcode_t) b[-2] == charset_not
                && (syntax & RE_HAT_LISTS_NOT_NEWLINE))
              SET_LIST_BIT ('\n');

            /* Read in characters and ranges, setting map bits.  */
            for (;;)
              {
                if (p == pend) FREE_STACK_RETURN (REG_EBRACK);

                PATFETCH (c);

                /* \ might escape characters inside [...] and [^...].  */
                if ((syntax & RE_BACKSLASH_ESCAPE_IN_LISTS) && c == '\\')
                  {
                    if (p == pend) FREE_STACK_RETURN (REG_EESCAPE);

                    PATFETCH (c1);
                    SET_LIST_BIT (c1);
                    continue;
                  }

                /* Could be the end of the bracket expression.  If it's
                   not (i.e., when the bracket expression is `[]' so
                   far), the ']' character bit gets set way below.  */
                if (c == ']' && p != p1 + 1)
                  break;

                /* Look ahead to see if it's a range when the last thing
                   was a character class.  */
                if (had_char_class && c == '-' && *p != ']')
                  FREE_STACK_RETURN (REG_ERANGE);

                /* Look ahead to see if it's a range when the last thing
                   was a character: if this is a hyphen not at the
                   beginning or the end of a list, then it's the range
                   operator.  */
                if (c == '-'
                    && !(p - 2 >= pattern && p[-2] == '[')
                    && !(p - 3 >= pattern && p[-3] == '[' && p[-2] == '^')
                    && *p != ']')
                  {
                    reg_errcode_t ret
                      = compile_range (&p, pend, translate, syntax, b);
                    if (ret != REG_NOERROR) FREE_STACK_RETURN (ret);
                  }

                else if (p[0] == '-' && p[1] != ']')
                  { /* This handles ranges made up of characters only.  */
                    reg_errcode_t ret;

		    /* Move past the `-'.  */
                    PATFETCH (c1);

                    ret = compile_range (&p, pend, translate, syntax, b);
                    if (ret != REG_NOERROR) FREE_STACK_RETURN (ret);
                  }

                /* See if we're at the beginning of a possible character
                   class.  */

                else if (syntax & RE_CHAR_CLASSES && c == '[' && *p == ':')
                  { /* Leave room for the null.  */
                    char str[CHAR_CLASS_MAX_LENGTH + 1];

                    PATFETCH (c);
                    c1 = 0;

                    /* If pattern is `[[:'.  */
                    if (p == pend) FREE_STACK_RETURN (REG_EBRACK);

                    for (;;)
                      {
                        PATFETCH (c);
                        if ((c == ':' && *p == ']') || p == pend
                            || c1 == CHAR_CLASS_MAX_LENGTH)
                          break;
                        str[c1++] = c;
                      }
                    str[c1] = '\0';

                    /* If isn't a word bracketed by `[:' and `:]':
                       undo the ending character, the letters, and leave
                       the leading `:' and `[' (but set bits for them).  */
                    if (c == ':' && *p == ']')
                      {
#if defined _LIBC || (defined HAVE_WCTYPE_H && defined HAVE_WCHAR_H)
                        boolean is_lower = STREQ (str, "lower");
                        boolean is_upper = STREQ (str, "upper");
			wctype_t wt;
                        int ch;

			wt = IS_CHAR_CLASS (str);
			if (wt == 0)
			  FREE_STACK_RETURN (REG_ECTYPE);

                        /* Throw away the ] at the end of the character
                           class.  */
                        PATFETCH (c);

                        if (p == pend) FREE_STACK_RETURN (REG_EBRACK);

                        for (ch = 0; ch < 1 << BYTEWIDTH; ++ch)
			  {
# ifdef _LIBC
			    if (__iswctype (__btowc (ch), wt))
			      SET_LIST_BIT (ch);
#else
			    if (iswctype (btowc (ch), wt))
			      SET_LIST_BIT (ch);
#endif

			    if (translate && (is_upper || is_lower)
				&& (ISUPPER (ch) || ISLOWER (ch)))
			      SET_LIST_BIT (ch);
			  }

                        had_char_class = true;
#else
                        int ch;
                        boolean is_alnum = STREQ (str, "alnum");
                        boolean is_alpha = STREQ (str, "alpha");
                        boolean is_blank = STREQ (str, "blank");
                        boolean is_cntrl = STREQ (str, "cntrl");
                        boolean is_digit = STREQ (str, "digit");
                        boolean is_graph = STREQ (str, "graph");
                        boolean is_lower = STREQ (str, "lower");
                        boolean is_print = STREQ (str, "print");
                        boolean is_punct = STREQ (str, "punct");
                        boolean is_space = STREQ (str, "space");
                        boolean is_upper = STREQ (str, "upper");
                        boolean is_xdigit = STREQ (str, "xdigit");

                        if (!IS_CHAR_CLASS (str))
			  FREE_STACK_RETURN (REG_ECTYPE);

                        /* Throw away the ] at the end of the character
                           class.  */
                        PATFETCH (c);

                        if (p == pend) FREE_STACK_RETURN (REG_EBRACK);

                        for (ch = 0; ch < 1 << BYTEWIDTH; ch++)
                          {
			    /* This was split into 3 if's to
			       avoid an arbitrary limit in some compiler.  */
                            if (   (is_alnum  && ISALNUM (ch))
                                || (is_alpha  && ISALPHA (ch))
                                || (is_blank  && ISBLANK (ch))
                                || (is_cntrl  && ISCNTRL (ch)))
			      SET_LIST_BIT (ch);
			    if (   (is_digit  && ISDIGIT (ch))
                                || (is_graph  && ISGRAPH (ch))
                                || (is_lower  && ISLOWER (ch))
                                || (is_print  && ISPRINT (ch)))
			      SET_LIST_BIT (ch);
			    if (   (is_punct  && ISPUNCT (ch))
                                || (is_space  && ISSPACE (ch))
                                || (is_upper  && ISUPPER (ch))
                                || (is_xdigit && ISXDIGIT (ch)))
			      SET_LIST_BIT (ch);
			    if (   translate && (is_upper || is_lower)
				&& (ISUPPER (ch) || ISLOWER (ch)))
			      SET_LIST_BIT (ch);
                          }
                        had_char_class = true;
#endif	/* libc || wctype.h */
                      }
                    else
                      {
                        c1++;
                        while (c1--)
                          PATUNFETCH;
                        SET_LIST_BIT ('[');
                        SET_LIST_BIT (':');
                        had_char_class = false;
                      }
                  }
                else
                  {
                    had_char_class = false;
                    SET_LIST_BIT (c);
                  }
              }

            /* Discard any (non)matching list bytes that are all 0 at the
               end of the map.  Decrease the map-length byte too.  */
            while ((int) b[-1] > 0 && b[b[-1] - 1] == 0)
              b[-1]--;
            b += b[-1];
          }
          break;


	case '(':
          if (syntax & RE_NO_BK_PARENS)
            goto handle_open;
          else
            goto normal_char;


        case ')':
          if (syntax & RE_NO_BK_PARENS)
            goto handle_close;
          else
            goto normal_char;


        case '\n':
          if (syntax & RE_NEWLINE_ALT)
            goto handle_alt;
          else
            goto normal_char;


	case '|':
          if (syntax & RE_NO_BK_VBAR)
            goto handle_alt;
          else
            goto normal_char;


        case '{':
           if (syntax & RE_INTERVALS && syntax & RE_NO_BK_BRACES)
             goto handle_interval;
           else
             goto normal_char;


        case '\\':
          if (p == pend) FREE_STACK_RETURN (REG_EESCAPE);

          /* Do not translate the character after the \, so that we can
             distinguish, e.g., \B from \b, even if we normally would
             translate, e.g., B to b.  */
          PATFETCH_RAW (c);

          switch (c)
            {
            case '(':
              if (syntax & RE_NO_BK_PARENS)
                goto normal_backslash;

            handle_open:
              bufp->re_nsub++;
              regnum++;

              if (COMPILE_STACK_FULL)
                {
                  RETALLOC (compile_stack.stack, compile_stack.size << 1,
                            compile_stack_elt_t);
                  if (compile_stack.stack == NULL) return REG_ESPACE;

                  compile_stack.size <<= 1;
                }

              /* These are the values to restore when we hit end of this
                 group.  They are all relative offsets, so that if the
                 whole pattern moves because of realloc, they will still
                 be valid.  */
              COMPILE_STACK_TOP.begalt_offset = begalt - bufp->buffer;
              COMPILE_STACK_TOP.fixup_alt_jump
                = fixup_alt_jump ? fixup_alt_jump - bufp->buffer + 1 : 0;
              COMPILE_STACK_TOP.laststart_offset = b - bufp->buffer;
              COMPILE_STACK_TOP.regnum = regnum;

              /* We will eventually replace the 0 with the number of
                 groups inner to this one.  But do not push a
                 start_memory for groups beyond the last one we can
                 represent in the compiled pattern.  */
              if (regnum <= MAX_REGNUM)
                {
                  COMPILE_STACK_TOP.inner_group_offset = b - bufp->buffer + 2;
                  BUF_PUSH_3 (start_memory, regnum, 0);
                }

              compile_stack.avail++;

              fixup_alt_jump = 0;
              laststart = 0;
              begalt = b;
	      /* If we've reached MAX_REGNUM groups, then this open
		 won't actually generate any code, so we'll have to
		 clear pending_exact explicitly.  */
	      pending_exact = 0;
              break;


            case ')':
              if (syntax & RE_NO_BK_PARENS) goto normal_backslash;

              if (COMPILE_STACK_EMPTY)
		{
		  if (syntax & RE_UNMATCHED_RIGHT_PAREN_ORD)
		    goto normal_backslash;
		  else
		    FREE_STACK_RETURN (REG_ERPAREN);
		}

            handle_close:
              if (fixup_alt_jump)
                { /* Push a dummy failure point at the end of the
                     alternative for a possible future
                     `pop_failure_jump' to pop.  See comments at
                     `push_dummy_failure' in `re_match_2'.  */
                  BUF_PUSH (push_dummy_failure);

                  /* We allocated space for this jump when we assigned
                     to `fixup_alt_jump', in the `handle_alt' case below.  */
                  STORE_JUMP (jump_past_alt, fixup_alt_jump, b - 1);
                }

              /* See similar code for backslashed left paren above.  */
              if (COMPILE_STACK_EMPTY)
		{
		  if (syntax & RE_UNMATCHED_RIGHT_PAREN_ORD)
		    goto normal_char;
		  else
		    FREE_STACK_RETURN (REG_ERPAREN);
		}

              /* Since we just checked for an empty stack above, this
                 ``can't happen''.  */
              assert (compile_stack.avail != 0);
              {
                /* We don't just want to restore into `regnum', because
                   later groups should continue to be numbered higher,
                   as in `(ab)c(de)' -- the second group is #2.  */
                regnum_t this_group_regnum;

                compile_stack.avail--;
                begalt = bufp->buffer + COMPILE_STACK_TOP.begalt_offset;
                fixup_alt_jump
                  = COMPILE_STACK_TOP.fixup_alt_jump
                    ? bufp->buffer + COMPILE_STACK_TOP.fixup_alt_jump - 1
                    : 0;
                laststart = bufp->buffer + COMPILE_STACK_TOP.laststart_offset;
                this_group_regnum = COMPILE_STACK_TOP.regnum;
		/* If we've reached MAX_REGNUM groups, then this open
		   won't actually generate any code, so we'll have to
		   clear pending_exact explicitly.  */
		pending_exact = 0;

                /* We're at the end of the group, so now we know how many
                   groups were inside this one.  */
                if (this_group_regnum <= MAX_REGNUM)
                  {
                    unsigned char *inner_group_loc
                      = bufp->buffer + COMPILE_STACK_TOP.inner_group_offset;

                    *inner_group_loc = regnum - this_group_regnum;
                    BUF_PUSH_3 (stop_memory, this_group_regnum,
                                regnum - this_group_regnum);
                  }
              }
              break;


            case '|':					/* `\|'.  */
              if (syntax & RE_LIMITED_OPS || syntax & RE_NO_BK_VBAR)
                goto normal_backslash;
            handle_alt:
              if (syntax & RE_LIMITED_OPS)
                goto normal_char;

              /* Insert before the previous alternative a jump which
                 jumps to this alternative if the former fails.  */
              GET_BUFFER_SPACE (3);
              INSERT_JUMP (on_failure_jump, begalt, b + 6);
              pending_exact = 0;
              b += 3;

              /* The alternative before this one has a jump after it
                 which gets executed if it gets matched.  Adjust that
                 jump so it will jump to this alternative's analogous
                 jump (put in below, which in turn will jump to the next
                 (if any) alternative's such jump, etc.).  The last such
                 jump jumps to the correct final destination.  A picture:
                          _____ _____
                          |   | |   |
                          |   v |   v
                         a | b   | c

                 If we are at `b', then fixup_alt_jump right now points to a
                 three-byte space after `a'.  We'll put in the jump, set
                 fixup_alt_jump to right after `b', and leave behind three
                 bytes which we'll fill in when we get to after `c'.  */

              if (fixup_alt_jump)
                STORE_JUMP (jump_past_alt, fixup_alt_jump, b);

              /* Mark and leave space for a jump after this alternative,
                 to be filled in later either by next alternative or
                 when know we're at the end of a series of alternatives.  */
              fixup_alt_jump = b;
              GET_BUFFER_SPACE (3);
              b += 3;

              laststart = 0;
              begalt = b;
              break;


            case '{':
              /* If \{ is a literal.  */
              if (!(syntax & RE_INTERVALS)
                     /* If we're at `\{' and it's not the open-interval
                        operator.  */
                  || ((syntax & RE_INTERVALS) && (syntax & RE_NO_BK_BRACES))
                  || (p - 2 == pattern  &&  p == pend))
                goto normal_backslash;

            handle_interval:
              {
                /* If got here, then the syntax allows intervals.  */

                /* At least (most) this many matches must be made.  */
                int lower_bound = -1, upper_bound = -1;

                beg_interval = p - 1;

                if (p == pend)
                  {
                    if (syntax & RE_NO_BK_BRACES)
                      goto unfetch_interval;
                    else
                      FREE_STACK_RETURN (REG_EBRACE);
                  }

                GET_UNSIGNED_NUMBER (lower_bound);

                if (c == ',')
                  {
                    GET_UNSIGNED_NUMBER (upper_bound);
                    if (upper_bound < 0) upper_bound = RE_DUP_MAX;
                  }
                else
                  /* Interval such as `{1}' => match exactly once. */
                  upper_bound = lower_bound;

                if (lower_bound < 0 || upper_bound > RE_DUP_MAX
                    || lower_bound > upper_bound)
                  {
                    if (syntax & RE_NO_BK_BRACES)
                      goto unfetch_interval;
                    else
                      FREE_STACK_RETURN (REG_BADBR);
                  }

                if (!(syntax & RE_NO_BK_BRACES))
                  {
                    if (c != '\\') FREE_STACK_RETURN (REG_EBRACE);

                    PATFETCH (c);
                  }

                if (c != '}')
                  {
                    if (syntax & RE_NO_BK_BRACES)
                      goto unfetch_interval;
                    else
                      FREE_STACK_RETURN (REG_BADBR);
                  }

                /* We just parsed a valid interval.  */

                /* If it's invalid to have no preceding re.  */
                if (!laststart)
                  {
                    if (syntax & RE_CONTEXT_INVALID_OPS)
                      FREE_STACK_RETURN (REG_BADRPT);
                    else if (syntax & RE_CONTEXT_INDEP_OPS)
                      laststart = b;
                    else
                      goto unfetch_interval;
                  }

                /* If the upper bound is zero, don't want to succeed at
                   all; jump from `laststart' to `b + 3', which will be
                   the end of the buffer after we insert the jump.  */
                 if (upper_bound == 0)
                   {
                     GET_BUFFER_SPACE (3);
                     INSERT_JUMP (jump, laststart, b + 3);
                     b += 3;
                   }

                 /* Otherwise, we have a nontrivial interval.  When
                    we're all done, the pattern will look like:
                      set_number_at <jump count> <upper bound>
                      set_number_at <succeed_n count> <lower bound>
                      succeed_n <after jump addr> <succeed_n count>
                      <body of loop>
                      jump_n <succeed_n addr> <jump count>
                    (The upper bound and `jump_n' are omitted if
                    `upper_bound' is 1, though.)  */
                 else
                   { /* If the upper bound is > 1, we need to insert
                        more at the end of the loop.  */
                     unsigned nbytes = 10 + (upper_bound > 1) * 10;

                     GET_BUFFER_SPACE (nbytes);

                     /* Initialize lower bound of the `succeed_n', even
                        though it will be set during matching by its
                        attendant `set_number_at' (inserted next),
                        because `re_compile_fastmap' needs to know.
                        Jump to the `jump_n' we might insert below.  */
                     INSERT_JUMP2 (succeed_n, laststart,
                                   b + 5 + (upper_bound > 1) * 5,
                                   lower_bound);
                     b += 5;

                     /* Code to initialize the lower bound.  Insert
                        before the `succeed_n'.  The `5' is the last two
                        bytes of this `set_number_at', plus 3 bytes of
                        the following `succeed_n'.  */
                     insert_op2 (set_number_at, laststart, 5, lower_bound, b);
                     b += 5;

                     if (upper_bound > 1)
                       { /* More than one repetition is allowed, so
                            append a backward jump to the `succeed_n'
                            that starts this interval.

                            When we've reached this during matching,
                            we'll have matched the interval once, so
                            jump back only `upper_bound - 1' times.  */
                         STORE_JUMP2 (jump_n, b, laststart + 5,
                                      upper_bound - 1);
                         b += 5;

                         /* The location we want to set is the second
                            parameter of the `jump_n'; that is `b-2' as
                            an absolute address.  `laststart' will be
                            the `set_number_at' we're about to insert;
                            `laststart+3' the number to set, the source
                            for the relative address.  But we are
                            inserting into the middle of the pattern --
                            so everything is getting moved up by 5.
                            Conclusion: (b - 2) - (laststart + 3) + 5,
                            i.e., b - laststart.

                            We insert this at the beginning of the loop
                            so that if we fail during matching, we'll
                            reinitialize the bounds.  */
                         insert_op2 (set_number_at, laststart, b - laststart,
                                     upper_bound - 1, b);
                         b += 5;
                       }
                   }
                pending_exact = 0;
                beg_interval = NULL;
              }
              break;

            unfetch_interval:
              /* If an invalid interval, match the characters as literals.  */
               assert (beg_interval);
               p = beg_interval;
               beg_interval = NULL;

               /* normal_char and normal_backslash need `c'.  */
               PATFETCH (c);

               if (!(syntax & RE_NO_BK_BRACES))
                 {
                   if (p > pattern  &&  p[-1] == '\\')
                     goto normal_backslash;
                 }
               goto normal_char;

#ifdef emacs
            /* There is no way to specify the before_dot and after_dot
               operators.  rms says this is ok.  --karl  */
            case '=':
              BUF_PUSH (at_dot);
              break;

            case 's':
              laststart = b;
              PATFETCH (c);
              BUF_PUSH_2 (syntaxspec, syntax_spec_code[c]);
              break;

            case 'S':
              laststart = b;
              PATFETCH (c);
              BUF_PUSH_2 (notsyntaxspec, syntax_spec_code[c]);
              break;
#endif /* emacs */


            case 'w':
	      if (syntax & RE_NO_GNU_OPS)
		goto normal_char;
              laststart = b;
              BUF_PUSH (wordchar);
              break;


            case 'W':
	      if (syntax & RE_NO_GNU_OPS)
		goto normal_char;
              laststart = b;
              BUF_PUSH (notwordchar);
              break;


            case '<':
	      if (syntax & RE_NO_GNU_OPS)
		goto normal_char;
              BUF_PUSH (wordbeg);
              break;

            case '>':
	      if (syntax & RE_NO_GNU_OPS)
		goto normal_char;
              BUF_PUSH (wordend);
              break;

            case 'b':
	      if (syntax & RE_NO_GNU_OPS)
		goto normal_char;
              BUF_PUSH (wordbound);
              break;

            case 'B':
	      if (syntax & RE_NO_GNU_OPS)
		goto normal_char;
              BUF_PUSH (notwordbound);
              break;

            case '`':
	      if (syntax & RE_NO_GNU_OPS)
		goto normal_char;
              BUF_PUSH (begbuf);
              break;

            case '\'':
	      if (syntax & RE_NO_GNU_OPS)
		goto normal_char;
              BUF_PUSH (endbuf);
              break;

            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9':
              if (syntax & RE_NO_BK_REFS)
                goto normal_char;

              c1 = c - '0';

              if (c1 > regnum)
                FREE_STACK_RETURN (REG_ESUBREG);

              /* Can't back reference to a subexpression if inside of it.  */
              if (group_in_compile_stack (compile_stack, (regnum_t) c1))
                goto normal_char;

              laststart = b;
              BUF_PUSH_2 (duplicate, c1);
              break;


            case '+':
            case '?':
              if (syntax & RE_BK_PLUS_QM)
                goto handle_plus;
              else
                goto normal_backslash;

            default:
            normal_backslash:
              /* You might think it would be useful for \ to mean
                 not to translate; but if we don't translate it
                 it will never match anything.  */
              c = TRANSLATE (c);
              goto normal_char;
            }
          break;


	default:
        /* Expects the character in `c'.  */
	normal_char:
	      /* If no exactn currently being built.  */
          if (!pending_exact

              /* If last exactn not at current position.  */
              || pending_exact + *pending_exact + 1 != b

              /* We have only one byte following the exactn for the count.  */
	      || *pending_exact == (1 << BYTEWIDTH) - 1

              /* If followed by a repetition operator.  */
              || *p == '*' || *p == '^'
	      || ((syntax & RE_BK_PLUS_QM)
		  ? *p == '\\' && (p[1] == '+' || p[1] == '?')
		  : (*p == '+' || *p == '?'))
	      || ((syntax & RE_INTERVALS)
                  && ((syntax & RE_NO_BK_BRACES)
		      ? *p == '{'
                      : (p[0] == '\\' && p[1] == '{'))))
	    {
	      /* Start building a new exactn.  */

              laststart = b;

	      BUF_PUSH_2 (exactn, 0);
	      pending_exact = b - 1;
            }

	  BUF_PUSH (c);
          (*pending_exact)++;
	  break;
        } /* switch (c) */
    } /* while p != pend */


  /* Through the pattern now.  */

  if (fixup_alt_jump)
    STORE_JUMP (jump_past_alt, fixup_alt_jump, b);

  if (!COMPILE_STACK_EMPTY)
    FREE_STACK_RETURN (REG_EPAREN);

  /* If we don't want backtracking, force success
     the first time we reach the end of the compiled pattern.  */
  if (syntax & RE_NO_POSIX_BACKTRACKING)
    BUF_PUSH (succeed);

  free (compile_stack.stack);

  /* We have succeeded; set the length of the buffer.  */
  bufp->used = b - bufp->buffer;

#ifdef DEBUG
  if (debug)
    {
      DEBUG_PRINT1 ("\nCompiled pattern: \n");
      print_compiled_pattern (bufp);
    }
#endif /* DEBUG */

#ifndef MATCH_MAY_ALLOCATE
  /* Initialize the failure stack to the largest possible stack.  This
     isn't necessary unless we're trying to avoid calling alloca in
     the search and match routines.  */
  {
    int num_regs = bufp->re_nsub + 1;

    /* Since DOUBLE_FAIL_STACK refuses to double only if the current size
       is strictly greater than re_max_failures, the largest possible stack
       is 2 * re_max_failures failure points.  */
    if (fail_stack.size < (2 * re_max_failures * MAX_FAILURE_ITEMS))
      {
	fail_stack.size = (2 * re_max_failures * MAX_FAILURE_ITEMS);

# ifdef emacs
	if (! fail_stack.stack)
	  fail_stack.stack
	    = (fail_stack_elt_t *) xmalloc (fail_stack.size
					    * sizeof (fail_stack_elt_t));
	else
	  fail_stack.stack
	    = (fail_stack_elt_t *) xrealloc (fail_stack.stack,
					     (fail_stack.size
					      * sizeof (fail_stack_elt_t)));
# else /* not emacs */
	if (! fail_stack.stack)
	  fail_stack.stack
	    = (fail_stack_elt_t *) malloc (fail_stack.size
					   * sizeof (fail_stack_elt_t));
	else
	  fail_stack.stack
	    = (fail_stack_elt_t *) realloc (fail_stack.stack,
					    (fail_stack.size
					     * sizeof (fail_stack_elt_t)));
# endif /* not emacs */
      }

    regex_grow_registers (num_regs);
  }
#endif /* not MATCH_MAY_ALLOCATE */

  return REG_NOERROR;
} /* regex_compile */

/* Subroutines for `regex_compile'.  */

/* Store OP at LOC followed by two-byte integer parameter ARG.  */

static void
store_op1 (op, loc, arg)
    re_opcode_t op;
    unsigned char *loc;
    int arg;
{
  *loc = (unsigned char) op;
  STORE_NUMBER (loc + 1, arg);
}


/* Like `store_op1', but for two two-byte parameters ARG1 and ARG2.  */

static void
store_op2 (op, loc, arg1, arg2)
    re_opcode_t op;
    unsigned char *loc;
    int arg1, arg2;
{
  *loc = (unsigned char) op;
  STORE_NUMBER (loc + 1, arg1);
  STORE_NUMBER (loc + 3, arg2);
}


/* Copy the bytes from LOC to END to open up three bytes of space at LOC
   for OP followed by two-byte integer parameter ARG.  */

static void
insert_op1 (op, loc, arg, end)
    re_opcode_t op;
    unsigned char *loc;
    int arg;
    unsigned char *end;
{
  register unsigned char *pfrom = end;
  register unsigned char *pto = end + 3;

  while (pfrom != loc)
    *--pto = *--pfrom;

  store_op1 (op, loc, arg);
}


/* Like `insert_op1', but for two two-byte parameters ARG1 and ARG2.  */

static void
insert_op2 (op, loc, arg1, arg2, end)
    re_opcode_t op;
    unsigned char *loc;
    int arg1, arg2;
    unsigned char *end;
{
  register unsigned char *pfrom = end;
  register unsigned char *pto = end + 5;

  while (pfrom != loc)
    *--pto = *--pfrom;

  store_op2 (op, loc, arg1, arg2);
}


/* P points to just after a ^ in PATTERN.  Return true if that ^ comes
   after an alternative or a begin-subexpression.  We assume there is at
   least one character before the ^.  */

static boolean
at_begline_loc_p (pattern, p, syntax)
    const char *pattern, *p;
    reg_syntax_t syntax;
{
  const char *prev = p - 2;
  boolean prev_prev_backslash = prev > pattern && prev[-1] == '\\';

  return
       /* After a subexpression?  */
       (*prev == '(' && (syntax & RE_NO_BK_PARENS || prev_prev_backslash))
       /* After an alternative?  */
    || (*prev == '|' && (syntax & RE_NO_BK_VBAR || prev_prev_backslash));
}


/* The dual of at_begline_loc_p.  This one is for $.  We assume there is
   at least one character after the $, i.e., `P < PEND'.  */

static boolean
at_endline_loc_p (p, pend, syntax)
    const char *p, *pend;
    reg_syntax_t syntax;
{
  const char *next = p;
  boolean next_backslash = *next == '\\';
  const char *next_next = p + 1 < pend ? p + 1 : 0;

  return
       /* Before a subexpression?  */
       (syntax & RE_NO_BK_PARENS ? *next == ')'
        : next_backslash && next_next && *next_next == ')')
       /* Before an alternative?  */
    || (syntax & RE_NO_BK_VBAR ? *next == '|'
        : next_backslash && next_next && *next_next == '|');
}


/* Returns true if REGNUM is in one of COMPILE_STACK's elements and
   false if it's not.  */

static boolean
group_in_compile_stack (compile_stack, regnum)
    compile_stack_type compile_stack;
    regnum_t regnum;
{
  int this_element;

  for (this_element = compile_stack.avail - 1;
       this_element >= 0;
       this_element--)
    if (compile_stack.stack[this_element].regnum == regnum)
      return true;

  return false;
}


/* Read the ending character of a range (in a bracket expression) from the
   uncompiled pattern *P_PTR (which ends at PEND).  We assume the
   starting character is in `P[-2]'.  (`P[-1]' is the character `-'.)
   Then we set the translation of all bits between the starting and
   ending characters (inclusive) in the compiled pattern B.

   Return an error code.

   We use these short variable names so we can use the same macros as
   `regex_compile' itself.  */

static reg_errcode_t
compile_range (p_ptr, pend, translate, syntax, b)
    const char **p_ptr, *pend;
    RE_TRANSLATE_TYPE translate;
    reg_syntax_t syntax;
    unsigned char *b;
{
  unsigned this_char;

  const char *p = *p_ptr;
  unsigned int range_start, range_end;

  if (p == pend)
    return REG_ERANGE;

  /* Even though the pattern is a signed `char *', we need to fetch
     with unsigned char *'s; if the high bit of the pattern character
     is set, the range endpoints will be negative if we fetch using a
     signed char *.

     We also want to fetch the endpoints without translating them; the
     appropriate translation is done in the bit-setting loop below.  */
  /* The SVR4 compiler on the 3B2 had trouble with unsigned const char *.  */
  range_start = ((const unsigned char *) p)[-2];
  range_end   = ((const unsigned char *) p)[0];

  /* Have to increment the pointer into the pattern string, so the
     caller isn't still at the ending character.  */
  (*p_ptr)++;

  /* If the start is after the end, the range is empty.  */
  if (range_start > range_end)
    return syntax & RE_NO_EMPTY_RANGES ? REG_ERANGE : REG_NOERROR;

  /* Here we see why `this_char' has to be larger than an `unsigned
     char' -- the range is inclusive, so if `range_end' == 0xff
     (assuming 8-bit characters), we would otherwise go into an infinite
     loop, since all characters <= 0xff.  */
  for (this_char = range_start; this_char <= range_end; this_char++)
    {
      SET_LIST_BIT (TRANSLATE (this_char));
    }

  return REG_NOERROR;
}

/* re_compile_fastmap computes a ``fastmap'' for the compiled pattern in
   BUFP.  A fastmap records which of the (1 << BYTEWIDTH) possible
   characters can start a string that matches the pattern.  This fastmap
   is used by re_search to skip quickly over impossible starting points.

   The caller must supply the address of a (1 << BYTEWIDTH)-byte data
   area as BUFP->fastmap.

   We set the `fastmap', `fastmap_accurate', and `can_be_null' fields in
   the pattern buffer.

   Returns 0 if we succeed, -2 if an internal error.   */

int
re_compile_fastmap (bufp)
     struct re_pattern_buffer *bufp;
{
  int j, k;
#ifdef MATCH_MAY_ALLOCATE
  fail_stack_type fail_stack;
#endif
#ifndef REGEX_MALLOC
  char *destination;
#endif

  register char *fastmap = bufp->fastmap;
  unsigned char *pattern = bufp->buffer;
  unsigned char *p = pattern;
  register unsigned char *pend = pattern + bufp->used;

#ifdef REL_ALLOC
  /* This holds the pointer to the failure stack, when
     it is allocated relocatably.  */
  fail_stack_elt_t *failure_stack_ptr;
#endif

  /* Assume that each path through the pattern can be null until
     proven otherwise.  We set this false at the bottom of switch
     statement, to which we get only if a particular path doesn't
     match the empty string.  */
  boolean path_can_be_null = true;

  /* We aren't doing a `succeed_n' to begin with.  */
  boolean succeed_n_p = false;

  assert (fastmap != NULL && p != NULL);

  INIT_FAIL_STACK ();
  bzero (fastmap, 1 << BYTEWIDTH);  /* Assume nothing's valid.  */
  bufp->fastmap_accurate = 1;	    /* It will be when we're done.  */
  bufp->can_be_null = 0;

  while (1)
    {
      if (p == pend || *p == succeed)
	{
	  /* We have reached the (effective) end of pattern.  */
	  if (!FAIL_STACK_EMPTY ())
	    {
	      bufp->can_be_null |= path_can_be_null;

	      /* Reset for next path.  */
	      path_can_be_null = true;

	      p = fail_stack.stack[--fail_stack.avail].pointer;

	      continue;
	    }
	  else
	    break;
	}

      /* We should never be about to go beyond the end of the pattern.  */
      assert (p < pend);

      switch (SWITCH_ENUM_CAST ((re_opcode_t) *p++))
	{

        /* I guess the idea here is to simply not bother with a fastmap
           if a backreference is used, since it's too hard to figure out
           the fastmap for the corresponding group.  Setting
           `can_be_null' stops `re_search_2' from using the fastmap, so
           that is all we do.  */
	case duplicate:
	  bufp->can_be_null = 1;
          goto done;


      /* Following are the cases which match a character.  These end
         with `break'.  */

	case exactn:
          fastmap[p[1]] = 1;
	  break;


        case charset:
          for (j = *p++ * BYTEWIDTH - 1; j >= 0; j--)
	    if (p[j / BYTEWIDTH] & (1 << (j % BYTEWIDTH)))
              fastmap[j] = 1;
	  break;


	case charset_not:
	  /* Chars beyond end of map must be allowed.  */
	  for (j = *p * BYTEWIDTH; j < (1 << BYTEWIDTH); j++)
            fastmap[j] = 1;

	  for (j = *p++ * BYTEWIDTH - 1; j >= 0; j--)
	    if (!(p[j / BYTEWIDTH] & (1 << (j % BYTEWIDTH))))
              fastmap[j] = 1;
          break;


	case wordchar:
	  for (j = 0; j < (1 << BYTEWIDTH); j++)
	    if (SYNTAX (j) == Sword)
	      fastmap[j] = 1;
	  break;


	case notwordchar:
	  for (j = 0; j < (1 << BYTEWIDTH); j++)
	    if (SYNTAX (j) != Sword)
	      fastmap[j] = 1;
	  break;


        case anychar:
	  {
	    int fastmap_newline = fastmap['\n'];

	    /* `.' matches anything ...  */
	    for (j = 0; j < (1 << BYTEWIDTH); j++)
	      fastmap[j] = 1;

	    /* ... except perhaps newline.  */
	    if (!(bufp->syntax & RE_DOT_NEWLINE))
	      fastmap['\n'] = fastmap_newline;

	    /* Return if we have already set `can_be_null'; if we have,
	       then the fastmap is irrelevant.  Something's wrong here.  */
	    else if (bufp->can_be_null)
	      goto done;

	    /* Otherwise, have to check alternative paths.  */
	    break;
	  }

#ifdef emacs
        case syntaxspec:
	  k = *p++;
	  for (j = 0; j < (1 << BYTEWIDTH); j++)
	    if (SYNTAX (j) == (enum syntaxcode) k)
	      fastmap[j] = 1;
	  break;


	case notsyntaxspec:
	  k = *p++;
	  for (j = 0; j < (1 << BYTEWIDTH); j++)
	    if (SYNTAX (j) != (enum syntaxcode) k)
	      fastmap[j] = 1;
	  break;


      /* All cases after this match the empty string.  These end with
         `continue'.  */


	case before_dot:
	case at_dot:
	case after_dot:
          continue;
#endif /* emacs */


        case no_op:
        case begline:
        case endline:
	case begbuf:
	case endbuf:
	case wordbound:
	case notwordbound:
	case wordbeg:
	case wordend:
        case push_dummy_failure:
          continue;


	case jump_n:
        case pop_failure_jump:
	case maybe_pop_jump:
	case jump:
        case jump_past_alt:
	case dummy_failure_jump:
          EXTRACT_NUMBER_AND_INCR (j, p);
	  p += j;
	  if (j > 0)
	    continue;

          /* Jump backward implies we just went through the body of a
             loop and matched nothing.  Opcode jumped to should be
             `on_failure_jump' or `succeed_n'.  Just treat it like an
             ordinary jump.  For a * loop, it has pushed its failure
             point already; if so, discard that as redundant.  */
          if ((re_opcode_t) *p != on_failure_jump
	      && (re_opcode_t) *p != succeed_n)
	    continue;

          p++;
          EXTRACT_NUMBER_AND_INCR (j, p);
          p += j;

          /* If what's on the stack is where we are now, pop it.  */
          if (!FAIL_STACK_EMPTY ()
	      && fail_stack.stack[fail_stack.avail - 1].pointer == p)
            fail_stack.avail--;

          continue;


        case on_failure_jump:
        case on_failure_keep_string_jump:
	handle_on_failure_jump:
          EXTRACT_NUMBER_AND_INCR (j, p);

          /* For some patterns, e.g., `(a?)?', `p+j' here points to the
             end of the pattern.  We don't want to push such a point,
             since when we restore it above, entering the switch will
             increment `p' past the end of the pattern.  We don't need
             to push such a point since we obviously won't find any more
             fastmap entries beyond `pend'.  Such a pattern can match
             the null string, though.  */
          if (p + j < pend)
            {
              if (!PUSH_PATTERN_OP (p + j, fail_stack))
		{
		  RESET_FAIL_STACK ();
		  return -2;
		}
            }
          else
            bufp->can_be_null = 1;

          if (succeed_n_p)
            {
              EXTRACT_NUMBER_AND_INCR (k, p);	/* Skip the n.  */
              succeed_n_p = false;
	    }

          continue;


	case succeed_n:
          /* Get to the number of times to succeed.  */
          p += 2;

          /* Increment p past the n for when k != 0.  */
          EXTRACT_NUMBER_AND_INCR (k, p);
          if (k == 0)
	    {
              p -= 4;
  	      succeed_n_p = true;  /* Spaghetti code alert.  */
              goto handle_on_failure_jump;
            }
          continue;


	case set_number_at:
          p += 4;
          continue;


	case start_memory:
        case stop_memory:
	  p += 2;
	  continue;


	default:
          abort (); /* We have listed all the cases.  */
        } /* switch *p++ */

      /* Getting here means we have found the possible starting
         characters for one path of the pattern -- and that the empty
         string does not match.  We need not follow this path further.
         Instead, look at the next alternative (remembered on the
         stack), or quit if no more.  The test at the top of the loop
         does these things.  */
      path_can_be_null = false;
      p = pend;
    } /* while p */

  /* Set `can_be_null' for the last path (also the first path, if the
     pattern is empty).  */
  bufp->can_be_null |= path_can_be_null;

 done:
  RESET_FAIL_STACK ();
  return 0;
} /* re_compile_fastmap */
#ifdef _LIBC
weak_alias (__re_compile_fastmap, re_compile_fastmap)
#endif

/* Set REGS to hold NUM_REGS registers, storing them in STARTS and
   ENDS.  Subsequent matches using PATTERN_BUFFER and REGS will use
   this memory for recording register information.  STARTS and ENDS
   must be allocated using the malloc library routine, and must each
   be at least NUM_REGS * sizeof (regoff_t) bytes long.

   If NUM_REGS == 0, then subsequent matches should allocate their own
   register data.

   Unless this function is called, the first search or match using
   PATTERN_BUFFER will allocate its own register data, without
   freeing the old data.  */

void
re_set_registers (bufp, regs, num_regs, starts, ends)
    struct re_pattern_buffer *bufp;
    struct re_registers *regs;
    unsigned num_regs;
    regoff_t *starts, *ends;
{
  if (num_regs)
    {
      bufp->regs_allocated = REGS_REALLOCATE;
      regs->num_regs = num_regs;
      regs->start = starts;
      regs->end = ends;
    }
  else
    {
      bufp->regs_allocated = REGS_UNALLOCATED;
      regs->num_regs = 0;
      regs->start = regs->end = (regoff_t *) 0;
    }
}
#ifdef _LIBC
weak_alias (__re_set_registers, re_set_registers)
#endif

/* Searching routines.  */

/* Like re_search_2, below, but only one string is specified, and
   doesn't let you say where to stop matching. */

int
re_search (bufp, string, size, startpos, range, regs)
     struct re_pattern_buffer *bufp;
     const char *string;
     int size, startpos, range;
     struct re_registers *regs;
{
  return re_search_2 (bufp, NULL, 0, string, size, startpos, range,
		      regs, size);
}
#ifdef _LIBC
weak_alias (__re_search, re_search)
#endif


/* Using the compiled pattern in BUFP->buffer, first tries to match the
   virtual concatenation of STRING1 and STRING2, starting first at index
   STARTPOS, then at STARTPOS + 1, and so on.

   STRING1 and STRING2 have length SIZE1 and SIZE2, respectively.

   RANGE is how far to scan while trying to match.  RANGE = 0 means try
   only at STARTPOS; in general, the last start tried is STARTPOS +
   RANGE.

   In REGS, return the indices of the virtual concatenation of STRING1
   and STRING2 that matched the entire BUFP->buffer and its contained
   subexpressions.

   Do not consider matching one past the index STOP in the virtual
   concatenation of STRING1 and STRING2.

   We return either the position in the strings at which the match was
   found, -1 if no match, or -2 if error (such as failure
   stack overflow).  */

int
re_search_2 (bufp, string1, size1, string2, size2, startpos, range, regs, stop)
     struct re_pattern_buffer *bufp;
     const char *string1, *string2;
     int size1, size2;
     int startpos;
     int range;
     struct re_registers *regs;
     int stop;
{
  int val;
  register char *fastmap = bufp->fastmap;
  register RE_TRANSLATE_TYPE translate = bufp->translate;
  int total_size = size1 + size2;
  int endpos = startpos + range;

  /* Check for out-of-range STARTPOS.  */
  if (startpos < 0 || startpos > total_size)
    return -1;

  /* Fix up RANGE if it might eventually take us outside
     the virtual concatenation of STRING1 and STRING2.
     Make sure we won't move STARTPOS below 0 or above TOTAL_SIZE.  */
  if (endpos < 0)
    range = 0 - startpos;
  else if (endpos > total_size)
    range = total_size - startpos;

  /* If the search isn't to be a backwards one, don't waste time in a
     search for a pattern that must be anchored.  */
  if (bufp->used > 0 && (re_opcode_t) bufp->buffer[0] == begbuf && range > 0)
    {
      if (startpos > 0)
	return -1;
      else
	range = 1;
    }

#ifdef emacs
  /* In a forward search for something that starts with \=.
     don't keep searching past point.  */
  if (bufp->used > 0 && (re_opcode_t) bufp->buffer[0] == at_dot && range > 0)
    {
      range = PT - startpos;
      if (range <= 0)
	return -1;
    }
#endif /* emacs */

  /* Update the fastmap now if not correct already.  */
  if (fastmap && !bufp->fastmap_accurate)
    if (re_compile_fastmap (bufp) == -2)
      return -2;

  /* Loop through the string, looking for a place to start matching.  */
  for (;;)
    {
      /* If a fastmap is supplied, skip quickly over characters that
         cannot be the start of a match.  If the pattern can match the
         null string, however, we don't need to skip characters; we want
         the first null string.  */
      if (fastmap && startpos < total_size && !bufp->can_be_null)
	{
	  if (range > 0)	/* Searching forwards.  */
	    {
	      register const char *d;
	      register int lim = 0;
	      int irange = range;

              if (startpos < size1 && startpos + range >= size1)
                lim = range - (size1 - startpos);

	      d = (startpos >= size1 ? string2 - size1 : string1) + startpos;

              /* Written out as an if-else to avoid testing `translate'
                 inside the loop.  */
	      if (translate)
                while (range > lim
                       && !fastmap[(unsigned char)
				   translate[(unsigned char) *d++]])
                  range--;
	      else
                while (range > lim && !fastmap[(unsigned char) *d++])
                  range--;

	      startpos += irange - range;
	    }
	  else				/* Searching backwards.  */
	    {
	      register char c = (size1 == 0 || startpos >= size1
                                 ? string2[startpos - size1]
                                 : string1[startpos]);

	      if (!fastmap[(unsigned char) TRANSLATE (c)])
		goto advance;
	    }
	}

      /* If can't match the null string, and that's all we have left, fail.  */
      if (range >= 0 && startpos == total_size && fastmap
          && !bufp->can_be_null)
	return -1;

      val = re_match_2_internal (bufp, string1, size1, string2, size2,
				 startpos, regs, stop);
#ifndef REGEX_MALLOC
# ifdef C_ALLOCA
      alloca (0);
# endif
#endif

      if (val >= 0)
	return startpos;

      if (val == -2)
	return -2;

    advance:
      if (!range)
        break;
      else if (range > 0)
        {
          range--;
          startpos++;
        }
      else
        {
          range++;
          startpos--;
        }
    }
  return -1;
} /* re_search_2 */
#ifdef _LIBC
weak_alias (__re_search_2, re_search_2)
#endif

/* This converts PTR, a pointer into one of the search strings `string1'
   and `string2' into an offset from the beginning of that string.  */
#define POINTER_TO_OFFSET(ptr)			\
  (FIRST_STRING_P (ptr)				\
   ? ((regoff_t) ((ptr) - string1))		\
   : ((regoff_t) ((ptr) - string2 + size1)))

/* Macros for dealing with the split strings in re_match_2.  */

#define MATCHING_IN_FIRST_STRING  (dend == end_match_1)

/* Call before fetching a character with *d.  This switches over to
   string2 if necessary.  */
#define PREFETCH()							\
  while (d == dend)						    	\
    {									\
      /* End of string2 => fail.  */					\
      if (dend == end_match_2) 						\
        goto fail;							\
      /* End of string1 => advance to string2.  */ 			\
      d = string2;						        \
      dend = end_match_2;						\
    }


/* Test if at very beginning or at very end of the virtual concatenation
   of `string1' and `string2'.  If only one string, it's `string2'.  */
#define AT_STRINGS_BEG(d) ((d) == (size1 ? string1 : string2) || !size2)
#define AT_STRINGS_END(d) ((d) == end2)


/* Test if D points to a character which is word-constituent.  We have
   two special cases to check for: if past the end of string1, look at
   the first character in string2; and if before the beginning of
   string2, look at the last character in string1.  */
#define WORDCHAR_P(d)							\
  (SYNTAX ((d) == end1 ? *string2					\
           : (d) == string2 - 1 ? *(end1 - 1) : *(d))			\
   == Sword)

/* Disabled due to a compiler bug -- see comment at case wordbound */
#if 0
/* Test if the character before D and the one at D differ with respect
   to being word-constituent.  */
#define AT_WORD_BOUNDARY(d)						\
  (AT_STRINGS_BEG (d) || AT_STRINGS_END (d)				\
   || WORDCHAR_P (d - 1) != WORDCHAR_P (d))
#endif

/* Free everything we malloc.  */
#ifdef MATCH_MAY_ALLOCATE
# define FREE_VAR(var) if (var) REGEX_FREE (var); var = NULL
# define FREE_VARIABLES()						\
  do {									\
    REGEX_FREE_STACK (fail_stack.stack);				\
    FREE_VAR (regstart);						\
    FREE_VAR (regend);							\
    FREE_VAR (old_regstart);						\
    FREE_VAR (old_regend);						\
    FREE_VAR (best_regstart);						\
    FREE_VAR (best_regend);						\
    FREE_VAR (reg_info);						\
    FREE_VAR (reg_dummy);						\
    FREE_VAR (reg_info_dummy);						\
  } while (0)
#else
# define FREE_VARIABLES() ((void)0) /* Do nothing!  But inhibit gcc warning. */
#endif /* not MATCH_MAY_ALLOCATE */

/* These values must meet several constraints.  They must not be valid
   register values; since we have a limit of 255 registers (because
   we use only one byte in the pattern for the register number), we can
   use numbers larger than 255.  They must differ by 1, because of
   NUM_FAILURE_ITEMS above.  And the value for the lowest register must
   be larger than the value for the highest register, so we do not try
   to actually save any registers when none are active.  */
#define NO_HIGHEST_ACTIVE_REG (1 << BYTEWIDTH)
#define NO_LOWEST_ACTIVE_REG (NO_HIGHEST_ACTIVE_REG + 1)

/* Matching routines.  */

#ifndef emacs   /* Emacs never uses this.  */
/* re_match is like re_match_2 except it takes only a single string.  */

int
re_match (bufp, string, size, pos, regs)
     struct re_pattern_buffer *bufp;
     const char *string;
     int size, pos;
     struct re_registers *regs;
{
  int result = re_match_2_internal (bufp, NULL, 0, string, size,
				    pos, regs, size);
# ifndef REGEX_MALLOC
#  ifdef C_ALLOCA
  alloca (0);
#  endif
# endif
  return result;
}
# ifdef _LIBC
weak_alias (__re_match, re_match)
# endif
#endif /* not emacs */

static boolean group_match_null_string_p _RE_ARGS ((unsigned char **p,
						    unsigned char *end,
						register_info_type *reg_info));
static boolean alt_match_null_string_p _RE_ARGS ((unsigned char *p,
						  unsigned char *end,
						register_info_type *reg_info));
static boolean common_op_match_null_string_p _RE_ARGS ((unsigned char **p,
							unsigned char *end,
						register_info_type *reg_info));
static int bcmp_translate _RE_ARGS ((const char *s1, const char *s2,
				     int len, char *translate));

/* re_match_2 matches the compiled pattern in BUFP against the
   the (virtual) concatenation of STRING1 and STRING2 (of length SIZE1
   and SIZE2, respectively).  We start matching at POS, and stop
   matching at STOP.

   If REGS is non-null and the `no_sub' field of BUFP is nonzero, we
   store offsets for the substring each group matched in REGS.  See the
   documentation for exactly how many groups we fill.

   We return -1 if no match, -2 if an internal error (such as the
   failure stack overflowing).  Otherwise, we return the length of the
   matched substring.  */

int
re_match_2 (bufp, string1, size1, string2, size2, pos, regs, stop)
     struct re_pattern_buffer *bufp;
     const char *string1, *string2;
     int size1, size2;
     int pos;
     struct re_registers *regs;
     int stop;
{
  int result = re_match_2_internal (bufp, string1, size1, string2, size2,
				    pos, regs, stop);
#ifndef REGEX_MALLOC
# ifdef C_ALLOCA
  alloca (0);
# endif
#endif
  return result;
}
#ifdef _LIBC
weak_alias (__re_match_2, re_match_2)
#endif

/* This is a separate function so that we can force an alloca cleanup
   afterwards.  */
static int
re_match_2_internal (bufp, string1, size1, string2, size2, pos, regs, stop)
     struct re_pattern_buffer *bufp;
     const char *string1, *string2;
     int size1, size2;
     int pos;
     struct re_registers *regs;
     int stop;
{
  /* General temporaries.  */
  int mcnt;
  unsigned char *p1;

  /* Just past the end of the corresponding string.  */
  const char *end1, *end2;

  /* Pointers into string1 and string2, just past the last characters in
     each to consider matching.  */
  const char *end_match_1, *end_match_2;

  /* Where we are in the data, and the end of the current string.  */
  const char *d, *dend;

  /* Where we are in the pattern, and the end of the pattern.  */
  unsigned char *p = bufp->buffer;
  register unsigned char *pend = p + bufp->used;

  /* Mark the opcode just after a start_memory, so we can test for an
     empty subpattern when we get to the stop_memory.  */
  unsigned char *just_past_start_mem = 0;

  /* We use this to map every character in the string.  */
  RE_TRANSLATE_TYPE translate = bufp->translate;

  /* Failure point stack.  Each place that can handle a failure further
     down the line pushes a failure point on this stack.  It consists of
     restart, regend, and reg_info for all registers corresponding to
     the subexpressions we're currently inside, plus the number of such
     registers, and, finally, two char *'s.  The first char * is where
     to resume scanning the pattern; the second one is where to resume
     scanning the strings.  If the latter is zero, the failure point is
     a ``dummy''; if a failure happens and the failure point is a dummy,
     it gets discarded and the next next one is tried.  */
#ifdef MATCH_MAY_ALLOCATE /* otherwise, this is global.  */
  fail_stack_type fail_stack;
#endif
#ifdef DEBUG
  static unsigned failure_id = 0;
  unsigned nfailure_points_pushed = 0, nfailure_points_popped = 0;
#endif

#ifdef REL_ALLOC
  /* This holds the pointer to the failure stack, when
     it is allocated relocatably.  */
  fail_stack_elt_t *failure_stack_ptr;
#endif

  /* We fill all the registers internally, independent of what we
     return, for use in backreferences.  The number here includes
     an element for register zero.  */
  size_t num_regs = bufp->re_nsub + 1;

  /* The currently active registers.  */
  active_reg_t lowest_active_reg = NO_LOWEST_ACTIVE_REG;
  active_reg_t highest_active_reg = NO_HIGHEST_ACTIVE_REG;

  /* Information on the contents of registers. These are pointers into
     the input strings; they record just what was matched (on this
     attempt) by a subexpression part of the pattern, that is, the
     regnum-th regstart pointer points to where in the pattern we began
     matching and the regnum-th regend points to right after where we
     stopped matching the regnum-th subexpression.  (The zeroth register
     keeps track of what the whole pattern matches.)  */
#ifdef MATCH_MAY_ALLOCATE /* otherwise, these are global.  */
  const char **regstart, **regend;
#endif

  /* If a group that's operated upon by a repetition operator fails to
     match anything, then the register for its start will need to be
     restored because it will have been set to wherever in the string we
     are when we last see its open-group operator.  Similarly for a
     register's end.  */
#ifdef MATCH_MAY_ALLOCATE /* otherwise, these are global.  */
  const char **old_regstart, **old_regend;
#endif

  /* The is_active field of reg_info helps us keep track of which (possibly
     nested) subexpressions we are currently in. The matched_something
     field of reg_info[reg_num] helps us tell whether or not we have
     matched any of the pattern so far this time through the reg_num-th
     subexpression.  These two fields get reset each time through any
     loop their register is in.  */
#ifdef MATCH_MAY_ALLOCATE /* otherwise, this is global.  */
  register_info_type *reg_info;
#endif

  /* The following record the register info as found in the above
     variables when we find a match better than any we've seen before.
     This happens as we backtrack through the failure points, which in
     turn happens only if we have not yet matched the entire string. */
  unsigned best_regs_set = false;
#ifdef MATCH_MAY_ALLOCATE /* otherwise, these are global.  */
  const char **best_regstart, **best_regend;
#endif

  /* Logically, this is `best_regend[0]'.  But we don't want to have to
     allocate space for that if we're not allocating space for anything
     else (see below).  Also, we never need info about register 0 for
     any of the other register vectors, and it seems rather a kludge to
     treat `best_regend' differently than the rest.  So we keep track of
     the end of the best match so far in a separate variable.  We
     initialize this to NULL so that when we backtrack the first time
     and need to test it, it's not garbage.  */
  const char *match_end = NULL;

  /* This helps SET_REGS_MATCHED avoid doing redundant work.  */
  int set_regs_matched_done = 0;

  /* Used when we pop values we don't care about.  */
#ifdef MATCH_MAY_ALLOCATE /* otherwise, these are global.  */
  const char **reg_dummy;
  register_info_type *reg_info_dummy;
#endif

#ifdef DEBUG
  /* Counts the total number of registers pushed.  */
  unsigned num_regs_pushed = 0;
#endif

  DEBUG_PRINT1 ("\n\nEntering re_match_2.\n");

  INIT_FAIL_STACK ();

#ifdef MATCH_MAY_ALLOCATE
  /* Do not bother to initialize all the register variables if there are
     no groups in the pattern, as it takes a fair amount of time.  If
     there are groups, we include space for register 0 (the whole
     pattern), even though we never use it, since it simplifies the
     array indexing.  We should fix this.  */
  if (bufp->re_nsub)
    {
      regstart = REGEX_TALLOC (num_regs, const char *);
      regend = REGEX_TALLOC (num_regs, const char *);
      old_regstart = REGEX_TALLOC (num_regs, const char *);
      old_regend = REGEX_TALLOC (num_regs, const char *);
      best_regstart = REGEX_TALLOC (num_regs, const char *);
      best_regend = REGEX_TALLOC (num_regs, const char *);
      reg_info = REGEX_TALLOC (num_regs, register_info_type);
      reg_dummy = REGEX_TALLOC (num_regs, const char *);
      reg_info_dummy = REGEX_TALLOC (num_regs, register_info_type);

      if (!(regstart && regend && old_regstart && old_regend && reg_info
            && best_regstart && best_regend && reg_dummy && reg_info_dummy))
        {
          FREE_VARIABLES ();
          return -2;
        }
    }
  else
    {
      /* We must initialize all our variables to NULL, so that
         `FREE_VARIABLES' doesn't try to free them.  */
      regstart = regend = old_regstart = old_regend = best_regstart
        = best_regend = reg_dummy = NULL;
      reg_info = reg_info_dummy = (register_info_type *) NULL;
    }
#endif /* MATCH_MAY_ALLOCATE */

  /* The starting position is bogus.  */
  if (pos < 0 || pos > size1 + size2)
    {
      FREE_VARIABLES ();
      return -1;
    }

  /* Initialize subexpression text positions to -1 to mark ones that no
     start_memory/stop_memory has been seen for. Also initialize the
     register information struct.  */
  for (mcnt = 1; (unsigned) mcnt < num_regs; mcnt++)
    {
      regstart[mcnt] = regend[mcnt]
        = old_regstart[mcnt] = old_regend[mcnt] = REG_UNSET_VALUE;

      REG_MATCH_NULL_STRING_P (reg_info[mcnt]) = MATCH_NULL_UNSET_VALUE;
      IS_ACTIVE (reg_info[mcnt]) = 0;
      MATCHED_SOMETHING (reg_info[mcnt]) = 0;
      EVER_MATCHED_SOMETHING (reg_info[mcnt]) = 0;
    }

  /* We move `string1' into `string2' if the latter's empty -- but not if
     `string1' is null.  */
  if (size2 == 0 && string1 != NULL)
    {
      string2 = string1;
      size2 = size1;
      string1 = 0;
      size1 = 0;
    }
  end1 = string1 + size1;
  end2 = string2 + size2;

  /* Compute where to stop matching, within the two strings.  */
  if (stop <= size1)
    {
      end_match_1 = string1 + stop;
      end_match_2 = string2;
    }
  else
    {
      end_match_1 = end1;
      end_match_2 = string2 + stop - size1;
    }

  /* `p' scans through the pattern as `d' scans through the data.
     `dend' is the end of the input string that `d' points within.  `d'
     is advanced into the following input string whenever necessary, but
     this happens before fetching; therefore, at the beginning of the
     loop, `d' can be pointing at the end of a string, but it cannot
     equal `string2'.  */
  if (size1 > 0 && pos <= size1)
    {
      d = string1 + pos;
      dend = end_match_1;
    }
  else
    {
      d = string2 + pos - size1;
      dend = end_match_2;
    }

  DEBUG_PRINT1 ("The compiled pattern is:\n");
  DEBUG_PRINT_COMPILED_PATTERN (bufp, p, pend);
  DEBUG_PRINT1 ("The string to match is: `");
  DEBUG_PRINT_DOUBLE_STRING (d, string1, size1, string2, size2);
  DEBUG_PRINT1 ("'\n");

  /* This loops over pattern commands.  It exits by returning from the
     function if the match is complete, or it drops through if the match
     fails at this starting point in the input data.  */
  for (;;)
    {
#ifdef _LIBC
      DEBUG_PRINT2 ("\n%p: ", p);
#else
      DEBUG_PRINT2 ("\n0x%x: ", p);
#endif

      if (p == pend)
	{ /* End of pattern means we might have succeeded.  */
          DEBUG_PRINT1 ("end of pattern ... ");

	  /* If we haven't matched the entire string, and we want the
             longest match, try backtracking.  */
          if (d != end_match_2)
	    {
	      /* 1 if this match ends in the same string (string1 or string2)
		 as the best previous match.  */
	      boolean same_str_p = (FIRST_STRING_P (match_end)
				    == MATCHING_IN_FIRST_STRING);
	      /* 1 if this match is the best seen so far.  */
	      boolean best_match_p;

	      /* AIX compiler got confused when this was combined
		 with the previous declaration.  */
	      if (same_str_p)
		best_match_p = d > match_end;
	      else
		best_match_p = !MATCHING_IN_FIRST_STRING;

              DEBUG_PRINT1 ("backtracking.\n");

              if (!FAIL_STACK_EMPTY ())
                { /* More failure points to try.  */

                  /* If exceeds best match so far, save it.  */
                  if (!best_regs_set || best_match_p)
                    {
                      best_regs_set = true;
                      match_end = d;

                      DEBUG_PRINT1 ("\nSAVING match as best so far.\n");

                      for (mcnt = 1; (unsigned) mcnt < num_regs; mcnt++)
                        {
                          best_regstart[mcnt] = regstart[mcnt];
                          best_regend[mcnt] = regend[mcnt];
                        }
                    }
                  goto fail;
                }

              /* If no failure points, don't restore garbage.  And if
                 last match is real best match, don't restore second
                 best one. */
              else if (best_regs_set && !best_match_p)
                {
  	        restore_best_regs:
                  /* Restore best match.  It may happen that `dend ==
                     end_match_1' while the restored d is in string2.
                     For example, the pattern `x.*y.*z' against the
                     strings `x-' and `y-z-', if the two strings are
                     not consecutive in memory.  */
                  DEBUG_PRINT1 ("Restoring best registers.\n");

                  d = match_end;
                  dend = ((d >= string1 && d <= end1)
		           ? end_match_1 : end_match_2);

		  for (mcnt = 1; (unsigned) mcnt < num_regs; mcnt++)
		    {
		      regstart[mcnt] = best_regstart[mcnt];
		      regend[mcnt] = best_regend[mcnt];
		    }
                }
            } /* d != end_match_2 */

	succeed_label:
          DEBUG_PRINT1 ("Accepting match.\n");

          /* If caller wants register contents data back, do it.  */
          if (regs && !bufp->no_sub)
	    {
              /* Have the register data arrays been allocated?  */
              if (bufp->regs_allocated == REGS_UNALLOCATED)
                { /* No.  So allocate them with malloc.  We need one
                     extra element beyond `num_regs' for the `-1' marker
                     GNU code uses.  */
                  regs->num_regs = MAX (RE_NREGS, num_regs + 1);
                  regs->start = TALLOC (regs->num_regs, regoff_t);
                  regs->end = TALLOC (regs->num_regs, regoff_t);
                  if (regs->start == NULL || regs->end == NULL)
		    {
		      FREE_VARIABLES ();
		      return -2;
		    }
                  bufp->regs_allocated = REGS_REALLOCATE;
                }
              else if (bufp->regs_allocated == REGS_REALLOCATE)
                { /* Yes.  If we need more elements than were already
                     allocated, reallocate them.  If we need fewer, just
                     leave it alone.  */
                  if (regs->num_regs < num_regs + 1)
                    {
                      regs->num_regs = num_regs + 1;
                      RETALLOC (regs->start, regs->num_regs, regoff_t);
                      RETALLOC (regs->end, regs->num_regs, regoff_t);
                      if (regs->start == NULL || regs->end == NULL)
			{
			  FREE_VARIABLES ();
			  return -2;
			}
                    }
                }
              else
		{
		  /* These braces fend off a "empty body in an else-statement"
		     warning under GCC when assert expands to nothing.  */
		  assert (bufp->regs_allocated == REGS_FIXED);
		}

              /* Convert the pointer data in `regstart' and `regend' to
                 indices.  Register zero has to be set differently,
                 since we haven't kept track of any info for it.  */
              if (regs->num_regs > 0)
                {
                  regs->start[0] = pos;
                  regs->end[0] = (MATCHING_IN_FIRST_STRING
				  ? ((regoff_t) (d - string1))
			          : ((regoff_t) (d - string2 + size1)));
                }

              /* Go through the first `min (num_regs, regs->num_regs)'
                 registers, since that is all we initialized.  */
	      for (mcnt = 1; (unsigned) mcnt < MIN (num_regs, regs->num_regs);
		   mcnt++)
		{
                  if (REG_UNSET (regstart[mcnt]) || REG_UNSET (regend[mcnt]))
                    regs->start[mcnt] = regs->end[mcnt] = -1;
                  else
                    {
		      regs->start[mcnt]
			= (regoff_t) POINTER_TO_OFFSET (regstart[mcnt]);
                      regs->end[mcnt]
			= (regoff_t) POINTER_TO_OFFSET (regend[mcnt]);
                    }
		}

              /* If the regs structure we return has more elements than
                 were in the pattern, set the extra elements to -1.  If
                 we (re)allocated the registers, this is the case,
                 because we always allocate enough to have at least one
                 -1 at the end.  */
              for (mcnt = num_regs; (unsigned) mcnt < regs->num_regs; mcnt++)
                regs->start[mcnt] = regs->end[mcnt] = -1;
	    } /* regs && !bufp->no_sub */

          DEBUG_PRINT4 ("%u failure points pushed, %u popped (%u remain).\n",
                        nfailure_points_pushed, nfailure_points_popped,
                        nfailure_points_pushed - nfailure_points_popped);
          DEBUG_PRINT2 ("%u registers pushed.\n", num_regs_pushed);

          mcnt = d - pos - (MATCHING_IN_FIRST_STRING
			    ? string1
			    : string2 - size1);

          DEBUG_PRINT2 ("Returning %d from re_match_2.\n", mcnt);

          FREE_VARIABLES ();
          return mcnt;
        }

      /* Otherwise match next pattern command.  */
      switch (SWITCH_ENUM_CAST ((re_opcode_t) *p++))
	{
        /* Ignore these.  Used to ignore the n of succeed_n's which
           currently have n == 0.  */
        case no_op:
          DEBUG_PRINT1 ("EXECUTING no_op.\n");
          break;

	case succeed:
          DEBUG_PRINT1 ("EXECUTING succeed.\n");
	  goto succeed_label;

        /* Match the next n pattern characters exactly.  The following
           byte in the pattern defines n, and the n bytes after that
           are the characters to match.  */
	case exactn:
	  mcnt = *p++;
          DEBUG_PRINT2 ("EXECUTING exactn %d.\n", mcnt);

          /* This is written out as an if-else so we don't waste time
             testing `translate' inside the loop.  */
          if (translate)
	    {
	      do
		{
		  PREFETCH ();
		  if ((unsigned char) translate[(unsigned char) *d++]
		      != (unsigned char) *p++)
                    goto fail;
		}
	      while (--mcnt);
	    }
	  else
	    {
	      do
		{
		  PREFETCH ();
		  if (*d++ != (char) *p++) goto fail;
		}
	      while (--mcnt);
	    }
	  SET_REGS_MATCHED ();
          break;


        /* Match any character except possibly a newline or a null.  */
	case anychar:
          DEBUG_PRINT1 ("EXECUTING anychar.\n");

          PREFETCH ();

          if ((!(bufp->syntax & RE_DOT_NEWLINE) && TRANSLATE (*d) == '\n')
              || (bufp->syntax & RE_DOT_NOT_NULL && TRANSLATE (*d) == '\000'))
	    goto fail;

          SET_REGS_MATCHED ();
          DEBUG_PRINT2 ("  Matched `%d'.\n", *d);
          d++;
	  break;


	case charset:
	case charset_not:
	  {
	    register unsigned char c;
	    boolean not = (re_opcode_t) *(p - 1) == charset_not;

            DEBUG_PRINT2 ("EXECUTING charset%s.\n", not ? "_not" : "");

	    PREFETCH ();
	    c = TRANSLATE (*d); /* The character to match.  */

            /* Cast to `unsigned' instead of `unsigned char' in case the
               bit list is a full 32 bytes long.  */
	    if (c < (unsigned) (*p * BYTEWIDTH)
		&& p[1 + c / BYTEWIDTH] & (1 << (c % BYTEWIDTH)))
	      not = !not;

	    p += 1 + *p;

	    if (!not) goto fail;

	    SET_REGS_MATCHED ();
            d++;
	    break;
	  }


        /* The beginning of a group is represented by start_memory.
           The arguments are the register number in the next byte, and the
           number of groups inner to this one in the next.  The text
           matched within the group is recorded (in the internal
           registers data structure) under the register number.  */
        case start_memory:
	  DEBUG_PRINT3 ("EXECUTING start_memory %d (%d):\n", *p, p[1]);

          /* Find out if this group can match the empty string.  */
	  p1 = p;		/* To send to group_match_null_string_p.  */

          if (REG_MATCH_NULL_STRING_P (reg_info[*p]) == MATCH_NULL_UNSET_VALUE)
            REG_MATCH_NULL_STRING_P (reg_info[*p])
              = group_match_null_string_p (&p1, pend, reg_info);

          /* Save the position in the string where we were the last time
             we were at this open-group operator in case the group is
             operated upon by a repetition operator, e.g., with `(a*)*b'
             against `ab'; then we want to ignore where we are now in
             the string in case this attempt to match fails.  */
          old_regstart[*p] = REG_MATCH_NULL_STRING_P (reg_info[*p])
                             ? REG_UNSET (regstart[*p]) ? d : regstart[*p]
                             : regstart[*p];
	  DEBUG_PRINT2 ("  old_regstart: %d\n",
			 POINTER_TO_OFFSET (old_regstart[*p]));

          regstart[*p] = d;
	  DEBUG_PRINT2 ("  regstart: %d\n", POINTER_TO_OFFSET (regstart[*p]));

          IS_ACTIVE (reg_info[*p]) = 1;
          MATCHED_SOMETHING (reg_info[*p]) = 0;

	  /* Clear this whenever we change the register activity status.  */
	  set_regs_matched_done = 0;

          /* This is the new highest active register.  */
          highest_active_reg = *p;

          /* If nothing was active before, this is the new lowest active
             register.  */
          if (lowest_active_reg == NO_LOWEST_ACTIVE_REG)
            lowest_active_reg = *p;

          /* Move past the register number and inner group count.  */
          p += 2;
	  just_past_start_mem = p;

          break;


        /* The stop_memory opcode represents the end of a group.  Its
           arguments are the same as start_memory's: the register
           number, and the number of inner groups.  */
	case stop_memory:
	  DEBUG_PRINT3 ("EXECUTING stop_memory %d (%d):\n", *p, p[1]);

          /* We need to save the string position the last time we were at
             this close-group operator in case the group is operated
             upon by a repetition operator, e.g., with `((a*)*(b*)*)*'
             against `aba'; then we want to ignore where we are now in
             the string in case this attempt to match fails.  */
          old_regend[*p] = REG_MATCH_NULL_STRING_P (reg_info[*p])
                           ? REG_UNSET (regend[*p]) ? d : regend[*p]
			   : regend[*p];
	  DEBUG_PRINT2 ("      old_regend: %d\n",
			 POINTER_TO_OFFSET (old_regend[*p]));

          regend[*p] = d;
	  DEBUG_PRINT2 ("      regend: %d\n", POINTER_TO_OFFSET (regend[*p]));

          /* This register isn't active anymore.  */
          IS_ACTIVE (reg_info[*p]) = 0;

	  /* Clear this whenever we change the register activity status.  */
	  set_regs_matched_done = 0;

          /* If this was the only register active, nothing is active
             anymore.  */
          if (lowest_active_reg == highest_active_reg)
            {
              lowest_active_reg = NO_LOWEST_ACTIVE_REG;
              highest_active_reg = NO_HIGHEST_ACTIVE_REG;
            }
          else
            { /* We must scan for the new highest active register, since
                 it isn't necessarily one less than now: consider
                 (a(b)c(d(e)f)g).  When group 3 ends, after the f), the
                 new highest active register is 1.  */
              unsigned char r = *p - 1;
              while (r > 0 && !IS_ACTIVE (reg_info[r]))
                r--;

              /* If we end up at register zero, that means that we saved
                 the registers as the result of an `on_failure_jump', not
                 a `start_memory', and we jumped to past the innermost
                 `stop_memory'.  For example, in ((.)*) we save
                 registers 1 and 2 as a result of the *, but when we pop
                 back to the second ), we are at the stop_memory 1.
                 Thus, nothing is active.  */
	      if (r == 0)
                {
                  lowest_active_reg = NO_LOWEST_ACTIVE_REG;
                  highest_active_reg = NO_HIGHEST_ACTIVE_REG;
                }
              else
                highest_active_reg = r;
            }

          /* If just failed to match something this time around with a
             group that's operated on by a repetition operator, try to
             force exit from the ``loop'', and restore the register
             information for this group that we had before trying this
             last match.  */
          if ((!MATCHED_SOMETHING (reg_info[*p])
               || just_past_start_mem == p - 1)
	      && (p + 2) < pend)
            {
              boolean is_a_jump_n = false;

              p1 = p + 2;
              mcnt = 0;
              switch ((re_opcode_t) *p1++)
                {
                  case jump_n:
		    is_a_jump_n = true;
                  case pop_failure_jump:
		  case maybe_pop_jump:
		  case jump:
		  case dummy_failure_jump:
                    EXTRACT_NUMBER_AND_INCR (mcnt, p1);
		    if (is_a_jump_n)
		      p1 += 2;
                    break;

                  default:
                    /* do nothing */ ;
                }
	      p1 += mcnt;

              /* If the next operation is a jump backwards in the pattern
	         to an on_failure_jump right before the start_memory
                 corresponding to this stop_memory, exit from the loop
                 by forcing a failure after pushing on the stack the
                 on_failure_jump's jump in the pattern, and d.  */
              if (mcnt < 0 && (re_opcode_t) *p1 == on_failure_jump
                  && (re_opcode_t) p1[3] == start_memory && p1[4] == *p)
		{
                  /* If this group ever matched anything, then restore
                     what its registers were before trying this last
                     failed match, e.g., with `(a*)*b' against `ab' for
                     regstart[1], and, e.g., with `((a*)*(b*)*)*'
                     against `aba' for regend[3].

                     Also restore the registers for inner groups for,
                     e.g., `((a*)(b*))*' against `aba' (register 3 would
                     otherwise get trashed).  */

                  if (EVER_MATCHED_SOMETHING (reg_info[*p]))
		    {
		      unsigned r;

                      EVER_MATCHED_SOMETHING (reg_info[*p]) = 0;

		      /* Restore this and inner groups' (if any) registers.  */
                      for (r = *p; r < (unsigned) *p + (unsigned) *(p + 1);
			   r++)
                        {
                          regstart[r] = old_regstart[r];

                          /* xx why this test?  */
                          if (old_regend[r] >= regstart[r])
                            regend[r] = old_regend[r];
                        }
                    }
		  p1++;
                  EXTRACT_NUMBER_AND_INCR (mcnt, p1);
                  PUSH_FAILURE_POINT (p1 + mcnt, d, -2);

                  goto fail;
                }
            }

          /* Move past the register number and the inner group count.  */
          p += 2;
          break;


	/* \<digit> has been turned into a `duplicate' command which is
           followed by the numeric value of <digit> as the register number.  */
        case duplicate:
	  {
	    register const char *d2, *dend2;
	    int regno = *p++;   /* Get which register to match against.  */
	    DEBUG_PRINT2 ("EXECUTING duplicate %d.\n", regno);

	    /* Can't back reference a group which we've never matched.  */
            if (REG_UNSET (regstart[regno]) || REG_UNSET (regend[regno]))
              goto fail;

            /* Where in input to try to start matching.  */
            d2 = regstart[regno];

            /* Where to stop matching; if both the place to start and
               the place to stop matching are in the same string, then
               set to the place to stop, otherwise, for now have to use
               the end of the first string.  */

            dend2 = ((FIRST_STRING_P (regstart[regno])
		      == FIRST_STRING_P (regend[regno]))
		     ? regend[regno] : end_match_1);
	    for (;;)
	      {
		/* If necessary, advance to next segment in register
                   contents.  */
		while (d2 == dend2)
		  {
		    if (dend2 == end_match_2) break;
		    if (dend2 == regend[regno]) break;

                    /* End of string1 => advance to string2. */
                    d2 = string2;
                    dend2 = regend[regno];
		  }
		/* At end of register contents => success */
		if (d2 == dend2) break;

		/* If necessary, advance to next segment in data.  */
		PREFETCH ();

		/* How many characters left in this segment to match.  */
		mcnt = dend - d;

		/* Want how many consecutive characters we can match in
                   one shot, so, if necessary, adjust the count.  */
                if (mcnt > dend2 - d2)
		  mcnt = dend2 - d2;

		/* Compare that many; failure if mismatch, else move
                   past them.  */
		if (translate
                    ? bcmp_translate (d, d2, mcnt, translate)
                    : memcmp (d, d2, mcnt))
		  goto fail;
		d += mcnt, d2 += mcnt;

		/* Do this because we've match some characters.  */
		SET_REGS_MATCHED ();
	      }
	  }
	  break;


        /* begline matches the empty string at the beginning of the string
           (unless `not_bol' is set in `bufp'), and, if
           `newline_anchor' is set, after newlines.  */
	case begline:
          DEBUG_PRINT1 ("EXECUTING begline.\n");

          if (AT_STRINGS_BEG (d))
            {
              if (!bufp->not_bol) break;
            }
          else if (d[-1] == '\n' && bufp->newline_anchor)
            {
              break;
            }
          /* In all other cases, we fail.  */
          goto fail;


        /* endline is the dual of begline.  */
	case endline:
          DEBUG_PRINT1 ("EXECUTING endline.\n");

          if (AT_STRINGS_END (d))
            {
              if (!bufp->not_eol) break;
            }

          /* We have to ``prefetch'' the next character.  */
          else if ((d == end1 ? *string2 : *d) == '\n'
                   && bufp->newline_anchor)
            {
              break;
            }
          goto fail;


	/* Match at the very beginning of the data.  */
        case begbuf:
          DEBUG_PRINT1 ("EXECUTING begbuf.\n");
          if (AT_STRINGS_BEG (d))
            break;
          goto fail;


	/* Match at the very end of the data.  */
        case endbuf:
          DEBUG_PRINT1 ("EXECUTING endbuf.\n");
	  if (AT_STRINGS_END (d))
	    break;
          goto fail;


        /* on_failure_keep_string_jump is used to optimize `.*\n'.  It
           pushes NULL as the value for the string on the stack.  Then
           `pop_failure_point' will keep the current value for the
           string, instead of restoring it.  To see why, consider
           matching `foo\nbar' against `.*\n'.  The .* matches the foo;
           then the . fails against the \n.  But the next thing we want
           to do is match the \n against the \n; if we restored the
           string value, we would be back at the foo.

           Because this is used only in specific cases, we don't need to
           check all the things that `on_failure_jump' does, to make
           sure the right things get saved on the stack.  Hence we don't
           share its code.  The only reason to push anything on the
           stack at all is that otherwise we would have to change
           `anychar's code to do something besides goto fail in this
           case; that seems worse than this.  */
        case on_failure_keep_string_jump:
          DEBUG_PRINT1 ("EXECUTING on_failure_keep_string_jump");

          EXTRACT_NUMBER_AND_INCR (mcnt, p);
#ifdef _LIBC
          DEBUG_PRINT3 (" %d (to %p):\n", mcnt, p + mcnt);
#else
          DEBUG_PRINT3 (" %d (to 0x%x):\n", mcnt, p + mcnt);
#endif

          PUSH_FAILURE_POINT (p + mcnt, NULL, -2);
          break;


	/* Uses of on_failure_jump:

           Each alternative starts with an on_failure_jump that points
           to the beginning of the next alternative.  Each alternative
           except the last ends with a jump that in effect jumps past
           the rest of the alternatives.  (They really jump to the
           ending jump of the following alternative, because tensioning
           these jumps is a hassle.)

           Repeats start with an on_failure_jump that points past both
           the repetition text and either the following jump or
           pop_failure_jump back to this on_failure_jump.  */
	case on_failure_jump:
        on_failure:
          DEBUG_PRINT1 ("EXECUTING on_failure_jump");

          EXTRACT_NUMBER_AND_INCR (mcnt, p);
#ifdef _LIBC
          DEBUG_PRINT3 (" %d (to %p)", mcnt, p + mcnt);
#else
          DEBUG_PRINT3 (" %d (to 0x%x)", mcnt, p + mcnt);
#endif

          /* If this on_failure_jump comes right before a group (i.e.,
             the original * applied to a group), save the information
             for that group and all inner ones, so that if we fail back
             to this point, the group's information will be correct.
             For example, in \(a*\)*\1, we need the preceding group,
             and in \(zz\(a*\)b*\)\2, we need the inner group.  */

          /* We can't use `p' to check ahead because we push
             a failure point to `p + mcnt' after we do this.  */
          p1 = p;

          /* We need to skip no_op's before we look for the
             start_memory in case this on_failure_jump is happening as
             the result of a completed succeed_n, as in \(a\)\{1,3\}b\1
             against aba.  */
          while (p1 < pend && (re_opcode_t) *p1 == no_op)
            p1++;

          if (p1 < pend && (re_opcode_t) *p1 == start_memory)
            {
              /* We have a new highest active register now.  This will
                 get reset at the start_memory we are about to get to,
                 but we will have saved all the registers relevant to
                 this repetition op, as described above.  */
              highest_active_reg = *(p1 + 1) + *(p1 + 2);
              if (lowest_active_reg == NO_LOWEST_ACTIVE_REG)
                lowest_active_reg = *(p1 + 1);
            }

          DEBUG_PRINT1 (":\n");
          PUSH_FAILURE_POINT (p + mcnt, d, -2);
          break;


        /* A smart repeat ends with `maybe_pop_jump'.
	   We change it to either `pop_failure_jump' or `jump'.  */
        case maybe_pop_jump:
          EXTRACT_NUMBER_AND_INCR (mcnt, p);
          DEBUG_PRINT2 ("EXECUTING maybe_pop_jump %d.\n", mcnt);
          {
	    register unsigned char *p2 = p;

            /* Compare the beginning of the repeat with what in the
               pattern follows its end. If we can establish that there
               is nothing that they would both match, i.e., that we
               would have to backtrack because of (as in, e.g., `a*a')
               then we can change to pop_failure_jump, because we'll
               never have to backtrack.

               This is not true in the case of alternatives: in
               `(a|ab)*' we do need to backtrack to the `ab' alternative
               (e.g., if the string was `ab').  But instead of trying to
               detect that here, the alternative has put on a dummy
               failure point which is what we will end up popping.  */

	    /* Skip over open/close-group commands.
	       If what follows this loop is a ...+ construct,
	       look at what begins its body, since we will have to
	       match at least one of that.  */
	    while (1)
	      {
		if (p2 + 2 < pend
		    && ((re_opcode_t) *p2 == stop_memory
			|| (re_opcode_t) *p2 == start_memory))
		  p2 += 3;
		else if (p2 + 6 < pend
			 && (re_opcode_t) *p2 == dummy_failure_jump)
		  p2 += 6;
		else
		  break;
	      }

	    p1 = p + mcnt;
	    /* p1[0] ... p1[2] are the `on_failure_jump' corresponding
	       to the `maybe_finalize_jump' of this case.  Examine what
	       follows.  */

            /* If we're at the end of the pattern, we can change.  */
            if (p2 == pend)
	      {
		/* Consider what happens when matching ":\(.*\)"
		   against ":/".  I don't really understand this code
		   yet.  */
  	        p[-3] = (unsigned char) pop_failure_jump;
                DEBUG_PRINT1
                  ("  End of pattern: change to `pop_failure_jump'.\n");
              }

            else if ((re_opcode_t) *p2 == exactn
		     || (bufp->newline_anchor && (re_opcode_t) *p2 == endline))
	      {
		register unsigned char c
                  = *p2 == (unsigned char) endline ? '\n' : p2[2];

                if ((re_opcode_t) p1[3] == exactn && p1[5] != c)
                  {
  		    p[-3] = (unsigned char) pop_failure_jump;
                    DEBUG_PRINT3 ("  %c != %c => pop_failure_jump.\n",
                                  c, p1[5]);
                  }

		else if ((re_opcode_t) p1[3] == charset
			 || (re_opcode_t) p1[3] == charset_not)
		  {
		    int not = (re_opcode_t) p1[3] == charset_not;

		    if (c < (unsigned char) (p1[4] * BYTEWIDTH)
			&& p1[5 + c / BYTEWIDTH] & (1 << (c % BYTEWIDTH)))
		      not = !not;

                    /* `not' is equal to 1 if c would match, which means
                        that we can't change to pop_failure_jump.  */
		    if (!not)
                      {
  		        p[-3] = (unsigned char) pop_failure_jump;
                        DEBUG_PRINT1 ("  No match => pop_failure_jump.\n");
                      }
		  }
	      }
            else if ((re_opcode_t) *p2 == charset)
	      {
#ifdef DEBUG
		register unsigned char c
                  = *p2 == (unsigned char) endline ? '\n' : p2[2];
#endif

#if 0
                if ((re_opcode_t) p1[3] == exactn
		    && ! ((int) p2[1] * BYTEWIDTH > (int) p1[5]
			  && (p2[2 + p1[5] / BYTEWIDTH]
			      & (1 << (p1[5] % BYTEWIDTH)))))
#else
                if ((re_opcode_t) p1[3] == exactn
		    && ! ((int) p2[1] * BYTEWIDTH > (int) p1[4]
			  && (p2[2 + p1[4] / BYTEWIDTH]
			      & (1 << (p1[4] % BYTEWIDTH)))))
#endif
                  {
  		    p[-3] = (unsigned char) pop_failure_jump;
                    DEBUG_PRINT3 ("  %c != %c => pop_failure_jump.\n",
                                  c, p1[5]);
                  }

		else if ((re_opcode_t) p1[3] == charset_not)
		  {
		    int idx;
		    /* We win if the charset_not inside the loop
		       lists every character listed in the charset after.  */
		    for (idx = 0; idx < (int) p2[1]; idx++)
		      if (! (p2[2 + idx] == 0
			     || (idx < (int) p1[4]
				 && ((p2[2 + idx] & ~ p1[5 + idx]) == 0))))
			break;

		    if (idx == p2[1])
                      {
  		        p[-3] = (unsigned char) pop_failure_jump;
                        DEBUG_PRINT1 ("  No match => pop_failure_jump.\n");
                      }
		  }
		else if ((re_opcode_t) p1[3] == charset)
		  {
		    int idx;
		    /* We win if the charset inside the loop
		       has no overlap with the one after the loop.  */
		    for (idx = 0;
			 idx < (int) p2[1] && idx < (int) p1[4];
			 idx++)
		      if ((p2[2 + idx] & p1[5 + idx]) != 0)
			break;

		    if (idx == p2[1] || idx == p1[4])
                      {
  		        p[-3] = (unsigned char) pop_failure_jump;
                        DEBUG_PRINT1 ("  No match => pop_failure_jump.\n");
                      }
		  }
	      }
	  }
	  p -= 2;		/* Point at relative address again.  */
	  if ((re_opcode_t) p[-1] != pop_failure_jump)
	    {
	      p[-1] = (unsigned char) jump;
              DEBUG_PRINT1 ("  Match => jump.\n");
	      goto unconditional_jump;
	    }
        /* Note fall through.  */


	/* The end of a simple repeat has a pop_failure_jump back to
           its matching on_failure_jump, where the latter will push a
           failure point.  The pop_failure_jump takes off failure
           points put on by this pop_failure_jump's matching
           on_failure_jump; we got through the pattern to here from the
           matching on_failure_jump, so didn't fail.  */
        case pop_failure_jump:
          {
            /* We need to pass separate storage for the lowest and
               highest registers, even though we don't care about the
               actual values.  Otherwise, we will restore only one
               register from the stack, since lowest will == highest in
               `pop_failure_point'.  */
            active_reg_t dummy_low_reg, dummy_high_reg;
            unsigned char *pdummy;
            const char *sdummy;

            DEBUG_PRINT1 ("EXECUTING pop_failure_jump.\n");
            POP_FAILURE_POINT (sdummy, pdummy,
                               dummy_low_reg, dummy_high_reg,
                               reg_dummy, reg_dummy, reg_info_dummy);
          }
	  /* Note fall through.  */

	unconditional_jump:
#ifdef _LIBC
	  DEBUG_PRINT2 ("\n%p: ", p);
#else
	  DEBUG_PRINT2 ("\n0x%x: ", p);
#endif
          /* Note fall through.  */

        /* Unconditionally jump (without popping any failure points).  */
        case jump:
	  EXTRACT_NUMBER_AND_INCR (mcnt, p);	/* Get the amount to jump.  */
          DEBUG_PRINT2 ("EXECUTING jump %d ", mcnt);
	  p += mcnt;				/* Do the jump.  */
#ifdef _LIBC
          DEBUG_PRINT2 ("(to %p).\n", p);
#else
          DEBUG_PRINT2 ("(to 0x%x).\n", p);
#endif
	  break;


        /* We need this opcode so we can detect where alternatives end
           in `group_match_null_string_p' et al.  */
        case jump_past_alt:
          DEBUG_PRINT1 ("EXECUTING jump_past_alt.\n");
          goto unconditional_jump;


        /* Normally, the on_failure_jump pushes a failure point, which
           then gets popped at pop_failure_jump.  We will end up at
           pop_failure_jump, also, and with a pattern of, say, `a+', we
           are skipping over the on_failure_jump, so we have to push
           something meaningless for pop_failure_jump to pop.  */
        case dummy_failure_jump:
          DEBUG_PRINT1 ("EXECUTING dummy_failure_jump.\n");
          /* It doesn't matter what we push for the string here.  What
             the code at `fail' tests is the value for the pattern.  */
          PUSH_FAILURE_POINT (NULL, NULL, -2);
          goto unconditional_jump;


        /* At the end of an alternative, we need to push a dummy failure
           point in case we are followed by a `pop_failure_jump', because
           we don't want the failure point for the alternative to be
           popped.  For example, matching `(a|ab)*' against `aab'
           requires that we match the `ab' alternative.  */
        case push_dummy_failure:
          DEBUG_PRINT1 ("EXECUTING push_dummy_failure.\n");
          /* See comments just above at `dummy_failure_jump' about the
             two zeroes.  */
          PUSH_FAILURE_POINT (NULL, NULL, -2);
          break;

        /* Have to succeed matching what follows at least n times.
           After that, handle like `on_failure_jump'.  */
        case succeed_n:
          EXTRACT_NUMBER (mcnt, p + 2);
          DEBUG_PRINT2 ("EXECUTING succeed_n %d.\n", mcnt);

          assert (mcnt >= 0);
          /* Originally, this is how many times we HAVE to succeed.  */
          if (mcnt > 0)
            {
               mcnt--;
	       p += 2;
               STORE_NUMBER_AND_INCR (p, mcnt);
#ifdef _LIBC
               DEBUG_PRINT3 ("  Setting %p to %d.\n", p - 2, mcnt);
#else
               DEBUG_PRINT3 ("  Setting 0x%x to %d.\n", p - 2, mcnt);
#endif
            }
	  else if (mcnt == 0)
            {
#ifdef _LIBC
              DEBUG_PRINT2 ("  Setting two bytes from %p to no_op.\n", p+2);
#else
              DEBUG_PRINT2 ("  Setting two bytes from 0x%x to no_op.\n", p+2);
#endif
	      p[2] = (unsigned char) no_op;
              p[3] = (unsigned char) no_op;
              goto on_failure;
            }
          break;

        case jump_n:
          EXTRACT_NUMBER (mcnt, p + 2);
          DEBUG_PRINT2 ("EXECUTING jump_n %d.\n", mcnt);

          /* Originally, this is how many times we CAN jump.  */
          if (mcnt)
            {
               mcnt--;
               STORE_NUMBER (p + 2, mcnt);
#ifdef _LIBC
               DEBUG_PRINT3 ("  Setting %p to %d.\n", p + 2, mcnt);
#else
               DEBUG_PRINT3 ("  Setting 0x%x to %d.\n", p + 2, mcnt);
#endif
	       goto unconditional_jump;
            }
          /* If don't have to jump any more, skip over the rest of command.  */
	  else
	    p += 4;
          break;

	case set_number_at:
	  {
            DEBUG_PRINT1 ("EXECUTING set_number_at.\n");

            EXTRACT_NUMBER_AND_INCR (mcnt, p);
            p1 = p + mcnt;
            EXTRACT_NUMBER_AND_INCR (mcnt, p);
#ifdef _LIBC
            DEBUG_PRINT3 ("  Setting %p to %d.\n", p1, mcnt);
#else
            DEBUG_PRINT3 ("  Setting 0x%x to %d.\n", p1, mcnt);
#endif
	    STORE_NUMBER (p1, mcnt);
            break;
          }

#if 0
	/* The DEC Alpha C compiler 3.x generates incorrect code for the
	   test  WORDCHAR_P (d - 1) != WORDCHAR_P (d)  in the expansion of
	   AT_WORD_BOUNDARY, so this code is disabled.  Expanding the
	   macro and introducing temporary variables works around the bug.  */

	case wordbound:
	  DEBUG_PRINT1 ("EXECUTING wordbound.\n");
	  if (AT_WORD_BOUNDARY (d))
	    break;
	  goto fail;

	case notwordbound:
	  DEBUG_PRINT1 ("EXECUTING notwordbound.\n");
	  if (AT_WORD_BOUNDARY (d))
	    goto fail;
	  break;
#else
	case wordbound:
	{
	  boolean prevchar, thischar;

	  DEBUG_PRINT1 ("EXECUTING wordbound.\n");
	  if (AT_STRINGS_BEG (d) || AT_STRINGS_END (d))
	    break;

	  prevchar = WORDCHAR_P (d - 1);
	  thischar = WORDCHAR_P (d);
	  if (prevchar != thischar)
	    break;
	  goto fail;
	}

      case notwordbound:
	{
	  boolean prevchar, thischar;

	  DEBUG_PRINT1 ("EXECUTING notwordbound.\n");
	  if (AT_STRINGS_BEG (d) || AT_STRINGS_END (d))
	    goto fail;

	  prevchar = WORDCHAR_P (d - 1);
	  thischar = WORDCHAR_P (d);
	  if (prevchar != thischar)
	    goto fail;
	  break;
	}
#endif

	case wordbeg:
          DEBUG_PRINT1 ("EXECUTING wordbeg.\n");
	  if (WORDCHAR_P (d) && (AT_STRINGS_BEG (d) || !WORDCHAR_P (d - 1)))
	    break;
          goto fail;

	case wordend:
          DEBUG_PRINT1 ("EXECUTING wordend.\n");
	  if (!AT_STRINGS_BEG (d) && WORDCHAR_P (d - 1)
              && (!WORDCHAR_P (d) || AT_STRINGS_END (d)))
	    break;
          goto fail;

#ifdef emacs
  	case before_dot:
          DEBUG_PRINT1 ("EXECUTING before_dot.\n");
 	  if (PTR_CHAR_POS ((unsigned char *) d) >= point)
  	    goto fail;
  	  break;

  	case at_dot:
          DEBUG_PRINT1 ("EXECUTING at_dot.\n");
 	  if (PTR_CHAR_POS ((unsigned char *) d) != point)
  	    goto fail;
  	  break;

  	case after_dot:
          DEBUG_PRINT1 ("EXECUTING after_dot.\n");
          if (PTR_CHAR_POS ((unsigned char *) d) <= point)
  	    goto fail;
  	  break;

	case syntaxspec:
          DEBUG_PRINT2 ("EXECUTING syntaxspec %d.\n", mcnt);
	  mcnt = *p++;
	  goto matchsyntax;

        case wordchar:
          DEBUG_PRINT1 ("EXECUTING Emacs wordchar.\n");
	  mcnt = (int) Sword;
        matchsyntax:
	  PREFETCH ();
	  /* Can't use *d++ here; SYNTAX may be an unsafe macro.  */
	  d++;
	  if (SYNTAX (d[-1]) != (enum syntaxcode) mcnt)
	    goto fail;
          SET_REGS_MATCHED ();
	  break;

	case notsyntaxspec:
          DEBUG_PRINT2 ("EXECUTING notsyntaxspec %d.\n", mcnt);
	  mcnt = *p++;
	  goto matchnotsyntax;

        case notwordchar:
          DEBUG_PRINT1 ("EXECUTING Emacs notwordchar.\n");
	  mcnt = (int) Sword;
        matchnotsyntax:
	  PREFETCH ();
	  /* Can't use *d++ here; SYNTAX may be an unsafe macro.  */
	  d++;
	  if (SYNTAX (d[-1]) == (enum syntaxcode) mcnt)
	    goto fail;
	  SET_REGS_MATCHED ();
          break;

#else /* not emacs */
	case wordchar:
          DEBUG_PRINT1 ("EXECUTING non-Emacs wordchar.\n");
	  PREFETCH ();
          if (!WORDCHAR_P (d))
            goto fail;
	  SET_REGS_MATCHED ();
          d++;
	  break;

	case notwordchar:
          DEBUG_PRINT1 ("EXECUTING non-Emacs notwordchar.\n");
	  PREFETCH ();
	  if (WORDCHAR_P (d))
            goto fail;
          SET_REGS_MATCHED ();
          d++;
	  break;
#endif /* not emacs */

        default:
          abort ();
	}
      continue;  /* Successfully executed one pattern command; keep going.  */


    /* We goto here if a matching operation fails. */
    fail:
      if (!FAIL_STACK_EMPTY ())
	{ /* A restart point is known.  Restore to that state.  */
          DEBUG_PRINT1 ("\nFAIL:\n");
          POP_FAILURE_POINT (d, p,
                             lowest_active_reg, highest_active_reg,
                             regstart, regend, reg_info);

          /* If this failure point is a dummy, try the next one.  */
          if (!p)
	    goto fail;

          /* If we failed to the end of the pattern, don't examine *p.  */
	  assert (p <= pend);
          if (p < pend)
            {
              boolean is_a_jump_n = false;

              /* If failed to a backwards jump that's part of a repetition
                 loop, need to pop this failure point and use the next one.  */
              switch ((re_opcode_t) *p)
                {
                case jump_n:
                  is_a_jump_n = true;
                case maybe_pop_jump:
                case pop_failure_jump:
                case jump:
                  p1 = p + 1;
                  EXTRACT_NUMBER_AND_INCR (mcnt, p1);
                  p1 += mcnt;

                  if ((is_a_jump_n && (re_opcode_t) *p1 == succeed_n)
                      || (!is_a_jump_n
                          && (re_opcode_t) *p1 == on_failure_jump))
                    goto fail;
                  break;
                default:
                  /* do nothing */ ;
                }
            }

          if (d >= string1 && d <= end1)
	    dend = end_match_1;
        }
      else
        break;   /* Matching at this starting point really fails.  */
    } /* for (;;) */

  if (best_regs_set)
    goto restore_best_regs;

  FREE_VARIABLES ();

  return -1;         			/* Failure to match.  */
} /* re_match_2 */

/* Subroutine definitions for re_match_2.  */


/* We are passed P pointing to a register number after a start_memory.

   Return true if the pattern up to the corresponding stop_memory can
   match the empty string, and false otherwise.

   If we find the matching stop_memory, sets P to point to one past its number.
   Otherwise, sets P to an undefined byte less than or equal to END.

   We don't handle duplicates properly (yet).  */

static boolean
group_match_null_string_p (p, end, reg_info)
    unsigned char **p, *end;
    register_info_type *reg_info;
{
  int mcnt;
  /* Point to after the args to the start_memory.  */
  unsigned char *p1 = *p + 2;

  while (p1 < end)
    {
      /* Skip over opcodes that can match nothing, and return true or
	 false, as appropriate, when we get to one that can't, or to the
         matching stop_memory.  */

      switch ((re_opcode_t) *p1)
        {
        /* Could be either a loop or a series of alternatives.  */
        case on_failure_jump:
          p1++;
          EXTRACT_NUMBER_AND_INCR (mcnt, p1);

          /* If the next operation is not a jump backwards in the
	     pattern.  */

	  if (mcnt >= 0)
	    {
              /* Go through the on_failure_jumps of the alternatives,
                 seeing if any of the alternatives cannot match nothing.
                 The last alternative starts with only a jump,
                 whereas the rest start with on_failure_jump and end
                 with a jump, e.g., here is the pattern for `a|b|c':

                 /on_failure_jump/0/6/exactn/1/a/jump_past_alt/0/6
                 /on_failure_jump/0/6/exactn/1/b/jump_past_alt/0/3
                 /exactn/1/c

                 So, we have to first go through the first (n-1)
                 alternatives and then deal with the last one separately.  */


              /* Deal with the first (n-1) alternatives, which start
                 with an on_failure_jump (see above) that jumps to right
                 past a jump_past_alt.  */

              while ((re_opcode_t) p1[mcnt-3] == jump_past_alt)
                {
                  /* `mcnt' holds how many bytes long the alternative
                     is, including the ending `jump_past_alt' and
                     its number.  */

                  if (!alt_match_null_string_p (p1, p1 + mcnt - 3,
				                      reg_info))
                    return false;

                  /* Move to right after this alternative, including the
		     jump_past_alt.  */
                  p1 += mcnt;

                  /* Break if it's the beginning of an n-th alternative
                     that doesn't begin with an on_failure_jump.  */
                  if ((re_opcode_t) *p1 != on_failure_jump)
                    break;

		  /* Still have to check that it's not an n-th
		     alternative that starts with an on_failure_jump.  */
		  p1++;
                  EXTRACT_NUMBER_AND_INCR (mcnt, p1);
                  if ((re_opcode_t) p1[mcnt-3] != jump_past_alt)
                    {
		      /* Get to the beginning of the n-th alternative.  */
                      p1 -= 3;
                      break;
                    }
                }

              /* Deal with the last alternative: go back and get number
                 of the `jump_past_alt' just before it.  `mcnt' contains
                 the length of the alternative.  */
              EXTRACT_NUMBER (mcnt, p1 - 2);

              if (!alt_match_null_string_p (p1, p1 + mcnt, reg_info))
                return false;

              p1 += mcnt;	/* Get past the n-th alternative.  */
            } /* if mcnt > 0 */
          break;


        case stop_memory:
	  assert (p1[1] == **p);
          *p = p1 + 2;
          return true;


        default:
          if (!common_op_match_null_string_p (&p1, end, reg_info))
            return false;
        }
    } /* while p1 < end */

  return false;
} /* group_match_null_string_p */


/* Similar to group_match_null_string_p, but doesn't deal with alternatives:
   It expects P to be the first byte of a single alternative and END one
   byte past the last. The alternative can contain groups.  */

static boolean
alt_match_null_string_p (p, end, reg_info)
    unsigned char *p, *end;
    register_info_type *reg_info;
{
  int mcnt;
  unsigned char *p1 = p;

  while (p1 < end)
    {
      /* Skip over opcodes that can match nothing, and break when we get
         to one that can't.  */

      switch ((re_opcode_t) *p1)
        {
	/* It's a loop.  */
        case on_failure_jump:
          p1++;
          EXTRACT_NUMBER_AND_INCR (mcnt, p1);
          p1 += mcnt;
          break;

	default:
          if (!common_op_match_null_string_p (&p1, end, reg_info))
            return false;
        }
    }  /* while p1 < end */

  return true;
} /* alt_match_null_string_p */


/* Deals with the ops common to group_match_null_string_p and
   alt_match_null_string_p.

   Sets P to one after the op and its arguments, if any.  */

static boolean
common_op_match_null_string_p (p, end, reg_info)
    unsigned char **p, *end;
    register_info_type *reg_info;
{
  int mcnt;
  boolean ret;
  int reg_no;
  unsigned char *p1 = *p;

  switch ((re_opcode_t) *p1++)
    {
    case no_op:
    case begline:
    case endline:
    case begbuf:
    case endbuf:
    case wordbeg:
    case wordend:
    case wordbound:
    case notwordbound:
#ifdef emacs
    case before_dot:
    case at_dot:
    case after_dot:
#endif
      break;

    case start_memory:
      reg_no = *p1;
      assert (reg_no > 0 && reg_no <= MAX_REGNUM);
      ret = group_match_null_string_p (&p1, end, reg_info);

      /* Have to set this here in case we're checking a group which
         contains a group and a back reference to it.  */

      if (REG_MATCH_NULL_STRING_P (reg_info[reg_no]) == MATCH_NULL_UNSET_VALUE)
        REG_MATCH_NULL_STRING_P (reg_info[reg_no]) = ret;

      if (!ret)
        return false;
      break;

    /* If this is an optimized succeed_n for zero times, make the jump.  */
    case jump:
      EXTRACT_NUMBER_AND_INCR (mcnt, p1);
      if (mcnt >= 0)
        p1 += mcnt;
      else
        return false;
      break;

    case succeed_n:
      /* Get to the number of times to succeed.  */
      p1 += 2;
      EXTRACT_NUMBER_AND_INCR (mcnt, p1);

      if (mcnt == 0)
        {
          p1 -= 4;
          EXTRACT_NUMBER_AND_INCR (mcnt, p1);
          p1 += mcnt;
        }
      else
        return false;
      break;

    case duplicate:
      if (!REG_MATCH_NULL_STRING_P (reg_info[*p1]))
        return false;
      break;

    case set_number_at:
      p1 += 4;

    default:
      /* All other opcodes mean we cannot match the empty string.  */
      return false;
  }

  *p = p1;
  return true;
} /* common_op_match_null_string_p */


/* Return zero if TRANSLATE[S1] and TRANSLATE[S2] are identical for LEN
   bytes; nonzero otherwise.  */

static int
bcmp_translate (s1, s2, len, translate)
     const char *s1, *s2;
     register int len;
     RE_TRANSLATE_TYPE translate;
{
  register const unsigned char *p1 = (const unsigned char *) s1;
  register const unsigned char *p2 = (const unsigned char *) s2;
  while (len)
    {
      if (translate[*p1++] != translate[*p2++]) return 1;
      len--;
    }
  return 0;
}

/* Entry points for GNU code.  */

/* re_compile_pattern is the GNU regular expression compiler: it
   compiles PATTERN (of length SIZE) and puts the result in BUFP.
   Returns 0 if the pattern was valid, otherwise an error string.

   Assumes the `allocated' (and perhaps `buffer') and `translate' fields
   are set in BUFP on entry.

   We call regex_compile to do the actual compilation.  */

const char *
re_compile_pattern (pattern, length, bufp)
     const char *pattern;
     size_t length;
     struct re_pattern_buffer *bufp;
{
  reg_errcode_t ret;

  /* GNU code is written to assume at least RE_NREGS registers will be set
     (and at least one extra will be -1).  */
  bufp->regs_allocated = REGS_UNALLOCATED;

  /* And GNU code determines whether or not to get register information
     by passing null for the REGS argument to re_match, etc., not by
     setting no_sub.  */
  bufp->no_sub = 0;

  /* Match anchors at newline.  */
  bufp->newline_anchor = 1;

  ret = regex_compile (pattern, length, re_syntax_options, bufp);

  if (!ret)
    return NULL;
  return gettext (re_error_msgid[(int) ret]);
}
#ifdef _LIBC
weak_alias (__re_compile_pattern, re_compile_pattern)
#endif

/* Entry points compatible with 4.2 BSD regex library.  We don't define
   them unless specifically requested.  */

#if defined _REGEX_RE_COMP || defined _LIBC

/* BSD has one and only one pattern buffer.  */
static struct re_pattern_buffer re_comp_buf;

char *
#ifdef _LIBC
/* Make these definitions weak in libc, so POSIX programs can redefine
   these names if they don't use our functions, and still use
   regcomp/regexec below without link errors.  */
weak_function
#endif
re_comp (s)
    const char *s;
{
  reg_errcode_t ret;

  if (!s)
    {
      if (!re_comp_buf.buffer)
	return gettext ("No previous regular expression");
      return 0;
    }

  if (!re_comp_buf.buffer)
    {
      re_comp_buf.buffer = (unsigned char *) malloc (200);
      if (re_comp_buf.buffer == NULL)
        return gettext (re_error_msgid[(int) REG_ESPACE]);
      re_comp_buf.allocated = 200;

      re_comp_buf.fastmap = (char *) malloc (1 << BYTEWIDTH);
      if (re_comp_buf.fastmap == NULL)
	return gettext (re_error_msgid[(int) REG_ESPACE]);
    }

  /* Since `re_exec' always passes NULL for the `regs' argument, we
     don't need to initialize the pattern buffer fields which affect it.  */

  /* Match anchors at newlines.  */
  re_comp_buf.newline_anchor = 1;

  ret = regex_compile (s, strlen (s), re_syntax_options, &re_comp_buf);

  if (!ret)
    return NULL;

  /* Yes, we're discarding `const' here if !HAVE_LIBINTL.  */
  return (char *) gettext (re_error_msgid[(int) ret]);
}


int
#ifdef _LIBC
weak_function
#endif
re_exec (s)
    const char *s;
{
  const int len = strlen (s);
  return
    0 <= re_search (&re_comp_buf, s, len, 0, len, (struct re_registers *) 0);
}

#endif /* _REGEX_RE_COMP */

/* POSIX.2 functions.  Don't define these for Emacs.  */

#ifndef emacs

/* regcomp takes a regular expression as a string and compiles it.

   PREG is a regex_t *.  We do not expect any fields to be initialized,
   since POSIX says we shouldn't.  Thus, we set

     `buffer' to the compiled pattern;
     `used' to the length of the compiled pattern;
     `syntax' to RE_SYNTAX_POSIX_EXTENDED if the
       REG_EXTENDED bit in CFLAGS is set; otherwise, to
       RE_SYNTAX_POSIX_BASIC;
     `newline_anchor' to REG_NEWLINE being set in CFLAGS;
     `fastmap' and `fastmap_accurate' to zero;
     `re_nsub' to the number of subexpressions in PATTERN.

   PATTERN is the address of the pattern string.

   CFLAGS is a series of bits which affect compilation.

     If REG_EXTENDED is set, we use POSIX extended syntax; otherwise, we
     use POSIX basic syntax.

     If REG_NEWLINE is set, then . and [^...] don't match newline.
     Also, regexec will try a match beginning after every newline.

     If REG_ICASE is set, then we considers upper- and lowercase
     versions of letters to be equivalent when matching.

     If REG_NOSUB is set, then when PREG is passed to regexec, that
     routine will report only success or failure, and nothing about the
     registers.

   It returns 0 if it succeeds, nonzero if it doesn't.  (See regex-gnu.h for
   the return codes and their meanings.)  */

int
regcomp (preg, pattern, cflags)
    regex_t *preg;
    const char *pattern;
    int cflags;
{
  reg_errcode_t ret;
  reg_syntax_t syntax
    = (cflags & REG_EXTENDED) ?
      RE_SYNTAX_POSIX_EXTENDED : RE_SYNTAX_POSIX_BASIC;

  /* regex_compile will allocate the space for the compiled pattern.  */
  preg->buffer = 0;
  preg->allocated = 0;
  preg->used = 0;

  /* Don't bother to use a fastmap when searching.  This simplifies the
     REG_NEWLINE case: if we used a fastmap, we'd have to put all the
     characters after newlines into the fastmap.  This way, we just try
     every character.  */
  preg->fastmap = 0;

  if (cflags & REG_ICASE)
    {
      unsigned i;

      preg->translate
	= (RE_TRANSLATE_TYPE) malloc (CHAR_SET_SIZE
				      * sizeof (*(RE_TRANSLATE_TYPE)0));
      if (preg->translate == NULL)
        return (int) REG_ESPACE;

      /* Map uppercase characters to corresponding lowercase ones.  */
      for (i = 0; i < CHAR_SET_SIZE; i++)
        preg->translate[i] = ISUPPER (i) ? tolower (i) : i;
    }
  else
    preg->translate = NULL;

  /* If REG_NEWLINE is set, newlines are treated differently.  */
  if (cflags & REG_NEWLINE)
    { /* REG_NEWLINE implies neither . nor [^...] match newline.  */
      syntax &= ~RE_DOT_NEWLINE;
      syntax |= RE_HAT_LISTS_NOT_NEWLINE;
      /* It also changes the matching behavior.  */
      preg->newline_anchor = 1;
    }
  else
    preg->newline_anchor = 0;

  preg->no_sub = !!(cflags & REG_NOSUB);

  /* POSIX says a null character in the pattern terminates it, so we
     can use strlen here in compiling the pattern.  */
  ret = regex_compile (pattern, strlen (pattern), syntax, preg);

  /* POSIX doesn't distinguish between an unmatched open-group and an
     unmatched close-group: both are REG_EPAREN.  */
  if (ret == REG_ERPAREN) ret = REG_EPAREN;

  return (int) ret;
}
#ifdef _LIBC
weak_alias (__regcomp, regcomp)
#endif


/* regnexec searches for a given pattern, specified by PREG, in the
   memory buffer STRING of lengthe LEN.

   If NMATCH is zero or REG_NOSUB was set in the cflags argument to
   `regcomp', we ignore PMATCH.  Otherwise, we assume PMATCH has at
   least NMATCH elements, and we set them to the offsets of the
   corresponding matched substrings.

   EFLAGS specifies `execution flags' which affect matching: if
   REG_NOTBOL is set, then ^ does not match at the beginning of the
   string; if REG_NOTEOL is set, then $ does not match at the end.

   We return 0 if we find a match and REG_NOMATCH if not.  */

int
regnexec (preg, string, len, nmatch, pmatch, eflags)
    const regex_t *preg;
    const char *string;
    size_t len;
    size_t nmatch;
    regmatch_t pmatch[];
    int eflags;
{
  int ret;
  struct re_registers regs;
  regex_t private_preg;
  boolean want_reg_info = !preg->no_sub && nmatch > 0;

  private_preg = *preg;

  private_preg.not_bol = !!(eflags & REG_NOTBOL);
  private_preg.not_eol = !!(eflags & REG_NOTEOL);

  /* The user has told us exactly how many registers to return
     information about, via `nmatch'.  We have to pass that on to the
     matching routines.  */
  private_preg.regs_allocated = REGS_FIXED;

  if (want_reg_info)
    {
      regs.num_regs = nmatch;
      regs.start = TALLOC (nmatch, regoff_t);
      regs.end = TALLOC (nmatch, regoff_t);
      if (regs.start == NULL || regs.end == NULL)
        return (int) REG_NOMATCH;
    }

  /* Perform the searching operation.  */
  ret = re_search (&private_preg, string, len,
                   /* start: */ 0, /* range: */ len,
                   want_reg_info ? &regs : (struct re_registers *) 0);

  /* Copy the register information to the POSIX structure.  */
  if (want_reg_info)
    {
      if (ret >= 0)
        {
          unsigned r;

          for (r = 0; r < nmatch; r++)
            {
              pmatch[r].rm_so = regs.start[r];
              pmatch[r].rm_eo = regs.end[r];
            }
        }

      /* If we needed the temporary register info, free the space now.  */
      free (regs.start);
      free (regs.end);
    }

  /* We want zero return to mean success, unlike `re_search'.  */
  return ret >= 0 ? (int) REG_NOERROR : (int) REG_NOMATCH;
}
#ifdef _LIBC
weak_alias (__regnexec, regnexec)
#endif

/* regexec searches for a given pattern, specified by PREG, in the
   string STRING.

   If NMATCH is zero or REG_NOSUB was set in the cflags argument to
   `regcomp', we ignore PMATCH.  Otherwise, we assume PMATCH has at
   least NMATCH elements, and we set them to the offsets of the
   corresponding matched substrings.

   EFLAGS specifies `execution flags' which affect matching: if
   REG_NOTBOL is set, then ^ does not match at the beginning of the
   string; if REG_NOTEOL is set, then $ does not match at the end.

   We return 0 if we find a match and REG_NOMATCH if not.  */

int
regexec (preg, string, nmatch, pmatch, eflags)
    const regex_t *preg;
    const char *string;
    size_t nmatch;
    regmatch_t pmatch[];
    int eflags;
{
  return regnexec(preg, string, strlen(string), nmatch, pmatch, eflags);
}
#ifdef _LIBC
weak_alias (__regexec, regexec)
#endif


/* Returns a message corresponding to an error code, ERRCODE, returned
   from either regcomp or regexec.   We don't use PREG here.  */

size_t
regerror (errcode, preg, errbuf, errbuf_size)
    int errcode;
    const regex_t *preg;
    char *errbuf;
    size_t errbuf_size;
{
  const char *msg;
  size_t msg_size;

  if (errcode < 0
      || errcode >= (int) (sizeof (re_error_msgid)
			   / sizeof (re_error_msgid[0])))
    /* Only error codes returned by the rest of the code should be passed
       to this routine.  If we are given anything else, or if other regex
       code generates an invalid error code, then the program has a bug.
       Dump core so we can fix it.  */
    abort ();

  msg = gettext (re_error_msgid[errcode]);

  msg_size = strlen (msg) + 1; /* Includes the null.  */

  if (errbuf_size != 0)
    {
      if (msg_size > errbuf_size)
        {
#if defined HAVE_MEMPCPY || defined _LIBC
	  *((char *) __mempcpy (errbuf, msg, errbuf_size - 1)) = '\0';
#else
          memcpy (errbuf, msg, errbuf_size - 1);
          errbuf[errbuf_size - 1] = 0;
#endif
        }
      else
        memcpy (errbuf, msg, msg_size);
    }

  return msg_size;
}
#ifdef _LIBC
weak_alias (__regerror, regerror)
#endif


/* Free dynamically allocated space used by PREG.  */

void
regfree (preg)
    regex_t *preg;
{
  if (preg->buffer != NULL)
    free (preg->buffer);
  preg->buffer = NULL;

  preg->allocated = 0;
  preg->used = 0;

  if (preg->fastmap != NULL)
    free (preg->fastmap);
  preg->fastmap = NULL;
  preg->fastmap_accurate = 0;

  if (preg->translate != NULL)
    free (preg->translate);
  preg->translate = NULL;
}
#ifdef _LIBC
weak_alias (__regfree, regfree)
#endif

#endif /* not emacs  */
