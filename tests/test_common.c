/* Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common functions used by tests.
 */

#include "test_common.h"

#include <stdio.h>

#include "file_keys.h"
#include "rsa_utility.h"
#include "utility.h"

/* ANSI Color coding sequences. */
#define COL_GREEN "\e[1;32m"
#define COL_RED "\e[0;31m"
#define COL_STOP "\e[m"

/* Global test success flag. */
int gTestSuccess = 1;

int TEST_EQ(int result, int expected_result, char* testname) {
  if (result == expected_result) {
    fprintf(stderr, "%s Test " COL_GREEN " PASSED\n" COL_STOP, testname);
    return 1;
  }
  else {
    fprintf(stderr, "%s Test " COL_RED " FAILED\n" COL_STOP, testname);
    gTestSuccess = 0;
    return 0;
  }
}

FirmwareImage* GenerateTestFirmwareImage(int algorithm,
                                         const uint8_t* firmware_sign_key,
                                         int firmware_key_version,
                                         int firmware_version,
                                         int firmware_len,
                                         const char* root_key_file,
                                         const char* firmware_key_file) {
  FirmwareImage* image = FirmwareImageNew();

  Memcpy(image->magic, FIRMWARE_MAGIC, FIRMWARE_MAGIC_SIZE);
  image->firmware_sign_algorithm = algorithm;
  image->firmware_sign_key = (uint8_t*) Malloc(
      RSAProcessedKeySize(image->firmware_sign_algorithm));
  Memcpy(image->firmware_sign_key, firmware_sign_key,
         RSAProcessedKeySize(image->firmware_sign_algorithm));
  image->firmware_key_version = firmware_key_version;

  /* Update correct header length. */
  image->header_len = GetFirmwareHeaderLen(image);

  /* Calculate SHA-512 digest on header and populate header_checksum. */
  CalculateFirmwareHeaderChecksum(image, image->header_checksum);

  /* Populate firmware and preamble with dummy data. */
  image->firmware_version = firmware_version;
  image->firmware_len = firmware_len;
  image->preamble_signature = image->firmware_signature = NULL;
  Memset(image->preamble, 'P', FIRMWARE_PREAMBLE_SIZE);
  image->firmware_data = Malloc(image->firmware_len);
  Memset(image->firmware_data, 'F', image->firmware_len);

  /* Generate and populate signatures. */
  if (!AddFirmwareKeySignature(image, root_key_file)) {
    fprintf(stderr, "Couldn't create key signature.\n");
    FirmwareImageFree(image);
    return NULL;
  }

  if (!AddFirmwareSignature(image, firmware_key_file)) {
    fprintf(stderr, "Couldn't create firmware and preamble signature.\n");
    FirmwareImageFree(image);
    return NULL;
  }
  return image;
}

uint8_t* GenerateTestFirmwareBlob(int algorithm,
                                  const uint8_t* firmware_sign_key,
                                  int firmware_key_version,
                                  int firmware_version,
                                  int firmware_len,
                                  const char* root_key_file,
                                  const char* firmware_key_file) {
  FirmwareImage* image = NULL;
  uint8_t* firmware_blob = NULL;
  uint64_t firmware_blob_len = 0;

  image = GenerateTestFirmwareImage(algorithm,
                                    firmware_sign_key,
                                    firmware_key_version,
                                    firmware_version,
                                    firmware_len,
                                    root_key_file,
                                    firmware_key_file);
  firmware_blob = GetFirmwareBlob(image, &firmware_blob_len);
  FirmwareImageFree(image);
  return firmware_blob;
}

uint8_t* GenerateRollbackTestImage(int firmware_key_version,
                                   int firmware_version,
                                   int is_corrupt) {
  FirmwareImage* image = NULL;
  uint64_t len;
  uint8_t* firmware_blob = NULL;
  uint8_t* firmware_sign_key = NULL;

  firmware_sign_key = BufferFromFile("testkeys/key_rsa1024.keyb",
                                     &len);
  if (!firmware_sign_key)
    return NULL;
  image = GenerateTestFirmwareImage(0,  /* RSA1024/SHA1 */
                                    firmware_sign_key,
                                    firmware_key_version,
                                    firmware_version,
                                    1,  /* Firmware length. */
                                    "testkeys/key_rsa8192.pem",
                                    "testkeys/key_rsa1024.pem");
  if (!image)
    return NULL;
  if (is_corrupt) {
    /* Invalidate image. */
    Memset(image->firmware_data, 'X', image->firmware_len);
  }

  firmware_blob = GetFirmwareBlob(image, &len);
  FirmwareImageFree(image);
  return firmware_blob;
}


KernelImage* GenerateTestKernelImage(int firmware_sign_algorithm,
                                     int kernel_sign_algorithm,
                                     const uint8_t* kernel_sign_key,
                                     int kernel_key_version,
                                     int kernel_version,
                                     int kernel_len,
                                     const char* firmware_key_file,
                                     const char* kernel_key_file) {
  KernelImage* image = KernelImageNew();

  Memcpy(image->magic, KERNEL_MAGIC, KERNEL_MAGIC_SIZE);
  image->header_version = 1;
  image->firmware_sign_algorithm = firmware_sign_algorithm;
  image->kernel_sign_algorithm = kernel_sign_algorithm;
  image->kernel_key_version = kernel_key_version;
  image->kernel_sign_key = (uint8_t*) Malloc(
      RSAProcessedKeySize(image->kernel_sign_algorithm));
  Memcpy(image->kernel_sign_key, kernel_sign_key,
         RSAProcessedKeySize(image->kernel_sign_algorithm));

  /* Update correct header length. */
  image->header_len = GetKernelHeaderLen(image);

  /* Calculate SHA-512 digest on header and populate header_checksum. */
  CalculateKernelHeaderChecksum(image, image->header_checksum);

  /* Populate kernel options and data with dummy data. */
  image->kernel_version = kernel_version;
  image->options.version[0] = 1;
  image->options.version[1] = 0;
  Memset(image->options.cmd_line, 0, sizeof(image->options.cmd_line));
  image->options.kernel_len = kernel_len;
  image->options.kernel_load_addr = 0;
  image->options.kernel_entry_addr = 0;
  image->kernel_key_signature = image->kernel_signature = NULL;
  image->kernel_data = Malloc(kernel_len);
  Memset(image->kernel_data, 'F', kernel_len);

  /* Generate and populate signatures. */
  if (!AddKernelKeySignature(image, firmware_key_file)) {
    fprintf(stderr, "Couldn't create key signature.\n");
    KernelImageFree(image);
    return NULL;
  }

  if (!AddKernelSignature(image, kernel_key_file)) {
    fprintf(stderr, "Couldn't create kernel option and kernel signature.\n");
    KernelImageFree(image);
    return NULL;
  }

  return image;
}

uint8_t* GenerateTestKernelBlob(int firmware_sign_algorithm,
                                int kernel_sign_algorithm,
                                const uint8_t* kernel_sign_key,
                                int kernel_key_version,
                                int kernel_version,
                                int kernel_len,
                                const char* firmware_key_file,
                                const char* kernel_key_file) {
  KernelImage* image = NULL;
  uint8_t* kernel_blob = NULL;
  uint64_t kernel_blob_len = 0;

  image = GenerateTestKernelImage(firmware_sign_algorithm,
                                  kernel_sign_algorithm,
                                  kernel_sign_key,
                                  kernel_key_version,
                                  kernel_version,
                                  kernel_len,
                                  firmware_key_file,
                                  kernel_key_file);

  kernel_blob = GetKernelBlob(image, &kernel_blob_len);
  KernelImageFree(image);
  return kernel_blob;
}
