//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
 
	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

#include <peersafe/app/sql/SQLConditionTree.h>

#include <ripple/json/Output.h> // Json::jsonAsString

#include <boost/format.hpp> // boost::format
#include <boost/algorithm/string.hpp> // boost::iequals

namespace ripple {

conditionTree::conditionTree(NodeType type)
: type_(type)
, bind_values_index_(-1)
, expression_()
, children_()
, bind_values_() {

}

conditionTree::conditionTree(const conditionTree& t)
: type_(t.type_)
, bind_values_index_(t.bind_values_index_)
, expression_(t.expression_)
, children_(t.children_)
, bind_values_(t.bind_values_) {

}

conditionTree::~conditionTree() {

}

bool isIncompleteAnd(const std::string& sAnd) {
	if (sAnd.size() == 3 && (sAnd[0] == 'a' || sAnd[0] == 'A')
		&& (sAnd[1] == 'n' || sAnd[0] == 'N')
		&& (sAnd[2] == 'd' || sAnd[2] == 'D'))
		return true;
	return false;
}

bool isIncompleteOr(const std::string& sOr) {
	if (sOr.size() == 2 && (sOr[0] == 'o' || sOr[0] == 'O')
		&& (sOr[1] == 'r' || sOr[0] == 'R'))
		return true;
	return false;
}

std::pair<int, conditionTree> conditionTree::createRoot(const Json::Value& conditions) {
	conditionTree::NodeType node_type = conditionTree::NodeType::Expression;
	std::pair<int, conditionTree> result = { -1, conditionTree(node_type) };

	do {
		if (conditions.isArray() == false) {
			break;
		}

		Json::UInt size = conditions.size();
		if (size == 1) {
			const std::vector<std::string>& keys = conditions[size - 1].getMemberNames();
			if (keys.size() > 1) {
				node_type = conditionTree::NodeType::Logical_And;
			}
			else if (keys.size() == 1) {
				const std::string& key_name = keys[0];
				if (isIncompleteAnd(key_name) || isIncompleteOr(key_name)) {
					break;
				}
				if(boost::iequals(key_name, "$and"))
					node_type = conditionTree::NodeType::Logical_And;
				else if (boost::iequals(key_name, "$or"))
					node_type = conditionTree::NodeType::Logical_Or;
				else {
					const Json::Value& v = conditions[size - 1];
					std::string debug = Json::jsonAsString(v);
					const Json::Value& x = v[key_name];
					if (x.isObject() && x.getMemberNames().size() > 1) {
						// pattern: [{"age":{"$ge":20,"$le":100}}]
						node_type = conditionTree::NodeType::Logical_And;
					}
					else {
						// pattern: [{"age":{"$ge":20}}] or [{"age":20}]
						node_type = conditionTree::NodeType::Expression;
					}
				}
			}
			else {
				break;
			}
		}
		else if (size > 1) {
			node_type = conditionTree::NodeType::Logical_Or;
		}
		else {
			break;
		}
		result = { 0, conditionTree(node_type) };
	} while (false);
	return result;
}

const std::string conditionTree::asString() const {
	std::string str;
	format_conditions(0, str);
	return str;
}

const std::pair<int, std::string> conditionTree::asConditionString() const {
	std::string conditions;
	bind_values_.clear();
	format_conditions(1, conditions);
	return{0, conditions};
}

const std::pair<int, std::string> conditionTree::bind_value(soci::details::once_temp_type& t) {
	std::string conditions;
	std::pair<int, std::string> result = { -1, "bind value unsuccessfully." };
	if(bind_values_.empty())
		format_conditions(1, conditions);

	size_t size = bind_values_.size();
	for (size_t indx = 0; indx < size; indx++) {
		const std::vector<BindValue>& v = bind_values_[indx];
		if (v.size() == 1) {
			if (bind_value(v[0], t) != 0)
				return result;
		}
		else if (v.size() > 1) {
			if (bind_array(v, t) != 0)
				return result;
		}
		else {
			break;
		}
	}
	return {0, "success"};
}

int conditionTree::bind_value(const BindValue& value, soci::details::once_temp_type& t) {
	int result = 0;
	if (value.isString() || value.isBlob() || value.isText() || value.isVarchar()) {
		t = t, soci::use(value.asString());
	}
	else if (value.isInt()) {
		t = t, soci::use(value.asInt());
	}
	else if (value.isUint()) {
		t = t, soci::use(value.asUint());
	}
	else if (value.isDouble() || value.isNumeric()) {
		t = t, soci::use(value.asDouble());
	}
	else {
		result = -1;
	}
	return result;
}

int conditionTree::bind_array(const std::vector<BindValue>& values, soci::details::once_temp_type& t) {
	int result = 0;
	size_t size = values.size();
	const BindValue& v = values[0];
	if (v.isBlob() || v.isString() || v.isText() || v.isVarchar()) {
		for (size_t i = 0; i < size; i++) {
			t = t, soci::use(values[i].asString());
		}
	}
	else if (v.isInt()) {
		for (size_t i = 0; i < size; i++) {
			t = t, soci::use(values[i].asInt());
		}
	}
	else if (v.isUint()) {
		for (size_t i = 0; i < size; i++) {
			t = t, soci::use(values[i].asUint());
		}
	}
	else if (v.isDouble() || v.isNumeric()) {
		for (size_t i = 0; i < size; i++) {
			t = t, soci::use(values[i].asDouble());
		}
	}
	else {
		result = -1;
	}

	return result;
}

int conditionTree::format_conditions(int style, std::string& conditions) const {
	std::string c; // this is group of condition
	conditionTree root = *this;
	conditionParse::traverse(root,
		[this, &c, &style](const conditionTree::expression_result& result) {
		std::string keyname = std::get<0>(result);
		std::string op = std::get<1>(result);
		std::vector<BindValue> value = std::get<2>(result);
		if (value.empty()) {
			c = (boost::format("ERROR: %s %s null. value can't be null or nil") %keyname %op).str();
			return false;
		}

		if (boost::iequals(op, "$eq")) {
			op = "=";
		}
		else if (boost::iequals(op, "$ne")) {
			op = "!=";
		}
		else if (boost::iequals(op, "$lt")) {
			op = "<";
		}
		else if (boost::iequals(op, "$le")) {
			op = "<=";
		}
		else if (boost::iequals(op, "$gt")) {
			op = ">";
		}
		else if (boost::iequals(op, "$ge")) {
			op = ">=";
		}
		else if (boost::iequals(op, "$regex")) {
			op = "like";
		}
		else if (boost::iequals(op, "$in")) {
			op = "in";
		}
		else if (boost::iequals(op, "$nin")) {
			op = "not in";
		}
        else if (boost::iequals(op, "$is")) {
            op = "is";
        }
        else if (boost::iequals(op, "$isnot")) {
            op = "is not";
        }

		// handle regex 
		std::function<bool(std::string&, int)> modify_bind_string = [this](std::string& fv, int style) {
			if(style == 0) {
				if (fv[1] == '/' && fv[2] == '^' && fv[fv.size() - 2] == '/') {
					std::string s("'%");
					s.insert(s.end(), fv.begin() + 3, fv.end() - 2);
					fv = s + "'";
				}
				else if( fv[1] == '/' && fv[fv.size() - 3] == '^' && fv[fv.size() - 2] == '/') {
					std::string s("'");
					s.insert(s.end(), fv.begin() + 2, fv.end() - 3);
					fv = s + "%'";
				}
				else if (fv[1] == '/' && fv[fv.size() - 2] == '/') {
					fv[1] = '%';
					fv[fv.size() - 2] = '%';
				}
				else {
					return false;
				}
			}
			else {
				if (fv[0] == '/' && fv[1] == '^' && fv[fv.size() - 1] == '/') {
					std::string s("%");
					s.insert(s.end(), fv.begin() + 2, fv.end() - 1);
					fv = s;
				}else if (fv[0] == '/' && fv[fv.size() - 2] == '^' && fv[fv.size() - 1] == '/') {
					std::string s;
					s.insert(s.end(), fv.begin() + 1, fv.end() - 2);
					fv = s + "%";
				}
				else if (fv[0] == '/' && fv[fv.size() - 1] == '/') {
					fv[0] = '%';
					fv[fv.size() - 1] = '%';
				}
				else {
					return false;
				}
			}
			
			return true;
		};

		std::string sub;
		if (style == 0) {
			if(op == "in" || op == "not in") {
				// op must be in or nin
				//assert(op == "in" || op == "not in");
				std::string element = "(";
				size_t size = value.size();
				for (size_t index = 0; index < size; index++) {
					const BindValue& v = value[index];
					std::string real_v;
					if (format_value(v, real_v) != 0) {
						return false;
					}

					element += real_v;
					if (index != size - 1)
						element += ",";
				}
				element += ")";

				sub = (boost::format("%1% %2% %3%")
					% keyname %op %element).str();
            } else if (op == "is" || op == "is not") {
                if (!(value[0].isString() || value[0].isBlob() ||
                      value[0].isText() || value[0].isVarchar())) {
                    return false;
                }

                std::string s = value[0].asString();
                transform(s.begin(), s.end(), s.begin(), ::toupper);
                if (s != "NULL") {
                    return false;
                }

                sub += (boost::format("%1% %2% %3%") %keyname %op %s).str();
            } else {
				//assert(value.size() == 1);
				assert(op != "in" && op != "not in");
				const BindValue& v = value[0];
				std::string fv;
				if (format_value(v, fv) != 0)
					return false;

				if (boost::iequals(op, "like")) {
					modify_bind_string(fv, style);
				}

				sub = (boost::format("%1% %2% %3%")
					% keyname %op %fv).str();
			}
		}
		else {
			std::string placeHoder;
			if (boost::iequals(op, "in") || boost::iequals(op, "not in")) {
				placeHoder += "(";
				const size_t& size = value.size();
				for (size_t i = 0; i < size; i++) {
					placeHoder += (boost::format(":%1%_%2%") %keyname %i).str();
					if (i != size - 1) {
						placeHoder += ",";
					}
				}
				placeHoder += ")";
			}
			else {
				if (bind_values_index_ != -1) {
					placeHoder = (boost::format(":%1%") %(++bind_values_index_)).str();
				}
				else {
					placeHoder = (boost::format(":%1%") %keyname).str();
				}
			}

			sub = (boost::format("%1% %2% %3%")
					%keyname %op %placeHoder).str();

			if (boost::iequals(op, "like")) {
				const std::string& v = value[0].asString();
				std::string fv = v;
				if (modify_bind_string(fv, style))
					value[0] = BindValue(fv);
			}

			bind_values_.push_back(value);
		}

		c += sub;

		return true;

	}, [this, &c](const conditionTree& node, int process_state) {

		if (process_state == 0) {
			// starting a group of condition
			c += "(";
		}
		else if (process_state == 2) {
			// end a group of condition
			c += ")";
		}
		else {
			if (node.node_type() == conditionTree::NodeType::Logical_And) {
				c += " and ";
			}
			else if (node.node_type() == conditionTree::NodeType::Logical_Or) {
				c += " or ";
			}
		}
		return 0;
	});
	conditions += c;
	return 0;
}

int conditionTree::format_value(const BindValue& value, std::string& result) const {
	int ret = 0;
	if (value.isString() || value.isBlob() || value.isText() || value.isVarchar()) {
		if(value.asString().find(".") == std::string::npos)
			result = (boost::format("'%1%'") % value.asString()).str();
		else {
			// especially for one field equal another field
			const std::string& v = value.asString();
			if (v[0] == '$' && v[1] == '.') {
				result = (boost::format("%1%") % v.substr(2)).str();
			}
			else {
				result = (boost::format("%1%") % v).str();
			}
		}
	}
	else if (value.isInt()) {
		result = (boost::format("%1%") % value.asInt()).str();
	}
	else if (value.isUint()) {
		result = (boost::format("%1%") % value.asUint()).str();
	}
	else if (value.isDouble() || value.isNumeric()) {
		result = (boost::format("%1%") % value.asDouble()).str();
	}
	else {
		ret = -1;
		result = "Not support value type";
	}
	return ret;
}

int conditionTree::parse_array(const Json::Value& j, std::vector<BindValue>& v) {
	int result = 0;
	assert(j.isArray());
	do {
		if (j.isArray() == false)
			break;
		Json::UInt size = j.size();
		for (Json::UInt i = 0; i < size; i++) {
			const Json::Value& e = j[i];
			BindValue value;
			if (parse_value(e, value) == 0)
				v.push_back(value);
		}
	} while (false);
	return result;
}

int conditionTree::parse_value(const Json::Value& j, BindValue& v) {
	int result = 0;
	if (j.isString())
		v = BindValue(j.asString());
	else if (j.isInt())
		v = BindValue(j.asInt());
	else if (j.isUInt())
		v = BindValue(j.asUInt());
	else if (j.isDouble())
		v = BindValue(j.asDouble());
	else
		v = BindValue(j.asString());
	return result;
}

conditionTree::expression_result conditionTree::parse_expression(const Json::Value& e) {
	std::string keyname;
	std::string op;
	std::vector<BindValue> value;
	conditionTree::expression_result result;

	std::string debug = Json::jsonAsString(e);
	do {
		if (e.isObject() == false)
			break;
		const std::vector<std::string>& keys = e.getMemberNames();
		keyname = keys[0];
		const Json::Value& v = e[keyname];
		if (v.isObject()) {
			const std::vector<std::string>& ops = v.getMemberNames();
			op = ops[0];
			if (v[op].isArray()) {
				if (parse_array(v[op], value) != 0)
					break;
			}
			else {
				value.push_back(BindValue());
				if (parse_value(v[op], value[0]) != 0)
					break;
			}
		}
		else {
			op = "$eq";
			value.push_back(BindValue());
			if (parse_value(v, value[0]) != 0)
				break;
		}
	} while (false);
	result = std::make_tuple(keyname, op, value);
	return result;
}

namespace conditionParse {

	bool isLogicChars(const std::string& logic) {
		if (boost::iequals(logic, "$eq") || boost::iequals(logic, "$ne")
			|| boost::iequals(logic, "$lt") || boost::iequals(logic, "$le")
			|| boost::iequals(logic, "$gt") || boost::iequals(logic, "$ge")
            || boost::iequals(logic, "$is") || boost::iequals(logic, "$isnot")
			|| boost::iequals(logic, "$regex") || boost::iequals(logic, "$in")
			|| boost::iequals(logic, "$nin")) {
			return true;
		}
		return false;
	}
	
	std::pair<int, std::vector<Json::Value>> parse_value(const Json::Value& value) {
		std::vector<Json::Value> values;
		std::pair<int, std::vector<Json::Value>> result = { -1, values };
		if (value.isNull() == false && value.isObject() == false) {
			values.push_back(value);
			result = { 0, values };
			return result;
		}

		const std::vector<std::string>& keys = value.getMemberNames();
		if (keys.size() == 1) {
			if (!isLogicChars(keys[0])) {
				return{ -1, values };
			}
			else {
				values.push_back(value);
				result = { 0, values };
			}
		}
		else {
			size_t size = keys.size();
			for (size_t index = 0; index < size; index++) {
				const std::string& key_name = keys[index];
				if (!isLogicChars(key_name)) {
					return{ -1, values };
				}
				else {
					Json::Value v;
					v[key_name] = value[key_name];
					values.push_back(v);
				}
			}
			result = { 0, values };
		}
		return result;
	}

	std::pair<int, std::string> generate_leafnode_and_add_into_tree(const std::string& key_name,
		const Json::Value& condition_value, conditionTree& root) {

		std::pair<int, std::string> result{ 0, "success" };

		if (condition_value.isObject()) {
			auto ret = parse_value(condition_value);
			if (ret.first == 0) {
				size_t size = ret.second.size();
				for (size_t index = 0; index < size; index++) {
					conditionTree node(conditionTree::NodeType::Expression);
					Json::Value expression;
					expression[key_name] = ret.second[index];
					node.set_expression(expression);

					root.add_child(node);
				}
			}
			else {
				result = { -1, (boost::format("value is error. [%s]")
					% Json::jsonAsString(condition_value)).str() };
			}
		}
		else if (condition_value.isBool() || condition_value.isInt()
			|| condition_value.isDouble() || condition_value.isUInt()
			|| condition_value.isString() || condition_value.isIntegral()
			|| condition_value.isNumeric()) {
			conditionTree node(conditionTree::NodeType::Expression);
			Json::Value expression;
			expression[key_name] = condition_value;
			node.set_expression(expression);

			root.add_child(node);
		}
		else {
			result = { -1, (boost::format("value is error. [%s]")
				% Json::jsonAsString(condition_value)).str() };
		}

		return result;
	}

	bool isSampleAndCondition(const Json::Value& c) {
		bool isSample = false;
		do {
			if (c.isObject() == false) 
				break;
			const std::vector<std::string>& keys = c.getMemberNames();
			if (keys.size() == 0 || keys.size() > 1)
				break;
			const std::string& key_name = keys[0];
			const Json::Value& key_value = c[key_name];
			if (key_value.isObject() == false)
				break;

			const std::vector<std::string>& key2s = key_value.getMemberNames();
			if (key2s.size() != 2) {
				break;
			}
			if (isLogicChars(key2s[0]) == false || isLogicChars(key2s[1]) == false)
				break;
			isSample = true;
		} while (false);
		return isSample;
	}

	bool isAppropriate(const Json::Value& andOrValue) {
		bool result = false;
		do {
			if (andOrValue.isArray() == false) {
				break;
			}
			Json::UInt size = andOrValue.size();

			// for format: {\"$and\":[{\"age\":{\"$ge\":20,\"$le\":60}}]}
			// {\"$or\":[{\"age\":{\"$ge\":20,\"$le\":60}}]}
			if (size == 1 && isSampleAndCondition(andOrValue[0U]) == true) {
				result = true;
				break;
			}
			// size of an array is less than 2.
			if (size < 2) {
				break;
			}
			result = true;
		} while (false);
		return result;
	}

	std::pair<int, std::string> parse_logicalOr(const Json::Value& condition, conditionTree& root);
	std::pair<int, std::string> parse_logicalAnd(const Json::Value& condition, conditionTree& root) {
		std::pair<int, std::string> result{ 0, "success" };
		do {
			const std::vector<std::string>& keys = condition.getMemberNames();
			if (keys.empty()) {
				result = { -1, (boost::format("condition is malformed in parsing logicalAnd. [%s]")
					% Json::jsonAsString(condition)).str() };
				break;
			}

			size_t size = keys.size();
			if (size == 1) {
				if (isIncompleteAnd(keys[0]) || isIncompleteOr(keys[0])) {
					result = { -1, (boost::format("%s can't be recognized in conditions. [%s]")
							% keys[0]
							% Json::jsonAsString(condition)).str() };
					break;
				}
				if (boost::iequals(keys[0], "$and")) {
					Json::Value condition_value = condition["$and"];
					if (isAppropriate(condition_value) == false) {
						result = { -1, (boost::format("condition may be malformed. [%s]")
									% Json::jsonAsString(condition)).str() };
						break;
					}
					Json::UInt size = condition_value.size();
					for (Json::UInt index = 0; index < size; index++) {
						Json::Value& element = condition_value[index];
						const std::vector<std::string>& element_keys = element.getMemberNames();
						if (element_keys.size() == 1) {
							const std::string& key_name = element_keys[0];
							if (isIncompleteAnd(key_name) || isIncompleteOr(key_name)) {
								result = { -1, (boost::format("%s can't be recognized in conditions. [%s]")
																% key_name
																% Json::jsonAsString(condition)).str() };
								return result;
							}
							if (boost::iequals(key_name, "$or")) {
								conditionTree node(conditionTree::NodeType::Logical_Or);
								result = parse_logicalOr(element, node);
								if (result.first == 0)
									root.add_child(node);
								else
									return result;
							}
							else if (boost::iequals(key_name, "$and")) {
								//result = { -1, (boost::format("Can't contain $and-element in an array. %s")
								//	% Json::jsonAsString(condition_value)).str() };
								//return result;
								conditionTree node(conditionTree::NodeType::Logical_And);
								result = parse_logicalAnd(element, node);
								if (result.first == 0) {
									root.add_child(node);
								}
								else {
									return result;
								}
							}
							else {
								Json::Value& key_value = element[key_name];
								result = generate_leafnode_and_add_into_tree(key_name, key_value, root);
								if (result.first != 0)
									return result;
							}
						}
						else {
							result = { -1, (boost::format("'%s' has more than one keys.")
								% Json::jsonAsString(condition_value[index])).str() };
							return result;
						}
					}
				}
				else {
					const std::string& key_name = keys[0];
					result = generate_leafnode_and_add_into_tree(key_name, condition[key_name], root);
					if (result.first != 0)
						break;
				}
			}
			else if (size > 1) {
				for (size_t index = 0; index < size; index++) {
					const std::string& key_name = keys[index];
					result = generate_leafnode_and_add_into_tree(key_name, condition[key_name], root);
					if (result.first != 0)
						return result;
				}
			}
			else {
				result = { -1, (boost::format("logical operator is error. [%s]") % keys[0]).str() };
				break;
			}
		} while (false);
		return result;
	}

	std::pair<int, std::string> parse_logicalOr(const Json::Value& condition, conditionTree& root) {
		std::pair<int, std::string> result{ 0, "success" };
		do {
			const std::vector<std::string>& keys = condition.getMemberNames();
			if (keys.empty()) {
				result = { -1, (boost::format("condition is malformed in parsing logicalOr. [%s]")
					% Json::jsonAsString(condition)).str() };
				break;
			}

			Json::UInt size = keys.size();
			if (size == 1) {
				const std::string& key_name = keys[size - 1];
				const Json::Value& values = condition[key_name];
				if (isAppropriate(values) == false) {
					result = { -1, (boost::format("condition may be malformed. [%s]")
								% Json::jsonAsString(condition)).str() };
					break;
				}
				size_t size = values.size();
				for (Json::UInt index = 0; index < size; index++) {
					const Json::Value& v = values[index];
					const std::vector<std::string>& keys = v.getMemberNames();
					const std::string& key_name = keys[0];
					if (isIncompleteAnd(key_name) || isIncompleteOr(key_name)) {
						result = { -1, (boost::format("%s can't be recognized in conditions.")
												% key_name).str() };
						return result;
					}
					if (boost::iequals(key_name, "$and")) {
						conditionTree node(conditionTree::NodeType::Logical_And);
						result = parse_logicalAnd(v, node);
						if (result.first == 0) {
							root.add_child(node);
						}
						else {
							return result;
						}
					}
					else if (boost::iequals(key_name, "$or")) {
						conditionTree node(conditionTree::NodeType::Logical_Or);
						result = parse_logicalOr(v, node);
						if (result.first == 0)
							root.add_child(node);
						else
							return result;
					}
					else {
						result = generate_leafnode_and_add_into_tree(key_name, v[key_name], root);
						if (result.first != 0)
							return result;
					}
				}
			}
			else {
				std::pair<int, std::string> result{ -1, (boost::format("Not support.[%s]")
					% Json::jsonAsString(condition)).str() };
				break;
			}
		} while (false);
		return result;
	}

	std::pair<int, std::string> adjust_conditionOr(const Json::Value& old_value, Json::Value& new_value) {
		assert(old_value.isArray());
		std::pair<int, std::string> result{ 0, "success" };
		do {
			Json::UInt size = old_value.size();
			if (size == 0) {
				result = { -1, (boost::format("Not support. [%s]") % Json::jsonAsString(old_value)).str() };
				break;
			}
			else if (size == 1) {
				/*
				* if old_value matchs the standard pattern that is `[{$or:[{exp1},{exp2},...,{expn}]}]`,
				* then return old_value directly
				*/
				const Json::Value& one_condition = old_value[size - 1];
				if (one_condition.isObject() == false) {
					result = { -1, (boost::format("Not support. [%s]") % Json::jsonAsString(old_value)).str() };
					break;
				}

				const std::vector<std::string>& keys = one_condition.getMemberNames();
				if (keys.size() <= 0 || keys.size() > 1) {
					result = { -1, (boost::format("Not support. [%s]") % Json::jsonAsString(old_value)).str() };
					break;
				}

				const std::string& key_name = keys[0];
				if (isIncompleteOr(key_name)) {
					result = { -1, (boost::format("%s can't be recognized in conditions. ") % key_name).str() };
					break;
				}
				if (boost::iequals(key_name, "$or") == false) {
					result = { -1, (boost::format("This operator can't support. [%s]") % key_name).str() };
					break;
				}
				// return directly
				new_value = old_value;
			}
			else {
				/*
				* other's pattern will be translated from current pattern into standard pattern, `[{$or:[{exp1},{exp2},...,{expn}]}]`
				*/
				Json::Value or_elements;
				for (Json::UInt index = 0; index < size; index++) {

					const Json::Value& one_condition = old_value[index];
					if (one_condition.isObject() == false) {
						result = { -1, (boost::format("Not support. [%s]") % Json::jsonAsString(old_value)).str() };
						break;
					}

					const std::vector<std::string>& keys = one_condition.getMemberNames();
					if (keys.empty()) {
						result = { -1, (boost::format("Not support. [%s]") % Json::jsonAsString(old_value)).str() };
						break;
					}

					size_t keys_size = keys.size();
					if (keys_size == 1) {
						/*
						* translate from [{key1:value1},{key2:value2}]
						* into standard, `[{$or:[{key1:value1},{key2:value2}]}]`
						*/
						const std::string& key_name = keys[0];
						if (boost::iequals(key_name, "$or")
							|| boost::iequals(key_name, "$and")
							|| boost::iequals(key_name, "$in")
							|| boost::iequals(key_name, "$nin")
							|| boost::iequals(key_name, "$limit")
							|| boost::iequals(key_name, "$order")) {
							result = { -1, (boost::format("%s dosen't be supported. [%s]")
								% key_name
								%Json::jsonAsString(old_value)).str() };
							break;
						}
						const Json::Value& key_value = one_condition[key_name];
						std::pair<int, std::vector<Json::Value>> ret = parse_value(key_value);
						if (ret.first != 0) {
							result = { -1, (boost::format("Parse value failed. [%s]")
								% Json::jsonAsString(key_value)).str() };
							break;
						}

						size_t s = ret.second.size();
						if (s == 1) {
							Json::Value v;
							v[key_name] = ret.second[0];
							or_elements.append(v);
						}
						else {
							Json::Value and_values;
							for (size_t index = 0; index < s; index++) {
								Json::Value v;
								v[key_name] = ret.second[index];
								and_values.append(v);
							}
							Json::Value json_and;
							json_and["$and"] = and_values;
							or_elements.append(json_and);
						}
					}
					else {
						/*
						* translate from [{key1:value1,key2:value2},{key3:value3}]
						* into standard, `[{$or:[{$and:[{key1:value1},{key2:value2}]},{key3:value3}]}]`
						*/
						Json::Value and_values;
						for (size_t i = 0; i < keys_size; i++) {
							const std::string& key_name = keys[i];
							const Json::Value& key_value = one_condition[key_name];
							std::pair<int, std::vector<Json::Value>> ret = parse_value(key_value);
							if (ret.first != 0) {
								result = { -1, (boost::format("Parse value failed. [%s]")
									% Json::jsonAsString(key_value)).str() };
								return result;
							}

							size_t s = ret.second.size();
							for (size_t index = 0; index < s; index++) {
								Json::Value v;
								v[key_name] = ret.second[index];
								and_values.append(v);
							}
						}
						Json::Value json_and;
						json_and["$and"] = and_values;
						or_elements.append(json_and);
					}
				}
				Json::Value out;
				out["$or"] = or_elements;
				new_value.append(out);
			}
		} while (0);
		std::string debug = Json::jsonAsString(new_value);
		return result;
	}

	std::pair<int, std::string> parse_expression(const Json::Value& condition, conditionTree& root) {
		std::pair<int, std::string> result{ 0, "success" };
		do {
			const std::vector<std::string>& keys = condition.getMemberNames();
			if (condition.isObject() && keys.size() > 1) {
				result = { -1, (boost::format("condtion is malformed in parsing expression.[%s]")
					% Json::jsonAsString(condition)).str() };
				break;
			}
			const std::string& key = keys[0];
			Json::Value value = condition[key];
			if (value.isObject()) {
				if (value.getMemberNames().size() != 1) {
					result = { -1, (boost::format("condtion is malformed in parsing expression.[%s]")
						% Json::jsonAsString(condition)).str() };
					break;
				}
				const auto& opKeys = value.getMemberNames();
				const std::string& op = opKeys[0];
				if (!isLogicChars(op)) {
					result = { -1, (boost::format("An operator can't be supported.[%s]")
						% Json::jsonAsString(op)).str() };
					break;
				}
			}
			root.set_expression(condition);
		} while (false);
		return result;
	}

	std::pair<int, std::string> parse_conditions(const Json::Value& raw_value, conditionTree& root) {
		std::pair<int, std::string> result{ 0, "success" };
		do {
			if (raw_value.isArray() == false) {
				result = { -1, (boost::format("raw_value is malformed. [%s]")
					% Json::jsonAsString(raw_value)).str() };
				break;
			}

			if (root.node_type() == conditionTree::NodeType::Expression) {
				Json::UInt index = 0;
				result = parse_expression(raw_value[index], root);
			}
			else if (root.node_type() == conditionTree::NodeType::Logical_And) {
				Json::UInt index = 0;
				result = parse_logicalAnd(raw_value[index], root);
			}
			else if (root.node_type() == conditionTree::NodeType::Logical_Or) {
				Json::Value adjusted;
				result = adjust_conditionOr(raw_value, adjusted);
				if (result.first == 0) {
					Json::UInt index = 0;
					result = parse_logicalOr(adjusted[index], root);
				}
				else {
					break;
				}
			}
		} while (false);
		return result;
	}

	bool judge_expression(const conditionTree::expression_result& expressione, handlevaluecb cb) {

		bool result = false;
		if (cb && cb(expressione)) {
			result = true;
		}
		return result;
	}

	bool judge(conditionTree& root, handlevaluecb cb) {
		bool result = false;
		if (root.node_type() == conditionTree::NodeType::Expression) {
			result = judge_expression(root.parse_expression(), cb);
		}
		else {
			assert(root.size() > 1);
			if (root.size() < 2)
				return result;

			auto it = root.begin();
			for (; it != root.end(); it++) {
				conditionTree node = *it;
				if (node.node_type() == conditionTree::NodeType::Expression) {
					result = judge_expression(node.parse_expression(), cb);
				} 
				else {
					result = judge(node, cb);
				}

				if (root.node_type() == conditionTree::NodeType::Logical_Or) {
					if (result == true)
						break;
					else
						continue;
				}
				else if (root.node_type() == conditionTree::NodeType::Logical_And) {
					if (result == false)
						break;
					else
						continue;
				}
			}
		}
		return result;
	}

	void traverse(conditionTree& root, handlevaluecb vcb, handlenodecb ncb) {
		if (root.node_type() == conditionTree::NodeType::Expression) {
			conditionTree::expression_result result = root.parse_expression();
			if (vcb)
				vcb(result);
		}
		else {
			size_t size = root.size();
			assert(size > 1);
			if (size < 2)
				return;
			auto it = root.begin();
			int index = 0;
			for (; it != root.end(); it++, index++) {
				conditionTree node = *it;
				if (node.node_type() == conditionTree::NodeType::Expression) {
					conditionTree::expression_result result = node.parse_expression();
					if (vcb)
						vcb(result);
					if (ncb && index != size - 1)
						ncb(root, 1);
				}
				else {
					ncb(root, 0);
					traverse(node, vcb, ncb);
					ncb(root, 2);
					if (ncb && index != size - 1)
						ncb(root, 1);
				}
			}

		}
	}
} // namespace helper

} // namespace ripple
