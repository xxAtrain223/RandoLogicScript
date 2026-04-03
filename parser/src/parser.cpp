#include "parser.h"

#include "builder.h"
#include "grammar.h"

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>

#include <stdexcept>

namespace rls::parser {

rls::ast::File Parse(const std::string& source) {
	tao::pegtl::memory_input in(source, "input");

	auto root = tao::pegtl::parse_tree::parse<
		grammar::rls_file, selector
	>(in);

	if (!root) {
		throw std::runtime_error("Parse failed");
	}

	return buildFile(*root);
}

} // namespace rls::parser
