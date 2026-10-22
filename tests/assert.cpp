#include <cstdlib>

#include <cane/log.hpp>

// A failed `report_if` (cane's assertion) must throw. Returning success means
// it did not.
int main(int, const char*[]) {
	try {
		cane::report_if(true, cane::ReportKind::Internal, "assert");
	}

	catch (const cane::Fatal&) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
