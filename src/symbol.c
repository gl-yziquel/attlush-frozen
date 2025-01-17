/*   Lush Lisp interpreter and development tools
 *   Copyright (C) 1991-1999 Leon Bottou and Neuristique.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
/***********************************************************************
  T-Lisp3: (C) LYB YLC 1988
  symbol.c
  $Id: symbol.c,v 0.1 2001/10/31 17:31:04 profshadoko Exp $

********************************************************************** */

#include "header.h"

/* 
 * predefined symbols 
 */

at *at_true;
static at *at_scope;


/*
 * hash table for
 * the symbols' names
 */

struct hash_name *names[HASHTABLESIZE];
struct alloc_root hash_name_alloc =
{
  NIL,
  NIL,
  sizeof(struct hash_name),
  100
};

/*
 * search_name(s,mode) Returns a ptr to the HASH_NAME node for the name S.
 * Returns NIL if an error occurs. if MODE is 0 found it. if MODE is +1
 * creates it, pointing to NIL. if MODE is -1 deletes it, and unlocks
 * target
 */


#define LASTWORD
#ifdef LASTWORD

/*
 * With LASTWORD option, Lush confuses '-' and '_'. So 'nlf-tanh' and
 * 'nlf_tanh' refer to the same symbol !
 */

static int
mystrncmp(char *s, char *d, int n)
{
  unsigned char b, c;

  while ((*s || *d) && (n-- > 0)) {
    if ((b = *s++) == '-')
      b = '_';
    if ((c = *d++) == '-')
      c = '_';
    if (c != b)
      return b > c ? 1 : -1;
  }
  return 0;
}

#else

#define mystrncmp(s,d,n)   (int)strncmp(s,d,(int) m)

#endif


static struct hash_name *
search_by_name(unsigned char *s, int mode)
{
  struct hash_name **lasthn;

  /* Calculate hash value */
  {
    unsigned int hash;
    int i;

    hash = 0;
    for (i = 0; i < 8; i++)
#ifdef LASTWORD
      if (s[i] == '-')
	hash += '_';
      else
#endif
      if (s[i])
	hash += s[i];
      else
	break;
    hash %= HASHTABLESIZE;
    lasthn = names + hash;
  }

  /* Search in list and operate */
  {
    struct hash_name *hn;

    hn = *lasthn;
    while (hn && mystrncmp((char *) s, (char *) hn->name, (int) NAMELENGTH)) {
      lasthn = &(hn->next);
      hn = *lasthn;
    }

    if (!hn && mode == 1) {
      hn = allocate(&hash_name_alloc);
      hn->next = NIL;
      hn->named = NIL;
      strncpy((char *) hn->name, (char *) s, NAMELENGTH);
      *lasthn = hn;
    } else if (hn && mode == -1) {
      UNLOCK(hn->named);
      *lasthn = hn->next;
      deallocate(&hash_name_alloc, (struct empty_alloc *) hn);
      hn = NIL;
    }
    return hn;
  }
}


/*
 * kill_name: Destroys the named symbol whose name structure is passed as
 * argument. Tests before that the count is 1 and the value is (). Because it
 * tests the count field, this function can't be called from LISP.
 */
void 
kill_name(struct hash_name *hn)
{
  /* also called by allocate.c */
  if (hn->named->count == 1)
    search_by_name((unsigned char *) hn->name, (int) -1);
}

/*
 * named(s) finds the SYMBOL named S. Actually does the same job as
 * NEW_SYMBOL. doesn't return NIL if the symbol doesn't exists, but creates
 * it.
 */

at *
named(char *s)
{
  return new_symbol(s);
}

DX(xnamed)
{
  ARG_NUMBER(1);
  ALL_ARGS_EVAL;
  return new_symbol(ASTRING(1));
}

/*
 * nameof(p) returns the name of the SYMBOL p
 */

char *
nameof(at *p)
{
  struct symbol *symb;
  static char answer[NAMELENGTH + 1];

  answer[0] = answer[NAMELENGTH] = 0;
  if (p && (p->flags & X_SYMBOL)) {
    symb = (struct symbol *) (p->Object);
    if (symb->name) {
      strncpy(answer, symb->name->name, NAMELENGTH);
      return answer;
    }
  }
  return NIL;
}


/*
 * (symblist) (oblist) Returns the sorted list of all the currently defined
 * symbols
 */
at *
symblist(void)
{
  at *answer;
  at **where;

  struct hash_name **j, *hn;
  char name[NAMELENGTH + 1];

  answer = NIL;
  name[NAMELENGTH] = 0;
  iter_hash_name(j, hn) {
    strncpy(name, hn->name, NAMELENGTH);
    where = &answer;
    while (*where && strcmp(name, SADD((*where)->Car->Object)) > 0)
      where = &((*where)->Cdr);
    *where = cons(new_string(name), *where);
  }
  return answer;
}

DX(xsymblist)
{
  ARG_NUMBER(0);
  return symblist();
}

at *
oblist(void)
{
  at *answer;
  struct hash_name **j, *hn;

  answer = NIL;
  iter_hash_name(j, hn) {
    LOCK(hn->named);
    answer = cons(hn->named, answer);
  }
  return answer;
}

DX(xoblist)
{
  ARG_NUMBER(0);
  return oblist();
}






struct alloc_root symbol_alloc =
{
  NIL,
  NIL,
  sizeof(struct symbol),
  100
};


/*
 * Class functions
 * 
 * symbol_dispose: expunge a symbol out of the memory
 * symbol_action: does an action on the symbol reference
 * symbol_eval:	get the value of a symbol
 */

static void
symbol_dispose(at *p)
{
  struct symbol *s1, *s2;

  s1 = p->Object;
  while (s1) {
    s2 = s1->next;
    UNLOCK(s1->value);
    deallocate(&symbol_alloc, (struct empty_alloc *) s1);
    s1 = s2;
  }
  p->Object = NIL;
  p->flags &= ~X_SYMBOL;
}

static void
symbol_action(at *p, void (*action) (at *))
{
  struct symbol *s1, *s2;

  s1 = p->Object;
  while (s1) {
    s2 = s1->next;
    (*action) (s1->value);
    s1 = s2;
  }
}

static char *
symbol_name(at *p)
{
  char *s;

  s = nameof(p);
  if (s)
    return s;
  else
    return generic_name(p);
}


static at *
symbol_eval(at *p)
{
  at *q;
  struct symbol *symb;
  symb = p->Object;
  
  if (symb->valueptr)
    q = *(symb->valueptr);
  else
    q = NIL;
  
  if (q && q->flags & X_ZOMBIE) {
    UNLOCK(q);
    *(symb->valueptr) = NIL;
    return NIL;
  } else {
    LOCK(q);
    return q;
  }
}


/* SYMBOL_CLASS DEFINITION */

class symbol_class =
{
  symbol_dispose,
  symbol_action,
  symbol_name,
  symbol_eval,
  generic_listeval,		/* no function abilities		 */
};






/* Others functions on symbols	 */






at *
setq(at *p, at *q)		/* WARNING: returns an UNLOCKED AT	 */
          			/* never use this AT in a C program	 */
{
  extern int in_object_scope;	/* From OOSTRUCT.C */

  if (p && (p->flags&X_SYMBOL)) 
  {
    /* 
     * (setq symbol value) 
     */
    struct symbol *symb;
    symb = (struct symbol *) (p->Object);
    if (symb->mode == SYMBOL_LOCKED)
      error(NIL, "locked symbol", p);
    if (!symb->valueptr) {
      if (in_object_scope)
	error(NIL, "cannot create a new variable from object scope",p);
      else {
	symb->valueptr = &(symb->value);
	symb->value = NIL;
      }
    }
    p = *(symb->valueptr);
    LOCK(q);
    *(symb->valueptr) = q;
    UNLOCK(p);
    return q;

  } 
  else if (CONSP(p)) 
  {
    /*
     * (setq :symbol value)
     * (setq :object:slot value)
     */
    if (p->Car!=at_scope || !CONSP(p->Cdr))
      error(NIL,"illegal scope syntax",p);
    p = p->Cdr;

    if (! p->Cdr) 
    {
      /*
       * :globalvar
       */
      struct symbol *symb;
      p = p->Car;
      ifn (p && (p->flags&X_SYMBOL))
	error(NIL,"illegal global scope specification",p);
      symb = p->Object;
      while(symb->next)
	symb = symb->next;
      if (symb->mode == SYMBOL_LOCKED)
	error(NIL,"locked global variable",p);
      if (!symb->valueptr)
      {
	symb->value = NIL;
	symb->valueptr = &(symb->value);
      }
      LOCK(q);
      p = symb->value;
      symb->value = q;
      UNLOCK(p);
      return q;
    }
    else
    {
      /*
       * :object:slot:slot:slot
       */

      at *obj;
      obj = eval(p->Car);
      setslot(&obj, p->Cdr, q);
      UNLOCK(obj);
      return q;
    }
  } 
  else
    error(NIL,"neither a symbol nor an explicit scope specification",p);
}

DX(xsetq)
{
  int i;
  at *q;

  if ((arg_number & 1) || (arg_number < 2))
    error(NIL, "two arguments were expected", NIL);
  q = NIL;
  for (i = 2; i <= arg_number; i += 2)
    ARG_EVAL(i);
  for (i = 1; i <= arg_number; i += 2) {
    q = setq(APOINTER(i), APOINTER(i + 1));
  }
  LOCK(q);
  return q;
}


/*
 * symbol_push(p,q) pushes the value q on the symbol's stack
 */
void
symbol_push(at *p, at *q)
{
  struct symbol *symb;

  symb = allocate(&symbol_alloc);
  symb->mode = SYMBOL_UNLOCKED;
  symb->nopurge = 0;
  symb->next = (struct symbol *) (p->Object);
  symb->name = symb->next->name;
  symb->value = q;
  symb->valueptr = &(symb->value);
  LOCK(q);
  p->Object = symb;
}

/*
 * symbol_pop(p) pops a value on the symbol's stack
 */
void
symbol_pop(at *p)
{
  struct symbol *symb;

  symb = (struct symbol *) (p->Object);
  if (symb->next) {
    p->Object = symb->next;
    UNLOCK(symb->value);
    deallocate(&symbol_alloc, (struct empty_alloc *) symb);
  }
}


/*
 * new_symbol(s) Return a symbol whose name is s. If such a symbol exists,
 * don't create it. Just return it.
 */

at *
new_symbol(char *s)
{
  at *p;
  struct symbol *symb;
  struct hash_name *hn;

  if (s[0] == ':' && s[1] == ':')
    error(s, "belongs to a reserved package... ", NIL);

  hn = search_by_name((unsigned char *) s, (int) 1);
  if (hn->named) {		/* symbol exists	 */
    LOCK(hn->named);		/* don't create it !	 */
    return hn->named;
  }
  symb = allocate(&symbol_alloc);
  symb->mode = SYMBOL_UNLOCKED;
  symb->nopurge = FALSE;
  symb->next = NIL;
  symb->value = NIL;
  symb->valueptr = NIL;
  symb->name = hn;

  hn->named = p = new_extern(&symbol_class, symb);
  p->flags |= X_SYMBOL;

  LOCK(p);
  return p;
}



DX(xlock_symbol)
{
  int i;

  for (i = 1; i <= arg_number; i++)
    ((struct symbol *) (ASYMBOL(i)))->mode = SYMBOL_LOCKED;

  return NIL;
}

DX(xunlock_symbol)
{
  int i;

  for (i = 1; i <= arg_number; i++)
    ((struct symbol *) (ASYMBOL(i)))->mode = SYMBOL_UNLOCKED;
  return NIL;
}


DX(xsymbolp)
{

  at *p;

  ARG_NUMBER(1);
  ARG_EVAL(1);
  p = APOINTER(1);
  if (p && (p->flags & X_SYMBOL)) {
    LOCK(p);
    return p;
  } else
    return NIL;
}

DX(xincr)
{
  real incr;
  at *q;
  
  if (arg_number == 1)
    incr = 1;
  else {
    ARG_NUMBER(2);
    ARG_EVAL(2);
    incr = AREAL(2);
  }
  ASYMBOL(1);
  q = var_get(APOINTER(1));
  ifn (q && q->flags&C_NUMBER)
    error(NIL, "not a number",q);
  incr += q->Number;
  UNLOCK(q);
  q = NEW_NUMBER(incr);
  var_set(APOINTER(1),q);
  return q;
}


/* --------- STRUCTURED LISTES  --------- */


static at **
search_label(struct symbol *symb, at *prop)
{
  extern int in_object_scope;	/* From OOSTRUCT.C */
  at **last, *list;
  if (!symb->valueptr) {
    if (in_object_scope)
      error(NIL, "cannot create a new variable from object scope",NIL);
    else {
      symb->valueptr = &(symb->value);
      symb->value = NIL;
    }
  }
  last = symb->valueptr;
  list = *last;
  ifn (LISTP(list))
    error(NIL, "symbol value isn't a list", list);
  
  while (CONSP(list) && CONSP(list->Cdr)) {
    if (list->Car == prop)
      return last;
    last = &(list->Cdr->Cdr);
    list = *last;
  }
  UNLOCK(*last);
  LOCK(prop);
  list = *last = cons(prop, cons(NIL, NIL));
  return last;
}

void
putprop(struct symbol *symb, at *value, at *prop)
{
  at **loc, *list;

  ifn (symb->valueptr) {
    symb->valueptr = &(symb->value);
    symb->value = NIL;
  }
  list = *(symb->valueptr);
  ifn (list) {
    LOCK(prop);
    LOCK(value);
    *(symb->valueptr) = cons(prop, cons(value, NIL));
  }
  loc = search_label(symb, prop);
  if (value) {
    loc = &((*loc)->Cdr->Car);
    list = *loc;
    UNLOCK(list);
    LOCK(value);
    *loc = value;
  } else {
    list = *loc;
    *loc = list->Cdr->Cdr;
    list->Cdr->Cdr = NIL;
    UNLOCK(list);
  }
}

DX(xputprop)
{
  ARG_NUMBER(3);
  ALL_ARGS_EVAL;
  ASYMBOL(3);
  putprop(ASYMBOL(1), APOINTER(2), APOINTER(3));
  LOCK(APOINTER(2));
  return APOINTER(2);
}

at *
getprop(struct symbol *symb, at *prop)
{
  at *list;

  list = NIL;
  if (symb->valueptr)
    list = *(symb->valueptr);
  ifn (LISTP(list))
    error(NIL, "symbol value isn't a list", symb->value);
  

  while (CONSP(list) && CONSP(list->Cdr)) {
    if (list->Car == prop) {
      LOCK(list->Cdr->Car);
      return list->Cdr->Car;
    }
    list = list->Cdr->Cdr;
  }
  return NIL;
}

DX(xgetprop)
{

  ARG_NUMBER(2);
  ALL_ARGS_EVAL;
  ASYMBOL(2);
  return getprop(ASYMBOL(1), APOINTER(2));
}


/* --------- TRUE_SYMBOL_DEFINITION --------- */

at *
true(void)
{
  at *q;

  q = at_true;
  LOCK(q);
  return q;
}


/* --------- SYMBOL ACCESS FROM C -------- */


at *
var_get(at *p)
{
  at *r;
  struct symbol *symb;
  symb = p->Object;
  ifn (symb->valueptr) {
    symb->valueptr = &(symb->value);
    symb->value = NIL;
  }
  r = *(symb->valueptr);
  LOCK(r);
  return r;
}

void
var_set(at *p, at *q)
{
  at *r;
  struct symbol *symb;
  symb = p->Object;
  if (symb->mode == SYMBOL_LOCKED)
    error(NIL, "locked symbol", p);
  ifn (symb->valueptr) {
    symb->valueptr = &(symb->value);
    symb->value = NIL;
  }
  r = *(symb->valueptr);
  LOCK(q);
  *(symb->valueptr) = q;
  UNLOCK(r);
}

void
var_SET(at *p, at *q)
{
  at *r;
  struct symbol *symb;
  symb = p->Object;
  ifn (symb->valueptr) {
    symb->valueptr = &(symb->value);
    symb->value = NIL;
  }
  r = *(symb->valueptr);
  LOCK(q);
  *(symb->valueptr) = q;
  UNLOCK(r);
}


void
var_lock(at *p)
{
  ((struct symbol *) (p->Object))->mode = SYMBOL_LOCKED;
}


at *
var_define(char *s)
{
  at *p;
  struct symbol *symb;
  p = named(s);
  symb = (struct symbol *) (p->Object);
  symb->nopurge = TRUE;
  symb->valueptr = &(symb->value);
  UNLOCK(p);
  return p;
}


DX(xset)
{
  at *q;
  
  ARG_NUMBER(2);
  ALL_ARGS_EVAL;
  ASYMBOL(1);
  var_set(APOINTER(1), q = APOINTER(2));
  LOCK(q);
  return q;
}



/* --------- INITIALISATION CODE --------- */

void
init_symbol(void)
{
  at_scope = var_define("scope");
  at_true = var_define("t");
  var_set(at_true, at_true);
  var_lock(at_true);

  class_define("SYMB",&symbol_class );

  dx_define("named", xnamed);
  dx_define("symblist", xsymblist);
  dx_define("oblist", xoblist);
  dx_define("set", xset);
  dx_define("setq", xsetq);
  dx_define("lock_symbol", xlock_symbol);
  dx_define("unlock_symbol", xunlock_symbol);
  dx_define("symbolp", xsymbolp);
  dx_define("incr", xincr);
  dx_define("putp", xputprop);
  dx_define("getp", xgetprop);
}
