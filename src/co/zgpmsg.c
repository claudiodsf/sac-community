/** 
 * @file   zgpmsg.c
 * 
 * @brief  Print a prompt and get a reply
 * 
 */

#include <stdio.h>
#include <string.h>

#include "mach.h"
#include "co.h"

#include "config.h"

#include "select.h"
#include "debug.h"

#if !defined(READLINE)
void
zgpmsg(prmt, prmtlen, msg, msglen)
     char *prmt;                /* pointer to prompt message */
     int prmtlen;               /* length of prmt array */
     char *msg;                 /* pointer to character array to receive input */
     int msglen;                /* length of msg array */

{
    int i;                      /* index for prefilling string w/ NULLs */
    char *psave;                /* save msg */

    psave = msg;
    for (i = 0; i < (int) msglen; ++i)  /* prefill with NULLs */
        *(psave++) = '\0';

    if (use_tty()) {
        while (*prmt != '$')
            putchar(*(prmt++)); /* print prompt */
    }

    fflush(stdout);

    if (!use_tty()) {
        /* This is the plot commands' "wait for a keypress between frames".
         * There is no terminal and so no key to wait for, but the wait still
         * has to consume its input, and it has to consume a whole line: with
         * readline enabled co/select.c does exactly that, by reading the line
         * with getline_stdin(), and the testsuite reference output was produced
         * with such a build.  Reading only the first msglen-1 characters - what
         * getfline() below does - left the rest of the line behind, and the
         * command dispatcher then ran it: sm/plot's "plot illegaloption" lost
         * its first nine characters and "galoption" was executed as a system
         * command, which cmd.exe reported as an unknown command.
         *
         * Note this whole file is only compiled without readline, so builds
         * that have it are unaffected. */
        int c, i = 0;
        while ((c = getc(stdin)) != EOF && c != '\n') {
            if (i < (int) msglen - 1)
                msg[i++] = (char) c;
        }
        msg[i] = '\0';
        return;
    }

/* A control-d sets the message response to quit */
    if (getfline(stdin, msg, (short) msglen) == -1)
        if (msglen >= 5)
            strncpy(msg, "quit", 4);

}

#else

/** 
 * Process a command line
 * 
 * @param p 
 *   String (command) to process
 *
 */
static void
process_line(char *p) {
    select_loop_continue(SELECT_OFF);   /* Turn off select loop */
    select_loop_message(p, SELECT_MSG_SET);     /* Set the outgoing message */
    FREE(p);
}

/** 
 * Print a prompt and receive a reply
 * 
 * @param prmt 
 *    Prompt
 * @param prmtlen 
 *    Length of \p prmt
 * @param msg 
 *    Reply
 * @param msglen 
 *    Length of \p msg
 *
 * @note Calls co/select_loop()
 */
void
zgpmsg(char *prmt, int prmtlen, char *msg, int msglen) {
    select_loop(prmt, prmtlen, msg, msglen, NULL, process_line, TRUE, TRUE);
}

#endif /* !READLINE */
