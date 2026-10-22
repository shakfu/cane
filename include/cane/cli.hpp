#ifndef CANE_CLI_HPP
#define CANE_CLI_HPP

#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace cane {
	constexpr std::string_view USAGE = R"(usage: cane [options] [FILE]

With no FILE, start a REPL. With FILE, play it once and exit.

options:
  -w, --watch          REPL, and re-evaluate FILE on each save
  -p, --port NAME      send to an existing output port (exact name, or a
                       unique part of one)
  -v, --virtual NAME   create a virtual output port NAME (default: cane)
  -n, --no-midi        print events only
  -l, --list           list output ports and exit
  -h, --help           show this message
)";

	struct Options {
		std::optional<std::string> file;
		std::optional<std::string> port;
		std::optional<std::string> virtual_name;  // Unset: the default port.

		bool watch = false;
		bool no_midi = false;
		bool list = false;
		bool help = false;
	};

	// `args` excludes the program name.
	inline std::expected<Options, std::string>
	parse_args(std::span<const char* const> args) {
		Options opts;

		for (size_t i = 0; i < args.size(); ++i) {
			std::string_view arg = args[i];

			auto value = [&]() -> std::optional<std::string> {
				if (i + 1 >= args.size()) {
					return std::nullopt;
				}

				return args[++i];
			};

			if (arg == "-h" or arg == "--help") {
				opts.help = true;
			}

			else if (arg == "-l" or arg == "--list") {
				opts.list = true;
			}

			else if (arg == "-w" or arg == "--watch") {
				opts.watch = true;
			}

			else if (arg == "-n" or arg == "--no-midi") {
				opts.no_midi = true;
			}

			else if (arg == "-p" or arg == "--port") {
				opts.port = value();

				if (not opts.port) {
					return std::unexpected { std::format("{} needs a NAME", arg) };
				}
			}

			else if (arg == "-v" or arg == "--virtual") {
				opts.virtual_name = value();

				if (not opts.virtual_name) {
					return std::unexpected { std::format("{} needs a NAME", arg) };
				}
			}

			else if (arg.starts_with('-')) {
				return std::unexpected { std::format("unknown option '{}'", arg) };
			}

			else if (opts.file) {
				return std::unexpected { "only one FILE may be given" };
			}

			else {
				opts.file = std::string { arg };
			}
		}

		if (opts.watch and not opts.file) {
			return std::unexpected { "--watch needs a FILE" };
		}

		if (opts.port and opts.virtual_name) {
			return std::unexpected { "--port and --virtual are exclusive" };
		}

		if (opts.no_midi and (opts.port or opts.virtual_name)) {
			return std::unexpected { "--no-midi excludes --port and --virtual" };
		}

		return opts;
	}
}  // namespace cane

#endif
