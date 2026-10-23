#ifndef CANE_PARSE_HPP
#define CANE_PARSE_HPP

#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

#include <cane/def.hpp>
#include <cane/log.hpp>
#include <cane/util.hpp>
#include <cane/lex.hpp>

namespace cane {

	///////////////
	// Utilities //
	///////////////

	template <typename T>
	inline std::shared_ptr<T> deepcopy(
		std::shared_ptr<T> root,

		std::shared_ptr<T> T::* lhs,
		std::shared_ptr<T> T::* rhs
	) {
		if (root == nullptr) {
			return nullptr;
		}

		auto ptr = std::make_shared<T>(*root);

		(*ptr).*lhs = deepcopy(root->lhs, lhs, rhs);
		(*ptr).*rhs = deepcopy(root->rhs, lhs, rhs);

		return ptr;
	}

	//////////
	// Type //
	//////////

	struct Type {
		TypeKind kind;

		std::shared_ptr<Type> lhs;
		std::shared_ptr<Type> rhs;

		Type(
			TypeKind kind_,

			std::shared_ptr<Type> lhs_,
			std::shared_ptr<Type> rhs_
		):
				kind(kind_), lhs(lhs_), rhs(rhs_) {}

		Type(TypeKind kind_): kind(kind_), lhs(nullptr), rhs(nullptr) {}

		Type(TypeKind kind_, std::shared_ptr<Type> type):
				kind(kind_), lhs(type), rhs(nullptr) {}
	};

	/////////
	// AST //
	/////////

	struct Node {
		SymbolKind kind;
		SymbolKind op;

		std::string_view sv;

		TypeKind type;

		std::shared_ptr<Node> lhs;
		std::shared_ptr<Node> rhs;

		Node(const Node& n) = default;

		Node(
			SymbolKind kind_,
			SymbolKind op_,

			std::string_view sv_,

			TypeKind type_,

			std::shared_ptr<Node> lhs_,
			std::shared_ptr<Node> rhs_
		):
				kind(kind_),
				op(op_),
				sv(sv_),
				type(type_),
				lhs(lhs_),
				rhs(rhs_) {}

		// No child nodes
		Node(SymbolKind kind_, std::string_view sv_, TypeKind type_):
				kind(kind_),
				op(SymbolKind::None),
				sv(sv_),
				type(type_),
				lhs(nullptr),
				rhs(nullptr) {}

		// Binary
		Node(
			SymbolKind kind_,
			std::string_view sv_,

			TypeKind type_,

			std::shared_ptr<Node> lhs_,
			std::shared_ptr<Node> rhs_
		):
				kind(kind_),
				op(SymbolKind::None),
				sv(sv_),
				type(type_),
				lhs(lhs_),
				rhs(rhs_) {}

		// Unary
		Node(
			SymbolKind kind_,
			std::string_view sv_,

			TypeKind type_,

			std::shared_ptr<Node> node
		):
				kind(kind_),
				op(SymbolKind::None),
				sv(sv_),
				type(type_),
				lhs(node),
				rhs(nullptr) {}
	};

	inline std::ostream& operator<<(std::ostream& os, Node n) {
		os << "{ ";
		os << "kind: " << n.kind << ", ";
		os << "op: " << n.op << ", ";
		os << "sv: '" << n.sv << "', ";
		os << "type: " << n.type << ", ";
		os << "lhs: " << static_cast<void*>(n.lhs.get()) << ", ";
		os << "rhs: " << static_cast<void*>(n.rhs.get());
		os << " }";
		return os;
	}
}  // namespace cane

CANE_FORMATTER_DEF(cane::Node);

namespace cane {

	////////////
	// PARSER //
	////////////

	using BoxNode = std::shared_ptr<Node>;
	using OptionalBoxNode = std::optional<BoxNode>;

	class Parser {
		private:
		Lexer lx;

		public:
		Parser(std::string_view sv): lx(sv) {}

		//////////////////////
		// PARSER FUNCTIONS //
		//////////////////////

		// Core parsing functions
		[[nodiscard]] OptionalBoxNode parse() {
			// CANE_FUNC();

			if (lx.peek_is_kind(SymbolKind::EndFile)) {
				return std::nullopt;
			}

			auto expr = parse_expression();

			if (not lx.peek_is_kind(SymbolKind::EndFile)) {
				cane::report(
					ReportKind::Syntactical,
					"unexpected token `{}`",
					lx.peek().sv
				);
			}

			return expr;
		}

		std::optional<TypeKind> type_annotation() {
			// TODO: Parse nested types into a type tree
			// CANE_FUNC();

			if (lx.discard_if_kind(SymbolKind::TypeNumber)) {
				return TypeKind::Scalar;
			}

			else if (lx.discard_if_kind(SymbolKind::TypeString)) {
				return TypeKind::String;
			}

			else if (lx.discard_if_kind(SymbolKind::TypeRhythm)) {
				return TypeKind::Rhythm;
			}

			else if (lx.discard_if_kind(SymbolKind::TypeMelody)) {
				return TypeKind::Melody;
			}

			else if (lx.discard_if_kind(SymbolKind::TypeSequence)) {
				return TypeKind::Sequence;
			}

			else if (lx.discard_if_kind(SymbolKind::TypePattern)) {
				return TypeKind::Pattern;
			}

			return std::nullopt;
		}

		std::optional<TypeKind> type() {
			// CANE_FUNC();

			if (not lx.discard_if_kind(SymbolKind::Arrow)) {
				return std::nullopt;
			}

			return type_annotation();
		}

		[[nodiscard]] OptionalBoxNode parse_primary() {
			// CANE_FUNC();

			Symbol symbol = lx.peek();
			auto bp = binding_power(symbol.kind);

			if (not bp.has_value()) {
				return std::nullopt;
			}

			switch (symbol.kind) {
				// Literals
				case SymbolKind::Identifier: {
					lx.discard();
					return std::make_shared<Node>(
						SymbolKind::Identifier, symbol.sv, TypeKind::None
					);
				}

				case SymbolKind::String: {
					lx.discard();
					return std::make_shared<Node>(
						symbol.kind, symbol.sv, TypeKind::String
					);
				}

				case SymbolKind::Number: {
					lx.discard();
					return std::make_shared<Node>(
						symbol.kind, symbol.sv, TypeKind::Scalar
					);
				}

				// Literals (Implicit Concat)
				case SymbolKind::Beat:
				case SymbolKind::Rest: {
					lx.discard();

					auto root = std::make_shared<Node>(
						symbol.kind, symbol.sv, TypeKind::Rhythm
					);

					while (lx.peek_is_kind(SymbolKind::Beat) or
						   lx.peek_is_kind(SymbolKind::Rest)) {
						auto beat = lx.take();

						auto rhythm = std::make_shared<Node>(
							beat.kind, beat.sv, TypeKind::Rhythm
						);

						root = std::make_shared<Node>(
							SymbolKind::Concatenate,
							beat.sv,
							TypeKind::Rhythm,
							root,
							rhythm
						);
					}

					return root;
				} break;

				case SymbolKind::LeftParen: {
					lx.discard();  // Skip `(`

					// `(op a)` and `(op)` are sections. `(- a)` negates, as in
					// Haskell.
					auto op = lx.peek();
					auto after = lx;
					after.discard();

					bool bare = after.peek_is_kind(SymbolKind::RightParen);

					if (is_sectionable(op.kind) and
						(bare or op.kind != SymbolKind::Sub)) {
						lx.discard();

						auto rhs = bare ?
							nullptr :
							parse_expression(binding_power(op.kind)->right);

						if (not lx.discard_if_kind(SymbolKind::RightParen)) {
							cane::report(ReportKind::Syntactical, "expected `)`");
						}

						return section(op, nullptr, rhs);
					}

					auto expr = parse_expression(0);

					if (not lx.discard_if_kind(SymbolKind::RightParen)) {
						cane::report(ReportKind::Syntactical, "expected `)`");
					}

					return expr;
				} break;

				// `[a, b]` is a melody. Elements bind tighter than `,`, which
				// separates them. A trailing `,` is allowed.
				case SymbolKind::LeftBracket: {
					lx.discard();  // Skip `[`

					BoxNode melody = std::make_shared<Node>(
						SymbolKind::Nil, symbol.sv, TypeKind::Melody
					);

					auto element_bp =
						binding_power(SymbolKind::Concatenate)->right;

					while (not lx.peek_is_kind(SymbolKind::RightBracket)) {
						melody = std::make_shared<Node>(
							SymbolKind::Append,
							symbol.sv,
							TypeKind::None,
							melody,
							parse_expression(element_bp)
						);

						if (not lx.discard_if_kind(SymbolKind::Concatenate)) {
							break;
						}
					}

					if (not lx.discard_if_kind(SymbolKind::RightBracket)) {
						cane::report(ReportKind::Syntactical, "expected `]`");
					}

					return melody;
				} break;

				case SymbolKind::Function: {
					lx.discard();  // Skip `\`

					// Parameter
					auto identifier =
						lx.take_if_kind_opt(SymbolKind::Identifier);
					cane::report_if(
						not identifier.has_value(),
						ReportKind::Syntactical,
						"expected an identifier"
					);

					auto param = std::make_shared<Node>(
						SymbolKind::Identifier,
						identifier.value().sv,
						TypeKind::None
					);

					auto param_type = type();
					if (not param_type.has_value()) {
						cane::report(
							ReportKind::Lexical, "expected a type annotation"
						);
					}

					// Reset binding power and parse body
					auto body = parse_expression(bp.value().right);

					return std::make_shared<Node>(
						SymbolKind::Function,
						symbol.sv,
						TypeKind::Function,
						param,
						body
					);
				} break;

				default: break;
			}

			return std::nullopt;
		}

		[[nodiscard]] OptionalBoxNode parse_prefix() {
			// CANE_FUNC();

			Symbol symbol = lx.peek();
			auto bp = binding_power(symbol.kind);

			if (not bp.has_value()) {
				return std::nullopt;
			}

			// Remapping
			switch (symbol.kind) {
				case SymbolKind::Sub: symbol.kind = SymbolKind::Neg; break;
				default: break;
			}

			if (not is_opfix(OpfixKind::Prefix, symbol.kind)) {
				return std::nullopt;
			}

			lx.discard();
			auto expr = parse_expression(bp.value().right);

			return std::make_shared<Node>(
				symbol.kind, symbol.sv, TypeKind::None, expr
			);
		}

		static bool is_sectionable(SymbolKind kind) {
			return is_opfix(OpfixKind::Infix, kind) and
				kind != SymbolKind::Block and kind != SymbolKind::Assign and
				kind != SymbolKind::Call;
		}

		// `lhs op rhs` as a function of whichever side is null. The lexer
		// cannot produce these parameter names, so they capture no user name.
		static BoxNode section(Symbol op, BoxNode lhs, BoxNode rhs) {
			auto param = [](std::string_view sv) {
				return std::make_shared<Node>(
					SymbolKind::Identifier, sv, TypeKind::None
				);
			};

			auto function = [&](std::string_view sv, BoxNode body) {
				return std::make_shared<Node>(
					SymbolKind::Function, op.sv, TypeKind::Function, param(sv), body
				);
			};

			BoxNode body = std::make_shared<Node>(
				op.kind,
				op.sv,
				TypeKind::None,
				lhs == nullptr ? param("%lhs") : lhs,
				rhs == nullptr ? param("%rhs") : rhs
			);

			if (rhs == nullptr) {
				body = function("%rhs", body);
			}

			if (lhs == nullptr) {
				body = function("%lhs", body);
			}

			return body;
		}

		[[nodiscard]] OptionalBoxNode parse_infix(BoxNode lhs, size_t min_bp) {
			// CANE_FUNC();

			Symbol symbol = lx.peek();
			auto bp = binding_power(symbol.kind);

			if (not bp.has_value()) {
				return std::nullopt;
			}

			if (not is_opfix(OpfixKind::Infix, symbol.kind)) {
				return std::nullopt;
			}

			// `f x y = body` defines `f = \x -> \y -> body`. The parameters
			// parsed as calls: `(f x) y`.
			std::vector<BoxNode> params;

			if (symbol.kind == SymbolKind::Assign) {
				while (lhs->kind == SymbolKind::Call) {
					cane::report_if(
						lhs->rhs->kind != SymbolKind::Identifier,
						ReportKind::Syntactical,
						"expected a parameter name, not `{}`",
						lhs->rhs->sv
					);

					cane::report_if(
						std::ranges::contains(params, lhs->rhs->sv, &Node::sv),
						ReportKind::Syntactical,
						"duplicate parameter `{}`",
						lhs->rhs->sv
					);

					params.push_back(lhs->rhs);
					lhs = lhs->lhs;
				}

				cane::report_if(
					lhs->kind != SymbolKind::Identifier,
					ReportKind::Syntactical,
					"expected an identifier before `=`"
				);
			}

			lx.discard();

			// `(a op)` is a section only as the whole of the parentheses, so
			// `(1 + 2 *)` is an error.
			if (min_bp == 0 and is_sectionable(symbol.kind) and
				lx.peek_is_kind(SymbolKind::RightParen)) {
				return section(symbol, lhs, nullptr);
			}

			// A trailing `;` is allowed, so a line copied from a file also
			// works in the REPL.
			if (symbol.kind == SymbolKind::Block and
				lx.peek_is_kind(SymbolKind::EndFile)) {
				return lhs;
			}

			auto rhs = parse_expression(bp.value().right);

			// `params` is last to first, so the first parameter wraps last.
			for (const auto& param: params) {
				rhs = std::make_shared<Node>(
					SymbolKind::Function, symbol.sv, TypeKind::Function, param, rhs
				);
			}

			return std::make_shared<Node>(
				symbol.kind, symbol.sv, TypeKind::None, lhs, rhs
			);
		}

		[[nodiscard]] OptionalBoxNode parse_postfix(BoxNode lhs) {
			// CANE_FUNC();

			Symbol symbol = lx.peek();

			if (not is_opfix(OpfixKind::Postfix, symbol.kind)) {
				return std::nullopt;
			}

			lx.discard();

			return std::make_shared<Node>(
				symbol.kind, symbol.sv, TypeKind::None, lhs
			);
		}

		[[nodiscard]] BoxNode parse_expression(size_t min_bp = 0) {
			// CANE_FUNC();

			OptionalBoxNode node =
				parse_primary().or_else([&] { return parse_prefix(); });

			cane::report_if(
				not node.has_value(),
				ReportKind::Syntactical,
				"expected a primary expression or a prefix operator"

			);

			constexpr auto is_call = [](SymbolKind kind) {
				return is_opfix(OpfixKind::Literal, kind) or
					is_opfix(OpfixKind::Primary, kind) or
					kind == SymbolKind::Call;
			};

			while (true) {
				auto [kind, sv] = lx.peek();

				if (not(is_opfix(OpfixKind::Infix, kind) or
						is_opfix(OpfixKind::Postfix, kind) or is_call(kind))) {
					break;
				}

				// Remap to a call operator if the next symbol is a
				// primary expression
				kind = is_call(kind) ? SymbolKind::Call : kind;

				auto bp = binding_power(kind);

				if (not bp.has_value() or bp.value().left < min_bp) {
					break;
				}

				// Parse a function call.
				if (is_call(kind)) {
					auto rhs = parse_expression(bp.value().right);

					node = std::make_shared<Node>(
						SymbolKind::Call, sv, TypeKind::None, node.value(), rhs
					);

					continue;
				}

				// Parse everything else.
				node = parse_postfix(node.value()).or_else([&] {
					return parse_infix(node.value(), min_bp);
				});

				cane::report_if(
					not node.has_value(),
					ReportKind::Syntactical,
					"expected an infix or postfix operator"
				);
			}

			return node.value();
		}
	};

}  // namespace cane

#endif
