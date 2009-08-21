/**
 * @file ap_config.h
 * @brief Symbol export macros and hook functions
 */

#ifndef ZEVENT_CONFIG_H
#define ZEVENT_CONFIG_H

#include "apr.h"
#include "apr_hooks.h"

/** a normal exit */
#define ZEVENTEXIT_OK 0x0
/** A fatal error arising during the server's init sequence */
#define ZEVENTEXIT_INIT 0x2
/**  The child died during its init sequence */
#define ZEVENTEXIT_CHILDINIT 0x3
/**  
 *  *   The child exited due to a resource shortage.
 *   *   The parent should limit the rate of forking until
 *    *   the situation is resolved.
 *     */
#define ZEVENTEXIT_CHILDSICK        0x7
/** 
 *  *     A fatal error, resulting in the whole server aborting.
 *   *     If a child exits with this error, the parent process
 *    *     considers this a server-wide fatal error and aborts.
 *     */
#define ZEVENTEXIT_CHILDFATAL 0xf


#define DECLINED -1/**< Module declines to handle */
#define DONE -2/**< Module has served the response completely */
#define OK 0

#define ZEVENT_NORESTART APR_OS_START_USEERR + 1


#if !defined(WIN32)
/**
 * Apache Core dso functions are declared with ZEVENT_DECLARE(), so they may
 * use the most appropriate calling convention.  Hook functions and other
 * Core functions with variable arguments must use ZEVENT_DECLARE_NONSTD().
 * @code
 * ZEVENT_DECLARE(rettype) ap_func(args)
 * @endcode
 */
#define ZEVENT_DECLARE(type)            type

/**
 * Apache Core dso variable argument and hook functions are declared with 
 * ZEVENT_DECLARE_NONSTD(), as they must use the C language calling convention.
 * @see ZEVENT_DECLARE
 * @code
 * ZEVENT_DECLARE_NONSTD(rettype) ap_func(args [...])
 * @endcode
 */
#define ZEVENT_DECLARE_NONSTD(type)     type

/**
 * Apache Core dso variables are declared with ZEVENT_MODULE_DECLARE_DATA.
 * This assures the appropriate indirection is invoked at compile time.
 *
 * @note ZEVENT_DECLARE_DATA extern type apr_variable; syntax is required for
 * declarations within headers to properly import the variable.
 * @code
 * ZEVENT_DECLARE_DATA type apr_variable
 * @endcode
 */
#define ZEVENT_DECLARE_DATA

#elif defined(ZEVENT_DECLARE_STATIC)
#define ZEVENT_DECLARE(type)            type __stdcall
#define ZEVENT_DECLARE_NONSTD(type)     type
#define ZEVENT_DECLARE_DATA
#elif defined(ZEVENT_DECLARE_EXPORT)
#define ZEVENT_DECLARE(type)            __declspec(dllexport) type __stdcall
#define ZEVENT_DECLARE_NONSTD(type)     __declspec(dllexport) type
#define ZEVENT_DECLARE_DATA             __declspec(dllexport)
#else
#define ZEVENT_DECLARE(type)            __declspec(dllimport) type __stdcall
#define ZEVENT_DECLARE_NONSTD(type)     __declspec(dllimport) type
#define ZEVENT_DECLARE_DATA             __declspec(dllimport)
#endif

/**
 * Declare a hook function
 * @param ret The return type of the hook
 * @param name The hook's name (as a literal)
 * @param args The arguments the hook function takes, in brackets.
 */
#define ZEVENT_DECLARE_HOOK(ret,name,args) \
	APR_DECLARE_EXTERNAL_HOOK(zevent,ZEVENT,ret,name,args)

/** @internal */
#define ZEVENT_IMPLEMENT_HOOK_BASE(name) \
	APR_IMPLEMENT_EXTERNAL_HOOK_BASE(zevent,ZEVENT,name)

/**
 * Implement an Apache core hook that has no return code, and
 * therefore runs all of the registered functions. The implementation
 * is called ap_run_<i>name</i>.
 *
 * @param name The name of the hook
 * @param args_decl The declaration of the arguments for the hook, for example
 * "(int x,void *y)"
 * @param args_use The arguments for the hook as used in a call, for example
 * "(x,y)"
 * @note If IMPLEMENTing a hook that is not linked into the Apache core,
 * (e.g. within a dso) see APR_IMPLEMENT_EXTERNAL_HOOK_VOID.
 */
#define ZEVENT_IMPLEMENT_HOOK_VOID(name,args_decl,args_use) \
	APR_IMPLEMENT_EXTERNAL_HOOK_VOID(zevent,ZEVENT,name,args_decl,args_use)

/**
 * Implement an Apache core hook that runs until one of the functions
 * returns something other than ok or decline. That return value is
 * then returned from the hook runner. If the hooks run to completion,
 * then ok is returned. Note that if no hook runs it would probably be
 * more correct to return decline, but this currently does not do
 * so. The implementation is called ap_run_<i>name</i>.
 *
 * @param ret The return type of the hook (and the hook runner)
 * @param name The name of the hook
 * @param args_decl The declaration of the arguments for the hook, for example
 * "(int x,void *y)"
 * @param args_use The arguments for the hook as used in a call, for example
 * "(x,y)"
 * @param ok The "ok" return value
 * @param decline The "decline" return value
 * @return ok, decline or an error.
 * @note If IMPLEMENTing a hook that is not linked into the Apache core,
 * (e.g. within a dso) see APR_IMPLEMENT_EXTERNAL_HOOK_RUN_ALL.
 */
#define ZEVENT_IMPLEMENT_HOOK_RUN_ALL(ret,name,args_decl,args_use,ok,decline) \
	APR_IMPLEMENT_EXTERNAL_HOOK_RUN_ALL(zevent,ZEVENT,ret,name,args_decl, \
                                            args_use,ok,decline)

/**
 * Implement a hook that runs until a function returns something other than 
 * decline. If all functions return decline, the hook runner returns decline. 
 * The implementation is called ap_run_<i>name</i>.
 *
 * @param ret The return type of the hook (and the hook runner)
 * @param name The name of the hook
 * @param args_decl The declaration of the arguments for the hook, for example
 * "(int x,void *y)"
 * @param args_use The arguments for the hook as used in a call, for example
 * "(x,y)"
 * @param decline The "decline" return value
 * @return decline or an error.
 * @note If IMPLEMENTing a hook that is not linked into the Apache core
 * (e.g. within a dso) see APR_IMPLEMENT_EXTERNAL_HOOK_RUN_FIRST.
 */
#define ZEVENT_IMPLEMENT_HOOK_RUN_FIRST(ret,name,args_decl,args_use,decline) \
	APR_IMPLEMENT_EXTERNAL_HOOK_RUN_FIRST(zevent,ZEVENT,ret,name,args_decl, \
                                              args_use,decline)

#endif /* ZEVENT_CONFIG_H */
