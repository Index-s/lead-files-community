// See http://www.boost.org/libs/any for Documentation.

#ifndef __IPKN_ANY_FUNCTION_VARIATION_OF_BOOST_ANY_INCLUDED
#define __IPKN_ANY_FUNCTION_VARIATION_OF_BOOST_ANY_INCLUDED

// what:  variant type boost::any
// who:   contributed by Kevlin Henney,
//        with features contributed and bugs found by
//        Ed Brey, Mark Rodgers, Peter Dimov, and James Curran
// when:  July 2001
// where: tested with BCC 5.5, MSVC 6.0, and g++ 2.95

#include <algorithm>
#include <typeinfo>

#define boost _boost_func_of_SQLMsg
#define func_arg_type SQLMsg*
#define func_arg pmsg
#include "any_function.inc"
#undef func_arg
#undef func_arg_type
#undef boost

typedef _boost_func_of_SQLMsg::any any_function;

#define boost _boost_func_of_void
#define func_arg_type 
#define func_arg 
#include "any_function.inc"
#undef func_arg
#undef func_arg_type
#undef boost

typedef _boost_func_of_void::any any_void_function;

// Copyright Kevlin Henney, 2000, 2001, 2002. All rights reserved.
//
// Permission to use, copy, modify, and distribute this software for any
// purpose is hereby granted without fee, provided that this copyright and
// permissions notice appear in all copies and derivatives.
//
// This software is provided "as is" without express or implied warranty.

#endif


