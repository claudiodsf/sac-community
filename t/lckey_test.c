
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "cpf.h"
#include "co.h"
#include "unit.h"
void *tokenize_line(char *in);

int
main() {
    int rv;

    //printf("ADDING HELLO\n");
    tokenize_line("HELLO");
    //printf("CHECKING ARGUMENT\n");
    
    rv = lckey("THERE", 6);
    ok(rv == 0, "THERE != HELLO %d", rv);

    rv = lckey("HELLO", 0);
    ok(rv == 1, "HELLO == HELLO %d",rv);

    tokenize_line("HELLO");
    rv = lckey("HELLO$", 0);
    ok(rv == 1, "HELLO$ == HELLO %d",rv);

    tokenize_line("HELLO");
    rv = lckey("H#ELLO$", 0);
    ok(rv == 1, "H#ELLO$ == HELLO %d",rv);

    tokenize_line("HELLO");
    rv = lckey("HE#LLO$", 0);
    ok(rv == 1, "HE#LLO$ == HELLO %d",rv);

    tokenize_line("HELLO");
    rv = lckey("HE#LLO$", 0);
    ok(rv == 1, "HE#LLO$ == HELLO %d",rv);

    tokenize_line("HELLO");
    rv = lckey("HEL#LO$", 0);
    ok(rv == 1, "HEL#LO$ == HELLO %d", rv);

    tokenize_line("HELLO");
    rv = lckey("HELL#O$", 0);
    ok(rv == 1, "HELL#O$ == HELLO %d", rv);

    tokenize_line("HELLO");
    rv = lckey("HELLO#$", 0);
    ok(rv == 1, "HELLO#$ == HELLO %d", rv);

    tokenize_line("HELLNO");
    rv = lckey("HEL#LO$", 0);
    ok(rv == 0, "HEL#LO$ != HELLNO %d", rv);

    tokenize_line("triangle-pulse.sac");
    rv = lckey("TRI#ANGLE$", 0);
    ok(rv == 0, "TRI#ANGLE$ ==  triangle-pulse.sac", rv);

    /* zsysop_gets() runs a command and returns what it printed, which is what
     * "systemcommand <cmd> &TO$ <var>" uses to set a blackboard variable.  On
     * Windows its whole body used to be compiled out (it is written around
     * popen()), so that form silently did nothing there; it now uses
     * _popen()/_pclose().  The check lives in this program because it is the
     * only compiled test whose link line pulls in libsac_all.a, where co/
     * lives (see lckey_test_LDADD in t/Makefile.am).  "echo" prints the same
     * to sh and to cmd, so the expectation holds on both platforms. */
    {
        char cmd[] = "echo zsysop_gets ok";
        int len = strlen(cmd), perr = 0;
        char *out = zsysop_gets(cmd, len, &len, &perr);

        ok(perr == 0, "zsysop_gets() reported no error");
        ok(out != NULL, "zsysop_gets() returns the command output");
        if (out != NULL) {
            ok(strcmp(out, "zsysop_gets ok\n") == 0,
               "zsysop_gets() output is <%s>", out);
            free(out);
        }
    }

    TEST_FINISH;

}
