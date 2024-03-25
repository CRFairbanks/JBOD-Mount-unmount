#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <assert.h>

#include "jbod.h"
#include "mdadm.h"
#include "util.h"
#include "tester.h"

/* Test functions. */
int test_mount_unmount();
int test_read_before_mount();
int test_read_invalid_parameters();
int test_read_within_block();
int test_read_across_blocks();
int test_read_three_blocks();
int test_read_across_disks();

/* Utility functions. */
char *stringify(uint8_t buf[], int length);

int main(int argc, char *argv[])
{
  int score = 0;

  score += test_mount_unmount();
  score += test_read_before_mount();
  score += test_read_invalid_parameters();
  score += test_read_within_block();
  score += test_read_across_blocks();
  score += test_read_three_blocks();
  score += test_read_across_disks();

  printf("Total score: %d/%d\n", score, 10);

  return 0;
}

int test_mount_unmount() {
  printf("running %s: ", __func__);

  int rc = mdadm_mount();
  if (rc != 1) {
    printf("failed: mount should succeed on an unmounted system but it failed.\n");
    return 0;
  }

  rc = mdadm_mount();
  if (rc == 1) {
    printf("failed: mount should fail on an already mounted system but it succeeded.\n");
    return 0;
  }

  if (rc != -1) {
    printf("failed: mount should return -1 on failure but returned %d\n", rc);
    return 0;
  }

  rc = mdadm_unmount();
  if (rc != 1) {
    printf("failed: unmount should succeed on a mounted system but it failed.\n");
    return 0;
  }

  rc = mdadm_unmount();
  if (rc == 1) {
    printf("failed: unmount should fail on an already unmounted system but it succeeded.\n");
    return 0;
  }

  if (rc != -1) {
    printf("failed: unmount should return -1 on failure but returned %d\n", rc);
    return 0;
  }

  printf("passed\n");
  return 3;
}

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

#define SIZE 16

int test_read_before_mount() {
  printf("running %s: ", __func__);

  uint8_t buf[SIZE];
  if (mdadm_read(0, SIZE, buf) != -1) {
    printf("failed: read should fail on an umounted system but it did not.\n");
    return 0;
  }

  printf("passed\n");
  return 1;
}

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

int test_read_invalid_parameters() {
  printf("running %s: ", __func__);

  mdadm_mount();

  bool success = false;
  uint8_t buf1[SIZE];
  uint32_t addr = 0x1fffffff;
  if (mdadm_read(addr, SIZE, buf1) != -1) {
    printf("failed: read should fail on an out-of-bound linear address but it did not.\n");
    goto out;
  }

  addr = 1048570;
  if (mdadm_read(addr, SIZE, buf1) != -1) {
    printf("failed: read should fail if it goes beyond the end of the linear address space but it did not.\n");
    goto out;
  }

  uint8_t buf2[2048];
  if (mdadm_read(0, sizeof(buf2), buf2) != -1) {
    printf("failed: read should fail on larger than 1024-byte I/O sizes but it did not.\n");
    goto out;
  }

  if (mdadm_read(0, SIZE, NULL) != -1) {
    printf("failed: read should fail when passed a NULL pointer and non-zero length but it did not.\n");
    goto out;
  }

  if (mdadm_read(0, 0, NULL) != 0) {
    printf("failed: 0-length read should succeed with a NULL pointer but it did not.\n");
    goto out;
  }
  success = true;

out:
  mdadm_unmount();
  if (!success)
    return 0;

  printf("passed\n");
  return 1;
}

/*
 * This test reads the first 16 bytes of the linear address, which corresponds
 * to the first 16 bytes of the 0th block of the 0th disk.
 */
int test_read_within_block() {
  printf("running %s: ", __func__);

  mdadm_mount();

  /* Set the contents of JBOD drives to a specific pattern. */
  jbod_initialize_drives_contents();

  bool success = false;
  uint8_t out[SIZE];
  if (mdadm_read(0, SIZE, out) != SIZE) {
    printf("failed: read failed\n");
    return 0;
  }

  uint8_t expected[SIZE] = {
    0xaa, 0xaa, 0xaa, 0xaa,
    0xaa, 0xaa, 0xaa, 0xaa,
    0xaa, 0xaa, 0xaa, 0xaa,
    0xaa, 0xaa, 0xaa, 0xaa,
  };

  if (memcmp(out, expected, SIZE) != 0) {
    char *out_s = stringify(out, SIZE);
    char *expected_s = stringify(expected, SIZE);

    printf("failed:\n  got:      %s\n  expected: %s\n", out_s, expected_s);

    free(out_s);
    free(expected_s);
    goto out;
  }
  success = true;

out:
  mdadm_unmount();
  if (!success)
    return 0;

  printf("passed\n");
  return 1;
}

/*
 * This test reads 16 bytes starting at the linear address 248, which
 * corresponds to the last 8 bytes of the 0th block and first 8 bytes of the 1st
 * block, both on the 0th disk.
 */
int test_read_across_blocks() {
  printf("running %s: ", __func__);

  mdadm_mount();

  /* Set the contents of JBOD drives to a specific pattern. */
  jbod_initialize_drives_contents();

  bool success = false;
  uint8_t out[SIZE];
  if (mdadm_read(248, SIZE, out) != SIZE) {
    printf("failed: read failed\n");
    goto out;
  }

  uint8_t expected[SIZE] = {
    0xaa, 0xaa, 0xaa, 0xaa,
    0xaa, 0xaa, 0xaa, 0xaa,
    0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb,
  };

  if (memcmp(out, expected, SIZE) != 0) {
    char *out_s = stringify(out, SIZE);
    char *expected_s = stringify(expected, SIZE);

    printf("failed:\n  got:      %s\n  expected: %s\n", out_s, expected_s);

    free(out_s);
    free(expected_s);
    goto out;
  }
  success = true;

out:
  mdadm_unmount();
  if (!success)
    return 0;

  printf("passed\n");
  return 1;
}

/*
 * This test reads 258 bytes starting at the linear address 255, which
 * corresponds to the last byte of the 0th block, all bytes of the 1st block,
 * and the first byte of the 2nd block, where all blocks are the 0th disk.
 */

#define TEST3_SIZE 258

int test_read_three_blocks() {
  printf("running %s: ", __func__);

  mdadm_mount();

  /* Set the contents of JBOD drives to a specific pattern. */
  jbod_initialize_drives_contents();

  bool success = false;
  uint8_t out[TEST3_SIZE];
  if (mdadm_read(255, TEST3_SIZE, out) != TEST3_SIZE) {
    printf("failed: read failed\n");
    goto out;
  }

  uint8_t expected[TEST3_SIZE] = {
    0xaa, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb,
    0xbb, 0xcc,
  };

  if (memcmp(out, expected, TEST3_SIZE) != 0) {
    char *out_s = stringify(out, TEST3_SIZE);
    char *expected_s = stringify(expected, TEST3_SIZE);

    printf("failed:\n  got:\n%s\n  expected:\n%s\n", out_s, expected_s);

    free(out_s);
    free(expected_s);
    goto out;
  }
  success = true;

out:
  mdadm_unmount();
  if (!success)
    return 0;

  printf("passed\n");
  return 1;
}

/*
 * This test reads 16 bytes starting at the linear address 983032, which
 * corresponds to the last 8 bytes of disk 14 and first 8 bytes on disk 15.
 */
int test_read_across_disks() {
  printf("running %s: ", __func__);

  int rc = mdadm_mount();
  if (rc != 1) {
    printf("failed: mount should succeed on an unmounted system but it failed.\n");
    return 0;
  }

  /* Set the contents of JBOD drives to a specific pattern. */
  jbod_initialize_drives_contents();

  bool success = false;
  uint8_t out[SIZE];
  if (mdadm_read(983032, SIZE, out) != SIZE) {
    printf("failed: read failed\n");
    goto out;
  }

  uint8_t expected[SIZE] = {
    0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
  };

  if (memcmp(out, expected, SIZE) != 0) {
    char *out_s = stringify(out, SIZE);
    char *expected_s = stringify(expected, SIZE);

    printf("failed:\n  got:\n%s\n  expected:\n%s\n", out_s, expected_s);

    free(out_s);
    free(expected_s);
    goto out;
  }
  success = true;

out:
  mdadm_unmount();
  if (!success)
    return 0;

  printf("passed\n");
  return 2;
}

char *stringify(uint8_t buf[], int length) {
  char *p = (char *)malloc(length * 6);
  for (int i = 0, n = 0; i < length; ++i) {
    if (i && i % 16 == 0)
      n += sprintf(p + n, "\n");
    n += sprintf(p + n, "0x%02x ", buf[i]);
  }
  return p;
}