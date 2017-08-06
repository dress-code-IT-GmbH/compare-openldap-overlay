/*
 * trace.c - traces overlay invocation
 */
/*
 * $OpenLDAP$
 */
/*
 * This work is part of OpenLDAP Software <http://www.openldap.org/>.
 * Copyright 2006-2016 The OpenLDAP Foundation. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP Public
 * License. A copy of this license is available in the file LICENSE in
 * the top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/*
 * ACKNOWLEDGEMENTS: This work was initially developed by Pierangelo
 * Masarati for inclusion in OpenLDAP Software.
 */

#include "portable.h"
#ifndef SLAPD_OVER_COMPARE
#define SLAPD_OVER_COMPARE SLAPD_MOD_DYNAMIC
#endif

// #undef ldap_debug /* silence a warning in ldap-int.h */
// #include "../../../libraries/libldap/ldap-int.h" /* for ldap_ld_free */

#ifdef SLAPD_OVER_COMPARE

#include <stdio.h>

#include <ac/socket.h>
#include <ac/errno.h>
#include <ac/time.h>
#include <ac/string.h>
#include <ac/ctype.h>

#include "lutil.h"
#include "slap.h"
#include "config.h"

#include "twcompare.h"

char *bval2str(struct berval bv)
{
	char *s;
	s = ch_calloc(bv.bv_len + 1, sizeof(char));
	strncpy(s, bv.bv_val, bv.bv_len);
	return s;
}

int same_equality_and_ordering(AttributeDescription * ad_a,
			       AttributeDescription * ad_b)
{
	LDAPAttributeType sat_atype_a;
	LDAPAttributeType sat_atype_b;
	sat_atype_a = ad_a->ad_type->sat_atype;
	sat_atype_b = ad_b->ad_type->sat_atype;
	char *at_equality_oid_a = sat_atype_a.at_equality_oid;
	char *at_ordering_oid_a = sat_atype_a.at_ordering_oid;
	char *at_equality_oid_b = sat_atype_b.at_equality_oid;
	char *at_ordering_oid_b = sat_atype_b.at_ordering_oid;

	Log4(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
	     "twcompare: comparing: %s-%s vs %s-%s\n", at_equality_oid_a,
	     at_ordering_oid_a, at_equality_oid_b, at_ordering_oid_b);

	if (strcasecmp(at_equality_oid_a, at_equality_oid_b)) {
		return OV_COMPARE_NOT_THE_SAME_ORDERING;
	}
	if (strcasecmp(at_ordering_oid_a, at_ordering_oid_b)) {
		return OV_COMPARE_NOT_THE_SAME_ORDERING;
	}
	return OV_COMPARE_SUCCESS;
}

twcompare_function_result_t attr_add(twcompare_attributes_t * attributes,
				     AttributeDescription * ad)
{
	twcompare_attrs_t *attr;
	int i;
	int rc;
	attr = ch_calloc(1, sizeof(twcompare_attrs_t));
	attr->attr = ad;

	for (i = 0; i < 2; i++) {
		if (!attributes->attribs[i]) {
			if (i > 0) {
				rc = same_equality_and_ordering
				    (attributes->attribs[0]->attr, ad);
				if (rc != OV_COMPARE_SUCCESS) {
					return OV_COMPARE_NOT_THE_SAME_ORDERING;
				}
			}
			attributes->attribs[i] = attr;
			return OV_COMPARE_SUCCESS;
		}
	}

	ch_free(attr);
	return OV_COMPARE_MORE_THAN_TWO_ARGUMENTS_ERROR;
}

void attr_del(twcompare_attributes_t * attributes, int attnum)
{
	ch_free(attributes->attribs[attnum]);
	attributes->attribs[attnum] = (void *)0;
}

void attr_log(twcompare_attributes_t * attributes)
{
	int i;
	char *s[2];
	static const char fmt[] = "twcompare: attribute 0:=%s, 1:=%s\n";

	for (i = 0; i < 2; i++) {
		if (attributes->attribs[i]) {
			s[i] = bval2str(attributes->attribs[i]->attr->ad_cname);
		} else {
			s[i] = "empty";
		}
	}
	Log2(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO, fmt, s[0], s[1]);
}

void twcompare_emsg(int retcode, char **emsg)
{
	switch (retcode) {
	case OV_COMPARE_MORE_THAN_TWO_ARGUMENTS_ERROR:
		*emsg = "only two arguments are allowed for comparison";
		break;
	case OV_COMPARE_SLAP_CONFIG_EMIT_CALLED_ERROR:
		*emsg = "LDAP_CONFIG_EMIT_CALLED: Don't know what to do?";
	case OV_COMPARE_NOT_THE_SAME_ORDERING:
		*emsg = "attributes do not have the same ordering/equality";
		break;
	default:
		*emsg = "undefined retcode";
	}
}

twcompare_attributes_t *create_private()
{
	twcompare_attributes_t *new_priv;

	new_priv = ch_calloc(1, sizeof(twcompare_attributes_t));
	return new_priv;
}

static int twcompare_cf_gen(ConfigArgs * c)
{
	slap_overinst *on = (slap_overinst *) c->bi;
	twcompare_attributes_t *attributes =
	    (twcompare_attributes_t *) on->on_bi.bi_private;
	AttributeDescription *ad;
	char *param = NULL;
	char *emsg;
	int rc = 0;

	Log1(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
	     "twcompare: entered twcompare config handler with opcode %d\n",
	     c->op);
	switch (c->op) {
	case SLAP_CONFIG_EMIT:
		/*
		 * should never be triggered anyway, but just in case, we just
		 * provide an error
		 */
		Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_ERR,
		     "SLAP_CONFIG_EMIT called\n");
		twcompare_emsg(OV_COMPARE_SLAP_CONFIG_EMIT_CALLED_ERROR, &emsg);
		snprintf(c->cr_msg, sizeof(c->cr_msg), "ERROR: %s: %s", param,
			 emsg);
		rc = ARG_BAD_CONF;
		break;
	case LDAP_MOD_ADD:
		param = c->argv[1];
		Log1(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
		     "twcompare: LDAP_MOD_ADD: %s\n", param);
	case SLAP_CONFIG_ADD:
		if (!param) {
			param = c->argv[1];
			Log1(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
			     "twcompare: SLAP_CONFIG_ADD: %s\n", param);
		}
		ad = NULL;
		const char *text;
		static const char fmt[] = "(twcompare) attribute: %s: %s";
		if (slap_str2ad(param, &ad, &text) != LDAP_SUCCESS) {
			snprintf(c->cr_msg, sizeof(c->cr_msg), fmt, param,
				 text);
			rc = ARG_BAD_CONF;
			break;
		}
		rc = attr_add(attributes, ad);
		if (rc != LDAP_SUCCESS) {
			twcompare_emsg(rc, &emsg);
			snprintf(c->cr_msg, sizeof(c->cr_msg), fmt, param,
				 emsg);
			rc = ARG_BAD_CONF;
			break;
		}
		break;
	case LDAP_MOD_DELETE:
		if (c->valx < 0) {
			Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
			     "twcompare: LDAP_MOD_DELETE: all\n");
			attr_del(attributes, 0);
			attr_del(attributes, 1);
		} else {
			Log1(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
			     "twcompare: LDAP_MOD_DELETE: %d\n", c->valx);
			attr_del(attributes, c->valx);
		}
		return rc;
		break;
	default:
		break;
	}
	attr_log(attributes);
	return rc;
}

int reduce_result(int r)
{
	if (r < 0)
		r = -1;
	if (r > 0)
		r = 1;
	return r;
}

int compare_existance(void *a, void *b)
{
	if (a && b)
		return EXISTANCE_EQUAL_BOTH_EXIST;
	if ((!a) && !(b))
		return EXISTANCE_EQUAL_BOTH_DONTEXIST;
	if (a && (!b))
		return EXISTANCE_BLTA_ONLY_A_EXISTS;
	if ((!a) && b)
		return EXISTANCE_ALTB_ONLY_B_EXISTS;
	return -1;		// never reached
}

int attr_is_not_single(Attribute * a)
{
	// TODO: Another approach would be just to work on attributes which
	// are defined as single-valued and let the server do this
	if (a->a_next)
		return 1;
	return 0;
}

static int twcompare_add(Operation * op, SlapReply * rs)
{
	slap_overinst *on = (slap_overinst *) op->o_bd->bd_info;
	twcompare_attributes_t *cfg;
	Attribute *entry_attribs, *attr_a, *attr_b;
	AttributeDescription *ad_a, *ad_b;
	MatchingRule *mr;
	//slap_mr_normalize_func* normal_fun;  /* we don't do normal fun - see below */
	slap_mr_match_func *match_fun;
	int match;
	int rc = 0;
	char *msg = NULL;
	int cmp;
	BerValue val_a, val_b;
	struct berval bv_result = BER_BVNULL;

	if (get_relax(op) || SLAPD_SYNC_IS_SYNCCONN(op->o_connid)) {
		return SLAP_CB_CONTINUE;
	}

	/* TODO: Should we check something here? on? */

	cfg = on->on_bi.bi_private;
	Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO, "twcompare: twcompare_add\n");
	attr_log(cfg);

	ad_a = cfg->attribs[0]->attr;
	ad_b = cfg->attribs[1]->attr;

	Entry *e = op->ora_e;
	entry_attribs = e->e_attrs;

	attr_a = attrs_find(entry_attribs, ad_a);
	attr_b = attrs_find(entry_attribs, ad_b);

	if (attr_is_not_single(attr_a) && attr_is_not_single(attr_b)) {
		rc = LDAP_OPERATIONS_ERROR;
		msg = "compared attributes must only appear once in an entry";
		send_ldap_error(op, rs, rc, msg);
		return rc;
	}

	cmp = compare_existance(attr_a, attr_b);

	if (cmp == CMP_EQUAL) {

		/* TODO: it seems, that we always get normalized values here.
		 * For now, we take that as granted, but maybe we should
		 * look that up in the API documentation.
		 */

		val_a = attr_a->a_nvals[0];
		val_b = attr_b->a_nvals[0];

		mr = ad_a->ad_type->sat_ordering;
		if (!mr) {
			mr = ad_a->ad_type->sat_equality;
		}
		if (!mr) {
			rc = LDAP_OPERATIONS_ERROR;
			msg =
			    "there are neither ordering or equality defined for this";
			//TODO: We should check that while configuring
			send_ldap_error(op, rs, rc, msg);
			return rc;
		}
		//normal_fun = mr->smr_normalize;
		match_fun = mr->smr_match;

		Log2(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
		     "twcompare: comparing %s:%s\n", val_a.bv_val,
		     val_b.bv_val);

		match_fun(&match, 0, ad_a->ad_type->sat_syntax, mr, &val_a,
			  &val_b);

		cmp = match;
	}

	cmp = reduce_result(cmp);

	Log1(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO, "twcompare: result: %d\n", cmp);

	char *cbuf = ch_calloc(sizeof(char), 23);
	bv_result.bv_val = cbuf;
	bv_result.bv_len = snprintf(cbuf, 23, "%d", cmp);
	attr_merge_one(e, ad_idnSyncDiff, &bv_result, NULL);
	return SLAP_CB_CONTINUE;
}

static int twcompare_update(Operation * op, SlapReply * rs)
{
	slap_overinst *on = (slap_overinst *) op->o_bd->bd_info;
	twcompare_attributes_t *cfg;
	Modifications *modlist, *m;
	MatchingRule *mr;
	Attribute *entry_attribs, *attr_a, *attr_b;
	AttributeDescription *ad_a, *ad_b, *ad_m;
	struct berval bv_result;
	Backend *be = NULL;
	Entry *target_entry = NULL;
	BerValue *val_a, *val_b;
	char *msg = NULL;
	slap_mr_match_func *match_fun;
	int rc;
	int cmp;
	int match;
	Modifications *new_mod = NULL;

	rc = 0;

	if (get_relax(op) || SLAPD_SYNC_IS_SYNCCONN(op->o_connid)) {
		return SLAP_CB_CONTINUE;
	}
	if (op->o_tag == LDAP_REQ_MODRDN) {
		return SLAP_CB_CONTINUE;
	}

	cfg = on->on_bi.bi_private;
	Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO, "twcompare: twcompare_update\n");
	attr_log(cfg);

	modlist = op->orm_modlist;

	ad_a = cfg->attribs[0]->attr;
	ad_b = cfg->attribs[1]->attr;

	val_a = val_b = NULL;

	for (m = modlist; m != NULL; m = m->sml_next) {
		ad_m = m->sml_mod.sm_desc;
		if (!strcmp(ad_m->ad_cname.bv_val, ad_a->ad_cname.bv_val)) {
			val_a = &m->sml_mod.sm_nvalues[0];
		}
		if (!strcmp(ad_m->ad_cname.bv_val, ad_b->ad_cname.bv_val)) {
			val_b = &m->sml_mod.sm_nvalues[0];
		}
	}

	if (!(val_a || val_b)) {
		Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
		     "twcompare: no comparable attributes recognized: continuing\n");
		return SLAP_CB_CONTINUE;
	}

	if (!(val_a && val_b)) {
		be = op->o_bd;
		op->o_bd = on->on_info->oi_origdb;
		rc = be_entry_get_rw(op, &op->o_req_ndn, NULL, NULL, 0,
				     &target_entry);

		if (target_entry == NULL) {
			/* hmm ... this shouldn't happen, but let's deal that problem to the server */
			return SLAP_CB_CONTINUE;
		}
		entry_attribs = target_entry->e_attrs;
		if (!val_a) {
			attr_a = attrs_find(entry_attribs, ad_a);
			if (attr_a) {
				val_a = ber_bvdup(&attr_a->a_nvals[0]);
			}
		}
		if (!val_b) {
			attr_b = attrs_find(entry_attribs, ad_b);
			if (attr_b) {
				val_b = ber_bvdup(&attr_b->a_nvals[0]);
			}
		}
		be_entry_release_r(op, target_entry);
		op->o_bd = be;
	}

	cmp = compare_existance(val_a, val_b);

	if (cmp == CMP_EQUAL) {

		mr = ad_a->ad_type->sat_ordering;
		if (!mr) {
			mr = ad_a->ad_type->sat_equality;
		}
		if (!mr) {
			rc = LDAP_OPERATIONS_ERROR;
			msg =
			    "there are neither ordering or equality defined for this";
			//TODO: We should check that while configuring
			send_ldap_error(op, rs, rc, msg);
			return rc;
		}
		//normal_fun = mr->smr_normalize;
		match_fun = mr->smr_match;

		Log2(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
		     "twcompare: comparing %s:%s\n", val_a->bv_val,
		     val_b->bv_val);

		match_fun(&match, 0, ad_a->ad_type->sat_syntax, mr, val_a,
			  val_b);

		cmp = match;

	}
	cmp = reduce_result(cmp);

	Log1(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO, "twcompare: result: %d\n", cmp);

	//asm("int $3");
	char *cbuf = ch_calloc(sizeof(char), 23);
	bv_result.bv_val = cbuf;
	bv_result.bv_len = snprintf(cbuf, 23, "%d", cmp);

	new_mod = (Modifications *) ch_calloc(sizeof(Modifications), 1);
	new_mod->sml_op = LDAP_MOD_REPLACE;
	new_mod->sml_desc = ad_idnSyncDiff;
	new_mod->sml_type = ad_idnSyncDiff->ad_cname;
	new_mod->sml_values =
	    (struct berval *)ch_calloc(sizeof(struct berval), 2);
	new_mod->sml_nvalues =
	    (struct berval *)ch_calloc(sizeof(struct berval), 2);
	ber_dupbv(&new_mod->sml_values[0], &bv_result);
	ber_dupbv(&new_mod->sml_nvalues[0], &bv_result);
	new_mod->sml_numvals = 1;

	for (m = modlist; m->sml_next != NULL; m = m->sml_next) ;
	m->sml_next = new_mod;

	return SLAP_CB_CONTINUE;
}

static int twcompare_db_init(BackendDB * be)
{

	slap_overinst *oi = (slap_overinst *) be->bd_info;
	oi->on_bi.bi_private = (void *)create_private();
	Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO, "twcompare: DB_INIT:\n");
	attr_log(oi->on_bi.bi_private);
	return 0;
}

static int twcompare_db_config(BackendDB * be, const char *fname, int lineno,
			       int argc, char **argv)
{
	Log2(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
	     "twcompare:  DB_CONFIG argc=%d argv[0]=\"%s\"\n", argc, argv[0]);
	return 0;
}

static int twcompare_db_open(BackendDB * be, ConfigReply * cr)
{
	slap_overinst *on;
	twcompare_attributes_t *cfg;
	on = (slap_overinst *) be->bd_info;
	cfg = on->on_bi.bi_private;
	if (!cfg) {
		Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
		     "twcompare: DB_OPEN no config (now)\n");
		return 0;
	}
	Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO, "twcompare: DB_OPEN\n");
	attr_log(cfg);
	return 0;
}

static int twcompare_db_close(BackendDB * be)
{
	Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO, "twcompare DB_CLOSE\n");
	return 0;
}

static int twcompare_db_destroy(BackendDB * be)
{
	Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO, "twcompare: DB_DESTROY\n");
	return 0;
}

static slap_overinst twcompare;
#if SLAPD_OVER_TWCOMPARE == SLAPD_MOD_DYNAMIC
static
#endif
int twcompare_initialize()
{
	int rc;
	int i, code;
	for (i = 0; twcompare_OpSchema[i].def; i++) {
		code =
		    register_at(twcompare_OpSchema[i].def,
				twcompare_OpSchema[i].ad, 0);
		if (code) {
			Log0(LDAP_DEBUG_ANY, LDAP_LEVEL_INFO,
			     "twcompare: initialize: register_at failed\n");
			return code;
		}
	}

	twcompare.on_bi.bi_type = "twcompare";
	twcompare.on_bi.bi_db_init = (void *)twcompare_db_init;
	twcompare.on_bi.bi_db_open = (void *)twcompare_db_open;
	twcompare.on_bi.bi_db_config = (void *)twcompare_db_config;
	twcompare.on_bi.bi_db_close = (void *)twcompare_db_close;
	twcompare.on_bi.bi_db_destroy = (void *)twcompare_db_destroy;
	twcompare.on_bi.bi_op_add = (void *)twcompare_add;
	// twcompare.on_bi.bi_op_bind = twcompare_op_func;
	// twcompare.on_bi.bi_op_unbind = twcompare_op_func;
	// twcompare.on_bi.bi_op_compare = twcompare_op_func;
	// twcompare.on_bi.bi_op_delete = twcompare_op_func;
	twcompare.on_bi.bi_op_modify = (void *)twcompare_update;
	// twcompare.on_bi.bi_op_modrdn = twcompare_op_func;
	// twcompare.on_bi.bi_op_search = twcompare_op_func;
	// twcompare.on_bi.bi_op_abandon = twcompare_op_func;
	// twcompare.on_bi.bi_extended = twcompare_op_func;
	// twcompare.on_response = twcompare_response;
	twcompare.on_bi.bi_cf_ocs = twcompareocs;
	rc = config_register_schema(twcomparecfg, twcompareocs);
	if (rc)
		return rc;
	return overlay_register(&twcompare);
}

#if SLAPD_OVER_TWCOMPARE == SLAPD_MOD_DYNAMIC
int init_module(int argc, char *argv[])
{
	return twcompare_initialize();
}
#endif				/* SLAPD_OVER_TWCOMPARE ==
				 * SLAPD_MOD_DYNAMIC */

#endif				/* defined(SLAPD_OVER_TWCOMPARE) */
