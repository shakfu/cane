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

		// `|` layers: both start at time 0.
		check(
			times(eval("bd@!! | sd@!")) == Times { { 0, 500 }, { 500, 500 }, { 0, 500 } } and
				notes(eval("bd@!! | sd@!")) == std::vector { 36, 36, 38 },
			"layer"
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
