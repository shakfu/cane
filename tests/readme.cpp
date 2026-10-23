#include <cstdlib>
#include <fstream>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <cane/cane.hpp>

// Keeps README.md in sync with the language. Checks that:
// - each cane code block evaluates to a pattern, both as a file and typed
//   into the REPL line by line,
// - the `cane>` transcript runs as one REPL session,
// - each table row whose first two cells are code, e.g. | `~!...` | `.xxx` |,
//   evaluates to the second cell.
// Usage: readme README.md

namespace {
	int failures = 0;

	cane::Session make_session() {
		return cane::Session { { .bpm = 120, .channel_bindings = {
			{ "drums", 10 },
			{ "303", 12 },
		}, } };
	}

	// Rhythms as `x`/`.` steps, melodies as `a, b`, scalars as numbers,
	// sequences as one note or `.` per event, separated by spaces.
	std::string show(const cane::Value& value) {
		std::string out;

		if (const auto* seq = std::get_if<cane::Sequence>(&value)) {
			for (const auto& ev: *seq) {
				out += out.empty() ? "" : " ";
				out += ev.kind == cane::EventKind::Beat ?
					std::to_string(ev.note) :
					".";
			}
		}

		if (const auto* rhythm = std::get_if<cane::Rhythm>(&value)) {
			for (auto step: *rhythm) {
				out += step == cane::EventKind::Beat ? 'x' : '.';
			}
		}

		else if (const auto* melody = std::get_if<cane::Melody>(&value)) {
			for (auto note: *melody) {
				out += (out.empty() ? "" : ", ") + std::to_string(note);
			}

			out = "[" + out + "]";
		}

		else if (const auto* scalar = std::get_if<cane::Scalar>(&value)) {
			out = std::to_string(*scalar);
		}

		return out;
	}

	void fail(std::string_view what, std::string_view src) {
		std::println(stderr, "FAIL: {}:\n{}", what, src);
		++failures;
	}

	void check_block(const std::string& src) {
		try {
			auto session = make_session();

			if (session.eval(src).type != cane::TypeKind::Pattern) {
				fail("not a pattern", src);
			}
		}

		catch (const cane::Fatal& e) {
			fail(e.what(), src);
		}
	}

	void check_transcript(const std::vector<std::string>& lines) {
		auto session = make_session();

		for (const auto& line: lines) {
			try {
				CANE_UNUSED(session.eval(line));
			}

			catch (const cane::Fatal& e) {
				fail(e.what(), line);
			}
		}
	}

	// Returns the code spans of a table row's first two cells, if both are code.
	bool table_row(std::string_view line, std::string& expr, std::string& want) {
		std::vector<std::string_view> cells;

		for (size_t i = 1; i < line.size();) {
			size_t j = line.find(" | ", i);
			cells.push_back(line.substr(i, j - i));

			if (j == std::string_view::npos) {
				break;
			}

			i = j + 3;
		}

		auto code = [](std::string_view cell, std::string& out) {
			while (cell.starts_with(' ')) {
				cell.remove_prefix(1);
			}

			// The last cell keeps the row's closing ` |`.
			while (cell.ends_with(' ') or cell.ends_with('|')) {
				cell.remove_suffix(1);
			}

			if (cell.size() < 2 or not cell.starts_with('`') or
				cell.find('`', 1) != cell.size() - 1) {
				return false;
			}

			out = cell.substr(1, cell.size() - 2);
			return true;
		};

		return line.starts_with("| ") and cells.size() >= 2 and
			code(cells[0], expr) and code(cells[1], want);
	}
}  // namespace

int main(int argc, const char* argv[]) {
	if (argc != 2) {
		std::println(stderr, "usage: readme README.md");
		return EXIT_FAILURE;
	}

	std::ifstream file { argv[1] };

	if (not file) {
		std::println(stderr, "cannot open '{}'", argv[1]);
		return EXIT_FAILURE;
	}

	std::vector<std::string> lines;

	for (std::string line; std::getline(file, line);) {
		lines.push_back(line);
	}

	size_t blocks = 0;
	size_t rows = 0;

	for (size_t i = 0; i < lines.size(); ++i) {
		if (std::string expr, want; table_row(lines[i], expr, want)) {
			++rows;

			try {
				auto session = make_session();
				auto got = show(session.eval(expr).value);

				if (got != want) {
					fail(std::format("got `{}`, want `{}`", got, want), expr);
				}
			}

			catch (const cane::Fatal& e) {
				fail(e.what(), expr);
			}

			continue;
		}

		if (not lines[i].starts_with("```")) {
			continue;
		}

		auto lang = lines[i].substr(3);
		std::vector<std::string> body;

		while (++i < lines.size() and not lines[i].starts_with("```")) {
			body.push_back(lines[i]);
		}

		if (not lang.empty() or body.empty() or body[0].starts_with("step") or
			body[0].starts_with("time")) {
			continue;
		}

		++blocks;

		if (body[0].starts_with("cane> ")) {
			for (auto& line: body) {
				line = line.substr(6);
			}

			check_transcript(body);
			continue;
		}

		std::ostringstream src;

		for (const auto& line: body) {
			src << line << '\n';
		}

		check_block(src.str());
		check_transcript(body);
	}

	std::println("{} code blocks, {} table rows checked", blocks, rows);

	if (blocks == 0 or rows == 0) {
		std::println(stderr, "FAIL: nothing found to check");
		return EXIT_FAILURE;
	}

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
