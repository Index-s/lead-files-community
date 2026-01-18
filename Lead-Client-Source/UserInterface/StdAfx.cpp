// stdafx.cpp : source file that includes just the standard includes
//	UserInterface.pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

// TODO: Remove workaround by using jpeg build from vcpkg
//       This is just needed to make the compiler shut up for now.
extern "C" {
    FILE* __cdecl __iob_func(void) {
        static FILE* iob[3] = { stdin, stdout, stderr };
        return iob[0];
    }
}