/*
 *  Type definition lint module
 *  by Ben Allan
 *  Created: 9/16/96
 *  Version: $Revision: 1.48 $
 *  Version control file: $RCSfile: typelint.c,v $
 *  Date last modified: $Date: 1998/07/23 13:57:59 $
 *  Last modified by: $Author: ballan $
 *
 *  This file is part of the Ascend Language Interpreter.
 *
 *  Copyright (C) 1996 Benjamin Andrew Allan
 *
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
 */

#include <math.h>
#include <ctype.h>
#include <ascend/general/platform.h>
#include <ascend/general/list.h>
#include <ascend/general/dstring.h>

#include "functype.h"
#include "expr_types.h"
#include "stattypes.h"
#include "statement.h"
#include "slist.h"
#include "statio.h"
#include "symtab.h"
#include "module.h"
#include "library.h"
#include "child.h"
#include "vlist.h"
#include "vlistio.h"
#include "name.h"
#include "nameio.h"
#include "when.h"
#include "select.h"
#include "switch.h"
#include "sets.h"
#include "exprs.h"
#include "forvars.h"
#include "syntax.h"
#include "setinstval.h"
#include "childinfo.h"
#include "evaluate.h"
#include "proc.h"
#include "type_desc.h"
#include "typedef.h"
#include "typelint.h"

struct errormessage {
  const char *str;
  int level;
};

/*
 * Messages with a level < g_parser_warnings do not get printed.
 * g_parser_warnings ==
 * 2 => suppress style warnings.
 * 3 => suppress warnings
 * 4 => suppress errors
 * 5 => suppress fatality explanations.
 * The effect is obviously cumulative.
 */
int g_parser_warnings = 0;
#define DEM_LVL_OK 0
#define DEM_LVL_NOTE 1
#define DEM_LVL_WARN 2
#define DEM_LVL_ERR 3
#define DEM_LVL_FATAL 4
#define DEM_LVL_UNKN 5

static struct errormessage g_DefinitionErrorMessages[] = {
  /*0*/{"No error", DEM_LVL_OK},
  {"Name of a part has been reused (failure)",DEM_LVL_ERR},
  {"Name of a part has incorrect structure (failure)",DEM_LVL_ERR},
  {"Name of a part/variable that does not exist",DEM_LVL_ERR},
  {"Name of undefined function (failure)",DEM_LVL_ERR},
  /*5*/{"Illegal right hand side type in ALIASES",DEM_LVL_ERR},
  {"Illegal expression encountered (parser, lint or memory error, failure)",DEM_LVL_ERR},
  {"Illegal parameter type for WILL_BE (failure)",DEM_LVL_ERR},
  {"Statement not allowed in context (failure)",DEM_LVL_ERR},
  {"Illegal parameter (more than 1 name per IS_A/WILL_BE - failure)",DEM_LVL_ERR},
  /*10*/{"Illegal parameter reduction (LHS not defined by ancestral IS_A)",DEM_LVL_ERR},
  {"Illegal parameter reduction (reassigning part - failure)",DEM_LVL_ERR},
  {"Illegal parameter type for IS_A (punting). Try WILL_BE?",DEM_LVL_ERR},
  {"Unverifiable name or illegal type in a relation",DEM_LVL_WARN},
  {"Unverifiable name or illegal type in RHS of :==",DEM_LVL_WARN},
  /*15*/{"Incorrect number of arguments passed to parameterized type. (failure)",DEM_LVL_ERR},
  {"Incorrect argument passed to parameterized type. (failure)",DEM_LVL_ERR},
  {"Statement possibly modifies type of parameter. (failure)",DEM_LVL_ERR},
  {"Unverifiable LHS of :==, possibly undefined part or non-constant type",DEM_LVL_WARN},
  {"Unverifiable LHS of :=, possibly undefined part or non-variable type",DEM_LVL_WARN},
  /*20*/{"Illegal type or undefined name in RHS of := (failure)",DEM_LVL_ERR},
  {"Object with incompatible type specified in refinement",DEM_LVL_ERR},
  {"FOR loop index shadows previous declaration (failure)",DEM_LVL_ERR},
  {"Unverifiable name or illegal type in ARE_ALIKE",DEM_LVL_WARN},
  {"Illegal parameterized type in ARE_ALIKE",DEM_LVL_ERR},
  /*25*/{"Illegal set of array elements in ARE_ALIKE",DEM_LVL_ERR},
  {"WILL(_NOT)_BE_THE_SAME contains incorrect names.",DEM_LVL_ERR},
  {"WILL_BE in WHERE clause not yet implemented.",DEM_LVL_ERR},
  {"Mismatched alias array subscript and set name in ALIASES-ISA.",DEM_LVL_ERR},
  {"Unverifiable FOR loop set, possibly undefined part or non-constant type",DEM_LVL_WARN},
  /*30*/{"rELECT statements are not allowed inside a FOR loop",DEM_LVL_ERR},
  {"Illegal relation -- too many relational operators (<,>,=,<=,>=,<>)",DEM_LVL_ERR},
  {"Illegal logical relation -- too many equality operators (!=,==)",DEM_LVL_ERR},
  {"Illegal USE found outside WHEN statement",DEM_LVL_ERR},
  {"Illegal FOR used in a method must be FOR/DO",DEM_LVL_ERR},
  /*35*/{"Illegal FOR used in body must be FOR/CREATE",DEM_LVL_ERR},
  {"Illegal ATOM attribute or relation type in ARE_THE_SAME",DEM_LVL_ERR},
  {":= used outside METHOD makes models hard to debug or reuse",DEM_LVL_NOTE},
  {"Illegal BREAK used outside loop or SWITCH.",DEM_LVL_ERR},
  {"Illegal CONTINUE used outside loop.",DEM_LVL_ERR},
  /*40*/{"Illegal FALL_THROUGH used outside SWITCH.",DEM_LVL_ERR},
  {"Incorrect nested argument definition for MODEL. (failure)",DEM_LVL_ERR},
  {"Indexed relation has incorrect subscripts. (failure)",DEM_LVL_ERR},
  {"Illegal FOR used in WHERE statements must be FOR/CHECK",DEM_LVL_ERR},
  {"Illegal FOR used in parameter definitions must be FOR/EXPECT",DEM_LVL_ERR},
  /*45*/{"Miscellaneous style",DEM_LVL_NOTE},
  {"Miscellaneous warning",DEM_LVL_WARN},
  {"Miscellaneous error",DEM_LVL_ERR},
  {"Unknown error encountered in statement",DEM_LVL_UNKN}
};

void TypeLintErrorAuxillary(FILE *f, char *str, enum typelinterr err,
                            int uselabel)
{
  assert(f!=NULL);
  assert(str!=NULL);
  if (g_DefinitionErrorMessages[err].level < g_parser_warnings) {
    /* no element of gDEM should have .level == 0 */
    return;
  }
  if (uselabel) {
    FPRINTF(f,"%s%s",StatioLabel(g_DefinitionErrorMessages[err].level),str);
  } else {
    FPRINTF(f,"%s",str);
  }
}

void TypeLintError(FILE *f, CONST struct Statement *stat,enum typelinterr err){
  assert(f!=NULL);
  assert(stat!=NULL);
  if (g_DefinitionErrorMessages[err].level < g_parser_warnings) {
    /* no element of gDEM should have .level == 0 */
    return;
  }
  error_severity_t sev;
  switch(g_DefinitionErrorMessages[err].level){
  case 1: sev = ASC_USER_NOTE; break;
  case 2: sev = ASC_USER_WARNING; break;
  case 3: sev = ASC_USER_ERROR; break;
  case 4: sev = ASC_PROG_FATAL; break;
  default:
	  sev = ASC_PROG_WARNING; break;
  }
  WriteStatementError(sev,stat,1,g_DefinitionErrorMessages[err].str);
}

void TypeLintName(FILE *f, CONST struct Name *n, char *m)
{
  TypeLintNameMsg(f,n,m,0);
}

void TypeLintNameMsg(FILE *f, CONST struct Name *n, char *m,int level)
{
  assert(f!=NULL);
  assert(n!=NULL);
  assert(m!=NULL);
  if (level > 0 && level < g_parser_warnings) {
    return;
  }
  switch(level){
	case 2:
		ERROR_REPORTER_START_NOLINE(ASC_PROG_WARNING);
		break;
	case 3:
		ERROR_REPORTER_START_NOLINE(ASC_PROG_ERROR);
		break;
	case 4:
		ERROR_REPORTER_START_NOLINE(ASC_PROG_FATAL);
		break;
	default:
		ERROR_REPORTER_START_NOLINE(ASC_PROG_NOTE);
  }
  FPRINTF(f,"%s",m);
  WriteName(f,n);
  error_reporter_end_flush();
}

void TypeLintNameNode(FILE *f, CONST struct Name *n, char *m)
{
  assert(f!=NULL);
  assert(n!=NULL);
  assert(m!=NULL);
  FPRINTF(f,"%s",m);
  WriteNameNode(f,n);
  FPRINTF(f,"\n");
}

void TypeLintNameNodeMsg(FILE*f, CONST struct Name *n, char *m)
{
  assert(f!=NULL);
  assert(n!=NULL);
  assert(m!=NULL);
  if (3 < g_parser_warnings) {
    return;
  }
  ERROR_REPORTER_START_NOLINE(ASC_USER_ERROR);
  FPRINTF(f,"%s '",m);
  WriteNameNode(f,n);
  FPRINTF(f,"'");
  error_reporter_end_flush();
}

/*
 * Returns position of first compound name in list
 * if a CompoundName (e.g. a.b) is found in the
 * list. Array names are simple e.g. a[i][j],
 * but compound names include names like a[i][j].b.
 * Returns -1 OTHERWISE.
 */
static
int CompoundNameInList(CONST struct VariableList *vl)
{
  int rval = 0;
  while (vl != NULL) {
    if (NameCompound(NamePointer(vl))) {
      return rval;
    }
    vl = NextVariableNode(vl);
    rval++;
  }
  return -1;
}

/*
 * pulls the single name element out of the last set name (subscript)
 * in the alias array name of the ALIASES-IS_A statement.
 */
static
CONST struct Name *ExtractARRName(CONST struct Statement *s)
{
  CONST struct Name *n;
  CONST struct Set *sn;
  CONST struct Expr *ex;
  int len;

  assert(s!=NULL);
  assert(StatementType(s)==ARR);

  /* get name from var. */
  n = NamePointer(ArrayStatAvlNames(s));
  len = NameLength(n);
  if (len < 2) {
    return NULL;
  }
  /* get last subscript in name */
  while (len > 1) {
    len--;
    n = NextName(n);
  }
  /* must be subscript */
  if (NameId(n)!=0) {
    return NULL;
  }
  sn = NameSetPtr(n);
  /* must be single set, not range */
  if (SetType(sn)!=0) {
    return NULL;
  }
  ex = GetSingleExpr(sn);
  /* must be name of var we're to IS_A elsewhere */
  if (ExprListLength(ex) != 1 || ExprType(ex) !=e_var) {
    return NULL;
  }
  return ExprName(ex);
}

static int g_tlibs_depth=0;
/* counter to avoid redundant spew */
enum typelinterr TypeLintIllegalBodyStats(FILE *fp,
                                          symchar *name,
                                          CONST struct StatementList *sl,
                                          unsigned int context)
{
  unsigned long c,len;
  struct gl_list_t *gl;
  struct Statement *s;
  struct TypeDescription *d;
  enum typelinterr rval = DEF_OKAY, tmperr;
  struct SelectList *selcase;
  struct WhenList *wcase;
  struct StatementList *slint;

  g_tlibs_depth++;
  assert(name !=NULL);
  len = StatementListLength(sl);
  if (len == 0L) {
    g_tlibs_depth--;
    return rval;
  }
  gl = GetList(sl);
  for(c=1; c<=len; c++) {
    s = (struct Statement *)gl_fetch(gl,c);
    switch(StatementType(s)) {
    case ISA:
      d = FindType(GetStatType(s));
      if (GetBaseType(d)== model_type /* || GetBaseType(d) == patch_type */ ) {
        /* check arg list length. can't do types until after. */
        if (GetModelParameterCount(d) != SetLength(GetStatTypeArgs(s))) {
          if (TLINT_ERROR) {
            FPRINTF(fp,"%sType %s needs %u arguments. Got %lu.\n",
              StatioLabel(3),
              SCP(GetStatType(s)),
              GetModelParameterCount(d),
              SetLength(GetStatTypeArgs(s)));
          }
          rval = DEF_ARGNUM_INCORRECT;
          TypeLintError(fp,s,rval);
          break;
        }
      }
      /* fall through */
    case ALIASES:
      /* check simple name */
      if (CompoundNameInList(GetStatVarList(s)) != -1) {
        if (TLINT_ERROR) {
          FPRINTF(fp,
            "%sCannot create parts in another object with IS_A/ALIASES.\n",
	    StatioLabel(3));
          FPRINTF(fp,"%sName %d is incorrect.\n",
            StatioLabel(3),
            CompoundNameInList(GetStatVarList(s)));
        }
        rval = DEF_NAME_INCORRECT;
        TypeLintError(fp,s,rval);
      }
      break;
    case ARR:
      /* like ALIASES, only different. */
      /* assumptions about the parser: the avlname and setname varlists
       * will only have 1 name pointer in them, as the parser stops
       * longer lists.
       */
      /* check simple names */
      if (CompoundNameInList(ArrayStatAvlNames(s)) != -1) {
        if (TLINT_ERROR) {
          FPRINTF(fp,"%sCannot create parts in another object with ALIASES.\n",
                  StatioLabel(3));
        }
        rval = DEF_NAME_INCORRECT;
        TypeLintError(fp,s,rval);
      }
      if (CompoundNameInList(ArrayStatSetName(s)) != -1) {
        if (TLINT_ERROR) {
          FPRINTF(fp,"%sCannot create parts in another object with IS_A.\n",
                  StatioLabel(3));
        }
        rval = DEF_NAME_INCORRECT;
        TypeLintError(fp,s,rval);
      }
      if (CompareNames(NamePointer(ArrayStatSetName(s)),ExtractARRName(s))) {
        if (TLINT_ERROR) {
          FPRINTF(fp,"%sName of set defined with IS_A ",StatioLabel(3));
          WriteVariableList(fp,ArrayStatSetName(s));
          FPRINTF(fp,"\n  must match last subscript of ALIASES-ISA LHS.\n");
        }
        rval = DEF_ARR_INCORRECT;
        TypeLintError(fp,s,rval);
      }
      break;
    case REL:
      /* check simple name */
      if (NameCompound(RelationStatName(s)) != 0) {
        if (TLINT_ERROR) {
          FPRINTF(fp,"%sCannot create relations in another object.\n",
                  StatioLabel(3));
        }
        rval = DEF_NAME_INCORRECT;
        TypeLintError(fp,s,rval);
      }
      if (NumberOfRelOps(RelationStatExpr(s)) > 1) {
        rval = DEF_TOOMANY_RELOP;
        TypeLintError(fp,s,rval);
      }
      break;
    case LOGREL:
      /* check simple name */
      if (NameCompound(LogicalRelStatName(s)) != 0) {
        if (TLINT_ERROR) {
          FPRINTF(fp,"%sCannot create logical relations in another object.\n",
                  StatioLabel(3));
        }
        rval = DEF_NAME_INCORRECT;
        TypeLintError(fp,s,rval);
      }
      if (NumberOfRelOps(LogicalRelStatExpr(s)) > 1) {
        rval = DEF_TOOMANY_LOGOP;
        TypeLintError(fp,s,rval);
      }
      break;
    case IRT:
      d = FindType(GetStatType(s));
      if (GetBaseType(d)== model_type /* || GetBaseType(d) == patch_type */) {
        /* check arg list length. can't do types until after. */
        if (GetModelParameterCount(d) != SetLength(GetStatTypeArgs(s))) {
          if (TLINT_ERROR) {
            FPRINTF(fp,"%sType %s needs %u arguments. Got %lu.\n",
              StatioLabel(3),
              SCP(GetStatType(s)),
              GetModelParameterCount(d),
              SetLength(GetStatTypeArgs(s)));
          }
          rval = DEF_ARGNUM_INCORRECT;
          TypeLintError(fp,s,rval);
          break;
        }
      }
      break;
    case ATS:
      break;
    case AA:
      if(TLINT_STYLE){
        FPRINTF(fp,"%sType \"%s\" contains AA:\n",
                StatioLabel(1),SCP(name));
        WriteStatement(fp,s,2);
      }
      break;
    case LNK:
      if(TLINT_STYLE){
        FPRINTF(fp,"%sType \"%s\" contains LNK:\n",
                StatioLabel(1),SCP(name));
        WriteStatement(fp,s,2);
      }
      break;
    case UNLNK:
      if(TLINT_STYLE){
        FPRINTF(fp,"%sType \"%s\" contains UNLNK:\n",
                StatioLabel(1),SCP(name));
        WriteStatement(fp,s,2);
      }
      break;
    case FOR:
      if (ForContainsSelect(s)) {
        rval = DEF_ILLEGAL_SELECT;
        TypeLintError(fp,s,rval);
      }
      if (ForLoopKind(s) != fk_create) {
        rval = DEF_FOR_NOTBODY; /* err fatal to type */
        TypeLintError(fp,s,rval);
      } else {
        tmperr = TypeLintIllegalBodyStats(fp,name,ForStatStmts(s),
                                          (context | context_FOR));
        if (tmperr != DEF_OKAY) {
          /* don't want DEF_OKAY to wipe out rval if badness already found.
           */
          rval = tmperr;
        }
      }
      break;
    case ASGN:
      TypeLintError(fp,s,DEF_STAT_BODYASGN);
      TypeLintErrorAuxillary(fp,
          "  Move default assignments to METHOD default_self.\n",
          DEF_STAT_BODYASGN,FALSE);
      break;
    case CASGN:
      break;
    case FNAME:
      if ((context & context_WHEN) == 0) {
        rval = DEF_USE_NOTWHEN;
        TypeLintError(fp,s,rval);
        if ((context & context_SELECT) != 0)  {
          FPRINTF(fp,"  Perhaps the surrounding SELECT should be WHEN?\n");
        }
      }
      break;
    case WHEN:
      /* check simple name */
     /* vicente, what's up with this? we can name whens? */
      if (NameCompound(WhenStatName(s)) != 0) {
        FPRINTF(fp,"  Cannot create whens in another object.\n");
        rval = DEF_NAME_INCORRECT;
        TypeLintError(fp,s,rval);
      }
      wcase = WhenStatCases(s);
      while ( wcase!=NULL ) {
        slint = WhenStatementList(wcase);
        tmperr = TypeLintIllegalBodyStats(fp,name,slint,
                                          (context | context_WHEN));
        if (tmperr != DEF_OKAY) {
          rval = tmperr;
        }
        wcase = NextWhenCase(wcase);
      }
      break;
    case SELECT:
      selcase = SelectStatCases(s);
      while ( selcase!=NULL ) {
        slint = SelectStatementList(selcase);
        tmperr = TypeLintIllegalBodyStats(fp,name,slint,
                                          (context | context_SELECT));
        if (tmperr != DEF_OKAY) {
          rval = tmperr;
        }
        selcase = NextSelectCase(selcase);
      }
      break;
    case EXT:
      if (ExternalStatMode(s) == ek_method) {
        Asc_StatErrMsg_NotAllowedDeclarative(fp,s,"");
        rval = DEF_STAT_MISLOCATED;
      }
      break;
    case REF:
      break;
    case COND:
      tmperr = TypeLintIllegalBodyStats(fp,name,CondStatList(s),
                                        (context | context_COND));
      if (tmperr != DEF_OKAY) {
        rval = tmperr;
      }
      break;
    /* Stuff illegal in body */
    case SWITCH:
      TypeLintError(fp,s,DEF_STAT_MISLOCATED);
      rval = DEF_STAT_MISLOCATED;
      FPRINTF(fp,"  Perhaps SWITCH should be WHEN or SELECT?\n");
      break;
    case FLOW: /* fallthrough */
    case WHILE:
      TypeLintError(fp,s,DEF_STAT_MISLOCATED);
      rval = DEF_STAT_MISLOCATED;
      FPRINTF(fp,"  Flow controls are allowed only in methods.\n");
      break;
    case IF:
      TypeLintError(fp,s,DEF_STAT_MISLOCATED);
      rval = DEF_STAT_MISLOCATED;
      FPRINTF(fp,"  Perhaps IF should be WHEN or SELECT?\n");
      break;
    case RUN:
    case CALL:
    case WILLBE:
    case WBTS:
    case WNBTS:
    default:
      TypeLintError(fp,s,DEF_STAT_MISLOCATED);
      rval = DEF_STAT_MISLOCATED;
    }
  }
  if (rval != DEF_OKAY && g_tlibs_depth < 2 /* at top */) {
    FPRINTF(fp,"  Errors detected in declarative section of '%s'\n",
            SCP(name));
  }
  g_tlibs_depth--;
  return rval;
}

enum typelinterr TypeLintIllegalParamStats(FILE * fp,
                                           symchar *name,
                                           CONST struct StatementList *sl)
{
  unsigned long c,len;
  struct gl_list_t *gl;
  struct Statement *s;
  enum typelinterr rval = DEF_OKAY;
  symchar *tid;
  CONST struct TypeDescription *t;

  assert(name !=NULL);
  len = StatementListLength(sl);
  if (len == 0L) {
    return rval;
  }
  gl = GetList(sl);
  for(c=1; c<=len;c++) {
    s = (struct Statement *)gl_fetch(gl,c);
    switch(StatementType(s)) {
    case WILLBE:
      /* check rhs for bad type here */
      tid = GetStatType(s);
      t = FindType(tid);
      switch (GetBaseType(t)) {
      case relation_type:
      case logrel_type:
      case when_type:
        rval = DEF_ILLEGAL_PARAM;
        TypeLintError(fp,s,rval);
        break;
#if 0
      case patch_type:
#endif
      case model_type:
        /* check arg list length. can't do types until after. */
        if ( SetLength(GetStatTypeArgs(s)) != 0L) {
          if (TLINT_ERROR) {
            FPRINTF(fp,
              "%sArguments defined with WILL_BE cannot " /* no comma */
              "have arguments. Got %lu.\n",
            StatioLabel(3),
            SetLength(GetStatTypeArgs(s)));
            FPRINTF(fp, "%sYou may want to use WILL_BE_THE_SAME instead.\n",
                    StatioLabel(2));
          }
          rval = DEF_ARGDEF_INCORRECT;
          TypeLintError(fp,s,rval);
        }
        break;
      default:
        break;
      }
      if (VariableListLength(GetStatVarList(s)) > 1L) {
        rval = DEF_MULTI_PARAM;
        TypeLintError(fp,s,rval);
        break; /* no point complaining twice */
      }
      /* check simple names */
      if (CompoundNameInList(GetStatVarList(s)) != -1) {
        if (TLINT_ERROR) {
          FPRINTF(fp,"%sCannot use . in defining MODEL arguments.\n",
            StatioLabel(3));
        }
        rval = DEF_NAME_INCORRECT;
        TypeLintError(fp,s,rval);
      }
      break;
    case ISA:
      /* check rhs for bad type here */
      tid = GetStatType(s);
      t = FindType(tid);
      if (BaseTypeIsSet(t)==0 && BaseTypeIsConstant(t)==0) {
        rval = DEF_ILLEGAL_VALPAR;
        TypeLintError(fp,s,rval);
        break; /* no point complaining twice */
      }
      if (VariableListLength(GetStatVarList(s)) > 1L) {
        rval = DEF_MULTI_PARAM;
        TypeLintError(fp,s,rval);
        break; /* no point complaining twice */
      }
      if (CompoundNameInList(GetStatVarList(s)) != -1) {
        if (TLINT_ERROR) {
          FPRINTF(fp,"%sCannot create parts in another object with IS_A.\n",
            StatioLabel(3));
          FPRINTF(fp,"  Name %d is incorrect.\n",
            CompoundNameInList(GetStatVarList(s)));
        }
        rval = DEF_NAME_INCORRECT;
        TypeLintError(fp,s,rval);
      }
      break;
    case ALIASES: /* huge fallthrough */
    case ARR:
    case IRT:
    case ATS:
    case WBTS:
    case WNBTS:
    case AA:
    case LNK:
    case UNLNK:
    case FOR: /* eventually for legal and fk_expect required */
    case REL:
    case LOGREL:
    case ASGN:
    case CASGN:
    case WHEN:
    case FNAME:
    case SELECT:
    case SWITCH:
    case EXT:
    case REF:
    case COND:
    case RUN:
    case CALL:
    case FLOW:
    case WHILE:
    case IF:
    default:
      TypeLintError(fp,s,DEF_STAT_MISLOCATED);
      rval = DEF_STAT_MISLOCATED;
    }
  }
  if (rval != DEF_OKAY) {
    FPRINTF(fp,"  Errors detected in argument definitions of '%s'\n",SCP(name));
  }
  return rval;
}

/* could stand to be a lot more rigorous */
enum typelinterr
TypeLintIllegalWhereStats(FILE * fp,
                          symchar *name,
                          CONST struct StatementList *sl)
{
  unsigned long c,len;
  struct gl_list_t *gl;
  struct Statement *s;
  enum typelinterr rval = DEF_OKAY, tmperr;

  assert(name !=NULL);
  len = StatementListLength(sl);
  if (len == 0L) {
    return rval;
  }
  gl = GetList(sl);
  for(c=1; c<=len;c++) {
    s = (struct Statement *)gl_fetch(gl,c);
    switch(StatementType(s)) {
    case FOR:
      if (ForLoopKind(s) != fk_check) {
        rval = DEF_FOR_NOTCHECK;
        TypeLintError(fp,s,rval);
      } else {
        tmperr = TypeLintIllegalWhereStats(fp,name,ForStatStmts(s));
        if (tmperr != DEF_OKAY) {
          rval = tmperr;
        }
      }
      break;
    case WBTS:
    case WNBTS:
    case LOGREL:
    case REL:
      break;
    case CASGN:
    case ALIASES:
    case ARR:
    case ISA:
    case IRT:
    case ATS:
    case AA:
    case LNK:
    case UNLNK:
    case ASGN:
    case WHEN:
    case FNAME:
    case SELECT:
    case SWITCH:
    case EXT:
    case REF:
    case COND:
    case RUN:
    case CALL:
    case IF:
    case FLOW:
    case WHILE:
    case WILLBE:
    default:
      TypeLintError(fp,s,DEF_STAT_MISLOCATED);
      rval = DEF_STAT_MISLOCATED;
    }
  }
  if (rval != DEF_OKAY) {
    FPRINTF(fp," Errors detected in WHERE statements of '%s'\n",SCP(name));
  }
  return rval;
}

enum typelinterr
TypeLintIllegalReductionStats(FILE * fp,
                              symchar *name,
                              CONST struct StatementList *sl)
{
  unsigned long c,len;
  struct gl_list_t *gl;
  struct Statement *s;
  enum typelinterr rval = DEF_OKAY;

  assert(name !=NULL);
  len = StatementListLength(sl);
  if (len == 0L) {
    return rval;
  }
  gl = GetList(sl);
  for(c=1; c<=len;c++) {
    s = (struct Statement *)gl_fetch(gl,c);
    switch(StatementType(s)) {
    case CASGN:
      if (NameCompound(AssignStatVar(s)) != 0) {
        if (TLINT_ERROR) {
          FPRINTF(fp,"%sCannot assign parts in an object being passed in.\n",
	    StatioLabel(3));
        }
        rval = DEF_NAME_INCORRECT;
        TypeLintError(fp,s,rval);
      }
      break;
    case ALIASES:
    case ARR:
    case ISA:
    case IRT:
    case ATS:
    case WBTS:
    case WNBTS:
    case AA:
    case LNK:
    case UNLNK:
    case FOR: /* probably should be legal now and require fk_create */
    case REL:
    case LOGREL:
    case ASGN:
    case WHEN:
    case FNAME:
    case SELECT:
    case SWITCH:
    case EXT:
    case REF:
    case COND:
    case RUN:
    case CALL:
    case IF:
    case FLOW:
    case WHILE:
    case WILLBE:
    default:
      TypeLintError(fp,s,DEF_STAT_MISLOCATED);
      rval = DEF_STAT_MISLOCATED;
    }
  }
  if (rval != DEF_OKAY) {
    FPRINTF(fp,"  Errors detected in parameter assignments of '%s'\n",SCP(name));
  }
  return rval;
}

static
enum typelinterr
TypeLintIllegalMethodStatList(FILE *fp,
                              symchar *name, symchar *pname,
                              struct StatementList *sl,
                              unsigned int context)
{
  unsigned long c,len;
  struct gl_list_t *gl;
  struct Statement *s;
  struct SwitchList *sw;
  struct StatementList *sublist;
  enum typelinterr rval = DEF_OKAY, tmperr;

  if (sl == NULL) {
    return rval;
  }
  len = StatementListLength(sl);
  gl = GetList(sl);
  for(c=1; c<=len;c++) {
    s = (struct Statement *)gl_fetch(gl,c);
    switch(StatementType(s)) {
    case CASGN:
    case ALIASES:
    case ARR:
    case ISA:
    case IRT:
    case ATS:
    case WBTS:
    case WNBTS:
    case AA:
    case REF:
    case COND:
    case WILLBE:
    case FNAME:
      Asc_StatErrMsg_NotAllowedMethod(fp,s,"");
      rval = DEF_STAT_MISLOCATED;
      break;
    case REL:
    case LOGREL:
      Asc_StatErrMsg_NotAllowedMethod(fp,s,"Perhaps '=' or '==' should be ':='?");
      rval = DEF_STAT_MISLOCATED;
      break;
    case WHEN:
      Asc_StatErrMsg_NotAllowedMethod(fp,s,"Perhaps WHEN should be SWITCH?");
      rval = DEF_STAT_MISLOCATED;
      break;
    case SELECT:
      Asc_StatErrMsg_NotAllowedMethod(fp,s,"Perhaps SELECT should be SWITCH?");
      rval = DEF_STAT_MISLOCATED;
      break;
    case FOR:
      if (ForLoopKind(s) != fk_do) {
        rval = DEF_FOR_NOTMETH;
        TypeLintError(fp,s,rval);
      } else {
        tmperr = TypeLintIllegalMethodStatList(fp,name,pname,ForStatStmts(s),
                                               (context | context_FOR));
        if (tmperr != DEF_OKAY) {
          rval = tmperr;
        }
      }
      break;
    case EXT:
      if (ExternalStatMode(s) != ek_method) {
        Asc_StatErrMsg_NotAllowedMethod(fp,s,"");
        rval = DEF_STAT_MISLOCATED;
      }
      break;
    case LNK:
    case UNLNK:
      /* DS: in case we provide functionality for other statements inside LINK, check their legal status */
      /* DS: in case we provide functionality for other statements inside UNLINK, check their legal status */
      break;
    case ASGN:
    case RUN:
    case FIX:
    case FREE:
    case CALL:
    case SOLVER:
    case OPTION:
    case SOLVE:
      break;
    case WHILE:
      if (WhileStatBlock(s) != NULL) {
        tmperr = TypeLintIllegalMethodStatList(fp,name,pname,WhileStatBlock(s),
                                               (context | context_WHILE));
        if (tmperr != DEF_OKAY) {
          rval = tmperr;
          break;
        }
      }
      break;
    case ASSERT:
      /* no sublists for TEST */
      break;

    case IF:
      if (IfStatThen(s) != NULL) {
        tmperr = TypeLintIllegalMethodStatList(fp,name,pname,IfStatThen(s),
                                               (context | context_IF));
        if (tmperr != DEF_OKAY) {
          rval = tmperr;
          break;
        }
      }
      if (IfStatElse(s) != NULL){
        tmperr = TypeLintIllegalMethodStatList(fp,name,pname,IfStatElse(s),
                                               (context | context_IF));
        if (tmperr != DEF_OKAY) {
          rval = tmperr;
	}
      }
      break;
    case SWITCH:
      sw = SwitchStatCases(s);
      while (sw!=NULL) {
        sublist = SwitchStatementList(sw);
        if (sublist!=NULL) {
          tmperr = TypeLintIllegalMethodStatList(fp,name,pname,sublist,
                                                 (context | context_SWITCH));
          if (tmperr != DEF_OKAY) {
            rval = tmperr;
            break;
          }
	}
        sw = NextSwitchCase(sw);
      }
      break;
    case FLOW:
      switch (FlowStatControl(s)) {
      case fc_break:
        if ((context & (context_FOR | context_SWITCH | context_WHILE))==0) {
          rval = DEF_ILLEGAL_BREAK;
          TypeLintError(fp,s,rval);
        }
        break;
      case fc_continue:
        if ((context & (context_FOR | context_WHILE))==0) {
          rval = DEF_ILLEGAL_CONTINUE;
          TypeLintError(fp,s,rval);
        }
        break;
      case fc_fallthru:
        if ((context & context_SWITCH)==0) {
          rval = DEF_ILLEGAL_FALLTHRU;
          TypeLintError(fp,s,rval);
        }
        break;
      case fc_return:
      case fc_stop:
        break;
      }
      break;
    default:
      rval = DEF_UNKNOWN_ERR;
      TypeLintError(fp,s,rval);
      break;
    }
  }
  if (rval != DEF_OKAY) {
    FPRINTF(fp,"  Errors detected in METHOD '%s' of '%s'\n",SCP(pname),SCP(name));
  }
  return rval;
}

enum typelinterr TypeLintIllegalMethodStats(FILE * fp,
                                            symchar *name,
                                            struct gl_list_t *pl,
                                            unsigned int context)
{
  unsigned long pc,plen;
  struct StatementList *sl;
  symchar *pname;
  enum typelinterr rval = DEF_OKAY, tmperr;

  assert(name != NULL);
  if (pl == NULL) {
    return rval;
  }
  plen = gl_length(pl);
  for (pc=1; pc <= plen; pc++) {
    sl = ProcStatementList((struct InitProcedure *)gl_fetch(pl,pc));
    pname= ProcName((struct InitProcedure *)gl_fetch(pl,pc));
    tmperr = TypeLintIllegalMethodStatList(fp,name,pname,sl,context);
    if (tmperr != DEF_OKAY) {
      rval = tmperr;
		printf("asdsaf \n");
    }
  }
  return rval;
}
