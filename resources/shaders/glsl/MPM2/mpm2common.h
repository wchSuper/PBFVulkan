#ifndef MPM2COMMON_H
#define MPM2COMMON_H

struct Cell {
    int vx;
    int vy;
    int vz;
    int mass;
};

struct Particle {
    vec3 Location;
    vec3 Velocity;
    vec3 DeltaLocation;
    float Lambda;
    float Density;
    float Mass;

    vec3 TmpVelocity;

    uint CellHash;
    uint TmpCellHash;

    uint NumNgbrs;
};

struct Affine {
    vec3 c0;
    vec3 c1;
    vec3 c2;
};

layout(set = 0, binding = 0) uniform SimParams {
    vec3 real_box_size;
    vec3 init_box_size;
    vec3 world_min;
    vec3 world_size;
    uint cellCount;
    uint particleCount;
    float gravity;
    float fixed_point_multiplier;
    float dt;

    float stiffness;
    float rest_density;
    float viscosity;
};

layout(set = 0, binding = 1) buffer CellBuffer {
    Cell cells[];
};

layout(set = 0, binding = 2) buffer ParticleBuffer {
    Particle particles[];
};

layout(set = 0, binding = 3) buffer AffineBuffer {
    Affine Cs[];
};

int encodeFixedPoint(float floating_point) {
    return int(floating_point * fixed_point_multiplier);
}

float decodeFixedPoint(int fixed_point) {
    return float(fixed_point) / fixed_point_multiplier;
}

#endif
