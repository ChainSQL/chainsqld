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

#include <vector>
#include <tuple>
#include <functional>

#include <boost/algorithm/string/trim.hpp>

#include <peersafe/app/sql/SQLDataType.h>
#include <peersafe/app/sql/SQLConditionTree.h>
#include <peersafe/app/sql/STTx2SQL.h>
#include <peersafe/app/sql/TxStore.h>
#include <peersafe/app/sql/SQLConditionTree.h>

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include <ripple/basics/base_uint.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/STArray.h>
#include <ripple/json/json_reader.h>
#include <ripple/json/impl/json_assert.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/net/RPCErr.h>

#define TABLE_PREFIX    "t_"
//#define SELECT_ITEM_LIMIT   200

namespace ripple {

class BuildField {

// properties of field
#define PK	0x00000001		// primary key
#define NN	0x00000002		// not null 
#define UQ	0x00000004		// unique
#define AI	0x00000008		// auto increase
#define ID	0x00000010		// index
#define DF	0x00000020		// has default value
#define FK	0x00000040		// foreign key
#define NU  0x00000080      // default null

public:
	explicit BuildField(const std::string& name)
	: name_(name)
	, compare_operator_("=")
	, value_()
	, foreigns_()
	, length_(0)
	, flag_(0) {

	}

	explicit BuildField(const BuildField& field) 
	: name_(field.name_)
	, compare_operator_(field.compare_operator_)
	, value_(field.value_)
	, foreigns_(field.foreigns_)
	, length_(field.length_)
	, flag_(field.flag_) {

	}

	~BuildField() {
	}
    
    static bool HaveSpecialCharacters(const std::string& name) {
        static std::vector<char> special_characters = {
            '~', '`', '!', '@', '#', '$', '%', '^',
            '&', '*', '(', ')', '-', '+', '=', '{',
            '}', '[', ']', '|', '\\', '/', ':', ';',
            '"', ',', '<', '>', '.', '?', ' ',
        };
        std::size_t size = name.size();
        for(std::size_t i = 0; i < size; i++) {
            auto result = std::find(special_characters.begin(), special_characters.end(), name[i]);
            if (result != special_characters.end())
                return true;
        }
        return false;
    }

	void SetFieldValue(const std::string& value, int flag) {
		value_ = FieldValue(value, flag);
	}

	void SetFieldValue(const std::string& value) {
		value_ = value;
	}

	void SetFieldValue(const int value) {
		value_ = value;
	}

	void SetFieldValue(const unsigned int value) {
		value_ = value;
	}

	void SetForeigns(const Json::Value& foreign) {
		foreigns_ = foreign;
	}

	void SetFieldValue(const float value) {
		value_ = value;
	}

	void SetFieldValue(const double value) {
		value_ = value;
	}

	void SetFieldValue(const InnerDateTime& value) {
		value_ = value;
	}

	void SetFieldValue(const InnerDate& value) {
		value_ = value;
	}

	void SetFieldValue(const InnerDecimal& value) {
		value_ = value;
	}

	void SetFieldValue(const int64_t value) {
		value_ = value;
	}

	void SetFieldValue(const InnerNull& value) {
		value_ = value;
	}

	void setCompareOperator(const std::string& op) {
		std::string trim_op = op;
		boost::algorithm::trim(trim_op);
		compare_operator_ = "=";
		if(boost::iequals(trim_op, "eq") || boost::iequals(trim_op, "=="))
			compare_operator_ = "="; 
		else if (boost::iequals(trim_op, "ne") || boost::iequals(trim_op, "!=") || boost::iequals(trim_op, "<>"))
			compare_operator_ = "!="; 
		else if (boost::iequals(trim_op, "gt") || boost::iequals(trim_op, ">"))
			compare_operator_ = ">"; 
		else if (boost::iequals(trim_op, "lt") || boost::iequals(trim_op, "<"))
			compare_operator_ = "<"; 
		else if (boost::iequals(trim_op, "ge") || boost::iequals(trim_op, ">="))
			compare_operator_ = ">="; 
		else if (boost::iequals(trim_op, "le") || boost::iequals(trim_op, "<="))
			compare_operator_ = "<="; 
	}

	std::string compareOpretor() const {
		return compare_operator_;
	}

	void SetLength(const int length) {
		length_ = length;
	}

	BuildField& operator =(const BuildField& field) {
		name_ = field.name_;
		compare_operator_ = field.compare_operator_;
		value_ = field.value_;
		foreigns_ = field.foreigns_;
		length_ = field.length_;
		flag_ = field.flag_;
		return *this;
	}

	const std::string& asString() {
		return value_.asString();
	}

	const std::string& asString() const {
		return value_.asString();
	}

	const int& asInt() {
		return value_.asInt();
	}

	const int& asInt() const {
		return value_.asInt();
	}

	const unsigned int& asUint() {
		return value_.asUint();
	}

	const unsigned int& asUint() const {
		return value_.asUint();
	}

	const int64_t& asInt64() {
		return value_.asInt64();
	}

	const int64_t& asInt64() const {
		return value_.asInt64();
	}

	const float& asFloat() {
		return value_.asFloat();
	}

	const float& asFloat() const {
		return value_.asFloat();
	}

	const double& asDouble() {
		return value_.asDouble();
	}

	const double& asDouble() const {
		return value_.asDouble();
	}

	InnerDecimal& asDecimal() {
		return value_.asDecimal();
	}

	const InnerDecimal& asDecimal() const {
		return value_.asDecimal();
	}

	const std::string& Name() {
		return name_;
	}

	const std::string& Name() const {
		return name_;
	}

	const Json::Value& Foreigns() {
		return foreigns_;
	}

	const Json::Value& Foreigns() const {
		return foreigns_;
	}

	bool isNumeric() {
		return value_.isNumeric();
	}

	bool isNumeric() const {
		return value_.isNumeric();
	}

	bool isInt() {
		return value_.isInt();
	}

	bool isInt() const {
		return value_.isInt();
	}

	bool isFloat() {
		return value_.isFloat();;
	}

	bool isFloat() const {
		return value_.isFloat();;
	}

	bool isDouble() {
		return value_.isDouble();
	}

	bool isDouble() const {
		return value_.isDouble();
	}

	bool isDecimal() {
		return value_.isDecimal();
	}

	bool isDecimal() const {
		return value_.isDecimal();
	}

	bool isInt64() {
		return value_.isInt64();
	}

	bool isInt64() const {
		return value_.isInt64();
	}

	bool isString() {
		return value_.isString();
	}

	bool isString() const {
		return value_.isString();
	}

	bool isVarchar() {
		return value_.isVarchar();
	}

	bool isVarchar() const {
		return value_.isVarchar();
	}

	bool isChar() {
		return value_.isChar();
	}

	bool isChar() const {
		return value_.isChar();
	}

	bool isText() {
		return value_.isText();
	}

	bool isText() const {
		return value_.isText();
	}

	bool isBlob() {
		return value_.isBlob();
	}

	bool isBlob() const {
		return value_.isBlob();
	}

	bool isDateTime() {
		return value_.isDateTime();
	}

	bool isDateTime() const {
		return value_.isDateTime();
	}

	bool isDate() {
		return value_.isDate();
	}

	bool isDate() const {
		return value_.isDate();
	}

	bool isNull() {
		return value_.isNull();
	}

	bool isCommand() {
		return value_.isCommand();
	}

	bool isLongText() {
		return value_.isLongText();
	}

	void SetPrimaryKey() {
		flag_ |= PK;
	}

	void SetNotNull() {
		flag_ |= NN;
	}

	void SetUnique() {
		flag_ |= UQ;
	}

	void SetAutoIncrease() {
		flag_ |= AI;
	}

	void SetIndex() {
		flag_ |= ID;
	}

	void SetDefault() {
		flag_ |= DF;
	}

	void SetForeignKey() {
		flag_ |= FK;
	}

    void SetDefaultNull()
    {
        flag_ |= NU;
    }

	bool isPrimaryKey() {
		return (flag_ & PK) == PK;
	}

	bool isForeigKey() {
		return (flag_ & FK) == FK;
	}

	bool isNotNull() {
		return (flag_ & NN) == NN;
	}

	bool isUnique() {
		return (flag_ & UQ) == UQ;
	}

	bool isAutoIncrease() {
		return (flag_ & AI) == AI;
	}

	bool isIndex() {
		return (flag_ & ID) == ID;
	}

	bool isDefault() {
		return (flag_ & DF) == DF;
	}

    bool isDefaultNull()
    {
        return (flag_ & NU) == NU;
    }

	int length() {
		return length_;
	}

private:
	std::string name_;	// field name
	std::string compare_operator_; // 
	FieldValue value_;	// value
	Json::Value foreigns_;
	int length_;		// value holds how much bytes
	int flag_;			// PK NN UQ AI Index default
};

std::pair<int, std::string> parseField(const Json::Value& json, BuildField& field) {
	//check blank space
	if (field.Name().find(' ') != std::string::npos) {
		return { -1, (boost::format("Field [%s] contains blank space.")
				 % field.Name()).str() };
	}
	Json::Value val;
	if (json.type() == Json::ValueType::objectValue) {
		val = json["value"];
		Json::Value op = json["op"];
		if (op.isString() == false)
			return{-1, "operator's type must be string"};
		field.setCompareOperator(op.asString());
	} else {
		val = json;
		field.setCompareOperator("eq"); // op's default value is eq
	}

	if (val.isString())
		field.SetFieldValue(val.asString());
	else if (val.isInt() || val.isIntegral())
		field.SetFieldValue(val.asInt());
	else if (val.isUInt())
		field.SetFieldValue(val.asUInt());
	else if (val.isDouble())
		field.SetFieldValue(val.asDouble());
	else if (val.isNull()){
		InnerNull value;
		field.SetFieldValue(value);
	}else {
		std::string error = (boost::format("Unknown type when parsing fields.[%s]") 
			%Json::jsonAsString(val)).str();
		return{ -1, error };
	}
	return {0, "success"};
}

class BuildSQL {
public:
	typedef std::vector<BuildField> AndCondtionsType;
	typedef std::vector<AndCondtionsType> OrConditionsType;

	enum BUILDTYPE
	{
		BUILD_UNKOWN,
		BUILD_CREATETABLE_SQL = 1,
		BUILD_DROPTABLE_SQL = 2,
		BUILD_RENAMETABLE_SQL = 3,
		BUILD_ASSIGN_SQL = 4,
		BUILD_CANCEL_ASSIGN_SQL = 5,
		BUILD_INSERT_SQL = 6,
		BUILD_SELECT_SQL = 7,
		BUILD_UPDATE_SQL = 8,
		BUILD_DELETE_SQL = 9,
		BUILD_ASSERT_STATEMENT = 10,
        BUILD_RECREATE_SQL = 12,
		BUILD_ALTER_ADD_SQL = 14,
		BUILD_ALTER_DEL_SQL = 15,
		BUILD_ALTER_MOD_SQL = 16,
		BUILD_CREATE_INDEX_SQL = 17,
		BUILD_DROP_INDEX_SQL = 18,
		BUILD_EXIST_TABLE = 1000,
		BUILD_NOSQL
	};

	explicit BuildSQL() {
	}

	virtual ~BuildSQL() {
	}

	virtual void AddTable(const std::string& tablename) = 0;
	virtual void AddTable(const Json::Value& table) = 0;
	virtual void AddField(const BuildField& field) = 0;
	virtual void AddCondition(const AndCondtionsType& condition) = 0;
	virtual void AddCondition(const Json::Value& condition) = 0;
	virtual void AddLimitCondition(const Json::Value& limit) = 0;
	virtual void AddOrderCondition(const Json::Value& order) = 0;
	virtual void AddGroupByCondition(const Json::Value& group) = 0;
	virtual void AddHavingCondition(const Json::Value& having) = 0;
	virtual void AddJoinOnCondition(const Json::Value& join) = 0;
	virtual int parseExtraCondition(const Json::Value& object, 
		std::function<void(const Json::Value& limit)> handlelimit,
		std::function<void(const Json::Value& order)> handleorder,
		std::function<void(const Json::Value& join)> handlejoin,
		std::function<void(const Json::Value& group)> handlegroupby,
		std::function<void(const Json::Value& having)> handlehaving,
		std::function<void(const Json::Value& error)> handleerror) = 0;

	virtual const std::vector<std::string>& Tables() const = 0;
	virtual const std::vector<BuildField>&  Fields() const = 0;
	virtual const OrConditionsType& Conditions() const = 0;

	virtual BUILDTYPE build_type(BUILDTYPE type) = 0;
	virtual std::string asString() = 0;
	virtual int execSQL() = 0;
	virtual void clear() = 0;

	virtual std::pair<int, std::string> last_error()     = 0;
    virtual void set_last_error(const std::pair<int, std::string> &error) = 0;
    
    virtual void batch_insert(const uint32_t batch) = 0;
    virtual uint32_t batch_insert() const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// class BuildMySQL
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DisposeSQL {
public:
	class DisposeError {
	public:
		DisposeError()
		: error_code_(0)
		, error_message_("success") {

		}

		DisposeError(int error_code, const std::string& error_msg)
		: error_code_(error_code)
		, error_message_(error_msg) {

		}

		~DisposeError() {

		}

		DisposeError& operator=(const DisposeError& other) {
			error_code_ = other.error_code_;
			error_message_ = other.error_message_;
			return *this;
		}

		const std::pair<int, std::string> value() const {
			std::pair<int, std::string> v{error_code_, error_message_};
			return v;
		}

	private:
		int error_code_;
		std::string error_message_;
	};

	DisposeSQL(BuildSQL::BUILDTYPE type, DatabaseCon* dbconn)
	: tables_()
	, tables_obj_()
	, fields_()
	, orders_()
	, limit_()
	, group_()
	, having_()
    , indi_null(soci::i_null)
    , batch_insert_(0)
	, build_type_(type)
	, index_(0)	
	, conditions_()
	, db_conn_(dbconn)
	, condition_(Json::ValueType::nullValue)
	, join_on_condition_(Json::ValueType::nullValue)
	, last_error_() {
	}

	virtual ~DisposeSQL() {

	}

	void AddTable(const std::string& tablename) {
		tables_.push_back(tablename);
	}

	void AddTable(const Json::Value& table) {
		tables_obj_.push_back(table);
	}

	void AddField(const BuildField& field) {
		fields_.push_back(field);
	}

	void AddCondition(const BuildSQL::AndCondtionsType& condition) {
		conditions_.push_back(condition);
	}

	void AddCondition(const Json::Value& condition) {
		condition_ = condition;
	}

	void AddLimitCondition(const Json::Value& limit) {
		limit_ = limit;
	}

	void AddGroupByCondition(const Json::Value& group) {
		group_ = group;
	}

	void AddHavingCondition(const Json::Value& having) {
		having_ = having;
	}

	bool isIncompleteExtKeyWord(const std::string& keyword) {
		if (keyword.size() == 5
			&& keyword[0] == 'l'
			&& keyword[1] == 'i'
			&& keyword[2] == 'm'
			&& keyword[3] == 'i'
			&& keyword[4] == 't') {
			return true;
		}
		else if (keyword.size() == 5
			&& keyword[0] == 'o'
			&& keyword[1] == 'r'
			&& keyword[2] == 'd'
			&& keyword[3] == 'e'
			&& keyword[4] == 'r') {
			return true;
		}
		else if (keyword.size() == 4 
			&& keyword[0] == 'j'
			&& keyword[1] == 'o'
			&& keyword[2] == 'i'
			&& keyword[3] == 'n') {
			return true;
		}
		else if (keyword.size() == 5 
			&& keyword[0] == 'g'
			&& keyword[1] == 'r'
			&& keyword[2] == 'o'
			&& keyword[3] == 'u'
			&& keyword[4] == 'p') {
			return true;
		}
		else if (keyword.size() == 6
			&& keyword[0] == 'h'
			&& keyword[1] == 'a'
			&& keyword[2] == 'v'
			&& keyword[3] == 'i'
			&& keyword[4] == 'n'
			&& keyword[5] == 'g') {

		}
		return false;
	}
	/*
	* return int	0 parsing limit or order is successfull
	*				-1 parsing limit or order is unsuccessfull
	*				1 parsing not-limit and not-order,meant that
	*				it's where-conditions
	*/
	int parseExtraCondition(const Json::Value& object, 
		std::function<void(const Json::Value& limit)> handlelimit,
		std::function<void(const Json::Value& order)> handleorder,
		std::function<void(const Json::Value& join)> handlejoin,
		std::function<void(const Json::Value& group)> handlegroupby,
		std::function<void(const Json::Value& having)> handlehaving,
		std::function<void(const Json::Value& error)> handleerror) {
		//bool hasLimit = false;
		//bool hasOrder = false;
		int code = 1;
		Json::Value error;
		std::vector<std::string> keys = object.getMemberNames();
		size_t size = keys.size();
		for (size_t idx = 0; idx < size; idx++) {
			std::string& key_name = keys[idx];
			if (isIncompleteExtKeyWord(key_name)) {
				code = -1;
				if (handleerror) {
					error["result"] = -1;
					error["message"] = (boost::format("`%s` can't be recognized by ChainSQL")
						% key_name).str();
					handleerror(error);
				}
				return code;
			}
			
			Json::Value v = object[key_name];
			if (boost::iequals(key_name, "$limit")) {
				if (v.isObject() && v["index"].isInt() && v["total"].isInt()) {
					if (handlelimit) {
						handlelimit(v);
					}
					code = 0;
				}
				else {
					code = -1;
				}
			} else if (boost::iequals(key_name, "$order")) {
				if (v.isArray()) {
					Json::UInt length = v.size();
					for (Json::UInt i = 0; i < length; i++) {
						Json::Value& order = v[i];
						if (handleorder)
							handleorder(order);
					}
					code = 0;
				}
				else {
					code = -1;
				}
			}
			else if (boost::iequals(key_name, "$join")) {
				if (v.isObject()) {
					if (handlejoin)
						handlejoin(v);
					code = 0;
				}
				else {
					code = -1;
				}
			}
			else if (boost::iequals(key_name, "$group")) {
				if (v.isArray()) {
					if (handlegroupby)
						handlegroupby(v);
					code = 0;				
				}
				else {
					code = -1;
				}
			}
			else if (boost::iequals(key_name, "$having")) {
				if (v.isObject()) {
					if (handlehaving)
						handlehaving(v);
					code = 0;				
				}
				else {
					code = -1;
				}
			}
			
			if (code == 0) {
				if (handleerror) {
					error["result"] = 0;
					error["message"] = "success";
					handleerror(error);
				}
			}
			else {
				if (handleerror) {
					error["result"] = -1;
					error["message"] = (boost::format("Parsing extraCondition(s) unsuccessfully.[%s]")
						% Json::jsonAsString(v)).str();
					handleerror(error);
				}
			}
		}
		
		return code;
	}

	void AddOrderCondition(const Json::Value& order) {
		orders_.push_back(order);
	}

	void AddJoinOnCondition(const Json::Value& join) {
		join_on_condition_ = join;
	}

	const std::vector<std::string>& Tables() const {
		return tables_;
	}

	const std::vector<BuildField>&  Fields() const {
		return fields_;
	}

	const BuildSQL::OrConditionsType& Conditions() const {
		return conditions_;
	}

	BuildSQL::BUILDTYPE build_type(BuildSQL::BUILDTYPE type) {
		BuildSQL::BUILDTYPE old = build_type_;
		build_type_ = type;
		return old;
	}

	std::string asString() {
		std::string sql;
		switch (build_type_)
		{
		case BuildSQL::BUILD_CREATETABLE_SQL:
			sql = build_createtable_sql();
			break;
		case BuildSQL::BUILD_DROPTABLE_SQL:
			sql = build_droptable_sql();
			break;
		case BuildSQL::BUILD_RENAMETABLE_SQL:
			sql = build_renametable_sql();
			break;
		case BuildSQL::BUILD_INSERT_SQL:
			sql = build_insert_sql();
			break;
		case BuildSQL::BUILD_UPDATE_SQL:
			sql = build_update_sql();
			break;
		case BuildSQL::BUILD_DELETE_SQL:
			sql = build_delete_sql();
			break;
		case BuildSQL::BUILD_SELECT_SQL:
			sql = build_select_sql();
			break;
		case BuildSQL::BUILD_EXIST_TABLE:
			sql = build_exist_sql();
			break;
		case BuildSQL::BUILD_ALTER_ADD_SQL:
			sql = build_addcolumn_sql();
			break;
		case BuildSQL::BUILD_ALTER_DEL_SQL:
            sql = build_delcolumn_sql();
			break;
		case BuildSQL::BUILD_ALTER_MOD_SQL:
			sql = build_modifycolumn_sql();
			break;
		case BuildSQL::BUILD_CREATE_INDEX_SQL:
			sql = build_createindex_sql();
			break;
		case BuildSQL::BUILD_DROP_INDEX_SQL:
			sql = build_dropindex_sql();
			break;
		default:
			break;
		}
		return sql;
	}

	int execSQL() {
		int ret = -1;

		if (db_conn_ == nullptr)
			return ret;

		switch (build_type_)
		{
		case BuildSQL::BUILD_CREATETABLE_SQL:
			ret = execute_createtable_sql();
			break;
		case BuildSQL::BUILD_DROPTABLE_SQL:
			ret = execute_droptable_sql();
			break;
		case BuildSQL::BUILD_RENAMETABLE_SQL:
			ret = execute_renametable_sql();
			break;
		case BuildSQL::BUILD_INSERT_SQL:
			ret = execute_insert_sql();
			break;
		case BuildSQL::BUILD_UPDATE_SQL:
			ret = execute_update_sql();
			break;
		case BuildSQL::BUILD_DELETE_SQL:
			ret = execute_delete_sql();
			break;
		case BuildSQL::BUILD_SELECT_SQL:
			break;
        case BuildSQL::BUILD_RECREATE_SQL:
            ret = execute_createtable_sql();
            break;
		case BuildSQL::BUILD_ALTER_ADD_SQL:
			ret = execute_addcolumn_sql();
			break;
		case BuildSQL::BUILD_ALTER_DEL_SQL:
			ret = execute_delcolumn_sql();
			break;
		case BuildSQL::BUILD_ALTER_MOD_SQL:
			ret = execute_modifycolumn_sql();
			break;
		case BuildSQL::BUILD_CREATE_INDEX_SQL:
			ret = execute_createindex_sql();
			break;
		case BuildSQL::BUILD_DROP_INDEX_SQL:
			ret = execute_dropindex_sql();
			break;
		default:
			break;
		}

		// fix an issue that we can't catch an exception on top-level,
		// because destructor of one-temp-type driver to execute actual SQL-engine API.
		// however destructor can catch an exception but can't throw an exception that was catched by destructor.
		if (db_conn_->getSession().last_error().first != 0) {
			last_error(db_conn_->getSession().last_error());
			ret = -1;
		}

		return ret;
	}

	void clear() {
		tables_.clear();
		fields_.clear();
		conditions_.clear();
	}

	void last_error(const std::pair<int,std::string>& error) {
		last_error_ = std::move(DisposeError(error.first, error.second));
	}

	const DisposeError& last_error() const {
		return last_error_;
	}
    
    void batch_insert(const uint32_t batch) {
        batch_insert_ = batch;
    }
    
    uint32_t batch_insert() const {
        return batch_insert_;
    }

protected:
	DisposeSQL() {};
	virtual std::size_t analyse_fields_and_build_colunms(std::vector<std::string>& columns) = 0;
	virtual std::string build_createtable_sql() = 0;
	virtual int execute_createtable_sql() = 0;
	virtual std::string build_exist_sql() = 0;

	std::vector<std::string> tables_;
	std::vector<Json::Value> tables_obj_;
	std::vector<BuildField> fields_;
	std::vector<Json::Value> orders_;
	Json::Value limit_;
	Json::Value group_;
	Json::Value having_;
	soci::indicator indi_null;
    uint32_t batch_insert_;

	int execute_droptable_sql() {
		if (tables_.size() == 0)
			return -1;
		try {
            bool bExist = STTx2SQL::IsTableExistBySelect(db_conn_, tables_[0]);
            if (bExist)
            {
                LockedSociSession sql = db_conn_->checkoutDb();
                *sql << build_droptable_sql();
            }			
		}
		catch (soci::soci_error& e) {
			last_error(std::make_pair<int, std::string>(-1, e.what()));
			return -1;
		}

		return 0;
	}

private:

	std::string build_droptable_sql() {
		std::string sql;
		if (tables_.size() == 0)
			return sql;
		sql = (boost::format("drop table %s") % tables_[0]).str();
		return sql;
	}

	std::string build_renametable_sql() {
		std::string sql;
		if (tables_.size() == 0)
			return sql;
		sql = (boost::format("rename table %s to %s") 
			% tables_[0] 
			% tables_[1]).str();
		return sql;
	}

	int execute_renametable_sql() {
		if (tables_.size() == 0) {
			last_error(std::make_pair<int,std::string>(-1, "Table miss when executing rename table"));
			return -1;
		}
		
		try {
			LockedSociSession sql = db_conn_->checkoutDb();
			*sql << "rename table :old to :new", soci::use(tables_[0]), soci::use(tables_[1]);
		}
		catch (soci::soci_error& e) {
			last_error(std::make_pair<int,std::string>(-1, e.what()));
			return -1;
		}
		return 0;
	}

	std::string build_insert_sql() {
		std::string sql;
		if (tables_.size() == 0) {
			last_error(std::make_pair<int,std::string>(-1, "Table miss when building sql"));
			return sql;
		}

		if (fields_.size() == 0) {
			if (last_error().value().first == 0)
			{
				last_error(std::make_pair<int, std::string>(
					-1, "Fields are empty when building create-sql"));
			}
			return sql;
		}
        
        uint32_t inserting_counts = batch_insert();
        if(inserting_counts == 0) {
            last_error(std::make_pair<int,std::string>(-1, "batch_insert is invalid"));
            return sql;
        }

		std::string& tablename = tables_[0];
        uint32_t fields = fields_.size() / inserting_counts;
		std::string fields_str;
		std::string values_str;
		for (uint32_t idx = 0; idx < fields; idx++) {
			BuildField& field = fields_[idx];
			fields_str += field.Name();
			if (idx != fields - 1) {
				fields_str += ",";
			}
		}
        for(uint32_t r = 0; r < inserting_counts; r++) {
            values_str += "(";
            for (uint32_t idx = 0; idx < fields; idx++) {
                BuildField& field = fields_[idx];
                if (field.isString() || field.isVarchar()
                    || field.isBlob() || field.isText()
                    || field.isLongText())
                    values_str += (boost::format("\"%1%\"") % field.asString()).str();
                else if (field.isInt())
                    values_str += (boost::format("%d") % field.asInt()).str();
                else if (field.isFloat())
                    values_str += (boost::format("%f") % field.asFloat()).str();
                else if (field.isDouble() || field.isDecimal())
                    values_str += (boost::format("%f") % field.asDouble()).str();
                else if (field.isInt64() || field.isDateTime())
                    values_str += (boost::format("%1%") % field.asInt64()).str();
                else if (field.isNull())
                    values_str += "NULL";
                
                if (idx != fields - 1) {
                    values_str += ",";
                }
            }
            
            values_str += ")";
            
            if (r == inserting_counts - 1) {
                values_str += ";";
            } else {
                values_str += ",";
            }
            
        }
		sql = (boost::format("insert into %s (%s) values %s")
			%tablename
			%fields_str
			%values_str).str();
		return sql;
	}

	int execute_insert_sql() {
		std::string sql_str;
		if (tables_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when executing sql"));
			return -1;
		}

		if (fields_.size() == 0) {
			if (last_error().value().first == 0)
			{
				last_error(std::make_pair<int, std::string>(
					-1, "Fields are empty when building create-sql"));
			}
			return -1;
		}
        
        uint32_t inserting_records = batch_insert();
        if (inserting_records == 0) {
            last_error(std::make_pair<int, std::string>(-1, "batch_insert is invalid"));
            return -1;
        }

		std::string& tablename = tables_[0];
        uint32_t field_counts = fields_.size() / inserting_records;
		std::string fields_str;
		std::string values_str;
		for (uint32_t idx = 0; idx < field_counts; idx++) {
			BuildField& field = fields_[idx];
			fields_str += field.Name();
			if (idx != field_counts - 1) {
				fields_str += ",";
			}
		}
        
        for(uint32_t r = 0; r < inserting_records; r++) {
            uint32_t base = r * field_counts;
            values_str += "(";
            for (uint32_t idx = 0; idx < field_counts; idx++) {
                values_str += ":" + std::to_string(base + idx + 1);
                
                if (idx != field_counts - 1) {
                    values_str += ",";
                }
            }
            values_str += ")";
            
            if (r == inserting_records - 1) {
                values_str += ";";
            } else {
                values_str += ",";
            }
        }
        
		sql_str = (boost::format("insert into %s (%s) values %s")
			% tablename
			%fields_str
			%values_str).str();

        LockedSociSession sql = db_conn_->checkoutDb();
		{
			auto tmp = *sql << sql_str;
			bind_fields_value(tmp);
		}
		// fix an issue that we can't catch an exception on top-level,
		// beacause desctructor of one-temp-type driver to execute actual SQL-engine API.
		// however destructor can catch an exception but can't throw an exception that was catched by destructor.
		//if (db_conn_->getSession().last_error().first != 0) {
		//	last_error(db_conn_->getSession().last_error());
		//	return -1;
		//}
		return 0;
	}

	std::string build_update_sql() {
		std::string sql;
		if (tables_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when building update-sql"));
			return sql;
		}

		if (fields_.size() == 0) {
			if (last_error().value().first == 0)
			{
				last_error(std::make_pair<int, std::string>(
					-1, "Fields are empty when building create-sql"));
			}
			return sql;
		}

		std::string& tablename = tables_[0];
		std::string update_fields;

		for (size_t idx = 0; idx < fields_.size(); idx++) {
			BuildField& field = fields_[idx];
			if(field.isString() || field.isVarchar()
				|| field.isBlob() || field.isText() || field.isLongText() ) {
				update_fields += (boost::format("%1%=\"%2%\"")
					%field.Name()
					%field.asString()).str();
			} else if (field.isInt()) {
				update_fields += (boost::format("%1%=%2%")
					%field.Name()
					%field.asInt()).str();
			} else if (field.isFloat()) { 
				update_fields += (boost::format("%s=%f")
					%field.Name()
					%field.asFloat()).str();
			} else if (field.isDouble() || field.isDecimal()) {
				update_fields += (boost::format("%s=%f")
					%field.Name()
					%field.asDouble()).str();
			} else if (field.isDateTime() || field.isInt64()) {
				update_fields += (boost::format("%1%=%2%")
					%field.Name()
					%field.asInt64()).str();
			}else if (field.isCommand()) {
				update_fields += (boost::format("%1%=%2%")
					% field.Name()
					% field.asString()).str();
			}
			else if (field.isNull()) {
				update_fields += (boost::format("%1%=%2%")
					% field.Name()
					% "NULL").str();
			}
			else if (field.isNull()) {
				update_fields += (boost::format("%1%=%2%")
					% field.Name()
					% "NULL").str();
			}

			if (idx != fields_.size() - 1)
				update_fields += ",";
		}

		auto conditions = build_conditions();
		if (conditions.first != 0) {
			last_error(std::make_pair<int, std::string>(-1, "Building conditions is unsuccessful when building update-sql"));
			return conditions.second;
		}
		else {
			if (conditions.second.empty()) {
				sql = (boost::format("update %s set %s")
					%tablename
					%update_fields).str();
			}
			else {
				sql = (boost::format("update %s set %s where %s")
					%tablename
					%update_fields
					%conditions.second).str();
			}
		}
		return sql;
	}

	int execute_update_sql() {
		std::string sql_str;
		if (tables_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when executing update-sql"));
			return -1;
		}

		if (fields_.size() == 0) {
			if (last_error().value().first == 0)
			{
				last_error(std::make_pair<int, std::string>(
					-1, "Fields are empty when building create-sql"));
			}
			return -1;
		}

		std::string& tablename = tables_[0];
		std::string update_fields;

		std::string update_command_fields;
		
		index_ = 0;
		for (size_t idx = 0; idx < fields_.size(); idx++) {
			BuildField& field = fields_[idx];
			if (field.isCommand()) {
				update_command_fields = field.Name() + std::string("=") + field.asString();
				continue;
			}
			update_fields += field.Name() + std::string("=:") + std::to_string(++index_);
			if (idx != fields_.size() - 1)
				update_fields += ",";
		}

		auto conditions = build_execute_conditions();
		if (std::get<0>(conditions) == 0) {
			const std::string& c = std::get<1>(conditions);

			// similar to this sql statement : update table_01 set age=1,name=concat("a_",name);
			if (!update_command_fields.empty()) {
				std::string extraComma = (update_fields.back() == ',' ? std::string("") : std::string(","));
				update_fields += extraComma + update_command_fields;
			}

			if (c.empty()) {
				sql_str = (boost::format("update %s set %s")
					% tablename
					% update_fields).str();
			}
			else {
				sql_str = (boost::format("update %s set %s where %s")
					% tablename
					% update_fields
					% c).str();
			}

			LockedSociSession sql = db_conn_->checkoutDb();
			{
				auto tmp = *sql << sql_str;
				bind_fields_value(tmp);
				if (c.empty() == false)
					if (std::get<2>(conditions).bind_value(tmp).first != 0) {
						last_error(std::make_pair<int, std::string>(-1,
							"Binding values is unsuccessful when executing update-sql"));
						return -1;
					}
			}
			// fix an issue that we can't catch an exception on top-level,
			// because destructor of one-temp-type driver to execute actual SQL-engine API.
			// however destructor can catch an exception but can't throw an exception that was catched by destructor.
			//if (db_conn_->getSession().last_error().first != 0) {
			//	last_error(db_conn_->getSession().last_error());
			//	return -1;
			//}
		}
		else {
			return -1;
		}
		
		return 0;
	}

	std::string build_delete_sql() {
		std::string sql;
		if (tables_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when building delete-sql"));
			return sql;
		}

		std::string& tablename = tables_[0];

		auto conditions = build_conditions();
		if (conditions.first != 0) {
			return conditions.second;
		}
		else {
			if (conditions.second.empty()) {
				sql = (boost::format("delete from %s")
					%tablename).str();
			}
			else {
				sql = (boost::format("delete from %s where %s")
					%tablename
					%conditions.second).str();
			}
		}

		return sql;
	}

	int execute_delete_sql() {
		std::string sql_str;
		if (tables_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when executing delete-sql"));
			return -1;
		}

		std::string& tablename = tables_[0];
		index_ = 0;
		auto conditions = build_execute_conditions();
		if (std::get<0>(conditions) == 0) {
			const std::string& c = std::get<1>(conditions);
			if (c.empty()) {
				sql_str = (boost::format("delete from %s")
					% tablename).str();
			}
			else {
				sql_str = (boost::format("delete from %s where %s")
					% tablename
					%c).str();
			}

			LockedSociSession sql = db_conn_->checkoutDb();
			{
				auto tmp = *sql << sql_str;
				if (c.empty() == false)
					if (std::get<2>(conditions).bind_value(tmp).first != 0) {
						last_error(std::make_pair<int, std::string>(-1, 
							"binding values is unsuccessfull when executing delete-sql"));
						return -1;
					}
			}
			// fix an issue that we can't catch an exception on top-level,
			// because destructor of one-temp-type driver to execute actual SQL-engine API.
			// however destructor can catch an exception but can't throw an exception that was catched by destructor.
			//if (db_conn_->getSession().last_error().first != 0) {
			//	last_error(db_conn_->getSession().last_error());
			//	return -1;
			//}
		}
		else {
			return -1;
		}
		return 0;
	}

	std::string build_select_sql() {
		std::string sql;
		if (tables_.size() == 0 && tables_obj_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when building select-sql"));
			return sql;
		}

		bool is_union_query = false;
		std::string tablename;
		if(tables_.empty() == false) {
			tablename = tables_[0];
		}
		else if (tables_obj_.empty() == false) {
			if (tables_obj_.size() == 1) {
				// single table query
				if (tables_obj_[0U].isMember("Table") == false) {
					last_error(std::make_pair<int, std::string>(-1,
						(boost::format("Table's element maybe malformed. `%1%`")
							% Json::jsonAsString(tables_obj_[0U])).str()));
					return sql;
				}

				const Json::Value& table_obj = tables_obj_[0U]["Table"];
				if (table_obj.isMember("TableName") == false) {
					last_error(std::make_pair<int, std::string>(-1,
						(boost::format("malformed table. %1%") %Json::jsonAsString(table_obj)).str()));
					return sql;
				}

				const Json::Value& tablename_obj = table_obj["TableName"];
				if (tablename_obj.isString() == false) {
					last_error(std::make_pair<int, std::string>(-1,
						(boost::format("Tablename must be string. %1%") %Json::jsonAsString(table_obj)).str()));
					return sql;
				}

				tablename = std::string(TABLE_PREFIX) + tablename_obj.asString();
			}
			else if (tables_obj_.size() == 2) {
				// only support union query of two tables
				if (tables_obj_[0U].isMember("Table") == false
					|| tables_obj_[1U].isMember("Table") == false) {
					last_error(std::make_pair<int, std::string>(-1,
						(boost::format("Table's element maybe malformed. `%1%` or `%2%` ")
							% Json::jsonAsString(tables_obj_[0U])
							% Json::jsonAsString(tables_obj_[1U])).str()));
					return sql;
				}

				const Json::Value& first_table_obj = tables_obj_[0U]["Table"];
				const Json::Value& second_table_obj = tables_obj_[1U]["Table"];

				// first element must have `$join`, but second element's `$join` will be ignored.
				if (first_table_obj.isMember("join") == false) {
					last_error(std::make_pair<int, std::string>(-1,
						(boost::format("First element must specify type of the join. %1%") 
							%Json::jsonAsString(first_table_obj)).str()));
					return sql;
				}

				const Json::Value& json_type_obj = first_table_obj["join"];
				if (json_type_obj.isString() == false
					&& json_type_obj.asString().empty() == false) {
					last_error(std::make_pair<int, std::string>(-1,
						(boost::format("type of join must be string or not empty. %1%") 
							%Json::jsonAsString(first_table_obj)).str()));
					return sql;
				}
				const std::string join_type = json_type_obj.asString();
				if (boost::iequals(join_type, "inner") == false
					&& boost::iequals(join_type, "left") == false
					&& boost::iequals(join_type, "right") == false) {
					last_error(std::make_pair<int, std::string>(-1,
						(boost::format("type of join must be inner or left or right. current type is %1%") 
							%Json::jsonAsString(join_type)).str()));
					return sql;
				}

				if (first_table_obj.isMember("TableName") == false
					|| second_table_obj.isMember("TableName") == false) {
					last_error(std::make_pair<int, std::string>(-1,
						(boost::format("malformed table. %1% or %2%") 
							%Json::jsonAsString(first_table_obj)
							%Json::jsonAsString(second_table_obj)).str()));
					return sql;
				}

				const Json::Value& first_tablename_obj = first_table_obj["TableName"];
				const Json::Value& second_tablename_obj = second_table_obj["TableName"];
				if (first_tablename_obj.isString() == false || second_tablename_obj.isString() == false) {
					last_error(std::make_pair<int, std::string>(-1,
						(boost::format("Tablename must be string. %1% or %2%") 
							%Json::jsonAsString(first_table_obj)
							%Json::jsonAsString(second_table_obj)).str()));
					return sql;
				}

				std::string first_tablename = std::string(TABLE_PREFIX) + first_tablename_obj.asString();
				tablename += first_tablename;
				if (first_table_obj.isMember("Alias")
					&& first_table_obj["Alias"].isString()) {
					std::string alias = first_table_obj["Alias"].asString();
					if (alias.empty() == false) {
						tablename += " ";
						tablename += alias;
					}
				}

				tablename += " ";
				tablename += join_type + " join ";
				std::string second_tablename = std::string(TABLE_PREFIX) + second_tablename_obj.asString();
				tablename += second_tablename;
				if (second_table_obj.isMember("Alias")
					&& second_table_obj["Alias"].isString()) {
					std::string alias = second_table_obj["Alias"].asString();
					if (alias.empty() == false) {
						tablename += " ";
						tablename += alias;
					}
				}

				is_union_query = true;
			}
		}

		std::string fields;
		if (fields_.size() == 0)
			fields = " * ";
		else {
			for (size_t idx = 0; idx < fields_.size(); idx++) {
				BuildField& field = fields_[idx];
				fields += field.Name();
				if (idx != fields_.size() - 1)
					fields += ",";
			}
		}

		// parsing on-conditions
		std::string join_on_condition_str;
		if (is_union_query) {
			auto join_on_condition = build_join_on_condition();
			if (join_on_condition.first != 0) {
				last_error(std::make_pair<int, std::string>(-1,
					std::move(join_on_condition.second)));
				return "";
			}

			join_on_condition_str = join_on_condition.second;
			if (join_on_condition_str.empty()) {
				last_error(std::make_pair<int, std::string>(-1,
					"On-conditions must be specified when executing union query"));
				return sql;
			}
		}

		// parsing where-condition
		auto conditions = build_conditions();
		if (conditions.first != 0) {
			last_error(std::make_pair<int, std::string>(-1,
				std::move(conditions.second)));
			return "";
		}
		std::string where_conditions_str = conditions.second;

		if(is_union_query)
		{
			if (where_conditions_str.empty()) {
				sql = (boost::format("select %s from %s on %s")
					%fields
					%tablename
					%join_on_condition_str).str();
			}
			else {
				sql = (boost::format("select %s from %s on %s where %s")
					%fields
					%tablename
					%join_on_condition_str
					%where_conditions_str).str();
			}
		}
		else {
			if (where_conditions_str.empty()) {
				sql = (boost::format("select %s from %s")
					%fields
					%tablename).str();
			}
			else {
				sql = (boost::format("select %s from %s where %s")
					%fields
					%tablename
					%where_conditions_str).str();
			}
		}

		// group by
		if (group_.isArray()) {
			std::string groupby;
			Json::UInt size = group_.size();
			for (Json::UInt index = 0; index < size; index++) {
				Json::Value& v = group_[index];
				if (v.isString() == false)
					continue;
				if (index == 0) {
					groupby = (boost::format(" group by %s") %v.asString()).str();
				}
				else {
					groupby += (boost::format(",%s") %v.asString()).str();
				}
			}

			if (groupby.empty() == false)
				sql += groupby;
		}

		if (having_.isArray() && having_.size() > 0) {
			auto having = build_having_conditions();
			if (having.first == 0 && having.second.empty() == false) {
				sql += (boost::format(" having %s") %having.second).str();
			}
		}

		std::string orders;
		for (size_t i = 0; i < orders_.size(); i++) {
			const Json::Value& order = orders_[i];
			if (order.isObject() == false)
				break;
			const std::vector<std::string>& ks = order.getMemberNames();
			const std::string& field_name = ks[0];
			const Json::Value& value = order[field_name];
			std::string order_by = "ASC";
			if (value.isString()) {
				const std::string& order = value.asString();
				if (boost::iequals(order, "asc") == false && boost::iequals(order, "desc") == false) {
					order_by = "ASC";
				} else {
					order_by = value.asString();
				}
			}
			else if (value.isNumeric()) {
				if(value.asInt() == 1)
					order_by = "ASC";
				else if (value.asInt() == -1)
					order_by = "DESC";
				else
					order_by = "ASC";
			}
			//std::string order_by = order[field_name].asString();
			if (i == 0) {
				orders = (boost::format(" order by %s %s")
					%field_name %order_by).str();
			} else {
				orders += (boost::format(",%s %s")
					%field_name %order_by).str();
			}
		}

		if (orders.empty() == false)
			sql += orders;

		if (limit_.isObject()) {
			std::string limit;
			const std::vector<std::string>& keys = limit_.getMemberNames();
			if (keys.size() == 2
				&& boost::iequals(keys[0], "index")
				&& boost::iequals(keys[1], "total")) {
				limit = (boost::format(" limit %d,%d")
					% limit_["index"].asInt() % limit_["total"].asInt()).str();
			}

			if (limit.empty() == false)
				sql += limit;
		}
		return sql;
	}

	std::string build_addcolumn_sql() {
		std::string& tablename = tables_[0];
		std::vector<std::string> columns;
		analyse_fields_and_build_colunms(columns);

		std::string columns_str;
		for (size_t idx = 0; idx < columns.size(); idx++) {
			if (idx == 0) {
				columns_str += "ADD ";
			}

			std::string& element = columns[idx];
			if (element.compare(",") == 0) {
				columns_str += element;
				columns_str += "ADD ";
			}
			else {
				columns_str += element;
				columns_str += " ";
			}

		}

		std::string sql = (boost::format("ALTER TABLE %s %s")
			% tablename
			% columns_str).str();
		return sql;
	}

	std::string build_delcolumn_sql() {
		std::string columns;
		for (size_t idx = 0; idx < fields_.size(); idx++) {
			BuildField& field = fields_[idx];
			columns += "DROP ";
			columns += field.Name();
			if (idx != fields_.size() - 1) {
				columns += ",";
			}
		}

		std::string& tablename = tables_[0];
		std::string sql = (boost::format("ALTER TABLE %s %s")
			% tablename
			% columns).str();
		return sql;
	}

	std::string build_modifycolumn_sql() {
		std::string& tablename = tables_[0];
		std::vector<std::string> columns;
		analyse_fields_and_build_colunms(columns);

		std::string columns_str;
		for (size_t idx = 0; idx < columns.size(); idx++) {
			if (idx == 0) {
				columns_str += "MODIFY ";
			}

			std::string& element = columns[idx];
			if (element.compare(",") == 0) {
				columns_str += element;
				columns_str += "MODIFY ";
			}
			else {
				columns_str += element;
				columns_str += " ";
			}

		}

		std::string sql = (boost::format("ALTER TABLE %s %s")
			% tablename
			% columns_str).str();
		return sql;
	}

	std::string build_createindex_sql() {
		std::string index_name;
		std::string columns;
		for (size_t idx = 0; idx < fields_.size(); idx++) {
			BuildField& field = fields_[idx];
			if (field.isIndex()) {
				index_name = field.Name();
			}
			else {
				columns += field.Name();
				if (idx != fields_.size() - 1) {
					columns += ",";
				}
			}
		}

		std::string& tablename = tables_[0];
		std::string sql = (boost::format("CREATE INDEX %s ON %s (%s)")
			% index_name
			% tablename
			% columns).str();
		return sql;
	}

	std::string build_dropindex_sql() {
		assert(fields_[0].isIndex());
		std::string index_name = fields_[0].Name();

		std::string& tablename = tables_[0];
		std::string sql = (boost::format("DROP INDEX %s ON %s")
			% index_name
			% tablename).str();
		return sql;
	}

	int execute_addcolumn_sql() {
		std::string sql_str = build_addcolumn_sql();
		if (sql_str.empty()) {
			last_error(std::make_pair<int, std::string>(-1, "executing alter table add columns unsuccessfully"));
			return -1;
		}

		try {
			LockedSociSession sql = db_conn_->checkoutDb();
			soci::statement st =
				(sql->prepare << sql_str);
			st.execute();
		}
		catch (soci::soci_error& e) {
			last_error(std::make_pair<int, std::string>(-1, e.what()));
			return -1;
		}

		return 0;
	}

	int execute_delcolumn_sql() {
		std::string sql_str = build_delcolumn_sql();
		if (sql_str.empty()) {
			last_error(std::make_pair<int, std::string>(-1, "executing alter table drop columns unsuccessfully"));
			return -1;
		}

		try {
			LockedSociSession sql = db_conn_->checkoutDb();
			soci::statement st =
				(sql->prepare << sql_str);
			st.execute();
		}
		catch (soci::soci_error& e) {
			last_error(std::make_pair<int, std::string>(-1, e.what()));
			return -1;
		}

		return 0;
	}

	int execute_modifycolumn_sql() {
		std::string sql_str = build_modifycolumn_sql();
		if (sql_str.empty()) {
			last_error(std::make_pair<int, std::string>(-1, "executing alter table modify columns unsuccessfully"));
			return -1;
		}

		try {
			LockedSociSession sql = db_conn_->checkoutDb();
			soci::statement st =
				(sql->prepare << sql_str);
			st.execute();
		}
		catch (soci::soci_error& e) {
			last_error(std::make_pair<int, std::string>(-1, e.what()));
			return -1;
		}
		return 0;
	}

	int execute_createindex_sql() {
		std::string sql_str = build_createindex_sql();
		if (sql_str.empty()) {
			last_error(std::make_pair<int, std::string>(-1, "executing create index unsuccessfully"));
			return -1;
		}

		try {
			LockedSociSession sql = db_conn_->checkoutDb();
			soci::statement st =
				(sql->prepare << sql_str);
			st.execute();
		}
		catch (soci::soci_error& e) {
			last_error(std::make_pair<int, std::string>(-1, e.what()));
			return -1;
		}
		return 0;
	}

	int execute_dropindex_sql() {
		std::string sql_str = build_dropindex_sql();
		if (sql_str.empty()) {
			last_error(std::make_pair<int, std::string>(-1, "executing drop index unsuccessfully"));
			return -1;
		}

		try {
			LockedSociSession sql = db_conn_->checkoutDb();
			soci::statement st =
				(sql->prepare << sql_str);
			st.execute();
		}
		catch (soci::soci_error& e) {
			last_error(std::make_pair<int, std::string>(-1, e.what()));
			return -1;
		}
		return 0;
	}

	int execute_select_sql() {
		std::string sql_str;
		if (tables_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when executing select-sql"));
			return -1;
		}

		std::string& tablename = tables_[0];
		std::string fields;
		if (fields_.size() == 0)
			fields = " * ";
		else {
			for (size_t idx = 0; idx < fields_.size(); idx++) {
				BuildField& field = fields_[idx];
				fields += field.Name();
				if (idx != fields_.size() - 1)
					fields += ",";
			}
		}

		index_ = 0;
		auto conditions = build_execute_conditions();
		if (std::get<0>(conditions) == 0) {
			const std::string& c = std::get<1>(conditions);
			if (c.empty()) {
				sql_str = (boost::format("select %s from %s")
					% fields
					%tablename).str();
			}
			else {
				sql_str = (boost::format("select %s from %s where %s")
					% fields
					%tablename
					%c).str();
			}

			LockedSociSession sql = db_conn_->checkoutDb();
			auto tmp = *sql << sql_str;
			auto& t = tmp;
			if (c.empty() == false) {
				if (std::get<2>(conditions).bind_value(t).first != 0) {
					last_error(std::make_pair<int, std::string>(-1,
						"bind values is unsuccessfull when executing select-sql"));
					return -1;
				}
			}
		}
		return 0;
	}

	std::pair<int, std::string> impl_build_condtions(const Json::Value& c) {
		std::pair<int, std::string> result = { 0, "" };
		do {
			if (c.isArray() == false)
				break;
			if (c.size() == 0)
				break;
			auto node = conditionTree::createRoot(c);
			if (node.first != 0) {
				result = { -1, (boost::format("create condition unsuccessfully.[%s]")
					%Json::jsonAsString(c)).str() };
				break;
			}
			result = conditionParse::parse_conditions(c, node.second);
			if (result.first != 0)
				break;
			result = { 0, node.second.asString() };
		} while (false);
		return result;

	}
	
	std::pair<int, std::string> build_conditions() {
		std::pair<int, std::string> result = { 0, "" };
		if (condition_.isNull())
			return result;
		return impl_build_condtions(condition_);
	}

	std::pair<int, std::string> build_having_conditions() {
		std::pair<int, std::string> result = { 0, "" };
		if (having_.isNull())
			return result;
		return impl_build_condtions(having_);
	}

	std::pair<int, std::string> build_join_on_condition() {
		std::pair<int, std::string> result = { 0, "" };
		if (join_on_condition_.isNull())
			return result;
		return impl_build_condtions(join_on_condition_);
	}

	std::tuple<int, std::string, conditionTree> build_execute_conditions() {
		do {
			if (condition_.isNull())
				break;
			if (condition_.isArray() == false)
				return std::make_tuple(0, "",conditionTree(conditionTree::NodeType::Expression));

			if(condition_.size() == 0)
				return std::make_tuple(0, "",conditionTree(conditionTree::NodeType::Expression));

			auto node = conditionTree::createRoot(condition_);
			if (node.first != 0) {
				return std::make_tuple(-1, (boost::format("create condition unsuccessfully.[%s]")
					%Json::jsonAsString(condition_)).str(),conditionTree(conditionTree::NodeType::Expression));
			}

			{
				auto result = conditionParse::parse_conditions(condition_, node.second);
				if (result.first != 0)
					return std::make_tuple(-1, result.second, conditionTree(conditionTree::NodeType::Expression));
			}

			{
				node.second.set_bind_values_index(index_);
				auto result = node.second.asConditionString();
				if(result.first != 0)
					return std::make_tuple(-1, result.second, conditionTree(conditionTree::NodeType::Expression));
				return std::make_tuple(0, result.second,node.second);
			}

		} while (false);
		return std::make_tuple(-1,"error", 
			conditionTree(conditionTree::NodeType::Expression));
	}

	void bind_fields_value(soci::details::once_temp_type& t) {
		for (size_t idx = 0; idx < fields_.size(); idx++) {
			BuildField& field = fields_[idx];
			if (field.isString() || field.isVarchar()
				|| field.isBlob() || field.isText()
				|| field.isLongText()) {
				t = t, soci::use(field.asString());
			}
			else if (field.isInt())
				t = t, soci::use(field.asInt());
			else if (field.isFloat())
				t = t, soci::use(static_cast<double>(field.asFloat()));
			else if (field.isDouble() || field.isDecimal())
				t = t, soci::use(field.asDouble());
			else if (field.isInt64() || field.isDateTime())
				t = t, soci::use(field.asInt64());
			else if (field.isNull())
				t = t, soci::use(0, indi_null);
		}
	}

	BuildSQL::BUILDTYPE build_type_;
	int index_; 
	BuildSQL::OrConditionsType conditions_;
	DatabaseCon* db_conn_;
	Json::Value condition_;
	Json::Value join_on_condition_; //union query
	DisposeError last_error_;
};

class DisposeMySQL : public DisposeSQL {
public:
	DisposeMySQL(BuildSQL::BUILDTYPE type, DatabaseCon* dbconn)
		: DisposeSQL(type, dbconn)
		, db_conn_(dbconn) {

	}

	~DisposeMySQL() {

	}

protected:

	std::size_t analyse_fields_and_build_colunms(std::vector<std::string>& columns) override {
		columns.clear();
		if (tables_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when building create-sql"));
			return 0;
		}

		if (fields_.size() == 0) {
            if (last_error().value().first == 0)
            {
                last_error(std::make_pair<int, std::string>(
                    -1, "Fields are empty when building create-sql"));
            }
			return 0;
		}

		std::vector<std::string> fields;
		std::vector<std::string> indexs;
		std::vector<std::string> primary_keys;
		std::vector<std::string> foreign_keys;
		std::vector<Json::Value> references;
		for (size_t idx = 0; idx < fields_.size(); idx++) {
			BuildField& field = fields_[idx];

			fields.push_back(field.Name());
			int length = field.length();
			if (field.isString() || field.isVarchar()) {
				std::string str;
				if (length > 0)
					str = (boost::format("VARCHAR(%d)") % length).str();
				else
					str = "VARCHAR(64)";
				fields.push_back(str);
			}
			else if (field.isChar()) {
				std::string str;
				if (length > 0)
					str = (boost::format("CHAR(%d)") % length).str();
				else
					str = "CHAR(128)";
				fields.push_back(str);
			}
			else if (field.isText()) {
				std::string str;
				if (length > 0)
					str = (boost::format("TEXT(%d)") % length).str();
				else
					str = "TEXT";
				fields.push_back(str);
			}
			else if (field.isBlob()) {
				std::string str = "BLOB";
				fields.push_back(str);
			}
			else if (field.isInt()) {
				std::string str;
				str = "INT";
				fields.push_back(str);
			}
			else if (field.isFloat()) {
				std::string str = "FLOAT";
				fields.push_back(str);
			}
			else if (field.isDouble()) {
				std::string str = "DOUBLE";
				fields.push_back(str);
			}
			else if (field.isDecimal()) {
				std::string str = "DECIMAL";
				str = (boost::format("DECIMAL(%d,%d)")
					% field.asDecimal().length()
					% field.asDecimal().accuracy()).str();
				fields.push_back(str);
			}
			else if (field.isDateTime()) {
				fields.push_back(std::string("datetime"));
			}
			else if (field.isDate()) {
				fields.push_back(std::string("date"));
			}
			else if (field.isLongText()) {
				std::string str;
				if (length > 0)
					str = (boost::format("LONGTEXT(%d)") % length).str();
				else
					str = "LONGTEXT";
				fields.push_back(str);
			}

			if (field.isPrimaryKey()) {
				primary_keys.push_back(field.Name());
			}

			if (field.isForeigKey()) {
				foreign_keys.push_back(field.Name());
				references.push_back(field.Foreigns());
			}
			if (field.isNotNull())
				fields.push_back(std::string("NOT NULL"));
			if (field.isUnique())
				fields.push_back(std::string("UNIQUE"));
			// fix an bug on RR-525, disable auto increment
			//if (field.isAutoIncrease())
			//	fields.push_back(std::string("AUTO_INCREMENT"));
			if (field.isIndex()) {
				//fields.push_back(std::string("INDEX"));
				indexs.push_back(field.Name());
			}
			if (field.isDefault()) {
				std::string str;
                if (field.isDefaultNull())
                {
                    str = "DEFAULT NULL";
                }
                // mysql engine: BLOB, TEXT, GEOMETRY or JSON column can't have a default value
                else if (field.isChar() || field.isVarchar() ||
                    /*field.isBlob() || field.isText() || field.isLongText() ||*/
                    field.isDate() || field.isDateTime())
                {
                    str = (boost::format("DEFAULT '%1%'") % field.asString()).str();
                }
                else if (field.isInt())
                {
                    str = (boost::format("DEFAULT %1%") % field.asInt()).str();
                }
                else if (field.isFloat())
                {
                    str = (boost::format("DEFAULT %1%") % field.asFloat()).str();
                }
                else if (field.isDouble())
                {
                    str = (boost::format("DEFAULT %1%") % field.asDouble()).str();
                }
                else if (field.isDecimal())
                {
                    str = (boost::format("DEFAULT %1%") % field.asDecimal().value()).str();
                }
                else if (field.isNumeric())
                {
                    str = (boost::format("DEFAULT %1%") % field.asInt()).str();
                }
				fields.push_back(str);
			}

			if (idx != fields_.size() - 1)
				fields.push_back(std::string(","));
		}

		if (fields.size()) {
			for (size_t idx = 0; idx < fields.size(); idx++) {
				std::string& element = fields[idx];
				columns.push_back(element);
			}

			// primary keys
			size_t size = primary_keys.size();
			std::string primarys;
			for (size_t i = 0; i < size; i++) {
				if (i != 0) {
					primarys += ",";
				}
				primarys += primary_keys[i];
			}

			if (primarys.empty() == false) {
				columns.push_back(",");
				columns.push_back((boost::format("primary key(%s)") % primarys).str());
			}

			// indexs
			size = indexs.size();
			for (size_t i = 0; i < size; i++) {
                columns.push_back(
                    (boost::format(",index(%s)") % indexs[i]).str());
			}

			// foreign keys
			std::string refs;
			size = foreign_keys.size();
			//assert(size > 0 && size == references.size());
			for (size_t i = 0; i < size; i++) {
				Json::Value& r = references[i];
				refs += (boost::format("foreign key(%s) references %s(%s)")
					% foreign_keys[i]
					% r["table"].asString()
					% r["field"].asString()).str();
			}
			if (refs.empty() == false) {
				columns.push_back(",");
				columns.push_back(refs);
			}
		}

		return columns.size();
	}

	std::string build_createtable_sql() override {
		std::string& tablename = tables_[0];
		std::vector<std::string> columns;
		analyse_fields_and_build_colunms(columns);

		std::string columns_str;
		for (size_t idx = 0; idx < columns.size(); idx++) {
			std::string& element = columns[idx];
			columns_str += element;
			columns_str += " ";
		}

		std::string sql = (boost::format("CREATE TABLE %s (%s)")//ENGINE=InnoDB DEFAULT CHARSET=utf8
			% tablename
			% columns_str).str();

		return sql;
	}

	int execute_createtable_sql() override {
		// first drop the same of a table when create a table
        // call drop in the higher level, avoiding drop tx in a transaction
		//execute_droptable_sql();
		std::string sql_str = build_createtable_sql();
		if (sql_str.empty()) {
			last_error(std::make_pair<int, std::string>(-1, "executing create table unsuccessfully"));
			return -1;
		}

//        std::string sql_str = build_createtable_sql();
		try {
			LockedSociSession sql = db_conn_->checkoutDb();
            soci::statement st =
                (sql->prepare << sql_str);
            st.execute();
		} catch (soci::soci_error& e) {
			last_error(std::make_pair<int, std::string>(-1, e.what()));
			return -1;
		}

		return 0;
	}

	std::string	build_exist_sql() override {
		std::string sql;
		if (tables_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when building exist-sql"));
			return sql;
		}
		std::string& tablename = tables_[0];
		std::string s = "%";
		sql = (boost::format("show tables like '%1%%2%%1%'") % s %tablename).str();
		return sql;
	}

private:
	DisposeMySQL()
		: DisposeSQL() {
	}

	DatabaseCon* db_conn_;
};

class DisposeSqlite : public DisposeSQL {
public:
	DisposeSqlite(BuildSQL::BUILDTYPE type, DatabaseCon* dbconn)
		: DisposeSQL(type, dbconn)
		, db_conn_(dbconn) {

	}

	~DisposeSqlite() {

	}

protected:

	std::size_t analyse_fields_and_build_colunms(std::vector<std::string>& columns) override {
		columns.clear();
		if (tables_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when building create-sql"));
			return 0;
		}

		if (fields_.size() == 0) {
			if (last_error().value().first == 0)
			{
				last_error(std::make_pair<int, std::string>(
					-1, "Fields are empty when building create-sql"));
			}
			return 0;
		}

		std::vector<std::string> fields;
		std::vector<std::string> primary_keys;
		std::vector<std::string> foreign_keys;
		std::vector<Json::Value> references;
		for (size_t idx = 0; idx < fields_.size(); idx++) {
			BuildField& field = fields_[idx];

			fields.push_back(field.Name());
			//int length = field.length();
			if (field.isString() || field.isVarchar() || field.isText()) {
				std::string str = "TEXT";
				fields.push_back(str);
			}
			else if (field.isBlob()) {
				std::string str = "BLOB";
				fields.push_back(str);
			}
			else if (field.isInt()) {
				std::string str = "INTEGER";
				fields.push_back(str);
			}
			else if (field.isFloat() || field.isDouble() || field.isDecimal()) {
				std::string str = "REAL";
				fields.push_back(str);
			}
			else if (field.isDateTime()) {
				std::string str = "NUMERIC";
				fields.push_back(str);
			}
			else if (field.isLongText()) {
				std::string str = "LONGTEXT";
				fields.push_back(str);
			}

			if (field.isPrimaryKey()) {
				fields.push_back(std::string("PRIMARY KEY"));
				primary_keys.push_back(field.Name());
			}

			if (field.isForeigKey()) {
				foreign_keys.push_back(field.Name());
				references.push_back(field.Foreigns());
			}

			// fix an bug on RR-525, disable auto increment
			//if (field.isAutoIncrease())  
			//	fields.push_back(std::string("AUTOINCREMENT"));
			if (field.isNotNull())
				fields.push_back(std::string("NOT NULL"));
			if (field.isUnique())
				fields.push_back(std::string("UNIQUE"));

			if (field.isIndex())
				fields.push_back(std::string("INDEX"));
			if (field.isDefault()) {
				std::string str;
				if (field.isNumeric()) {
					str = (boost::format("DEFAULT %d") % field.asInt()).str();
				}
				else if (field.isString()) {
					std::string default_value = field.asString();
					if (default_value.empty()
						|| boost::iequals(default_value, "null")
						|| boost::iequals(default_value, "nil"))
						str = "DEFAULT NULL";
					else
						str = (boost::format("DEFAULT \"%s\"") % default_value).str();
				}
				fields.push_back(str);
			}

			if (idx != fields_.size() - 1)
				fields.push_back(std::string(","));
		}

		if (fields.size()) {
			for (size_t idx = 0; idx < fields.size(); idx++) {
				std::string& element = fields[idx];
				columns.push_back(element);
			}
			// foreign keys
			std::string refs;
			size_t size = foreign_keys.size();
			//assert(size > 0 && size == references.size());
			for (size_t i = 0; i < size; i++) {
				Json::Value& r = references[i];
				refs += (boost::format("foreign key(%s) references %s(%s)")
					% foreign_keys[i]
					% r["table"].asString()
					% r["field"].asString()).str();
			}
			if (refs.empty() == false) {
				columns.push_back(",");
				columns.push_back(refs);
			}
		}
		return columns.size();
	}

	std::string
    build_createtable_sql() override
    {
        std::string& tablename = tables_[0];
        std::vector<std::string> columns;
        analyse_fields_and_build_colunms(columns);
        std::string columns_str;
        for (size_t idx = 0; idx < columns.size(); idx++)
        {
            std::string& element = columns[idx];
            columns_str += element;
            columns_str += " ";
        }
        std::string sql =
            (boost::format("CREATE TABLE if not exists %s (%s)") %
                tablename % columns_str)
                .str();
        return sql;
    }

	int execute_createtable_sql() override {
		// first drop the same of a table when create a table
        // call drop in the higher level, avoiding drop tx in a transaction
		//execute_droptable_sql();
		std::string sql_str = build_createtable_sql();
		if (sql_str.empty()) {
			last_error(std::make_pair<int, std::string>(-1, "executing create table unsuccessfully"));
			return -1;
		}

		try {
			LockedSociSession sql = db_conn_->checkoutDb();
            *sql << sql_str;
		}
		catch (soci::soci_error& e) {
			last_error(std::make_pair<int, std::string>(-1, e.what()));
			return -1;
		}

		return 0;
	}

	std::string	build_exist_sql() override {
		std::string sql;
		if (tables_.size() == 0) {
			last_error(std::make_pair<int, std::string>(-1, "Table miss when building exist-sql"));
			return sql;
		}

		std::string& tablename = tables_[0];
		//sql = (boost::format("show tables like '%1%%2%%1%'") % s %tablename).str();
		sql = (boost::format("SELECT count(*) FROM sqlite_master WHERE type='table' AND name='%1%'") %tablename).str();
		return sql;
	}

private:
	DisposeSqlite()
		: DisposeSQL() {
	}
	DatabaseCon* db_conn_;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// class BuildMySQL
//////////////////////////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string>		EMPTY_TABLES;
static std::vector<BuildField>		EMPTY_FIELDS;
static BuildSQL::OrConditionsType	EMPYT_CONDITIONS;

class BuildMySQL : public BuildSQL {
public:
	explicit BuildMySQL(BuildSQL::BUILDTYPE type, DatabaseCon* dbconn) 
	: BuildSQL()
	, disposesql_(std::make_shared<DisposeMySQL>(type, dbconn)) {
	}

	~BuildMySQL() {

	}

	void AddTable(const std::string& tablename) override {
		if (disposesql_)
			disposesql_->AddTable(tablename);
	}

	void AddTable(const Json::Value& table) override {
		if (disposesql_)
			disposesql_->AddTable(table);
	}

	void AddField(const BuildField& field) override {
		if (disposesql_)
			disposesql_->AddField(field);
	}

	void AddCondition(const AndCondtionsType& condition) override {
		if (disposesql_)
			disposesql_->AddCondition(condition);
	}

	void AddCondition(const Json::Value& condition) override {
		if (disposesql_)
			disposesql_->AddCondition(condition);
	}

	void AddLimitCondition(const Json::Value& limit) override {
		if (disposesql_)
			disposesql_->AddLimitCondition(limit);
	}

	void AddOrderCondition(const Json::Value& order) override {
		if (disposesql_)
			disposesql_->AddOrderCondition(order);
	}

	void AddJoinOnCondition(const Json::Value& join) override {
		if (disposesql_)
			disposesql_->AddJoinOnCondition(join);
	}

	void AddGroupByCondition(const Json::Value& group) override {
		if (disposesql_)
			disposesql_->AddGroupByCondition(group);
	}

	void AddHavingCondition(const Json::Value& having) override {
		if (disposesql_)
			disposesql_->AddHavingCondition(having);
	}

	int parseExtraCondition(const Json::Value& object, 
		std::function<void(const Json::Value& limit)> handlelimit,
		std::function<void(const Json::Value& order)> handleorder,
		std::function<void(const Json::Value& join)> handlejoin,
		std::function<void(const Json::Value& group)> handlegroupby,
		std::function<void(const Json::Value& having)> handlehaving,
		std::function<void(const Json::Value& error)> handleerror) override {
		if (disposesql_)
			return disposesql_->parseExtraCondition(object, handlelimit, handleorder, handlejoin, handlegroupby, handlehaving, handleerror);
		return false;
	}

	const std::vector<std::string>& Tables() const override {
		if (disposesql_)
			return disposesql_->Tables();

		return EMPTY_TABLES;
	}

	const std::vector<BuildField>&  Fields() const override {
		if (disposesql_)
			return disposesql_->Fields();

		return EMPTY_FIELDS;
	}

	const OrConditionsType& Conditions() const override {
		if (disposesql_)
			return disposesql_->Conditions();

		return EMPYT_CONDITIONS;
	}

	BUILDTYPE build_type(BUILDTYPE type) override {
		if (disposesql_)
			return disposesql_->build_type(type);
		return BUILD_NOSQL;
	}

	std::string asString() override {
		std::string sql;
		if (disposesql_)
			sql = disposesql_->asString();
		return sql;
	}

	int execSQL() override {
		int ret = -1;
		if (disposesql_)
			ret = disposesql_->execSQL();
		return ret;
	}

	void clear() override {
		if (disposesql_)
			disposesql_->clear();
	}

	std::pair<int, std::string> last_error() override {
		if (disposesql_)
			return disposesql_->last_error().value();
		return std::make_pair<int, std::string>(-1, "exhausted resource");        
	}

    void set_last_error(const std::pair<int, std::string> &error) override {
        if (disposesql_)
            return disposesql_->last_error(error);        
    }
    
    void batch_insert(const uint32_t batch) override {
        disposesql_->batch_insert(batch);
    }
    
    uint32_t batch_insert() const override {
        return disposesql_->batch_insert();
    }
 
private:
	explicit BuildMySQL() {};
	std::shared_ptr<DisposeMySQL> disposesql_;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// class BuildSqlite
//////////////////////////////////////////////////////////////////////////////////////////////////////

class BuildSqlite : public BuildSQL {
public:
	BuildSqlite(BuildSQL::BUILDTYPE type, DatabaseCon* dbconn)
	: BuildSQL()
	, disposesql_(std::make_shared<DisposeSqlite>(type, dbconn)) {

	}

	~BuildSqlite() {

	}

	void AddTable(const std::string& tablename) override {
		if (disposesql_)
			disposesql_->AddTable(tablename);
	}

	void AddTable(const Json::Value& table) override {
		if (disposesql_)
			disposesql_->AddTable(table);
	}

	void AddField(const BuildField& field) override {
		if (disposesql_)
			disposesql_->AddField(field);
	}

	void AddCondition(const AndCondtionsType& condition) override {
		if (disposesql_)
			disposesql_->AddCondition(condition);
	}

	void AddCondition(const Json::Value& condition) override {
		if (disposesql_)
			disposesql_->AddCondition(condition);
	}

	void AddLimitCondition(const Json::Value& limit) override {
		if (disposesql_)
			disposesql_->AddLimitCondition(limit);
	}

	void AddOrderCondition(const Json::Value& order) override {
		if (disposesql_)
			disposesql_->AddOrderCondition(order);
	}

	void AddJoinOnCondition(const Json::Value& join) override {
		if (disposesql_)
			disposesql_->AddJoinOnCondition(join);
	}

	void AddGroupByCondition(const Json::Value& group) override {
		if (disposesql_)
			disposesql_->AddGroupByCondition(group);
	}

	void AddHavingCondition(const Json::Value& having) override {
		if (disposesql_)
			disposesql_->AddHavingCondition(having);
	}

	int parseExtraCondition(const Json::Value& object, 
		std::function<void(const Json::Value& limit)> handlelimit,
		std::function<void(const Json::Value& order)> handleorder,
		std::function<void(const Json::Value& join)> handlejoin,
		std::function<void(const Json::Value& group)> handlegroupby,
		std::function<void(const Json::Value& having)> handlehaving,
		std::function<void(const Json::Value& error)> handleerror) override {
		if (disposesql_)
			return disposesql_->parseExtraCondition(object, handlelimit, handleorder, handlejoin,handlegroupby, handlehaving, handleerror);
		return false;
	}

	const std::vector<std::string>& Tables() const override {
		if (disposesql_)
			return disposesql_->Tables();

		return EMPTY_TABLES;
	}

	const std::vector<BuildField>&  Fields() const override {
		if (disposesql_)
			return disposesql_->Fields();

		return EMPTY_FIELDS;
	}

	const OrConditionsType& Conditions() const override {
		if (disposesql_)
			return disposesql_->Conditions();

		return EMPYT_CONDITIONS;
	}

	BUILDTYPE build_type(BUILDTYPE type) override {
		if (disposesql_)
			return disposesql_->build_type(type);
		return BUILD_NOSQL;
	}

	std::string asString() override {
		std::string sql;
		if (disposesql_)
			sql = disposesql_->asString();
		return sql;
	}

	int execSQL() override {
		int ret = -1;
		if (disposesql_)
			ret = disposesql_->execSQL();
		return ret;
	}

	void clear() override {
		if (disposesql_)
			disposesql_->clear();
	}

	std::pair<int, std::string> last_error() override {
		if (disposesql_)
			return disposesql_->last_error().value();
		return std::make_pair<int, std::string>(-1, "exhausted resource");
	}

    void set_last_error(const std::pair<int, std::string> &error) override {
        if (disposesql_)
            return disposesql_->last_error(error);
    }
    
    void batch_insert(const uint32_t batch) override {
        disposesql_->batch_insert(batch);
    }
    
    uint32_t batch_insert() const override {
        return disposesql_->batch_insert();
    }

private:
	explicit BuildSqlite() {};
	std::shared_ptr<DisposeSqlite> disposesql_;
};

namespace helper {


	std::pair<int, std::string> ParseQueryJson(const Json::Value& tx_json, BuildSQL &buildsql) {

		std::string error = "Unknown Error when parse json";
		int code = 0;
		do {
			Json::Value obj_tables = tx_json["Tables"];
			if (obj_tables.isArray() == false) {
				error = (boost::format("Tables' type is error. [%s]")
					% Json::jsonAsString(obj_tables)).str();
				code = -1;
				break;
			}

			for (Json::UInt idx = 0; idx < obj_tables.size(); idx++) {
				const Json::Value& e = obj_tables[idx];
				if (e.isObject() == false) {
					error = (boost::format("Parsed object is error. [%s]")
						% Json::jsonAsString(e)).str();
					code = -1;
					break;
				}

				// add table
				if (e.isMember("Table")) {
					Json::Value v = e["Table"];
					if (v.isObject() == false) {
						error = (boost::format("Parsed object is error. [%s]")
							% Json::jsonAsString(v)).str();
						code = -1;
						break;
					}

					//Json::Value tn = v["TableName"];
					//if (tn.isString() == false) {
					//	error = (boost::format("Parsed object is error. [%s]")
					//		% Json::jsonAsString(tn)).str();
					//	code = -1;
					//	break;
					//}
					//buildsql.AddTable(std::string(TABLE_PREFIX) + tn.asString());
					buildsql.AddTable(e);
				}
				else {
					error = (boost::format("Not support key. [%s]")
						% Json::jsonAsString(e)).str();
					code = -1;
					return {code, error };
				}

			}

			Json::Value raw = tx_json["Raw"];

			if (raw.isString() == false) {
				error = (boost::format("Parsed object is error. [%s]")
					% Json::jsonAsString(raw)).str();
				code = -1;
				break;
			}

			Json::Value obj_raw;
			if (Json::Reader().parse(raw.asString(), obj_raw) == false) {
				error = (boost::format("Parsed Raw is error. [%s]")
					% raw.asString()).str();
				code = -1;
				break;
			}

			if (obj_raw.isArray() == false) {
				error = (boost::format("Raw's type is error. [%s]")
					% Json::jsonAsString(obj_raw)).str();
				code = -2;
				break;
			}

			Json::UInt size = obj_raw.size();
			Json::Value conditions;
            
			for (Json::UInt idx = 0; idx < size; idx++) {
				const Json::Value& v = obj_raw[idx];
				if (idx == 0) {
					// query field
                    if (!v.isArray())
                    {
                        error = (boost::format("Raw's type is error, the first item must be array. [%s]")
                            % Json::jsonAsString(obj_raw)).str();
                        code = -2;
                        break;
                    }
						

					for (Json::UInt i = 0; i < v.size(); i++) {
						const Json::Value& fieldname = v[i];
						if (fieldname.isString() == false) {
							error = (boost::format("Field's type is not string . [%s]")
								% Json::jsonAsString(fieldname)).str();
							code = -3;
							break;
						}
						////check blank space
						//auto field_name_str = fieldname.asString();
						//if (field_name_str.find(' ') != std::string::npos){
						//	error = (boost::format("Field [%s] contains blank space.")
						//		% field_name_str).str();
						//	break;
						//}
						auto field_name_str = fieldname.asString();
						BuildField field(field_name_str);
						buildsql.AddField(field);
					}
				}
				else {
					if (v.isObject() == false)
						return{ -1, (boost::format("Conditions' type is error.[%s]")
							% Json::jsonAsString(v)).str() };

					// first, parse limit,order conditions or join that union query will use
					code = buildsql.parseExtraCondition(v, [&buildsql](const Json::Value& limit) {
						// handle limit condition
						buildsql.AddLimitCondition(limit);
					},
						[&buildsql](const Json::Value& order) {
						// handle order conditions
						buildsql.AddOrderCondition(order);
					},
						[&buildsql](const Json::Value& join) {
						Json::Value conditions;
						conditions.append(join);
						buildsql.AddJoinOnCondition(conditions);
					},
						[&buildsql](const Json::Value& group) {
						buildsql.AddGroupByCondition(group);
					},
						[&buildsql](const Json::Value& having) {
						Json::Value having_conditions;
						having_conditions.append(having);
						buildsql.AddHavingCondition(having_conditions);
					},
						[&error](const Json::Value& e) {
						if (e["result"].asInt() != 0) {
							error = (boost::format("Parsing limit-condition or order-condition is unsuccessfull.[%s]")
								% e["message"].asString()).str();
						}
					});
					if (code == 0) {
						// Parsing ExtraCondition is successfull
					}
					else if (code == -1) {
						break;
					}
					else if (code == 1) {
						conditions.append(v);
					}
				}
			}

			assert(conditions.isArrayOrNull());
			size = conditions.size();
			if (size > 0) {
				buildsql.AddCondition(conditions);
			}
		} while (0);

		if (code == 0 || code == 1) {
			code = 0;
		}

		return{ code, error };
	}

	std::vector<std::vector<Json::Value>> query_result_2d(const soci::rowset<soci::row>& records)
	{
		std::vector<std::vector<Json::Value>> vecRet;
		soci::rowset<soci::row>::const_iterator r = records.begin();
		for (; r != records.end(); r++) {
			std::vector<Json::Value> vecCol;
			for (size_t i = 0; i < r->size(); i++) {
				Json::Value e;
				std::string key = r->get_properties(i).get_name();
				if (r->get_indicator(i) == soci::i_null || r->get_indicator(i) == soci::i_truncated)
				{
					e[r->get_properties(i).get_name()] = Json::Value::null;
				}
				if (r->get_properties(i).get_data_type() == soci::dt_string
					|| r->get_properties(i).get_data_type() == soci::dt_blob) {
					if (r->get_indicator(i) == soci::i_ok)
						e[key] = r->get<std::string>(i);
				}
				else if (r->get_properties(i).get_data_type() == soci::dt_integer) {
					if (r->get_indicator(i) == soci::i_ok)
						e[key] = r->get<int>(i);
				}
				else if (r->get_properties(i).get_data_type() == soci::dt_double) {
					if (r->get_indicator(i) == soci::i_ok)
						e[key] = r->get<double>(i);
				}
				else if (r->get_properties(i).get_data_type() == soci::dt_long_long) {
					if (r->get_indicator(i) == soci::i_ok)
						e[key] = static_cast<int>(r->get<long long>(i));
				}
				else if (r->get_properties(i).get_data_type() == soci::dt_unsigned_long_long) {
					if (r->get_indicator(i) == soci::i_ok)
						e[key] = static_cast<int>(r->get<unsigned long long>(i));
				}
				else if (r->get_properties(i).get_data_type() == soci::dt_date) {
					std::tm tm = { };
					std::string datetime = "NULL";
					if (r->get_indicator(i) == soci::i_ok) {
						tm = r->get<std::tm>(i);
						datetime = (boost::format("%d/%d/%d %d:%d:%d")
							% (tm.tm_year + 1900) % (tm.tm_mon + 1) % tm.tm_mday
							%tm.tm_hour % (tm.tm_min) % tm.tm_sec).str();
					}

					e[key] = datetime;
				}
				vecCol.push_back(e);
			}
			vecRet.push_back(vecCol);
		}
		return vecRet;
	}

	Json::Value query_result(const soci::rowset<soci::row>& records) {
		Json::Value obj;
		Json::Value lines(Json::arrayValue);
		try {
			soci::rowset<soci::row>::const_iterator r = records.begin();
			for (; r != records.end(); r++) {
				Json::Value e;
				for (size_t i = 0; i < r->size(); i++) {
					if (r->get_indicator(i) == soci::i_null || r->get_indicator(i) == soci::i_truncated)
					{
						e[r->get_properties(i).get_name()] = Json::Value::null;
					}

					if (r->get_properties(i).get_data_type() == soci::dt_string
						|| r->get_properties(i).get_data_type() == soci::dt_blob) {
						if (r->get_indicator(i) == soci::i_ok)
							e[r->get_properties(i).get_name()] = r->get<std::string>(i);
					}
					else if (r->get_properties(i).get_data_type() == soci::dt_integer) {
						if (r->get_indicator(i) == soci::i_ok)
							e[r->get_properties(i).get_name()] = r->get<int>(i);
					}
					else if (r->get_properties(i).get_data_type() == soci::dt_double) {
						if (r->get_indicator(i) == soci::i_ok)
							e[r->get_properties(i).get_name()] = r->get<double>(i);
					}
					else if (r->get_properties(i).get_data_type() == soci::dt_long_long) {
						if (r->get_indicator(i) == soci::i_ok)
							e[r->get_properties(i).get_name()] = static_cast<int>(r->get<long long>(i));
					}
					else if (r->get_properties(i).get_data_type() == soci::dt_unsigned_long_long) {
						if (r->get_indicator(i) == soci::i_ok)
							e[r->get_properties(i).get_name()] = static_cast<int>(r->get<unsigned long long>(i));
					}
					else if (r->get_properties(i).get_data_type() == soci::dt_date) {
						std::tm tm = { };
						std::string datetime = "NULL";
						if (r->get_indicator(i) == soci::i_ok) {
							tm = r->get<std::tm>(i);
							datetime = (boost::format("%d/%d/%d %d:%d:%d")
								% (tm.tm_year + 1900) % (tm.tm_mon + 1) % tm.tm_mday
								%tm.tm_hour % (tm.tm_min) % tm.tm_sec).str();
						}

						e[r->get_properties(i).get_name()] = datetime;
					}
				}
				lines.append(e);
			}
			obj[jss::lines] = lines;
		}
		catch (soci::soci_error& e) {
			return RPC::make_error(rpcGENERAL, e.what());
		}
		return obj;
	}
    
    void modifyLimitCount(std::string& sSql,int selectLimit)
    {   
        std::string sLimit;

        int iPosLimitLower = sSql.find(" limit ");
        int iPosLimitUpper = sSql.find(" LIMIT ");
        if (iPosLimitLower >= 0 || iPosLimitUpper >= 0)
        {
			if (iPosLimitLower >= 0)   sLimit = sSql.substr(iPosLimitLower);
			else                       sLimit = sSql.substr(iPosLimitUpper);

            const char * chLimit = sLimit.c_str();
            for (int i = 7; i<sLimit.length(); i++)
            {
                if ((chLimit[i] > 'a' && chLimit[i] < 'z') || 
					(chLimit[i] > 'A' && chLimit[i] < 'Z') || 
					chLimit[i] == ';')
                {
                    sLimit = sLimit.substr(0, i);
                    break;
                }
            }
            std::string sLimitNew = "";
            std::string sCount = to_string(selectLimit);
            int iPosComma = sLimit.find(",");
            if (iPosComma >= 0)
            {
                sCount = sLimit.substr(iPosComma + 1);
                boost::algorithm::trim(sCount);
                int iCount = std::stoi(sCount.c_str());
                if (iCount > selectLimit)
                {
                    iCount = selectLimit;
                    sCount = to_string(iCount);
                    sLimitNew = sLimit.substr(0, iPosComma + 1);
                    sLimitNew += sCount;
                }                
            }
            else
            {
                sCount = sLimit.substr(7);
                boost::algorithm::trim(sCount);
                int iCount = std::stoi(sCount.c_str());
                if (iCount > selectLimit)
                {
                    iCount = selectLimit;
                    sCount = to_string(iCount);
                    sLimitNew = " limit " + sCount;
                }
            }

            if (!sLimitNew.empty())
            {
                StringReplace(sSql, sLimit, sLimitNew);
            }
        }
        else
        {
            std::string sLimit = " limit " + to_string(selectLimit);
            int iPosSemicolon = sSql.find(';');
            if (iPosSemicolon >= 0)
            {
                sSql = sSql.substr(0, iPosSemicolon);
                sSql += sLimit + ";";
            }
            else
            {
                sSql += sLimit;
            }
        }
    }

    Json::Value query_directly(DatabaseCon* conn, std::string sql,int selectLimit) {
        Json::Value obj;
        modifyLimitCount(sql, selectLimit);
        try {
            LockedSociSession query = conn->checkoutDb();
            soci::rowset<soci::row> records = ((*query).prepare << sql);
            obj = query_result(records);
        }
        catch (soci::soci_error& e) {
            obj[jss::error] = e.what();
        }
        return obj;
    }

	Json::Value query_directly(const Json::Value& tx_json, DatabaseCon* conn, BuildSQL* buildsql, int selectLimit) {
		Json::Value obj;
		std::pair<int, std::string> result = ParseQueryJson(tx_json, *buildsql);
		if (result.first != 0) {
			return RPC::make_error(rpcJSON_PARSED_ERR, result.second);
		}

		try {
			std::string sql = buildsql->asString();
            modifyLimitCount(sql, selectLimit);
			auto last_error = buildsql->last_error();
			//Todo-LC:need to modify error
			if (last_error.first != 0) {
				obj = RPC::make_error(rpcSQL_DISPOSE_ERR, last_error.second);
				return obj;
			}
			LockedSociSession query = conn->checkoutDb();
			soci::rowset<soci::row> records = ((*query).prepare << sql);
			obj = query_result(records);
		}
		catch (soci::soci_error& e) {
			obj = RPC::make_error(rpcGENERAL, e.what());
		}
		return obj;
	}
	std::pair<std::vector<std::vector<Json::Value>>, std::string> query_directly2d(const Json::Value& tx_json, DatabaseCon* conn, BuildSQL* buildsql, int selectLimit) {
		std::vector<std::vector<Json::Value>> obj;
		std::pair<int, std::string> result = ParseQueryJson(tx_json, *buildsql);
		if (result.first != 0) {
			return std::make_pair(obj,result.second);
		}

		try {
			std::string sql = buildsql->asString();
			modifyLimitCount(sql, selectLimit);
			auto last_error = buildsql->last_error();
			if (last_error.first != 0) {
				return std::make_pair(obj,last_error.second);
			}
			LockedSociSession query = conn->checkoutDb();
			soci::rowset<soci::row> records = ((*query).prepare << sql);
			obj = query_result_2d(records);
		}
		catch (soci::soci_error& e) {
			return std::make_pair(obj,e.what());
		}
		return std::make_pair(std::move(obj),"");
	}
    
} // namespace helper

//////////////////////////////////////////////////////////////////////////////////////////////////////
// class STTx2SQL
//////////////////////////////////////////////////////////////////////////////////////////////////////


STTx2SQL::STTx2SQL(const std::string& db_type)
: db_type_(db_type)
, db_conn_(nullptr) {

}

STTx2SQL::STTx2SQL(const std::string& db_type, DatabaseCon* dbconn)
: db_type_(db_type)
, db_conn_(dbconn) {

}

STTx2SQL::~STTx2SQL() {
}

bool STTx2SQL::IsTableExistBySelect(DatabaseCon* dbconn, std::string sTable)
{
    if (dbconn == nullptr)   return false;
    
    LockedSociSession sql_session = dbconn->checkoutDb();
    bool bExist = false;
    try {
        std::string sSql = "SELECT * FROM " + sTable + " LIMIT 1";
        boost::optional<std::string> r;
        soci::statement st = (sql_session->prepare << sSql, soci::into(r));
        st.execute(true);
        bExist = true;
    }
    catch (std::exception const& /* e */)
    {        
        bExist = false;
    }
    
    return bExist;
}

// parse condition to string, like  "[{\"id\" : {\"$ge\" : 3}}]" to "id>=3"
bool STTx2SQL::ConvertCondition2SQL(const Json::Value& condition, std::string& sSql)
{

	auto rTree = conditionTree::createRoot(condition);
	if (rTree.first != 0)
		return false;

	auto rConditions = conditionParse::parse_conditions(condition, rTree.second);
	if (rConditions.first != 0)
		return false;

	sSql = rTree.second.asString();

	return true;
}


int STTx2SQL::ParseFieldDefinitionAndAdd(const Json::Value& Raw, BuildSQL *buildsql) {
	int ret = -1;
	std::string sError = "";
	if (Raw.isArray()) {
		for (Json::UInt index = 0; index < Raw.size(); index++) {
			Json::Value v = Raw[index];

			// both field and type are requirement 
			if (v.isMember("field") == false && v.isMember("type") == false)
				return ret;
			//field and type
			std::string fieldname = v["field"].asString();
            
            if (BuildField::HaveSpecialCharacters(fieldname)) {
                buildsql->set_last_error(std::make_pair<int, std::string>(-1, (boost::format("fieldname is illegal: %s") % fieldname).str()));
                return ret;
            }
            
			std::string type = v["type"].asString();
			BuildField buildfield(fieldname);
			// set default value when create table
			if (boost::iequals(type, "int") || boost::iequals(type, "integer"))
				buildfield.SetFieldValue(0);
			else if (boost::iequals(type, "float"))
				buildfield.SetFieldValue(0.0f);
			else if (boost::iequals(type, "double"))
				buildfield.SetFieldValue((double)0.0f);
			else if (boost::iequals(type, "text"))
				buildfield.SetFieldValue("", FieldValue::fTEXT);
			else if (boost::iequals(type, "varchar"))
				buildfield.SetFieldValue("", FieldValue::fVARCHAR);
			else if (boost::iequals(type, "char"))
				buildfield.SetFieldValue("", FieldValue::fCHAR);
			else if (boost::iequals(type, "blob"))
				buildfield.SetFieldValue("", FieldValue::fBLOB);
			else if (boost::iequals(type, "datetime"))
				buildfield.SetFieldValue(InnerDateTime());
			else if (boost::iequals(type, "date"))
				buildfield.SetFieldValue(InnerDate());
			else if (boost::iequals(type, "decimal"))
				buildfield.SetFieldValue(InnerDecimal(32, 0));
			else if (boost::iequals(type, "longtext"))
				buildfield.SetFieldValue("", FieldValue::fLONGTEXT);
			else
			{
				buildsql->set_last_error(std::make_pair<int, std::string>(-1, (boost::format("type : %s is not support") % type).str()));
				return ret;
			}

			//about length
			int length = 0;
			if (v.isMember("length")) {
				length = v["length"].asInt();
			}

			if (boost::iequals(type, "decimal")) {
				if (length == 0)
					length = 32;
				int accuracy = 2;
				if (v.isMember("accuracy"))
					accuracy = v["accuracy"].asInt();
				// update decimal
				buildfield.asDecimal().update(InnerDecimal(length, accuracy));
			}
			else {
				if (length)
					buildfield.SetLength(length);
			}


			//
			if (v.isMember("FK") && v.isMember("REFERENCES")) {
				buildfield.SetForeignKey();
				if (v["REFERENCES"].isMember("table") == false || v["REFERENCES"].isMember("field") == false)
				{
					buildsql->set_last_error(std::make_pair<int, std::string>(-1, "There is no table or field in REFERENCES object."));
					return ret;
				}

				buildfield.SetForeigns(v["REFERENCES"]);
			}

			//other fields
			Json::Value::Members members = v.getMemberNames();
			for (auto it = members.cbegin(); it != members.cend(); it++)
			{
				if ((*it).compare("field") == 0 || (*it).compare("type") == 0 ||
					(*it).compare("length") == 0 || (*it).compare("accuracy") == 0 ||
					(*it).compare("FK") == 0 || (*it).compare("REFERENCES") == 0)   continue;
				else if ((*it).compare("PK") == 0)
				{
					if (v["PK"].asInt() == 1) buildfield.SetPrimaryKey();
				}
				else if ((*it).compare("index") == 0)
				{
					if (v["index"].asInt() == 1) buildfield.SetIndex();
				}
				else if ((*it).compare("NN") == 0)
				{
					if (v["NN"].asInt() == 1) buildfield.SetNotNull();
				}
				else if ((*it).compare("UQ") == 0)
				{
					if (v["UQ"].asInt() == 1) buildfield.SetUnique();
				}
				else if ((*it).compare("default") == 0)
				{
					buildfield.SetDefault();
                    if (v["default"].isString())
                    {
                        if (buildfield.isChar())
                            buildfield.SetFieldValue(v["default"].asString(), FieldValue::fCHAR);
                        else if (buildfield.isVarchar())
                            buildfield.SetFieldValue(v["default"].asString(), FieldValue::fVARCHAR);
                        else if (buildfield.isLongText())
                            buildfield.SetFieldValue(v["default"].asString(), FieldValue::fLONGTEXT);
                        else if (buildfield.isDate())
                            buildfield.SetFieldValue(v["default"].asString(), FieldValue::fDATE);
                        else if (buildfield.isDateTime())
                            buildfield.SetFieldValue(v["default"].asString(), FieldValue::fDATETIME);
                    }
                    else if (v["default"].isNumeric())
                    {
                        if (buildfield.isInt())
                            buildfield.SetFieldValue(v["default"].asInt());
                        else if (buildfield.isFloat())
                            buildfield.SetFieldValue((float)v["default"].asDouble());
                        else if (buildfield.isDouble())
                            buildfield.SetFieldValue(v["default"].asDouble());
                        else if (buildfield.isDecimal())
                            buildfield.SetFieldValue(InnerDecimal(
                                    buildfield.asDecimal().length(),
                                    buildfield.asDecimal().accuracy(),
                                    v["default"].asDouble()));
                    }
                    else if (v["default"].isNull())
                    {
                        buildfield.SetDefaultNull();
                    }
				}
				else
				{
					buildsql->set_last_error(std::make_pair<int, std::string>(-1, (boost::format("key word : %s is not support") % *it).str()));
					return ret;
				}
			}

			buildsql->AddField(buildfield);
		}
		ret = 0;
	}

	return ret;
}

int STTx2SQL::GenerateCreateTableSql(const Json::Value& Raw, BuildSQL *buildsql) {
	return ParseFieldDefinitionAndAdd(Raw, buildsql);
}

std::pair<int, std::string> STTx2SQL::GenerateInsertSql(const Json::Value& raw, BuildSQL *buildsql) {
	std::vector<std::string> members = raw.getMemberNames();
	// retrieve members in object
	for (size_t i = 0; i < members.size(); i++) {
		std::string field_name = members[i];
        
        if (BuildField::HaveSpecialCharacters(field_name)) {
            return std::make_pair(-1, (boost::format("fieldname is illegal: %s") % field_name).str());
        }
		
		BuildField insert_field(field_name);
		std::pair<int, std::string> result = parseField(raw[field_name], insert_field);
		if (result.first != 0) {
			return result;
		}
		buildsql->AddField(insert_field);
	}
	return std::make_pair(0,"");

}

int STTx2SQL::GenerateDeleteSql(const Json::Value& raw, BuildSQL *buildsql) {

	buildsql->AddCondition(raw);
	return 0;
}

int STTx2SQL::GenerateAddColumnsSql(const Json::Value& raw, BuildSQL *buildsql) {
	return ParseFieldDefinitionAndAdd(raw, buildsql);
}

int STTx2SQL::GenerateDelColumnsSql(const Json::Value& raw, BuildSQL *buildsql) {
	if (raw.isArray() == false)
		return -1;

	for (Json::UInt index = 0; index < raw.size(); index++) {
		Json::Value v = raw[index];
		std::string fieldname = v["field"].asString();
        
        if (BuildField::HaveSpecialCharacters(fieldname)) {
            buildsql->set_last_error(std::make_pair<int, std::string>(-1, (boost::format("fieldname is illegal: %s") % fieldname).str()));
            return -1;
        }
        
		buildsql->AddField(BuildField(fieldname));
	}
	return 0;
}

int STTx2SQL::GenerateModifyColumnsSql(const Json::Value& raw, BuildSQL *buildsql) {
	return ParseFieldDefinitionAndAdd(raw, buildsql);
}

int STTx2SQL::GenerateOperateIndex(const Json::Value& raw, BuildSQL *buildsql) {
	for (Json::UInt index = 0; index < raw.size(); index++) {
		Json::Value v = raw[index];
		std::string fieldname;
		if (index == 0) {
			fieldname = v["index"].asString();
		}
		else {
			fieldname = v["field"].asString();
		}
        
        if (BuildField::HaveSpecialCharacters(fieldname)) {
            buildsql->set_last_error(std::make_pair<int, std::string>(-1, (boost::format("fieldname is illegal: %s") % fieldname).str()));
            return -1;
        }
        
		BuildField field(fieldname);
		if (index == 0) {
			field.SetIndex();
		}

		buildsql->AddField(field);
	}
	return 0;
}

std::map<std::string, STTx2SQL::SFieldWithValue>
STTx2SQL::ParseAutoFields(
    const ripple::STTx& tx,
    SyncParam const& param,
    std::string const& txt_tablename)
{
    std::array<const SF_Blob*, 4> fields = {
        &sfAutoFillField, 
		&sfTxsHashFillField, 
		&sfLedgerTimeField, 
		&sfLedgerSeqField
    };
    std::map<std::string, SFieldWithValue> mapFields;
    for (auto& field : fields)
    {
        SFieldWithValue fieldValue;
        const SF_Blob& sfield = *field;
        if (sfield == sfAutoFillField || sfield == sfTxsHashFillField)
            fieldValue.value = to_string(tx.getRealTxID());
        else if (sfield == sfLedgerSeqField)
            fieldValue.value = to_string(param.ledgerSeq);
        else if (sfield == sfLedgerTimeField)
            fieldValue.value = param.ledgerTime;
        fieldValue.pairField = ParseFieldVL(tx, sfield, txt_tablename);
        mapFields[sfield.getName()] = fieldValue;
    }
    return std::move(mapFields);
}

std::pair<bool, std::string>
STTx2SQL::ParseFieldVL(const ripple::STTx& tx, const SF_Blob& field,std::string const& tableName)
{
    std::string fieldName;
    bool bHasField = false;
    if (tx.isFieldPresent(field))
    {
        bHasField = true;
        auto blob = tx.getFieldVL(field);
        fieldName.assign(blob.begin(), blob.end());
        //auto sql_str =
        //    (boost::format("select * from information_schema.columns WHERE "
        //                   "table_name ='%s'AND column_name ='%s'") %
        //     tableName % fieldName).str();
        //LockedSociSession sql = db_conn_->checkoutDb();
        //soci::rowset<soci::row> records = ((*sql).prepare << sql_str);
        //bHasField = records.end() != records.begin();
        //if(!bHasField)
        //{
        //    std::string tableNameNew = tableName;
        //    transform(tableName.begin(),tableName.end(),tableNameNew.begin(),::tolower);
        //    auto sql_str_again =
        //        (boost::format("select * from information_schema.columns WHERE "
        //                       "table_name ='%s'AND column_name ='%s'") %
        //         tableNameNew % fieldName).str();
        //    soci::rowset<soci::row> records = ((*sql).prepare << sql_str_again);
        //    bHasField = records.end() != records.begin();
        //}
    }
    return std::make_pair(bHasField, fieldName);
}

std::pair<int, std::string> STTx2SQL::GenerateSelectSql(const Json::Value& raw, BuildSQL *buildsql) {
	//BuildSQL::AndCondtionsType and_conditions;
	Json::Value conditions;
	// parse record
	for (Json::UInt idx = 0; idx < raw.size(); idx++) {
		auto& v = raw[idx];
		if(idx == 0) {
			if (v.isArray()) {
				for (Json::UInt i = 0; i < v.size(); i++) {
					std::string field_name = v[i].asString();
                    if (BuildField::HaveSpecialCharacters(field_name)) {
                        return {-1, (boost::format("fieldname is illegal: %s") % field_name).str()};
                    }
					BuildField field(field_name);
					buildsql->AddField(field);
				}
			}
		} else {
			if (v.isObject() == false)
				return { -1,"Condition field is not a json-object." };

			int code = -1;
			std::string error;
			// first, parse limit and order conditions
			code = buildsql->parseExtraCondition(v, [&buildsql](const Json::Value& limit) {
				// handle limit condition
				buildsql->AddLimitCondition(limit);
			},
				[&buildsql](const Json::Value& order) {
				// handle order conditions
				buildsql->AddOrderCondition(order);
			},
				[&buildsql](const Json::Value& join) {
				Json::Value conditions;
				conditions.append(join);
				buildsql->AddJoinOnCondition(conditions);
			},
				[&buildsql](const Json::Value& group) {
				buildsql->AddGroupByCondition(group);
			},
				[&buildsql](const Json::Value& having) {
				Json::Value having_conditions;
				having_conditions.append(having);
				buildsql->AddHavingCondition(having_conditions);
			},
				[&error](const Json::Value& e) {
				if (e["result"].asInt() != 0) {
					error = (boost::format("Parsing limit-condition or order-condition is unsuccessfull.[%s]")
						% e["message"].asString()).str();
				}
			});
			if (code == 0) {
				// Parsing ExtraCondition is successfull
			}
			else if (code == -1) {
				break;
			}
			else if (code == 1) {
				conditions.append(v);
			}
		}
	}
	if (conditions.isArray() && conditions.size() > 0)
		buildsql->AddCondition(conditions);
	return { 0,"" };
}

std::pair<bool, std::string> STTx2SQL::handle_assert_statement(const Json::Value& raw, BuildSQL *buildsql) {
	std::pair<bool, std::string> result = { true, "assert true" };
	do {
		Json::UInt size = raw.size();
		if (size == 0) {
			result = { false, (boost::format("Raw is malformed.[%s]")
				%Json::jsonAsString(raw)).str()};
			break;
		}

		Json::UInt assert_index = 0;
		const Json::Value& aseert_condition = raw[assert_index];
		std::string query_sql;
		int assert_type = -1;
		if (aseert_condition.isMember("$IsExisted")) {
			BuildSQL::BUILDTYPE old = buildsql->build_type(BuildSQL::BUILD_EXIST_TABLE);
			query_sql = buildsql->asString();
			buildsql->build_type(old);
			assert_type = 0;
		}
		else if(aseert_condition.isMember("$RowCount")) {
			// include $RowCount and others 
			Json::Value ajuested_raw;
			Json::Value fields(Json::arrayValue);
			ajuested_raw.append(fields);
			for (assert_index = 1; assert_index < size; assert_index++) {
				ajuested_raw.append(raw[assert_index]);
			}
			BuildSQL::BUILDTYPE old = buildsql->build_type(BuildSQL::BUILD_SELECT_SQL);
			auto ret = GenerateSelectSql(ajuested_raw, buildsql);
			if (ret.first != 0) {
				result = { false, ret.second};
				break;
			}
			query_sql = buildsql->asString();
			buildsql->build_type(old);
			assert_type = 1;
		}
		else {
			return{ false, (boost::format("Assert statement may be malformed.[%s]")
					% Json::jsonAsString(aseert_condition)).str() };
		}

		try {
			LockedSociSession query = db_conn_->checkoutDb();
			soci::rowset<soci::row> records = ((*query).prepare << query_sql);
			if (assert_result(records, aseert_condition)) {
				// assert successful
				break;
			}
			else {
				result = { false, "assert false" };
				break;
			}
		}
		catch (const soci::soci_error& e) {
			result = { false, e.what() };
			break;
		}
	} while (false);
	return result;
}

bool STTx2SQL::assert_result(const soci::rowset<soci::row>& records, const Json::Value& expect) {
	bool result = false;
	do {
		Json::Value value;
		int assert_type = -1;
		if (expect.isMember("$IsExisted")) {
			value = expect["$IsExisted"];
			assert_type = 0;
		}
		else if (expect.isMember("$RowCount")) {
			value = expect["$RowCount"];
			assert_type = 0;
		}
		else {
			assert_type = 1;
		}

		if (assert_type == 0) {
			Json::Int row_counts = 0;
			for (auto it = records.begin(); it != records.end(); it++)
				row_counts++;
			if (value.isInt() && value.asInt() == row_counts) {
				result = true;
				break;
			}
		}
		else if (assert_type == 1) {
			Json::Value r = helper::query_result(records);
			if (!r.isMember(jss::error)) {
				Json::Value c;
				c.append(expect);
				auto node = conditionTree::createRoot(c);
				if (node.first == 0) {
					auto ret = conditionParse::parse_conditions(c, node.second);
					if (ret.first != 0)
						break;

					result = conditionParse::judge(node.second,
						[&r](const conditionTree::expression_result& expression) {
						bool result = false;
						std::string keyname = std::get<0>(expression);
						std::string op = std::get<1>(expression);
						std::vector<BindValue> value = std::get<2>(expression);
						const Json::Value& lines = r[jss::lines];
						if (lines.isArray() == false)
							return result;
						Json::UInt size = lines.size();
						for (Json::UInt i = 0; i < size; i++) {
							const Json::Value& l = lines[i];
							if (l.isMember(keyname) == false)
								break;
							const Json::Value& v = l[keyname];
							BindValue fv;
							if (v.isString())
								fv = BindValue(v.asString());
							else if (v.isInt())
								fv = BindValue(v.asInt());
							else if (v.isUInt())
								fv = BindValue(v.asUInt());
							else if (v.isDouble())
								fv = BindValue(v.asDouble());

							if (boost::iequals(op, "$eq")) {
								const BindValue& e = value[0];
								result = (fv == e);
							}
							else if (boost::iequals(op, "$lt")) {
								const BindValue& e = value[0];
								result = (fv < e);
							}
							else if (boost::iequals(op, "$le")) {
								const BindValue& e = value[0];
								result = (fv <= e);
							}
							else if (boost::iequals(op, "$gt")) {
								const BindValue& e = value[0];
								result = (fv > e);
							}
							else if (boost::iequals(op, "$ge")) {
								const BindValue& e = value[0];
								result = (fv >= e);
							}
							else if (boost::iequals(op, "$in") || boost::iequals(op, "$nin")) {
								size_t s = value.size();
								for (size_t x = 0; x < s; x++) {
									const BindValue& e = value[x];
									if(boost::iequals(op, "$in")) {
										if (fv == e) {
											result = true;
											break;
										}
									}
									else if (boost::iequals(op, "$nin")) {
										if (fv == e) {
											return false;
										}
										else {
											result = true;
										}
									}
								}
							}

							if (result == true)
								break;
						}
						return result;
					});
				}
			}
		}
	} while (false);
	return result;
}

bool STTx2SQL::check_raw(const Json::Value& raw, const uint16_t optype) {
	bool check = true;
	if (optype == BuildSQL::BUILD_DROPTABLE_SQL
		|| optype == BuildSQL::BUILD_RENAMETABLE_SQL
		|| optype == BuildSQL::BUILD_CANCEL_ASSIGN_SQL
		|| optype == BuildSQL::BUILD_ASSIGN_SQL) {
		return check;
	}

	if (raw.isArray() == false) {
		return false;
	}

	Json::UInt size = raw.size();
    // fix an issue: when delete all data from table, raw array size is 0
	if (size == 0 && optype != BuildSQL::BUILD_DELETE_SQL) {
		return false;
	}

	switch (optype) {
	case BuildSQL::BUILD_CREATETABLE_SQL:
	case BuildSQL::BUILD_INSERT_SQL:
	case BuildSQL::BUILD_UPDATE_SQL:
	case BuildSQL::BUILD_DELETE_SQL:
	case BuildSQL::BUILD_ASSERT_STATEMENT:
	case BuildSQL::BUILD_ALTER_ADD_SQL:
	case BuildSQL::BUILD_ALTER_DEL_SQL:
	case BuildSQL::BUILD_ALTER_MOD_SQL:
	case BuildSQL::BUILD_CREATE_INDEX_SQL:
	case BuildSQL::BUILD_DROP_INDEX_SQL:
		for (Json::UInt idx = 0; idx < size; idx++) {
			const Json::Value& e = raw[idx];
			if (e.isObject() == false) {
				check = false;
				break;
			}
			// null object
			if (e.getMemberNames().size() == 0) {
				check = false;
				break;
			}
		}
		break;
	default:
		check = false;
		break;
	}
	return check;
}

std::pair<bool, std::string> STTx2SQL::check_optionalRule(const std::string& optionalRule) {
	Json::Value rule;
	if (Json::Reader().parse(optionalRule, rule) == false) {
		return { false, 
			std::string("parse optionalRule unsuccessfully.") + optionalRule};
	}

	const std::vector<std::string>& keys = rule.getMemberNames();
	for (size_t i = 0; i < keys.size(); i++) {
		const std::string& key = keys[i];
		
		if(rule[key].isObject() && rule[key].isMember("Condition")) {
			//Json::Value& condition = rule[key]["Condition"];
			Json::Value conditions;
			conditions.append(std::move(rule[key]["Condition"]));
			
			auto node = conditionTree::createRoot(conditions);
			if (node.first != 0) {
				return { false, (boost::format("create condition unsuccessfully.[%s]")
					% optionalRule).str() };
			}
			auto result = conditionParse::parse_conditions(conditions, node.second);
			if (result.first != 0) {
				return { false, result.second };
			}
		}
	}
	return { true, "success"};
}

std::pair<int /*retcode*/, std::string /*sql*/> STTx2SQL::ExecuteSQL(
	const ripple::STTx& tx, 
	const SyncParam& param,
	bool bVerifyAffectedRows /* = false */) {
	std::pair<int, std::string> ret = { -1, "inner error" };
	if (tx.getTxnType() != ttTABLELISTSET && tx.getTxnType() != ttSQLSTATEMENT) {
		ret = { -1, "Transaction's type is error." };
		return ret;
	}

	uint16_t optype = tx.getFieldU16(sfOpType);
	const ripple::STArray& tables = tx.getFieldArray(sfTables);
	ripple::uint160 hex_tablename = tables[0].getFieldH160(sfNameInDB);
	//ripple::uint160 hex_tablename = tx.getFieldH160(sfNameInDB);
	std::string tn = ripple::to_string(hex_tablename);
	if (tn.empty()) {
		ret = { -1, "Table name is empty." };
		return ret;
	}

	std::string txt_tablename = std::string(TABLE_PREFIX) + tn;

	if (optype == 1 && tx.isFieldPresent(sfOperationRule)) {
		auto strOperationRule = strCopy(tx.getFieldVL(sfOperationRule));
		auto check = check_optionalRule(strOperationRule);
		if (check.first == false) {
			return{ -1, check.second };
		}
	}

	std::string sRaw = tx.buildRaw(param.rules);
	Json::Value raw_json;
	if (sRaw.size()) {
		if (Json::Reader().parse(sRaw, raw_json) == false) {
			ret = { -1, "parse Raw unsuccessfully." };
			return ret;
		}
	}
	else if (optype != 2) {	// delete sql hasn't raw
		ret = { -1, "Raw data is empty except delete-sql." };
		return ret;
	}

	if (check_raw(raw_json, optype) == false) {
		ret = { -1, (boost::format("Raw data is malformed. %s") % Json::jsonAsString(raw_json)).str() };
		return ret;
	}

	BuildSQL::BUILDTYPE build_type = BuildSQL::BUILD_UNKOWN;
	switch (optype)
	{
	case 1:
		build_type = BuildSQL::BUILD_CREATETABLE_SQL;
		break;
	case 2:
		build_type = BuildSQL::BUILD_DROPTABLE_SQL;
		break;
	case 3:
		build_type = BuildSQL::BUILD_RENAMETABLE_SQL;	// ignore handle
		break;
	case 4:
		build_type = BuildSQL::BUILD_ASSIGN_SQL;		// ignore handle
		break;
	case 5:
		build_type = BuildSQL::BUILD_CANCEL_ASSIGN_SQL;	// ignore handle
		break;
	case 6:
		build_type = BuildSQL::BUILD_INSERT_SQL;
		break;
	case 8:
		build_type = BuildSQL::BUILD_UPDATE_SQL;
		break;
	case 9:
		build_type = BuildSQL::BUILD_DELETE_SQL;
		break;
	case 10:
		build_type = BuildSQL::BUILD_ASSERT_STATEMENT;
		break;
	case 12:
		build_type = BuildSQL::BUILD_RECREATE_SQL;
		break;
	case 14:
		build_type = BuildSQL::BUILD_ALTER_ADD_SQL;
		break;
	case 15:
		build_type = BuildSQL::BUILD_ALTER_DEL_SQL;
		break;
	case 16:
		build_type = BuildSQL::BUILD_ALTER_MOD_SQL;
		break;
	case 17:
		build_type = BuildSQL::BUILD_CREATE_INDEX_SQL;
		break;
	case 18:
		build_type = BuildSQL::BUILD_DROP_INDEX_SQL;
		break;
	default:
		break;
	}

	std::shared_ptr<BuildSQL> buildsql = nullptr;
	if (boost::iequals(db_type_, "mycat") || boost::iequals(db_type_, "mysql")) {
		buildsql = std::make_shared<BuildMySQL>(build_type, db_conn_);
	}
	else if (boost::iequals(db_type_, "sqlite")) {
		buildsql = std::make_shared<BuildSqlite>(build_type, db_conn_);
	}

	if (buildsql == nullptr) {
		ret = { -1, "Resource may be exhausted." };
		return ret;
	}
	buildsql->AddTable(txt_tablename);

	auto mapFieldValue = ParseAutoFields(tx, param, txt_tablename);

	if (build_type == BuildSQL::BUILD_INSERT_SQL) {
		std::string sql;
		int affected_rows = 0;
        buildsql->batch_insert((uint32_t)raw_json.size());
        
		for (Json::UInt idx = 0; idx < raw_json.size(); idx++) {
			auto& v = raw_json[idx];
			if (v.isObject() == false) {
				//JSON_ASSERT(v.isObject());
				ret = { -1, "Element of raw may be malformed." };
				return ret;
			}

			auto retPair = GenerateInsertSql(v, buildsql.get());
			if (retPair.first != 0) {
				return retPair;
			}

			//Fill auto-fill fields
			for (auto &kv : mapFieldValue)
            {
                if (kv.second.pairField.first)
                {
                    std::string& fieldname = kv.second.pairField.second;
                    if (BuildField::HaveSpecialCharacters(fieldname)) {
                        return {-1, (boost::format("fieldname is illegal: %s") % fieldname).str()};
                    }
                    
                    BuildField field(fieldname);
                    field.SetFieldValue(kv.second.value);
                    buildsql->AddField(field);
                }
            }
		}
        
        sql = buildsql->asString();
        if (buildsql->execSQL() != 0) {
            //ret = { -1, std::string("Executing SQL was failure.") + sql };
            if(sql.size() < 1024)
            {
                ret = { -1, (boost::format("Executing `%1%` was failure. %2%")
                    % sql
                    %buildsql->last_error().second).str() };
            }
            else
            {
                ret = { -1, (boost::format("Executing was failure. %1%")
                    %buildsql->last_error().second).str() };
            }

            return ret;
        }
        affected_rows += db_conn_->getSession().get_affected_row_count();
        db_conn_->getSession().set_affected_row_count(0);

		if (bVerifyAffectedRows && affected_rows == 0)
			return{ -1, "insert operation affect 0 rows." };

		return{ 0, sql };
	}
	else if (build_type == BuildSQL::BUILD_ASSERT_STATEMENT) {
		auto result = handle_assert_statement(raw_json, buildsql.get());
		if (result.first) {
			return{ 0, result.second };
		}
		return{ 1, result.second };
	}


	int result = -1;

	if (build_type == BuildSQL::BUILD_UPDATE_SQL) {

		Json::Value conditions;
		// parse record
		for (Json::UInt idx = 0; idx < raw_json.size(); idx++) {
			auto& v = raw_json[idx];
			if (v.isObject() == false) {
				//JSON_ASSERT(v.isObject());
				result = -1;
			}

			if (idx == 0) {
				std::vector<std::string> members = v.getMemberNames();

				// Fill auto-fill fields except for tx-history
				for (auto& kv : mapFieldValue)
				{
					if (kv.first == sfTxsHashFillField.getName())
						continue;
           
					if (kv.second.pairField.first)
					{
                        std::string& fieldname = kv.second.pairField.second;
                        if (BuildField::HaveSpecialCharacters(fieldname)) {
                            return {-1, (boost::format("fieldname is illegal: %s") % fieldname).str()};
                        }
                        
						BuildField field(fieldname);
						field.SetFieldValue(kv.second.value);
						buildsql->AddField(field);
					}
				}

				for (size_t i = 0; i < members.size(); i++) {
					std::string field_name = members[i];

                    if (BuildField::HaveSpecialCharacters(field_name)) {
                        return {-1, (boost::format("fieldname is illegal: %s") % field_name).str()};
                    }

					BuildField field(field_name);
					std::pair<int, std::string> ret = parseField(v[field_name], field);
					if (ret.first != 0) {
						result = ret.first;
					}
					buildsql->AddField(field);
				}
			}
			else {
				conditions.append(v);
			}
		}

		if (mapFieldValue[sfTxsHashFillField.getName()].pairField.first)
        {
            auto fieldName =
                mapFieldValue[sfTxsHashFillField.getName()].pairField.second;
            
            if (BuildField::HaveSpecialCharacters(fieldName)) {
                return{-1, (boost::format("fieldname is illegal: %s") % fieldName).str()};
            }
            
            BuildField update_field(fieldName);
            std::string updateStr =
                        (boost::format("concat(%1%,\",%2%\")") %
                         fieldName % to_string(tx.getRealTxID())).str();
			update_field.SetFieldValue(updateStr, FieldValue::fCOMMAND);
			buildsql->AddField(update_field);
		}

		if (conditions.isArray() && conditions.size() > 0)
			buildsql->AddCondition(conditions);

		result = 0;
	}



	switch (build_type)
	{
	case BuildSQL::BUILD_CREATETABLE_SQL:
		result = GenerateCreateTableSql(raw_json, buildsql.get());
		break;
	case BuildSQL::BUILD_DROPTABLE_SQL:
		result = 0;	// only has tablename
		break;
	case BuildSQL::BUILD_RENAMETABLE_SQL:
		break;
	case BuildSQL::BUILD_ASSIGN_SQL:
		break;
	case BuildSQL::BUILD_CANCEL_ASSIGN_SQL:
		break;
		//case BuildSQL::BUILD_UPDATE_SQL:
		//	result = GenerateUpdateSql(raw_json, buildsql.get());
		//	break;
	case BuildSQL::BUILD_DELETE_SQL:
		result = GenerateDeleteSql(raw_json, buildsql.get());
		break;
	case BuildSQL::BUILD_RECREATE_SQL:
		result = GenerateCreateTableSql(raw_json, buildsql.get());
		break;
	case BuildSQL::BUILD_ALTER_ADD_SQL:
		result = GenerateAddColumnsSql(raw_json, buildsql.get());
		break;
	case BuildSQL::BUILD_ALTER_DEL_SQL:
		result = GenerateDelColumnsSql(raw_json, buildsql.get());
		break;
	case BuildSQL::BUILD_ALTER_MOD_SQL:
		result = GenerateModifyColumnsSql(raw_json, buildsql.get());
		break;
	case BuildSQL::BUILD_CREATE_INDEX_SQL:
		result = GenerateOperateIndex(raw_json, buildsql.get());
		break;
	case BuildSQL::BUILD_DROP_INDEX_SQL:
		result = GenerateOperateIndex(raw_json, buildsql.get());
		break;
	default:
		break;
	}

	if (result == 0 && buildsql->execSQL() == 0) {
		if (bVerifyAffectedRows && db_conn_->getSession().get_affected_row_count() == 0) {
			if (build_type == BuildSQL::BUILD_UPDATE_SQL)
				return{ -1, "update operation affect 0 rows." };
			if (build_type == BuildSQL::BUILD_DELETE_SQL)
				return{ -1, "delete operation affect 0 rows." };
		}
		db_conn_->getSession().set_affected_row_count(0);

		ret = { 0, (boost::format("Execute `%1%` successfully") % buildsql->asString()).str() };
	}
	else {
		ret = { -1, (boost::format("Execute `%1%` unsuccessfully. %2%")
			% buildsql->asString()
			% buildsql->last_error().second).str() };
	}

	return ret;
}

///////////////////////////////////////////////////////////////////////////////////
// TxStore::TxHistory
///////////////////////////////////////////////////////////////////////////////////
DatabaseCon* TxStore::getDatabaseCon() {
	return databasecon_;
}

Json::Value TxStore::txHistory(RPC::JsonContext& context) {
    return txHistory(context.params[jss::tx_json]);
}

std::pair<std::vector<std::vector<Json::Value>>, std::string> TxStore::txHistory2d(RPC::JsonContext& context)
{
	return txHistory2d(context.params[jss::tx_json]);
}

Json::Value TxStore::txHistory(Json::Value& tx_json) {
    Json::Value obj;
    if (databasecon_ == nullptr)
        return rpcError(rpcNODB);

    std::shared_ptr<BuildSQL> buildsql = nullptr;
    if (boost::iequals(db_type_, "sqlite"))
        buildsql = std::make_shared<BuildSqlite>(BuildSQL::BUILD_SELECT_SQL, databasecon_);
    else if(boost::iequals(db_type_, "mycat") || boost::iequals(db_type_, "mysql"))
        buildsql = std::make_shared<BuildMySQL>(BuildSQL::BUILD_SELECT_SQL, databasecon_);

    if (buildsql == nullptr)
    {
		std::string errMsg = "Initial buildsql failed.";
		return RPC::make_error(rpcINTERNAL, errMsg);
    }

    return helper::query_directly(tx_json, databasecon_, buildsql.get(), select_limit_);
}

std::pair<std::vector<std::vector<Json::Value>>, std::string> TxStore::txHistory2d(Json::Value& tx_json) {
	std::vector<std::vector<Json::Value>> ret;
	if (databasecon_ == nullptr)
		return std::make_pair(ret,"internal error: connection object is null");

	std::shared_ptr<BuildSQL> buildsql = nullptr;
	if (boost::iequals(db_type_, "sqlite"))
		buildsql = std::make_shared<BuildSqlite>(BuildSQL::BUILD_SELECT_SQL, databasecon_);
	else if (boost::iequals(db_type_, "mycat") || boost::iequals(db_type_, "mysql"))
		buildsql = std::make_shared<BuildMySQL>(BuildSQL::BUILD_SELECT_SQL, databasecon_);

	if (buildsql == nullptr)
	{
		return std::make_pair(ret,"there is no DB in this node");
	}

	return helper::query_directly2d(tx_json, databasecon_, buildsql.get(),select_limit_);
}

Json::Value TxStore::txHistory(std::string sql) {
    Json::Value obj;
	if (databasecon_ == nullptr) {
		return rpcError(rpcNODB);
	}
      

	std::shared_ptr<BuildSQL> buildsql = nullptr;
	
	if (boost::iequals(db_type_, "sqlite"))
		buildsql = std::make_shared<BuildSqlite>(BuildSQL::BUILD_SELECT_SQL, databasecon_);
    else if (boost::iequals(db_type_, "mycat") || boost::iequals(db_type_, "mysql"))
        buildsql = std::make_shared<BuildMySQL>(BuildSQL::BUILD_SELECT_SQL, databasecon_);

    if (buildsql == nullptr)
    {
		std::string errMsg = "Initial buildsql failed.";
		return RPC::make_error(rpcINTERNAL, errMsg);
    }

	return helper::query_directly(databasecon_, sql, select_limit_);
}
}	// namespace ripple
