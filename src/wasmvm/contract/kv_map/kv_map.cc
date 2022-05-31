#include "chainsqlib/contracts/contract.h"
#include "chainsqlib/contracts/map.h"

struct person {
    std::string name;
    std::string title;
    int age;
};

class kv_map : public chainsql::contract {
public:
    using contract::contract;

    void insert(int id, std::string name, std::string title, int age) {
        person p = {name, title, age};
        persons[id] = p;
    }

    void update(int id, std::string name) {
        auto it = persons.find(id);
        if it != persons.end() {
            it->name = name;
        }
    }

    void erase(int id) {
        auto it = persons.find(id);
        if(it != persons.end()) {
            persons.erase(id);
        }
    }

    person get(int id) {
        return persons.find(id)->second;
    }

    void show() {
        for(auto it = persons.begin(); it != persons.end(); it++) {
            chainsql::printf("id = %d, name = %s, title = %s, age = %d\n"
            it->first, it->second.name, it->second.title, it->second.age);
        }
    }
private:
    chainsql::kv::map<chainsql::name("person"), int, person> persons;
};

CHAINSQL_DISPATCH(kv_map, (insert)(update)(erase)(get)(show))