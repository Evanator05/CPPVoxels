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
    vec3 invDir = 1.0 / ray.dir;

    vec3 t1 = (box.pos - ray.pos) * invDir;
    vec3 t2 = (box.pos + box.size - ray.pos) * invDir;
    
    float tmin = max(max(min(t1.x, t2.x), min(t1.y, t2.y)), min(t1.z, t2.z));
    float tmax = min(min(max(t1.x, t2.x), max(t1.y, t2.y)), max(t1.z, t2.z));
    
    tmin = max(tmin, 0.0); // clamp near to 0
    
    Intersection intersection;
    intersection.near = tmin;
    intersection.far = tmax;
    intersection.hit = (tmin <= tmax) && (tmax > 0.0);
    
    return intersection;
}