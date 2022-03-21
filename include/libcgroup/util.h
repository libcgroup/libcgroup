#ifndef _LIBCGROUP_UTIL_H_
#define _LIBCGROUP_UTIL_H_

/**
 * \def defer(fn)
 * \brief When used along with a variable, defers freeing up/cleanup
 *
 * When placed next to a variable, calls @param fn. This can be used
 * to simplify cleanup and avoid a lot of the if-err-else style
 * cleanup. NOTE: if the variable is a pointer and it's value is
 * going to be alias'd, please DO NOT use this facility.
 *
 * \param fn function to invoke when the variable goes out of scope
 */
#define defer(fn)	__attribute__((__cleanup__(fn)))

/*
 * Some common cleanup functions
 */
void common_charptr_free(char **ptr);

#endif /* _LIBCGROUP_UTIL_H_ */
