#ifndef S390_FEATURES_H_
#define S390_FEATURES_H_

extern int PREFIX(s390_cpu_has_vx);

void Z_INTERNAL PREFIX(s390_check_features)(void);

#endif
