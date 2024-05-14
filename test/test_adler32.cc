/* test_adler32.c -- unit test for adler32 in the zlib compression library
 * Copyright (C) 2020 IBM Corporation
 * Author: Rogerio Alves <rcardoso@linux.ibm.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#  include "zbuild.h"
#  include "test_cpu_features.h"
}

#include <gtest/gtest.h>

typedef struct {
    uint32_t adler;
    const uint8_t *buf;
    uint32_t len;
    uint32_t expect;
} adler32_test;

static const uint8_t long_string[5552] = {
    'q','j','d','w','q','4','8','m','B','u','k','J','V','U','z','V','V','f','M','j','i','q','S','W','L','5','G','n','F','S','P','Q',
    'Q','D','i','6','m','E','9','Z','a','A','P','h','9','d','r','b','5','t','X','U','U','L','w','q','e','k','E','H','6','W','7','k',
    'A','x','N','Q','R','k','d','V','5','y','n','U','N','W','Q','Y','i','W','5','9','R','p','D','C','x','p','u','h','C','a','m','r',
    'z','n','z','A','d','J','6','u','N','e','r','x','7','Q','3','v','V','h','H','S','H','S','f','K','f','e','E','T','9','J','f','K',
    'w','t','x','J','2','y','7','B','x','X','X','p','G','b','T','g','3','k','U','6','E','Z','M','t','J','q','v','n','S','T','6','x',
    '5','x','4','P','z','p','M','F','V','b','d','m','f','G','n','J','m','w','z','K','8','a','q','E','D','e','b','3','h','B','V','g',
    'y','3','P','L','5','8','r','z','X','b','Q','g','H','7','L','c','Z','B','3','C','4','y','t','u','k','z','h','v','C','Y','p','p',
    '8','H','v','5','X','w','4','L','R','V','V','4','U','C','8','4','T','E','a','N','Z','S','7','U','u','z','f','H','p','P','J','u',
    'Y','Z','h','T','6','e','v','z','V','F','h','u','y','H','b','k','J','M','f','3','6','g','y','L','E','W','t','B','B','d','d','9',
    'u','M','Z','k','F','G','f','h','q','k','5','k','f','r','M','7','c','M','7','y','n','u','8','b','d','7','Q','f','E','m','F','K',
    'x','W','f','B','2','F','8','5','q','z','y','3','R','i','U','m','X','k','h','N','J','y','B','C','h','u','x','4','f','k','J','5',
    '6','X','T','W','h','8','J','4','m','K','p','N','3','g','C','g','A','E','e','Z','x','A','P','2','E','4','t','Q','5','X','Y','j',
    '6','m','b','h','G','a','v','6','t','v','6','C','M','G','P','u','B','C','A','V','b','2','9','d','2','c','5','a','b','X','w','V',
    'G','6','a','7','c','8','G','6','K','U','Q','m','w','P','V','5','N','x','b','v','x','E','N','C','A','N','t','v','N','B','z','X',
    'B','R','q','U','n','i','A','Q','d','m','a','D','7','Y','f','3','J','8','Y','m','w','Z','b','w','r','H','q','E','j','c','u','E',
    'i','i','S','b','n','G','P','a','F','j','c','R','D','D','G','F','v','i','a','i','M','7','B','e','w','m','L','E','F','2','Y','4',
    '4','7','Y','C','t','y','q','7','2','V','G','m','m','E','e','V','u','m','L','p','R','X','W','z','V','K','E','k','p','V','r','J',
    'd','N','3','t','i','u','S','V','w','2','w','U','Q','3','F','q','4','h','q','k','B','7','R','X','B','F','Q','Z','b','b','4','E',
    'K','v','T','B','w','k','V','C','x','d','K','g','N','S','u','k','p','9','z','w','c','y','U','M','V','E','2','Y','P','F','h','9',
    'T','y','h','w','b','9','P','w','G','c','W','W','k','j','J','Q','N','B','U','G','6','9','U','b','v','a','N','9','N','C','G','n',
    'x','R','6','9','Q','C','h','e','j','P','U','h','U','R','i','4','T','B','W','5','w','m','J','p','e','7','r','9','t','c','9','Z',
    'j','p','r','F','C','e','U','P','x','T','A','N','7','6','a','i','y','e','w','F','C','X','H','Y','G','C','q','q','m','A','t','7',
    'z','u','D','S','L','U','C','f','7','e','t','G','V','F','u','c','x','5','M','7','N','i','M','6','h','2','n','H','S','h','K','M',
    'd','T','z','X','d','x','x','4','q','z','d','D','a','2','X','r','p','r','R','m','U','U','y','S','H','c','a','F','e','Z','a','U',
    'P','9','V','J','e','q','j','Y','M','x','e','v','K','7','M','P','N','2','b','6','f','P','h','H','4','U','X','k','n','f','Q','M',
    '9','9','a','J','N','e','w','y','f','F','P','p','a','F','Y','a','M','L','W','i','T','M','B','3','U','v','X','v','G','p','7','a',
    'f','u','4','S','y','X','9','g','g','b','B','G','c','i','M','U','n','m','a','7','q','f','9','n','Q','2','V','L','6','e','T','R',
    '2','4','9','d','6','Q','B','Y','q','2','4','9','G','Q','E','b','Y','5','u','2','T','Q','G','L','5','n','4','Y','2','y','G','F',
    'j','c','8','M','G','L','e','3','a','N','v','A','A','W','t','R','S','2','i','D','R','8','j','d','Q','3','6','C','V','M','e','w',
    'j','U','Z','w','M','4','b','m','8','J','P','Q','L','P','R','c','r','b','V','C','3','N','8','K','4','d','W','D','N','U','A','A',
    '2','J','p','b','D','d','p','j','N','C','k','A','j','B','a','c','u','v','L','X','U','B','4','U','X','W','e','C','b','C','u','d',
    'A','v','U','z','P','t','D','e','5','y','Y','c','x','K','4','7','j','e','e','D','M','5','K','B','Q','6','d','p','T','T','R','j',
    'M','E','E','M','r','N','6','8','7','q','x','F','S','x','E','U','4','d','B','6','5','W','C','e','m','J','e','5','j','w','V','J',
    'w','v','d','7','v','f','K','u','m','8','h','W','T','e','Q','j','M','8','R','Y','d','B','R','2','r','F','j','7','d','E','q','V',
    'k','e','j','P','9','3','X','R','p','R','b','A','v','7','4','A','M','2','k','r','E','7','X','3','7','k','5','c','B','7','W','5',
    'u','J','B','Q','R','2','V','7','h','Q','h','9','g','G','y','c','c','x','M','z','7','G','2','J','w','v','j','5','9','E','b','k',
    'z','W','T','C','b','4','K','R','X','T','k','V','S','G','2','j','d','6','y','E','4','P','H','K','w','a','m','F','Z','x','9','j',
    'i','2','d','X','u','a','4','a','M','z','8','p','p','z','g','t','H','5','Y','L','Q','c','R','F','m','E','n','G','X','d','f','7',
    'x','8','j','g','J','z','D','S','a','S','h','y','5','h','Y','N','p','w','Y','W','h','E','N','v','8','Q','D','W','Z','k','f','e',
    'r','Z','D','7','R','D','T','2','H','X','z','G','X','f','v','E','z','P','v','U','H','e','4','R','W','U','x','t','t','4','w','p',
    'r','z','K','9','f','g','h','P','r','f','v','k','h','c','e','5','8','a','L','F','J','M','G','R','a','N','q','S','g','W','e','7',
    'R','K','R','A','B','z','6','v','S','p','w','n','e','x','k','E','r','j','f','Y','x','8','9','z','e','T','6','E','G','v','9','f',
    'D','A','N','v','y','U','7','D','M','2','E','5','W','G','6','b','9','q','g','Y','F','f','k','q','Q','E','x','Y','C','R','G','6',
    'R','h','4','J','d','U','D','b','9','b','8','r','f','V','d','g','b','2','z','Z','d','m','X','v','j','Y','d','w','K','8','G','r',
    'v','j','N','y','c','h','u','5','z','g','J','H','a','Z','b','z','G','C','r','P','f','y','P','6','F','P','h','7','9','w','7','y',
    'R','3','n','E','h','G','D','4','m','Y','E','q','k','a','f','a','R','B','q','t','W','E','T','p','H','7','k','X','2','d','X','6',
    'W','n','H','m','w','M','i','Y','M','E','F','5','R','p','p','y','c','b','q','R','9','Y','t','T','7','w','u','K','M','Q','z','n',
    'P','7','g','x','6','R','4','x','N','v','w','M','6','j','K','v','7','a','Y','4','a','M','6','n','z','3','E','2','V','N','4','i',
    'E','f','u','W','J','W','e','8','3','Q','e','a','F','P','c','3','P','k','i','z','d','q','m','q','M','a','d','8','D','3','F','M',
    'e','d','E','j','z','V','e','d','z','H','D','J','8','X','g','E','i','u','c','7','A','w','S','J','2','A','e','8','r','q','C','m',
    '9','9','a','g','2','y','y','P','M','e','8','3','T','r','m','8','j','v','r','p','M','Z','Y','g','a','9','2','d','H','B','m','9',
    '4','6','a','Z','V','u','S','H','g','3','X','h','i','N','3','B','S','E','k','9','k','2','9','R','A','i','3','L','X','M','B','S',
    '4','S','F','F','F','w','u','d','M','T','9','K','B','7','R','U','R','8','D','8','T','5','U','t','E','R','x','n','x','h','v','k',
    'B','N','k','E','U','T','t','p','r','u','Z','h','t','E','4','i','P','z','f','z','q','M','p','f','A','K','2','D','t','j','f','c',
    'Y','E','N','M','x','k','g','7','T','U','2','c','d','V','g','2','z','L','i','j','Y','q','b','T','A','y','v','a','t','N','5','t',
    'Z','5','n','D','a','y','G','n','P','x','V','k','M','8','t','J','Z','G','g','5','9','R','h','P','P','J','N','X','p','G','J','p',
    '2','y','A','v','d','G','U','z','3','V','M','y','q','U','N','M','Y','p','B','Z','U','h','j','q','z','q','x','w','7','d','J','Q',
    'u','F','q','3','m','9','c','Q','W','d','6','7','b','V','M','7','P','j','r','k','9','h','R','z','m','b','i','B','u','E','L','9',
    'k','v','h','h','W','2','K','e','M','U','Q','p','A','Q','Y','J','G','E','T','U','L','f','q','G','4','z','K','K','y','a','U','W',
    'K','D','P','c','N','D','V','S','Y','6','T','p','R','y','y','J','a','T','J','W','Q','9','p','F','P','X','y','k','9','z','z','4',
    'G','d','a','z','X','n','h','4','J','P','W','V','D','r','U','m','a','8','a','b','X','F','J','X','L','4','S','X','5','W','p','W',
    'h','y','x','B','f','d','C','X','w','7','r','g','V','T','H','a','i','4','N','v','c','w','n','2','3','A','i','A','J','9','N','c',
    'z','7','n','n','3','n','h','n','i','R','i','b','E','h','k','U','c','c','U','6','f','x','q','N','y','H','M','e','J','B','U','B',
    'r','g','a','8','V','a','G','V','y','u','c','c','v','C','H','W','y','g','z','Q','2','4','k','S','m','f','e','G','H','v','Q','3',
    'P','e','f','S','V','P','c','U','e','3','P','x','d','c','7','c','f','g','D','w','2','t','q','y','g','2','Q','V','4','K','a','Q',
    'g','B','b','L','x','9','m','a','K','4','i','x','g','Q','M','9','W','N','2','w','p','v','2','k','B','y','9','k','A','c','f','Z',
    'D','R','A','S','d','v','w','f','f','q','t','K','3','j','x','D','G','P','n','u','r','v','U','k','A','2','d','R','N','T','G','4',
    'B','g','k','t','h','7','J','k','F','A','C','g','W','g','J','F','z','S','Q','c','v','M','b','D','e','H','Q','S','j','v','G','E',
    'R','k','f','i','P','E','F','N','6','y','p','b','t','M','c','Q','B','7','g','w','J','7','3','d','V','E','m','z','6','6','P','P',
    'd','i','r','J','H','D','H','J','r','b','n','v','z','W','e','u','g','B','u','Z','2','m','D','5','h','F','X','B','2','r','6','w',
    'u','Y','4','N','X','K','a','v','V','3','j','B','r','r','C','c','w','R','g','S','8','V','b','F','2','N','M','c','K','8','Y','E',
    'E','N','K','X','K','V','B','x','n','Q','p','a','q','f','k','t','z','Y','E','P','Z','y','n','a','c','B','V','a','x','b','d','X',
    'r','d','8','P','H','F','v','r','V','5','g','J','w','6','i','h','d','d','p','J','c','c','Y','S','q','W','m','U','5','G','b','H',
    'N','z','E','Z','K','E','y','M','c','G','i','d','w','Z','D','N','N','w','S','t','g','y','a','Y','b','H','e','M','N','f','Y','Y',
    '7','a','9','b','M','U','k','a','V','k','C','n','a','k','U','H','A','M','i','v','k','t','a','d','i','3','F','d','5','2','A','p',
    'U','c','J','U','R','h','G','d','A','Y','v','q','X','c','w','r','x','4','j','3','4','b','F','d','a','L','N','J','3','Z','g','6',
    'W','Q','R','u','P','t','M','A','3','F','6','y','K','Y','G','2','t','v','u','p','w','b','G','S','K','5','p','4','d','E','w','6',
    'g','t','V','4','b','2','n','b','Z','3','3','f','m','d','2','c','a','m','j','X','U','E','D','6','6','F','w','H','9','7','Z','Y',
    'd','X','C','K','i','g','p','F','Y','n','2','b','F','4','R','u','V','k','f','d','J','i','a','b','X','H','7','v','K','a','Q','i',
    'W','M','j','M','i','a','i','n','F','h','r','q','4','w','x','m','4','q','y','F','8','w','i','4','D','B','A','L','B','U','u','K',
    'v','K','n','a','Q','i','e','k','v','Q','U','5','w','Q','c','r','A','6','M','w','y','g','n','e','v','K','7','W','u','2','y','f',
    'Q','u','e','r','y','a','w','V','p','f','Q','z','C','u','i','i','9','S','P','q','L','r','C','H','S','3','E','p','8','S','m','Q',
    'S','K','r','V','b','J','R','m','w','c','n','Q','N','Q','4','M','u','f','X','S','f','U','Z','x','U','4','j','K','4','G','z','X',
    '7','Q','j','R','h','i','G','m','q','c','V','T','x','U','a','E','b','Q','q','E','i','F','K','7','K','i','R','J','5','Y','F','V',
    'B','7','R','8','M','i','f','j','Z','w','j','b','B','u','p','N','Y','r','S','r','f','h','E','J','T','B','P','R','D','V','K','A',
    'Z','A','R','j','z','f','B','i','Y','L','F','G','V','Y','w','R','C','P','G','m','9','7','C','5','e','y','w','N','K','N','a','Q',
    'j','a','W','3','2','f','G','w','n','M','6','F','u','K','8','g','8','M','G','r','e','9','Z','z','y','2','G','U','k','G','6','m',
    'A','D','4','n','b','8','a','q','S','m','S','6','5','R','5','D','5','S','B','g','X','T','8','Q','V','d','A','n','g','y','8','a',
    'h','7','K','9','H','D','J','F','w','G','4','w','T','J','F','f','i','8','X','e','B','J','K','H','7','V','y','X','7','E','8','S',
    'A','d','b','w','S','8','Y','a','J','d','j','E','V','J','T','E','U','R','5','7','V','M','E','v','D','3','z','5','r','k','z','v',
    'e','m','A','7','P','8','j','X','E','f','Q','q','8','D','g','y','8','j','A','e','B','c','c','M','z','k','2','c','q','v','v','y',
    'Q','y','h','g','p','v','M','m','m','C','G','D','k','8','u','T','n','Q','H','G','H','f','b','J','j','5','X','c','i','7','7','q',
    'b','R','8','b','b','z','f','f','h','Y','Q','7','u','B','X','e','i','j','M','q','C','T','M','v','t','J','J','w','b','F','v','J',
    'm','e','2','u','e','8','L','V','G','q','A','j','m','7','m','g','m','5','i','r','p','p','U','y','F','6','f','b','u','6','q','L',
    'M','E','t','V','W','C','t','e','p','w','a','n','w','y','X','h','8','e','G','C','H','q','r','X','G','9','c','h','7','k','8','M',
    'G','b','a','m','Y','Q','w','8','J','z','a','F','r','4','W','M','j','P','q','a','z','U','y','u','3','b','Z','f','Y','5','7','g',
    'N','M','h','M','a','3','C','K','6','6','f','a','p','i','f','q','k','T','i','z','w','f','Z','c','H','L','X','g','6','m','g','r',
    'w','Y','u','K','8','L','p','8','P','R','A','R','A','b','Z','V','a','x','V','c','G','A','H','t','Y','6','P','T','L','W','N','z',
    'g','z','k','d','E','v','C','t','Z','M','Z','K','4','w','9','5','D','W','f','U','8','5','u','6','b','5','B','8','g','y','C','E',
    'Q','z','e','9','p','N','S','P','D','D','f','x','k','Z','4','R','v','X','V','k','p','b','n','t','c','F','R','e','x','9','C','D',
    'J','2','6','f','Z','D','w','J','R','j','j','9','b','w','N','N','p','R','f','Z','z','j','F','r','Q','e','F','x','f','t','V','V',
    'A','y','J','G','W','Z','H','r','D','5','M','u','H','V','L','N','U','V','X','z','j','9','r','v','e','d','R','c','u','V','x','r',
    'c','6','k','L','h','q','w','U','W','Q','g','G','F','C','t','E','a','D','h','x','9','5','P','R','Z','E','M','5','f','4','2','t',
    'A','6','f','r','X','G','X','Y','B','8','G','E','n','B','v','x','f','M','R','f','B','z','Y','3','2','q','z','G','t','P','C','6',
    '6','r','z','J','r','c','n','d','6','h','e','w','D','D','h','V','L','u','i','b','5','K','d','S','y','9','N','p','E','r','D','k',
    'B','z','u','v','d','Q','p','K','5','m','J','r','b','Y','Z','7','p','M','J','F','E','q','x','f','E','K','U','U','4','f','a','6',
    'g','5','a','q','D','U','8','F','y','R','a','P','5','5','x','z','6','V','T','P','D','m','y','7','U','5','C','A','7','Q','h','w',
    'r','6','x','g','Q','i','b','K','F','p','B','X','Q','h','i','E','r','C','z','v','x','W','Q','6','p','6','b','M','K','V','x','u',
    'k','d','R','S','k','Q','p','n','h','d','Q','Y','x','n','x','5','K','t','5','w','A','5','p','k','F','z','W','p','j','U','y','V',
    'x','G','m','y','L','A','X','H','G','A','a','J','5','E','P','q','E','U','7','p','6','A','9','n','d','G','D','g','i','h','t','W',
    'b','c','E','2','P','d','y','J','M','u','4','g','P','S','X','J','v','w','3','v','D','q','U','i','U','T','q','E','Y','5','2','t',
    'b','j','P','2','j','D','9','y','i','B','5','Y','3','X','L','w','m','V','X','z','X','r','Z','d','H','L','A','H','k','R','X','5',
    'i','L','m','q','3','p','a','G','P','j','g','h','R','P','Y','U','z','M','5','R','M','A','E','Q','V','c','w','r','4','M','S','k',
    'N','D','i','R','R','x','t','q','T','i','u','N','K','R','x','Z','K','a','g','G','y','9','c','j','J','S','9','3','H','T','f','F',
    'q','6','D','W','F','K','h','e','p','p','b','q','N','k','A','C','m','y','u','B','J','v','q','D','e','j','e','b','2','w','R','t',
    'J','N','j','F','T','A','8','L','m','X','i','T','g','j','c','V','4','V','h','2','h','R','p','2','9','k','c','c','G','D','h','z',
    't','i','h','t','W','R','n','Y','i','8','u','6','G','9','T','P','9','9','J','P','Y','R','h','X','K','z','h','L','W','r','C','U',
    '2','L','T','k','2','m','6','W','L','P','T','Z','z','t','i','H','5','G','w','t','E','v','z','k','b','H','b','b','W','W','u','b',
    'i','h','C','Q','n','H','N','u','5','u','K','X','r','M','W','U','3','Y','k','P','2','k','x','f','x','C','w','z','z','b','G','8',
    'y','W','e','j','v','2','v','r','t','q','z','p','Y','d','w','6','Z','D','J','L','9','F','z','G','U','4','a','8','H','6','U','a',
    'q','7','y','Q','J','v','m','D','P','S','j','q','v','t','n','t','g','j','3','t','8','f','K','K','7','b','W','d','F','i','N','K',
    'a','R','V','V','V','v','m','A','Q','2','y','j','c','t','f','k','j','7','X','y','j','b','U','F','w','W','3','9','6','A','S','J',
    'p','q','2','Z','7','L','p','b','7','b','5','i','p','r','r','h','P','M','h','j','c','y','e','u','h','B','d','9','9','u','f','d',
    'g','u','p','w','u','9','S','c','L','U','g','A','y','V','F','V','6','D','D','X','i','V','m','u','Y','P','J','v','L','T','A','F',
    'M','Q','H','Z','6','v','8','p','A','L','P','z','C','V','a','C','h','X','j','W','8','G','z','j','d','M','4','u','x','w','H','g',
    'V','q','K','z','b','g','2','3','D','N','y','G','X','F','T','v','T','L','y','v','L','9','g','c','C','R','8','L','A','7','Y','N',
    't','n','R','6','b','n','m','9','i','h','t','T','F','a','V','N','J','J','3','J','q','p','W','7','b','T','G','r','M','k','a','7',
    'D','H','v','y','T','A','C','U','P','u','q','L','R','Y','4','q','h','y','f','F','J','x','K','7','N','B','v','3','a','Z','M','t',
    'U','x','8','9','V','E','t','j','K','r','u','Y','Y','A','u','w','Y','2','y','Q','z','S','n','J','B','2','t','X','x','K','z','g',
    '6','d','n','i','7','Z','N','F','Q','6','w','N','r','b','k','d','W','X','S','t','c','U','m','6','4','2','e','w','6','x','Z','a',
    'Q','A','7','4','h','H','z','r','e','J','q','j','w','4','q','c','i','R','4','x','n','r','j','r','P','g','E','7','t','k','b','Z',
    'r','A','b','d','g','i','G','V','D','E','U','L','b','J','U','q','2','S','K','m','A','U','L','k','Q','4','N','p','k','G','C','6',
    'R','Z','B','y','B','B','j','y','x','L','d','h','L','G','6','x','H','z','T','5','d','Y','4','2','m','q','Q','y','H','6','c','N',
    'u','m','U','v','i','Y','Z','7','4','L','K','F','b','v','2','Y','h','x','8','a','R','w','q','x','E','a','T','y','m','C','2','Q',
    'U','T','D','Q','v','u','M','9','D','8','r','8','b','m','p','E','7','C','T','9','B','A','G','k','b','G','z','Z','G','L','N','k',
    'h','3','k','J','e','f','d','x','F','8','W','K','7','T','6','h','H','V','C','h','P','u','H','e','v','w','z','P','K','r','D','G',
    'X','Z','B','X','f','H','Q','4','e','D','y','W','Z','6','4','K','A','e','a','F','S','N','h','x','S','W','J','c','E','P','g','j',
    'a','w','T','m','Z','X','E','P','Y','R','M','2','R','2','X','N','F','X','Y','W','x','z','p','J','g','n','D','4','i','p','6','N',
    'r','9','G','k','E','h','T','h','U','h','x','B','Q','9','H','7','w','U','P','Q','d','G','6','q','p','j','j','v','C','a','X','J',
    'N','G','Y','w','f','H','C','x','F','k','z','3','9','r','h','8','7','5','V','i','V','C','R','q','x','N','2','2','i','W','F','U',
    '7','T','H','f','z','E','a','n','u','Q','t','U','Y','G','t','3','A','m','r','6','d','f','e','n','e','z','F','u','U','N','8','m',
    'h','p','R','N','S','H','6','6','V','M','S','t','q','P','E','i','u','y','g','8','L','Q','Y','Y','G','e','W','W','C','G','y','b',
    'y','t','u','P','R','P','5','m','N','K','B','Z','w','f','t','k','x','3','L','b','q','d','w','S','G','E','h','R','F','4','q','e',
    '5','6','F','2','n','q','T','R','y','f','n','Y','h','2','F','u','x','M','i','i','h','w','G','C','Z','v','i','C','a','X','U','C',
    'Y','8','d','h','R','x','V','n','v','G','i','D','a','U','p','U','a','e','b','F','w','P','d','X','n','K','h','9','H','r','b','g',
    '2','f','m','X','k','m','q','6','n','5','b','G','H','d','R','9','D','U','c','r','Z','Y','W','S','Z','x','p','t','x','y','4','k',
    'j','F','U','t','C','i','e','i','b','p','e','4','C','z','h','3','3','5','Q','P','n','G','i','A','8','c','Q','z','B','a','V','4',
    '2','B','2','z','u','u','3','i','L','w','y','g','K','H','k','y','2','B','b','e','5','e','4','e','U','4','z','n','P','z','a','c',
    'E','f','u','M','G','C','g','z','j','4','E','7','R','t','D','K','c','t','p','g','W','H','C','H','J','Q','J','c','F','5','4','W',
    'K','7','j','h','A','T','K','z','t','S','f','f','j','C','c','8','n','7','c','T','U','R','Q','E','7','A','W','Z','z','K','5','j',
    '2','H','k','a','j','g','g','W','w','4','T','A','9','J','U','e','S','N','P','K','d','k','L','Q','G','Z','e','W','i','H','u','j',
    'C','z','4','E','2','v','5','L','u','9','Z','a','9','A','b','C','M','G','X','B','C','2','Y','Z','e','U','n','E','5','Y','n','y',
    'F','h','H','p','9','j','Y','F','V','w','Y','r','8','Q','f','C','J','4','T','t','z','Q','N','M','e','7','4','3','y','E','M','m',
    'b','S','c','h','w','a','X','E','d','E','z','t','h','9','k','p','A','k','K','H','x','q','K','Z','B','u','a','9','3','U','U','u',
    '8','E','D','v','y','k','W','Y','X','k','r','R','D','X','n','Q','V','d','e','D','g','x','E','V','Y','w','k','m','K','r','H','D',
    't','2','6','N','U','g','3','t','B','9','t','u','M','D','z','Y','K','z','K','r','V','5','i','e','p','M','d','t','w','6','a','f',
    'f','W','k','L','i','g','M','V','M','Y','b','x','e','4','h','h','Y','g','w','Z','m','e','e','6','R','W','M','x','G','y','V','n',
    '6','e','g','A','g','K','a','N','7','p','a','u','E','4','6','M','t','X','h','g','b','j','p','5','x','x','B','P','3','J','M','7',
    'j','Z','P','y','e','Q','Z','e','t','j','3','t','F','V','x','m','b','b','B','y','J','L','L','9','3','R','a','5','j','S','V','t',
    'e','2','6','m','H','w','r','w','r','6','Q','3','x','z','m','A','d','x','t','E','H','c','Z','x','c','P','j','r','u','U','W','k',
    '6','g','X','g','n','f','n','7','H','M','B','t','v','6','v','x','g','M','f','e','2','w','m','y','d','H','S','q','c','K','U','H',
    '2','X','h','d','p','Q','7','J','X','i','X','f','a','z','V','A','F','2','8','z','v','h','C','h','e','4','g','z','w','z','h','q',
    'p','6','B','n','m','8','h','W','U','7','z','h','T','6','J','f','4','Z','n','Q','W','z','2','N','4','t','g','7','u','4','X','2',
    'C','F','L','n','J','n','m','j','3','P','3','Y','e','J','R','A','H','e','R','D','z','7','u','X','Y','y','D','w','J','m','G','U',
    'P','H','5','S','d','a','F','F','Y','c','M','f','3','3','L','v','V','B','U','C','A','d','N','H','Q','h','7','8','4','r','p','G',
    'v','M','D','H','7','e','E','r','i','K','Q','i','B','D','M','Z','p','c','R','G','u','c','H','a','N','k','E','f','9','R','7','x',
    '6','3','5','u','x','3','h','v','p','6','q','r','j','u','f','W','T','q','P','n','Y','L','B','6','U','w','P','2','T','W','R','g',
    '2','3','3','e','N','V','a','j','b','e','4','T','u','J','u','u','F','B','D','G','H','x','x','k','5','G','e','3','4','B','m','L',
    'S','b','i','t','T','p','M','D','Z','A','A','i','r','J','p','4','H','U','A','G','y','d','Q','5','U','R','F','8','q','a','S','H',
    'n','5','z','9','g','3','u','R','H','m','G','m','b','p','c','L','Z','Y','u','m','i','K','A','Q','R','T','X','G','t','b','8','7',
    '7','6','w','M','N','f','R','G','r','L','m','q','n','7','5','k','X','8','g','u','K','7','Y','w','K','q','U','e','W','A','r','i',
    'Z','a','p','q','L','5','P','u','n','t','y','G','x','C','N','X','q','P','r','U','v','A','r','r','q','e','f','c','z','M','7','N',
    '6','a','z','Z','a','t','f','p','4','v','J','Y','j','h','M','D','t','k','A','B','p','Q','A','y','x','X','7','p','S','8','m','M',
    'y','K','B','A','5','2','7','b','y','R','K','q','A','u','3','J'};

static const adler32_test tests[] = {
   {0x1, (const uint8_t *)0x0, 0, 0x1},
   {0x1, (const uint8_t *)"", 1, 0x10001},
   {0x1, (const uint8_t *)"a", 1, 0x620062},
   {0x1, (const uint8_t *)"abacus", 6, 0x8400270},
   {0x1, (const uint8_t *)"backlog", 7, 0xb1f02d4},
   {0x1, (const uint8_t *)"campfire", 8, 0xea10348},
   {0x1, (const uint8_t *)"delta", 5, 0x61a020b},
   {0x1, (const uint8_t *)"executable", 10, 0x16fa0423},
   {0x1, (const uint8_t *)"file", 4, 0x41401a1},
   {0x1, (const uint8_t *)"greatest", 8, 0xefa0360},
   {0x1, (const uint8_t *)"inverter", 8, 0xf6f0370},
   {0x1, (const uint8_t *)"jigsaw", 6, 0x8bd0286},
   {0x1, (const uint8_t *)"karate", 6, 0x8a50279},
   {0x1, (const uint8_t *)"landscape", 9, 0x126a03ac},
   {0x1, (const uint8_t *)"machine", 7, 0xb5302d6},
   {0x1, (const uint8_t *)"nanometer", 9, 0x12d803ca},
   {0x1, (const uint8_t *)"oblivion", 8, 0xf220363},
   {0x1, (const uint8_t *)"panama", 6, 0x8a1026f},
   {0x1, (const uint8_t *)"quest", 5, 0x6970233},
   {0x1, (const uint8_t *)"resource", 8, 0xf8d0369},
   {0x1, (const uint8_t *)"secret", 6, 0x8d10287},
   {0x1, (const uint8_t *)"ultimate", 8, 0xf8d0366},
   {0x1, (const uint8_t *)"vector", 6, 0x8fb0294},
   {0x1, (const uint8_t *)"walrus", 6, 0x918029f},
   {0x1, (const uint8_t *)"xeno", 4, 0x45e01bb},
   {0x1, (const uint8_t *)"yelling", 7, 0xbfe02f5},
   {0x1, (const uint8_t *)"zero", 4, 0x46e01c1},
   {0x1, (const uint8_t *)"4BJD7PocN1VqX0jXVpWB", 20, 0x3eef064d},
   {0x1, (const uint8_t *)"F1rPWI7XvDs6nAIRx41l", 20, 0x425d065f},
   {0x1, (const uint8_t *)"ldhKlsVkPFOveXgkGtC2", 20, 0x4f1a073e},
   {0x1, (const uint8_t *)"5KKnGOOrs8BvJ35iKTOS", 20, 0x42290650},
   {0x1, (const uint8_t *)"0l1tw7GOcem06Ddu7yn4", 20, 0x43fd0690},
   {0x1, (const uint8_t *)"MCr47CjPIn9R1IvE1Tm5", 20, 0x3f770609},
   {0x1, (const uint8_t *)"UcixbzPKTIv0SvILHVdO", 20, 0x4c7c0703},
   {0x1, (const uint8_t *)"dGnAyAhRQDsWw0ESou24", 20, 0x48ac06b7},
   {0x1, (const uint8_t *)"di0nvmY9UYMYDh0r45XT", 20, 0x489a0698},
   {0x1, (const uint8_t *)"2XKDwHfAhFsV0RhbqtvH", 20, 0x44a906e6},
   {0x1, (const uint8_t *)"ZhrANFIiIvRnqClIVyeD", 20, 0x4a29071c},
   {0x1, (const uint8_t *)"v7Q9ehzioTOVeDIZioT1", 20, 0x4a7706f9},
   {0x1, (const uint8_t *)"Yod5hEeKcYqyhfXbhxj2", 20, 0x4ce60769},
   {0x1, (const uint8_t *)"GehSWY2ay4uUKhehXYb0", 20, 0x48ae06e5},
   {0x1, (const uint8_t *)"kwytJmq6UqpflV8Y8GoE", 20, 0x51d60750},
   {0x1, (const uint8_t *)"70684206568419061514", 20, 0x2b100414},
   {0x1, (const uint8_t *)"42015093765128581010", 20, 0x2a550405},
   {0x1, (const uint8_t *)"88214814356148806939", 20, 0x2b450423},
   {0x1, (const uint8_t *)"43472694284527343838", 20, 0x2b460421},
   {0x1, (const uint8_t *)"49769333513942933689", 20, 0x2bc1042b},
   {0x1, (const uint8_t *)"54979784887993251199", 20, 0x2ccd043d},
   {0x1, (const uint8_t *)"58360544869206793220", 20, 0x2b68041a},
   {0x1, (const uint8_t *)"27347953487840714234", 20, 0x2b84041d},
   {0x1, (const uint8_t *)"07650690295365319082", 20, 0x2afa0417},
   {0x1, (const uint8_t *)"42655507906821911703", 20, 0x2aff0412},
   {0x1, (const uint8_t *)"29977409200786225655", 20, 0x2b8d0420},
   {0x1, (const uint8_t *)"85181542907229116674", 20, 0x2b140419},
   {0x1, (const uint8_t *)"87963594337989416799", 20, 0x2c8e043f},
   {0x1, (const uint8_t *)"21395988329504168551", 20, 0x2b68041f},
   {0x1, (const uint8_t *)"51991013580943379423", 20, 0x2af10417},
   {0x1, (const uint8_t *)"*]+@!);({_$;}[_},?{?;(_?,=-][@", 30, 0x7c9d0841},
   {0x1, (const uint8_t *)"_@:_).&(#.[:[{[:)$++-($_;@[)}+", 30, 0x71060751},
   {0x1, (const uint8_t *)"&[!,[$_==}+.]@!;*(+},[;:)$;)-@", 30, 0x7095070a},
   {0x1, (const uint8_t *)"]{.[.+?+[[=;[?}_#&;[=)__$$:+=_", 30, 0x82530815},
   {0x1, (const uint8_t *)"-%.)=/[@].:.(:,()$;=%@-$?]{%+%", 30, 0x61250661},
   {0x1, (const uint8_t *)"+]#$(@&.=:,*];/.!]%/{:){:@(;)$", 30, 0x642006a3},
   {0x1, (const uint8_t *)")-._.:?[&:.=+}(*$/=!.${;(=$@!}", 30, 0x674206cb},
   {0x1, (const uint8_t *)":(_*&%/[[}+,?#$&*+#[([*-/#;%(]", 30, 0x67670680},
   {0x1, (const uint8_t *)"{[#-;:$/{)(+[}#]/{&!%(@)%:@-$:", 30, 0x7547070f},
   {0x1, (const uint8_t *)"_{$*,}(&,@.)):=!/%(&(,,-?$}}}!", 30, 0x69ea06ee},
   {0x1, (const uint8_t *)"e$98KNzqaV)Y:2X?]77].{gKRD4G5{mHZk,Z)SpU%L3FSgv!Wb8MLAFdi{+fp)c,@8m6v)yXg@]HBDFk?.4&}g5_udE*JHCiH=aL", 100, 0x1b01e92},
   {0x1, (const uint8_t *)"r*Fd}ef+5RJQ;+W=4jTR9)R*p!B;]Ed7tkrLi;88U7g@3v!5pk2X6D)vt,.@N8c]@yyEcKi[vwUu@.Ppm@C6%Mv*3Nw}Y,58_aH)", 100, 0xfbdb1e96},
   {0x1, (const uint8_t *)"h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&", 100, 0x47a61ec8},
   {0x1, (const uint8_t *)long_string, 5552, 0x8b81718f},
   {0x7a30360d, (const uint8_t *)0x0, 0, 0x1},
   {0x6fd767ee, (const uint8_t *)"", 1, 0xd7c567ee},
   {0xefeb7589, (const uint8_t *)"a", 1, 0x65e475ea},
   {0x61cf7e6b, (const uint8_t *)"abacus", 6, 0x60b880da},
   {0xdc712e2,  (const uint8_t *)"backlog", 7, 0x9d0d15b5},
   {0xad23c7fd, (const uint8_t *)"campfire", 8, 0xfbfecb44},
   {0x85cb2317, (const uint8_t *)"delta", 5, 0x3b622521},
   {0x9eed31b0, (const uint8_t *)"executable", 10, 0xa6db35d2},
   {0xb94f34ca, (const uint8_t *)"file", 4, 0x9096366a},
   {0xab058a2,  (const uint8_t *)"greatest", 8, 0xded05c01},
   {0x5bff2b7a, (const uint8_t *)"inverter", 8, 0xc7452ee9},
   {0x605c9a5f, (const uint8_t *)"jigsaw", 6, 0x7899ce4},
   {0x51bdeea5, (const uint8_t *)"karate", 6, 0xf285f11d},
   {0x85c21c79, (const uint8_t *)"landscape", 9, 0x98732024},
   {0x97216f56, (const uint8_t *)"machine", 7, 0xadf4722b},
   {0x18444af2, (const uint8_t *)"nanometer", 9, 0xcdb34ebb},
   {0xbe6ce359, (const uint8_t *)"oblivion", 8, 0xe8b7e6bb},
   {0x843071f1, (const uint8_t *)"panama", 6, 0x389e745f},
   {0xf2480c60, (const uint8_t *)"quest", 5, 0x36c90e92},
   {0x2d2feb3d, (const uint8_t *)"resource", 8, 0x9705eea5},
   {0x7490310a, (const uint8_t *)"secret", 6, 0xa3a63390},
   {0x97d247d4, (const uint8_t *)"ultimate", 8, 0xe6154b39},
   {0x93cf7599, (const uint8_t *)"vector", 6, 0x5e87782c},
   {0x73c84278, (const uint8_t *)"walrus", 6, 0xbc84516},
   {0x228a87d1, (const uint8_t *)"xeno", 4, 0x4646898b},
   {0xa7a048d0, (const uint8_t *)"yelling", 7, 0xb1654bc4},
   {0x1f0ded40, (const uint8_t *)"zero", 4, 0xd8a4ef00},
   {0xa804a62f, (const uint8_t *)"4BJD7PocN1VqX0jXVpWB", 20, 0xe34eac7b},
   {0x508fae6a, (const uint8_t *)"F1rPWI7XvDs6nAIRx41l", 20, 0x33f2b4c8},
   {0xe5adaf4f, (const uint8_t *)"ldhKlsVkPFOveXgkGtC2", 20, 0xe7b1b68c},
   {0x67136a40, (const uint8_t *)"5KKnGOOrs8BvJ35iKTOS", 20, 0xf6a0708f},
   {0xb00c4a10, (const uint8_t *)"0l1tw7GOcem06Ddu7yn4", 20, 0xbd8f509f},
   {0x2e0c84b5, (const uint8_t *)"MCr47CjPIn9R1IvE1Tm5", 20, 0xcc298abd},
   {0x81238d44, (const uint8_t *)"UcixbzPKTIv0SvILHVdO", 20, 0xd7809446},
   {0xf853aa92, (const uint8_t *)"dGnAyAhRQDsWw0ESou24", 20, 0x9525b148},
   {0x5a692325, (const uint8_t *)"di0nvmY9UYMYDh0r45XT", 20, 0x620029bc},
   {0x3275b9f,  (const uint8_t *)"2XKDwHfAhFsV0RhbqtvH", 20, 0x70916284},
   {0x38371feb, (const uint8_t *)"ZhrANFIiIvRnqClIVyeD", 20, 0xd52706},
   {0xafc8bf62, (const uint8_t *)"v7Q9ehzioTOVeDIZioT1", 20, 0xeeb4c65a},
   {0x9b07db73, (const uint8_t *)"Yod5hEeKcYqyhfXbhxj2", 20, 0xde3e2db},
   {0xe75b214,  (const uint8_t *)"GehSWY2ay4uUKhehXYb0", 20, 0x4171b8f8},
   {0x72d0fe6f, (const uint8_t *)"kwytJmq6UqpflV8Y8GoE", 20, 0xa66a05cd},
   {0xf857a4b1, (const uint8_t *)"70684206568419061514", 20, 0x1f9a8c4},
   {0x54b8e14,  (const uint8_t *)"42015093765128581010", 20, 0x49c19218},
   {0xd6aa5616, (const uint8_t *)"88214814356148806939", 20, 0xbbfc5a38},
   {0x11e63098, (const uint8_t *)"43472694284527343838", 20, 0x93434b8},
   {0xbe92385,  (const uint8_t *)"49769333513942933689", 20, 0xfe1827af},
   {0x49511de0, (const uint8_t *)"54979784887993251199", 20, 0xcba8221c},
   {0x3db13bc1, (const uint8_t *)"58360544869206793220", 20, 0x14643fda},
   {0xbb899bea, (const uint8_t *)"27347953487840714234", 20, 0x1604a006},
   {0xf6cd9436, (const uint8_t *)"07650690295365319082", 20, 0xb69f984c},
   {0x9109e6c3, (const uint8_t *)"42655507906821911703", 20, 0xc43eead4},
   {0x75770fc,  (const uint8_t *)"29977409200786225655", 20, 0x707751b},
   {0x69b1d19b, (const uint8_t *)"85181542907229116674", 20, 0xf5bdd5b3},
   {0xc6132975, (const uint8_t *)"87963594337989416799", 20, 0x2fed2db3},
   {0xd58cb00c, (const uint8_t *)"21395988329504168551", 20, 0xc2a2b42a},
   {0xb63b8caa, (const uint8_t *)"51991013580943379423", 20, 0xdf0590c0},
   {0x8a45a2b8, (const uint8_t *)"*]+@!);({_$;}[_},?{?;(_?,=-][@", 30, 0x1980aaf8},
   {0xcbe95b78, (const uint8_t *)"_@:_).&(#.[:[{[:)$++-($_;@[)}+", 30, 0xf58662c8},
   {0x4ef8a54b, (const uint8_t *)"&[!,[$_==}+.]@!;*(+},[;:)$;)-@", 30, 0x1f65ac54},
   {0x76ad267a, (const uint8_t *)"]{.[.+?+[[=;[?}_#&;[=)__$$:+=_", 30, 0x7b792e8e},
   {0x569e613c, (const uint8_t *)"-%.)=/[@].:.(:,()$;=%@-$?]{%+%", 30, 0x1d61679c},
   {0x36aa61da, (const uint8_t *)"+]#$(@&.=:,*];/.!]%/{:){:@(;)$", 30, 0x12ec687c},
   {0xf67222df, (const uint8_t *)")-._.:?[&:.=+}(*$/=!.${;(=$@!}", 30, 0x740329a9},
   {0x74b34fd3, (const uint8_t *)":(_*&%/[[}+,?#$&*+#[([*-/#;%(]", 30, 0x374c5652},
   {0x351fd770, (const uint8_t *)"{[#-;:$/{)(+[}#]/{&!%(@)%:@-$:", 30, 0xeadfde7e},
   {0xc45aef77, (const uint8_t *)"_{$*,}(&,@.)):=!/%(&(,,-?$}}}!", 30, 0x3fcbf664},
   {0xd034ea71, (const uint8_t *)"e$98KNzqaV)Y:2X?]77].{gKRD4G5{mHZk,Z)SpU%L3FSgv!Wb8MLAFdi{+fp)c,@8m6v)yXg@]HBDFk?.4&}g5_udE*JHCiH=aL", 100, 0x6b080911},
   {0xdeadc0de, (const uint8_t *)"r*Fd}ef+5RJQ;+W=4jTR9)R*p!B;]Ed7tkrLi;88U7g@3v!5pk2X6D)vt,.@N8c]@yyEcKi[vwUu@.Ppm@C6%Mv*3Nw}Y,58_aH)", 100, 0x355fdf73},
   {0xba5eba11, (const uint8_t *)"h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&", 100, 0xb48bd8d8},
   {0x7712aa45, (const uint8_t *)long_string, 5552, 0x7dc51be2},
};

class adler32_variant : public ::testing::TestWithParam<adler32_test> {
public:
    void hash(adler32_test param, adler32_func adler32) {
        uint32_t adler = adler32((uint32_t)param.adler, param.buf, param.len);
        EXPECT_EQ(adler, param.expect);
    }
};

INSTANTIATE_TEST_SUITE_P(adler32, adler32_variant, testing::ValuesIn(tests));

#define TEST_ADLER32(name, func, support_flag) \
    TEST_P(adler32_variant, name) { \
        if (!support_flag) { \
            GTEST_SKIP(); \
            return; \
        } \
        hash(GetParam(), func); \
    }

TEST_ADLER32(c, adler32_c, 1)

#ifdef ARM_NEON
TEST_ADLER32(neon, adler32_neon, test_cpu_features.arm.has_neon)
#elif defined(POWER8_VSX)
TEST_ADLER32(power8, adler32_power8, test_cpu_features.power.has_arch_2_07)
#elif defined(PPC_VMX)
TEST_ADLER32(vmx, adler32_vmx, test_cpu_features.power.has_altivec)
#elif defined(RISCV_RVV)
TEST_ADLER32(rvv, adler32_rvv, test_cpu_features.riscv.has_rvv)
#endif

#ifdef X86_SSSE3
TEST_ADLER32(ssse3, adler32_ssse3, test_cpu_features.x86.has_ssse3)
#endif
#ifdef X86_AVX2
TEST_ADLER32(avx2, adler32_avx2, test_cpu_features.x86.has_avx2)
#endif
#ifdef X86_AVX512
TEST_ADLER32(avx512, adler32_avx512, test_cpu_features.x86.has_avx512_common)
#endif
#ifdef X86_AVX512VNNI
TEST_ADLER32(avx512_vnni, adler32_avx512_vnni, test_cpu_features.x86.has_avx512vnni)
#endif
