#include "parser.h"

namespace rls::parser {

rls::AstNode Parse(const std::string& source) {
	rls::AstNode node;
	node.name = source.empty() ? "empty" : source;
	return node;
}

} // namespace rls::parser
