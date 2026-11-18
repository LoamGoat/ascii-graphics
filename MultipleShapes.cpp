#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <cstring>

static volatile std::sig_atomic_t running = 1;
static void on_sigint(int) { running = 0; }

struct Vec3 { float x, y, z; };
struct Point { Vec3 pos, nrm; };

// ---- rotation helpers (same math as before, now as separate steps) ----
Vec3 rotX(Vec3 v, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {v.x, v.y * c - v.z * s, v.y * s + v.z * c};
}
Vec3 rotY(Vec3 v, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}
Vec3 rotZ(Vec3 v, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

// ---- shape generators: points + normals in local space ----
std::vector<Point> makeTorus(float R, float r) {
    std::vector<Point> pts;
    for (float t = 0; t < 6.28318f; t += 0.07f)
        for (float p = 0; p < 6.28318f; p += 0.04f) {
            float ct = std::cos(t), st = std::sin(t);
            float cp = std::cos(p), sp = std::sin(p);
            pts.push_back({{(R + r * ct) * cp, r * st, (R + r * ct) * sp},
                           {ct * cp, st, ct * sp}});
        }
    return pts;
}

std::vector<Point> makeSphere(float r) {
    std::vector<Point> pts;
    for (float t = 0; t < 3.14159f; t += 0.05f)
        for (float p = 0; p < 6.28318f; p += 0.05f) {
            Vec3 n = {std::sin(t) * std::cos(p), std::cos(t), std::sin(t) * std::sin(p)};
            pts.push_back({{n.x * r, n.y * r, n.z * r}, n});
        }
    return pts;
}

std::vector<Point> makeDisk(float r) {
    std::vector<Point> pts;
    for (float t = 0; t < 3.14159f; t += 0.05f)
        for (float p = 0; p < 6.28318f; p += 0.05f) {
            Vec3 n = {std::sin(t) * std::cos(p), 1.0f,  std::sin(t) * std::sin(p)};
            pts.push_back({{n.x * r, n.y * r, n.z * r}, n});
        }
    return pts;
}

std::vector<Point> makeCube(float half) {
    std::vector<Point> pts;
    for (int axis = 0; axis < 3; ++axis)
        for (float sign : {-1.0f, 1.0f})
            for (float u = -1; u <= 1; u += 0.04f)
                for (float v = -1; v <= 1; v += 0.04f) {
                    float c[3], n[3] = {0, 0, 0};
                    c[axis] = sign;
                    c[(axis + 1) % 3] = u;
                    c[(axis + 2) % 3] = v;
                    n[axis] = sign;
                    pts.push_back({{c[0] * half, c[1] * half, c[2] * half},
                                   {n[0], n[1], n[2]}});
                }
    return pts;
}

// ---- an object = shape + placement + spin ----
struct Object {
    std::vector<Point> pts;
    Vec3 offset;         // where it sits in the world
    Vec3 angle{0, 0, 0}; // current rotation (x, y, z)
    Vec3 spin;           // rotation speed, radians/sec
};

const int W = 80, H = 24;
const char* chars = " .'-:;+=*xX#%&@";const int nChars = (int)std::strlen(chars);
const Vec3 light = {0, 0.7071f, -0.7071f};

void drawObject(const Object& o, std::vector<char>& out, std::vector<float>& zbuf) {
    for (const Point& p : o.pts) {
        // rotate the normal and the position, then move into the world
        Vec3 n = rotZ(rotY(rotX(p.nrm, o.angle.x), o.angle.y), o.angle.z);
        Vec3 v = rotZ(rotY(rotX(p.pos, o.angle.x), o.angle.y), o.angle.z);
        v.x += o.offset.x; v.y += o.offset.y; v.z += o.offset.z;

        if (v.z < 0.5f) continue;                 // behind or too near the camera
        float ooz = 1.0f / v.z;
        int xp = int(W / 2 + 30 * ooz * v.x);
        int yp = int(H / 2 - 15 * ooz * v.y);

        float L = n.x * light.x + n.y * light.y + n.z * light.z;
        if (L <= 0 || xp < 0 || xp >= W || yp < 0 || yp >= H) continue;

        int idx = xp + yp * W;
        if (ooz > zbuf[idx]) {                    // shared z-buffer
            zbuf[idx] = ooz;
            // in drawObject, replace the old out[idx] = ... line:
            out[idx] = chars[std::min(int(L * nChars), nChars - 1)];
        }
    }
}

int main() {
    std::vector<Object> scene;

    //scene.push_back({makeShape(size), {x,  y,  z}, {0, 0, 0}, {1.2f, 0.6f, 0.0f}});    

    //scene.push_back({makeTorus(1.2f, 0.5f), {-3.0f,  0.0f, 8.0f}, {0, 0, 0}, {1.2f, 0.6f, 0.0f}});
    //scene.push_back({makeSphere(2.0f),      { 0.0f,  0.0f, 6.0f}, {0, 0, 0}, {0.0f, 0.5f, 0.0f}});
    //scene.push_back({makeCube(2.0f),        { 0.0f,  0.0f, 6.0f}, {0, 0, 0}, {0.8f, 1.1f, 0.4f}});
    //scene.push_back({makeDisk(2.0f),        { 0.0f,  0.0f, 6.0f}, {0, 0, 0}, {1.2f, 0.6f, 0.0f}});

    scene.push_back({makeTorus(3.0f, 1.0f), {1, 0.0f, 10}, {0, 0, 0}, {0.5f, 0.3f, 0.1f}}); // scene[0]: big torus
    scene.push_back({makeSphere(1.0f), {1, 0.0f, 10}, {0, 0, 0}, {0, 1.0f, 0}}); // scene[1]: small sphere
    scene.push_back({makeCube(1.0f), {1, 0.0f, 10}, {0, 0, 0}, {0, 0, 1.0f}}); // scene[2]: small cube

    std::vector<char>  out(W * H);
    std::vector<float> zbuf(W * H);
    std::string frame;

    std::signal(SIGINT, on_sigint);
    std::fputs("\x1b[2J\x1b[?25l", stdout);

    using clock = std::chrono::steady_clock;
    const auto frame_time = std::chrono::duration_cast<clock::duration>(
        std::chrono::duration<double>(1.0 / 60.0));
    auto last = clock::now();

    float time = 0;
    const float orbitRadius = 5.0f;   // distance from the center sphere
    const float orbitSpeed  = 1.0f;   // radians per second
    const float orbitTilt   = 0.6f;   // radians

    

    while (running) {
        auto start = clock::now();
        float dt = std::chrono::duration<float>(start - last).count();
        last = start;
        time += dt;

        std::fill(out.begin(), out.end(), ' ');   // clear ONCE per frame
        std::fill(zbuf.begin(), zbuf.end(), 0.0f);

        // orbit update (must be inside the loop, before drawing)
        Object& center = scene[0];
        Object& moon   = scene[1];
        Vec3 orbit = rotX({orbitRadius * std::cos(time * orbitSpeed), 0,
                           orbitRadius * std::sin(time * orbitSpeed)}, orbitTilt);
        moon.offset = {center.offset.x + orbit.x,
                       center.offset.y + orbit.y,
                       center.offset.z + orbit.z};

        std::fill(out.begin(), out.end(), ' ');
        std::fill(zbuf.begin(), zbuf.end(), 0.0f);


        for (Object& o : scene) {
            o.angle.x += o.spin.x * dt;
            o.angle.y += o.spin.y * dt;
            o.angle.z += o.spin.z * dt;
            drawObject(o, out, zbuf);             // all draw into the same buffers
        }

        frame = "\x1b[H";
        for (int r = 0; r < H; ++r) {
            frame.append(&out[r * W], W);
            frame += '\n';
        }
        std::fwrite(frame.data(), 1, frame.size(), stdout);
        std::fflush(stdout);

        std::this_thread::sleep_until(start + frame_time);
    }
    std::fputs("\x1b[?25h\x1b[2J\x1b[H", stdout);
}