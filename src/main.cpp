#include "scheduler/thread_pool.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

namespace {
std::mutex output_mutex;
void line(const std::string& text) {
    std::lock_guard<std::mutex> lock(output_mutex);
    std::cout << text << '\n' << std::flush;
}
void print_stats(const scheduler::ThreadPool& pool) {
    const auto s = pool.stats();
    line("queued=" + std::to_string(s.queued) + " active=" + std::to_string(s.active)
         + " submitted=" + std::to_string(s.submitted) + " completed=" + std::to_string(s.completed)
         + " accepting=" + (s.accepting ? "yes" : "no"));
}
bool positive_number(const std::string& text, std::size_t& value, std::size_t maximum) {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) { return false; }
    try {
        const auto parsed = std::stoull(text);
        if (parsed == 0 || parsed > maximum) { return false; }
        value = static_cast<std::size_t>(parsed);
        return true;
    } catch (const std::exception&) { return false; }
}
void submit_job(scheduler::ThreadPool& pool, const std::string& name, std::size_t ms) {
    // Hold the output lock through submission so ACCEPTED always prints before START.
    std::lock_guard<std::mutex> lock(output_mutex);
    pool.submit([name, ms] {
        line("START " + name);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        line("DONE " + name);
    });
    std::cout << "ACCEPTED " << name << '\n' << std::flush;
}
}
int main(int argc, char** argv) {
    std::size_t workers = 3, capacity = 8;
    bool demo = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--demo") { demo = true; }
        else if ((arg == "--workers" || arg == "--capacity") && i + 1 < argc) {
            std::size_t number = 0;
            if (!positive_number(argv[++i], number, arg == "--workers" ? 64 : 100000)) {
                std::cerr << "Invalid positive numeric option\n"; return 2;
            }
            if (arg == "--workers") { workers = number; } else { capacity = number; }
        } else if (arg == "--help") {
            std::cout << "scheduler_cli [--workers 1..64] [--capacity 1..100000] [--demo]\n"; return 0;
        } else { std::cerr << "Unknown or incomplete option: " << arg << '\n'; return 2; }
    }
    try {
        scheduler::ThreadPool pool(workers, capacity);
        if (demo) {
            for (int i = 1; i <= 5; ++i) {
                // A barrier retry avoids a spin loop when a small demo queue fills.
                for (;;) {
                    try { submit_job(pool, "demo-" + std::to_string(i), 40); break; }
                    catch (const scheduler::QueueFull&) { pool.wait_idle(); }
                }
            }
        } else {
            line("Commands: submit NAME MILLISECONDS | status | wait | help | quit");
            line("Duration: 1..60000 ms. Names: one word, max 64 characters. FIFO dequeue order, not start or finish order.");
            std::string input;
            while (std::getline(std::cin, input)) {
                std::istringstream tokens(input);
                std::string command, name, duration, extra;
                tokens >> command;
                if (command.empty()) { continue; }
                if (command == "submit") {
                    std::size_t ms = 0;
                    if (!(tokens >> name >> duration) || (tokens >> extra) || name.size() > 64
                        || !positive_number(duration, ms, 60000)) {
                        line("ERROR usage: submit NAME MILLISECONDS (1..60000)"); continue;
                    }
                    try { submit_job(pool, name, ms); }
                    catch (const scheduler::QueueFull& e) { line(std::string("REJECTED ") + e.what()); }
                } else if (tokens >> extra) { line("ERROR unexpected arguments"); }
                else if (command == "status") { print_stats(pool); }
                else if (command == "wait") { pool.wait_idle(); line("IDLE"); }
                else if (command == "quit") { break; }
                else if (command == "help") { line("submit NAME MS | status | wait | quit"); }
                else { line("ERROR unknown command"); }
            }
        }
        line("SHUTDOWN draining accepted jobs");
        pool.shutdown();
        print_stats(pool);
        line("STOPPED");
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n'; return 1;
    }
}
