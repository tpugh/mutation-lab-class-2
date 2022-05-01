#ifndef _REGEXEC_H_
#define _REGEXEC_H_


static reg_errcode_t match_ctx_init (re_match_context_t *cache, int eflags,
				     re_string_t *input, int n);
static void match_ctx_free (re_match_context_t *cache);
static reg_errcode_t match_ctx_add_entry (re_match_context_t *cache, int node,
					  int str_idx, int from, int to,
					  re_sub_match_top_t *top,
					  re_sub_match_last_t *last);
static int search_cur_bkref_entry (re_match_context_t *mctx, int str_idx);
static void match_ctx_clear_flag (re_match_context_t *mctx);
static reg_errcode_t match_ctx_add_subtop (re_match_context_t *mctx, int node,
					   int str_idx);
static re_sub_match_last_t * match_ctx_add_sublast (re_sub_match_top_t *subtop,
						   int node, int str_idx);
static void sift_ctx_init (re_sift_context_t *sctx, re_dfastate_t **sifted_sts,
			   re_dfastate_t **limited_sts, int last_node,
			   int last_str_idx, int check_subexp);
static int re_search_2_stub (struct re_pattern_buffer *bufp,
			     const char *string1, int length1,
			     const char *string2, int length2,
			     int start, int range, struct re_registers *regs,
			     int stop, int ret_len);
static int re_search_stub (struct re_pattern_buffer *bufp,
			   const char *string, int length, int start,
			   int range, int stop, struct re_registers *regs,
			   int ret_len);
static unsigned re_copy_regs (struct re_registers *regs, regmatch_t *pmatch,
			      int nregs, int regs_allocated);
static inline re_dfastate_t *acquire_init_state_context (reg_errcode_t *err,
							 const regex_t *preg,
							 const re_match_context_t *mctx,
							 int idx);
static reg_errcode_t prune_impossible_nodes (const regex_t *preg,
					     re_match_context_t *mctx);
static int check_matching (const regex_t *preg, re_match_context_t *mctx,
			   int fl_search, int fl_longest_match);
static int check_halt_node_context (const re_dfa_t *dfa, int node,
				    unsigned int context);
static int check_halt_state_context (const regex_t *preg,
				     const re_dfastate_t *state,
				     const re_match_context_t *mctx, int idx);
static void update_regs (re_dfa_t *dfa, regmatch_t *pmatch, int cur_node,
			 int cur_idx, int nmatch);
static int proceed_next_node (const regex_t *preg, int nregs, regmatch_t *regs,
			      const re_match_context_t *mctx,
			      int *pidx, int node, re_node_set *eps_via_nodes,
			      struct re_fail_stack_t *fs);
static reg_errcode_t push_fail_stack (struct re_fail_stack_t *fs,
				      int str_idx, int *dests, int nregs,
				      regmatch_t *regs,
				      re_node_set *eps_via_nodes);
static int pop_fail_stack (struct re_fail_stack_t *fs, int *pidx, int nregs,
			   regmatch_t *regs, re_node_set *eps_via_nodes);
static reg_errcode_t set_regs (const regex_t *preg,
			       const re_match_context_t *mctx,
			       size_t nmatch, regmatch_t *pmatch,
			       int fl_backtrack);
static reg_errcode_t free_fail_stack_return (struct re_fail_stack_t *fs);

#ifdef RE_ENABLE_I18N
static int sift_states_iter_mb (const regex_t *preg,
				const re_match_context_t *mctx,
				re_sift_context_t *sctx,
				int node_idx, int str_idx, int max_str_idx);
#endif /* RE_ENABLE_I18N */
static reg_errcode_t sift_states_backward (const regex_t *preg,
					   re_match_context_t *mctx,
					   re_sift_context_t *sctx);
static reg_errcode_t update_cur_sifted_state (const regex_t *preg,
					      re_match_context_t *mctx,
					      re_sift_context_t *sctx,
					      int str_idx,
					      re_node_set *dest_nodes);
static reg_errcode_t add_epsilon_src_nodes (re_dfa_t *dfa,
					    re_node_set *dest_nodes,
					    const re_node_set *candidates);
static reg_errcode_t sub_epsilon_src_nodes (re_dfa_t *dfa, int node,
					    re_node_set *dest_nodes,
					    const re_node_set *and_nodes);
static int check_dst_limits (re_dfa_t *dfa, re_node_set *limits,
			     re_match_context_t *mctx, int dst_node,
			     int dst_idx, int src_node, int src_idx);
static int check_dst_limits_calc_pos (re_dfa_t *dfa, re_match_context_t *mctx,
				      int limit, re_node_set *eclosures,
				      int subexp_idx, int node, int str_idx);
static reg_errcode_t check_subexp_limits (re_dfa_t *dfa,
					  re_node_set *dest_nodes,
					  const re_node_set *candidates,
					  re_node_set *limits,
					  struct re_backref_cache_entry *bkref_ents,
					  int str_idx);
static reg_errcode_t sift_states_bkref (const regex_t *preg,
					re_match_context_t *mctx,
					re_sift_context_t *sctx,
					int str_idx, re_node_set *dest_nodes);
static reg_errcode_t clean_state_log_if_need (re_match_context_t *mctx,
					      int next_state_log_idx);
static reg_errcode_t merge_state_array (re_dfa_t *dfa, re_dfastate_t **dst,
					re_dfastate_t **src, int num);
static re_dfastate_t *transit_state (reg_errcode_t *err, const regex_t *preg,
				     re_match_context_t *mctx,
				     re_dfastate_t *state, int fl_search);
static reg_errcode_t check_subexp_matching_top (re_dfa_t *dfa,
						re_match_context_t *mctx,
						re_node_set *cur_nodes,
						int str_idx);
static re_dfastate_t *transit_state_sb (reg_errcode_t *err, const regex_t *preg,
					re_dfastate_t *pstate,
					int fl_search,
					re_match_context_t *mctx);


#ifdef RE_ENABLE_I18N
static reg_errcode_t transit_state_mb (const regex_t *preg,
				       re_dfastate_t *pstate,
				       re_match_context_t *mctx);
#endif /* RE_ENABLE_I18N */
static reg_errcode_t transit_state_bkref (const regex_t *preg,
					  re_dfastate_t *pstate,
					  re_match_context_t *mctx);
static reg_errcode_t transit_state_bkref_loop (const regex_t *preg,
					       re_node_set *nodes,
					       re_match_context_t *mctx);
static reg_errcode_t get_subexp (const regex_t *preg, re_match_context_t *mctx,
				 int bkref_node,
				 int bkref_str_idx, int subexp_idx);
static reg_errcode_t get_subexp_sub (const regex_t *preg,
				     re_match_context_t *mctx,
				     re_sub_match_top_t *sub_top,
				     re_sub_match_last_t *sub_last,
				     int bkref_node, int bkref_str,
				     int subexp_idx);
static int find_subexp_node (re_dfa_t *dfa, re_node_set *nodes,
			     int subexp_idx, int fl_open);
static reg_errcode_t check_arrival (const regex_t *preg,
				    re_match_context_t *mctx,
				    re_sub_match_top_t *sub_top,
				    re_sub_match_last_t *sub_last,
				    int bkref_node, int bkref_str);
static reg_errcode_t expand_eclosures (re_dfa_t *dfa, re_node_set *cur_nodes,
				       int ex_subexp, int fl_open);
static reg_errcode_t expand_eclosures_sub (re_dfa_t *dfa, re_node_set *dst_nodes,
					   int target, int ex_subexp, int fl_open);
static reg_errcode_t expand_bkref_cache (const regex_t *preg,
					 re_match_context_t *mctx,
					 re_sub_match_top_t *sub_top,
					 re_sub_match_last_t *sub_last,
					 re_node_set *cur_nodes, int cur_str,
					 int last_str, int ex_subexp,
					 int fl_open);
static re_dfastate_t **build_trtable (const regex_t *dfa,
				      const re_dfastate_t *state,
				      int fl_search);
#ifdef RE_ENABLE_I18N
static int check_node_accept_bytes (const regex_t *preg, int node_idx,
				    const re_string_t *input, int idx);
# ifdef _LIBC
static unsigned int find_collation_sequence_value (const unsigned char *mbs,
						   size_t name_len);
# endif /* _LIBC */
#endif /* RE_ENABLE_I18N */
static int group_nodes_into_DFAstates (const regex_t *dfa,
				       const re_dfastate_t *state,
				       re_node_set *states_node,
				       bitset *states_ch);
static int check_node_accept (const regex_t *preg, const re_token_t *node,
			      const re_match_context_t *mctx, int idx);
static reg_errcode_t extend_buffers (re_match_context_t *mctx);
static inline int my_memcmp (char *s1, char *s2, unsigned int l);

#endif
