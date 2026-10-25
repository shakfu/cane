#ifndef CANE_SESSION_HPP
#define CANE_SESSION_HPP

#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>

#include <cane/parse.hpp>
#include <cane/passes.hpp>

namespace cane {
	// General MIDI drum notes, under the short names TidalCycles and Strudel
	// use, so they do not clash with bindings such as `kick` or `snare`.
	constexpr std::string_view PRELUDE = R"(
		bd = 36; rim = 37; sd = 38; cp = 39;
		hh = 42; oh = 46; lt = 45; mt = 47; ht = 50;
		cr = 49; rd = 51; tb = 54; cb = 56; sh = 70;
	)";

	// Builtin functions. Their bodies use operators the lexer does not
	// produce, so they cannot be written in the prelude.
	inline BindingEnvironment builtins() {
		auto param = [](std::string_view sv) {
			return std::make_shared<Node>(
				SymbolKind::Identifier, sv, TypeKind::None
			);
		};

		auto function = [&](std::string_view sv, BoxNode body) {
			return std::make_shared<Node>(
				SymbolKind::Function, sv, TypeKind::Function, param(sv), body
			);
		};

		auto entry = [](std::string_view name, BoxNode fn) {
			return BindingEntry { name, BindingKind::Binding, 0, fn };
		};

		// `name` is `\x -> op x`.
		auto unary = [&](std::string_view name, SymbolKind op) {
			return entry(
				name,
				function(
					"%x", std::make_shared<Node>(op, name, TypeKind::None, param("%x"))
				)
			);
		};

		// `name` is `\n -> \x -> x op n`, so `take 2 x` reads as in Haskell.
		auto binary = [&](std::string_view name, SymbolKind op) {
			auto body = std::make_shared<Node>(
				op, name, TypeKind::None, param("%x"), param("%n")
			);

			return entry(name, function("%n", function("%x", body)));
		};

		auto node = [](SymbolKind kind, std::string_view sv, BoxNode lhs, BoxNode rhs) {
			return std::make_shared<Node>(kind, sv, TypeKind::None, lhs, rhs);
		};

		// Both branches of `every` and `sometimes`: `f x`, then `x`.
		auto either = [&](std::string_view sv) {
			return node(
				SymbolKind::Either,
				sv,
				node(SymbolKind::Call, sv, param("%f"), param("%x")),
				param("%x")
			);
		};

		// `every k f x`, `sometimes f x`.
		auto every = node(SymbolKind::Every, "every", param("%k"), either("every"));
		auto sometimes =
			node(SymbolKind::Sometimes, "sometimes", either("sometimes"), nullptr);

		return {
			entry(
				"every",
				function("%k", function("%f", function("%x", every)))
			),
			entry("sometimes", function("%f", function("%x", sometimes))),
			entry(
				"cycle",
				std::make_shared<Node>(SymbolKind::Cycle, "cycle", TypeKind::Scalar)
			),
			unary("abs", SymbolKind::Abs),
			unary("head", SymbolKind::Head),
			unary("tail", SymbolKind::Tail),
			binary("take", SymbolKind::Take),
			binary("scale", SymbolKind::Scale),
			binary("stack", SymbolKind::Stack),
			unary("chord", SymbolKind::Chord),
			binary("drop", SymbolKind::Drop),
		};
	}

	// Bindings persist across calls to `eval`. Callers may be on different
	// threads (REPL and file watcher).
	class Session {
		private:
		Configuration cfg;
		BindingEnvironment bindings = builtins();

		// Nodes and bindings are views into source text, so all of it is kept.
		// A deque never relocates its elements. Shared with each `Result::at`,
		// which may outlive the session on the player thread.
		std::shared_ptr<std::deque<std::string>> sources =
			std::make_shared<std::deque<std::string>>();

		std::mutex mutex;

		public:
		explicit Session(Configuration cfg_): cfg(cfg_) {
			eval(std::string { PRELUDE });
		}

		struct Result {
			Value value;    // Cycle 0.
			TypeKind type;  // Only a Pattern is playable.

			// The value at any cycle. Safe to call from another thread.
			std::function<Value(Scalar cycle)> at;
		};

		// Bindings are only committed once evaluation succeeds.
		Result eval(std::string src) {
			std::scoped_lock lock { mutex };

			cane::Parser parser { sources->emplace_back(std::move(src)) };
			auto root = parser.parse();

			if (not root.has_value()) {
				return { std::monostate {}, TypeKind::None, {} };
			}

			auto valid = cane::pass_validate(cfg, root.value());
			auto [resolved, env] = cane::resolve_bindings(cfg, valid, bindings);
			auto typed = cane::pass_type_resolution(cfg, resolved);
			auto at = [cfg = cfg, typed, sources = sources](Scalar cycle) {
				return cane::pass_evaluator_walk(
					cfg, cycle_environment(cfg, cycle), typed
				);
			};

			auto value = at(0);

			bindings = std::move(env);
			return { std::move(value), typed->type, std::move(at) };
		}
	};

	// Calls `fn` now and after each change to `path`'s modification time,
	// until the returned thread is destroyed. A missing file is skipped, since
	// editors often save by replacing the file.
	[[nodiscard]] inline std::jthread
	watch(std::filesystem::path path, std::function<void()> fn) {
		return std::jthread { [path = std::move(path),
		                       fn = std::move(fn)](std::stop_token stop) {
			std::filesystem::file_time_type last {};

			while (not stop.stop_requested()) {
				std::error_code ec;
				auto now = std::filesystem::last_write_time(path, ec);

				if (not ec and now != last) {
					last = now;
					fn();
				}

				std::this_thread::sleep_for(std::chrono::milliseconds { 100 });
			}
		} };
	}
}  // namespace cane

#endif
