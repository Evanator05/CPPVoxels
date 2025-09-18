struct Ray {
    vec3 pos;
    vec3 dir;
};

struct Intersection {
    bool hit;
    float near;
    float far;
};

struct Box {
    vec3 pos;
    vec3 size;
};

Ray createRay(vec3 pos, vec3 dir) {
    Ray ray;
    ray.pos = pos;
    ray.dir = dir;
    return ray;
}

Box createBox(vec3 pos, vec3 size) {
    Box box;
    box.pos = pos;
    box.size = size;
    return box;
}

vec4 quatMultiply(vec4 q1, vec4 q2) {
    return vec4(
        q1.w*q2.xyz + q2.w*q1.xyz + cross(q1.xyz, q2.xyz),
        q1.w*q2.w - dot(q1.xyz, q2.xyz)
    );
}

vec3 rotateVectorByQuat(vec3 v, vec4 q) {
    // q = (x, y, z, w)
    vec3 u = q.xyz;
    float s = q.w;

    return 2.0 * dot(u, v) * u
         + (s*s - dot(u,u)) * v
         + 2.0 * s * cross(u, v);
}

Intersection intersectAABB(Ray ray, Box box) {
    vec3 t1 = (box.pos - ray.pos) / ray.dir;
    vec3 t2 = (box.pos+box.size - ray.pos) / ray.dir;

    vec3 intersectMin = min(t1, t2); // get the minimum intersection
    vec3 intersectMax = max(t1, t2); // get the maximum intersection
    
    float near = max(max(intersectMin.x, intersectMin.y), intersectMin.z); // get the near hit distance
    float far  = min(min(intersectMax.x, intersectMax.y), intersectMax.z); // get the far hit fistance
    
    if (near < 0.0) near = 0.0;

    Intersection intersection;
    intersection.near = near;
    intersection.far = far;
    intersection.hit = near <= far && far > 0.0; // get if the ray hit

    return intersection;
}