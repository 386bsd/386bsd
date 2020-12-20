/*
 * Dummy library intercept routine that is called by "main" functions
 * compiled by the GCC2 suite of compilers. This mechanism allows
 * languages like C++ the ability to call static constructor functions
 * and to register static destructors to be executed by the atexit()
 * mechanism in turn. If the final linked image has any C++ static
 * destructors/constructors present, the C++ library's version of
 * this routine should be executed prior to the contents of the main
 * function. Since a C main routine might be present in an intermixed
 * executable, only the linker can discern when to link the real
 * intercept routine instead of this dummy, even though all "main"
 * functions are compiled with a call to this routine. Please refrain
 * from making more general use of this artifice, it will probably
 * be going away or changing some time in the future. -wfj
 */
__main() {}
