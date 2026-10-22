#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <print>
#include <thread>

#include <cane/session.hpp>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// Waits up to 2s for `count` to reach `n`.
static bool wait_for(const std::atomic<int>& count, int n) {
	for (int i = 0; i < 200 and count < n; ++i) {
		std::this_thread::sleep_for(10ms);
	}

	return count >= n;
}

// The watcher fires once on start and once per change, and edits made by
// the watcher thread are visible to bindings used by the main thread.
int main(int, const char*[]) {
	auto path = fs::temp_directory_path() / "cane-watch-test.cane";
	std::ofstream { path } << "x = 2";

	cane::Session session { {} };
	std::atomic<int> count = 0;

	{
		auto watcher = cane::watch(path, [&] {
			session.eval(cane::read_file(path));
			++count;
		});

		if (not wait_for(count, 1)) {
			std::println(stderr, "no initial evaluation");
			return EXIT_FAILURE;
		}

		std::ofstream { path } << "y = x";
		// Some filesystems store mtime in whole seconds.
		fs::last_write_time(path, fs::last_write_time(path) + 1s);

		if (not wait_for(count, 2)) {
			std::println(stderr, "change not detected");
			return EXIT_FAILURE;
		}
	}

	try {
		session.eval("x + y");
	}

	catch (const cane::Fatal& e) {
		std::println(stderr, "bindings did not persist: {}", e.what());
		return EXIT_FAILURE;
	}

	fs::remove(path);
	return EXIT_SUCCESS;
}
