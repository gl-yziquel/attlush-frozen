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
	function.c
	1- LISP function   DE DF DM
	2- C function  DX DY
	$Id: function.c,v 0.1 2001/10/31 17:30:47 profshadoko Exp $
********************************************************************** */


#include "header.h"

static at *at_optional, *at_rest;
static void parse_optional_stuff(at *formal_list, at *real_list);


struct alloc_root function_alloc =
{ NULL,
  NULL,
  sizeof(struct function),
  200,
};


/* General cfunc routines -----------------------------	 */


static void 
cfunc_dispose(at *p)
{
  struct function *func;

  func = p->Object;
  UNLOCK(func->evaluable_list);
  deallocate(&function_alloc, (struct empty_alloc *) func);
}

static void 
cfunc_action(at *p, void (*action) (at *p))
{
  struct function *func;
  func = p->Object;
  (*action) (func->evaluable_list);
}


static char *
dx_name(at *p)
{
  struct function *func;
  func = p->Object;
  if (func->evaluable_list)
    sprintf(string_buffer, "::DX:%s", nameof(func->evaluable_list));
  else
    sprintf(string_buffer, "::DX:%lx", (long)func);
  return string_buffer;
}

static char *
dy_name(at *p)
{
  struct function *func;
  func = p->Object;
  if (func->evaluable_list)
    sprintf(string_buffer, "::DY:%s", nameof(func->evaluable_list));
  else
    sprintf(string_buffer, "::DY:%lx", (long)func);
  return string_buffer;
}




/* General lfunc routines -----------------------------	 */

static void 
lfunc_dispose(at *p)
{
  struct function *func;

  func = p->Object;
  UNLOCK(func->formal_arg_list);
  UNLOCK(func->evaluable_list);
  deallocate(&function_alloc, (struct empty_alloc *) func);
}

static void 
lfunc_action(at *p, void (*action) (at *))
{
  struct function *func;
  func = p->Object;
  (*action) (func->formal_arg_list);
  (*action) (func->evaluable_list);
}




static void
pop_args(at *formal_list)
{
  while (CONSP(formal_list)) {
    pop_args(formal_list->Car);
    formal_list = formal_list->Cdr;
  }
  if (formal_list && (formal_list->flags & X_SYMBOL))
    /* symbol_pop(formal_list);			 */
    /* WE POP IT INLINE				 */
  {
    extern struct alloc_root symbol_alloc;
    struct symbol *symb;

    symb = (struct symbol *) (formal_list->Object);
    if (symb->next) {
      formal_list->Object = symb->next;
      UNLOCK(symb->value);
      deallocate(&symbol_alloc, (struct empty_alloc *) symb);
    }
  }
}


static void
push_args(at *formal_list, at *real_list)
{
  /* fast non tail-recursive loop for parsing the trees */
  
  while (CONSP(formal_list) && 
	 CONSP(real_list) && 
	 formal_list->Car!=at_rest &&
	 formal_list->Car!=at_optional) {
    push_args(formal_list->Car, real_list->Car);
    real_list = real_list->Cdr;
    formal_list = formal_list->Cdr;
  };
  
  /* parsing a single symbol in a tree */
  
  if (formal_list) {
    
    if (formal_list->flags & X_SYMBOL) {
      symbol_push( formal_list, real_list );
      return;
      
    } else if ((formal_list->flags&C_CONS) && 
	       (formal_list->Car==at_optional) ) {
      parse_optional_stuff(formal_list,real_list);
      return;
      
    } else if ((formal_list->flags&C_CONS) && 
	       (formal_list->Car==at_rest) ) {
      parse_optional_stuff(formal_list,real_list);
      return;
      
    } else
      error(NIL,"missing arguments",formal_list);
  }

  if (real_list) {
    error(NIL, "too many arguments", real_list);
  }
}


static void 
parse_optional_stuff(at *formal_list, at *real_list)
{
  if (CONSP(formal_list) && formal_list->Car == at_optional) {

    push_args(at_optional,NIL);
    formal_list = formal_list->Cdr;
    
    while (CONSP(formal_list) && formal_list->Car!=at_rest) {
      if (CONSP(formal_list) 
	  && formal_list->Car
	  && (formal_list->Car->flags&X_SYMBOL)) {
	
	if (CONSP(real_list)) {
	  push_args(formal_list->Car,real_list->Car);
	  real_list = real_list->Cdr;
	} else {
	  push_args(formal_list->Car,NIL);
	}
	
      } else if (CONSP(formal_list) 
		 && CONSP(formal_list->Car)
		 && formal_list->Car->Car
		 && (formal_list->Car->Car->flags&X_SYMBOL)
		 && CONSP(formal_list->Car->Cdr)
		 && formal_list->Car->Cdr->Cdr == NIL ) {
	if (CONSP(real_list)) {
	  push_args(formal_list->Car->Car,real_list->Car);
	  real_list = real_list->Cdr;
	} else {
	  push_args(formal_list->Car->Car,formal_list->Car->Cdr->Car);
	}
	
      } else
	error(NIL,"error in &optional syntax",formal_list);

      formal_list = formal_list->Cdr;
    } 
  }
  if (CONSP(formal_list) && formal_list->Car==at_rest) {
    push_args(at_rest,NIL);
    formal_list=formal_list->Cdr;
    if (CONSP(formal_list)
	&& formal_list->Cdr==NIL
	&& formal_list->Car
	&& (formal_list->Car->flags&X_SYMBOL) ) {
      push_args(formal_list->Car,real_list);
      return;
    } else
      error(NIL,"error in &rest syntax",formal_list);
  }
  
  if (formal_list)
    error(NIL,"error in &optional syntax",formal_list);		
  if (real_list)
    error(NIL,"too many arguments",real_list);
}


at *
eval_a_list(at *p)
{
  at **now;
  at *list;

  list = p;
  p = NIL;
  now = &p;

  while (CONSP(list)) {
    *now = cons((*argeval_ptr) (list->Car), NIL);
    now = &((*now)->Cdr);
    list = list->Cdr;
  }
  if (list)
    *now = eval(list);
  return p;
}


/* DX class -------------------------------------------	 */



at **dx_stack;
at **dx_sp;

at *
dx_listeval(at *p, at *q)
{
  at *answer;
  at **arg_pos, **spbuff;
  int arg_num;
  struct function *cfunc;
  at *(*call) (int, at**);

  cfunc = p->Object;
  arg_pos = spbuff = dx_sp;
  arg_num = 0;
  q = q->Cdr;
  while (CONSP(q)) {
    arg_num++;
    if (++spbuff >= dx_stack + DXSTACKSIZE)
      error(NIL, "sorry, stack full (Merci Yann)", NIL);
    *spbuff = q->Car;
    LOCK(q->Car);
    q = q->Cdr;
  }
  if (q)
    error("dx", "Bad argument list", NIL);

  dx_sp = spbuff;
  call = cfunc->c_function;
  answer = (*call) (arg_num, arg_pos);

  while (arg_pos < spbuff) {
    UNLOCK(*spbuff);
    spbuff--;
  }
  dx_sp = spbuff;

  if (answer && (answer->flags&X_ZOMBIE)) {
    UNLOCK(answer);
    return NIL;
  } else
    return answer;
}



class dx_class =
{
  cfunc_dispose,
  cfunc_action,
  dx_name,
  generic_eval,
  dx_listeval,
};


at *
new_dx(at *(*addr) (int, at **))
{
  struct function *cfunc;
  at *p;

  cfunc = allocate(&function_alloc);
  cfunc->c_function = addr;
  cfunc->formal_arg_list = NIL;
  cfunc->evaluable_list = NIL;
  p = new_extern(&dx_class, cfunc);
  p->flags |= X_FUNCTION;
  cfunc->formal_arg_list = p;
  return p;
}

void 
dx_define(char *name, at *(*addr) (int, at **))
{
  at *func, *symb;

  func = new_dx(addr);
  symb = new_symbol(name);
  if (((struct symbol *) (symb->Object))->mode == SYMBOL_LOCKED) {
    fprintf(stderr, "init: attempt to redefine symbol '%s'\n", name);
  } else {
    var_set(symb, func);
    ((struct symbol *) (symb->Object))->mode = SYMBOL_LOCKED;
    ((struct function*) (func->Object))->evaluable_list = symb;
  }
  UNLOCK(func);
  UNLOCK(symb)
}

/* DY class -------------------------------------------	 */

at *
dy_listeval(at *p, at *q)
{
  struct function *cfunc;
  at *(*call) (at*);
  
  cfunc = p->Object;
  call = cfunc->c_function;
  q = (*call) (q->Cdr);

  if (q && (q->flags&X_ZOMBIE)) {
    UNLOCK(q);
    return NIL;
  } else
    return q;
}


class dy_class =
{
  cfunc_dispose,
  cfunc_action,
  dy_name,
  generic_eval,
  dy_listeval,
};


at *
new_dy(at *(*addr) (at *))
{
  struct function *cfunc;
  at *p;

  cfunc = allocate(&function_alloc);
  cfunc->formal_arg_list = NIL;
  cfunc->evaluable_list = NIL;
  cfunc->c_function = addr;
  p = new_extern(&dy_class, cfunc);
  p->flags |= X_FUNCTION;
  cfunc->formal_arg_list = p;
  return p;
}

void 
dy_define(char *name, at *(*addr) (at *))
{
  at *func, *symb;

  func = new_dy(addr);
  symb = new_symbol(name);
  if (((struct symbol *) (symb->Object))->mode == SYMBOL_LOCKED) {
    fprintf(stderr, "init: attempt to redefine symbol '%s'\n", name);
  } else {
    var_set(symb, func);
    ((struct symbol *) (symb->Object))->mode = SYMBOL_LOCKED;
    ((struct function*) (func->Object))->evaluable_list = symb;
  }
  UNLOCK(func);
  UNLOCK(symb)
}

/* DE class -------------------------------------------	 */

at *
de_listeval(at *p, at *q)
{
  struct function *lfunc;
  at *answer;

  lfunc = p->Object;
  q = eval_a_list(q->Cdr);
  push_args(lfunc->formal_arg_list, q);
  UNLOCK(q);
  answer = progn(lfunc->evaluable_list);
  pop_args(lfunc->formal_arg_list);
  return answer;
}


class de_class =
{
  lfunc_dispose,
  lfunc_action,
  generic_name,
  generic_eval,
  de_listeval,
};


at *
new_de(at *formal, at *evaluable)
{
  struct function *lfunc;
  at *p;

  lfunc = allocate(&function_alloc);
  lfunc->c_function = 0;
  lfunc->formal_arg_list = formal;
  LOCK(formal);
  lfunc->evaluable_list = evaluable;
  LOCK(evaluable);
  p = new_extern(&de_class, lfunc);
  p->flags |= X_FUNCTION;
  return p;
}

DY(ylambda)
{
  at *q;

  q = ARG_LIST;
  ifn(CONSP(q) && CONSP(q->Cdr))
    error("lambda", "illegal definition of function", NIL);
  return new_de(q->Car, q->Cdr);
}

DY(yde)
{
  at *q, *symbol;
  at *func;

  q = ARG_LIST;
  ifn(CONSP(q) && CONSP(q->Cdr) && CONSP(q->Cdr->Cdr))
    error("de", "illegal definition of function", NIL);
  symbol = q->Car;
  ifn(symbol && (symbol->flags & X_SYMBOL))
    error("de", "not a symbol", symbol);
  func = new_de(q->Cdr->Car, q->Cdr->Cdr);
  var_set(symbol, func);
  UNLOCK(func);
  LOCK(symbol);
  return symbol;
}


/* DF class -------------------------------------------	 */

at *
df_listeval(at *p, at *q)
{
  struct function *lfunc;
  at *answer;

  lfunc = p->Object;
  push_args(lfunc->formal_arg_list, q->Cdr);
  answer = progn(lfunc->evaluable_list);
  pop_args(lfunc->formal_arg_list);
  return answer;
}

class df_class =
{
  lfunc_dispose,
  lfunc_action,
  generic_name,
  generic_eval,
  df_listeval,
};

at *
new_df(at *formal, at *evaluable)
{
  struct function *lfunc;
  at *p;

  lfunc = allocate(&function_alloc);
  lfunc->c_function = 0;
  lfunc->formal_arg_list = formal;
  LOCK(formal);
  lfunc->evaluable_list = evaluable;
  LOCK(evaluable);
  p = new_extern(&df_class, lfunc);
  p->flags |= X_FUNCTION;
  return p;
}

DY(yflambda)
{
  at *q;

  q = ARG_LIST;
  ifn(CONSP(q) && CONSP(q->Cdr))
    error("flambda", "illegal definition of function", NIL);
  return new_df(q->Car, q->Cdr);
}

DY(ydf)
{
  at *q, *symbol;
  at *func;

  q = ARG_LIST;
  ifn(CONSP(q) && CONSP(q->Cdr) && CONSP(q->Cdr->Cdr))
    error("df", "illegal definition of function", NIL);
  symbol = q->Car;
  ifn(symbol && (symbol->flags & X_SYMBOL))
    error("df", "not a symbol", symbol);
  func = new_df(q->Cdr->Car, q->Cdr->Cdr);
  var_set(symbol, func);
  UNLOCK(func);
  LOCK(symbol);
  return symbol;
}

/* DM class -------------------------------------------	 */


at *
dm_listeval(at *p, at *q)
{
  struct function *lfunc;

  lfunc = p->Object;
  push_args(lfunc->formal_arg_list, q);
  q = progn(lfunc->evaluable_list);
  pop_args(lfunc->formal_arg_list);
  p = eval(q);
  UNLOCK(q);
  return p;
}


class dm_class =
{
  lfunc_dispose,
  lfunc_action,
  generic_name,
  generic_eval,
  dm_listeval,
};



at *
new_dm(at *formal, at *evaluable)
{
  struct function *lfunc;
  at *p;

  lfunc = allocate(&function_alloc);
  lfunc->c_function = 0;
  lfunc->formal_arg_list = formal;
  LOCK(formal);
  lfunc->evaluable_list = evaluable;
  LOCK(evaluable);
  p = new_extern(&dm_class, lfunc);
  p->flags |= X_FUNCTION;
  return p;
}

DY(ymlambda)
{
  at *q;

  q = ARG_LIST;
  ifn(CONSP(q) && CONSP(q->Cdr))
    error("mlambda", "illegal definition of function", NIL);
  return new_dm(q->Car, q->Cdr);
}

DY(Ydm)
{
  at *q, *symbol;
  at *func;

  q = ARG_LIST;
  ifn(CONSP(q) && CONSP(q->Cdr) && CONSP(q->Cdr->Cdr))
    error("dm", "illegal definition of function", NIL);
  symbol = q->Car;
  ifn(symbol && (symbol->flags & X_SYMBOL))
    error("dm", "not a symbol", symbol);
  func = new_dm(q->Cdr->Car, q->Cdr->Cdr);
  var_set(symbol, func);
  UNLOCK(func);
  LOCK(symbol);
  return symbol;
}

at *
dm_macro_expand(at *p, at *q)
{
  struct function *lfunc;
  at *before_eval;

  ifn(p && (p->flags & C_EXTERN) && p->Class == &dm_class)
    error("macro_expand", "not a macro", p);
  lfunc = p->Object;
  push_args(lfunc->formal_arg_list, q);
  before_eval = progn(lfunc->evaluable_list);
  pop_args(lfunc->formal_arg_list);
  return before_eval;
}

DY(ymacro_expand)
{
  at *p, *ans;

  ifn(CONSP(ARG_LIST) && CONSP(ARG_LIST->Car) && (!ARG_LIST->Cdr))
    error(NIL, "syntax error", NIL);
  p = eval(ARG_LIST->Car->Car);
  ans = dm_macro_expand(p, ARG_LIST->Car);
  UNLOCK(p);
  return ans;
}


/* General purpose routines -------------------	 */

/*
 * function definition extraction given a lisp function, builds the list  (X
 * ARG  {..LST..} ) where  X is   'lambda', 'flambda', 'mlambda', or   'de
 * XX' , 'df XX',   'dm XX', if a name is found, ARG is the formal arguments
 * list, LST is the definition of the function.
 */

at *
funcdef(at *f)
{
  struct function *func;
  at *s;
  
  s = NIL;
  
  if (f && (f->flags & X_SYMBOL)) {
    s = f;
    f = var_get(s);
    UNLOCK(f);
  }

  if (! (f && (f->flags & X_FUNCTION))) {
    if (s) 
      return NIL;
    error(NIL, "neither a function nor a symbol", f);
  }

  func = f->Object;
  if (f->Class == &dx_class || f->Class == &dy_class) {
    LOCK(f);
    return f;
  } else {
    at *q, *p;

    q = new_cons(func->formal_arg_list, func->evaluable_list);

    if (f->Class == &de_class) {
      if (s) {
	LOCK(s);
	p = cons(named("de"), cons(s, q));
      } else
	p = cons(named("lambda"), q);
    } else if (f->Class == &df_class) {
      if (s) {
	LOCK(s);
	p = cons(named("df"), cons(s, q));
      } else
	p = cons(named("flambda"), q);
    } else if (f->Class == &dm_class) {
      if (s) {
	LOCK(s);
	p = cons(named("dm"), cons(s, q));
      } else
	p = cons(named("mlambda"), q);
    } else
      return NIL;
    return p;
  }
}

DX(xfuncdef)
{
  ARG_NUMBER(1);
  ARG_EVAL(1);
  return funcdef(APOINTER(1));
}


DX(xfunctionp)
{
  at *p;

  ARG_NUMBER(1);
  ARG_EVAL(1);
  p = APOINTER(1);

  if (p && (p->flags & X_FUNCTION)) {
    LOCK(p);
    return p;
  } else
    return NIL;
}

/*
 * error routines: need_error... implanted in the macros  APOINTER & co
 */

static char need_arg_num[80];
static char *need_errlist[] =
{"bad argument number",
  "not a number",
  "not a list",
  "empty list",
  "not a string",
  "not a cell",
  "not a clist",
  "not a symbol",
  "not an external object",
  "not a nlf descriptor",
  "not an index"
};


gptr 
need_error(int i, int j, at **arg_array_ptr)
{
  char *s2;
  at *p;

  s2 = need_errlist[i];
  p = NIL;
  if (i)
    p = arg_array_ptr[j];
  else if (j > 1) {
    sprintf(need_arg_num, "%d arguments were expected", j);
    s2 = need_arg_num;
  } else if (j == 1)
    s2 = "one argument was expected";
  else if (j == 0)
    s2 = "no arguments expected";

  ifn(p && p->count)
    p = NIL;
  error(NIL, s2, p);
  return NIL;
}



void 
arg_eval(at **arg_array, int i)
{
  at *temp;

  temp = arg_array[i];
  arg_array[i] = (*argeval_ptr) (temp);
  UNLOCK(temp);
}

void 
all_args_eval(at **arg_array, int i)
{
  at *temp;

  while (i) {
    temp = *++arg_array;
    *arg_array = (*argeval_ptr) (temp);
    UNLOCK(temp);
    i--;
  }
}

/* WARNING: AUTOMATIC PROGRAM GENERATION */

void 
init_function(void)
{
  ifn(dx_stack = tl_malloc(sizeof(at *) * DXSTACKSIZE))
  abort("Not enough memory");

  class_define("DX",&dx_class );
  class_define("DY",&dy_class );
  class_define("DE",&de_class );
  class_define("DF",&df_class );
  class_define("DM",&dm_class );

  at_optional = var_define("&optional");
  at_rest = var_define("&rest");

  dy_define("lambda", ylambda);
  dy_define("de", yde);
  dy_define("flambda", yflambda);
  dy_define("df", ydf);
  dy_define("mlambda", ymlambda);
  dy_define("dm", Ydm);
  dy_define("macro_expand", ymacro_expand);
  dx_define("funcdef", xfuncdef);
  dx_define("functionp", xfunctionp);
}
