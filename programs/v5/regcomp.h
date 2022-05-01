#ifndef _REGCOMP_H_
#define _REGCOMP_H_

#include "regex_internal.h"

static void re_compile_fastmap_iter(regex_t *bufp, 
				    const re_dfastate_t *init_state, 
				    char *fastmap);
static reg_errcode_t init_dfa (re_dfa_t *dfa, int pat_len);
static reg_errcode_t init_word_char (re_dfa_t *dfa);
#ifdef RE_ENABLE_I18N
static void free_charset (re_charset_t *cset);
#endif /* RE_ENABLE_I18N */
static void free_workarea_compile (regex_t *preg);
static reg_errcode_t create_initial_state (re_dfa_t *dfa);
static reg_errcode_t analyze (re_dfa_t *dfa);
static reg_errcode_t analyze_tree (re_dfa_t *dfa, bin_tree_t *node);
static void calc_first (re_dfa_t *dfa, bin_tree_t *node);
static void calc_next (re_dfa_t *dfa, bin_tree_t *node);
static void calc_epsdest (re_dfa_t *dfa, bin_tree_t *node);
static reg_errcode_t duplicate_node_closure (re_dfa_t *dfa, int top_org_node,
					     int top_clone_node, int root_node,
					     unsigned int constraint);
static reg_errcode_t duplicate_node (int *new_idx, re_dfa_t *dfa, int org_idx,
				     unsigned int constraint);
static reg_errcode_t calc_eclosure (re_dfa_t *dfa);
static reg_errcode_t calc_eclosure_iter (re_node_set *new_set, re_dfa_t *dfa,
					 int node, int root);
static void calc_inveclosure (re_dfa_t *dfa);
static int fetch_number (re_string_t *input, re_token_t *token,
			 reg_syntax_t syntax);
static re_token_t fetch_token (re_string_t *input, reg_syntax_t syntax);
static int peek_token (re_token_t *token, re_string_t *input,
			reg_syntax_t syntax);
static int peek_token_bracket (re_token_t *token, re_string_t *input,
			       reg_syntax_t syntax);
static bin_tree_t *parse (re_string_t *regexp, regex_t *preg,
			  reg_syntax_t syntax, reg_errcode_t *err);
static bin_tree_t *parse_reg_exp (re_string_t *regexp, regex_t *preg,
				  re_token_t *token, reg_syntax_t syntax,
				  int nest, reg_errcode_t *err);
static bin_tree_t *parse_branch (re_string_t *regexp, regex_t *preg,
				 re_token_t *token, reg_syntax_t syntax,
				 int nest, reg_errcode_t *err);
static bin_tree_t *parse_expression (re_string_t *regexp, regex_t *preg,
				     re_token_t *token, reg_syntax_t syntax,
				     int nest, reg_errcode_t *err);
static bin_tree_t *parse_sub_exp (re_string_t *regexp, regex_t *preg,
				  re_token_t *token, reg_syntax_t syntax,
				  int nest, reg_errcode_t *err);
static bin_tree_t *parse_dup_op (bin_tree_t *dup_elem, re_string_t *regexp,
				 re_dfa_t *dfa, re_token_t *token,
				 reg_syntax_t syntax, reg_errcode_t *err);
static bin_tree_t *parse_bracket_exp (re_string_t *regexp, re_dfa_t *dfa,
				      re_token_t *token, reg_syntax_t syntax,
				      reg_errcode_t *err);
static reg_errcode_t parse_bracket_element (bracket_elem_t *elem,
					    re_string_t *regexp,
					    re_token_t *token, int token_len,
					    re_dfa_t *dfa,
					    reg_syntax_t syntax);
static reg_errcode_t parse_bracket_symbol (bracket_elem_t *elem,
					  re_string_t *regexp,
					  re_token_t *token);
#ifndef _LIBC
# ifdef RE_ENABLE_I18N
static reg_errcode_t build_range_exp (re_bitset_ptr_t sbcset,
				      re_charset_t *mbcset, int *range_alloc,
				      bracket_elem_t *start_elem,
				      bracket_elem_t *end_elem);
static reg_errcode_t build_collating_symbol (re_bitset_ptr_t sbcset,
					     re_charset_t *mbcset,
					     int *coll_sym_alloc,
					     const unsigned char *name);
# else /* not RE_ENABLE_I18N */
static reg_errcode_t build_range_exp (re_bitset_ptr_t sbcset,
				      bracket_elem_t *start_elem,
				      bracket_elem_t *end_elem);
static reg_errcode_t build_collating_symbol (re_bitset_ptr_t sbcset,
					     const unsigned char *name);
# endif /* not RE_ENABLE_I18N */
#endif /* not _LIBC */
#ifdef RE_ENABLE_I18N
static reg_errcode_t build_equiv_class (re_bitset_ptr_t sbcset,
					re_charset_t *mbcset,
					int *equiv_class_alloc,
					const unsigned char *name);
static reg_errcode_t build_charclass (re_bitset_ptr_t sbcset,
				      re_charset_t *mbcset,
				      int *char_class_alloc,
				      const unsigned char *class_name,
				      reg_syntax_t syntax);
#else  /* not RE_ENABLE_I18N */
static reg_errcode_t build_equiv_class (re_bitset_ptr_t sbcset,
					const unsigned char *name);
static reg_errcode_t build_charclass (re_bitset_ptr_t sbcset,
				      const unsigned char *class_name,
				      reg_syntax_t syntax);
#endif /* not RE_ENABLE_I18N */
static bin_tree_t *build_word_op (re_dfa_t *dfa, int not, reg_errcode_t *err);
static void free_bin_tree (bin_tree_t *tree);
static bin_tree_t *create_tree (bin_tree_t *left, bin_tree_t *right,
				re_token_type_t type, int index);
static bin_tree_t *duplicate_tree (const bin_tree_t *src, re_dfa_t *dfa);

#endif
