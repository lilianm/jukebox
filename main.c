#include "jukebox.h"

int main(int argc, char *argv[])
{
    int            port           = 8085;

    (void) argc;
    (void) argv;

    jukebox_init(port);
    jukebox_launch();

    return 0;
}

