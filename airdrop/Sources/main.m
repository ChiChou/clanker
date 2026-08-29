#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#import "AirDrop.h"

static void usage(void) {
    printf("USAGE: %1$s <file1> [file2] [file3] ...\n"
           "    file1, file2, file3, ... – URLs or paths to files to AirDrop\n"
           "    You can specify multiple items - both local files and web URLs, and you can mix them too.\n"
           "\nEXAMPLES:\n"
           "    %1$s document.pdf\n"
           "    %1$s image1.jpg image2.png\n"
           "    %1$s file.txt https://apple.com/\n"
           "\nOPTIONS:\n"
           "    -h, --help – print help info\n",
           getprogname());
}

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        if (argc < 2) {
            usage();
            return 1;
        }

        const char *first_argument = argv[1];
        if (argc == 2 &&
            (strcmp(first_argument, "-h") == 0 || strcmp(first_argument, "--help") == 0)) {
            usage();
            return 0;
        }
        if (argc == 2 && first_argument[0] == '-') {
            fputs("\nError: Unknown option, see usage.\n", stderr);
            usage();
            return 1;
        }

        NSArray<NSString *> *paths = [NSProcessInfo.processInfo.arguments
            subarrayWithRange:NSMakeRange(1, argc - 1)];
        return airdrop_run(paths);
    }
}
