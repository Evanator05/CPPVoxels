#define VOXELRED   0x001F
#define VOXELGREEN 0x03E0
#define VOXELBLUE  0x7C00
#define VOXELSOLID 0x8000

#define CHUNKWIDTH 64

struct Chunk {
	ivec3 pos;
	int index;
	ivec3 size;
    int pad;
};

vec3 getVoxelColor(int data) {
    return vec3(                       // values from 0-31
        float(data&VOXELRED),          // get the red date
        float((data&VOXELGREEN) >> 5), // get the green data
        float((data&VOXELBLUE) >> 10)  // get the blue data
    ) / float(VOXELRED);               // divide by 31 to make values 0-1
}