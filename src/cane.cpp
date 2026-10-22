#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include <linenoise.h>
#include <libremidi/libremidi.hpp>

#include <cane/cane.hpp>
#include <cane/cli.hpp>
#include <cane/midi.hpp>

namespace {
	constexpr std::string_view DEFAULT_PORT = "cane";

	// One line per event for sequences and patterns; other values as-is.
	void print_value(const cane::Value& value) {
		if (const auto* seq = std::get_if<cane::Sequence>(&value)) {
			for (const auto& ev: *seq) {
				std::println(
					"{:>6} ms  ch {:>2}  note {:>3}  {}",
					std::chrono::duration_cast<std::chrono::milliseconds>(
						ev.timestamp
					)
						.count(),
					static_cast<int>(ev.channel),
					static_cast<int>(ev.note),
					ev.kind
				);
			}
		}

		else if (not std::holds_alternative<std::monostate>(value)) {
			std::println("{}", value);
		}
	}

	// Evaluates the source `read` returns, prints the value, and plays it if
	// it is a pattern. Returns nullopt if reading or evaluation failed.
	std::optional<cane::Session::Result> run(
		cane::Session& session,
		cane::Player* player,
		const std::function<std::string()>& read,
		bool loop = true
	) {
		try {
			auto result = session.eval(read());
			print_value(result.value);

			if (player != nullptr and result.type == cane::TypeKind::Pattern) {
				player->play(std::get<cane::Sequence>(result.value), loop);
			}

			return result;
		}

		catch (const cane::Fatal& e) {
			std::cerr << e.what();
		}

		// Any other exception is a cane bug. Thrown on the watcher thread, it
		// would call std::terminate.
		catch (const std::exception& e) {
			std::println(stderr, "[!] internal error: {}", e.what());
		}

		return std::nullopt;
	}

	// Each line is compiled on its own. Errors are reported and the loop
	// continues. Ctrl+C discards the line, Ctrl+D exits.
	int repl(cane::Session& session, cane::Player* player) {
		std::unique_ptr<linenoise_context_t, decltype(&linenoise_context_destroy)>
			ctx { linenoise_context_create(), linenoise_context_destroy };

		if (ctx == nullptr) {
			std::println(stderr, "failed to start repl");
			return EXIT_FAILURE;
		}

		while (true) {
			std::unique_ptr<char, decltype(&linenoise_free)> line {
				linenoise_read(ctx.get(), "cane> "), linenoise_free
			};

			if (line == nullptr) {
				if (linenoise_get_error() == LINENOISE_ERR_INTERRUPTED) {
					continue;
				}

				return EXIT_SUCCESS;
			}

			linenoise_history_add(ctx.get(), line.get());
			CANE_UNUSED(run(session, player, [&] { return line.get(); }));
		}
	}

	std::string message(const stdx::error& err) {
		auto msg = err.message();
		return { msg.data(), msg.size() };
	}

	// Includes virtual ports, e.g. a DAW's input, which libremidi skips by
	// default.
	std::vector<libremidi::output_port> output_ports() {
		libremidi::observer obs { { .track_hardware = true, .track_virtual = true } };
		return obs.get_output_ports();
	}

	int list_ports() {
		auto ports = output_ports();

		if (ports.empty()) {
			std::println(stderr, "no output ports");
		}

		for (const auto& port: ports) {
			std::println("{}", port.display_name);
		}

		return EXIT_SUCCESS;
	}

	// Opens the output `opts` asks for. Failing to create the default
	// virtual port is not an error: cane then prints events only.
	std::expected<std::optional<libremidi::midi_out>, std::string>
	open_output(const cane::Options& opts) {
		if (opts.no_midi) {
			return std::nullopt;
		}

		libremidi::midi_out out;

		if (opts.port) {
			auto [port, error] = cane::find_port(output_ports(), *opts.port);

			if (not port) {
				return std::unexpected { error };
			}

			if (auto err = out.open_port(*port, DEFAULT_PORT);
				err != stdx::error {}) {
				return std::unexpected { std::format(
					"cannot open '{}': {}", port->display_name, message(err)
				) };
			}

			std::println(stderr, "[midi] sending to '{}'", port->display_name);
			return out;
		}

		auto name = opts.virtual_name.value_or(std::string { DEFAULT_PORT });

		if (auto err = out.open_virtual_port(name); err != stdx::error {}) {
			auto msg = std::format(
				"cannot create virtual port '{}': {}", name, message(err)
			);

			if (opts.virtual_name) {
				return std::unexpected { msg };
			}

			std::println(stderr, "[midi] {}; printing events only", msg);
			return std::nullopt;
		}

		std::println(stderr, "[midi] virtual port '{}'", name);
		return out;
	}
}  // namespace

int main(int argc, const char* argv[]) {
	auto opts = cane::parse_args(std::span { argv + 1, argv + argc });

	if (not opts) {
		std::println(stderr, "cane: {}\n\n{}", opts.error(), cane::USAGE);
		return EXIT_FAILURE;
	}

	if (opts->help) {
		std::print("{}", cane::USAGE);
		return EXIT_SUCCESS;
	}

	if (opts->list) {
		return list_ports();
	}

	auto out = open_output(*opts);

	if (not out) {
		std::println(stderr, "cane: {}", out.error());
		return EXIT_FAILURE;
	}

	// Declared after `out`, so it stops, and releases its notes, first.
	std::optional<cane::Player> player;

	if (out->has_value()) {
		player.emplace([&midi = out->value()](const cane::MidiMessage& msg) {
			CANE_UNUSED(midi.send_message(msg.data(), msg.size()));
		});
	}

	auto* p = player ? &player.value() : nullptr;

	cane::Session session { { .bpm = 120, .channel_bindings = {
		{ "drums", 10 },
		{ "303", 12 },
	}, } };

	if (not opts->file) {
		return repl(session, p);
	}

	std::filesystem::path path = *opts->file;

	auto read = [path] { return cane::read_file(path); };

	if (opts->watch) {
		auto watcher = cane::watch(path, [&session, p, path, read] {
			std::println(stderr, "[watch] evaluating '{}'", path.string());
			CANE_UNUSED(run(session, p, read));
		});

		return repl(session, p);
	}

	if (not run(session, p, read, false)) {
		return EXIT_FAILURE;
	}

	if (p != nullptr) {
		p->wait_idle();
	}

	return EXIT_SUCCESS;
}
