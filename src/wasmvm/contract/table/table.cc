#include <string>

#include "chainsqlib/contracts/contract.h"
#include "chainsqlib/contracts/table.h"

class testTable : public chainsql::contract {
public:
    using contract::contract;
    void constructor()
    {

    }

    void create()
    {
        chainsql::ColumnDefiniton<int> id = {
            NAME "id",
            DEFINITION chainsql::ColumnDescription<int>{
                NOTNULL true,
                KEY true,
                UNIQUE true,
                INDEX true,
                AUTOCREMENT true,
                DEFUALTVALUE 1}};
        chainsql::ColumnDefiniton<std::string> name = {
            NAME "name",
            DEFINITION chainsql::ColumnDescription<std::string>{
                NOTNULL false,
                KEY false,
                UNIQUE true,
                INDEX false,
                AUTOCREMENT false,
                DEFUALTVALUE ""}};
        chainsql::ColumnDefiniton<int> age = {
            NAME "age",
            DEFINITION chainsql::ColumnDescription<int>{
                NOTNULL false,
                KEY false,
                UNIQUE false,
                INDEX false,
                AUTOCREMENT false,
                DEFUALTVALUE 0}};
        chainsql::BuildTable::New<chainsql::Table::Create>("peersafe")
            .addColumn(id)
            .addColumn(name)
            .addColumn(age)
            .execute();
    }
};

CHAINSQL_DISPATCH(testTable, (create))