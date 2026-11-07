#include "tracker_validation.h"

#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "tracker_roots.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

bool sameType(const ValueType& left, const ValueType& right) {
	// Shared runtime storage does not make distinct source types or enum identities interchangeable.
	return left.sourceType == right.sourceType && left.enumInfo == right.enumInfo &&
		left.representation == right.representation;
}

bool validTarget(IntegralTarget target) {
	switch (target) {
	case IntegralTarget::Bool:
	case IntegralTarget::Int8:
	case IntegralTarget::Int16:
	case IntegralTarget::Int32:
	case IntegralTarget::UInt8:
	case IntegralTarget::UInt16:
	case IntegralTarget::UInt32: return true;
	}

	return false;
}

bool validType(const ast::Project& project, const ValueType& type) {
	if (type.sourceType == ast::Type::Condition) {
		return !type.enumInfo && std::holds_alternative<ConditionTarget>(type.representation);
	}

	const auto* target = std::get_if<IntegralTarget>(&type.representation);
	if (!target || !validTarget(*target)) return false;

	if (type.sourceType == ast::Type::Enum) {
		return type.enumInfo && project.getEnumInfo(type.enumInfo->name.text) == type.enumInfo &&
			*target == IntegralTarget::Int32;
	}

	if (type.enumInfo) return false;

	switch (type.sourceType) {
	case ast::Type::Bool: return *target == IntegralTarget::Bool;
	// Explicit conversion nodes may use narrower or unsigned consumer integer targets.
	case ast::Type::Int: return *target != IntegralTarget::Bool;
	case ast::Type::Region:
	case ast::Type::Event:
	case ast::Type::Location: return *target == IntegralTarget::Int32;
	default: return false;
	}
}

std::optional<ast::Diagnostic> checkChainSources(const ast::Project& project,
	const Graph& graph, const Node& node, const std::vector<NodeHandle>& children,
	ast::BinaryOp operation, IntegralTarget target)
{
	const auto fail = [&](std::string message) -> std::optional<ast::Diagnostic> {
		return ast::Diagnostic{"", node.provenance.span, ast::DiagnosticLevel::Error,
			"Tracker chain " + std::move(message)};
	};
	if (node.provenance.expressions.empty() || !node.provenance.expressions.front()) {
		return fail("is missing source operator provenance.");
	}

	const auto& expression = *node.provenance.expressions.front();
	const auto* binary = std::get_if<ast::BinaryExpr>(&expression.node);
	if (!binary || binary->op != operation) {
		return fail("operator differs from its source expression.");
	}
	const auto collected = CollectChainSources(project, expression);
	if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&collected)) return *diagnostic;
	const auto& sources = std::get<ChainSources>(collected);
	if (sources.provenance.expressions != node.provenance.expressions) {
		return fail("flattened operator provenance crosses a semantic barrier or omits an operator.");
	}
	if (sources.operands.size() != children.size()) {
		return fail("operand count violates source grouping or flattening boundaries.");
	}

	for (size_t i = 0; i < children.size(); ++i) {
		const auto& child = graph.nodes[children[i].slot];
		const auto* operand = sources.operands[i];
		if (child.provenance.expressions.empty() || child.provenance.expressions.front() != operand) {
			return fail("operand order/provenance differs from its source expression.");
		}

		const auto analyzed = GetValueType(project, *operand);
		if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&analyzed)) return *diagnostic;
		const auto& sourceType = std::get<ValueType>(analyzed);
		if (sourceType.representation != Representation{target}) {
			// Only the operand's normalization may be transparent here. Never walk into
			// calls or nested operators to find a replacement operand.
			const auto* conversion = std::get_if<Conversion>(&child.payload);
			if (!conversion || conversion->target != target) {
				return fail("operand is missing its source normalization conversion.");
			}
			const auto& converted = graph.nodes[conversion->operand.slot];
			if (converted.provenance.expressions.empty() ||
				converted.provenance.expressions.front() != operand ||
				!converted.valueType || !sameType(*converted.valueType, sourceType)) {
				return fail("conversion bypasses its source operand boundary.");
			}
		}
	}
	return std::nullopt;
}

} // namespace

std::variant<GraphStructure, std::vector<ast::Diagnostic>> PreflightGraph(
	const ast::Project& project, const Graph& graph,
	const std::filesystem::path& sourceRoot, std::span<const HostEnumBinding> customEnums)
{
	// Establish valid handles and required declaration pointers before dereferencing below.
	auto structure = InspectGraph(graph, sourceRoot);
	if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&structure)) return *errors;
	std::vector<ast::Diagnostic> diagnostics;

	const auto error = [&](const ast::Span& span, std::string message) {
		diagnostics.push_back({"", span, ast::DiagnosticLevel::Error, std::move(message)});
	};

	const auto checkAnalyzed = [&](const ValueTypeResult& result,
		const std::optional<ValueType>& actual, const ast::Span& span, const std::string& label) {
		if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&result)) {
			diagnostics.push_back(*diagnostic);
		} else if (!actual || !sameType(std::get<ValueType>(result), *actual)) {
			error(span, "Tracker " + label + " does not match its analyzed type.");
		}
	};

	std::unordered_set<const ast::EnumInfo*> checkedEnums;
	const auto checkEnum = [&](const ValueType& type) {
		if (!validType(project, type) || !type.enumInfo || !checkedEnums.insert(type.enumInfo).second) return;
		const auto& info = *type.enumInfo;
		const auto label = "Tracker enum '" + info.name.text + "'";
		if (const auto host = GetHostEnumType(info, customEnums); !host || host->empty()) {
			error(info.span, label + " is missing a supported host type mapping.");
		}
		if (info.underlyingType != ast::Type::Int ||
			(info.kind != ast::EnumKind::Normal && info.kind != ast::EnumKind::Extern)) {
			error(info.span, label + " has unsupported analyzed enum metadata.");
			return;
		}

		// Sema owns numbering. Missing normalized values are errors, never an invitation
		// to recompute ordinals. Extern entries without values remain symbolic host keys.
		for (const auto& entry : info.entries) {
			const auto* member = std::get_if<ast::EnumMemberInfo>(&entry);
			if (!member) {
				if (info.kind == ast::EnumKind::Normal)
					error(info.span, label + " contains an unsupported normalized wildcard entry.");
				continue;
			}
			if (!member->value) {
				if (info.kind == ast::EnumKind::Normal)
					error(member->span, label + " member '" + member->name.text + "' is missing its normalized value.");
				continue;
			}
			const auto checked = CheckInt32Value(*member->value, member->span);
			if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&checked)) {
				error(member->span, label + " member '" + member->name.text + "': " + diagnostic->message);
			}
		}
	};

	// Inspect each descriptor once instead of recursively expanding helper bodies.
	// This includes skipped/unreachable nodes and terminates for cyclic helper references.
	for (const auto& node : graph.nodes) {
		const auto& span = node.provenance.span;
		const auto fail = [&](std::string message) { error(span, "Tracker " + std::move(message)); };

		for (const auto* expression : node.provenance.expressions) {
			if (!expression) {
				fail("node has a missing source expression.");
				continue;
			}

			const auto analyzed = GetValueType(project, *expression);
			if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&analyzed)) {
				diagnostics.push_back(*diagnostic);
			}

			if (const auto* call = std::get_if<ast::CallExpr>(&expression->node)) {
				const auto* arguments = project.getResolvedCallArgs(call);
				if (!arguments) {
					fail("call '" + call->callee.text + "' is missing analyzed argument metadata.");
				} else {
					for (const auto* argument : *arguments) {
						if (!argument) fail("call '" + call->callee.text + "' has a missing analyzed argument.");
					}
				}
			}
		}

		// Arms and pattern lists organize match evaluation but do not produce runtime values.
		const bool structural = std::holds_alternative<MatchArm>(node.payload) ||
			std::holds_alternative<MatchPatterns>(node.payload);
		if (structural ? node.valueType.has_value() :
			(!node.valueType || !validType(project, *node.valueType))) {
			fail("node has a missing or unsupported payload value type/representation.");
		}
		if (node.valueType) checkEnum(*node.valueType);

		const auto typeOf = [&](NodeHandle handle) -> const std::optional<ValueType>& {
			return graph.nodes[handle.slot].valueType;
		};

		// Operand boundaries check storage representation; declaration boundaries use sameType.
		const auto require = [&](const std::optional<ValueType>& type, const Representation& expected,
			std::string_view label) {
			if (!type || type->representation != expected) {
				fail(std::string(label) + " has an incompatible representation (required coercion missing).");
			}
		};

		const auto integral = [&](NodeHandle handle) {
			const auto& type = typeOf(handle);
			if (!type || !std::holds_alternative<IntegralTarget>(type->representation)) {
				fail("operand must be an integral value, not a Condition or structural node.");
			}
		};

		const auto result = [&](const Representation& expected) {
			require(node.valueType, expected, "node result");
		};

		const auto checkCall = [&](const auto& declaration, const std::vector<NodeHandle>& parameters) {
			if (parameters.size() != declaration.params.size()) {
				fail("call '" + declaration.name.text + "' has an incomplete parameter list.");
				return;
			}

			for (size_t i = 0; i < parameters.size(); ++i) {
				const auto& argument = graph.nodes[parameters[i].slot];
				const auto* wrapper = std::get_if<Parameter>(&argument.payload);
				if (!wrapper || wrapper->position != i || wrapper->declaration != &declaration.params[i]) {
					fail("call '" + declaration.name.text + "' requires ordered declaration-specific parameter wrappers.");
				}

				checkAnalyzed(GetValueType(project, declaration.params[i]), argument.valueType,
					span, "call '" + declaration.name.text + "' parameter '" + declaration.params[i].name.text + "'");
			}

			// Sema already ordered named arguments and inserted defaults. Verify those exact
			// expression pointers rather than resolving or binding the call a second time.
			for (const auto* expression : node.provenance.expressions) {
				if (!expression) continue;
				const auto* call = std::get_if<ast::CallExpr>(&expression->node);
				if (!call) continue; // Bare function references have no CallExpr.

				const auto* resolved = project.getResolvedCallArgs(call);
				if (call->callee.text != declaration.name.text || !resolved || resolved->size() != parameters.size()) {
					fail("call '" + declaration.name.text + "' differs from its analyzed argument metadata.");
					continue;
				}

				for (size_t i = 0; i < parameters.size(); ++i) {
					const auto& provenance = graph.nodes[parameters[i].slot].provenance.expressions;
					if (provenance.size() != 1 || provenance[0] != (*resolved)[i]) {
						fail("call '" + declaration.name.text + "' parameter provenance differs from its analyzed argument.");
					}
				}

				checkAnalyzed(GetValueType(project, *expression), node.valueType, span,
					"call '" + declaration.name.text + "' result");
			}
		};

		std::visit([&](const auto& payload) {
			using T = std::decay_t<decltype(payload)>;
			if constexpr (std::is_same_v<T, Value>) {
				std::visit([&](const auto& value) {
					using V = std::decay_t<decltype(value)>;
					if constexpr (std::is_same_v<V, bool>) {
						result(IntegralTarget::Bool);
						if (node.valueType && node.valueType->sourceType != ast::Type::Bool)
							fail("Bool literal has an incompatible source type.");
					} else if constexpr (std::is_same_v<V, int32_t>) {
						result(IntegralTarget::Int32);
						if (node.valueType && node.valueType->sourceType != ast::Type::Int)
							fail("Int literal has an incompatible source type.");
					} else if constexpr (std::is_same_v<V, EnumConstant>) {
						if (!value.declaration || !value.member || !node.valueType ||
							node.valueType->sourceType != ast::Type::Enum ||
							node.valueType->enumInfo != value.declaration) {
							fail("enum constant is missing its analyzed declaration/member identity.");
						} else if (value.declaration->kind == ast::EnumKind::Normal) {
							const auto& entries = value.declaration->entries;
							const bool present = std::any_of(entries.begin(), entries.end(), [&](const ast::EnumEntryInfo& entry) {
								const auto* member = std::get_if<ast::EnumMemberInfo>(&entry);
								return member && member->name.text == value.member->text;
							});
							if (!present) fail("enum constant '" + value.member->text + "' is absent from its analyzed enum.");
						}
					} else if constexpr (std::is_same_v<V, KeyConstant>) {
						if (!value.name || !node.valueType ||
							(node.valueType->sourceType != ast::Type::Region &&
							 node.valueType->sourceType != ast::Type::Event &&
							 node.valueType->sourceType != ast::Type::Location)) {
							fail("key constant is missing its name or key type.");
						}
					} else if constexpr (std::is_same_v<V, ParameterRead>) {
						if (!value.declaration) fail("parameter read is missing its declaration.");
						else checkAnalyzed(GetValueType(project, *value.declaration), node.valueType,
							span, "parameter read '" + value.declaration->name.text + "'");
					} else if constexpr (std::is_same_v<V, HereValue>) {
						if (!value.region || !project.RegionDecls.contains(value.region->key.text) ||
							project.RegionDecls.at(value.region->key.text) != value.region ||
							!node.valueType || node.valueType->sourceType != ast::Type::Region) {
							fail("here value is missing its analyzed region identity.");
						}
					}
				}, payload.value);

				for (const auto* expression : node.provenance.expressions) {
					if (!expression) continue;
					if (const auto* literal = std::get_if<ast::IntLiteral>(&expression->node)) {
						const auto checked = CheckInt32Value(literal->value, expression->span);
						if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&checked)) {
							diagnostics.push_back(*diagnostic);
						} else {
							const auto* actual = std::get_if<int32_t>(&payload.value);
							if (!actual || *actual != std::get<int32_t>(checked)) fail("Int literal differs from its source value.");
						}
					} else if (const auto* literal = std::get_if<ast::BoolLiteral>(&expression->node)) {
						const auto* actual = std::get_if<bool>(&payload.value);
						if (!actual || *actual != literal->value) fail("Bool literal differs from its source value.");
					} else if (const auto* here = std::get_if<ast::HereRef>(&expression->node)) {
						const auto* actual = std::get_if<HereValue>(&payload.value);
						if (!actual || !actual->region || here->resolvedRegion.text != actual->region->key.text) {
							fail("here value differs from its sema-resolved source region.");
						}
					} else if (const auto* member = std::get_if<ast::MemberExpr>(&expression->node)) {
						const auto* actual = std::get_if<EnumConstant>(&payload.value);
						if (!actual || !actual->member || actual->member->text != member->member.text)
							fail("enum constant differs from its source member.");
					} else if (const auto* identifier = std::get_if<ast::Identifier>(&expression->node)) {
						const auto* enumeration = std::get_if<EnumConstant>(&payload.value);
						const auto* key = std::get_if<KeyConstant>(&payload.value);
						const auto* parameter = std::get_if<ParameterRead>(&payload.value);
						const bool matches =
							(identifier->kind == ast::IdentifierKind::EnumValue && enumeration && enumeration->member &&
								enumeration->member->text == identifier->name.text) ||
							(identifier->kind == ast::IdentifierKind::DeclaredValue && key && key->name &&
								key->name->text == identifier->name.text) ||
							(identifier->kind == ast::IdentifierKind::Parameter && parameter && parameter->declaration &&
								parameter->declaration->name.text == identifier->name.text);
						if (!matches) fail("value identifier differs from its analyzed source identity.");
					} else if (const auto* call = std::get_if<ast::CallExpr>(&expression->node)) {
						// Callable-parameter reads borrow the invocation's source expression,
						// whose Bool result is distinct from the stored Condition value.
						const auto* parameter = std::get_if<ParameterRead>(&payload.value);
						if (!parameter || !parameter->declaration ||
							parameter->declaration->name.text != call->callee.text ||
							!node.valueType || !std::holds_alternative<ConditionTarget>(node.valueType->representation) ||
							project.getType(expression) != ast::Type::Bool) {
							fail("callable parameter read differs from its source invocation.");
						}
						continue;
					} else {
						fail("value node has unsupported source expression provenance.");
					}
					checkAnalyzed(GetValueType(project, *expression), node.valueType, span, "value");
				}
			} else if constexpr (std::is_same_v<T, Logical> || std::is_same_v<T, Arithmetic>) {
				const auto target = std::is_same_v<T, Logical> ? IntegralTarget::Bool : IntegralTarget::Int32;
				std::optional<ast::BinaryOp> sourceOperation;
				if constexpr (std::is_same_v<T, Logical>) {
					switch (payload.operation) {
					case LogicalOperator::And: sourceOperation = ast::BinaryOp::And; break;
					case LogicalOperator::Or: sourceOperation = ast::BinaryOp::Or; break;
					default: fail("logical node has an unsupported operator.");
					}
				} else {
					switch (payload.operation) {
					case ArithmeticOperator::Add: sourceOperation = ast::BinaryOp::Add; break;
					case ArithmeticOperator::Subtract: sourceOperation = ast::BinaryOp::Sub; break;
					case ArithmeticOperator::Multiply: sourceOperation = ast::BinaryOp::Mul; break;
					case ArithmeticOperator::Divide: sourceOperation = ast::BinaryOp::Div; break;
					default: fail("arithmetic node has an unsupported operator.");
					}
				}

				result(target);
				for (const auto child : payload.children) require(typeOf(child), target, "chain operand");

				if (sourceOperation) {
					if (const auto diagnostic = checkChainSources(project, graph, node,
						payload.children, *sourceOperation, target)) diagnostics.push_back(*diagnostic);
				}
			} else if constexpr (std::is_same_v<T, Not>) {
				result(IntegralTarget::Bool);
				require(typeOf(payload.operand), IntegralTarget::Bool, "not operand");
			} else if constexpr (std::is_same_v<T, Comparison>) {
				switch (payload.operation) {
				case ComparisonOperator::Equal: case ComparisonOperator::NotEqual:
				case ComparisonOperator::Less: case ComparisonOperator::LessEqual:
				case ComparisonOperator::Greater: case ComparisonOperator::GreaterEqual: break;
				default: fail("comparison node has an unsupported operator.");
				}

				result(IntegralTarget::Bool);
				integral(payload.left);
				integral(payload.right);
			} else if constexpr (std::is_same_v<T, Ternary>) {
				require(typeOf(payload.condition), IntegralTarget::Bool, "ternary condition");
				if (node.valueType) {
					require(typeOf(payload.trueBranch), node.valueType->representation, "ternary true branch");
					require(typeOf(payload.falseBranch), node.valueType->representation, "ternary false branch");
				}
			} else if constexpr (std::is_same_v<T, HelperCall>) {
				const auto& helper = graph.helpers[payload.definition.slot];
				checkCall(*helper.declaration, payload.parameters);

				const auto& bodyType = typeOf(*helper.body);
				if (!node.valueType || !bodyType || !sameType(*node.valueType, *bodyType)) {
					fail("helper '" + helper.declaration->name.text + "' body/result types differ.");
				}
			} else if constexpr (std::is_same_v<T, ExternCall>) {
				const auto& adapter = graph.adapters[payload.adapter.slot];
				checkCall(*adapter.declaration, payload.parameters);
				if (!node.valueType || !sameType(*node.valueType, adapter.result)) {
					fail("extern call '" + adapter.declaration->name.text + "' result differs from its adapter.");
				}
			} else if constexpr (std::is_same_v<T, Parameter>) {
				checkAnalyzed(GetValueType(project, *payload.declaration), node.valueType,
					span, "parameter '" + payload.declaration->name.text + "'");
				if (node.valueType) require(typeOf(payload.value), node.valueType->representation, "parameter value");
			} else if constexpr (std::is_same_v<T, Conversion>) {
				if (!validTarget(payload.target)) fail("conversion has an unsupported integral target.");
				result(payload.target);
				integral(payload.operand);
			} else if constexpr (std::is_same_v<T, Condition>) {
				result(ConditionTarget{});
				require(typeOf(payload.body), IntegralTarget::Bool, "condition body");
			} else if constexpr (std::is_same_v<T, Invoke>) {
				result(IntegralTarget::Bool);
				require(typeOf(payload.callable), ConditionTarget{}, "invocation callable");
			} else if constexpr (std::is_same_v<T, Match>) {
				if (!validTarget(payload.target)) fail("match has an unsupported integral target.");
				result(payload.target);
				integral(payload.discriminant);
				for (const auto arm : payload.arms) {
					const auto* branch = std::get_if<MatchArm>(&graph.nodes[arm.slot].payload);
					if (!branch) fail("match child must be a match arm.");
					else require(typeOf(branch->body), payload.target, "match arm body");
				}
			} else if constexpr (std::is_same_v<T, MatchArm>) {
				if (!std::holds_alternative<MatchPatterns>(graph.nodes[payload.patterns.slot].payload)) {
					fail("match arm requires a pattern-list node.");
				}
				integral(payload.body);
			} else if constexpr (std::is_same_v<T, MatchPatterns>) {
				for (const auto pattern : payload.patterns) integral(pattern);
			} else {
				static_assert(std::is_same_v<T, void>, "Unhandled tracker payload");
			}
		}, node.payload);
	}

	for (const auto& helper : graph.helpers) {
		const auto& declaration = *helper.declaration;
		const auto found = project.DefineDecls.find(declaration.name.text);
		if (found == project.DefineDecls.end() || found->second != &declaration || !declaration.body) {
			error(declaration.span, "Tracker helper '" + declaration.name.text + "' is missing its analyzed declaration/body.");
		} else {
			checkAnalyzed(GetValueType(project, *declaration.body), graph.nodes[helper.body->slot].valueType,
				declaration.span, "helper '" + declaration.name.text + "' body");
		}
	}

	std::unordered_set<const ast::ExternDefineDecl*> externs;
	for (const auto& adapter : graph.adapters) {
		const auto& declaration = *adapter.declaration;
		if (!validType(project, adapter.result)) {
			error(declaration.span, "Tracker adapter '" + declaration.name.text + "' has an unsupported result type/representation.");
		}
		checkEnum(adapter.result);

		const auto found = project.ExternDefineDecls.find(declaration.name.text);
		if (found == project.ExternDefineDecls.end() || found->second != &declaration) {
			error(declaration.span, "Tracker extern '" + declaration.name.text + "' is missing its analyzed declaration.");
		}

		if (!externs.insert(&declaration).second) {
			error(declaration.span, "Tracker extern '" + declaration.name.text + "' has duplicate adapter handles.");
		}

		for (size_t i = 0; i < adapter.parameters.size() && i < declaration.params.size(); ++i) {
			checkEnum(adapter.parameters[i]);
			checkAnalyzed(GetValueType(project, declaration.params[i]), adapter.parameters[i],
				declaration.span, "adapter '" + declaration.name.text + "' parameter");
		}
	}

	for (const auto& root : graph.roots) {
		const auto owner = project.RegionDecls.find(root.owner->key.text);
		if (owner == project.RegionDecls.end() || owner->second != root.owner) {
			error(root.provenance.span, "Tracker root '" + root.entry->name.text + "' is missing its analyzed owner.");
		}

		switch (root.kind) {
		case ast::SectionKind::Events: case ast::SectionKind::Locations: case ast::SectionKind::Exits: break;
		default: error(root.provenance.span, "Tracker root '" + root.entry->name.text + "' has an unsupported kind.");
		}

		const auto& type = graph.nodes[root.expression.slot].valueType;
		if (!type || type->representation != Representation{IntegralTarget::Bool}) {
			error(root.provenance.span, "Tracker root '" + root.entry->name.text + "' requires a Bool result.");
		}
		const auto& expressions = graph.nodes[root.expression.slot].provenance.expressions;
		if (!root.entry->condition || expressions.empty() || expressions.front() != root.entry->condition.get()) {
			error(root.provenance.span, "Tracker root '" + root.entry->name.text + "' differs from its source condition.");
		}
	}

	// Valid individual roots are insufficient: lowering must cover every source entry,
	// including extensions, without substituting another entry with the same symbolic name.
	const auto sources = CollectRoots(project, sourceRoot);
	if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&sources)) {
		diagnostics.insert(diagnostics.end(), errors->begin(), errors->end());
	} else {
		std::unordered_map<const ast::Entry*, const RootSource*> entries;
		for (const auto& source : std::get<std::vector<RootSource>>(sources)) entries.emplace(source.entry, &source);

		for (const auto& root : graph.roots) {
			const auto found = entries.find(root.entry);
			if (found == entries.end() || found->second->owner != root.owner ||
				found->second->kind != root.kind || root.occurrence != 0) {
				error(root.provenance.span, "Tracker root '" + root.entry->name.text + "' differs from its source record.");
			} else {
				entries.erase(found);
			}
		}

		// Report omitted entries in deterministic source order, not hash-table order.
		for (const auto& source : std::get<std::vector<RootSource>>(sources)) {
			if (entries.contains(source.entry)) {
				error(source.entry->span, "Tracker graph is missing root '" + source.entry->name.text + "'.");
			}
		}
	}

	// Check host contracts without rendering C++; orchestration owns numbering and generation.
	const auto adapterDiagnostics = ValidateHostAdapters(graph, customEnums);
	diagnostics.insert(diagnostics.end(), adapterDiagnostics.begin(), adapterDiagnostics.end());

	if (!diagnostics.empty()) return diagnostics;
	return std::get<GraphStructure>(std::move(structure));
}

} // namespace rls::transpilers::soh_tracker::ir
