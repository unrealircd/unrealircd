/*
  tre-compile.c - TRE regex compiler

  Copyright (C) 2001-2003 Ville Laurikari <vl@iki.fi>

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

/*
  TODO:
   - Fix tre_ast_to_tnfa() to recurse using a stack instead of recursive
     function calls.
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#define _GNU_SOURCE

#include <assert.h>
#ifdef TRE_DEBUG
#include <errno.h>
#endif /* TRE_DEBUG */
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif /* HAVE_WCHAR_H */
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif /* HAVE_WCTYPE_H */
#ifndef TRE_WCHAR
#include <ctype.h>
#endif /* TRE_WCHAR */
#include <limits.h>

#include "tre-internal.h"
#include "tre-mem.h"
#include "regex.h"
#include "xmalloc.h"



/*
  Stack functions.
*/

typedef struct {
  int size;
  int max_size;
  int increment;
  int ptr;
  void **stack;
} tre_stack_t;

/* Creates a new stack object.  `size' is initial size in bytes, `max_size'
   is maximum size, and `increment' specifies how much more space will be
   allocated with realloc() if all space gets used up.  Returns the stack
   object or NULL if out of memory. */
static tre_stack_t *
tre_stack_new(int size, int max_size, int increment)
{
  tre_stack_t *s;

  s = xmalloc(sizeof(*s));
  if (s != NULL)
    {
      s->stack = xmalloc(sizeof(*s->stack) * size);
      if (s->stack == NULL)
	{
	  xfree(s);
	  return NULL;
	}
      s->size = size;
      s->max_size = max_size;
      s->increment = increment;
      s->ptr = 0;
    }
  return s;
}

/* Frees the stack object. */
static void
tre_stack_destroy(tre_stack_t *s)
{
  xfree(s->stack);
  xfree(s);
}

/* Returns the current number of objects in the stack. */
inline static int
tre_stack_num_objects(tre_stack_t *s)
{
  return s->ptr;
}

/* Pushes `value' on top of stack `s'.  Returns REG_ESPACE if out of memory
   (tries to realloc() more space before failing if maximum size not yet
   reached).  Returns REG_OK if successful. */
static reg_errcode_t
tre_stack_push(tre_stack_t *s, void *value)
{
  if (s->ptr < s->size)
    {
      s->stack[s->ptr] = value;
      s->ptr++;
    }
  else
    {
      if (s->size >= s->max_size)
	{
	  DPRINT(("tre_stack_push: stack full\n"));
	  return REG_ESPACE;
	}
      else
	{
	  void **new_buffer;
	  int new_size;
	  DPRINT(("tre_stack_push: trying to realloc more space\n"));
	  new_size = s->size + s->increment;
	  if (new_size > s->max_size)
	    new_size = s->max_size;
	  new_buffer = xrealloc(s->stack, sizeof(*new_buffer) * new_size);
	  if (new_buffer == NULL)
	    {
	      DPRINT(("tre_stack_push: realloc failed.\n"));
	      return REG_ESPACE;
	    }
	  DPRINT(("tre_stack_push: realloc succeeded.\n"));
	  assert(new_size > s->size);
	  s->size = new_size;
	  s->stack = new_buffer;
	  tre_stack_push(s, value);
	}
    }
  return REG_OK;
}

/* Just to save some typing. */
#define STACK_PUSH(s, value)                                                  \
  do									      \
    {									      \
      status = tre_stack_push(s, (void *)(value));			      \
    }                                                                         \
  while (0)

#define STACK_PUSHX(s, value)                                                 \
  {									      \
    status = tre_stack_push(s, (void *)(value));			      \
    if (status != REG_OK)                                                     \
      break;                                                                  \
  }

#define STACK_PUSHR(s, value)                                                 \
  {									      \
    reg_errcode_t status;						      \
    status = tre_stack_push(s, (void *)(value));			      \
    if (status != REG_OK)                                                     \
      return status;                                                          \
  }


/* Pops the topmost element off of stack `s' and returns it.  The stack must
   not be empty. */
inline static void *
tre_stack_pop(tre_stack_t *s)
{
  return s->stack[--s->ptr];
}



/*
  Regex AST stuff.
*/

typedef enum { LITERAL, CATENATION, ITERATION, UNION } tre_ast_type_t;

/* Special leaf types. */
#define EMPTY     -1   /* Empty leaf (denotes empty string). */
#define ASSERTION -2   /* Assertion leaf. */
#define TAG       -3   /* Tag leaf. */
#define BACKREF   -4   /* Back reference leaf. */
#define PARAMETER -5   /* Parameter. */

#define IS_SPECIAL(x)   ((x)->code_min < 0)
#define IS_EMPTY(x)     ((x)->code_min == EMPTY)
#define IS_ASSERTION(x) ((x)->code_min == ASSERTION)
#define IS_TAG(x)       ((x)->code_min == TAG)
#define IS_BACKREF(x)   ((x)->code_min == BACKREF)
#define IS_PARAMETER(x) ((x)->code_min == PARAMETER)

typedef struct {
  int position;
  int code_min;
  int code_max;
  int *tags;
  int assertions;
  tre_ctype_t class;
  tre_ctype_t *neg_classes;
  int backref;
  int *params;
} tre_pos_and_tags_t;

typedef struct {
  tre_ast_type_t type;
  void *obj;
  int nullable;
  int submatch_id;
  int num_submatches;
  int num_tags;
  tre_pos_and_tags_t *firstpos;
  tre_pos_and_tags_t *lastpos;
} tre_ast_node_t;

/* A "literal" node.  These are created for assertions, back references,
   tags, matching parameter settings, and all expressions that match one
   character. */
typedef struct {
  long code_min;
  long code_max;
  int position;
  union {
    tre_ctype_t class;
    unsigned int *params;
  } u;
  tre_ctype_t *neg_classes;
} tre_literal_t;

/* A "catenation" node.  These are created when two regexps are concatenated.
   If there are more than one subexpressions in sequence, the `left' part
   holds all but the last, and `right' part holds the last subexpression
   (catenation is left associative). */
typedef struct {
  tre_ast_node_t *left;
  tre_ast_node_t *right;
} tre_catenation_t;

/* An "iteration" node.  These are created for the "*", "+", "?", and "{m,n}"
   operators. */
typedef struct {
  /* Subexpression to match. */
  tre_ast_node_t *arg;
  /* Minimum number of consecutive matches. */
  int min;
  /* Maximum number of consecutive matches. */
  int max;
  /* If 0, match as many characters as possible, if 1 match as few as
     possible.  Note that this does not always mean the same thing as
     matching as many/few repetitions as possible. */
  unsigned int minimal:1;
  /* Approximate matching parameters (or NULL). */
  unsigned int *params;
} tre_iteration_t;

/* An "union" node.  These are created for the "|" operator. */
typedef struct {
  tre_ast_node_t *left;
  tre_ast_node_t *right;
} tre_union_t;

static tre_ast_node_t *
tre_ast_new_node(tre_mem_t mem, tre_ast_type_t type, size_t size)
{
  tre_ast_node_t *node;

  node = tre_mem_calloc(mem, sizeof(*node));
  if (node == NULL)
    return NULL;
  node->obj = tre_mem_calloc(mem, size);
  if (node->obj == NULL)
    return NULL;
  node->type = type;
  node->nullable = -1;
  node->submatch_id = -1;

  return node;
}

static tre_ast_node_t *
tre_ast_new_literal(tre_mem_t mem, int code_min, int code_max, int position)
{
  tre_ast_node_t *node;
  tre_literal_t *lit;

  node = tre_ast_new_node(mem, LITERAL, sizeof(tre_literal_t));
  if (!node)
    return NULL;
  lit = node->obj;
  lit->code_min = code_min;
  lit->code_max = code_max;
  lit->position = position;

  return node;
}

static tre_ast_node_t *
tre_ast_new_iter(tre_mem_t mem, tre_ast_node_t *arg, int min, int max,
		 int minimal)
{
  tre_ast_node_t *node;
  tre_iteration_t *iter;

  node = tre_ast_new_node(mem, ITERATION, sizeof(tre_iteration_t));
  if (!node)
    return NULL;
  iter = node->obj;
  iter->arg = arg;
  iter->min = min;
  iter->max = max;
  iter->minimal = minimal;
  node->num_submatches = arg->num_submatches;

  return node;
}

static tre_ast_node_t *
tre_ast_new_union(tre_mem_t mem, tre_ast_node_t *left, tre_ast_node_t *right)
{
  tre_ast_node_t *node;

  node = tre_ast_new_node(mem, UNION, sizeof(tre_union_t));
  if (node == NULL)
    return NULL;
  ((tre_union_t *)node->obj)->left = left;
  ((tre_union_t *)node->obj)->right = right;
  node->num_submatches = left->num_submatches + right->num_submatches;

  return node;
}

static tre_ast_node_t *
tre_ast_new_catenation(tre_mem_t mem, tre_ast_node_t *left,
		       tre_ast_node_t *right)
{
  tre_ast_node_t *node;

  node = tre_ast_new_node(mem, CATENATION, sizeof(tre_catenation_t));
  if (node == NULL)
    return NULL;
  ((tre_catenation_t *)node->obj)->left = left;
  ((tre_catenation_t *)node->obj)->right = right;
  node->num_submatches = left->num_submatches + right->num_submatches;

  return node;
}

#ifdef TRE_DEBUG

static void
tre_findent(FILE *stream, int i)
{
  while (i-- > 0)
    fputc(' ', stream);
}

static void
tre_print_params(int *params)
{
  int i;
  if (params)
    {
      DPRINT(("params ["));
      for (i = 0; i < TRE_PARAM_LAST; i++)
	{
	  if (params[i] == TRE_PARAM_UNSET)
	    DPRINT(("unset"));
	  else if (params[i] == TRE_PARAM_DEFAULT)
	    DPRINT(("default"));
	  else
	    DPRINT(("%d", params[i]));
	  if (i < TRE_PARAM_LAST - 1)
	    DPRINT((", "));
	}
      DPRINT(("]"));
    }
}

static void
tre_do_print(FILE *stream, tre_ast_node_t *ast, int indent)
{
  int code_min, code_max, pos;
  int num_tags = ast->num_tags;
  tre_literal_t *lit;
  tre_iteration_t *iter;

  tre_findent(stream, indent);
  switch (ast->type)
    {
    case LITERAL:
      lit = ast->obj;
      code_min = lit->code_min;
      code_max = lit->code_max;
      pos = lit->position;
      if (IS_EMPTY(lit))
	{
	  fprintf(stream, "literal empty\n");
	}
      else if (IS_ASSERTION(lit))
	{
	  int i;
	  char *assertions[] = { "bol", "eol", "ctype", "!ctype",
				 "bow", "eow", "wb", "!wb" };
	  if (code_max >= ASSERT_LAST << 1)
	    assert(0);
	  fprintf(stream, "assertions: ");
	  for (i = 0; (1 << i) <= ASSERT_LAST; i++)
	    if (code_max & (1 << i))
	      fprintf(stream, "%s ", assertions[i]);
	  fprintf(stream, "\n");
	}
      else if (IS_TAG(lit))
	{
	  fprintf(stream, "tag %d\n", code_max);
	}
      else if (IS_BACKREF(lit))
	{
	  fprintf(stream, "backref %d, pos %d\n", code_max, pos);
	}
      else if (IS_PARAMETER(lit))
	{
	  tre_print_params(lit->u.params);
	  fprintf(stream, "\n");
	}
      else
	{
	  fprintf(stream, "literal (%c, %c) (%d, %d), pos %d, sub %d, "
		  "%d tags\n", code_min, code_max, code_min, code_max, pos,
		  ast->submatch_id, num_tags);
	}
      break;
    case ITERATION:
      iter = ast->obj;
      fprintf(stream, "iteration {%d, %d}, sub %d, %d tags, %s\n",
	      iter->min, iter->max, ast->submatch_id, num_tags,
	      iter->minimal ? "minimal" : "greedy");
      tre_do_print(stream, iter->arg, indent + 2);
      break;
    case UNION:
      fprintf(stream, "union, sub %d, %d tags\n", ast->submatch_id, num_tags);
      tre_do_print(stream, ((tre_union_t *)ast->obj)->left, indent + 2);
      tre_do_print(stream, ((tre_union_t *)ast->obj)->right, indent + 2);
      break;
    case CATENATION:
      fprintf(stream, "catenation, sub %d, %d tags\n", ast->submatch_id,
	      num_tags);
      tre_do_print(stream, ((tre_catenation_t *)ast->obj)->left, indent + 2);
      tre_do_print(stream, ((tre_catenation_t *)ast->obj)->right, indent + 2);
      break;
    default:
      assert(0);
      break;
    }
}

static void
tre_ast_fprint(FILE *stream, tre_ast_node_t *ast)
{
  tre_do_print(stream, ast, 0);
}

static void
tre_ast_print(tre_ast_node_t *tree)
{
  printf("AST:\n");
  tre_ast_fprint(stdout, tree);
}

#endif /* TRE_DEBUG */


/*
  Algorithms to setup tags so that submatch addressing can be done.
*/


/* Inserts a catenation node to the root of the tree given in `node'.
   As the left child a new tag with number `tag_id' to `node' is added,
   and the right child is the old root. */
static reg_errcode_t
tre_add_tag_left(tre_mem_t mem, tre_ast_node_t *node, int tag_id)
{
  tre_catenation_t *c;

  DPRINT(("add_tag_left: tag %d\n", tag_id));

  c = tre_mem_alloc(mem, sizeof(*c));
  if (c == NULL)
    return REG_ESPACE;
  c->left = tre_ast_new_literal(mem, TAG, tag_id, -1);
  if (c->left == NULL)
    return REG_ESPACE;
  c->right = tre_mem_alloc(mem, sizeof(tre_ast_node_t));
  if (c->right == NULL)
    return REG_ESPACE;

  c->right->obj = node->obj;
  c->right->type = node->type;
  c->right->nullable = -1;
  c->right->submatch_id = -1;
  c->right->firstpos = NULL;
  c->right->lastpos = NULL;
  c->right->num_tags = 0;
  node->obj = c;
  node->type = CATENATION;
  return REG_OK;
}

/* Inserts a catenation node to the root of the tree given in `node'.
   As the right child a new tag with number `tag_id' to `node' is added,
   and the left child is the old root. */
static reg_errcode_t
tre_add_tag_right(tre_mem_t mem, tre_ast_node_t *node, int tag_id)
{
  tre_catenation_t *c;

  DPRINT(("tre_add_tag_right: tag %d\n", tag_id));

  c = tre_mem_alloc(mem, sizeof(*c));
  if (c == NULL)
    return REG_ESPACE;
  c->right = tre_ast_new_literal(mem, TAG, tag_id, -1);
  if (c->right == NULL)
    return REG_ESPACE;
  c->left = tre_mem_alloc(mem, sizeof(tre_ast_node_t));
  if (c->left == NULL)
    return REG_ESPACE;

  c->left->obj = node->obj;
  c->left->type = node->type;
  c->left->nullable = -1;
  c->left->submatch_id = -1;
  c->left->firstpos = NULL;
  c->left->lastpos = NULL;
  c->left->num_tags = 0;
  node->obj = c;
  node->type = CATENATION;
  return REG_OK;
}

typedef enum {
  ADDTAGS_RECURSE,
  ADDTAGS_AFTER_ITERATION,
  ADDTAGS_AFTER_UNION_LEFT,
  ADDTAGS_AFTER_UNION_RIGHT,
  ADDTAGS_AFTER_CAT_LEFT,
  ADDTAGS_AFTER_CAT_RIGHT,
  ADDTAGS_SET_SUBMATCH_END
} tre_addtags_symbol_t;


typedef struct {
  int tag;
  int next_tag;
} tre_tag_states_t;

/* Adds tags to appropriate locations in the parse tree in `tree', so that
   subexpressions marked for submatch addressing can be traced. */
static reg_errcode_t
tre_add_tags(tre_mem_t mem, tre_stack_t *stack, tre_ast_node_t *tree,
	     tre_tnfa_t *tnfa)
{
  reg_errcode_t status = REG_OK;
  tre_addtags_symbol_t symbol;
  tre_ast_node_t *node = tree; /* Tree node we are currently looking at. */
  int bottom = tre_stack_num_objects(stack);
  /* True for first pass (counting number of needed tags) */
  int first_pass = (mem == NULL || tnfa == NULL);
  int *regset, *orig_regset;
  int num_tags = 0; /* Total number of tags. */
  int num_minimals = 0;  /* Number of special minimal tags. */
  int tag = 0;      /* The tag that is to be added next. */
  int next_tag = 1; /* Next tag to use after this one. */
  int *parents;     /* Stack of submatches the current submatch is
		       contained in. */
  int minimal_tag = -1; /* Tag that marks the beginning of a minimal match. */
  tre_tag_states_t *saved_states;

  tre_tag_direction_t direction = TRE_TAG_MINIMIZE;
  if (!first_pass)
    {
      tnfa->end_tag = 0;
      tnfa->minimal_tags[0] = -1;
    }

  regset = xmalloc(sizeof(*regset) * ((tnfa->num_submatches + 1) * 2));
  if (regset == NULL)
    return REG_ESPACE;
  regset[0] = -1;
  orig_regset = regset;

  parents = xmalloc(sizeof(*parents) * (tnfa->num_submatches + 1));
  if (parents == NULL)
    {
      xfree(regset);
      return REG_ESPACE;
    }
  parents[0] = -1;

  saved_states = xmalloc(sizeof(*saved_states) * (tnfa->num_submatches + 1));
  if (saved_states == NULL)
    {
      xfree(regset);
      xfree(parents);
      return REG_ESPACE;
    }
  else
    {
      unsigned int i;
      for (i = 0; i <= tnfa->num_submatches; i++)
	saved_states[i].tag = -1;
    }

  STACK_PUSH(stack, node);
  STACK_PUSH(stack, ADDTAGS_RECURSE);

  while (tre_stack_num_objects(stack) > bottom)
    {
      if (status != REG_OK)
	break;

      symbol = (tre_addtags_symbol_t)tre_stack_pop(stack);
      switch (symbol)
	{

	case ADDTAGS_SET_SUBMATCH_END:
	  {
	    int id = (int)tre_stack_pop(stack);
	    int i;

	    /* Add end of this submatch to regset. */
	    for (i = 0; regset[i] >= 0; i++);
	    regset[i] = id * 2 + 1;
	    regset[i + 1] = -1;

	    /* Pop this submatch from the parents stack. */
	    for (i = 0; parents[i] >= 0; i++);
	    parents[i - 1] = -1;
	    break;
	  }

	case ADDTAGS_RECURSE:
	  node = tre_stack_pop(stack);

	  if (node->submatch_id >= 0)
	    {
	      int id = node->submatch_id;
	      int i;


	      /* Add start of this submatch to regset. */
	      for (i = 0; regset[i] >= 0; i++);
	      regset[i] = id * 2;
	      regset[i + 1] = -1;

	      if (!first_pass)
		{
		  for (i = 0; parents[i] >= 0; i++);
		  tnfa->submatch_data[id].parents = NULL;
		  if (i > 0)
		    {
		      int *p = xmalloc(sizeof(*p) * (i + 1));
		      if (p == NULL)
			{
			  status = REG_ESPACE;
			  break;
			}
		      assert(tnfa->submatch_data[id].parents == NULL);
		      tnfa->submatch_data[id].parents = p;
		      for (i = 0; parents[i] >= 0; i++)
			p[i] = parents[i];
		      p[i] = -1;
		    }
		}

	      /* Add end of this submatch to regset after processing this
		 node. */
	      STACK_PUSHX(stack, node->submatch_id);
	      STACK_PUSHX(stack, ADDTAGS_SET_SUBMATCH_END);
	    }

	  switch (node->type)
	    {
	    case LITERAL:
	      {
		tre_literal_t *lit = node->obj;

		if (!IS_SPECIAL(lit) || IS_BACKREF(lit))
		  {
		    int i;
		    DPRINT(("Literal %d-%d\n",
			    (int)lit->code_min, (int)lit->code_max));
		    if (regset[0] >= 0)
		      {
			/* Regset is not empty, so add a tag before the
			   literal or backref. */
			if (!first_pass)
			  {
			    status = tre_add_tag_left(mem, node, tag);
			    tnfa->tag_directions[tag] = direction;
			    if (minimal_tag >= 0)
			      {
				DPRINT(("Minimal %d, %d\n", minimal_tag, tag));
				for (i = 0; tnfa->minimal_tags[i] >= 0; i++);
				tnfa->minimal_tags[i] = tag;
				tnfa->minimal_tags[i + 1] = minimal_tag;
				tnfa->minimal_tags[i + 2] = -1;
				minimal_tag = -1;
				num_minimals++;
			      }
			    /* Go through the regset and set submatch data for
			       submatches that are using this tag. */
			    for (i = 0; regset[i] >= 0; i++)
			      {
				int id = regset[i] / 2;
				int start = !(regset[i] % 2);
				DPRINT(("  Using tag %d for %s offset of "
					"submatch %d\n", tag,
					start ? "start" : "end", id));
				if (start)
				  tnfa->submatch_data[id].so_tag = tag;
				else
				  tnfa->submatch_data[id].eo_tag = tag;
			      }
			  }
			else
			  {
			    DPRINT(("  num_tags = 1\n"));
			    node->num_tags = 1;
			  }

			DPRINT(("  num_tags++\n"));
			regset[0] = -1;
			tag = next_tag;
			num_tags++;
			next_tag++;
		      }
		  }
		else
		  {
		    assert(!IS_TAG(lit));
		  }
		break;
	      }
	    case CATENATION:
	      {
		tre_catenation_t *cat = node->obj;
		tre_ast_node_t *left = cat->left;
		tre_ast_node_t *right = cat->right;
		int reserved_tag = -1;
		DPRINT(("Catenation, next_tag = %d\n", next_tag));


		/* After processing right child. */
		STACK_PUSHX(stack, node);
		STACK_PUSHX(stack, ADDTAGS_AFTER_CAT_RIGHT);

		/* Process right child. */
		STACK_PUSHX(stack, right);
		STACK_PUSHX(stack, ADDTAGS_RECURSE);

		/* After processing left child. */
		STACK_PUSHX(stack, next_tag + left->num_tags);
		DPRINT(("  Pushing %d for after left\n",
			next_tag + left->num_tags));
		if (left->num_tags > 0 && right->num_tags > 0)
		  {
		    /* Reserve the next tag to the right child. */
		    DPRINT(("  Reserving next_tag %d to right child\n",
			    next_tag));
		    reserved_tag = next_tag;
		    next_tag++;
		  }
		STACK_PUSHX(stack, reserved_tag);
		STACK_PUSHX(stack, ADDTAGS_AFTER_CAT_LEFT);

		/* Process left child. */
		STACK_PUSHX(stack, left);
		STACK_PUSHX(stack, ADDTAGS_RECURSE);

		}
	      break;
	    case ITERATION:
	      {
		tre_iteration_t *iter = node->obj;
		DPRINT(("Iteration\n"));

		if (first_pass)
		  {
		    STACK_PUSHX(stack, regset[0] >= 0 || iter->minimal);
		  }
		else
		  {
		    STACK_PUSHX(stack, tag);
		    STACK_PUSHX(stack, iter->minimal);
		  }
		STACK_PUSHX(stack, node);
		STACK_PUSHX(stack, ADDTAGS_AFTER_ITERATION);

		STACK_PUSHX(stack, iter->arg);
		STACK_PUSHX(stack, ADDTAGS_RECURSE);

		/* Regset is not empty, so add a tag here. */
		if (regset[0] >= 0 || iter->minimal)
		  {
		    if (!first_pass)
		      {
			int i;
			status = tre_add_tag_left(mem, node, tag);
			if (iter->minimal)
			  tnfa->tag_directions[tag] = TRE_TAG_MAXIMIZE;
			else
			  tnfa->tag_directions[tag] = direction;
			if (minimal_tag >= 0)
			  {
			    DPRINT(("Minimal %d, %d\n", minimal_tag, tag));
			    for (i = 0; tnfa->minimal_tags[i] >= 0; i++);
			    tnfa->minimal_tags[i] = tag;
			    tnfa->minimal_tags[i + 1] = minimal_tag;
			    tnfa->minimal_tags[i + 2] = -1;
			    minimal_tag = -1;
			    num_minimals++;
			  }
			/* Go through the regset and set submatch data for
			   submatches that are using this tag. */
			for (i = 0; regset[i] >= 0; i++)
			  {
			    int id = regset[i] / 2;
			    int start = !(regset[i] % 2);
			    DPRINT(("  Using tag %d for %s offset of "
				    "submatch %d\n", tag,
				    start ? "start" : "end", id));
			    if (start)
			      tnfa->submatch_data[id].so_tag = tag;
			    else
			      tnfa->submatch_data[id].eo_tag = tag;
			  }
		      }

		    DPRINT(("  num_tags++\n"));
		    regset[0] = -1;
		    tag = next_tag;
		    num_tags++;
		    next_tag++;
		  }
		direction = TRE_TAG_MINIMIZE;
	      }
	      break;
	    case UNION:
	      {
		tre_union_t *uni = node->obj;
		tre_ast_node_t *left = uni->left;
		tre_ast_node_t *right = uni->right;
		int left_tag;
		int right_tag;

		if (regset[0] >= 0)
		  {
		    left_tag = next_tag;
		    right_tag = next_tag + 1;
		  }
		else
		  {
		    left_tag = tag;
		    right_tag = next_tag;
		  }

		DPRINT(("Union\n"));

		/* After processing right child. */
		STACK_PUSHX(stack, right_tag);
		STACK_PUSHX(stack, left_tag);
		STACK_PUSHX(stack, regset);
		STACK_PUSHX(stack, regset[0] >= 0);
		STACK_PUSHX(stack, node);
		STACK_PUSHX(stack, right);
		STACK_PUSHX(stack, left);
		STACK_PUSHX(stack, ADDTAGS_AFTER_UNION_RIGHT);

		/* Process right child. */
		STACK_PUSHX(stack, right);
		STACK_PUSHX(stack, ADDTAGS_RECURSE);

		/* After processing left child. */
		STACK_PUSHX(stack, ADDTAGS_AFTER_UNION_LEFT);

		/* Process left child. */
		STACK_PUSHX(stack, left);
		STACK_PUSHX(stack, ADDTAGS_RECURSE);

		/* Regset is not empty, so add a tag here. */
		if (regset[0] >= 0)
		  {
		    if (!first_pass)
		      {
			int i;
			status = tre_add_tag_left(mem, node, tag);
			tnfa->tag_directions[tag] = direction;
			if (minimal_tag >= 0)
			  {
			    DPRINT(("Minimal %d, %d\n", minimal_tag, tag));
			    for (i = 0; tnfa->minimal_tags[i] >= 0; i++);
			    tnfa->minimal_tags[i] = tag;
			    tnfa->minimal_tags[i + 1] = minimal_tag;
			    tnfa->minimal_tags[i + 2] = -1;
			    minimal_tag = -1;
			    num_minimals++;
			  }
			/* Go through the regset and set submatch data for
			   submatches that are using this tag. */
			for (i = 0; regset[i] >= 0; i++)
			  {
			    int id = regset[i] / 2;
			    int start = !(regset[i] % 2);
			    DPRINT(("  Using tag %d for %s offset of "
				    "submatch %d\n", tag,
				    start ? "start" : "end", id));
			    if (start)
			      tnfa->submatch_data[id].so_tag = tag;
			    else
			      tnfa->submatch_data[id].eo_tag = tag;
			  }
		      }

		    DPRINT(("  num_tags++\n"));
		    regset[0] = -1;
		    tag = next_tag;
		    num_tags++;
		    next_tag++;
		  }

		if (node->num_submatches > 0)
		  {
		    /* The next two tags are reserved for markers. */
		    next_tag++;
		    tag = next_tag;
		    next_tag++;
		  }

		break;
	      }
	    }

	  if (node->submatch_id >= 0)
	    {
	      int i;
	      /* Push this submatch on the parents stack. */
	      for (i = 0; parents[i] >= 0; i++);
	      parents[i] = node->submatch_id;
	      parents[i + 1] = -1;
	    }

	  break; /* end case: ADDTAGS_RECURSE */

	case ADDTAGS_AFTER_ITERATION:
	  {
	    int minimal = 0;
	    int enter_tag;
	    node = tre_stack_pop(stack);
	    if (first_pass)
	      {
		node->num_tags = ((tre_iteration_t *)node->obj)->arg->num_tags
		  + (int)tre_stack_pop(stack);
		minimal_tag = -1;
	      }
	    else
	      {
		minimal = (int)tre_stack_pop(stack);
		enter_tag = (int)tre_stack_pop(stack);
		if (minimal)
		  minimal_tag = enter_tag;
	      }

	    DPRINT(("After iteration\n"));
	    if (!first_pass)
	      {
		DPRINT(("  Setting direction to %s\n",
			minimal ? "minimize" : "maximize"));
		if (minimal)
		  direction = TRE_TAG_MINIMIZE;
		else
		  direction = TRE_TAG_MAXIMIZE;
	      }
	    break;
	  }

	case ADDTAGS_AFTER_CAT_LEFT:
	  {
	    int new_tag = (int)tre_stack_pop(stack);
	    next_tag = (int)tre_stack_pop(stack);
	    DPRINT(("After cat left, tag = %d, next_tag = %d\n",
		    tag, next_tag));
	    if (new_tag >= 0)
	      {
		DPRINT(("  Setting tag to %d\n", new_tag));
		tag = new_tag;
	      }
	    break;
	  }

	case ADDTAGS_AFTER_CAT_RIGHT:
	  DPRINT(("After cat right\n"));
	  node = tre_stack_pop(stack);
	  if (first_pass)
	    node->num_tags = ((tre_catenation_t *)node->obj)->left->num_tags
	      + ((tre_catenation_t *)node->obj)->right->num_tags;
	  break;

	case ADDTAGS_AFTER_UNION_LEFT:
	  DPRINT(("After union left\n"));
	  /* Lift the bottom of the `regset' array so that when processing
	     the right operand the items currently in the array are
	     invisible.  The original bottom was saved at ADDTAGS_UNION and
	     will be restored at ADDTAGS_AFTER_UNION_RIGHT below. */
	  while (*regset >= 0)
	    regset++;
	  break;

	case ADDTAGS_AFTER_UNION_RIGHT:
	  {
	    int added_tags, tag_left, tag_right;
	    tre_ast_node_t *left = tre_stack_pop(stack);
	    tre_ast_node_t *right = tre_stack_pop(stack);
	    DPRINT(("After union right\n"));
	    node = tre_stack_pop(stack);
	    added_tags = (int)tre_stack_pop(stack);
	    if (first_pass)
	      {
		node->num_tags = ((tre_union_t *)node->obj)->left->num_tags
		  + ((tre_union_t *)node->obj)->right->num_tags + added_tags
		  + ((node->num_submatches > 0) ? 2 : 0);
	      }
	    regset = tre_stack_pop(stack);
	    tag_left = (int)tre_stack_pop(stack);
	    tag_right = (int)tre_stack_pop(stack);

	    /* Add tags after both children, the left child gets a smaller
	       tag than the right child.  This guarantees that we prefer
	       the left child over the right child. */
	    /* XXX - This is not always necessary (if the children have
	       tags which must be seen for every match of that child). */
	    /* XXX - Check if this is the only place where tre_add_tag_right
	       is used.  If so, use tre_add_tag_left (putting the tag before
	       the child as opposed after the child) and throw away
	       tre_add_tag_right. */
	    if (node->num_submatches > 0)
	      {
		if (!first_pass)
		  {
		    status = tre_add_tag_right(mem, left, tag_left);
		    tnfa->tag_directions[tag] = TRE_TAG_MAXIMIZE;
		    status = tre_add_tag_right(mem, right, tag_right);
		    tnfa->tag_directions[tag] = TRE_TAG_MAXIMIZE;
		  }
		DPRINT(("  num_tags += 2\n"));
		num_tags += 2;
	      }
	    direction = TRE_TAG_MAXIMIZE;
	    break;
	  }

	default:
	  assert(0);
	  break;

	} /* end switch(symbol) */
    } /* end while(tre_stack_num_objects(stack) > bottom) */

  if (!first_pass)
    {
      int i;
      /* Go through the regset and set submatch data for
	 submatches that are using this tag. */
      for (i = 0; regset[i] >= 0; i++)
	{
	  int id = regset[i] / 2;
	  int start = !(regset[i] % 2);
	  DPRINT(("  Using tag %d for %s offset of "
		  "submatch %d\n", num_tags,
		  start ? "start" : "end", id));
	  if (start)
	    tnfa->submatch_data[id].so_tag = num_tags;
	  else
	    tnfa->submatch_data[id].eo_tag = num_tags;
	}
    }

  if (!first_pass && minimal_tag >= 0)
    {
      int i;
      DPRINT(("Minimal %d, %d\n", minimal_tag, tag));
      for (i = 0; tnfa->minimal_tags[i] >= 0; i++);
      tnfa->minimal_tags[i] = tag;
      tnfa->minimal_tags[i + 1] = minimal_tag;
      tnfa->minimal_tags[i + 2] = -1;
      minimal_tag = -1;
      num_minimals++;
    }

  DPRINT(("tre_add_tags: %s complete.  Number of tags %d.\n",
	  first_pass? "First pass" : "Second pass", num_tags));

  assert(tree->num_tags == num_tags);
  tnfa->end_tag = num_tags;
  tnfa->num_tags = num_tags;
  tnfa->num_minimals = num_minimals;
  xfree(orig_regset);
  xfree(parents);
  xfree(saved_states);
  return status;
}





/*
  AST to TNFA compilation routines.
*/

typedef enum {
  COPY_RECURSE,
  COPY_SET_RESULT_PTR
} tre_copyast_symbol_t;

/* Flags for tre_copy_ast(). */
#define COPY_REMOVE_TAGS         1
#define COPY_MAXIMIZE_FIRST_TAG  2

static reg_errcode_t
tre_copy_ast(tre_mem_t mem, tre_stack_t *stack, tre_ast_node_t *ast,
	     int flags, int *pos_add, tre_tag_direction_t *tag_directions,
	     tre_ast_node_t **copy)
{
  reg_errcode_t status = REG_OK;
  int bottom = tre_stack_num_objects(stack);
  int num_copied = 0;
  int first_tag = 1;
  tre_ast_node_t **result = copy;
  tre_copyast_symbol_t symbol;

  STACK_PUSH(stack, ast);
  STACK_PUSH(stack, COPY_RECURSE);

  while (status == REG_OK && tre_stack_num_objects(stack) > bottom)
    {
      tre_ast_node_t *node;
      if (status != REG_OK)
	break;

      symbol = (tre_copyast_symbol_t)tre_stack_pop(stack);
      switch (symbol)
	{
	case COPY_SET_RESULT_PTR:
	  result = tre_stack_pop(stack);
	  break;
	case COPY_RECURSE:
	  node = tre_stack_pop(stack);
	  switch (node->type)
	    {
	    case LITERAL:
	      {
		tre_literal_t *lit = node->obj;
		int pos = lit->position;
		int min = lit->code_min;
		int max = lit->code_max;
		if (!IS_SPECIAL(lit) || IS_BACKREF(lit))
		  {
		    /* XXX - e.g. [ab] has only one position but two
		       nodes, so we are creating holes in the state space
		       here.  Not fatal, just wastes memory. */
		    pos += *pos_add;
		    num_copied++;
		  }
		else if (IS_TAG(lit) && (flags & COPY_REMOVE_TAGS))
		  {
		    /* Change this tag to empty. */
		    min = EMPTY;
		    max = pos = -1;
		  }
		else if (IS_TAG(lit) && (flags & COPY_MAXIMIZE_FIRST_TAG)
			 && first_tag)
		  {
		    /* Maximize the first tag. */
		    tag_directions[max] = TRE_TAG_MAXIMIZE;
		    first_tag = 0;
		  }
		*result = tre_ast_new_literal(mem, min, max, pos);
		if (*result == NULL)
		  status = REG_ESPACE;
	      }
	      break;
	    case UNION:
	      {
		tre_union_t *uni = node->obj;
		tre_union_t *copy;
		*result = tre_ast_new_union(mem, uni->left, uni->right);
		if (*result == NULL)
		  {
		    status = REG_ESPACE;
		    break;
		  }
		copy = (*result)->obj;
		result = &copy->left;
		STACK_PUSHX(stack, uni->right);
		STACK_PUSHX(stack, COPY_RECURSE);
		STACK_PUSHX(stack, &copy->right);
		STACK_PUSHX(stack, COPY_SET_RESULT_PTR);
		STACK_PUSHX(stack, uni->left);
		STACK_PUSHX(stack, COPY_RECURSE);
		break;
	      }
	    case CATENATION:
	      {
		tre_catenation_t *cat = node->obj;
		tre_catenation_t *copy;
		*result = tre_ast_new_catenation(mem, cat->left, cat->right);
		if (*result == NULL)
		  {
		    status = REG_ESPACE;
		    break;
		  }
		copy = (*result)->obj;
		copy->left = NULL;
		copy->right = NULL;
		result = &copy->left;

		STACK_PUSHX(stack, cat->right);
		STACK_PUSHX(stack, COPY_RECURSE);
		STACK_PUSHX(stack, &copy->right);
		STACK_PUSHX(stack, COPY_SET_RESULT_PTR);
		STACK_PUSHX(stack, cat->left);
		STACK_PUSHX(stack, COPY_RECURSE);
		break;
	      }
	    case ITERATION:
	      {
		tre_iteration_t *iter = node->obj;
		STACK_PUSHX(stack, iter->arg);
		*result = tre_ast_new_iter(mem, iter->arg, iter->min,
					   iter->max, iter->minimal);
		if (*result == NULL)
		  {
		    status = REG_ESPACE;
		    break;
		  }
		iter = (*result)->obj;
		result = &iter->arg;
		break;
	      }
	    default:
	      assert(0);
	      break;
	    }
	  break;
	}
    }
  *pos_add += num_copied;
  return status;
}

typedef enum {
  EXPAND_RECURSE,
  EXPAND_AFTER_ITER
} tre_expand_ast_symbol_t;

/* Expands each iteration node that has a finite nonzero minimum or maximum
   iteration count to a catenated sequence of copies of the node. */
static reg_errcode_t
tre_expand_ast(tre_mem_t mem, tre_stack_t *stack, tre_ast_node_t *ast,
	       int *position, tre_tag_direction_t *tag_directions,
	       int *max_depth)
{
  reg_errcode_t status = REG_OK;
  int bottom = tre_stack_num_objects(stack);
  int pos_add = 0;
  int pos_add_total = 0;
  /* Current approximate matching parameters. */
  int params[TRE_PARAM_LAST];
  /* Approximate parameter nesting level. */
  int params_depth = 0;
  int i;

  for (i = 0; i < TRE_PARAM_LAST; i++)
    params[i] = TRE_PARAM_DEFAULT;

  STACK_PUSHR(stack, ast);
  STACK_PUSHR(stack, EXPAND_RECURSE);
  while (status == REG_OK && tre_stack_num_objects(stack) > bottom)
    {
      tre_ast_node_t *node;
      tre_expand_ast_symbol_t symbol;

      if (status != REG_OK)
	break;

      DPRINT(("pos_add %d\n", pos_add));

      symbol = (tre_expand_ast_symbol_t)tre_stack_pop(stack);
      node = tre_stack_pop(stack);
      switch (symbol)
	{
	case EXPAND_RECURSE:
	  switch (node->type)
	    {
	    case LITERAL:
	      {
		tre_literal_t *lit= node->obj;
		if (lit->code_min >= 0)
		  lit->position += pos_add;
		break;
	      }
	    case UNION:
	      {
		tre_union_t *uni = node->obj;
		STACK_PUSHX(stack, uni->right);
		STACK_PUSHX(stack, EXPAND_RECURSE);
		STACK_PUSHX(stack, uni->left);
		STACK_PUSHX(stack, EXPAND_RECURSE);
		break;
	      }
	    case CATENATION:
	      {
		tre_catenation_t *cat = node->obj;
		STACK_PUSHX(stack, cat->right);
		STACK_PUSHX(stack, EXPAND_RECURSE);
		STACK_PUSHX(stack, cat->left);
		STACK_PUSHX(stack, EXPAND_RECURSE);
		break;
	      }
	    case ITERATION:
	      {
		tre_iteration_t *iter = node->obj;
		STACK_PUSHX(stack, pos_add);
		STACK_PUSHX(stack, node);
		STACK_PUSHX(stack, EXPAND_AFTER_ITER);
		STACK_PUSHX(stack, iter->arg);
		STACK_PUSHX(stack, EXPAND_RECURSE);
		DPRINT(("iter\n"));
		break;
	      }
	    default:
	      assert(0);
	      break;
	    }
	  break;
	case EXPAND_AFTER_ITER:
	  {
	    tre_iteration_t *iter = node->obj;
	    int pos_add_last;
	    pos_add = (int)tre_stack_pop(stack);
	    pos_add_last = pos_add;
	    DPRINT(("expand %d %d\n", iter->min, iter->max));
	    if (iter->min > 1 || iter->max > 1)
	      {
		tre_ast_node_t *seq1 = NULL, *seq2 = NULL;
		int i;
		int pos_add_save = pos_add;

		/* Create a catenated sequence of copies of the node. */
		for (i = 0; i < iter->min; i++)
		  {
		    tre_ast_node_t *copy;
		    /* Remove tags from all but the last copy. */
		    int flags = ((i + 1 < iter->min)
				 ? COPY_REMOVE_TAGS
				 : COPY_MAXIMIZE_FIRST_TAG);
		    DPRINT(("  pos_add %d\n", pos_add));
		    pos_add_save = pos_add;
		    status = tre_copy_ast(mem, stack, iter->arg, flags,
					  &pos_add, tag_directions, &copy);
		    if (status != REG_OK)
		      return status;
		    if (seq1 != NULL)
		      seq1 = tre_ast_new_catenation(mem, seq1, copy);
		    else
		      seq1 = copy;
		    if (seq1 == NULL)
		      return REG_ESPACE;
		  }

		if (iter->max == -1)
		  {
		    /* No upper limit. */
		    pos_add_save = pos_add;
		    status = tre_copy_ast(mem, stack, iter->arg, 0,
					  &pos_add, NULL, &seq2);
		    if (status != REG_OK)
		      return status;
		    seq2 = tre_ast_new_iter(mem, seq2, 0, -1, 0);
		    if (seq2 == NULL)
		      return REG_ESPACE;
		  }
		else
		  {
		    for (i = iter->min; i < iter->max; i++)
		      {
			tre_ast_node_t *tmp, *copy;
			pos_add_save = pos_add;
			status = tre_copy_ast(mem, stack, iter->arg, 0,
					      &pos_add, NULL, &copy);
			if (status != REG_OK)
			  return status;
			if (seq2 != NULL)
			  seq2 = tre_ast_new_catenation(mem, copy, seq2);
			else
			  seq2 = copy;
			if (seq2 == NULL)
			  return REG_ESPACE;
			tmp = tre_ast_new_literal(mem, EMPTY, -1, -1);
			if (tmp == NULL)
			  return REG_ESPACE;
			seq2 = tre_ast_new_union(mem, tmp, seq2);
			if (seq2 == NULL)
			  return REG_ESPACE;
		      }
		  }

		pos_add = pos_add_save;
		if (seq1 == NULL)
		  seq1 = seq2;
		else if (seq2 != NULL)
		  seq1 = tre_ast_new_catenation(mem, seq1, seq2);
		if (seq1 == NULL)
		  return REG_ESPACE;
		node->obj = seq1->obj;
		node->type = seq1->type;
	      }

	    pos_add_total += pos_add - pos_add_last;

	    /* If approximate parameters are specified, surround the result
	       with two parameter setting nodes.  The one on the left sets
	       the specified parameters, and the one on the right restores
	       the old parameters. */
	    if (iter->params)
	      {
		tre_ast_node_t *tmp_l, *tmp_r, *tmp_node, *node_copy;
		unsigned int *old_params;

		tmp_l = tre_ast_new_literal(mem, PARAMETER, 0, -1);
		if (!tmp_l)
		  return REG_ESPACE;
		((tre_literal_t *)tmp_l->obj)->u.params = iter->params;
		iter->params[TRE_PARAM_DEPTH] = params_depth + 1;
		tmp_r = tre_ast_new_literal(mem, PARAMETER, 0, -1);
		if (!tmp_r)
		  return REG_ESPACE;
		old_params = tre_mem_alloc(mem, sizeof(*old_params)
					   * TRE_PARAM_LAST);
		if (!old_params)
		  return REG_ESPACE;
		for (i = 0; i < TRE_PARAM_LAST; i++)
		  old_params[i] = params[i];
		((tre_literal_t *)tmp_r->obj)->u.params = old_params;
		old_params[TRE_PARAM_DEPTH] = params_depth;
		node_copy = tre_ast_new_node(mem, ITERATION,
					     sizeof(tre_iteration_t));
		if (!node_copy)
		  return REG_ESPACE;
		node_copy->obj = node->obj;
		tmp_node = tre_ast_new_catenation(mem, tmp_l, node_copy);
		if (!tmp_node)
		  return REG_ESPACE;
		tmp_node = tre_ast_new_catenation(mem, tmp_node, tmp_r);
		if (!tmp_node)
		  return REG_ESPACE;
		/* Replace the contents of `node' with `tmp_node'. */
		memcpy(node, tmp_node, sizeof(*node));
		node->obj = tmp_node->obj;
		node->type = tmp_node->type;
		params_depth++;
		if (params_depth > *max_depth)
		  *max_depth = params_depth;
	      }
	    break;
	  }
	default:
	  assert(0);
	  break;
	}
    }

  *position += pos_add_total;
  return status;
}

static tre_pos_and_tags_t *
tre_set_empty(tre_mem_t mem)
{
  tre_pos_and_tags_t *new_set;

  new_set = tre_mem_calloc(mem, sizeof(*new_set));
  if (new_set == NULL)
    return NULL;

  new_set[0].position = -1;
  new_set[0].code_min = -1;
  new_set[0].code_max = -1;

  return new_set;
}

static tre_pos_and_tags_t *
tre_set_one(tre_mem_t mem, int position, int code_min, int code_max,
	    tre_ctype_t class, tre_ctype_t *neg_classes, int backref)
{
  tre_pos_and_tags_t *new_set;

  new_set = tre_mem_calloc(mem, sizeof(*new_set) * 2);
  if (new_set == NULL)
    return NULL;

  new_set[0].position = position;
  new_set[0].code_min = code_min;
  new_set[0].code_max = code_max;
  new_set[0].class = class;
  new_set[0].neg_classes = neg_classes;
  new_set[0].backref = backref;
  new_set[1].position = -1;
  new_set[1].code_min = -1;
  new_set[1].code_max = -1;

  return new_set;
}

static tre_pos_and_tags_t *
tre_set_union(tre_mem_t mem, tre_pos_and_tags_t *set1, tre_pos_and_tags_t *set2,
	      int *tags, int assertions, int *params)
{
  int s1, s2, i, j;
  tre_pos_and_tags_t *new_set;
  int *new_tags;
  int num_tags;

  for (num_tags = 0; tags != NULL && tags[num_tags] >= 0; num_tags++);
  for (s1 = 0; set1[s1].position >= 0; s1++);
  for (s2 = 0; set2[s2].position >= 0; s2++);
  new_set = tre_mem_calloc(mem, sizeof(*new_set) * (s1 + s2 + 1));
  if (!new_set )
    return NULL;

  for (s1 = 0; set1[s1].position >= 0; s1++)
    {
      new_set[s1].position = set1[s1].position;
      new_set[s1].code_min = set1[s1].code_min;
      new_set[s1].code_max = set1[s1].code_max;
      new_set[s1].assertions = set1[s1].assertions | assertions;
      new_set[s1].class = set1[s1].class;
      new_set[s1].neg_classes = set1[s1].neg_classes;
      new_set[s1].backref = set1[s1].backref;
      if (set1[s1].tags == NULL && tags == NULL)
	new_set[s1].tags = NULL;
      else
	{
	  for (i = 0; set1[s1].tags != NULL && set1[s1].tags[i] >= 0; i++);
	  new_tags = tre_mem_alloc(mem, (sizeof(*new_tags)
					 * (i + num_tags + 1)));
	  if (new_tags == NULL)
	    return NULL;
	  for (j = 0; j < i; j++)
	    new_tags[j] = set1[s1].tags[j];
	  for (i = 0; i < num_tags; i++)
	    new_tags[j + i] = tags[i];
	  new_tags[j + i] = -1;
	  new_set[s1].tags = new_tags;
	}
      if (set1[s1].params)
	new_set[s1].params = set1[s1].params;
      if (params)
	{
	  if (!new_set[s1].params)
	    new_set[s1].params = params;
	  else
	    {
	      new_set[s1].params = tre_mem_alloc(mem, sizeof(*params) *
						 TRE_PARAM_LAST);
	      if (!new_set[s1].params)
		return NULL;
	      for (i = 0; i < TRE_PARAM_LAST; i++)
		if (params[i] != TRE_PARAM_UNSET)
		  new_set[s1].params[i] = params[i];
	    }
	}
    }

  for (s2 = 0; set2[s2].position >= 0; s2++)
    {
      new_set[s1 + s2].position = set2[s2].position;
      new_set[s1 + s2].code_min = set2[s2].code_min;
      new_set[s1 + s2].code_max = set2[s2].code_max;
      /* XXX - why not | assertions here as well? */
      new_set[s1 + s2].assertions = set2[s2].assertions;
      new_set[s1 + s2].class = set2[s2].class;
      new_set[s1 + s2].neg_classes = set2[s2].neg_classes;
      new_set[s1 + s2].backref = set2[s2].backref;
      if (set2[s2].tags == NULL)
	new_set[s1 + s2].tags = NULL;
      else
	{
	  for (i = 0; set2[s2].tags[i] >= 0; i++);
	  new_tags = tre_mem_alloc(mem, sizeof(*new_tags) * (i + 1));
	  if (new_tags == NULL)
	    return NULL;
	  for (j = 0; j < i; j++)
	    new_tags[j] = set2[s2].tags[j];
	  new_tags[j] = -1;
	  new_set[s1 + s2].tags = new_tags;
	}
      if (set2[s2].params)
	new_set[s1 + s2].params = set2[s2].params;
      if (params)
	{
	  if (!new_set[s1 + s2].params)
	    new_set[s1 + s2].params = params;
	  else
	    {
	      new_set[s1 + s2].params = tre_mem_alloc(mem, sizeof(*params) *
						      TRE_PARAM_LAST);
	      if (!new_set[s1 + s2].params)
		return NULL;
	      for (i = 0; i < TRE_PARAM_LAST; i++)
		if (params[i] != TRE_PARAM_UNSET)
		  new_set[s1 + s2].params[i] = params[i];
	    }
	}
    }
  new_set[s1 + s2].position = -1;
  return new_set;
}

/* Finds the empty path through `node' which is the one that should be
   taken according to POSIX.2 rules, and adds the tags on that path to
   `tags'.   `tags' may be NULL.  If `num_tags_seen' is not NULL, it is
   set to the number of tags seen on the path. */
static reg_errcode_t
tre_match_empty(tre_stack_t *stack, tre_ast_node_t *node, int *tags,
		int *assertions, int *params, int *num_tags_seen,
		int *params_seen)
{
  tre_literal_t *lit;
  tre_union_t *uni;
  tre_catenation_t *cat;
  tre_iteration_t *iter;
  int i;
  int bottom = tre_stack_num_objects(stack);
  reg_errcode_t status = REG_OK;
  if (num_tags_seen)
    *num_tags_seen = 0;
  if (params_seen)
    *params_seen = 0;

  status = tre_stack_push(stack, node);

  /* Walk through the tree recursively. */
  while (status == REG_OK && tre_stack_num_objects(stack) > bottom)
    {
      node = tre_stack_pop(stack);

      switch (node->type)
	{
	case LITERAL:
	  lit = (tre_literal_t *)node->obj;
	  switch (lit->code_min)
	    {
	    case TAG:
	      if (lit->code_max >= 0)
		{
		  if (tags != NULL)
		    {
		      /* Add the tag to `tags'. */
		      for (i = 0; tags[i] >= 0; i++)
			if (tags[i] == lit->code_max)
			  break;
		      if (tags[i] < 0)
			{
			  tags[i] = lit->code_max;
			  tags[i + 1] = -1;
			}
		    }
		  if (num_tags_seen)
		    (*num_tags_seen)++;
		}
	      break;
	    case ASSERTION:
	      assert(lit->code_max >= 1
		     || lit->code_max <= ASSERT_LAST);
	      if (assertions != NULL)
		*assertions |= lit->code_max;
	      break;
	    case PARAMETER:
	      if (params != NULL)
		for (i = 0; i < TRE_PARAM_LAST; i++)
		  params[i] = lit->u.params[i];
	      if (params_seen != NULL)
		*params_seen = 1;
	      break;
	    case EMPTY:
	      break;
	    default:
	      assert(0);
	      break;
	    }
	  break;

	case UNION:
	  /* Subexpressions starting earlier take priority over ones
	     starting later, so we prefer the left subexpression over the
	     right subexpression. */
	  uni = (tre_union_t *)node->obj;
	  if (uni->left->nullable)
	    STACK_PUSHX(stack, uni->left)
	  else if (uni->right->nullable)
	    STACK_PUSHX(stack, uni->right)
	  else
	    assert(0);
	  break;

	case CATENATION:
	  /* The path must go through both children. */
	  cat = (tre_catenation_t *)node->obj;
	  assert(cat->left->nullable);
	  assert(cat->right->nullable);
	  STACK_PUSHX(stack, cat->left);
	  STACK_PUSHX(stack, cat->right);
	  break;

	case ITERATION:
	  /* A match with an empty string is preferred over no match at
	     all, so we go through the argument if possible. */
	  iter = (tre_iteration_t *)node->obj;
	  if (iter->arg->nullable)
	    STACK_PUSHX(stack, iter->arg);
	  break;

	default:
	  assert(0);
	  break;
	}
    }

  return status;
}


typedef enum {
  NFL_RECURSE,
  NFL_POST_UNION,
  NFL_POST_CATENATION,
  NFL_POST_ITERATION
} tre_nfl_stack_symbol_t;


/* Computes and fills in the fields `nullable', `firstpos', and `lastpos' for
   the nodes of the AST `tree'. */
static reg_errcode_t
tre_compute_nfl(tre_mem_t mem, tre_stack_t *stack, tre_ast_node_t *tree)
{
  int bottom = tre_stack_num_objects(stack);

  STACK_PUSHR(stack, tree);
  STACK_PUSHR(stack, NFL_RECURSE);

  while (tre_stack_num_objects(stack) > bottom)
    {
      tre_nfl_stack_symbol_t symbol;
      tre_ast_node_t *node;

      symbol = (tre_nfl_stack_symbol_t) tre_stack_pop(stack);
      node = tre_stack_pop(stack);
      switch (symbol)
	{
	case NFL_RECURSE:
	  switch (node->type)
	    {
	    case LITERAL:
	      {
		tre_literal_t *lit = (tre_literal_t *)node->obj;
		if (IS_BACKREF(lit))
		  {
		    /* Back references: nullable = false, firstpos = {i},
		       lastpos = {i}. */
		    node->nullable = 0;
		    node->firstpos = tre_set_one(mem, lit->position, 0,
					     TRE_CHAR_MAX, 0, NULL, -1);
		    if (!node->firstpos)
		      return REG_ESPACE;
		    node->lastpos = tre_set_one(mem, lit->position, 0,
						TRE_CHAR_MAX, 0, NULL,
						lit->code_max);
		    if (!node->lastpos)
		      return REG_ESPACE;
		  }
		else if (lit->code_min < 0)
		  {
		    /* Tags, empty strings, params, and zero width assertions:
		       nullable = true, firstpos = {}, and lastpos = {}. */
		    node->nullable = 1;
		    node->firstpos = tre_set_empty(mem);
		    if (!node->firstpos)
		      return REG_ESPACE;
		    node->lastpos = tre_set_empty(mem);
		    if (!node->lastpos)
		      return REG_ESPACE;
		  }
		else
		  {
		    /* Literal at position i: nullable = false, firstpos = {i},
		       lastpos = {i}. */
		    node->nullable = 0;
		    node->firstpos =
		      tre_set_one(mem, lit->position, lit->code_min,
				  lit->code_max, 0, NULL, -1);
		    if (!node->firstpos)
		      return REG_ESPACE;
		    node->lastpos = tre_set_one(mem, lit->position,
						lit->code_min, lit->code_max,
						lit->u.class, lit->neg_classes,
						-1);
		    if (!node->lastpos)
		      return REG_ESPACE;
		  }
		break;
	      }

	    case UNION:
	      /* Compute the attributes for the two subtrees, and after that
		 for this node. */
	      STACK_PUSHR(stack, node);
	      STACK_PUSHR(stack, NFL_POST_UNION);
	      STACK_PUSHR(stack, ((tre_union_t *)node->obj)->right);
	      STACK_PUSHR(stack, NFL_RECURSE);
	      STACK_PUSHR(stack, ((tre_union_t *)node->obj)->left);
	      STACK_PUSHR(stack, NFL_RECURSE);
	      break;

	    case CATENATION:
	      /* Compute the attributes for the two subtrees, and after that
		 for this node. */
	      STACK_PUSHR(stack, node);
	      STACK_PUSHR(stack, NFL_POST_CATENATION);
	      STACK_PUSHR(stack, ((tre_catenation_t *)node->obj)->right);
	      STACK_PUSHR(stack, NFL_RECURSE);
	      STACK_PUSHR(stack, ((tre_catenation_t *)node->obj)->left);
	      STACK_PUSHR(stack, NFL_RECURSE);
	      break;

	    case ITERATION:
	      /* Compute the attributes for the subtree, and after that for
		 this node. */
	      STACK_PUSHR(stack, node);
	      STACK_PUSHR(stack, NFL_POST_ITERATION);
	      STACK_PUSHR(stack, ((tre_iteration_t *)node->obj)->arg);
	      STACK_PUSHR(stack, NFL_RECURSE);
	      break;
	    }
	  break; /* end case: NFL_RECURSE */

	case NFL_POST_UNION:
	  {
	    tre_union_t *uni = (tre_union_t *)node->obj;
	    node->nullable = uni->left->nullable || uni->right->nullable;
	    node->firstpos = tre_set_union(mem, uni->left->firstpos,
					   uni->right->firstpos, NULL, 0, NULL);
	    if (!node->firstpos)
	      return REG_ESPACE;
	    node->lastpos = tre_set_union(mem, uni->left->lastpos,
					  uni->right->lastpos, NULL, 0, NULL);
	    if (!node->lastpos)
	      return REG_ESPACE;
	    break;
	  }

	case NFL_POST_ITERATION:
	  {
	    tre_iteration_t *iter = (tre_iteration_t *)node->obj;

	    if (iter->min == 0 || iter->arg->nullable)
	      node->nullable = 1;
	    else
	      node->nullable = 0;
	    node->firstpos = iter->arg->firstpos;
	    node->lastpos = iter->arg->lastpos;
	    break;
	  }

	case NFL_POST_CATENATION:
	  {
	    int num_tags, *tags, assertions, params_seen;
	    int *params;
	    reg_errcode_t status;
	    tre_catenation_t *cat = node->obj;
	    node->nullable = cat->left->nullable && cat->right->nullable;

	    /* Compute firstpos. */
	    if (cat->left->nullable)
	      {
		/* The left side matches the empty string.  Make a first pass
		   with tre_match_empty() to get the number of tags and
		   parameters. */
		status = tre_match_empty(stack, cat->left,
					 NULL, NULL, NULL, &num_tags,
					 &params_seen);
		if (status != REG_OK)
		  return status;
		/* Allocate arrays for the tags and parameters. */
		tags = xmalloc(sizeof(*tags) * (num_tags + 1));
		if (!tags)
		  return REG_ESPACE;
		tags[0] = -1;
		assertions = 0;
		params = NULL;
		if (params_seen)
		  {
		    params = tre_mem_alloc(mem, sizeof(*params)
					   * TRE_PARAM_LAST);
		    if (!params)
		      {
			xfree(tags);
			return REG_ESPACE;
		      }
		  }
		/* Second pass with tre_mach_empty() to get the list of
		   tags and parameters. */
		status = tre_match_empty(stack, cat->left, tags,
					 &assertions, params, NULL, NULL);
		if (status != REG_OK)
		  {
		    xfree(tags);
		    return status;
		  }
		node->firstpos =
		  tre_set_union(mem, cat->right->firstpos, cat->left->firstpos,
				tags, assertions, params);
		xfree(tags);
		if (!node->firstpos)
		  return REG_ESPACE;
	      }
	    else
	      {
		node->firstpos = cat->left->firstpos;
	      }

	    /* Compute lastpos. */
	    if (cat->right->nullable)
	      {
		/* The right side matches the empty string.  Make a first pass
		   with tre_match_empty() to get the number of tags and
		   parameters. */
		status = tre_match_empty(stack, cat->right,
					 NULL, NULL, NULL, &num_tags,
					 &params_seen);
		if (status != REG_OK)
		  return status;
		/* Allocate arrays for the tags and parameters. */
		tags = xmalloc(sizeof(int) * (num_tags + 1));
		if (!tags)
		  return REG_ESPACE;
		tags[0] = -1;
		assertions = 0;
		params = NULL;
		if (params_seen)
		  {
		    params = tre_mem_alloc(mem, sizeof(*params)
					   * TRE_PARAM_LAST);
		    if (!params)
		      {
			xfree(tags);
			return REG_ESPACE;
		      }
		  }
		/* Second pass with tre_mach_empty() to get the list of
		   tags and parameters. */
		status = tre_match_empty(stack, cat->right, tags,
					 &assertions, params, NULL, NULL);
		if (status != REG_OK)
		  {
		    xfree(tags);
		    return status;
		  }
		node->lastpos =
		  tre_set_union(mem, cat->left->lastpos, cat->right->lastpos,
				tags, assertions, params);
		xfree(tags);
		if (!node->lastpos)
		  return REG_ESPACE;
	      }
	    else
	      {
		node->lastpos = cat->right->lastpos;
	      }
	    break;
	  }

	default:
	  assert(0);
	  break;
	}
    }

  return REG_OK;
}


/* Adds a transition from each position in `p1' to each position in `p2'. */
static reg_errcode_t
tre_make_trans(tre_pos_and_tags_t *p1, tre_pos_and_tags_t *p2,
	       tre_tnfa_transition_t *transitions,
	       int *counts, int *offs)
{
  tre_pos_and_tags_t *orig_p2 = p2;
  tre_tnfa_transition_t *trans;
  int i, j, k, l, dup, prev_p2_pos;

  if (transitions != NULL)
    while (p1->position >= 0)
      {
	p2 = orig_p2;
	prev_p2_pos = -1;
	while (p2->position >= 0)
	  {
	    /* Optimization: if this position was already handled, skip it. */
	    if (p2->position == prev_p2_pos)
	      {
		p2++;
		continue;
	      }
	    prev_p2_pos = p2->position;
	    /* Set `trans' to point to the next unused transition from
	       position `p1->position'. */
	    trans = transitions + offs[p1->position];
	    while (trans->state != NULL)
	      {
#if 0
		/* If we find a previous transition from `p1->position' to
		   `p2->position', it is overwritten.  This can happen only
		   if there are nested loops in the regexp, like in "((a)*)*".
		   In POSIX.2 repetition using the outer loop is always
		   preferred over using the inner loop.  Therefore the
		   transition for the inner loop is useless and can be thrown
		   away. */
		/* XXX - The same position is used for all nodes in a bracket
		   expression, so this optimization cannot be used (it will
		   break bracket expressions) unless I figure out a way to
		   detect it here. */
		if (trans->state_id == p2->position)
		  {
		    DPRINT(("*"));
		    break;
		  }
#endif
		trans++;
	      }

	    if (trans->state == NULL)
	      (trans + 1)->state = NULL;
	    /* Use the character ranges, assertions, etc. from `p1' for
	       the transition from `p1' to `p2'. */
	    trans->code_min = p1->code_min;
	    trans->code_max = p1->code_max;
	    trans->state = transitions + offs[p2->position];
	    trans->state_id = p2->position;
	    trans->assertions = p1->assertions | p2->assertions
	      | (p1->class ? ASSERT_CHAR_CLASS : 0)
	      | (p1->neg_classes != NULL ? ASSERT_CHAR_CLASS_NEG : 0);
	    if (p1->backref >= 0)
	      {
		assert(trans->assertions == 0);
		assert(p2->backref < 0);
		trans->u.backref = p1->backref;
		trans->assertions |= ASSERT_BACKREF;
	      }
	    else
	      trans->u.class = p1->class;
	    if (p1->neg_classes != NULL)
	      {
		for (i = 0; p1->neg_classes[i] != (tre_ctype_t)0; i++);
		trans->neg_classes =
		  xmalloc(sizeof(*trans->neg_classes) * (i + 1));
		if (trans->neg_classes == NULL)
		  return REG_ESPACE;
		for (i = 0; p1->neg_classes[i] != (tre_ctype_t)0; i++)
		  trans->neg_classes[i] = p1->neg_classes[i];
		trans->neg_classes[i] = (tre_ctype_t)0;
	      }
	    else
	      trans->neg_classes = NULL;

	    /* Find out how many tags this transition has. */
	    i = 0;
	    if (p1->tags != NULL)
	      while(p1->tags[i] >= 0)
		i++;
	    j = 0;
	    if (p2->tags != NULL)
	      while(p2->tags[j] >= 0)
		j++;

	    /* If we are overwriting a transition, free the old tag array. */
	    if (trans->tags != NULL)
	      xfree(trans->tags);
	    trans->tags = NULL;

	    /* If there were any tags, allocate an array and fill it. */
	    if (i + j > 0)
	      {
		trans->tags = xmalloc(sizeof(*trans->tags) * (i + j + 1));
		if (!trans->tags)
		  return REG_ESPACE;
		i = 0;
		if (p1->tags != NULL)
		  while(p1->tags[i] >= 0)
		    {
		      trans->tags[i] = p1->tags[i];
		      i++;
		    }
		l = i;
		j = 0;
		if (p2->tags != NULL)
		  while (p2->tags[j] >= 0)
		    {
		      /* Don't add duplicates. */
		      dup = 0;
		      for (k = 0; k < i; k++)
			if (trans->tags[k] == p2->tags[j])
			  {
			    dup = 1;
			    break;
			  }
		      if (!dup)
			trans->tags[l++] = p2->tags[j];
		      j++;
		    }
		trans->tags[l] = -1;
	      }

	    /* Set the parameter array.  If both `p2' and `p1' have same
	       parameters, the values in `p2' override those in `p1'. */
	    if (p1->params || p2->params)
	      {
		if (!trans->params)
		  trans->params = xmalloc(sizeof(*trans->params)
					  * TRE_PARAM_LAST);
		if (!trans->params)
		  return REG_ESPACE;
		for (i = 0; i < TRE_PARAM_LAST; i++)
		  {
		    trans->params[i] = TRE_PARAM_UNSET;
		    if (p1->params && p1->params[i] != TRE_PARAM_UNSET)
		      trans->params[i] = p1->params[i];
		    if (p2->params && p2->params[i] != TRE_PARAM_UNSET)
		      trans->params[i] = p2->params[i];
		  }
	      }
	    else
	      {
		if (trans->params)
		  xfree(trans->params);
		trans->params = NULL;
	      }


#ifdef TRE_DEBUG
	    {
	      int *tags;

	      DPRINT(("  %2d -> %2d on %3d", p1->position, p2->position,
		      p1->code_min));
	      if (p1->code_max != p1->code_min)
		DPRINT(("-%3d", p1->code_max));
	      tags = trans->tags;
	      if (tags)
		{
		  DPRINT((", tags ["));
		  while (*tags >= 0)
		    {
		      DPRINT(("%d", *tags));
		      tags++;
		      if (*tags >= 0)
			DPRINT((","));
		    }
		  DPRINT(("]"));
		}
	      if (trans->assertions)
		DPRINT((", assert %d", trans->assertions));
	      if (trans->assertions & ASSERT_BACKREF)
		DPRINT((", backref %d", trans->u.backref));
	      else if (trans->u.class)
		DPRINT((", class %ld", (long)trans->u.class));
	      if (trans->neg_classes)
		DPRINT((", neg_classes %p", trans->neg_classes));
	      if (trans->params)
		{
		  DPRINT((", "));
		  tre_print_params(trans->params);
		}
	      DPRINT(("\n"));
	    }
#endif /* TRE_DEBUG */
	    p2++;
	  }
	p1++;
      }
  else
    /* Compute a maximum limit for the number of transitions leaving
       from each state. */
    while (p1->position >= 0)
      {
	p2 = orig_p2;
	while (p2->position >= 0)
	  {
	    counts[p1->position]++;
	    p2++;
	  }
	p1++;
      }
  return REG_OK;
}

/* Converts the syntax tree to a TNFA.  All the transitions in the TNFA are
   labelled with one character range (there are no transitions on empty
   strings). The TNFA takes O(n^2) space in the worst case, `n' is size of
   the regexp. */
static reg_errcode_t
tre_ast_to_tnfa(tre_ast_node_t *node, tre_tnfa_transition_t *transitions,
		int *counts, int *offs)
{
  tre_union_t *uni;
  tre_catenation_t *cat;
  tre_iteration_t *iter;
  reg_errcode_t errcode = REG_OK;

  /* XXX - recurse using a stack!. */
  switch (node->type)
    {
    case LITERAL:
      break;
    case UNION:
      uni = (tre_union_t *)node->obj;
      errcode = tre_ast_to_tnfa(uni->left, transitions, counts, offs);
      if (errcode != REG_OK)
	return errcode;
      errcode = tre_ast_to_tnfa(uni->right, transitions, counts, offs);
      break;

    case CATENATION:
      cat = (tre_catenation_t *)node->obj;
      /* Add a transition from each position in cat->left->lastpos
	 to each position in cat->right->firstpos. */
      errcode = tre_make_trans(cat->left->lastpos, cat->right->firstpos,
			       transitions, counts, offs);
      if (errcode != REG_OK)
	return errcode;
      errcode = tre_ast_to_tnfa(cat->left, transitions, counts, offs);
      if (errcode != REG_OK)
	return errcode;
      errcode = tre_ast_to_tnfa(cat->right, transitions, counts, offs);
      break;

    case ITERATION:
      iter = (tre_iteration_t *)node->obj;
      assert(iter->max == -1 || iter->max == 1);

      if (iter->max == -1)
	{
	  assert(iter->min == 0 || iter->min == 1);
	  /* Add a transition from each last position in the iterated
	     expression to each first position. */
	  errcode = tre_make_trans(iter->arg->lastpos, iter->arg->firstpos,
				   transitions, counts, offs);
	  if (errcode != REG_OK)
	    return errcode;
	}
      errcode = tre_ast_to_tnfa(iter->arg, transitions, counts, offs);
      break;
    }
  return errcode;
}




/*
  Regexp parser.

  The parser is just a simple recursive descent parser for POSIX.2
  regexps.  Supports both the obsolete default syntax and the "extended"
  syntax, and some nonstandard extensions.

*/


/* Characters with special meanings in regexp syntax. */
#define CHAR_PIPE          L'|'
#define CHAR_LPAREN        L'('
#define CHAR_RPAREN        L')'
#define CHAR_LBRACE        L'{'
#define CHAR_RBRACE        L'}'
#define CHAR_LBRACKET      L'['
#define CHAR_RBRACKET      L']'
#define CHAR_MINUS         L'-'
#define CHAR_STAR          L'*'
#define CHAR_QUESTIONMARK  L'?'
#define CHAR_PLUS          L'+'
#define CHAR_PERIOD        L'.'
#define CHAR_COLON         L':'
#define CHAR_EQUAL         L'='
#define CHAR_COMMA         L','
#define CHAR_CARET         L'^'
#define CHAR_DOLLAR        L'$'
#define CHAR_BACKSLASH     L'\\'
#define CHAR_HASH          L'#'
#define CHAR_TILDE         L'~'


/* Some macros for expanding \w, \s, etc. */
/* XXX - combine the two below? */
#ifdef TRE_WCHAR
static const tre_char_t *tre_macros[] =
  { L"t", L"\t",             L"n", L"\n",             L"r", L"\r",
    L"f", L"\f",             L"a", L"\a",             L"e", L"\033",
    L"w", L"[[:alnum:]_]",   L"W", L"[^[:alnum:]_]",  L"s", L"[[:space:]]",
    L"S", L"[^[:space:]]",   L"d", L"[[:digit:]]",    L"D", L"[^[:digit:]]",
   NULL };
#else /* !TRE_WCHAR */
static const tre_char_t *tre_macros[] =
  { "t", "\t",             "n", "\n",             "r", "\r",
    "f", "\f",             "a", "\a",             "e", "\033",
    "w", "[[:alnum:]_]",   "W", "[^[:alnum:]_]",  "s", "[[:space:]]",
    "S", "[^[:space:]]",   "d", "[[:digit:]]",    "D", "[^[:digit:]]",
   NULL };
#endif /* !TRE_WCHAR */



/* Parse context. */
typedef struct {
  /* Memory allocator.  The AST is allocated using this. */
  tre_mem_t mem;
  /* Stack used for keeping track of regexp syntax. */
  tre_stack_t *stack;
  /* The parse result. */
  tre_ast_node_t *result;
  /* The regexp to parse and its length. */
  const tre_char_t *re;
  /* The first character of the entire regexp. */
  const tre_char_t *re_start;
  /* The first character after the end of the regexp. */
  const tre_char_t *re_end;
  int len;
  /* Current submatch ID. */
  int submatch_id;
  /* Current position (number of literal). */
  int position;
  /* This flag is set if the regexp contains back references. */
  int have_backrefs;
  /* This flag is set if the regexp uses approximate matching. */
  int have_approx;
  /* Compilation flags. */
  int cflags;
  /* If this flag is set the top-level submatch is not captured. */
  int nofirstsub;
  /* The currently set approximate matching parameters. */
  int params[TRE_PARAM_LAST];
} tre_parse_ctx_t;

static const tre_char_t *
tre_expand_macro(const tre_char_t *regex, const tre_char_t *regex_end)
{
  int i;
  size_t len = regex_end - regex;
  for (i = 0; tre_macros[i] != NULL; i += 2)
    {
      if (tre_strlen(tre_macros[i]) > len)
	continue;
#ifdef TRE_WCHAR
      if (wcsncmp(tre_macros[i], regex, tre_strlen(tre_macros[i])) == 0)
#else /* !TRE_WCHAR */
      if (strncmp(tre_macros[i], regex, tre_strlen(tre_macros[i])) == 0)
#endif /* TRE_WCHAR */
	{
	  DPRINT(("Expanding macro '%ls' => '%ls'\n"
		  , tre_macros[i], tre_macros[i + 1]));
	  break;
	}
    }
  if (tre_macros[i] == NULL)
    return NULL;
  return tre_macros[i + 1];
}

static reg_errcode_t
tre_new_item(tre_mem_t mem, int min, int max, int *i, int *max_i,
	 tre_ast_node_t ***items)
{
  reg_errcode_t status;
  tre_ast_node_t **array = *items;
  /* Allocate more space if necessary. */
  if (*i >= *max_i)
    {
      tre_ast_node_t **new_items;
      DPRINT(("out of array space, i = %d\n", *i));
      /* If the array is already 1024 items large, give up -- there's
	 probably an error in the regexp (e.g. not a '\0' terminated
	 string and missing ']') */
      if (*max_i > 1024)
	return REG_ESPACE;
      *max_i *= 2;
      new_items = xrealloc(array, sizeof(*items) * *max_i);
      if (new_items == NULL)
	return REG_ESPACE;
      *items = array = new_items;
    }
  array[*i] = tre_ast_new_literal(mem, min, max, -1);
  status = array[*i] == NULL ? REG_ESPACE : REG_OK;
  (*i)++;
  return status;
}


/* Expands a character class to character ranges. */
static reg_errcode_t
tre_expand_ctype(tre_mem_t mem, tre_ctype_t class, tre_ast_node_t ***items,
	     int *i, int *max_i, int cflags)
{
  reg_errcode_t status = REG_OK;
  tre_cint_t c;
  int j, min = -1, max = 0;
  assert(TRE_MB_CUR_MAX == 1);

  DPRINT(("  expanding class to character ranges\n"));
  for (j = 0; (j < 256) && (status == REG_OK); j++)
    {
      c = j;
      if (tre_isctype(c, class)
	  || ((cflags & REG_ICASE)
	      && (tre_isctype(tre_tolower(c), class)
		  || tre_isctype(tre_toupper(c), class))))
	{
	  if (min < 0)
	    min = c;
	  max = c;
	}
      else if (min >= 0)
	{
	  DPRINT(("  range %c (%d) to %c (%d)\n", min, min, max, max));
	  status = tre_new_item(mem, min, max, i, max_i, items);
	  min = -1;
	}
    }
  if (min >= 0 && status == REG_OK)
    status = tre_new_item(mem, min, max, i, max_i, items);
  return status;
}


static int
tre_compare_items(const void *a, const void *b)
{
  tre_ast_node_t *node_a = *(tre_ast_node_t **)a;
  tre_ast_node_t *node_b = *(tre_ast_node_t **)b;
  tre_literal_t *l_a = node_a->obj, *l_b = node_b->obj;
  int a_min = l_a->code_min, b_min = l_b->code_min;

  if (a_min < b_min)
    return -1;
  else if (a_min > b_min)
    return 1;
  else
    return 0;
}

#ifndef TRE_WCHAR
/* isalnum() and the rest may be macros, so wrap them... */
int tre_isalnum(tre_cint_t c) { return isalnum(c); }
int tre_isalpha(tre_cint_t c) { return isalpha(c); }
#ifdef HAVE_ISASCII
int tre_isascii(tre_cint_t c) { return isascii(c); }
#endif /* HAVE_ISASCII */
#ifdef HAVE_ISBLANK
int tre_isblank(tre_cint_t c) { return isblank(c); }
#endif /* HAVE_ISBLANK */
int tre_iscntrl(tre_cint_t c) { return iscntrl(c); }
int tre_isdigit(tre_cint_t c) { return isdigit(c); }
int tre_isgraph(tre_cint_t c) { return isgraph(c); }
int tre_islower(tre_cint_t c) { return islower(c); }
int tre_isprint(tre_cint_t c) { return isprint(c); }
int tre_ispunct(tre_cint_t c) { return ispunct(c); }
int tre_isspace(tre_cint_t c) { return isspace(c); }
int tre_isupper(tre_cint_t c) { return isupper(c); }
int tre_isxdigit(tre_cint_t c) { return isxdigit(c); }

struct {
  char *name;
  int (*func)(tre_cint_t);
} tre_ctype_map[] = {
  { "alnum", &tre_isalnum },
  { "alpha", &tre_isalpha },
#ifdef HAVE_ISASCII
  { "ascii", &tre_isascii },
#endif /* HAVE_ISASCII */
#ifdef HAVE_ISBLANK
  { "blank", &tre_isblank },
#endif /* HAVE_ISBLANK */
  { "cntrl", &tre_iscntrl },
  { "digit", &tre_isdigit },
  { "graph", &tre_isgraph },
  { "lower", &tre_islower },
  { "print", &tre_isprint },
  { "punct", &tre_ispunct },
  { "space", &tre_isspace },
  { "upper", &tre_isupper },
  { "xdigit", &tre_isxdigit },
  { NULL, NULL}
};

tre_ctype_t tre_ctype(const char *name)
{
  int i;
  for (i = 0; tre_ctype_map[i].name != NULL; i++)
    {
      if (strcmp(name, tre_ctype_map[i].name) == 0)
	return tre_ctype_map[i].func;
    }
  return (tre_ctype_t)0;
}
#endif /* !TRE_WCHAR */

/* Maximum number of character classes that can occur in a negated bracket
   expression.  */
#define MAX_NEG_CLASSES 64

/* Maximum length of character class names. */
#define MAX_CLASS_NAME

static reg_errcode_t
tre_parse_bracket_items(tre_parse_ctx_t *ctx, int negate,
			tre_ctype_t neg_classes[], int *num_neg_classes,
			tre_ast_node_t ***items, int *num_items,
			int *items_size)
{
  const tre_char_t *re = ctx->re;
  reg_errcode_t status = REG_OK;
  tre_ctype_t class = (tre_ctype_t)0;
  int i = *num_items;
  int max_i = *items_size;
  int skip;

  /* Build an array of the items in the bracket expression. */
  while (status == REG_OK)
    {
      skip = 0;
      if (re == ctx->re_end)
	{
	  status = REG_EBRACK;
	}
      else if (*re == CHAR_RBRACKET && re > ctx->re)
	{
	  DPRINT(("tre_parse_bracket:   done: '%.*ls'\n",
		  ctx->re_end - re, re));
	  re++;
	  break;
	}
      else
	{
	  tre_cint_t min = 0, max = 0;

	  class = (tre_ctype_t)0;
	  if (re + 2 < ctx->re_end
	      && *(re + 1) == CHAR_MINUS && *(re + 2) != CHAR_RBRACKET)
	    {
	      DPRINT(("tre_parse_bracket:  range: '%.*ls'\n",
		      ctx->re_end - re, re));
	      min = *re;
	      max = *(re + 2);
	      re += 3;
	      /* XXX - Should use collation order instead of encoding values
		 in character ranges. */
	      if (min > max)
		status = REG_ERANGE;
	    }
	  else if (re + 1 < ctx->re_end
		   && *re == CHAR_LBRACKET && *(re + 1) == CHAR_PERIOD)
	    status = REG_ECOLLATE;
	  else if (re + 1 < ctx->re_end
		   && *re == CHAR_LBRACKET && *(re + 1) == CHAR_EQUAL)
	    status = REG_ECOLLATE;
	  else if (re + 1 < ctx->re_end
		   && *re == CHAR_LBRACKET && *(re + 1) == CHAR_COLON)
	    {
	      char tmp_str[64];
	      const tre_char_t *endptr = re + 2;
	      int len;
	      DPRINT(("tre_parse_bracket:  class: '%.*ls'\n",
		      ctx->re_end - re, re));
	      while (endptr < ctx->re_end && *endptr != CHAR_COLON)
		endptr++;
	      if (endptr != ctx->re_end)
		{
		  len = MIN(endptr - re - 2, 63);
#ifdef TRE_WCHAR
		  {
		    tre_char_t tmp_wcs[64];
		    wcsncpy(tmp_wcs, re + 2, len);
		    tmp_wcs[len] = L'\0';
#if defined HAVE_WCSRTOMBS
		    {
		      mbstate_t state;
		      const tre_char_t *src = tmp_wcs;
		      memset(&state, '\0', sizeof(state));
		      len = wcsrtombs(tmp_str, &src, 63, &state);
		    }
#elif defined HAVE_WCSTOMBS
		    len = wcstombs(tmp_str, tmp_wcs, 63);
#endif /* defined HAVE_WCSTOMBS */
		  }
#else /* !TRE_WCHAR */
		  strncpy(tmp_str, re + 2, len);
#endif /* !TRE_WCHAR */
		  tmp_str[len] = '\0';
		  DPRINT(("  class name: %s\n", tmp_str));
		  class = tre_ctype(tmp_str);
		  if (!class)
		    status = REG_ECTYPE;
		  /* Optimize character classes for 8 bit character sets. */
		  if (status == REG_OK && TRE_MB_CUR_MAX == 1)
		    {
		      status = tre_expand_ctype(ctx->mem, class, items,
						&i, &max_i, ctx->cflags);
		      class = (tre_ctype_t)0;
		      skip = 1;
		    }
		  re = endptr + 2;
		}
	      else
		status = REG_ECTYPE;
	      min = 0;
	      max = TRE_CHAR_MAX;
	    }
	  else
	    {
	      DPRINT(("tre_parse_bracket:   char: '%.*ls'\n",
		      ctx->re_end - re, re));
	      if (*re == CHAR_MINUS && *(re + 1) != CHAR_RBRACKET
		  && ctx->re != re)
		/* Two ranges are not allowed to share and endpoint. */
		status = REG_ERANGE;
	      min = max = *re++;
	    }

	  if (status != REG_OK)
	    break;

	  if (class && negate)
	    if (*num_neg_classes >= MAX_NEG_CLASSES)
	      status = REG_ESPACE;
	    else
	      neg_classes[(*num_neg_classes)++] = class;
	  else if (!skip)
	    {
	      status = tre_new_item(ctx->mem, min, max, &i, &max_i, items);
	      if (status != REG_OK)
		break;
	      ((tre_literal_t*)((*items)[i-1])->obj)->u.class = class;
	    }

	  /* Add opposite-case counterpoints if REG_ICASE is present.
	     This is broken if there are more than two "same" characters. */
	  if (ctx->cflags & REG_ICASE && !class && status == REG_OK && !skip)
	    {
	      int cmin, ccurr;

	      DPRINT(("adding opposite-case counterpoints\n"));
	      while (min <= max)
		{
		  if (tre_islower(min))
		    {
		      cmin = ccurr = tre_toupper(min++);
		      while (tre_islower(min) && tre_toupper(min) == ccurr + 1
			     && min <= max)
			ccurr = tre_toupper(min++);
		      status = tre_new_item(ctx->mem, cmin, ccurr,
					    &i, &max_i, items);
		    }
		  else if (tre_isupper(min))
		    {
		      cmin = ccurr = tre_tolower(min++);
		      while (tre_isupper(min) && tre_tolower(min) == ccurr + 1
			     && min <= max)
			ccurr = tre_tolower(min++);
		      status = tre_new_item(ctx->mem, cmin, ccurr,
					    &i, &max_i, items);
		    }
		  else min++;
		  if (status != REG_OK)
		    break;
		}
	      if (status != REG_OK)
		break;
	    }
	}
    }
  *num_items = i;
  *items_size = max_i;
  ctx->re = re;
  return status;
}

static reg_errcode_t
tre_parse_bracket(tre_parse_ctx_t *ctx, tre_ast_node_t **result)
{
  tre_ast_node_t *node = NULL;
  int negate = 0;
  reg_errcode_t status = REG_OK;
  tre_ast_node_t **items, *u, *n;
  int i = 0, j, max_i = 32, curr_max, curr_min;
  tre_ctype_t neg_classes[MAX_NEG_CLASSES];
  int num_neg_classes = 0;

  /* Start off with an array of `max_i' elements. */
  items = xmalloc(sizeof(*items) * max_i);
  if (items == NULL)
    return REG_ESPACE;

  if (*ctx->re == CHAR_CARET)
    {
      DPRINT(("tre_parse_bracket: negate: '%.*ls'\n",
	      ctx->re_end - ctx->re, ctx->re));
      negate = 1;
      ctx->re++;
    }

  status = tre_parse_bracket_items(ctx, negate, neg_classes, &num_neg_classes,
				   &items, &i, &max_i);

  if (status != REG_OK)
    goto parse_bracket_done;

  /* Sort the array if we need to negate it. */
  if (negate)
    qsort(items, i, sizeof(*items), tre_compare_items);

  curr_max = curr_min = 0;
  /* Build a union of the items in the array, negated if necessary. */
  for (j = 0; j < i && status == REG_OK; j++)
    {
      int min, max;
      tre_literal_t *l = items[j]->obj;
      min = l->code_min;
      max = l->code_max;

      DPRINT(("item: %d - %d, class %ld, curr_max = %d\n",
	      (int)l->code_min, (int)l->code_max, (long)l->u.class, curr_max));

      if (negate)
	{
	  if (min < curr_max)
	    {
	      /* Overlap. */
	      curr_max = MAX(max + 1, curr_max);
	      DPRINT(("overlap, curr_max = %d\n", curr_max));
	      l = NULL;
	    }
	  else
	    {
	      /* No overlap. */
	      curr_max = min - 1;
	      if (curr_max >= curr_min)
		{
		  DPRINT(("no overlap\n"));
		  l->code_min = curr_min;
		  l->code_max = curr_max;
		}
	      else
		{
		  DPRINT(("no overlap, zero room\n"));
		  l = NULL;
		}
	      curr_min = curr_max = max + 1;
	    }
	}

      if (l != NULL)
	{
	  int k;
	  DPRINT(("creating %d - %d\n", (int)l->code_min, (int)l->code_max));
	  l->position = ctx->position;
	  if (num_neg_classes > 0)
	    {
	      l->neg_classes = tre_mem_alloc(ctx->mem,
					     (sizeof(l->neg_classes)
					      * (num_neg_classes + 1)));
	      if (l->neg_classes == NULL)
		{
		  status = REG_ESPACE;
		  break;
		}
	      for (k = 0; k < num_neg_classes; k++)
		l->neg_classes[k] = neg_classes[k];
	      l->neg_classes[k] = (tre_ctype_t)0;
	    }
	  else
	    l->neg_classes = NULL;
	  if (node == NULL)
	    node = items[j];
	  else
	    {
	      u = tre_ast_new_union(ctx->mem, node, items[j]);
	      if (u == NULL)
		status = REG_ESPACE;
	      node = u;
	    }
	}
    }

  if (status != REG_OK)
    goto parse_bracket_done;

  if (negate)
    {
      int k;
      DPRINT(("final: creating %d - %d\n", curr_min, (int)TRE_CHAR_MAX));
      n = tre_ast_new_literal(ctx->mem, curr_min, TRE_CHAR_MAX, ctx->position);
      if (n == NULL)
	status = REG_ESPACE;
      else
	{
	  tre_literal_t *l = n->obj;
	  if (num_neg_classes > 0)
	    {
	      l->neg_classes = tre_mem_alloc(ctx->mem,
					     (sizeof(l->neg_classes)
					      * (num_neg_classes + 1)));
	      if (l->neg_classes == NULL)
		{
		  status = REG_ESPACE;
		  goto parse_bracket_done;
		}
	      for (k = 0; k < num_neg_classes; k++)
		l->neg_classes[k] = neg_classes[k];
	      l->neg_classes[k] = (tre_ctype_t)0;
	    }
	  else
	    l->neg_classes = NULL;
	  if (node == NULL)
	    node = n;
	  else
	    {
	      u = tre_ast_new_union(ctx->mem, node, n);
	      if (u == NULL)
		status = REG_ESPACE;
	      node = u;
	    }
	}
    }

  if (status != REG_OK)
    goto parse_bracket_done;

#ifdef TRE_DEBUG
  tre_ast_print(node);
#endif /* TRE_DEBUG */

 parse_bracket_done:
  xfree(items);
  ctx->position++;
  *result = node;
  return status;
}


/* Parses a positive decimal integer.  Returns -1 if the string does not
   contain a valid number. */
static int
tre_parse_int(const tre_char_t **regex, const tre_char_t *regex_end)
{
  int num = -1;
  const tre_char_t *r = *regex;
  while (r < regex_end && *r >= L'0' && *r <= L'9')
    {
      if (num < 0)
	num = 0;
      num = num * 10 + *r - L'0';
      r++;
    }
  *regex = r;
  return num;
}


static reg_errcode_t
tre_parse_bound(tre_parse_ctx_t *ctx, tre_ast_node_t **result)
{
  int min, max, i;
  int cost_ins, cost_del, cost_subst, cost_max;
  int limit_ins, limit_del, limit_subst, limit_err;
  const tre_char_t *r = ctx->re;
  const tre_char_t *start;
  int minimal = 0;
  int approx = 0;
  int costs_set = 0;
  int counts_set = 0;

  cost_ins = cost_del = cost_subst = cost_max = TRE_PARAM_UNSET;
  limit_ins = limit_del = limit_subst = limit_err = TRE_PARAM_UNSET;

  /* Parse number (minimum repetition count). */
  min = -1;
  if (r < ctx->re_end && *r >= L'0' && *r <= L'9') {
    DPRINT(("tre_parse:   min count: '%.*ls'\n", ctx->re_end - r, r));
    min = tre_parse_int(&r, ctx->re_end);
  }

  /* Parse comma and second number (maximum repetition count). */
  max = min;
  if (r < ctx->re_end && *r == CHAR_COMMA)
    {
      r++;
      DPRINT(("tre_parse:   max count: '%.*ls'\n", ctx->re_end - r, r));
      max = tre_parse_int(&r, ctx->re_end);
    }

  /* Check that the repeat counts are sane. */
  if ((max >= 0 && min > max) || max > RE_DUP_MAX)
    return REG_BADBR;


  /*
   '{'
     optionally followed immediately by a number == minimum repcount
     optionally followed by , then a number == maximum repcount
      + then a number == maximum insertion count
      - then a number == maximum deletion count
      # then a number == maximum substitution count
      ~ then a number == maximum number of errors
      Any of +, -, # or ~ without followed by a number means that
      the maximum count/number of errors is infinite.

      An equation of the form
        Xi + Yd + Zs < C
      can be specified to set costs and the cost limit to a value
      different from the default value:
        - X is the cost of an insertion
        - Y is the cost of a deletion
        - Z is the cost of a substitution
        - C is the maximum cost

      If no count limit or cost is set for an operation, the operation
      is not allowed at all.
  */


  do {
    int done;
    start = r;

    /* Parse count limit settings */
    done = 0;
    if (!counts_set)
      while (r + 1 < ctx->re_end && !done)
	{
	  switch (*r)
	    {
	    case CHAR_PLUS:  /* Insert limit */
	      DPRINT(("tre_parse:   ins limit: '%.*ls'\n", ctx->re_end - r, r));
	      r++;
	      limit_ins = tre_parse_int(&r, ctx->re_end);
	      if (limit_ins < 0)
		limit_ins = INT_MAX;
	      counts_set = 1;
	      break;
	    case CHAR_MINUS: /* Delete limit */
	      DPRINT(("tre_parse:   del limit: '%.*ls'\n", ctx->re_end - r, r));
	      r++;
	      limit_del = tre_parse_int(&r, ctx->re_end);
	      if (limit_del < 0)
		limit_del = INT_MAX;
	      counts_set = 1;
	      break;
	    case CHAR_HASH:  /* Substitute limit */
	      DPRINT(("tre_parse: subst limit: '%.*ls'\n", ctx->re_end - r, r));
	      r++;
	      limit_subst = tre_parse_int(&r, ctx->re_end);
	      if (limit_subst < 0)
		limit_subst = INT_MAX;
	      counts_set = 1;
	      break;
	    case CHAR_TILDE: /* Maximum number of changes */
	      DPRINT(("tre_parse: count limit: '%.*ls'\n", ctx->re_end - r, r));
	      r++;
	      limit_err = tre_parse_int(&r, ctx->re_end);
	      if (limit_err < 0)
		limit_err = INT_MAX;
	      approx = 1;
	      break;
	    case CHAR_COMMA:
	      r++;
	      break;
	    case L' ':
	      r++;
	      break;
	    case L'}':
	      done = 1;
	      break;
	    default:
	      done = 1;
	      break;
	    }
	}

    /* Parse cost restriction equation. */
    done = 0;
    if (!costs_set)
      while (r + 1 < ctx->re_end && !done)
	{
	  switch (*r)
	    {
	    case CHAR_PLUS:
	    case L' ':
	      r++;
	      break;
	    case L'<':
	      DPRINT(("tre_parse:    max cost: '%.*ls'\n", ctx->re_end - r, r));
	      r++;
	      while (*r == L' ')
		r++;
	      cost_max = tre_parse_int(&r, ctx->re_end);
	      if (cost_max < 0)
		cost_max = INT_MAX;
	      else
		cost_max--;
	      approx = 1;
	      break;
	    case CHAR_COMMA:
	      r++;
	      done = 1;
	      DPRINT(("comma\n"));
	      break;
	    default:
	      if (*r >= L'0' && *r <= L'9')
		{
#ifdef TRE_DEBUG
		  const tre_char_t *sr = r;
#endif /* TRE_DEBUG */
		  int cost = tre_parse_int(&r, ctx->re_end);
		  /* XXX - make sure r is not past end. */
		  switch (*r)
		    {
		    case L'i':  /* Insert cost */
		      DPRINT(("tre_parse:    ins cost: '%.*ls'\n",
			      ctx->re_end - sr, sr));
		      r++;
		      cost_ins = cost;
		      costs_set = 1;
		      break;
		    case L'd':  /* Delete cost */
		      DPRINT(("tre_parse:    del cost: '%.*ls'\n",
			      ctx->re_end - sr, sr));
		      r++;
		      cost_del = cost;
		      costs_set = 1;
		      break;
		    case L's':  /* Substitute cost */
		      DPRINT(("tre_parse:  subst cost: '%.*ls'\n",
			      ctx->re_end - sr, sr));
		      r++;
		      cost_subst = cost;
		      costs_set = 1;
		      break;
		    default:
		      return REG_BADBR;
		    }
		}
	      else
		{
		  done = 1;
		  break;
		}
	    }
	}
  } while (start != r);

  /* Missing }. */
  if (r >= ctx->re_end)
    return REG_EBRACE;

  /* Empty contents of {}. */
  if (r == ctx->re)
    return REG_BADBR;

  /* Parse the ending '}' or '\}'.*/
  if (ctx->cflags & REG_EXTENDED)
    {
      if (r >= ctx->re_end || *r != CHAR_RBRACE)
	return REG_BADBR;
      r++;
    }
  else
    {
      if (r + 1 >= ctx->re_end
	  || *r != CHAR_BACKSLASH
	  || *(r + 1) != CHAR_RBRACE)
	return REG_BADBR;
      r += 2;
    }


  /* Parse trailing '?' marking minimal repetition. */
  if (r < ctx->re_end && *r == CHAR_QUESTIONMARK)
    {
      minimal = 1;
      r++;
    }

  /* Create the AST node(s). */
  if (min == 0 && max == 0)
    {
      *result = tre_ast_new_literal(ctx->mem, EMPTY, -1, -1);
      if (*result == NULL)
	return REG_ESPACE;
    }
  else
    {
      if (min < 0 && max < 0)
	/* Only approximate parameters set, no repetitions. */
	min = max = 1;

      *result = tre_ast_new_iter(ctx->mem, *result, min, max, minimal);
      if (!*result)
	return REG_ESPACE;

      /* If approximate matching parameters are set, add them to the
	 iteration node. */
      if (approx || costs_set || counts_set)
	{
	  unsigned int *params;
	  tre_iteration_t *iter = (*result)->obj;

	  if (costs_set || counts_set)
	    {
	      if (limit_ins == TRE_PARAM_UNSET)
		{
		  if (cost_ins == TRE_PARAM_UNSET)
		    limit_ins = 0;
		  else
		    limit_ins = INT_MAX;
		}

	      if (limit_del == TRE_PARAM_UNSET)
		{
		  if (cost_del == TRE_PARAM_UNSET)
		    limit_del = 0;
		  else
		    limit_del = INT_MAX;
		}

	      if (limit_subst == TRE_PARAM_UNSET)
		{
		  if (cost_subst == TRE_PARAM_UNSET)
		    limit_subst = 0;
		  else
		    limit_subst = INT_MAX;
		}
	    }

	  if (cost_max == TRE_PARAM_UNSET)
	    cost_max = INT_MAX;
	  if (limit_err == TRE_PARAM_UNSET)
	    limit_err = INT_MAX;

	  ctx->have_approx = 1;
	  params = tre_mem_alloc(ctx->mem, sizeof(*params) * TRE_PARAM_LAST);
	  if (!params)
	    return REG_ESPACE;
	  for (i = 0; i < TRE_PARAM_LAST; i++)
	    params[i] = TRE_PARAM_UNSET;
	  params[TRE_PARAM_COST_INS] = cost_ins;
	  params[TRE_PARAM_COST_DEL] = cost_del;
	  params[TRE_PARAM_COST_SUBST] = cost_subst;
	  params[TRE_PARAM_COST_MAX] = cost_max;
	  params[TRE_PARAM_MAX_INS] = limit_ins;
	  params[TRE_PARAM_MAX_DEL] = limit_del;
	  params[TRE_PARAM_MAX_SUBST] = limit_subst;
	  params[TRE_PARAM_MAX_ERR] = limit_err;
	  iter->params = params;
	}
    }

  DPRINT(("tre_parse_bound: min %d, max %d, costs [%d,%d,%d, total %d], "
	  "limits [%d,%d,%d, total %d]\n",
	  min, max, cost_ins, cost_del, cost_subst, cost_max,
	  limit_ins, limit_del, limit_subst, limit_err));


  ctx->re = r;
  return REG_OK;
}

typedef enum {
  PARSE_RE = 0,
  PARSE_ATOM,
  PARSE_MARK_FOR_SUBMATCH,
  PARSE_BRANCH,
  PARSE_PIECE,
  PARSE_CATENATION,
  PARSE_POST_CATENATION,
  PARSE_UNION,
  PARSE_POST_UNION,
  PARSE_POSTFIX
} tre_parse_re_stack_symbol_t;


/* Parses a wide character regexp pattern into a syntax tree.  This parser
   handles both syntaxes (BRE and ERE), including the TRE extensions. */
static reg_errcode_t
tre_parse(tre_parse_ctx_t *ctx)
{
  tre_ast_node_t *result = NULL;
  tre_parse_re_stack_symbol_t symbol;
  reg_errcode_t status = REG_OK;
  tre_stack_t *stack = ctx->stack;
  int bottom = tre_stack_num_objects(stack);
  int depth = 0;

  DPRINT(("tre_parse: parsing '%.*ls', len = %d\n",
	  ctx->len, ctx->re, ctx->len));

  if (!ctx->nofirstsub)
    {
      STACK_PUSH(stack, ctx->re);
      STACK_PUSH(stack, ctx->submatch_id);
      STACK_PUSH(stack, PARSE_MARK_FOR_SUBMATCH);
      ctx->submatch_id++;
    }
  STACK_PUSH(stack, PARSE_RE);
  ctx->re_start = ctx->re;
  ctx->re_end = ctx->re + ctx->len;


  /* The following is basically just a recursive descent parser.  I use
     an explicit stack instead of recursive functions mostly because of
     two reasons: compatibility with systems which have an overflowable
     call stack, and efficiency (both in lines of code and speed).  */
  while (tre_stack_num_objects(stack) > bottom && status == REG_OK)
    {
      if (status != REG_OK)
	break;
      symbol = (tre_parse_re_stack_symbol_t)tre_stack_pop(stack);
      switch (symbol)
	{
	case PARSE_RE:
	  /* Parse a full regexp.  A regexp is one or more branches,
	     separated by the union operator `|'. */
#ifdef REG_LITERAL
	  if (!(ctx->cflags & REG_LITERAL)
	      && ctx->cflags & REG_EXTENDED)
#endif /* REG_LITERAL */
	    STACK_PUSHX(stack, PARSE_UNION);
	  STACK_PUSHX(stack, PARSE_BRANCH);
	  break;

	case PARSE_BRANCH:
	  /* Parse a branch.  A branch is one or more pieces, concatenated.
             A piece is an atom possibly followed by a postfix operator. */
	  STACK_PUSHX(stack, PARSE_CATENATION);
	  STACK_PUSHX(stack, PARSE_PIECE);
	  break;

	case PARSE_PIECE:
	  /* Parse a piece.  A piece is an atom possibly followed by one
	     or more postfix operators. */
#ifdef REG_LITERAL
	  if (!(ctx->cflags & REG_LITERAL))
#endif /* REG_LITERAL */
	    STACK_PUSHX(stack, PARSE_POSTFIX);
	  STACK_PUSHX(stack, PARSE_ATOM);
	  break;

	case PARSE_CATENATION:
	  /* If the expression has not ended, parse another piece. */
	  {
	    tre_char_t c;
	    if (ctx->re >= ctx->re_end)
	      break;
	    c = *ctx->re;
#ifdef REG_LITERAL
	    if (!(ctx->cflags & REG_LITERAL))
	      {
#endif /* REG_LITERAL */
		if (ctx->cflags & REG_EXTENDED && c == CHAR_PIPE)
		  break;
		if ((ctx->cflags & REG_EXTENDED
		     && c == CHAR_RPAREN && depth > 0)
		    || (!(ctx->cflags & REG_EXTENDED)
			&& (c == CHAR_BACKSLASH
			    && *(ctx->re + 1) == CHAR_RPAREN)))
		  {
		    if (!(ctx->cflags & REG_EXTENDED) && depth == 0)
		      status = REG_EPAREN;
		    DPRINT(("tre_parse:   group end: '%.*ls'\n",
			    ctx->re_end - ctx->re, ctx->re));
		    depth--;
		    if (!(ctx->cflags & REG_EXTENDED))
		      ctx->re += 2;
		    break;
		  }
#ifdef REG_LITERAL
	      }
#endif /* REG_LITERAL */

	    STACK_PUSHX(stack, PARSE_CATENATION);
	    STACK_PUSHX(stack, result);
	    STACK_PUSHX(stack, PARSE_POST_CATENATION);
	    STACK_PUSHX(stack, PARSE_PIECE);
	    break;
	  }

	case PARSE_POST_CATENATION:
	  {
	    tre_ast_node_t *tree = tre_stack_pop(stack);
	    tre_ast_node_t *tmp_node;
	    tmp_node = tre_ast_new_catenation(ctx->mem, tree, result);
	    if (!tmp_node)
	      return REG_ESPACE;
	    result = tmp_node;
	    break;
	  }

	case PARSE_UNION:
	  if (ctx->re >= ctx->re_end)
	    break;
	  switch (*ctx->re)
	    {
	    case CHAR_PIPE:
	      DPRINT(("tre_parse:       union: '%.*ls'\n",
		      ctx->re_end - ctx->re, ctx->re));
	      STACK_PUSHX(stack, PARSE_UNION);
	      STACK_PUSHX(stack, result);
	      STACK_PUSHX(stack, PARSE_POST_UNION);
	      STACK_PUSHX(stack, PARSE_BRANCH);
	      ctx->re++;
	      break;

	    case CHAR_RPAREN:
	      ctx->re++;
	      break;

	    default:
	      break;
	    }
	  break;

	case PARSE_POST_UNION:
	  {
	    tre_ast_node_t *tmp_node;
	    tre_ast_node_t *tree = tre_stack_pop(stack);
	    tmp_node = tre_ast_new_union(ctx->mem, tree, result);
	    if (!tmp_node)
	      return REG_ESPACE;
	    result = tmp_node;
	    break;
	  }

	case PARSE_POSTFIX:
	  /* Parse postfix operators. */
	  if (ctx->re >= ctx->re_end)
	    break;
	  switch (*ctx->re)
	    {
	    case CHAR_STAR:
	    case CHAR_PLUS:
	    case CHAR_QUESTIONMARK:
	      {
		tre_ast_node_t *tmp_node;
		int minimal = 0;
		int rep_min = 0;
		int rep_max = -1;
		if (*ctx->re == CHAR_PLUS)
		  rep_min = 1;
		if (*ctx->re == CHAR_QUESTIONMARK)
		  rep_max = 1;

		if (ctx->re + 1 < ctx->re_end
		    && *(ctx->re + 1) == CHAR_QUESTIONMARK)
		  minimal = 1;
		DPRINT(("tre_parse: %s star: '%.*ls'\n",
			minimal ? "  minimal" : "greedy",
			ctx->re_end - ctx->re, ctx->re));
		ctx->re += minimal + 1;
		tmp_node = tre_ast_new_iter(ctx->mem, result, rep_min, rep_max,
					    minimal);
		if (tmp_node == NULL)
		  return REG_ESPACE;
		result = tmp_node;
		STACK_PUSHX(stack, PARSE_POSTFIX);
		break;
	      }

	    case CHAR_BACKSLASH:
	      /* "\{" is special without REG_EXTENDED */
	      if (!(ctx->cflags & REG_EXTENDED)
		  && ctx->re + 1 < ctx->re_end
		  && *(ctx->re + 1) == CHAR_LBRACE)
		{
		  ctx->re++;
		  goto parse_brace;
		}
	      else
		break;

	    case CHAR_LBRACE:
	      /* "{" is literal without REG_EXTENDED */
	      if (!(ctx->cflags & REG_EXTENDED))
		break;

	    parse_brace:
	      DPRINT(("tre_parse:       bound: '%.*ls'\n",
		      ctx->re_end - ctx->re, ctx->re));
	      ctx->re++;

	      status = tre_parse_bound(ctx, &result);
	      if (status != REG_OK)
		return status;
	      STACK_PUSHX(stack, PARSE_POSTFIX);
	      break;
	    }
	  break;

	case PARSE_ATOM:
	  /* Parse an atom.  An atom is a regular expression enclosed in `()',
	     an empty set of `()', a bracket expression, `.', `^', `$',
	     a `\' followed by a character, or a single character. */

	  /* End of regexp? (empty string). */
	  if (ctx->re >= ctx->re_end)
	    goto parse_literal;

#ifdef REG_LITERAL
	  if (ctx->cflags & REG_LITERAL)
	    goto parse_literal;
#endif /* REG_LITERAL */

	  switch (*ctx->re)
	    {
	    case CHAR_LPAREN:  /* parenthesized subexpression */
	      if (ctx->cflags & REG_EXTENDED
		  || (ctx->re > ctx->re_start
		      && *(ctx->re - 1) == CHAR_BACKSLASH))
		{
		  depth++;
		  if (ctx->re + 2 < ctx->re_end
		      && *(ctx->re + 1) == CHAR_QUESTIONMARK
		      && *(ctx->re + 2) == CHAR_COLON)
		    {
		      DPRINT(("tre_parse: group begin: '%.*ls', no submatch\n",
			      ctx->re_end - ctx->re, ctx->re));
		      /* Don't mark for submatching. */
		      ctx->re += 3;
		      STACK_PUSHX(stack, PARSE_RE);
		    }
		  else
		    {
		      DPRINT(("tre_parse: group begin: '%.*ls', submatch %d\n",
			      ctx->re_end - ctx->re, ctx->re,
			      ctx->submatch_id));
		      ctx->re++;
		      /* First parse a whole RE, then mark the resulting tree
			 for submatching. */
		      STACK_PUSHX(stack, ctx->submatch_id);
		      STACK_PUSHX(stack, PARSE_MARK_FOR_SUBMATCH);
		      STACK_PUSHX(stack, PARSE_RE);
		      ctx->submatch_id++;
		    }
		}
	      else
		goto parse_literal;
	      break;

	    case CHAR_RPAREN:  /* end of current subexpression */
	      if ((ctx->cflags & REG_EXTENDED && depth > 0)
		  || (ctx->re > ctx->re_start
		      && *(ctx->re - 1) == CHAR_BACKSLASH))
		{
		  DPRINT(("tre_parse:       empty: '%.*ls'\n",
			  ctx->re_end - ctx->re, ctx->re));
		  /* We were expecting an atom, but instead the current
		     subexpression was closed.  POSIX leaves the meaning of
		     this to be implementation-defined.  We interpret this as
		     an empty expression (which matches an empty string).  */
		  result = tre_ast_new_literal(ctx->mem, EMPTY, -1, -1);
		  if (result == NULL)
		    return REG_ESPACE;
		  if (!(ctx->cflags & REG_EXTENDED))
		    ctx->re--;
		}
	      else
		goto parse_literal;
	      break;

	    case CHAR_LBRACKET: /* bracket expression */
	      DPRINT(("tre_parse:     bracket: '%.*ls'\n",
		      ctx->re_end - ctx->re, ctx->re));
	      ctx->re++;
	      status = tre_parse_bracket(ctx, &result);
	      if (status != REG_OK)
		return status;
	      break;

	    case CHAR_BACKSLASH:
	      /* If this is "\(" or "\)" chew off the backslash and
		 try again. */
	      if (!(ctx->cflags & REG_EXTENDED)
		  && ctx->re + 1 < ctx->re_end
		  && (*(ctx->re + 1) == CHAR_LPAREN
		      || *(ctx->re + 1) == CHAR_RPAREN))
		{
		  ctx->re++;
		  STACK_PUSHX(stack, PARSE_ATOM);
		  break;
		}

	      /* If a macro is used, parse the expanded macro recursively. */
	      {
		const tre_char_t *r;
		r = tre_expand_macro(ctx->re + 1, ctx->re_end);
		if (r)
		  {
		    tre_parse_ctx_t subctx;
		    memcpy(&subctx, ctx, sizeof(subctx));
		    subctx.re = r;
		    subctx.len = tre_strlen(r);
		    subctx.nofirstsub = 1;
		    status = tre_parse(&subctx);
		    if (status != REG_OK)
		      return status;
		    ctx->re += 2;
		    ctx->position += subctx.position;
		    result = subctx.result;
		    break;
		  }
	      }

	      ctx->re++;
	      if (ctx->re >= ctx->re_end)
		/* Trailing backslash. */
		return REG_EESCAPE;
	      switch (*ctx->re)
		{
		case L'b':
		  result = tre_ast_new_literal(ctx->mem, ASSERTION,
					       ASSERT_AT_WB, -1);
		  break;
		case L'B':
		  result = tre_ast_new_literal(ctx->mem, ASSERTION,
					       ASSERT_AT_WB_NEG, -1);
		  break;
		case L'<':
		  result = tre_ast_new_literal(ctx->mem, ASSERTION,
					       ASSERT_AT_BOW, -1);
		  break;
		case L'>':
		  result = tre_ast_new_literal(ctx->mem, ASSERTION,
					       ASSERT_AT_EOW, -1);
		  break;
		default:
		  if (tre_isdigit(*ctx->re))
		    {
		      /* Back reference. */
		      int val = *ctx->re - L'0';
		      DPRINT(("tre_parse:     backref: '%.*ls'\n",
			      ctx->re_end - ctx->re + 1, ctx->re - 1));
		      result = tre_ast_new_literal(ctx->mem, BACKREF, val,
						   ctx->position);
		      if (result == NULL)
			return REG_ESPACE;
		      ctx->position++;
		      ctx->have_backrefs = 1;
		    }
		  else
		    {
		      /* Escaped character. */
		      DPRINT(("tre_parse:     escaped: '%.*ls'\n",
			      ctx->re_end - ctx->re + 1, ctx->re - 1));
		      result = tre_ast_new_literal(ctx->mem, *ctx->re, *ctx->re,
						   ctx->position);
		      ctx->position++;
		    }
		  break;
		}
	      if (result == NULL)
		return REG_ESPACE;
	      ctx->re++;
	      break;

	    case CHAR_PERIOD:    /* the any-symbol */
	      DPRINT(("tre_parse:         any: '%.*ls'\n",
		      ctx->re_end - ctx->re, ctx->re));
	      if (ctx->cflags & REG_NEWLINE)
		{
		  tre_ast_node_t *tmp1;
		  tre_ast_node_t *tmp2;
		  tmp1 = tre_ast_new_literal(ctx->mem, 0, L'\n' - 1,
					     ctx->position);
		  if (!tmp1)
		    return REG_ESPACE;
		  tmp2 = tre_ast_new_literal(ctx->mem, L'\n' + 1, TRE_CHAR_MAX,
					     ctx->position + 1);
		  if (!tmp2)
		    return REG_ESPACE;
		  result = tre_ast_new_union(ctx->mem, tmp1, tmp2);
		  if (!result)
		    return REG_ESPACE;
		  ctx->position += 2;
		}
	      else
		{
		  result = tre_ast_new_literal(ctx->mem, 0, TRE_CHAR_MAX,
					       ctx->position);
		  if (!result)
		    return REG_ESPACE;
		  ctx->position++;
		}
	      ctx->re++;
	      break;

	    case CHAR_CARET:     /* beginning of line assertion */
	      /* '^' has a special meaning everywhere in EREs, and in the
		 beginning of the RE and after \( is BREs. */
	      if (ctx->cflags & REG_EXTENDED
		  || (ctx->re - 2 >= ctx->re_start
		      && *(ctx->re - 2) == CHAR_BACKSLASH
		      && *(ctx->re - 1) == CHAR_LPAREN)
		  || ctx->re == ctx->re_start)
		{
		  DPRINT(("tre_parse:         BOL: '%.*ls'\n",
			  ctx->re_end - ctx->re, ctx->re));
		  result = tre_ast_new_literal(ctx->mem, ASSERTION,
					       ASSERT_AT_BOL, -1);
		  if (result == NULL)
		    return REG_ESPACE;
		  ctx->re++;
		}
	      else
		goto parse_literal;
	      break;

	    case CHAR_DOLLAR:    /* end of line assertion. */
	      /* '$' is special everywhere in EREs, and in the end of the
		 string and before \) is BREs. */
	      if (ctx->cflags & REG_EXTENDED
		  || (ctx->re + 2 < ctx->re_end
		      && *(ctx->re + 1) == CHAR_BACKSLASH
		      && *(ctx->re + 2) == CHAR_RPAREN)
		  || ctx->re + 1 == ctx->re_end)
		{
		  DPRINT(("tre_parse:         EOL: '%.*ls'\n",
			  ctx->re_end - ctx->re, ctx->re));
		  result = tre_ast_new_literal(ctx->mem, ASSERTION,
					       ASSERT_AT_EOL, -1);
		  if (result == NULL)
		    return REG_ESPACE;
		  ctx->re++;
		}
	      else
		goto parse_literal;
	      break;

	    default:
	    parse_literal:
	      /* We are expecting an atom.  If the subexpression (or the whole
		 regexp ends here, we interpret it as an empty expression
		 (which matches an empty string).  */
	      if (
#ifdef REG_LITERAL
		  !(ctx->cflags & REG_LITERAL) &&
#endif /* REG_LITERAL */
		  (ctx->re >= ctx->re_end
		   || *ctx->re == CHAR_STAR
		   || (ctx->cflags & REG_EXTENDED
		       && (*ctx->re == CHAR_PIPE
			   || *ctx->re == CHAR_LBRACE
			   || *ctx->re == CHAR_PLUS
			   || *ctx->re == CHAR_QUESTIONMARK))
		   /* Test for "\)" in BRE mode. */
		   || (!(ctx->cflags & REG_EXTENDED)
		       && ctx->re + 1 < ctx->re_end
		       && *ctx->re == CHAR_BACKSLASH
		       && *(ctx->re + 1) == CHAR_LBRACE)))
		{
		  DPRINT(("tre_parse:       empty: '%.*ls'\n",
			  ctx->re_end - ctx->re, ctx->re));
		  result = tre_ast_new_literal(ctx->mem, EMPTY, -1, -1);
		  if (!result)
		    return REG_ESPACE;
		  break;
		}

	      DPRINT(("tre_parse:     literal: '%.*ls'\n",
		      ctx->re_end - ctx->re, ctx->re));
	      /* Note that we can't use an tre_isalpha() test here, since there
		 may be characters which are alphabetic but neither upper or
		 lower case. */
	      if (ctx->cflags & REG_ICASE
		  && (tre_isupper(*ctx->re) || tre_islower(*ctx->re)))
		{
		  tre_ast_node_t *tmp1;
		  tre_ast_node_t *tmp2;

		  /* XXX - Can there be more than one opposite-case
		     counterpoints for some character in some locale?  Or
		     more than two characters which all should be regarded
		     the same character if case is ignored?  If yes, there
		     does not seem to be a portable way to detect it.  I guess
		     that at least for multi-character collating elements there
		     could be several opposite-case counterpoints, but they
		     cannot be supported portably anyway. */
		  tmp1 = tre_ast_new_literal(ctx->mem, tre_toupper(*ctx->re),
					     tre_toupper(*ctx->re),
					     ctx->position);
		  if (!tmp1)
		    return REG_ESPACE;
		  tmp2 = tre_ast_new_literal(ctx->mem, tre_tolower(*ctx->re),
					     tre_tolower(*ctx->re),
					     ctx->position);
		  if (!tmp2)
		    return REG_ESPACE;
		  result = tre_ast_new_union(ctx->mem, tmp1, tmp2);
		  if (!result)
		    return REG_ESPACE;
		}
	      else
		{
		  result = tre_ast_new_literal(ctx->mem, *ctx->re, *ctx->re,
					       ctx->position);
		  if (!result)
		    return REG_ESPACE;
		}
	      ctx->position++;
	      ctx->re++;
	      break;
	    }
	  break;

	case PARSE_MARK_FOR_SUBMATCH:
	  {
	    int submatch_id = (int)tre_stack_pop(stack);

	    if (result->submatch_id >= 0)
	      {
		tre_ast_node_t *n, *tmp_node;
		n = tre_ast_new_literal(ctx->mem, EMPTY, -1, -1);
		if (n == NULL)
		  return REG_ESPACE;
		tmp_node = tre_ast_new_catenation(ctx->mem, n, result);
		if (tmp_node == NULL)
		  return REG_ESPACE;
		tmp_node->num_submatches = result->num_submatches;
		result = tmp_node;
	      }
	    result->submatch_id = submatch_id;
	    result->num_submatches++;
	    break;
	  }
	}
    }

  /* Check for missing closing parentheses. */
  if (depth > 0)
    return REG_EPAREN;


  if (status == REG_OK)
    ctx->result = result;

  return status;
}



/*
   POSIX.2 and wide character API
*/

#define ERROR_EXIT(err)           \
  do                              \
    {                             \
      errcode = err;              \
      if (1) goto error_exit;     \
    }                             \
 while (0)


int
tre_compile(regex_t *preg, const tre_char_t *regex, size_t n, int cflags)
{
  tre_stack_t *stack;
  tre_ast_node_t *tree, *tmp_ast_l, *tmp_ast_r;
  tre_pos_and_tags_t *p;
  int *counts = NULL, *offs = NULL;
  int i, add = 0;
  tre_tnfa_transition_t *transitions, *initial;
  tre_tnfa_t *tnfa = NULL;
  tre_submatch_data_t *submatch_data;
  tre_tag_direction_t *tag_directions = NULL;
  reg_errcode_t errcode;
  tre_mem_t mem;

  /* Parse context. */
  tre_parse_ctx_t parse_ctx;

  /* Allocate a stack used throughout the compilation process for various
     purposes. */
  stack = tre_stack_new(512, 10240, 128);
  if (!stack)
    return REG_ESPACE;
  /* Allocate a fast memory allocator. */
  mem = tre_mem_new();
  if (!mem)
    {
      tre_stack_destroy(stack);
      return REG_ESPACE;
    }

  /* Parse the regexp. */
  memset(&parse_ctx, 0, sizeof(parse_ctx));
  parse_ctx.mem = mem;
  parse_ctx.stack = stack;
  parse_ctx.re = regex;
  parse_ctx.len = n;
  parse_ctx.cflags = cflags;
  DPRINT(("tre_compile: parsing '%.*ls'\n", n, regex));
  errcode = tre_parse(&parse_ctx);
  if (errcode != REG_OK)
    ERROR_EXIT(errcode);
  preg->re_nsub = parse_ctx.submatch_id - 1;
  tree = parse_ctx.result;

  /* Back references and approximate matching cannot currently be used
     in the same regexp. */
  if (parse_ctx.have_backrefs && parse_ctx.have_approx)
    ERROR_EXIT(REG_BADPAT);

#ifdef TRE_DEBUG
  tre_ast_print(tree);
#endif /* TRE_DEBUG */

  /* Allocate the TNFA struct. */
  tnfa = xcalloc(1, sizeof(tre_tnfa_t));
  if (tnfa == NULL)
    ERROR_EXIT(REG_ESPACE);
  tnfa->have_backrefs = parse_ctx.have_backrefs;
  tnfa->have_approx = parse_ctx.have_approx;
  tnfa->num_submatches = parse_ctx.submatch_id;

  /* Set up tags for submatch addressing.  If REG_NOSUB is set and the
     regexp does not have back references, this can be skipped. */
  if (tnfa->have_backrefs || !(cflags & REG_NOSUB))
    {
      DPRINT(("tre_compile: setting up tags\n"));

      /* Figure out how many tags we will need. */
      errcode = tre_add_tags(NULL, stack, tree, tnfa);
      if (errcode != REG_OK)
	ERROR_EXIT(errcode);
#ifdef TRE_DEBUG
      tre_ast_print(tree);
#endif /* TRE_DEBUG */

      if (tnfa->num_tags > 0)
	{
	  tag_directions = xmalloc(sizeof(*tag_directions)
				   * (tnfa->num_tags + 1));
	  if (tag_directions == NULL)
	    ERROR_EXIT(REG_ESPACE);
	  tnfa->tag_directions = tag_directions;
	  memset(tag_directions, -1,
		 sizeof(*tag_directions) * (tnfa->num_tags + 1));
	}
      tnfa->minimal_tags = xcalloc(tnfa->num_tags * 2 + 1,
				   sizeof(tnfa->minimal_tags));
      if (tnfa->minimal_tags == NULL)
	ERROR_EXIT(REG_ESPACE);

      submatch_data = xcalloc(parse_ctx.submatch_id, sizeof(*submatch_data));
      if (submatch_data == NULL)
	ERROR_EXIT(REG_ESPACE);
      tnfa->submatch_data = submatch_data;

      errcode = tre_add_tags(mem, stack, tree, tnfa);
      if (errcode != REG_OK)
	ERROR_EXIT(errcode);

#ifdef TRE_DEBUG
      for (i = 0; i < parse_ctx.submatch_id; i++)
	DPRINT(("pmatch[%d] = {t%d, t%d}\n",
		i, submatch_data[i].so_tag, submatch_data[i].eo_tag));
      for (i = 0; i < tnfa->num_tags; i++)
	DPRINT(("t%d is %s\n", i,
		tag_directions[i] == TRE_TAG_MINIMIZE ?
		"minimized" : "maximized"));
#endif /* TRE_DEBUG */
    }

  /* Expand iteration nodes. */
  errcode = tre_expand_ast(mem, stack, tree, &parse_ctx.position,
			   tag_directions, &tnfa->params_depth);
  if (errcode != REG_OK)
    ERROR_EXIT(errcode);

  /* Add a dummy node for the final state.
     XXX - For certain patterns this dummy node can be optimized away,
           for example "a*" or "ab*".   Figure out a simple way to detect
           this possibility. */
  tmp_ast_l = tree;
  tmp_ast_r = tre_ast_new_literal(mem, 0, 0, parse_ctx.position++);
  if (tmp_ast_r == NULL)
    ERROR_EXIT(REG_ESPACE);

  tree = tre_ast_new_catenation(mem, tmp_ast_l, tmp_ast_r);
  if (tree == NULL)
    ERROR_EXIT(REG_ESPACE);

#ifdef TRE_DEBUG
  tre_ast_print(tree);
#endif /* TRE_DEBUG */

  errcode = tre_compute_nfl(mem, stack, tree);
  if (errcode != REG_OK)
    ERROR_EXIT(errcode);

  counts = xmalloc(sizeof(int) * parse_ctx.position);
  if (counts == NULL)
    ERROR_EXIT(REG_ESPACE);

  offs = xmalloc(sizeof(int) * parse_ctx.position);
  if (offs == NULL)
    ERROR_EXIT(REG_ESPACE);

  for (i = 0; i < parse_ctx.position; i++)
    counts[i] = 0;
  tre_ast_to_tnfa(tree, NULL, counts, NULL);

  add = 0;
  for (i = 0; i < parse_ctx.position; i++)
    {
      offs[i] = add;
      add += counts[i] + 1;
      counts[i] = 0;
    }
  transitions = xcalloc(add + 1, sizeof(*transitions));
  if (transitions == NULL)
    ERROR_EXIT(REG_ESPACE);
  tnfa->transitions = transitions;
  tnfa->num_transitions = add;

  DPRINT(("Converting to TNFA:\n"));
  errcode = tre_ast_to_tnfa(tree, transitions, counts, offs);
  if (errcode != REG_OK)
    ERROR_EXIT(errcode);

  /* If in eight bit mode, compute a table of characters that can be the
     first character of a match. */
  tnfa->first_char = -1;
  if (TRE_MB_CUR_MAX == 1 && !tmp_ast_l->nullable)
    {
      int count = 0;
      int k;
      DPRINT(("Characters that can start a match:"));
      tnfa->firstpos_chars = xcalloc(256, sizeof(char));
      if (tnfa->firstpos_chars == NULL)
	ERROR_EXIT(REG_ESPACE);
      for (p = tree->firstpos; p->position >= 0; p++)
	{
	  tre_tnfa_transition_t *j = transitions + offs[p->position];
	  while (j->state != NULL)
	    {
	      for (k = j->code_min; k <= j->code_max && k < 256; k++)
		{
		  DPRINT((" %d", k));
		  tnfa->firstpos_chars[k] = 1;
		  count++;
		}
	      j++;
	    }
	}
      DPRINT(("\n"));
#if TRE_OPTIMIZE_FIRST_CHAR
      if (count == 1)
	{
	  for (k = 0; k < 256; k++)
	    if (tnfa->firstpos_chars[k])
	      {
		DPRINT(("first char must be %d\n", k));
		tnfa->first_char = k;
		xfree(tnfa->firstpos_chars);
		tnfa->firstpos_chars = NULL;
		break;
	      }
	}
#endif

    }
  else
    tnfa->firstpos_chars = NULL;


  p = tree->firstpos;
  i = 0;
  while (p->position >= 0)
    {
      i++;

#ifdef TRE_DEBUG
      {
	int *tags;
	DPRINT(("initial: %d", p->position));
	tags = p->tags;
	if (tags != NULL)
	  {
	    if (*tags >= 0)
	      DPRINT(("/"));
	    while (*tags >= 0)
	      {
		DPRINT(("%d", *tags));
		tags++;
		if (*tags >= 0)
		  DPRINT((","));
	      }
	  }
	DPRINT((", assert %d", p->assertions));
	if (p->params)
	  {
	    DPRINT((", "));
	    tre_print_params(p->params);
	  }
	DPRINT(("\n"));
      }
#endif /* TRE_DEBUG */

      p++;
    }

  initial = xcalloc(i + 1, sizeof(tre_tnfa_transition_t));
  if (initial == NULL)
    ERROR_EXIT(REG_ESPACE);
  tnfa->initial = initial;

  i = 0;
  for (p = tree->firstpos; p->position >= 0; p++)
    {
      initial[i].state = transitions + offs[p->position];
      initial[i].state_id = p->position;
      initial[i].tags = NULL;
      /* Copy the arrays p->tags, and p->params, they are allocated
	 from a tre_mem object. */
      if (p->tags)
	{
	  int j;
	  for (j = 0; p->tags[j] >= 0; j++);
	  initial[i].tags = xmalloc(sizeof(*p->tags) * (j + 1));
	  if (!initial[i].tags)
	    ERROR_EXIT(REG_ESPACE);
	  memcpy(initial[i].tags, p->tags, sizeof(*p->tags) * (j + 1));
	}
      initial[i].params = NULL;
      if (p->params)
	{
	  initial[i].params = xmalloc(sizeof(*p->params) * TRE_PARAM_LAST);
	  if (!initial[i].params)
	    ERROR_EXIT(REG_ESPACE);
	  memcpy(initial[i].params, p->params,
		 sizeof(*p->params) * TRE_PARAM_LAST);
	}
      initial[i].assertions = p->assertions;
      i++;
    }
  initial[i].state = NULL;

  tnfa->num_transitions = add;
  tnfa->final = transitions + offs[tree->lastpos[0].position];
  tnfa->num_states = parse_ctx.position;
  tnfa->cflags = cflags;

  DPRINT(("final state %p\n", (void *)tnfa->final));

  tre_mem_destroy(mem);
  tre_stack_destroy(stack);
  xfree(counts);
  xfree(offs);

  preg->TRE_REGEX_T_FIELD = (void *)tnfa;
  return REG_OK;

 error_exit:
  /* Free everything that was allocated and return the error code. */
  tre_mem_destroy(mem);
  if (stack != NULL)
    tre_stack_destroy(stack);
  if (counts != NULL)
    xfree(counts);
  if (offs != NULL)
    xfree(offs);
  preg->TRE_REGEX_T_FIELD = (void *)tnfa;
  tre_free(preg);
  return errcode;
}




void
tre_free(regex_t *preg)
{
  tre_tnfa_t *tnfa;
  unsigned int i;
  tre_tnfa_transition_t *trans;

  tnfa = (void *)preg->TRE_REGEX_T_FIELD;
  if (!tnfa)
    return;

  for (i = 0; i < tnfa->num_transitions; i++)
    if (tnfa->transitions[i].state)
      {
	if (tnfa->transitions[i].tags)
	  xfree(tnfa->transitions[i].tags);
	if (tnfa->transitions[i].neg_classes)
	  xfree(tnfa->transitions[i].neg_classes);
	if (tnfa->transitions[i].params)
	  xfree(tnfa->transitions[i].params);
      }
  if (tnfa->transitions)
    xfree(tnfa->transitions);

  if (tnfa->initial)
    {
      for (trans = tnfa->initial; trans->state; trans++)
	{
	  if (trans->tags)
	    xfree(trans->tags);
	  if (trans->params)
	    xfree(trans->params);
	}
      xfree(tnfa->initial);
    }

  if (tnfa->submatch_data)
    {
      for (i = 0; i < tnfa->num_submatches; i++)
	if (tnfa->submatch_data[i].parents)
	  xfree(tnfa->submatch_data[i].parents);
      xfree(tnfa->submatch_data);
    }

  if (tnfa->tag_directions)
    xfree(tnfa->tag_directions);
  if (tnfa->firstpos_chars)
    xfree(tnfa->firstpos_chars);
  if (tnfa->minimal_tags)
    xfree(tnfa->minimal_tags);
  xfree(tnfa);
}


/* EOF */
