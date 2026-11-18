#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

static volatile std::sig_atomic_t running = 1;
static void on_sigint(int) { running = 0; }

int main() {
    const int W = 80, H = 24;
    const char* chars = ".,-~:;=!*#$@";
    const double FPS = 60.0;
    const auto frame_time = std::chrono::duration<double>(1.0 / FPS);

    std::vector<char>  output(W * H);
    std::vector<float> zbuf(W * H);
    std::string frame;
    frame.reserve(W * H + H + 8);

    float A = 0, B = 0;
    std::signal(SIGINT, on_sigint);
    std::fputs("\x1b[2J\x1b[?25l", stdout);

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    while (running) {
        auto start = clock::now();
        float dt = std::chrono::duration<float>(start - last).count();
        last = start;

        std::fill(output.begin(), output.end(), ' ');
        std::fill(zbuf.begin(), zbuf.end(), 0.0f);

        float cA = std::cos(A), sA = std::sin(A);
        float cB = std::cos(B), sB = std::sin(B);

        for (float theta = 0; theta < 6.28318f; theta += 0.07f) {
            float ct = std::cos(theta), st = std::sin(theta);
            for (float phi = 0; phi < 6.28318f; phi += 0.02f) {
                float cp = std::cos(phi), sp = std::sin(phi);
                float cx = 2 + ct, cy = st;

                float x = cx * (cB * cp + sA * sB * sp) - cy * cA * sB;
                float y = cx * (sB * cp - sA * cB * sp) + cy * cA * cB;
                float z = 5 + cA * cx * sp + cy * sA;
                float ooz = 1.0f / z;

                int xp = int(W / 2 + 30 * ooz * x);
                int yp = int(H / 2 - 15 * ooz * y);

                float L = cp * ct * sB - cA * ct * sp - sA * st
                        + cB * (cA * st - ct * sA * sp);

                if (L > 0 && xp >= 0 && xp < W && yp >= 0 && yp < H) {
                    int idx = xp + yp * W;
                    if (ooz > zbuf[idx]) {
                        zbuf[idx] = ooz;
                        output[idx] = chars[std::min(int(L * 8), 11)];
                    }
                }
            }
        }

        frame = "\x1b[H";
        for (int r = 0; r < H; ++r) {
            frame.append(&output[r * W], W);
            frame += '\n';
        }
        std::fwrite(frame.data(), 1, frame.size(), stdout);
        std::fflush(stdout);

        A += 1.2f * dt;   // radians per second, frame-rate independent
        B += 0.6f * dt;

        std::this_thread::sleep_until(start + std::chrono::duration_cast<clock::duration>(frame_time));
    }

    std::fputs("\x1b[?25h\x1b[2J\x1b[H", stdout);  // restore cursor
    return 0;
}