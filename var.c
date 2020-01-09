/*	$NetBSD: var.c,v 1.221 2018/12/21 05:50:19 sjg Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef MAKE_NATIVE
static char rcsid[] = "$NetBSD: var.c,v 1.221 2018/12/21 05:50:19 sjg Exp $";
#else
                                                                                                                        #include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)var.c	8.3 (Berkeley) 3/19/94";
#else
__RCSID("$NetBSD: var.c,v 1.221 2018/12/21 05:50:19 sjg Exp $");
#endif
#endif /* not lint */
#endif

/*-
 * var.c --
 *	Variable-handling functions
 *
 * Interface:
 *	Var_Set		    Set the value of a variable in the given
 *			    context. The variable is created if it doesn't
 *			    yet exist. The value and variable name need not
 *			    be preserved.
 *
 *	Var_Append	    Append more characters to an existing variable
 *			    in the given context. The variable needn't
 *			    exist already -- it will be created if it doesn't.
 *			    A space is placed between the old value and the
 *			    new one.
 *
 *	Var_Exists	    See if a variable exists.
 *
 *	Var_Value 	    Return the value of a variable in a context or
 *			    NULL if the variable is undefined.
 *
 *	Var_Subst 	    Substitute named variable, or all variables if
 *			    NULL in a string using
 *			    the given context as the top-most one. If the
 *			    third argument is non-zero, Parse_Error is
 *			    called if any variables are undefined.
 *
 *	Var_Parse 	    Parse a variable expansion from a string and
 *			    return the result and the number of characters
 *			    consumed.
 *
 *	Var_Delete	    Delete a variable in a context.
 *
 *	Var_Init  	    Initialize this module.
 *
 * Debugging:
 *	Var_Dump  	    Print out all variables defined in the given
 *			    context.
 *
 * XXX: There's a lot of duplication in these functions.
 */

#include    <sys/stat.h>

#if (defined _WIN32 && !defined __CYGWIN__)
#include "headers-mingw/sys_stat.h"
#endif
#ifndef NO_REGEX

#include    <sys/types.h>

# if (defined _WIN32 && !defined __CYGWIN__)
                                                                                                                        #include "headers-mingw/regex.h"
#include "headers-mingw/regex_internal.h"
# else

#include    <regex.h>

# endif
#endif

#include    <ctype.h>
#include    <stdlib.h>
#include    <limits.h>
#include    <time.h>

#include    "make.h"
#include    "buf.h"
#include    "dir.h"
#include    "job.h"
#include    "metachar.h"

extern int makelevel;
/*
 * This lets us tell if we have replaced the original environ
 * (which we cannot free).
 */
char **savedEnv = NULL;

/*
 * This is a harmless return value for Var_Parse that can be used by Var_Subst
 * to determine if there was an error in parsing -- easier than returning
 * a flag, as things outside this module don't give a hoot.
 */
char var_Error[] = "";

/*
 * Similar to var_Error, but returned when the 'VARF_UNDEFERR' flag for
 * Var_Parse is not set. Why not just use a constant? Well, gcc likes
 * to condense identical string instances...
 */
static char varNoError[] = "";

/*
 * Traditionally we consume $$ during := like any other expansion.
 * Other make's do not.
 * This knob allows controlling the behavior.
 * FALSE for old behavior.
 * TRUE for new compatible.
 */
#define SAVE_DOLLARS ".MAKE.SAVE_DOLLARS"
static Boolean save_dollars = FALSE;

/*
 * Internally, variables are contained in four different contexts.
 *	1) the environment. They may not be changed. If an environment
 *	    variable is appended-to, the result is placed in the global
 *	    context.
 *	2) the global context. Variables set in the Makefile are located in
 *	    the global context. It is the penultimate context searched when
 *	    substituting.
 *	3) the command-line context. All variables set on the command line
 *	   are placed in this context. They are UNALTERABLE once placed here.
 *	4) the local context. Each target has associated with it a context
 *	   list. On this list are located the structures describing such
 *	   local variables as $(@) and $(*)
 * The four contexts are searched in the reverse order from which they are
 * listed.
 */
GNode *VAR_INTERNAL; /* variables from make itself */
GNode *VAR_GLOBAL;   /* variables from the makefile */
GNode *VAR_CMD;      /* variables defined on the command-line */

#define FIND_CMD    0x1   /* look in VAR_CMD when searching */
#define FIND_GLOBAL    0x2   /* look in VAR_GLOBAL as well */
#define FIND_ENV    0x4   /* look in the environment also */

typedef struct Var {
    char *name;    /* the variable's name */
    Buffer val;        /* its value */
    int flags;        /* miscellaneous status flags */
#define VAR_IN_USE    1        /* Variable's value currently being used.
				     * Used to avoid recursion */
#define VAR_FROM_ENV    2        /* Variable comes from the environment */
#define VAR_JUNK    4        /* Variable is a junk variable that
				     * should be destroyed when done with
				     * it. Used by Var_Parse for undefined,
				     * modified variables */
#define VAR_KEEP    8        /* Variable is VAR_JUNK, but we found
				     * a use for it in some modifier and
				     * the value is therefore valid */
#define VAR_EXPORTED    16        /* Variable is exported */
#define VAR_REEXPORT    32        /* Indicate if var needs re-export.
				     * This would be true if it contains $'s
				     */
#define VAR_FROM_CMD    64        /* Variable came from command line */
} Var;

/*
 * Exporting vars is expensive so skip it if we can
 */
#define VAR_EXPORTED_NONE    0
#define VAR_EXPORTED_YES    1
#define VAR_EXPORTED_ALL    2
static int var_exportedVars = VAR_EXPORTED_NONE;
/*
 * We pass this to Var_Export when doing the initial export
 * or after updating an exported var.
 */
#define VAR_EXPORT_PARENT    1
/*
 * We pass this to Var_Export1 to tell it to leave the value alone.
 */
#define VAR_EXPORT_LITERAL    2

/* Var*Pattern flags */
#define VAR_SUB_GLOBAL    0x01    /* Apply substitution globally */
#define VAR_SUB_ONE    0x02    /* Apply substitution to one word */
#define VAR_SUB_MATCHED    0x04    /* There was a match */
#define VAR_MATCH_START    0x08    /* Match at start of word */
#define VAR_MATCH_END    0x10    /* Match at end of word */
#define VAR_NOSUBST    0x20    /* don't expand vars in VarGetPattern */

/* Var_Set flags */
#define VAR_NO_EXPORT    0x01    /* do not export */

typedef struct {
    /*
     * The following fields are set by Var_Parse() when it
     * encounters modifiers that need to keep state for use by
     * subsequent modifiers within the same variable expansion.
     */
    Byte varSpace;    /* Word separator in expansions */
    Boolean oneBigWord;    /* TRUE if we will treat the variable as a
				 * single big word, even if it contains
				 * embedded spaces (as opposed to the
				 * usual behaviour of treating it as
				 * several space-separated words). */
} Var_Parse_State;

/* struct passed as 'void *' to VarSubstitute() for ":S/lhs/rhs/",
 * to VarSYSVMatch() for ":lhs=rhs". */
typedef struct {
    const char *lhs;        /* String to match */
    int leftLen; /* Length of string */
    const char *rhs;        /* Replacement string (w/ &'s removed) */
    int rightLen; /* Length of replacement */
    int flags;
} VarPattern;

/* struct passed as 'void *' to VarLoopExpand() for ":@tvar@str@" */
typedef struct {
    GNode *ctxt;        /* variable context */
    char *tvar;        /* name of temp var */
    int tvarLen;
    char *str;        /* string to expand */
    int strLen;
    int errnum;        /* errnum for not defined */
} VarLoop_t;

#ifndef NO_REGEX
/* struct passed as 'void *' to VarRESubstitute() for ":C///" */
typedef struct {
    regex_t re;
    int nsub;
    regmatch_t *matches;
    char *replace;
    int flags;
} VarREPattern;
#endif

/* struct passed to VarSelectWords() for ":[start..end]" */
typedef struct {
    int start;        /* first word to select */
    int end;        /* last word to select */
} VarSelectWords_t;

static Var *VarFind(const char *, GNode *, int);

static void VarAdd(const char *, const char *, GNode *);

static Boolean VarHead(GNode *, Var_Parse_State *,
                       char *, Boolean, Buffer *, void *);

static Boolean VarTail(GNode *, Var_Parse_State *,
                       char *, Boolean, Buffer *, void *);

static Boolean VarSuffix(GNode *, Var_Parse_State *,
                         char *, Boolean, Buffer *, void *);

static Boolean VarRoot(GNode *, Var_Parse_State *,
                       char *, Boolean, Buffer *, void *);

static Boolean VarMatch(GNode *, Var_Parse_State *,
                        char *, Boolean, Buffer *, void *);

#ifdef SYSVVARSUB

static Boolean VarSYSVMatch(GNode *, Var_Parse_State *,
                            char *, Boolean, Buffer *, void *);

#endif

static Boolean VarNoMatch(GNode *, Var_Parse_State *,
                          char *, Boolean, Buffer *, void *);

#ifndef NO_REGEX

static void VarREError(int, regex_t *, const char *);

static Boolean VarRESubstitute(GNode *, Var_Parse_State *,
                               char *, Boolean, Buffer *, void *);

#endif

static Boolean VarSubstitute(GNode *, Var_Parse_State *,
                             char *, Boolean, Buffer *, void *);

static Boolean VarLoopExpand(GNode *, Var_Parse_State *,
                             char *, Boolean, Buffer *, void *);

static char *VarGetPattern(GNode *, Var_Parse_State *,
                           int, const char **, int, int *, int *,
                           VarPattern *);

static char *VarQuote(char *, Boolean);

static char *VarHash(char *);

static char *VarModify(GNode *, Var_Parse_State *,
                       const char *,
                       Boolean (*)(GNode *, Var_Parse_State *, char *, Boolean, Buffer *, void *),
                       void *);

static char *VarOrder(const char *, const char);

static char *VarUniq(const char *);

static int VarWordCompare(const void *, const void *);

static void VarPrintVar(void *);

#define BROPEN    '{'
#define BRCLOSE    '}'
#define PROPEN    '('
#define PRCLOSE    ')'

/*-
 *-----------------------------------------------------------------------
 * VarFind --
 *	Find the given variable in the given context and any other contexts
 *	indicated.
 *
 * Input:
 *	name		name to find
 *	ctxt		context in which to find it
 *	flags		FIND_GLOBAL set means to look in the
 *			VAR_GLOBAL context as well. FIND_CMD set means
 *			to look in the VAR_CMD context also. FIND_ENV
 *			set means to look in the environment
 *
 * Results:
 *	A pointer to the structure describing the desired variable or
 *	NULL if the variable does not exist.
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static Var *
VarFind(const char *name, GNode *ctxt, int flags) {
    Hash_Entry *var;
    Var *v;

    /*
	 * If the variable name begins with a '.', it could very well be one of
	 * the local ones.  We check the name against all the local variables
	 * and substitute the short version in for 'name' if it matches one of
	 * them.
	 */
    if (*name == '.' && isupper((unsigned char) name[1]))
        switch (name[1]) {
            case 'A':
                if (!strcmp(name, ".ALLSRC"))
                    name = ALLSRC;
                if (!strcmp(name, ".ARCHIVE"))
                    name = ARCHIVE;
                break;
            case 'I':
                if (!strcmp(name, ".IMPSRC"))
                    name = IMPSRC;
                break;
            case 'M':
                if (!strcmp(name, ".MEMBER"))
                    name = MEMBER;
                break;
            case 'O':
                if (!strcmp(name, ".OODATE"))
                    name = OODATE;
                break;
            case 'P':
                if (!strcmp(name, ".PREFIX"))
                    name = PREFIX;
                break;
            case 'T':
                if (!strcmp(name, ".TARGET"))
                    name = TARGET;
                break;
        }
#ifdef notyet
                                                                                                                            /* for compatibility with gmake */
    if (name[0] == '^' && name[1] == '\0')
	    name = ALLSRC;
#endif

    /*
     * First look for the variable in the given context. If it's not there,
     * look for it in VAR_CMD, VAR_GLOBAL and the environment, in that order,
     * depending on the FIND_* flags in 'flags'
     */
    var = Hash_FindEntry(&ctxt->context, name);

    if ((var == NULL) && (flags & FIND_CMD) && (ctxt != VAR_CMD)) {
        var = Hash_FindEntry(&VAR_CMD->context, name);
    }
    if (!checkEnvFirst && (var == NULL) && (flags & FIND_GLOBAL) &&
        (ctxt != VAR_GLOBAL)) {
        var = Hash_FindEntry(&VAR_GLOBAL->context, name);
        if ((var == NULL) && (ctxt != VAR_INTERNAL)) {
            /* VAR_INTERNAL is subordinate to VAR_GLOBAL */
            var = Hash_FindEntry(&VAR_INTERNAL->context, name);
        }
    }
    if ((var == NULL) && (flags & FIND_ENV)) {
        char *env;

        if ((env = getenv(name)) != NULL) {
            int len;

            v = bmake_malloc(sizeof(Var));
            v->name = bmake_strdup(name);

            len = strlen(env);

            Buf_Init(&v->val, len + 1);
            Buf_AddBytes(&v->val, len, env);

            v->flags = VAR_FROM_ENV;
            return (v);
        } else if (checkEnvFirst && (flags & FIND_GLOBAL) &&
                   (ctxt != VAR_GLOBAL)) {
            var = Hash_FindEntry(&VAR_GLOBAL->context, name);
            if ((var == NULL) && (ctxt != VAR_INTERNAL)) {
                var = Hash_FindEntry(&VAR_INTERNAL->context, name);
            }
            if (var == NULL) {
                return NULL;
            } else {
                return ((Var *) Hash_GetValue(var));
            }
        } else {
            return NULL;
        }
    } else if (var == NULL) {
        return NULL;
    } else {
        return ((Var *) Hash_GetValue(var));
    }
}

/*-
 *-----------------------------------------------------------------------
 * VarFreeEnv  --
 *	If the variable is an environment variable, free it
 *
 * Input:
 *	v		the variable
 *	destroy		true if the value buffer should be destroyed.
 *
 * Results:
 *	1 if it is an environment variable 0 ow.
 *
 * Side Effects:
 *	The variable is free'ed if it is an environent variable.
 *-----------------------------------------------------------------------
 */
static Boolean
VarFreeEnv(Var *v, Boolean destroy) {
    if ((v->flags & VAR_FROM_ENV) == 0)
        return FALSE;
    free(v->name);
    Buf_Destroy(&v->val, destroy);
    free(v);
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * VarAdd  --
 *	Add a new variable of name name and value val to the given context
 *
 * Input:
 *	name		name of variable to add
 *	val		value to set it to
 *	ctxt		context in which to set it
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The new variable is placed at the front of the given context
 *	The name and val arguments are duplicated so they may
 *	safely be freed.
 *-----------------------------------------------------------------------
 */
static void
VarAdd(const char *name, const char *val, GNode *ctxt) {
    Var *v;
    int len;
    Hash_Entry *h;

    v = bmake_malloc(sizeof(Var));

    len = val ? strlen(val) : 0;
    Buf_Init(&v->val, len + 1);
    Buf_AddBytes(&v->val, len, val);

    v->flags = 0;

    h = Hash_CreateEntry(&ctxt->context, name, NULL);
    Hash_SetValue(h, v);
    v->name = h->name;
    if (DEBUG(VAR) && (ctxt->flags & INTERNAL) == 0) {
        fprintf(debug_file, "%s:%s = %s\n", ctxt->name, name, val);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Var_Delete --
 *	Remove a variable from a context.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The Var structure is removed and freed.
 *
 *-----------------------------------------------------------------------
 */
void
Var_Delete(const char *name, GNode *ctxt) {
    Hash_Entry *ln;
    char *cp;

    if (strchr(name, '$')) {
        cp = Var_Subst(NULL, name, VAR_GLOBAL, VARF_WANTRES);
    } else {
        cp = (char *) name;
    }
    ln = Hash_FindEntry(&ctxt->context, cp);
    if (DEBUG(VAR)) {
        fprintf(debug_file, "%s:delete %s%s\n",
                ctxt->name, cp, ln ? "" : " (not found)");
    }
    if (cp != name) {
        free(cp);
    }
    if (ln != NULL) {
        Var *v;

        v = (Var *) Hash_GetValue(ln);
        if ((v->flags & VAR_EXPORTED)) {
            unsetenv(v->name);
        }
        if (strcmp(MAKE_EXPORTED, v->name) == 0) {
            var_exportedVars = VAR_EXPORTED_NONE;
        }
        if (v->name != ln->name)
            free(v->name);
        Hash_DeleteEntry(&ctxt->context, ln);
        Buf_Destroy(&v->val, TRUE);
        free(v);
    }
}


/*
 * Export a var.
 * We ignore make internal variables (those which start with '.')
 * Also we jump through some hoops to avoid calling setenv
 * more than necessary since it can leak.
 * We only manipulate flags of vars if 'parent' is set.
 */
static int
Var_Export1(const char *name, int flags) {
    char tmp[BUFSIZ];
    Var *v;
    char *val = NULL;
    int n;
    int parent = (flags & VAR_EXPORT_PARENT);

    if (*name == '.')
        return 0;            /* skip internals */
    if (!name[1]) {
        /*
	 * A single char.
	 * If it is one of the vars that should only appear in
	 * local context, skip it, else we can get Var_Subst
	 * into a loop.
	 */
        switch (name[0]) {
            case '@':
            case '%':
            case '*':
            case '!':
                return 0;
        }
    }
    v = VarFind(name, VAR_GLOBAL, 0);
    if (v == NULL) {
        return 0;
    }
    if (!parent &&
        (v->flags & (VAR_EXPORTED | VAR_REEXPORT)) == VAR_EXPORTED) {
        return 0;            /* nothing to do */
    }
    val = Buf_GetAll(&v->val, NULL);
    if ((flags & VAR_EXPORT_LITERAL) == 0 && strchr(val, '$')) {
        if (parent) {
            /*
	     * Flag this as something we need to re-export.
	     * No point actually exporting it now though,
	     * the child can do it at the last minute.
	     */
            v->flags |= (VAR_EXPORTED | VAR_REEXPORT);
            return 1;
        }
        if (v->flags & VAR_IN_USE) {
            /*
	     * We recursed while exporting in a child.
	     * This isn't going to end well, just skip it.
	     */
            return 0;
        }
        n = snprintf(tmp, sizeof(tmp), "${%s}", name);
        if (n < (int) sizeof(tmp)) {
            val = Var_Subst(NULL, tmp, VAR_GLOBAL, VARF_WANTRES);
            setenv(name, val, 1);
            free(val);
        }
    } else {
        if (parent) {
            v->flags &= ~VAR_REEXPORT;    /* once will do */
        }
        if (parent || !(v->flags & VAR_EXPORTED)) {
            setenv(name, val, 1);
        }
    }
    /*
     * This is so Var_Set knows to call Var_Export again...
     */
    if (parent) {
        v->flags |= VAR_EXPORTED;
    }
    return 1;
}

/*
 * This gets called from our children.
 */
void
Var_ExportVars(void) {
    char tmp[BUFSIZ];
    Hash_Entry *var;
    Hash_Search state;
    Var *v;
    char *val;
    int n;

    /*
     * Several make's support this sort of mechanism for tracking
     * recursion - but each uses a different name.
     * We allow the makefiles to update MAKELEVEL and ensure
     * children see a correctly incremented value.
     */
    snprintf(tmp, sizeof(tmp), "%d", makelevel + 1);
    setenv(MAKE_LEVEL_ENV, tmp, 1);

    if (VAR_EXPORTED_NONE == var_exportedVars)
        return;

    if (VAR_EXPORTED_ALL == var_exportedVars) {
        /*
	 * Ouch! This is crazy...
	 */
        for (var = Hash_EnumFirst(&VAR_GLOBAL->context, &state);
             var != NULL;
             var = Hash_EnumNext(&state)) {
            v = (Var *) Hash_GetValue(var);
            Var_Export1(v->name, 0);
        }
        return;
    }
    /*
     * We have a number of exported vars,
     */
    n = snprintf(tmp, sizeof(tmp), "${" MAKE_EXPORTED ":O:u}");
    if (n < (int) sizeof(tmp)) {
        char **av;
        char *as;
        int ac;
        int i;

        val = Var_Subst(NULL, tmp, VAR_GLOBAL, VARF_WANTRES);
        if (*val) {
            av = brk_string(val, &ac, FALSE, &as);
            for (i = 0; i < ac; i++) {
                Var_Export1(av[i], 0);
            }
            free(as);
            free(av);
        }
        free(val);
    }
}

/*
 * This is called when .export is seen or
 * .MAKE.EXPORTED is modified.
 * It is also called when any exported var is modified.
 */
void
Var_Export(char *str, int isExport) {
    char *name;
    char *val;
    char **av;
    char *as;
    int flags;
    int ac;
    int i;

    if (isExport && (!str || !str[0])) {
        var_exportedVars = VAR_EXPORTED_ALL; /* use with caution! */
        return;
    }

    flags = 0;
    if (strncmp(str, "-env", 4) == 0) {
        str += 4;
    } else if (strncmp(str, "-literal", 8) == 0) {
        str += 8;
        flags |= VAR_EXPORT_LITERAL;
    } else {
        flags |= VAR_EXPORT_PARENT;
    }
    val = Var_Subst(NULL, str, VAR_GLOBAL, VARF_WANTRES);
    if (*val) {
        av = brk_string(val, &ac, FALSE, &as);
        for (i = 0; i < ac; i++) {
            name = av[i];
            if (!name[1]) {
                /*
		 * A single char.
		 * If it is one of the vars that should only appear in
		 * local context, skip it, else we can get Var_Subst
		 * into a loop.
		 */
                switch (name[0]) {
                    case '@':
                    case '%':
                    case '*':
                    case '!':
                        continue;
                }
            }
            if (Var_Export1(name, flags)) {
                if (VAR_EXPORTED_ALL != var_exportedVars)
                    var_exportedVars = VAR_EXPORTED_YES;
                if (isExport && (flags & VAR_EXPORT_PARENT)) {
                    Var_Append(MAKE_EXPORTED, name, VAR_GLOBAL);
                }
            }
        }
        free(as);
        free(av);
    }
    free(val);
}


/*
 * This is called when .unexport[-env] is seen.
 */
extern char **environ;

void
Var_UnExport(char *str) {
    char tmp[BUFSIZ];
    char *vlist;
    char *cp;
    Boolean unexport_env;
    int n;

    if (!str || !str[0]) {
        return;            /* assert? */
    }

    vlist = NULL;

    str += 8;
    unexport_env = (strncmp(str, "-env", 4) == 0);
    if (unexport_env) {
        char **newenv;

        cp = getenv(MAKE_LEVEL_ENV);    /* we should preserve this */
        if (environ == savedEnv) {
            /* we have been here before! */
            newenv = bmake_realloc(environ, 2 * sizeof(char *));
        } else {
            if (savedEnv) {
                free(savedEnv);
                savedEnv = NULL;
            }
            newenv = bmake_malloc(2 * sizeof(char *));
        }
        if (!newenv)
            return;
        /* Note: we cannot safely free() the original environ. */
        environ = savedEnv = newenv;
        newenv[0] = NULL;
        newenv[1] = NULL;
        if (cp && *cp)
            setenv(MAKE_LEVEL_ENV, cp, 1);
    } else {
        for (; *str != '\n' && isspace((unsigned char) *str); str++)
            continue;
        if (str[0] && str[0] != '\n') {
            vlist = str;
        }
    }

    if (!vlist) {
        /* Using .MAKE.EXPORTED */
        n = snprintf(tmp, sizeof(tmp), "${" MAKE_EXPORTED ":O:u}");
        if (n < (int) sizeof(tmp)) {
            vlist = Var_Subst(NULL, tmp, VAR_GLOBAL, VARF_WANTRES);
        }
    }
    if (vlist) {
        Var *v;
        char **av;
        char *as;
        int ac;
        int i;

        av = brk_string(vlist, &ac, FALSE, &as);
        for (i = 0; i < ac; i++) {
            v = VarFind(av[i], VAR_GLOBAL, 0);
            if (!v)
                continue;
            if (!unexport_env &&
                (v->flags & (VAR_EXPORTED | VAR_REEXPORT)) == VAR_EXPORTED) {
                unsetenv(v->name);
            }
            v->flags &= ~(VAR_EXPORTED | VAR_REEXPORT);
            /*
	     * If we are unexporting a list,
	     * remove each one from .MAKE.EXPORTED.
	     * If we are removing them all,
	     * just delete .MAKE.EXPORTED below.
	     */
            if (vlist == str) {
                n = snprintf(tmp, sizeof(tmp),
                             "${" MAKE_EXPORTED ":N%s}", v->name);
                if (n < (int) sizeof(tmp)) {
                    cp = Var_Subst(NULL, tmp, VAR_GLOBAL, VARF_WANTRES);
                    Var_Set(MAKE_EXPORTED, cp, VAR_GLOBAL, 0);
                    free(cp);
                }
            }
        }
        free(as);
        free(av);
        if (vlist != str) {
            Var_Delete(MAKE_EXPORTED, VAR_GLOBAL);
            free(vlist);
        }
    }
}

/*-
 *-----------------------------------------------------------------------
 * Var_Set --
 *	Set the variable name to the value val in the given context.
 *
 * Input:
 *	name		name of variable to set
 *	val		value to give to the variable
 *	ctxt		context in which to set it
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	If the variable doesn't yet exist, a new record is created for it.
 *	Else the old value is freed and the new one stuck in its place
 *
 * Notes:
 *	The variable is searched for only in its context before being
 *	created in that context. I.e. if the context is VAR_GLOBAL,
 *	only VAR_GLOBAL->context is searched. Likewise if it is VAR_CMD, only
 *	VAR_CMD->context is searched. This is done to avoid the literally
 *	thousands of unnecessary strcmp's that used to be done to
 *	set, say, $(@) or $(<).
 *	If the context is VAR_GLOBAL though, we check if the variable
 *	was set in VAR_CMD from the command line and skip it if so.
 *-----------------------------------------------------------------------
 */
void
Var_Set(const char *name, const char *val, GNode *ctxt, int flags) {
    Var *v;
    char *expanded_name = NULL;

    /*
     * We only look for a variable in the given context since anything set
     * here will override anything in a lower context, so there's not much
     * point in searching them all just to save a bit of memory...
     */
    if (strchr(name, '$') != NULL) {
        expanded_name = Var_Subst(NULL, name, ctxt, VARF_WANTRES);
        if (expanded_name[0] == 0) {
            if (DEBUG(VAR)) {
                fprintf(debug_file, "Var_Set(\"%s\", \"%s\", ...) "
                                    "name expands to empty string - ignored\n",
                        name, val);
            }
            free(expanded_name);
            return;
        }
        name = expanded_name;
    }
    if (ctxt == VAR_GLOBAL) {
        v = VarFind(name, VAR_CMD, 0);
        if (v != NULL) {
            if ((v->flags & VAR_FROM_CMD)) {
                if (DEBUG(VAR)) {
                    fprintf(debug_file, "%s:%s = %s ignored!\n", ctxt->name, name, val);
                }
                goto out;
            }
            VarFreeEnv(v, TRUE);
        }
    }
    v = VarFind(name, ctxt, 0);
    if (v == NULL) {
        if (ctxt == VAR_CMD && (flags & VAR_NO_EXPORT) == 0) {
            /*
	     * This var would normally prevent the same name being added
	     * to VAR_GLOBAL, so delete it from there if needed.
	     * Otherwise -V name may show the wrong value.
	     */
            Var_Delete(name, VAR_GLOBAL);
        }
        VarAdd(name, val, ctxt);
    } else {
        Buf_Empty(&v->val);
        if (val)
            Buf_AddBytes(&v->val, strlen(val), val);

        if (DEBUG(VAR)) {
            fprintf(debug_file, "%s:%s = %s\n", ctxt->name, name, val);
        }
        if ((v->flags & VAR_EXPORTED)) {
            Var_Export1(name, VAR_EXPORT_PARENT);
        }
    }
    /*
     * Any variables given on the command line are automatically exported
     * to the environment (as per POSIX standard)
     */
    if (ctxt == VAR_CMD && (flags & VAR_NO_EXPORT) == 0) {
        if (v == NULL) {
            /* we just added it */
            v = VarFind(name, ctxt, 0);
        }
        if (v != NULL)
            v->flags |= VAR_FROM_CMD;
        /*
	 * If requested, don't export these in the environment
	 * individually.  We still put them in MAKEOVERRIDES so
	 * that the command-line settings continue to override
	 * Makefile settings.
	 */
        if (varNoExportEnv != TRUE)
            setenv(name, val ? val : "", 1);

        Var_Append(MAKEOVERRIDES, name, VAR_GLOBAL);
    }
    if (*name == '.') {
        if (strcmp(name, SAVE_DOLLARS) == 0)
            save_dollars = s2Boolean(val, save_dollars);
    }

    out:
    free(expanded_name);
    if (v != NULL)
        VarFreeEnv(v, TRUE);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Append --
 *	The variable of the given name has the given value appended to it in
 *	the given context.
 *
 * Input:
 *	name		name of variable to modify
 *	val		String to append to it
 *	ctxt		Context in which this should occur
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	If the variable doesn't exist, it is created. Else the strings
 *	are concatenated (with a space in between).
 *
 * Notes:
 *	Only if the variable is being sought in the global context is the
 *	environment searched.
 *	XXX: Knows its calling circumstances in that if called with ctxt
 *	an actual target, it will only search that context since only
 *	a local variable could be being appended to. This is actually
 *	a big win and must be tolerated.
 *-----------------------------------------------------------------------
 */
void
Var_Append(const char *name, const char *val, GNode *ctxt) {
    Var *v;
    Hash_Entry *h;
    char *expanded_name = NULL;

    if (strchr(name, '$') != NULL) {
        expanded_name = Var_Subst(NULL, name, ctxt, VARF_WANTRES);
        if (expanded_name[0] == 0) {
            if (DEBUG(VAR)) {
                fprintf(debug_file, "Var_Append(\"%s\", \"%s\", ...) "
                                    "name expands to empty string - ignored\n",
                        name, val);
            }
            free(expanded_name);
            return;
        }
        name = expanded_name;
    }

    v = VarFind(name, ctxt, (ctxt == VAR_GLOBAL) ? (FIND_CMD | FIND_ENV) : 0);

    if (v == NULL) {
        Var_Set(name, val, ctxt, 0);
    } else if (ctxt == VAR_CMD || !(v->flags & VAR_FROM_CMD)) {
        Buf_AddByte(&v->val, ' ');
        Buf_AddBytes(&v->val, strlen(val), val);

        if (DEBUG(VAR)) {
            fprintf(debug_file, "%s:%s = %s\n", ctxt->name, name,
                    Buf_GetAll(&v->val, NULL));
        }

        if (v->flags & VAR_FROM_ENV) {
            /*
	     * If the original variable came from the environment, we
	     * have to install it in the global context (we could place
	     * it in the environment, but then we should provide a way to
	     * export other variables...)
	     */
            v->flags &= ~VAR_FROM_ENV;
            h = Hash_CreateEntry(&ctxt->context, name, NULL);
            Hash_SetValue(h, v);
        }
    }
    free(expanded_name);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Exists --
 *	See if the given variable exists.
 *
 * Input:
 *	name		Variable to find
 *	ctxt		Context in which to start search
 *
 * Results:
 *	TRUE if it does, FALSE if it doesn't
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
Boolean
Var_Exists(const char *name, GNode *ctxt) {
    Var *v;
    char *cp;

    if ((cp = strchr(name, '$')) != NULL) {
        cp = Var_Subst(NULL, name, ctxt, VARF_WANTRES);
    }
    v = VarFind(cp ? cp : name, ctxt, FIND_CMD | FIND_GLOBAL | FIND_ENV);
    free(cp);
    if (v == NULL) {
        return (FALSE);
    } else {
        (void) VarFreeEnv(v, TRUE);
    }
    return (TRUE);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Value --
 *	Return the value of the named variable in the given context
 *
 * Input:
 *	name		name to find
 *	ctxt		context in which to search for it
 *
 * Results:
 *	The value if the variable exists, NULL if it doesn't
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
char *
Var_Value(const char *name, GNode *ctxt, char **frp) {
    Var *v;

    v = VarFind(name, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
    *frp = NULL;
    if (v != NULL) {
        char *p = (Buf_GetAll(&v->val, NULL));
        if (VarFreeEnv(v, FALSE))
            *frp = p;
        return p;
    } else {
        return NULL;
    }
}

/*-
 *-----------------------------------------------------------------------
 * VarHead --
 *	Remove the tail of the given word and place the result in the given
 *	buffer.
 *
 * Input:
 *	word		Word to trim
 *	addSpace	True if need to add a space to the buffer
 *			before sticking in the head
 *	buf		Buffer in which to store it
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarHead(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
        char *word, Boolean addSpace, Buffer *buf,
        void *dummy MAKE_ATTR_UNUSED) {
    char *slash;

    slash = strrchr(word, '/');
    if (slash != NULL) {
        if (addSpace && vpstate->varSpace) {
            Buf_AddByte(buf, vpstate->varSpace);
        }
        *slash = '\0';
        Buf_AddBytes(buf, strlen(word), word);
        *slash = '/';
        return (TRUE);
    } else {
        /*
	 * If no directory part, give . (q.v. the POSIX standard)
	 */
        if (addSpace && vpstate->varSpace)
            Buf_AddByte(buf, vpstate->varSpace);
        Buf_AddByte(buf, '.');
    }
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * VarTail --
 *	Remove the head of the given word and place the result in the given
 *	buffer.
 *
 * Input:
 *	word		Word to trim
 *	addSpace	True if need to add a space to the buffer
 *			before adding the tail
 *	buf		Buffer in which to store it
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarTail(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
        char *word, Boolean addSpace, Buffer *buf,
        void *dummy MAKE_ATTR_UNUSED) {
    char *slash;

    if (addSpace && vpstate->varSpace) {
        Buf_AddByte(buf, vpstate->varSpace);
    }

    slash = strrchr(word, '/');
    if (slash != NULL) {
        *slash++ = '\0';
        Buf_AddBytes(buf, strlen(slash), slash);
        slash[-1] = '/';
    } else {
        Buf_AddBytes(buf, strlen(word), word);
    }
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * VarSuffix --
 *	Place the suffix of the given word in the given buffer.
 *
 * Input:
 *	word		Word to trim
 *	addSpace	TRUE if need to add a space before placing the
 *			suffix in the buffer
 *	buf		Buffer in which to store it
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The suffix from the word is placed in the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarSuffix(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
          char *word, Boolean addSpace, Buffer *buf,
          void *dummy MAKE_ATTR_UNUSED) {
    char *dot;

    dot = strrchr(word, '.');
    if (dot != NULL) {
        if (addSpace && vpstate->varSpace) {
            Buf_AddByte(buf, vpstate->varSpace);
        }
        *dot++ = '\0';
        Buf_AddBytes(buf, strlen(dot), dot);
        dot[-1] = '.';
        addSpace = TRUE;
    }
    return addSpace;
}

/*-
 *-----------------------------------------------------------------------
 * VarRoot --
 *	Remove the suffix of the given word and place the result in the
 *	buffer.
 *
 * Input:
 *	word		Word to trim
 *	addSpace	TRUE if need to add a space to the buffer
 *			before placing the root in it
 *	buf		Buffer in which to store it
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarRoot(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
        char *word, Boolean addSpace, Buffer *buf,
        void *dummy MAKE_ATTR_UNUSED) {
    char *dot;

    if (addSpace && vpstate->varSpace) {
        Buf_AddByte(buf, vpstate->varSpace);
    }

    dot = strrchr(word, '.');
    if (dot != NULL) {
        *dot = '\0';
        Buf_AddBytes(buf, strlen(word), word);
        *dot = '.';
    } else {
        Buf_AddBytes(buf, strlen(word), word);
    }
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * VarMatch --
 *	Place the word in the buffer if it matches the given pattern.
 *	Callback function for VarModify to implement the :M modifier.
 *
 * Input:
 *	word		Word to examine
 *	addSpace	TRUE if need to add a space to the buffer
 *			before adding the word, if it matches
 *	buf		Buffer in which to store it
 *	pattern		Pattern the word must match
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarMatch(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
         char *word, Boolean addSpace, Buffer *buf,
         void *pattern) {
    if (DEBUG(VAR))
        fprintf(debug_file, "VarMatch [%s] [%s]\n", word, (char *) pattern);
    if (Str_Match(word, (char *) pattern)) {
        if (addSpace && vpstate->varSpace) {
            Buf_AddByte(buf, vpstate->varSpace);
        }
        addSpace = TRUE;
        Buf_AddBytes(buf, strlen(word), word);
    }
    return (addSpace);
}

#ifdef SYSVVARSUB

/*-
 *-----------------------------------------------------------------------
 * VarSYSVMatch --
 *	Place the word in the buffer if it matches the given pattern.
 *	Callback function for VarModify to implement the System V %
 *	modifiers.
 *
 * Input:
 *	word		Word to examine
 *	addSpace	TRUE if need to add a space to the buffer
 *			before adding the word, if it matches
 *	buf		Buffer in which to store it
 *	patp		Pattern the word must match
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarSYSVMatch(GNode *ctx, Var_Parse_State *vpstate,
             char *word, Boolean addSpace, Buffer *buf,
             void *patp) {
    int len;
    char *ptr;
    VarPattern *pat = (VarPattern *) patp;
    char *varexp;

    if (addSpace && vpstate->varSpace)
        Buf_AddByte(buf, vpstate->varSpace);

    addSpace = TRUE;

    if ((ptr = Str_SYSVMatch(word, pat->lhs, &len)) != NULL) {
        varexp = Var_Subst(NULL, pat->rhs, ctx, VARF_WANTRES);
        Str_SYSVSubst(buf, varexp, ptr, len);
        free(varexp);
    } else {
        Buf_AddBytes(buf, strlen(word), word);
    }

    return (addSpace);
}

#endif


/*-
 *-----------------------------------------------------------------------
 * VarNoMatch --
 *	Place the word in the buffer if it doesn't match the given pattern.
 *	Callback function for VarModify to implement the :N modifier.
 *
 * Input:
 *	word		Word to examine
 *	addSpace	TRUE if need to add a space to the buffer
 *			before adding the word, if it matches
 *	buf		Buffer in which to store it
 *	pattern		Pattern the word must match
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarNoMatch(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
           char *word, Boolean addSpace, Buffer *buf,
           void *pattern) {
    if (!Str_Match(word, (char *) pattern)) {
        if (addSpace && vpstate->varSpace) {
            Buf_AddByte(buf, vpstate->varSpace);
        }
        addSpace = TRUE;
        Buf_AddBytes(buf, strlen(word), word);
    }
    return (addSpace);
}


/*-
 *-----------------------------------------------------------------------
 * VarSubstitute --
 *	Perform a string-substitution on the given word, placing the
 *	result in the passed buffer.
 *
 * Input:
 *	word		Word to modify
 *	addSpace	True if space should be added before
 *			other characters
 *	buf		Buffer for result
 *	patternp	Pattern for substitution
 *
 * Results:
 *	TRUE if a space is needed before more characters are added.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarSubstitute(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
              char *word, Boolean addSpace, Buffer *buf,
              void *patternp) {
    int wordLen;    /* Length of word */
    char *cp;        /* General pointer */
    VarPattern *pattern = (VarPattern *) patternp;

    wordLen = strlen(word);
    if ((pattern->flags & (VAR_SUB_ONE | VAR_SUB_MATCHED)) !=
        (VAR_SUB_ONE | VAR_SUB_MATCHED)) {
        /*
	 * Still substituting -- break it down into simple anchored cases
	 * and if none of them fits, perform the general substitution case.
	 */
        if ((pattern->flags & VAR_MATCH_START) &&
            (strncmp(word, pattern->lhs, pattern->leftLen) == 0)) {
            /*
		 * Anchored at start and beginning of word matches pattern
		 */
            if ((pattern->flags & VAR_MATCH_END) &&
                (wordLen == pattern->leftLen)) {
                /*
			 * Also anchored at end and matches to the end (word
			 * is same length as pattern) add space and rhs only
			 * if rhs is non-null.
			 */
                if (pattern->rightLen != 0) {
                    if (addSpace && vpstate->varSpace) {
                        Buf_AddByte(buf, vpstate->varSpace);
                    }
                    addSpace = TRUE;
                    Buf_AddBytes(buf, pattern->rightLen, pattern->rhs);
                }
                pattern->flags |= VAR_SUB_MATCHED;
            } else if (pattern->flags & VAR_MATCH_END) {
                /*
		     * Doesn't match to end -- copy word wholesale
		     */
                goto nosub;
            } else {
                /*
		     * Matches at start but need to copy in trailing characters
		     */
                if ((pattern->rightLen + wordLen - pattern->leftLen) != 0) {
                    if (addSpace && vpstate->varSpace) {
                        Buf_AddByte(buf, vpstate->varSpace);
                    }
                    addSpace = TRUE;
                }
                Buf_AddBytes(buf, pattern->rightLen, pattern->rhs);
                Buf_AddBytes(buf, wordLen - pattern->leftLen,
                             (word + pattern->leftLen));
                pattern->flags |= VAR_SUB_MATCHED;
            }
        } else if (pattern->flags & VAR_MATCH_START) {
            /*
	     * Had to match at start of word and didn't -- copy whole word.
	     */
            goto nosub;
        } else if (pattern->flags & VAR_MATCH_END) {
            /*
	     * Anchored at end, Find only place match could occur (leftLen
	     * characters from the end of the word) and see if it does. Note
	     * that because the $ will be left at the end of the lhs, we have
	     * to use strncmp.
	     */
            cp = word + (wordLen - pattern->leftLen);
            if ((cp >= word) &&
                (strncmp(cp, pattern->lhs, pattern->leftLen) == 0)) {
                /*
		 * Match found. If we will place characters in the buffer,
		 * add a space before hand as indicated by addSpace, then
		 * stuff in the initial, unmatched part of the word followed
		 * by the right-hand-side.
		 */
                if (((cp - word) + pattern->rightLen) != 0) {
                    if (addSpace && vpstate->varSpace) {
                        Buf_AddByte(buf, vpstate->varSpace);
                    }
                    addSpace = TRUE;
                }
                Buf_AddBytes(buf, cp - word, word);
                Buf_AddBytes(buf, pattern->rightLen, pattern->rhs);
                pattern->flags |= VAR_SUB_MATCHED;
            } else {
                /*
		 * Had to match at end and didn't. Copy entire word.
		 */
                goto nosub;
            }
        } else {
            /*
	     * Pattern is unanchored: search for the pattern in the word using
	     * String_FindSubstring, copying unmatched portions and the
	     * right-hand-side for each match found, handling non-global
	     * substitutions correctly, etc. When the loop is done, any
	     * remaining part of the word (word and wordLen are adjusted
	     * accordingly through the loop) is copied straight into the
	     * buffer.
	     * addSpace is set FALSE as soon as a space is added to the
	     * buffer.
	     */
            Boolean done;
            int origSize;

            done = FALSE;
            origSize = Buf_Size(buf);
            while (!done) {
                cp = Str_FindSubstring(word, pattern->lhs);
                if (cp != NULL) {
                    if (addSpace && (((cp - word) + pattern->rightLen) != 0)) {
                        Buf_AddByte(buf, vpstate->varSpace);
                        addSpace = FALSE;
                    }
                    Buf_AddBytes(buf, cp - word, word);
                    Buf_AddBytes(buf, pattern->rightLen, pattern->rhs);
                    wordLen -= (cp - word) + pattern->leftLen;
                    word = cp + pattern->leftLen;
                    if (wordLen == 0) {
                        done = TRUE;
                    }
                    if ((pattern->flags & VAR_SUB_GLOBAL) == 0) {
                        done = TRUE;
                    }
                    pattern->flags |= VAR_SUB_MATCHED;
                } else {
                    done = TRUE;
                }
            }
            if (wordLen != 0) {
                if (addSpace && vpstate->varSpace) {
                    Buf_AddByte(buf, vpstate->varSpace);
                }
                Buf_AddBytes(buf, wordLen, word);
            }
            /*
	     * If added characters to the buffer, need to add a space
	     * before we add any more. If we didn't add any, just return
	     * the previous value of addSpace.
	     */
            return ((Buf_Size(buf) != origSize) || addSpace);
        }
        return (addSpace);
    }
    nosub:
    if (addSpace && vpstate->varSpace) {
        Buf_AddByte(buf, vpstate->varSpace);
    }
    Buf_AddBytes(buf, wordLen, word);
    return (TRUE);
}

#ifndef NO_REGEX

/*-
 *-----------------------------------------------------------------------
 * VarREError --
 *	Print the error caused by a regcomp or regexec call.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	An error gets printed.
 *
 *-----------------------------------------------------------------------
 */
static void
VarREError(int reerr, regex_t *pat, const char *str) {
    char *errbuf;
    int errlen;

    errlen = regerror(reerr, pat, 0, 0);
    errbuf = bmake_malloc(errlen);
    regerror(reerr, pat, errbuf, errlen);
    Error("%s: %s", str, errbuf);
    free(errbuf);
}


/*-
 *-----------------------------------------------------------------------
 * VarRESubstitute --
 *	Perform a regex substitution on the given word, placing the
 *	result in the passed buffer.
 *
 * Results:
 *	TRUE if a space is needed before more characters are added.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarRESubstitute(GNode *ctx MAKE_ATTR_UNUSED,
                Var_Parse_State *vpstate MAKE_ATTR_UNUSED,
                char *word, Boolean addSpace, Buffer *buf,
                void *patternp) {
    VarREPattern *pat;
    int xrv;
    char *wp;
    char *rp;
    int added;
    int flags = 0;

#define MAYBE_ADD_SPACE()        \
    if (addSpace && !added)        \
        Buf_AddByte(buf, ' ');    \
    added = 1

    added = 0;
    wp = word;
    pat = patternp;

    if ((pat->flags & (VAR_SUB_ONE | VAR_SUB_MATCHED)) ==
        (VAR_SUB_ONE | VAR_SUB_MATCHED))
        xrv = REG_NOMATCH;
    else {
        tryagain:
        xrv = regexec(&pat->re, wp, pat->nsub, pat->matches, flags);
    }

    switch (xrv) {
        case 0:
            pat->flags |= VAR_SUB_MATCHED;
            if (pat->matches[0].rm_so > 0) {
                MAYBE_ADD_SPACE();
                Buf_AddBytes(buf, pat->matches[0].rm_so, wp);
            }

            for (rp = pat->replace; *rp; rp++) {
                if ((*rp == '\\') && ((rp[1] == '&') || (rp[1] == '\\'))) {
                    MAYBE_ADD_SPACE();
                    Buf_AddByte(buf, rp[1]);
                    rp++;
                } else if ((*rp == '&') ||
                           ((*rp == '\\') && isdigit((unsigned char) rp[1]))) {
                    int n;
                    const char *subbuf;
                    int sublen;
                    char errstr[3];

                    if (*rp == '&') {
                        n = 0;
                        errstr[0] = '&';
                        errstr[1] = '\0';
                    } else {
                        n = rp[1] - '0';
                        errstr[0] = '\\';
                        errstr[1] = rp[1];
                        errstr[2] = '\0';
                        rp++;
                    }

                    if (n > pat->nsub) {
                        Error("No subexpression %s", &errstr[0]);
                        subbuf = "";
                        sublen = 0;
                    } else if ((pat->matches[n].rm_so == -1) &&
                               (pat->matches[n].rm_eo == -1)) {
                        Error("No match for subexpression %s", &errstr[0]);
                        subbuf = "";
                        sublen = 0;
                    } else {
                        subbuf = wp + pat->matches[n].rm_so;
                        sublen = pat->matches[n].rm_eo - pat->matches[n].rm_so;
                    }

                    if (sublen > 0) {
                        MAYBE_ADD_SPACE();
                        Buf_AddBytes(buf, sublen, subbuf);
                    }
                } else {
                    MAYBE_ADD_SPACE();
                    Buf_AddByte(buf, *rp);
                }
            }
            wp += pat->matches[0].rm_eo;
            if (pat->flags & VAR_SUB_GLOBAL) {
                flags |= REG_NOTBOL;
                if (pat->matches[0].rm_so == 0 && pat->matches[0].rm_eo == 0) {
                    MAYBE_ADD_SPACE();
                    Buf_AddByte(buf, *wp);
                    wp++;

                }
                if (*wp)
                    goto tryagain;
            }
            if (*wp) {
                MAYBE_ADD_SPACE();
                Buf_AddBytes(buf, strlen(wp), wp);
            }
            break;
        default:
            VarREError(xrv, &pat->re, "Unexpected regex error");
            /* fall through */
        case REG_NOMATCH:
            if (*wp) {
                MAYBE_ADD_SPACE();
                Buf_AddBytes(buf, strlen(wp), wp);
            }
            break;
    }
    return (addSpace || added);
}

#endif


/*-
 *-----------------------------------------------------------------------
 * VarLoopExpand --
 *	Implements the :@<temp>@<string>@ modifier of ODE make.
 *	We set the temp variable named in pattern.lhs to word and expand
 *	pattern.rhs storing the result in the passed buffer.
 *
 * Input:
 *	word		Word to modify
 *	addSpace	True if space should be added before
 *			other characters
 *	buf		Buffer for result
 *	pattern		Datafor substitution
 *
 * Results:
 *	TRUE if a space is needed before more characters are added.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarLoopExpand(GNode *ctx MAKE_ATTR_UNUSED,
              Var_Parse_State *vpstate MAKE_ATTR_UNUSED,
              char *word, Boolean addSpace, Buffer *buf,
              void *loopp) {
    VarLoop_t *loop = (VarLoop_t *) loopp;
    char *s;
    int slen;

    if (word && *word) {
        Var_Set(loop->tvar, word, loop->ctxt, VAR_NO_EXPORT);
        s = Var_Subst(NULL, loop->str, loop->ctxt, loop->errnum | VARF_WANTRES);
        if (s != NULL && *s != '\0') {
            if (addSpace && *s != '\n')
                Buf_AddByte(buf, ' ');
            Buf_AddBytes(buf, (slen = strlen(s)), s);
            addSpace = (slen > 0 && s[slen - 1] != '\n');
        }
        free(s);
    }
    return addSpace;
}


/*-
 *-----------------------------------------------------------------------
 * VarSelectWords --
 *	Implements the :[start..end] modifier.
 *	This is a special case of VarModify since we want to be able
 *	to scan the list backwards if start > end.
 *
 * Input:
 *	str		String whose words should be trimmed
 *	seldata		words to select
 *
 * Results:
 *	A string of all the words selected.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarSelectWords(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
               const char *str, VarSelectWords_t *seldata) {
    Buffer buf;            /* Buffer for the new string */
    Boolean addSpace;        /* TRUE if need to add a space to the
				     * buffer before adding the trimmed
				     * word */
    char **av;                /* word list */
    char *as;                /* word list memory */
    int ac, i;
    int start, end, step;

    Buf_Init(&buf, 0);
    addSpace = FALSE;

    if (vpstate->oneBigWord) {
        /* fake what brk_string() would do if there were only one word */
        ac = 1;
        av = bmake_malloc((ac + 1) * sizeof(char *));
        as = bmake_strdup(str);
        av[0] = as;
        av[1] = NULL;
    } else {
        av = brk_string(str, &ac, FALSE, &as);
    }

    /*
     * Now sanitize seldata.
     * If seldata->start or seldata->end are negative, convert them to
     * the positive equivalents (-1 gets converted to argc, -2 gets
     * converted to (argc-1), etc.).
     */
    if (seldata->start < 0)
        seldata->start = ac + seldata->start + 1;
    if (seldata->end < 0)
        seldata->end = ac + seldata->end + 1;

    /*
     * We avoid scanning more of the list than we need to.
     */
    if (seldata->start > seldata->end) {
        start = MIN(ac, seldata->start) - 1;
        end = MAX(0, seldata->end - 1);
        step = -1;
    } else {
        start = MAX(0, seldata->start - 1);
        end = MIN(ac, seldata->end);
        step = 1;
    }

    for (i = start;
         (step < 0 && i >= end) || (step > 0 && i < end);
         i += step) {
        if (av[i] && *av[i]) {
            if (addSpace && vpstate->varSpace) {
                Buf_AddByte(&buf, vpstate->varSpace);
            }
            Buf_AddBytes(&buf, strlen(av[i]), av[i]);
            addSpace = TRUE;
        }
    }

    free(as);
    free(av);

    return Buf_Destroy(&buf, FALSE);
}


/*-
 * VarRealpath --
 *	Replace each word with the result of realpath()
 *	if successful.
 */
static Boolean
VarRealpath(GNode *ctx MAKE_ATTR_UNUSED, Var_Parse_State *vpstate,
            char *word, Boolean addSpace, Buffer *buf,
            void *patternp MAKE_ATTR_UNUSED) {
    struct stat st;
    char rbuf[MAXPATHLEN];
    char *rp;

    if (addSpace && vpstate->varSpace) {
        Buf_AddByte(buf, vpstate->varSpace);
    }
    addSpace = TRUE;
    rp = cached_realpath(word, rbuf);
    if (rp && *rp == '/' && stat(rp, &st) == 0)
        word = rp;

    Buf_AddBytes(buf, strlen(word), word);
    return (addSpace);
}

/*-
 *-----------------------------------------------------------------------
 * VarModify --
 *	Modify each of the words of the passed string using the given
 *	function. Used to implement all modifiers.
 *
 * Input:
 *	str		String whose words should be trimmed
 *	modProc		Function to use to modify them
 *	datum		Datum to pass it
 *
 * Results:
 *	A string of all the words modified appropriately.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarModify(GNode *ctx, Var_Parse_State *vpstate,
          const char *str,
          Boolean (*modProc)(GNode *, Var_Parse_State *, char *,
                             Boolean, Buffer *, void *),
          void *datum) {
    Buffer buf;            /* Buffer for the new string */
    Boolean addSpace;        /* TRUE if need to add a space to the
				     * buffer before adding the trimmed
				     * word */
    char **av;                /* word list */
    char *as;                /* word list memory */
    int ac, i;

    Buf_Init(&buf, 0);
    addSpace = FALSE;

    if (vpstate->oneBigWord) {
        /* fake what brk_string() would do if there were only one word */
        ac = 1;
        av = bmake_malloc((ac + 1) * sizeof(char *));
        as = bmake_strdup(str);
        av[0] = as;
        av[1] = NULL;
    } else {
        av = brk_string(str, &ac, FALSE, &as);
    }

    for (i = 0; i < ac; i++) {
        addSpace = (*modProc)(ctx, vpstate, av[i], addSpace, &buf, datum);
    }

    free(as);
    free(av);

    return Buf_Destroy(&buf, FALSE);
}


static int
VarWordCompare(const void *a, const void *b) {
    int r = strcmp(*(const char *const *) a, *(const char *const *) b);
    return r;
}

/*-
 *-----------------------------------------------------------------------
 * VarOrder --
 *	Order the words in the string.
 *
 * Input:
 *	str		String whose words should be sorted.
 *	otype		How to order: s - sort, x - random.
 *
 * Results:
 *	A string containing the words ordered.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarOrder(const char *str, const char otype) {
    Buffer buf;            /* Buffer for the new string */
    char **av;                /* word list [first word does not count] */
    char *as;                /* word list memory */
    int ac, i;

    Buf_Init(&buf, 0);

    av = brk_string(str, &ac, FALSE, &as);

    if (ac > 0)
        switch (otype) {
            case 's':    /* sort alphabetically */
                qsort(av, ac, sizeof(char *), VarWordCompare);
                break;
            case 'x':    /* randomize */
            {
                int rndidx;
                char *t;

                /*
	     * We will use [ac..2] range for mod factors. This will produce
	     * random numbers in [(ac-1)..0] interval, and minimal
	     * reasonable value for mod factor is 2 (the mod 1 will produce
	     * 0 with probability 1).
	     */
                for (i = ac - 1; i > 0; i--) {
                    rndidx = random() % (i + 1);
                    if (i != rndidx) {
                        t = av[i];
                        av[i] = av[rndidx];
                        av[rndidx] = t;
                    }
                }
            }
        } /* end of switch */

    for (i = 0; i < ac; i++) {
        Buf_AddBytes(&buf, strlen(av[i]), av[i]);
        if (i != ac - 1)
            Buf_AddByte(&buf, ' ');
    }

    free(as);
    free(av);

    return Buf_Destroy(&buf, FALSE);
}


/*-
 *-----------------------------------------------------------------------
 * VarUniq --
 *	Remove adjacent duplicate words.
 *
 * Input:
 *	str		String whose words should be sorted
 *
 * Results:
 *	A string containing the resulting words.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarUniq(const char *str) {
    Buffer buf;            /* Buffer for new string */
    char **av;            /* List of words to affect */
    char *as;            /* Word list memory */
    int ac, i, j;

    Buf_Init(&buf, 0);
    av = brk_string(str, &ac, FALSE, &as);

    if (ac > 1) {
        for (j = 0, i = 1; i < ac; i++)
            if (strcmp(av[i], av[j]) != 0 && (++j != i))
                av[j] = av[i];
        ac = j + 1;
    }

    for (i = 0; i < ac; i++) {
        Buf_AddBytes(&buf, strlen(av[i]), av[i]);
        if (i != ac - 1)
            Buf_AddByte(&buf, ' ');
    }

    free(as);
    free(av);

    return Buf_Destroy(&buf, FALSE);
}

/*-
 *-----------------------------------------------------------------------
 * VarRange --
 *	Return an integer sequence
 *
 * Input:
 *	str		String whose words provide default range
 *	ac		range length, if 0 use str words
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarRange(const char *str, int ac) {
    Buffer buf;            /* Buffer for new string */
    char tmp[32];        /* each element */
    char **av;            /* List of words to affect */
    char *as;            /* Word list memory */
    int i, n;

    Buf_Init(&buf, 0);
    if (ac > 0) {
        as = NULL;
        av = NULL;
    } else {
        av = brk_string(str, &ac, FALSE, &as);
    }
    for (i = 0; i < ac; i++) {
        n = snprintf(tmp, sizeof(tmp), "%d", 1 + i);
        if (n >= (int) sizeof(tmp))
            break;
        Buf_AddBytes(&buf, n, tmp);
        if (i != ac - 1)
            Buf_AddByte(&buf, ' ');
    }

    free(as);
    free(av);

    return Buf_Destroy(&buf, FALSE);
}


/*-
 *-----------------------------------------------------------------------
 * VarGetPattern --
 *	Pass through the tstr looking for 1) escaped delimiters,
 *	'$'s and backslashes (place the escaped character in
 *	uninterpreted) and 2) unescaped $'s that aren't before
 *	the delimiter (expand the variable substitution unless flags
 *	has VAR_NOSUBST set).
 *	Return the expanded string or NULL if the delimiter was missing
 *	If pattern is specified, handle escaped ampersands, and replace
 *	unescaped ampersands with the lhs of the pattern.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *	If length is specified, return the string length of the buffer
 *	If flags is specified and the last character of the pattern is a
 *	$ set the VAR_MATCH_END bit of flags.
 *
 * Side Effects:
 *	None.
 *-----------------------------------------------------------------------
 */
static char *
VarGetPattern(GNode *ctxt, Var_Parse_State *vpstate MAKE_ATTR_UNUSED,
              int flags, const char **tstr, int delim, int *vflags,
              int *length, VarPattern *pattern) {
    const char *cp;
    char *rstr;
    Buffer buf;
    int junk;
    int errnum = flags & VARF_UNDEFERR;

    Buf_Init(&buf, 0);
    if (length == NULL)
        length = &junk;

#define IS_A_MATCH(cp, delim) \
    ((cp[0] == '\\') && ((cp[1] == delim) ||  \
     (cp[1] == '\\') || (cp[1] == '$') || (pattern && (cp[1] == '&'))))

    /*
     * Skim through until the matching delimiter is found;
     * pick up variable substitutions on the way. Also allow
     * backslashes to quote the delimiter, $, and \, but don't
     * touch other backslashes.
     */
    for (cp = *tstr; *cp && (*cp != delim); cp++) {
        if (IS_A_MATCH(cp, delim)) {
            Buf_AddByte(&buf, cp[1]);
            cp++;
        } else if (*cp == '$') {
            if (cp[1] == delim) {
                if (vflags == NULL)
                    Buf_AddByte(&buf, *cp);
                else
                    /*
		     * Unescaped $ at end of pattern => anchor
		     * pattern at end.
		     */
                    *vflags |= VAR_MATCH_END;
            } else {
                if (vflags == NULL || (*vflags & VAR_NOSUBST) == 0) {
                    char *cp2;
                    int len;
                    void *freeIt;

                    /*
		     * If unescaped dollar sign not before the
		     * delimiter, assume it's a variable
		     * substitution and recurse.
		     */
                    cp2 = Var_Parse(cp, ctxt, errnum | VARF_WANTRES, &len,
                                    &freeIt);
                    Buf_AddBytes(&buf, strlen(cp2), cp2);
                    free(freeIt);
                    cp += len - 1;
                } else {
                    const char *cp2 = &cp[1];

                    if (*cp2 == PROPEN || *cp2 == BROPEN) {
                        /*
			 * Find the end of this variable reference
			 * and suck it in without further ado.
			 * It will be interperated later.
			 */
                        int have = *cp2;
                        int want = (*cp2 == PROPEN) ? PRCLOSE : BRCLOSE;
                        int depth = 1;

                        for (++cp2; *cp2 != '\0' && depth > 0; ++cp2) {
                            if (cp2[-1] != '\\') {
                                if (*cp2 == have)
                                    ++depth;
                                if (*cp2 == want)
                                    --depth;
                            }
                        }
                        Buf_AddBytes(&buf, cp2 - cp, cp);
                        cp = --cp2;
                    } else
                        Buf_AddByte(&buf, *cp);
                }
            }
        } else if (pattern && *cp == '&')
            Buf_AddBytes(&buf, pattern->leftLen, pattern->lhs);
        else
            Buf_AddByte(&buf, *cp);
    }

    if (*cp != delim) {
        *tstr = cp;
        *length = 0;
        return NULL;
    }

    *tstr = ++cp;
    *length = Buf_Size(&buf);
    rstr = Buf_Destroy(&buf, FALSE);
    if (DEBUG(VAR))
        fprintf(debug_file, "Modifier pattern: \"%s\"\n", rstr);
    return rstr;
}

/*-
 *-----------------------------------------------------------------------
 * VarQuote --
 *	Quote shell meta-characters and space characters in the string
 *	if quoteDollar is set, also quote and double any '$' characters.
 *
 * Results:
 *	The quoted string
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarQuote(char *str, Boolean quoteDollar) {

    Buffer buf;
    const char *newline;
    size_t nlen;

    if ((newline = Shell_GetNewline()) == NULL)
        newline = "\\\n";
    nlen = strlen(newline);

    Buf_Init(&buf, 0);

    for (; *str != '\0'; str++) {
        if (*str == '\n') {
            Buf_AddBytes(&buf, nlen, newline);
            continue;
        }
        if (isspace((unsigned char) *str) || ismeta((unsigned char) *str))
            Buf_AddByte(&buf, '\\');
        Buf_AddByte(&buf, *str);
        if (quoteDollar && *str == '$')
            Buf_AddBytes(&buf, 2, "\\$");
    }

    str = Buf_Destroy(&buf, FALSE);
    if (DEBUG(VAR))
        fprintf(debug_file, "QuoteMeta: [%s]\n", str);
    return str;
}

/*-
 *-----------------------------------------------------------------------
 * VarHash --
 *      Hash the string using the MurmurHash3 algorithm.
 *      Output is computed using 32bit Little Endian arithmetic.
 *
 * Input:
 *	str		String to modify
 *
 * Results:
 *      Hash value of str, encoded as 8 hex digits.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------
 */
static char *
VarHash(char *str) {
    static const char hexdigits[16] = "0123456789abcdef";
    Buffer buf;
    size_t len, len2;
    unsigned char *ustr = (unsigned char *) str;
    unsigned int h, k, c1, c2;

    h = 0x971e137bU;
    c1 = 0x95543787U;
    c2 = 0x2ad7eb25U;
    len2 = strlen(str);

    for (len = len2; len;) {
        k = 0;
        switch (len) {
            default:
                k = (ustr[3] << 24) | (ustr[2] << 16) | (ustr[1] << 8) | ustr[0];
                len -= 4;
                ustr += 4;
                break;
            case 3:
                k |= (ustr[2] << 16);
            case 2:
                k |= (ustr[1] << 8);
            case 1:
                k |= ustr[0];
                len = 0;
        }
        c1 = c1 * 5 + 0x7b7d159cU;
        c2 = c2 * 5 + 0x6bce6396U;
        k *= c1;
        k = (k << 11) ^ (k >> 21);
        k *= c2;
        h = (h << 13) ^ (h >> 19);
        h = h * 5 + 0x52dce729U;
        h ^= k;
    }
    h ^= len2;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    Buf_Init(&buf, 0);
    for (len = 0; len < 8; ++len) {
        Buf_AddByte(&buf, hexdigits[h & 15]);
        h >>= 4;
    }

    return Buf_Destroy(&buf, FALSE);
}

static char *
VarStrftime(const char *fmt, int zulu, time_t utc) {
    char buf[BUFSIZ];

    if (!utc)
        time(&utc);
    if (!*fmt)
        fmt = "%c";
    strftime(buf, sizeof(buf), fmt, zulu ? gmtime(&utc) : localtime(&utc));

    buf[sizeof(buf) - 1] = '\0';
    return bmake_strdup(buf);
}

/*
 * Now we need to apply any modifiers the user wants applied.
 * These are:
 *  	  :M<pattern>	words which match the given <pattern>.
 *  			<pattern> is of the standard file
 *  			wildcarding form.
 *  	  :N<pattern>	words which do not match the given <pattern>.
 *  	  :S<d><pat1><d><pat2><d>[1gW]
 *  			Substitute <pat2> for <pat1> in the value
 *  	  :C<d><pat1><d><pat2><d>[1gW]
 *  			Substitute <pat2> for regex <pat1> in the value
 *  	  :H		Substitute the head of each word
 *  	  :T		Substitute the tail of each word
 *  	  :E		Substitute the extension (minus '.') of
 *  			each word
 *  	  :R		Substitute the root of each word
 *  			(pathname minus the suffix).
 *	  :O		("Order") Alphabeticaly sort words in variable.
 *	  :Ox		("intermiX") Randomize words in variable.
 *	  :u		("uniq") Remove adjacent duplicate words.
 *	  :tu		Converts the variable contents to uppercase.
 *	  :tl		Converts the variable contents to lowercase.
 *	  :ts[c]	Sets varSpace - the char used to
 *			separate words to 'c'. If 'c' is
 *			omitted then no separation is used.
 *	  :tW		Treat the variable contents as a single
 *			word, even if it contains spaces.
 *			(Mnemonic: one big 'W'ord.)
 *	  :tw		Treat the variable contents as multiple
 *			space-separated words.
 *			(Mnemonic: many small 'w'ords.)
 *	  :[index]	Select a single word from the value.
 *	  :[start..end]	Select multiple words from the value.
 *	  :[*] or :[0]	Select the entire value, as a single
 *			word.  Equivalent to :tW.
 *	  :[@]		Select the entire value, as multiple
 *			words.	Undoes the effect of :[*].
 *			Equivalent to :tw.
 *	  :[#]		Returns the number of words in the value.
 *
 *	  :?<true-value>:<false-value>
 *			If the variable evaluates to true, return
 *			true value, else return the second value.
 *    	  :lhs=rhs  	Like :S, but the rhs goes to the end of
 *    			the invocation.
 *	  :sh		Treat the current value as a command
 *			to be run, new value is its output.
 * The following added so we can handle ODE makefiles.
 *	  :@<tmpvar>@<newval>@
 *			Assign a temporary local variable <tmpvar>
 *			to the current value of each word in turn
 *			and replace each word with the result of
 *			evaluating <newval>
 *	  :D<newval>	Use <newval> as value if variable defined
 *	  :U<newval>	Use <newval> as value if variable undefined
 *	  :L		Use the name of the variable as the value.
 *	  :P		Use the path of the node that has the same
 *			name as the variable as the value.  This
 *			basically includes an implied :L so that
 *			the common method of refering to the path
 *			of your dependent 'x' in a rule is to use
 *			the form '${x:P}'.
 *	  :!<cmd>!	Run cmd much the same as :sh run's the
 *			current value of the variable.
 * The ::= modifiers, actually assign a value to the variable.
 * Their main purpose is in supporting modifiers of .for loop
 * iterators and other obscure uses.  They always expand to
 * nothing.  In a target rule that would otherwise expand to an
 * empty line they can be preceded with @: to keep make happy.
 * Eg.
 *
 * foo:	.USE
 * .for i in ${.TARGET} ${.TARGET:R}.gz
 * 	@: ${t::=$i}
 *	@echo blah ${t:T}
 * .endfor
 *
 *	  ::=<str>	Assigns <str> as the new value of variable.
 *	  ::?=<str>	Assigns <str> as value of variable if
 *			it was not already set.
 *	  ::+=<str>	Appends <str> to variable.
 *	  ::!=<cmd>	Assigns output of <cmd> as the new value of
 *			variable.
 */

/* we now have some modifiers with long names */
#define STRMOD_MATCH(s, want, n) \
    (strncmp(s, want, n) == 0 && (s[n] == endc || s[n] == ':'))
#define STRMOD_MATCHX(s, want, n) \
    (strncmp(s, want, n) == 0 && (s[n] == endc || s[n] == ':' || s[n] == '='))
#define CHARMOD_MATCH(c) (c == endc || c == ':')

static char *
ApplyModifiers(char *nstr, const char *tstr,
               int startc, int endc,
               Var *v, GNode *ctxt, int flags,
               int *lengthPtr, void **freePtr) {
    const char *start;
    const char *cp;        /* Secondary pointer into str (place marker
				 * for tstr) */
    char *newStr;    /* New value to return */
    char *ep;
    char termc;    /* Character which terminated scan */
    int cnt;    /* Used to count brace pairs when variable in
				 * in parens or braces */
    char delim;
    int modifier;    /* that we are processing */
    Var_Parse_State parsestate; /* Flags passed to helper functions */
    time_t utc;        /* for VarStrftime */

    delim = '\0';
    parsestate.oneBigWord = FALSE;
    parsestate.varSpace = ' ';    /* word separator */

    start = cp = tstr;

    while (*tstr && *tstr != endc) {

        if (*tstr == '$') {
            /*
	     * We may have some complex modifiers in a variable.
	     */
            void *freeIt;
            char *rval;
            int rlen;
            int c;

            rval = Var_Parse(tstr, ctxt, flags, &rlen, &freeIt);

            /*
	     * If we have not parsed up to endc or ':',
	     * we are not interested.
	     */
            if (rval != NULL && *rval &&
                (c = tstr[rlen]) != '\0' &&
                c != ':' &&
                c != endc) {
                free(freeIt);
                goto apply_mods;
            }

            if (DEBUG(VAR)) {
                fprintf(debug_file, "Got '%s' from '%.*s'%.*s\n",
                        rval, rlen, tstr, rlen, tstr + rlen);
            }

            tstr += rlen;

            if (rval != NULL && *rval) {
                int used;

                nstr = ApplyModifiers(nstr, rval,
                                      0, 0, v, ctxt, flags, &used, freePtr);
                if (nstr == var_Error
                    || (nstr == varNoError && (flags & VARF_UNDEFERR) == 0)
                    || strlen(rval) != (size_t) used) {
                    free(freeIt);
                    goto out;        /* error already reported */
                }
            }
            free(freeIt);
            if (*tstr == ':')
                tstr++;
            else if (!*tstr && endc) {
                Error("Unclosed variable specification after complex modifier (expecting '%c') for %s", endc, v->name);
                goto out;
            }
            continue;
        }
        apply_mods:
        if (DEBUG(VAR)) {
            fprintf(debug_file, "Applying[%s] :%c to \"%s\"\n", v->name,
                    *tstr, nstr);
        }
        newStr = var_Error;
        switch ((modifier = *tstr)) {
            case ':': {
                if (tstr[1] == '=' ||
                    (tstr[2] == '=' &&
                     (tstr[1] == '!' || tstr[1] == '+' || tstr[1] == '?'))) {
                    /*
		     * "::=", "::!=", "::+=", or "::?="
		     */
                    GNode *v_ctxt;        /* context where v belongs */
                    const char *emsg;
                    char *sv_name;
                    VarPattern pattern;
                    int how;
                    int vflags;

                    if (v->name[0] == 0)
                        goto bad_modifier;

                    v_ctxt = ctxt;
                    sv_name = NULL;
                    ++tstr;
                    if (v->flags & VAR_JUNK) {
                        /*
			 * We need to bmake_strdup() it incase
			 * VarGetPattern() recurses.
			 */
                        sv_name = v->name;
                        v->name = bmake_strdup(v->name);
                    } else if (ctxt != VAR_GLOBAL) {
                        Var *gv = VarFind(v->name, ctxt, 0);
                        if (gv == NULL)
                            v_ctxt = VAR_GLOBAL;
                        else
                            VarFreeEnv(gv, TRUE);
                    }

                    switch ((how = *tstr)) {
                        case '+':
                        case '?':
                        case '!':
                            cp = &tstr[2];
                            break;
                        default:
                            cp = ++tstr;
                            break;
                    }
                    delim = startc == PROPEN ? PRCLOSE : BRCLOSE;
                    pattern.flags = 0;

                    vflags = (flags & VARF_WANTRES) ? 0 : VAR_NOSUBST;
                    pattern.rhs = VarGetPattern(ctxt, &parsestate, flags,
                                                &cp, delim, &vflags,
                                                &pattern.rightLen,
                                                NULL);
                    if (v->flags & VAR_JUNK) {
                        /* restore original name */
                        free(v->name);
                        v->name = sv_name;
                    }
                    if (pattern.rhs == NULL)
                        goto cleanup;

                    termc = *--cp;
                    delim = '\0';

                    if (flags & VARF_WANTRES) {
                        switch (how) {
                            case '+':
                                Var_Append(v->name, pattern.rhs, v_ctxt);
                                break;
                            case '!':
                                newStr = Cmd_Exec(pattern.rhs, &emsg);
                                if (emsg)
                                    Error(emsg, nstr);
                                else
                                    Var_Set(v->name, newStr, v_ctxt, 0);
                                free(newStr);
                                break;
                            case '?':
                                if ((v->flags & VAR_JUNK) == 0)
                                    break;
                                /* FALLTHROUGH */
                            default:
                                Var_Set(v->name, pattern.rhs, v_ctxt, 0);
                                break;
                        }
                    }
                    free(UNCONST(pattern.rhs));
                    newStr = varNoError;
                    break;
                }
                goto default_case; /* "::<unrecognised>" */
            }
            case '@': {
                VarLoop_t loop;
                int vflags = VAR_NOSUBST;

                cp = ++tstr;
                delim = '@';
                if ((loop.tvar = VarGetPattern(ctxt, &parsestate, flags,
                                               &cp, delim,
                                               &vflags, &loop.tvarLen,
                                               NULL)) == NULL)
                    goto cleanup;

                if ((loop.str = VarGetPattern(ctxt, &parsestate, flags,
                                              &cp, delim,
                                              &vflags, &loop.strLen,
                                              NULL)) == NULL)
                    goto cleanup;

                termc = *cp;
                delim = '\0';

                loop.errnum = flags & VARF_UNDEFERR;
                loop.ctxt = ctxt;
                newStr = VarModify(ctxt, &parsestate, nstr, VarLoopExpand,
                                   &loop);
                Var_Delete(loop.tvar, ctxt);
                free(loop.tvar);
                free(loop.str);
                break;
            }
            case '_':            /* remember current value */
                cp = tstr + 1;    /* make sure it is set */
                if (STRMOD_MATCHX(tstr, "_", 1)) {
                    if (tstr[1] == '=') {
                        char *np;
                        int n;

                        cp++;
                        n = strcspn(cp, ":)}");
                        np = bmake_strndup(cp, n + 1);
                        np[n] = '\0';
                        cp = tstr + 2 + n;
                        Var_Set(np, nstr, ctxt, 0);
                        free(np);
                    } else {
                        Var_Set("_", nstr, ctxt, 0);
                    }
                    newStr = nstr;
                    termc = *cp;
                    break;
                }
                goto default_case;
            case 'D':
            case 'U': {
                Buffer buf;        /* Buffer for patterns */
                int nflags;

                if (flags & VARF_WANTRES) {
                    int wantres;
                    if (*tstr == 'U')
                        wantres = ((v->flags & VAR_JUNK) != 0);
                    else
                        wantres = ((v->flags & VAR_JUNK) == 0);
                    nflags = flags & ~VARF_WANTRES;
                    if (wantres)
                        nflags |= VARF_WANTRES;
                } else
                    nflags = flags;
                /*
		 * Pass through tstr looking for 1) escaped delimiters,
		 * '$'s and backslashes (place the escaped character in
		 * uninterpreted) and 2) unescaped $'s that aren't before
		 * the delimiter (expand the variable substitution).
		 * The result is left in the Buffer buf.
		 */
                Buf_Init(&buf, 0);
                for (cp = tstr + 1;
                     *cp != endc && *cp != ':' && *cp != '\0';
                     cp++) {
                    if ((*cp == '\\') &&
                        ((cp[1] == ':') ||
                         (cp[1] == '$') ||
                         (cp[1] == endc) ||
                         (cp[1] == '\\'))) {
                        Buf_AddByte(&buf, cp[1]);
                        cp++;
                    } else if (*cp == '$') {
                        /*
			     * If unescaped dollar sign, assume it's a
			     * variable substitution and recurse.
			     */
                        char *cp2;
                        int len;
                        void *freeIt;

                        cp2 = Var_Parse(cp, ctxt, nflags, &len, &freeIt);
                        Buf_AddBytes(&buf, strlen(cp2), cp2);
                        free(freeIt);
                        cp += len - 1;
                    } else {
                        Buf_AddByte(&buf, *cp);
                    }
                }

                termc = *cp;

                if ((v->flags & VAR_JUNK) != 0)
                    v->flags |= VAR_KEEP;
                if (nflags & VARF_WANTRES) {
                    newStr = Buf_Destroy(&buf, FALSE);
                } else {
                    newStr = nstr;
                    Buf_Destroy(&buf, TRUE);
                }
                break;
            }
            case 'L': {
                if ((v->flags & VAR_JUNK) != 0)
                    v->flags |= VAR_KEEP;
                newStr = bmake_strdup(v->name);
                cp = ++tstr;
                termc = *tstr;
                break;
            }
            case 'P': {
                GNode *gn;

                if ((v->flags & VAR_JUNK) != 0)
                    v->flags |= VAR_KEEP;
                gn = Targ_FindNode(v->name, TARG_NOCREATE);
                if (gn == NULL || gn->type & OP_NOPATH) {
                    newStr = NULL;
                } else if (gn->path) {
                    newStr = bmake_strdup(gn->path);
                } else {
                    newStr = Dir_FindFile(v->name, Suff_FindPath(gn));
                }
                if (!newStr) {
                    newStr = bmake_strdup(v->name);
                }
                cp = ++tstr;
                termc = *tstr;
                break;
            }
            case '!': {
                const char *emsg;
                VarPattern pattern;
                pattern.flags = 0;

                delim = '!';
                emsg = NULL;
                cp = ++tstr;
                if ((pattern.rhs = VarGetPattern(ctxt, &parsestate, flags,
                                                 &cp, delim,
                                                 NULL, &pattern.rightLen,
                                                 NULL)) == NULL)
                    goto cleanup;
                if (flags & VARF_WANTRES)
                    newStr = Cmd_Exec(pattern.rhs, &emsg);
                else
                    newStr = varNoError;
                free(UNCONST(pattern.rhs));
                if (emsg)
                    Error(emsg, nstr);
                termc = *cp;
                delim = '\0';
                if (v->flags & VAR_JUNK) {
                    v->flags |= VAR_KEEP;
                }
                break;
            }
            case '[': {
                /*
		 * Look for the closing ']', recursively
		 * expanding any embedded variables.
		 *
		 * estr is a pointer to the expanded result,
		 * which we must free().
		 */
                char *estr;

                cp = tstr + 1; /* point to char after '[' */
                delim = ']'; /* look for closing ']' */
                estr = VarGetPattern(ctxt, &parsestate,
                                     flags, &cp, delim,
                                     NULL, NULL, NULL);
                if (estr == NULL)
                    goto cleanup; /* report missing ']' */
                /* now cp points just after the closing ']' */
                delim = '\0';
                if (cp[0] != ':' && cp[0] != endc) {
                    /* Found junk after ']' */
                    free(estr);
                    goto bad_modifier;
                }
                if (estr[0] == '\0') {
                    /* Found empty square brackets in ":[]". */
                    free(estr);
                    goto bad_modifier;
                } else if (estr[0] == '#' && estr[1] == '\0') {
                    /* Found ":[#]" */

                    /*
		     * We will need enough space for the decimal
		     * representation of an int.  We calculate the
		     * space needed for the octal representation,
		     * and add enough slop to cope with a '-' sign
		     * (which should never be needed) and a '\0'
		     * string terminator.
		     */
                    int newStrSize =
                            (sizeof(int) * CHAR_BIT + 2) / 3 + 2;

                    newStr = bmake_malloc(newStrSize);
                    if (parsestate.oneBigWord) {
                        strncpy(newStr, "1", newStrSize);
                    } else {
                        /* XXX: brk_string() is a rather expensive
			 * way of counting words. */
                        char **av;
                        char *as;
                        int ac;

                        av = brk_string(nstr, &ac, FALSE, &as);
                        snprintf(newStr, newStrSize, "%d", ac);
                        free(as);
                        free(av);
                    }
                    termc = *cp;
                    free(estr);
                    break;
                } else if (estr[0] == '*' && estr[1] == '\0') {
                    /* Found ":[*]" */
                    parsestate.oneBigWord = TRUE;
                    newStr = nstr;
                    termc = *cp;
                    free(estr);
                    break;
                } else if (estr[0] == '@' && estr[1] == '\0') {
                    /* Found ":[@]" */
                    parsestate.oneBigWord = FALSE;
                    newStr = nstr;
                    termc = *cp;
                    free(estr);
                    break;
                } else {
                    /*
		     * We expect estr to contain a single
		     * integer for :[N], or two integers
		     * separated by ".." for :[start..end].
		     */
                    VarSelectWords_t seldata = {0, 0};

                    seldata.start = strtol(estr, &ep, 0);
                    if (ep == estr) {
                        /* Found junk instead of a number */
                        free(estr);
                        goto bad_modifier;
                    } else if (ep[0] == '\0') {
                        /* Found only one integer in :[N] */
                        seldata.end = seldata.start;
                    } else if (ep[0] == '.' && ep[1] == '.' &&
                               ep[2] != '\0') {
                        /* Expecting another integer after ".." */
                        ep += 2;
                        seldata.end = strtol(ep, &ep, 0);
                        if (ep[0] != '\0') {
                            /* Found junk after ".." */
                            free(estr);
                            goto bad_modifier;
                        }
                    } else {
                        /* Found junk instead of ".." */
                        free(estr);
                        goto bad_modifier;
                    }
                    /*
		     * Now seldata is properly filled in,
		     * but we still have to check for 0 as
		     * a special case.
		     */
                    if (seldata.start == 0 && seldata.end == 0) {
                        /* ":[0]" or perhaps ":[0..0]" */
                        parsestate.oneBigWord = TRUE;
                        newStr = nstr;
                        termc = *cp;
                        free(estr);
                        break;
                    } else if (seldata.start == 0 ||
                               seldata.end == 0) {
                        /* ":[0..N]" or ":[N..0]" */
                        free(estr);
                        goto bad_modifier;
                    }
                    /*
		     * Normal case: select the words
		     * described by seldata.
		     */
                    newStr = VarSelectWords(ctxt, &parsestate,
                                            nstr, &seldata);

                    termc = *cp;
                    free(estr);
                    break;
                }

            }
            case 'g':
                cp = tstr + 1;    /* make sure it is set */
                if (STRMOD_MATCHX(tstr, "gmtime", 6)) {
                    if (tstr[6] == '=') {
                        utc = strtoul(&tstr[7], &ep, 10);
                        cp = ep;
                    } else {
                        utc = 0;
                        cp = tstr + 6;
                    }
                    newStr = VarStrftime(nstr, 1, utc);
                    termc = *cp;
                } else {
                    goto default_case;
                }
                break;
            case 'h':
                cp = tstr + 1;    /* make sure it is set */
                if (STRMOD_MATCH(tstr, "hash", 4)) {
                    newStr = VarHash(nstr);
                    cp = tstr + 4;
                    termc = *cp;
                } else {
                    goto default_case;
                }
                break;
            case 'l':
                cp = tstr + 1;    /* make sure it is set */
                if (STRMOD_MATCHX(tstr, "localtime", 9)) {
                    if (tstr[9] == '=') {
                        utc = strtoul(&tstr[10], &ep, 10);
                        cp = ep;
                    } else {
                        utc = 0;
                        cp = tstr + 9;
                    }
                    newStr = VarStrftime(nstr, 0, utc);
                    termc = *cp;
                } else {
                    goto default_case;
                }
                break;
            case 't': {
                cp = tstr + 1;    /* make sure it is set */
                if (tstr[1] != endc && tstr[1] != ':') {
                    if (tstr[1] == 's') {
                        /*
			 * Use the char (if any) at tstr[2]
			 * as the word separator.
			 */
                        VarPattern pattern;

                        if (tstr[2] != endc &&
                            (tstr[3] == endc || tstr[3] == ':')) {
                            /* ":ts<unrecognised><endc>" or
			     * ":ts<unrecognised>:" */
                            parsestate.varSpace = tstr[2];
                            cp = tstr + 3;
                        } else if (tstr[2] == endc || tstr[2] == ':') {
                            /* ":ts<endc>" or ":ts:" */
                            parsestate.varSpace = 0; /* no separator */
                            cp = tstr + 2;
                        } else if (tstr[2] == '\\') {
                            const char *xp = &tstr[3];
                            int base = 8; /* assume octal */

                            switch (tstr[3]) {
                                case 'n':
                                    parsestate.varSpace = '\n';
                                    cp = tstr + 4;
                                    break;
                                case 't':
                                    parsestate.varSpace = '\t';
                                    cp = tstr + 4;
                                    break;
                                case 'x':
                                    base = 16;
                                    xp++;
                                    goto get_numeric;
                                case '0':
                                    base = 0;
                                    goto get_numeric;
                                default:
                                    if (isdigit((unsigned char) tstr[3])) {

                                        get_numeric:
                                        parsestate.varSpace =
                                                strtoul(xp, &ep, base);
                                        if (*ep != ':' && *ep != endc)
                                            goto bad_modifier;
                                        cp = ep;
                                    } else {
                                        /*
				     * ":ts<backslash><unrecognised>".
				     */
                                        goto bad_modifier;
                                    }
                                    break;
                            }
                        } else {
                            /*
			     * Found ":ts<unrecognised><unrecognised>".
			     */
                            goto bad_modifier;
                        }

                        termc = *cp;

                        /*
			 * We cannot be certain that VarModify
			 * will be used - even if there is a
			 * subsequent modifier, so do a no-op
			 * VarSubstitute now to for str to be
			 * re-expanded without the spaces.
			 */
                        pattern.flags = VAR_SUB_ONE;
                        pattern.lhs = pattern.rhs = "\032";
                        pattern.leftLen = pattern.rightLen = 1;

                        newStr = VarModify(ctxt, &parsestate, nstr,
                                           VarSubstitute,
                                           &pattern);
                    } else if (tstr[2] == endc || tstr[2] == ':') {
                        /*
			 * Check for two-character options:
			 * ":tu", ":tl"
			 */
                        if (tstr[1] == 'A') { /* absolute path */
                            newStr = VarModify(ctxt, &parsestate, nstr,
                                               VarRealpath, NULL);
                            cp = tstr + 2;
                            termc = *cp;
                        } else if (tstr[1] == 'u') {
                            char *dp = bmake_strdup(nstr);
                            for (newStr = dp; *dp; dp++)
                                *dp = toupper((unsigned char) *dp);
                            cp = tstr + 2;
                            termc = *cp;
                        } else if (tstr[1] == 'l') {
                            char *dp = bmake_strdup(nstr);
                            for (newStr = dp; *dp; dp++)
                                *dp = tolower((unsigned char) *dp);
                            cp = tstr + 2;
                            termc = *cp;
                        } else if (tstr[1] == 'W' || tstr[1] == 'w') {
                            parsestate.oneBigWord = (tstr[1] == 'W');
                            newStr = nstr;
                            cp = tstr + 2;
                            termc = *cp;
                        } else {
                            /* Found ":t<unrecognised>:" or
			     * ":t<unrecognised><endc>". */
                            goto bad_modifier;
                        }
                    } else {
                        /*
			 * Found ":t<unrecognised><unrecognised>".
			 */
                        goto bad_modifier;
                    }
                } else {
                    /*
		     * Found ":t<endc>" or ":t:".
		     */
                    goto bad_modifier;
                }
                break;
            }
            case 'N':
            case 'M': {
                char *pattern;
                const char *endpat; /* points just after end of pattern */
                char *cp2;
                Boolean copy;    /* pattern should be, or has been, copied */
                Boolean needSubst;
                int nest;

                copy = FALSE;
                needSubst = FALSE;
                nest = 1;
                /*
		 * In the loop below, ignore ':' unless we are at
		 * (or back to) the original brace level.
		 * XXX This will likely not work right if $() and ${}
		 * are intermixed.
		 */
                for (cp = tstr + 1;
                     *cp != '\0' && !(*cp == ':' && nest == 1);
                     cp++) {
                    if (*cp == '\\' &&
                        (cp[1] == ':' ||
                         cp[1] == endc || cp[1] == startc)) {
                        if (!needSubst) {
                            copy = TRUE;
                        }
                        cp++;
                        continue;
                    }
                    if (*cp == '$') {
                        needSubst = TRUE;
                    }
                    if (*cp == '(' || *cp == '{')
                        ++nest;
                    if (*cp == ')' || *cp == '}') {
                        --nest;
                        if (nest == 0)
                            break;
                    }
                }
                termc = *cp;
                endpat = cp;
                if (copy) {
                    /*
		     * Need to compress the \:'s out of the pattern, so
		     * allocate enough room to hold the uncompressed
		     * pattern (note that cp started at tstr+1, so
		     * cp - tstr takes the null byte into account) and
		     * compress the pattern into the space.
		     */
                    pattern = bmake_malloc(cp - tstr);
                    for (cp2 = pattern, cp = tstr + 1;
                         cp < endpat;
                         cp++, cp2++) {
                        if ((*cp == '\\') && (cp + 1 < endpat) &&
                            (cp[1] == ':' || cp[1] == endc)) {
                            cp++;
                        }
                        *cp2 = *cp;
                    }
                    *cp2 = '\0';
                    endpat = cp2;
                } else {
                    /*
		     * Either Var_Subst or VarModify will need a
		     * nul-terminated string soon, so construct one now.
		     */
                    pattern = bmake_strndup(tstr + 1, endpat - (tstr + 1));
                }
                if (needSubst) {
                    /*
		     * pattern contains embedded '$', so use Var_Subst to
		     * expand it.
		     */
                    cp2 = pattern;
                    pattern = Var_Subst(NULL, cp2, ctxt, flags | VARF_WANTRES);
                    free(cp2);
                }
                if (DEBUG(VAR))
                    fprintf(debug_file, "Pattern[%s] for [%s] is [%s]\n",
                            v->name, nstr, pattern);
                if (*tstr == 'M') {
                    newStr = VarModify(ctxt, &parsestate, nstr, VarMatch,
                                       pattern);
                } else {
                    newStr = VarModify(ctxt, &parsestate, nstr, VarNoMatch,
                                       pattern);
                }
                free(pattern);
                break;
            }
            case 'S': {
                VarPattern pattern;
                Var_Parse_State tmpparsestate;

                pattern.flags = 0;
                tmpparsestate = parsestate;
                delim = tstr[1];
                tstr += 2;

                /*
		 * If pattern begins with '^', it is anchored to the
		 * start of the word -- skip over it and flag pattern.
		 */
                if (*tstr == '^') {
                    pattern.flags |= VAR_MATCH_START;
                    tstr += 1;
                }

                cp = tstr;
                if ((pattern.lhs = VarGetPattern(ctxt, &parsestate, flags,
                                                 &cp, delim,
                                                 &pattern.flags,
                                                 &pattern.leftLen,
                                                 NULL)) == NULL)
                    goto cleanup;

                if ((pattern.rhs = VarGetPattern(ctxt, &parsestate, flags,
                                                 &cp, delim, NULL,
                                                 &pattern.rightLen,
                                                 &pattern)) == NULL)
                    goto cleanup;

                /*
		 * Check for global substitution. If 'g' after the final
		 * delimiter, substitution is global and is marked that
		 * way.
		 */
                for (;; cp++) {
                    switch (*cp) {
                        case 'g':
                            pattern.flags |= VAR_SUB_GLOBAL;
                            continue;
                        case '1':
                            pattern.flags |= VAR_SUB_ONE;
                            continue;
                        case 'W':
                            tmpparsestate.oneBigWord = TRUE;
                            continue;
                    }
                    break;
                }

                termc = *cp;
                newStr = VarModify(ctxt, &tmpparsestate, nstr,
                                   VarSubstitute,
                                   &pattern);

                /*
		 * Free the two strings.
		 */
                free(UNCONST(pattern.lhs));
                free(UNCONST(pattern.rhs));
                delim = '\0';
                break;
            }
            case '?': {
                VarPattern pattern;
                Boolean value;
                int cond_rc;
                int lhs_flags, rhs_flags;

                /* find ':', and then substitute accordingly */
                if (flags & VARF_WANTRES) {
                    cond_rc = Cond_EvalExpression(NULL, v->name, &value, 0, FALSE);
                    if (cond_rc == COND_INVALID) {
                        lhs_flags = rhs_flags = VAR_NOSUBST;
                    } else if (value) {
                        lhs_flags = 0;
                        rhs_flags = VAR_NOSUBST;
                    } else {
                        lhs_flags = VAR_NOSUBST;
                        rhs_flags = 0;
                    }
                } else {
                    /* we are just consuming and discarding */
                    cond_rc = value = 0;
                    lhs_flags = rhs_flags = VAR_NOSUBST;
                }
                pattern.flags = 0;

                cp = ++tstr;
                delim = ':';
                if ((pattern.lhs = VarGetPattern(ctxt, &parsestate, flags,
                                                 &cp, delim, &lhs_flags,
                                                 &pattern.leftLen,
                                                 NULL)) == NULL)
                    goto cleanup;

                /* BROPEN or PROPEN */
                delim = endc;
                if ((pattern.rhs = VarGetPattern(ctxt, &parsestate, flags,
                                                 &cp, delim, &rhs_flags,
                                                 &pattern.rightLen,
                                                 NULL)) == NULL)
                    goto cleanup;

                termc = *--cp;
                delim = '\0';
                if (cond_rc == COND_INVALID) {
                    Error("Bad conditional expression `%s' in %s?%s:%s",
                          v->name, v->name, pattern.lhs, pattern.rhs);
                    goto cleanup;
                }

                if (value) {
                    newStr = UNCONST(pattern.lhs);
                    free(UNCONST(pattern.rhs));
                } else {
                    newStr = UNCONST(pattern.rhs);
                    free(UNCONST(pattern.lhs));
                }
                if (v->flags & VAR_JUNK) {
                    v->flags |= VAR_KEEP;
                }
                break;
            }
#ifndef NO_REGEX
            case 'C': {
                VarREPattern pattern;
                char *re;
                int error;
                Var_Parse_State tmpparsestate;

                pattern.flags = 0;
                tmpparsestate = parsestate;
                delim = tstr[1];
                tstr += 2;

                cp = tstr;

                if ((re = VarGetPattern(ctxt, &parsestate, flags, &cp, delim,
                                        NULL, NULL, NULL)) == NULL)
                    goto cleanup;

                if ((pattern.replace = VarGetPattern(ctxt, &parsestate,
                                                     flags, &cp, delim, NULL,
                                                     NULL, NULL)) == NULL) {
                    free(re);
                    goto cleanup;
                }

                for (;; cp++) {
                    switch (*cp) {
                        case 'g':
                            pattern.flags |= VAR_SUB_GLOBAL;
                            continue;
                        case '1':
                            pattern.flags |= VAR_SUB_ONE;
                            continue;
                        case 'W':
                            tmpparsestate.oneBigWord = TRUE;
                            continue;
                    }
                    break;
                }

                termc = *cp;

                error = regcomp(&pattern.re, re, REG_EXTENDED);
                free(re);
                if (error) {
                    *lengthPtr = cp - start + 1;
                    VarREError(error, &pattern.re, "RE substitution error");
                    free(pattern.replace);
                    goto cleanup;
                }

                pattern.nsub = pattern.re.re_nsub + 1;
                if (pattern.nsub < 1)
                    pattern.nsub = 1;
                if (pattern.nsub > 10)
                    pattern.nsub = 10;
                pattern.matches = bmake_malloc(pattern.nsub *
                                               sizeof(regmatch_t));
                newStr = VarModify(ctxt, &tmpparsestate, nstr,
                                   VarRESubstitute,
                                   &pattern);
                regfree(&pattern.re);
                free(pattern.replace);
                free(pattern.matches);
                delim = '\0';
                break;
            }
#endif
            case 'q':
            case 'Q':
                if (tstr[1] == endc || tstr[1] == ':') {
                    newStr = VarQuote(nstr, modifier == 'q');
                    cp = tstr + 1;
                    termc = *cp;
                    break;
                }
                goto default_case;
            case 'T':
                if (tstr[1] == endc || tstr[1] == ':') {
                    newStr = VarModify(ctxt, &parsestate, nstr, VarTail,
                                       NULL);
                    cp = tstr + 1;
                    termc = *cp;
                    break;
                }
                goto default_case;
            case 'H':
                if (tstr[1] == endc || tstr[1] == ':') {
                    newStr = VarModify(ctxt, &parsestate, nstr, VarHead,
                                       NULL);
                    cp = tstr + 1;
                    termc = *cp;
                    break;
                }
                goto default_case;
            case 'E':
                if (tstr[1] == endc || tstr[1] == ':') {
                    newStr = VarModify(ctxt, &parsestate, nstr, VarSuffix,
                                       NULL);
                    cp = tstr + 1;
                    termc = *cp;
                    break;
                }
                goto default_case;
            case 'R':
                if (tstr[1] == endc || tstr[1] == ':') {
                    newStr = VarModify(ctxt, &parsestate, nstr, VarRoot,
                                       NULL);
                    cp = tstr + 1;
                    termc = *cp;
                    break;
                }
                goto default_case;
            case 'r':
                cp = tstr + 1;    /* make sure it is set */
                if (STRMOD_MATCHX(tstr, "range", 5)) {
                    int n;

                    if (tstr[5] == '=') {
                        n = strtoul(&tstr[6], &ep, 10);
                        cp = ep;
                    } else {
                        n = 0;
                        cp = tstr + 5;
                    }
                    newStr = VarRange(nstr, n);
                    termc = *cp;
                    break;
                }
                goto default_case;
            case 'O': {
                char otype;

                cp = tstr + 1;    /* skip to the rest in any case */
                if (tstr[1] == endc || tstr[1] == ':') {
                    otype = 's';
                    termc = *cp;
                } else if ((tstr[1] == 'x') &&
                           (tstr[2] == endc || tstr[2] == ':')) {
                    otype = tstr[1];
                    cp = tstr + 2;
                    termc = *cp;
                } else {
                    goto bad_modifier;
                }
                newStr = VarOrder(nstr, otype);
                break;
            }
            case 'u':
                if (tstr[1] == endc || tstr[1] == ':') {
                    newStr = VarUniq(nstr);
                    cp = tstr + 1;
                    termc = *cp;
                    break;
                }
                goto default_case;
#ifdef SUNSHCMD
            case 's':
                if (tstr[1] == 'h' && (tstr[2] == endc || tstr[2] == ':')) {
                    const char *emsg;
                    if (flags & VARF_WANTRES) {
                        newStr = Cmd_Exec(nstr, &emsg);
                        if (emsg)
                            Error(emsg, nstr);
                    } else
                        newStr = varNoError;
                    cp = tstr + 2;
                    termc = *cp;
                    break;
                }
                goto default_case;
#endif
            default:
            default_case:
            {
#ifdef SYSVVARSUB
                /*
	     * This can either be a bogus modifier or a System-V
	     * substitution command.
	     */
                VarPattern pattern;
                Boolean eqFound;

                pattern.flags = 0;
                eqFound = FALSE;
                /*
	     * First we make a pass through the string trying
	     * to verify it is a SYSV-make-style translation:
	     * it must be: <string1>=<string2>)
	     */
                cp = tstr;
                cnt = 1;
                while (*cp != '\0' && cnt) {
                    if (*cp == '=') {
                        eqFound = TRUE;
                        /* continue looking for endc */
                    } else if (*cp == endc)
                        cnt--;
                    else if (*cp == startc)
                        cnt++;
                    if (cnt)
                        cp++;
                }
                if (*cp == endc && eqFound) {

                    /*
		 * Now we break this sucker into the lhs and
		 * rhs. We must null terminate them of course.
		 */
                    delim = '=';
                    cp = tstr;
                    if ((pattern.lhs = VarGetPattern(ctxt, &parsestate,
                                                     flags, &cp, delim, &pattern.flags,
                                                     &pattern.leftLen, NULL)) == NULL)
                        goto cleanup;
                    delim = endc;
                    if ((pattern.rhs = VarGetPattern(ctxt, &parsestate,
                                                     flags, &cp, delim, NULL, &pattern.rightLen,
                                                     &pattern)) == NULL)
                        goto cleanup;

                    /*
		 * SYSV modifications happen through the whole
		 * string. Note the pattern is anchored at the end.
		 */
                    termc = *--cp;
                    delim = '\0';
                    if (pattern.leftLen == 0 && *nstr == '\0') {
                        newStr = nstr;    /* special case */
                    } else {
                        newStr = VarModify(ctxt, &parsestate, nstr,
                                           VarSYSVMatch,
                                           &pattern);
                    }
                    free(UNCONST(pattern.lhs));
                    free(UNCONST(pattern.rhs));
                } else
#endif
                {
                    Error("Unknown modifier '%c'", *tstr);
                    for (cp = tstr + 1;
                         *cp != ':' && *cp != endc && *cp != '\0';
                         cp++)
                        continue;
                    termc = *cp;
                    newStr = var_Error;
                }
            }
        }
        if (DEBUG(VAR)) {
            fprintf(debug_file, "Result[%s] of :%c is \"%s\"\n",
                    v->name, modifier, newStr);
        }

        if (newStr != nstr) {
            if (*freePtr) {
                free(nstr);
                *freePtr = NULL;
            }
            nstr = newStr;
            if (nstr != var_Error && nstr != varNoError) {
                *freePtr = nstr;
            }
        }
        if (termc == '\0' && endc != '\0') {
            Error("Unclosed variable specification (expecting '%c') for \"%s\" (value \"%s\") modifier %c", endc,
                  v->name, nstr, modifier);
        } else if (termc == ':') {
            cp++;
        }
        tstr = cp;
    }
    out:
    *lengthPtr = tstr - start;
    return (nstr);

    bad_modifier:
    /* "{(" */
    Error("Bad modifier `:%.*s' for %s", (int) strcspn(tstr, ":)}"), tstr,
          v->name);

    cleanup:
    *lengthPtr = cp - start;
    if (delim != '\0')
        Error("Unclosed substitution for %s (%c missing)",
              v->name, delim);
    free(*freePtr);
    *freePtr = NULL;
    return (var_Error);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Parse --
 *	Given the start of a variable invocation, extract the variable
 *	name and find its value, then modify it according to the
 *	specification.
 *
 * Input:
 *	str		The string to parse
 *	ctxt		The context for the variable
 *	flags		VARF_UNDEFERR	if undefineds are an error
 *			VARF_WANTRES	if we actually want the result
 *			VARF_ASSIGN	if we are in a := assignment
 *	lengthPtr	OUT: The length of the specification
 *	freePtr		OUT: Non-NULL if caller should free *freePtr
 *
 * Results:
 *	The (possibly-modified) value of the variable or var_Error if the
 *	specification is invalid. The length of the specification is
 *	placed in *lengthPtr (for invalid specifications, this is just
 *	2...?).
 *	If *freePtr is non-NULL then it's a pointer that the caller
 *	should pass to free() to free memory used by the result.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
/* coverity[+alloc : arg-*4] */
char *
Var_Parse(const char *str, GNode *ctxt, int flags,
          int *lengthPtr, void **freePtr) {
    const char *tstr;        /* Pointer into str */
    Var *v;        /* Variable in invocation */
    Boolean haveModifier;/* TRUE if have modifiers for the variable */
    char endc;        /* Ending character when variable in parens
				 * or braces */
    char startc;    /* Starting character when variable in parens
				 * or braces */
    int vlen;    /* Length of variable name */
    const char *start;    /* Points to original start of str */
    char *nstr;    /* New string, used during expansion */
    Boolean dynamic;    /* TRUE if the variable is local and we're
				 * expanding it in a non-local context. This
				 * is done to support dynamic sources. The
				 * result is just the invocation, unaltered */
    const char *extramodifiers; /* extra modifiers to apply first */
    char name[2];

    *freePtr = NULL;
    extramodifiers = NULL;
    dynamic = FALSE;
    start = str;

    startc = str[1];
    if (startc != PROPEN && startc != BROPEN) {
        /*
	 * If it's not bounded by braces of some sort, life is much simpler.
	 * We just need to check for the first character and return the
	 * value if it exists.
	 */

        /* Error out some really stupid names */
        if (startc == '\0' || strchr(")}:$", startc)) {
            *lengthPtr = 1;
            return var_Error;
        }
        name[0] = startc;
        name[1] = '\0';

        v = VarFind(name, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
        if (v == NULL) {
            *lengthPtr = 2;

            if ((ctxt == VAR_CMD) || (ctxt == VAR_GLOBAL)) {
                /*
		 * If substituting a local variable in a non-local context,
		 * assume it's for dynamic source stuff. We have to handle
		 * this specially and return the longhand for the variable
		 * with the dollar sign escaped so it makes it back to the
		 * caller. Only four of the local variables are treated
		 * specially as they are the only four that will be set
		 * when dynamic sources are expanded.
		 */
                switch (str[1]) {
                    case '@':
                        return UNCONST("$(.TARGET)");
                    case '%':
                        return UNCONST("$(.MEMBER)");
                    case '*':
                        return UNCONST("$(.PREFIX)");
                    case '!':
                        return UNCONST("$(.ARCHIVE)");
                }
            }
            /*
	     * Error
	     */
            return (flags & VARF_UNDEFERR) ? var_Error : varNoError;
        } else {
            haveModifier = FALSE;
            tstr = &str[1];
            endc = str[1];
        }
    } else {
        Buffer buf;    /* Holds the variable name */
        int depth = 1;

        endc = startc == PROPEN ? PRCLOSE : BRCLOSE;
        Buf_Init(&buf, 0);

        /*
	 * Skip to the end character or a colon, whichever comes first.
	 */
        for (tstr = str + 2; *tstr != '\0'; tstr++) {
            /*
	     * Track depth so we can spot parse errors.
	     */
            if (*tstr == startc) {
                depth++;
            }
            if (*tstr == endc) {
                if (--depth == 0)
                    break;
            }
            if (depth == 1 && *tstr == ':') {
                break;
            }
            /*
	     * A variable inside a variable, expand
	     */
            if (*tstr == '$') {
                int rlen;
                void *freeIt;
                char *rval = Var_Parse(tstr, ctxt, flags, &rlen, &freeIt);
                if (rval != NULL) {
                    Buf_AddBytes(&buf, strlen(rval), rval);
                }
                free(freeIt);
                tstr += rlen - 1;
            } else
                Buf_AddByte(&buf, *tstr);
        }
        if (*tstr == ':') {
            haveModifier = TRUE;
        } else if (*tstr == endc) {
            haveModifier = FALSE;
        } else {
            /*
	     * If we never did find the end character, return NULL
	     * right now, setting the length to be the distance to
	     * the end of the string, since that's what make does.
	     */
            *lengthPtr = tstr - str;
            Buf_Destroy(&buf, TRUE);
            return (var_Error);
        }
        str = Buf_GetAll(&buf, &vlen);

        /*
	 * At this point, str points into newly allocated memory from
	 * buf, containing only the name of the variable.
	 *
	 * start and tstr point into the const string that was pointed
	 * to by the original value of the str parameter.  start points
	 * to the '$' at the beginning of the string, while tstr points
	 * to the char just after the end of the variable name -- this
	 * will be '\0', ':', PRCLOSE, or BRCLOSE.
	 */

        v = VarFind(str, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
        /*
	 * Check also for bogus D and F forms of local variables since we're
	 * in a local context and the name is the right length.
	 */
        if ((v == NULL) && (ctxt != VAR_CMD) && (ctxt != VAR_GLOBAL) &&
            (vlen == 2) && (str[1] == 'F' || str[1] == 'D') &&
            strchr("@%?*!<>", str[0]) != NULL) {
            /*
	     * Well, it's local -- go look for it.
	     */
            name[0] = *str;
            name[1] = '\0';
            v = VarFind(name, ctxt, 0);

            if (v != NULL) {
                if (str[1] == 'D') {
                    extramodifiers = "H:";
                } else { /* F */
                    extramodifiers = "T:";
                }
            }
        }

        if (v == NULL) {
            if (((vlen == 1) ||
                 (((vlen == 2) && (str[1] == 'F' || str[1] == 'D')))) &&
                ((ctxt == VAR_CMD) || (ctxt == VAR_GLOBAL))) {
                /*
		 * If substituting a local variable in a non-local context,
		 * assume it's for dynamic source stuff. We have to handle
		 * this specially and return the longhand for the variable
		 * with the dollar sign escaped so it makes it back to the
		 * caller. Only four of the local variables are treated
		 * specially as they are the only four that will be set
		 * when dynamic sources are expanded.
		 */
                switch (*str) {
                    case '@':
                    case '%':
                    case '*':
                    case '!':
                        dynamic = TRUE;
                        break;
                }
            } else if ((vlen > 2) && (*str == '.') &&
                       isupper((unsigned char) str[1]) &&
                       ((ctxt == VAR_CMD) || (ctxt == VAR_GLOBAL))) {
                int len;

                len = vlen - 1;
                if ((strncmp(str, ".TARGET", len) == 0) ||
                    (strncmp(str, ".ARCHIVE", len) == 0) ||
                    (strncmp(str, ".PREFIX", len) == 0) ||
                    (strncmp(str, ".MEMBER", len) == 0)) {
                    dynamic = TRUE;
                }
            }

            if (!haveModifier) {
                /*
		 * No modifiers -- have specification length so we can return
		 * now.
		 */
                *lengthPtr = tstr - start + 1;
                if (dynamic) {
                    char *pstr = bmake_strndup(start, *lengthPtr);
                    *freePtr = pstr;
                    Buf_Destroy(&buf, TRUE);
                    return (pstr);
                } else {
                    Buf_Destroy(&buf, TRUE);
                    return (flags & VARF_UNDEFERR) ? var_Error : varNoError;
                }
            } else {
                /*
		 * Still need to get to the end of the variable specification,
		 * so kludge up a Var structure for the modifications
		 */
                v = bmake_malloc(sizeof(Var));
                v->name = UNCONST(str);
                Buf_Init(&v->val, 1);
                v->flags = VAR_JUNK;
                Buf_Destroy(&buf, FALSE);
            }
        } else
            Buf_Destroy(&buf, TRUE);
    }

    if (v->flags & VAR_IN_USE) {
        Fatal("Variable %s is recursive.", v->name);
        /*NOTREACHED*/
    } else {
        v->flags |= VAR_IN_USE;
    }
    /*
     * Before doing any modification, we have to make sure the value
     * has been fully expanded. If it looks like recursion might be
     * necessary (there's a dollar sign somewhere in the variable's value)
     * we just call Var_Subst to do any other substitutions that are
     * necessary. Note that the value returned by Var_Subst will have
     * been dynamically-allocated, so it will need freeing when we
     * return.
     */
    nstr = Buf_GetAll(&v->val, NULL);
    if (strchr(nstr, '$') != NULL) {
        nstr = Var_Subst(NULL, nstr, ctxt, flags);
        *freePtr = nstr;
    }

    v->flags &= ~VAR_IN_USE;

    if ((nstr != NULL) && (haveModifier || extramodifiers != NULL)) {
        void *extraFree;
        int used;

        extraFree = NULL;
        if (extramodifiers != NULL) {
            nstr = ApplyModifiers(nstr, extramodifiers, '(', ')',
                                  v, ctxt, flags, &used, &extraFree);
        }

        if (haveModifier) {
            /* Skip initial colon. */
            tstr++;

            nstr = ApplyModifiers(nstr, tstr, startc, endc,
                                  v, ctxt, flags, &used, freePtr);
            tstr += used;
            free(extraFree);
        } else {
            *freePtr = extraFree;
        }
    }
    if (*tstr) {
        *lengthPtr = tstr - start + 1;
    } else {
        *lengthPtr = tstr - start;
    }

    if (v->flags & VAR_FROM_ENV) {
        Boolean destroy = FALSE;

        if (nstr != Buf_GetAll(&v->val, NULL)) {
            destroy = TRUE;
        } else {
            /*
	     * Returning the value unmodified, so tell the caller to free
	     * the thing.
	     */
            *freePtr = nstr;
        }
        VarFreeEnv(v, destroy);
    } else if (v->flags & VAR_JUNK) {
        /*
	 * Perform any free'ing needed and set *freePtr to NULL so the caller
	 * doesn't try to free a static pointer.
	 * If VAR_KEEP is also set then we want to keep str as is.
	 */
        if (!(v->flags & VAR_KEEP)) {
            if (*freePtr) {
                free(nstr);
                *freePtr = NULL;
            }
            if (dynamic) {
                nstr = bmake_strndup(start, *lengthPtr);
                *freePtr = nstr;
            } else {
                nstr = (flags & VARF_UNDEFERR) ? var_Error : varNoError;
            }
        }
        if (nstr != Buf_GetAll(&v->val, NULL))
            Buf_Destroy(&v->val, TRUE);
        free(v->name);
        free(v);
    }
    return (nstr);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Subst  --
 *	Substitute for all variables in the given string in the given context
 *	If flags & VARF_UNDEFERR, Parse_Error will be called when an undefined
 *	variable is encountered.
 *
 * Input:
 *	var		Named variable || NULL for all
 *	str		the string which to substitute
 *	ctxt		the context wherein to find variables
 *	flags		VARF_UNDEFERR	if undefineds are an error
 *			VARF_WANTRES	if we actually want the result
 *			VARF_ASSIGN	if we are in a := assignment
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None. The old string must be freed by the caller
 *-----------------------------------------------------------------------
 */
char *
Var_Subst(const char *var, const char *str, GNode *ctxt, int flags) {
    Buffer buf;            /* Buffer for forming things */
    char *val;            /* Value to substitute for a variable */
    int length;        /* Length of the variable invocation */
    Boolean trailingBslash;   /* variable ends in \ */
    void *freeIt = NULL;    /* Set if it should be freed */
    static Boolean errorReported;   /* Set true if an error has already
				     * been reported to prevent a plethora
				     * of messages when recursing */

    Buf_Init(&buf, 0);
    errorReported = FALSE;
    trailingBslash = FALSE;

    while (*str) {
        if (*str == '\n' && trailingBslash)
            Buf_AddByte(&buf, ' ');
        if (var == NULL && (*str == '$') && (str[1] == '$')) {
            /*
	     * A dollar sign may be escaped either with another dollar sign.
	     * In such a case, we skip over the escape character and store the
	     * dollar sign into the buffer directly.
	     */
            if (save_dollars && (flags & VARF_ASSIGN))
                Buf_AddByte(&buf, *str);
            str++;
            Buf_AddByte(&buf, *str);
            str++;
        } else if (*str != '$') {
            /*
	     * Skip as many characters as possible -- either to the end of
	     * the string or to the next dollar sign (variable invocation).
	     */
            const char *cp;

            for (cp = str++; *str != '$' && *str != '\0'; str++)
                continue;
            Buf_AddBytes(&buf, str - cp, cp);
        } else {
            if (var != NULL) {
                int expand;
                for (;;) {
                    if (str[1] == '\0') {
                        /* A trailing $ is kind of a special case */
                        Buf_AddByte(&buf, str[0]);
                        str++;
                        expand = FALSE;
                    } else if (str[1] != PROPEN && str[1] != BROPEN) {
                        if (str[1] != *var || strlen(var) > 1) {
                            Buf_AddBytes(&buf, 2, str);
                            str += 2;
                            expand = FALSE;
                        } else
                            expand = TRUE;
                        break;
                    } else {
                        const char *p;

                        /*
			 * Scan up to the end of the variable name.
			 */
                        for (p = &str[2]; *p &&
                                          *p != ':' && *p != PRCLOSE && *p != BRCLOSE; p++)
                            if (*p == '$')
                                break;
                        /*
			 * A variable inside the variable. We cannot expand
			 * the external variable yet, so we try again with
			 * the nested one
			 */
                        if (*p == '$') {
                            Buf_AddBytes(&buf, p - str, str);
                            str = p;
                            continue;
                        }

                        if (strncmp(var, str + 2, p - str - 2) != 0 ||
                            var[p - str - 2] != '\0') {
                            /*
			     * Not the variable we want to expand, scan
			     * until the next variable
			     */
                            for (; *p != '$' && *p != '\0'; p++)
                                continue;
                            Buf_AddBytes(&buf, p - str, str);
                            str = p;
                            expand = FALSE;
                        } else
                            expand = TRUE;
                        break;
                    }
                }
                if (!expand)
                    continue;
            }

            val = Var_Parse(str, ctxt, flags, &length, &freeIt);

            /*
	     * When we come down here, val should either point to the
	     * value of this variable, suitably modified, or be NULL.
	     * Length should be the total length of the potential
	     * variable invocation (from $ to end character...)
	     */
            if (val == var_Error || val == varNoError) {
                /*
		 * If performing old-time variable substitution, skip over
		 * the variable and continue with the substitution. Otherwise,
		 * store the dollar sign and advance str so we continue with
		 * the string...
		 */
                if (oldVars) {
                    str += length;
                } else if ((flags & VARF_UNDEFERR) || val == var_Error) {
                    /*
		     * If variable is undefined, complain and skip the
		     * variable. The complaint will stop us from doing anything
		     * when the file is parsed.
		     */
                    if (!errorReported) {
                        Parse_Error(PARSE_FATAL,
                                    "Undefined variable \"%.*s\"", length, str);
                    }
                    str += length;
                    errorReported = TRUE;
                } else {
                    Buf_AddByte(&buf, *str);
                    str += 1;
                }
            } else {
                /*
		 * We've now got a variable structure to store in. But first,
		 * advance the string pointer.
		 */
                str += length;

                /*
		 * Copy all the characters from the variable value straight
		 * into the new string.
		 */
                length = strlen(val);
                Buf_AddBytes(&buf, length, val);
                trailingBslash = length > 0 && val[length - 1] == '\\';
            }
            free(freeIt);
            freeIt = NULL;
        }
    }

    return Buf_DestroyCompact(&buf);
}

/*-
 *-----------------------------------------------------------------------
 * Var_GetTail --
 *	Return the tail from each of a list of words. Used to set the
 *	System V local variables.
 *
 * Input:
 *	file		Filename to modify
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
#if 0
                                                                                                                        char *
Var_GetTail(char *file)
{
    return(VarModify(file, VarTail, NULL));
}

/*-
 *-----------------------------------------------------------------------
 * Var_GetHead --
 *	Find the leading components of a (list of) filename(s).
 *	XXX: VarHead does not replace foo by ., as (sun) System V make
 *	does.
 *
 * Input:
 *	file		Filename to manipulate
 *
 * Results:
 *	The leading components.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_GetHead(char *file)
{
    return(VarModify(file, VarHead, NULL));
}
#endif

/*-
 *-----------------------------------------------------------------------
 * Var_Init --
 *	Initialize the module
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The VAR_CMD and VAR_GLOBAL contexts are created
 *-----------------------------------------------------------------------
 */
void
Var_Init(void) {
    VAR_INTERNAL = Targ_NewGN("Internal");
    VAR_GLOBAL = Targ_NewGN("Global");
    VAR_CMD = Targ_NewGN("Command");

}


void
Var_End(void) {
}


/****************** PRINT DEBUGGING INFO *****************/
static void
VarPrintVar(void *vp) {
    Var *v = (Var *) vp;
    fprintf(debug_file, "%-16s = %s\n", v->name, Buf_GetAll(&v->val, NULL));
}

/*-
 *-----------------------------------------------------------------------
 * Var_Dump --
 *	print all variables in a context
 *-----------------------------------------------------------------------
 */
void
Var_Dump(GNode *ctxt) {
    Hash_Search search;
    Hash_Entry *h;

    for (h = Hash_EnumFirst(&ctxt->context, &search);
         h != NULL;
         h = Hash_EnumNext(&search)) {
        VarPrintVar(Hash_GetValue(h));
    }
}

#if (!defined(HAVE_REGCOMP) || !defined(HAVE_REGERROR) || !defined(HAVE_REGEXEC) || !defined(HAVE_REGFREE))

/* Return the codeset of the current locale, if this is easily deducible.
   Otherwise, return "".  */
static char *
ctype_codeset(void) {
    static char buf[2 + 10 + 1];
    char const *locale = setlocale(LC_CTYPE, NULL);
    char *codeset = buf;
    size_t codesetlen;
    codeset[0] = '\0';

    if (locale && locale[0]) {
        /* If the locale name contains an encoding after the dot, return it.  */
        char *dot = strchr(locale, '.');

        if (dot) {
            /* Look for the possible @... trailer and remove it, if any.  */
            char *codeset_start = dot + 1;
            char const *modifier = strchr(codeset_start, '@');

            if (!modifier)
                codeset = codeset_start;
            else {
                codesetlen = modifier - codeset_start;
                if (codesetlen < sizeof buf) {
                    codeset = memcpy(buf, codeset_start, codesetlen);
                    codeset[codesetlen] = '\0';
                }
            }
        }
    }

    /* If setlocale is successful, it returns the number of the
     codepage, as a string.  Otherwise, fall back on Windows API
     GetACP, which returns the locale's codepage as a number (although
     this doesn't change according to what the 'setlocale' call specified).
     Either way, prepend "CP" to make it a valid codeset name.  */
    codesetlen = strlen(codeset);
    if (0 < codesetlen && codesetlen < sizeof buf - 2)
        memmove(buf + 2, codeset, codesetlen + 1);
    else
        sprintf(buf + 2, "%u", GetACP());
    codeset = memcpy(buf, "CP", 2);
    return codeset;
}

/* Provide nl_langinfo from scratch, either for native MS-Windows, or
   for old Unix platforms without locales, such as Linux libc5 or
   BeOS.  */

# include <time.h>

char *
nl_langinfo(nl_item item) {
    static char nlbuf[100];
    struct tm tmm = {0};

    switch (item) {
        /* nl_langinfo items of the LC_CTYPE category */
        case CODESET: {
            char *codeset = ctype_codeset();
            if (*codeset)
                return codeset;
        }
            return (char *) "ISO-8859-1";
            /* nl_langinfo items of the LC_NUMERIC category */
        case RADIXCHAR:
            return localeconv()->decimal_point;
        case THOUSEP:
            return localeconv()->thousands_sep;
            /* nl_langinfo items of the LC_TIME category.
               TODO: Really use the locale.  */
        case D_T_FMT:
        case ERA_D_T_FMT:
            return (char *) "%a %b %e %H:%M:%S %Y";
        case D_FMT:
        case ERA_D_FMT:
            return (char *) "%m/%d/%y";
        case T_FMT:
        case ERA_T_FMT:
            return (char *) "%H:%M:%S";
        case T_FMT_AMPM:
            return (char *) "%I:%M:%S %p";
        case AM_STR:
            if (!strftime(nlbuf, sizeof nlbuf, "%p", &tmm))
                return (char *) "AM";
            return nlbuf;
        case PM_STR:
            tmm.tm_hour = 12;
            if (!strftime(nlbuf, sizeof nlbuf, "%p", &tmm))
                return (char *) "PM";
            return nlbuf;
        case DAY_1:
        case DAY_2:
        case DAY_3:
        case DAY_4:
        case DAY_5:
        case DAY_6:
        case DAY_7: {
            static char const days[][sizeof "Wednesday"] = {
                    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
                    "Friday", "Saturday"
            };
            tmm.tm_wday = item - DAY_1;
            if (!strftime(nlbuf, sizeof nlbuf, "%A", &tmm))
                return (char *) days[item - DAY_1];
            return nlbuf;
        }
        case ABDAY_1:
        case ABDAY_2:
        case ABDAY_3:
        case ABDAY_4:
        case ABDAY_5:
        case ABDAY_6:
        case ABDAY_7: {
            static char const abdays[][sizeof "Sun"] = {
                    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
            };
            tmm.tm_wday = item - ABDAY_1;
            if (!strftime(nlbuf, sizeof nlbuf, "%a", &tmm))
                return (char *) abdays[item - ABDAY_1];
            return nlbuf;
        }
            {
                static char const months[][sizeof "September"] = {
                        "January", "February", "March", "April", "May", "June", "July",
                        "September", "October", "November", "December"
                };
                case MON_1:
                case MON_2:
                case MON_3:
                case MON_4:
                case MON_5:
                case MON_6:
                case MON_7:
                case MON_8:
                case MON_9:
                case MON_10:
                case MON_11:
                case MON_12:
                    tmm.tm_mon = item - MON_1;
                if (!strftime(nlbuf, sizeof nlbuf, "%B", &tmm))
                    return (char *) months[item - MON_1];
                return nlbuf;
                case ALTMON_1:
                case ALTMON_2:
                case ALTMON_3:
                case ALTMON_4:
                case ALTMON_5:
                case ALTMON_6:
                case ALTMON_7:
                case ALTMON_8:
                case ALTMON_9:
                case ALTMON_10:
                case ALTMON_11:
                case ALTMON_12:
                    tmm.tm_mon = item - ALTMON_1;
                /* The platforms without nl_langinfo() don't support strftime with %OB.
                   We don't even need to try.  */
                if (!strftime(nlbuf, sizeof nlbuf, "%B", &tmm))
                    return (char *) months[item - ALTMON_1];
                return nlbuf;
            }
        case ABMON_1:
        case ABMON_2:
        case ABMON_3:
        case ABMON_4:
        case ABMON_5:
        case ABMON_6:
        case ABMON_7:
        case ABMON_8:
        case ABMON_9:
        case ABMON_10:
        case ABMON_11:
        case ABMON_12: {
            static char const abmonths[][sizeof "Jan"] = {
                    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                    "Sep", "Oct", "Nov", "Dec"
            };
            tmm.tm_mon = item - ABMON_1;
            if (!strftime(nlbuf, sizeof nlbuf, "%b", &tmm))
                return (char *) abmonths[item - ABMON_1];
            return nlbuf;
        }
        case ERA:
            return (char *) "";
        case ALT_DIGITS:
            return (char *) "\0\0\0\0\0\0\0\0\0\0";
            /* nl_langinfo items of the LC_MONETARY category.  */
        case CRNCYSTR:
            return localeconv()->currency_symbol;
# ifdef INT_CURR_SYMBOL
                                                                                                                                case INT_CURR_SYMBOL:
      return localeconv () ->int_curr_symbol;
    case MON_DECIMAL_POINT:
      return localeconv () ->mon_decimal_point;
    case MON_THOUSANDS_SEP:
      return localeconv () ->mon_thousands_sep;
    case MON_GROUPING:
      return localeconv () ->mon_grouping;
    case POSITIVE_SIGN:
      return localeconv () ->positive_sign;
    case NEGATIVE_SIGN:
      return localeconv () ->negative_sign;
    case FRAC_DIGITS:
      return & localeconv () ->frac_digits;
    case INT_FRAC_DIGITS:
      return & localeconv () ->int_frac_digits;
    case P_CS_PRECEDES:
      return & localeconv () ->p_cs_precedes;
    case N_CS_PRECEDES:
      return & localeconv () ->n_cs_precedes;
    case P_SEP_BY_SPACE:
      return & localeconv () ->p_sep_by_space;
    case N_SEP_BY_SPACE:
      return & localeconv () ->n_sep_by_space;
    case P_SIGN_POSN:
      return & localeconv () ->p_sign_posn;
    case N_SIGN_POSN:
      return & localeconv () ->n_sign_posn;
# endif
            /* nl_langinfo items of the LC_MESSAGES category
               TODO: Really use the locale. */
        case YESEXPR:
            return (char *) "^[yY]";
        case NOEXPR:
            return (char *) "^[nN]";
        default:
            return (char *) "";
    }
}

/* Extended regular expression matching and search library.
   Copyright (C) 2002-2018 Free Software Foundation, Inc.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

static void re_string_construct_common(const char *str, Idx len,
                                       re_string_t *pstr,
                                       RE_TRANSLATE_TYPE trans, bool icase,
                                       const re_dfa_t *dfa);

static re_dfastate_t *create_ci_newstate(const re_dfa_t *dfa,
                                         const re_node_set *nodes,
                                         re_hashval_t hash);

static re_dfastate_t *create_cd_newstate(const re_dfa_t *dfa,
                                         const re_node_set *nodes,
                                         unsigned int context,
                                         re_hashval_t hash);

static reg_errcode_t re_string_realloc_buffers(re_string_t *pstr,
                                               Idx new_buf_len);

static void build_upper_buffer(re_string_t *pstr);

static void re_string_translate_buffer(re_string_t *pstr);

static unsigned int re_string_context_at(const re_string_t *input, Idx idx,
                                         int eflags) __attribute__ ((pure));

static reg_errcode_t match_ctx_init(re_match_context_t *cache, int eflags,
                                    Idx n);

static void match_ctx_clean(re_match_context_t *mctx);

static void match_ctx_free(re_match_context_t *cache);

static reg_errcode_t match_ctx_add_entry(re_match_context_t *cache, Idx node,
                                         Idx str_idx, Idx from, Idx to);

static Idx search_cur_bkref_entry(const re_match_context_t *mctx, Idx str_idx);

static reg_errcode_t match_ctx_add_subtop(re_match_context_t *mctx, Idx node,
                                          Idx str_idx);

static re_sub_match_last_t *match_ctx_add_sublast(re_sub_match_top_t *subtop,
                                                  Idx node, Idx str_idx);

static void sift_ctx_init(re_sift_context_t *sctx, re_dfastate_t **sifted_sts,
                          re_dfastate_t **limited_sts, Idx last_node,
                          Idx last_str_idx);

static reg_errcode_t re_search_internal(const regex_t *preg,
                                        const char *string, Idx length,
                                        Idx start, Idx last_start, Idx stop,
                                        size_t nmatch, regmatch_t pmatch[],
                                        int eflags);

static regoff_t re_search_2_stub(struct re_pattern_buffer *bufp,
                                 const char *string1, Idx length1,
                                 const char *string2, Idx length2,
                                 Idx start, regoff_t range,
                                 struct re_registers *regs,
                                 Idx stop, bool ret_len);

static regoff_t re_search_stub(struct re_pattern_buffer *bufp,
                               const char *string, Idx length, Idx start,
                               regoff_t range, Idx stop,
                               struct re_registers *regs,
                               bool ret_len);

static unsigned re_copy_regs(struct re_registers *regs, regmatch_t *pmatch,
                             Idx nregs, int regs_allocated);

static reg_errcode_t prune_impossible_nodes(re_match_context_t *mctx);

static Idx check_matching(re_match_context_t *mctx, bool fl_longest_match,
                          Idx *p_match_first);

static Idx check_halt_state_context(const re_match_context_t *mctx,
                                    const re_dfastate_t *state, Idx idx);

static void update_regs(const re_dfa_t *dfa, regmatch_t *pmatch,
                        regmatch_t *prev_idx_match, Idx cur_node,
                        Idx cur_idx, Idx nmatch);

static reg_errcode_t push_fail_stack(struct re_fail_stack_t *fs,
                                     Idx str_idx, Idx dest_node, Idx nregs,
                                     regmatch_t *regs,
                                     re_node_set *eps_via_nodes);

static reg_errcode_t set_regs(const regex_t *preg,
                              const re_match_context_t *mctx,
                              size_t nmatch, regmatch_t *pmatch,
                              bool fl_backtrack);

static reg_errcode_t free_fail_stack_return(struct re_fail_stack_t *fs);

static reg_errcode_t sift_states_backward(const re_match_context_t *mctx,
                                          re_sift_context_t *sctx);

static reg_errcode_t build_sifted_states(const re_match_context_t *mctx,
                                         re_sift_context_t *sctx, Idx str_idx,
                                         re_node_set *cur_dest);

static reg_errcode_t update_cur_sifted_state(const re_match_context_t *mctx,
                                             re_sift_context_t *sctx,
                                             Idx str_idx,
                                             re_node_set *dest_nodes);

static reg_errcode_t add_epsilon_src_nodes(const re_dfa_t *dfa,
                                           re_node_set *dest_nodes,
                                           const re_node_set *candidates);

static bool check_dst_limits(const re_match_context_t *mctx,
                             const re_node_set *limits,
                             Idx dst_node, Idx dst_idx, Idx src_node,
                             Idx src_idx);

static int check_dst_limits_calc_pos_1(const re_match_context_t *mctx,
                                       int boundaries, Idx subexp_idx,
                                       Idx from_node, Idx bkref_idx);

static int check_dst_limits_calc_pos(const re_match_context_t *mctx,
                                     Idx limit, Idx subexp_idx,
                                     Idx node, Idx str_idx,
                                     Idx bkref_idx);

static reg_errcode_t check_subexp_limits(const re_dfa_t *dfa,
                                         re_node_set *dest_nodes,
                                         const re_node_set *candidates,
                                         re_node_set *limits,
                                         struct re_backref_cache_entry *bkref_ents,
                                         Idx str_idx);

static reg_errcode_t sift_states_bkref(const re_match_context_t *mctx,
                                       re_sift_context_t *sctx,
                                       Idx str_idx, const re_node_set *candidates);

static reg_errcode_t merge_state_array(const re_dfa_t *dfa,
                                       re_dfastate_t **dst,
                                       re_dfastate_t **src, Idx num);

static re_dfastate_t *find_recover_state(reg_errcode_t *err,
                                         re_match_context_t *mctx);

static re_dfastate_t *transit_state(reg_errcode_t *err,
                                    re_match_context_t *mctx,
                                    re_dfastate_t *state);

static re_dfastate_t *merge_state_with_log(reg_errcode_t *err,
                                           re_match_context_t *mctx,
                                           re_dfastate_t *next_state);

static reg_errcode_t check_subexp_matching_top(re_match_context_t *mctx,
                                               re_node_set *cur_nodes,
                                               Idx str_idx);

static reg_errcode_t transit_state_bkref(re_match_context_t *mctx,
                                         const re_node_set *nodes);

static reg_errcode_t get_subexp(re_match_context_t *mctx,
                                Idx bkref_node, Idx bkref_str_idx);

static reg_errcode_t get_subexp_sub(re_match_context_t *mctx,
                                    const re_sub_match_top_t *sub_top,
                                    re_sub_match_last_t *sub_last,
                                    Idx bkref_node, Idx bkref_str);

static Idx find_subexp_node(const re_dfa_t *dfa, const re_node_set *nodes,
                            Idx subexp_idx, int type);

static reg_errcode_t check_arrival(re_match_context_t *mctx,
                                   state_array_t *path, Idx top_node,
                                   Idx top_str, Idx last_node, Idx last_str,
                                   int type);

static reg_errcode_t check_arrival_add_next_nodes(re_match_context_t *mctx,
                                                  Idx str_idx,
                                                  re_node_set *cur_nodes,
                                                  re_node_set *next_nodes);

static reg_errcode_t check_arrival_expand_ecl(const re_dfa_t *dfa,
                                              re_node_set *cur_nodes,
                                              Idx ex_subexp, int type);

static reg_errcode_t check_arrival_expand_ecl_sub(const re_dfa_t *dfa,
                                                  re_node_set *dst_nodes,
                                                  Idx target, Idx ex_subexp,
                                                  int type);

static reg_errcode_t expand_bkref_cache(re_match_context_t *mctx,
                                        re_node_set *cur_nodes, Idx cur_str,
                                        Idx subexp_num, int type);

static bool build_trtable(const re_dfa_t *dfa, re_dfastate_t *state);

static Idx group_nodes_into_DFAstates(const re_dfa_t *dfa,
                                      const re_dfastate_t *state,
                                      re_node_set *states_node,
                                      bitset_t *states_ch);

static bool check_node_accept(const re_match_context_t *mctx,
                              const re_token_t *node, Idx idx);

static reg_errcode_t extend_buffers(re_match_context_t *mctx, int min_len);

/* Check NODE match the current context.  */

static bool
check_halt_node_context(const re_dfa_t *dfa, Idx node, unsigned int context) {
    re_token_type_t type = dfa->nodes[node].type;
    unsigned int constraint = dfa->nodes[node].constraint;
    if (type != END_OF_RE)
        return false;
    if (!constraint)
        return true;
    if (NOT_SATISFY_NEXT_CONSTRAINT(constraint, context))
        return false;
    return true;
}

/* Check the halt state STATE match the current context.
   Return 0 if not match, if the node, STATE has, is a halt node and
   match the context, return the node.  */

static Idx
check_halt_state_context(const re_match_context_t *mctx,
                         const re_dfastate_t *state, Idx idx) {
    Idx i;
    unsigned int context;
    context = re_string_context_at(&mctx->input, idx, mctx->eflags);
    for (i = 0; i < state->nodes.nelem; ++i)
        if (check_halt_node_context(mctx->dfa, state->nodes.elems[i], context))
            return state->nodes.elems[i];
    return 0;
}

/* Functions for string operation.  */

/* This function allocate the buffers.  It is necessary to call
   re_string_reconstruct before using the object.  */

static reg_errcode_t

re_string_allocate(re_string_t *pstr, const char *str, Idx len, Idx init_len,
                   RE_TRANSLATE_TYPE trans, bool icase, const re_dfa_t *dfa) {
    reg_errcode_t ret;
    Idx init_buf_len;

/* Ensure at least one character fits into the buffers.  */
    if (init_len < dfa->mb_cur_max)
        init_len = dfa->mb_cur_max;
    init_buf_len = (len + 1 < init_len) ? len + 1 : init_len;
    re_string_construct_common(str, len, pstr, trans, icase, dfa);

    ret = re_string_realloc_buffers(pstr, init_buf_len);
    if ((ret != REG_NOERROR))
        return ret;

    pstr->word_char = dfa->word_char;
    pstr->word_ops_used = dfa->word_ops_used;
    pstr->mbs = pstr->mbs_allocated ? pstr->mbs : (unsigned char *) str;
    pstr->valid_len = (pstr->mbs_allocated || dfa->mb_cur_max > 1) ? 0 : len;
    pstr->valid_raw_len = pstr->valid_len;
    return REG_NOERROR;
}

/* This function allocate the buffers, and initialize them.  */

static reg_errcode_t

re_string_construct(re_string_t *pstr, const char *str, Idx len,
                    RE_TRANSLATE_TYPE trans, bool icase, const re_dfa_t *dfa) {
    reg_errcode_t ret;
    memset(pstr, '\0', sizeof(re_string_t));
    re_string_construct_common(str, len, pstr, trans, icase, dfa);

    if (len > 0) {
        ret = re_string_realloc_buffers(pstr, len + 1);
        if ((ret != REG_NOERROR))
            return ret;
    }
    pstr->mbs = pstr->mbs_allocated ? pstr->mbs : (unsigned char *) str;

    if (icase) {

        build_upper_buffer(pstr);
    } else {

        {
            if (trans != NULL)
                re_string_translate_buffer(pstr);
            else {
                pstr->valid_len = pstr->bufs_len;
                pstr->valid_raw_len = pstr->bufs_len;
            }
        }
    }

    return REG_NOERROR;
}

/* Helper functions for re_string_allocate, and re_string_construct.  */

static reg_errcode_t

re_string_realloc_buffers(re_string_t *pstr, Idx new_buf_len) {

    if (pstr->mbs_allocated) {
        unsigned char *new_mbs = re_realloc(pstr->mbs,
        unsigned char,
        new_buf_len);
        if ((new_mbs == NULL))
            return REG_ESPACE;
        pstr->mbs = new_mbs;
    }
    pstr->bufs_len = new_buf_len;
    return REG_NOERROR;
}


static void
re_string_construct_common(const char *str, Idx len, re_string_t *pstr,
                           RE_TRANSLATE_TYPE trans, bool icase,
                           const re_dfa_t *dfa) {
    pstr->raw_mbs = (const unsigned char *) str;
    pstr->len = len;
    pstr->raw_len = len;
    pstr->trans = trans;
    pstr->icase = icase;
    pstr->mbs_allocated = (trans != NULL || icase);
    pstr->mb_cur_max = dfa->mb_cur_max;
    pstr->is_utf8 = dfa->is_utf8;
    pstr->map_notascii = dfa->map_notascii;
    pstr->stop = pstr->len;
    pstr->raw_stop = pstr->stop;
}

/* Build the buffer PSTR->MBS, and apply the translation if we need.
   This function is used in case of REG_ICASE.  */

static void
build_upper_buffer(re_string_t *pstr) {
    Idx char_idx, end_idx;
    end_idx = (pstr->bufs_len > pstr->len) ? pstr->len : pstr->bufs_len;

    for (char_idx = pstr->valid_len; char_idx < end_idx; ++char_idx) {
        int ch = pstr->raw_mbs[pstr->raw_mbs_idx + char_idx];
        if ((pstr->trans != NULL))
            ch = pstr->trans[ch];
        pstr->mbs[char_idx] = toupper(ch);
    }
    pstr->valid_len = char_idx;
    pstr->valid_raw_len = char_idx;
}

/* Apply TRANS to the buffer in PSTR.  */

static void
re_string_translate_buffer(re_string_t *pstr) {
    Idx buf_idx, end_idx;
    end_idx = (pstr->bufs_len > pstr->len) ? pstr->len : pstr->bufs_len;

    for (buf_idx = pstr->valid_len; buf_idx < end_idx; ++buf_idx) {
        int ch = pstr->raw_mbs[pstr->raw_mbs_idx + buf_idx];
        pstr->mbs[buf_idx] = pstr->trans[ch];
    }

    pstr->valid_len = buf_idx;
    pstr->valid_raw_len = buf_idx;
}

/* This function re-construct the buffers.
   Concretely, convert to wide character in case of pstr->mb_cur_max > 1,
   convert to upper case in case of REG_ICASE, apply translation.  */

static reg_errcode_t

re_string_reconstruct(re_string_t *pstr, Idx idx, int eflags) {
    Idx offset;

    if ((pstr->raw_mbs_idx <= idx))
        offset = idx - pstr->raw_mbs_idx;
    else {
/* Reset buffer.  */

        pstr->len = pstr->raw_len;
        pstr->stop = pstr->raw_stop;
        pstr->valid_len = 0;
        pstr->raw_mbs_idx = 0;
        pstr->valid_raw_len = 0;
        pstr->offsets_needed = 0;
        pstr->tip_context = ((eflags & REG_NOTBOL) ? CONTEXT_BEGBUF
                                                   : CONTEXT_NEWLINE | CONTEXT_BEGBUF);
        if (!pstr->mbs_allocated)
            pstr->mbs = (unsigned char *) pstr->raw_mbs;
        offset = idx;
    }

    if ((offset != 0)) {
/* Should the already checked characters be kept?  */
        if ((offset < pstr->valid_raw_len)) {
/* Yes, move them to the front of the buffer.  */

            {
                pstr->tip_context = re_string_context_at(pstr, offset - 1,
                                                         eflags);

                if ((pstr->mbs_allocated))
                    memmove(pstr->mbs, pstr->mbs + offset,
                            pstr->valid_len - offset);
                pstr->valid_len -= offset;
                pstr->valid_raw_len -= offset;

            }
        } else {

            pstr->valid_len = 0;

            {
                int c = pstr->raw_mbs[pstr->raw_mbs_idx + offset - 1];
                pstr->valid_raw_len = 0;
                if (pstr->trans)
                    c = pstr->trans[c];
                pstr->tip_context = (bitset_contain(pstr->word_char, c)
                                     ? CONTEXT_WORD
                                     : ((IS_NEWLINE(c) && pstr->newline_anchor)
                                        ? CONTEXT_NEWLINE : 0));
            }
        }
        if (!(pstr->mbs_allocated))
            pstr->mbs += offset;
    }
    pstr->raw_mbs_idx = idx;
    pstr->len -= offset;
    pstr->stop -= offset;

/* Then build the buffers.  */

    if ((pstr->mbs_allocated)) {
        if (pstr->icase)
            build_upper_buffer(pstr);
        else if (pstr->trans != NULL)
            re_string_translate_buffer(pstr);
    } else
        pstr->valid_len = pstr->len;

    pstr->cur_idx = 0;
    return REG_NOERROR;
}

static unsigned char
__attribute__ ((pure))
re_string_peek_byte_case(const re_string_t *pstr, Idx idx) {
    int ch;
    Idx off;

    /* Handle the common (easiest) cases first.  */
    if ((!pstr->mbs_allocated))
        return re_string_peek_byte(pstr, idx);

    off = pstr->cur_idx + idx;

    ch = pstr->raw_mbs[pstr->raw_mbs_idx + off];

    return ch;
}

static unsigned char
re_string_fetch_byte_case(re_string_t *pstr) {
    if ((!pstr->mbs_allocated))
        return re_string_fetch_byte(pstr);

    return pstr->raw_mbs[pstr->raw_mbs_idx + pstr->cur_idx++];
}

static void
re_string_destruct(re_string_t *pstr) {

    if (pstr->mbs_allocated)
        re_free(pstr->mbs);
}

/* Return the context at IDX in INPUT.  */

static unsigned int
re_string_context_at(const re_string_t *input, Idx idx, int eflags) {
    int c;
    if ((idx < 0))
        /* In this case, we use the value stored in input->tip_context,
           since we can't know the character in input->mbs[-1] here.  */
        return input->tip_context;
    if ((idx == input->len))
        return ((eflags & REG_NOTEOL) ? CONTEXT_ENDBUF
                                      : CONTEXT_NEWLINE | CONTEXT_ENDBUF);

    {
        c = re_string_byte_at(input, idx);
        if (bitset_contain(input->word_char, c))
            return CONTEXT_WORD;
        return IS_NEWLINE(c) && input->newline_anchor ? CONTEXT_NEWLINE : 0;
    }
}

/* Functions for set operation.  */

static reg_errcode_t

re_node_set_alloc(re_node_set *set, Idx size) {
    set->alloc = size;
    set->nelem = 0;
    set->elems = re_malloc(Idx, size);
    if ((set->elems == NULL)
        && (MALLOC_0_IS_NONNULL || size != 0))
        return REG_ESPACE;
    return REG_NOERROR;
}

static reg_errcode_t

re_node_set_init_1(re_node_set *set, Idx elem) {
    set->alloc = 1;
    set->nelem = 1;
    set->elems = re_malloc(Idx, 1);
    if ((set->elems == NULL)) {
        set->alloc = set->nelem = 0;
        return REG_ESPACE;
    }
    set->elems[0] = elem;
    return REG_NOERROR;
}

static reg_errcode_t

re_node_set_init_2(re_node_set *set, Idx elem1, Idx elem2) {
    set->alloc = 2;
    set->elems = re_malloc(Idx, 2);
    if ((set->elems == NULL))
        return REG_ESPACE;
    if (elem1 == elem2) {
        set->nelem = 1;
        set->elems[0] = elem1;
    } else {
        set->nelem = 2;
        if (elem1 < elem2) {
            set->elems[0] = elem1;
            set->elems[1] = elem2;
        } else {
            set->elems[0] = elem2;
            set->elems[1] = elem1;
        }
    }
    return REG_NOERROR;
}

static reg_errcode_t

re_node_set_init_copy(re_node_set *dest, const re_node_set *src) {
    dest->nelem = src->nelem;
    if (src->nelem > 0) {
        dest->alloc = dest->nelem;
        dest->elems = re_malloc(Idx, dest->alloc);
        if ((dest->elems == NULL)) {
            dest->alloc = dest->nelem = 0;
            return REG_ESPACE;
        }
        memcpy(dest->elems, src->elems, src->nelem * sizeof(Idx));
    } else
        re_node_set_init_empty(dest);
    return REG_NOERROR;
}

/* Calculate the intersection of the sets SRC1 and SRC2. And merge it to
   DEST. Return value indicate the error code or REG_NOERROR if succeeded.
   Note: We assume dest->elems is NULL, when dest->alloc is 0.  */

static reg_errcode_t

re_node_set_add_intersect(re_node_set *dest, const re_node_set *src1,
                          const re_node_set *src2) {
    Idx i1, i2, is, id, delta, sbase;
    if (src1->nelem == 0 || src2->nelem == 0)
        return REG_NOERROR;

/* We need dest->nelem + 2 * elems_in_intersection; this is a
   conservative estimate.  */
    if (src1->nelem + src2->nelem + dest->nelem > dest->alloc) {
        Idx new_alloc = src1->nelem + src2->nelem + dest->alloc;
        Idx *new_elems = re_realloc(dest->elems, Idx, new_alloc);
        if ((new_elems == NULL))
            return REG_ESPACE;
        dest->elems = new_elems;
        dest->alloc = new_alloc;
    }

/* Find the items in the intersection of SRC1 and SRC2, and copy
   into the top of DEST those that are not already in DEST itself.  */
    sbase = dest->nelem + src1->nelem + src2->nelem;
    i1 = src1->nelem - 1;
    i2 = src2->nelem - 1;
    id = dest->nelem - 1;
    for (;;) {
        if (src1->elems[i1] == src2->elems[i2]) {
/* Try to find the item in DEST.  Maybe we could binary search?  */
            while (id >= 0 && dest->elems[id] > src1->elems[i1])
                --id;

            if (id < 0 || dest->elems[id] != src1->elems[i1])
                dest->elems[--sbase] = src1->elems[i1];

            if (--i1 < 0 || --i2 < 0)
                break;
        }

/* Lower the highest of the two items.  */
        else if (src1->elems[i1] < src2->elems[i2]) {
            if (--i2 < 0)
                break;
        } else {
            if (--i1 < 0)
                break;
        }
    }

    id = dest->nelem - 1;
    is = dest->nelem + src1->nelem + src2->nelem - 1;
    delta = is - sbase + 1;

/* Now copy.  When DELTA becomes zero, the remaining
   DEST elements are already in place; this is more or
   less the same loop that is in re_node_set_merge.  */
    dest->nelem += delta;
    if (delta > 0 && id >= 0)
        for (;;) {
            if (dest->elems[is] > dest->elems[id]) {
/* Copy from the top.  */
                dest->elems[id + delta--] = dest->elems[is--];
                if (delta == 0)
                    break;
            } else {
/* Slide from the bottom.  */
                dest->elems[id + delta] = dest->elems[id];
                if (--id < 0)
                    break;
            }
        }

/* Copy remaining SRC elements.  */
    memcpy(dest->elems, dest->elems + sbase, delta * sizeof(Idx));

    return REG_NOERROR;
}

/* Calculate the union set of the sets SRC1 and SRC2. And store it to
   DEST. Return value indicate the error code or REG_NOERROR if succeeded.  */

static reg_errcode_t

re_node_set_init_union(re_node_set *dest, const re_node_set *src1,
                       const re_node_set *src2) {
    Idx i1, i2, id;
    if (src1 != NULL && src1->nelem > 0 && src2 != NULL && src2->nelem > 0) {
        dest->alloc = src1->nelem + src2->nelem;
        dest->elems = re_malloc(Idx, dest->alloc);
        if ((dest->elems == NULL))
            return REG_ESPACE;
    } else {
        if (src1 != NULL && src1->nelem > 0)
            return re_node_set_init_copy(dest, src1);
        else if (src2 != NULL && src2->nelem > 0)
            return re_node_set_init_copy(dest, src2);
        else
            re_node_set_init_empty(dest);
        return REG_NOERROR;
    }
    for (i1 = i2 = id = 0; i1 < src1->nelem && i2 < src2->nelem;) {
        if (src1->elems[i1] > src2->elems[i2]) {
            dest->elems[id++] = src2->elems[i2++];
            continue;
        }
        if (src1->elems[i1] == src2->elems[i2])
            ++i2;
        dest->elems[id++] = src1->elems[i1++];
    }
    if (i1 < src1->nelem) {
        memcpy(dest->elems + id, src1->elems + i1,
               (src1->nelem - i1) * sizeof(Idx));
        id += src1->nelem - i1;
    } else if (i2 < src2->nelem) {
        memcpy(dest->elems + id, src2->elems + i2,
               (src2->nelem - i2) * sizeof(Idx));
        id += src2->nelem - i2;
    }
    dest->nelem = id;
    return REG_NOERROR;
}

/* Calculate the union set of the sets DEST and SRC. And store it to
   DEST. Return value indicate the error code or REG_NOERROR if succeeded.  */

static reg_errcode_t

re_node_set_merge(re_node_set *dest, const re_node_set *src) {
    Idx is, id, sbase, delta;
    if (src == NULL || src->nelem == 0)
        return REG_NOERROR;
    if (dest->alloc < 2 * src->nelem + dest->nelem) {
        Idx new_alloc = 2 * (src->nelem + dest->alloc);
        Idx *new_buffer = re_realloc(dest->elems, Idx, new_alloc);
        if ((new_buffer == NULL))
            return REG_ESPACE;
        dest->elems = new_buffer;
        dest->alloc = new_alloc;
    }

    if ((dest->nelem == 0)) {
        dest->nelem = src->nelem;
        memcpy(dest->elems, src->elems, src->nelem * sizeof(Idx));
        return REG_NOERROR;
    }

/* Copy into the top of DEST the items of SRC that are not
   found in DEST.  Maybe we could binary search in DEST?  */
    for (sbase = dest->nelem + 2 * src->nelem,
         is = src->nelem - 1, id = dest->nelem - 1; is >= 0 && id >= 0;) {
        if (dest->elems[id] == src->elems[is])
            is--, id--;
        else if (dest->elems[id] < src->elems[is])
            dest->elems[--sbase] = src->elems[is--];
        else /* if (dest->elems[id] > src->elems[is]) */
            --id;
    }

    if (is >= 0) {
/* If DEST is exhausted, the remaining items of SRC must be unique.  */
        sbase -= is + 1;
        memcpy(dest->elems + sbase, src->elems, (is + 1) * sizeof(Idx));
    }

    id = dest->nelem - 1;
    is = dest->nelem + 2 * src->nelem - 1;
    delta = is - sbase + 1;
    if (delta == 0)
        return REG_NOERROR;

/* Now copy.  When DELTA becomes zero, the remaining
   DEST elements are already in place.  */
    dest->nelem += delta;
    for (;;) {
        if (dest->elems[is] > dest->elems[id]) {
/* Copy from the top.  */
            dest->elems[id + delta--] = dest->elems[is--];
            if (delta == 0)
                break;
        } else {
/* Slide from the bottom.  */
            dest->elems[id + delta] = dest->elems[id];
            if (--id < 0) {
/* Copy remaining SRC elements.  */
                memcpy(dest->elems, dest->elems + sbase,
                       delta * sizeof(Idx));
                break;
            }
        }
    }

    return REG_NOERROR;
}

/* Insert the new element ELEM to the re_node_set* SET.
   SET should not already have ELEM.
   Return true if successful.  */

static bool

re_node_set_insert(re_node_set *set, Idx elem) {
    Idx idx;
/* In case the set is empty.  */
    if (set->alloc == 0)
        return (re_node_set_init_1(set, elem) == REG_NOERROR);

    if ((set->nelem) == 0) {
/* We already guaranteed above that set->alloc != 0.  */
        set->elems[0] = elem;
        ++set->nelem;
        return true;
    }

/* Realloc if we need.  */
    if (set->alloc == set->nelem) {
        Idx *new_elems;
        set->alloc = set->alloc * 2;
        new_elems = re_realloc(set->elems, Idx, set->alloc);
        if ((new_elems == NULL))
            return false;
        set->elems = new_elems;
    }

/* Move the elements which follows the new element.  Test the
   first element separately to skip a check in the inner loop.  */
    if (elem < set->elems[0]) {
        idx = 0;
        for (idx = set->nelem; idx > 0; idx--)
            set->elems[idx] = set->elems[idx - 1];
    } else {
        for (idx = set->nelem; set->elems[idx - 1] > elem; idx--)
            set->elems[idx] = set->elems[idx - 1];
    }

/* Insert the new element.  */
    set->elems[idx] = elem;
    ++set->nelem;
    return true;
}

/* Insert the new element ELEM to the re_node_set* SET.
   SET should not already have any element greater than or equal to ELEM.
   Return true if successful.  */

static bool

re_node_set_insert_last(re_node_set *set, Idx elem) {
/* Realloc if we need.  */
    if (set->alloc == set->nelem) {
        Idx *new_elems;
        set->alloc = (set->alloc + 1) * 2;
        new_elems = re_realloc(set->elems, Idx, set->alloc);
        if ((new_elems == NULL))
            return false;
        set->elems = new_elems;
    }

/* Insert the new element.  */
    set->elems[set->nelem++] = elem;
    return true;
}

/* Compare two node sets SET1 and SET2.
   Return true if SET1 and SET2 are equivalent.  */

static bool
__attribute__ ((pure))
re_node_set_compare(const re_node_set *set1, const re_node_set *set2) {
    Idx i;
    if (set1 == NULL || set2 == NULL || set1->nelem != set2->nelem)
        return false;
    for (i = set1->nelem; --i >= 0;)
        if (set1->elems[i] != set2->elems[i])
            return false;
    return true;
}

/* Return (idx + 1) if SET contains the element ELEM, return 0 otherwise.  */

static Idx
__attribute__ ((pure))
re_node_set_contains(const re_node_set *set, Idx elem) {
    __re_size_t idx, right, mid;
    if (set->nelem <= 0)
        return 0;

    /* Binary search the element.  */
    idx = 0;
    right = set->nelem - 1;
    while (idx < right) {
        mid = (idx + right) / 2;
        if (set->elems[mid] < elem)
            idx = mid + 1;
        else
            right = mid;
    }
    return set->elems[idx] == elem ? idx + 1 : 0;
}

static void
re_node_set_remove_at(re_node_set *set, Idx idx) {
    if (idx < 0 || idx >= set->nelem)
        return;
    --set->nelem;
    for (; idx < set->nelem; idx++)
        set->elems[idx] = set->elems[idx + 1];
}


/* Add the token TOKEN to dfa->nodes, and return the index of the token.
   Or return -1 if an error occurred.  */

static Idx
re_dfa_add_node(re_dfa_t *dfa, re_token_t token) {
    if ((dfa->nodes_len >= dfa->nodes_alloc)) {
        size_t new_nodes_alloc = dfa->nodes_alloc * 2;
        Idx *new_nexts, *new_indices;
        re_node_set *new_edests, *new_eclosures;
        re_token_t *new_nodes;

        /* Avoid overflows in realloc.  */
        const size_t max_object_size = MAX (sizeof(re_token_t),
                                            MAX(sizeof(re_node_set),
                                                sizeof(Idx)));
        if ((MIN (IDX_MAX, SIZE_MAX / max_object_size)
             < new_nodes_alloc))
            return -1;

        new_nodes = re_realloc(dfa->nodes, re_token_t, new_nodes_alloc);
        if ((new_nodes == NULL))
            return -1;
        dfa->nodes = new_nodes;
        new_nexts = re_realloc(dfa->nexts, Idx, new_nodes_alloc);
        new_indices = re_realloc(dfa->org_indices, Idx, new_nodes_alloc);
        new_edests = re_realloc(dfa->edests, re_node_set, new_nodes_alloc);
        new_eclosures = re_realloc(dfa->eclosures, re_node_set, new_nodes_alloc);
        if ((new_nexts == NULL || new_indices == NULL
             || new_edests == NULL || new_eclosures == NULL)) {
            re_free(new_nexts);
            re_free(new_indices);
            re_free(new_edests);
            re_free(new_eclosures);
            return -1;
        }
        dfa->nexts = new_nexts;
        dfa->org_indices = new_indices;
        dfa->edests = new_edests;
        dfa->eclosures = new_eclosures;
        dfa->nodes_alloc = new_nodes_alloc;
    }
    dfa->nodes[dfa->nodes_len] = token;
    dfa->nodes[dfa->nodes_len].constraint = 0;
    dfa->nexts[dfa->nodes_len] = -1;
    re_node_set_init_empty(dfa->edests + dfa->nodes_len);
    re_node_set_init_empty(dfa->eclosures + dfa->nodes_len);
    return dfa->nodes_len++;
}

static re_hashval_t
calc_state_hash(const re_node_set *nodes, unsigned int context) {
    re_hashval_t hash = nodes->nelem + context;
    Idx i;
    for (i = 0; i < nodes->nelem; i++)
        hash += nodes->elems[i];
    return hash;
}

/* Search for the state whose node_set is equivalent to NODES.
   Return the pointer to the state, if we found it in the DFA.
   Otherwise create the new one and return it.  In case of an error
   return NULL and set the error code in ERR.
   Note: - We assume NULL as the invalid state, then it is possible that
	   return value is NULL and ERR is REG_NOERROR.
	 - We never return non-NULL value in case of any errors, it is for
	   optimization.  */

static re_dfastate_t *

re_acquire_state(reg_errcode_t *err, const re_dfa_t *dfa,
                 const re_node_set *nodes) {
    re_hashval_t hash;
    re_dfastate_t *new_state;
    struct re_state_table_entry *spot;
    Idx i;
#if defined GCC_LINT || defined lint
                                                                                                                            /* Suppress bogus uninitialized-variable warnings.  */
  *err = REG_NOERROR;
#endif
    if ((nodes->nelem == 0)) {
        *err = REG_NOERROR;
        return NULL;
    }
    hash = calc_state_hash(nodes, 0);
    spot = dfa->state_table + (hash & dfa->state_hash_mask);

    for (i = 0; i < spot->num; i++) {
        re_dfastate_t *state = spot->array[i];
        if (hash != state->hash)
            continue;
        if (re_node_set_compare(&state->nodes, nodes))
            return state;
    }

/* There are no appropriate state in the dfa, create the new one.  */
    new_state = create_ci_newstate(dfa, nodes, hash);
    if ((new_state == NULL))
        *err = REG_ESPACE;

    return new_state;
}

/* Search for the state whose node_set is equivalent to NODES and
   whose context is equivalent to CONTEXT.
   Return the pointer to the state, if we found it in the DFA.
   Otherwise create the new one and return it.  In case of an error
   return NULL and set the error code in ERR.
   Note: - We assume NULL as the invalid state, then it is possible that
	   return value is NULL and ERR is REG_NOERROR.
	 - We never return non-NULL value in case of any errors, it is for
	   optimization.  */

static re_dfastate_t *

re_acquire_state_context(reg_errcode_t *err, const re_dfa_t *dfa,
                         const re_node_set *nodes, unsigned int context) {
    re_hashval_t hash;
    re_dfastate_t *new_state;
    struct re_state_table_entry *spot;
    Idx i;
#if defined GCC_LINT || defined lint
                                                                                                                            /* Suppress bogus uninitialized-variable warnings.  */
  *err = REG_NOERROR;
#endif
    if (nodes->nelem == 0) {
        *err = REG_NOERROR;
        return NULL;
    }
    hash = calc_state_hash(nodes, context);
    spot = dfa->state_table + (hash & dfa->state_hash_mask);

    for (i = 0; i < spot->num; i++) {
        re_dfastate_t *state = spot->array[i];
        if (state->hash == hash
            && state->context == context
            && re_node_set_compare(state->entrance_nodes, nodes))
            return state;
    }
/* There are no appropriate state in 'dfa', create the new one.  */
    new_state = create_cd_newstate(dfa, nodes, context, hash);
    if ((new_state == NULL))
        *err = REG_ESPACE;

    return new_state;
}

/* Finish initialization of the new state NEWSTATE, and using its hash value
   HASH put in the appropriate bucket of DFA's state table.  Return value
   indicates the error code if failed.  */

static reg_errcode_t

register_state(const re_dfa_t *dfa, re_dfastate_t *newstate,
               re_hashval_t hash) {
    struct re_state_table_entry *spot;
    reg_errcode_t err;
    Idx i;

    newstate->hash = hash;
    err = re_node_set_alloc(&newstate->non_eps_nodes, newstate->nodes.nelem);
    if ((err != REG_NOERROR))
        return REG_ESPACE;
    for (i = 0; i < newstate->nodes.nelem; i++) {
        Idx elem = newstate->nodes.elems[i];
        if (!IS_EPSILON_NODE(dfa->nodes[elem].type))
            if (!re_node_set_insert_last(&newstate->non_eps_nodes, elem))
                return REG_ESPACE;
    }

    spot = dfa->state_table + (hash & dfa->state_hash_mask);
    if ((spot->alloc <= spot->num)) {
        Idx new_alloc = 2 * spot->num + 2;
        re_dfastate_t **new_array = re_realloc(spot->array, re_dfastate_t * ,
                                               new_alloc);
        if ((new_array == NULL))
            return REG_ESPACE;
        spot->array = new_array;
        spot->alloc = new_alloc;
    }
    spot->array[spot->num++] = newstate;
    return REG_NOERROR;
}

static void
free_state(re_dfastate_t *state) {
    re_node_set_free(&state->non_eps_nodes);
    re_node_set_free(&state->inveclosure);
    if (state->entrance_nodes != &state->nodes) {
        re_node_set_free(state->entrance_nodes);
        re_free(state->entrance_nodes);
    }
    re_node_set_free(&state->nodes);
    re_free(state->word_trtable);
    re_free(state->trtable);
    re_free(state);
}

/* Create the new state which is independent of contexts.
   Return the new state if succeeded, otherwise return NULL.  */

static re_dfastate_t *

create_ci_newstate(const re_dfa_t *dfa, const re_node_set *nodes,
                   re_hashval_t hash) {
    Idx i;
    reg_errcode_t err;
    re_dfastate_t *newstate;

    newstate = (re_dfastate_t *) calloc(sizeof(re_dfastate_t), 1);
    if ((newstate == NULL))
        return NULL;
    err = re_node_set_init_copy(&newstate->nodes, nodes);
    if ((err != REG_NOERROR)) {
        re_free(newstate);
        return NULL;
    }

    newstate->entrance_nodes = &newstate->nodes;
    for (i = 0; i < nodes->nelem; i++) {
        re_token_t *node = dfa->nodes + nodes->elems[i];
        re_token_type_t type = node->type;
        if (type == CHARACTER && !node->constraint)
            continue;

        /* If the state has the halt node, the state is a halt state.  */
        if (type == END_OF_RE)
            newstate->halt = 1;
        else if (type == OP_BACK_REF)
            newstate->has_backref = 1;
        else if (type == ANCHOR || node->constraint)
            newstate->has_constraint = 1;
    }
    err = register_state(dfa, newstate, hash);
    if ((err != REG_NOERROR)) {
        free_state(newstate);
        newstate = NULL;
    }
    return newstate;
}

/* Create the new state which is depend on the context CONTEXT.
   Return the new state if succeeded, otherwise return NULL.  */

static re_dfastate_t *

create_cd_newstate(const re_dfa_t *dfa, const re_node_set *nodes,
                   unsigned int context, re_hashval_t hash) {
    Idx i, nctx_nodes = 0;
    reg_errcode_t err;
    re_dfastate_t *newstate;

    newstate = (re_dfastate_t *) calloc(sizeof(re_dfastate_t), 1);
    if ((newstate == NULL))
        return NULL;
    err = re_node_set_init_copy(&newstate->nodes, nodes);
    if ((err != REG_NOERROR)) {
        re_free(newstate);
        return NULL;
    }

    newstate->context = context;
    newstate->entrance_nodes = &newstate->nodes;

    for (i = 0; i < nodes->nelem; i++) {
        re_token_t *node = dfa->nodes + nodes->elems[i];
        re_token_type_t type = node->type;
        unsigned int constraint = node->constraint;

        if (type == CHARACTER && !constraint)
            continue;

        /* If the state has the halt node, the state is a halt state.  */
        if (type == END_OF_RE)
            newstate->halt = 1;
        else if (type == OP_BACK_REF)
            newstate->has_backref = 1;

        if (constraint) {
            if (newstate->entrance_nodes == &newstate->nodes) {
                newstate->entrance_nodes = re_malloc(re_node_set, 1);
                if ((newstate->entrance_nodes == NULL)) {
                    free_state(newstate);
                    return NULL;
                }
                if (re_node_set_init_copy(newstate->entrance_nodes, nodes)
                    != REG_NOERROR)
                    return NULL;
                nctx_nodes = 0;
                newstate->has_constraint = 1;
            }

            if (NOT_SATISFY_PREV_CONSTRAINT(constraint, context)) {
                re_node_set_remove_at(&newstate->nodes, i - nctx_nodes);
                ++nctx_nodes;
            }
        }
    }
    err = register_state(dfa, newstate, hash);
    if ((err != REG_NOERROR)) {
        free_state(newstate);
        newstate = NULL;
    }
    return newstate;
}

/* Extended regular expression matching and search library.
   Copyright (C) 2002-2018 Free Software Foundation, Inc.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef RE_TRANSLATE_TYPE
# define __RE_TRANSLATE_TYPE unsigned char *
# ifdef __USE_GNU
#  define RE_TRANSLATE_TYPE __RE_TRANSLATE_TYPE
# endif
#endif

# define __REPB_PREFIX(name) name


static reg_errcode_t re_compile_internal(regex_t *preg, const char *pattern,
                                         size_t length, reg_syntax_t syntax);

static void re_compile_fastmap_iter(regex_t *bufp,
                                    const re_dfastate_t *init_state,
                                    char *fastmap);

static reg_errcode_t init_dfa(re_dfa_t *dfa, size_t pat_len);

static void free_workarea_compile(regex_t *preg);

static reg_errcode_t create_initial_state(re_dfa_t *dfa);

static reg_errcode_t analyze(regex_t *preg);

static reg_errcode_t preorder(bin_tree_t *root,
                              reg_errcode_t (fn(void *, bin_tree_t *)),
                              void *extra);

static reg_errcode_t postorder(bin_tree_t *root,
                               reg_errcode_t (fn(void *, bin_tree_t *)),
                               void *extra);

static reg_errcode_t optimize_subexps(void *extra, bin_tree_t *node);

static reg_errcode_t lower_subexps(void *extra, bin_tree_t *node);

static bin_tree_t *lower_subexp(reg_errcode_t *err, regex_t *preg,
                                bin_tree_t *node);

static reg_errcode_t calc_first(void *extra, bin_tree_t *node);

static reg_errcode_t calc_next(void *extra, bin_tree_t *node);

static reg_errcode_t link_nfa_nodes(void *extra, bin_tree_t *node);

static Idx duplicate_node(re_dfa_t *dfa, Idx org_idx, unsigned int constraint);

static Idx search_duplicated_node(const re_dfa_t *dfa, Idx org_node,
                                  unsigned int constraint);

static reg_errcode_t calc_eclosure(re_dfa_t *dfa);

static reg_errcode_t calc_eclosure_iter(re_node_set *new_set, re_dfa_t *dfa,
                                        Idx node, bool root);

static reg_errcode_t calc_inveclosure(re_dfa_t *dfa);

static Idx fetch_number(re_string_t *input, re_token_t *token,
                        reg_syntax_t syntax);

static int peek_token(re_token_t *token, re_string_t *input,
                      reg_syntax_t syntax);

static bin_tree_t *parse(re_string_t *regexp, regex_t *preg,
                         reg_syntax_t syntax, reg_errcode_t *err);

static bin_tree_t *parse_reg_exp(re_string_t *regexp, regex_t *preg,
                                 re_token_t *token, reg_syntax_t syntax,
                                 Idx nest, reg_errcode_t *err);

static bin_tree_t *parse_branch(re_string_t *regexp, regex_t *preg,
                                re_token_t *token, reg_syntax_t syntax,
                                Idx nest, reg_errcode_t *err);

static bin_tree_t *parse_expression(re_string_t *regexp, regex_t *preg,
                                    re_token_t *token, reg_syntax_t syntax,
                                    Idx nest, reg_errcode_t *err);

static bin_tree_t *parse_sub_exp(re_string_t *regexp, regex_t *preg,
                                 re_token_t *token, reg_syntax_t syntax,
                                 Idx nest, reg_errcode_t *err);

static bin_tree_t *parse_dup_op(bin_tree_t *dup_elem, re_string_t *regexp,
                                re_dfa_t *dfa, re_token_t *token,
                                reg_syntax_t syntax, reg_errcode_t *err);

static bin_tree_t *parse_bracket_exp(re_string_t *regexp, re_dfa_t *dfa,
                                     re_token_t *token, reg_syntax_t syntax,
                                     reg_errcode_t *err);

static reg_errcode_t parse_bracket_element(bracket_elem_t *elem,
                                           re_string_t *regexp,
                                           re_token_t *token, int token_len,
                                           re_dfa_t *dfa,
                                           reg_syntax_t syntax,
                                           bool accept_hyphen);

static reg_errcode_t parse_bracket_symbol(bracket_elem_t *elem,
                                          re_string_t *regexp,
                                          re_token_t *token);

static reg_errcode_t build_equiv_class(bitset_t sbcset,
                                       const unsigned char *name);

static reg_errcode_t build_charclass(RE_TRANSLATE_TYPE trans,
                                     bitset_t sbcset,
                                     const char *class_name,
                                     reg_syntax_t syntax);

static bin_tree_t *build_charclass_op(re_dfa_t *dfa,
                                      RE_TRANSLATE_TYPE trans,
                                      const char *class_name,
                                      const char *extra,
                                      bool non_match, reg_errcode_t *err);

static bin_tree_t *create_tree(re_dfa_t *dfa,
                               bin_tree_t *left, bin_tree_t *right,
                               re_token_type_t type);

static bin_tree_t *create_token_tree(re_dfa_t *dfa,
                                     bin_tree_t *left, bin_tree_t *right,
                                     const re_token_t *token);

static bin_tree_t *duplicate_tree(const bin_tree_t *src, re_dfa_t *dfa);

static void free_token(re_token_t *node);

static reg_errcode_t free_tree(void *extra, bin_tree_t *node);

static reg_errcode_t mark_opt_subexp(void *extra, bin_tree_t *node);

/* This table gives an error message for each of the error codes listed
   in regex.h.  Obviously the order here has to be same as there.
   POSIX doesn't require that we do anything for REG_NOERROR,
   but why not be nice?  */

static const char __re_error_msgid[] =
        {
#define REG_NOERROR_IDX    0
                gettext_noop("Success")    /* REG_NOERROR */
                "\0"
#define REG_NOMATCH_IDX (REG_NOERROR_IDX + sizeof "Success")
                gettext_noop ("No match")    /* REG_NOMATCH */
                "\0"
#define REG_BADPAT_IDX    (REG_NOMATCH_IDX + sizeof "No match")
                gettext_noop ("Invalid regular expression") /* REG_BADPAT */
                "\0"
#define REG_ECOLLATE_IDX (REG_BADPAT_IDX + sizeof "Invalid regular expression")
                gettext_noop ("Invalid collation character") /* REG_ECOLLATE */
                "\0"
#define REG_ECTYPE_IDX    (REG_ECOLLATE_IDX + sizeof "Invalid collation character")
                gettext_noop ("Invalid character class name") /* REG_ECTYPE */
                "\0"
#define REG_EESCAPE_IDX    (REG_ECTYPE_IDX + sizeof "Invalid character class name")
                gettext_noop ("Trailing backslash") /* REG_EESCAPE */
                "\0"
#define REG_ESUBREG_IDX    (REG_EESCAPE_IDX + sizeof "Trailing backslash")
                gettext_noop ("Invalid back reference") /* REG_ESUBREG */
                "\0"
#define REG_EBRACK_IDX    (REG_ESUBREG_IDX + sizeof "Invalid back reference")
                gettext_noop ("Unmatched [, [^, [:, [., or [=")    /* REG_EBRACK */
                "\0"
#define REG_EPAREN_IDX    (REG_EBRACK_IDX + sizeof "Unmatched [, [^, [:, [., or [=")
                gettext_noop ("Unmatched ( or \\(") /* REG_EPAREN */
                "\0"
#define REG_EBRACE_IDX    (REG_EPAREN_IDX + sizeof "Unmatched ( or \\(")
                gettext_noop ("Unmatched \\{") /* REG_EBRACE */
                "\0"
#define REG_BADBR_IDX    (REG_EBRACE_IDX + sizeof "Unmatched \\{")
                gettext_noop ("Invalid content of \\{\\}") /* REG_BADBR */
                "\0"
#define REG_ERANGE_IDX    (REG_BADBR_IDX + sizeof "Invalid content of \\{\\}")
                gettext_noop ("Invalid range end")    /* REG_ERANGE */
                "\0"
#define REG_ESPACE_IDX    (REG_ERANGE_IDX + sizeof "Invalid range end")
                gettext_noop ("Memory exhausted") /* REG_ESPACE */
                "\0"
#define REG_BADRPT_IDX    (REG_ESPACE_IDX + sizeof "Memory exhausted")
                gettext_noop ("Invalid preceding regular expression") /* REG_BADRPT */
                "\0"
#define REG_EEND_IDX    (REG_BADRPT_IDX + sizeof "Invalid preceding regular expression")
                gettext_noop ("Premature end of regular expression") /* REG_EEND */
                "\0"
#define REG_ESIZE_IDX    (REG_EEND_IDX + sizeof "Premature end of regular expression")
                gettext_noop ("Regular expression too big") /* REG_ESIZE */
                "\0"
#define REG_ERPAREN_IDX    (REG_ESIZE_IDX + sizeof "Regular expression too big")
                gettext_noop ("Unmatched ) or \\)") /* REG_ERPAREN */
        };

static const size_t __re_error_msgid_idx[] =
        {
                REG_NOERROR_IDX,
                REG_NOMATCH_IDX,
                REG_BADPAT_IDX,
                REG_ECOLLATE_IDX,
                REG_ECTYPE_IDX,
                REG_EESCAPE_IDX,
                REG_ESUBREG_IDX,
                REG_EBRACK_IDX,
                REG_EPAREN_IDX,
                REG_EBRACE_IDX,
                REG_BADBR_IDX,
                REG_ERANGE_IDX,
                REG_ESPACE_IDX,
                REG_BADRPT_IDX,
                REG_EEND_IDX,
                REG_ESIZE_IDX,
                REG_ERPAREN_IDX
        };

/* Entry points for GNU code.  */

/* re_compile_pattern is the GNU regular expression compiler: it
   compiles PATTERN (of length LENGTH) and puts the result in BUFP.
   Returns 0 if the pattern was valid, otherwise an error string.

   Assumes the 'allocated' (and perhaps 'buffer') and 'translate' fields
   are set in BUFP on entry.  */

const char *
re_compile_pattern(const char *pattern, size_t length,
                   struct re_pattern_buffer *bufp) {
    reg_errcode_t ret;

    /* And GNU code determines whether or not to get register information
       by passing null for the REGS argument to re_match, etc., not by
       setting no_sub, unless RE_NO_SUB is set.  */
    bufp->no_sub = !!(re_syntax_options & RE_NO_SUB);

    /* Match anchors at newline.  */
    bufp->newline_anchor = 1;

    ret = re_compile_internal(bufp, pattern, length, re_syntax_options);

    if (!ret)
        return NULL;
    return gettext(__re_error_msgid + __re_error_msgid_idx[(int) ret]);
}


/* Set by 're_set_syntax' to the current regexp syntax to recognize.  Can
   also be assigned to arbitrarily: each pattern buffer stores its own
   syntax, so it can be changed between regex compilations.  */
/* This has no initializer because initialized variables in Emacs
   become read-only after dumping.  */
reg_syntax_t re_syntax_options;


/* Specify the precise syntax of regexps for compilation.  This provides
   for compatibility for various utilities which historically have
   different, incompatible syntaxes.

   The argument SYNTAX is a bit mask comprised of the various bits
   defined in regex.h.  We return the old syntax.  */

reg_syntax_t
re_set_syntax(reg_syntax_t syntax) {
    reg_syntax_t ret = re_syntax_options;

    re_syntax_options = syntax;
    return ret;
}

int
re_compile_fastmap(struct re_pattern_buffer *bufp) {
    re_dfa_t *dfa = bufp->buffer;
    char *fastmap = bufp->fastmap;

    memset(fastmap, '\0', sizeof(char) * SBC_MAX);
    re_compile_fastmap_iter(bufp, dfa->init_state, fastmap);
    if (dfa->init_state != dfa->init_state_word)
        re_compile_fastmap_iter(bufp, dfa->init_state_word, fastmap);
    if (dfa->init_state != dfa->init_state_nl)
        re_compile_fastmap_iter(bufp, dfa->init_state_nl, fastmap);
    if (dfa->init_state != dfa->init_state_begbuf)
        re_compile_fastmap_iter(bufp, dfa->init_state_begbuf, fastmap);
    bufp->fastmap_accurate = 1;
    return 0;
}


static inline void
__attribute__ ((always_inline))
re_set_fastmap(char *fastmap, bool icase, int ch) {
    fastmap[ch] = 1;
    if (icase)
        fastmap[tolower(ch)] = 1;
}

/* Helper function for re_compile_fastmap.
   Compile fastmap for the initial_state INIT_STATE.  */

static void
re_compile_fastmap_iter(regex_t *bufp, const re_dfastate_t *init_state,
                        char *fastmap) {
    re_dfa_t *dfa = bufp->buffer;
    Idx node_cnt;
    bool icase = (dfa->mb_cur_max == 1 && (bufp->syntax & RE_ICASE));
    for (node_cnt = 0; node_cnt < init_state->nodes.nelem; ++node_cnt) {
        Idx node = init_state->nodes.elems[node_cnt];
        re_token_type_t type = dfa->nodes[node].type;

        if (type == CHARACTER) {
            re_set_fastmap(fastmap, icase, dfa->nodes[node].opr.c);
        } else if (type == SIMPLE_BRACKET) {
            int i, ch;
            for (i = 0, ch = 0; i < BITSET_WORDS; ++i) {
                int j;
                bitset_word_t w = dfa->nodes[node].opr.sbcset[i];
                for (j = 0; j < BITSET_WORD_BITS; ++j, ++ch)
                    if (w & ((bitset_word_t) 1 << j))
                        re_set_fastmap(fastmap, icase, ch);
            }
        } else if (type == OP_PERIOD
                   || type == END_OF_RE) {
            memset(fastmap, '\1', sizeof(char) * SBC_MAX);
            if (type == END_OF_RE)
                bufp->can_be_null = 1;
            return;
        }
    }
}

/* Entry point for POSIX code.  */
/* regcomp takes a regular expression as a string and compiles it.

   PREG is a regex_t *.  We do not expect any fields to be initialized,
   since POSIX says we shouldn't.  Thus, we set

     'buffer' to the compiled pattern;
     'used' to the length of the compiled pattern;
     'syntax' to RE_SYNTAX_POSIX_EXTENDED if the
       REG_EXTENDED bit in CFLAGS is set; otherwise, to
       RE_SYNTAX_POSIX_BASIC;
     'newline_anchor' to REG_NEWLINE being set in CFLAGS;
     'fastmap' to an allocated space for the fastmap;
     'fastmap_accurate' to zero;
     're_nsub' to the number of subexpressions in PATTERN.

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

   It returns 0 if it succeeds, nonzero if it doesn't.  (See regex.h for
   the return codes and their meanings.)  */

int
regcomp(regex_t *_Restrict_ preg, const char *_Restrict_ pattern, int cflags) {
    reg_errcode_t ret;
    reg_syntax_t syntax = ((cflags & REG_EXTENDED) ? RE_SYNTAX_POSIX_EXTENDED
                                                   : RE_SYNTAX_POSIX_BASIC);

    preg->buffer = NULL;
    preg->allocated = 0;
    preg->used = 0;

    /* Try to allocate space for the fastmap.  */
    preg->fastmap = re_malloc(
    char, SBC_MAX);
    if ((preg->fastmap == NULL))
        return REG_ESPACE;

    syntax |= (cflags & REG_ICASE) ? RE_ICASE : 0;

    /* If REG_NEWLINE is set, newlines are treated differently.  */
    if (cflags & REG_NEWLINE) { /* REG_NEWLINE implies neither . nor [^...] match newline.  */
        syntax &= ~RE_DOT_NEWLINE;
        syntax |= RE_HAT_LISTS_NOT_NEWLINE;
        /* It also changes the matching behavior.  */
        preg->newline_anchor = 1;
    } else
        preg->newline_anchor = 0;
    preg->no_sub = !!(cflags & REG_NOSUB);
    preg->translate = NULL;

    ret = re_compile_internal(preg, pattern, strlen(pattern), syntax);

    /* POSIX doesn't distinguish between an unmatched open-group and an
       unmatched close-group: both are REG_EPAREN.  */
    if (ret == REG_ERPAREN)
        ret = REG_EPAREN;

    /* We have already checked preg->fastmap != NULL.  */
    if ((ret == REG_NOERROR))
        /* Compute the fastmap now, since regexec cannot modify the pattern
           buffer.  This function never fails in this implementation.  */
        (void) re_compile_fastmap(preg);
    else {
        /* Some error occurred while compiling the expression.  */
        re_free(preg->fastmap);
        preg->fastmap = NULL;
    }

    return (int) ret;
}

/* Returns a message corresponding to an error code, ERRCODE, returned
   from either regcomp or regexec.   We don't use PREG here.  */

size_t
regerror(int errcode, const regex_t *_Restrict_ preg, char *_Restrict_ errbuf,
         size_t errbuf_size) {
    const char *msg;
    size_t msg_size;
    int nerrcodes = sizeof __re_error_msgid_idx / sizeof __re_error_msgid_idx[0];

    if ((errcode < 0 || errcode >= nerrcodes))
        /* Only error codes returned by the rest of the code should be passed
           to this routine.  If we are given anything else, or if other regex
           code generates an invalid error code, then the program has a bug.
           Dump core so we can fix it.  */
        abort();

    msg = gettext(__re_error_msgid + __re_error_msgid_idx[errcode]);

    msg_size = strlen(msg) + 1; /* Includes the null.  */

    if ((errbuf_size != 0)) {
        size_t cpy_size = msg_size;
        if ((msg_size > errbuf_size)) {
            cpy_size = errbuf_size - 1;
            errbuf[cpy_size] = '\0';
        }
        memcpy(errbuf, msg, cpy_size);
    }

    return msg_size;
}

static void
free_dfa_content(re_dfa_t *dfa) {
    Idx i, j;

    if (dfa->nodes)
        for (i = 0; i < dfa->nodes_len; ++i)
            free_token(dfa->nodes + i);
    re_free(dfa->nexts);
    for (i = 0; i < dfa->nodes_len; ++i) {
        if (dfa->eclosures != NULL)
            re_node_set_free(dfa->eclosures + i);
        if (dfa->inveclosures != NULL)
            re_node_set_free(dfa->inveclosures + i);
        if (dfa->edests != NULL)
            re_node_set_free(dfa->edests + i);
    }
    re_free(dfa->edests);
    re_free(dfa->eclosures);
    re_free(dfa->inveclosures);
    re_free(dfa->nodes);

    if (dfa->state_table)
        for (i = 0; i <= dfa->state_hash_mask; ++i) {
            struct re_state_table_entry *entry = dfa->state_table + i;
            for (j = 0; j < entry->num; ++j) {
                re_dfastate_t *state = entry->array[j];
                free_state(state);
            }
            re_free(entry->array);
        }
    re_free(dfa->state_table);
    re_free(dfa->subexp_map);
    re_free(dfa);
}


/* Free dynamically allocated space used by PREG.  */

void
regfree(regex_t *preg) {
    re_dfa_t *dfa = preg->buffer;
    if ((dfa != NULL)) {
        lock_fini(dfa->lock);
        free_dfa_content(dfa);
    }
    preg->buffer = NULL;
    preg->allocated = 0;

    re_free(preg->fastmap);
    preg->fastmap = NULL;

    re_free(preg->translate);
    preg->translate = NULL;
}

/* Internal entry point.
   Compile the regular expression PATTERN, whose length is LENGTH.
   SYNTAX indicate regular expression's syntax.  */

static reg_errcode_t
re_compile_internal(regex_t *preg, const char *pattern, size_t length,
                    reg_syntax_t syntax) {
    reg_errcode_t err = REG_NOERROR;
    re_dfa_t *dfa;
    re_string_t regexp;

    /* Initialize the pattern buffer.  */
    preg->fastmap_accurate = 0;
    preg->syntax = syntax;
    preg->not_bol = preg->not_eol = 0;
    preg->used = 0;
    preg->re_nsub = 0;
    preg->can_be_null = 0;
    preg->regs_allocated = REGS_UNALLOCATED;

    /* Initialize the dfa.  */
    dfa = preg->buffer;
    if ((preg->allocated < sizeof(re_dfa_t))) {
        /* If zero allocated, but buffer is non-null, try to realloc
       enough space.  This loses if buffer's address is bogus, but
       that is the user's responsibility.  If ->buffer is NULL this
       is a simple allocation.  */
        dfa = re_realloc(preg->buffer, re_dfa_t, 1);
        if (dfa == NULL)
            return REG_ESPACE;
        preg->allocated = sizeof(re_dfa_t);
        preg->buffer = dfa;
    }
    preg->used = sizeof(re_dfa_t);

    err = init_dfa(dfa, length);
    if ((err == REG_NOERROR && lock_init(dfa->lock) != 0))
        err = REG_ESPACE;
    if ((err != REG_NOERROR)) {
        free_dfa_content(dfa);
        preg->buffer = NULL;
        preg->allocated = 0;
        return err;
    }
    err = re_string_construct(&regexp, pattern, length, preg->translate,
                              (syntax & RE_ICASE) != 0, dfa);
    if ((err != REG_NOERROR)) {
        re_compile_internal_free_return:
        free_workarea_compile(preg);
        re_string_destruct(&regexp);
        lock_fini(dfa->lock);
        free_dfa_content(dfa);
        preg->buffer = NULL;
        preg->allocated = 0;
        return err;
    }

    /* Parse the regular expression, and build a structure tree.  */
    preg->re_nsub = 0;
    dfa->str_tree = parse(&regexp, preg, syntax, &err);
    if ((dfa->str_tree == NULL))
        goto re_compile_internal_free_return;

    /* Analyze the tree and create the nfa.  */
    err = analyze(preg);
    if ((err != REG_NOERROR))
        goto re_compile_internal_free_return;

    /* Then create the initial state of the dfa.  */
    err = create_initial_state(dfa);

    /* Release work areas.  */
    free_workarea_compile(preg);
    re_string_destruct(&regexp);

    if ((err != REG_NOERROR)) {
        lock_fini(dfa->lock);
        free_dfa_content(dfa);
        preg->buffer = NULL;
        preg->allocated = 0;
    }

    return err;
}

/* Initialize DFA.  We use the length of the regular expression PAT_LEN
   as the initial length of some arrays.  */

static reg_errcode_t
init_dfa(re_dfa_t *dfa, size_t pat_len) {
    __re_size_t table_size;
    const char *codeset_name;

    size_t max_i18n_object_size = 0;

    size_t max_object_size =
            MAX (sizeof(struct re_state_table_entry),
                 MAX(sizeof(re_token_t),
                     MAX(sizeof(re_node_set),
                         MAX(sizeof(regmatch_t),
                             max_i18n_object_size))));

    memset(dfa, '\0', sizeof(re_dfa_t));

    /* Force allocation of str_tree_storage the first time.  */
    dfa->str_tree_storage_idx = BIN_TREE_STORAGE_SIZE;

    /* Avoid overflows.  The extra "/ 2" is for the table_size doubling
       calculation below, and for similar doubling calculations
       elsewhere.  And it's <= rather than <, because some of the
       doubling calculations add 1 afterwards.  */
    if ((MIN (IDX_MAX, SIZE_MAX / max_object_size) / 2
         <= pat_len))
        return REG_ESPACE;

    dfa->nodes_alloc = pat_len + 1;
    dfa->nodes = re_malloc(re_token_t, dfa->nodes_alloc);

    /*  table_size = 2 ^ ceil(log pat_len) */
    for (table_size = 1;; table_size <<= 1)
        if (table_size > pat_len)
            break;

    dfa->state_table = calloc(sizeof(struct re_state_table_entry), table_size);
    dfa->state_hash_mask = table_size - 1;

    dfa->mb_cur_max = MB_CUR_MAX;
    codeset_name = nl_langinfo(CODESET);
    if ((codeset_name[0] == 'U' || codeset_name[0] == 'u')
        && (codeset_name[1] == 'T' || codeset_name[1] == 't')
        && (codeset_name[2] == 'F' || codeset_name[2] == 'f')
        && strcmp(codeset_name + 3 + (codeset_name[3] == '-'), "8") == 0)
        dfa->is_utf8 = 1;

    /* We check exhaustively in the loop below if this charset is a
       superset of ASCII.  */
    dfa->map_notascii = 0;

    if ((dfa->nodes == NULL || dfa->state_table == NULL))
        return REG_ESPACE;
    return REG_NOERROR;
}

/* Initialize WORD_CHAR table, which indicate which character is
   "word".  In this case "word" means that it is the word construction
   character used by some operators like "\<", "\>", etc.  */

static void
init_word_char(re_dfa_t *dfa) {
    int i = 0;
    int j;
    int ch = 0;
    dfa->word_ops_used = 1;
    if ((dfa->map_notascii == 0)) {
        /* Avoid uint32_t and uint64_t as some non-GCC platforms lack
       them, an issue when this code is used in Gnulib.  */
        bitset_word_t bits0 = 0x00000000;
        bitset_word_t bits1 = 0x03ff0000;
        bitset_word_t bits2 = 0x87fffffe;
        bitset_word_t bits3 = 0x07fffffe;
        if (BITSET_WORD_BITS == 64) {
            /* Pacify gcc -Woverflow on 32-bit platformns.  */
            dfa->word_char[0] = bits1 << 31 << 1 | bits0;
            dfa->word_char[1] = bits3 << 31 << 1 | bits2;
            i = 2;
        } else if (BITSET_WORD_BITS == 32) {
            dfa->word_char[0] = bits0;
            dfa->word_char[1] = bits1;
            dfa->word_char[2] = bits2;
            dfa->word_char[3] = bits3;
            i = 4;
        } else
            goto general_case;
        ch = 128;

        if ((dfa->is_utf8)) {
            memset(&dfa->word_char[i], '\0', (SBC_MAX - ch) / 8);
            return;
        }
    }

    general_case:
    for (; i < BITSET_WORDS; ++i)
        for (j = 0; j < BITSET_WORD_BITS; ++j, ++ch)
            if (isalnum(ch) || ch == '_')
                dfa->word_char[i] |= (bitset_word_t) 1 << j;
}

/* Free the work area which are only used while compiling.  */

static void
free_workarea_compile(regex_t *preg) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_storage_t *storage, *next;
    for (storage = dfa->str_tree_storage; storage; storage = next) {
        next = storage->next;
        re_free(storage);
    }
    dfa->str_tree_storage = NULL;
    dfa->str_tree_storage_idx = BIN_TREE_STORAGE_SIZE;
    dfa->str_tree = NULL;
    re_free(dfa->org_indices);
    dfa->org_indices = NULL;
}

/* Create initial states for all contexts.  */

static reg_errcode_t
create_initial_state(re_dfa_t *dfa) {
    Idx first, i;
    reg_errcode_t err;
    re_node_set init_nodes;

    /* Initial states have the epsilon closure of the node which is
       the first node of the regular expression.  */
    first = dfa->str_tree->first->node_idx;
    dfa->init_node = first;
    err = re_node_set_init_copy(&init_nodes, dfa->eclosures + first);
    if ((err != REG_NOERROR))
        return err;

    /* The back-references which are in initial states can epsilon transit,
       since in this case all of the subexpressions can be null.
       Then we add epsilon closures of the nodes which are the next nodes of
       the back-references.  */
    if (dfa->nbackref > 0)
        for (i = 0; i < init_nodes.nelem; ++i) {
            Idx node_idx = init_nodes.elems[i];
            re_token_type_t type = dfa->nodes[node_idx].type;

            Idx clexp_idx;
            if (type != OP_BACK_REF)
                continue;
            for (clexp_idx = 0; clexp_idx < init_nodes.nelem; ++clexp_idx) {
                re_token_t *clexp_node;
                clexp_node = dfa->nodes + init_nodes.elems[clexp_idx];
                if (clexp_node->type == OP_CLOSE_SUBEXP
                    && clexp_node->opr.idx == dfa->nodes[node_idx].opr.idx)
                    break;
            }
            if (clexp_idx == init_nodes.nelem)
                continue;

            if (type == OP_BACK_REF) {
                Idx dest_idx = dfa->edests[node_idx].elems[0];
                if (!re_node_set_contains(&init_nodes, dest_idx)) {
                    reg_errcode_t merge_err
                            = re_node_set_merge(&init_nodes, dfa->eclosures + dest_idx);
                    if (merge_err != REG_NOERROR)
                        return merge_err;
                    i = 0;
                }
            }
        }

    /* It must be the first time to invoke acquire_state.  */
    dfa->init_state = re_acquire_state_context(&err, dfa, &init_nodes, 0);
    /* We don't check ERR here, since the initial state must not be NULL.  */
    if ((dfa->init_state == NULL))
        return err;
    if (dfa->init_state->has_constraint) {
        dfa->init_state_word = re_acquire_state_context(&err, dfa, &init_nodes,
                                                        CONTEXT_WORD);
        dfa->init_state_nl = re_acquire_state_context(&err, dfa, &init_nodes,
                                                      CONTEXT_NEWLINE);
        dfa->init_state_begbuf = re_acquire_state_context(&err, dfa,
                                                          &init_nodes,
                                                          CONTEXT_NEWLINE
                                                          | CONTEXT_BEGBUF);
        if ((dfa->init_state_word == NULL
             || dfa->init_state_nl == NULL
             || dfa->init_state_begbuf == NULL))
            return err;
    } else
        dfa->init_state_word = dfa->init_state_nl
                = dfa->init_state_begbuf = dfa->init_state;

    re_node_set_free(&init_nodes);
    return REG_NOERROR;
}


/* Analyze the structure tree, and calculate "first", "next", "edest",
   "eclosure", and "inveclosure".  */

static reg_errcode_t
analyze(regex_t *preg) {
    re_dfa_t *dfa = preg->buffer;
    reg_errcode_t ret;

    /* Allocate arrays.  */
    dfa->nexts = re_malloc(Idx, dfa->nodes_alloc);
    dfa->org_indices = re_malloc(Idx, dfa->nodes_alloc);
    dfa->edests = re_malloc(re_node_set, dfa->nodes_alloc);
    dfa->eclosures = re_malloc(re_node_set, dfa->nodes_alloc);
    if ((dfa->nexts == NULL || dfa->org_indices == NULL
         || dfa->edests == NULL || dfa->eclosures == NULL))
        return REG_ESPACE;

    dfa->subexp_map = re_malloc(Idx, preg->re_nsub);
    if (dfa->subexp_map != NULL) {
        Idx i;
        for (i = 0; i < preg->re_nsub; i++)
            dfa->subexp_map[i] = i;
        preorder(dfa->str_tree, optimize_subexps, dfa);
        for (i = 0; i < preg->re_nsub; i++)
            if (dfa->subexp_map[i] != i)
                break;
        if (i == preg->re_nsub) {
            re_free(dfa->subexp_map);
            dfa->subexp_map = NULL;
        }
    }

    ret = postorder(dfa->str_tree, lower_subexps, preg);
    if ((ret != REG_NOERROR))
        return ret;
    ret = postorder(dfa->str_tree, calc_first, dfa);
    if ((ret != REG_NOERROR))
        return ret;
    preorder(dfa->str_tree, calc_next, dfa);
    ret = preorder(dfa->str_tree, link_nfa_nodes, dfa);
    if ((ret != REG_NOERROR))
        return ret;
    ret = calc_eclosure(dfa);
    if ((ret != REG_NOERROR))
        return ret;

    /* We only need this during the prune_impossible_nodes pass in regexec.c;
       skip it if p_i_n will not run, as calc_inveclosure can be quadratic.  */
    if ((!preg->no_sub && preg->re_nsub > 0 && dfa->has_plural_match)
        || dfa->nbackref) {
        dfa->inveclosures = re_malloc(re_node_set, dfa->nodes_len);
        if ((dfa->inveclosures == NULL))
            return REG_ESPACE;
        ret = calc_inveclosure(dfa);
    }

    return ret;
}

/* Our parse trees are very unbalanced, so we cannot use a stack to
   implement parse tree visits.  Instead, we use parent pointers and
   some hairy code in these two functions.  */
static reg_errcode_t
postorder(bin_tree_t *root, reg_errcode_t (fn(void *, bin_tree_t *)),
          void *extra) {
    bin_tree_t *node, *prev;

    for (node = root;;) {
        /* Descend down the tree, preferably to the left (or to the right
       if that's the only child).  */
        while (node->left || node->right)
            if (node->left)
                node = node->left;
            else
                node = node->right;

        do {
            reg_errcode_t err = fn(extra, node);
            if ((err != REG_NOERROR))
                return err;
            if (node->parent == NULL)
                return REG_NOERROR;
            prev = node;
            node = node->parent;
        }
            /* Go up while we have a node that is reached from the right.  */
        while (node->right == prev || node->right == NULL);
        node = node->right;
    }
}

static reg_errcode_t
preorder(bin_tree_t *root, reg_errcode_t (fn(void *, bin_tree_t *)),
         void *extra) {
    bin_tree_t *node;

    for (node = root;;) {
        reg_errcode_t err = fn(extra, node);
        if ((err != REG_NOERROR))
            return err;

        /* Go to the left node, or up and to the right.  */
        if (node->left)
            node = node->left;
        else {
            bin_tree_t *prev = NULL;
            while (node->right == prev || node->right == NULL) {
                prev = node;
                node = node->parent;
                if (!node)
                    return REG_NOERROR;
            }
            node = node->right;
        }
    }
}

/* Optimization pass: if a SUBEXP is entirely contained, strip it and tell
   re_search_internal to map the inner one's opr.idx to this one's.  Adjust
   backreferences as well.  Requires a preorder visit.  */
static reg_errcode_t
optimize_subexps(void *extra, bin_tree_t *node) {
    re_dfa_t *dfa = (re_dfa_t *) extra;

    if (node->token.type == OP_BACK_REF && dfa->subexp_map) {
        int idx = node->token.opr.idx;
        node->token.opr.idx = dfa->subexp_map[idx];
        dfa->used_bkref_map |= 1 << node->token.opr.idx;
    } else if (node->token.type == SUBEXP
               && node->left && node->left->token.type == SUBEXP) {
        Idx other_idx = node->left->token.opr.idx;

        node->left = node->left->left;
        if (node->left)
            node->left->parent = node;

        dfa->subexp_map[other_idx] = dfa->subexp_map[node->token.opr.idx];
        if (other_idx < BITSET_WORD_BITS)
            dfa->used_bkref_map &= ~((bitset_word_t) 1 << other_idx);
    }

    return REG_NOERROR;
}

/* Lowering pass: Turn each SUBEXP node into the appropriate concatenation
   of OP_OPEN_SUBEXP, the body of the SUBEXP (if any) and OP_CLOSE_SUBEXP.  */
static reg_errcode_t
lower_subexps(void *extra, bin_tree_t *node) {
    regex_t *preg = (regex_t *) extra;
    reg_errcode_t err = REG_NOERROR;

    if (node->left && node->left->token.type == SUBEXP) {
        node->left = lower_subexp(&err, preg, node->left);
        if (node->left)
            node->left->parent = node;
    }
    if (node->right && node->right->token.type == SUBEXP) {
        node->right = lower_subexp(&err, preg, node->right);
        if (node->right)
            node->right->parent = node;
    }

    return err;
}

static bin_tree_t *
lower_subexp(reg_errcode_t *err, regex_t *preg, bin_tree_t *node) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_t *body = node->left;
    bin_tree_t *op, *cls, *tree1, *tree;

    if (preg->no_sub
        /* We do not optimize empty subexpressions, because otherwise we may
       have bad CONCAT nodes with NULL children.  This is obviously not
       very common, so we do not lose much.  An example that triggers
       this case is the sed "script" /\(\)/x.  */
        && node->left != NULL
        && (node->token.opr.idx >= BITSET_WORD_BITS
            || !(dfa->used_bkref_map
                 & ((bitset_word_t) 1 << node->token.opr.idx))))
        return node->left;

    /* Convert the SUBEXP node to the concatenation of an
       OP_OPEN_SUBEXP, the contents, and an OP_CLOSE_SUBEXP.  */
    op = create_tree(dfa, NULL, NULL, OP_OPEN_SUBEXP);
    cls = create_tree(dfa, NULL, NULL, OP_CLOSE_SUBEXP);
    tree1 = body ? create_tree(dfa, body, cls, CONCAT) : cls;
    tree = create_tree(dfa, op, tree1, CONCAT);
    if ((tree == NULL || tree1 == NULL
         || op == NULL || cls == NULL)) {
        *err = REG_ESPACE;
        return NULL;
    }

    op->token.opr.idx = cls->token.opr.idx = node->token.opr.idx;
    op->token.opt_subexp = cls->token.opt_subexp = node->token.opt_subexp;
    return tree;
}

/* Pass 1 in building the NFA: compute FIRST and create unlinked automaton
   nodes.  Requires a postorder visit.  */
static reg_errcode_t
calc_first(void *extra, bin_tree_t *node) {
    re_dfa_t *dfa = (re_dfa_t *) extra;
    if (node->token.type == CONCAT) {
        node->first = node->left->first;
        node->node_idx = node->left->node_idx;
    } else {
        node->first = node;
        node->node_idx = re_dfa_add_node(dfa, node->token);
        if ((node->node_idx == -1))
            return REG_ESPACE;
        if (node->token.type == ANCHOR)
            dfa->nodes[node->node_idx].constraint = node->token.opr.ctx_type;
    }
    return REG_NOERROR;
}

/* Pass 2: compute NEXT on the tree.  Preorder visit.  */
static reg_errcode_t
calc_next(void *extra, bin_tree_t *node) {
    switch (node->token.type) {
        case OP_DUP_ASTERISK:
            node->left->next = node;
            break;
        case CONCAT:
            node->left->next = node->right->first;
            node->right->next = node->next;
            break;
        default:
            if (node->left)
                node->left->next = node->next;
            if (node->right)
                node->right->next = node->next;
            break;
    }
    return REG_NOERROR;
}

/* Pass 3: link all DFA nodes to their NEXT node (any order will do).  */
static reg_errcode_t
link_nfa_nodes(void *extra, bin_tree_t *node) {
    re_dfa_t *dfa = (re_dfa_t *) extra;
    Idx idx = node->node_idx;
    reg_errcode_t err = REG_NOERROR;

    switch (node->token.type) {
        case CONCAT:
            break;

        case END_OF_RE:
            assert(node->next == NULL);
            break;

        case OP_DUP_ASTERISK:
        case OP_ALT: {
            Idx left, right;
            dfa->has_plural_match = 1;
            if (node->left != NULL)
                left = node->left->first->node_idx;
            else
                left = node->next->node_idx;
            if (node->right != NULL)
                right = node->right->first->node_idx;
            else
                right = node->next->node_idx;
            assert(left > -1);
            assert(right > -1);
            err = re_node_set_init_2(dfa->edests + idx, left, right);
        }
            break;

        case ANCHOR:
        case OP_OPEN_SUBEXP:
        case OP_CLOSE_SUBEXP:
            err = re_node_set_init_1(dfa->edests + idx, node->next->node_idx);
            break;

        case OP_BACK_REF:
            dfa->nexts[idx] = node->next->node_idx;
            if (node->token.type == OP_BACK_REF)
                err = re_node_set_init_1(dfa->edests + idx, dfa->nexts[idx]);
            break;

        default:
            assert(!IS_EPSILON_NODE(node->token.type));
            dfa->nexts[idx] = node->next->node_idx;
            break;
    }

    return err;
}

/* Duplicate the epsilon closure of the node ROOT_NODE.
   Note that duplicated nodes have constraint INIT_CONSTRAINT in addition
   to their own constraint.  */

static reg_errcode_t
duplicate_node_closure(re_dfa_t *dfa, Idx top_org_node, Idx top_clone_node,
                       Idx root_node, unsigned int init_constraint) {
    Idx org_node, clone_node;
    bool ok;
    unsigned int constraint = init_constraint;
    for (org_node = top_org_node, clone_node = top_clone_node;;) {
        Idx org_dest, clone_dest;
        if (dfa->nodes[org_node].type == OP_BACK_REF) {
            /* If the back reference epsilon-transit, its destination must
               also have the constraint.  Then duplicate the epsilon closure
               of the destination of the back reference, and store it in
               edests of the back reference.  */
            org_dest = dfa->nexts[org_node];
            re_node_set_empty(dfa->edests + clone_node);
            clone_dest = duplicate_node(dfa, org_dest, constraint);
            if ((clone_dest == -1))
                return REG_ESPACE;
            dfa->nexts[clone_node] = dfa->nexts[org_node];
            ok = re_node_set_insert(dfa->edests + clone_node, clone_dest);
            if ((!ok))
                return REG_ESPACE;
        } else if (dfa->edests[org_node].nelem == 0) {
            /* In case of the node can't epsilon-transit, don't duplicate the
               destination and store the original destination as the
               destination of the node.  */
            dfa->nexts[clone_node] = dfa->nexts[org_node];
            break;
        } else if (dfa->edests[org_node].nelem == 1) {
            /* In case of the node can epsilon-transit, and it has only one
               destination.  */
            org_dest = dfa->edests[org_node].elems[0];
            re_node_set_empty(dfa->edests + clone_node);
            /* If the node is root_node itself, it means the epsilon closure
               has a loop.  Then tie it to the destination of the root_node.  */
            if (org_node == root_node && clone_node != org_node) {
                ok = re_node_set_insert(dfa->edests + clone_node, org_dest);
                if ((!ok))
                    return REG_ESPACE;
                break;
            }
            /* In case the node has another constraint, append it.  */
            constraint |= dfa->nodes[org_node].constraint;
            clone_dest = duplicate_node(dfa, org_dest, constraint);
            if ((clone_dest == -1))
                return REG_ESPACE;
            ok = re_node_set_insert(dfa->edests + clone_node, clone_dest);
            if ((!ok))
                return REG_ESPACE;
        } else /* dfa->edests[org_node].nelem == 2 */
        {
            /* In case of the node can epsilon-transit, and it has two
               destinations. In the bin_tree_t and DFA, that's '|' and '*'.   */
            org_dest = dfa->edests[org_node].elems[0];
            re_node_set_empty(dfa->edests + clone_node);
            /* Search for a duplicated node which satisfies the constraint.  */
            clone_dest = search_duplicated_node(dfa, org_dest, constraint);
            if (clone_dest == -1) {
                /* There is no such duplicated node, create a new one.  */
                reg_errcode_t err;
                clone_dest = duplicate_node(dfa, org_dest, constraint);
                if ((clone_dest == -1))
                    return REG_ESPACE;
                ok = re_node_set_insert(dfa->edests + clone_node, clone_dest);
                if ((!ok))
                    return REG_ESPACE;
                err = duplicate_node_closure(dfa, org_dest, clone_dest,
                                             root_node, constraint);
                if ((err != REG_NOERROR))
                    return err;
            } else {
                /* There is a duplicated node which satisfies the constraint,
               use it to avoid infinite loop.  */
                ok = re_node_set_insert(dfa->edests + clone_node, clone_dest);
                if ((!ok))
                    return REG_ESPACE;
            }

            org_dest = dfa->edests[org_node].elems[1];
            clone_dest = duplicate_node(dfa, org_dest, constraint);
            if ((clone_dest == -1))
                return REG_ESPACE;
            ok = re_node_set_insert(dfa->edests + clone_node, clone_dest);
            if ((!ok))
                return REG_ESPACE;
        }
        org_node = org_dest;
        clone_node = clone_dest;
    }
    return REG_NOERROR;
}

/* Search for a node which is duplicated from the node ORG_NODE, and
   satisfies the constraint CONSTRAINT.  */

static Idx
search_duplicated_node(const re_dfa_t *dfa, Idx org_node,
                       unsigned int constraint) {
    Idx idx;
    for (idx = dfa->nodes_len - 1; dfa->nodes[idx].duplicated && idx > 0; --idx) {
        if (org_node == dfa->org_indices[idx]
            && constraint == dfa->nodes[idx].constraint)
            return idx; /* Found.  */
    }
    return -1; /* Not found.  */
}

/* Duplicate the node whose index is ORG_IDX and set the constraint CONSTRAINT.
   Return the index of the new node, or -1 if insufficient storage is
   available.  */

static Idx
duplicate_node(re_dfa_t *dfa, Idx org_idx, unsigned int constraint) {
    Idx dup_idx = re_dfa_add_node(dfa, dfa->nodes[org_idx]);
    if ((dup_idx != -1)) {
        dfa->nodes[dup_idx].constraint = constraint;
        dfa->nodes[dup_idx].constraint |= dfa->nodes[org_idx].constraint;
        dfa->nodes[dup_idx].duplicated = 1;

        /* Store the index of the original node.  */
        dfa->org_indices[dup_idx] = org_idx;
    }
    return dup_idx;
}

static reg_errcode_t
calc_inveclosure(re_dfa_t *dfa) {
    Idx src, idx;
    bool ok;
    for (idx = 0; idx < dfa->nodes_len; ++idx)
        re_node_set_init_empty(dfa->inveclosures + idx);

    for (src = 0; src < dfa->nodes_len; ++src) {
        Idx *elems = dfa->eclosures[src].elems;
        for (idx = 0; idx < dfa->eclosures[src].nelem; ++idx) {
            ok = re_node_set_insert_last(dfa->inveclosures + elems[idx], src);
            if ((!ok))
                return REG_ESPACE;
        }
    }

    return REG_NOERROR;
}

/* Calculate "eclosure" for all the node in DFA.  */

static reg_errcode_t
calc_eclosure(re_dfa_t *dfa) {
    Idx node_idx;
    bool incomplete;
    incomplete = false;
    /* For each nodes, calculate epsilon closure.  */
    for (node_idx = 0;; ++node_idx) {
        reg_errcode_t err;
        re_node_set eclosure_elem;
        if (node_idx == dfa->nodes_len) {
            if (!incomplete)
                break;
            incomplete = false;
            node_idx = 0;
        }

        /* If we have already calculated, skip it.  */
        if (dfa->eclosures[node_idx].nelem != 0)
            continue;
        /* Calculate epsilon closure of 'node_idx'.  */
        err = calc_eclosure_iter(&eclosure_elem, dfa, node_idx, true);
        if ((err != REG_NOERROR))
            return err;

        if (dfa->eclosures[node_idx].nelem == 0) {
            incomplete = true;
            re_node_set_free(&eclosure_elem);
        }
    }
    return REG_NOERROR;
}

/* Calculate epsilon closure of NODE.  */

static reg_errcode_t
calc_eclosure_iter(re_node_set *new_set, re_dfa_t *dfa, Idx node, bool root) {
    reg_errcode_t err;
    Idx i;
    re_node_set eclosure;
    bool ok;
    bool incomplete = false;
    err = re_node_set_alloc(&eclosure, dfa->edests[node].nelem + 1);
    if ((err != REG_NOERROR))
        return err;

    /* This indicates that we are calculating this node now.
       We reference this value to avoid infinite loop.  */
    dfa->eclosures[node].nelem = -1;

    /* If the current node has constraints, duplicate all nodes
       since they must inherit the constraints.  */
    if (dfa->nodes[node].constraint
        && dfa->edests[node].nelem
        && !dfa->nodes[dfa->edests[node].elems[0]].duplicated) {
        err = duplicate_node_closure(dfa, node, node, node,
                                     dfa->nodes[node].constraint);
        if ((err != REG_NOERROR))
            return err;
    }

    /* Expand each epsilon destination nodes.  */
    if (IS_EPSILON_NODE(dfa->nodes[node].type))
        for (i = 0; i < dfa->edests[node].nelem; ++i) {
            re_node_set eclosure_elem;
            Idx edest = dfa->edests[node].elems[i];
            /* If calculating the epsilon closure of 'edest' is in progress,
               return intermediate result.  */
            if (dfa->eclosures[edest].nelem == -1) {
                incomplete = true;
                continue;
            }
            /* If we haven't calculated the epsilon closure of 'edest' yet,
               calculate now. Otherwise use calculated epsilon closure.  */
            if (dfa->eclosures[edest].nelem == 0) {
                err = calc_eclosure_iter(&eclosure_elem, dfa, edest, false);
                if ((err != REG_NOERROR))
                    return err;
            } else
                eclosure_elem = dfa->eclosures[edest];
            /* Merge the epsilon closure of 'edest'.  */
            err = re_node_set_merge(&eclosure, &eclosure_elem);
            if ((err != REG_NOERROR))
                return err;
            /* If the epsilon closure of 'edest' is incomplete,
               the epsilon closure of this node is also incomplete.  */
            if (dfa->eclosures[edest].nelem == 0) {
                incomplete = true;
                re_node_set_free(&eclosure_elem);
            }
        }

    /* An epsilon closure includes itself.  */
    ok = re_node_set_insert(&eclosure, node);
    if ((!ok))
        return REG_ESPACE;
    if (incomplete && !root)
        dfa->eclosures[node].nelem = 0;
    else
        dfa->eclosures[node] = eclosure;
    *new_set = eclosure;
    return REG_NOERROR;
}

/* Functions for token which are used in the parser.  */

/* Fetch a token from INPUT.
   We must not use this function inside bracket expressions.  */

static void
fetch_token(re_token_t *result, re_string_t *input, reg_syntax_t syntax) {
    re_string_skip_bytes(input, peek_token(result, input, syntax));
}

/* Peek a token from INPUT, and return the length of the token.
   We must not use this function inside bracket expressions.  */

static int
peek_token(re_token_t *token, re_string_t *input, reg_syntax_t syntax) {
    unsigned char c;

    if (re_string_eoi(input)) {
        token->type = END_OF_RE;
        return 0;
    }

    c = re_string_peek_byte(input, 0);
    token->opr.c = c;

    token->word_char = 0;

    if (c == '\\') {
        unsigned char c2;
        if (re_string_cur_idx(input) + 1 >= re_string_length(input)) {
            token->type = BACK_SLASH;
            return 1;
        }

        c2 = re_string_peek_byte_case(input, 1);
        token->opr.c = c2;
        token->type = CHARACTER;

        token->word_char = IS_WORD_CHAR(c2) != 0;

        switch (c2) {
            case '|':
                if (!(syntax & RE_LIMITED_OPS) && !(syntax & RE_NO_BK_VBAR))
                    token->type = OP_ALT;
                break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (!(syntax & RE_NO_BK_REFS)) {
                    token->type = OP_BACK_REF;
                    token->opr.idx = c2 - '1';
                }
                break;
            case '<':
                if (!(syntax & RE_NO_GNU_OPS)) {
                    token->type = ANCHOR;
                    token->opr.ctx_type = WORD_FIRST;
                }
                break;
            case '>':
                if (!(syntax & RE_NO_GNU_OPS)) {
                    token->type = ANCHOR;
                    token->opr.ctx_type = WORD_LAST;
                }
                break;
            case 'b':
                if (!(syntax & RE_NO_GNU_OPS)) {
                    token->type = ANCHOR;
                    token->opr.ctx_type = WORD_DELIM;
                }
                break;
            case 'B':
                if (!(syntax & RE_NO_GNU_OPS)) {
                    token->type = ANCHOR;
                    token->opr.ctx_type = NOT_WORD_DELIM;
                }
                break;
            case 'w':
                if (!(syntax & RE_NO_GNU_OPS))
                    token->type = OP_WORD;
                break;
            case 'W':
                if (!(syntax & RE_NO_GNU_OPS))
                    token->type = OP_NOTWORD;
                break;
            case 's':
                if (!(syntax & RE_NO_GNU_OPS))
                    token->type = OP_SPACE;
                break;
            case 'S':
                if (!(syntax & RE_NO_GNU_OPS))
                    token->type = OP_NOTSPACE;
                break;
            case '`':
                if (!(syntax & RE_NO_GNU_OPS)) {
                    token->type = ANCHOR;
                    token->opr.ctx_type = BUF_FIRST;
                }
                break;
            case '\'':
                if (!(syntax & RE_NO_GNU_OPS)) {
                    token->type = ANCHOR;
                    token->opr.ctx_type = BUF_LAST;
                }
                break;
            case '(':
                if (!(syntax & RE_NO_BK_PARENS))
                    token->type = OP_OPEN_SUBEXP;
                break;
            case ')':
                if (!(syntax & RE_NO_BK_PARENS))
                    token->type = OP_CLOSE_SUBEXP;
                break;
            case '+':
                if (!(syntax & RE_LIMITED_OPS) && (syntax & RE_BK_PLUS_QM))
                    token->type = OP_DUP_PLUS;
                break;
            case '?':
                if (!(syntax & RE_LIMITED_OPS) && (syntax & RE_BK_PLUS_QM))
                    token->type = OP_DUP_QUESTION;
                break;
            case '{':
                if ((syntax & RE_INTERVALS) && (!(syntax & RE_NO_BK_BRACES)))
                    token->type = OP_OPEN_DUP_NUM;
                break;
            case '}':
                if ((syntax & RE_INTERVALS) && (!(syntax & RE_NO_BK_BRACES)))
                    token->type = OP_CLOSE_DUP_NUM;
                break;
            default:
                break;
        }
        return 2;
    }

    token->type = CHARACTER;

    token->word_char = IS_WORD_CHAR(token->opr.c);

    switch (c) {
        case '\n':
            if (syntax & RE_NEWLINE_ALT)
                token->type = OP_ALT;
            break;
        case '|':
            if (!(syntax & RE_LIMITED_OPS) && (syntax & RE_NO_BK_VBAR))
                token->type = OP_ALT;
            break;
        case '*':
            token->type = OP_DUP_ASTERISK;
            break;
        case '+':
            if (!(syntax & RE_LIMITED_OPS) && !(syntax & RE_BK_PLUS_QM))
                token->type = OP_DUP_PLUS;
            break;
        case '?':
            if (!(syntax & RE_LIMITED_OPS) && !(syntax & RE_BK_PLUS_QM))
                token->type = OP_DUP_QUESTION;
            break;
        case '{':
            if ((syntax & RE_INTERVALS) && (syntax & RE_NO_BK_BRACES))
                token->type = OP_OPEN_DUP_NUM;
            break;
        case '}':
            if ((syntax & RE_INTERVALS) && (syntax & RE_NO_BK_BRACES))
                token->type = OP_CLOSE_DUP_NUM;
            break;
        case '(':
            if (syntax & RE_NO_BK_PARENS)
                token->type = OP_OPEN_SUBEXP;
            break;
        case ')':
            if (syntax & RE_NO_BK_PARENS)
                token->type = OP_CLOSE_SUBEXP;
            break;
        case '[':
            token->type = OP_OPEN_BRACKET;
            break;
        case '.':
            token->type = OP_PERIOD;
            break;
        case '^':
            if (!(syntax & (RE_CONTEXT_INDEP_ANCHORS | RE_CARET_ANCHORS_HERE)) &&
                re_string_cur_idx(input) != 0) {
                char prev = re_string_peek_byte(input, -1);
                if (!(syntax & RE_NEWLINE_ALT) || prev != '\n')
                    break;
            }
            token->type = ANCHOR;
            token->opr.ctx_type = LINE_FIRST;
            break;
        case '$':
            if (!(syntax & RE_CONTEXT_INDEP_ANCHORS) &&
                re_string_cur_idx(input) + 1 != re_string_length(input)) {
                re_token_t next;
                re_string_skip_bytes(input, 1);
                peek_token(&next, input, syntax);
                re_string_skip_bytes(input, -1);
                if (next.type != OP_ALT && next.type != OP_CLOSE_SUBEXP)
                    break;
            }
            token->type = ANCHOR;
            token->opr.ctx_type = LINE_LAST;
            break;
        default:
            break;
    }
    return 1;
}

/* Peek a token from INPUT, and return the length of the token.
   We must not use this function out of bracket expressions.  */

static int
peek_token_bracket(re_token_t *token, re_string_t *input, reg_syntax_t syntax) {
    unsigned char c;
    if (re_string_eoi(input)) {
        token->type = END_OF_RE;
        return 0;
    }
    c = re_string_peek_byte(input, 0);
    token->opr.c = c;

    if (c == '\\' && (syntax & RE_BACKSLASH_ESCAPE_IN_LISTS)
        && re_string_cur_idx(input) + 1 < re_string_length(input)) {
        /* In this case, '\' escape a character.  */
        unsigned char c2;
        re_string_skip_bytes(input, 1);
        c2 = re_string_peek_byte(input, 0);
        token->opr.c = c2;
        token->type = CHARACTER;
        return 1;
    }
    if (c == '[') /* '[' is a special char in a bracket exps.  */
    {
        unsigned char c2;
        int token_len;
        if (re_string_cur_idx(input) + 1 < re_string_length(input))
            c2 = re_string_peek_byte(input, 1);
        else
            c2 = 0;
        token->opr.c = c2;
        token_len = 2;
        switch (c2) {
            case '.':
                token->type = OP_OPEN_COLL_ELEM;
                break;

            case '=':
                token->type = OP_OPEN_EQUIV_CLASS;
                break;

            case ':':
                if (syntax & RE_CHAR_CLASSES) {
                    token->type = OP_OPEN_CHAR_CLASS;
                    break;
                }
                FALLTHROUGH;
            default:
                token->type = CHARACTER;
                token->opr.c = c;
                token_len = 1;
                break;
        }
        return token_len;
    }
    switch (c) {
        case '-':
            token->type = OP_CHARSET_RANGE;
            break;
        case ']':
            token->type = OP_CLOSE_BRACKET;
            break;
        case '^':
            token->type = OP_NON_MATCH_LIST;
            break;
        default:
            token->type = CHARACTER;
    }
    return 1;
}

/* Functions for parser.  */

/* Entry point of the parser.
   Parse the regular expression REGEXP and return the structure tree.
   If an error occurs, ERR is set by error code, and return NULL.
   This function build the following tree, from regular expression <reg_exp>:
	   CAT
	   / \
	  /   \
   <reg_exp>  EOR

   CAT means concatenation.
   EOR means end of regular expression.  */

static bin_tree_t *
parse(re_string_t *regexp, regex_t *preg, reg_syntax_t syntax,
      reg_errcode_t *err) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_t *tree, *eor, *root;
    re_token_t current_token;
    dfa->syntax = syntax;
    fetch_token(&current_token, regexp, syntax | RE_CARET_ANCHORS_HERE);
    tree = parse_reg_exp(regexp, preg, &current_token, syntax, 0, err);
    if ((*err != REG_NOERROR && tree == NULL))
        return NULL;
    eor = create_tree(dfa, NULL, NULL, END_OF_RE);
    if (tree != NULL)
        root = create_tree(dfa, tree, eor, CONCAT);
    else
        root = eor;
    if ((eor == NULL || root == NULL)) {
        *err = REG_ESPACE;
        return NULL;
    }
    return root;
}

/* This function build the following tree, from regular expression
   <branch1>|<branch2>:
	   ALT
	   / \
	  /   \
   <branch1> <branch2>

   ALT means alternative, which represents the operator '|'.  */

static bin_tree_t *
parse_reg_exp(re_string_t *regexp, regex_t *preg, re_token_t *token,
              reg_syntax_t syntax, Idx nest, reg_errcode_t *err) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_t *tree, *branch = NULL;
    bitset_word_t initial_bkref_map = dfa->completed_bkref_map;
    tree = parse_branch(regexp, preg, token, syntax, nest, err);
    if ((*err != REG_NOERROR && tree == NULL))
        return NULL;

    while (token->type == OP_ALT) {
        fetch_token(token, regexp, syntax | RE_CARET_ANCHORS_HERE);
        if (token->type != OP_ALT && token->type != END_OF_RE
            && (nest == 0 || token->type != OP_CLOSE_SUBEXP)) {
            bitset_word_t accumulated_bkref_map = dfa->completed_bkref_map;
            dfa->completed_bkref_map = initial_bkref_map;
            branch = parse_branch(regexp, preg, token, syntax, nest, err);
            if ((*err != REG_NOERROR && branch == NULL)) {
                if (tree != NULL)
                    postorder(tree, free_tree, NULL);
                return NULL;
            }
            dfa->completed_bkref_map |= accumulated_bkref_map;
        } else
            branch = NULL;
        tree = create_tree(dfa, tree, branch, OP_ALT);
        if ((tree == NULL)) {
            *err = REG_ESPACE;
            return NULL;
        }
    }
    return tree;
}

/* This function build the following tree, from regular expression
   <exp1><exp2>:
	CAT
	/ \
       /   \
   <exp1> <exp2>

   CAT means concatenation.  */

static bin_tree_t *
parse_branch(re_string_t *regexp, regex_t *preg, re_token_t *token,
             reg_syntax_t syntax, Idx nest, reg_errcode_t *err) {
    bin_tree_t *tree, *expr;
    re_dfa_t *dfa = preg->buffer;
    tree = parse_expression(regexp, preg, token, syntax, nest, err);
    if ((*err != REG_NOERROR && tree == NULL))
        return NULL;

    while (token->type != OP_ALT && token->type != END_OF_RE
           && (nest == 0 || token->type != OP_CLOSE_SUBEXP)) {
        expr = parse_expression(regexp, preg, token, syntax, nest, err);
        if ((*err != REG_NOERROR && expr == NULL)) {
            if (tree != NULL)
                postorder(tree, free_tree, NULL);
            return NULL;
        }
        if (tree != NULL && expr != NULL) {
            bin_tree_t *newtree = create_tree(dfa, tree, expr, CONCAT);
            if (newtree == NULL) {
                postorder(expr, free_tree, NULL);
                postorder(tree, free_tree, NULL);
                *err = REG_ESPACE;
                return NULL;
            }
            tree = newtree;
        } else if (tree == NULL)
            tree = expr;
        /* Otherwise expr == NULL, we don't need to create new tree.  */
    }
    return tree;
}

/* This function build the following tree, from regular expression a*:
	 *
	 |
	 a
*/

static bin_tree_t *
parse_expression(re_string_t *regexp, regex_t *preg, re_token_t *token,
                 reg_syntax_t syntax, Idx nest, reg_errcode_t *err) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_t *tree;
    switch (token->type) {
        case CHARACTER:
            tree = create_token_tree(dfa, NULL, NULL, token);
            if ((tree == NULL)) {
                *err = REG_ESPACE;
                return NULL;
            }
            break;

        case OP_OPEN_SUBEXP:
            tree = parse_sub_exp(regexp, preg, token, syntax, nest + 1, err);
            if ((*err != REG_NOERROR && tree == NULL))
                return NULL;
            break;

        case OP_OPEN_BRACKET:
            tree = parse_bracket_exp(regexp, dfa, token, syntax, err);
            if ((*err != REG_NOERROR && tree == NULL))
                return NULL;
            break;

        case OP_BACK_REF:
            if (!(dfa->completed_bkref_map & (1 << token->opr.idx))) {
                *err = REG_ESUBREG;
                return NULL;
            }
            dfa->used_bkref_map |= 1 << token->opr.idx;
            tree = create_token_tree(dfa, NULL, NULL, token);
            if ((tree == NULL)) {
                *err = REG_ESPACE;
                return NULL;
            }
            ++dfa->nbackref;
            dfa->has_mb_node = 1;
            break;

        case OP_OPEN_DUP_NUM:
            if (syntax & RE_CONTEXT_INVALID_DUP) {
                *err = REG_BADRPT;
                return NULL;
            }
            FALLTHROUGH;
        case OP_DUP_ASTERISK:
        case OP_DUP_PLUS:
        case OP_DUP_QUESTION:
            if (syntax & RE_CONTEXT_INVALID_OPS) {
                *err = REG_BADRPT;
                return NULL;
            } else if (syntax & RE_CONTEXT_INDEP_OPS) {
                fetch_token(token, regexp, syntax);
                return parse_expression(regexp, preg, token, syntax, nest, err);
            }
            FALLTHROUGH;
        case OP_CLOSE_SUBEXP:
            if ((token->type == OP_CLOSE_SUBEXP) &&
                !(syntax & RE_UNMATCHED_RIGHT_PAREN_ORD)) {
                *err = REG_ERPAREN;
                return NULL;
            }
            FALLTHROUGH;
        case OP_CLOSE_DUP_NUM:
            /* We treat it as a normal character.  */

            /* Then we can these characters as normal characters.  */
            token->type = CHARACTER;
            /* mb_partial and word_char bits should be initialized already
           by peek_token.  */
            tree = create_token_tree(dfa, NULL, NULL, token);
            if ((tree == NULL)) {
                *err = REG_ESPACE;
                return NULL;
            }
            break;

        case ANCHOR:
            if ((token->opr.ctx_type
                 & (WORD_DELIM | NOT_WORD_DELIM | WORD_FIRST | WORD_LAST))
                && dfa->word_ops_used == 0)
                init_word_char(dfa);
            if (token->opr.ctx_type == WORD_DELIM
                || token->opr.ctx_type == NOT_WORD_DELIM) {
                bin_tree_t *tree_first, *tree_last;
                if (token->opr.ctx_type == WORD_DELIM) {
                    token->opr.ctx_type = WORD_FIRST;
                    tree_first = create_token_tree(dfa, NULL, NULL, token);
                    token->opr.ctx_type = WORD_LAST;
                } else {
                    token->opr.ctx_type = INSIDE_WORD;
                    tree_first = create_token_tree(dfa, NULL, NULL, token);
                    token->opr.ctx_type = INSIDE_NOTWORD;
                }
                tree_last = create_token_tree(dfa, NULL, NULL, token);
                tree = create_tree(dfa, tree_first, tree_last, OP_ALT);
                if ((tree_first == NULL || tree_last == NULL
                     || tree == NULL)) {
                    *err = REG_ESPACE;
                    return NULL;
                }
            } else {
                tree = create_token_tree(dfa, NULL, NULL, token);
                if ((tree == NULL)) {
                    *err = REG_ESPACE;
                    return NULL;
                }
            }
            /* We must return here, since ANCHORs can't be followed
           by repetition operators.
           eg. RE"^*" is invalid or "<ANCHOR(^)><CHAR(*)>",
               it must not be "<ANCHOR(^)><REPEAT(*)>".  */
            fetch_token(token, regexp, syntax);
            return tree;

        case OP_PERIOD:
            tree = create_token_tree(dfa, NULL, NULL, token);
            if ((tree == NULL)) {
                *err = REG_ESPACE;
                return NULL;
            }
            if (dfa->mb_cur_max > 1)
                dfa->has_mb_node = 1;
            break;

        case OP_WORD:
        case OP_NOTWORD:
            tree = build_charclass_op(dfa, regexp->trans,
                                      "alnum",
                                      "_",
                                      token->type == OP_NOTWORD, err);
            if ((*err != REG_NOERROR && tree == NULL))
                return NULL;
            break;

        case OP_SPACE:
        case OP_NOTSPACE:
            tree = build_charclass_op(dfa, regexp->trans,
                                      "space",
                                      "",
                                      token->type == OP_NOTSPACE, err);
            if ((*err != REG_NOERROR && tree == NULL))
                return NULL;
            break;

        case OP_ALT:
        case END_OF_RE:
            return NULL;

        case BACK_SLASH:
            *err = REG_EESCAPE;
            return NULL;

        default:
            /* Must not happen?  */
            return NULL;
    }
    fetch_token(token, regexp, syntax);

    while (token->type == OP_DUP_ASTERISK || token->type == OP_DUP_PLUS
           || token->type == OP_DUP_QUESTION || token->type == OP_OPEN_DUP_NUM) {
        bin_tree_t *dup_tree = parse_dup_op(tree, regexp, dfa, token,
                                            syntax, err);
        if ((*err != REG_NOERROR && dup_tree == NULL)) {
            if (tree != NULL)
                postorder(tree, free_tree, NULL);
            return NULL;
        }
        tree = dup_tree;
        /* In BRE consecutive duplications are not allowed.  */
        if ((syntax & RE_CONTEXT_INVALID_DUP)
            && (token->type == OP_DUP_ASTERISK
                || token->type == OP_OPEN_DUP_NUM)) {
            if (tree != NULL)
                postorder(tree, free_tree, NULL);
            *err = REG_BADRPT;
            return NULL;
        }
    }

    return tree;
}

/* This function build the following tree, from regular expression
   (<reg_exp>):
	 SUBEXP
	    |
	<reg_exp>
*/

static bin_tree_t *
parse_sub_exp(re_string_t *regexp, regex_t *preg, re_token_t *token,
              reg_syntax_t syntax, Idx nest, reg_errcode_t *err) {
    re_dfa_t *dfa = preg->buffer;
    bin_tree_t *tree;
    size_t cur_nsub;
    cur_nsub = preg->re_nsub++;

    fetch_token(token, regexp, syntax | RE_CARET_ANCHORS_HERE);

    /* The subexpression may be a null string.  */
    if (token->type == OP_CLOSE_SUBEXP)
        tree = NULL;
    else {
        tree = parse_reg_exp(regexp, preg, token, syntax, nest, err);
        if ((*err == REG_NOERROR
             && token->type != OP_CLOSE_SUBEXP)) {
            if (tree != NULL)
                postorder(tree, free_tree, NULL);
            *err = REG_EPAREN;
        }
        if ((*err != REG_NOERROR))
            return NULL;
    }

    if (cur_nsub <= '9' - '1')
        dfa->completed_bkref_map |= 1 << cur_nsub;

    tree = create_tree(dfa, tree, NULL, SUBEXP);
    if ((tree == NULL)) {
        *err = REG_ESPACE;
        return NULL;
    }
    tree->token.opr.idx = cur_nsub;
    return tree;
}

/* This function parse repetition operators like "*", "+", "{1,3}" etc.  */

static bin_tree_t *
parse_dup_op(bin_tree_t *elem, re_string_t *regexp, re_dfa_t *dfa,
             re_token_t *token, reg_syntax_t syntax, reg_errcode_t *err) {
    bin_tree_t *tree = NULL, *old_tree = NULL;
    Idx i, start, end, start_idx = re_string_cur_idx(regexp);
    re_token_t start_token = *token;

    if (token->type == OP_OPEN_DUP_NUM) {
        end = 0;
        start = fetch_number(regexp, token, syntax);
        if (start == -1) {
            if (token->type == CHARACTER && token->opr.c == ',')
                start = 0; /* We treat "{,m}" as "{0,m}".  */
            else {
                *err = REG_BADBR; /* <re>{} is invalid.  */
                return NULL;
            }
        }
        if ((start != -2)) {
            /* We treat "{n}" as "{n,n}".  */
            end = ((token->type == OP_CLOSE_DUP_NUM) ? start
                                                     : ((token->type == CHARACTER && token->opr.c == ',')
                                                        ? fetch_number(regexp, token, syntax) : -2));
        }
        if ((start == -2 || end == -2)) {
            /* Invalid sequence.  */
            if ((!(syntax & RE_INVALID_INTERVAL_ORD))) {
                if (token->type == END_OF_RE)
                    *err = REG_EBRACE;
                else
                    *err = REG_BADBR;

                return NULL;
            }

            /* If the syntax bit is set, rollback.  */
            re_string_set_index(regexp, start_idx);
            *token = start_token;
            token->type = CHARACTER;
            /* mb_partial and word_char bits should be already initialized by
               peek_token.  */
            return elem;
        }

        if (((end != -1 && start > end)
             || token->type != OP_CLOSE_DUP_NUM)) {
            /* First number greater than second.  */
            *err = REG_BADBR;
            return NULL;
        }

        if ((RE_DUP_MAX < (end == -1 ? start : end))) {
            *err = REG_ESIZE;
            return NULL;
        }
    } else {
        start = (token->type == OP_DUP_PLUS) ? 1 : 0;
        end = (token->type == OP_DUP_QUESTION) ? 1 : -1;
    }

    fetch_token(token, regexp, syntax);

    if ((elem == NULL))
        return NULL;
    if ((start == 0 && end == 0)) {
        postorder(elem, free_tree, NULL);
        return NULL;
    }

    /* Extract "<re>{n,m}" to "<re><re>...<re><re>{0,<m-n>}".  */
    if ((start > 0)) {
        tree = elem;
        for (i = 2; i <= start; ++i) {
            elem = duplicate_tree(elem, dfa);
            tree = create_tree(dfa, tree, elem, CONCAT);
            if ((elem == NULL || tree == NULL))
                goto parse_dup_op_espace;
        }

        if (start == end)
            return tree;

        /* Duplicate ELEM before it is marked optional.  */
        elem = duplicate_tree(elem, dfa);
        if ((elem == NULL))
            goto parse_dup_op_espace;
        old_tree = tree;
    } else
        old_tree = NULL;

    if (elem->token.type == SUBEXP) {
        uintptr_t subidx = elem->token.opr.idx;
        postorder(elem, mark_opt_subexp, (void *) subidx);
    }

    tree = create_tree(dfa, elem, NULL,
                       (end == -1 ? OP_DUP_ASTERISK : OP_ALT));
    if ((tree == NULL))
        goto parse_dup_op_espace;

    /* This loop is actually executed only when end != -1,
       to rewrite <re>{0,n} as (<re>(<re>...<re>?)?)?...  We have
       already created the start+1-th copy.  */
    if (TYPE_SIGNED(Idx) || end != -1)
        for (i = start + 2; i <= end; ++i) {
            elem = duplicate_tree(elem, dfa);
            tree = create_tree(dfa, tree, elem, CONCAT);
            if ((elem == NULL || tree == NULL))
                goto parse_dup_op_espace;

            tree = create_tree(dfa, tree, NULL, OP_ALT);
            if ((tree == NULL))
                goto parse_dup_op_espace;
        }

    if (old_tree)
        tree = create_tree(dfa, old_tree, tree, CONCAT);

    return tree;

    parse_dup_op_espace:
    *err = REG_ESPACE;
    return NULL;
}

/* Size of the names for collating symbol/equivalence_class/character_class.
   I'm not sure, but maybe enough.  */
#define BRACKET_NAME_BUF_SIZE 32

/* Local function for parse_bracket_exp only used in case of NOT _LIBC.
   Build the range expression which starts from START_ELEM, and ends
   at END_ELEM.  The result are written to MBCSET and SBCSET.
   RANGE_ALLOC is the allocated size of mbcset->range_starts, and
   mbcset->range_ends, is a pointer argument since we may
   update it.  */

static reg_errcode_t
build_range_exp(const reg_syntax_t syntax,
                bitset_t sbcset,
                const bracket_elem_t *start_elem,
                const bracket_elem_t *end_elem) {
    unsigned int start_ch, end_ch;
    /* Equivalence Classes and Character Classes can't be a range start/end.  */
    if ((start_elem->type == EQUIV_CLASS
         || start_elem->type == CHAR_CLASS
         || end_elem->type == EQUIV_CLASS
         || end_elem->type == CHAR_CLASS))
        return REG_ERANGE;

    /* We can handle no multi character collating elements without libc
       support.  */
    if (((start_elem->type == COLL_SYM
          && strlen((char *) start_elem->opr.name) > 1)
         || (end_elem->type == COLL_SYM
             && strlen((char *) end_elem->opr.name) > 1)))
        return REG_ECOLLATE;

    {
        unsigned int ch;
        start_ch = ((start_elem->type == SB_CHAR) ? start_elem->opr.ch
                                                  : ((start_elem->type == COLL_SYM) ? start_elem->opr.name[0]
                                                                                    : 0));
        end_ch = ((end_elem->type == SB_CHAR) ? end_elem->opr.ch
                                              : ((end_elem->type == COLL_SYM) ? end_elem->opr.name[0]
                                                                              : 0));
        if (start_ch > end_ch)
            return REG_ERANGE;
        /* Build the table for single byte characters.  */
        for (ch = 0; ch < SBC_MAX; ++ch)
            if (start_ch <= ch && ch <= end_ch)
                bitset_set(sbcset, ch);
    }
    return REG_NOERROR;
}

/* Helper function for parse_bracket_exp only used in case of NOT _LIBC..
   Build the collating element which is represented by NAME.
   The result are written to MBCSET and SBCSET.
   COLL_SYM_ALLOC is the allocated size of mbcset->coll_sym, is a
   pointer argument since we may update it.  */

static reg_errcode_t
build_collating_symbol(bitset_t sbcset, const unsigned char *name) {
    size_t name_len = strlen((const char *) name);
    if ((name_len != 1))
        return REG_ECOLLATE;
    else {
        bitset_set(sbcset, name[0]);
        return REG_NOERROR;
    }
}


/* This function parse bracket expression like "[abc]", "[a-c]",
   "[[.a-a.]]" etc.  */

static bin_tree_t *
parse_bracket_exp(re_string_t *regexp, re_dfa_t *dfa, re_token_t *token,
                  reg_syntax_t syntax, reg_errcode_t *err) {
    re_token_t br_token;
    re_bitset_ptr_t sbcset;
    bool non_match = false;
    bin_tree_t *work_tree;
    int token_len;
    bool first_round = true;
    sbcset = (re_bitset_ptr_t) calloc(sizeof(bitset_t), 1);
    if ((sbcset == NULL)) {
        re_free(sbcset);
        *err = REG_ESPACE;
        return NULL;
    }

    token_len = peek_token_bracket(token, regexp, syntax);
    if ((token->type == END_OF_RE)) {
        *err = REG_BADPAT;
        goto parse_bracket_exp_free_return;
    }
    if (token->type == OP_NON_MATCH_LIST) {
        non_match = true;
        if (syntax & RE_HAT_LISTS_NOT_NEWLINE)
            bitset_set(sbcset, '\n');
        re_string_skip_bytes(regexp, token_len); /* Skip a token.  */
        token_len = peek_token_bracket(token, regexp, syntax);
        if ((token->type == END_OF_RE)) {
            *err = REG_BADPAT;
            goto parse_bracket_exp_free_return;
        }
    }

    /* We treat the first ']' as a normal character.  */
    if (token->type == OP_CLOSE_BRACKET)
        token->type = CHARACTER;

    while (1) {
        bracket_elem_t start_elem, end_elem;
        unsigned char start_name_buf[BRACKET_NAME_BUF_SIZE];
        unsigned char end_name_buf[BRACKET_NAME_BUF_SIZE];
        reg_errcode_t ret;
        int token_len2 = 0;
        bool is_range_exp = false;
        re_token_t token2;

        start_elem.opr.name = start_name_buf;
        start_elem.type = COLL_SYM;
        ret = parse_bracket_element(&start_elem, regexp, token, token_len, dfa,
                                    syntax, first_round);
        if ((ret != REG_NOERROR)) {
            *err = ret;
            goto parse_bracket_exp_free_return;
        }
        first_round = false;

        /* Get information about the next token.  We need it in any case.  */
        token_len = peek_token_bracket(token, regexp, syntax);

        /* Do not check for ranges if we know they are not allowed.  */
        if (start_elem.type != CHAR_CLASS && start_elem.type != EQUIV_CLASS) {
            if ((token->type == END_OF_RE)) {
                *err = REG_EBRACK;
                goto parse_bracket_exp_free_return;
            }
            if (token->type == OP_CHARSET_RANGE) {
                re_string_skip_bytes(regexp, token_len); /* Skip '-'.  */
                token_len2 = peek_token_bracket(&token2, regexp, syntax);
                if ((token2.type == END_OF_RE)) {
                    *err = REG_EBRACK;
                    goto parse_bracket_exp_free_return;
                }
                if (token2.type == OP_CLOSE_BRACKET) {
                    /* We treat the last '-' as a normal character.  */
                    re_string_skip_bytes(regexp, -token_len);
                    token->type = CHARACTER;
                } else
                    is_range_exp = true;
            }
        }

        if (is_range_exp == true) {
            end_elem.opr.name = end_name_buf;
            end_elem.type = COLL_SYM;
            ret = parse_bracket_element(&end_elem, regexp, &token2, token_len2,
                                        dfa, syntax, true);
            if ((ret != REG_NOERROR)) {
                *err = ret;
                goto parse_bracket_exp_free_return;
            }

            token_len = peek_token_bracket(token, regexp, syntax);

            *err = build_range_exp(syntax, sbcset, &start_elem, &end_elem);

            if ((*err != REG_NOERROR))
                goto parse_bracket_exp_free_return;
        } else {
            switch (start_elem.type) {
                case SB_CHAR:
                    bitset_set(sbcset, start_elem.opr.ch);
                    break;
                case EQUIV_CLASS:
                    *err = build_equiv_class(sbcset,
                                             start_elem.opr.name);
                    if ((*err != REG_NOERROR))
                        goto parse_bracket_exp_free_return;
                    break;
                case COLL_SYM:
                    *err = build_collating_symbol(sbcset,
                                                  start_elem.opr.name);
                    if ((*err != REG_NOERROR))
                        goto parse_bracket_exp_free_return;
                    break;
                case CHAR_CLASS:
                    *err = build_charclass(regexp->trans, sbcset,
                                           (const char *) start_elem.opr.name,
                                           syntax);
                    if ((*err != REG_NOERROR))
                        goto parse_bracket_exp_free_return;
                    break;
                default:
                    assert(0);
                    break;
            }
        }
        if ((token->type == END_OF_RE)) {
            *err = REG_EBRACK;
            goto parse_bracket_exp_free_return;
        }
        if (token->type == OP_CLOSE_BRACKET)
            break;
    }

    re_string_skip_bytes(regexp, token_len); /* Skip a token.  */

    /* If it is non-matching list.  */
    if (non_match)
        bitset_not(sbcset);

    {
        /* Build a tree for simple bracket.  */
        br_token.type = SIMPLE_BRACKET;
        br_token.opr.sbcset = sbcset;
        work_tree = create_token_tree(dfa, NULL, NULL, &br_token);
        if ((work_tree == NULL))
            goto parse_bracket_exp_espace;
    }
    return work_tree;

    parse_bracket_exp_espace:
    *err = REG_ESPACE;
    parse_bracket_exp_free_return:
    re_free(sbcset);
    return NULL;
}

/* Parse an element in the bracket expression.  */

static reg_errcode_t
parse_bracket_element(bracket_elem_t *elem, re_string_t *regexp,
                      re_token_t *token, int token_len, re_dfa_t *dfa,
                      reg_syntax_t syntax, bool accept_hyphen) {
    re_string_skip_bytes(regexp, token_len); /* Skip a token.  */
    if (token->type == OP_OPEN_COLL_ELEM || token->type == OP_OPEN_CHAR_CLASS
        || token->type == OP_OPEN_EQUIV_CLASS)
        return parse_bracket_symbol(elem, regexp, token);
    if ((token->type == OP_CHARSET_RANGE) && !accept_hyphen) {
        /* A '-' must only appear as anything but a range indicator before
       the closing bracket.  Everything else is an error.  */
        re_token_t token2;
        (void) peek_token_bracket(&token2, regexp, syntax);
        if (token2.type != OP_CLOSE_BRACKET)
            /* The actual error value is not standardized since this whole
               case is undefined.  But ERANGE makes good sense.  */
            return REG_ERANGE;
    }
    elem->type = SB_CHAR;
    elem->opr.ch = token->opr.c;
    return REG_NOERROR;
}

/* Parse a bracket symbol in the bracket expression.  Bracket symbols are
   such as [:<character_class>:], [.<collating_element>.], and
   [=<equivalent_class>=].  */

static reg_errcode_t
parse_bracket_symbol(bracket_elem_t *elem, re_string_t *regexp,
                     re_token_t *token) {
    unsigned char ch, delim = token->opr.c;
    int i = 0;
    if (re_string_eoi(regexp))
        return REG_EBRACK;
    for (;; ++i) {
        if (i >= BRACKET_NAME_BUF_SIZE)
            return REG_EBRACK;
        if (token->type == OP_OPEN_CHAR_CLASS)
            ch = re_string_fetch_byte_case(regexp);
        else
            ch = re_string_fetch_byte(regexp);
        if (re_string_eoi(regexp))
            return REG_EBRACK;
        if (ch == delim && re_string_peek_byte(regexp, 0) == ']')
            break;
        elem->opr.name[i] = ch;
    }
    re_string_skip_bytes(regexp, 1);
    elem->opr.name[i] = '\0';
    switch (token->type) {
        case OP_OPEN_COLL_ELEM:
            elem->type = COLL_SYM;
            break;
        case OP_OPEN_EQUIV_CLASS:
            elem->type = EQUIV_CLASS;
            break;
        case OP_OPEN_CHAR_CLASS:
            elem->type = CHAR_CLASS;
            break;
        default:
            break;
    }
    return REG_NOERROR;
}

/* Helper function for parse_bracket_exp.
   Build the equivalence class which is represented by NAME.
   The result are written to MBCSET and SBCSET.
   EQUIV_CLASS_ALLOC is the allocated size of mbcset->equiv_classes,
   is a pointer argument since we may update it.  */

static reg_errcode_t
build_equiv_class(bitset_t sbcset, const unsigned char *name) {
    {
        if ((strlen((const char *) name) != 1))
            return REG_ECOLLATE;
        bitset_set(sbcset, *name);
    }
    return REG_NOERROR;
}

/* Helper function for parse_bracket_exp.
   Build the character class which is represented by NAME.
   The result are written to MBCSET and SBCSET.
   CHAR_CLASS_ALLOC is the allocated size of mbcset->char_classes,
   is a pointer argument since we may update it.  */

static reg_errcode_t
build_charclass(RE_TRANSLATE_TYPE trans, bitset_t sbcset,
                const char *class_name, reg_syntax_t syntax) {
    int i;
    const char *name = class_name;

    /* In case of REG_ICASE "upper" and "lower" match the both of
       upper and lower cases.  */
    if ((syntax & RE_ICASE)
        && (strcmp(name, "upper") == 0 || strcmp(name, "lower") == 0))
        name = "alpha";

#define BUILD_CHARCLASS_LOOP(ctype_func)    \
  do {                        \
    if ( (trans != NULL))            \
      {                        \
    for (i = 0; i < SBC_MAX; ++i)        \
      if (ctype_func (i))            \
        bitset_set (sbcset, trans[i]);    \
      }                        \
    else                    \
      {                        \
    for (i = 0; i < SBC_MAX; ++i)        \
      if (ctype_func (i))            \
        bitset_set (sbcset, i);        \
      }                        \
  } while (0)

    if (strcmp(name, "alnum") == 0)
        BUILD_CHARCLASS_LOOP (isalnum);
    else if (strcmp(name, "cntrl") == 0)
        BUILD_CHARCLASS_LOOP (iscntrl);
    else if (strcmp(name, "lower") == 0)
        BUILD_CHARCLASS_LOOP (islower);
    else if (strcmp(name, "space") == 0)
        BUILD_CHARCLASS_LOOP (isspace);
    else if (strcmp(name, "alpha") == 0)
        BUILD_CHARCLASS_LOOP (isalpha);
    else if (strcmp(name, "digit") == 0)
        BUILD_CHARCLASS_LOOP (isdigit);
    else if (strcmp(name, "print") == 0)
        BUILD_CHARCLASS_LOOP (isprint);
    else if (strcmp(name, "upper") == 0)
        BUILD_CHARCLASS_LOOP (isupper);
    else if (strcmp(name, "blank") == 0)
        BUILD_CHARCLASS_LOOP (isblank);
    else if (strcmp(name, "graph") == 0)
        BUILD_CHARCLASS_LOOP (isgraph);
    else if (strcmp(name, "punct") == 0)
        BUILD_CHARCLASS_LOOP (ispunct);
    else if (strcmp(name, "xdigit") == 0)
        BUILD_CHARCLASS_LOOP (isxdigit);
    else
        return REG_ECTYPE;

    return REG_NOERROR;
}

static bin_tree_t *
build_charclass_op(re_dfa_t *dfa, RE_TRANSLATE_TYPE trans,
                   const char *class_name,
                   const char *extra, bool non_match,
                   reg_errcode_t *err) {
    re_bitset_ptr_t sbcset;
    reg_errcode_t ret;
    re_token_t br_token;
    bin_tree_t *tree;

    sbcset = (re_bitset_ptr_t) calloc(sizeof(bitset_t), 1);
    if ((sbcset == NULL)) {
        *err = REG_ESPACE;
        return NULL;
    }

    /* We don't care the syntax in this case.  */
    ret = build_charclass(trans, sbcset,
                          class_name, 0);

    if ((ret != REG_NOERROR)) {
        re_free(sbcset);
        *err = ret;
        return NULL;
    }
    /* \w match '_' also.  */
    for (; *extra; extra++)
        bitset_set(sbcset, *extra);

    /* If it is non-matching list.  */
    if (non_match)
        bitset_not(sbcset);

    /* Build a tree for simple bracket.  */
    br_token.type = SIMPLE_BRACKET;
    br_token.opr.sbcset = sbcset;
    tree = create_token_tree(dfa, NULL, NULL, &br_token);
    if ((tree == NULL))
        goto build_word_op_espace;

    return tree;

    build_word_op_espace:
    re_free(sbcset);
    *err = REG_ESPACE;
    return NULL;
}

/* This is intended for the expressions like "a{1,3}".
   Fetch a number from 'input', and return the number.
   Return -1 if the number field is empty like "{,1}".
   Return RE_DUP_MAX + 1 if the number field is too large.
   Return -2 if an error occurred.  */

static Idx
fetch_number(re_string_t *input, re_token_t *token, reg_syntax_t syntax) {
    Idx num = -1;
    unsigned char c;
    while (1) {
        fetch_token(token, input, syntax);
        c = token->opr.c;
        if ((token->type == END_OF_RE))
            return -2;
        if (token->type == OP_CLOSE_DUP_NUM || c == ',')
            break;
        num = ((token->type != CHARACTER || c < '0' || '9' < c || num == -2)
               ? -2
               : num == -1
                 ? c - '0'
                 : MIN (RE_DUP_MAX + 1, num * 10 + c - '0'));
    }
    return num;
}

/* Functions for binary tree operation.  */

/* Create a tree node.  */

static bin_tree_t *
create_tree(re_dfa_t *dfa, bin_tree_t *left, bin_tree_t *right,
            re_token_type_t type) {
    re_token_t t;
    t.type = type;
    return create_token_tree(dfa, left, right, &t);
}

static bin_tree_t *
create_token_tree(re_dfa_t *dfa, bin_tree_t *left, bin_tree_t *right,
                  const re_token_t *token) {
    bin_tree_t *tree;
    if ((dfa->str_tree_storage_idx == BIN_TREE_STORAGE_SIZE)) {
        bin_tree_storage_t *storage = re_malloc(bin_tree_storage_t, 1);

        if (storage == NULL)
            return NULL;
        storage->next = dfa->str_tree_storage;
        dfa->str_tree_storage = storage;
        dfa->str_tree_storage_idx = 0;
    }
    tree = &dfa->str_tree_storage->data[dfa->str_tree_storage_idx++];

    tree->parent = NULL;
    tree->left = left;
    tree->right = right;
    tree->token = *token;
    tree->token.duplicated = 0;
    tree->token.opt_subexp = 0;
    tree->first = NULL;
    tree->next = NULL;
    tree->node_idx = -1;

    if (left != NULL)
        left->parent = tree;
    if (right != NULL)
        right->parent = tree;
    return tree;
}

/* Mark the tree SRC as an optional subexpression.
   To be called from preorder or postorder.  */

static reg_errcode_t
mark_opt_subexp(void *extra, bin_tree_t *node) {
    Idx idx = (uintptr_t) extra;
    if (node->token.type == SUBEXP && node->token.opr.idx == idx)
        node->token.opt_subexp = 1;

    return REG_NOERROR;
}

/* Free the allocated memory inside NODE. */

static void
free_token(re_token_t *node) {
    if (node->type == SIMPLE_BRACKET && node->duplicated == 0)
        re_free(node->opr.sbcset);
}

/* Worker function for tree walking.  Free the allocated memory inside NODE
   and its children. */

static reg_errcode_t
free_tree(void *extra, bin_tree_t *node) {
    free_token(&node->token);
    return REG_NOERROR;
}


/* Duplicate the node SRC, and return new node.  This is a preorder
   visit similar to the one implemented by the generic visitor, but
   we need more infrastructure to maintain two parallel trees --- so,
   it's easier to duplicate.  */

static bin_tree_t *
duplicate_tree(const bin_tree_t *root, re_dfa_t *dfa) {
    const bin_tree_t *node;
    bin_tree_t *dup_root;
    bin_tree_t **p_new = &dup_root, *dup_node = root->parent;

    for (node = root;;) {
        /* Create a new tree and link it back to the current parent.  */
        *p_new = create_token_tree(dfa, NULL, NULL, &node->token);
        if (*p_new == NULL)
            return NULL;
        (*p_new)->parent = dup_node;
        (*p_new)->token.duplicated = 1;
        dup_node = *p_new;

        /* Go to the left node, or up and to the right.  */
        if (node->left) {
            node = node->left;
            p_new = &dup_node->left;
        } else {
            const bin_tree_t *prev = NULL;
            while (node->right == prev || node->right == NULL) {
                prev = node;
                node = node->parent;
                dup_node = dup_node->parent;
                if (!node)
                    return dup_root;
            }
            node = node->right;
            p_new = &dup_node->right;
        }
    }
}

static reg_errcode_t
prune_impossible_nodes(re_match_context_t *mctx) {
    const re_dfa_t *const dfa = mctx->dfa;
    Idx halt_node, match_last;
    reg_errcode_t ret;
    re_dfastate_t **sifted_states;
    re_dfastate_t **lim_states = NULL;
    re_sift_context_t sctx;
    match_last = mctx->match_last;
    halt_node = mctx->last_node;

/* Avoid overflow.  */
    if ((MIN (IDX_MAX, SIZE_MAX / sizeof(re_dfastate_t * ))
         <= match_last))
        return REG_ESPACE;

    sifted_states = re_malloc(re_dfastate_t * , match_last + 1);
    if ((sifted_states == NULL)) {
        ret = REG_ESPACE;
        goto free_return;
    }
    if (dfa->nbackref) {
        lim_states = re_malloc(re_dfastate_t * , match_last + 1);
        if ((lim_states == NULL)) {
            ret = REG_ESPACE;
            goto free_return;
        }
        while (1) {
            memset(lim_states, '\0',
                   sizeof(re_dfastate_t * ) * (match_last + 1));
            sift_ctx_init(&sctx, sifted_states, lim_states, halt_node,
                          match_last);
            ret = sift_states_backward(mctx, &sctx);
            re_node_set_free(&sctx.limits);
            if ((ret != REG_NOERROR))
                goto free_return;
            if (sifted_states[0] != NULL || lim_states[0] != NULL)
                break;
            do {
                --match_last;
                if (match_last < 0) {
                    ret = REG_NOMATCH;
                    goto free_return;
                }
            } while (mctx->state_log[match_last] == NULL
                     || !mctx->state_log[match_last]->halt);
            halt_node = check_halt_state_context(mctx,
                                                 mctx->state_log[match_last],
                                                 match_last);
        }
        ret = merge_state_array(dfa, sifted_states, lim_states,
                                match_last + 1);
        re_free(lim_states);
        lim_states = NULL;
        if ((ret != REG_NOERROR))
            goto free_return;
    } else {
        sift_ctx_init(&sctx, sifted_states, lim_states, halt_node, match_last);
        ret = sift_states_backward(mctx, &sctx);
        re_node_set_free(&sctx.limits);
        if ((ret != REG_NOERROR))
            goto free_return;
        if (sifted_states[0] == NULL) {
            ret = REG_NOMATCH;
            goto free_return;
        }
    }
    re_free(mctx->state_log);
    mctx->state_log = sifted_states;
    sifted_states = NULL;
    mctx->last_node = halt_node;
    mctx->match_last = match_last;
    ret = REG_NOERROR;
    free_return:
    re_free(sifted_states);
    re_free(lim_states);
    return ret;
}

/* Acquire an initial state and return it.
   We must select appropriate initial state depending on the context,
   since initial states may have constraints like "\<", "^", etc..  */

static inline re_dfastate_t *
acquire_init_state_context(reg_errcode_t *err, const re_match_context_t *mctx,
                           Idx idx) {
    const re_dfa_t *const dfa = mctx->dfa;
    if (dfa->init_state->has_constraint) {
        unsigned int context;
        context = re_string_context_at(&mctx->input, idx - 1, mctx->eflags);
        if (IS_WORD_CONTEXT(context))
            return dfa->init_state_word;
        else if (IS_ORDINARY_CONTEXT(context))
            return dfa->init_state;
        else if (IS_BEGBUF_CONTEXT(context) && IS_NEWLINE_CONTEXT(context))
            return dfa->init_state_begbuf;
        else if (IS_NEWLINE_CONTEXT(context))
            return dfa->init_state_nl;
        else if (IS_BEGBUF_CONTEXT(context)) {
            /* It is relatively rare case, then calculate on demand.  */
            return re_acquire_state_context(err, dfa,
                                            dfa->init_state->entrance_nodes,
                                            context);
        } else
            /* Must not happen?  */
            return dfa->init_state;
    } else
        return dfa->init_state;
}

/* Check whether the node accepts the byte which is IDX-th
   byte of the INPUT.  */

static bool
check_node_accept(const re_match_context_t *mctx, const re_token_t *node,
                  Idx idx) {
    unsigned char ch;
    ch = re_string_byte_at(&mctx->input, idx);
    switch (node->type) {
        case CHARACTER:
            if (node->opr.c != ch)
                return false;
            break;

        case SIMPLE_BRACKET:
            if (!bitset_contain(node->opr.sbcset, ch))
                return false;
            break;

        case OP_PERIOD:
            if ((ch == '\n' && !(mctx->dfa->syntax & RE_DOT_NEWLINE))
                || (ch == '\0' && (mctx->dfa->syntax & RE_DOT_NOT_NULL)))
                return false;
            break;

        default:
            return false;
    }

    if (node->constraint) {
        /* The node has constraints.  Check whether the current context
       satisfies the constraints.  */
        unsigned int context = re_string_context_at(&mctx->input, idx,
                                                    mctx->eflags);
        if (NOT_SATISFY_NEXT_CONSTRAINT(node->constraint, context))
            return false;
    }

    return true;
}


/* Extend the buffers, if the buffers have run out.  */

static reg_errcode_t
extend_buffers(re_match_context_t
               *mctx,
               int min_len
) {
    reg_errcode_t ret;
    re_string_t *pstr = &mctx->input;

/* Avoid overflow.  */
    if (
            (MIN (IDX_MAX, SIZE_MAX / sizeof(re_dfastate_t * )) / 2
             <= pstr->bufs_len))
        return
                REG_ESPACE;

/* Double the lengths of the buffers, but allocate at least MIN_LEN.  */
    ret = re_string_realloc_buffers(pstr,
                                    MAX (min_len,
                                         MIN(pstr->len, pstr->bufs_len * 2)));
    if (
            (ret
             != REG_NOERROR))
        return
                ret;

    if (mctx->state_log != NULL) {
/* And double the length of state_log.  */
/* XXX We have no indication of the size of this buffer.  If this
allocation fail we have no indication that the state_log array
does not have the right size.  */
        re_dfastate_t **new_array = re_realloc(mctx->state_log, re_dfastate_t * ,
                                               pstr->bufs_len + 1);
        if (
                (new_array
                 == NULL))
            return
                    REG_ESPACE;
        mctx->
                state_log = new_array;
    }

/* Then reconstruct the buffers.  */
    if (pstr->icase) {
        build_upper_buffer(pstr);
    } else {
        {
            if (pstr->trans != NULL)
                re_string_translate_buffer(pstr);
        }
    }
    return
            REG_NOERROR;
}

/* Functions for state transition.  */

/* Helper functions for transit_state.  */

/* Return the first entry with the same str_idx, or -1 if none is
   found.  Note that MCTX->BKREF_ENTS is already sorted by MCTX->STR_IDX.  */

static Idx
search_cur_bkref_entry (const re_match_context_t *mctx, Idx str_idx)
{
    Idx left, right, mid, last;
    last = right = mctx->nbkref_ents;
    for (left = 0; left < right;)
    {
        mid = (left + right) / 2;
        if (mctx->bkref_ents[mid].str_idx < str_idx)
            left = mid + 1;
        else
            right = mid;
    }
    if (left < last && mctx->bkref_ents[left].str_idx == str_idx)
        return left;
    else
        return -1;
}

/* Register the node NODE, whose type is OP_OPEN_SUBEXP, and which matches
   at STR_IDX.  */

static reg_errcode_t
match_ctx_add_subtop(re_match_context_t *mctx, Idx node, Idx str_idx) {
    if ((mctx->nsub_tops == mctx->asub_tops)) {
        Idx new_asub_tops = mctx->asub_tops * 2;
        re_sub_match_top_t **new_array = re_realloc(mctx->sub_tops,
                                                    re_sub_match_top_t * ,
                                                    new_asub_tops);
        if ((new_array == NULL))
            return REG_ESPACE;
        mctx->sub_tops = new_array;
        mctx->asub_tops = new_asub_tops;
    }
    mctx->sub_tops[mctx->nsub_tops] = calloc(1, sizeof(re_sub_match_top_t));
    if ((mctx->sub_tops[mctx->nsub_tops] == NULL))
        return REG_ESPACE;
    mctx->sub_tops[mctx->nsub_tops]->node = node;
    mctx->sub_tops[mctx->nsub_tops++]->str_idx = str_idx;
    return REG_NOERROR;
}

/* Register the node NODE, whose type is OP_CLOSE_SUBEXP, and which matches
   at STR_IDX, whose corresponding OP_OPEN_SUBEXP is SUB_TOP.  */

static re_sub_match_last_t *
match_ctx_add_sublast (re_sub_match_top_t *subtop, Idx node, Idx str_idx)
{
    re_sub_match_last_t *new_entry;
    if ( (subtop->nlasts == subtop->alasts))
    {
        Idx new_alasts = 2 * subtop->alasts + 1;
        re_sub_match_last_t **new_array = re_realloc (subtop->lasts,
                                                      re_sub_match_last_t *,
                                                      new_alasts);
        if ( (new_array == NULL))
            return NULL;
        subtop->lasts = new_array;
        subtop->alasts = new_alasts;
    }
    new_entry = calloc (1, sizeof (re_sub_match_last_t));
    if ( (new_entry != NULL))
    {
        subtop->lasts[subtop->nlasts] = new_entry;
        new_entry->node = node;
        new_entry->str_idx = str_idx;
        ++subtop->nlasts;
    }
    return new_entry;
}


/* From the node set CUR_NODES, pick up the nodes whose types are
   OP_OPEN_SUBEXP and which have corresponding back references in the regular
   expression. And register them to use them later for evaluating the
   corresponding back references.  */

static reg_errcode_t
check_subexp_matching_top(re_match_context_t *mctx, re_node_set *cur_nodes,
                          Idx str_idx) {
    const re_dfa_t *const dfa = mctx->dfa;
    Idx node_idx;
    reg_errcode_t err;

    /* TODO: This isn't efficient.
         Because there might be more than one nodes whose types are
         OP_OPEN_SUBEXP and whose index is SUBEXP_IDX, we must check all
         nodes.
         E.g. RE: (a){2}  */
    for (node_idx = 0; node_idx < cur_nodes->nelem; ++node_idx) {
        Idx node = cur_nodes->elems[node_idx];
        if (dfa->nodes[node].type == OP_OPEN_SUBEXP
            && dfa->nodes[node].opr.idx < BITSET_WORD_BITS
            && (dfa->used_bkref_map
                & ((bitset_word_t) 1 << dfa->nodes[node].opr.idx))) {
            err = match_ctx_add_subtop(mctx, node, str_idx);
            if ((err != REG_NOERROR))
                return err;
        }
    }
    return REG_NOERROR;
}

/* Compute the next node to which "NFA" transit from NODE("NFA" is a NFA
   corresponding to the DFA).
   Return the destination node, and update EPS_VIA_NODES;
   return -1 in case of errors.  */

static Idx
proceed_next_node (const re_match_context_t *mctx, Idx nregs, regmatch_t *regs,
                   Idx *pidx, Idx node, re_node_set *eps_via_nodes,
                   struct re_fail_stack_t *fs)
{
    const re_dfa_t *const dfa = mctx->dfa;
    Idx i;
    bool ok;
    if (IS_EPSILON_NODE (dfa->nodes[node].type))
    {
        re_node_set *cur_nodes = &mctx->state_log[*pidx]->nodes;
        re_node_set *edests = &dfa->edests[node];
        Idx dest_node;
        ok = re_node_set_insert (eps_via_nodes, node);
        if ( (! ok))
            return -2;
        /* Pick up a valid destination, or return -1 if none
       is found.  */
        for (dest_node = -1, i = 0; i < edests->nelem; ++i)
        {
            Idx candidate = edests->elems[i];
            if (!re_node_set_contains (cur_nodes, candidate))
                continue;
            if (dest_node == -1)
                dest_node = candidate;

            else
            {
                /* In order to avoid infinite loop like "(a*)*", return the second
               epsilon-transition if the first was already considered.  */
                if (re_node_set_contains (eps_via_nodes, dest_node))
                    return candidate;

                    /* Otherwise, push the second epsilon-transition on the fail stack.  */
                else if (fs != NULL
                         && push_fail_stack (fs, *pidx, candidate, nregs, regs,
                                             eps_via_nodes))
                    return -2;

                /* We know we are going to exit.  */
                break;
            }
        }
        return dest_node;
    }
    else
    {
        Idx naccepted = 0;
        re_token_type_t type = dfa->nodes[node].type;

        if (type == OP_BACK_REF)
        {
            Idx subexp_idx = dfa->nodes[node].opr.idx + 1;
            naccepted = regs[subexp_idx].rm_eo - regs[subexp_idx].rm_so;
            if (fs != NULL)
            {
                if (regs[subexp_idx].rm_so == -1 || regs[subexp_idx].rm_eo == -1)
                    return -1;
                else if (naccepted)
                {
                    char *buf = (char *) re_string_get_buffer (&mctx->input);
                    if (memcmp (buf + regs[subexp_idx].rm_so, buf + *pidx,
                                naccepted) != 0)
                        return -1;
                }
            }

            if (naccepted == 0)
            {
                Idx dest_node;
                ok = re_node_set_insert (eps_via_nodes, node);
                if ( (! ok))
                    return -2;
                dest_node = dfa->edests[node].elems[0];
                if (re_node_set_contains (&mctx->state_log[*pidx]->nodes,
                                          dest_node))
                    return dest_node;
            }
        }

        if (naccepted != 0
            || check_node_accept (mctx, dfa->nodes + node, *pidx))
        {
            Idx dest_node = dfa->nexts[node];
            *pidx = (naccepted == 0) ? *pidx + 1 : *pidx + naccepted;
            if (fs && (*pidx > mctx->match_last || mctx->state_log[*pidx] == NULL
                       || !re_node_set_contains (&mctx->state_log[*pidx]->nodes,
                                                 dest_node)))
                return -1;
            re_node_set_empty (eps_via_nodes);
            return dest_node;
        }
    }
    return -1;
}

static reg_errcode_t
        
push_fail_stack (struct re_fail_stack_t *fs, Idx str_idx, Idx dest_node,
                 Idx nregs, regmatch_t *regs, re_node_set *eps_via_nodes)
{
    reg_errcode_t err;
    Idx num = fs->num++;
    if (fs->num == fs->alloc)
    {
        struct re_fail_stack_ent_t *new_array;
        new_array = re_realloc (fs->stack, struct re_fail_stack_ent_t,
        fs->alloc * 2);
        if (new_array == NULL)
            return REG_ESPACE;
        fs->alloc *= 2;
        fs->stack = new_array;
    }
    fs->stack[num].idx = str_idx;
    fs->stack[num].node = dest_node;
    fs->stack[num].regs = re_malloc (regmatch_t, nregs);
    if (fs->stack[num].regs == NULL)
        return REG_ESPACE;
    memcpy (fs->stack[num].regs, regs, sizeof (regmatch_t) * nregs);
    err = re_node_set_init_copy (&fs->stack[num].eps_via_nodes, eps_via_nodes);
    return err;
}

static Idx
pop_fail_stack (struct re_fail_stack_t *fs, Idx *pidx, Idx nregs,
                regmatch_t *regs, re_node_set *eps_via_nodes)
{
    Idx num = --fs->num;
    assert (num >= 0);
    *pidx = fs->stack[num].idx;
    memcpy (regs, fs->stack[num].regs, sizeof (regmatch_t) * nregs);
    re_node_set_free (eps_via_nodes);
    re_free (fs->stack[num].regs);
    *eps_via_nodes = fs->stack[num].eps_via_nodes;
    return fs->stack[num].node;
}

/* Set the positions where the subexpressions are starts/ends to registers
   PMATCH.
   Note: We assume that pmatch[0] is already set, and
   pmatch[i].rm_so == pmatch[i].rm_eo == -1 for 0 < i < nmatch.  */

static reg_errcode_t
        
set_regs (const regex_t *preg, const re_match_context_t *mctx, size_t nmatch,
          regmatch_t *pmatch, bool fl_backtrack)
{
    const re_dfa_t *dfa = preg->buffer;
    Idx idx, cur_node;
    re_node_set eps_via_nodes;
    struct re_fail_stack_t *fs;
    struct re_fail_stack_t fs_body = { 0, 2, NULL };
    regmatch_t *prev_idx_match;
    bool prev_idx_match_malloced = false;
    if (fl_backtrack)
    {
        fs = &fs_body;
        fs->stack = re_malloc (struct re_fail_stack_ent_t, fs->alloc);
        if (fs->stack == NULL)
            return REG_ESPACE;
    }
    else
        fs = NULL;

    cur_node = dfa->init_node;
    re_node_set_init_empty (&eps_via_nodes);

    if (__libc_use_alloca (nmatch * sizeof (regmatch_t)))
        prev_idx_match = (regmatch_t *) alloca (nmatch * sizeof (regmatch_t));
    else
    {
        prev_idx_match = re_malloc (regmatch_t, nmatch);
        if (prev_idx_match == NULL)
        {
            free_fail_stack_return (fs);
            return REG_ESPACE;
        }
        prev_idx_match_malloced = true;
    }
    memcpy (prev_idx_match, pmatch, sizeof (regmatch_t) * nmatch);

    for (idx = pmatch[0].rm_so; idx <= pmatch[0].rm_eo ;)
    {
        update_regs (dfa, pmatch, prev_idx_match, cur_node, idx, nmatch);

        if (idx == pmatch[0].rm_eo && cur_node == mctx->last_node)
        {
            Idx reg_idx;
            if (fs)
            {
                for (reg_idx = 0; reg_idx < nmatch; ++reg_idx)
                    if (pmatch[reg_idx].rm_so > -1 && pmatch[reg_idx].rm_eo == -1)
                        break;
                if (reg_idx == nmatch)
                {
                    re_node_set_free (&eps_via_nodes);
                    if (prev_idx_match_malloced)
                        re_free (prev_idx_match);
                    return free_fail_stack_return (fs);
                }
                cur_node = pop_fail_stack (fs, &idx, nmatch, pmatch,
                                           &eps_via_nodes);
            }
            else
            {
                re_node_set_free (&eps_via_nodes);
                if (prev_idx_match_malloced)
                    re_free (prev_idx_match);
                return REG_NOERROR;
            }
        }

        /* Proceed to next node.  */
        cur_node = proceed_next_node (mctx, nmatch, pmatch, &idx, cur_node,
                                      &eps_via_nodes, fs);

        if ( (cur_node < 0))
        {
            if ( (cur_node == -2))
            {
                re_node_set_free (&eps_via_nodes);
                if (prev_idx_match_malloced)
                    re_free (prev_idx_match);
                free_fail_stack_return (fs);
                return REG_ESPACE;
            }
            if (fs)
                cur_node = pop_fail_stack (fs, &idx, nmatch, pmatch,
                                           &eps_via_nodes);
            else
            {
                re_node_set_free (&eps_via_nodes);
                if (prev_idx_match_malloced)
                    re_free (prev_idx_match);
                return REG_NOMATCH;
            }
        }
    }
    re_node_set_free (&eps_via_nodes);
    if (prev_idx_match_malloced)
        re_free (prev_idx_match);
    return free_fail_stack_return (fs);
}

static reg_errcode_t
free_fail_stack_return (struct re_fail_stack_t *fs)
{
    if (fs)
    {
        Idx fs_idx;
        for (fs_idx = 0; fs_idx < fs->num; ++fs_idx)
        {
            re_node_set_free (&fs->stack[fs_idx].eps_via_nodes);
            re_free (fs->stack[fs_idx].regs);
        }
        re_free (fs->stack);
    }
    return REG_NOERROR;
}

static void
update_regs (const re_dfa_t *dfa, regmatch_t *pmatch,
             regmatch_t *prev_idx_match, Idx cur_node, Idx cur_idx, Idx nmatch)
{
    int type = dfa->nodes[cur_node].type;
    if (type == OP_OPEN_SUBEXP)
    {
        Idx reg_num = dfa->nodes[cur_node].opr.idx + 1;

        /* We are at the first node of this sub expression.  */
        if (reg_num < nmatch)
        {
            pmatch[reg_num].rm_so = cur_idx;
            pmatch[reg_num].rm_eo = -1;
        }
    }
    else if (type == OP_CLOSE_SUBEXP)
    {
        Idx reg_num = dfa->nodes[cur_node].opr.idx + 1;
        if (reg_num < nmatch)
        {
            /* We are at the last node of this sub expression.  */
            if (pmatch[reg_num].rm_so < cur_idx)
            {
                pmatch[reg_num].rm_eo = cur_idx;
                /* This is a non-empty match or we are not inside an optional
               subexpression.  Accept this right away.  */
                memcpy (prev_idx_match, pmatch, sizeof (regmatch_t) * nmatch);
            }
            else
            {
                if (dfa->nodes[cur_node].opt_subexp
                    && prev_idx_match[reg_num].rm_so != -1)
                    /* We transited through an empty match for an optional
                       subexpression, like (a?)*, and this is not the subexp's
                       first match.  Copy back the old content of the registers
                       so that matches of an inner subexpression are undone as
                       well, like in ((a?))*.  */
                    memcpy (pmatch, prev_idx_match, sizeof (regmatch_t) * nmatch);
                else
                    /* We completed a subexpression, but it may be part of
                       an optional one, so do not update PREV_IDX_MATCH.  */
                    pmatch[reg_num].rm_eo = cur_idx;
            }
        }
    }
}

/* This function checks the STATE_LOG from the SCTX->last_str_idx to 0
   and sift the nodes in each states according to the following rules.
   Updated state_log will be wrote to STATE_LOG.

   Rules: We throw away the Node 'a' in the STATE_LOG[STR_IDX] if...
     1. When STR_IDX == MATCH_LAST(the last index in the state_log):
	If 'a' isn't the LAST_NODE and 'a' can't epsilon transit to
	the LAST_NODE, we throw away the node 'a'.
     2. When 0 <= STR_IDX < MATCH_LAST and 'a' accepts
	string 's' and transit to 'b':
	i. If 'b' isn't in the STATE_LOG[STR_IDX+strlen('s')], we throw
	   away the node 'a'.
	ii. If 'b' is in the STATE_LOG[STR_IDX+strlen('s')] but 'b' is
	    thrown away, we throw away the node 'a'.
     3. When 0 <= STR_IDX < MATCH_LAST and 'a' epsilon transit to 'b':
	i. If 'b' isn't in the STATE_LOG[STR_IDX], we throw away the
	   node 'a'.
	ii. If 'b' is in the STATE_LOG[STR_IDX] but 'b' is thrown away,
	    we throw away the node 'a'.  */

#define STATE_NODE_CONTAINS(state,node) \
  ((state) != NULL && re_node_set_contains (&(state)->nodes, node))

static reg_errcode_t
sift_states_backward (const re_match_context_t *mctx, re_sift_context_t *sctx)
{
    reg_errcode_t err;
    int null_cnt = 0;
    Idx str_idx = sctx->last_str_idx;
    re_node_set cur_dest;

    /* Build sifted state_log[str_idx].  It has the nodes which can epsilon
       transit to the last_node and the last_node itself.  */
    err = re_node_set_init_1 (&cur_dest, sctx->last_node);
    if ( (err != REG_NOERROR))
        return err;
    err = update_cur_sifted_state (mctx, sctx, str_idx, &cur_dest);
    if ( (err != REG_NOERROR))
        goto free_return;

    /* Then check each states in the state_log.  */
    while (str_idx > 0)
    {
        /* Update counters.  */
        null_cnt = (sctx->sifted_states[str_idx] == NULL) ? null_cnt + 1 : 0;
        if (null_cnt > mctx->max_mb_elem_len)
        {
            memset (sctx->sifted_states, '\0',
                    sizeof (re_dfastate_t *) * str_idx);
            re_node_set_free (&cur_dest);
            return REG_NOERROR;
        }
        re_node_set_empty (&cur_dest);
        --str_idx;

        if (mctx->state_log[str_idx])
        {
            err = build_sifted_states (mctx, sctx, str_idx, &cur_dest);
            if ( (err != REG_NOERROR))
                goto free_return;
        }

        /* Add all the nodes which satisfy the following conditions:
       - It can epsilon transit to a node in CUR_DEST.
       - It is in CUR_SRC.
       And update state_log.  */
        err = update_cur_sifted_state (mctx, sctx, str_idx, &cur_dest);
        if ( (err != REG_NOERROR))
            goto free_return;
    }
    err = REG_NOERROR;
    free_return:
    re_node_set_free (&cur_dest);
    return err;
}

static reg_errcode_t
build_sifted_states (const re_match_context_t *mctx, re_sift_context_t *sctx,
                     Idx str_idx, re_node_set *cur_dest)
{
    const re_dfa_t *const dfa = mctx->dfa;
    const re_node_set *cur_src = &mctx->state_log[str_idx]->non_eps_nodes;
    Idx i;

    /* Then build the next sifted state.
       We build the next sifted state on 'cur_dest', and update
       'sifted_states[str_idx]' with 'cur_dest'.
       Note:
       'cur_dest' is the sifted state from 'state_log[str_idx + 1]'.
       'cur_src' points the node_set of the old 'state_log[str_idx]'
       (with the epsilon nodes pre-filtered out).  */
    for (i = 0; i < cur_src->nelem; i++)
    {
        Idx prev_node = cur_src->elems[i];
        int naccepted = 0;
        bool ok;

        /* We don't check backreferences here.
       See update_cur_sifted_state().  */
        if (!naccepted
            && check_node_accept (mctx, dfa->nodes + prev_node, str_idx)
            && STATE_NODE_CONTAINS (sctx->sifted_states[str_idx + 1],
                                    dfa->nexts[prev_node]))
            naccepted = 1;

        if (naccepted == 0)
            continue;

        if (sctx->limits.nelem)
        {
            Idx to_idx = str_idx + naccepted;
            if (check_dst_limits (mctx, &sctx->limits,
                                  dfa->nexts[prev_node], to_idx,
                                  prev_node, str_idx))
                continue;
        }
        ok = re_node_set_insert (cur_dest, prev_node);
        if ( (! ok))
            return REG_ESPACE;
    }

    return REG_NOERROR;
}

static reg_errcode_t
clean_state_log_if_needed (re_match_context_t *mctx, Idx next_state_log_idx)
{
    Idx top = mctx->state_log_top;

    if ((next_state_log_idx >= mctx->input.bufs_len
         && mctx->input.bufs_len < mctx->input.len)
        || (next_state_log_idx >= mctx->input.valid_len
            && mctx->input.valid_len < mctx->input.len))
    {
        reg_errcode_t err;
        err = extend_buffers (mctx, next_state_log_idx + 1);
        if ( (err != REG_NOERROR))
            return err;
    }

    if (top < next_state_log_idx)
    {
        memset (mctx->state_log + top + 1, '\0',
                sizeof (re_dfastate_t *) * (next_state_log_idx - top));
        mctx->state_log_top = next_state_log_idx;
    }
    return REG_NOERROR;
}

/* Enumerate all the candidates which the backreference BKREF_NODE can match
   at BKREF_STR_IDX, and register them by match_ctx_add_entry().
   Note that we might collect inappropriate candidates here.
   However, the cost of checking them strictly here is too high, then we
   delay these checking for prune_impossible_nodes().  */

static reg_errcode_t

get_subexp(re_match_context_t *mctx, Idx bkref_node, Idx bkref_str_idx) {
    const re_dfa_t *const dfa = mctx->dfa;
    Idx subexp_num, sub_top_idx;
    const char *buf = (const char *) re_string_get_buffer(&mctx->input);
/* Return if we have already checked BKREF_NODE at BKREF_STR_IDX.  */
    Idx cache_idx = search_cur_bkref_entry(mctx, bkref_str_idx);
    if (cache_idx != -1) {
        const struct re_backref_cache_entry *entry
                = mctx->bkref_ents + cache_idx;
        do
            if (entry->node == bkref_node)
                return REG_NOERROR; /* We already checked it.  */
        while (entry++->more);
    }

    subexp_num = dfa->nodes[bkref_node].opr.idx;

/* For each sub expression  */
    for (sub_top_idx = 0; sub_top_idx < mctx->nsub_tops; ++sub_top_idx) {
        reg_errcode_t err;
        re_sub_match_top_t *sub_top = mctx->sub_tops[sub_top_idx];
        re_sub_match_last_t *sub_last;
        Idx sub_last_idx, sl_str, bkref_str_off;

        if (dfa->nodes[sub_top->node].opr.idx != subexp_num)
            continue; /* It isn't related.  */

        sl_str = sub_top->str_idx;
        bkref_str_off = bkref_str_idx;
/* At first, check the last node of sub expressions we already
evaluated.  */
        for (sub_last_idx = 0; sub_last_idx < sub_top->nlasts; ++sub_last_idx) {
            regoff_t sl_str_diff;
            sub_last = sub_top->lasts[sub_last_idx];
            sl_str_diff = sub_last->str_idx - sl_str;
/* The matched string by the sub expression match with the substring
   at the back reference?  */
            if (sl_str_diff > 0) {
                if ((bkref_str_off + sl_str_diff
                     > mctx->input.valid_len)) {
/* Not enough chars for a successful match.  */
                    if (bkref_str_off + sl_str_diff > mctx->input.len)
                        break;

                    err = clean_state_log_if_needed(mctx,
                                                    bkref_str_off
                                                    + sl_str_diff);
                    if ((err != REG_NOERROR))
                        return err;
                    buf = (const char *) re_string_get_buffer(&mctx->input);
                }
                if (memcmp(buf + bkref_str_off, buf + sl_str, sl_str_diff) != 0)
/* We don't need to search this sub expression any more.  */
                    break;
            }
            bkref_str_off += sl_str_diff;
            sl_str += sl_str_diff;
            err = get_subexp_sub(mctx, sub_top, sub_last, bkref_node,
                                 bkref_str_idx);

/* Reload buf, since the preceding call might have reallocated
   the buffer.  */
            buf = (const char *) re_string_get_buffer(&mctx->input);

            if (err == REG_NOMATCH)
                continue;
            if ((err != REG_NOERROR))
                return err;
        }

        if (sub_last_idx < sub_top->nlasts)
            continue;
        if (sub_last_idx > 0)
            ++sl_str;
/* Then, search for the other last nodes of the sub expression.  */
        for (; sl_str <= bkref_str_idx; ++sl_str) {
            Idx cls_node;
            regoff_t sl_str_off;
            const re_node_set *nodes;
            sl_str_off = sl_str - sub_top->str_idx;
/* The matched string by the sub expression match with the substring
   at the back reference?  */
            if (sl_str_off > 0) {
                if ((bkref_str_off >= mctx->input.valid_len)) {
/* If we are at the end of the input, we cannot match.  */
                    if (bkref_str_off >= mctx->input.len)
                        break;

                    err = extend_buffers(mctx, bkref_str_off + 1);
                    if ((err != REG_NOERROR))
                        return err;

                    buf = (const char *) re_string_get_buffer(&mctx->input);
                }
                if (buf[bkref_str_off++] != buf[sl_str - 1])
                    break; /* We don't need to search this sub expression
			  any more.  */
            }
            if (mctx->state_log[sl_str] == NULL)
                continue;
/* Does this state have a ')' of the sub expression?  */
            nodes = &mctx->state_log[sl_str]->nodes;
            cls_node = find_subexp_node(dfa, nodes, subexp_num,
                                        OP_CLOSE_SUBEXP);
            if (cls_node == -1)
                continue; /* No.  */
            if (sub_top->path == NULL) {
                sub_top->path = calloc(sizeof(state_array_t),
                                       sl_str - sub_top->str_idx + 1);
                if (sub_top->path == NULL)
                    return REG_ESPACE;
            }
/* Can the OP_OPEN_SUBEXP node arrive the OP_CLOSE_SUBEXP node
   in the current context?  */
            err = check_arrival(mctx, sub_top->path, sub_top->node,
                                sub_top->str_idx, cls_node, sl_str,
                                OP_CLOSE_SUBEXP);
            if (err == REG_NOMATCH)
                continue;
            if ((err != REG_NOERROR))
                return err;
            sub_last = match_ctx_add_sublast(sub_top, cls_node, sl_str);
            if ((sub_last == NULL))
                return REG_ESPACE;
            err = get_subexp_sub(mctx, sub_top, sub_last, bkref_node,
                                 bkref_str_idx);
            if (err == REG_NOMATCH)
                continue;
        }
    }
    return REG_NOERROR;
}

/* Helper functions for get_subexp().  */

/* Check SUB_LAST can arrive to the back reference BKREF_NODE at BKREF_STR.
   If it can arrive, register the sub expression expressed with SUB_TOP
   and SUB_LAST.  */

static reg_errcode_t
get_subexp_sub(re_match_context_t *mctx, const re_sub_match_top_t *sub_top,
               re_sub_match_last_t *sub_last, Idx bkref_node, Idx bkref_str) {
    reg_errcode_t err;
    Idx to_idx;
    /* Can the subexpression arrive the back reference?  */
    err = check_arrival(mctx, &sub_last->path, sub_last->node,
                        sub_last->str_idx, bkref_node, bkref_str,
                        OP_OPEN_SUBEXP);
    if (err != REG_NOERROR)
        return err;
    err = match_ctx_add_entry(mctx, bkref_node, bkref_str, sub_top->str_idx,
                              sub_last->str_idx);
    if ((err != REG_NOERROR))
        return err;
    to_idx = bkref_str + sub_last->str_idx - sub_top->str_idx;
    return clean_state_log_if_needed(mctx, to_idx);
}

/* Find the first node which is '(' or ')' and whose index is SUBEXP_IDX.
   Search '(' if FL_OPEN, or search ')' otherwise.
   TODO: This function isn't efficient...
	 Because there might be more than one nodes whose types are
	 OP_OPEN_SUBEXP and whose index is SUBEXP_IDX, we must check all
	 nodes.
	 E.g. RE: (a){2}  */

static Idx
find_subexp_node(const re_dfa_t *dfa, const re_node_set *nodes,
                 Idx subexp_idx, int type) {
    Idx cls_idx;
    for (cls_idx = 0; cls_idx < nodes->nelem; ++cls_idx) {
        Idx cls_node = nodes->elems[cls_idx];
        const re_token_t *node = dfa->nodes + cls_node;
        if (node->type == type
            && node->opr.idx == subexp_idx)
            return cls_node;
    }
    return -1;
}

/* Check whether the node TOP_NODE at TOP_STR can arrive to the node
   LAST_NODE at LAST_STR.  We record the path onto PATH since it will be
   heavily reused.
   Return REG_NOERROR if it can arrive, or REG_NOMATCH otherwise.  */

static reg_errcode_t

check_arrival(re_match_context_t *mctx, state_array_t *path, Idx top_node,
              Idx top_str, Idx last_node, Idx last_str, int type) {
    const re_dfa_t *const dfa = mctx->dfa;
    reg_errcode_t err = REG_NOERROR;
    Idx subexp_num, backup_cur_idx, str_idx, null_cnt;
    re_dfastate_t *cur_state = NULL;
    re_node_set *cur_nodes, next_nodes;
    re_dfastate_t **backup_state_log;
    unsigned int context;

    subexp_num = dfa->nodes[top_node].opr.idx;
/* Extend the buffer if we need.  */
    if ((path->alloc < last_str + mctx->max_mb_elem_len + 1)) {
        re_dfastate_t **new_array;
        Idx old_alloc = path->alloc;
        Idx incr_alloc = last_str + mctx->max_mb_elem_len + 1;
        Idx new_alloc;
        if ((IDX_MAX - old_alloc < incr_alloc))
            return REG_ESPACE;
        new_alloc = old_alloc + incr_alloc;
        if ((SIZE_MAX / sizeof(re_dfastate_t * ) < new_alloc))
            return REG_ESPACE;
        new_array = re_realloc(path->array, re_dfastate_t * , new_alloc);
        if ((new_array == NULL))
            return REG_ESPACE;
        path->array = new_array;
        path->alloc = new_alloc;
        memset(new_array + old_alloc, '\0',
               sizeof(re_dfastate_t * ) * (path->alloc - old_alloc));
    }

    str_idx = path->next_idx ? path->next_idx : top_str;

/* Temporary modify MCTX.  */
    backup_state_log = mctx->state_log;
    backup_cur_idx = mctx->input.cur_idx;
    mctx->state_log = path->array;
    mctx->input.cur_idx = str_idx;

/* Setup initial node set.  */
    context = re_string_context_at(&mctx->input, str_idx - 1, mctx->eflags);
    if (str_idx == top_str) {
        err = re_node_set_init_1(&next_nodes, top_node);
        if ((err != REG_NOERROR))
            return err;
        err = check_arrival_expand_ecl(dfa, &next_nodes, subexp_num, type);
        if ((err != REG_NOERROR)) {
            re_node_set_free(&next_nodes);
            return err;
        }
    } else {
        cur_state = mctx->state_log[str_idx];
        if (cur_state && cur_state->has_backref) {
            err = re_node_set_init_copy(&next_nodes, &cur_state->nodes);
            if ((err != REG_NOERROR))
                return err;
        } else
            re_node_set_init_empty(&next_nodes);
    }
    if (str_idx == top_str || (cur_state && cur_state->has_backref)) {
        if (next_nodes.nelem) {
            err = expand_bkref_cache(mctx, &next_nodes, str_idx,
                                     subexp_num, type);
            if ((err != REG_NOERROR)) {
                re_node_set_free(&next_nodes);
                return err;
            }
        }
        cur_state = re_acquire_state_context(&err, dfa, &next_nodes, context);
        if ((cur_state == NULL && err != REG_NOERROR)) {
            re_node_set_free(&next_nodes);
            return err;
        }
        mctx->state_log[str_idx] = cur_state;
    }

    for (null_cnt = 0; str_idx < last_str && null_cnt <= mctx->max_mb_elem_len;) {
        re_node_set_empty(&next_nodes);
        if (mctx->state_log[str_idx + 1]) {
            err = re_node_set_merge(&next_nodes,
                                    &mctx->state_log[str_idx + 1]->nodes);
            if ((err != REG_NOERROR)) {
                re_node_set_free(&next_nodes);
                return err;
            }
        }
        if (cur_state) {
            err = check_arrival_add_next_nodes(mctx, str_idx,
                                               &cur_state->non_eps_nodes,
                                               &next_nodes);
            if ((err != REG_NOERROR)) {
                re_node_set_free(&next_nodes);
                return err;
            }
        }
        ++str_idx;
        if (next_nodes.nelem) {
            err = check_arrival_expand_ecl(dfa, &next_nodes, subexp_num, type);
            if ((err != REG_NOERROR)) {
                re_node_set_free(&next_nodes);
                return err;
            }
            err = expand_bkref_cache(mctx, &next_nodes, str_idx,
                                     subexp_num, type);
            if ((err != REG_NOERROR)) {
                re_node_set_free(&next_nodes);
                return err;
            }
        }
        context = re_string_context_at(&mctx->input, str_idx - 1, mctx->eflags);
        cur_state = re_acquire_state_context(&err, dfa, &next_nodes, context);
        if ((cur_state == NULL && err != REG_NOERROR)) {
            re_node_set_free(&next_nodes);
            return err;
        }
        mctx->state_log[str_idx] = cur_state;
        null_cnt = cur_state == NULL ? null_cnt + 1 : 0;
    }
    re_node_set_free(&next_nodes);
    cur_nodes = (mctx->state_log[last_str] == NULL ? NULL
                                                   : &mctx->state_log[last_str]->nodes);
    path->next_idx = str_idx;

/* Fix MCTX.  */
    mctx->state_log = backup_state_log;
    mctx->input.cur_idx = backup_cur_idx;

/* Then check the current node set has the node LAST_NODE.  */
    if (cur_nodes != NULL && re_node_set_contains(cur_nodes, last_node))
        return REG_NOERROR;

    return REG_NOMATCH;
}

/* Helper functions for check_arrival.  */

/* Calculate the destination nodes of CUR_NODES at STR_IDX, and append them
   to NEXT_NODES.
   TODO: This function is similar to the functions transit_state*(),
	 however this function has many additional works.
	 Can't we unify them?  */

static reg_errcode_t

check_arrival_add_next_nodes(re_match_context_t *mctx, Idx str_idx,
                             re_node_set *cur_nodes, re_node_set *next_nodes) {
    const re_dfa_t *const dfa = mctx->dfa;
    bool ok;
    Idx cur_idx;
    re_node_set union_set;
    re_node_set_init_empty(&union_set);
    for (cur_idx = 0; cur_idx < cur_nodes->nelem; ++cur_idx) {
        int naccepted = 0;
        Idx cur_node = cur_nodes->elems[cur_idx];
        if (naccepted
            || check_node_accept(mctx, dfa->nodes + cur_node, str_idx)) {
            ok = re_node_set_insert(next_nodes, dfa->nexts[cur_node]);
            if ((!ok)) {
                re_node_set_free(&union_set);
                return REG_ESPACE;
            }
        }
    }
    re_node_set_free(&union_set);
    return REG_NOERROR;
}

/* For all the nodes in CUR_NODES, add the epsilon closures of them to
   CUR_NODES, however exclude the nodes which are:
    - inside the sub expression whose number is EX_SUBEXP, if FL_OPEN.
    - out of the sub expression whose number is EX_SUBEXP, if !FL_OPEN.
*/

static reg_errcode_t
check_arrival_expand_ecl(const re_dfa_t *dfa, re_node_set *cur_nodes,
                         Idx ex_subexp, int type) {
    reg_errcode_t err;
    Idx idx, outside_node;
    re_node_set new_nodes;
    err = re_node_set_alloc(&new_nodes, cur_nodes->nelem);
    if ((err != REG_NOERROR))
        return err;
    /* Create a new node set NEW_NODES with the nodes which are epsilon
       closures of the node in CUR_NODES.  */

    for (idx = 0; idx < cur_nodes->nelem; ++idx) {
        Idx cur_node = cur_nodes->elems[idx];
        const re_node_set *eclosure = dfa->eclosures + cur_node;
        outside_node = find_subexp_node(dfa, eclosure, ex_subexp, type);
        if (outside_node == -1) {
            /* There are no problematic nodes, just merge them.  */
            err = re_node_set_merge(&new_nodes, eclosure);
            if ((err != REG_NOERROR)) {
                re_node_set_free(&new_nodes);
                return err;
            }
        } else {
            /* There are problematic nodes, re-calculate incrementally.  */
            err = check_arrival_expand_ecl_sub(dfa, &new_nodes, cur_node,
                                               ex_subexp, type);
            if ((err != REG_NOERROR)) {
                re_node_set_free(&new_nodes);
                return err;
            }
        }
    }
    re_node_set_free(cur_nodes);
    *cur_nodes = new_nodes;
    return REG_NOERROR;
}

/* Helper function for check_arrival_expand_ecl.
   Check incrementally the epsilon closure of TARGET, and if it isn't
   problematic append it to DST_NODES.  */

static reg_errcode_t

check_arrival_expand_ecl_sub(const re_dfa_t *dfa, re_node_set *dst_nodes,
                             Idx target, Idx ex_subexp, int type) {
    Idx cur_node;
    for (cur_node = target; !re_node_set_contains(dst_nodes, cur_node);) {
        bool ok;

        if (dfa->nodes[cur_node].type == type
            && dfa->nodes[cur_node].opr.idx == ex_subexp) {
            if (type == OP_CLOSE_SUBEXP) {
                ok = re_node_set_insert(dst_nodes, cur_node);
                if ((!ok))
                    return REG_ESPACE;
            }
            break;
        }
        ok = re_node_set_insert(dst_nodes, cur_node);
        if ((!ok))
            return REG_ESPACE;
        if (dfa->edests[cur_node].nelem == 0)
            break;
        if (dfa->edests[cur_node].nelem == 2) {
            reg_errcode_t err;
            err = check_arrival_expand_ecl_sub(dfa, dst_nodes,
                                               dfa->edests[cur_node].elems[1],
                                               ex_subexp, type);
            if ((err != REG_NOERROR))
                return err;
        }
        cur_node = dfa->edests[cur_node].elems[0];
    }
    return REG_NOERROR;
}

/* For all the back references in the current state, calculate the
   destination of the back references by the appropriate entry
   in MCTX->BKREF_ENTS.  */

static reg_errcode_t
expand_bkref_cache(re_match_context_t *mctx, re_node_set *cur_nodes,
                   Idx cur_str, Idx subexp_num, int type) {
    const re_dfa_t *const dfa = mctx->dfa;
    reg_errcode_t err;
    Idx cache_idx_start = search_cur_bkref_entry(mctx, cur_str);
    struct re_backref_cache_entry *ent;

    if (cache_idx_start == -1)
        return REG_NOERROR;

    restart:
    ent = mctx->bkref_ents + cache_idx_start;
    do {
        Idx to_idx, next_node;

/* Is this entry ENT is appropriate?  */
        if (!re_node_set_contains(cur_nodes, ent->node))
            continue; /* No.  */

        to_idx = cur_str + ent->subexp_to - ent->subexp_from;
/* Calculate the destination of the back reference, and append it
to MCTX->STATE_LOG.  */
        if (to_idx == cur_str) {
/* The backreference did epsilon transit, we must re-check all the
   node in the current state.  */
            re_node_set new_dests;
            reg_errcode_t err2, err3;
            next_node = dfa->edests[ent->node].elems[0];
            if (re_node_set_contains(cur_nodes, next_node))
                continue;
            err = re_node_set_init_1(&new_dests, next_node);
            err2 = check_arrival_expand_ecl(dfa, &new_dests, subexp_num, type);
            err3 = re_node_set_merge(cur_nodes, &new_dests);
            re_node_set_free(&new_dests);
            if ((err != REG_NOERROR || err2 != REG_NOERROR
                 || err3 != REG_NOERROR)) {
                err = (err != REG_NOERROR ? err
                                          : (err2 != REG_NOERROR ? err2 : err3));
                return err;
            }
/* TODO: It is still inefficient...  */
            goto restart;
        } else {
            re_node_set union_set;
            next_node = dfa->nexts[ent->node];
            if (mctx->state_log[to_idx]) {
                bool ok;
                if (re_node_set_contains(&mctx->state_log[to_idx]->nodes,
                                         next_node))
                    continue;
                err = re_node_set_init_copy(&union_set,
                                            &mctx->state_log[to_idx]->nodes);
                ok = re_node_set_insert(&union_set, next_node);
                if ((err != REG_NOERROR || !ok)) {
                    re_node_set_free(&union_set);
                    err = err != REG_NOERROR ? err : REG_ESPACE;
                    return err;
                }
            } else {
                err = re_node_set_init_1(&union_set, next_node);
                if ((err != REG_NOERROR))
                    return err;
            }
            mctx->state_log[to_idx] = re_acquire_state(&err, dfa, &union_set);
            re_node_set_free(&union_set);
            if ((mctx->state_log[to_idx] == NULL
                 && err != REG_NOERROR))
                return err;
        }
    } while (ent++->more);
    return REG_NOERROR;
}

/* Build transition table for the state.
   Return true if successful.  */

static bool
build_trtable (const re_dfa_t *dfa, re_dfastate_t *state)
{
    reg_errcode_t err;
    Idx i, j;
    int ch;
    bool need_word_trtable = false;
    bitset_word_t elem, mask;
    bool dests_node_malloced = false;
    bool dest_states_malloced = false;
    Idx ndests; /* Number of the destination states from 'state'.  */
    re_dfastate_t **trtable;
    re_dfastate_t **dest_states = NULL, **dest_states_word, **dest_states_nl;
    re_node_set follows, *dests_node;
    bitset_t *dests_ch;
    bitset_t acceptable;

    struct dests_alloc
    {
        re_node_set dests_node[SBC_MAX];
        bitset_t dests_ch[SBC_MAX];
    } *dests_alloc;

    /* We build DFA states which corresponds to the destination nodes
       from 'state'.  'dests_node[i]' represents the nodes which i-th
       destination state contains, and 'dests_ch[i]' represents the
       characters which i-th destination state accepts.  */
    if (__libc_use_alloca (sizeof (struct dests_alloc)))
        dests_alloc = (struct dests_alloc *) alloca (sizeof (struct dests_alloc));
    else
    {
        dests_alloc = re_malloc (struct dests_alloc, 1);
        if ( (dests_alloc == NULL))
            return false;
        dests_node_malloced = true;
    }
    dests_node = dests_alloc->dests_node;
    dests_ch = dests_alloc->dests_ch;

    /* Initialize transition table.  */
    state->word_trtable = state->trtable = NULL;

    /* At first, group all nodes belonging to 'state' into several
       destinations.  */
    ndests = group_nodes_into_DFAstates (dfa, state, dests_node, dests_ch);
    if ( (ndests <= 0))
    {
        if (dests_node_malloced)
            re_free (dests_alloc);
        /* Return false in case of an error, true otherwise.  */
        if (ndests == 0)
        {
            state->trtable = (re_dfastate_t **)
                    calloc (sizeof (re_dfastate_t *), SBC_MAX);
            if ( (state->trtable == NULL))
                return false;
            return true;
        }
        return false;
    }

    err = re_node_set_alloc (&follows, ndests + 1);
    if ( (err != REG_NOERROR))
        goto out_free;

    /* Avoid arithmetic overflow in size calculation.  */
    size_t ndests_max
            = ((SIZE_MAX - (sizeof (re_node_set) + sizeof (bitset_t)) * SBC_MAX)
               / (3 * sizeof (re_dfastate_t *)));
    if ( (ndests_max < ndests))
        goto out_free;

    if (__libc_use_alloca ((sizeof (re_node_set) + sizeof (bitset_t)) * SBC_MAX
                           + ndests * 3 * sizeof (re_dfastate_t *)))
        dest_states = (re_dfastate_t **)
                alloca (ndests * 3 * sizeof (re_dfastate_t *));
    else
    {
        dest_states = re_malloc (re_dfastate_t *, ndests * 3);
        if ( (dest_states == NULL))
        {
            out_free:
            if (dest_states_malloced)
                re_free (dest_states);
            re_node_set_free (&follows);
            for (i = 0; i < ndests; ++i)
                re_node_set_free (dests_node + i);
            if (dests_node_malloced)
                re_free (dests_alloc);
            return false;
        }
        dest_states_malloced = true;
    }
    dest_states_word = dest_states + ndests;
    dest_states_nl = dest_states_word + ndests;
    bitset_empty (acceptable);

    /* Then build the states for all destinations.  */
    for (i = 0; i < ndests; ++i)
    {
        Idx next_node;
        re_node_set_empty (&follows);
        /* Merge the follows of this destination states.  */
        for (j = 0; j < dests_node[i].nelem; ++j)
        {
            next_node = dfa->nexts[dests_node[i].elems[j]];
            if (next_node != -1)
            {
                err = re_node_set_merge (&follows, dfa->eclosures + next_node);
                if ( (err != REG_NOERROR))
                    goto out_free;
            }
        }
        dest_states[i] = re_acquire_state_context (&err, dfa, &follows, 0);
        if ( (dest_states[i] == NULL && err != REG_NOERROR))
            goto out_free;
        /* If the new state has context constraint,
       build appropriate states for these contexts.  */
        if (dest_states[i]->has_constraint)
        {
            dest_states_word[i] = re_acquire_state_context (&err, dfa, &follows,
                                                            CONTEXT_WORD);
            if ( (dest_states_word[i] == NULL
                                  && err != REG_NOERROR))
                goto out_free;

            if (dest_states[i] != dest_states_word[i] && dfa->mb_cur_max > 1)
                need_word_trtable = true;

            dest_states_nl[i] = re_acquire_state_context (&err, dfa, &follows,
                                                          CONTEXT_NEWLINE);
            if ( (dest_states_nl[i] == NULL && err != REG_NOERROR))
                goto out_free;
        }
        else
        {
            dest_states_word[i] = dest_states[i];
            dest_states_nl[i] = dest_states[i];
        }
        bitset_merge (acceptable, dests_ch[i]);
    }

    if (! (need_word_trtable))
    {
        /* We don't care about whether the following character is a word
       character, or we are in a single-byte character set so we can
       discern by looking at the character code: allocate a
       256-entry transition table.  */
        trtable = state->trtable =
                (re_dfastate_t **) calloc (sizeof (re_dfastate_t *), SBC_MAX);
        if ( (trtable == NULL))
            goto out_free;

        /* For all characters ch...:  */
        for (i = 0; i < BITSET_WORDS; ++i)
            for (ch = i * BITSET_WORD_BITS, elem = acceptable[i], mask = 1;
                 elem;
                 mask <<= 1, elem >>= 1, ++ch)
                if ( (elem & 1))
                {
                    /* There must be exactly one destination which accepts
                   character ch.  See group_nodes_into_DFAstates.  */
                    for (j = 0; (dests_ch[j][i] & mask) == 0; ++j)
                        ;

                    /* j-th destination accepts the word character ch.  */
                    if (dfa->word_char[i] & mask)
                        trtable[ch] = dest_states_word[j];
                    else
                        trtable[ch] = dest_states[j];
                }
    }
    else
    {
        /* We care about whether the following character is a word
       character, and we are in a multi-byte character set: discern
       by looking at the character code: build two 256-entry
       transition tables, one starting at trtable[0] and one
       starting at trtable[SBC_MAX].  */
        trtable = state->word_trtable =
                (re_dfastate_t **) calloc (sizeof (re_dfastate_t *), 2 * SBC_MAX);
        if ( (trtable == NULL))
            goto out_free;

        /* For all characters ch...:  */
        for (i = 0; i < BITSET_WORDS; ++i)
            for (ch = i * BITSET_WORD_BITS, elem = acceptable[i], mask = 1;
                 elem;
                 mask <<= 1, elem >>= 1, ++ch)
                if ( (elem & 1))
                {
                    /* There must be exactly one destination which accepts
                   character ch.  See group_nodes_into_DFAstates.  */
                    for (j = 0; (dests_ch[j][i] & mask) == 0; ++j)
                        ;

                    /* j-th destination accepts the word character ch.  */
                    trtable[ch] = dest_states[j];
                    trtable[ch + SBC_MAX] = dest_states_word[j];
                }
    }

    /* new line */
    if (bitset_contain (acceptable, NEWLINE_CHAR))
    {
        /* The current state accepts newline character.  */
        for (j = 0; j < ndests; ++j)
            if (bitset_contain (dests_ch[j], NEWLINE_CHAR))
            {
                /* k-th destination accepts newline character.  */
                trtable[NEWLINE_CHAR] = dest_states_nl[j];
                if (need_word_trtable)
                    trtable[NEWLINE_CHAR + SBC_MAX] = dest_states_nl[j];
                /* There must be only one destination which accepts
                   newline.  See group_nodes_into_DFAstates.  */
                break;
            }
    }

    if (dest_states_malloced)
        re_free (dest_states);

    re_node_set_free (&follows);
    for (i = 0; i < ndests; ++i)
        re_node_set_free (dests_node + i);

    if (dests_node_malloced)
        re_free (dests_alloc);

    return true;
}

/* Group all nodes belonging to STATE into several destinations.
   Then for all destinations, set the nodes belonging to the destination
   to DESTS_NODE[i] and set the characters accepted by the destination
   to DEST_CH[i].  This function return the number of destinations.  */

static Idx
group_nodes_into_DFAstates (const re_dfa_t *dfa, const re_dfastate_t *state,
                            re_node_set *dests_node, bitset_t *dests_ch)
{
    reg_errcode_t err;
    bool ok;
    Idx i, j, k;
    Idx ndests; /* Number of the destinations from 'state'.  */
    bitset_t accepts; /* Characters a node can accept.  */
    const re_node_set *cur_nodes = &state->nodes;
    bitset_empty (accepts);
    ndests = 0;

    /* For all the nodes belonging to 'state',  */
    for (i = 0; i < cur_nodes->nelem; ++i)
    {
        re_token_t *node = &dfa->nodes[cur_nodes->elems[i]];
        re_token_type_t type = node->type;
        unsigned int constraint = node->constraint;

        /* Enumerate all single byte character this node can accept.  */
        if (type == CHARACTER)
            bitset_set (accepts, node->opr.c);
        else if (type == SIMPLE_BRACKET)
        {
            bitset_merge (accepts, node->opr.sbcset);
        }
        else if (type == OP_PERIOD)
        {
            bitset_set_all (accepts);
            if (!(dfa->syntax & RE_DOT_NEWLINE))
                bitset_clear (accepts, '\n');
            if (dfa->syntax & RE_DOT_NOT_NULL)
                bitset_clear (accepts, '\0');
        }
        else
            continue;

        /* Check the 'accepts' and sift the characters which are not
       match it the context.  */
        if (constraint)
        {
            if (constraint & NEXT_NEWLINE_CONSTRAINT)
            {
                bool accepts_newline = bitset_contain (accepts, NEWLINE_CHAR);
                bitset_empty (accepts);
                if (accepts_newline)
                    bitset_set (accepts, NEWLINE_CHAR);
                else
                    continue;
            }
            if (constraint & NEXT_ENDBUF_CONSTRAINT)
            {
                bitset_empty (accepts);
                continue;
            }

            if (constraint & NEXT_WORD_CONSTRAINT)
            {
                bitset_word_t any_set = 0;
                if (type == CHARACTER && !node->word_char)
                {
                    bitset_empty (accepts);
                    continue;
                }
                for (j = 0; j < BITSET_WORDS; ++j)
                    any_set |= (accepts[j] &= dfa->word_char[j]);
                if (!any_set)
                    continue;
            }
            if (constraint & NEXT_NOTWORD_CONSTRAINT)
            {
                bitset_word_t any_set = 0;
                if (type == CHARACTER && node->word_char)
                {
                    bitset_empty (accepts);
                    continue;
                }
                for (j = 0; j < BITSET_WORDS; ++j)
                    any_set |= (accepts[j] &= ~dfa->word_char[j]);
                if (!any_set)
                    continue;
            }
        }

        /* Then divide 'accepts' into DFA states, or create a new
       state.  Above, we make sure that accepts is not empty.  */
        for (j = 0; j < ndests; ++j)
        {
            bitset_t intersec; /* Intersection sets, see below.  */
            bitset_t remains;
            /* Flags, see below.  */
            bitset_word_t has_intersec, not_subset, not_consumed;

            /* Optimization, skip if this state doesn't accept the character.  */
            if (type == CHARACTER && !bitset_contain (dests_ch[j], node->opr.c))
                continue;

            /* Enumerate the intersection set of this state and 'accepts'.  */
            has_intersec = 0;
            for (k = 0; k < BITSET_WORDS; ++k)
                has_intersec |= intersec[k] = accepts[k] & dests_ch[j][k];
            /* And skip if the intersection set is empty.  */
            if (!has_intersec)
                continue;

            /* Then check if this state is a subset of 'accepts'.  */
            not_subset = not_consumed = 0;
            for (k = 0; k < BITSET_WORDS; ++k)
            {
                not_subset |= remains[k] = ~accepts[k] & dests_ch[j][k];
                not_consumed |= accepts[k] = accepts[k] & ~dests_ch[j][k];
            }

            /* If this state isn't a subset of 'accepts', create a
               new group state, which has the 'remains'. */
            if (not_subset)
            {
                bitset_copy (dests_ch[ndests], remains);
                bitset_copy (dests_ch[j], intersec);
                err = re_node_set_init_copy (dests_node + ndests, &dests_node[j]);
                if ( (err != REG_NOERROR))
                    goto error_return;
                ++ndests;
            }

            /* Put the position in the current group. */
            ok = re_node_set_insert (&dests_node[j], cur_nodes->elems[i]);
            if ( (! ok))
                goto error_return;

            /* If all characters are consumed, go to next node. */
            if (!not_consumed)
                break;
        }
        /* Some characters remain, create a new group. */
        if (j == ndests)
        {
            bitset_copy (dests_ch[ndests], accepts);
            err = re_node_set_init_1 (dests_node + ndests, cur_nodes->elems[i]);
            if ( (err != REG_NOERROR))
                goto error_return;
            ++ndests;
            bitset_empty (accepts);
        }
    }
    return ndests;
    error_return:
    for (j = 0; j < ndests; ++j)
        re_node_set_free (dests_node + j);
    return -1;
}


static reg_errcode_t
update_cur_sifted_state (const re_match_context_t *mctx,
                         re_sift_context_t *sctx, Idx str_idx,
                         re_node_set *dest_nodes)
{
    const re_dfa_t *const dfa = mctx->dfa;
    reg_errcode_t err = REG_NOERROR;
    const re_node_set *candidates;
    candidates = ((mctx->state_log[str_idx] == NULL) ? NULL
                                                     : &mctx->state_log[str_idx]->nodes);

    if (dest_nodes->nelem == 0)
        sctx->sifted_states[str_idx] = NULL;
    else
    {
        if (candidates)
        {
            /* At first, add the nodes which can epsilon transit to a node in
               DEST_NODE.  */
            err = add_epsilon_src_nodes (dfa, dest_nodes, candidates);
            if ( (err != REG_NOERROR))
                return err;

            /* Then, check the limitations in the current sift_context.  */
            if (sctx->limits.nelem)
            {
                err = check_subexp_limits (dfa, dest_nodes, candidates, &sctx->limits,
                                           mctx->bkref_ents, str_idx);
                if ( (err != REG_NOERROR))
                    return err;
            }
        }

        sctx->sifted_states[str_idx] = re_acquire_state (&err, dfa, dest_nodes);
        if ( (err != REG_NOERROR))
            return err;
    }

    if (candidates && mctx->state_log[str_idx]->has_backref)
    {
        err = sift_states_bkref (mctx, sctx, str_idx, candidates);
        if ( (err != REG_NOERROR))
            return err;
    }
    return REG_NOERROR;
}

static reg_errcode_t
        
add_epsilon_src_nodes (const re_dfa_t *dfa, re_node_set *dest_nodes,
                       const re_node_set *candidates)
{
    reg_errcode_t err = REG_NOERROR;
    Idx i;

    re_dfastate_t *state = re_acquire_state (&err, dfa, dest_nodes);
    if ( (err != REG_NOERROR))
        return err;

    if (!state->inveclosure.alloc)
    {
        err = re_node_set_alloc (&state->inveclosure, dest_nodes->nelem);
        if ( (err != REG_NOERROR))
            return REG_ESPACE;
        for (i = 0; i < dest_nodes->nelem; i++)
        {
            err = re_node_set_merge (&state->inveclosure,
                                     dfa->inveclosures + dest_nodes->elems[i]);
            if ( (err != REG_NOERROR))
                return REG_ESPACE;
        }
    }
    return re_node_set_add_intersect (dest_nodes, candidates,
                                      &state->inveclosure);
}

static reg_errcode_t
sub_epsilon_src_nodes (const re_dfa_t *dfa, Idx node, re_node_set *dest_nodes,
                       const re_node_set *candidates)
{
    Idx ecl_idx;
    reg_errcode_t err;
    re_node_set *inv_eclosure = dfa->inveclosures + node;
    re_node_set except_nodes;
    re_node_set_init_empty (&except_nodes);
    for (ecl_idx = 0; ecl_idx < inv_eclosure->nelem; ++ecl_idx)
    {
        Idx cur_node = inv_eclosure->elems[ecl_idx];
        if (cur_node == node)
            continue;
        if (IS_EPSILON_NODE (dfa->nodes[cur_node].type))
        {
            Idx edst1 = dfa->edests[cur_node].elems[0];
            Idx edst2 = ((dfa->edests[cur_node].nelem > 1)
                         ? dfa->edests[cur_node].elems[1] : -1);
            if ((!re_node_set_contains (inv_eclosure, edst1)
                 && re_node_set_contains (dest_nodes, edst1))
                || (edst2 > 0
                    && !re_node_set_contains (inv_eclosure, edst2)
                    && re_node_set_contains (dest_nodes, edst2)))
            {
                err = re_node_set_add_intersect (&except_nodes, candidates,
                                                 dfa->inveclosures + cur_node);
                if ( (err != REG_NOERROR))
                {
                    re_node_set_free (&except_nodes);
                    return err;
                }
            }
        }
    }
    for (ecl_idx = 0; ecl_idx < inv_eclosure->nelem; ++ecl_idx)
    {
        Idx cur_node = inv_eclosure->elems[ecl_idx];
        if (!re_node_set_contains (&except_nodes, cur_node))
        {
            Idx idx = re_node_set_contains (dest_nodes, cur_node) - 1;
            re_node_set_remove_at (dest_nodes, idx);
        }
    }
    re_node_set_free (&except_nodes);
    return REG_NOERROR;
}

static bool
check_dst_limits (const re_match_context_t *mctx, const re_node_set *limits,
                  Idx dst_node, Idx dst_idx, Idx src_node, Idx src_idx)
{
    const re_dfa_t *const dfa = mctx->dfa;
    Idx lim_idx, src_pos, dst_pos;

    Idx dst_bkref_idx = search_cur_bkref_entry (mctx, dst_idx);
    Idx src_bkref_idx = search_cur_bkref_entry (mctx, src_idx);
    for (lim_idx = 0; lim_idx < limits->nelem; ++lim_idx)
    {
        Idx subexp_idx;
        struct re_backref_cache_entry *ent;
        ent = mctx->bkref_ents + limits->elems[lim_idx];
        subexp_idx = dfa->nodes[ent->node].opr.idx;

        dst_pos = check_dst_limits_calc_pos (mctx, limits->elems[lim_idx],
                                             subexp_idx, dst_node, dst_idx,
                                             dst_bkref_idx);
        src_pos = check_dst_limits_calc_pos (mctx, limits->elems[lim_idx],
                                             subexp_idx, src_node, src_idx,
                                             src_bkref_idx);

        /* In case of:
       <src> <dst> ( <subexp> )
       ( <subexp> ) <src> <dst>
       ( <subexp1> <src> <subexp2> <dst> <subexp3> )  */
        if (src_pos == dst_pos)
            continue; /* This is unrelated limitation.  */
        else
            return true;
    }
    return false;
}

static int
check_dst_limits_calc_pos_1 (const re_match_context_t *mctx, int boundaries,
                             Idx subexp_idx, Idx from_node, Idx bkref_idx)
{
    const re_dfa_t *const dfa = mctx->dfa;
    const re_node_set *eclosures = dfa->eclosures + from_node;
    Idx node_idx;

    /* Else, we are on the boundary: examine the nodes on the epsilon
       closure.  */
    for (node_idx = 0; node_idx < eclosures->nelem; ++node_idx)
    {
        Idx node = eclosures->elems[node_idx];
        switch (dfa->nodes[node].type)
        {
            case OP_BACK_REF:
                if (bkref_idx != -1)
                {
                    struct re_backref_cache_entry *ent = mctx->bkref_ents + bkref_idx;
                    do
                    {
                        Idx dst;
                        int cpos;

                        if (ent->node != node)
                            continue;

                        if (subexp_idx < BITSET_WORD_BITS
                            && !(ent->eps_reachable_subexps_map
                                 & ((bitset_word_t) 1 << subexp_idx)))
                            continue;

                        /* Recurse trying to reach the OP_OPEN_SUBEXP and
                           OP_CLOSE_SUBEXP cases below.  But, if the
                           destination node is the same node as the source
                           node, don't recurse because it would cause an
                           infinite loop: a regex that exhibits this behavior
                           is ()\1*\1*  */
                        dst = dfa->edests[node].elems[0];
                        if (dst == from_node)
                        {
                            if (boundaries & 1)
                                return -1;
                            else /* if (boundaries & 2) */
                                return 0;
                        }

                        cpos =
                                check_dst_limits_calc_pos_1 (mctx, boundaries, subexp_idx,
                                                             dst, bkref_idx);
                        if (cpos == -1 /* && (boundaries & 1) */)
                            return -1;
                        if (cpos == 0 && (boundaries & 2))
                            return 0;

                        if (subexp_idx < BITSET_WORD_BITS)
                            ent->eps_reachable_subexps_map
                                    &= ~((bitset_word_t) 1 << subexp_idx);
                    }
                    while (ent++->more);
                }
                break;

            case OP_OPEN_SUBEXP:
                if ((boundaries & 1) && subexp_idx == dfa->nodes[node].opr.idx)
                    return -1;
                break;

            case OP_CLOSE_SUBEXP:
                if ((boundaries & 2) && subexp_idx == dfa->nodes[node].opr.idx)
                    return 0;
                break;

            default:
                break;
        }
    }

    return (boundaries & 2) ? 1 : 0;
}

static int
check_dst_limits_calc_pos (const re_match_context_t *mctx, Idx limit,
                           Idx subexp_idx, Idx from_node, Idx str_idx,
                           Idx bkref_idx)
{
    struct re_backref_cache_entry *lim = mctx->bkref_ents + limit;
    int boundaries;

    /* If we are outside the range of the subexpression, return -1 or 1.  */
    if (str_idx < lim->subexp_from)
        return -1;

    if (lim->subexp_to < str_idx)
        return 1;

    /* If we are within the subexpression, return 0.  */
    boundaries = (str_idx == lim->subexp_from);
    boundaries |= (str_idx == lim->subexp_to) << 1;
    if (boundaries == 0)
        return 0;

    /* Else, examine epsilon closure.  */
    return check_dst_limits_calc_pos_1 (mctx, boundaries, subexp_idx,
                                        from_node, bkref_idx);
}

/* Check the limitations of sub expressions LIMITS, and remove the nodes
   which are against limitations from DEST_NODES. */

static reg_errcode_t
check_subexp_limits (const re_dfa_t *dfa, re_node_set *dest_nodes,
                     const re_node_set *candidates, re_node_set *limits,
                     struct re_backref_cache_entry *bkref_ents, Idx str_idx)
{
    reg_errcode_t err;
    Idx node_idx, lim_idx;

    for (lim_idx = 0; lim_idx < limits->nelem; ++lim_idx)
    {
        Idx subexp_idx;
        struct re_backref_cache_entry *ent;
        ent = bkref_ents + limits->elems[lim_idx];

        if (str_idx <= ent->subexp_from || ent->str_idx < str_idx)
            continue; /* This is unrelated limitation.  */

        subexp_idx = dfa->nodes[ent->node].opr.idx;
        if (ent->subexp_to == str_idx)
        {
            Idx ops_node = -1;
            Idx cls_node = -1;
            for (node_idx = 0; node_idx < dest_nodes->nelem; ++node_idx)
            {
                Idx node = dest_nodes->elems[node_idx];
                re_token_type_t type = dfa->nodes[node].type;
                if (type == OP_OPEN_SUBEXP
                    && subexp_idx == dfa->nodes[node].opr.idx)
                    ops_node = node;
                else if (type == OP_CLOSE_SUBEXP
                         && subexp_idx == dfa->nodes[node].opr.idx)
                    cls_node = node;
            }

            /* Check the limitation of the open subexpression.  */
            /* Note that (ent->subexp_to = str_idx != ent->subexp_from).  */
            if (ops_node >= 0)
            {
                err = sub_epsilon_src_nodes (dfa, ops_node, dest_nodes,
                                             candidates);
                if ( (err != REG_NOERROR))
                    return err;
            }

            /* Check the limitation of the close subexpression.  */
            if (cls_node >= 0)
                for (node_idx = 0; node_idx < dest_nodes->nelem; ++node_idx)
                {
                    Idx node = dest_nodes->elems[node_idx];
                    if (!re_node_set_contains (dfa->inveclosures + node,
                                               cls_node)
                        && !re_node_set_contains (dfa->eclosures + node,
                                                  cls_node))
                    {
                        /* It is against this limitation.
                           Remove it form the current sifted state.  */
                        err = sub_epsilon_src_nodes (dfa, node, dest_nodes,
                                                     candidates);
                        if ( (err != REG_NOERROR))
                            return err;
                        --node_idx;
                    }
                }
        }
        else /* (ent->subexp_to != str_idx)  */
        {
            for (node_idx = 0; node_idx < dest_nodes->nelem; ++node_idx)
            {
                Idx node = dest_nodes->elems[node_idx];
                re_token_type_t type = dfa->nodes[node].type;
                if (type == OP_CLOSE_SUBEXP || type == OP_OPEN_SUBEXP)
                {
                    if (subexp_idx != dfa->nodes[node].opr.idx)
                        continue;
                    /* It is against this limitation.
                       Remove it form the current sifted state.  */
                    err = sub_epsilon_src_nodes (dfa, node, dest_nodes,
                                                 candidates);
                    if ( (err != REG_NOERROR))
                        return err;
                }
            }
        }
    }
    return REG_NOERROR;
}

static reg_errcode_t
        
sift_states_bkref (const re_match_context_t *mctx, re_sift_context_t *sctx,
                   Idx str_idx, const re_node_set *candidates)
{
    const re_dfa_t *const dfa = mctx->dfa;
    reg_errcode_t err;
    Idx node_idx, node;
    re_sift_context_t local_sctx;
    Idx first_idx = search_cur_bkref_entry (mctx, str_idx);

    if (first_idx == -1)
        return REG_NOERROR;

    local_sctx.sifted_states = NULL; /* Mark that it hasn't been initialized.  */

    for (node_idx = 0; node_idx < candidates->nelem; ++node_idx)
    {
        Idx enabled_idx;
        re_token_type_t type;
        struct re_backref_cache_entry *entry;
        node = candidates->elems[node_idx];
        type = dfa->nodes[node].type;
        /* Avoid infinite loop for the REs like "()\1+".  */
        if (node == sctx->last_node && str_idx == sctx->last_str_idx)
            continue;
        if (type != OP_BACK_REF)
            continue;

        entry = mctx->bkref_ents + first_idx;
        enabled_idx = first_idx;
        do
        {
            Idx subexp_len;
            Idx to_idx;
            Idx dst_node;
            bool ok;
            re_dfastate_t *cur_state;

            if (entry->node != node)
                continue;
            subexp_len = entry->subexp_to - entry->subexp_from;
            to_idx = str_idx + subexp_len;
            dst_node = (subexp_len ? dfa->nexts[node]
                                   : dfa->edests[node].elems[0]);

            if (to_idx > sctx->last_str_idx
                || sctx->sifted_states[to_idx] == NULL
                || !STATE_NODE_CONTAINS (sctx->sifted_states[to_idx], dst_node)
                || check_dst_limits (mctx, &sctx->limits, node,
                                     str_idx, dst_node, to_idx))
                continue;

            if (local_sctx.sifted_states == NULL)
            {
                local_sctx = *sctx;
                err = re_node_set_init_copy (&local_sctx.limits, &sctx->limits);
                if ( (err != REG_NOERROR))
                    goto free_return;
            }
            local_sctx.last_node = node;
            local_sctx.last_str_idx = str_idx;
            ok = re_node_set_insert (&local_sctx.limits, enabled_idx);
            if ( (! ok))
            {
                err = REG_ESPACE;
                goto free_return;
            }
            cur_state = local_sctx.sifted_states[str_idx];
            err = sift_states_backward (mctx, &local_sctx);
            if ( (err != REG_NOERROR))
                goto free_return;
            if (sctx->limited_states != NULL)
            {
                err = merge_state_array (dfa, sctx->limited_states,
                                         local_sctx.sifted_states,
                                         str_idx + 1);
                if ( (err != REG_NOERROR))
                    goto free_return;
            }
            local_sctx.sifted_states[str_idx] = cur_state;
            re_node_set_remove (&local_sctx.limits, enabled_idx);

            /* mctx->bkref_ents may have changed, reload the pointer.  */
            entry = mctx->bkref_ents + enabled_idx;
        }
        while (enabled_idx++, entry++->more);
    }
    err = REG_NOERROR;
    free_return:
    if (local_sctx.sifted_states != NULL)
    {
        re_node_set_free (&local_sctx.limits);
    }

    return err;
}


static reg_errcode_t
transit_state_bkref(re_match_context_t *mctx, const re_node_set *nodes) {
    const re_dfa_t *const dfa = mctx->dfa;
    reg_errcode_t err;
    Idx i;
    Idx cur_str_idx = re_string_cur_idx(&mctx->input);

    for (i = 0; i < nodes->nelem; ++i) {
        Idx dest_str_idx, prev_nelem, bkc_idx;
        Idx node_idx = nodes->elems[i];
        unsigned int context;
        const re_token_t *node = dfa->nodes + node_idx;
        re_node_set *new_dest_nodes;

        /* Check whether 'node' is a backreference or not.  */
        if (node->type != OP_BACK_REF)
            continue;

        if (node->constraint) {
            context = re_string_context_at(&mctx->input, cur_str_idx,
                                           mctx->eflags);
            if (NOT_SATISFY_NEXT_CONSTRAINT(node->constraint, context))
                continue;
        }

        /* 'node' is a backreference.
       Check the substring which the substring matched.  */
        bkc_idx = mctx->nbkref_ents;
        err = get_subexp(mctx, node_idx, cur_str_idx);
        if ((err != REG_NOERROR))
            goto free_return;

        /* And add the epsilon closures (which is 'new_dest_nodes') of
       the backreference to appropriate state_log.  */

        for (; bkc_idx < mctx->nbkref_ents; ++bkc_idx) {
            Idx subexp_len;
            re_dfastate_t *dest_state;
            struct re_backref_cache_entry *bkref_ent;
            bkref_ent = mctx->bkref_ents + bkc_idx;
            if (bkref_ent->node != node_idx || bkref_ent->str_idx != cur_str_idx)
                continue;
            subexp_len = bkref_ent->subexp_to - bkref_ent->subexp_from;
            new_dest_nodes = (subexp_len == 0
                              ? dfa->eclosures + dfa->edests[node_idx].elems[0]
                              : dfa->eclosures + dfa->nexts[node_idx]);
            dest_str_idx = (cur_str_idx + bkref_ent->subexp_to
                            - bkref_ent->subexp_from);
            context = re_string_context_at(&mctx->input, dest_str_idx - 1,
                                           mctx->eflags);
            dest_state = mctx->state_log[dest_str_idx];
            prev_nelem = ((mctx->state_log[cur_str_idx] == NULL) ? 0
                                                                 : mctx->state_log[cur_str_idx]->nodes.nelem);
            /* Add 'new_dest_node' to state_log.  */
            if (dest_state == NULL) {
                mctx->state_log[dest_str_idx]
                        = re_acquire_state_context(&err, dfa, new_dest_nodes,
                                                   context);
                if ((mctx->state_log[dest_str_idx] == NULL
                     && err != REG_NOERROR))
                    goto free_return;
            } else {
                re_node_set dest_nodes;
                err = re_node_set_init_union(&dest_nodes,
                                             dest_state->entrance_nodes,
                                             new_dest_nodes);
                if ((err != REG_NOERROR)) {
                    re_node_set_free(&dest_nodes);
                    goto free_return;
                }
                mctx->state_log[dest_str_idx]
                        = re_acquire_state_context(&err, dfa, &dest_nodes, context);
                re_node_set_free(&dest_nodes);
                if ((mctx->state_log[dest_str_idx] == NULL
                     && err != REG_NOERROR))
                    goto free_return;
            }
            /* We need to check recursively if the backreference can epsilon
               transit.  */
            if (subexp_len == 0
                && mctx->state_log[cur_str_idx]->nodes.nelem > prev_nelem) {
                err = check_subexp_matching_top(mctx, new_dest_nodes,
                                                cur_str_idx);
                if ((err != REG_NOERROR))
                    goto free_return;
                err = transit_state_bkref(mctx, new_dest_nodes);
                if ((err != REG_NOERROR))
                    goto free_return;
            }
        }
    }
    err = REG_NOERROR;
    free_return:
    return err;
}

/* Return the next state to which the current state STATE will transit by
   accepting the current input byte, and update STATE_LOG if necessary.
   If STATE can accept a multibyte char/collating element/back reference
   update the destination of STATE_LOG.  */

static re_dfastate_t *

transit_state(reg_errcode_t
              *err,
              re_match_context_t *mctx,
              re_dfastate_t
              *state) {
    re_dfastate_t **trtable;
    unsigned char ch;

/* Use transition table  */
    ch = re_string_fetch_byte(&mctx->input);
    for (;;) {
        trtable = state->trtable;
        if ((trtable != NULL))
            return trtable[ch];

        trtable = state->word_trtable;
        if ((trtable != NULL)) {
            unsigned int context;
            context
                    = re_string_context_at(&mctx->input,
                                           re_string_cur_idx(&mctx->input) - 1,
                                           mctx->eflags);
            if (
                    IS_WORD_CONTEXT(context)
                    )
                return trtable[ch + SBC_MAX];
            else
                return trtable[ch];
        }

        if (!
                build_trtable(mctx
                                      ->dfa, state)) {
            *
                    err = REG_ESPACE;
            return
                    NULL;
        }

/* Retry, we now have a transition table.  */
    }
}

/* Update the state_log if we need */
static re_dfastate_t *
merge_state_with_log(reg_errcode_t *err, re_match_context_t *mctx,
                     re_dfastate_t *next_state) {
    const re_dfa_t *const dfa = mctx->dfa;
    Idx cur_idx = re_string_cur_idx(&mctx->input);

    if (cur_idx > mctx->state_log_top) {
        mctx->state_log[cur_idx] = next_state;
        mctx->state_log_top = cur_idx;
    } else if (mctx->state_log[cur_idx] == 0) {
        mctx->state_log[cur_idx] = next_state;
    } else {
        re_dfastate_t *pstate;
        unsigned int context;
        re_node_set next_nodes, *log_nodes, *table_nodes = NULL;
        /* If (state_log[cur_idx] != 0), it implies that cur_idx is
       the destination of a multibyte char/collating element/
       back reference.  Then the next state is the union set of
       these destinations and the results of the transition table.  */
        pstate = mctx->state_log[cur_idx];
        log_nodes = pstate->entrance_nodes;
        if (next_state != NULL) {
            table_nodes = next_state->entrance_nodes;
            *err = re_node_set_init_union(&next_nodes, table_nodes,
                                          log_nodes);
            if ((*err != REG_NOERROR))
                return NULL;
        } else
            next_nodes = *log_nodes;
        /* Note: We already add the nodes of the initial state,
       then we don't need to add them here.  */

        context = re_string_context_at(&mctx->input,
                                       re_string_cur_idx(&mctx->input) - 1,
                                       mctx->eflags);
        next_state = mctx->state_log[cur_idx]
                = re_acquire_state_context(err, dfa, &next_nodes, context);
        /* We don't need to check errors here, since the return value of
       this function is next_state and ERR is already set.  */

        if (table_nodes != NULL)
            re_node_set_free(&next_nodes);
    }

    if ((dfa->nbackref) && next_state != NULL) {
        /* Check OP_OPEN_SUBEXP in the current state in case that we use them
       later.  We must check them here, since the back references in the
       next state might use them.  */
        *err = check_subexp_matching_top(mctx, &next_state->nodes,
                                         cur_idx);
        if ((*err != REG_NOERROR))
            return NULL;

        /* If the next state has back references.  */
        if (next_state->has_backref) {
            *err = transit_state_bkref(mctx, &next_state->nodes);
            if ((*err != REG_NOERROR))
                return NULL;
            next_state = mctx->state_log[cur_idx];
        }
    }

    return next_state;
}

/* Skip bytes in the input that correspond to part of a
   multi-byte match, then look in the log for a state
   from which to restart matching.  */
static re_dfastate_t *
find_recover_state(reg_errcode_t *err, re_match_context_t *mctx) {
    re_dfastate_t *cur_state;
    do {
        Idx max = mctx->state_log_top;
        Idx cur_str_idx = re_string_cur_idx(&mctx->input);

        do {
            if (++cur_str_idx > max)
                return NULL;
            re_string_skip_bytes(&mctx->input, 1);
        } while (mctx->state_log[cur_str_idx] == NULL);

        cur_state = merge_state_with_log(err, mctx, NULL);
    } while (*err == REG_NOERROR && cur_state == NULL);
    return cur_state;
}

/* Functions for matching context.  */

/* Initialize MCTX.  */

static reg_errcode_t
match_ctx_init(re_match_context_t *mctx, int eflags, Idx n) {
    mctx->eflags = eflags;
    mctx->match_last = -1;
    if (n > 0) {
/* Avoid overflow.  */
        size_t max_object_size =
                MAX (sizeof(struct re_backref_cache_entry),
                     sizeof(re_sub_match_top_t * ));
        if ((MIN (IDX_MAX, SIZE_MAX / max_object_size) < n))
            return REG_ESPACE;

        mctx->bkref_ents = re_malloc(
        struct re_backref_cache_entry, n);
        mctx->sub_tops = re_malloc(re_sub_match_top_t * , n);
        if ((mctx->bkref_ents == NULL || mctx->sub_tops == NULL))
            return REG_ESPACE;
    }
/* Already zero-ed by the caller.
   else
     mctx->bkref_ents = NULL;
   mctx->nbkref_ents = 0;
   mctx->nsub_tops = 0;  */
    mctx->abkref_ents = n;
    mctx->max_mb_elem_len = 1;
    mctx->asub_tops = n;
    return REG_NOERROR;
}

/* Clean the entries which depend on the current input in MCTX.
   This function must be invoked when the matcher changes the start index
   of the input, or changes the input string.  */

static void
match_ctx_clean(re_match_context_t *mctx) {
    Idx st_idx;
    for (st_idx = 0; st_idx < mctx->nsub_tops; ++st_idx) {
        Idx sl_idx;
        re_sub_match_top_t *top = mctx->sub_tops[st_idx];
        for (sl_idx = 0; sl_idx < top->nlasts; ++sl_idx) {
            re_sub_match_last_t *last = top->lasts[sl_idx];
            re_free(last->path.array);
            re_free(last);
        }
        re_free(top->lasts);
        if (top->path) {
            re_free(top->path->array);
            re_free(top->path);
        }
        re_free(top);
    }

    mctx->nsub_tops = 0;
    mctx->nbkref_ents = 0;
}

/* Free all the memory associated with MCTX.  */

static void
match_ctx_free(re_match_context_t *mctx) {
    /* First, free all the memory associated with MCTX->SUB_TOPS.  */
    match_ctx_clean(mctx);
    re_free(mctx->sub_tops);
    re_free(mctx->bkref_ents);
}

/* Add a new backreference entry to MCTX.
   Note that we assume that caller never call this function with duplicate
   entry, and call with STR_IDX which isn't smaller than any existing entry.
*/

static reg_errcode_t

match_ctx_add_entry(re_match_context_t *mctx, Idx node, Idx str_idx, Idx from,
                    Idx to) {
    if (mctx->nbkref_ents >= mctx->abkref_ents) {
        struct re_backref_cache_entry *new_entry;
        new_entry = re_realloc(mctx->bkref_ents,
        struct re_backref_cache_entry,
        mctx->abkref_ents * 2);
        if ((new_entry == NULL)) {
            re_free(mctx->bkref_ents);
            return REG_ESPACE;
        }
        mctx->bkref_ents = new_entry;
        memset(mctx->bkref_ents + mctx->nbkref_ents, '\0',
               sizeof(struct re_backref_cache_entry) * mctx->abkref_ents);
        mctx->abkref_ents *= 2;
    }
    if (mctx->nbkref_ents > 0
        && mctx->bkref_ents[mctx->nbkref_ents - 1].str_idx == str_idx)
        mctx->bkref_ents[mctx->nbkref_ents - 1].more = 1;

    mctx->bkref_ents[mctx->nbkref_ents].node = node;
    mctx->bkref_ents[mctx->nbkref_ents].str_idx = str_idx;
    mctx->bkref_ents[mctx->nbkref_ents].subexp_from = from;
    mctx->bkref_ents[mctx->nbkref_ents].subexp_to = to;

/* This is a cache that saves negative results of check_dst_limits_calc_pos.
   If bit N is clear, means that this entry won't epsilon-transition to
   an OP_OPEN_SUBEXP or OP_CLOSE_SUBEXP for the N+1-th subexpression.  If
   it is set, check_dst_limits_calc_pos_1 will recurse and try to find one
   such node.

   A backreference does not epsilon-transition unless it is empty, so set
   to all zeros if FROM != TO.  */
    mctx->bkref_ents[mctx->nbkref_ents].eps_reachable_subexps_map
            = (from == to ? -1 : 0);

    mctx->bkref_ents[mctx->nbkref_ents++].more = 0;
    if (mctx->max_mb_elem_len < to - from)
        mctx->max_mb_elem_len = to - from;
    return REG_NOERROR;
}

/* Check whether the regular expression match input string INPUT or not,
   and return the index where the matching end.  Return -1 if
   there is no match, and return -2 in case of an error.
   FL_LONGEST_MATCH means we want the POSIX longest matching.
   If P_MATCH_FIRST is not NULL, and the match fails, it is set to the
   next place where we may want to try matching.
   Note that the matcher assumes that the matching starts from the current
   index of the buffer.  */

static Idx
check_matching(re_match_context_t *mctx, bool fl_longest_match,
               Idx *p_match_first) {
    const re_dfa_t *const dfa = mctx->dfa;
    reg_errcode_t err;
    Idx match = 0;
    Idx match_last = -1;
    Idx cur_str_idx = re_string_cur_idx(&mctx->input);
    re_dfastate_t *cur_state;
    bool at_init_state = p_match_first != NULL;
    Idx next_start_idx = cur_str_idx;

    err = REG_NOERROR;
    cur_state = acquire_init_state_context(&err, mctx, cur_str_idx);
/* An initial state must not be NULL (invalid).  */
    if ((cur_state == NULL)) {
        assert(err == REG_ESPACE);
        return -2;
    }

    if (mctx->state_log != NULL) {
        mctx->state_log[cur_str_idx] = cur_state;

/* Check OP_OPEN_SUBEXP in the initial state in case that we use them
later.  E.g. Processing back references.  */
        if ((dfa->nbackref)) {
            at_init_state = false;
            err = check_subexp_matching_top(mctx, &cur_state->nodes, 0);
            if ((err != REG_NOERROR))
                return err;

            if (cur_state->has_backref) {
                err = transit_state_bkref(mctx, &cur_state->nodes);
                if ((err != REG_NOERROR))
                    return err;
            }
        }
    }

/* If the RE accepts NULL string.  */
    if ((cur_state->halt)) {
        if (!cur_state->has_constraint
            || check_halt_state_context(mctx, cur_state, cur_str_idx)) {
            if (!fl_longest_match)
                return cur_str_idx;
            else {
                match_last = cur_str_idx;
                match = 1;
            }
        }
    }

    while (!re_string_eoi(&mctx->input)) {
        re_dfastate_t *old_state = cur_state;
        Idx next_char_idx = re_string_cur_idx(&mctx->input) + 1;

        if (((next_char_idx >= mctx->input.bufs_len)
             && mctx->input.bufs_len < mctx->input.len)
            || ((next_char_idx >= mctx->input.valid_len)
                && mctx->input.valid_len < mctx->input.len)) {
            err = extend_buffers(mctx, next_char_idx + 1);
            if ((err != REG_NOERROR)) {
                assert(err == REG_ESPACE);
                return -2;
            }
        }

        cur_state = transit_state(&err, mctx, cur_state);
        if (mctx->state_log != NULL)
            cur_state = merge_state_with_log(&err, mctx, cur_state);

        if (cur_state == NULL) {
/* Reached the invalid state or an error.  Try to recover a valid
   state using the state log, if available and if we have not
   already found a valid (even if not the longest) match.  */
            if ((err != REG_NOERROR))
                return -2;

            if (mctx->state_log == NULL
                || (match && !fl_longest_match)
                || (cur_state = find_recover_state(&err, mctx)) == NULL)
                break;
        }

        if ((at_init_state)) {
            if (old_state == cur_state)
                next_start_idx = next_char_idx;
            else
                at_init_state = false;
        }

        if (cur_state->halt) {
/* Reached a halt state.
   Check the halt state can satisfy the current context.  */
            if (!cur_state->has_constraint
                || check_halt_state_context(mctx, cur_state,
                                            re_string_cur_idx(&mctx->input))) {
/* We found an appropriate halt state.  */
                match_last = re_string_cur_idx(&mctx->input);
                match = 1;

/* We found a match, do not modify match_first below.  */
                p_match_first = NULL;
                if (!fl_longest_match)
                    break;
            }
        }
    }

    if (p_match_first)
        *p_match_first += next_start_idx;

    return match_last;
}

/* Searches for a compiled pattern PREG in the string STRING, whose
   length is LENGTH.  NMATCH, PMATCH, and EFLAGS have the same
   meaning as with regexec.  LAST_START is START + RANGE, where
   START and RANGE have the same meaning as with re_search.
   Return REG_NOERROR if we find a match, and REG_NOMATCH if not,
   otherwise return the error code.
   Note: We assume front end functions already check ranges.
   (0 <= LAST_START && LAST_START <= LENGTH)  */

static reg_errcode_t
re_search_internal(const regex_t *preg, const char *string, Idx length,
                   Idx start, Idx last_start, Idx stop, size_t nmatch,
                   regmatch_t pmatch[], int eflags) {
    reg_errcode_t err;
    const re_dfa_t *dfa = preg->buffer;
    Idx left_lim, right_lim;
    int incr;
    bool fl_longest_match;
    int match_kind;
    Idx match_first;
    Idx match_last = -1;
    Idx extra_nmatch;
    bool sb;
    int ch;
    re_match_context_t mctx;
    char *fastmap = ((preg->fastmap != NULL && preg->fastmap_accurate
                      && start != last_start && !preg->can_be_null)
                     ? preg->fastmap : NULL);
    RE_TRANSLATE_TYPE t = preg->translate;

    memset(&mctx, '\0', sizeof(re_match_context_t));
    mctx.dfa = dfa;


    extra_nmatch = (nmatch > preg->re_nsub) ? nmatch - (preg->re_nsub + 1) : 0;
    nmatch -= extra_nmatch;

    /* Check if the DFA haven't been compiled.  */
    if ((preg->used == 0 || dfa->init_state == NULL
         || dfa->init_state_word == NULL
         || dfa->init_state_nl == NULL
         || dfa->init_state_begbuf == NULL))
        return REG_NOMATCH;

    /* If initial states with non-begbuf contexts have no elements,
       the regex must be anchored.  If preg->newline_anchor is set,
       we'll never use init_state_nl, so do not check it.  */
    if (dfa->init_state->nodes.nelem == 0
        && dfa->init_state_word->nodes.nelem == 0
        && (dfa->init_state_nl->nodes.nelem == 0
            || !preg->newline_anchor)) {
        if (start != 0 && last_start != 0)
            return REG_NOMATCH;
        start = last_start = 0;
    }

    /* We must check the longest matching, if nmatch > 0.  */
    fl_longest_match = (nmatch != 0 || dfa->nbackref);

    err = re_string_allocate(&mctx.input, string, length, dfa->nodes_len + 1,
                             preg->translate, (preg->syntax & RE_ICASE) != 0,
                             dfa);
    if ((err != REG_NOERROR))
        goto free_return;
    mctx.input.stop = stop;
    mctx.input.raw_stop = stop;
    mctx.input.newline_anchor = preg->newline_anchor;

    err = match_ctx_init(&mctx, eflags, dfa->nbackref * 2);
    if ((err != REG_NOERROR))
        goto free_return;

    /* We will log all the DFA states through which the dfa pass,
       if nmatch > 1, or this dfa has "multibyte node", which is a
       back-reference or a node which can accept multibyte character or
       multi character collating element.  */
    if (nmatch > 1 || dfa->has_mb_node) {
        /* Avoid overflow.  */
        if (((MIN (IDX_MAX, SIZE_MAX / sizeof(re_dfastate_t * ))
              <= mctx.input.bufs_len))) {
            err = REG_ESPACE;
            goto free_return;
        }

        mctx.state_log = re_malloc(re_dfastate_t * , mctx.input.bufs_len + 1);
        if ((mctx.state_log == NULL)) {
            err = REG_ESPACE;
            goto free_return;
        }
    } else
        mctx.state_log = NULL;

    match_first = start;
    mctx.input.tip_context = (eflags & REG_NOTBOL) ? CONTEXT_BEGBUF
                                                   : CONTEXT_NEWLINE | CONTEXT_BEGBUF;

    /* Check incrementally whether the input string matches.  */
    incr = (last_start < start) ? -1 : 1;
    left_lim = (last_start < start) ? last_start : start;
    right_lim = (last_start < start) ? start : last_start;
    sb = dfa->mb_cur_max == 1;
    match_kind =
            (fastmap
             ? ((sb || !(preg->syntax & RE_ICASE || t) ? 4 : 0)
                | (start <= last_start ? 2 : 0)
                | (t != NULL ? 1 : 0))
             : 8);

    for (;; match_first += incr) {
        err = REG_NOMATCH;
        if (match_first < left_lim || right_lim < match_first)
            goto free_return;

        /* Advance as rapidly as possible through the string, until we
       find a plausible place to start matching.  This may be done
       with varying efficiency, so there are various possibilities:
       only the most common of them are specialized, in order to
       save on code size.  We use a switch statement for speed.  */
        switch (match_kind) {
            case 8:
                /* No fastmap.  */
                break;

            case 7:
                /* Fastmap with single-byte translation, match forward.  */
                while ((match_first < right_lim)
                       && !fastmap[t[(unsigned char) string[match_first]]])
                    ++match_first;
                goto forward_match_found_start_or_reached_end;

            case 6:
                /* Fastmap without translation, match forward.  */
                while ((match_first < right_lim)
                       && !fastmap[(unsigned char) string[match_first]])
                    ++match_first;

            forward_match_found_start_or_reached_end:
                if ((match_first == right_lim)) {
                    ch = match_first >= length
                         ? 0 : (unsigned char) string[match_first];
                    if (!fastmap[t ? t[ch] : ch])
                        goto free_return;
                }
                break;

            case 4:
            case 5:
                /* Fastmap without multi-byte translation, match backwards.  */
                while (match_first >= left_lim) {
                    ch = match_first >= length
                         ? 0 : (unsigned char) string[match_first];
                    if (fastmap[t ? t[ch] : ch])
                        break;
                    --match_first;
                }
                if (match_first < left_lim)
                    goto free_return;
                break;

            default:
                /* In this case, we can't determine easily the current byte,
                   since it might be a component byte of a multibyte
                   character.  Then we use the constructed buffer instead.  */
                for (;;) {
                    /* If MATCH_FIRST is out of the valid range, reconstruct the
                   buffers.  */
                    __re_size_t offset = match_first - mctx.input.raw_mbs_idx;
                    if ((offset
                         >= (__re_size_t) mctx.input.valid_raw_len)) {
                        err = re_string_reconstruct(&mctx.input, match_first,
                                                    eflags);
                        if ((err != REG_NOERROR))
                            goto free_return;

                        offset = match_first - mctx.input.raw_mbs_idx;
                    }
                    /* If MATCH_FIRST is out of the buffer, leave it as '\0'.
                   Note that MATCH_FIRST must not be smaller than 0.  */
                    ch = (match_first >= length
                          ? 0 : re_string_byte_at(&mctx.input, offset));
                    if (fastmap[ch])
                        break;
                    match_first += incr;
                    if (match_first < left_lim || match_first > right_lim) {
                        err = REG_NOMATCH;
                        goto free_return;
                    }
                }
                break;
        }

        /* Reconstruct the buffers so that the matcher can assume that
       the matching starts from the beginning of the buffer.  */
        err = re_string_reconstruct(&mctx.input, match_first, eflags);
        if ((err != REG_NOERROR))
            goto free_return;

        /* It seems to be appropriate one, then use the matcher.  */
        /* We assume that the matching starts from 0.  */
        mctx.state_log_top = mctx.nbkref_ents = mctx.max_mb_elem_len = 0;
        match_last = check_matching(&mctx, fl_longest_match,
                                    start <= last_start ? &match_first : NULL);
        if (match_last != -1) {
            if ((match_last == -2)) {
                err = REG_ESPACE;
                goto free_return;
            } else {
                mctx.match_last = match_last;
                if ((!preg->no_sub && nmatch > 1) || dfa->nbackref) {
                    re_dfastate_t *pstate = mctx.state_log[match_last];
                    mctx.last_node = check_halt_state_context(&mctx, pstate,
                                                              match_last);
                }
                if ((!preg->no_sub && nmatch > 1 && dfa->has_plural_match)
                    || dfa->nbackref) {
                    err = prune_impossible_nodes(&mctx);
                    if (err == REG_NOERROR)
                        break;
                    if ((err != REG_NOMATCH))
                        goto free_return;
                    match_last = -1;
                } else
                    break; /* We found a match.  */
            }
        }

        match_ctx_clean(&mctx);
    }

    /* Set pmatch[] if we need.  */
    if (nmatch > 0) {
        Idx reg_idx;

        /* Initialize registers.  */
        for (reg_idx = 1; reg_idx < nmatch; ++reg_idx)
            pmatch[reg_idx].rm_so = pmatch[reg_idx].rm_eo = -1;

        /* Set the points where matching start/end.  */
        pmatch[0].rm_so = 0;
        pmatch[0].rm_eo = mctx.match_last;
        /* FIXME: This function should fail if mctx.match_last exceeds
       the maximum possible regoff_t value.  We need a new error
       code REG_OVERFLOW.  */

        if (!preg->no_sub && nmatch > 1) {
            err = set_regs(preg, &mctx, nmatch, pmatch,
                           dfa->has_plural_match && dfa->nbackref > 0);
            if ((err != REG_NOERROR))
                goto free_return;
        }

        /* At last, add the offset to each register, since we slid
       the buffers so that we could assume that the matching starts
       from 0.  */
        for (reg_idx = 0; reg_idx < nmatch; ++reg_idx)
            if (pmatch[reg_idx].rm_so != -1) {
                pmatch[reg_idx].rm_so += match_first;
                pmatch[reg_idx].rm_eo += match_first;
            }
        for (reg_idx = 0; reg_idx < extra_nmatch; ++reg_idx) {
            pmatch[nmatch + reg_idx].rm_so = -1;
            pmatch[nmatch + reg_idx].rm_eo = -1;
        }

        if (dfa->subexp_map)
            for (reg_idx = 0; reg_idx + 1 < nmatch; reg_idx++)
                if (dfa->subexp_map[reg_idx] != reg_idx) {
                    pmatch[reg_idx + 1].rm_so
                            = pmatch[dfa->subexp_map[reg_idx] + 1].rm_so;
                    pmatch[reg_idx + 1].rm_eo
                            = pmatch[dfa->subexp_map[reg_idx] + 1].rm_eo;
                }
    }

    free_return:
    re_free(mctx.state_log);
    if (dfa->nbackref)
        match_ctx_free(&mctx);
    re_string_destruct(&mctx.input);
    return err;
}

/* Helper functions.  */

static reg_errcode_t
merge_state_array (const re_dfa_t *dfa, re_dfastate_t **dst,
                   re_dfastate_t **src, Idx num)
{
    Idx st_idx;
    reg_errcode_t err;
    for (st_idx = 0; st_idx < num; ++st_idx)
    {
        if (dst[st_idx] == NULL)
            dst[st_idx] = src[st_idx];
        else if (src[st_idx] != NULL)
        {
            re_node_set merged_set;
            err = re_node_set_init_union (&merged_set, &dst[st_idx]->nodes,
                                          &src[st_idx]->nodes);
            if ( (err != REG_NOERROR))
                return err;
            dst[st_idx] = re_acquire_state (&err, dfa, &merged_set);
            re_node_set_free (&merged_set);
            if ( (err != REG_NOERROR))
                return err;
        }
    }
    return REG_NOERROR;
}

static void
sift_ctx_init (re_sift_context_t *sctx, re_dfastate_t **sifted_sts,
               re_dfastate_t **limited_sts, Idx last_node, Idx last_str_idx)
{
    sctx->sifted_states = sifted_sts;
    sctx->limited_states = limited_sts;
    sctx->last_node = last_node;
    sctx->last_str_idx = last_str_idx;
    re_node_set_init_empty (&sctx->limits);
}


/* regexec searches for a given pattern, specified by PREG, in the
   string STRING.

   If NMATCH is zero or REG_NOSUB was set in the cflags argument to
   'regcomp', we ignore PMATCH.  Otherwise, we assume PMATCH has at
   least NMATCH elements, and we set them to the offsets of the
   corresponding matched substrings.

   EFLAGS specifies "execution flags" which affect matching: if
   REG_NOTBOL is set, then ^ does not match at the beginning of the
   string; if REG_NOTEOL is set, then $ does not match at the end.

   We return 0 if we find a match and REG_NOMATCH if not.  */

int
regexec(const regex_t *_Restrict_ preg, const char *_Restrict_ string,
        size_t nmatch, regmatch_t pmatch[], int eflags) {
    reg_errcode_t err;
    Idx start, length;
    re_dfa_t *dfa = preg->buffer;

    if (eflags & ~(REG_NOTBOL | REG_NOTEOL | REG_STARTEND))
        return REG_BADPAT;

    if (eflags & REG_STARTEND) {
        start = pmatch[0].rm_so;
        length = pmatch[0].rm_eo;
    } else {
        start = 0;
        length = strlen(string);
    }

    lock_lock(dfa->lock);
    if (preg->no_sub)
        err = re_search_internal(preg, string, length, start, length,
                                 length, 0, NULL, eflags);
    else
        err = re_search_internal(preg, string, length, start, length,
                                 length, nmatch, pmatch, eflags);
    lock_unlock(dfa->lock);
    return err != REG_NOERROR;
}

#endif
