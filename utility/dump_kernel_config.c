/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Exports the kernel commandline from a given partition/image.
 */


#include <getopt.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "vboot_host.h"

enum {
  OPT_KLOADADDR = 1000,
};

static struct option long_opts[] = {
  { "kloadaddr", 1, NULL, OPT_KLOADADDR },
  { NULL, 0, NULL, 0 }
};

/* Print help and return error */
static int PrintHelp(void) {
  puts("dump_kernel_config - Prints the kernel command line\n"
       "\n"
       "Usage:  dump_kernel_config [--kloadaddr <ADDRESS>] "
       "<image/blockdevice>\n"
       "\n"
       "");
  return 1;
}

int main(int argc, char* argv[]) {
  char *infile = NULL;
  char *config = NULL;
  uint64_t kernel_body_load_address = USE_PREAMBLE_LOAD_ADDR;
  int parse_error = 0;
  char *e;
  int i;

  while (((i = getopt_long(argc, argv, ":", long_opts, NULL)) != -1) &&
         !parse_error) {
    switch (i) {
      default:
      case '?':
        /* Unhandled option */
        parse_error = 1;
        break;

      case 0:
        /* silently handled option */
        break;

      case OPT_KLOADADDR:
        kernel_body_load_address = strtoul(optarg, &e, 0);
        if (!*optarg || (e && *e)) {
          fprintf(stderr, "Invalid --kloadaddr\n");
          parse_error = 1;
        }
        break;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Expected argument after options\n");
    parse_error = 1;
  } else
    infile = argv[optind];

  if (parse_error)
    return PrintHelp();

  if (!infile || !*infile) {
    fprintf(stderr, "Must specify filename\n");
    return 1;
  }

  config = FindKernelConfig(infile, kernel_body_load_address);
  if (!config)
    return 1;

  printf("%s", config);

  free(config);
  return 0;
}
