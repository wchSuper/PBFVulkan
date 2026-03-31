#ifndef MPM2_MG_COMMON_H
#define MPM2_MG_COMMON_H

layout(push_constant) uniform MGPush {
    int n;
    int levelOffset;
    int coarseOffset;
    int src;
} mg;

#endif
