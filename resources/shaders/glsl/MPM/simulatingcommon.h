struct Particle{
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

struct Cell {
    vec3 vel;        
    float mass;      
};

struct Fs {
    vec3 fs1;       
    vec3 fs2;
    vec3 fs3;
};

struct ExtraInfo {
    float mass;
    float volume_0;
    vec3 C1;
    vec3 C2;
    vec3 C3;
};

layout(binding = 0) buffer readonly PosIn  { Particle particlesIn[]; }; 
layout(binding = 1) buffer          PosOut { Particle particlesOut[]; };        
layout(binding = 2) buffer          cells  { Cell grid[]; };                      
layout(binding = 3) buffer readonly deformationGradientIn  { Fs FsIn[]; };       
layout(binding = 4) buffer          deformationGradientOut { Fs FsOut[]; };     
layout(binding = 5) buffer readonly extraInfoInBuffer  { ExtraInfo extraInfoIn[]; };
layout(binding = 6) buffer          extraInfoOutBuffer { ExtraInfo extraInfoOut[]; };

layout(binding = 7) uniform UBO {
    float deltaT;           
    float particleCount;    
    float elastic_lambda;  
    float elastic_mu;       
    vec3 world_min;
    vec3 world_size;
};

const int GRID_RESOLUTION = 32;
const float GRID_SIZE = 1.0;
const float GRAVITY = 9.8;
const float INV_GRID_RESOLUTION = 1.0 / GRID_RESOLUTION;
const float DX = GRID_SIZE * INV_GRID_RESOLUTION;
const float INV_DX = 1.0 / DX;

const float Ef = 100.0;

vec3 world_to_mpm(vec3 x_world) {
    return (x_world - world_min) / world_size * GRID_SIZE;
}

vec3 mpm_to_world(vec3 x_mpm) {
    return world_min + x_mpm / GRID_SIZE * world_size;
}

vec3 world_vel_to_mpm(vec3 v_world) {
    return v_world / world_size * GRID_SIZE;
}

vec3 mpm_vel_to_world(vec3 v_mpm) {
    return v_mpm / GRID_SIZE * world_size;
}

mat3 transpose_mat3(mat3 m) {
    return transpose(m);
}

float determinant_mat3(mat3 m) {
    return determinant(m);
}

mat3 inverse_mat3(mat3 m) {
    return inverse(m);
}

mat3 multiply_mat3(mat3 a, mat3 b) {
    return a * b;
}

mat3 subtract_mat3(mat3 a, mat3 b) {
    return a - b;
}

mat3 scalar_mult_mat3(mat3 m, float s) {
    return m * s;
}

mat3 add_mat3(mat3 a, mat3 b) {
    return a + b;
}

int index_3d_to_1d(ivec3 idx) {
    return idx.x * GRID_RESOLUTION * GRID_RESOLUTION + idx.y * GRID_RESOLUTION + idx.z;
}

ivec3 index_1d_to_3d(int idx) {
    int z = idx % GRID_RESOLUTION;
    int y = (idx / GRID_RESOLUTION) % GRID_RESOLUTION;
    int x = idx / (GRID_RESOLUTION * GRID_RESOLUTION);
    return ivec3(x, y, z);
}

mat3 outer_product(vec3 a, vec3 b) {
    return mat3(
        a.x * b,
        a.y * b,
        a.z * b
    );
}
