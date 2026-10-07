/** 
 * @file   zfiles.c
 * 
 * @brief  Get a list of files
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

/* glob() is used on every platform: on Windows win/compat/glob.c provides
 * it (and matches the pattern itself, which FindFirstFile() does not do for
 * the [...] character classes). */
#include <glob.h>
#ifdef WIN32
#include <windows.h>
#endif

#include "clf.h"
#include "co.h"
#include "bool.h"
#include "msg.h"

#include "errors.h"
#include "string_utils.h"
#include "debug.h"

/** 
 * Obtain a list of files in a directory given a regualr expression.
 *    Hidden files and sub-directories are not included.
 * 
 * @param kdirin 
 *    Directory to search
 * @param kpatrn 
 *    Regular Expression to be matched
 * @param files 
 *    Character array of files found. \p files is passed back to the 
 *      calling function. The calling function is responsible for
 *      freeing the memory allocated here.
 * @param nFiles 
 *    Number of files found
 * @param nErr 
 *    Error Return Flag
 *    - 0 on Success
 *    - ERROR_OUT_OF_MEMORY
 * 
 * @return 
 *    - TRUE on Successful completion, \p nFiles may be 0
 *    - FALSE on Error
 *
 * @date   961031:  Added kpatrn, a string (containing wile card characters)
 *                  which indicates the files to be opened. Reworked the way
 *                  that strings are used.  maf
 * @date   961018:  Overhauled to use dynamic memory allocation, and make
 *                  files local to the calling function.  Removed the 
 *                  variables faLen and stringLen.  files is now a pointer
 *                  to a pointer so that the pointer to the allocated space
 *                  is passed by reference.  maf
 * @date   910703:  Changed drLen, faLen to *drLen, *faLen
 *                  because fortran2c passing of string lens seems buggy
 *
 * @note Base for wildfl
 *
 */
string_list *
zfiles(char *kdirin, char *kpatrn, int *nErr) {

    char *command;
    string_list *files;
    int i;
    glob_t g;

    *nErr = 0;
    files = string_list_init();

    asprintf(&command, "%s%s", kdirin, kpatrn);
    if (!command) {
        *nErr = ERROR_OUT_OF_MEMORY;
        error(*nErr, "Error: Insufficient memory to read in this directory");
        outmsg();
        clrmsg();
        string_list_free(files);
        return NULL;
    }

    *nErr = 0;

    /* Use glob() on every platform.  On Windows win/compat/glob.c provides
     * it, and it matches the pattern itself instead of relying on
     * FindFirstFile(), which does not understand the [...] character classes
     * that patterns such as "arr_[r,b]*sac" need. */
    glob(command, 0, NULL, &g);
    for (i = 0; i < (int) g.gl_pathc; i++) {
        string_list_put(files, g.gl_pathv[i], -1);
    }
    free(command);
    globfree(&g);
    return files;
}
