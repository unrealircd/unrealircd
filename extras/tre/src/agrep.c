/*
  agrep.c - Approximate grep

  Copyright (C) 2002-2003 Ville Laurikari <vl@iki.fi>.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License version 2 (June
  1991) as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* HAVE_GETOPT_H */
#include <locale.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>
#include "regex.h"
#include "gettext.h"
#define _(String) gettext(String)

/* Short options. */
static char const short_options[] =
"cd:e:hilnsvwBD:E:HI:S:V0123456789";

static int show_help;
char *program_name;

#ifdef HAVE_GETOPT_LONG
/* Long option equivalences. */
static struct option const long_options[] =
{
  {"count", no_argument, NULL, 'c'},
  {"delimiter", no_argument, NULL, 'd'},
  {"regexp", required_argument, NULL, 'e'},
  {"no-filename", no_argument, NULL, 'h'},
  {"ignore-case", no_argument, NULL, 'i'},
  {"files-with-matches", no_argument, NULL, 'l'},
  {"line-number", no_argument, NULL, 'n'},
  {"show-cost", no_argument, NULL, 's'},
  {"invert-match", no_argument, NULL, 'v'},
  {"word-regexp", no_argument, NULL, 'w'},
  {"best-match", no_argument, NULL, 'B'},
  {"delete-cost", required_argument, NULL, 'D'},
  {"max-errors", required_argument, NULL, 'E'},
  {"insert-cost", required_argument, NULL, 'I'},
  {"substitute-cost", required_argument, NULL, 'S'},
  {"version", no_argument, NULL, 'V'},
  {"help", no_argument, &show_help, 'H'},
  {0, 0, 0, 0}
};
#endif /* HAVE_GETOPT_LONG */

static void
usage (int status)
{
  if (status != 0)
    {
      fprintf(stderr, _("Usage: %s [OPTION]... PATTERN [FILE]...\n"),
	      program_name);
#ifdef HAVE_GETOPT_LONG
      fprintf(stderr, _("Try `%s --help' for more information.\n"),
	      program_name);
#else /* !HAVE_GETOPT_LONG */
      fprintf(stderr, _("Try `%s -H' for more information.\n"),
	      program_name);
#endif /* !HAVE_GETOPT_LONG */
    }
  else
    {
      printf(_("Usage: %s [OPTION]... PATTERN [FILE]...\n"), program_name);
      printf(_("\
Searches for approximate matches of PATTERN in each FILE or standard input.\n\
Example: `%s -2 optimize foo.txt' outputs all lines in file `foo.txt' that\n\
match \"optimize\" within two errors.  E.g. lines which contain \"optimise\",\n\
\"optmise\", and \"opitmize\" all match.\n"), program_name);
      printf("\n");
      printf(_("\
Regexp selection and interpretation:\n\
  -e, --regexp=PATTERN      use PATTERN as a regular expression\n\
  -i, --ignore-case         ignore case distinctions\n\
  -w, --word-regexp         force PATTERN to match only whole words\n\
\n\
Approximate matching settings:\n\
  -D, --delete-cost=NUM     set cost of missing characters\n\
  -I, --insert-cost=NUM     set cost of extra characters\n\
  -S, --substitute-cost=NUM set cost of wrong characters\n\
  -E, --max-errors=NUM      select records that have at most NUM errors\n\
  -#                        select records that have at most # errors (# is a\n\
                            digit between 0 and 9)\n\
\n\
Miscellaneous:\n\
  -d, --delimiter=PATTERN   set the record delimiter regular expression\n\
  -v, --invert-match        select non-matching records\n\
  -V, --version             print version information and exit\n\
  -H, --help                display this help and exit\n\
\n\
Output control:\n\
  -B, --best-match          only output records with least errors\n\
  -c, --count               only print a count of matching records per FILE\n\
  -h, --no-filename         suppress the prefixing filename on output\n\
  -l, --files-with-matches  only print FILE names containing matches\n\
  -n, --record-number       print record number with output\n\
  -s, --show-cost           print match cost with output\n"));
      printf("\n");
      printf(_("\
With no FILE, or when FILE is -, reads standard input.  If less than two\n\
FILEs are given, -h is assumed.  Exit status is 0 if a match is found, 1 for\n\
no match, and 2 if there were errors.  If -E or -# is not specified, only\n\
exact matches are selected.\n"));
      printf("\n");
      printf(_("\
PATTERN is a POSIX extended regular expression (ERE) with the TRE extensions.\n\
See tre(7) for a complete description.\n"));
      printf("\n");
      printf(_("Report bugs to Ville Laurikari <vl@iki.fi>.\n"));
    }
  exit(status);
}

static regex_t preg;      /* Compiled pattern to search for. */
static regex_t delim;     /* Compiled record delimiter pattern. */

#define INITIAL_BUF_SIZE 10240  /* Initial size of the buffer. */
static char *buf;          /* Buffer for scanning text. */
static int buf_size;       /* Current size of the buffer. */
static int data_len;       /* Amount of data in the buffer. */
static char *record;       /* Start of current record. */
static char *next_record;  /* Start of next record. */
static int record_len;     /* Length of current record. */
static int at_eof;

static int invert_match;   /* Show only non-matching records. */
static int print_filename; /* Output filename. */
static int print_recnum;   /* Output record number. */
static int print_cost;     /* Output match cost. */
static int count_matches;  /* Count matching records. */
static int list_files;     /* List matching files. */

static int best_match;       /* Output only best matches. */
static int best_cost;        /* Best match cost found so far. */
static int best_match_limit; /* Cost limit for best match. */

static regaparams_t match_params;


/* Sets `record' to the next complete record from file `fd', and `record_len'
   to the length of the record.  Returns 1 when there are no more records,
   0 otherwise. */
static inline int
tre_agrep_get_next_record(int fd)
{
  int errcode;
  regmatch_t pmatch[1];

  if (at_eof)
    return 1;

  while (1)
    {
      if (next_record == NULL)
	{
	  int r;
	  /* Fill the buffer with data from the file. */
	  r = read(fd, buf + data_len, buf_size - data_len);
	  /* XXX - check for failure or interruption. */
	  if (r == 0)
	    {
	      /* End of file.  Return the last record. */
	      record = buf;
	      record_len = data_len;
	      at_eof = 1;
	      /* The empty string after a trailing delimiter is not considered
		 to be a record. */
	      if (record_len == 0)
		return 1;
	      else
		return 0;
	    }
	  data_len += r;
	  next_record = buf;
	}

      /* Find the next record delimiter. */
      errcode = regnexec(&delim, next_record, data_len - (next_record - buf),
			 1, pmatch, 0);
      if (errcode == REG_ESPACE)
	{
	  fprintf(stderr, "%s: %s\n", program_name, _("Out of memory"));
	  exit(2);
	}
      else if (errcode == REG_NOMATCH)
	{
	  /* No record delimiter found. */
	  if (next_record == buf)
	    {
	      /* The buffer is full but we don't yet have a full record.
		 Grow the buffer. */
	      buf = realloc(buf, buf_size * 2);
	      if (!buf)
		{
		  fprintf(stderr, "%s: %s\n", program_name,
			  _("Out of memory"));
		  exit(2);
		}
	      buf_size *= 2;
	      next_record = NULL;
	    }
	  else
	    {
	      /* Move the data to start of the buffer and read more data. */
	      memmove(buf, next_record, buf + data_len - next_record);
	      data_len = buf + data_len - next_record;
	      next_record = NULL;
	    }
	}
      else if (errcode == REG_OK)
	{
	  /* Record delimiter found, now we know how long the current
	     record is. */
	  record = next_record;
	  record_len = pmatch[0].rm_so;
	  next_record = next_record + pmatch[0].rm_eo;
	  return 0;
	}
      else assert(0);
    }
}


static int
tre_agrep_handle_file(const char *filename)
{
  int fd;
  int count = 0;
  int recnum = 0;

  /* Allocate the initial buffer. */
  if (buf == NULL)
    {
      buf = malloc(INITIAL_BUF_SIZE);
      if (buf == NULL)
	{
	  fprintf(stderr, "%s: %s\n", program_name, _("Out of memory"));
	  exit(2);
	}
      buf_size = INITIAL_BUF_SIZE;
    }

  if (!filename || strcmp(filename, "-") == 0)
    {
      if (best_match)
	{
	  fprintf(stderr, "%s: %s\n", program_name,
		  _("Cannot use -B when reading from standard input."));
	  return 2;
	}
      fd = 0;
      filename = _("(standard input)");
    }
  else
    {
      fd = open(filename, O_RDONLY);
    }

  if (fd < 0)
    {
      fprintf(stderr, "%s: %s: %s\n", program_name, filename, strerror(errno));
      return 1;
    }


  /* Go through all records and output the matching ones, or the non-matching
     ones if `invert_match' is true. */
  at_eof = 0;
  while (!tre_agrep_get_next_record(fd))
    {
      int errcode;
      regamatch_t match;
      recnum++;
      memset(&match, 0, sizeof(match));
      /* See if the record matches. */
      errcode = reganexec(&preg, record, record_len, &match, match_params, 0);
      if ((!invert_match && errcode == REG_OK)
	  || (invert_match && errcode != REG_OK))
	{
	  count++;
	  if (best_match)
	    {
	      if (match.cost < best_cost)
		best_cost = match.cost;
	      continue;
	    }
	  if (list_files)
	    {
	      printf("%s\n", filename);
	      break;
	    }
	  else if (!count_matches)
	    {
	      if (print_filename)
		printf("%s:", filename);
	      if (print_recnum)
		printf("%d:", recnum);
	      if (print_cost)
		printf("%d:", match.cost);
	      printf("%.*s\n", record_len, record);
	    }
	}
    }

  if (count_matches && !best_match)
    {
      if (print_filename)
	printf("%s:", filename);
      printf("%d\n", count);
    }

  return 0;
}



int
main(int argc, char **argv)
{
  int c, errcode;
  int comp_flags = REG_NOSUB | REG_EXTENDED;
  char *tmp_str;
  char *regexp = NULL;
  char *delim_regexp = "\n";
  int word_regexp = 0;

  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* Get the program name without the path (for error messages etc). */
  program_name = argv[0];
  if (program_name)
    {
      tmp_str = strrchr(program_name, '/');
      if (tmp_str)
	program_name = tmp_str + 1;
    }

  /* Defaults. */
  print_filename = 1;
  print_cost = 0;
  regaparams_default(&match_params);
  match_params.max_cost = 0;

  best_match_limit = INT_MAX;
  /* Parse command line options. */
  while (1)
    {
#ifdef HAVE_GETOPT_LONG
      c = getopt_long(argc, argv, short_options, long_options, NULL);
#else /* !HAVE_GETOPT_LONG */
      c = getopt(argc, argv, short_options);
#endif /* !HAVE_GETOPT_LONG */
      if (c == -1)
	break;

      switch (c)
	{
	case 'c':
	  /* Count number of matching records. */
	  count_matches = 1;
	  break;
	case 'd':
	  /* Set record delimiter regexp. */
	  delim_regexp = optarg;
	  break;
	case 'e':
	  /* Regexp to use. */
	  regexp = optarg;
	  break;
	case 'h':
	  /* Don't prefix filename on output if there are multiple files. */
	  print_filename = 0;
	  break;
	case 'i':
	  /* Ignore case. */
	  comp_flags |= REG_ICASE;
	  break;
	case 'l':
	  /* Only print files that contain matches. */
	  list_files = 1;
	  break;
	case 'n':
	  /* Print record number of matching record. */
	  print_recnum = 1;
	  break;
	case 's':
	  /* Print match cost of matching record. */
	  print_cost = 1;
	  break;
	case 'v':
	  /* Select non-matching records. */
	  invert_match = 1;
	  break;
	case 'w':
	  /* Match only whole words. */
	  word_regexp = 1;
	  break;
	case 'B':
	  /* Select only the records which have the best match. */
	  best_match = 1;
	  best_cost = INT_MAX;
	  match_params.max_cost = INT_MAX;
	  break;
	case 'D':
	  /* Set the cost of a deletion. */
	  match_params.cost_del = atoi(optarg);
	  break;
	case 'E':
	  /* Set the maximum number of errors allowed for a record to match. */
	  match_params.max_cost = atoi(optarg);
	  best_match_limit = match_params.max_cost;
	  break;
	case 'I':
	  /* Set the cost of an insertion. */
	  match_params.cost_ins = atoi(optarg);
	  break;
	case 'S':
	  /* Set the cost of a substitution. */
	  match_params.cost_subst = atoi(optarg);
	  break;
	case 'V':
	  /* Print version string and exit. */
	  printf("%s (" PACKAGE_NAME " agrep) " PACKAGE_VERSION "\n\n",
		 program_name);
	  printf(_("Copyright (C) 2002-2003 Ville Laurikari.\n"));
	  printf(_("\
This is free software; see the source for copying conditions. There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A \
PARTICULAR PURPOSE.\n"));
	  printf("\n");
	  exit(0);
	  break;

	case '?':
	  /* Ambiguous match or extraneous parameter. */
	  break;
	case 'H':
	  show_help = 1;
	  break;
	case 0:
	  /* Long options without corresponding short options. */
	  break;

	default:
	  if (c >= '0' && c <= '9')
	    match_params.max_cost = c - '0';
	  else
	    usage(2);
	  break;
	}
    }

  if (show_help)
    usage(0);

  /* Get the pattern. */
  if (regexp == NULL)
    {
      if (optind >= argc)
	usage(2);
      regexp = argv[optind++];
    }

  /* If -w is specified, prepend beginning-of-word and end-of-word
     assertions to the regexp before compiling. */
  if (word_regexp)
    {
      char *tmp = regexp;
      int len = strlen(tmp);
      regexp = malloc(len + 7);
      if (regexp == NULL)
	{
	  fprintf(stderr, "%s: %s\n", program_name, _("Out of memory"));
	  return 2;
	}
      strcpy(regexp, "\\<(");
      strcpy(regexp + 3, tmp);
      strcpy(regexp + len + 3, ")\\>");
    }

  /* Compile the pattern. */
  errcode = regcomp(&preg, regexp, comp_flags);
  if (errcode)
    {
      char errbuf[256];
      regerror(errcode, &preg, errbuf, sizeof(errbuf));
      fprintf(stderr, "%s: %s: %s\n",
	      program_name, _("Error in search pattern"), errbuf);
      return 2;
    }

  /* Compile the record delimiter pattern. */
  errcode = regcomp(&delim, delim_regexp, REG_EXTENDED | REG_NEWLINE);
  if (errcode)
    {
      char errbuf[256];
      regerror(errcode, &preg, errbuf, sizeof(errbuf));
      fprintf(stderr, "%s: %s: %s\n",
	      program_name, _("Error in record delimiter pattern"), errbuf);
      return 2;
    }

  /* The rest of the arguments are file(s) to match.  If there are no files
     specified, read from stdin. */
  if (argc - optind <= 1)
    print_filename = 0;
  if (optind >= argc)
    {
      /* Read from standard input. */
      tre_agrep_handle_file(NULL);
    }
  else if (best_match)
    {
      /* Best match mode: scan all files twice and print only the
	 records that had a best match. */
      int first_ind = optind;
      while (optind < argc)
	tre_agrep_handle_file(argv[optind++]);
      best_match = 0;
      /* If there were no matches, bail out now. */
      if (best_cost == INT_MAX)
	return 1;
      /* Otherwise, rescan the files with max_cost set to the cost
	 of the best match found previously. */
      match_params.max_cost = best_cost;
      optind = first_ind;
      while (optind < argc)
	tre_agrep_handle_file(argv[optind++]);
    }
  else
    {
      /* Normal mode. */
      while (optind < argc)
	tre_agrep_handle_file(argv[optind++]);
    }

  return 0;
}
