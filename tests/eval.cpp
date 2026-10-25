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

	cane::Session::Result compile(std::string_view src, uint64_t seed = 0) {
		cane::Session session { { .bpm = 120, .channel_bindings = {
			{ "drums", 10 },
		}, .seed = seed } };

		return session.eval(std::string { src });
	}

	cane::Value eval(std::string_view src) {
		return compile(src).value;
	}

	// `src` at cycles 0 to n - 1, as scalars.
	std::vector<cane::Scalar> cycles(std::string_view src, int n, uint64_t seed = 0) {
		auto at = compile(src, seed).at;
		std::vector<cane::Scalar> out;

		for (int i = 0; i < n; ++i) {
			out.push_back(std::get<cane::Scalar>(at(i)));
		}

		return out;
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

	std::string show_steps(const cane::Value& value) {
		std::string out;

		for (auto step: std::get<cane::Rhythm>(value)) {
			out += step == cane::EventKind::Beat ? 'x' : '.';
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
			notes(eval("[60, 62]@!!")) == std::vector { 60, 62 },
			"melody:rhythm"
		);

		// Melody-scalar `-` and `/` once had their operands swapped.
		check(
			std::get<cane::Melody>(eval("[60, 62] - 12")) ==
				cane::Melody { 48, 50 },
			"melody - scalar"
		);
		check(
			std::get<cane::Melody>(eval("[60, 62] / 2")) ==
				cane::Melody { 30, 31 },
			"melody / scalar"
		);

		// `?` is seeded by the cycle: it varies across cycles, and a cycle
		// always repeats its pick. It once always picked one side.
		{
			auto picks = cycles("1 ? 2", 64);
			check(
				std::set(picks.begin(), picks.end()) == std::set<cane::Scalar> { 1, 2 },
				"random choice"
			);
			check(cycles("1 ? 2", 64) == picks, "random choice repeats");
			check(cycles("1 ? 2", 64, 1) != picks, "random choice seed");

			// Two `?` in one cycle take different draws.
			auto pairs = cycles("(1 ? 2) * 10 + (1 ? 2)", 64);
			check(
				std::ranges::any_of(pairs, [](auto x) { return x == 12 or x == 21; }),
				"random choice draws"
			);
		}

		// `cycle` is the cycle number; `value` is cycle 0.
		check(cycles("cycle * 2", 4) == std::vector<cane::Scalar> { 0, 2, 4, 6 }, "cycle");
		check(std::get<cane::Scalar>(eval("cycle")) == 0, "cycle 0");
		check(
			beat_notes(compile("bd@! + cycle").at(2)) == std::vector { 38 },
			"cycle in a sequence"
		);
		check(std::get<cane::Scalar>(eval("cycle = 5; cycle")) == 5, "rebind cycle");


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
			notes(eval("[c4, eb4, g4]@!!!")) == std::vector { 60, 63, 67 },
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
			notes(eval("[bd, sd, hh, oh]@!!!!")) == std::vector { 36, 38, 42, 46 },
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

		// Melodies are `[a, b]`, as in Haskell. `,` only concatenates.
		auto melody = [](std::string_view src) {
			return std::get<cane::Melody>(eval(src));
		};

		check(melody("[60]") == cane::Melody { 60 }, "singleton");
		check(melody("[]").empty(), "empty melody");
		check(melody("[60, 62,]") == cane::Melody { 60, 62 }, "trailing ,");
		check(melody("[1 + 2, 4 * 5]") == cane::Melody { 3, 20 }, "elements");
		check(
			melody("[60, 62], [64]") == cane::Melody { 60, 62, 64 },
			"concatenate melodies"
		);
		check(
			throws("60, 62", "`,` does not take scalar and scalar"),
			"no melody from `,`"
		);
		check(
			throws("m = [1]; [m, 60]", "does not take melody and melody"),
			"melody element"
		);
		check(throws("[60, 62", "expected `]`"), "unclosed melody");
		check(throws("&60", "unknown character '&'"), "no `&`");

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

		// A third `:` part shifts left, as in Tidal: `(3:8):1`.
		check(show_steps(eval("2:8:2")) == "..x...x.", "2:8:2");
		check(show_steps(eval("3:8:1")) == "..x..x.x", "3:8:1");
		check(show_steps(eval("3:8:-1")) == ".x..x..x", "3:8:-1");
		check(show_steps(eval("3:8:9")) == "..x..x.x", "3:8:9 wraps");
		check(show_steps(eval("0:0:1")) == "", "0:0:1");
		check(steps_of(eval("sd@2:8:2")) == "..x...x.", "sd@2:8:2");

		// More hits than steps, or a negative count, is an error. It also
		// catches a note in place of hits: `bd:3:8` is `(36:3):8`.
		check(throws("bd:3:8", "needs 0 <= hits <= steps"), "bd:3:8");
		check(throws("3:-8", "needs 0 <= hits <= steps"), "negative steps");
		check(throws("(-1):8", "needs 0 <= hits <= steps"), "negative hits");
		check(show_steps(eval("8:8")) == "xxxxxxxx", "8:8");

		// Transforms after `@` act on the sequence.
		check(steps_of(eval("sd@..!.**2")) == "..x...x.", "sequence **");
		check(steps_of(eval("hh@3:8>>2")) == "x.x..x..", "sequence >>");
		check(steps_of(eval("hh@3:8<<1")) == "..x..x.x", "sequence <<");
		check(steps_of(eval("'(bd@!..)")) == "..x", "sequence reverse");
		check(
			beat_notes(eval("[c2, eb2, g2]@!.!!+12")) == std::vector { 48, 55, 48 },
			"sequence + scalar"
		);
		check(
			beat_notes(eval("c4@!! - 12")) == std::vector { 48, 48 },
			"sequence - scalar"
		);
		check(throws("bd@3:8 - 40", "outside 0-127"), "transpose below 0");
		check(throws("bd@!! ** -1", "cannot repeat"), "negative repeat");

		// `|` layers: both start at time 0, and each repeats to the LCM of
		// their lengths.
		check(
			times(eval("bd@!! | sd@!")) == Times { { 0, 500 }, { 0, 500 }, { 500, 500 }, { 500, 500 } } and
				notes(eval("bd@!! | sd@!")) == std::vector { 36, 38, 36, 38 },
			"layer"
		);

		// Polymeter: 3 against 4 steps realigns after 12.
		{
			auto v = eval("36@!.. | 38@!...");
			std::vector<long long> kick, snare;

			for (const auto& ev: std::get<cane::Sequence>(v)) {
				if (ev.kind == cane::EventKind::Beat) {
					(ev.note == 36 ? kick : snare).push_back(ev.timestamp.count() / 500'000);
				}
			}

			check(
				std::get<cane::Sequence>(v).size() == 24 and
					cane::length(std::get<cane::Sequence>(v)) == std::chrono::seconds { 6 } and
					kick == std::vector<long long> { 0, 3, 6, 9 } and
					snare == std::vector<long long> { 0, 4, 8 },
				"layer to lcm"
			);
		}

		check(
			cane::length(std::get<cane::Sequence>(eval("36@3:8 | 38@!."))) == std::chrono::seconds { 4 } and
				beat_notes(eval("38@!. | 36@3:8")) == std::vector { 38, 36, 38, 36, 38, 38, 36 },
			"layer divides"
		);
		check(
			beat_notes(eval("36@! | 38@! | 42@!!")) == std::vector { 36, 38, 42, 36, 38, 42 },
			"layer three"
		);
		check(
			throws("36@!!!!!!! | 38@!**64", "loop every 448 steps"),
			"layer over the limit"
		);

		// `,` starts the right side where the left ends, rests included.
		check(
			times(eval("36@!., 38@!!")) == Times { { 0, 500 }, { 500, 500 }, { 1000, 500 }, { 1500, 500 } },
			"concatenate sequences"
		);

		// `scale name degrees`: degrees to semitones, wrapping by octave.
		check(
			std::get<cane::Melody>(eval("scale \"minor\" [0, 2, 4, 7, -1]")) ==
				cane::Melody { 0, 3, 7, 12, -2 },
			"scale"
		);
		check(
			std::get<cane::Melody>(eval("scale \"majPent\" [5, -5, -6]")) ==
				cane::Melody { 12, -12, -15 },
			"scale wraps"
		);
		check(
			std::get<cane::Melody>(eval("scale \"minor\" [0, 2, 4] + C4")) ==
				cane::Melody { 60, 63, 67 },
			"scale with root"
		);
		check(std::get<cane::Scalar>(eval("scale \"major\" 9")) == 16, "scale scalar");
		check(
			cycles("head (scale <\"major\", \"minor\"> [2])", 2) == std::vector<cane::Scalar> { 4, 3 },
			"scale alternate"
		);
		check(throws("scale \"mnior\" [0]", "unknown scale `mnior`; one of: major, minor"), "scale unknown");

		// `chord name` is intervals; `stack` sounds them together.
		check(
			std::get<cane::Melody>(eval("chord \"maj7\" + C4")) == cane::Melody { 60, 64, 67, 71 },
			"chord"
		);
		check(throws("chord \"maj9\"", "unknown chord `maj9`"), "chord unknown");
		check(
			beat_notes(eval("stack (chord \"min\") (C3@!.!)")) == std::vector { 48, 51, 55, 48, 51, 55 } and
				times(eval("stack [0, 7] (C3@!)")) == Times { { 0, 500 }, { 0, 500 } },
			"stack"
		);
		check(throws("stack [] (C3@!)", "`stack` needs at least one interval"), "stack empty");
		check(throws("stack [0, 100] (C3@!)", "outside 0-127"), "stack range");

		// `fit n` rescales a sequence to exactly n steps: polyrhythm.
		{
			auto us = [](const cane::Value& v) {
				std::vector<std::pair<long long, long long>> out;

				for (const auto& ev: std::get<cane::Sequence>(v)) {
					out.emplace_back(ev.timestamp.count(), (ev.timestamp + ev.duration).count());
				}

				return out;
			};

			using Spans = std::vector<std::pair<long long, long long>>;
			check(
				us(eval("36@!!! fit 4")) == Spans { { 0, 666'666 }, { 666'666, 1'333'333 }, { 1'333'333, 2'000'000 } },
				"fit"
			);

			auto poly = eval("36@!!! fit 4 | 38@!!!!");
			check(
				std::get<cane::Sequence>(poly).size() == 7 and
					cane::length(std::get<cane::Sequence>(poly)) == std::chrono::seconds { 2 },
				"fit polyrhythm"
			);

			// A triplet in the middle step.
			check(
				us(eval("36@!, 36@!!! fit 1, 36@!")) == Spans {
					{ 0, 500'000 }, { 500'000, 666'666 }, { 666'666, 833'333 },
					{ 833'333, 1'000'000 }, { 1'000'000, 1'500'000 } },
				"fit subdivision"
			);
			check(
				std::get<cane::Sequence>(eval("36@!! gate 50 fit 1")).front().gate == 50,
				"fit keeps gate"
			);
		}
		check(throws("36@! fit 0", "`fit 0` needs 1 to 256 steps"), "fit 0");
		check(throws("36@! fit 257", "needs 1 to 256 steps"), "fit limit");
		check(throws("!!! fit 4", "`fit` does not take rhythm and scalar"), "fit rhythm");

		// `vel` and `gate` assign one value per event, cycling; rests use one
		// up, as with `@`. A scalar is one value.
		{
			auto field = [](const cane::Value& v, auto member) {
				std::vector<int> out;

				for (const auto& ev: std::get<cane::Sequence>(v)) {
					out.push_back(static_cast<int>(ev.*member));
				}

				return out;
			};

			auto vel = &cane::Event::velocity;
			auto gate = &cane::Event::gate;

			check(field(eval("bd@!.!! vel [100, 60]"), vel) == std::vector { 100, 60, 100, 60 }, "vel melody");
			check(field(eval("bd@!! vel 80"), vel) == std::vector { 80, 80 }, "vel scalar");
			check(field(eval("bd@!!"), vel) == std::vector { 127, 127 }, "vel default");
			check(field(eval("bd@!!! gate [50, 200]"), gate) == std::vector { 50, 200, 50 }, "gate");
			check(field(eval("bd@!!"), gate) == std::vector { 100, 100 }, "gate default");

			// Binds looser than `+`, tighter than `|`.
			check(
				field(eval("[C4, E4]@!! + 12 vel 80 | sd@!"), vel) == std::vector { 80, 127, 80, 127 },
				"vel precedence"
			);
			check(
				field(compile("every 2 (vel 60) (bd@!)").at(1), vel) == std::vector { 127 } and
					field(eval("every 2 (vel 60) (bd@!)"), vel) == std::vector { 60 },
				"vel section"
			);
			check(field(eval("bd@!! vel <100, 60>"), vel) == std::vector { 100, 100 }, "vel alternate");

			// The gate leaves the loop length alone.
			check(
				cane::length(std::get<cane::Sequence>(eval("bd@!!!! gate 25"))) == std::chrono::seconds { 2 },
				"gate keeps length"
			);
		}
		check(throws("bd@! vel 128", "`vel 128` is outside 0-127"), "vel range");
		check(throws("bd@! vel -1", "outside 0-127"), "vel negative");
		check(throws("bd@! gate 0", "`gate 0` is outside 1-10000"), "gate range");
		check(throws("bd@! vel []", "`vel` needs at least one value"), "vel empty");
		check(throws("!.!. vel 80", "`vel` does not take rhythm and scalar"), "vel rhythm");

		// `<a, b>` picks element `n mod len`. The chosen element sees its own
		// count, `n / len`, so a nested one advances only when chosen.
		check(cycles("<1, 2, 3>", 4) == std::vector<cane::Scalar> { 1, 2, 3, 1 }, "alternate");
		check(cycles("<1, <2, 3> >", 4) == std::vector<cane::Scalar> { 1, 2, 1, 3 }, "alternate nested");
		check(cycles("<1, 2,>", 2) == std::vector<cane::Scalar> { 1, 2 }, "alternate trailing comma");
		check(cycles("<cycle, 0>", 6) == std::vector<cane::Scalar> { 0, 0, 1, 0, 2, 0 }, "alternate element cycle");
		{
			auto at = compile("<36, 38>@! | <42@!!, 46@!>").at;
			check(
				beat_notes(at(0)) == std::vector { 36, 42, 36, 42 } and
					beat_notes(at(1)) == std::vector { 38, 46 },
				"alternate sequences"
			);
		}
		check(throws("<1, bd@!>", "`<>` does not take scalar and sequence"), "alternate types");
		check(throws("<>", "needs at least one element"), "alternate empty");
		check(throws("<1, 2", "expected `>`"), "alternate unclosed");

		// `every k f x`: `f x` on cycles 0, k, 2k, ..., as in Tidal.
		check(cycles("every 3 (+ 10) 1", 4) == std::vector<cane::Scalar> { 11, 1, 1, 11 }, "every");
		check(cycles("f x = every 2 (* 2) x; f 5", 2) == std::vector<cane::Scalar> { 10, 5 }, "every in a function");
		{
			auto at = compile("every 2 (>> 1) (bd@!.)").at;
			check(steps_of(at(0)) == ".x" and steps_of(at(1)) == "x.", "every on a sequence");
		}
		check(throws("every 0 (+ 1) 1", "`every 0` needs k > 0"), "every 0");
		check(throws("every 2 (@ !!) 3", "`f x` is sequence but `x` is scalar"), "every types");

		// `sometimes f x` takes a seeded draw, like `?`; only one branch runs,
		// so the other's draws and errors do not happen.
		{
			auto picks = cycles("sometimes (+ 10) 1", 64);
			check(
				std::set(picks.begin(), picks.end()) == std::set<cane::Scalar> { 1, 11 } and
					cycles("sometimes (+ 10) 1", 64) == picks,
				"sometimes"
			);
		}
		// Only the chosen branch is evaluated: the other would fail.
		check(
			beat_notes(eval("<bd@!, bd@! + 200>")) == std::vector { 36 },
			"alternate evaluates one element"
		);
		check(
			beat_notes(eval("every 2 (\\s -> seq bd@!) (bd@! + 200)")) == std::vector { 36 },
			"every evaluates one branch"
		);
		check(throws("bd@! $ sd@!", "unknown character '$'"), "`$` no longer layers");

		// Operand types with no rule once reached the evaluator and failed
		// there as an internal error.
		check(
			throws("!.!. | !..!", "`|` does not take rhythm and rhythm"),
			"layer rhythms"
		);
		check(throws("'5", "`'` does not take a scalar"), "prefix type");
		check(
			throws("-1:8", "`-` does not take a rhythm"), "`-1:8` is `-(1:8)`"
		);

		// Functions: `f x y = body` defines a curried function. Calls bind
		// tightest.
		check(scalar("f x = x + 1; f 3") == 4, "call");
		check(scalar("g x y = (x + y) / 2; g 4 8") == 6, "two parameters");
		check(scalar("f x = x + 1; f 3 + 10") == 14, "call binds tightest");
		check(scalar("f x = x + 1; g x = f (f x); g 1") == 3, "nested call");
		check(scalar("add x = \\y -> num x + y; add 1 2") == 3, "returns lambda");
		check(scalar("g x y = x - y; h = g 10; h 3") == 7, "partial application");
		check(scalar("twice f x = f (f x); inc x = x + 1; twice inc 5") == 7, "higher order");
		check(scalar("x = 100; f x = x * 2; f 3 + x") == 106, "parameter shadows");
		check(
			scalar("k y = (\\x -> num \\y -> num x + y) y; k 1 2") == 3,
			"no capture"
		);
		check(
			steps_of(eval("hit n = bd@n:8; hit 3")) == "x..x..x.", "rhythm argument"
		);
		check(throws("5 3", "`5` is not a function"), "call a number");
		check(throws("f x = x; f 1 2", "is not a function"), "too many arguments");
		check(throws("f 3 = 4", "expected a parameter name"), "number parameter");
		check(throws("f x x = x", "duplicate parameter `x`"), "duplicate parameter");
		check(throws("f x = f x", "unknown binding `f`"), "no recursion");

		// Operator sections, as in Haskell. `(- a)` still negates.
		check(scalar("(+ 1) 3") == 4, "right section");
		check(scalar("(10 -) 3") == 7, "left section");
		check(scalar("(-) 10 3") == 7 and scalar("(+) 1 2") == 3, "bare section");
		check(scalar("(- 3)") == -3, "(- a) negates");

		// `abs` is a builtin function: callable, passable and rebindable.
		check(scalar("abs (-3)") == 3 and scalar("abs (2 - 5) + 1") == 4, "abs");
		check(scalar("twice f x = f (f x); twice abs (-3)") == 3, "pass abs");
		check(scalar("abs = 1; abs") == 1, "rebind abs");
		check(throws("abs \"x\"", "`abs` does not take a string"), "abs type");
		check(
			throws("abs -3", "`-` does not take function and scalar"),
			"abs -3 subtracts, as in Haskell"
		);
		check(throws("+3", "expected a primary"), "no prefix +");

		// `head` and `tail` are builtin functions too.
		// Haskell semantics: `head` returns the element, so it takes only a
		// melody. `take` and `drop` keep the type.
		check(scalar("head [60, 62, 64]") == 60, "head");
		check(scalar("head [c4, e4, g4] + 12") == 72, "head is a scalar");
		check(throws("head !.!.", "`head` does not take a rhythm"), "head rhythm");
		check(
			throws("head (drop 2 [1, 2])", "head of an empty melody"),
			"head of empty"
		);
		check(show_steps(eval("take 4 (3:8)")) == "x..x", "take rhythm");
		check(
			std::get<cane::Melody>(eval("drop 2 [60, 62, 64]")) == cane::Melody { 64 },
			"drop melody"
		);
		check(steps_of(eval("take 2 (bd@3:8)")) == "x.", "take sequence");
		check(
			std::get<cane::Melody>(eval("take (-1) [1, 2]")).empty() and
				std::get<cane::Melody>(eval("drop (-1) [1, 2]")).size() == 2,
			"negative count is 0"
		);
		check(
			std::get<cane::Melody>(eval("twice f x = f (f x); twice tail [1, 2, 3]")) ==
				cane::Melody { 3 },
			"pass tail"
		);
		check(scalar("head = 1; head") == 1, "rebind head");
		check(
			throws("head 60, 62", "`head` does not take a scalar"),
			"head binds as a call"
		);
		check(scalar("(+ 1 * 2) 3") == 5, "section operand binds tighter");
		check(scalar("(1 + 2 +) 3") == 6, "left section of an expression");
		check(
			scalar("twice f x = f (f x); twice (* 2) 3") == 12, "section argument"
		);
		check(steps_of(eval("(@ 3:8) bd")) == "x..x..x.", "map section");
		check(
			scalar("x = 5; (x -) 1 + (- x)") == -1, "section names do not capture"
		);
		check(throws("(1 + 2 *)", "expected a primary"), "nested left section");
		check(throws("(* 1 + 2)", "expected `)`"), "loose section operand");
		check(throws("(;)", "expected a primary"), "`;` is not sectionable");

		// Mapping then repeating restarts the melody; repeating the rhythm
		// first continues it.
		check(
			beat_notes(eval("[c4, e4, g4]@!!!!**2")) ==
				std::vector { 60, 64, 67, 60, 60, 64, 67, 60 },
			"map then repeat"
		);
		check(
			beat_notes(eval("[c4, e4, g4]@(!!!!**2)")) ==
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
