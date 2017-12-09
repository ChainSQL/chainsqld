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

#ifndef RIPPLE_APP_MISC_SQLCONDITIONTREE_H_INCLUDED
#define RIPPLE_APP_MISC_SQLCONDITIONTREE_H_INCLUDED

#include <vector>
#include <functional>
#include <tuple>

#include <peersafe/app/sql/SQLDataType.h>

#include <ripple/json/json_value.h>
#include<ripple/core/SociDB.h> // soci::details::once_temp_type

namespace ripple {

typedef FieldValue BindValue;

class conditionTree {
public:
	typedef std::vector<conditionTree>::iterator iterator;
	typedef std::vector<conditionTree>::const_iterator const_iterator;
	typedef std::tuple<std::string /*key*/, std::string /*operator*/, std::vector<BindValue> /*key's value*/> expression_result;

	typedef enum {
		Logical_And,
		Logical_Or,
		Expression
	} NodeType;

	conditionTree(NodeType);
	conditionTree(const conditionTree& t);
	~conditionTree();

	conditionTree& operator = (const conditionTree& rhs) {
		type_ = rhs.type_;
		bind_values_index_ = rhs.bind_values_index_;
		expression_ = rhs.expression_;
		children_ = rhs.children_;
		bind_values_ = rhs.bind_values_;
		return *this;
	}

	/*
	 * description				create root of a tree
	 * @param conditions		values of raw that `$limit`,`$order` and tables' fields have been exluded out original raw
	 * @return					result.first is zero that indicates success,otherwise is failure.
	 * @author db.liu
	 * @date 2017/02/14
	*/
	static std::pair<int, conditionTree> createRoot(const Json::Value& conditions);

	const NodeType node_type() const {
		return type_;
	}

	void set_expression(const Json::Value& express) {
		expression_ = express;
	}

	expression_result parse_expression() {
		return parse_expression(expression_);
	}

	void set_bind_values_index(int fromindex) {
		bind_values_index_ = fromindex;
	}

	const Json::Value& expression() const {
		return expression_;
	}

	void add_child(const conditionTree& child) {
		children_.push_back(child);
	}

	iterator begin() {
		return children_.begin();
	}

	const_iterator begin() const {
		return children_.begin();
	}

	iterator end() {
		return children_.end();
	}

	const_iterator end() const {
		return children_.end();
	}

	size_t size() const {
		return children_.size();
	}

	// return a string that match pattern likes `id ?> 10 and name = 'chainsql'`
	const std::string asString() const;
	// return a string that match pattern likes `id > :id and name = :name`
	const std::pair<int, std::string> asConditionString() const;
	// bind once_temp_type with values,return {0, "success"} if success,otherwise return {-1, "bind value unsuccessfully"}
	const std::pair<int, std::string> bind_value(soci::details::once_temp_type& t);
private:
	int format_conditions(int style, std::string& conditions) const;
	int format_value(const BindValue& value, std::string& result) const;
	// bind values
	int bind_value(const BindValue& value, soci::details::once_temp_type& t);
	int bind_array(const std::vector<BindValue>& values, soci::details::once_temp_type& t);
	// parse values
	int parse_array(const Json::Value& j, std::vector<BindValue>& v);
	int parse_value(const Json::Value& j, BindValue& v);
	expression_result parse_expression(const Json::Value& e);

	NodeType type_;
	mutable int	bind_values_index_; // binding same fields may be failure in sqlite
	Json::Value expression_;
	std::vector<conditionTree> children_;
	mutable std::vector<std::vector<BindValue>> bind_values_;
};

namespace conditionParse {

	/*
	* description				Parse a list of conditions and generate a syntax tree
	* @param raw_value			values of raw that `$limit`,`$order` and tables' fields have been exluded from original raw
	* @param root				Root of a tree
	* @return					result.first is zero that indicates success,otherwise is failure.
	*							result.second is message
	* @author db.liu
	* @date 2017/02/14
	*/
	std::pair<int, std::string> parse_conditions(const Json::Value& raw_value, conditionTree& root);

	typedef std::function<bool(const conditionTree::expression_result&)> handlevaluecb;							// handle leaf node
	typedef std::function<int(const conditionTree&, int /*0-starting,1-processing,2-end*/)> handlenodecb;		// handle parent node
	bool judge(conditionTree& root, handlevaluecb cb);
	void traverse(conditionTree& root, handlevaluecb cb, handlenodecb ncb);
} // namespace helper

} // namespace ripple

#endif // RIPPLE_APP_MISC_SQLCONDITIONTREE_H_INCLUDED
