#include "soh_ap.h"

#include <sstream>

namespace rls::transpilers::soh_ap {

std::string SohApTranspiler::renderEnumValue(rls::ast::Type type, const std::string& name) const {
	switch (type) {
		case rls::ast::Type::Item: return "Items." + name;
		case rls::ast::Type::Enemy: return "Enemies." + name;
		case rls::ast::Type::Distance: return "EnemyDistance." + name;
		case rls::ast::Type::Trick: return "Tricks." + name;
		// Water-level values live in the Events enum in the reference oot_soh world.
		case rls::ast::Type::Logic:
		case rls::ast::Type::WaterLevel:
			return "Events." + name;
		case rls::ast::Type::Setting:
			// Catch the case where it is generic on or off
			if (name == "RO_GENERIC_YES") {
				return "True";
			} else if (name == "RO_GENERIC_NO") {
				return "False";
			}
			return "RandomizerSettingKey." + name;
		case rls::ast::Type::Region: return "Regions." + name;
		case rls::ast::Type::Check: return "Locations." + name;
		case rls::ast::Type::Trial: return "TrialKey." + name;
		default: return name;
	}
}

std::optional<std::string> SohApTranspiler::renderHostCall(const rls::ast::CallExpr& node) const {
	const auto* resolvedPtr = project.getResolvedCallArgs(&node);
	if (!resolvedPtr || resolvedPtr->empty()) {
		return std::nullopt;
	}
	const auto& resolved = *resolvedPtr;

	if (node.callee.text == "has" || node.callee.text == "flag") {
		return "has_item(bundle, " + GenerateExpression(resolved[0]->node) + ")";
	}
	if (node.callee.text == "trick") {
		return "can_do_trick(bundle, " + GenerateExpression(resolved[0]->node) + ")";
	}
	if (node.callee.text == "check_price") {
		// Special case: check_price(...) should output can_afford_slot(...).
		// If the argument is an RC_UNKNOWN_CHECK identifier, use the current location from context.
		if (auto* id = std::get_if<rls::ast::Identifier>(&resolved[0]->node);
			id && id->name.text == "RC_UNKNOWN_CHECK" && currentLocationName.has_value()) {
			return "can_afford_slot(Locations." + currentLocationName.value() + ")";
		}
		return "can_afford_slot(" + GenerateExpression(resolved[0]->node) + ")";
	}

	return std::nullopt;
}

std::optional<std::string> SohApTranspiler::renderBinarySpecialCase(const rls::ast::BinaryExpr& node) const {
	// Special case: check_price(...) <= wallet_capacity(...) should only output check_price(...)
	if (node.op == rls::ast::BinaryOp::LtEq) {
		auto* leftCall = std::get_if<rls::ast::CallExpr>(&node.left->node);
		auto* rightCall = std::get_if<rls::ast::CallExpr>(&node.right->node);

		if (leftCall && rightCall &&
			leftCall->callee.text == "check_price" &&
			rightCall->callee.text == "wallet_capacity") {
			return GenerateExpression(node.left);
		}
	}

	// Special case: collected_triforce_pieces(...) >= required_triforce_pieces(...) should output CanWinTriforceHunt()
	if (node.op == rls::ast::BinaryOp::GtEq) {
		auto* leftCall = std::get_if<rls::ast::CallExpr>(&node.left->node);
		auto* rightCall = std::get_if<rls::ast::CallExpr>(&node.right->node);

		if (leftCall && rightCall &&
			leftCall->callee.text == "collected_triforce_pieces" &&
			rightCall->callee.text == "required_triforce_pieces") {
			return std::string("CanWinTriforceHunt()");
		}
	}

	return std::nullopt;
}

// Going to have to fiddle with this.
// I think as long as we define the SharedSpirit functions and the SharedSpiritData map this could be done
std::string SohApTranspiler::renderSharedBlock(const rls::ast::SharedBlock& node) const {
	std::ostringstream oss;

	const auto& firstBranch = node.branches[0];
	oss << "spirit_shared(" << firstBranch.region->text << ", "
		<< "(lambda: " << GenerateExpression(firstBranch.condition) << "), "
		<< (node.anyAge ? "True" : "False");

	for (size_t i = 1; i < node.branches.size(); i++) {
		oss << ", " << node.branches[i].region->text << ", "
			<< "(lambda:" << GenerateExpression(node.branches[i].condition) << ")";
	}

	oss << ")";

	return oss.str();
}

// This one I'm not quite sure how it works. Seems like Ship calls this recursively for the current region
std::string SohApTranspiler::renderAnyAgeBlock(const rls::ast::AnyAgeBlock& node) const {
	return "";
	// return "AnyAgeTime((lambda:" + GenerateExpression(node.body) + "))";
}

} // namespace rls::transpilers::soh_ap
