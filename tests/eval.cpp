#include <cstdlib>
#include <format>
#include <print>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <cane/cane.hpp>

namespace {
	int failures = 0;

	void check(bool ok, std::string_view what) {
		if (not ok) {
			std::println(stderr, "FAIL: {}", what);
			++failures;
		}
	}

	cane::Value eval(std::string_view src) {
		cane::Session session { { .bpm = 120, .channel_bindings = {
			{ "drums", 10 },
		}, } };

		return session.eval(std::string { src }).value;
	}

	std::vector<int> notes(const cane::Value& value) {
		std::vector<int> out;

		for (const auto& ev: std::get<cane::Sequence>(value)) {
			out.push_back(ev.note);
		}

		return out;
	}

	// Start time and duration of each event, in ms.
	std::vector<std::pair<long long, long long>> times(const cane::Value& value) {
		using std::chrono::duration_cast, std::chrono::milliseconds;
		std::vector<std::pair<long long, long long>> out;

		for (const auto& ev: std::get<cane::Sequence>(value)) {
			out.emplace_back(
				duration_cast<milliseconds>(ev.timestamp).count(),
				duration_cast<milliseconds>(ev.duration).count()
			);
		}

		return out;
	}

	// One character per step of one beat: `x` beat, `.` rest, `?` neither.
	std::string steps_of(const cane::Value& value) {
		const auto& seq = std::get<cane::Sequence>(value);
		auto step = cane::MINUTE / 120;
		std::string out(cane::length(seq) / step, '?');

		for (const auto& ev: seq) {
			auto& c = out.at(ev.timestamp / step);
			c = ev.kind == cane::EventKind::Beat ? 'x' : '.';
		}

		return out;
	}

	std::vector<int> beat_notes(const cane::Value& value) {
		std::vector<int> out;

		for (const auto& ev: std::get<cane::Sequence>(value)) {
			if (ev.kind == cane::EventKind::Beat) {
				out.push_back(ev.note);
			}
		}

		return out;
	}

	std::vector<cane::EventKind> steps(const cane::Value& value) {
		const auto& rhythm = std::get<cane::Rhythm>(value);
		return { rhythm.begin(), rhythm.end() };
	}
}  // namespace

int main(int, const char*[]) {
	try {
		// Printing a sequence once recursed until the stack overflowed.
		auto pattern = eval("36@!.!. ~> \"drums\"");
		std::println("{}", pattern);
		check(notes(pattern) == std::vector { 36, 36, 36, 36 }, "pattern");

		// Melody-first mapping once read the melody as a scalar.
		check(
			notes(eval("(60, 62)@!!")) == std::vector { 60, 62 },
			"melody:rhythm"
		);

		// Melody-scalar `-` and `/` once had their operands swapped.
		check(
			std::get<cane::Melody>(eval("(60, 62) - 12")) ==
				cane::Melody { 48, 50 },
			"melody - scalar"
		);
		check(
			std::get<cane::Melody>(eval("(60, 62) / 2")) ==
				cane::Melody { 30, 31 },
			"melody / scalar"
		);

		// `?` once used an unseeded generator and always picked one side.
		// Chance of a false failure: 2^-63.
		std::set<cane::Scalar> seen;

		for (int i = 0; i < 64; ++i) {
			seen.insert(std::get<cane::Scalar>(eval("1 ? 2")));
		}

		check(seen == std::set<cane::Scalar> { 1, 2 }, "random choice");

		// Sequence `*` and `/` once scaled durations but not start times.
		using Times = std::vector<std::pair<long long, long long>>;
		check(
			times(eval("36@!. * 2")) == Times { { 0, 1000 }, { 1000, 1000 } },
			"sequence * scalar"
		);
		check(
			times(eval("36@!. / 2")) == Times { { 0, 250 }, { 250, 250 } },
			"sequence / scalar"
		);

		// A trailing `;` once failed to parse, and a trailing comment read past
		// the end of the source.
		check(std::get<cane::Scalar>(eval("1;")) == 1, "trailing ;");
		check(std::get<cane::Scalar>(eval("1; #! c")) == 1, "trailing comment");
		check(
			std::holds_alternative<std::monostate>(eval("#! c")), "comment only"
		);

		// Operators in one precedence group are equal and left-associative.
		// `/` once bound tighter than `*`, and `and` tighter than `or`.
		check(std::get<cane::Scalar>(eval("3 * 4 / 8")) == 1, "* and / equal");

		using enum cane::EventKind;
		check(
			steps(eval("!... or !.!. and .!..")) ==
				std::vector { Rest, Rest, Rest, Rest },
			"or and and equal"
		);

		// Note names: scientific pitch, case insensitive.
		auto scalar = [](std::string_view src) {
			return std::get<cane::Scalar>(eval(src));
		};

		check(scalar("C4") == 60, "C4");
		check(scalar("c4") == 60, "c4");
		check(scalar("C#4") == 61 and scalar("c#4") == 61, "sharp");
		check(scalar("Bb3") == 58 and scalar("bb3") == 58, "flat");
		check(scalar("C0") == 12 and scalar("G9") == 127, "range ends");
		check(scalar("Cb4") == 59 and scalar("B#3") == 60, "enharmonic wrap");
		check(scalar("C4 + 12") == 72 and scalar("C4-12") == 48, "arithmetic");
		check(
			notes(eval("(c4, eb4, g4)@!!!")) == std::vector { 60, 63, 67 },
			"melody of notes"
		);

		// Longer names are identifiers, so these are unbound.
		for (auto src: { "c4x", "c45", "c4_", "h4" }) {
			bool threw = false;

			try {
				eval(src);
			}

			catch (const cane::Fatal&) {
				threw = true;
			}

			check(threw, std::format("`{}` is not a note", src));
		}

		bool above = false;

		try {
			eval("G#9");
		}

		catch (const cane::Fatal& e) {
			above = std::string_view { e.what() }.contains("above 127");
		}

		check(above, "G#9 is rejected");

		// `c` is an identifier, then `#!` starts a comment.
		check(scalar("c = 5; c #! x") == 5, "c before comment");

		// Drum prelude, and its names can be rebound.
		check(
			notes(eval("(bd, sd, hh, oh)@!!!!")) == std::vector { 36, 38, 42, 46 },
			"drum names"
		);
		check(scalar("bd = 35; bd") == 35, "rebind drum name");

		auto throws = [](std::string_view src, std::string_view what) {
			try {
				eval(src);
			}

			catch (const cane::Fatal& e) {
				return std::string_view { e.what() }.contains(what);
			}

			return false;
		};

		// `=` binds a name; anything else on its left is a syntax error.
		check(scalar("x = 2; y = x + 1; y") == 3, "assign");
		check(
			throws("3 = 4", "expected an identifier before `=`"), "assign to number"
		);

		// `:` groups right to left: note, then Euclidean rhythm.
		check(
			steps_of(eval("bd@3:8")) == "x..x..x." and
				notes(eval("bd@3:8")) == std::vector(8, 36),
			"bd@3:8"
		);

		// `:` is only Euclidean and groups left to right: `(bd:3):8` is
		// rhythm:scalar, which has no rule.
		check(steps(eval("bd:3")) == std::vector(3, cane::EventKind::Beat), "bd:3");
		check(throws("bd:3:8", "unable to evaluate"), "bd:3:8 is not a map");

		// Transforms after `@` act on the sequence.
		check(steps_of(eval("sd@..!.**2")) == "..x...x.", "sequence **");
		check(steps_of(eval("hh@3:8>>2")) == "x.x..x..", "sequence >>");
		check(steps_of(eval("hh@3:8<<1")) == "..x..x.x", "sequence <<");
		check(steps_of(eval("'(bd@!..)")) == "..x", "sequence reverse");
		check(
			beat_notes(eval("(c2, eb2, g2)@!.!!+12")) == std::vector { 48, 55, 48 },
			"sequence + scalar"
		);
		check(
			beat_notes(eval("c4@!! - 12")) == std::vector { 48, 48 },
			"sequence - scalar"
		);
		check(throws("bd@3:8 - 40", "outside 0-127"), "transpose below 0");
		check(throws("bd@!! ** -1", "cannot repeat"), "negative repeat");

		// Mapping then repeating restarts the melody; repeating the rhythm
		// first continues it.
		check(
			beat_notes(eval("(c4, e4, g4)@!!!!**2")) ==
				std::vector { 60, 64, 67, 60, 60, 64, 67, 60 },
			"map then repeat"
		);
		check(
			beat_notes(eval("(c4, e4, g4)@(!!!!**2)")) ==
				std::vector { 60, 64, 67, 60, 64, 67, 60, 64 },
			"repeat then map"
		);
	}

	catch (const cane::Fatal& e) {
		std::println(stderr, "FAIL: {}", e.what());
		return EXIT_FAILURE;
	}

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
