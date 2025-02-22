/*	ASCEND modelling environment
	Copyright (C) 1990 Karl Michael Westerberg
	Copyright (C) 1993 Joseph Zaher
	Copyright (C) 1994 Joseph Zaher, Benjamin Andrew Allan
	Copyright (C) 2006 Carnegie Mellon University

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*//*
	by Karl Michael Westerberg
	Created: 2/6/90
	Last in CVS $Revision: 1.32 $ $Date: 1998/01/29 00:42:28 $ $Author: ballan $
*/

#include "rel.h"

#include <math.h>
#include <stdarg.h>
#include <ascend/general/platform.h>
#include <ascend/general/panic.h>
#include <ascend/general/ascMalloc.h>
#include <ascend/general/mem.h>
#include <ascend/general/list.h>
#include <ascend/general/dstring.h>


#include <ascend/compiler/symtab.h>

#include <ascend/compiler/instance_enum.h>
#include <ascend/compiler/instance_io.h>
#include <ascend/compiler/symtab.h>
#include <ascend/compiler/extfunc.h>
#include <ascend/compiler/extcall.h>
#include <ascend/compiler/functype.h>
#include <ascend/compiler/safe.h>

#include <ascend/compiler/expr_types.h>
#include <ascend/compiler/find.h>
#include <ascend/compiler/atomvalue.h>
#include <ascend/compiler/instquery.h>
#include <ascend/compiler/mathinst.h>
#include <ascend/compiler/parentchild.h>
#include <ascend/compiler/instance_io.h>
#include <ascend/compiler/rel_blackbox.h>
#include <ascend/compiler/vlist.h>
#include <ascend/compiler/relation.h>
#include <ascend/compiler/relation_util.h>
#include <ascend/compiler/packages.h>

#include <ascend/linear/mtx.h>

#include "discrete.h"
#include "conditional.h"
#include "logrel.h"
#include "bnd.h"
#include "slv_server.h"

/*------------------------------------------------------------------------------
	forward declarations, constants, typedefs
*/

#ifdef DEBUG
# define REL_DEBUG CONSOLE_DEBUG
#else
# define REL_DEBUG(MSG,...) ((void)0)
#endif

#define IPTR(i) ((struct Instance *)(i))
#define REIMPLEMENT 0 /* if set to 1, compiles code tagged with it. */

/* define symchar names needed */
static symchar *g_strings[1];
#define INCLUDED_R g_strings[0]
//Do we need more gstrings for relations? eg: for obtaining relation upper and lower bounds??

static const struct rel_relation g_rel_defaults = {
   NULL,		    /* instance */
   NULL,		    /* extnode */
   NULL,		    /* incidence */
   e_rel_token,	    /* e_token */
   0,			    /* n_incidences */
   -1,		    	/* mindex */
   -1,			    /* sindex */
   -1,		    	/* model index */
   (REL_INCLUDED)	/* flags */
};
/**<
	@NOTE Don't forget to update the initialization when the structure is modified.
*/

/*------------------------------------------------------------------------------
  GENERAL 'RELATION' ROUTINES
*/

static struct rel_relation *rel_copy(const struct rel_relation *rel){
	struct rel_relation *newrel;
	newrel = ASC_NEW(struct rel_relation);
	REL_DEBUG("Copying REL_RELATION from %p to %p",rel,newrel);
	*newrel = *rel;
	return(newrel);
}

struct rel_relation *
rel_create(SlvBackendToken instance, struct rel_relation *newrel)
{
#ifdef DIEDIEDIE
  struct ExtCallNode *ext;
#endif
  enum Expr_enum ctype;

  if (newrel==NULL) {
    newrel = rel_copy(&g_rel_defaults); /* malloc the relation */
  } else {
    *newrel = g_rel_defaults; /* init the space we've been sent */
  }
  assert(newrel!=NULL);
  newrel->instance = instance;
  (void)GetInstanceRelation(IPTR(instance),&ctype); /* get just ctype */
  switch (ctype) {
  case e_token:
    newrel->type = e_rel_token;
    break;
#if 0
  case e_opcode:
    ASC_PANIC("switching on e_opcode");
    break;
  case e_glassbox:
    newrel->type = e_rel_glassbox;
    break;
#endif
  case e_blackbox:
    newrel->type = e_rel_blackbox;
    break;
  default:
    FPRINTF(stderr,"Unknown relation type in rel_create\n");
    break;
  }
  return(newrel);
}

SlvBackendToken rel_instance(struct rel_relation *rel){
  if (rel==NULL) return NULL;
  return (SlvBackendToken) rel->instance;
}

void rel_write_name(slv_system_t sys,struct rel_relation *rel,FILE *fp){
  if(rel == NULL || fp==NULL) return;
  if(sys!=NULL) {
    WriteInstanceName(fp,rel_instance(rel),slv_instance(sys));
  }else{
    WriteInstanceName(fp,rel_instance(rel),NULL);
  }
}

void rel_destroy(struct rel_relation *rel){
   struct Instance *inst;
   switch (rel->type) {
   case e_rel_token:
     break;
   default:
     break;
   }
   if (rel->nodeinfo) {
     rel->nodeinfo->cache = NULL;
     ascfree(rel->nodeinfo);
     rel->nodeinfo = NULL;
   }
   ascfree((POINTER)rel->incidence);
   inst = IPTR(rel->instance);
   if (inst) {
     if (GetInterfacePtr(inst)==rel) { /* we own the pointer -- reset it. */
       SetInterfacePtr(inst,NULL);
     }
   }
   ascfree((POINTER)rel);
}

uint32 rel_flags( struct rel_relation *rel){
  return rel->flags;
}

void rel_set_flags(struct rel_relation *rel, uint32 flags){
  rel->flags = flags;
}

uint32 rel_flagbit(struct rel_relation *rel, uint32 one){
  if (rel==NULL || rel->instance == NULL) {
    FPRINTF(stderr,"ERROR: rel_flagbit called with bad var.\n");
    return 0;
  }
  return (rel->flags & one);
}

void rel_set_flagbit(struct rel_relation *rel, uint32 field,uint32 one){
  if (one) {
    rel->flags |= field;
  } else {
    rel->flags &= ~field;
  }
}

/* this needs to be reimplemented properly. */
boolean rel_less(struct rel_relation *rel){
  switch( RelationRelop(GetInstanceRelationOnly(IPTR(rel->instance))) ) {
  case e_notequal:
  case e_less:
  case e_lesseq:
    return TRUE;
  default:
    return FALSE;
  }
}

/**
	this needs to be reimplemented properly.
*/
boolean rel_equal( struct rel_relation *rel){
  switch( RelationRelop(GetInstanceRelationOnly(IPTR(rel->instance))) ) {
  case e_equal:
  case e_lesseq:
  case e_greatereq:
    return TRUE;
  default:
    return FALSE;
  }
}

boolean rel_greater(struct rel_relation *rel){
  switch( RelationRelop(GetInstanceRelationOnly(IPTR(rel->instance))) ) {
  case e_notequal:
  case e_greater:
  case e_greatereq:
    return TRUE;
  default:
    return FALSE;
  }
}

static enum rel_enum compenum2relenum(enum Expr_enum t){
  switch (t) {
  case e_equal:    return e_rel_equal;
  case e_less:     return e_rel_less;
  case e_greater:  return e_rel_greater;
  case e_lesseq:   return e_rel_lesseq;
  case e_greatereq:return e_rel_greatereq;
  default:
    FPRINTF(ASCERR,"ERROR (rel.c): compenum2relenum miscalled.\n");
    return e_rel_not_equal;
  }
}

enum rel_enum rel_relop( struct rel_relation *rel){
  return
    compenum2relenum(RelationRelop(
        GetInstanceRelationOnly(IPTR(rel->instance))));
}

char *rel_make_name(slv_system_t sys,struct rel_relation *rel){
	if(sys == NULL){
		ERROR_REPORTER_HERE(ASC_PROG_ERR,"called with NULL system.");
		return NULL;
	}
	return WriteInstanceNameString(IPTR(rel->instance),IPTR(slv_instance(sys)));
}

int32 rel_mindex( struct rel_relation *rel){
   return( rel->mindex );
}

void rel_set_mindex( struct rel_relation *rel, int32 mindex)
{
   rel->mindex = mindex;
}

int32 rel_sindex( const struct rel_relation *rel){
   return( rel->sindex );
}

void rel_set_sindex( struct rel_relation *rel, int32 sindex)
{
   rel->sindex = sindex;
}

int32 rel_model(const struct rel_relation *rel){
   return((const int32) rel->model );
}

void rel_set_model( struct rel_relation *rel, int32 mindex)
{
   rel->model = mindex;
}

real64 rel_residual(struct rel_relation *rel){
   return( RelationResidual(GetInstanceRelationOnly(IPTR(rel->instance))));
}

void rel_set_residual( struct rel_relation *rel, real64 residual){
  struct relation *reln;
  reln = (struct relation *)GetInstanceRelationOnly(IPTR(rel->instance));
  SetRelationResidual(reln,residual);
}

real64 rel_multiplier(struct rel_relation *rel){
  return( RelationMultiplier(GetInstanceRelationOnly(IPTR(rel->instance))));
}

void rel_set_multiplier(struct rel_relation *rel, real64 multiplier){
  struct relation *reln;
  reln = (struct relation *)GetInstanceRelationOnly(IPTR(rel->instance));
  SetRelationMultiplier(reln,multiplier);
}

real64 rel_nominal( struct rel_relation *rel){
  return( RelationNominal(GetInstanceRelationOnly(IPTR(rel->instance))));
}

void rel_set_nominal( struct rel_relation *rel, real64 nominal){
  struct relation *reln;
  reln = (struct relation *)GetInstanceRelationOnly(IPTR(rel->instance));
  SetRelationNominal(reln,nominal);
}

/**
	too bad there's no entry point that rel must call before being used
	generally, like the FindType checking stuff in var.c
*/
static void check_included_flag(void){
  if (INCLUDED_R == NULL || AscFindSymbol(INCLUDED_R) == NULL) {
    INCLUDED_R = AddSymbol("included");
  }
}
uint32 rel_included( struct rel_relation *rel){
	struct Instance *c;
	check_included_flag();
	c = ChildByChar(IPTR(rel->instance),INCLUDED_R);
	if( c == NULL ) {
		FPRINTF(stderr,"ERROR:  (rel) rel_included\n");
		FPRINTF(stderr,"        No 'included' field in relation.\n");
		WriteInstance(stderr,IPTR(rel->instance));
		return FALSE;
	}
	rel_set_flagbit(rel,REL_INCLUDED,GetBooleanAtomValue(c));
	return( GetBooleanAtomValue(c) );
}

void rel_set_included( struct rel_relation *rel, uint32 included){
	struct Instance *c;
	check_included_flag();
	c = ChildByChar(IPTR(rel->instance),INCLUDED_R);
	if( c == NULL ) {
		FPRINTF(stderr,"ERROR:  (rel) rel_set_included\n");
		FPRINTF(stderr,"        No 'included' field in relation.\n");
		WriteInstance(stderr,IPTR(rel->instance));
		return;
	}
	SetBooleanAtomValue(c,included,(unsigned)0);
	rel_set_flagbit(rel,REL_INCLUDED,included);
}

int32 rel_apply_filter( const struct rel_relation *rel, const rel_filter_t *filter){
  if (rel==NULL || filter==NULL) {
    FPRINTF(stderr,"rel_apply_filter miscalled with NULL\n");
    return FALSE;
  }
  /* AND to mask off irrelevant bits in flags and match value, then compare */
  return ( (filter->matchbits & rel->flags) ==
           (filter->matchbits & filter->matchvalue)
         );
}

/**
	Implementation function for rel_n_incidences().  Do not call
	this function directly - use rel_n_incidences() instead.
*/
int32 rel_n_incidencesF(struct rel_relation *rel){
  if (rel!=NULL) return rel->n_incidences;
  FPRINTF(stderr,"rel_n_incidences miscalled with NULL\n");
  return 0;
}

void rel_set_incidencesF(struct rel_relation *rel,int32 n,
		struct var_variable **i
){
  if(rel!=NULL && n >=0) {
    if (n && i==NULL) {
      FPRINTF(stderr,"rel_set_incidence miscalled with NULL ilist\n");
    }
    rel->n_incidences = n;
    rel->incidence = i;
    return;
  }
  FPRINTF(stderr,"rel_set_incidence miscalled with NULL or n < 0\n");
}

const struct var_variable **rel_incidence_list( struct rel_relation *rel){
  if (rel==NULL) return NULL;
  return( (const struct var_variable **)rel->incidence );
}

struct var_variable **rel_incidence_list_to_modify( struct rel_relation *rel){
  if (rel==NULL) return NULL;
  return( (struct var_variable **)rel->incidence );
}


/*
 * External relations access. See far below for more details.
 */

struct rel_extnode *rel_extnodeinfo( struct rel_relation *rel)
{
  return(rel->nodeinfo);
}

/**
	Flag relations that contain a derivative (with respect to the indep var).

	This function must only be run once the Integrator's analysis has been
	completed, otherwise the var_deriv flags won't have been set.

	@return 1 if the rel was differential, 0 if not.
*/
int rel_classify_differential(struct rel_relation *rel){
	const struct var_variable **vars;
	int i, diff = 0;
	vars = rel_incidence_list(rel);
	for(i=0; i < rel_n_incidences(rel); ++i){
		if(var_deriv(vars[i])){
			diff = 1;
			break;
		}
	}
	rel_set_differential(rel,diff);
	return diff;
}
