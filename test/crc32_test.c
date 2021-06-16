/* crc32_test.c -- crc32 unit test
 * Copyright (C) 2019-2021 IBM Corporation
 * Authors: Rogerio Alves    <rogealve@br.ibm.com>
 *          Matheus Castanho <msc@linux.ibm.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

typedef struct {
    uint32_t line;
    unsigned long crc;
    const unsigned char *buf;
    size_t len;
    unsigned long expect;
} crc32_test;

void test_crc32(unsigned long crc, const unsigned char *buf, size_t len, uint32_t chk, uint32_t line) {
    uint32_t res = PREFIX(crc32_z)((uint32_t)crc, buf, len);
    if (res != chk) {
        fprintf(stderr, "FAIL [%d]: crc32 returned 0x%08X expected 0x%08X\n",
                line, res, chk);
        exit(1);
    }
}

static const crc32_test tests[] = {
  {__LINE__, 0x0, (const unsigned char *)0x0, 0, 0x0},
  {__LINE__, 0xffffffff, (const unsigned char *)0x0, 0, 0x0},
  {__LINE__, 0x0, (const unsigned char *)0x0, 255, 0x0}, /*  BZ 174799.  */
  {__LINE__, 0x0, (const unsigned char *)0x0, 256, 0x0},
  {__LINE__, 0x0, (const unsigned char *)0x0, 257, 0x0},
  {__LINE__, 0x0, (const unsigned char *)0x0, 32767, 0x0},
  {__LINE__, 0x0, (const unsigned char *)0x0, 32768, 0x0},
  {__LINE__, 0x0, (const unsigned char *)0x0, 32769, 0x0},
  {__LINE__, 0x0, (const unsigned char *)"", 0, 0x0},
  {__LINE__, 0xffffffff, (const unsigned char *)"", 0, 0xffffffff},
  {__LINE__, 0x0, (const unsigned char *)"abacus", 6, 0xc3d7115b},
  {__LINE__, 0x0, (const unsigned char *)"backlog", 7, 0x269205},
  {__LINE__, 0x0, (const unsigned char *)"campfire", 8, 0x22a515f8},
  {__LINE__, 0x0, (const unsigned char *)"delta", 5, 0x9643fed9},
  {__LINE__, 0x0, (const unsigned char *)"executable", 10, 0xd68eda01},
  {__LINE__, 0x0, (const unsigned char *)"file", 4, 0x8c9f3610},
  {__LINE__, 0x0, (const unsigned char *)"greatest", 8, 0xc1abd6cd},
  {__LINE__, 0x0, (const unsigned char *)"hello", 5, 0x3610a686},
  {__LINE__, 0x0, (const unsigned char *)"inverter", 8, 0xc9e962c9},
  {__LINE__, 0x0, (const unsigned char *)"jigsaw", 6, 0xce4e3f69},
  {__LINE__, 0x0, (const unsigned char *)"karate", 6, 0x890be0e2},
  {__LINE__, 0x0, (const unsigned char *)"landscape", 9, 0xc4e0330b},
  {__LINE__, 0x0, (const unsigned char *)"machine", 7, 0x1505df84},
  {__LINE__, 0x0, (const unsigned char *)"nanometer", 9, 0xd4e19f39},
  {__LINE__, 0x0, (const unsigned char *)"oblivion", 8, 0xdae9de77},
  {__LINE__, 0x0, (const unsigned char *)"panama", 6, 0x66b8979c},
  {__LINE__, 0x0, (const unsigned char *)"quest", 5, 0x4317f817},
  {__LINE__, 0x0, (const unsigned char *)"resource", 8, 0xbc91f416},
  {__LINE__, 0x0, (const unsigned char *)"secret", 6, 0x5ca2e8e5},
  {__LINE__, 0x0, (const unsigned char *)"test", 4, 0xd87f7e0c},
  {__LINE__, 0x0, (const unsigned char *)"ultimate", 8, 0x3fc79b0b},
  {__LINE__, 0x0, (const unsigned char *)"vector", 6, 0x1b6e485b},
  {__LINE__, 0x0, (const unsigned char *)"walrus", 6, 0xbe769b97},
  {__LINE__, 0x0, (const unsigned char *)"xeno", 4, 0xe7a06444},
  {__LINE__, 0x0, (const unsigned char *)"yelling", 7, 0xfe3944e5},
  {__LINE__, 0x0, (const unsigned char *)"zlib", 4, 0x73887d3a},
  {__LINE__, 0x0, (const unsigned char *)"4BJD7PocN1VqX0jXVpWB", 20, 0xd487a5a1},
  {__LINE__, 0x0, (const unsigned char *)"F1rPWI7XvDs6nAIRx41l", 20, 0x61a0132e},
  {__LINE__, 0x0, (const unsigned char *)"ldhKlsVkPFOveXgkGtC2", 20, 0xdf02f76},
  {__LINE__, 0x0, (const unsigned char *)"5KKnGOOrs8BvJ35iKTOS", 20, 0x579b2b0a},
  {__LINE__, 0x0, (const unsigned char *)"0l1tw7GOcem06Ddu7yn4", 20, 0xf7d16e2d},
  {__LINE__, 0x0, (const unsigned char *)"MCr47CjPIn9R1IvE1Tm5", 20, 0x731788f5},
  {__LINE__, 0x0, (const unsigned char *)"UcixbzPKTIv0SvILHVdO", 20, 0x7112bb11},
  {__LINE__, 0x0, (const unsigned char *)"dGnAyAhRQDsWw0ESou24", 20, 0xf32a0dac},
  {__LINE__, 0x0, (const unsigned char *)"di0nvmY9UYMYDh0r45XT", 20, 0x625437bb},
  {__LINE__, 0x0, (const unsigned char *)"2XKDwHfAhFsV0RhbqtvH", 20, 0x896930f9},
  {__LINE__, 0x0, (const unsigned char *)"ZhrANFIiIvRnqClIVyeD", 20, 0x8579a37},
  {__LINE__, 0x0, (const unsigned char *)"v7Q9ehzioTOVeDIZioT1", 20, 0x632aa8e0},
  {__LINE__, 0x0, (const unsigned char *)"Yod5hEeKcYqyhfXbhxj2", 20, 0xc829af29},
  {__LINE__, 0x0, (const unsigned char *)"GehSWY2ay4uUKhehXYb0", 20, 0x1b08b7e8},
  {__LINE__, 0x0, (const unsigned char *)"kwytJmq6UqpflV8Y8GoE", 20, 0x4e33b192},
  {__LINE__, 0x0, (const unsigned char *)"70684206568419061514", 20, 0x59a179f0},
  {__LINE__, 0x0, (const unsigned char *)"42015093765128581010", 20, 0xcd1013d7},
  {__LINE__, 0x0, (const unsigned char *)"88214814356148806939", 20, 0xab927546},
  {__LINE__, 0x0, (const unsigned char *)"43472694284527343838", 20, 0x11f3b20c},
  {__LINE__, 0x0, (const unsigned char *)"49769333513942933689", 20, 0xd562d4ca},
  {__LINE__, 0x0, (const unsigned char *)"54979784887993251199", 20, 0x233395f7},
  {__LINE__, 0x0, (const unsigned char *)"58360544869206793220", 20, 0x2d167fd5},
  {__LINE__, 0x0, (const unsigned char *)"27347953487840714234", 20, 0x8b5108ba},
  {__LINE__, 0x0, (const unsigned char *)"07650690295365319082", 20, 0xc46b3cd8},
  {__LINE__, 0x0, (const unsigned char *)"42655507906821911703", 20, 0xc10b2662},
  {__LINE__, 0x0, (const unsigned char *)"29977409200786225655", 20, 0xc9a0f9d2},
  {__LINE__, 0x0, (const unsigned char *)"85181542907229116674", 20, 0x9341357b},
  {__LINE__, 0x0, (const unsigned char *)"87963594337989416799", 20, 0xf0424937},
  {__LINE__, 0x0, (const unsigned char *)"21395988329504168551", 20, 0xd7c4c31f},
  {__LINE__, 0x0, (const unsigned char *)"51991013580943379423", 20, 0xf11edcc4},
  {__LINE__, 0x0, (const unsigned char *)"*]+@!);({_$;}[_},?{?;(_?,=-][@", 30, 0x40795df4},
  {__LINE__, 0x0, (const unsigned char *)"_@:_).&(#.[:[{[:)$++-($_;@[)}+", 30, 0xdd61a631},
  {__LINE__, 0x0, (const unsigned char *)"&[!,[$_==}+.]@!;*(+},[;:)$;)-@", 30, 0xca907a99},
  {__LINE__, 0x0, (const unsigned char *)"]{.[.+?+[[=;[?}_#&;[=)__$$:+=_", 30, 0xf652deac},
  {__LINE__, 0x0, (const unsigned char *)"-%.)=/[@].:.(:,()$;=%@-$?]{%+%", 30, 0xaf39a5a9},
  {__LINE__, 0x0, (const unsigned char *)"+]#$(@&.=:,*];/.!]%/{:){:@(;)$", 30, 0x6bebb4cf},
  {__LINE__, 0x0, (const unsigned char *)")-._.:?[&:.=+}(*$/=!.${;(=$@!}", 30, 0x76430bac},
  {__LINE__, 0x0, (const unsigned char *)":(_*&%/[[}+,?#$&*+#[([*-/#;%(]", 30, 0x6c80c388},
  {__LINE__, 0x0, (const unsigned char *)"{[#-;:$/{)(+[}#]/{&!%(@)%:@-$:", 30, 0xd54d977d},
  {__LINE__, 0x0, (const unsigned char *)"_{$*,}(&,@.)):=!/%(&(,,-?$}}}!", 30, 0xe3966ad5},
  {__LINE__, 0x0, (const unsigned char *)"e$98KNzqaV)Y:2X?]77].{gKRD4G5{mHZk,Z)SpU%L3FSgv!Wb8MLAFdi{+fp)c,@8m6v)yXg@]HBDFk?.4&}g5_udE*JHCiH=aL", 100, 0xe7c71db9},
  {__LINE__, 0x0, (const unsigned char *)"r*Fd}ef+5RJQ;+W=4jTR9)R*p!B;]Ed7tkrLi;88U7g@3v!5pk2X6D)vt,.@N8c]@yyEcKi[vwUu@.Ppm@C6%Mv*3Nw}Y,58_aH)", 100, 0xeaa52777},
  {__LINE__, 0x0, (const unsigned char *)"h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&", 100, 0xcd472048},
  {__LINE__, 0x7a30360d, (const unsigned char *)"abacus", 6, 0xf8655a84},
  {__LINE__, 0x6fd767ee, (const unsigned char *)"backlog", 7, 0x1ed834b1},
  {__LINE__, 0xefeb7589, (const unsigned char *)"campfire", 8, 0x686cfca},
  {__LINE__, 0x61cf7e6b, (const unsigned char *)"delta", 5, 0x1554e4b1},
  {__LINE__, 0xdc712e2,  (const unsigned char *)"executable", 10, 0x761b4254},
  {__LINE__, 0xad23c7fd, (const unsigned char *)"file", 4, 0x7abdd09b},
  {__LINE__, 0x85cb2317, (const unsigned char *)"greatest", 8, 0x4ba91c6b},
  {__LINE__, 0x9eed31b0, (const unsigned char *)"inverter", 8, 0xd5e78ba5},
  {__LINE__, 0xb94f34ca, (const unsigned char *)"jigsaw", 6, 0x23649109},
  {__LINE__, 0xab058a2,  (const unsigned char *)"karate", 6, 0xc5591f41},
  {__LINE__, 0x5bff2b7a, (const unsigned char *)"landscape", 9, 0xf10eb644},
  {__LINE__, 0x605c9a5f, (const unsigned char *)"machine", 7, 0xbaa0a636},
  {__LINE__, 0x51bdeea5, (const unsigned char *)"nanometer", 9, 0x6af89afb},
  {__LINE__, 0x85c21c79, (const unsigned char *)"oblivion", 8, 0xecae222b},
  {__LINE__, 0x97216f56, (const unsigned char *)"panama", 6, 0x47dffac4},
  {__LINE__, 0x18444af2, (const unsigned char *)"quest", 5, 0x70c2fe36},
  {__LINE__, 0xbe6ce359, (const unsigned char *)"resource", 8, 0x1471d925},
  {__LINE__, 0x843071f1, (const unsigned char *)"secret", 6, 0x50c9a0db},
  {__LINE__, 0xf2480c60, (const unsigned char *)"ultimate", 8, 0xf973daf8},
  {__LINE__, 0x2d2feb3d, (const unsigned char *)"vector", 6, 0x344ac03d},
  {__LINE__, 0x7490310a, (const unsigned char *)"walrus", 6, 0x6d1408ef},
  {__LINE__, 0x97d247d4, (const unsigned char *)"xeno", 4, 0xe62670b5},
  {__LINE__, 0x93cf7599, (const unsigned char *)"yelling", 7, 0x1b36da38},
  {__LINE__, 0x73c84278, (const unsigned char *)"zlib", 4, 0x6432d127},
  {__LINE__, 0x228a87d1, (const unsigned char *)"4BJD7PocN1VqX0jXVpWB", 20, 0x997107d0},
  {__LINE__, 0xa7a048d0, (const unsigned char *)"F1rPWI7XvDs6nAIRx41l", 20, 0xdc567274},
  {__LINE__, 0x1f0ded40, (const unsigned char *)"ldhKlsVkPFOveXgkGtC2", 20, 0xdcc63870},
  {__LINE__, 0xa804a62f, (const unsigned char *)"5KKnGOOrs8BvJ35iKTOS", 20, 0x6926cffd},
  {__LINE__, 0x508fae6a, (const unsigned char *)"0l1tw7GOcem06Ddu7yn4", 20, 0xb52b38bc},
  {__LINE__, 0xe5adaf4f, (const unsigned char *)"MCr47CjPIn9R1IvE1Tm5", 20, 0xf83b8178},
  {__LINE__, 0x67136a40, (const unsigned char *)"UcixbzPKTIv0SvILHVdO", 20, 0xc5213070},
  {__LINE__, 0xb00c4a10, (const unsigned char *)"dGnAyAhRQDsWw0ESou24", 20, 0xbc7648b0},
  {__LINE__, 0x2e0c84b5, (const unsigned char *)"di0nvmY9UYMYDh0r45XT", 20, 0xd8123a72},
  {__LINE__, 0x81238d44, (const unsigned char *)"2XKDwHfAhFsV0RhbqtvH", 20, 0xd5ac5620},
  {__LINE__, 0xf853aa92, (const unsigned char *)"ZhrANFIiIvRnqClIVyeD", 20, 0xceae099d},
  {__LINE__, 0x5a692325, (const unsigned char *)"v7Q9ehzioTOVeDIZioT1", 20, 0xb07d2b24},
  {__LINE__, 0x3275b9f,  (const unsigned char *)"Yod5hEeKcYqyhfXbhxj2", 20, 0x24ce91df},
  {__LINE__, 0x38371feb, (const unsigned char *)"GehSWY2ay4uUKhehXYb0", 20, 0x707b3b30},
  {__LINE__, 0xafc8bf62, (const unsigned char *)"kwytJmq6UqpflV8Y8GoE", 20, 0x16abc6a9},
  {__LINE__, 0x9b07db73, (const unsigned char *)"70684206568419061514", 20, 0xae1fb7b7},
  {__LINE__, 0xe75b214,  (const unsigned char *)"42015093765128581010", 20, 0xd4eecd2d},
  {__LINE__, 0x72d0fe6f, (const unsigned char *)"88214814356148806939", 20, 0x4660ec7},
  {__LINE__, 0xf857a4b1, (const unsigned char *)"43472694284527343838", 20, 0xfd8afdf7},
  {__LINE__, 0x54b8e14,  (const unsigned char *)"49769333513942933689", 20, 0xc6d1b5f2},
  {__LINE__, 0xd6aa5616, (const unsigned char *)"54979784887993251199", 20, 0x32476461},
  {__LINE__, 0x11e63098, (const unsigned char *)"58360544869206793220", 20, 0xd917cf1a},
  {__LINE__, 0xbe92385,  (const unsigned char *)"27347953487840714234", 20, 0x4ad14a12},
  {__LINE__, 0x49511de0, (const unsigned char *)"07650690295365319082", 20, 0xe37b5c6c},
  {__LINE__, 0x3db13bc1, (const unsigned char *)"42655507906821911703", 20, 0x7cc497f1},
  {__LINE__, 0xbb899bea, (const unsigned char *)"29977409200786225655", 20, 0x99781bb2},
  {__LINE__, 0xf6cd9436, (const unsigned char *)"85181542907229116674", 20, 0x132256a1},
  {__LINE__, 0x9109e6c3, (const unsigned char *)"87963594337989416799", 20, 0xbfdb2c83},
  {__LINE__, 0x75770fc,  (const unsigned char *)"21395988329504168551", 20, 0x8d9d1e81},
  {__LINE__, 0x69b1d19b, (const unsigned char *)"51991013580943379423", 20, 0x7b6d4404},
  {__LINE__, 0xc6132975, (const unsigned char *)"*]+@!);({_$;}[_},?{?;(_?,=-][@", 30, 0x8619f010},
  {__LINE__, 0xd58cb00c, (const unsigned char *)"_@:_).&(#.[:[{[:)$++-($_;@[)}+", 30, 0x15746ac3},
  {__LINE__, 0xb63b8caa, (const unsigned char *)"&[!,[$_==}+.]@!;*(+},[;:)$;)-@", 30, 0xaccf812f},
  {__LINE__, 0x8a45a2b8, (const unsigned char *)"]{.[.+?+[[=;[?}_#&;[=)__$$:+=_", 30, 0x78af45de},
  {__LINE__, 0xcbe95b78, (const unsigned char *)"-%.)=/[@].:.(:,()$;=%@-$?]{%+%", 30, 0x25b06b59},
  {__LINE__, 0x4ef8a54b, (const unsigned char *)"+]#$(@&.=:,*];/.!]%/{:){:@(;)$", 30, 0x4ba0d08f},
  {__LINE__, 0x76ad267a, (const unsigned char *)")-._.:?[&:.=+}(*$/=!.${;(=$@!}", 30, 0xe26b6aac},
  {__LINE__, 0x569e613c, (const unsigned char *)":(_*&%/[[}+,?#$&*+#[([*-/#;%(]", 30, 0x7e2b0a66},
  {__LINE__, 0x36aa61da, (const unsigned char *)"{[#-;:$/{)(+[}#]/{&!%(@)%:@-$:", 30, 0xb3430dc7},
  {__LINE__, 0xf67222df, (const unsigned char *)"_{$*,}(&,@.)):=!/%(&(,,-?$}}}!", 30, 0x626c17a},
  {__LINE__, 0x74b34fd3, (const unsigned char *)"e$98KNzqaV)Y:2X?]77].{gKRD4G5{mHZk,Z)SpU%L3FSgv!Wb8MLAFdi{+fp)c,@8m6v)yXg@]HBDFk?.4&}g5_udE*JHCiH=aL", 100, 0xccf98060},
  {__LINE__, 0x351fd770, (const unsigned char *)"r*Fd}ef+5RJQ;+W=4jTR9)R*p!B;]Ed7tkrLi;88U7g@3v!5pk2X6D)vt,.@N8c]@yyEcKi[vwUu@.Ppm@C6%Mv*3Nw}Y,58_aH)", 100, 0xd8b95312},
  {__LINE__, 0xc45aef77, (const unsigned char *)"h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&", 100, 0xbb1c9912},
  {__LINE__, 0xc45aef77, (const unsigned char *)"h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&"
                         "h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&"
                         "h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&"
                         "h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&"
                         "h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&"
                         "h{bcmdC+a;t+Cf{6Y_dFq-{X4Yu&7uNfVDh?q&_u.UWJU],-GiH7ADzb7-V.Q%4=+v!$L9W+T=bP]$_:]Vyg}A.ygD.r;h-D]m%&", 600, 0x888AFA5B}
};

static const int test_size = sizeof(tests) / sizeof(tests[0]);

int main(void) {
    int i;
    for (i = 0; i < test_size; i++) {
        test_crc32(tests[i].crc, tests[i].buf, tests[i].len, tests[i].expect, tests[i].line);
    }
    return 0;
}
