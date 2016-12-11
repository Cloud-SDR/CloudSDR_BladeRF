////////////////////////////////////////////////////////
// CRAP Library
//	types.h
//
//	Creator:
//		Steffen Kopany <steffen@kopany.at>
//
//	Author(s):
// 	Steffen Kopany <steffen@kopany.at>
//
//	Copyright (c) 2012 Steffen Kopany
//
//	Description:
//		Caring about default types
//
//	Status (scratch, developed, final):
//		scratch
//
////////////////////////////////////////////////////////

#pragma once

#ifndef CRAP_CONFIG_TYPES_H
#define CRAP_CONFIG_TYPES_H

//standard defs if available
#if ( defined(CRAP_COMPILER_GCC) || defined(CRAP_COMPILER_VC) )
	#include <stddef.h>
#endif

//null pointer
#if defined(NULL)
    #undef NULL
#endif
#if defined(__cplusplus)
	#define NULL 0
#else
	#define NULL ((void*)0)
#endif

//getting sizetypes print & scan
#ifndef __STDC_FORMAT_MACROS // for inttypes.h
	#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <stdint.h>
#include <math.h>
#include <float.h>

//shortcut typedefs
typedef bool b8;
typedef char c8;
typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef float_t f32;
typedef double_t f64;
typedef intptr_t iptr;
typedef uintptr_t uptr;
typedef const char cstring;

#endif // CRAP_CONFIG_TYPES_H
