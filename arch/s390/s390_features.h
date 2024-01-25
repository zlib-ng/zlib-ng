#ifndef S390_FEATURES_H_
#define S390_FEATURES_H_

struct s390_cpu_features {
    int has_vx;
};

void Z_INTERNAL s390_check_features(struct s390_cpu_features *features);

#ifdef CPU_FEATURES_H_

#ifdef S390_CRC32_VX
extern uint32_t crc32_s390_vx(uint32_t crc, const uint8_t *buf, size_t len);
#endif

#endif

#endif
