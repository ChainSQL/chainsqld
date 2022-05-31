#include "chainsqlib/contracts/contract.h"
#include "chainsqlib/contracts/storage.h"
#include "chainsqlib/core/serialize.hpp"

struct person {
    std::string name;
    std::string title;
    int age;

    CHAINSQLLIB_SERIALIZE(person, (name)(title)(age))
};

class storage : public chainsql::contract {
public:
    using contract::contract;

    void setint(int value) {
        chainsql::setState("int", value);
    }

    void setperson(std::string name, std::string title, int age) {
        person p = {name, title, age};
        chainsql::setState(name, p);
    }

private:
};

CHAINSQL_DISPATCH(storage, (setint)(setperson))