#include "chainsqlib/contracts/table.h"

void TestCondtion()
{
    int a = 10;
    int b = 11;
    EqCondition<int> eq("id", a); //  id = 10
    NqCondition<int> ne("id", b); // id != 11

    GtCondition<int> gt("id", a); // id > 10
    LtCondition<int> lt("id", a); // id < 10 
    GeCondition<int> ge("id", a); // id >= 10
    LeCondition<int> le("id", b); // id <= 10 

    AndCondition<EqCondition, NqCondition> and(eq, ne); // id = 10 and id != 11
}

void createTable()
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
    chainsql::BuildTable::New<chainsql::Table::Create>("peersafe")
                            .addColumn(id)
                            .addColumn(name)
                            .execute();
}

void insertRecord()
{
    chainsql::ColunmValue<int> id = {
        NAME "id",
        COLUMNVALUE 2};
    chainsql::ColunmValue<std::string> name = {
        NAME "name",
        COLUMNVALUE "zxc"}};
    chainsql::BuildTable::New<chainsql::Table::Insert>("peersafe")
                            .addColumn(id)
                            .addColumn(name)
                            .execute();
}

void UpdateRecord()
{
    chainsql::ColunmValue<int> id = {
        NAME "id",
        COLUMNVALUE 2};
    chainsql::ColunmValue<std::string> name = {
        NAME "name",
        COLUMNVALUE "zxc"};
    chainsql::BuildTable::New<chainsql::Table::Update>("peersafe")
                            .addColumn(id)
                            .addColumn(name)
                            .withCondition(condition_expression)
                            .execute();
}

void DeleteRecord()
{
    chainsql::BuildTable::New<chainsql::Table::Delete>("peersafe")
        .withCondition(condition_expression)
        .execute();
}

void RenameTable()
{
    chainsql::BuildTable::New<chainsql::Table::Rename>("peersafe")
    .newName("newTable")
    .execute();
}

void DropTable()
{
    chainsql::BuildTable::New<chainsql::Table::Drop>("peersafe").execute();
}

void GrantTable()
{
    chainsql::BuildTable::New<chainsql::Table::Grant>("peersafe")
    .grant("n9LDNrsWjwFPQu5JeHje2tpfKLdgGP3TwRExtXQNQae3zXtyZEuw", SELECT|UPDATE|DELETE)
    .execute();
}

void Query()
{
    chainsql::Column id = {NAME "id"};
    chainsql::Column name = {NAME "name"};
    Handle h = chainsql::BuildTable::New<chainsql::Table::Query>("peersafe")
                            .addColumn(id)
                            .addColumn(name)
                            .withCondition(condition_expression)
                            .execute();

   chainsql::QueryResult result(h);
   do {
       chainsql::QueryResult::OneRecord record = result.get();
   } while(result.next())
}

void transaction()
{
    chainsql::Table::Transaction::begin();
    insertRecord();
    UpdateRecord();
    chainsql::Table::Transaction::commit();
}

int main(int argc, char **argv)
{

    return 0;
}