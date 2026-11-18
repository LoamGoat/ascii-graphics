// solar.cpp  --  g++ -O2 -o solar solar.cpp && ./solar
#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <sys/ioctl.h>
#include <unistd.h>

static volatile std::sig_atomic_t running = 1;
static void on_sigint(int) { running = 0; }

// ---------- math ----------
struct Vec3 { float x, y, z; };

Vec3 rotX(Vec3 v, float a) { float c = std::cos(a), s = std::sin(a); return {v.x, v.y*c - v.z*s, v.y*s + v.z*c}; }
Vec3 rotY(Vec3 v, float a) { float c = std::cos(a), s = std::sin(a); return {v.x*c + v.z*s, v.y, -v.x*s + v.z*c}; }
Vec3 rotZ(Vec3 v, float a) { float c = std::cos(a), s = std::sin(a); return {v.x*c - v.y*s, v.x*s + v.y*c, v.z}; }
Vec3 rotXYZ(Vec3 v, Vec3 a) { return rotZ(rotY(rotX(v, a.x), a.y), a.z); }

// ---------- shapes: points + outward normals in local space ----------
struct Point { Vec3 pos, nrm; float shade; };

std::vector<Point> makeSphere(float r, float step = 0.04f) {
    std::vector<Point> pts;
    for (float t = 0; t < 3.14159f; t += step)
        for (float p = 0; p < 6.28318f; p += step) {
            Vec3 n = {std::sin(t)*std::cos(p), std::cos(t), std::sin(t)*std::sin(p)};
            float shade = ((int(t / 0.4f) + int(p / 0.4f)) % 2) ? 1.0f : 0.45f; // checker
            pts.push_back({{n.x*r, n.y*r, n.z*r}, n, shade});
        }
    return pts;
}

std::vector<Point> makeTorus(float R, float r) {
    std::vector<Point> pts;
    for (float t = 0; t < 6.28318f; t += 0.07f)
        for (float p = 0; p < 6.28318f; p += 0.04f) {
            float ct = std::cos(t), st = std::sin(t), cp = std::cos(p), sp = std::sin(p);
            pts.push_back({{(R + r*ct)*cp, r*st, (R + r*ct)*sp}, {ct*cp, st, ct*sp}, 1.0f});
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
                    c[axis] = sign; c[(axis+1)%3] = u; c[(axis+2)%3] = v; n[axis] = sign;
                    pts.push_back({{c[0]*half, c[1]*half, c[2]*half}, {n[0], n[1], n[2]}, 1.0f});
                }
    return pts;
}

// ---------- scene objects ----------
struct Object {
    std::vector<Point> pts;
    Vec3  offset{0, 0, 10};   // world position (overwritten for orbiting bodies)
    Vec3  angle{0, 0, 0};     // current rotation
    Vec3  spin{0, 0, 0};      // rotation speed, rad/s
    // orbit: parent index (-1 = fixed), must be a LOWER index than this object
    int   parent = -1;
    float radius = 0, speed = 0, tilt = 0, phase = 0;
};

// ---------- rendering ----------
int W = 80, H = 24;
float zoomX, zoomY;
const char* chars = " .:-=+*#%@";
const int nChars = (int)std::strlen(chars);
const Vec3 light = {0, 0.7071f, -0.7071f};

void drawObject(const Object& o, std::vector<char>& out, std::vector<float>& zbuf) {
    for (const Point& p : o.pts) {
        Vec3 n = rotXYZ(p.nrm, o.angle);
        Vec3 v = rotXYZ(p.pos, o.angle);
        v.x += o.offset.x; v.y += o.offset.y; v.z += o.offset.z;
        if (v.z < 0.5f) continue;

        float ooz = 1.0f / v.z;
        int xp = int(W / 2 + zoomX * ooz * v.x);
        int yp = int(H / 2 - zoomY * ooz * v.y);

        float L = (n.x*light.x + n.y*light.y + n.z*light.z) * p.shade;
        if (L <= 0 || xp < 0 || xp >= W || yp < 0 || yp >= H) continue;

        int idx = xp + yp * W;
        if (ooz > zbuf[idx]) {
            zbuf[idx] = ooz;
            out[idx] = chars[std::min(int(L * nChars), nChars - 1)];
        }
    }
}

int main() {
    // fit to the terminal
    winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        W = ws.ws_col;
        H = ws.ws_row - 1;        // leave a row so the last newline doesn't scroll
    }
    zoomX = W * 0.375f;
    zoomY = zoomX * 0.5f;         // characters are ~2x taller than wide

    // ---------- build the scene here ----------
    std::vector<Object> scene;
    auto add = [&](Object o) { scene.push_back(std::move(o)); };

    //                  shape               offset    angle        spin
    Object sun;     sun.pts = makeSphere(1.8f);  sun.offset = {0, 0, 14}; sun.angle = {0, 0, 0.4f};  sun.spin = {0, 0.3f, 0};
    Object planetA; planetA.pts = makeSphere(0.5f); planetA.spin = {0, 1.0f, 0};
    planetA.parent = 0; planetA.radius = 3.5f; planetA.speed = 1.8f; planetA.tilt = 0.3f;  planetA.phase = 0;
    Object planetB; planetB.pts = makeSphere(0.8f); planetB.spin = {0.6f, 0.8f, 0};
    planetB.parent = 0; planetB.radius = 5.5f; planetB.speed = 1.0f; planetB.tilt = -0.4f; planetB.phase = 2.0f;
    Object moonB;   moonB.pts = makeSphere(0.25f);  moonB.spin = {0, 2.0f, 0};
    moonB.parent = 2;   moonB.radius = 1.5f; moonB.speed = 3.0f; moonB.tilt = 0.5f;        moonB.phase = 1.0f;
    Object cube;    cube.pts = makeCube(0.35f);     cube.spin = {0.8f, 1.1f, 0.4f};
    cube.parent = 0;    cube.radius = 7.5f; cube.speed = 0.6f; cube.tilt = 0.8f;           cube.phase = 4.0f;

    add(sun); add(planetA); add(planetB); add(moonB); add(cube);
    // ------------------------------------------

    std::vector<char>  out(W * H);
    std::vector<float> zbuf(W * H);
    std::string frame;

    std::signal(SIGINT, on_sigint);
    std::fputs("\x1b[2J\x1b[?25l", stdout);

    using clock = std::chrono::steady_clock;
    const auto frame_time = std::chrono::duration_cast<clock::duration>(
        std::chrono::duration<double>(1.0 / 60.0));
    auto last = clock::now();
    float t = 0;

    while (running) {
        auto start = clock::now();
        float dt = std::chrono::duration<float>(start - last).count();
        last = start;
        t += dt;

        std::fill(out.begin(), out.end(), ' ');
        std::fill(zbuf.begin(), zbuf.end(), 0.0f);

        for (size_t i = 0; i < scene.size(); ++i) {
            Object& o = scene[i];

            o.angle.x += o.spin.x * dt;       // self-rotation
            o.angle.y += o.spin.y * dt;
            o.angle.z += o.spin.z * dt;

            if (o.parent >= 0) {              // orbit (parent already updated this frame)
                float a = t * o.speed + o.phase;
                Vec3 p = rotX({o.radius * std::cos(a), 0, o.radius * std::sin(a)}, o.tilt);
                const Vec3& c = scene[o.parent].offset;
                o.offset = {c.x + p.x, c.y + p.y, c.z + p.z};
            }
            drawObject(o, out, zbuf);
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