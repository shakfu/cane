#include <cstdlib>
#include <print>
#include <string_view>
#include <vector>

#include <cane/cli.hpp>

namespace {
	int failures = 0;

	void check(bool ok, std::string_view what) {
		if (not ok) {
			std::println(stderr, "FAIL: {}", what);
			++failures;
		}
	}

	auto parse(std::vector<const char*> args) {
		return cane::parse_args(args);
	}
}  // namespace

int main(int, const char*[]) {
	// Defaults: REPL with the default virtual port.
	auto none = parse({});
	check(none and not none->file and not none->port, "no arguments");
	check(none and not none->virtual_name and not none->no_midi, "defaults");

	auto full = parse({ "-w", "--port", "IAC Bus", "live.cane" });
	check(full and full->watch and full->file == "live.cane", "watch file");
	check(full and full->port == "IAC Bus", "port name with a space");

	auto virt = parse({ "--virtual", "beats" });
	check(virt and virt->virtual_name == "beats", "virtual name");

	check(parse({ "-l" }).value_or(cane::Options {}).list, "list");
	check(parse({ "-h" }).value_or(cane::Options {}).help, "help");
	check(parse({ "-n" }).value_or(cane::Options {}).no_midi, "no midi");

	// Errors.
	check(not parse({ "--bogus" }), "unknown option");
	check(not parse({ "--port" }), "port without a name");
	check(not parse({ "--virtual" }), "virtual without a name");
	check(not parse({ "a.cane", "b.cane" }), "two files");
	check(not parse({ "--watch" }), "watch without a file");
	check(not parse({ "-p", "x", "-v", "y" }), "port and virtual");
	check(not parse({ "-n", "-p", "x" }), "no-midi and port");

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
